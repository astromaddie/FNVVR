import json
import io
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from tools import frame_audio_guard as guard


DEFAULT = "alsa_loopback_device.stereo.alsa_output.platform-sound.HiFi__Speaker__sink"
OTHER = "a-different-default-sink"


class FakePactl:
    def __init__(self):
        self.sinks = {
            DEFAULT: {"name": DEFAULT, "mute": False, "volume": {
                "front-left": {"value": 50627}, "front-right": {"value": 50111}}},
            guard.EQ_SINK: {"name": guard.EQ_SINK, "mute": True, "volume": {
                "front-left": {"value": 58958}, "front-right": {"value": 48123}}},
            OTHER: {"name": OTHER, "mute": False, "volume": {
                "front-left": {"value": 12000}, "front-right": {"value": 13000}}},
        }
        self.default = DEFAULT
        self.calls = []
        self.fail_mute_name = None
        self.fail_volume_name = None
        self.bad_readback = False
        self.fail_watchdog = False

    def __call__(self, args, **kwargs):
        self.calls.append(args)
        if args[0] == "systemd-run" and self.fail_watchdog:
            raise RuntimeError("watchdog refused")
        if args[:3] == ["pactl", "-f", "json"] and args[3:] == ["list", "sinks"]:
            sinks = json.loads(json.dumps(list(self.sinks.values())))
            if self.bad_readback:
                sinks[0]["mute"] = True
            return SimpleNamespace(stdout=json.dumps(sinks))
        if args[:3] == ["pactl", "-f", "json"] and args[3:] == ["info"]:
            return SimpleNamespace(stdout=json.dumps({"default_sink_name": self.default}))
        if args[:2] == ["pactl", "set-sink-volume"]:
            name = args[2]
            if self.fail_volume_name == name:
                raise RuntimeError("volume failed")
            channels = self.sinks[name]["volume"]
            for channel, raw in zip(channels, args[3:]):
                channels[channel]["value"] = int(raw)
        elif args[:2] == ["pactl", "set-sink-mute"]:
            name = args[2]
            if self.fail_mute_name == name:
                self.fail_mute_name = None
                raise RuntimeError("mute failed")
            self.sinks[name]["mute"] = args[3] == "1"
        return SimpleNamespace(stdout="")


class FrameAudioGuardTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.state = Path(self.tmp.name) / "state.json"
        self.fake = FakePactl()

    def test_restore_preserves_independent_mutes_and_raw_channel_volumes(self):
        expected = guard.snapshot(self.fake)
        guard.write_snapshot(self.state, expected)
        self.assertEqual(self.state.stat().st_mode & 0o777, 0o600)
        for sink in expected:
            self.fake.sinks[sink["name"]]["mute"] = not sink["mute"]
            for item in self.fake.sinks[sink["name"]]["volume"].values():
                item["value"] = 1234
        guard.restore(self.state, self.fake)
        self.assertFalse(self.state.exists())
        self.assertEqual(guard.snapshot(self.fake), expected)

    @patch.object(guard.subprocess, "run", return_value=SimpleNamespace(returncode=0))
    def test_partial_mute_failure_still_restores_every_sink(self, child):
        self.fake.fail_mute_name = guard.EQ_SINK
        with self.assertRaisesRegex(RuntimeError, "mute failed"):
            guard.run_guard(5, ["game"], self.state, self.fake)
        self.assertFalse(self.state.exists())
        self.assertEqual(guard.snapshot(self.fake), [
            {"name": DEFAULT, "mute": False, "volumes": {"front-left": 50627, "front-right": 50111}},
            {"name": guard.EQ_SINK, "mute": True, "volumes": {"front-left": 58958, "front-right": 48123}},
        ])
        child.assert_not_called()

    def test_watchdog_failure_never_mutes_and_cleans_snapshot(self):
        self.fake.fail_watchdog = True
        with self.assertRaisesRegex(RuntimeError, "not muted"):
            guard.run_guard(5, ["game"], self.state, self.fake)
        self.assertFalse(self.state.exists())
        self.assertFalse(any(c[:2] == ["pactl", "set-sink-mute"] for c in self.fake.calls))

    def test_failed_restore_readback_retains_state(self):
        expected = guard.snapshot(self.fake)
        guard.write_snapshot(self.state, expected)
        self.fake.bad_readback = True
        with self.assertRaisesRegex(RuntimeError, "readback mismatch"):
            guard.restore(self.state, self.fake)
        self.assertTrue(self.state.exists())

    def test_volume_failure_does_not_skip_mute_restore(self):
        expected = guard.snapshot(self.fake)
        guard.write_snapshot(self.state, expected)
        self.fake.sinks[DEFAULT]["mute"] = True
        self.fake.fail_volume_name = DEFAULT
        with self.assertRaisesRegex(RuntimeError, "volume restore failed"):
            guard.restore(self.state, self.fake)
        self.assertFalse(self.fake.sinks[DEFAULT]["mute"])
        self.assertTrue(self.state.exists())

    def test_restore_reads_original_sinks_after_default_changes(self):
        expected = guard.snapshot(self.fake)
        guard.write_snapshot(self.state, expected)
        self.fake.sinks[DEFAULT]["mute"] = True
        self.fake.default = OTHER
        guard.restore(self.state, self.fake)
        self.assertFalse(self.fake.sinks[DEFAULT]["mute"])
        self.assertTrue(self.fake.sinks[guard.EQ_SINK]["mute"])
        self.assertFalse(self.state.exists())

    @patch.object(guard.subprocess, "run", return_value=SimpleNamespace(returncode=0))
    def test_existing_state_is_never_overwritten_or_muted(self, child):
        self.state.write_text("unresolved recovery state\n")
        with self.assertRaises(FileExistsError):
            guard.run_guard(5, ["game"], self.state, self.fake)
        self.assertEqual(self.state.read_text(), "unresolved recovery state\n")
        self.assertFalse(any(c[:2] == ["pactl", "set-sink-mute"] for c in self.fake.calls))
        self.assertFalse(any(c[0] == "systemd-run" for c in self.fake.calls))
        child.assert_not_called()

    @patch.object(guard.subprocess, "run", return_value=SimpleNamespace(stdout=""))
    def test_control_commands_have_finite_timeout(self, subprocess_run):
        guard.command(["pactl", "info"])
        self.assertEqual(subprocess_run.call_args.kwargs["timeout"], guard.CONTROL_TIMEOUT_SECONDS)

    def test_restore_failure_prints_shell_quoted_recovery_command(self):
        missing = str(self.state / "state file.json")
        output = io.StringIO()
        with redirect_stderr(output):
            self.assertEqual(guard.main(["restore", missing]), 1)
        self.assertIn("Run: " + guard.shlex.join([
            guard.sys.executable, str(Path(guard.__file__).resolve()), "restore", missing
        ]), output.getvalue())


if __name__ == "__main__":
    unittest.main()
