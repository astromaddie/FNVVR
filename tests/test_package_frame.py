import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
import zipfile


REPO = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_frame", REPO / "tools/package_frame.py")
package_frame = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package_frame)


def run(*args, cwd):
    return subprocess.run(args, cwd=cwd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


def pe32_dll(machine=0x14C, magic=0x10B, dll=True, embedded=b""):
    pe_offset = 0x80
    data = bytearray(pe_offset + 24 + 2)
    data[:2] = b"MZ"
    data[0x3C:0x40] = pe_offset.to_bytes(4, "little")
    data[pe_offset:pe_offset + 4] = b"PE\0\0"
    coff = pe_offset + 4
    data[coff:coff + 2] = machine.to_bytes(2, "little")
    data[coff + 16:coff + 18] = (2).to_bytes(2, "little")
    data[coff + 18:coff + 20] = (0x2000 if dll else 0x0002).to_bytes(2, "little")
    data[coff + 20:coff + 22] = magic.to_bytes(2, "little")
    return bytes(data) + embedded


class PackageFrameTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "repo"
        self.root.mkdir()
        self._write("tools/frame_launch.sh", "#!/bin/sh\nexec \"$@\"\n", 0o755)
        self._write("packaging/steam-frame/dxvk.conf", "d3d9.vrBackend = openvr\n")
        self._write("docs/FRAME_INSTALL.md", "Install guidance\n")
        self._write("docs/FRAME_CONTROLS.md", "Controls\n")
        self._write("docs/VR_ARCHITECTURE.md", "[upstream changelog](../CHANGELOG.md)\n[install and troubleshooting guide](FRAME_INSTALL.md)\n[controls](FRAME_CONTROLS.md)\n")
        self._write("CHANGELOG.md", "Release changes\n")
        self._write("LICENSE", "license\n")
        self._write("src/d3d9/vr/openvr/actions.json", json.dumps({
            "default_bindings": [
                {"controller_type": "frame_controller", "binding_url": "bindings_frame_controller.json"},
                {"controller_type": "oculus_touch", "binding_url": "bindings_oculus_touch.json"},
            ],
            "actions": [{"name": "/actions/game/in/attack"}],
        }))
        binding = {"controller_type": None, "bindings": {"/actions/game": {"sources": [
            {"inputs": {"click": {"output": "/actions/game/in/attack"}}}
        ]}}}
        for name, kind in (("frame_controller", "frame_controller"), ("oculus_touch", "oculus_touch")):
            binding["controller_type"] = kind
            self._write(f"src/d3d9/vr/openvr/bindings_{name}.json", json.dumps(binding))

        run("git", "init", "-b", "master", cwd=self.root)
        run("git", "config", "user.name", "Package Test", cwd=self.root)
        run("git", "config", "user.email", "package@example.invalid", cwd=self.root)
        self._write(".gitignore", "/build/\n")
        run("git", "add", ".", cwd=self.root)
        run("git", "commit", "-m", "source", cwd=self.root)
        run("git", "tag", "-a", "v1.0", "-m", "version 1.0", cwd=self.root)
        self._write(".gitignore", "/build/\n")
        self._write("source.txt", "source revision\n")
        run("git", "add", ".", cwd=self.root)
        run("git", "commit", "-m", "release source", cwd=self.root)
        self.head = run("git", "rev-parse", "HEAD", cwd=self.root).stdout.strip()
        self.upstream = run("git", "rev-parse", "HEAD^", cwd=self.root).stdout.strip()
        run("git", "update-ref", "refs/remotes/upstream/master", self.upstream, cwd=self.root)
        self.source_version = run("git", "describe", "--dirty=+", cwd=self.root).stdout.strip()
        self.version = "frame-2026.10.29.1"
        self.build = self.root / "build"
        (self.build / "meson-private").mkdir(parents=True)
        (self.build / "meson-private/coredata.dat").write_bytes(b"meson metadata")
        (self.build / "meson-info").mkdir()
        (self.build / "meson-info/meson-info.json").write_text(json.dumps({
            "directories": {"source": str(self.root), "build": str(self.build)},
            "meson_version": {"full": "1.7.0"},
        }))
        (self.build / "meson-info/intro-compilers.json").write_text(json.dumps({
            "host": {"cpp": {"id": "gcc", "version": "14.2.0", "full_version": "gcc 14.2.0", "linker_id": "ld.bfd"}},
            "build": {},
        }))
        (self.build / "build.ninja").write_text("# meson build\n")
        (self.build / "version.h").write_text(f'#define DXVK_VERSION "{self.source_version}"\n')
        self.dll = self.build / "src/d3d9/d3d9.dll"
        self.dll.parent.mkdir(parents=True)
        self.dll.write_bytes(pe32_dll(embedded=self.source_version.encode("ascii")))

    def tearDown(self):
        self.temp.cleanup()

    def _write(self, relative, text, mode=0o644):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        path.chmod(mode)

    def package_to(self, name="release.zip"):
        output = Path(self.temp.name) / name
        return package_frame.package(self.root, self.dll, self.version, output)

    def test_creates_deterministic_allowlisted_zip_with_verifiable_metadata(self):
        first = self.package_to("first.zip")
        second = self.package_to("second.zip")
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertEqual(first.with_suffix(".zip.sha256").read_text(),
                         f"{hashlib.sha256(first.read_bytes()).hexdigest()}  first.zip\n")
        with zipfile.ZipFile(first) as archive:
            prefix = f"FNVVR-Frame-{self.version}/"
            members = {name.removeprefix(prefix): archive.read(name) for name in archive.namelist()}
            expected = {
                "d3d9.dll", "dxvk.conf", "frame_launch.sh", "fnvvr/actions.json",
                "fnvvr/bindings_frame_controller.json", "fnvvr/bindings_oculus_touch.json",
                "README.md", "FRAME_CONTROLS.md", "VR_ARCHITECTURE.md", "CHANGELOG.md", "LICENSE",
                "build-info.json", "SHA256SUMS",
            }
            self.assertEqual(set(members), expected)
            info = json.loads(members["build-info.json"])
            self.assertEqual(info["source_commit"], self.head)
            self.assertEqual(info["version"], self.version)
            self.assertEqual(info["dxvk_version"], self.source_version)
            self.assertEqual(info["build_tools"]["meson"], "1.7.0")
            self.assertEqual(info["build_tools"]["compilers"], [{
                "machine": "host", "language": "cpp", "id": "gcc", "version": "14.2.0",
                "full_version": "gcc 14.2.0", "linker_id": "ld.bfd",
            }])
            self.assertEqual(info["source_tree"], run("git", "rev-parse", "HEAD^{tree}", cwd=self.root).stdout.strip())
            self.assertEqual(info["upstream_commit"], self.upstream)
            self.assertTrue(info["tracked_worktree_clean"])
            self.assertEqual(info["target"], "Windows PE32 i386 DLL under Steam Frame Proton/FEX")
            self.assertNotIn("FalloutNV.exe", " ".join(members))
            self.assertNotIn("save", " ".join(members).lower())
            self.assertNotIn("headtrack", members["dxvk.conf"].decode().lower())
            for name, expected_file in info["files"].items():
                self.assertEqual(len(members[name]), expected_file["size_bytes"])
                self.assertEqual(hashlib.sha256(members[name]).hexdigest(), expected_file["sha256"])
            sums = dict(line.split("  ", 1)[::-1] for line in members["SHA256SUMS"].decode().splitlines())
            self.assertEqual(set(sums), set(members) - {"SHA256SUMS"})
            for name, digest in sums.items():
                self.assertEqual(hashlib.sha256(members[name]).hexdigest(), digest)
            self.assertEqual(archive.getinfo(prefix + "frame_launch.sh").external_attr >> 16 & 0o777, 0o755)
            for markdown_name in ("README.md", "VR_ARCHITECTURE.md"):
                markdown = members[markdown_name].decode("utf-8")
                for target in re.findall(r"\[[^]]+\]\(([^)]+)\)", markdown):
                    if "://" in target or target.startswith(("#", "mailto:")):
                        continue
                    linked_file = (Path(markdown_name).parent / target.split("#", 1)[0]).as_posix()
                    self.assertIn(linked_file, members, f"unresolved packaged link in {markdown_name}: {target}")
            self.assertIn("[upstream changelog](CHANGELOG.md)", members["VR_ARCHITECTURE.md"].decode())
            self.assertIn("[install and troubleshooting guide](README.md)", members["VR_ARCHITECTURE.md"].decode())

    def test_rejects_non_i386_non_pe32_and_non_dll_images(self):
        for image, expected in (
            (pe32_dll(machine=0x8664), "must target PE32 i386"),
            (pe32_dll(magic=0x20B), "must be PE32"),
            (pe32_dll(dll=False), "not marked as a DLL"),
            (b"not a PE image", "missing DOS header"),
        ):
            with self.subTest(expected=expected):
                self.dll.write_bytes(image)
                with self.assertRaisesRegex(package_frame.PackageError, expected):
                    self.package_to()
                self.dll.write_bytes(pe32_dll(embedded=self.source_version.encode("ascii")))

    def test_rejects_stale_or_unassociated_build_and_dirty_source(self):
        self.dll.write_bytes(pe32_dll(embedded=b"v0.9-old"))
        with self.assertRaisesRegex(package_frame.PackageError, "does not contain"):
            self.package_to()
        self.dll.write_bytes(pe32_dll(embedded=self.source_version.encode("ascii")))
        metadata_path = self.build / "meson-info/meson-info.json"
        metadata_path.write_text(json.dumps({
            "directories": {"source": str(self.root.parent), "build": str(self.build)},
        }))
        with self.assertRaisesRegex(package_frame.PackageError, "Meson build directory"):
            self.package_to()
        metadata_path.write_text(json.dumps({
            "directories": {"source": str(self.root), "build": str(self.build)},
        }))
        (self.build / "version.h").write_text('#define DXVK_VERSION "v0.9-old"\n')
        with self.assertRaisesRegex(package_frame.PackageError, "does not match"):
            self.package_to()
        (self.build / "version.h").write_text(f'#define DXVK_VERSION "{self.source_version}"\n')
        self._write("source.txt", "modified\n")
        run("git", "add", "source.txt", cwd=self.root)
        with self.assertRaisesRegex(package_frame.PackageError, "tracked worktree"):
            self.package_to()

    def test_rejects_invalid_manifest_closure_and_non_ancestor_upstream(self):
        self._write("src/d3d9/vr/openvr/bindings_frame_controller.json", json.dumps({
            "controller_type": "frame_controller", "bindings": {"/x": {"sources": [
                {"inputs": {"x": {"output": "/actions/not-defined"}}}
            ]}},
        }))
        run("git", "add", ".", cwd=self.root)
        run("git", "commit", "-m", "bad binding", cwd=self.root)
        # The ignored build header and DLL must now correspond to this new source identity.
        self.head = run("git", "rev-parse", "HEAD", cwd=self.root).stdout.strip()
        self.source_version = run("git", "describe", "--dirty=+", cwd=self.root).stdout.strip()
        (self.build / "version.h").write_text(f'#define DXVK_VERSION "{self.source_version}"\n')
        self.dll.write_bytes(pe32_dll(embedded=self.source_version.encode("ascii")))
        with self.assertRaisesRegex(package_frame.PackageError, "undefined actions"):
            self.package_to()

        self._write("src/d3d9/vr/openvr/bindings_frame_controller.json", json.dumps({
            "controller_type": "frame_controller", "bindings": {}
        }))
        run("git", "add", ".", cwd=self.root)
        run("git", "commit", "-m", "advance source", cwd=self.root)
        self.source_version = run("git", "describe", "--dirty=+", cwd=self.root).stdout.strip()
        (self.build / "version.h").write_text(f'#define DXVK_VERSION "{self.source_version}"\n')
        self.dll.write_bytes(pe32_dll(embedded=self.source_version.encode("ascii")))
        empty_tree = subprocess.run(
            ["git", "hash-object", "-w", "-t", "tree", "--stdin"], cwd=self.root,
            input="", check=True, stdout=subprocess.PIPE, text=True,
        ).stdout.strip()
        unrelated = run("git", "commit-tree", empty_tree, "-m", "unrelated upstream", cwd=self.root).stdout.strip()
        run("git", "update-ref", "refs/remotes/upstream/master", unrelated, cwd=self.root)
        with self.assertRaisesRegex(package_frame.PackageError, "not an ancestor"):
            self.package_to()

    def test_validates_pose_skeleton_and_haptic_action_references(self):
        path = self.root / "src/d3d9/vr/openvr/bindings_frame_controller.json"
        original = json.loads(path.read_text())
        for group in ("poses", "skeleton", "haptics"):
            with self.subTest(group=group):
                binding = json.loads(json.dumps(original))
                binding["bindings"]["/actions/game"][group] = [
                    {"output": "/actions/missing", "path": "/user/hand/left/pose/tip"}
                ]
                path.write_text(json.dumps(binding))
                with self.assertRaisesRegex(package_frame.PackageError, "undefined actions"):
                    package_frame.validate_actions(self.root)


if __name__ == "__main__":
    unittest.main()
