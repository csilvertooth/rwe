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

The engine is built as a static library `librwe` linked by executables.

| Subsystem | Path | Description |
|-----------|------|-------------|
| **sim/** | `src/rwe/sim/` | Deterministic game simulation (units, weapons, projectiles, terrain, resources). Uses `SimScalar`/`SimVector`/`SimAngle` types for reproducibility. |
| **game/** | `src/rwe/game/` | Game scenes, rendering orchestration, network service, player commands |
| **render/** | `src/rwe/render/` | OpenGL 3.2+ rendering with GLSL 150 shaders |
| **cob/** | `src/rwe/cob/` | COB virtual machine for TA unit behavior scripts |
| **io/** | `src/rwe/io/` | Parsers for TA file formats: HPI, GAF, TDF, 3DO, COB, FBI, TNT, PCX, OTA, GUI |
| **vfs/** | `src/rwe/vfs/` | Virtual file system over HPI archives and directories |
| **pathfinding/** | `src/rwe/pathfinding/` | A* pathfinding on grids |
| **ui/** | `src/rwe/ui/` | UI components and panels |
| **util/** | `src/rwe/util/` | Shared utilities (logging, string ops, safe lookups, match/overloaded, opaque IDs) |

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
- **Runtime/gameplay code**: Never throw. Use `LOG_ERROR` + graceful degradation (return default, skip operation). See `util/safe_lookup.h` for safe map lookups
- **Optionals**: Always check before calling `.value()` in runtime code

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
