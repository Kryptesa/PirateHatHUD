# Pirate Hat HUD 1.2.6

## Changed

- Pirate Hat HUD now detects the known minimap, menu and audio settings layouts
  from live game objects. This lets the mod recognize those layouts on a new
  game build.
- The mod retries when data is unavailable during loading. The log records the
  chosen UI layouts and audio settings offset at info level.

## Updating

Close the game and back up PirateHatHUD.ini. Import PirateHatHUD-1.2.6.zip into
CDUMM or DMM, enable the mod and ASI loader, then restart the game.

## Validation

The Release build, architecture and formatting checks, all 31 CTest checks, and
all 25 research-tool tests passed.

On Crimson Desert 2.05.00 (game EXE 1.0.0.3663), the test build selected the
expanded minimap and menu layouts and audio settings offset +0x1078. The treasure
icon and sound both worked after loading a save. The optional radius override
was disabled during the test.
