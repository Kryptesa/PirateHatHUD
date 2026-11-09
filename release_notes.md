# Pirate Hat HUD 1.2.4

Disable or freely reassign the mod's hotkeys.

## Changed

- Clearing `toggle=` or `unload=` now disables that action instead of restoring
  F9/F10. `None`, `Off` and `Disabled` also disable a binding.
- Reassign hotkeys to letters, digits, F1-F24, numpad and other keyboard keys,
  Mouse1-Mouse5, or hexadecimal Windows virtual-key codes. Add Ctrl, Alt, Shift
  and/or Win modifiers, for example `Ctrl+K` or `Alt+Mouse4`.
- Missing entries keep the F9/F10 defaults. Invalid values disable the affected
  action. If both actions use the same binding, only toggle remains enabled.

For example, use Numpad5 to toggle and disable the stop hotkey:

```ini
[hotkeys]
toggle=Numpad5
unload=
```

Hotkey edits require restarting the game. Windows-reserved shortcuts may still
perform their system action or be unavailable to the mod.

## Updating

Close the game and back up PirateHatHUD.ini. Import PirateHatHUD-1.2.4.zip into
CDUMM or DMM, enable the mod and ASI loader, then restart the game. Enable the
existing mod before importing the update, and keep only one copy of the ASI.

Existing F8-F11 assignments remain compatible. Review any empty or invalid
hotkey values: these now disable the action rather than restoring a default.

## Validation and compatibility

Release build, architecture-check, formatting, all 31 CTest checks and all 25
research-tool tests passed. Release metadata and ZIP contents were validated.
Regression tests cover missing versus empty INI entries, invalid/truncated
assignments, named and numeric keys, modifier matching, duplicate assignments
and press/hold behavior.

A local candidate run on game EXE 1.0.0.3576 logged Numpad5 as the configured
toggle key, followed by successful hotkey disable/enable actions. The same run
initialized the DX12 HUD at 2560 x 1440 in HDR10 on an RTX 5080 and recorded
treasure, minimap and menu observations.

Disabled bindings, modifier combinations, mouse bindings, stop/restoration and
force_show still need separate in-game verification. The final versioned package
has not received an additional in-game run.

Windows x64 and DirectX 12. The Pirate King Hat must be equipped for detection.
The executable file version may differ from the public patch number.
