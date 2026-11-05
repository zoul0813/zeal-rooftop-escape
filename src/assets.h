#ifndef ROOFTOP_ESCAPE_ASSETS_H
#define ROOFTOP_ESCAPE_ASSETS_H
#include <stdint.h>
#include <zos_errors.h>
#include <zvb_gfx.h>
#include <zar.h>

/* Open before loading assets; close on exit. */
extern zar_file_t pack;

/* From the asset archive, which must be open (zar_file_open(PACK_FILE, &pack), PACK_FILE from cmake/assets.cmake) */
zos_err_t load_palette(gfx_context* ctx);
zos_err_t load_tileset(gfx_context* ctx);

/*
 * One work area: the tileset streams to VRAM through it at startup, then it holds the music
 * patterns (MUSIC_RAM bytes, at the start) and the world tiles (WORLD_W * WORLD_H, after them).
 */
#define ASSET_WORK_SIZE (MUSIC_RAM + WORLD_W * WORLD_H)
extern uint8_t asset_work[];

#endif
