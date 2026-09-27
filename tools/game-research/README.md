# Game research tools

This directory contains reproducible tools for investigating game events and
checking compatibility after game updates.
These are development tools; they are not included in the ASI or install bundle.

- `ce/snapshot.lua`: reads the current UI chain and bytes at historical code sites.
- `ce/check-signatures.lua`: checks treasure/menu instruction pairs and the UI root signature from the mod's source files.
- `ce/inspect-ui.lua`: inspects the UI pointer chain and script RTTI.
- `ce/capture-transitions.lua`: records changes at explicitly selected byte addresses.
- `compare-reports.py`: compares structured reports from two runs.
- `collect-diagnostics.ps1`: archives explicitly selected reports and logs.
- `report-template.md`: records validation results for a specific game version.
- `event-template.md`: records a candidate event and the evidence for its meaning.
- Store raw reports, dumps, traces and experimental scripts under
  `research/events/<event>/<version>/` for event investigations or
  `research/game-update/<version>/` for updates. The entire `research/` directory
  is ignored by Git. Existing research does not need to be moved.

## Collecting baseline data

Attach CE to the actual `CrimsonDesert.exe`, load a save and enter gameplay.
The script does not write game memory or set breakpoints. It uses historical
addresses from **2.03.02**; it does not discover new addresses automatically.
Readable memory alone does not establish compatibility.

Run this in CE's Lua Engine. Replace `<checkout>` with your local repository
directory; it is a placeholder, not a literal path:

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

Use `return` instead of `print` through MCP. These checks read game memory and
repository files; they do not install hooks or modify game memory. Run signature checks before loading the ASI: installed hooks replace the bytes
at their sites and can make the original signatures disappear. `inspect-ui.lua`
remains a historical 2.03.02 chain/+25B probe, not the runtime resolver. A successful
structural check is not proof that a field still has the expected meaning.

See [transition recording](ce/transitions.md) for bounded state recordings and
[report tools](report-tools.md) for comparison and diagnostic archives.

## Investigating a new event

1. Define the action and expected event using [event-template.md](event-template.md).
   Include cases where the event must not fire, and distinguish input from a
   state change (for example, pressing Esc versus a menu actually opening).
2. Collect candidate fields or code sites through CE MCP memory scans, reference
   searches and disassembly. `inspect-ui.lua` can help with the current UI layout;
   it is not a general discovery tool for arbitrary game systems.
3. Resolve and label candidate field addresses, then use `capture-transitions.lua`
   while performing the action manually. Capture both positive and negative
   cases. An unreadable sample is unknown, not an inactive state.
4. Inspect accesses to promising fields through CE MCP, recording instruction
   bytes, registers and relevant call stacks. Keep raw traces in the local event
   directory. Correlation alone does not prove that a field represents the event.
5. Repeat after loading, object replacement and a fresh process. Prefer stable
   code signatures and verified object identity over historical heap addresses.
6. Compare reports from repeated experiments and record timing, missed transitions
   and remaining uncertainty. The comparison tool compares evidence, not causality.
7. Once the observation is established, implement it within the game observer
   boundary described in `docs/architecture.md`. Hooks capture data only; publish
   events from `poll()` on the owner thread. Add validation for the new behavior.

`check-signatures.lua` currently checks only the mod's treasure and menu pairs.
New event signatures need explicit support before that script can validate them.
The supplied scripts do not automatically discover or implement new events.

## Checking a game update

1. Record the game version, date, mod commit and graphics settings using the
   report template.
2. Check the current treasure and menu signatures across executable sections.
   Each must yield exactly one pair, with distances 0x2C and 0x141 respectively.
   The sources of truth are `include/game/patterns.hpp`, `src/game/hook_scan.cpp` and
   `src/game/ui_root_scan.cpp`; `src/pattern_scan.cpp` supplies generic scan semantics.
3. Check the UI root and pointer chain against `include/game/minimap_memory.hpp`
   and `include/game/menu_memory.hpp`. Investigate the specific failing step
   rather than starting a general scan for all 0/1 values.
4. To replace the fixed root RVA, investigate the write at historical RVA
   0x86D2AA8 and references to slot 0x6C8CC00. Capture nearby instructions,
   mask address operands in the signature, verify uniqueness, and decode the
   slot address from the matched instruction. Root signature scanning is
   **not implemented yet**.
5. Check UI types through RTTI. The menu is already resolved by exact type;
   the minimap still depends on fixed array entries. RootStatusGauge and
   RootMiniMap are different objects; verify behavior before substituting one.
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

These tests do not replace validation against CE and the game. Lua tests are
skipped when the optional runtime is unavailable.

## Related materials

- [Architecture](../../docs/architecture.md)
- [Minimap observation](../../docs/minimap-observation.md)
- [Menu observation](../../docs/menu-observation.md)
