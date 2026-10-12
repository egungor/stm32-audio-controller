#!/usr/bin/env bash
# Build the STM32 firmware on Linux with tools taken from .toolchain/.
#
# Purpose: a secondary, Linux-only build used by Claude's sandbox (which
# cannot run the Windows ST toolchain) and usable by any Linux developer.
# The reference build remains the Windows ST toolchain (docs/toolchain.md).
#
# Expected archives in <repo>/.toolchain/ (gitignored, not committed):
#   arm-gnu-toolchain-<ver>-x86_64-arm-none-eabi.tar.xz  (prefer 14.3.rel1)
#   cmake-<ver>-linux-x86_64.tar.gz                      (prefer 3.26.3)
#   ninja-linux.zip
# Exactly one archive of each kind must be present at the top level of
# .toolchain/ (subfolders are ignored, e.g. .toolchain/unused/). The versions actually
# used are printed at the start of every build.
#
# Archives are extracted once into $TOOLS_DIR (default ~/.cache/stm32-tools),
# outside the repository, so symlinks and exec bits survive.
#
# Usage: tools/build-firmware-linux.sh [Debug|Release]
set -euo pipefail

BUILD_TYPE="${1:-Debug}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCHIVES="$REPO_ROOT/.toolchain"
TOOLS_DIR="${TOOLS_DIR:-$HOME/.cache/stm32-tools}"
FW_DIR="$REPO_ROOT/firmware/STM32AudioController"
BUILD_DIR="$FW_DIR/build/linux-$BUILD_TYPE"

# find_one <glob>: print the single archive in .toolchain/ matching <glob>
find_one() {
    shopt -s nullglob
    local matches=("$ARCHIVES"/$1)
    shopt -u nullglob
    if [[ ${#matches[@]} -ne 1 ]]; then
        echo "error: expected exactly one $1 in $ARCHIVES, found ${#matches[@]}" >&2
        echo "       (see docs/toolchain.md, 'Linux build')" >&2
        exit 1
    fi
    basename "${matches[0]}"
}

mkdir -p "$TOOLS_DIR"

# Re-extract the compiler whenever the archive in .toolchain/ changes, and
# drop the old build directory so nothing built by the previous compiler
# is reused.
GCC_ARCHIVE="$(find_one 'arm-gnu-toolchain-*-x86_64-arm-none-eabi.tar.xz')"
GCC_STAMP="$TOOLS_DIR/gcc/.archive"
if [[ ! -x "$TOOLS_DIR/gcc/bin/arm-none-eabi-gcc" ]] \
   || [[ "$(cat "$GCC_STAMP" 2>/dev/null)" != "$GCC_ARCHIVE" ]]; then
    echo "Extracting $GCC_ARCHIVE ..."
    rm -rf "$TOOLS_DIR/gcc" && mkdir -p "$TOOLS_DIR/gcc"
    tar -xJf "$ARCHIVES/$GCC_ARCHIVE" -C "$TOOLS_DIR/gcc" --strip-components=1
    echo "$GCC_ARCHIVE" > "$GCC_STAMP"
    rm -rf "$BUILD_DIR"
fi

if [[ ! -x "$TOOLS_DIR/cmake/bin/cmake" ]]; then
    CMAKE_ARCHIVE="$(find_one 'cmake-*-linux-x86_64.tar.gz')"
    echo "Extracting $CMAKE_ARCHIVE ..."
    rm -rf "$TOOLS_DIR/cmake" && mkdir -p "$TOOLS_DIR/cmake"
    tar -xzf "$ARCHIVES/$CMAKE_ARCHIVE" -C "$TOOLS_DIR/cmake" --strip-components=1
fi

if [[ ! -x "$TOOLS_DIR/ninja/ninja" ]]; then
    NINJA_ARCHIVE="$(find_one 'ninja-linux.zip')"
    echo "Extracting $NINJA_ARCHIVE ..."
    rm -rf "$TOOLS_DIR/ninja" && mkdir -p "$TOOLS_DIR/ninja"
    unzip -q "$ARCHIVES/$NINJA_ARCHIVE" -d "$TOOLS_DIR/ninja"
    chmod +x "$TOOLS_DIR/ninja/ninja"
fi

export PATH="$TOOLS_DIR/gcc/bin:$TOOLS_DIR/cmake/bin:$TOOLS_DIR/ninja:$PATH"

echo "== Tool versions"
arm-none-eabi-gcc --version | head -1
cmake --version | head -1
echo "ninja $(ninja --version)"

echo "== Configure ($BUILD_TYPE) -> $BUILD_DIR"
cmake -S "$FW_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$FW_DIR/cmake/gcc-arm-none-eabi.cmake" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "== Build"
cmake --build "$BUILD_DIR"

echo "== Size"
arm-none-eabi-size "$BUILD_DIR/STM32AudioController.elf"
