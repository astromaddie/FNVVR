---
name: Bug report
about: Report crashes, rendering issues etc.
title: ''
labels: ''
assignees: ''

---

Please describe your issue as accurately as possible. Include screenshots or videos when relevant.

If you use Windows, please check the following page: https://github.com/doitsujin/dxvk/wiki/Windows

### Software information
Name of the game, settings used etc. Include any mods or add-ons if applicable.

### Steam Frame / FNVVR installation
If this is a Frame tracking or input issue, follow [the install checks](https://github.com/astromaddie/FNVVR/blob/master/docs/FRAME_INSTALL.md)
and include:

- Frame release/build ID and `build-info.json` (if present):
- Actual SHA-256 of `d3d9.dll`, `FalloutNV.exe` and action/binding JSON files:
- Proton version, Steam launch options and active `dxvk.conf`:
- Any `DXVK_CONFIG_FILE` / `DXVK_VR_BACKEND` overrides or game-hook mods:
- Does SteamVR track elsewhere, do menu panels respond, and does the first-person gameplay camera track?
- Complete fresh `FalloutNV_d3d9.log`, with private paths redacted if needed:

`First poses received, head tracked: no` describes only the first pose sample;
it does not establish that tracking stayed inactive. Do not upload the game executable or assets.

### System information
- GPU:
- Driver:
- Wine version: 
- DXVK version: 

### Apitrace file(s)
- Put a link here

For instructions on how to use apitrace, see: https://github.com/doitsujin/dxvk/wiki/Using-Apitrace

### Log files
Please attach Proton or Wine logs as a text file:
- When using Proton, set the Steam launch options for your game to `PROTON_LOG=1 %command%` and attach the corresponding `steam-xxxxx.log` file in your home directory.
- When using regular Wine, use `wine game.exe > game.log 2>&1` and attach the resulting `game.log` file.
- On Windows, DXVK log files will usually appear next to the games exe or in its base folder.
