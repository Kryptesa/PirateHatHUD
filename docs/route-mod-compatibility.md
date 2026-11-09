# Route mod compatibility investigation

Static inspection on 2026-11-08 compared PirateHatHUD 1.2.2 with the supplied
[CrimsonRoute](https://www.nexusmods.com/crimsondesert/mods/3175) 9.1.0 and
[WaypointRoute](https://www.nexusmods.com/crimsondesert/mods/3931) 1.2.8 archives.
Neither third-party ASI was loaded or executed during the initial static inspection.
At that point no combined in-game run, crash dump or user log was available.

## First combined run: WaypointRoute

On 2026-11-08 the user tested PirateHatHUD 1.2.2 with WaypointRoute 1.2.8 and
reported that the hat icon and route displays were visible and working. The game
log identifies EXE 1.0.0.3576. WaypointRoute captured the presentation queue through
Streamline and initialized a 2560x1440, three-buffer chain with format 24 and color
space 12. Its log records route construction and successful draw counts.

PirateHatHUD's same-session log contains an initial ResizeBuffers failure
(`0x80070057`), followed by selection of a new chain and successful overlay
initialization. The user reported no visible failure. This run does not reproduce
the reported incompatibility and does not establish resize, hotkey-stop or other
graphics-mode compatibility. The paired logs and configurations were copied under
`build/compatibility/runs/` and backed up again before switching the route mod.

## Second combined run: CrimsonRoute

The second test used CrimsonRoute 9.1.0 with `Renderer=Present`,
`[Diagnostics] Logging=1` and `SampleIntervalSeconds=5`. Its AutoMove keyboard
bindings were changed to F6/F7 while preserving the controller bindings.
WaypointRoute's ASI was renamed to `.asi.disabled` to test the route mods
separately. PirateHatHUD retained `level=debug` and `show_icon=1`.

The user confirmed that both mods were working visually in this run too. On EXE
1.0.0.3576, CrimsonRoute's log records a successful 312-point route, minimap drawing
and an active world route. Earlier attempts rejected the player's starting
connection or a wall edge; later route calculation succeeded after movement/target
changes. F6 attempts were correctly rejected with `feature_disabled` because
AutoMove remained disabled. These keys are not needed to activate route rendering.

CrimsonRoute logged a factory-method mismatch between dxgi.dll and
sl.interposer.dll, but continued with Present rendering. This warning alone is
therefore insufficient to diagnose the reported incompatibility. PirateHatHUD
again logged an initial ResizeBuffers `0x80070057` followed by successful overlay
initialization. Neither message caused a user-reported visible failure in this run.
Paired logs and configurations were copied under `build/compatibility/runs/`.

The original incompatibility was not reproduced with either route mod tested
separately alongside PirateHatHUD 1.2.2. At that stage no compatibility fix had
been established or applied. This does not cover all three mods together, original CrimsonRoute
F9/F10 bindings, stopping/unloading, all graphics modes or long-duration play.
Further reports need exact versions, a concrete symptom, graphics settings and
same-session logs before changing hook behavior.

## Reporter evidence received on 2026-11-09

The reported symptom is missing routes on both maps. Supplied screenshots show
the game map/minimap without a CrimsonRoute path. Both CrimsonRoute logs identify
version 9.1.0 and game EXE 1.0.0.3576; the accompanying PirateHatHUD log identifies
version 1.2.2. Raw text inputs were copied under
`build/compatibility/reports/2026-11-09-missing-routes/` for local investigation.
Do not publish their module inventories or installation paths without redaction.

In `CrimsonRoute (Pirate Hat MOD Installed).log`, route calculation succeeds twice
with 1084 points (lines 341 and 405), but Present renderer binding repeatedly fails:

```text
present_d3d12_bind_failed; stage=queue_unavailable; hr=0x80004002;
queue=0000000000000000; queue_source=captured_dxgi
```

The first binding failure is at line 404; there are four in that run. No successful
`render_backend=Present` marker appears. A drawable minimap snapshot and
`overlay_should_draw=1` are not proof that GPU drawing occurred: binding still
fails. The supplied PirateHatHUD log likewise never reaches swapchain selection
or renderer initialization. Its initial ResizeBuffers error is also seen in our
successful runs and is not a sufficient explanation.

`CrimsonRoute-20261108-191700Z.log` contains the same failure fourteen times,
starting at line 395. Its module inventory does not list PirateHatHUD. Route
calculation still succeeds with 1084 points and later 180 points. Thus this
failure is present in a supplied run without our ASI loaded; these logs do not
establish that PirateHatHUD causes it. Both runs still load numerous other ASIs.
The local successful run has zero queue-unavailable failures and a successful
D3D12 Present backend initialization marker.

Both reporter runs hook native dxgi.dll factory/swapchain methods first and then
reject alternate sl.interposer.dll factory methods. That may explain a missed
game-queue association, but the mismatch marker alone also occurs locally and
does not prove the mechanism. In the run with PirateHatHUD, our probe completed
before CrimsonRoute started, so capture of our startup probe is not demonstrated
by this chronology. Loaded upscaler libraries do not identify the active upscaler
or frame-generation setting.

Next isolate with only CrimsonRoute and the required ASI loader, then repeat with
PirateHatHUD added using identical graphics settings. If the isolated baseline
also fails, compare frame generation off/on and test CrimsonRoute's
`[Rendering] Renderer=DirectComposition` as a separate diagnostic after restart.
Independent composition behavior and its graphics-mode limitations still need
verification; it is not a confirmed fix. Obtain the exact active graphics settings
and forward the queue-binding evidence to the CrimsonRoute author if necessary.

## Additional WaypointRoute reporter logs on 2026-11-09

These identify WaypointRoute 1.2.8; the paired PirateHatHUD log identifies version
1.2.2 and game EXE 1.0.0.3576. Text inputs were copied under
`build/compatibility/reports/2026-11-09-waypoint/`. WaypointRoute timestamps use
local time, one hour ahead of the paired PirateHatHUD UTC timestamps in this run.

This is a different observed failure from CrimsonRoute's missing queue:

- `WaypointRoute (with Pirate Hat MOD).log` captures a Streamline presentation
  queue, initializes its 3840x2160 HDR10 renderer and records positive draw counts.
  There is no logged queue-unavailable or graphics-initialization failure.
- Its finder sees three minimap roots and selects one marked as having a player
  (line 95). All sixteen player-valid diagnostic samples then report the same
  position `(5372.9, 31130.5)` and rotation `5348448.0`. A world-map center also
  reaches `(2980384342016, 0)` with a near-zero negative zoom. This data is
  inconsistent with the other run and strongly suggests an unsuitable UI instance
  or incompatible field interpretation; the logs alone do not distinguish them.
- It builds only a two-point route from `(5373,31130)` to the correctly located
  waypoint `(-10408,-6008)` (line 112). Successful draw submission does not imply
  this route projects onto the visible map correctly.
- `WaypointRoute.log`, the subsequent supplied run, reads player position
  `(-10502.6,-4373.9)` with plausible changing rotation, and builds a 432-point
  road route to the same waypoint (line 65). It likewise initializes rendering
  and submits draws. This log has no module inventory, so confirm with the
  reporter that it is a run without PirateHatHUD rather than assuming that from
  the filename.

Both files log the same visible configuration summary, including enabled minimap
and world-map routes, and disabled in-world ribbon. Thus the disabled world
ribbon does not explain missing map lines. The complete INI and all effective
offsets are not in the supplied inputs. PirateHatHUD itself never reaches graphics
renderer initialization in its paired log, so active HUD draw commands are not
demonstrated as the source of the wrong coordinates. Its hooks and memory
allocations still occur, so this does not rule out an interaction.

The next useful evidence is the reporter's exact WaypointRoute INI and confirmation
of the second run's mod set. Repeated matched launches with only WaypointRoute and
then with PirateHatHUD added can test whether the UI candidate selection failure
tracks our mod. A third run with PirateHatHUD `show_icon=0` separates its graphics
startup path from its observation path, but timing/heap-layout effects remain
possible. Do not present the UI-instance hypothesis or a shared cause with
CrimsonRoute as a proven explanation.

## Other installed mods: investigation priorities

The reporter's list includes Minimap Tweaks as a non-ASI mod. The
[original author's description](https://www.nexusmods.com/crimsondesert/mods/275)
documents minimap size, zoom and panel changes, and another
[similarly named variant](https://www.nexusmods.com/crimsondesert/mods/3354)
also changes UI scale and layout. Obtain the exact mod ID/version before choosing
a matching test. This makes it relevant to WaypointRoute's anomalous UI readings,
but there is no evidence yet that either variant caused those readings. Data mods
must be disabled/reverted through the mod manager as appropriate; removing ASIs
alone does not isolate this variable. The original author also states ASI
compatibility, which is not a verification of this exact installed combination.

There is another documented keyboard overlap: the Calmer Dynamic Wind
[author's comments](https://www.nexusmods.com/crimsondesert/mods/3483?tab=posts)
identify F10 as its comparison toggle. PirateHatHUD uses F10 to stop. Verify the
reporter's installed version/binding before attributing any event to this key.
This overlap is distinct from the demonstrated queue-binding and UI-data failures.

CrimsonRoute's supplied hook-detail records report its installed hooks as owned
by CrimsonRoute, with no explicit failed/conflict state or other owner recorded.
That does not rule out earlier chained hooks, data changes or timing effects.
There is no evidenced basis to label the other named ASIs incompatible merely
from their names. Test the same graphics/configuration baseline with only the
route mod and PirateHatHUD, including the absence of optional data mods, then
restore additions one at a time or in batches to isolate the relevant difference.

## Load-order investigation: persistent false object signatures

Inspection on 2026-11-09 found a concrete interaction mechanism on our side.
Both the menu and minimap observers use `UiIdentityCache`. Each traverses the
entire live UI table and caches every identified immutable script type, including
types that do not match that observer's requested root. The original cache used
an absolute game vtable address as the key of each `std::map` node.

Those keys are aligned native vtable values on our heap. A scanner that considers
any readable aligned vtable occurrence a candidate script object can therefore
mistake each observer's cached key for another minimap/world-map object. Checking
the installed EXE's MSVC RTTI confirms that WaypointRoute's logged minimap RVA
`0x5a0fc78` is exactly `UIGamePlayControlRootMiniMap`, the type our cache reads.
Its world-map RVA `0x5a074c8` is `UIGamePlayControlRootWorldMap`, also traversed and
cached by both observers.

Static inspection of the supplied WaypointRoute 1.2.8 binary supports the mechanism:

- RVA `0xb0b0` scans input memory in eight-byte steps and appends addresses whose
  first word matches a known UI vtable. It does not establish native UI ownership
  at this candidate stage.
- RVA `0xb5b0` selects the first minimap candidate accepted by RVA `0xa0b0`, or
  falls back to the first candidate if none passes.
- The latter checks the vtable and accepts finite, nonzero X/Z values whose
  absolute magnitude is below 1,000,000. Our unrelated heap contents beyond a
  cached key can satisfy those weak checks. It does not prove those fields belong
  to a live game script.

Thus one real minimap plus one key in each of our two caches can produce exactly
the three candidates in the reporter's failing run. Their exact addresses/order
were not captured in a live dump, so attributing the chosen candidate to a specific
cache remains to be confirmed. This mechanism nevertheless reproduces independently
of game UI data: a regression test warms two actual production caches, copies their
payload bytes through the guarded reader, and scans for a native vtable signature.
Before the change, one synthetic script plus those caches yields three matches
and the test fails. After the change, both matched and other-type signatures
yield one match. Existing warm-cache read counts are unchanged (24 cold/18 warm
for the small fixture).

`UiIdentityCache` now keys types by image-relative vtable displacement, and clears
type identities when the image base or requested type changes. This removes those
persistent absolute-vtable signatures while preserving live traversal, RTTI
validation, cache capacity and all architecture boundaries. No game UI memory is
modified. The cache test also checks image-identity invalidation.

This explains why launch order/timing can affect the symptom without an actual
game-object write: it changes when cache nodes exist, their heap placement and
which candidate a broad scan picks first. `DllMain` starts an independent worker,
so filename load order alone does not serialize completion of graphics startup or
observer scans. Delays or renaming ASIs can change an outcome without removing
the false candidates. No arbitrary delay or loader-specific ordering requirement
was introduced.

The change passes a Release build, architecture-check, formatting and all 31
CTest cases. The experimental build received the local checks below. Confirmation
of the original failure still needs the reporter's same save/settings and
both WaypointRoute and PirateHatHUD logs; check normal position/rotation, road
routes, our treasure icon/sound, menu gating and hotkeys. Record upscaler/frame
generation/HDR settings. Stop/F10 and load-order variations remain additional
validation, not passed checks.

The [experimental ZIP](../dist/PirateHatHUD-ui-cache-test.zip) contains the test
ASI, bundled licenses/notices and instructions; it intentionally includes no INI
so the first comparison can preserve the reporter's existing configuration.
Its startup source fingerprint is `eb51e8a7570e84a9`; verify this in the new log.
The ASI SHA-256 is
`b66af6ced32ef25b587ec9ed9eb7969d0ec9cc0684a5f124fa33f0a05fe521ae`.

The user subsequently tested this ASI locally with CrimsonRoute 9.1.0 on
2026-11-09 and reported that gameplay was working. The new log confirms source
`eb51e8a7570e84a9`, EXE 1.0.0.3576 and successful HUD initialization. CrimsonRoute
initialized its HDR10 Present backend and calculated road routes with 103 and
97 points. Logs/configurations were preserved under
`build/compatibility/runs/crimson-cache-test-success-20261109-190956/` before
switching the active route mod to WaypointRoute 1.2.8. This local regression check
does not reproduce or resolve the reporter's CrimsonRoute queue failure.

The user then tested the same experimental ASI with WaypointRoute 1.2.8 on
2026-11-09 and reported normal behavior. The paired PirateHatHUD log confirms the
same source fingerprint and EXE 1.0.0.3576, followed by successful HUD
initialization. WaypointRoute records changing player coordinates and rotation,
106- and 97-point road routes and positive draw counts. Paired logs and settings
were preserved under `build/compatibility/runs/waypoint-cache-test-success-20261109/`.
These checks test each route mod separately alongside PirateHatHUD. The reporter
has not yet confirmed the fix on the originally failing setup, and the versioned
1.2.3 package has not received a separate in-game run.

The CrimsonRoute queue-unavailable failure remains separately unresolved. In its
paired log, our graphics probe finished before CrimsonRoute started, and the
supplied run without our ASI has the same queue failure. This cache change cannot
be claimed to fix that renderer binding issue. A robust upstream object finder
would additionally verify membership in the live UI tree rather than accepting
raw vtable matches from any allocation.

## Input identification

SHA-256 of the extracted ASIs:

| File | SHA-256 |
| --- | --- |
| CrimsonRoute.asi | `04fdb9db5cbf749b832ac4600f5c99b00f6cd9ec6f162308437d03ce4ddb7f70` |
| WaypointRoute.asi | `033d8ae2927616e7ed4c7d714e97897cfbc5969025ad0bc728a532a136d1a4f0` |

CrimsonRoute contains its default INI as binary text. WaypointRoute ships an INI
and third-party notices, including MinHook 1.3.4. Both binaries contain MinHook
code/identification; PirateHatHUD uses SafetyHook. Sharing an API target does not
by itself prove that two inline hooks cannot chain.

## Confirmed intersections

### CrimsonRoute hotkeys

CrimsonRoute's embedded defaults include:

```ini
[AutoMove]
Enabled=0
ToggleHotkey=F9|LeftStick,RightStick,X
ModeHotkey=F10|LeftStick,RightStick,Y
```

PirateHatHUD's [defaults](../config.ini) bind F9 to notification enablement and F10
to stopping the mod until the next launch. These keyboard bindings overlap if the
CrimsonRoute controls are used. Its AutoMove feature is disabled by default;
static inspection does not establish how it handles these keys while disabled.
PirateHatHUD uses RegisterHotKey with a key-state fallback, so actual delivery to
both mods also needs a live check. WaypointRoute's shipped bindings do not have
the same F9/F10 overlap.

Rebind one mod before testing the graphics issue. PirateHatHUD accepts only
F8 through F11, and CrimsonRoute also uses F8 for its settings window, so choose
bindings after checking the user's other mods rather than prescribing F8 blindly.

### Presentation and resize hooks

All three mods intercept Present, Present1, ResizeBuffers, ResizeBuffers1 and
SetColorSpace1. Both route mods capture swapchain creation; their diagnostics and
WaypointRoute's INI describe a Streamline presentation path. PirateHatHUD discovers
its hook addresses from a temporary native DXGI/DX12 chain in
[dxgi_probe.cpp](../src/render/dxgi_probe.cpp), then installs inline hooks in
[dxgi_hooks.cpp](../src/render/dxgi_hooks.cpp).

WaypointRoute disassembly shows that the first accepted D3D12 swapchain creation
claims a one-time hook-installation flag before installing its five swapchain hooks
(RVA `0x22070`, flag claim at `0x221bc`). CrimsonRoute rejects a different subsequent
Present target (RVA `0x933f4`) and can log:

```text
swap_chain_present_target_mismatch=1; existing_module=%s; new_module=%s; alternate_swap_chain_hooked=0
```

This makes selection of an auxiliary chain a relevant hypothesis. PirateHatHUD's
64x64 hidden probe is one such chain, but it has not been established that either
route mod observes this probe in the user's load order or graphics stack. Native
DXGI and Streamline methods may be different targets. Do not report probe capture
as the established cause without logs showing which methods were selected.

### Inline-hook removal

PirateHatHUD stops by disabling its graphics hooks. The pinned SafetyHook
implementation restores its saved original bytes on disable without checking
whether another library has subsequently patched the same entry point. If a route
mod installs an inline hook at that same address after PirateHatHUD, stopping
PirateHatHUD can overwrite the later patch. This is conditional on target identity
and installation order; it is not evidence of a startup crash. CrimsonRoute's F10
overlap makes this shutdown path especially relevant to investigate.

## Isolation runs

Test PirateHatHUD with each route mod separately before trying all three. Record
the public game patch, EXE file version, mod versions, GPU, HDR, upscaler, frame
generation and ASI load order for every run.

1. Capture the actual symptom and paired logs from one launch: startup crash,
   hang, missing hat icon, missing routes, or failure only after a hotkey/resize.
2. Rebind overlapping keys and repeat the same scenario. Avoid F10 while
   establishing the baseline because it intentionally stops PirateHatHUD.
3. Set PirateHatHUD `[indicator] show_icon=0` and restart. This skips its graphics
   probe and hooks while retaining observation and sound. If the failure remains,
   investigate beyond graphics interception. This is a diagnostic/workaround
   candidate, not verified compatibility.
4. Restore `show_icon=1`; switch the route mod to its independent renderer and
   restart: CrimsonRoute `[Rendering] Renderer=DirectComposition`, or WaypointRoute
   `[Style] Renderer=Composition`. WaypointRoute's INI says its in-world ribbon
   requires Present; Composition also has frame-generation/HDR limitations.
5. With both Present renderers enabled, compare first hook/probe markers and
   route hook target-module diagnostics. A crash dump or all-thread hang dump is
   needed to distinguish a bad hook chain from a lock cycle.

Set PirateHatHUD `[logging] level=debug`. Useful WaypointRoute lines include
`Overlay: swapchain hooks installed in`, `Overlay: swapchain-creation hooks
installed in`, queue-source diagnostics and Composition fallback messages.
For CrimsonRoute, collect `function_hook_failed`, target mismatch messages and
`present_boundary=factory_return_vtable_chain`. Logs from a successful single-mod
baseline help distinguish pre-existing layout failures from combined failures.

The initial static inspection made no runtime/configuration changes. Subsequent
test preparation installed the mods and settings described above. The experimental
PirateHatHUD build passed architecture-check and all 31 CTest cases. The cache fix
removes a reproduced false-object signature; confirmation on the reporter's
originally failing setup remains pending.
