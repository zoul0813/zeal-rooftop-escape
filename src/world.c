#include <zgdk/tilemap/flags.h>
/*
 * The rooftops: the course (course.c) pages the leg through a 120x15 world buffer, streamed into
 * layer 1 by the shared core (stream.h). The skyline on layer 0 is one 80-column ring
 * (sky.ztm) drawn once at startup, scrolled at half speed from the leg's origin.
 */
#include <core.h>
#include "game.h"
#include "sky.h"
#include "assets.h"
#include "zgdk/tilemap/scroll.h"

TILEFLAGS_DEFINE_CACHE(15);

#define FLASH_FRAMES 4

uint8_t *world;
spawn_t leg_spawns[MAX_SPAWNS];
camera_t cam;
static uint8_t sky_step;
static uint8_t flash;
static zar_entry_t leg_file;
static uint8_t leg_file_of = 0xFF;

static const uint8_t TILE_FLAGS[256] = { TILE_FLAGS_INIT };

/* Call after load_tileset(): the world is the end of its load buffer */
zos_err_t world_init(void) {
    uint8_t y;
    uint16_t size = 80 * WORLD_H;
    zos_err_t err;
    // music patterns first, then the world tiles
    world = asset_work + MUSIC_RAM;
    // the world buffer is free until the first leg loads: read the skyline through it
    size = WORLD_W * WORLD_H;
    err = zar_entry_load(&pack, "sky.ztm", world, &size);
    if (err != ERR_SUCCESS) return err;
    for (y = 0; y < WORLD_H; y++) {
        gfx_tilemap_load(&vctx, world + (uint16_t)y * 80, 80, SKY_LAYER, 0, y);
    }
    tilemap_fill(&vctx, SKY_LAYER, SKY_C, 0, WORLD_H, 80, 40 - WORLD_H);
    tileflags_register(TILE_FLAGS);
    // open sky to the left, right and below: falling past the bottom is a miss, not a floor
    tileflags_set_outside(TF_NONE);
    return ERR_SUCCESS;
}

static uint8_t find_leg(uint8_t leg) {
    char name[] = "leg0.ztm";
    if (leg_file_of == leg) return 0;
    name[3] = '0' + leg;
    if (zar_entry_from_name(&pack, name, &leg_file) != ERR_SUCCESS) return 1;
    leg_file_of = leg;
    return 0;
}

/* legN.ztm: native 40x15 tiled2zeal screen exports, consecutive pages. */
uint8_t course_page_read(uint8_t leg, uint8_t page, uint8_t *buf) {
    uint16_t size = WORLD_H * PAGE_W;
    if (find_leg(leg)) return 1;
    return zar_entry_read_at(&pack, &leg_file, (uint16_t)page * (WORLD_H * PAGE_W), buf, &size) != ERR_SUCCESS ||
           size != WORLD_H * PAGE_W;
}

uint8_t course_spawns_read(uint8_t leg) {
    const leg_t *l = &LEGS[leg];
    char name[] = "leg0.spn";
    zar_entry_t file;
    uint16_t size = (uint16_t)l->spawn_count * sizeof(spawn_t);
    name[3] = '0' + leg;
    if (zar_entry_from_name(&pack, name, &file) != ERR_SUCCESS) return 1;
    return zar_entry_read_at(&pack, &file, 0, leg_spawns, &size) != ERR_SUCCESS ||
           size != (uint16_t)l->spawn_count * sizeof(spawn_t);
}

void world_set_sky(uint8_t step) {
    if (step >= SKY_STEPS) step = SKY_STEPS - 1;
    sky_step = step;
    flash = 0;
    gfx_palette_load(&vctx, (void *)SKY_PALETTES[step], SKY_COLORS * 2, 0);
    gfx_palette_load(&vctx, (void *)MOON_PALETTES[step], 2 * 2, MOON_COLOR);
}

void world_flash(void) {
    flash = FLASH_FRAMES;
}

void world_frame(void) {
    course_frame();
    tilemap_stream_update(&vctx, &stream, cam.x, cam.y);
    entity_view_set(cam.x, cam.y);
}

void world_scroll(void) {
    uint16_t sky = ((uint16_t)course_base * 16 + cam.x) >> 1;
    camera_scroll_layer(&cam, PLAY_LAYER, 0, 0);
    // half speed from the leg's start: the camera's origin jumps by a page at each rebase
    while (sky >= 1280) sky -= 1280;
    tilemap_scroll(SKY_LAYER, sky, 0);
    // lightning: two bright frames, one dim, then back to the leg's sky
    if (flash) {
        flash--;
        gfx_palette_load(&vctx, (void *)SKY_PALETTES[flash == 0 ? sky_step : flash == 1 ? SKY_FLASH_DIM : SKY_FLASH],
                         SKY_COLORS * 2, 0);
    }
}
