# Steam Frame architecture and build verification

## Source and distribution

The Frame fork integrates `df4l/FNVVR` master through
`6b1a47d0` (2026-10-29), retaining the native Frame controller binding and Steam
launcher wrapper. This includes upstream v0.9.0 and later master changes: physics
grabs, holsters and the hotkey wheel, ammunition selection, per-eye visibility,
throwing and weapon effects, plus furniture and manual-reload fixes. See the
[upstream changelog](../CHANGELOG.md) for details.

The [Frame prerelease](https://github.com/astromaddie/FNVVR/releases/tag/frame-2026.10.29.1)
is a separate distribution. Its `build-info.json` identifies the exact fork
commit, upstream commit, target and payload hashes. Use the
[install and troubleshooting guide](FRAME_INSTALL.md) to compare an actual game
installation against that metadata. Source synchronization alone does not update
an installed DLL. The older October 25 binary is identified below.

## Runtime route

Fallout: New Vegas runs entirely on Frame using its original 32-bit Windows
executable under Proton/FEX. This is a standalone compatibility route, not a
native ARM64 engine rebuild. The VR layer patches fixed addresses targeting
FalloutNV.exe **1.4.0.525**; an ARM64 DLL alone would not convert those x86 hooks.

The game calls the fork's D3D9 front end. DXVK translates it to Vulkan, and
`VrStereoRenderer` renders the game's world for each eye through
`VrD3D9Bridge`. The OpenVR backend shares those Vulkan images with SteamVR.
Menus and HUD use separate overlay panels. Main-menu panel behavior is therefore
not a complete test of the first-person gameplay camera.

OpenVR's HMD poses drive the per-eye game camera and the player's look hook.
The backend reads validity on every pose sample, but its `head tracked: yes/no`
message records only the first successful sample. Initial standby can produce
`no` followed by normal tracking; it is not a continuous tracking monitor.
The source comparison from `c2b0eaa4` to `6b1a47d0` found no change to HMD pose
acquisition or pose-to-camera application. The newer per-eye frustum work fixes
visibility rather than pose tracking. A reported difference needs the actual
DLL/game hashes, effective configuration, launch command and complete startup log.

The native `frame_controller` profile precedes the Touch fallback. It uses
Frame's `thumbstick` inputs and the physical right controller's A/B/X/Y buttons.
Both grips, triggers, aim poses, skeletons and haptics have explicit routes.
This avoids Touch auto-remapping, which can send X/Y actions to the left D-pad or
multiple right buttons through the Touch B mapping. See [controls](FRAME_CONTROLS.md).

## Build and runtime evidence

| Evidence | Scope |
| --- | --- |
| Current release | Exact source, DLL hash and package identity are in the release's `build-info.json` and `SHA256SUMS`; the packager requires a clean source tree and a matching compiled version marker |
| Historical Frame initialization | The October 25 DLL initialized Vulkan on Turnip Adreno 750 and OpenVR through Proton Experimental ARM64; menu panels and action-manifest registration succeeded |
| Historical native binding check | SteamVR loaded the Frame binding directly; all 28 routes matched the installed profile, and the game logged that VR controllers reached its gamepad input |
| Refresh preferences | The earlier runs reported 90 Hz, then 120 Hz with a 2160x2160 recommended eye size; these are runtime settings, not measured gameplay FPS |
| Performance target | Sustained 90 Hz minimum (11.11 ms), ideally 120 Hz (8.33 ms); repeatable gameplay timing, misses and reprojection remain separate measurements |

The synchronized source passed a 418-step MinGW release build, the native
`dxvk:vr` test target (one target containing the VR cases), and all 14 Python
tests (nine audio-guard and five packaging tests). Release packaging requires a
final rebuild of the version marker from the clean committed source. These host
checks do not measure gameplay frame rate or diagnose another installed copy.

## Reproduce and package

Requires Python 3, Meson, Ninja, glslang/glslangValidator and the POSIX-thread
`i686-w64-mingw32` compiler named in `build-win32.txt`.

```sh
git submodule update --init --recursive
python3 -m venv build-tools
build-tools/bin/python -m pip install meson==1.12.1
build-tools/bin/meson setup --cross-file build-win32.txt --buildtype release build.32
ninja -C build.32
build-tools/bin/meson test -C build.32 --print-errorlogs
python3 -m unittest discover -s tests -v
python3 tools/package_frame.py --help
```

Package from a clean committed source tree. The packager checks the compiled
version marker, PE32/i386 DLL format, bindings and payload allowlist. It includes
an active OpenVR preset and no game executable, assets, saves, OpenVR DLL or
Vulkan driver. Proton supplies `openvr_api_dxvk.dll`; a local `openvr_api.dll`
takes precedence, so record the startup log's actual loaded library when comparing
installations. The game and its data can stay on Frame; the Mac only needs source
and build tools.

## October 25 reference identities

These identify the earlier test setup, not the current release or every valid
modified game executable:

| Item | Identity |
| --- | --- |
| Compiled upstream source | `c2b0eaa4fbdaa4526ea73ca07e0b3485e0c642a6` |
| Earlier DLL | PE32/i386, 21,181,524 bytes; SHA-256 `056d25784f6fd6002b4fad029287fc2e89afbb584a8037d13f528274c2d04518` |
| Steam game | AppID 22380, build 1510068; executable version 1.4.0.525 |
| Reference game SHA-256 | `3a87f92f011e5dc9179ddf733cf08be2b39ea6e5b7a8a9e3a9a72dafcc1b104d` |

The local Frame install's original preference backup and rollback metadata live
under `~/.local/state/fnvvr-frame/`. Keep those and user saves when updating.
Generated build trees and raw diagnostics are disposable after retaining current
artifacts, compact identity records and reproduction details.

## Automated Frame test operations

Book the shared Frame immediately before an install or launch, check current
proximity and scene apps, and release after cleanup. Launch through Steam.
The game process can rename itself to `Main`; identify it by full executable
arguments rather than process name. Cold startup can take longer than a short
smoke window because of first-run setup and shader compilation.

`tools/frame_audio_guard.py` snapshots the default speaker output and
`frame-speaker-eq-sink`, registers a restore watchdog before muting, then restores
and verifies the exact named outputs on completion, failure or interruption.
This owns audio only: the caller must stop its own Steam game before returning.
Steam's cancel/stop methods require string GameID `"22380"`; a returned `null`
is not proof that the process exited. Never kill Gamescope or SteamVR globally.

`tools/frame_adapter_probe.c` is an optional Win32 adapter diagnostic. DXVK can
report Qualcomm through its fallback AMD adapter identity; identify the actual
GPU from the Vulkan device log. Do not distribute a private machine's adapter
GUIDs or preferences as universal Frame defaults.
