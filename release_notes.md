# Pirate Hat HUD 0.4.0

This release improves how the icon follows the game's HUD and makes the overlay
more reliable when changing graphics settings.

## What's new

The chest icon hides when you open the main menu or turn off the minimap. When you
return to the game, it waits one second before appearing again. Change
`show_delay_ms` in the INI if you'd prefer a shorter delay, or set it to `0`.
Test mode follows these rules too.

Frame handling has been improved to reduce flicker. The overlay can recover when
the game replaces its swapchain, and auxiliary windows no longer take over its
rendering target. Menu detection also handles quick open/close transitions and
unavailable game memory more consistently.

The default icon is now built into the ASI. Custom icons still work: put a file
named `PirateHatHUD_treasure.png` beside it. Logs now have configurable levels and rotate
automatically instead of growing indefinitely.

## Updating

Back up your settings, import `PirateHatHUD-0.4.0.zip` into CDUMM, and restart the
game. If you're upgrading from a version that used `config.ini`, copy your settings
into `PirateHatHUD.ini`.

F9 toggles the icon. F10 stops the mod until the next game launch.

See the [README](README.md) for installation, settings and troubleshooting.

## Compatibility

The UI memory layout is based on Crimson Desert 2.03.02. Game updates may require
new offsets or instruction patterns.

SDR, scRGB and HDR10 output are supported. For HDR10, the mod needs to see the game's
color-space change; if the icon looks wrong, try switching HDR off and back on.
Swapchain recovery supports replacements within the same game window.
