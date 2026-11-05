#!/usr/bin/env bash
# Build and run the host simulation (gcc, no emulator). Usage: tests/run_sim.sh [leg 0-9] [-v]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$HERE/.."
DEPS="${ZDE_PATH:+$ZDE_PATH/home}"
ZOS="${ZOS_PATH:-$DEPS/Zeal-8-bit-OS}"; ZVB="${ZVB_SDK_PATH:-$DEPS/Zeal-VideoBoard-SDK}"; ZGDK="${ZGDK_PATH:-$DEPS/zeal-game-dev-kit}"
[ -d "$ZGDK/include" ] && [ -d "$ZOS/sdk" ] && [ -d "$ZVB/include" ] || {
  echo 'Set ZDE_PATH, or ZGDK_PATH/ZOS_PATH/ZVB_SDK_PATH, to the installed SDK dependencies' >&2
  exit 1
}
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
mkdir -p "$OUT/tiles"
for tmx in "$GAME/assets/legs/"*.tmx; do
    python3 "$ZVB/tools/tiled2zeal/tiled2zeal.py" -i "$tmx" -o "$OUT/tiles/$(basename "${tmx%.tmx}").ztm"
done
python3 "$GAME/tools/pack_maps.py" --out "$OUT/maps" --tiles "$OUT/tiles" > "$OUT/pack.log" 2>&1 || { cat "$OUT/pack.log"; exit 1; }
gcc -std=gnu11 -w -DAUTOPILOT=1 -DASSETS_DIR="\"$OUT/maps\"" '-D__sfr=static volatile unsigned char' '-D__at(x)=' '-D__banked=' -D__SDCC_VERSION_MAJOR=4 -D__SDCC_VERSION_MINOR=4 '-D__sdcccall(x)=' \
    -I"$HERE/stub" -I"$GAME/src" -I"$ZGDK/include" -I"$ZOS/sdk/sdcc/include" -I"$ZVB/include" \
    -o "$OUT/host_sim" "$HERE/host_sim.c" \
    "$GAME/src/hero.c" "$GAME/src/actors.c" "$GAME/src/zips.c" "$GAME/src/course.c" "$GAME/src/stream.c" "$GAME/src/stream_shift.c" "$GAME/src/autopilot.c" "$OUT/maps/map_data.c" \
    "$HERE/physics_reference.c" "$ZGDK/src/entities"/*.c "$ZGDK/src/physics/body"/*.c "$ZGDK/src/utils/clamp_i16.c" "$ZGDK/src/physics/platformer"/*.c "$ZGDK/src/tilemap/flags"/*.c "$ZGDK/src/anims"/*.c "$ZGDK/src/entities/presets"/*.c \
    "$ZGDK"/src/camera/*.c "$ZGDK/src/tilemap/stream.c" "$ZGDK/src/tilemap/stream_reader.c" \
    "$ZGDK/src/tilemap/stream_edit.c" "$ZGDK/src/tilemap/stream_buffer.c" "$ZGDK/src/tilemap/stream_ram.c" "$ZGDK/src/tilemap/stream_rebase.c" "$ZGDK/src/tilemap/stream_ring.c"
"$OUT/host_sim" "$@"
