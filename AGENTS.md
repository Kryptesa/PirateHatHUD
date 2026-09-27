# Repository Guidelines

## Project Structure & Module Organization

PirateHatHUD is a Windows x64 C++23 ASI mod for Crimson Desert. `src/main.cpp` is the DLL entry point; `src/app.cpp` composes modules and manages lifecycle. `src/game/` groups observation internals under `treasure/`, `minimap/`, `menu/`, `audio/` and `shared/`; these sources and `src/pattern_scan.cpp` form the `game_observers` static library. Public observer headers stay directly under `include/game/`. `include/core/signal.hpp` provides typed subscriptions. `src/features/treasure_indicator.cpp` forms the indicator policy library; `src/overlay.cpp` renders a HUD snapshot through DirectX 12. Configuration, hotkeys and logging live in `src/platform/`. Headers and instruction patterns live in `include/`; architecture contracts are documented in `docs/architecture.md`. `tests/*_tests.cpp` covers scanner, signals, indicator and observer startup behavior. `assets/icon.png` is embedded as the default HUD image; `config.ini` supplies runtime defaults packaged as `PirateHatHUD.ini` for CDUMM. `cmake/Format.cmake` manages formatting. Build outputs and fetched dependencies belong in `build/`.

## Architecture Rules

- Before adding a module, moving files, or changing dependencies, read `docs/architecture.md`.
- Put every project header (`.hpp`, `.h`) in `include/` and every implementation (`.cpp`) in `src/`. Keep module subdirectories aligned. Internal headers follow the same layout; placement under `include/` does not make them a public API.
- Follow the dependency matrix in `docs/architecture.md`. Game observers must not depend on HUD/rendering or mod configuration; rendering must not depend on game observation.
- Game hook callbacks capture data only. Publish observer events from `poll()` on the owner thread; pass a coherent HUD snapshot to rendering.
- Keep DLL entry points and application composition separate from feature and graphics implementations. Split by responsibility, not an arbitrary file-length limit.
- Update architecture documentation and checker rules together when intentionally changing a boundary. Do not weaken a check solely to make a failing change pass.
- When delegating, include the relevant architectural constraints and owned paths in each task. Review the integrated dependency graph and file layout before completing the task.
- Run `architecture-check` and CTest before completing tasks that change project code, tests, build configuration, or architecture checker rules. Discussion, planning, read-only investigation, and documentation-only changes do not require these checks unless the user explicitly requests them. For tasks requiring validation, report any checks that could not run and any behavior requiring in-game verification.

## Build, Test, and Development Commands

Use Visual Studio 2022 with Desktop development with C++, a Windows SDK, CMake 3.28+, and Git. Initial configuration fetches SafetyHook, Zydis, and Dear ImGui.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --target architecture-check
```

These commands configure, compile, and run tests. The installable bundle is `build/Release/PirateHatHUD/`. Load it through a compatible ASI loader to test in game.

In a sandbox, prefer `./cmake/Build.ps1` to avoid MSBuild FileTracker access errors.
It discovers the installed Visual Studio C++ toolchain and bundled CMake/Ninja,
builds with Ninja Multi-Config in `build/ninja/`, and runs architecture-check and
CTest. It reuses downloaded sources from `build/_deps/` when present; missing
dependencies need network access. The Release bundle is
`build/ninja/Release/PirateHatHUD/`. Use `-Configuration Debug` for a Debug build.

The default build checks source layout and includes; configuration checks target dependencies. CTest also exercises the checker against intentional violations. For a check without building or downloading dependencies, use `cmake -P cmake/Architecture.cmake`.

```powershell
python -m pip install --target build/format-tools -r requirements-format.txt
cmake -P cmake/Format.cmake
cmake -DCHECK=ON -P cmake/Format.cmake
```

These install clang-format 22.1.8, format project C++, and check formatting without edits.

## Coding Style & Naming Conventions

Follow `.clang-format` and `.editorconfig`: two spaces, no tabs, LLVM style, a 100-column limit, UTF-8, and a final newline. Keep short calls on one line. Keep short ternary expressions on one line. For long ternaries wrapped by the 100-column limit, put `?` and its matching `:` on new lines, indented two spaces beyond the condition. Run formatting through `cmake/Format.cmake`: `tools/format_cpp.py` applies this rule after clang-format, which determines when an expression needs wrapping. Strings, comments, raw shader strings and preprocessor directives are excluded from this extra pass. When argument or parameter lists need wrapping, put one item per line with a fixed two-space continuation indent and the closing parenthesis on its own line. For multiline braced initializers, start the elements after the opening brace on a new line and put the closing brace on its own line. Use a trailing comma in multiline nested or range-for initializer lists when needed to preserve this layout. Put each constructor initializer on its own line. For multiline `if` conditions, start the condition on the line after `if (` and put `) {` on its own line. Break conditions into logical parts; simplify deeply nested expressions instead of aligning them into a staircase. Separate functions, types and logical phases with blank lines. Preserve include order for Windows/COM compatibility. Use braces for new control-flow blocks and keep headers self-contained. Follow existing `snake_case` functions and variables, `PascalCase` types, `kName` constants, and `g_` globals within the `phi` namespace where applicable.

## Testing Guidelines

Tests use CTest with a standalone C++ executable and `CHECK` assertions; no external test framework or coverage threshold is configured. Follow the `*_tests.cpp` naming pattern. Extend scanner cases for missing, unique, ambiguous, and malformed patterns. For overlay changes, verify `force_show=1`, toggle/unload hotkeys, and normal treasure detection in game; record game version, graphics settings, and relevant logs.

## Commit & Pull Request Guidelines

History uses short imperative subjects such as “Add DX12 diagnostic icon overlay”; follow that convention. Keep changes focused. PRs should explain behavior changes and validation, link relevant issues, and include screenshots for HUD changes. Update documentation when configuration or installation changes. Preserve third-party notices and bundled licenses when changing distribution.

### Release commits

- Write release commit messages in English. Use exactly `Release X.Y.Z` as the
  subject, for example `Release 0.6.0`. Do not add `Prepare`, a `v` prefix,
  feature summaries or punctuation to the subject.
- Use this format for the commit that finalizes a mod version and its release
  documentation. Ordinary feature and fix commits keep the imperative style above.
  The version is the mod version, not the supported game version.
- Keep release commits focused on the version bump, changelog, release notes and
  packaging/documentation adjustments needed for that release. Commit substantial
  feature implementations and fixes separately before finalizing the release.
- In the body, use the three labels shown below. Summarize the main user-facing
  changes, actual validation results, and game compatibility or remaining in-game
  checks. Never report an unperformed check as passed.
- Keep the version in `CMakeLists.txt`, `README.md`, `CHANGELOG.md` and
  `release_notes.md` consistent. Generated package metadata derives its version
  from CMake; do not introduce a second version source.
- When creating a release tag, use `vX.Y.Z` pointing to the release commit.
  Creating the commit alone does not publish a release.
- Apply this convention to future releases. Do not rewrite existing commits or
  tags solely to normalize their names.

Message template (replace every placeholder with actual results):

```text
Release X.Y.Z

Changes: <main user-facing changes; details are in CHANGELOG.md>
Validation: <build, architecture-check, formatting and test results>
Compatibility: <verified game version/scenarios and outstanding in-game checks>
```
