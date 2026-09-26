# Repository Guidelines

## Project Structure & Module Organization

PirateHatHUD is a Windows x64 C++23 ASI mod for Crimson Desert. `src/main.cpp` manages configuration, state hooks, logging, and lifecycle; `src/pattern_scan.cpp` locates treasure-counter instructions; `src/overlay.cpp` renders the DirectX 12 HUD. Headers and instruction patterns live in `include/`. `tests/scanner_tests.cpp` covers scanner behavior. `assets/icon.png` is the required HUD image, and `config.ini` contains runtime defaults. `cmake/Format.cmake` manages formatting. Build outputs and fetched dependencies belong in `build/`.

## Build, Test, and Development Commands

Use Visual Studio 2022 with Desktop development with C++, a Windows SDK, CMake 3.28+, and Git. Initial configuration fetches SafetyHook, Zydis, and Dear ImGui.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

These commands configure, compile, and run tests. The installable bundle is `build/Release/PirateHatHUD/`. Load it through a compatible ASI loader to test in game.

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
