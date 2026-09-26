# Module boundaries

The project builds one ASI and two static libraries. Modules are linked into the ASI;
they are not dynamically loaded plugins.

- `game_observers`: game-specific scanning, hooks and safe memory reads. Public entry
  point: `include/game/treasure_observer.hpp`. No HUD, configuration or DirectX dependency.
- `core/signal.hpp`: reusable typed synchronous signals and move-only RAII subscriptions.
- `treasure_indicator`: presentation policy for this mod, producing a `HudState`.
- `overlay`: DX12 resources and drawing from a coherent `HudState` snapshot.
- `platform`: INI configuration, Windows hotkeys and file logging.
- `app`: composition, polling and shutdown. `main.cpp` only starts the worker and unloads.

## Using an observer

```cpp
phi::TreasureObserver observer;
auto subscription = observer.subscribe([](const phi::TreasureStateChanged& event) {
  // Respond to event.current (unknown, inactive or active).
});
if (observer.start()) {
  observer.poll(); // Call periodically from the same owner thread.
}
observer.stop();
```

Keep the subscription token alive while subscribed. Destruction or `reset()` unsubscribes.
A token may outlive its source. Query `state()` for the latest sampled state; subscribing
does not replay an event. `unknown` means no valid observation, not an inactive counter.
`stop()` resets the state to unknown; a subsequent `poll()` publishes that change if needed.

All observer operations, signal dispatch and token destruction happen on the owner
thread. Game hooks only capture data in atomics. Subscribers run synchronously inside
`poll()`, never on the game hook thread. Sampling reports the latest state and may miss
intermediate transitions between polls. Callback exceptions propagate to the caller.
Do not destroy an observer from its own callback or let subscriber exceptions escape
the application's worker entry point.

Signals preserve subscription order. Unsubscription takes effect during the current
dispatch; a new subscription participates starting with the next publish. Recursive
publish takes a new snapshot and may include subscribers added by the outer dispatch.
Future observers should expose their own typed events and current state, reusing Signal;
no global string-based bus is required.

## Reuse and lifecycle

Another mod can link `game_observers` without linking the indicator or ImGui. The static
library still requires Windows x64, SafetyHook, and a compatible Crimson Desert build.
Public observer types hide SafetyHook and game addresses. Scanner headers and patterns
remain implementation details, even though they live under the shared include directory.
Packaging/export rules and a stable DLL ABI are deliberately not implemented yet.

Only one observer can install treasure hooks per linked copy of the library. Separate
mods have separate copies and can still compete over the same instructions. A shared
DLL/ASI provider needs a separate design if simultaneous use becomes necessary.

Shutdown stops observation, releases subscriptions, stops graphics, then closes the log.
Hook disabling and callback draining retain the existing SafetyHook approach. A drained
C++ callback count does not prove all threads have exited the generated MidHook stub;
full in-game DLL unload safety is not established by these unit tests.

## Validation

CTest covers scanning, typed subscription lifetime/dispatch behavior, indicator policy,
and observer startup failure without the game. Game-hook activation and DX12 drawing
require in-game verification: force_show, toggle/unload, normal treasure detection,
game version, graphics settings and logs.
