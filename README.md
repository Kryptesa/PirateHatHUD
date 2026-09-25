# Pirate Hat HUD for Crimson Desert

**Source preview 0.1.0.** This is an ASI plugin project for Windows x64. It displays a small chest icon when the Pirate King Hat treasure-detection counter is positive. It does not freeze, NOP, or change the game's counter or treasure logic. Runtime observation is read-only; the hooks alter code flow only to call the observer and continue with the relocated original instructions.

**Release status:** The exact AOB bytes for the observed game build were not supplied. `include/patterns.hpp` deliberately contains `PLACEHOLDER_PATTERN` for both sites. The shipped code fails closed and installs **no hooks** until a mod author supplies and verifies unique signatures. No compiled, functioning release or live in-game validation is claimed here. Complete the validation checklist below before publishing a Nexus *main file*.

## Observed runtime behavior

At the investigated build's `CrimsonDesert.exe+125BD6E`, `inc dword ptr [rsi+08]` runs on entering chest radius. At `CrimsonDesert.exe+125BD9A`, `sub dword ptr [rsi+08],01` runs on leaving. `[RSI+0x08]` was 0 outside and 1 inside; without the hat the state did not switch. These are **reference RVAs only**, never runtime addresses in code. Game updates can change them.

The plugin scans executable PE sections for two unique AOBs, requires them to be 0x2C apart, verifies the instruction bytes, and then installs SafetyHook mid-hooks at both sites. Each callback records the current `RSI`. The DX11 Present hook checks `[RSI+0x08]` with guarded, read-only access and draws the icon through Dear ImGui. Invalid, missing, or ambiguous signatures disable the entire plugin. No standalone overlay runs in that case.

## Fill in signatures before building a functional release

1. On a game build you are permitted to inspect, open the process in Cheat Engine. Go to `CrimsonDesert.exe+125BD6E` and `CrimsonDesert.exe+125BD9A` in the Memory Viewer. Confirm the disassembly is exactly `FF 46 08` and `83 6E 08 01` respectively. Record the game's file version and executable hash.
2. For each instruction, copy at least 16 **whole instruction bytes starting at that instruction** into a separate AOB string. Include enough nearby stable bytes for uniqueness. Replace relocation-dependent displacements, pointers, and call/jump targets with `??`; keep each token two hex digits or `??`, separated by spaces. Do not wildcard the leading opcode bytes.
3. Put both strings in `include/patterns.hpp`. `kEnter` begins with `FF 46 08`; `kLeave` begins with `83 6E 08 01`. Keep the hook point at offset zero of each match. The scanner rejects patterns shorter than 16 bytes or containing fewer than 10 fixed bytes.
4. Scan the live executable: each signature must match exactly once, and the two hook sites must be 0x2C apart. Confirm SafetyHook can relocate the instructions at each site. If a game update changes layout or instruction semantics, review the code and signatures rather than relaxing validation.
5. Test hat off, hat on outside radius, entry, exit, fast travel, reload, resolution changes, fullscreen/windowed mode, and `F10` unload. Check `PirateHatHUD.log` beside the ASI. Repeat on every game update before publishing an updated binary.

Example **format only** (not actual game bytes): `FF 46 08 48 8B ?? ?? ?? ?? ?? 48 85 C0 74 ?? 90`. Do not copy it as a working signature.

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

If no log appears, the ASI loader likely did not load the plugin. If the log says signatures are missing, ambiguous, or invalid, the game build is unsupported or `patterns.hpp` is still a placeholder. If DX11 initialization fails, the game may be using a different graphics API or another plugin may own the rendering path. This plugin only supports DX11. `F10` gracefully unloads during normal play; terminating the game ends it with the process.

## Nexus publishing checklist

- Replace both placeholder signatures and document supported game version/hash.
- Build and test the Release ASI in game, including DMM install/uninstall and ordinary ASI loader install.
- Package a ZIP containing one `PirateHatHUD/` folder with the ASI and INI, plus README and license as desired. Do not put source files in the player-facing binary package unless you intend to publish source there.
- Upload a clear screenshot of the original icon; mark the mod as requiring DMM or another ASI loader. Explain that it observes the state and draws an overlay, without changing game logic.
- Include third-party license notices for SafetyHook, Zydis, and Dear ImGui when distributing a binary. The staging folder includes the first two; add `LICENSE-Zydis.txt` from the fetched Zydis source.

## Credits and license

Project code and original placeholder icon: MIT, see [LICENSE](LICENSE). SafetyHook is BSL-1.0; Dear ImGui is MIT; Zydis is MIT. Their code is fetched at build time and is not included in this source archive.
