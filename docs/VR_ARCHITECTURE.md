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
| Installed game identity | Download completed through Steam: AppID 22380, build 1510068, executable 1.4.0.525, 7,273,048,270 bytes on disk |
| Installed executable SHA-256 | `3a87f92f011e5dc9179ddf733cf08be2b39ea6e5b7a8a9e3a9a72dafcc1b104d` |
| Mod installed on Frame | DLL hash verified against the Mac baseline; action JSON files installed; `d3d9.vrBackend = off` remains the last confirmed configuration |
| Flat Vulkan initialization | Passed under Proton Experimental ARM64/FEX: Turnip Adreno 750, device creation and a 1280x720, four-image BGRA8 Vulkan swapchain; this proves graphics initialization, not visual correctness or gameplay |
| Stereo/head tracking/controllers on Frame | Pending: the Frame went offline before the OpenVR configuration/90 Hz request could run |
| Sustained 90 Hz / 120 Hz | Not measured |
| Gameplay and comfort acceptance | Pending |
| Audio cleanup | Nine host tests passed; real Frame success, timeout and SIGTERM runs restored both named outputs' exact mute and raw per-channel volume states; no watchdog timers remained |

The built DLL, hash manifest and compact build summary are retained locally in
`artifacts/frame-baseline/` (ignored by Git). They contain no game assets. Source
and host tests do not establish runtime compatibility or headset acceptance.

The game's main process renames itself to `Main`; find it by its full executable
arguments, not just its process name. The first cold graphics startup needed a
longer observation window: device creation appeared by the 45-second snapshot and
the swapchain was present by the 75-second snapshot. The completed 105-second run
was stopped through Steam. Shorter runs were not evidence of a startup failure.

The device's FNVVR files live beside `FalloutNV.exe` in the Steam install. The
launcher wrapper and audio guard live in
`~/.local/share/fnvvr-frame/tools/`. Rollback metadata and the original generated
preferences are in `~/.local/state/fnvvr-frame/baseline-install.json` and
`~/.local/state/fnvvr-frame/FalloutPrefs.before-frame.ini`. Existing saves were not
modified by the test tools. The wrapper keeps Steam's compatibility command and
changes its final launcher executable to the game executable.

An on-device Win32 adapter probe confirmed that the game loads our `d3d9.dll`.
DXVK reports Qualcomm as the fallback AMD RX 6700 XT adapter; the real Vulkan
device remains Adreno 750. The initial preferences had zero adapter GUID fields
and no display dimensions. Preferences were backed up, then matched to the actual
probe identity and a 1280x720 window before the successful initialization run.

Network access is the current blocker. No OpenVR/90 Hz result exists yet. Temporary
on-device probe files/logs still need removal once the Frame is reachable; keep
the installed DLL, scripts, original-preferences backup and rollback metadata.
Temporary Mac diagnostic logs, raw captures, probe binaries and Python caches
were consolidated into the compact local summary and removed, recovering 556 KiB.

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

Recheck idle state and the shared Frame booking, then enable `openvr` in
`dxvk.conf` and request a 90 Hz per-app rate. The last attempt to do so failed at
SSH connection time, so do not assume either setting was applied. Use a Steam
launch and allow for cold startup. Preserve the original preference backup and
all saves throughout testing.

Verify stereo, head tracking, menu interaction and controller bindings before
performance work. Compare repeatable interior and busy exterior scenes at fixed
resolution and settings, with warm-up and at least three repeats. Record CPU/GPU
timings, frame-time tails, missed deadlines and reprojection separately at 90 Hz
and 120 Hz. Use resolution comparisons to distinguish GPU load from CPU/translation
cost. Only claim a target after sustained gameplay measurements and worn checks.

## Test audio and launch helpers

`tools/frame_audio_guard.py` runs on the Frame from a stable file path. It records
the default sink and `frame-speaker-eq-sink`, registers a systemd user watchdog
before muting, then restores and reads back the saved outputs on completion,
failure or interruption. The watchdog also covers a killed guard process. It
requires `pactl`, a working user systemd instance and both named outputs. Failed
restoration retains a mode-0600 state file and prints the exact recovery command.

```sh
python3 tools/frame_audio_guard.py run --timeout 60 -- COMMAND ARGUMENTS
python3 tools/frame_audio_guard.py restore STATEFILE
python3 -m unittest tests.test_frame_audio_guard -v
```

This guard owns audio cleanup only. A test that launches a game through Steam
must cancel pending launches, stop its own game and verify process exit before
the guarded command ends. Steam's stop call takes a string GameID:
`SteamClient.Apps.TerminateApp("22380", false)`. An integer first argument is
rejected asynchronously, so a returned `null` alone is not shutdown evidence.

`tools/frame_launch.sh` preserves argument boundaries, including paths containing
spaces, while replacing `FalloutNVLauncher.exe` with `FalloutNV.exe`. Use it around
Steam's `%command%` with `WINEDLLOVERRIDES="d3d9=n,b"`; `DXVK_LOG_LEVEL=info` enables
the baseline diagnostics. Its Bash syntax and argument-preservation check passed.

`tools/frame_adapter_probe.c` is the small Win32 diagnostic used to obtain the
actual loaded DLL path, adapter description and GUID fields. It was compiled with
`i686-w64-mingw32-gcc -O2 -static tools/frame_adapter_probe.c -ld3d9 -o build-adapter-probe.exe`
and run successfully through the game's Steam compatibility command with VR off.
It writes `fnvvr-adapter.txt` to its working directory. An ad hoc Proton command
outside Steam did not reproduce the launch environment and failed; use Steam for
this probe as well as the game.
