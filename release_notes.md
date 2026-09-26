# Pirate Hat HUD 0.3.0 DX12 preview

## Current development build

The distribution now targets CDUMM's ASI importer: `PirateHatHUD.asi` and `PirateHatHUD.ini` install together into `bin64`. The default icon is embedded because CDUMM does not copy PNG companions. An optional manually installed `PirateHatHUD.png` overrides it. Back up old settings before import and transfer them into `PirateHatHUD.ini`; the old generic `config.ini` is only a fallback when the new file is absent. The archive also includes generated `modinfo.json`, the project license and dependency notices/licenses. Release, architecture-check, formatting and all 13 CTest tests pass. ZIP contents, embedded ASI PNG bytes and install/config discovery/disable/enable/uninstall were checked using CDUMM's `AsiManager` in a temporary directory. An upstream uninstall limitation was reproduced after updating a disabled plugin and later enabling it; see README. CDUMM UI and in-game validation remain pending.

The icon now follows minimap visibility: it hides in menus that hide the minimap and stays hidden when the minimap is disabled in game settings. This also applies to `force_show=1`. Treasure state is retained while hidden. Invalid or unavailable minimap observations hide the icon until reading recovers.

The minimap pointer chain was verified in Cheat Engine on game 2.03.02 across menu/inventory transitions, minimap settings, teleport and a full restart. Its root RVA and array slots are specific to the tested build; compatibility with other builds is not established. See [minimap observation](docs/minimap-observation.md) for the chain and limitations.

The Release bundle is `build/ninja/Release/PirateHatHUD/`. Architecture-check, formatting and 13/13 CTest tests pass. In-game validation of the compiled feature remains pending: check normal treasure detection and `force_show`, menus, minimap settings, save/load, F9/F10 and relevant graphics settings. Record the game version and log with results.

## Original 0.3.0 preview validation

The treasure counter scanner and read-only observer were confirmed in game on Crimson Desert 1.0.0.2976. This update replaces dormant DX11 code with a DX12 Present renderer and a force_show=1 diagnostic. The Release ASI compiles and scanner tests pass. The icon has not yet been observed in game, including with DLSS or Frame Generation. This is a test preview, not a confirmed Nexus main-file release.
