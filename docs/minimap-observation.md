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

The RVA is version-specific. No version detection or signature for the root is
implemented. Fixed array slots were empirically verified, not identified by type
or name. Updates or UI reconstruction may select another readable object, and
safe memory reads cannot detect a semantically wrong but readable chain. Do not
claim compatibility beyond the tested build. Investigate and update the chain or
add object identification/root signature support when the game changes.

MinimapObserver belongs to game_observers, has no rendering/config dependencies,
installs no hooks and is restartable. start resolves the executable module; poll
samples and publishes state changes on the owner thread. stop resets state to
unknown; a subsequent poll publishes the reset if needed. App stops observers,
releases both subscriptions and then stops rendering. The renderer receives one
snapshot after both observers have polled. Reads validate committed readable
regions and use ReadProcessMemory to tolerate unmapping races; failures hide the
icon. Startup and state transitions are logged, avoiding per-poll diagnostic noise.
