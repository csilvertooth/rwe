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

    brew install cmake glew boost sdl2 sdl2_image sdl2_mixer libpng zlib autoconf automake libtool

    git clone https://github.com/csilvertooth/rwe.git
    cd rwe
    git submodule update --init --recursive

    cd libs && ./build-protobuf.sh && cd ..

    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    cmake --build build -j$(sysctl -n hw.ncpu)

    ./build/rwe --data-path /path/to/ta/data

### Linux (Ubuntu/Debian)

    sudo apt-get install gcc g++ cmake libboost-dev libboost-filesystem-dev \
      libboost-program-options-dev libsdl2-dev libsdl2-image-dev \
      libsdl2-mixer-dev libglew-dev zlib1g-dev libpng-dev

    git clone https://github.com/csilvertooth/rwe.git
    cd rwe
    git submodule update --init --recursive

    cd libs && ./build-protobuf.sh && cd ..

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)

    ./build/rwe --data-path /path/to/ta/data

### Windows

See the original [RWE build instructions](https://github.com/MHeasell/rwe#how-to-build)
for Visual Studio and MSYS2 setup.

## How to Play

- Arrow keys to scroll the map
- Left click to select units
- Right click to move/attack
- A then click to attack-move
- S to stop
- F10 for in-game debug menu (spawn units)
- F11 for global debug menu

## License

GPLv3 -- see [LICENSE](LICENSE) for details.

The original Robot War Engine is Copyright (C) Michael Heasell, licensed under GPLv3.
