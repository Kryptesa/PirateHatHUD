# Minimap visibility observation

The icon requires a valid visible minimap sample, including in force_show mode.
Unknown memory hides the icon; polling retries naturally. Treasure state is
retained while menus or cutscenes hide the minimap.

## Runtime sampling

The UI root pointer slot comes from a unique executable launcher constructor
signature. Its RIP-relative store supplies the slot address, which must be
aligned and inside the loaded image. Missing or ambiguous code rejects startup;
there is no historical RVA fallback.

The observer follows the shared guarded UI owner chain and enumerates the live
UI array. Exact game-module MSVC script RTTI
`.?AVUIGamePlayControlRootMiniMap@uiCommonScript@pa@@`
selects exactly one render root. Array position is irrelevant; duplicate roots
or failed relevant reads yield unknown. Only immutable RTTI information is cached.

From the render root, +118 supplies the script and +120 the native definition
named `MinimapView`. Script +8 must identify `MinimapHudBody`; its +3F0 binding
supplies the native minimap canvas. Canvas +8 must identify `MinimapCanvas`,
and definition +D8 must point back to that canvas. Definition names are exact,
including their terminators, through the C-string pointer at +F8.

Sampling follows definition +38 parents from the canvas to the identified view.
The chain must include the owning script body. At most 24 definitions are read;
null links, cycles, excessive depth and an unrelated view yield unknown. Every
definition in the chain must pass these native draw admission conditions:

- Definition +B0: `(flags & 0x60) == 0x40` and bit 0x80 set.
- Definition +97: clipping bit 0x20 clear.
- Definition +C0 points to computed properties; float +40 is positive.

Zero opacity or a rejected draw flag yields hidden. Missing properties,
non-finite opacity, opacity outside [0, 1], or any failed relevant read yields
unknown, even if another ancestor already indicated hidden. All required reads
are completed before publishing a known state.

All heap links, names and properties are read afresh on each poll. The binding
and field offsets depend on the game version. Transient changes between reads
can yield unknown. The observer samples draw flags, clipping and opacity.

## Live evidence on Crimson Desert 2.03.02

Read-only research traced the native canvas virtual draw dispatch to the UI
draw admission function, which checks flags, clipping and computed opacity.
The observed parent chain was:

`MinimapCanvas -> NoStickyIconContainer -> MinimapScaleLayer ->
MinimapMaskContainer -> MinimapDisplayWrap -> MinimapHud ->
UIHudScaleMinimapHudWrap -> MinimapHudBody -> MinimapView`.

A 100-second capture at 100 ms intervals sampled the canvas and five ancestors,
plus the old proxy and menu state. All 20 fields remained readable:

- Visible gameplay: sampled draw flags D0, opacity 1.0.
- Hide setting enabled in gameplay: canvas and sampled ancestors changed to 50.
- Menu open: canvas could retain D0 and positive opacity, while the view changed
  to 90 and its opacity became zero.
- Returning to gameplay and disabling hiding restored the relevant flags.

A second capture sampled 270 times over 42 seconds at 150 ms intervals, with no
unreadable fields. During a dialogue/cutscene, the view changed
D0 -> 90 -> D0 while the menu byte stayed zero. Sampled opacity fell to about
0.0885, then returned to 1.0. The minimap disappeared and reappeared during
the same sequence. Positive opacity alone would incorrectly report visible;
the view's rejected draw flag supplies the necessary ancestor gating.

Earlier research used byte +BE of a CommonInfoDescription named
`CoolTimeToolIcon4` beneath RootStatusGauge as an empirical proxy. That byte
tracked several transitions but belonged to a status icon. The current observer
has replaced it with the native minimap canvas and ancestor checks.

## Validation and remaining checks

Unit tests cover canvas/ancestor draw gates, clipping, opacity, failed reads,
exact names, ownership, nulls, cycles, bounded depth, duplicate roots, outer
array reordering, replaced render roots/canvases and startup without the game.

The compiled ASI passed the in-game check on 2026-09-27 after a full game restart
with force_show enabled: visible gameplay; hiding in menus and the map; delayed
return; the hide-minimap setting; dialogue/cutscene hiding and return; continued
operation after save loading and teleport. Return timing was checked visually,
without an instrumented measurement. Game version, graphics settings and logs
were not recorded for this pass.

Remaining compiled checks: F9/F10 and normal treasure detection with force_show
disabled.

MinimapObserver belongs to game_observers and has no rendering/config dependency.
It publishes typed state changes from poll on the owner thread. Stop resets state
to unknown; the observer can restart. Application composition polls all sources
before publishing a coherent HUD snapshot and resets subscriptions before
graphics shutdown. The guarded memory reader validates committed readable
regions and uses ReadProcessMemory to tolerate unmapping races.
