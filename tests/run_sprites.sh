#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$HERE/.."
DEPS="${ZDE_PATH:+$ZDE_PATH/home}"
ZGDK="${ZGDK_PATH:-$DEPS/zeal-game-dev-kit}"
ZOS="${ZOS_PATH:-$DEPS/Zeal-8-bit-OS}"
ZVB="${ZVB_SDK_PATH:-$DEPS/Zeal-VideoBoard-SDK}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
cc -std=gnu11 -g -fsanitize=address,undefined \
    '-D__sfr=static volatile unsigned char' '-D__at(x)=' '-D__banked=' \
    -D__SDCC_VERSION_MAJOR=4 -D__SDCC_VERSION_MINOR=4 '-D__sdcccall(x)=' \
    -I"$HERE/stub" -I"$GAME/src" -I"$ZGDK/include" \
    -I"$ZOS/sdk/sdcc/include" -I"$ZVB/include" \
    "$HERE/host_sprites.c" "$GAME/src/sprite_groups.c" "$GAME/src/hud.c" "$GAME/src/storm.c" \
    "$ZGDK/src/sprites/state.c" "$ZGDK/src/sprites/register.c" "$ZGDK/src/sprites/register_arena.c" \
    "$ZGDK/src/hud/sprite/"*.c "$ZGDK/src/ascii.c" -o "$OUT/sprites"
"$OUT/sprites"
