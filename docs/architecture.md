# Module boundaries

The project builds one ASI and two static libraries linked into it.

## Required layout and dependencies

All project headers belong under `include/`; implementations belong under `src/`.
Module directories match across these roots. Internal headers use this same layout;
only explicitly documented interfaces are intended for library consumers.

| Module | Allowed project dependencies | External dependencies |
| --- | --- | --- |
| `core` | Its own headers | Standard library |
| `scanner` (`pattern_scan`) | Its own header | Windows, standard library |
| `game` / `game_observers` | Its own headers, `core`, scanner | Windows, SafetyHook |
| `features` / `treasure_indicator` | Its own headers, `game/observer_state.hpp`, `render/hud_state.hpp` | Standard library |
| `render` | Its own headers, including `HudState`, `core` | Windows, DX12/DXGI, D3DCompiler, WIC, ImGui, SafetyHook for graphics hooks |
| `overlay` facade | Its own header, `render`, `core` | Standard library |
| `platform` | Its own headers, `core` | Standard library, Windows |
| `app` and DLL entry | All modules for composition | Windows, standard library |

`game/observer_state.hpp` contains the shared state/event values and observer stop
result, independent of observer objects, Windows and SafetyHook. Features consume
these values without depending on observer lifecycle APIs. The source checker
rejects feature includes of concrete observer headers.

Game observers must not include rendering or mod configuration. Rendering receives
HUD state and must not read game memory or include game observers. Platform utilities
must not acquire game or graphics responsibilities. Application composition wires
the modules and owns their lifetimes. Adding a module requires an explicit place in
this matrix and `phi_module()` in `cmake/Architecture.cmake`.

The checker enforces direct literal project includes, layout, selected external
header restrictions, and the actual direct/interface link dependencies of the three
production CMake targets. It is not a full C++ dependency analyzer: macro-generated
includes, transitive standard-library includes, forward declarations and behavioral
thread/lifetime contracts still require review.

Run `cmake --build build --target architecture-check`, or run
`cmake -P cmake/Architecture.cmake` without configuring. The default build runs source
checks; CMake configuration validates target links. CTest runs both the source checker
and its regression cases. Changes to boundaries must update documentation and checks
in the same task, including any constraints passed to delegated agents.

- `pattern_scan`: generic masked byte matching, optional candidate validation and
  unique matching across executable PE sections. It has no game signatures, object
  types, hook sites or field offsets; dependencies on `game` are forbidden.
- `game/treasure/scan` and `game/menu/scan`: feature-specific hook signatures;
  menu scanning also validates the state displacement.
  `game/shared/ui_root_scan`: launcher signature, RIP operand extraction and UI slot checks.
  `game/treasure/patterns.hpp` holds the treasure constants. These use the generic scanner
  and expose explicit functions instead of boolean selectors.
- `game_observers`: game-specific scanning, hooks and safe memory reads. Public entry
  points: `include/game/treasure_observer.hpp`, `include/game/minimap_observer.hpp`,
  `include/game/menu_observer.hpp` and `include/game/audio_volume_observer.hpp`.
  No HUD, configuration or DirectX dependency.
  Internal files are grouped under `treasure/`, `minimap/`, `menu/` and `audio/`;
  their observer implementations follow the same grouping. Public observer headers
  and `observer_state.hpp` stay directly under `include/game/`.
  `shared/` contains guarded memory access, address arithmetic, UI identity resolution
  and hook retention used across observers. Feature-specific signatures and layout
  checks stay with their owner. Headers and implementations mirror these paths.
  A shared `MemoryReader` callback allows simulated-memory tests to exercise
  the same compiled implementation as production. Observer files own lifecycle
  and event publication, while memory files own traversal and state interpretation.
  These directories remain one `game` module and one static library: they do not
  introduce new dependency boundaries. The architecture checker already classifies
  nested paths and continues to reject game dependencies on configuration or graphics.
- `core/signal.hpp`: reusable typed synchronous signals and move-only RAII subscriptions.
- `treasure_indicator`: presentation policy for this mod, producing a `HudState`.
- `features/treasure_sound`: notification policy using observer state values and a
  steady-clock cooldown, independent of HUD visibility and return delay.
- `overlay`: DX12 resources and drawing from a coherent `HudState` snapshot.
- `platform`: INI configuration, Windows hotkeys, file logging and asynchronous WAV
  playback through `platform/sound`.
  Hotkeys register owner-thread WM_HOTKEY delivery with MOD_NOREPEAT while the game
  is foreground, falling back per unavailable binding to key-state edge detection.
  Registrations and atom IDs are released on focus loss and during application
  cleanup on that same thread, including exceptional exits.
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
- `dx12_hdr` and `hdr_shader`: icon-only HDR pipeline, converting sRGB to linear
  scRGB or Rec.2020/ST.2084 at 203 nits. Graphics hooks track successful
  SetColorSpace1 calls per swapchain. FP16 defaults to scRGB; pre-existing 10-bit
  chains remain SDR until their color-space selection is observed. HDR10 alpha
  blending is performed in PQ space and is approximate at translucent edges.
