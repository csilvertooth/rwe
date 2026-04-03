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

Built as a static library `librwe` linked by executables (`AnnihilationEngine`, `rwe_bridge`, `rwe_test`).

Key subsystems:

- **sim/** - Deterministic game simulation (units, weapons, projectiles, terrain, resources). Uses SimScalar/SimVector/SimAngle types.
- **scene/** - Scene state machine: MainMenuScene -> LoadingScene -> GameScene.
- **render/** - OpenGL 3.2+ rendering with GLSL 150 shaders (in `shaders/`).
- **cob/** - COB virtual machine for TA unit behavior scripts.
- **io/** - Parsers for TA formats: HPI, GAF, TDF, 3DO, COB, FBI, TNT, PCX, OTA, GUI.
- **vfs/** - Virtual file system over HPI archives and directories.
- **pathfinding/** - A* pathfinding on grids.

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
- Variant-based state machines for unit behavior
- Error handling uses `Result<T, E>` types

## CI

GitHub Actions: Linux (gcc-14, clang-18), macOS (ARM), Windows (MSVC 2026, MinGW64).
