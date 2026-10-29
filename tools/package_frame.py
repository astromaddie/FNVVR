#!/usr/bin/env python3
"""Create a deterministic, allowlisted Steam Frame prerelease ZIP."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import sys
import zipfile


REPO_ROOT = Path(__file__).resolve().parents[1]
ARCHIVE_ROOT = "FNVVR-Frame-{version}"
FIXED_ZIP_TIME = (1980, 1, 1, 0, 0, 0)
VERSION_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9._+-]*\Z")
VERSION_HEADER_RE = re.compile(r'^#define DXVK_VERSION "([^"\r\n]+)"$', re.MULTILINE)


class PackageError(Exception):
    """An expected packaging validation failure."""


def run_git(root: Path, *args: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, "stderr", "") or str(exc)
        raise PackageError(f"git {' '.join(args)} failed: {detail.strip()}") from exc
    return result.stdout.strip()


def source_identity(root: Path) -> tuple[str, str, str, str]:
    merge_head = Path(run_git(root, "rev-parse", "--git-path", "MERGE_HEAD"))
    cherry_pick_head = Path(run_git(root, "rev-parse", "--git-path", "CHERRY_PICK_HEAD"))
    if not merge_head.is_absolute():
        merge_head = root / merge_head
    if not cherry_pick_head.is_absolute():
        cherry_pick_head = root / cherry_pick_head
    if merge_head.exists() or cherry_pick_head.exists():
        raise PackageError("refusing to package during an unfinished Git operation")
    tracked_changes = run_git(root, "status", "--porcelain", "--untracked-files=no")
    if tracked_changes:
        raise PackageError("refusing to package with tracked worktree or index changes")
    head = run_git(root, "rev-parse", "HEAD")
    tree = run_git(root, "rev-parse", "HEAD^{tree}")
    upstream = run_git(root, "rev-parse", "upstream/master^{commit}")
    if subprocess.run(
        ["git", "-C", str(root), "merge-base", "--is-ancestor", upstream, head],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ).returncode:
        raise PackageError("upstream/master is not an ancestor of HEAD")
    version = run_git(root, "describe", "--dirty=+")
    if version.endswith("+"):
        raise PackageError("git describe reports a dirty source tree")
    return head, tree, upstream, version


def validate_dll(path: Path) -> None:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise PackageError(f"cannot read DLL: {exc}") from exc
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise PackageError("DLL is not a valid PE image (missing DOS header)")
    pe_offset = int.from_bytes(data[0x3C:0x40], "little")
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise PackageError("DLL is not a valid PE image (missing PE signature)")
    coff = pe_offset + 4
    machine = int.from_bytes(data[coff:coff + 2], "little")
    optional_size = int.from_bytes(data[coff + 16:coff + 18], "little")
    characteristics = int.from_bytes(data[coff + 18:coff + 20], "little")
    optional = coff + 20
    if optional_size < 2 or optional + optional_size > len(data):
        raise PackageError("DLL has an invalid PE optional header")
    magic = int.from_bytes(data[optional:optional + 2], "little")
    if machine != 0x014C:
        raise PackageError(f"DLL must target PE32 i386 (machine 0x014c), got 0x{machine:04x}")
    if magic != 0x010B:
        raise PackageError(f"DLL must be PE32, got optional-header magic 0x{magic:04x}")
    if not characteristics & 0x2000:
        raise PackageError("PE image is not marked as a DLL")


def _build_root_for(root: Path, dll: Path) -> Path:
    for candidate in dll.resolve().parents:
        if (
            (candidate / "build.ninja").is_file()
            and (candidate / "meson-private" / "coredata.dat").is_file()
            and (candidate / "version.h").is_file()
            and (candidate / "meson-info" / "meson-info.json").is_file()
        ):
            try:
                info = json.loads((candidate / "meson-info" / "meson-info.json").read_text(encoding="utf-8"))
            except (OSError, ValueError) as exc:
                raise PackageError(f"invalid Meson build metadata: {exc}") from exc
            directories = info.get("directories", {})
            if (
                Path(directories.get("source", "")).resolve() == root.resolve()
                and Path(directories.get("build", "")).resolve() == candidate.resolve()
            ):
                return candidate
    raise PackageError("DLL must be inside a Meson build directory with generated version.h")


def validate_build_version(root: Path, dll: Path, source_version: str) -> Path:
    build_root = _build_root_for(root, dll)
    header = (build_root / "version.h").read_text(encoding="utf-8")
    match = VERSION_HEADER_RE.search(header)
    if not match or match.group(1) != source_version:
        raise PackageError("Meson generated version.h does not match current git describe version")
    if source_version.encode("ascii") not in dll.read_bytes():
        raise PackageError("DLL does not contain the current source version string")
    return build_root


def build_tool_versions(build_root: Path) -> dict:
    info_dir = build_root / "meson-info"
    try:
        meson_info = json.loads((info_dir / "meson-info.json").read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise PackageError(f"invalid Meson build metadata: {exc}") from exc
    versions = {}
    meson_version = meson_info.get("meson_version", {}).get("full")
    if meson_version:
        versions["meson"] = meson_version
    compiler_path = info_dir / "intro-compilers.json"
    if compiler_path.is_file():
        try:
            compilers = json.loads(compiler_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            raise PackageError(f"invalid Meson compiler metadata: {exc}") from exc
        versions["compilers"] = [
            {
                "machine": machine,
                "language": language,
                **{key: compiler[key] for key in ("id", "version", "full_version", "linker_id") if compiler.get(key)},
            }
            for machine, entries in sorted(compilers.items())
            for language, compiler in sorted(entries.items())
        ]
    return versions


def validate_actions(root: Path) -> None:
    openvr = root / "src/d3d9/vr/openvr"
    try:
        actions = json.loads((openvr / "actions.json").read_text(encoding="utf-8"))
        names = {entry["name"] for entry in actions["actions"]}
        defaults = actions["default_bindings"]
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise PackageError(f"invalid OpenVR action manifest: {exc}") from exc
    expected = {"frame_controller", "oculus_touch"}
    if {entry.get("controller_type") for entry in defaults} != expected or len(defaults) != 2:
        raise PackageError("actions.json must contain the Frame and Touch default bindings")
    for binding in defaults:
        filename = binding.get("binding_url")
        if filename not in {"bindings_frame_controller.json", "bindings_oculus_touch.json"}:
            raise PackageError(f"unexpected binding URL in actions.json: {filename!r}")
        try:
            data = json.loads((openvr / filename).read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            raise PackageError(f"invalid binding file {filename}: {exc}") from exc
        if data.get("controller_type") != binding["controller_type"]:
            raise PackageError(f"controller type mismatch in {filename}")
        def output_paths(value):
            if isinstance(value, dict):
                for key, child in value.items():
                    if key == "output" and isinstance(child, str):
                        yield child
                    else:
                        yield from output_paths(child)
            elif isinstance(value, list):
                for child in value:
                    yield from output_paths(child)

        outputs = set(output_paths(data.get("bindings", {})))
        missing = outputs - names
        if missing:
            raise PackageError(f"{filename} references undefined actions: {', '.join(sorted(missing))}")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _read_required(path: Path) -> bytes:
    try:
        return path.read_bytes()
    except OSError as exc:
        raise PackageError(f"required package file missing: {path}") from exc


def package(root: Path, dll_path: Path, version: str, output: Path) -> Path:
    if not VERSION_RE.fullmatch(version) or ".." in version:
        raise PackageError("version must be a safe filename identifier")
    if output.suffix.lower() != ".zip":
        raise PackageError("output path must end in .zip")
    root = root.resolve()
    dll_path = dll_path.resolve()
    if not dll_path.is_file():
        raise PackageError(f"DLL does not exist: {dll_path}")
    if dll_path.name.lower() != "d3d9.dll":
        raise PackageError("DLL must be named d3d9.dll")
    head, tree, upstream, git_version = source_identity(root)
    validate_dll(dll_path)
    build_root = validate_build_version(root, dll_path, git_version)
    validate_actions(root)

    sources = {
        "d3d9.dll": dll_path,
        "dxvk.conf": root / "packaging/steam-frame/dxvk.conf",
        "frame_launch.sh": root / "tools/frame_launch.sh",
        "fnvvr/actions.json": root / "src/d3d9/vr/openvr/actions.json",
        "fnvvr/bindings_frame_controller.json": root / "src/d3d9/vr/openvr/bindings_frame_controller.json",
        "fnvvr/bindings_oculus_touch.json": root / "src/d3d9/vr/openvr/bindings_oculus_touch.json",
        "README.md": root / "docs/FRAME_INSTALL.md",
        "FRAME_CONTROLS.md": root / "docs/FRAME_CONTROLS.md",
        "VR_ARCHITECTURE.md": root / "docs/VR_ARCHITECTURE.md",
        "CHANGELOG.md": root / "CHANGELOG.md",
        "LICENSE": root / "LICENSE",
    }
    # Ignored build products are expected; every source payload must be committed.
    run_git(root, "ls-files", "--error-unmatch", "--",
            *(str(path.relative_to(root)) for name, path in sources.items() if name != "d3d9.dll"))
    payload = {name: _read_required(path) for name, path in sources.items()}
    architecture = payload["VR_ARCHITECTURE.md"].decode("utf-8")
    replacements = {
        "[upstream changelog](../CHANGELOG.md)": "[upstream changelog](CHANGELOG.md)",
        "[install and troubleshooting guide](FRAME_INSTALL.md)": "[install and troubleshooting guide](README.md)",
    }
    for old, new in replacements.items():
        if architecture.count(old) != 1:
            raise PackageError(f"VR architecture source must contain exactly one {old!r} link")
        architecture = architecture.replace(old, new)
    payload["VR_ARCHITECTURE.md"] = architecture.encode("utf-8")
    file_info = {
        name: {"size_bytes": len(data), "sha256": sha256(data)}
        for name, data in sorted(payload.items())
    }
    metadata = {
        "version": version,
        "dxvk_version": git_version,
        "source_commit": head,
        "source_tree": tree,
        "upstream_commit": upstream,
        "tracked_worktree_clean": True,
        "target": "Windows PE32 i386 DLL under Steam Frame Proton/FEX",
        "build_tools": build_tool_versions(build_root),
        "files": file_info,
    }
    payload["build-info.json"] = (json.dumps(metadata, indent=2, sort_keys=True) + "\n").encode()
    checksum_entries = []
    for name, data in sorted(payload.items()):
        checksum_entries.append(f"{sha256(data)}  {name}\n")
    payload["SHA256SUMS"] = "".join(checksum_entries).encode("ascii")

    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    archive_root = ARCHIVE_ROOT.format(version=version)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(payload.items()):
            source = sources.get(name)
            mode = stat.S_IMODE(source.stat().st_mode) if source else 0o644
            info = zipfile.ZipInfo(f"{archive_root}/{name}", FIXED_ZIP_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | mode) << 16
            archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    zip_hash = sha256(output.read_bytes())
    output.with_suffix(output.suffix + ".sha256").write_text(f"{zip_hash}  {output.name}\n", encoding="ascii")
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", required=True, type=Path, help="built PE32 i386 d3d9.dll in a Meson build directory")
    parser.add_argument("--version", required=True, help="safe prerelease identifier used for the archive name")
    parser.add_argument("--output", required=True, type=Path, help="output ZIP path")
    args = parser.parse_args(argv)
    try:
        package(REPO_ROOT, args.dll, args.version, args.output)
    except PackageError as exc:
        print(f"package_frame.py: error: {exc}", file=sys.stderr)
        return 2
    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
