# Pirate Hat HUD 0.3.0 DX12 preview

## Current development build

The icon now follows minimap visibility: it hides in menus that hide the minimap and stays hidden when the minimap is disabled in game settings. This also applies to `force_show=1`. Treasure state is retained while hidden. Invalid or unavailable minimap observations hide the icon until reading recovers.

The minimap pointer chain was verified in Cheat Engine on game 2.03.02 across menu/inventory transitions, minimap settings, teleport and a full restart. Its root RVA and array slots are specific to the tested build; compatibility with other builds is not established. See [minimap observation](docs/minimap-observation.md) for the chain and limitations.

The Release bundle is `build/ninja/Release/PirateHatHUD/`. Architecture-check, formatting and 12/12 CTest tests pass. In-game validation of the compiled feature remains pending: check normal treasure detection and `force_show`, menus, minimap settings, save/load, F9/F10 and relevant graphics settings. Record the game version and log with results.

## Original 0.3.0 preview validation

The treasure counter scanner and read-only observer were confirmed in game on Crimson Desert 1.0.0.2976. This update replaces dormant DX11 code with a DX12 Present renderer and a force_show=1 diagnostic. The Release ASI compiles and scanner tests pass. The icon has not yet been observed in game, including with DLSS or Frame Generation. This is a test preview, not a confirmed Nexus main-file release.
