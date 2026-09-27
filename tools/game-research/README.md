# Game research tools

This directory contains reproducible tools for investigating game events and
checking compatibility after game updates.
Run these tools from your checkout during game research.

- `ce/snapshot.lua`: reads the current UI chain and bytes at historical code sites.
- `ce/check-signatures.lua`: checks treasure/menu instruction pairs and the UI root signature from the mod's source files.
- `ce/inspect-ui.lua`: inspects the UI pointer chain and script RTTI.
- `ce/capture-transitions.lua`: records changes at selected byte addresses.
- `compare-reports.py`: compares structured reports from two runs.
- `collect-diagnostics.ps1`: archives selected reports and logs.
- `report-template.md`: records validation results for a specific game version.
- `event-template.md`: records a candidate event and the evidence for its meaning.
- Store raw reports, dumps, traces and experimental scripts under
  `research/events/<event>/<version>/` for event investigations or
  `research/game-update/<version>/` for updates. The entire `research/` directory
  is ignored by Git.

## Collecting baseline data

Attach CE to the actual `CrimsonDesert.exe`, load a save and enter gameplay.
The snapshot script reads the historical addresses from **2.03.02**.

Run this in CE's Lua Engine. Replace `<checkout>` with your local repository
directory:

```lua
print(dofile('<checkout>/tools/game-research/ce/snapshot.lua'))
```

Through MCP `evaluate_lua`, run the same `dofile` with `return` to retrieve the
result. Save it in the local version directory. Repeat during gameplay, with a
menu open, with the minimap disabled, and after restarting the game. Label each
snapshot. The script returns text and does not create files itself.

## Structured checks

The newer CE tools use JSON reports with `schema_version`, `tool`, `status` and
a `checks` array. Each check has an `id`, `status` and `details`. Keep reports
from different tools in separate files. `snapshot.lua` remains a plain-text
baseline collector and cannot be passed to the JSON comparison tool.

Configure your checkout locally before running the structured checks:

```lua
GAME_RESEARCH_CONFIG = {checkout = '<checkout>'}
print(dofile(GAME_RESEARCH_CONFIG.checkout .. '/tools/game-research/ce/check-signatures.lua'))
print(dofile(GAME_RESEARCH_CONFIG.checkout .. '/tools/game-research/ce/inspect-ui.lua'))
```

Use `return` instead of `print` through MCP. Run signature checks before loading
the ASI: installed hooks replace the bytes
at their sites and can make the original signatures disappear. `inspect-ui.lua`
checks the historical 2.03.02 chain and +25B field. Verify the field's behavior
in game after checking its layout.

See [transition recording](ce/transitions.md) for bounded state recordings and
[report tools](report-tools.md) for comparison and diagnostic archives.

## Investigating a new event

1. Define the action and expected event using [event-template.md](event-template.md).
   Include cases where the event must not fire, and distinguish input from a
   state change (for example, pressing Esc versus a menu actually opening).
2. Collect candidate fields or code sites through CE MCP memory scans, reference
   searches and disassembly. `inspect-ui.lua` can help with the current UI layout;
   use other scans for systems outside the UI.
3. Resolve and label candidate field addresses, then use `capture-transitions.lua`
   while performing the action manually. Capture both positive and negative
   cases. An unreadable sample is unknown, not an inactive state.
4. Inspect accesses to promising fields through CE MCP, recording instruction
   bytes, registers and relevant call stacks. Keep raw traces in the local event
   directory.
5. Repeat after loading, object replacement and a fresh process. Prefer stable
   code signatures and verified object identity over historical heap addresses.
6. Compare reports from repeated experiments and record timing, missed transitions
   and remaining uncertainty.
7. Once the observation is established, implement it within the game observer
   boundary described in `docs/architecture.md`. Hooks capture data only; publish
   events from `poll()` on the owner thread. Add validation for the new behavior.

`check-signatures.lua` currently checks only the mod's treasure and menu pairs.
New event signatures need explicit support before that script can validate them.

## Checking a game update

1. Record the game version, date, mod commit and graphics settings using the
   report template.
2. Check the current treasure and menu signatures across executable sections.
   Each must yield exactly one pair, with distances 0x2C and 0x141 respectively.
   The sources of truth are `include/game/treasure/patterns.hpp`, `src/game/treasure/scan.cpp`, `src/game/menu/scan.cpp` and
   `src/game/shared/ui_root_scan.cpp`; `src/pattern_scan.cpp` supplies generic scan semantics.
3. Check the UI root and pointer chain against `include/game/minimap/memory.hpp`
   and `include/game/menu/memory.hpp`. Investigate the specific failing step
   using the recorded pointer chain and field offsets.
4. Verify the launcher store signature and decoded RIP-relative UI slot in
   `src/game/shared/ui_root_scan.cpp`. Root signature scanning is implemented;
   do not restore the historical slot RVA as a fallback.
5. Check UI types through RTTI. Menu and minimap roots are resolved by exact
   script type. For the minimap, verify the native canvas binding, exact
   definition names, ownership and bounded parent chain. Check draw flags,
   clipping and computed opacity on every ancestor; see
   `docs/minimap-observation.md` for evidence and remaining limitations.
6. Verify state semantics in game: Esc/M/I, minimap settings, treasure detection,
   loading/teleport, restart, force_show, and F9/F10.
7. After code changes, run `./cmake/Build.ps1` (architecture-check and CTest),
   update the observer documentation, and record the results.

Do not reuse a historical RVA on a new version without checking the instruction.
Missing or ambiguous signatures and unverified chains must leave the state
unknown. Do not select an address based only on a readable 0/1 byte.

## Testing the tools

From the repository root:

```powershell
python -m unittest discover -s tools/game-research/tests -v
```

Lua regression tests use the optional `lupa` package with its Lua 5.3 runtime
and simulated CE memory/timers. Install test dependencies locally under the
ignored build directory if needed:

```powershell
python -m pip install --target build/game-research-test-tools lupa==2.8
```

Lua tests are skipped when the optional runtime is unavailable. Check the tools
in CE after changes to their memory reads or recording behavior.

## Related materials

- [Architecture](../../docs/architecture.md)
- [Minimap observation](../../docs/minimap-observation.md)
- [Menu observation](../../docs/menu-observation.md)
