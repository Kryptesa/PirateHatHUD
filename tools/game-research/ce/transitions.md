# Recording field transitions

Attach Cheat Engine to `CrimsonDesert.exe`. Resolve the candidate field addresses
with the UI inspection tools before recording. Run in CE's Lua Engine:

```lua
local recorder = dofile('<checkout>/tools/game-research/ce/capture-transitions.lua')
local capture = recorder.start({
  game_version = '2.03.02',
  scenario = 'Open and close the menu during gameplay',
  duration_ms = 15000,
  interval_ms = 100,
  max_samples = 200,
  fields = {
    -- Replace these expressions with verified candidate field addresses.
    {label = 'menu_candidate', address = 'YOUR_MENU_FIELD_ADDRESS', size = 1},
    {label = 'minimap_candidate', address = 'YOUR_MINIMAP_FIELD_ADDRESS', size = 1}
  }
})
print(capture.status())
```

Perform the labeled action manually while the CE timer runs. In a later Lua
Engine invocation, retrieve the report:

```lua
print(PirateHatGameResearchTransitions.status())
print(PirateHatGameResearchTransitions.result())
-- To stop early:
-- PirateHatGameResearchTransitions.stop()
```

For MCP Lua evaluation, return `PirateHatGameResearchTransitions.result()` once
the status is `completed` or `stopped`. Save the returned JSON under
`research/events/<event>/<version>/` for a new event or
`research/game-update/<version>/` for compatibility checks. The script does not
write files. The handle
returned by `start` also provides `status()`, `stop()` and `result()`; it remains
bound to its original capture after a restart.

Each sample records bytes in memory order, with elapsed milliseconds. Events
record the initial value and later changes, including transitions to or from
`unknown`. A failed read is unknown, never an inactive `0`. Report checks become
`unknown` if any sample of a field was unreadable. Completion only means capture
finished; it does not establish game compatibility or field semantics.

Addresses are resolved once at startup. Replaced UI objects require a fresh
capture with newly verified addresses. Polling can miss changes between samples;
timestamps describe CE reads, not the instant of game input. Menu state may lag
the input. Compare the trace with the action you performed and repeat across
loading, object replacement and game restart as needed.

Captures are bounded to 120 seconds, 32 fields, 6000 samples and 20000 total field
samples. Byte widths are 1 through 8; the default interval is 100 ms (minimum
20 ms). Reaching either the duration or sample limit ends capture. Explicit
stop, restart, script reload and sampling errors destroy the timer. Only the
named `PirateHatGameResearchTransitions` namespace is installed globally. The
script reads bytes only: it sets no breakpoints and sends no game inputs.
