# Pirate Hat HUD for Crimson Desert

**Diagnostic source preview 0.2.0.** This Windows x64 ASI dynamically discovers the two treasure counter instructions, installs SafetyHook mid-hooks, and logs the observed `RSI` base and `[RSI+0x08]` state. The hooks never freeze, NOP, or write the counter. This milestone is intended to obtain an in-game log showing `0 -> 1 -> 0`.

**Current validation:** The scanner found exactly one instruction pair in the installed Crimson Desert 1.0.0.2976 EXE (SHA-256 `57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7`). The pair is at RVAs `0x125BD6E` and `0x125BD9A`. These RVAs are reference values only; the plugin does not use them to locate code at runtime. The actual executable section containing the pair is named `.rsrc`. The code has compiled, but the ASI has not yet been loaded in a live game for the required state transition test.

## Runtime discovery and logging

The plugin scans every executable section of the loaded `CrimsonDesert.exe` for `FF 46 08` (`inc dword ptr [rsi+08]`) followed exactly `0x2C` bytes later by `83 6E 08 01` (`sub dword ptr [rsi+08],1`). This pair is required to occur exactly once. Missing or multiple pairs, invalid PE metadata, or failed hook installation leave state observation disabled and write a reason to `PirateHatHUD.log` beside the ASI. A game update may require a revised matcher after fresh inspection.

At either hook, the callback records `RSI`, the counter immediately before the original instruction, and an event count. A worker logs that pre-instruction sample and checks `[RSI+0x08]` with read-only guarded access every 30 ms and logs state changes with base, state address, and event count. Base changes without a state change are logged at most once per second. Hook events can be coalesced by the 30 ms poll; the latest pre-instruction sample is logged. This diagnostic build does not install the old DX11 Present hook or initialize a DX11 device. The UI code remains in the source pending the DX12 overlay milestone; showing the icon is a later milestone.

To validate in game, wear the Pirate King Hat and stand outside a chest radius, then enter and leave it. Confirm the log records the unique pair, active mid-hooks, and `Treasure state 1` followed by `Treasure state 0`. Repeat without the hat and after changing equipment or location. Do not publish a player-facing binary until those checks and overlay support are complete.

## Build: Visual Studio 2022 x64 Release

Install Visual Studio 2022 with Desktop development with C++, Windows SDK, CMake, and Git. A network connection is needed on first configure for [SafetyHook](https://github.com/cursey/safetyhook), its Zydis dependency, and [Dear ImGui](https://github.com/ocornut/imgui). From a Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The output is `build/Release/PirateHatHUD.asi`, with a `build/Release/PirateHatHUD/` staging folder containing the ASI, INI, and notices. This project requires C++23. The CMake build uses the static MSVC runtime and fetches dependency source; review their licenses before distributing binaries.

## Install

### Definitive Mod Manager (DMM)

Place the **folder** `PirateHatHUD` containing `PirateHatHUD.asi` and `config.ini` under DMM's `mods` folder, or import its ZIP in DMM. Enable and Mount it. DMM's ASI loader must be installed/enabled. Keep both files together. DMM supports ASI plugins with companion config files; see the [DMM Nexus page](https://www.nexusmods.com/crimsondesert/mods/633) for its current workflow.

### Ordinary ASI loader

Install a compatible x64 ASI loader for Crimson Desert. Copy `PirateHatHUD.asi` and `config.ini` together into the loader's ASI plugin directory (commonly the game's `bin64` folder, depending on the loader). Avoid installing two proxy loaders for the same game. Restart the game after changing the INI.

## Configuration and controls

`config.ini` uses pixel coordinates from the upper-left of the viewport. `scale_percent` accepts 25â€“400; values outside that range become 100. `enabled=0` starts hidden. `toggle=F9` toggles the icon for the current session. `unload=F10` removes hooks and unloads the ASI. Supported keys: F8, F9, F10, F11. Configuration is read at startup. The icon is drawn programmatically; the optional `assets/icon.png` is an original placeholder reference and is not loaded by the plugin.

## Troubleshooting

If no log appears, the ASI loader likely did not load the plugin. If the log says the instruction pair is missing or ambiguous, the game build is unsupported and the matcher needs review. This diagnostic build intentionally disables the DX11 overlay and logs state without graphics hooks. `F10` gracefully unloads during normal play; terminating the game ends it with the process.

## Nexus publishing checklist

- Confirm the discovered instruction pair and document supported game version/hash.
- Build and test the Release ASI in game, including the 0 -> 1 -> 0 log, DMM install/uninstall, and ordinary ASI loader install.
- Package a ZIP containing one `PirateHatHUD/` folder with the ASI and INI, plus README and license as desired. Do not put source files in the player-facing binary package unless you intend to publish source there.
- Upload a clear screenshot of the original icon; mark the mod as requiring DMM or another ASI loader. Explain that it observes the state and draws an overlay, without changing game logic.
- Include third-party license notices for SafetyHook, Zydis, and Dear ImGui when distributing a binary. The staging folder includes the first two; add `LICENSE-Zydis.txt` from the fetched Zydis source.

## Credits and license

Project code and original placeholder icon: MIT, see [LICENSE](LICENSE). SafetyHook is BSL-1.0; Dear ImGui is MIT; Zydis is MIT. Their code is fetched at build time and is not included in this source archive.
