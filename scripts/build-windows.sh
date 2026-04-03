#!/bin/bash
set -euo pipefail

# Annihilation Engine - Windows Build Script (MSYS2 MinGW64)
# Run this from an MSYS2 MinGW64 terminal

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="$ROOT_DIR/build"

echo "=== Annihilation Engine - Windows Build (MinGW64) ==="
echo "Build type: $BUILD_TYPE"
echo ""

# Check we're in MinGW64
if [ "${MSYSTEM:-}" != "MINGW64" ]; then
    echo "ERROR: This script must be run from an MSYS2 MinGW64 terminal."
    echo "Open 'MSYS2 MinGW64' from your Start menu."
    exit 1
fi

# Check/install dependencies
echo "--- Checking dependencies ---"
PACKAGES=(
    autoconf automake libtool make unzip
    mingw-w64-x86_64-toolchain
    mingw-w64-x86_64-glew
    mingw-w64-x86_64-zlib
    mingw-w64-x86_64-libpng
    mingw-w64-x86_64-cmake
)

pacman -S --needed --noconfirm "${PACKAGES[@]}"

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
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# Build
echo ""
echo "--- Building ---"
cmake --build "$BUILD_DIR" -j 4

# Run tests
echo ""
echo "--- Running tests ---"
cd "$BUILD_DIR"
./rwe_test.exe

echo ""
echo "=== Build complete ==="
echo "Binary: $BUILD_DIR/AnnihilationEngine.exe"
echo ""
echo "Run with:"
echo "  $BUILD_DIR/AnnihilationEngine.exe --data-path /path/to/ta/data"
