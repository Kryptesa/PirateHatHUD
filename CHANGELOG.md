# Changelog

## Unreleased — CDUMM packaging and minimap visibility

- Check only the reused backbuffer and ImGui buffer slot before drawing, allowing unrelated overlay submissions to remain in flight. Give each backbuffer its own command list and test ring tracking across skipped frames and backend recreation. The user confirmed on 2026-09-26 that the icon no longer flickers in game; graphics settings for this verification were not recorded.
- Recover the overlay when the game replaces its swapchain for the same window after a graphics setting change. Drain old GPU submissions before releasing resources; log resize HRESULTs and replacement recovery. In-game HDR switching still needs verification.
- Handle `ResizeBuffers1` as well as `ResizeBuffers`, releasing overlay backbuffers before resize and recreating them afterwards. Follow a replacement single present queue; disable drawing for unsupported multiple queues. In-game DLSS/Frame Generation switching still needs verification.
- Add local date/time with millisecond precision to runtime log lines and log resize lifecycle results.
- Package a matching `PirateHatHUD.ini` for CDUMM ASI import and config discovery; read legacy `config.ini` only if the named file is absent.
- Embed the default PNG into the ASI, with an optional manual `PirateHatHUD.png` override, so installation needs no external image.
- Generate `modinfo.json` from the CMake version and include the project's MIT license alongside dependency licenses.
- Hide the treasure icon while the minimap is hidden, including menus and the game's minimap display setting. Preserve treasure state so the icon can return when the minimap reappears.
- Apply the same minimap requirement to `force_show`; hide the icon when observation is unavailable.
- Add a read-only, polling minimap observer with guarded pointer reads and startup/state diagnostics. The version-specific chain was verified in Cheat Engine on Crimson Desert 2.03.02 across menus, inventory, teleport and a game restart.
- Release build, architecture-check, formatting and all 13 CTest tests pass. The compiled minimap/HUD integration still needs in-game verification; no root signature or automatic version detection is implemented.

## 0.3.0 — DX12 overlay preview

- Add DXGI swapchain and D3D12 queue capture, per-backbuffer ImGui DX12 rendering, fences, resize handling, and F9/F10 control.
- Add force_show diagnostic mode to test the icon independently of treasure-state observation.
- Build and scanner tests pass; live DX12 drawing, DLSS, and Frame Generation remain unverified.

## 0.2.0 — diagnostic preview

- Discover the unique enter/leave instruction pair in executable PE sections at runtime, including the game's executable `.rsrc` section.
- Log guarded, read-only treasure counter transitions and RSI base independently of DX11 overlay initialization.
- Add focused candidate scanner tests and confirm one offline match in Crimson Desert 1.0.0.2976.
- The user confirmed correct in-game state logging; the DX12 overlay remains unimplemented.

## 0.1.0 — 2026-09-25

- Initial source project with fail-closed AOB scanner, two SafetyHook mid-hooks, read-only counter observer, and DX11/ImGui icon.
- Added INI configuration, runtime toggle/unload hotkeys, logging, DMM packaging instructions, and original placeholder art.
- Binary release pending verified signatures and in-game validation.
