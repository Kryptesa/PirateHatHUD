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
| `features` / `treasure_indicator` | Its own headers, `game/treasure_observer.hpp`, `game/minimap_observer.hpp`, `render/hud_state.hpp` | Standard library |
| `render` | Its own headers, including `HudState` | Windows, DX12/DXGI, WIC, ImGui, SafetyHook for graphics hooks |
| `overlay` facade | Its own header, `render` | Standard library |
| `platform` | Its own headers | Standard library, Windows |
| `app` and DLL entry | All modules for composition | Windows, standard library |

The current feature interface includes the observer headers for state types; it does
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
  points: `include/game/treasure_observer.hpp` and `include/game/minimap_observer.hpp`. No HUD, configuration or DirectX dependency.
- `core/signal.hpp`: reusable typed synchronous signals and move-only RAII subscriptions.
- `treasure_indicator`: presentation policy for this mod, producing a `HudState`.
- `overlay`: DX12 resources and drawing from a coherent `HudState` snapshot.
- `platform`: INI configuration, Windows hotkeys and file logging.
- `app`: composition, polling and exception-safe shutdown. `main.cpp` starts the worker
  and unloads only when the application explicitly permits it.

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

Stop results distinguish disabling hooks, draining callback bodies, and releasing GPU
resources from permission to unload the DLL. Once hook activation has been attempted,
generated hook code is retained until process exit, and this module instance cannot restart.
The facade retains the renderer allocation if callbacks or GPU work cannot safely release it.
These process-lifetime allocations must not acquire automatic destructors that free them
during DLL detach.

The renderer tracks waiting, ready, resizing, faulted and stopped states. Normal drawing
skips an unfinished frame rather than blocking Present. Shutdown and resize use a shared
bounded fence-wait deadline; unresolved submissions retain their resources. Device loss
stops drawing. Retaining backbuffer references can prevent ResizeBuffers from succeeding;
this is an explicit failure mode, not a promise of recovery.

The HUD supports only image commands for its prepared icon. Backend texture uploads are
excluded from draw submission, avoiding the ImGui DX12 backend's unbounded texture-upload
wait. Adding text, user draw callbacks or other textures requires a separately reviewed
upload lifecycle rather than removing the command validation.

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
Once hook activation has been attempted, this observer instance and the linked library
are retired after stop; retries are supported only after failures before activation.

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

`MinimapObserver` installs no hooks. It samples a guarded pointer chain in `poll()`
and publishes typed visibility changes on the owner thread. Unknown observations
suppress the icon, including in force_show mode. Treasure state is retained while
the minimap is hidden. Application composition polls both observers before
publishing one HUD snapshot, then stops both observers and resets subscriptions
before graphics shutdown. The minimap observer may restart after stop; treasure
hook retirement rules do not apply to it. See [minimap observation](minimap-observation.md)
for the 2.03.02 chain, fixed-slot limitations and validation evidence.

Another mod can link `game_observers` without linking the indicator or ImGui. The static
library still requires Windows x64, SafetyHook, and a compatible Crimson Desert build.
Public observer types hide SafetyHook and game addresses. Scanner headers and patterns
remain implementation details, even though they live under the shared include directory.
Packaging/export rules and a stable DLL ABI are deliberately not implemented yet.

Only one observer can install treasure hooks per linked copy of the library. Separate
mods have separate copies and can still compete over the same instructions. A shared
DLL/ASI provider needs a separate design if simultaneous use becomes necessary.

Shutdown stops observation, releases subscriptions, stops graphics, then closes the log.
The application uses RAII and catches C++ exceptions at the worker boundary. Partial
startup is stopped too; any need to retain the module survives subsequent cleanup.
A drained C++ callback count does not prove all threads have exited generated hook code.
F10 therefore disables observation and rendering and finishes the worker, but retains the
DLL and any necessary hook/GPU allocations until process exit after hook activation.
Physical DLL unload is allowed only on paths that have not exposed hook code to execution.
Full in-game DLL unload safety is not established by these unit tests.

## Validation

CTest covers scanning, typed subscription lifetime/dispatch behavior, indicator policy,
observer startup failure without the game, hook retention and partial activation,
application cleanup under exceptions, bounded GPU wait policy, and WIC image decoding/retry.
Minimap tests cover the pointer traversal, read failures, null pointers, address
overflow, invalid visibility bytes and startup without the game. Indicator and app
tests cover minimap gating, force_show and minimap lifecycle exceptions. The chain
was separately verified in Cheat Engine on 2.03.02; compiled feature integration
still requires in-game validation.
Game-hook activation and DX12 drawing
require in-game verification: force_show, toggle/unload, normal treasure detection,
game version, graphics settings and logs.
