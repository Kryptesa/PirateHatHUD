# Comparing and collecting reports

These developer utilities use Python's standard library and native PowerShell.
They do not change game memory or production code.

## Compare evidence

```powershell
python tools/game-research/compare-reports.py research/game-update/2.03.02/signatures.json research/game-update/2.03.03/signatures.json
python -m unittest discover -s tools/game-research/tests -p 'test_*.py'
```

Compare JSON reports from the **same tool**, with integer `schema_version: 1`,
nonempty `tool` and `status` strings and a `checks` array. Each check requires a
unique nonempty `id`, a nonempty `status` and `details` (any JSON value). Optional
fields are compared too. Checks are matched by ID, regardless of ordering;
arrays inside details are compared by position. Address strings and numeric
values are compared exactly, without normalizing spelling or subtracting ASLR.
Prefer module-relative RVA fields when comparing runs.

Output lists additions, removals and changes, including nested fields. Changes
in timestamps or capture labels are evidence changes as well. The script does
not infer game compatibility. Exit code 0 means comparison succeeded, including
when differences exist; 2 means an invalid report, mismatched tool or I/O error.
Duplicate JSON keys, duplicate check IDs and nonstandard numbers are rejected.

## Collect selected evidence

```powershell
./tools/game-research/collect-diagnostics.ps1 -Files @('research/game-update/2.03.03/signatures.json', 'research/game-update/2.03.03/mod.log') -OutputZip research/game-update/2.03.03/diagnostics.zip -GameVersion 2.03.03 -ModVersion '<mod commit>'
```

The output directory must already exist. Existing archives are never overwritten.
Select individual files explicitly; the collector does not search for logs or
recursively include directories. The ZIP contains numbered basenames under
`files/` and a `manifest.json` with SHA-256 hashes, sizes, UTC collection time and
the supplied version labels. It records no source directory paths. Duplicate
basenames are retained as separate numbered entries.

File contents are copied unchanged. Logs and reports may themselves contain
personal paths, memory addresses or other sensitive information. Review the
selected files, filenames and version labels before sharing the archive; the
collector does not sanitize evidence automatically. Use stable, closed log files:
files being written by another process can fail to open. An incomplete archive
is removed if collection fails.
