# Repository Guidelines

## Project Structure & Module Organization

PirateHatHUD is a Windows x64 C++23 ASI mod for Crimson Desert. `src/main.cpp` is the DLL entry point; `src/app.cpp` composes modules and manages lifecycle. `src/game/treasure_observer.cpp` and `src/pattern_scan.cpp` form the `game_observers` static library. `include/core/signal.hpp` provides typed subscriptions. `src/features/treasure_indicator.cpp` forms the indicator policy library; `src/overlay.cpp` renders a HUD snapshot through DirectX 12. Configuration, hotkeys and logging live in `src/platform/`. Headers and instruction patterns live in `include/`; architecture contracts are documented in `docs/architecture.md`. `tests/*_tests.cpp` covers scanner, signals, indicator and observer startup behavior. `assets/icon.png` is the required HUD image, and `config.ini` contains runtime defaults. `cmake/Format.cmake` manages formatting. Build outputs and fetched dependencies belong in `build/`.

## Architecture Rules

- Before adding a module, moving files, or changing dependencies, read `docs/architecture.md`.
- Put every project header (`.hpp`, `.h`) in `include/` and every implementation (`.cpp`) in `src/`. Keep module subdirectories aligned. Internal headers follow the same layout; placement under `include/` does not make them a public API.
- Follow the dependency matrix in `docs/architecture.md`. Game observers must not depend on HUD/rendering or mod configuration; rendering must not depend on game observation.
- Game hook callbacks capture data only. Publish observer events from `poll()` on the owner thread; pass a coherent HUD snapshot to rendering.
- Keep DLL entry points and application composition separate from feature and graphics implementations. Split by responsibility, not an arbitrary file-length limit.
- Update architecture documentation and checker rules together when intentionally changing a boundary. Do not weaken a check solely to make a failing change pass.
- When delegating, include the relevant architectural constraints and owned paths in each task. Review the integrated dependency graph and file layout before completing the task.
- Run `architecture-check` and CTest before completion. Report any checks that could not run and any behavior requiring in-game verification.

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

These install clang-format 20.1.8, format project C++, and check formatting without edits.

## Coding Style & Naming Conventions

Follow `.clang-format` and `.editorconfig`: two spaces, no tabs, LLVM style, a 100-column limit, UTF-8, and a final newline. Preserve include order for Windows/COM compatibility. Use braces for new control-flow blocks and keep headers self-contained. Follow existing `snake_case` functions and variables, `PascalCase` types, `kName` constants, and `g_` globals within the `phi` namespace where applicable.

## Testing Guidelines

Tests use CTest with a standalone C++ executable and `CHECK` assertions; no external test framework or coverage threshold is configured. Follow the `*_tests.cpp` naming pattern. Extend scanner cases for missing, unique, ambiguous, and malformed patterns. For overlay changes, verify `force_show=1`, toggle/unload hotkeys, and normal treasure detection in game; record game version, graphics settings, and relevant logs.

## Commit & Pull Request Guidelines

History uses short imperative subjects such as “Add DX12 diagnostic icon overlay”; follow that convention. Keep changes focused. PRs should explain behavior changes and validation, link relevant issues, and include screenshots for HUD changes. Update documentation when configuration or installation changes. Preserve third-party notices and bundled licenses when changing distribution.
