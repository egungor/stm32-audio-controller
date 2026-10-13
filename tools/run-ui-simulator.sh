#!/usr/bin/env bash
# Build the headless UI simulator, run its script and convert every frame
# to PNG with tools/fb2png.py.
#
# Usage: tools/run-ui-simulator.sh [output directory]   (default: build/ui-sim)
# Needs: CMake, a C compiler, Python 3, and the LVGL submodule
#        (git submodule update --init).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/ui-sim-build"
OUT_DIR="$(mkdir -p "${1:-$REPO_ROOT/build/ui-sim}" && cd "${1:-$REPO_ROOT/build/ui-sim}" && pwd)"

cmake -S "$REPO_ROOT/firmware/simulator" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel

rm -f "$OUT_DIR"/*.bin "$OUT_DIR"/*.png
"$BUILD_DIR/ui_simulator" "$OUT_DIR"

for dump in "$OUT_DIR"/*.bin; do
    python3 "$REPO_ROOT/tools/fb2png.py" "$dump" "${dump%.bin}.png" > /dev/null
    rm "$dump"
done
echo "Screenshots in $OUT_DIR"
