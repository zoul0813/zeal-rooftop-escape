#include <stdint.h>
#include <stddef.h>
#include <zvb_gfx.h>
#include "game.h"
#include "assets.h"
#include <zgdk/tileset.h>

zar_file_t pack;

/* ZAR provides page-safe reads; ZGDK handles chunk decoding and VRAM uploads. */
static zos_err_t read_tileset(void *entry, void *buffer, uint16_t *size) {
    return zar_entry_read(&pack, (zar_entry_t *)entry, buffer, size);
}

/*
 * The tileset, palette, music and legs live in rooftop.zar (cmake/assets.cmake) and are
 * read when needed. The tileset goes to VRAM one LZ chunk at a time through asset_work, before the
 * music patterns and the world move in.
 */
typedef char work_holds_a_tile_chunk[(ASSET_WORK_SIZE >= TILE_CHUNK_MAX) ? 1 : -1];

uint8_t asset_work[ASSET_WORK_SIZE];

zos_err_t load_palette(gfx_context* ctx) {
    uint16_t size = ASSET_WORK_SIZE;
    zos_err_t err = zar_entry_load(&pack, "tiles.ztp", asset_work, &size);
    if (err != ERR_SUCCESS) return err;
    return gfx_palette_load(ctx, asset_work, size, 0) == GFX_SUCCESS ? ERR_SUCCESS : ERR_FAILURE;
}

zos_err_t load_tileset(gfx_context* ctx) {
    zar_entry_t entry;
    zos_err_t err;
    gfx_tileset_options options = { 0 };
    err = zar_entry_from_name(&pack, "tiles.tch", &entry);
    if (err != ERR_SUCCESS) return err;
    return tileset_load_chunks(ctx, read_tileset, &entry, gfx_tileset_load_lz,
                               asset_work, ASSET_WORK_SIZE, &options);
}
