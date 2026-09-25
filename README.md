# Pirate Hat HUD for Crimson Desert

**0.3.0 DX12 preview.** The ASI observes the Pirate King Hat treasure counter and draws a small vector chest icon when the observed state is positive. It never writes, freezes, NOPs, or patches game logic. The counter hooks have been validated in game; this DX12 renderer has compiled but has **not yet been tested in game**. Do not publish it as a confirmed working Nexus main file until the rendering checks below pass.

## How it works

The plugin scans executable sections of the loaded `CrimsonDesert.exe` for a unique pair: `FF 46 08` (`inc dword ptr [rsi+08]`) and, exactly `0x2C` bytes later, `83 6E 08 01` (`sub dword ptr [rsi+08],1`). It never uses a fixed runtime address. A missing or ambiguous pair disables only state observation and is logged. The user confirmed correct `0 -> 1 -> 0` transitions on Crimson Desert 1.0.0.2976 (EXE SHA-256 `57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7`). Hook callbacks capture RSI and the pre-instruction value; a guarded read-only poll checks `[RSI+0x08]`.

The DX12 renderer hooks DXGI Present and ResizeBuffers. It intercepts `CreateSwapChain`, `CreateSwapChainForHwnd`, `CreateSwapChainForCoreWindow`, and `CreateSwapChainForComposition` to pair the real swapchain with its direct command queue. If attachment occurs after swapchain creation, it may use an observed direct queue only when exactly one queue has been seen. It refuses a queue from another D3D12 device. It owns an allocator and render target per backbuffer, uses a fence before reusing resources, and recreates them after resize. The icon is loaded from `icon.png` beside the ASI and drawn as a DX12 texture. If the PNG is missing or invalid, the mod logs the error and unloads before installing hooks.

## Build

Install Visual Studio 2022 with Desktop development with C++, Windows SDK, CMake, and Git. First configure fetches SafetyHook, Zydis, and Dear ImGui. Use x64 Release:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The output is `build/Release/PirateHatHUD.asi`; the staging folder is `build/Release/PirateHatHUD/`. This checkout has also compiled with Visual Studio 2026 Build Tools. The project uses C++23 and the static MSVC runtime.

## Install

For DMM, place the staged `PirateHatHUD` folder under DMM's `mods` directory or import its ZIP, then enable and Mount it with the ASI loader enabled. For an ordinary compatible x64 ASI loader, put `PirateHatHUD.asi` and `config.ini` side by side in the loader's plugin directory. Restart the game after INI changes. See the [DMM page](https://www.nexusmods.com/crimsondesert/mods/633) for current manager steps.

## Config and controls

`enabled=1` enables drawing. `x` is the pixel position from the left edge; a negative `y` places the icon that many pixels above the bottom edge, while a nonnegative `y` is measured from the top. The default `x=350`, `y=-310` places it just above and left of the food icon beside the minimap at 2560×1440. `scale_percent` accepts 25–400. `force_show=1` is a **diagnostic** that draws the icon without the hat or an active treasure state, even if pattern scanning fails. Set it back to `0` for normal use. F9 toggles drawing for the current session; F10 removes hooks and unloads the ASI. Supported key names are F8–F11. The INI is read at startup. Logs are written beside the ASI to `PirateHatHUD.log`.

## In-game validation before release

1. Set `force_show=1`. Confirm the icon appears after loading a save and F9 hides/shows it. Try windowed, borderless, fullscreen, resize, alt-tab, resolution changes, and F10 unload.
2. Repeat with DLSS off/on and Frame Generation off/on where supported. NVIDIA Streamline can proxy the swapchain, and one game Present can produce multiple generated frames. The hook may miss a proxy Present, or the icon may appear only on game-rendered frames and be absent or interpolated on generated frames. Treat an unsupported path as a compatibility failure; do not claim generated-frame coverage until observed.
3. Set `force_show=0`. With Pirate King Hat, enter and leave a chest radius and confirm the icon follows `Treasure state 1` and `Treasure state 0`. Remove the hat inside a radius, change equipment/location, load a save, and confirm no stale icon.
4. Verify DMM install/uninstall and ordinary ASI loader install. Record game version, graphics settings, and exact log lines for any failure.

The renderer hooks DXGI's system vtables. A graphics proxy loaded ahead of it can expose a different swapchain implementation; check the log for `DX12 swapchain and present queue captured` and `DX12 overlay initialized`. If those messages never appear, this preview may need another integration path. The ExecuteCommandLists fallback is deliberately disabled after multiple distinct direct queues to avoid submitting overlay commands to an unrelated queue.

## Packaging and license

A player ZIP should contain one `PirateHatHUD/` folder with the ASI, INI, and `icon.png`, plus the README, LICENSE, and third-party notices. Do not include copyrighted game artwork. Include license files for SafetyHook, Dear ImGui, and Zydis with a binary release. Project code and original icon are MIT licensed. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
