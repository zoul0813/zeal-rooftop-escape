/*
 * The storm front: two columns of sprites down the left edge of the screen, a dark cloud wall
 * with a ragged, rain-streaked edge that churns. It lunges a few pixels when the runner is close.
 * From leg 3 on, lightning lights the sky now and then.
 */
#include "game.h"

#define STORM_Y0  16          // sprite y offset (sprites are drawn 16 px up and left)
static gfx_sprite *cloud[STORM_ROWS];
static gfx_sprite *edge[STORM_ROWS];
static uint8_t shown;
static uint8_t phase;
static uint8_t lunge;
static uint8_t bolt_wait = 200;

static gfx_sprite *add(void) {
    gfx_sprite s = { .y = 0, .x = 0, .tile = SPR_STORM, .flags = SPRITE_NONE, .options = SPRITE_OPTION_NONE };
    return sprites_register(s);
}

void storm_init(void) {
    uint8_t i;
    for (i = 0; i < STORM_ROWS; i++) {
        cloud[i] = add();
        edge[i] = add();
    }
}

/* the sprites only move when the storm lunges or churns: a few frames out of eight */
static void place_x(void) {
    uint8_t i, sway = (phase >> 3) & 1;
    for (i = 0; i < STORM_ROWS; i++) {
        cloud[i]->x = 16 + lunge;
        // the edge sways in and out by row
        edge[i]->x = 32 + lunge + ((i + sway) & 1) * 3;
    }
}

void storm_show(uint8_t on) {
    uint8_t i;
    shown = on;
    lunge = 0;
    for (i = 0; i < STORM_ROWS; i++) {
        uint16_t y = on ? (uint16_t)i * 16 + STORM_Y0 : 0;
        cloud[i]->y = y;
        edge[i]->y = y;
    }
    place_x();
}

void storm_update(void) {
    game_entity_t *hero = hero_entity();
    uint8_t i, want = 0, moved = 0;
    if (!shown) return;
    if (hero && !hero->entity.physics.visual.hidden && PHYSICS_FP_INT(hero->entity.physics.body.x) - cam.x < 64) want = 4;
    if (lunge != want) {
        lunge += lunge < want ? 1 : -1;
        moved = 1;
    }
    if ((++phase & 7) == 0) {
        uint8_t p = (phase >> 3) % 3;
        for (i = 0; i < STORM_ROWS; i++) {
            edge[i]->tile = SPR_STORM_EDGE1 + p;
            if (++p == 3) p = 0;
        }
        moved = 1;
    }
    if (moved) place_x();
    if (game.leg >= 2 && game.state == GS_PLAY && --bolt_wait == 0) {
        bolt_wait = 150 + (rand8() & 127) + (game.leg < 6 ? 120 : 0);
        world_flash();
        game_sfx_thunder();
    }
}
