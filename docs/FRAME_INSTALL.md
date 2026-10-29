# FNVVR on Steam Frame

Use the [Frame release](https://github.com/astromaddie/FNVVR/releases/tag/frame-2026.10.29.1)
from `astromaddie/FNVVR`. This fork includes upstream master through `6b1a47d0`,
plus the native Frame controller profile. The archive's `build-info.json` records
the exact source revision, upstream revision, target and payload hashes.

This is a **32-bit Windows D3D9 mod running locally under Frame's Proton/FEX**.
It is not an ARM64 executable or APK. Fallout: New Vegas must already be installed
through Steam on the Frame. The game hooks target **FalloutNV.exe 1.4.0.525**;
other executable revisions or mods that replace those hooks need separate checks.
No game executable, assets, saves, Vulkan driver or OpenVR runtime is distributed.

## Install or update

1. Download the Frame ZIP and its `.sha256` file from the release page. In the
   download directory, run `sha256sum -c FNVVR-Frame-frame-2026.10.29.1.zip.sha256`.
   Extract the ZIP and, inside its top-level folder, run `sha256sum -c SHA256SUMS`
   before modifying any packaged file. On macOS, use `shasum -a 256 -c` instead.
2. Exit New Vegas. In Steam, use **Manage > Browse local files** to locate the
   directory containing `FalloutNV.exe`. Back up any existing mod DLL,
   `dxvk.conf`, launch options and `fnvvr` JSON files before replacing them.
   Preserve `fnvvr/vr_options.ini`, all saves and your other mod files.
3. Copy `d3d9.dll`, `frame_launch.sh`, `build-info.json` and the JSON files from
   `fnvvr/` into their matching locations next to `FalloutNV.exe`. Keep both
   `bindings_frame_controller.json` and the Touch fallback with `actions.json`.
   Keep the archive and its checksum manifest for verification.
4. For a fresh setup, copy the supplied `dxvk.conf` next to `FalloutNV.exe`.
   For an existing configuration, merge these active lines and remove conflicting
   duplicates while retaining your other settings:

   ```ini
   d3d9.vrBackend = openvr
   d3d9.hideNvidiaGpu = False
   ```

   `DXVK_VR_BACKEND` in Steam launch options overrides the backend in this file.
   There is no `openxr` backend or separate head-tracking-enable setting.
5. The Frame setup uses Proton's `openvr_api_dxvk.dll`. A local `openvr_api.dll`
   takes precedence. If a previous generic package put one beside the game, back
   it up outside the game directory to use Proton's runtime. Leave the copies
   inside Steam and Proton installations intact. The startup log identifies the
   library actually loaded.
6. Make `frame_launch.sh` executable (`chmod +x frame_launch.sh`). For Steam's
   default internal-library location, set New Vegas launch options to:

   ```sh
   DXVK_LOG_LEVEL=info WINEDLLOVERRIDES="d3d9=n,b" "/home/steamos/.local/share/Steam/steamapps/common/Fallout New Vegas/frame_launch.sh" %command%
   ```

   Adjust the quoted path if Steam's actual game directory differs. Preserve any
   other intentional launch settings; use one launcher wrapper. This wrapper
   keeps Steam's Proton command and bypasses `FalloutNVLauncher.exe` in favor of
   `FalloutNV.exe`.
7. Launch New Vegas **from Steam on the Frame**. Allow for first-run setup and
   shader compilation. In a first-person gameplay scene, turn and lean gently
   to check the world camera; then check menus and [controller actions](FRAME_CONTROLS.md).
   A floating main-menu panel alone does not test gameplay head tracking.

## Check which build is installed

In a terminal on the Frame, change into the actual game directory:

```sh
sha256sum d3d9.dll FalloutNV.exe fnvvr/actions.json fnvvr/bindings_frame_controller.json
python3 -m json.tool build-info.json
```

Compare the DLL and binding hashes with the corresponding file entries in the
downloaded release's `build-info.json`. The source revision in that metadata is
useful only when the installed payload hashes match it. An old DLL with a newly
copied metadata file is still an old DLL. A locally edited `dxvk.conf` can differ
from the packaged preset without making the DLL stale.

The earlier October 25 build installed during Frame development had DLL SHA-256
`056d25784f6fd6002b4fad029287fc2e89afbb584a8037d13f528274c2d04518`, compiled
from upstream `c2b0eaa4`. That identifies the old build; it is not the new release's
expected hash. The historical Steam executable hash is recorded in the
[architecture notes](VR_ARCHITECTURE.md); it is a reference, not a universal
requirement for every modified executable.

## If head tracking differs between installations

First establish the actual symptom: does SteamVR track the headset elsewhere,
does the floating menu move, or does the first-person world fail to follow head
movement? Menus and the stereo gameplay camera use different rendering paths.

With `DXVK_LOG_LEVEL=info`, retain the complete fresh `FalloutNV_d3d9.log` from
the game directory. This command extracts useful markers without replacing the
full log:

```sh
grep -E "VR: (Using backend|Loaded |OpenVR initialized|Headset |Tracking system |First poses received|WaitGetPoses failed|The head aims|The player's look handler|Stereo rendering started|The game's frame code|First frame handed to the compositor)" FalloutNV_d3d9.log
```

| Evidence | What to check |
| --- | --- |
| No `VR:` markers | Confirm that this DLL is actually loaded and VR is enabled. The generic source `dxvk.conf` is a commented template; the Frame package includes an active OpenVR preset. |
| `Using backend 'Emulator'` or unknown backend | Check `DXVK_VR_BACKEND`, the effective `dxvk.conf` and the executable/working directory. Frame needs `openvr`. |
| `Loaded ...`, then OpenVR initialization error | Check the actual OpenVR library, SteamVR and Proton version before investigating game camera hooks. |
| `First poses received, head tracked: no` | This logs only the first successful pose sample. An idle startup can produce `no` and later track normally; this line is not a continuous tracking test. |
| `The player's look handler was not found, the head does not aim` | The expected game hook did not match. Compare executable revision and interfering mods. |
| `The game's frame code was not found, stereo rendering is disabled` | The stereo hooks did not install; compare the game executable and other hook-changing mods. |
| `Stereo rendering started` and first frame handed to compositor | Stereo rendering/submission started. Check actual head movement in gameplay separately. |
| `frame_hmd` has no configured binding | This warning alone does not establish a head-pose failure. Check OpenVR poses and gameplay camera markers; hand-controller binding errors concern `frame_controller`. |

When reporting the problem, include the release/build ID, actual DLL and game
hashes, Steam launch options, Proton version, active `dxvk.conf`, any
`DXVK_CONFIG_FILE`/`DXVK_VR_BACKEND` overrides, relevant game mods and the full
DXVK log. Do not upload the game executable or assets. Redact account/home paths
if necessary. Retain existing configuration and saves while comparing setups.

Upstream changes between `c2b0eaa4` and `6b1a47d0` include per-eye visibility
fixes, but no change to the HMD pose acquisition or pose-to-camera path was found
in that comparison. Updating removes version uncertainty; it does not by itself
explain or prove a fix for another headset's reported tracking problem.

## Build a Frame package from source

Build and test the 32-bit DLL using the repository's Meson/Ninja recipe, then
use `tools/package_frame.py --help` for the packaging command. Publish from a
clean committed source tree. The packager checks DLL provenance, architecture,
binding closure and an explicit payload allowlist, then emits build metadata,
per-file hashes and an archive checksum. It never packages game data or saves.
