# Pirate Hat HUD 1.0.1

A chest icon and short sound announce nearby treasure detected by the Pirate King
Hat, including when helmets are hidden. The hat must be equipped.

## Changes since 1.0.0

- Fixed crashes when switching upscalers or frame generation during gameplay.
  The overlay now waits for a successful presentation before recreating its
  backbuffers after resize, and pauses renderer work while resize callbacks are
  in progress.
- Added a DXGI/WARP regression test for successive resizes, presentation gating,
  renderer recovery and failed-resize handling.

## Installation and updating

Close the game and back up PirateHatHUD.ini. Drag PirateHatHUD-1.0.1.zip into
CDUMM or DMM, enable Pirate Hat HUD on the ASI page, and make sure the ASI loader
is enabled. When updating, enable the existing mod before importing the archive.
Restart the game. Keep only one installed copy of the ASI.

There are no configuration changes in this update. The default icon and sound are
embedded; no separate PNG or WAV is needed. See the [README](README.md) for manual
installation, customization and troubleshooting.

## Compatibility and validation

Windows x64 and DirectX 12. The observation layout was previously checked on
Crimson Desert 2.03.02. The Windows executable file version in the log can differ
from the public patch number.

The Release build, architecture-check, formatting check, all 28 CTest tests and
all 25 research-tool tests passed.

The user reproduced crashes when switching upscalers and frame generation with
1.0.0, then confirmed those switches no longer crashed with the corrected
RelWithDebInfo candidate on executable version 1.0.0.2976. A separate ReShade test
did not reproduce the reported startup failure, even with the old mod build.
The exact upscaler/frame-generation combinations, GPU and HDR settings were not
recorded. The separately reported startup failure remains undiagnosed.

The versioned 1.0.1 Release package has not had a separate in-game test. Normal
treasure detection, menu/minimap gating, sound, F9/F10, HDR and other PCs remain
to be rechecked for this release.

## Credits

- Icons by [Icons8](https://icons8.com/).
- Sound: "Subscribe alert - metal dings" by Roy's Noise, via [Uppbeat](https://uppbeat.io/).
- SafetyHook, Zydis and Dear ImGui developers; CDUMM and DMM developers.
