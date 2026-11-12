#include "game.h"
#include "sprite_groups.h"
#include <string.h>

#define CLOUD_FIRST (HUD_SPRITES + TEXT_LINES * TEXT_LEN)
#define EDGE_FIRST (CLOUD_FIRST + STORM_ROWS)
#define ACTORS_FIRST (EDGE_FIRST + STORM_ROWS)

static const uint8_t first[SPR_GROUP_COUNT] = {
    0, 3, 7,
    HUD_SPRITES, HUD_SPRITES + TEXT_LEN, HUD_SPRITES + 2 * TEXT_LEN,
    CLOUD_FIRST, EDGE_FIRST, ACTORS_FIRST
};
static const uint8_t count[SPR_GROUP_COUNT] = {
    3, METER_MAX, 5, TEXT_LEN, TEXT_LEN, TEXT_LEN,
    STORM_ROWS, STORM_ROWS, POOL_SIZE
};
static gfx_sprite *sprites;
/* Last prepared actor state. A failed upload retains its dirty bit. */
static gfx_sprite last_actors[POOL_SIZE];
static uint16_t dirty;

gfx_error rooftop_sprites_init(gfx_sprite *arena) {
    gfx_error err = sprites_register_arena(arena, ARENA_SIZE);
    if (err != GFX_SUCCESS) return err;
    sprites = arena;
    dirty = (1U << SPR_GROUP_COUNT) - 1;
    return GFX_SUCCESS;
}

void rooftop_sprites_dirty(uint8_t group) {
    dirty |= 1U << group;
}

void rooftop_sprites_prepare(void) {
    if (memcmp(sprites + ACTORS_FIRST, last_actors, sizeof(last_actors))) {
        memcpy(last_actors, sprites + ACTORS_FIRST, sizeof(last_actors));
        rooftop_sprites_dirty(SPR_GROUP_ACTORS);
    }
}

gfx_error rooftop_sprites_upload(gfx_context *ctx) {
    uint8_t group;
    uint16_t bit = 1;
    for (group = 0; group < SPR_GROUP_COUNT; group++, bit <<= 1) {
        gfx_error err;
        if (!(dirty & bit)) continue;
        err = gfx_sprite_render_array(ctx, first[group], sprites + first[group], count[group]);
        if (err != GFX_SUCCESS) return err;
        dirty &= ~bit;
    }
    return GFX_SUCCESS;
}
