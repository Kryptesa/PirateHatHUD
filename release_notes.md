# Pirate Hat HUD 1.1.0

A chest icon and short sound announce nearby treasure detected by the Pirate King
Hat, including when helmets are hidden. The hat must be equipped.

## Changes since 1.0.3

- Added an optional hat detection radius override, disabled by default and
  independent of the icon and sound toggle. Accepts 1 to 1000 meters, including decimals.
- Saved radius edits apply during gameplay within about two seconds when the
  override was enabled at startup. Enabling or disabling it requires a restart.
- Stopping the mod restores the captured original radius if it still owns the
  same value. Unknown layouts and conflicting changes by other mods are left untouched.
- Standardized the INI comments and settings descriptions, including defaults,
  accepted values and when saved changes apply.

## Installation and updating

Close the game and back up PirateHatHUD.ini. Drag PirateHatHUD-1.1.0.zip into
CDUMM or DMM, enable Pirate Hat HUD on the ASI page, and make sure the ASI loader
is enabled. When updating, enable the existing mod before importing the archive.
Restart the game. Keep only one installed copy of the ASI.

Existing INI files remain compatible. If the new section is missing, the radius
override stays disabled. The default icon and sound are embedded.

## Optional detection radius

To double the default 15-meter range, add or update this section and restart:

```ini
[treasure]
enabled=1
radius=30
```

Once enabled, saved radius edits apply without restarting. Set enabled=0 and
restart to disable the override. F9 toggles only the icon and sound; F10 stops
the mod and restores the original radius if still owned by this mod.

The setting changes the hat's native detection, including its feather effect.
The hat must still be equipped, and treasure must be detectable by the game.
Only session memory changes; game files and saves remain untouched.

## Reporting a problem

Set [logging] level=debug in PirateHatHUD.ini before launching, reproduce the
problem, and send the newest PirateHatHUD_*.log and your INI. Include the game
version, other installed mods and graphics settings. For ReShade/RenoDX issues,
also include ReShade.log from the same launch, ReShade.ini and the active preset
when applicable. Review files before sharing; ReShade files may contain local paths.
Logs stay local and the mod does not upload them. Restore level=info afterwards.

## Compatibility and validation

Windows x64 and DirectX 12. Observation layout was previously checked on Crimson
Desert 2.03.02; the executable file version can differ from the public patch number.

The Release build, architecture-check, formatting, all 30 CTest tests and all 25
research-tool tests passed.

The range feature was tested in game on executable version 1.0.0.2976 with an
RTX 5080. Changing the range made the icon appear and disappear at a stationary
position; the sound also worked with the game focused. Logs confirmed saved
radius changes during gameplay and restoration when stopping with F10.

The final startup-only enable behavior and versioned 1.1.0 package have not had
a separate in-game test. Other GPUs and systems have not been verified for this release.

## Credits

- Icons by [Icons8](https://icons8.com/).
- Sound: "Subscribe alert - metal dings" by Roy's Noise, via [Uppbeat](https://uppbeat.io/).
- SafetyHook, Zydis and Dear ImGui developers; CDUMM and DMM developers.
