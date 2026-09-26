# Pirate Hat HUD for Crimson Desert

Pirate Hat HUD is an ASI mod that shows a small treasure chest icon when the Pirate King Hat detects nearby treasure. You can change the icon's position and size, toggle it during play, or disable the mod for the session with a hotkey.

The icon is shown only while the minimap is visible. Menus that hide the minimap and the game's minimap display setting also hide the icon, including in diagnostic mode. The current development build uses a minimap memory chain verified on Crimson Desert **2.03.02**; compiled feature integration still needs in-game validation.

> **Status: 0.3.0 development build.** The user reported the existing icon drawing over menus near treasure. The new minimap gating builds and passes automated checks, but the compiled change still needs in-game verification. Treasure hooks were historically confirmed on 1.0.0.2976; the minimap chain was verified separately on 2.03.02. DLSS and Frame Generation validation is not recorded.

## Install

You need the Windows x64 version of Crimson Desert. With [CDUMM](https://github.com/faisalkindi/CrimsonDesert-UltimateModsManager), import the complete `PirateHatHUD-0.3.0.zip` using drag and drop, then enable the plugin on the ASI page with the ASI loader enabled. Restart the game after installing, updating or changing the plugin's enabled state.

The archive contains:

```text
PirateHatHUD/
|-- PirateHatHUD.asi
|-- PirateHatHUD.ini
|-- modinfo.json
|-- LICENSE
|-- THIRD_PARTY_NOTICES.md
|-- LICENSE-*.txt
```

CDUMM installs `PirateHatHUD.asi` and its matching `PirateHatHUD.ini` into the game's `bin64` directory. The standard icon is embedded in the ASI; no external PNG is required. Metadata and licenses remain in the distribution archive. `modinfo.json` describes the package; the ASI page may obtain version information separately rather than displaying these fields.

For a standalone ASI loader, copy `PirateHatHUD.asi` and `PirateHatHUD.ini` side by side into its plugin directory. Start the game with the minimap enabled; the icon appears when the Pirate King Hat's treasure state is active and the minimap is visible.

### Updating an older installation

The reviewed CDUMM `AsiManager` code has an uninstall limitation after updating a disabled plugin and later enabling it: the ASI can remain behind. Enable the plugin before importing an update, with the game closed; after uninstalling, check that neither `PirateHatHUD.asi` nor `PirateHatHUD.asi.disabled` remains in `bin64`.

Back up your old `config.ini` before importing the new ZIP. After installation, copy your settings into `PirateHatHUD.ini` (or replace it with your backed-up config). The new config takes precedence; `config.ini` is read only when `PirateHatHUD.ini` is absent. The mod does not modify or delete the old config or `icon.png`, because other plugins may use those generic names. Remove an old duplicate ASI installation before enabling the new one.

To use a custom icon, manually place `PirateHatHUD.png` beside the ASI. It overrides the embedded icon; an invalid image prevents startup and is logged. Rename an old custom `icon.png` to `PirateHatHUD.png` to preserve it. CDUMM's ASI importer does not install PNG files, and does not track this manually added override or the runtime log for removal.

## Configure

Edit `PirateHatHUD.ini` beside `PirateHatHUD.asi`:

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
| `enabled` | `1` enables the indicator subject to treasure and minimap visibility; `0` starts with it disabled. |
| `force_show` | `1` draws the icon for testing when the minimap is visible, regardless of treasure state or treasure pattern scan results. Return it to `0` for normal play. |
| `x` | Horizontal position in pixels from the left edge. |
| `y` | Negative values count pixels up from the bottom; zero and positive values count down from the top. |
| `scale_percent` | Icon scale from `25` to `400`; `100` draws 56 × 56 pixels. Values outside this range use `100`. |
| `toggle` | Enable or disable the indicator for the current session. Visibility still follows treasure/minimap state. Default: `F9`. |
| `unload` | Disable observation and rendering for the current session. Default: `F10`. The DLL and required hook allocations remain loaded until game exit after hook activation. |

Hotkeys accept `F8`, `F9`, `F10`, or `F11`. The default position (`x=350`, `y=-310`) was chosen for 2560 × 1440; adjust it for your display and HUD layout.

## If the icon does not appear

1. Check that `PirateHatHUD.asi` and `PirateHatHUD.ini` are side by side and the ASI loader is active. If you added `PirateHatHUD.png`, remove it temporarily to test the embedded icon.
2. Set `force_show=1`, restart the game, and load a save. Enable the minimap and close menus. This tests the overlay without requiring the hat or an active treasure state. Press `F9` to check the toggle.
3. Open `PirateHatHUD.log` beside the ASI. `DX12 hooks installed; waiting for swapchain` means the graphics hooks started. `DX12 swapchain and present queue captured` and `DX12 overlay initialized` indicate that the renderer reached the game swapchain. `State hooks disabled` means the game's instruction pattern was missing or ambiguous, so normal treasure detection is unavailable.
4. If the test icon works, restore `force_show=0` and check it while wearing the Pirate King Hat near treasure. If it fails only with DLSS or Frame Generation, record those settings along with the game version and log when reporting the issue.

Minimap diagnostics distinguish `Minimap visible`, `Minimap hidden` and `Minimap sample unavailable; icon hidden`. An unavailable sample hides the icon even with `force_show=1`; polling retries automatically. The root RVA and fixed array slots were verified on 2.03.02 only and may need updating after game changes.

The compiled minimap gating still needs in-game verification. Graphics proxies and generated frames may affect whether the icon is drawn. `force_show=1` is a diagnostic setting; it does not confirm that treasure detection works.

## Build from source

Install Visual Studio 2022 with **Desktop development with C++**, a Windows SDK, CMake 3.28 or newer, and Git. The first CMake configure downloads SafetyHook, Zydis, and Dear ImGui. Build the x64 Release configuration:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target package
```

The versioned installation archive is `dist/PirateHatHUD-0.3.0.zip`; it includes the ASI with its embedded icon, named configuration, package metadata, and license files inside the `PirateHatHUD/` folder. The compiled ASI and staging folder remain in `build/Release/`. The archive version comes from `project(... VERSION ...)` in CMake. The project uses C++23 and the static MSVC runtime.

For a sandbox where MSBuild fails with `FileTracker` / `E_ACCESSDENIED`, use the
Ninja build helper from a regular PowerShell session:

```powershell
./cmake/Build.ps1
# Optional configuration:
./cmake/Build.ps1 -Configuration Debug
```

The helper finds Visual Studio or Build Tools with the x64 C++ tools and bundled
CMake/Ninja, initializes MSVC for the current process, then configures, builds,
runs `architecture-check`, runs CTest, and packages the mod in `dist` only after
all checks pass. Release produces `dist/PirateHatHUD-0.3.0.zip`; other configurations
use a matching subfolder such as `dist/Debug/`. It restores the caller's environment
afterwards. Install the **C++ CMake tools for Windows** component if it is missing.
The separate Ninja build uses `build/ninja/`; the Release bundle is
`build/ninja/Release/PirateHatHUD/`. Existing dependency sources in `build/_deps/`
are reused when available; missing dependencies require network access during
configuration. This build uses MSVC directly through Ninja, avoiding MSBuild's
FileTracker.

## Code style

Architecture rules live in [AGENTS.md](AGENTS.md) and the
[module architecture](docs/architecture.md). The default build checks header/source
placement and module includes, and configuration validates production target links.
CTest includes checker regression cases. To check layout and includes without a build:

```powershell
cmake -P cmake/Architecture.cmake
```

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

The game observers are built as a separate static library with typed subscriptions.
The mod's indicator consumes treasure and minimap state changes, while the DX12 renderer receives
a complete HUD snapshot. See [module architecture](docs/architecture.md) for the public
API, thread and lifetime contracts, reuse boundaries, and validation limits.

The mod scans executable sections of `CrimsonDesert.exe` for a unique pair of treasure-counter instructions. It attaches observation hooks at the matching instructions, captures the counter address, and reads the counter to decide when to show the icon. It does not write to or freeze the counter. A missing or ambiguous match disables state observation and is logged.

The overlay draws `icon.png` through a DirectX 12 swapchain hook. The counter hooks were confirmed on Crimson Desert 1.0.0.2976 (EXE SHA-256 `57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7`); other game builds may need updated patterns. See [release notes](release_notes.md) and the [changelog](CHANGELOG.md) for version history.

The icon supports SDR, scRGB and HDR10 output. HDR icon white is fixed at 203 nits.
HDR10 requires the mod to observe the game's color-space selection; if the mod
starts after that selection, toggle HDR off and on in the game. Translucent edges
in HDR10 use approximate blending in PQ space. Verify color, brightness, HDR
switching, resize, force_show and F9/F10 behavior in game.

The icon follows minimap visibility and its display setting. The memory chain was tested on game 2.03.02; see [minimap observation](docs/minimap-observation.md) for version limitations and required integration checks.

## License

The project code and original icon are licensed under the [MIT License](LICENSE). Dependencies have their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md). Include the dependency license files from the staged build folder when distributing a binary.

`[indicator] show_delay_ms=1000` delays the icon's return until the minimap is visible
and Root_MainMenu is closed for that interval (0..60000 ms). Hiding cancels the wait
immediately. Unknown UI state hides the icon; `force_show` still obeys these UI gates.
Menu object resolution uses script RTTI; the UI root slot is specific to game 2.03.02.
In-game verification remains required for Esc/M/I, cutscenes, disabled minimap,
startup with an open menu, F9/F10 and a fresh game process. Adjust the delay after
measuring any remaining map closing animation.
