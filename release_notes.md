# Pirate Hat HUD 1.2.0

A chest icon and short sound announce nearby treasure detected by the Pirate King
Hat, including when helmets are hidden. The hat must be equipped.

## Changes since 1.1.0

- Added a sound-only mode: `[indicator] show_icon=0` hides the icon while keeping
  treasure notifications. PNG loading and graphics hooks are skipped in this mode.
- Added the missing `Present1` rendering path and resize recovery. A user confirmed
  that the icon now appears with Intel XeSS frame generation on an RTX 5080.
- Separated HUD command resources from screen buffers and expanded the submission
  ring to reduce skipped icon draws when the GPU is busy. Debug logs now identify
  a full ring. The reported flicker at 6x frame generation still needs an in-game check.

## Installation and updating

Close the game and back up PirateHatHUD.ini. Drag PirateHatHUD-1.2.0.zip into
CDUMM or DMM, enable Pirate Hat HUD on the ASI page, and make sure the ASI loader
is enabled. When updating, enable the existing mod before importing the archive.
Restart the game. Keep only one installed copy of the ASI.

Existing INI files remain compatible. If `show_icon` is missing, the icon remains
enabled by default. The default icon and sound are embedded.

## Sound-only mode

Add or update these entries in PirateHatHUD.ini and restart the game:

```ini
[indicator]
enabled=1
show_icon=0

[sound]
enabled=1
```

F9 still toggles notifications, while the icon stays hidden. `force_show=1` does
not override `show_icon=0`. Set `show_icon=1` and restart to restore the icon.
Notification volume, menu/minimap gating and the optional hat radius retain their
existing behavior. F10 stops the mod and restores an owned radius override.

## Reporting a problem

Set `[logging] level=debug` in PirateHatHUD.ini before launching, reproduce the
problem, and send the newest PirateHatHUD_*.log and your INI. Include the game
version, other installed mods and graphics settings, especially HDR, upscaler and
frame generation. For ReShade/RenoDX issues, also include ReShade.log from the
same launch, ReShade.ini and the active preset when applicable. Review files
before sharing; ReShade files may contain local paths. Logs stay local and the
mod does not upload them. Restore `level=info` afterwards.

## Compatibility and validation

Windows x64 and DirectX 12. Observation layout was previously checked on Crimson
Desert 2.03.02; the executable file version can differ from the public patch number.

The local Release build, architecture-check, formatting, all 31 CTest tests and
all 25 research-tool tests passed. Regression coverage includes HUD submissions
with a delayed GPU, native `Present1` interception, INI compatibility, sound without
graphics startup, and notification toggling in sound-only mode.

A user confirmed icon visibility with XeSS frame generation on an RTX 5080 using
the preceding test build. The supplied diagnostic session recorded game executable
version 1.0.0.2976. This does not establish compatibility across other GPUs or modes.

Remaining in-game checks: the versioned 1.2.0 package, sound-only playback and
F9/F10, camera-motion flicker at 6x frame generation, and a complete regression
of normal treasure detection, `force_show`, menu/minimap gating and graphics switches.

## Credits

- Icons by [Icons8](https://icons8.com/).
- Sound: "Subscribe alert - metal dings" by Roy's Noise, via [Uppbeat](https://uppbeat.io/).
- SafetyHook, Zydis and Dear ImGui developers; CDUMM and DMM developers.
