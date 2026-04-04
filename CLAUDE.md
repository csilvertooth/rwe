# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Annihilation Engine is a cross-platform, open-source game engine for Total Annihilation, forked from Robot War Engine (RWE) by MHeasell. Focused on faithful TA recreation with TA Escalation mod support and macOS ARM compatibility.

## Build Commands

### Quick Build (any platform)

```bash
# Use the platform-specific build script
scripts/build-macos.sh [Debug|Release]   # macOS
scripts/build-linux.sh [Debug|Release]   # Linux
scripts/build-windows.sh [Debug|Release] # Windows (MSYS2 MinGW64)
```

### Manual Build (macOS)

```bash
# First time setup
git submodule update --init --recursive --depth 1
cd libs && ./build-protobuf.sh && cd ..

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# Test
./build/rwe_test

# Run
./build/AnnihilationEngine --data-path /path/to/ta/data
```

### Manual Build (Linux)

```bash
sudo apt-get install gcc g++ cmake libglew-dev zlib1g-dev libpng-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libwayland-dev \
  wayland-protocols libxkbcommon-dev libdecor-0-dev autoconf automake libtool

git submodule update --init --recursive --depth 1
cd libs && ./build-protobuf.sh && cd ..

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Architecture

### Core Engine (`src/rwe/`)

Built as a static library `librwe` linked by executables (`AnnihilationEngine`, `rwe_bridge`, `rwe_test`). The source is organized into 5 logical sub-libraries (defined as separate CMake file lists that compose into `librwe`):

**UTIL_FILES** — Shared foundations:
- **util/** - Logging (`SimpleLogger`), string ops, safe lookups, `match()`, opaque IDs, `Result<T,E>`
- **math/** - Vector/Matrix types, trigonometry
- **geometry/** - Bounding boxes, rays, planes, triangles, rectangles
- **grid/** - 2D grid, discrete rects, directions
- **collections/** - `VectorMap`, `SimpleVectorMap`, `MinHeap`

**IO_FILES** — File format parsers (no sim/render dependencies):
- **io/** - Parsers for TA formats: HPI, GAF, TDF, 3DO, COB, FBI, TNT, PCX, OTA, GUI
- **vfs/** - Virtual file system over HPI archives and directories

**SIM_FILES** — Deterministic game simulation (no render dependencies):
- **sim/** - Units, weapons, projectiles, terrain, resources. Uses `SimScalar`/`SimVector`/`SimAngle` types
- **cob/** - COB virtual machine for TA unit behavior scripts
- **pathfinding/** - A* pathfinding on grids

**RENDER_FILES** — OpenGL rendering:
- **render/** - OpenGL 3.2+ rendering with GLSL 150 shaders (in `shaders/`)

**GAME_FILES** — Integration layer (depends on all above):
- **game/** - Game scenes, network service, player commands, media databases
- **scene/** - Scene state machine: MainMenuScene -> LoadingScene -> GameScene
- **ui/** - UI components and panels
- **sdl/** - SDL3 context management

### Dependencies

- **SDL3** - Vendored submodule (static linked), no system SDL needed
- **SDL3_mixer** - Vendored submodule (track-based audio)
- **Standalone Asio** - Vendored submodule (networking, no Boost)
- **ImGui v1.92** - SDL3 backend
- **Protobuf** - Built from source via `libs/build-protobuf.sh`
- **GLEW, zlib, libpng** - System packages

## Code Conventions

- All C++ code in `rwe::` namespace
- `.clang-format`: Allman brace style, 4-space indent, C++20
- Strong typing via opaque IDs: `UnitId`, `PlayerId`, `ProjectileId`
- Variant-based state machines for unit behavior; use `match()` from `util/match.h`
- Error handling: exceptions at init/loading, `LOG_ERROR` + graceful degradation at runtime (never crash in gameplay code). See `util/safe_lookup.h`
- Logging via `SimpleLogger` macros: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_CRITICAL`
- Platform-specific code behind `RWE_PLATFORM_WINDOWS`, `RWE_PLATFORM_APPLE`, `RWE_PLATFORM_LINUX` guards
- Use `target_compile_definitions()` (not `add_definitions()`) in CMake

## CI

GitHub Actions: Linux (gcc-14, clang-18), macOS (ARM), Windows (MSVC 2026, MinGW64).
