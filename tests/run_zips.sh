#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$HERE/.."
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
cc -std=gnu11 -w -g -fsanitize=address,undefined \
    '-D__sfr=static volatile unsigned char' '-D__at(x)=' \
    '-D__banked=' -D__SDCC_VERSION_MAJOR=4 -D__SDCC_VERSION_MINOR=4 '-D__sdcccall(x)=' \
    -I"$HERE/stub" -I"$GAME/src" -I"$ZGDK_PATH/include" \
    -I"$ZOS_PATH/sdk/sdcc/include" -I"$ZVB_SDK_PATH/include" \
    "$HERE/host_zips.c" "$GAME/src/zips.c" -o "$OUT/zips"
"$OUT/zips"
