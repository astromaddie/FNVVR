#!/usr/bin/env python3
"""Temporarily mute Frame speaker sinks and restore their exact prior state."""

import argparse
import json
import os
from pathlib import Path
import secrets
import shlex
import signal
import subprocess
import sys
import tempfile

EQ_SINK = "frame-speaker-eq-sink"
CONTROL_TIMEOUT_SECONDS = 10


def command(args, *, check=True):
    return subprocess.run(args, check=check, text=True, capture_output=True,
                          timeout=CONTROL_TIMEOUT_SECONDS)


def read_named_sinks(names, run=command):
    sinks = json.loads(run(["pactl", "-f", "json", "list", "sinks"]).stdout)
    by_name = {sink["name"]: sink for sink in sinks}
    result = []
    for name in names:
        if name not in by_name:
            raise RuntimeError(f"Required audio sink is missing: {name}")
        sink = by_name[name]
        result.append({"name": name, "mute": bool(sink["mute"]),
                       "volumes": {ch: int(value["value"])
                                   for ch, value in sink["volume"].items()}})
    return result


def snapshot(run=command):
    default = json.loads(run(["pactl", "-f", "json", "info"]).stdout).get("default_sink_name")
    names = list(dict.fromkeys([default, EQ_SINK]))
    if not default:
        raise RuntimeError(f"Required audio sink missing: default={default!r}, EQ={EQ_SINK!r}")
    return read_named_sinks(names, run)


def write_snapshot(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "w") as stream:
            json.dump({"sinks": data}, stream, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        # Linking the complete same-directory temporary file creates the final
        # path atomically and fails rather than replacing unresolved recovery data.
        os.link(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def restore(path, run=command):
    saved = json.loads(path.read_text())["sinks"]
    errors = []
    for sink in saved:
        name = sink["name"]
        # Attempt both operations independently: volume failure must not skip
        # restoring a previously unmuted sink.
        try:
            run(["pactl", "set-sink-volume", name, *map(str, sink["volumes"].values())])
        except Exception as exc:
            errors.append(f"{name}: volume restore failed: {exc}")
        try:
            run(["pactl", "set-sink-mute", name, "1" if sink["mute"] else "0"])
        except Exception as exc:
            errors.append(f"{name}: mute restore failed: {exc}")
    try:
        current = read_named_sinks([s["name"] for s in saved], run)
        for wanted, actual in zip(saved, current):
            if actual != wanted:
                errors.append(f"{wanted['name']}: readback mismatch; expected {wanted}, got {actual}")
    except Exception as exc:
        errors.append(f"readback failed: {exc}")
    if errors:
        raise RuntimeError("; ".join(errors))
    path.unlink()


def run_guard(timeout, argv, state_path=None, run=command):
    if state_path is None:
        runtime = os.environ.get("XDG_RUNTIME_DIR")
        if not runtime:
            raise RuntimeError("XDG_RUNTIME_DIR is unset; supply --state-file")
        state_path = Path(runtime) / f"frame-audio-guard-{os.getpid()}-{secrets.token_hex(6)}.json"
    else:
        state_path = Path(state_path)
    saved = snapshot(run)
    write_snapshot(state_path, saved)
    unit = f"frame-audio-guard-{os.getpid()}-{secrets.token_hex(4)}"
    script = str(Path(__file__).resolve())
    try:
        run(["systemd-run", "--user", f"--unit={unit}", f"--on-active={timeout + 30}s",
             "--timer-property=AccuracySec=1s", "--collect", sys.executable,
             script, "restore", str(state_path)])
    except Exception:
        state_path.unlink(missing_ok=True)
        raise RuntimeError("Could not schedule audio restore watchdog; audio was not muted")

    restored = False
    previous = {}

    def interrupted(signum, _frame):
        raise KeyboardInterrupt(f"signal {signum}")

    try:
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            previous[sig] = signal.signal(sig, interrupted)
        try:
            # Cleanup is registered before changing either sink.
            for sink in saved:
                run(["pactl", "set-sink-mute", sink["name"], "1"])
            result = subprocess.run(argv, timeout=timeout, check=False)
            status = result.returncode
        except subprocess.TimeoutExpired:
            status = 124
        finally:
            restore(state_path, run)
            restored = True
        return status
    finally:
        for sig, handler in previous.items():
            signal.signal(sig, handler)
        if restored:
            run(["systemctl", "--user", "stop", f"{unit}.timer", f"{unit}.service"], check=False)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    run_parser = sub.add_parser("run", help="mute speakers while running a bounded command")
    run_parser.add_argument("--timeout", type=int, required=True)
    run_parser.add_argument("--state-file")
    run_parser.add_argument("command", nargs=argparse.REMAINDER)
    restore_parser = sub.add_parser("restore", help="restore a saved snapshot")
    restore_parser.add_argument("state_file")
    args = parser.parse_args(argv)
    try:
        if args.action == "restore":
            restore(Path(args.state_file))
            return 0
        command_argv = args.command[1:] if args.command[:1] == ["--"] else args.command
        if args.timeout < 1 or not command_argv:
            parser.error("run requires --timeout >= 1 and -- COMMAND...")
        if not args.state_file:
            runtime = os.environ.get("XDG_RUNTIME_DIR")
            if runtime:
                args.state_file = str(Path(runtime) / f"frame-audio-guard-{os.getpid()}-{secrets.token_hex(6)}.json")
        return run_guard(args.timeout, command_argv, args.state_file)
    except Exception as exc:
        print(f"frame_audio_guard: {exc}", file=sys.stderr)
        state_file = getattr(args, "state_file", None)
        if args.action == "restore":
            state_file = args.state_file
        if state_file:
            recovery = shlex.join([sys.executable, str(Path(__file__).resolve()),
                                   "restore", state_file])
            print(f"Audio recovery state retained if present. Run: {recovery}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
