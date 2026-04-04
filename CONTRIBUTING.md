# Contributing to Annihilation Engine

## Getting Started

### Prerequisites

- C++20 compiler (GCC 14+, Clang 18+, MSVC 2026+)
- CMake 3.16+
- Platform-specific dependencies (see below)

### Build

Use the platform build script for the quickest setup:

```bash
scripts/build-macos.sh [Debug|Release]   # macOS
scripts/build-linux.sh [Debug|Release]   # Linux
scripts/build-windows.sh [Debug|Release] # Windows (MSYS2 MinGW64)
```

Or build manually:

```bash
git submodule update --init --recursive --depth 1
cd libs && ./build-protobuf.sh && cd ..
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)       # Linux
cmake --build build -j$(sysctl -n hw.ncpu)  # macOS
```

### Run Tests

```bash
./build/rwe_test
```

### Run the Engine

```bash
./build/AnnihilationEngine --data-path /path/to/ta/data
```

## Architecture Overview

The engine is built as a static library `librwe` linked by executables. The source is organized into 5 logical sub-libraries (CMake file lists) with a clear dependency hierarchy:

```
UTIL_FILES (foundations — no engine deps)
    ↑
IO_FILES (file parsers — depends on util only)
    ↑
SIM_FILES (simulation — depends on util only, no render deps)
    ↑
RENDER_FILES (OpenGL — depends on util, grid)
    ↑
GAME_FILES (integration — depends on all above)
```

### Sub-library Details

| Sub-library | Subsystems | Description |
|-------------|-----------|-------------|
| **UTIL_FILES** | `util/`, `math/`, `geometry/`, `grid/`, `collections/` | Shared foundations: logging, math, data structures, opaque IDs |
| **IO_FILES** | `io/`, `vfs/` | File format parsers (HPI, GAF, TDF, 3DO, etc.) and virtual file system |
| **SIM_FILES** | `sim/`, `cob/`, `pathfinding/` | Deterministic simulation, COB VM, A* pathfinding. Uses `SimScalar`/`SimVector`/`SimAngle` types. No rendering dependencies. |
| **RENDER_FILES** | `render/` | OpenGL 3.2+ rendering with GLSL 150 shaders |
| **GAME_FILES** | `game/`, `scene/`, `ui/`, `sdl/`, `proto/` | Integration layer: game scenes, network, player commands, UI, SDL context |

## Code Conventions

### Style
- All code in `rwe::` namespace
- Allman brace style, 4-space indent (enforced by `.clang-format`)
- C++20 standard

### Strong Typing
Use opaque IDs (`UnitId`, `PlayerId`, `ProjectileId`) instead of raw integers.

### Variant State Machines
Unit behavior and scene transitions use `std::variant`-based state machines. Use `match()` from `util/match.h` for pattern matching:

```cpp
#include <rwe/util/match.h>

match(someVariant,
    [](const StateA& a) { /* handle A */ },
    [](const StateB& b) { /* handle B */ });
```

### Error Handling

- **Loading/initialization**: Exceptions are acceptable (fail fast on bad data)
- **Runtime/gameplay code**: Never throw. Use `LOG_ERROR` + graceful degradation (return default, skip operation)
- **Definition lookups**: Use `GameSimulation::tryGetUnitDefinition()`, `tryGetUnitModelDefinition()`, `tryGetWeaponDefinition()`, `tryGetUnitScript()` — these return `const T*` (nullptr on miss, logs error). Never use `.at()` on definition maps in gameplay code.
- **Optionals**: Always check before calling `.value()` in runtime code
- **Container access**: Bounds-check vectors before indexing (e.g., `sprites`, `pieces`, `locals`). See `util/safe_lookup.h` for map utilities.

### Logging

Use `SimpleLogger` macros: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_CRITICAL`.

```cpp
#include <rwe/util/SimpleLogger.h>
LOG_ERROR << "Failed to find unit: " << unitId.value;
```

Log level is controlled at compile time via `RWE_LOG_LEVEL` (0=Debug through 5=Off).

### Cross-Platform

- Use `std::filesystem::path` for all file paths (never hardcode separators)
- Platform-specific code must be behind `RWE_PLATFORM_WINDOWS`, `RWE_PLATFORM_APPLE`, or `RWE_PLATFORM_LINUX` guards
- Use CMake `target_compile_definitions()` (not `add_definitions()`)

## CI

GitHub Actions runs on every push to `master` and all PRs:
- **Linux**: GCC 14 + Clang 18, Debug + Release
- **macOS**: ARM, Debug + Release
- **Windows**: MSVC 2026 + MinGW64, Debug + Release

All platforms run unit tests. Windows builds produce installer artifacts.
