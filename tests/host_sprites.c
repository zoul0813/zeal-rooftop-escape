/* Exercise the real HUD/storm owners and grouped uploader against sprite VRAM. */
#include "game.h"
#include "sprite_groups.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

gfx_context vctx;
game_t game;
camera_t cam;
uint8_t controls_pad;
static gfx_sprite arena[ARENA_SIZE], hardware[GFX_SPRITES_COUNT];
static unsigned uploaded;
static int fail_at = -1;
static game_entity_t hero;
static uint8_t have_hero;

void *mem_cpy(void *dst, const void *src, size_t n) { return memcpy(dst, src, n); }
int mem_cmp(const void *p1, const void *p2, size_t n) { return memcmp(p1, p2, n); }
void *mem_set(void *dst, uint8_t value, size_t n) { return memset(dst, value, n); }
void sprites_dma_prepare(void) {} /* Registration only; grouped uploads use the SDK range API. */
game_entity_t *hero_entity(void) { return have_hero ? &hero : NULL; }
char rand8(void) { return 0; }
void world_flash(void) {}
void game_sfx_thunder(void) {}

gfx_error gfx_sprite_render_array(gfx_context *ctx, uint8_t first, const gfx_sprite *data, uint8_t n) {
    assert(ctx == &vctx);
    assert(first + n <= ARENA_SIZE);
    if (first == fail_at) return GFX_INVALID_ARG;
    memcpy(hardware + first, data, n * sizeof(*data));
    uploaded += n;
    return GFX_SUCCESS;
}

static unsigned flush(void) {
    rooftop_sprites_prepare();
    uploaded = 0;
    assert(rooftop_sprites_upload(&vctx) == GFX_SUCCESS);
    assert(memcmp(arena, hardware, sizeof(arena)) == 0);
    return uploaded;
}

int main(void) {
    gfx_sprite hidden = {0};
    unsigned i;
    const unsigned cloud = HUD_SPRITES + TEXT_LINES * TEXT_LEN;
    const unsigned edge = cloud + STORM_ROWS;
    const unsigned actors = edge + STORM_ROWS;
    assert(rooftop_sprites_init(arena) == GFX_SUCCESS);
    assert(hud_init() == ERR_SUCCESS);
    storm_init();
    for (i = 0; i < POOL_SIZE; i++) assert(sprites_register(hidden));
    assert(flush() == ARENA_SIZE);
    assert(flush() == 0);

    hud_show(1); game.lives = 3; hud_draw();
    assert(flush() == HUD_SPRITES);
    hud_draw(); assert(flush() == 0);
    game.lives--; hud_draw(); assert(flush() == 3);
    game.meter++; hud_draw(); assert(flush() == METER_MAX);
    game.score = 123; hud_draw(); assert(flush() == 5);
    text_show(0, "LONG BANNER", 64); assert(flush() == TEXT_LEN);
    text_show(0, "SHORT", 64); assert(flush() == TEXT_LEN);
    for (i = 5; i < TEXT_LEN; i++)
        assert(hardware[HUD_SPRITES + i].y == 0 || hardware[HUD_SPRITES + i].y >= 256);
    text_hide(0); assert(flush() == TEXT_LEN);
    controls_pad = 1;
    text_show(1, "PAD", 84); assert(flush() == TEXT_LEN);
    text_hide_all(); assert(flush() == TEXT_LINES * TEXT_LEN);
    hud_show(0); assert(flush() == HUD_SPRITES);

    storm_show(1); assert(flush() == STORM_SPRITES);
    for (i = 0; i < 7; i++) { storm_update(); assert(flush() == 0); }
    storm_update(); assert(flush() == STORM_ROWS);
    assert(hardware[cloud].x == 16 && hardware[edge].x >= 32);
    have_hero = 1;
    for (i = 0; i < 4; i++) { storm_update(); assert(flush() == STORM_SPRITES); }
    assert(hardware[cloud].x == 20);
    storm_update(); assert(flush() == 0);
    have_hero = 0;
    for (i = 0; i < 4; i++) { storm_update(); assert(flush() == STORM_SPRITES); }
    assert(hardware[cloud].x == 16);
    storm_show(0); assert(flush() == STORM_SPRITES);

    arena[actors].x = 40; arena[actors].y = 50;
    assert(flush() == POOL_SIZE);
    assert(flush() == 0);
    arena[actors].tile++;
    assert(flush() == POOL_SIZE);
    arena[actors].y = 0; /* A despawn must erase the old hardware sprite. */
    assert(flush() == POOL_SIZE);

    /* Failure in a later group must not lose its state or replay earlier groups. */
    game.score++; hud_show(1); hud_draw();
    arena[actors + 1].y = 80;
    rooftop_sprites_prepare(); uploaded = 0; fail_at = actors;
    assert(rooftop_sprites_upload(&vctx) == GFX_INVALID_ARG);
    assert(uploaded == HUD_SPRITES);
    fail_at = -1;
    assert(flush() == POOL_SIZE);
    assert(flush() == 0);
    puts("sprites: clean frames, HUD/banner changes, storm groups, actor hide/reuse and failed-upload retry passed");
    return 0;
}
