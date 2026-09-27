# Tests

Tests follow the production module boundaries. Each C++ test remains a standalone
executable with `CHECK` assertions; no game process is needed.

| Directory / CTest label | Coverage |
| --- | --- |
| `core` | Signal dispatch and subscription lifetime |
| `game` | Scanner, memory traversal, observer startup and hook retention |
| `features` | Treasure indicator and sound policy |
| `platform` | Hotkeys, logging, WAV parsing and playback worker |
| `render` | GPU wait policy, swapchain selection, icon commands, shaders and images |
| `app` | Composition and cleanup, using test doubles |
| `tooling` | Architecture checker, packaging, release validation and formatting tools |

`tests/CMakeLists.txt` owns test targets, dependencies and CTest registration.
Production sources and headers stay under `src/` and `include/`. Add new test
executables to the relevant label as well as registering them with `add_test`.
Python and CMake regression scripts live under `tooling/`; the production tools
they exercise remain under `tools/` and `cmake/`.

Build and run all checks from the repository root:

```powershell
./cmake/Build.ps1
```

After building, run a group or an existing test by its unchanged name:

```powershell
ctest --test-dir build/ninja -C Release -L game --output-on-failure
ctest --test-dir build/ninja -C Release -R '^menu_tests$' --output-on-failure
```

Game-hook activation, DX12 drawing and actual sound playback still require
in-game verification; see `docs/architecture.md` for the validation limits.
