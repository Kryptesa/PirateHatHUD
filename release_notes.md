# Pirate Hat HUD 1.0.0

A chest icon and short sound announce nearby treasure detected by the Pirate King
Hat, including when helmets are hidden. The hat must be equipped.

## Changes since 0.5.0

- Fixed F9 and F10 by receiving Windows hotkey messages while the game is focused.
  F9 toggles the icon and sound; F10 stops the mod until the next game launch.
  Hotkeys are released on focus loss and shutdown. Unavailable bindings use polling.
- The mod starts only in CrimsonDesert.exe, avoiding a second instance and error
  log in the game's crash handler.
- Sound follows the game's master and effects volume sliders. Muted or unreadable
  settings suppress playback without replaying missed notifications.
- Added [sound] volume_percent (0–100, default 100) as an additional volume control.
  Saved changes apply within about one second without restarting the game.
- Startup logs include the executable file version and EXE/ASI names without
  installation paths or process IDs. Successful hotkey registration uses debug logging.
- Installation through DMM is documented alongside CDUMM.
- Organized observation internals and shared guarded memory access helpers.

## Installation and updating

Close the game and back up PirateHatHUD.ini. Drag PirateHatHUD-1.0.0.zip into
CDUMM or DMM, enable Pirate Hat HUD on the ASI page, and make sure the ASI loader
is enabled. When updating, enable the existing mod before importing the archive.
Restart the game. Keep only one installed copy of the ASI.

The default icon and sound are embedded; no separate PNG or WAV is needed.
Existing settings without volume_percent use 100. See the [README](README.md)
for manual installation, customization and troubleshooting.

## Compatibility and validation

Windows x64 and DirectX 12. The UI layout was checked on Crimson Desert 2.03.02;
game updates or different executable builds may require a mod update.
The Windows executable file version in the log can differ from the public patch number.
SDR, scRGB and HDR10 rendering are supported. For HDR10, the mod needs to observe
its color-space selection; try switching HDR off and back on if the icon looks wrong.

The Release build, architecture-check, formatting check, all 27 CTest tests and
all 25 research-tool tests passed.
The user confirmed the final candidate works in game, including the previously listed
checks for treasure detection, icon/sound toggling, F10 shutdown, focus changes,
menu/minimap gating and startup logging. Installation and operation were confirmed
with CDUMM and DMM. The game version and graphics settings for this final validation
were not recorded. The versioned 1.0.0 package has not had a separate in-game test;
testing on another PC and across all graphics configurations remains unverified.

## Credits

- Icons by [Icons8](https://icons8.com/).
- Sound: "Subscribe alert - metal dings" by Roy's Noise, via [Uppbeat](https://uppbeat.io/).
- SafetyHook, Zydis and Dear ImGui developers; CDUMM and DMM developers.
