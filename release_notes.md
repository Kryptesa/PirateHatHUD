# Pirate Hat HUD 1.0.2

A chest icon and short sound announce nearby treasure detected by the Pirate King
Hat, including when helmets are hidden. The hat must be equipped.

## Changes since 1.0.1

- Added local diagnostics for the active GPU, video memory, UMD driver version,
  Windows build and mod build identity.
- Added thread IDs and detailed debug markers for instruction scans, creation and
  activation of game hooks, and the first invocation of graphics callbacks.
- Added debug snapshots of selected loaded graphics/loader module names and file
  versions, plus swapchain settings, resize and color-space diagnostics.
- Graphics failures record HRESULT and device removal reason where available.
  Repeated identical presentation failures are suppressed.

These diagnostics help investigate startup failures; this release does not claim
to fix the reported ReShade/RenoDX startup issue.

## Installation and updating

Close the game and back up PirateHatHUD.ini. Drag PirateHatHUD-1.0.2.zip into
CDUMM or DMM, enable Pirate Hat HUD on the ASI page, and make sure the ASI loader
is enabled. When updating, enable the existing mod before importing the archive.
Restart the game. Keep only one installed copy of the ASI.

There are no configuration changes. Existing INI files remain compatible and the
default logging level is still info. The default icon and sound are embedded.

## Reporting a startup problem

For detailed diagnostics, set this in PirateHatHUD.ini before launching:

```ini
[logging]
level=debug
```

Send the newest PirateHatHUD_*.log from the failed launch. If ReShade/RenoDX is
installed, also include ReShade.log from the same launch, ReShade.ini, and the
active preset named by PresetPath when applicable. Review files before sharing;
ReShade configuration and logs can contain local paths. The mod's new diagnostic
records omit installation paths, usernames, GPU serials and adapter LUIDs, and
remain local. The mod does not upload files or collect ReShade configuration.

Restore level=info after troubleshooting. GPU details require renderer
initialization and can be absent when startup stops earlier. File versions and
module presence alone do not establish whether an add-on is active.

## Compatibility and validation

Windows x64 and DirectX 12. Observation layout was previously checked on Crimson
Desert 2.03.02; the Windows executable file version can differ from the public
patch number.

The Release build, architecture-check, formatting, all 28 CTest tests and all 25
research-tool tests passed.

The diagnostic Release candidate launched on executable version 1.0.0.2976 with
an RTX 5080, ReShade 6.8.0.2155 and RenoDX 0.2026.918.611. Logs confirmed game-hook
activation, graphics callback progress and successful overlay initialization.
A ResizeBuffers call returned E_INVALIDARG during startup, followed by successful
presentation and overlay initialization. The separately reported startup failure
was not reproduced and remains undiagnosed.

The final versioned 1.0.2 package has not had a separate in-game test. Treasure
notification behavior, F9/F10 and graphics setting changes remain to be rechecked
with that package; other GPUs and systems have not been verified for this release.

## Credits

- Icons by [Icons8](https://icons8.com/).
- Sound: "Subscribe alert - metal dings" by Roy's Noise, via [Uppbeat](https://uppbeat.io/).
- SafetyHook, Zydis and Dear ImGui developers; CDUMM and DMM developers.
