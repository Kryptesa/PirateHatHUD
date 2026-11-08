# Pirate Hat HUD 1.2.2

Fix for false treasure notifications without the Pirate King Hat equipped.

## Fixed

- Filtered unrelated object-detection effects out of the shared game hooks so
  they cannot trigger the treasure sound or icon.

## Updating

Close the game and back up PirateHatHUD.ini. Import PirateHatHUD-1.2.2.zip into
CDUMM or DMM, enable the mod and ASI loader, then restart the game. Enable the
existing mod before importing the update, and keep only one copy of the ASI.

Existing settings remain compatible. No configuration changes are required.

## Validation and compatibility

Release build, architecture-check, formatting, all 31 CTest checks and all 25
research-tool tests passed. Release metadata and ZIP contents were validated.
The installed candidate received preliminary user confirmation on game EXE
1.0.0.3576; its log records treasure activation/deactivation and menu/minimap
transitions. A complete equipment-change cycle, hotkeys and settings changes
were not separately documented. The versioned release package has not been
independently tested in game.

Windows x64 and DirectX 12. The Pirate King Hat must be equipped for detection.
The executable file version may differ from the public patch number.
