# Pirate Hat HUD

A small HUD mod for Crimson Desert. When the Pirate King Hat detects treasure nearby,
a chest icon appears on screen and a short sound plays when detection becomes active.
The icon hides while the minimap is off or a menu is open,
and returns after a short delay when you get back to the game.

**Current version: 0.5.0** · Windows x64 · DirectX 12

## Installation

With [CDUMM](https://github.com/faisalkindi/CrimsonDesert-UltimateModsManager), drag
`PirateHatHUD-0.5.0.zip` into the manager, enable the mod on the ASI page, and make
sure the ASI loader is enabled. Restart the game.

If you use another ASI loader, put `PirateHatHUD.asi` and `PirateHatHUD.ini` together
in its plugin folder. The default icon and sound are built into the mod, so you don't
need to copy a PNG or WAV.

When updating, back up your INI first and check that you don't have a second copy
of the ASI installed. Older versions used `config.ini`; move those settings into
`PirateHatHUD.ini`. The old filename is still accepted if the new file is missing.

For CDUMM updates, close the game and enable the mod in the manager before importing
the new archive. Updating a disabled mod can leave an extra ASI behind when it is
later uninstalled. If that happens, check `bin64` for `PirateHatHUD.asi` and
`PirateHatHUD.asi.disabled`.

## Settings and hotkeys

Edit `PirateHatHUD.ini` next to the ASI, then restart the game to apply your changes.

```ini
[indicator]
enabled=1
force_show=0
x=350
y=-310
scale_percent=100
show_delay_ms=1000

[sound]
enabled=1
cooldown_ms=1000

[hotkeys]
toggle=F9
unload=F10

[logging]
level=info
max_file_size_mb=5
max_files=3
```

- **F9** toggles the icon and sound. Set `[indicator] enabled=0` to start with them off.
- **F10** stops the mod for the rest of the session. Restart the game to use it again.
- **Position:** `x` counts from the left. A negative `y` counts up from the bottom;
  zero or a positive value counts down from the top. The defaults were chosen for
  2560 × 1440, so you may want to adjust them for your HUD.
- **Size:** `scale_percent=100` gives a 56 × 56 pixel icon. The allowed range is 25–400.
- **Return delay:** `show_delay_ms` controls how long the icon waits after the minimap
  is visible and the menu is closed. The default is one second; use `0` for no delay.
  Values up to `60000` are accepted.
- **Test mode:** `force_show=1` shows the icon without needing the hat or nearby
  treasure. It still hides in menus and when the minimap is off.
- **Sound:** `[sound] enabled=0` disables the notification. `cooldown_ms` sets the
  minimum interval between sounds (0 to 60000 ms; default 1000).

The sound plays once when observed treasure detection changes from inactive to active.
On startup, an already active perk also notifies once when gameplay first becomes
eligible for sound. After this initial sample, detection while the mod is disabled,
the game is unfocused, a menu is open or unknown, or the minimap is hidden or unknown
is silent and is not replayed later. Sound is independent of the icon's return delay;
`force_show` does not trigger it.

Hotkeys can be set to F8, F9, F10 or F11.

### Custom icon

The default chest is embedded in the ASI and isn't included as a separate PNG
in the archive. To use your own icon:

1. Save it as a **PNG**, preferably **128 × 128 pixels** with a transparent background.
2. Name the file **`PirateHatHUD_treasure.png`** and place it next to **`PirateHatHUD.asi`**
   (in the game's `bin64` folder when using CDUMM).
3. Restart the game.

Other sizes work too, up to **4096 × 4096 pixels**, but use a square image: the HUD
draws every icon as a square. Its on-screen size is 56 × 56 pixels at
`scale_percent=100`, regardless of the source image size.

CDUMM won't install this file for you or remove it when you uninstall the mod.
If the image can't be loaded, the mod won't start; remove it to go back to the
built-in icon.

### Custom sound

The default sound is embedded in the ASI. To override it, place a short PCM WAV named
`PirateHatHUD_treasure.wav` next to the ASI, then restart the game. Remove the file to
restore the embedded sound. Custom sounds must be installed manually.
Supported files are mono or stereo, 8-bit or 16-bit PCM, 8–192 kHz, up to 8 MiB.
An invalid custom WAV disables sound only and writes a warning to the log.

## Troubleshooting

If you don't see the icon, try `force_show=1`, restart the game, and load a save.
Make sure the minimap is on, close any menus, wait a second, and try F9.
If that works, set `force_show=0` and test again with the Pirate King Hat near treasure.

Logs are saved next to the ASI as `PirateHatHUD_*.log`. Check the newest file first.
Set `[logging] level=debug` for treasure, minimap and menu diagnostics. The default
keeps three log files of up to 5 MB each. You can change the size to 1–100 MB and
the file count to 1–20, or use `level=off` to disable logging.

If you're reporting a problem, include the log, your game version, and graphics
settings—especially HDR, DLSS and Frame Generation. A game update may change the
memory layout the mod relies on. The UI offsets were checked on **2.03.02**.

SDR, scRGB and HDR10 output are supported. If the icon looks wrong in HDR10, try
turning HDR off and back on so the mod can pick up the game's color-space setting.

## Building

Game research tools, event investigation notes and a compatibility checklist are in
[tools/game-research](tools/game-research/README.md).

You'll need Visual Studio 2022 or Build Tools with the x64 C++ toolchain, a Windows
SDK, the C++ CMake tools, and Git. From PowerShell:

```powershell
./cmake/Build.ps1
```

This builds Release, runs the architecture checks and tests, and creates
`dist/PirateHatHUD-0.5.0.zip`. The unpacked mod is in
`build/ninja/Release/PirateHatHUD/`. For Debug, add `-Configuration Debug`.
Dependencies are downloaded on the first build; existing sources in `build/_deps/`
are reused when available.

To build with your own CMake installation (3.28 or newer):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --build build --config Release --target architecture-check
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target package
```

The code uses C++23 and clang-format 20.1.8. To install and run the formatter:

```powershell
python -m pip install --target build/format-tools -r requirements-format.txt
cmake -P cmake/Format.cmake
cmake -DCHECK=ON -P cmake/Format.cmake
```

See [AGENTS.md](AGENTS.md) for contribution guidelines and
[docs/architecture.md](docs/architecture.md) for module boundaries and hook lifetimes.
The mod reads the game's treasure state; it doesn't change the counter.

## Credits

- Icons by [Icons8](https://icons8.com/).
- Sound: "Subscribe alert - metal dings" by Roy's Noise, via [Uppbeat](https://uppbeat.io/).

## License

[MIT](LICENSE). Third-party licenses are included in the archive and listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

[Release notes](release_notes.md) · [Changelog](CHANGELOG.md)
