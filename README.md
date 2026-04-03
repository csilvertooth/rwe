# Annihilation Engine

A cross-platform, open-source game engine for Total Annihilation,
focused on faithful recreation of the original gameplay experience
with support for modern systems and mods like TA Escalation.

## About

Annihilation Engine is a fork of the Robot War Engine (RWE) project,
an open-source real-time strategy game engine that is highly compatible
with Total Annihilation data files. This fork focuses on:

- macOS ARM (Apple Silicon) support
- TA Escalation mod compatibility
- Implementing the TA Unofficial Beta Patch 3.9.02 feature set
- Modern networking with NAT traversal
- Preserving the authentic TA look and feel

## Project Status

### Phase 1 -- macOS ARM Build [COMPLETE]
- Cross-platform build scripts (macOS, Linux, Windows)
- SDL3 migration (vendored submodule, static linked)
- GitHub Actions CI for Linux (gcc-14, clang-18), macOS ARM, Windows (MSVC, MinGW64)

### Phase 2 -- Core Engine [IN PROGRESS]
| Feature | Status |
|---------|--------|
| EXPLODE opcode (unit deaths) | Done |
| COB GET/SET completeness (30+ values) | Done |
| Fog of war + line-of-sight | Done |
| Radar / sonar / jamming | Not started |
| Category-based weapon targeting | Done |
| Guided projectile physics (missiles, bombs) | Done |
| Beam weapons (BeamWeapon, Duration) | Done |
| Reclaim / D-Gun / capture | Not started |
| Veterancy system (XP, stat bonuses) | Done |
| Paralyzer/EMP system | Done |
| Self-destruct / kamikaze | Done |

### Phase 3 -- 3.9.02 Patch Parity
Interceptor/shields, transports, naval, teleporters, stockpile weapons,
cloaking, DamageModifier/ArmorType, area commands, MegaMap strategic overlay.

### Phase 4 -- Modernization
GameNetworkingSockets networking, NAT traversal, basic AI, pathfinding
improvements, high-DPI support, expanded player slots.

## Acknowledgments

This project would not be possible without the work of:

- **Michael Heasell (MHeasell)** -- Creator of Robot War Engine, the foundation of
  this project is built upon. Licensed under GPLv3.
  https://github.com/MHeasell/rwe
- **Cavedog Entertainment** -- Original creators of Total Annihilation (1997).
- **N72** -- Creator of the TA Unofficial Beta Patch 3.9.02.
- **The TA Escalation Team** -- Creators of the TA Escalation mod.
  https://taesc.tauniverse.com
- **The TAUniverse Community** -- For decades of modding, mapping, and
  keeping Total Annihilation alive.
  https://www.tauniverse.com

## Build Status

[![GitHub Build Status](https://github.com/csilvertooth/rwe/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/csilvertooth/rwe/actions/workflows/build.yml)

## How to Build

### Prerequisites

Requires Total Annihilation data files (.hpi, .ufo, rev31.gp3, etc.)

### macOS (Apple Silicon / Intel)

    scripts/build-macos.sh Release

Or manually:

    brew install cmake glew zlib libpng

    git clone https://github.com/csilvertooth/rwe.git
    cd rwe
    git submodule update --init --recursive --depth 1

    cd libs && ./build-protobuf.sh && cd ..

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(sysctl -n hw.ncpu)

    ./build/AnnihilationEngine --data-path /path/to/ta/data

### Linux (Ubuntu/Debian)

    scripts/build-linux.sh Release

Or manually:

    sudo apt-get install gcc g++ cmake libglew-dev zlib1g-dev libpng-dev \
      libasound2-dev libpulse-dev libpipewire-0.3-dev libwayland-dev \
      wayland-protocols libxkbcommon-dev libdecor-0-dev autoconf automake libtool

    git clone https://github.com/csilvertooth/rwe.git
    cd rwe
    git submodule update --init --recursive --depth 1

    cd libs && ./build-protobuf.sh && cd ..

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)

    ./build/AnnihilationEngine --data-path /path/to/ta/data

### Windows (MSYS2 MinGW64)

    scripts/build-windows.sh Release

## How to Play

- Arrow keys to scroll the map
- Left click to select units
- Right click to move/attack
- A then click to attack-move
- S to stop
- F10 for in-game debug menu (spawn units, toggle fog of war)
- F11 for global debug menu

## License

GPLv3 -- see [LICENSE](LICENSE) for details.

The original Robot War Engine is Copyright (C) Michael Heasell, licensed under GPLv3.
