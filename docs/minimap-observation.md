# Minimap visibility observation

The icon requires a valid visible minimap sample, including in force_show mode.
Unknown memory or a byte other than 0/1 hides the icon; polling retries naturally.
Treasure state is retained while menus hide the minimap.

Timing caveat: the current byte lags menu opening, and the user observed the HUD
over the map closing animation. The checks below established eventual state,
not timely transitions. RTTI enumeration identifies the original array element
as RootStatusGauge (index 5), with a separate RootMiniMap at index 6. No earlier
replacement signal for the minimap has been validated yet.

The user verified this memory chain on Crimson Desert 2.03.02 in Cheat Engine:
menu and inventory transitions, minimap setting disabled/enabled, teleport and one
full game restart. The compiled observer/HUD integration still requires in-game
verification, including force_show, toggle/stop hotkeys, normal treasure detection,
load/save transitions and graphics settings/logs.

The root pointer slot is CrimsonDesert.exe RVA 0x6C8CC00. Read its pointer, then
read pointers at these offsets in traversal order:
30, 18, 88, 78, 0, 30EB8, 28, A0, 10, 48, 0, 290, 18 (all hexadecimal).
Finally read a byte at +BE: 0 hidden, 1 visible.

Runtime now resolves the root pointer slot from a unique executable launcher
constructor signature. Its `mov [rip+disp32],rbx` operand supplies the slot address;
the slot must be aligned and inside the loaded image. Missing or ambiguous code
rejects startup, without falling back to the historical RVA.

The current UI array is enumerated, and exact game-module MSVC script RTTI
`.?AVUIGamePlayControlRootStatusGauge@uiCommonScript@pa@@` selects the render root.
The outer index 5 is no longer used. Duplicate identities or unreadable relevant
entries yield unknown. Immutable RTTI classifications are cached; live heap links
are re-read. From the identified root, traversal remains 48,0,290,18 followed by
byte +BE. Owner[0], child[0] and leaf[3] remain historical selections. Shared
controller RTTI cannot distinguish the 17 leaf widgets; no automatic leaf repair
is claimed. Future instruction context, structure or field semantics changes
can still require CE research. Compatibility beyond the tested build is unverified.

MinimapObserver belongs to game_observers, has no rendering/config dependencies,
installs no hooks and is restartable. start resolves the executable module; poll
samples and publishes state changes on the owner thread. stop resets state to
unknown; a subsequent poll publishes the reset if needed. App stops observers,
releases both subscriptions and then stops rendering. The renderer receives one
snapshot after both observers have polled. Reads validate committed readable
regions and use ReadProcessMemory to tolerate unmapping races; failures hide the
icon. Startup and state transitions are logged, avoiding per-poll diagnostic noise.
