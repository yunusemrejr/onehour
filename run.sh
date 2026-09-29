#!/usr/bin/env bash
# One Hour - build (if needed) and launch.
#   ./run.sh              start the game
#   ./run.sh --scale 2    bigger window (logical resolution stays 1024x640)
#   ./run.sh --selftest   headless AI-vs-AI check
set -euo pipefail
cd "$(dirname "$0")"

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing: $1 (try: sudo apt install build-essential)"; exit 1; }; }
need g++; need make

# SDL2 runtime is required; headers are vendored so no -dev package is needed
if ! ldconfig -p 2>/dev/null | grep -q 'libSDL2-2.0.so.0' && [ ! -e /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 ]; then
  echo "SDL2 runtime not found. Install it with: sudo apt install libsdl2-2.0-0"
  exit 1
fi

make -s -j"$(nproc)" || { echo "build failed"; exit 1; }

# Prefer the platform's native windowing; SDL picks Wayland or X11 itself.
exec ./build/onehour "$@"
