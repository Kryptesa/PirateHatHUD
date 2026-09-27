# Menu observation (Crimson Desert 2.03.02)

Saved hardware traces cover Esc, M and I, with closing through Esc or the screen key.
Root_MainMenu +25B is the common menu target state, rather than a screen-specific
visibility flag. The game writes it roughly 100?170 ms after sampled input; hooking
these writes does not remove that delay.

The executable scanner requires exactly one instruction pair in executable sections:

- Clear at historical RVA 3D356F0: `C6 81 5B 02 00 00 00 84 D2 74 1C`;
  hook at the initial instruction, object in RCX.
- Set at historical RVA 3D35831: `C6 83 5B 02 00 00 01 48 8B 01 FF 50 30`;
  hook at the initial instruction, object in RBX.
- Pair distance 0x141. Historical RVAs are diagnostic only, not hook addresses.

Only the four disp32 bytes in each menu MOV are masked. Exact C6 /0 ModRM
encodings retain byte width, RCX/RBX bases without an index, and immediates 0/1.
Both positive displacements must agree and be at most 0x10000; this resolved
offset is used for startup and polling. Context bytes and delta 0x141 remain
constraints, so broader code changes still require a signature update.

The UI owner is resolved from a unique launcher constructor signature through
30,18,88,78,0. The RIP-relative store operand supplies the slot (historical
module +6C8CC00); there is no runtime fallback to that RVA.
The array is owner +30EB8, count is uint32 at +30EC0 (bounded to 4096).
Each entry follows +A0, +10 to its render root; root +118 identifies the script.
MSVC x64 RTTI must exactly match
`.?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@`, with locator image
base matching the game module. Multiple matches or read failures yield unknown.
No heap address or fixed UI array index is used. The root slot is resolved from code.
Null entries and links are accepted as empty slots; failed reads in non-null objects
invalidate uniqueness even when another object matches. Script RTTI is cached only
where its locator/vtable storage belongs to non-writable game-image sections and
read-only mapped pages. Writable type names are still read on every poll. The entire
live array and all heap links are traversed each time; no root ownership is cached.

Startup reads the resolved state offset immediately (historically +25B); poll resolves current identity again and reads the
state. Hook callbacks capture only atomics and never refer to the observer allocation.
An opening is latched until poll, so a rapid open/close cancels the return deadline.
Opening and transition generation share one atomic value, preventing split-flag races.
Closing settles through memory sampling; a racing write may defer publication one poll.
Only poll dispatches typed events and logs on the owner thread. Hook activation uses
the existing ObserverHooks retention policy, including failed/partial activation.

Policy tests cover independent event order, 999/1000 ms boundary, cancellation,
reopening, unknown sources and force_show gating. Memory tests cover identity,
ambiguity, read failures, invalid bytes and startup outside the game. Application tests
cover menu lifecycle, failures and DLL retention. These are not in-game verification.

The deterministic two-object fixture uses 24 reads on the cold identity pass and 18
after immutable RTTI is cached. Partial caching with a writable type name saves the
locator reads while continuing to detect name failures and changes. These counts
measure reader calls, not game CPU time; profile actual polling duration and UI array
size in game before changing the polling interval or caching live root identities.
The 257-entry fixture with two fully immutable script types reduces reader calls from
2064 to 1293 while still inspecting every live entry and detecting duplicates.

The compiled ASI passed the in-game check on 2026-09-27 for menu/map
hiding and delayed return, dialogue/cutscene hiding, the hide-minimap setting,
save loading and teleport in the requested fresh-process force_show check.
See [minimap observation](minimap-observation.md) for the scope and evidence limits.
Specific Esc/M/I coverage, startup with an open menu and F9/F10 remain unchecked.
Game version, graphics settings and logs were not recorded for this pass.
The return animation timing has not been instrumented; adjust
`[indicator] show_delay_ms` (default 1000, valid 0..60000) if needed.
