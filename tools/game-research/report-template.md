# Crimson Desert <version> compatibility report

- Date / reviewer:
- Game version and build source:
- Mod commit / version:
- CE / MCP version:
- Graphics settings, HDR, DLSS, Frame Generation:
- Local snapshot and log paths:

## Addresses and signatures

| Check | Result | Evidence / file |
| --- | --- | --- |
| Treasure: unique pair, delta 0x2C | Not checked | |
| Menu: unique pair, delta 0x141 | Not checked | |
| UI root slot and referencing instruction | Not checked | |
| UI chain and unique Root_MainMenu RTTI | Not checked | |
| Minimap object and visibility field | Not checked | |

## Behavior checks

| Scenario | Result | Log / observation |
| --- | --- | --- |
| Normal treasure detection | Not checked | |
| Esc / map / inventory: opening and closing | Not checked | |
| Startup with an open menu | Not checked | |
| Minimap disabled / enabled | Not checked | |
| force_show | Not checked | |
| F9 / F10 | Not checked | |
| Loading / teleport / fresh process | Not checked | |

## Outcome

- Changed signatures / offsets and reason:
- architecture-check / CTest:
- Unverified behavior:
- Compatibility: not established / confirmed for the listed scenarios.