- `hdr_bytecode`: compiles and validates shader bytecode on the application thread
  before hook activation. Bytecode survives resize and device replacement; graphics
  callbacks create device resources but never invoke the shader compiler.
- `image`: WIC decoding of files or the embedded PNG resource into CPU RGBA pixels, independent of DX12 and hooks.
- `hud_draw`: emits the ImGui draw command using HUD placement and a texture handle.

Swapchain creation records an explicit queue association but does not select a HUD
target. The first eligible Present must belong to the foreground, visible, unowned
top-level window in the current process. Selection then stays with that HWND while
unfocused; a newer creation-observed chain can replace the target only when it presents
to the same window. A late-discovered pre-existing chain cannot supersede it. Migration
to a different HWND is not supported by the renderer. Private DXGI generation data
prevents recycled COM addresses from inheriting queues/color spaces. The bounded
registry retains queues, not swapchains, preserves the active record, and suppresses
drawing when a known association has been lost. Fallback queue ambiguity is tracked
separately for each device and fails closed on capacity overflow.

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

The renderer tracks waiting, ready, resizing, faulted and stopped states. It does not
reacquire backbuffers inside ResizeBuffers/ResizeBuffers1: a successful resize
keeps the renderer in resizing state until a non-test Present returns S_OK. Graphics
wrappers can continue rebuilding after the hooked implementation returns. A graphics-mutex
protected count of in-flight resize callbacks suppresses Present-side renderer work across
threads during those calls. Failed or occluded presentations do not resume rendering.
Normal drawing checks only the current backbuffer's allocator/command list and the next
ImGui vertex/index buffer slot, tracked by a separate submission ring. It skips drawing only if one of those
resources is unfinished, rather than blocking Present or waiting for unrelated submissions.
Each backbuffer owns its command list. Backend recreation resets the submission ring;
minimized frames do not advance it. Shutdown and resize use a shared
bounded fence-wait deadline; unresolved submissions retain their resources. Device loss
stops drawing. Retaining backbuffer references can prevent ResizeBuffers from succeeding.

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
the minimap is hidden. Application composition polls all observers before
publishing one HUD snapshot, then stops all observers and resets subscriptions
before graphics shutdown. The minimap observer may restart after stop; treasure
hook retirement rules do not apply to it. See [minimap observation](minimap-observation.md)
for the native canvas/ancestor checks, remaining layout limitations and validation evidence.

Another mod can link `game_observers` without linking the indicator or ImGui. The static
library still requires Windows x64, SafetyHook, and a compatible Crimson Desert build.
Public observer types hide SafetyHook and game addresses. Scanner headers and patterns
remain implementation details, even though they live under the shared include directory.

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
overflow, canvas ownership, ancestor cycles/depth, draw flags, clipping, invalid
opacity and startup without the game. Indicator and app
tests cover minimap gating, force_show and minimap lifecycle exceptions. The chain
was separately verified in Cheat Engine on 2.03.02; compiled feature integration
still requires in-game validation.
Game-hook activation and DX12 drawing
require in-game verification: force_show, toggle/unload, normal treasure detection,
game version, graphics settings and logs.

## Menu gating and return delay

MenuObserver resolves Root_MainMenu by exact script MSVC RTTI in the current UI array,
using a unique RIP-relative launcher store signature for the UI root slot.
Menu and minimap share `game/shared/ui_memory` for bounded exact RTTI traversal and
immutable image classification caches. Neither uses a fixed outer array index.
The menu state displacement is extracted from the two exact MOV encodings and
must agree; startup and every poll sample that field, including an open menu.
A unique executable instruction pair (clear RCX / set RBX, delta 0x141) is required.
Hooks capture atomics only, filter by resolved root, and latch openings between polls.
Events and diagnostics run on the owner thread. Unknown or duplicate identities suppress
showing. Menu hooks use the same process-lifetime retention and single-instance rules
as treasure hooks. Replaced roots are resolved again; missed transitions during root
replacement remain an in-game validation limitation.

Menu identity resolution distinguishes successfully read empty links and other script
types from failed reads. A failed read of any potentially relevant entry invalidates
uniqueness and returns unknown even if another entry matched. Every poll still follows
all live heap links and checks the entire array for duplicates. Only RTTI classifications
whose vtable, locator and name are in non-writable, readable game image sections are
cached. If only the locator is immutable, its resolved name address is cached and the
writable name is read again every poll. Heap addresses and root ownership are never
cached. All owner-thread samplers use the same guarded memory reader and overflow-checked
field reads; the treasure hook keeps its separate SEH capture path. RIP-relative
slot decoding is shared by UI-root and audio scanning, with each scanner retaining
its own image bounds and alignment validation.
Menu transitions use one atomic generation/opening latch; the poll decision is tested
separately for transitions before, during and after sampling and unknown samples.

The policy keeps independent treasure, minimap and menu states. Hidden/unknown minimap
or open/unknown menu immediately cancels return eligibility. When minimap is visible
and menu closed, update() starts a steady_clock deadline; show_delay_ms defaults to
1000. No sleeping implements the deadline. force_show bypasses treasure only.
The application polls all sources and advances the policy before publishing one HUD
snapshot. Immediate means the first owner-thread publication after a captured signal;
the game's +25B transition itself is delayed relative to input.

