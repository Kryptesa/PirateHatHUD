# Pirate Hat HUD 1.2.3

Compatibility improvement for map route mods.

## Fixed

- Prevented PirateHatHUD's UI cache entries from being mistaken for additional
  minimap and world-map objects by whole-process scanners. These false objects
  could give WaypointRoute incorrect coordinates and a straight route from
  outside the map. The cache now stores image-relative offsets.

## Updating

Close the game and back up PirateHatHUD.ini. Import PirateHatHUD-1.2.3.zip into
CDUMM or DMM, enable the mod and ASI loader, then restart the game. Enable the
existing mod before importing the update, and keep only one copy of the ASI.

Existing settings remain compatible. No configuration changes are required.

## Validation and compatibility

Release build, architecture-check, formatting, all 31 CTest checks and all 25
research-tool tests passed. Release metadata and ZIP contents were validated.
The regression test reproduces three map-object candidates before the fix and
one after it, without increasing warm-cache memory reads.

The candidate was tested locally on game EXE 1.0.0.3576 with WaypointRoute 1.2.8
and CrimsonRoute 9.1.0 separately. The user reported normal behavior; paired logs
record successful HUD initialization and road routes. The WaypointRoute run also
records treasure state and minimap visibility transitions.

Confirmation on the reporter's originally failing setup remains pending. The
separate CrimsonRoute queue-binding failure is not established as fixed. A full
equipment-change cycle, hotkey-stop, load-order variations and other graphics
modes were not separately verified. The versioned release package has not
received an additional in-game run.

Windows x64 and DirectX 12. The Pirate King Hat must be equipped for detection.
The executable file version may differ from the public patch number.
