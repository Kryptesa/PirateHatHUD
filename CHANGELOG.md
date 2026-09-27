# Changelog

## Unreleased

## 1.0.0 — 2026-09-28

- Fixed F9/F10 input with Windows hotkey messages while the game is focused.
  Keys are released on focus loss and shutdown; unavailable bindings use polling.
- Prevented the mod from starting in the game's crash handler.
- Added the executable file version and EXE/ASI names to startup logs without
  installation paths. Successful hotkey registration now uses debug logging.
- Added installation instructions for DMM alongside CDUMM.

- Grouped game observation internals by responsibility and shared guarded address
  and memory reads across observers. Public observer header paths are unchanged.
- Added `[sound] volume_percent` (0 to 100, default 100) for additional notification
  volume control after the game sliders. Saved volume changes apply without a restart.
- Notification sound now follows the game's overall and effects volume sliders,
  including custom WAV files. Muted or unreadable settings suppress sound without
  replaying notifications later.

## 0.5.0 — 2026-09-27

- Added a single sound notification when treasure detection becomes active.
  The first active state after loading also notifies once when gameplay is ready.
- Embedded the default sound in the ASI, so no separate WAV installation is needed.
  An optional `PirateHatHUD_treasure.wav` beside the ASI overrides it.
- Added `[sound] enabled` and `cooldown_ms`; F9 toggles both the icon and sound.
- Suppressed sound in menus, with a hidden or unknown minimap, while the mod is
  disabled, and while the game is unfocused. Later menu exits do not replay it.
- Moved audio device calls off the HUD polling thread to keep observation responsive.
- Added icon and sound credits and regression tests for notification and audio lifetime.

## 0.4.0 — 2026-09-27

- The icon now hides when the minimap is off or the main menu is open. It returns
  after a configurable delay (`show_delay_ms`, one second by default).
- Test mode (`force_show`) follows the same menu and minimap rules.
- Improved frame resource handling to reduce icon flicker.
- Added recovery when graphics settings replace the game's swapchain, and support
  for both DX12 resize methods.
- Fixed swapchain selection so creating an auxiliary window doesn't take over the HUD.
- Made menu detection more conservative when game memory can't be read. Cached
  static type information to reduce repeated reads without keeping stale menu objects.
- Fixed capture of short menu open/close transitions between polls.
- Added configurable log levels, UTC timestamps and log rotation.
- Embedded the default icon. A `PirateHatHUD_treasure.png` beside the ASI can replace it.
- Switched to `PirateHatHUD.ini` and added CDUMM package metadata and bundled licenses.
  Older `config.ini` files are still accepted when the new file is missing.
- Moved HDR shader compilation out of graphics callbacks and pinned the SafetyHook
  dependency to a specific commit.
- Shared memory-reading code and observer state types, tightened module boundaries,
  and expanded regression tests.

## 0.3.0 — DX12 overlay preview

- Added a DirectX 12 HUD renderer with swapchain and command queue detection.
- Added frame synchronization, resize handling and F9/F10 controls.
- Added `force_show` to test the overlay without nearby treasure.

## 0.2.0 — diagnostic preview

- Added runtime scanning for the treasure counter's instruction pair.
- Added read-only counter observation and transition logging.
- Added scanner tests for missing, unique and ambiguous matches.

## 0.1.0 — 2026-09-25

- Initial implementation with a treasure counter scanner, observation hooks
  and a DX11/ImGui icon.
- Added INI settings, hotkeys, logging and installation instructions.
