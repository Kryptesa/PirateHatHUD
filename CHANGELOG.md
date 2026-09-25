# Changelog

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
