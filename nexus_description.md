# Pirate Hat HUD

## Description

I made this mod for myself, both to practice modding and to solve a small annoyance:
I like playing with helmets hidden, but then I can't see the Pirate King Hat's
treasure detection cue.

This mod adds a chest icon and a short sound when the hat detects nearby treasure,
so I can keep helmets hidden and still know when it finds something.
The Pirate King Hat must be equipped.

Source code and bug reports: [GitHub repository](https://github.com/Kryptesa/PirateHatHUD).

## Installation instructions

**With CDUMM (Crimson Desert Ultimate Mods Manager):**

1. Close the game.
2. Drag the downloaded PirateHatHUD ZIP into CDUMM.
3. Enable Pirate Hat HUD on the ASI page and make sure the ASI loader is enabled.
4. Launch the game, load a save and use the Pirate King Hat near treasure.

**With another compatible ASI loader:**

1. Close the game and extract the archive.
2. Place `PirateHatHUD.asi` and `PirateHatHUD.ini` together in the loader's plugin
   folder. Keep the included license and notice files with the mod.
3. Launch the game.

The default icon and sound are built in; no separate PNG or WAV is needed.

To change settings, edit `PirateHatHUD.ini` beside the ASI. Most changes require a
game restart. Changes to `[sound] volume_percent` apply after saving, within about
one second, without restarting.

When updating, back up your INI and avoid installing a second copy of the ASI.
For CDUMM, enable the existing mod before importing an update, with the game closed.

## Main features

- **Treasure indicator:** a chest icon while the Pirate King Hat detects treasure.
- **Sound notification:** one short alert when detection becomes active.
- **HUD visibility:** the icon hides with the minimap, in menus and when the game's
  minimap draw state hides it during dialogue or cutscenes. It returns after a
  configurable delay, one second by default.
- **Audio controls:** notification volume is controlled by the game's overall
  volume, effects volume, and `[sound] volume_percent` in `PirateHatHUD.ini`.
  The config setting applies an additional multiplier from 0 to 100 percent:
  `100` keeps the volume set by the game sliders, `50` halves the sound amplitude,
  and `0` mutes it. Setting either game slider to zero also mutes notifications.
  Save config volume changes to apply them without restarting. Sound can also be
  disabled independently of the icon.
- **Quiet menu transitions:** alerts are suppressed in menus, while the minimap
  is hidden, while the mod is disabled and while the game is unfocused.
- **Customization:** adjustable icon position, size, return delay and sound cooldown.
  Optional `PirateHatHUD_treasure.png` and `PirateHatHUD_treasure.wav` files beside
  the ASI replace the built-in assets. Custom sounds must be mono or stereo PCM
  WAV, 8-bit or 16-bit, 8-192 kHz, up to 8 MiB. Install custom assets manually and
  restart after changing these files.
- **Hotkeys:** F9 toggles the icon and sound; F10 stops the mod for the current
  session. Both keys can be reassigned in the INI.
- **Display support:** SDR, scRGB and HDR10 output.

## Requirements

- A compatible ASI loader. [CDUMM](https://github.com/faisalkindi/CrimsonDesert-UltimateModsManager)
  provides mod installation and ASI loader management.
- The Pirate King Hat must be equipped for treasure notifications. For an icon-only
  setup check, set `[indicator] force_show=1`; this still requires a visible minimap
  and closed menus, and does not trigger sound.

The UI layout was checked against game version 2.03.02. Game updates may require
a mod update.

## Bug reports and troubleshooting

Report problems in the Nexus Bugs tab or open an issue on
[GitHub](https://github.com/Kryptesa/PirateHatHUD/issues). Please include steps to
reproduce the problem, what you expected and what actually happened.

The mod saves logs beside `PirateHatHUD.asi` as `PirateHatHUD_*.log` (normally in
the game's `bin64` folder with CDUMM). To capture more detail:

1. Open `PirateHatHUD.ini` beside the ASI and change `level` under `[logging]` to
   `debug`:

   ```ini
   [logging]
   level=debug
   ```

2. Save the file, restart the game and reproduce the problem.
3. Attach the newest log from that session, your INI, the mod and game versions,
   your ASI loader, and any other installed mods. Include graphics settings such
   as HDR, DLSS and Frame Generation; a screenshot or short video also helps.
4. Afterwards, set `level=info` and restart to return to normal logging.

Logs rotate automatically: by default, the mod keeps three files of up to 5 MB
each. Copy the relevant logs before launching the game again, so they aren't
removed by rotation. If no log was created, mention that in the report and check
that the mod and ASI loader are enabled.

If the icon never appears, try `[indicator] force_show=1`, restart, load a save,
make sure the minimap is visible, close menus and wait a second. This checks the
overlay without needing nearby treasure. Set it back to `0` afterwards; normal
detection requires the Pirate King Hat to be equipped.

## Shout outs

- [Icons8](https://icons8.com/) for the icon.
- Roy's Noise for "Subscribe alert - metal dings", sourced from
  [Uppbeat](https://uppbeat.io/).
- The developers of [SafetyHook](https://github.com/cursey/safetyhook),
  [Zydis](https://github.com/zyantific/zydis) and
  [Dear ImGui](https://github.com/ocornut/imgui) for the libraries used by the mod.
- The [CDUMM](https://github.com/faisalkindi/CrimsonDesert-UltimateModsManager)
  developers for the mod manager and ASI loader support.
