#!/usr/bin/env bash
# Builds build/libh2d_native.a (static) from native/h2d_native.c.
# `bit build` runs this automatically ([build] -> native in Bit.hk).
set -euo pipefail
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
AR="${AR:-ar}"
mkdir -p build
CFLAGS="${CFLAGS:--O2 -Wall -Wextra}"
# shellcheck disable=SC2046
$CC $CFLAGS -fPIC -c native/h2d_native.c -o build/h2d_native.o $(pkg-config --cflags sdl2 SDL2_image SDL2_mixer SDL2_gfx SDL2_ttf)
$AR rcs build/libh2d_native.a build/h2d_native.o
echo "built build/libh2d_native.a"
