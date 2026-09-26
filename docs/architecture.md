# Module boundaries

The project builds one ASI and two static libraries. Modules are linked into the ASI;
they are not dynamically loaded plugins.

## Required layout and dependencies

All project headers belong under `include/`; implementations belong under `src/`.
Module directories match across these roots. Internal headers use this same layout;
only explicitly documented interfaces are intended for library consumers.

| Module | Allowed project dependencies | External dependencies |
| --- | --- | --- |
| `core` | Its own headers | Standard library |
| `game` / `game_observers` | Its own headers, `core`, scanner and patterns | Windows, SafetyHook |
| `features` / `treasure_indicator` | Its own headers, `game/treasure_observer.hpp`, `render/hud_state.hpp` | Standard library |
| `render` | Its own headers, including `HudState` | Windows, DX12/DXGI, WIC, ImGui, SafetyHook for graphics hooks |
| `overlay` facade | Its own header, `render` | Standard library |
| `platform` | Its own headers | Standard library, Windows |
| `app` and DLL entry | All modules for composition | Windows, standard library |

The current feature interface includes the observer header for state types; it does
not create or manage observers. If more features need those types, extract a small
game-state header and update this contract and the checker together.

Game observers must not include rendering or mod configuration. Rendering receives
HUD state and must not read game memory or include game observers. Platform utilities
must not acquire game or graphics responsibilities. Application composition wires
the modules and owns their lifetimes. Adding a module requires an explicit place in
this matrix and `phi_module()` in `cmake/Architecture.cmake`.

The checker enforces direct literal project includes, layout, selected external
header restrictions, and the actual direct/interface link dependencies of the three
production CMake targets. It is not a full C++ dependency analyzer: macro-generated
includes, transitive standard-library includes, forward declarations and behavioral
thread/lifetime contracts still require review. Do not use those gaps to bypass a boundary.

Run `cmake --build build --target architecture-check`, or run
`cmake -P cmake/Architecture.cmake` without configuring. The default build runs source
checks; CMake configuration validates target links. CTest runs both the source checker
and its regression cases. Changes to boundaries must update documentation and checks
in the same task, including any constraints passed to delegated agents.

- `game_observers`: game-specific scanning, hooks and safe memory reads. Public entry
  point: `include/game/treasure_observer.hpp`. No HUD, configuration or DirectX dependency.
- `core/signal.hpp`: reusable typed synchronous signals and move-only RAII subscriptions.
- `treasure_indicator`: presentation policy for this mod, producing a `HudState`.
- `overlay`: DX12 resources and drawing from a coherent `HudState` snapshot.
- `platform`: INI configuration, Windows hotkeys and file logging.
- `app`: composition, polling and shutdown. `main.cpp` only starts the worker and unloads.

## Renderer internals

`src/overlay.cpp` is a small application-facing facade. Implementations live under
`src/render`, and their headers live under `include/render`, following the project-wide
header/source layout. The application uses `include/overlay.hpp` and `HudState`; the
DX12 and hook headers describe internal renderer implementation rather than a public
library API:

- `dxgi_hooks`: hook installation/removal, swapchain/queue selection and callback draining.
- `dx12_renderer`: owns GPU/ImGui state, initialization, rendering and resize lifecycle.
- `dx12_frames`: frame buffers, fences and descriptor allocation for that renderer.
- `dx12_texture`: icon GPU allocation and upload commands for that renderer.
- `image`: WIC decoding into CPU RGBA pixels, independent of DX12 and hooks.
- `hud_draw`: emits the ImGui draw command using HUD placement and a texture handle.

The hook module serializes renderer access with its graphics mutex. The facade uses a
separate mutex for HUD snapshots, so publishing HUD state does not wait for GPU fences.
Descriptor callbacks recover their renderer from ImGui backend UserData rather than a
global graphics object. Image preparation and logger configuration happen before start;
logger clearing happens after stop. The facade prevents image replacement while running.

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
observer startup failure without the game, and WIC image decoding/retry. Game-hook activation and DX12 drawing
require in-game verification: force_show, toggle/unload, normal treasure detection,
game version, graphics settings and logs.
