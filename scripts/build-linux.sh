#!/bin/bash
set -euo pipefail

# Annihilation Engine - Linux Build Script
# Tested on Ubuntu 24.04

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="$ROOT_DIR/build"
JOBS=$(nproc)

echo "=== Annihilation Engine - Linux Build ==="
echo "Build type: $BUILD_TYPE"
echo "Jobs: $JOBS"
echo ""

# Check/install dependencies
echo "--- Checking dependencies ---"
PACKAGES=(
    gcc g++ cmake
    libglew-dev zlib1g-dev libpng-dev
    libasound2-dev libpulse-dev
    libpipewire-0.3-dev
    libwayland-dev wayland-protocols
    libxkbcommon-dev libdecor-0-dev
    autoconf automake libtool
)

missing=()
for pkg in "${PACKAGES[@]}"; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        missing+=("$pkg")
    fi
done

if [ ${#missing[@]} -gt 0 ]; then
    echo "Installing missing packages: ${missing[*]}"
    sudo apt-get update -y
    sudo apt-get install -y "${missing[@]}"
else
    echo "All dependencies present."
fi

# Init submodules if needed
echo ""
echo "--- Initializing submodules ---"
cd "$ROOT_DIR"
if [ ! -f "libs/sdl3/CMakeLists.txt" ]; then
    git submodule update --init --recursive --depth 1
    echo "Submodules initialized."
else
    echo "Submodules already initialized."
fi

# Build protobuf if needed
echo ""
echo "--- Building protobuf ---"
cd "$ROOT_DIR/libs"
./build-protobuf.sh

# CMake configure
echo ""
echo "--- CMake configure ---"
cd "$ROOT_DIR"
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# Build
echo ""
echo "--- Building ($JOBS threads) ---"
cmake --build "$BUILD_DIR" -j "$JOBS"

# Run tests
echo ""
echo "--- Running tests ---"
cd "$BUILD_DIR"
ctest --output-on-failure

echo ""
echo "=== Build complete ==="
echo "Binary: $BUILD_DIR/AnnihilationEngine"
echo ""
echo "Run with:"
echo "  $BUILD_DIR/AnnihilationEngine --data-path /path/to/ta/data"
