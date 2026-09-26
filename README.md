# Pirate Hat HUD for Crimson Desert

Pirate Hat HUD is an ASI mod that shows a small treasure chest icon when the Pirate King Hat detects nearby treasure. You can change the icon's position and size, toggle it during play, or unload the mod with a hotkey.

> **Status: 0.3.0 DX12 preview.** Treasure state detection was confirmed in game on Crimson Desert 1.0.0.2976. The DirectX 12 overlay builds, but its drawing has not yet been verified in game. Treat this as a test build, especially when using DLSS or Frame Generation.

## Install

You need the Windows x64 version of Crimson Desert and a compatible ASI loader. For Desert Mod Manager (DMM), use its ASI loader support.

1. Download or build the mod. Keep these three files together:

   ```text
   PirateHatHUD/
   ├── PirateHatHUD.asi
   ├── config.ini
   └── icon.png
   ```

2. With DMM, import the ZIP containing the `PirateHatHUD` folder, or place that folder in DMM's `mods` directory. Enable the mod and mount it with the ASI loader enabled. For a standalone ASI loader, put all three files in the loader's plugin directory, side by side.
3. Start the game. The icon should appear near the minimap when the Pirate King Hat's treasure state is active.

The icon file is required. If it is missing or invalid, the mod logs an error and unloads. After changing `config.ini`, restart the game. For current DMM setup steps, see the [mod manager page](https://www.nexusmods.com/crimsondesert/mods/633).

## Configure

Edit `config.ini` beside `PirateHatHUD.asi`:

```ini
[indicator]
enabled=1
force_show=0
x=350
y=-310
scale_percent=100

[hotkeys]
toggle=F9
unload=F10
```

| Setting | Meaning |
| --- | --- |
| `enabled` | `1` draws the icon; `0` starts with it hidden. |
| `force_show` | `1` always draws the icon for testing, regardless of treasure state or pattern scan results. Return it to `0` for normal play. |
| `x` | Horizontal position in pixels from the left edge. |
| `y` | Negative values count pixels up from the bottom; zero and positive values count down from the top. |
| `scale_percent` | Icon size from `25` to `400`. Values outside this range use `100`. |
| `toggle` | Show or hide the icon for the current session. Default: `F9`. |
| `unload` | Remove hooks and unload the mod for the current session. Default: `F10`. |

Hotkeys accept `F8`, `F9`, `F10`, or `F11`. The default position (`x=350`, `y=-310`) was chosen for 2560 × 1440; adjust it for your display and HUD layout.

## If the icon does not appear

1. Check that `PirateHatHUD.asi`, `config.ini`, and `icon.png` are in the same directory and that the ASI loader is active.
2. Set `force_show=1`, restart the game, and load a save. This tests the overlay without requiring the hat or an active treasure state. Press `F9` to check the toggle.
3. Open `PirateHatHUD.log` beside the ASI. `DX12 hooks installed; waiting for swapchain` means the graphics hooks started. `DX12 swapchain and present queue captured` and `DX12 overlay initialized` indicate that the renderer reached the game swapchain. `State hooks disabled` means the game's instruction pattern was missing or ambiguous, so normal treasure detection is unavailable.
4. If the test icon works, restore `force_show=0` and check it while wearing the Pirate King Hat near treasure. If it fails only with DLSS or Frame Generation, record those settings along with the game version and log when reporting the issue.

The DX12 renderer has not been verified in game yet. Graphics proxies and generated frames may affect whether the icon is drawn. `force_show=1` is a diagnostic setting; it does not confirm that treasure detection works.

## Build from source

Install Visual Studio 2022 with **Desktop development with C++**, a Windows SDK, CMake 3.28 or newer, and Git. The first CMake configure downloads SafetyHook, Zydis, and Dear ImGui. Build the x64 Release configuration:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The compiled ASI is `build/Release/PirateHatHUD.asi`. The ready-to-install folder is `build/Release/PirateHatHUD/`; it includes the ASI, configuration, icon, and third-party license files. The project uses C++23 and the static MSVC runtime.

## Code style

C++ code uses clang-format **20.1.8**, with LLVM style, two-space indentation,
a 100-column limit, and expanded control statements. The shared `.clang-format`
and `.editorconfig` files keep editor and command-line formatting consistent.
Include order is preserved because Windows and COM headers can depend on it.
Only project files in `src`, `include`, and `tests` are formatted; dependencies
and generated files are excluded.

Install the pinned formatter locally (Python and pip required):

```powershell
python -m pip install --target build/format-tools -r requirements-format.txt
```

After configuring the build, format or check without changing files:

```powershell
cmake --build build --target format
cmake --build build --target format-check
```

You can also run these commands without configuring or downloading build dependencies:

```powershell
cmake -P cmake/Format.cmake
cmake -DCHECK=ON -P cmake/Format.cmake
```

If the formatter is installed elsewhere, pass `-DCLANG_FORMAT="path/to/clang-format.exe"`
at configure time or before `-P`. Editors should use the same version and the
repository's `.clang-format`; enable format on save if desired.
Use braces for new control-flow blocks, keep headers self-contained, and follow
the existing naming conventions. Formatting changes should preserve behavior.

See the [clang-format style reference](https://clang.llvm.org/docs/ClangFormatStyleOptions.html).

## How it works

The game observer is built as a separate static library with typed subscriptions.
The mod's indicator consumes treasure state changes, while the DX12 renderer receives
a complete HUD snapshot. See [module architecture](docs/architecture.md) for the public
API, thread and lifetime contracts, reuse boundaries, and validation limits.

The mod scans executable sections of `CrimsonDesert.exe` for a unique pair of treasure-counter instructions. It attaches observation hooks at the matching instructions, captures the counter address, and reads the counter to decide when to show the icon. It does not write to or freeze the counter. A missing or ambiguous match disables state observation and is logged.

The overlay draws `icon.png` through a DirectX 12 swapchain hook. The counter hooks were confirmed on Crimson Desert 1.0.0.2976 (EXE SHA-256 `57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7`); other game builds may need updated patterns. See [release notes](release_notes.md) and the [changelog](CHANGELOG.md) for version history.

## License

The project code and original icon are licensed under the [MIT License](LICENSE). Dependencies have their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md). Include the dependency license files from the staged build folder when distributing a binary.
