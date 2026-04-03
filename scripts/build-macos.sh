#!/bin/bash
set -euo pipefail

# Annihilation Engine - macOS Build Script
# Supports both Apple Silicon (arm64) and Intel (x86_64)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="$ROOT_DIR/build"
JOBS=$(sysctl -n hw.ncpu)

echo "=== Annihilation Engine - macOS Build ==="
echo "Build type: $BUILD_TYPE"
echo "Jobs: $JOBS"
echo "Architecture: $(uname -m)"
echo ""

# Check dependencies
echo "--- Checking dependencies ---"
missing=()
for pkg in glew libpng zlib cmake; do
    if ! brew list "$pkg" &>/dev/null; then
        missing+=("$pkg")
    fi
done

if [ ${#missing[@]} -gt 0 ]; then
    echo "Installing missing packages: ${missing[*]}"
    brew install "${missing[@]}"
else
    echo "All Homebrew dependencies present."
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
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

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
