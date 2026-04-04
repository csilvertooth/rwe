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

### Phase 2 -- Core Engine [COMPLETE]
| Feature | Status |
|---------|--------|
| EXPLODE opcode (unit deaths) | Done |
| COB GET/SET completeness (30+ values) | Done |
| Fog of war + line-of-sight | Done |
| Radar / sonar / jamming | Done |
| Category-based weapon targeting | Done |
| Guided projectile physics (missiles, bombs) | Done |
| Beam weapons (BeamWeapon, Duration) | Done |
| Reclaim / D-Gun / capture | Done |
| Veterancy system (XP, stat bonuses) | Done |
| Paralyzer/EMP system | Done |
| Self-destruct / kamikaze | Done |

### Phase 2.5 -- Polish & Stability [COMPLETE]
| Feature | Status |
|---------|--------|
| Crash protection (cascade death, dead unit access) | Done |
| In-game options menu (F2, pause, settings) | Done |
| Modern ImGui UI theme | Done |
| TA:Escalation key bindings | Done |
| Nanolathe particle VFX (race-based colors) | Done |
| Build queue visuals (numbered, pulsing lines) | Done |
| Air unit flight model (banking, altitude, obstacles) | Done |
| Health bar batching (performance) | Done |
| Reclaim via right-click | Done |
| D-Gun manual fire (D key) | Done |

### Phase 3 -- 3.9.02 Patch Parity [NEEDS TESTING]
| Feature | Status |
|---------|--------|
| Interceptor system (anti-missile) | Done |
| Transport system (load/unload) | Done |
| Naval waterline | Done |
| Teleporter system | Done |
| Kamikaze units | Done |
| Cloaking system | Done |
| OnOffable / Activation | Done |
| DamageModifier / ArmorType | Done |
| Area reclaim | Done |
| Extended weapon tags (EdgeEffectiveness, Accuracy) | Done |
| Extended FBI tags (30+ new tags) | Done |
| MegaMap strategic overlay (scroll zoom) | Done |

### Phase 4 -- Modernization [IN PROGRESS]
| Feature | Status |
|---------|--------|
| Screen resolution + fullscreen (F11) | Done |
| High-DPI / Retina support | Done |
| Settings persistence (settings.cfg) | Done |
| Options menu (Sound, Interface, Visuals) | Done |
| TA panel scaling to resolution | Done |
| RmlUi integration (foundation) | Done |
| Multiplayer networking (Asio UDP) | Working (localhost) |
| GameNetworkingSockets (Valve) | Built, integration pending |
| NAT traversal | Pending (comes with GNS) |
| Basic AI | Pending |
| SDL3 GPU API rendering | Pending |
| Pathfinding improvements | Pending |
| Multiplayer desync fix | Pending |

## Acknowledgments

This project would not be possible without the work of:

- **Michael Heasell (MHeasell)** -- Creator of Robot War Engine, the foundation
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

## Controls

### Unit Commands (TA:Escalation standard)
| Key | Action |
|-----|--------|
| A | Attack |
| D | D-Gun (commander) |
| E | Reclaim |
| G | Guard |
| M | Move |
| P | Patrol |
| R | Repair |
| S | Stop |
| Ctrl+D | Self-destruct |
| Ctrl+C | Select commander |
| ~ | Toggle health bars |

### Game Controls
| Key | Action |
|-----|--------|
| F2 | Options menu |
| F10 | Debug menu |
| F11 | Global debug |
| Escape | Deselect / cancel |
| Pause | Pause game |
| +/- | Game speed |
| Enter | Cheat console |
| Shift | Queue orders |
| Arrow keys | Scroll map |

### Cheat Console (press Enter)
| Command | Action |
|---------|--------|
| +atm | 10000 metal and energy |
| +army | Spawn army at cursor |
| +kill | Kill selected units |
| +UNITNAME | Spawn unit at cursor |

## License

GPLv3 -- see [LICENSE](LICENSE) for details.

The original Robot War Engine is Copyright (C) Michael Heasell, licensed under GPLv3.
