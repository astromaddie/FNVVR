# Steam Frame: current architecture and verification

## Target and route

Run Fallout: New Vegas VR entirely on Steam Frame. The minimum performance target
is sustained 90 Hz (11.11 ms per frame); the preferred target is 120 Hz (8.33 ms).
These are application frame targets, not compositor refresh or reprojected FPS.
Neither target has been demonstrated for this game.

The first prototype keeps the upstream 32-bit Windows game and `d3d9.dll` together
under Frame's Proton/FEX compatibility path. This is not a native ARM64 engine
port. The VR layer patches fixed addresses in FalloutNV.exe 1.4.0.525; recompiling
the DLL alone for ARM64 would not convert the game or its engine hooks.

The Mac needs the source and cross-build tools. The game's assets can stay on the
Frame. A local copy of the executable may later help with disassembly or checking
hook addresses, but a second full game download is unnecessary. Do not commit or
upload the game's executable, assets, saves, or account data.

## Rendering and input

- The game runs inside Proton/FEX and calls the fork's D3D9 front end.
- DXVK translates D3D9 to Vulkan. `VrStereoRenderer` hooks the game's swap path,
  renders each eye, and passes the Vulkan-backed images through `VrD3D9Bridge`.
- The real-headset backend uses OpenVR and its compositor; the emulator backend
  supports host development. Compatibility between the translated 32-bit client,
  Vulkan device and Frame's SteamVR runtime remains to be checked on the device.
- Menus/HUD use panels. The shipped action manifest provides Oculus Touch default
  bindings; Frame controller poses, buttons and prompts need direct verification.
- CPU translation/draw submission and the two eye passes are separate potential
  costs. Foveation or reduced resolution must not be assumed to solve CPU costs.

## Verified baseline, 2026-10-25

Compiled source: `c2b0eaa4fbdaa4526ea73ca07e0b3485e0c642a6` from
<https://github.com/df4l/FNVVR>.

| Gate | State and evidence |
| --- | --- |
| Mac cross-build | Passed, unmodified upstream code; 397 Ninja steps completed |
| DLL identity | PE32 Windows DLL, Intel 80386, 21,181,524 bytes |
| DLL SHA-256 | `056d25784f6fd6002b4fad029287fc2e89afbb584a8037d13f528274c2d04518` |
| Native VR unit tests | Passed: 1/1 Meson test target (`dxvk:vr`), zero failures |
| Installed game identity | Pending: read-only check of the Steam libraries registered at `frame.local` and filename search under the home directory and common mount roots found library metadata, but no New Vegas installation manifest or `FalloutNV.exe`; installation location/launcher needs confirmation |
| Mod installed on Frame | Not performed |
| Stereo/head tracking/controllers on Frame | Not tested |
| Sustained 90 Hz / 120 Hz | Not measured |
| Worn gameplay and comfort | Not tested |

The built DLL, hash manifest and compact build summary are retained locally in
`artifacts/frame-baseline/` (ignored by Git). They contain no game assets. Source
and host tests do not establish runtime compatibility or headset acceptance.

## Reproduce the host baseline

Requires Python 3 with venv support, Ninja, glslang/glslangValidator and the
POSIX-thread i686 MinGW toolchain named by `build-win32.txt`.

```sh
git submodule update --init --recursive
python3 -m venv build-tools
build-tools/bin/python -m pip install meson==1.12.1
build-tools/bin/meson setup --cross-file build-win32.txt --buildtype release build.32
ninja -C build.32
build-tools/bin/meson test -C build.32 --print-errorlogs
```

The baseline used Python 3.14.6, Meson 1.12.1, Ninja 1.13.2,
i686 MinGW GCC 16.2.0 and Apple Clang 21.0.0 for native tests. No source changes
were needed. The temporary environment and build tree were removed after saving
the DLL and summary; this and log cleanup recovered 64,084 KiB (62.6 MiB).

## First device evidence required

Confirm the installation and executable version, then preserve existing mod/config
files and saves before adding the baseline. Use a Steam launch under the shared
Frame booking rules. Any temporary audio mute must save and restore the exact
affected outputs, including the speaker EQ sink, and verify restoration.

Verify stereo, head tracking, menu interaction and controller bindings before
performance work. Compare repeatable interior and busy exterior scenes at fixed
resolution and settings, with warm-up and at least three repeats. Record CPU/GPU
timings, frame-time tails, missed deadlines and reprojection separately at 90 Hz
and 120 Hz. Use resolution comparisons to distinguish GPU load from CPU/translation
cost. Only claim a target after sustained gameplay measurements and worn checks.
