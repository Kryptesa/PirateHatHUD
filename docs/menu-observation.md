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

The UI owner is resolved from module +6C8CC00 through 30,18,88,78,0.
The array is owner +30EB8, count is uint32 at +30EC0 (bounded to 4096).
Each entry follows +A0, +10 to its render root; root +118 identifies the script.
MSVC x64 RTTI must exactly match
`.?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@`, with locator image
base matching the game module. Multiple matches or read failures yield unknown.
No heap address or fixed UI array index is used. The root slot remains version-specific.

Startup reads +25B immediately; poll resolves current identity again and reads the
state. Hook callbacks capture only atomics and never refer to the observer allocation.
An opening is latched until poll, so a rapid open/close cancels the return deadline.
Closing settles through memory sampling; a racing write may defer publication one poll.
Only poll dispatches typed events and logs on the owner thread. Hook activation uses
the existing ObserverHooks retention policy, including failed/partial activation.

Policy tests cover independent event order, 999/1000 ms boundary, cancellation,
reopening, unknown sources and force_show gating. Memory tests cover identity,
ambiguity, read failures, invalid bytes and startup outside the game. Application tests
cover menu lifecycle, failures and DLL retention. These are not in-game verification.

Pending compiled integration checks: Esc/M/I, cutscenes, minimap setting disabled,
startup with an open menu, F9/F10, teleport and fresh process restart. Record game
version, graphics settings and logs. Measure any map animation remaining after return
and adjust `[indicator] show_delay_ms` (default 1000, valid 0..60000).
