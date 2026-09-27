# Pirate Hat HUD 0.5.0

Treasure detection now plays a short sound as well as showing the chest icon.
The default icon and sound are embedded in the ASI; no extra asset files are needed.

## What's new

The sound plays once when the Pirate King Hat starts detecting treasure. The first
active state after loading also notifies once when gameplay becomes ready. It does
not repeat while the perk stays active or on subsequent exits from menus.

Sound is suppressed while the mod is disabled, the minimap is hidden or unavailable,
a menu is open or unavailable, or the game is unfocused.

F9 now toggles both the icon and sound. To turn off sound independently, set
`enabled=0` under `[sound]` in `PirateHatHUD.ini`. `cooldown_ms` sets the minimum
interval between notifications (0–60000 ms; default 1000).

To use your own sound, manually place `PirateHatHUD_treasure.wav` beside the ASI.
Supported files are mono or stereo PCM WAV, 8-bit or 16-bit, 8–192 kHz, up to 8 MiB.
Remove the override to restore the built-in sound, and restart the game after changes.

## Updating

Back up your settings, close the game and import `PirateHatHUD-0.5.0.zip` into
CDUMM or DMM with the existing mod enabled. Restart the game. Existing INI files without a
`[sound]` section use the defaults: sound enabled, 1000 ms cooldown.

F10 stops the mod until the next game launch. See the [README](README.md) for
installation, settings and troubleshooting.

## Credits

- Icons by [Icons8](https://icons8.com/).
- Sound: "Subscribe alert - metal dings" by Roy's Noise, via [Uppbeat](https://uppbeat.io/).

## Compatibility and validation

Windows x64 and DirectX 12. The UI memory layout is based on Crimson Desert 2.03.02;
game updates may require new offsets or instruction patterns.
SDR, scRGB and HDR10 output are supported. For HDR10, the mod needs to see the game's
color-space change; if the icon looks wrong, try switching HDR off and back on.

Release validation includes architecture checks, formatting and all 21 CTest tests.
In-game feedback confirmed sound playback, improved icon responsiveness and the
first notification after loading. The game version and graphics settings for that
feedback were not recorded.