Menu implementation evidence and remaining game checks: [menu observation](menu-observation.md).

## Treasure sound

Application composition combines minimap, menu, mod enablement and foreground state
into playback eligibility, then passes this and sampled treasure state to the
sound policy, configured with sound enablement and cooldown. The first known treasure
sample with playback eligibility initializes the policy and notifies once if active,
including startup from unknown or waiting for loading/menu gating to clear. Later,
only an observed inactive-to-active transition can request playback; recovery from
unknown remains silent. Disabled sound/mod, hidden or unknown minimap, open or unknown menu,
an unfocused game and cooldown suppress the request without deferring it. The policy
does not consume HUD visibility, show_delay_ms or force_show.

The application owns the platform WAV player and connects policy requests to it.
The application thread queues playback/stop commands to a dedicated audio worker;
game hooks still capture data only. Device calls never run on the polling thread or
under the command mutex. A single pending command keeps the latest request, without
building a backlog; a stop cancels a play that has not started yet. Preparation and
destruction join any existing worker, which stops playback before PCM bytes are freed.
The worker is always joined before application exit and possible DLL unload.
The default WAV is embedded as RCDATA resource 102 in the ASI. An optional external
`PirateHatHUD_treasure.wav` overrides it; no standalone WAV is packaged. Both paths
validate PCM bytes before playback.
The embedded WAV was attenuated twice by a PCM gain of 0.75, for a combined gain
of approximately 0.5625 (about -5 dB relative to the original); custom WAV
files retain their own base amplitude. `AudioVolumeObserver` samples the game's master
and effects percentages at up to 20 Hz. It resolves an engine slot by unique signature,
validates object ownership and exact property names, and requires two matching live
option sets in two complete reads. Heap addresses are never cached. Unknown settings
or either slider at zero suppress playback and stop the current sound without deferring
notifications. This hook-free observer can restart after stop.

Application composition converts both percentages to linear PCM gain
(`master * effects / 10000`), multiplies it by `[sound] volume_percent / 100`
(default 100, range 0..100), and passes it to the platform player. Game observation
has no audio playback dependency, and the player has no game memory dependency.
`platform/config` checks the selected INI's modification time and size at most once
per second on the owner thread and reloads only `volume_percent`. Missing, incomplete
or invalid values retain the last valid volume and are retried. No observer settings
or lifecycle are reconfigured. Zero stops current playback without replaying events.
Each request scales a fresh copy of the prepared 8-bit or 16-bit PCM wave on the
worker. A successful play retains that copy until replacement or synchronous stop;
a refused play retains the previous buffer. Gain changes affect the next playback,
except mute or unknown settings, which queue a stop. The PCM gain is linear;
Wwise's slider gain curve has not been measured.
See [audio volume observation](audio-volume-observation.md) for the supported layout
and validation limits.
Sound policy tests cover transitions, suppression and cooldown;
actual playback, focus changes and notification timing require in-game verification.
The player preloads bounded, validated PCM RIFF bytes and retains them until a
synchronous PlaySound stop completes on the audio worker. Tests use a blocked audio
backend to verify that play/stop requests remain responsive and cleanup drains playback.
WinMM PlaySound playback is process-wide:
another mod using the same API can compete with playback or be affected by stopping it.

## Logging

`core/log.hpp` defines only severity and the synchronous callback type. Observers and
rendering pass explicit severity through callbacks; they do not access file logging or
configuration. Application composition reads configuration before opening the platform
logger and wires the callback. Hook capture remains free of observer event logging.
The logger serializes writes and rotation, bounds records, and disables writing on I/O
failure without throwing. UTC timestamp files rotate on startup and size; retention
matches only the logger's exact filename pattern and includes the current file.
Startup logs include the mod version, the executable's Windows file version,
and EXE/ASI basenames without installation paths. The executable file version may
differ from the game's public patch number. Successful hotkey registration is logged
at debug level; registration fallback remains a warning. Application composition
links the Windows version library to read the executable's version resource.
Renderer initialization logs the adapter matched to the game's actual DX12 device,
including its model, vendor/device IDs, dedicated video memory and software flag.
Swapchain size, format, buffer count, swap effect, flags and windowed mode are logged
at debug level at initialization and resize recovery. LUIDs are used only to resolve the adapter;
no LUID, serial number, username or installation path is added to these diagnostics.
Logs are local and are not uploaded by the mod. Adapter diagnostic failures do not
prevent renderer initialization.
Application composition records numeric Windows version information and a source-content
fingerprint generated by CMake, with build configuration and compiler version at info level.
The adapter UMD version is also logged at info level when DXGI exposes it. Safe mod
settings, startup begin/ready markers, resize requests, selected swapchain generations,
color-space changes and first-frame/submission progress are logged at debug level.
Resource failures are logged at error level and
include the operation, HRESULT and device removal reason where a device is available.
Repeated identical Present errors are suppressed until success or an error change.
Diagnostics neither install exception handlers nor enable the D3D12 debug layer; they
do not infer the game's active upscaler or upload logs.
