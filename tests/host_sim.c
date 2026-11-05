#include <zgdk/tilemap/flags.h>
/*
 * Host-side simulation of Rooftop Escape (gcc, no emulator): the real hero, actors, zips, course
 * (paging + the storm's scrolling), autopilot, core physics, streaming and level data
 * (generated leg<N>.ztm/leg<N>.spn, the files rooftop.zar is built from) run frame by frame; video, sound and sprites
 * are stubs. A fall or the storm restarts the leg at the last beacon, like the game.
 * Prints how each leg goes for the autopilot, so level and bot changes can be checked in seconds.
 * Usage: tests/run_sim.sh [leg] [-v]    PUSH=1: the bot holds Right (by default it never pushes,
 * so a clean run proves the leg can be done at the storm's speed)
 */
#include "game.h"   /* before stdio.h: zos_vfs.h declares SEEK_SET as an enum */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TILEFLAGS_DEFINE_CACHE(15);

/* ---- stubs for the parts that touch hardware */
gfx_context vctx;
game_t game;
entity_pool_t game_entities;
uint16_t input_now, input_pressed;
uint8_t *world;
spawn_t leg_spawns[MAX_SPAWNS];
camera_t cam;
Tilemap *_TILEMAP;
static Tilemap tilemap;
static uint8_t world_buf[WORLD_W * WORLD_H];
gfx_error gfx_tilemap_load(gfx_context *ctx, void *tiles, uint8_t size, uint8_t layer, uint8_t x, uint8_t y) { return GFX_SUCCESS; }
gfx_error gfx_tilemap_place(gfx_context *ctx, uint8_t tile, uint8_t layer, uint8_t x, uint8_t y) { return GFX_SUCCESS; }
static gfx_sprite sprite_pool[64];
static int sprite_used;
static int falls, stumbles, kicks, zips_ridden, chains, max_meter, slides;

gfx_sprite *sprites_register(gfx_sprite sprite) { sprite_pool[sprite_used] = sprite; return &sprite_pool[sprite_used++]; }
zos_err_t tilemap_register(Tilemap *t, uint8_t *tiles) { _TILEMAP = t; t->tiles = tiles; return ERR_SUCCESS; }
void tilemap_scroll(uint8_t layer, uint16_t x, uint16_t y) { (void)layer; (void)x; (void)y; }
void *mem_set(void *p, uint8_t v, uint16_t n) { return memset(p, v, n); }
void *mem_cpy(void *d, const void *s, uint16_t n) { return memmove(d, s, n); }
char rand8(void) { return (char)rand(); }
void score_add(uint16_t points) { game.score += points; }
void game_sfx_jump(void) {}
void game_sfx_chain(uint8_t level) { chains++; if (level > max_meter) max_meter = level; }
void game_sfx_climb(void) { kicks++; }
void game_sfx_slide(void) { slides++; }
void game_sfx_thunder(void) {}
void game_sfx_land(void) {}
void game_sfx_zip(void) { zips_ridden++; }
void game_sfx_pigeon(void) {}
void game_sfx_stamp(void) {}
void game_sfx_stumble(void) { stumbles++; }
void game_sfx_fall(void) {
    game_entity_t *h = hero_entity();
    falls++;
    if (getenv("FALLS")) printf("    out at leg column %d, row %d (%s)\n", course_base + body_px(&h->entity.physics.body) / 16, body_py(&h->entity.physics.body) / 16,
                                body_py(&h->entity.physics.body) > WORLD_H * 16 ? "fell" : "storm");
}
void game_sfx_beacon(void) {}


static const uint8_t TILE_FLAGS[256] = { TILE_FLAGS_INIT };
static game_entity_t pool[POOL_SIZE];
static uint16_t last;

static FILE *leg_open(uint8_t leg, const char *extension) {
    char path[512];
    FILE *f;
    snprintf(path, sizeof(path), "%s/leg%d.%s", ASSETS_DIR, leg, extension);
    f = fopen(path, "rb");
    if (!f) {
        printf("can't read %s\n", path);
        exit(1);
    }
    return f;
}

uint8_t course_page_read(uint8_t leg, uint8_t page, uint8_t *buf) {
    FILE *f = leg_open(leg, "ztm");
    size_t n;
    fseek(f, (long)page * WORLD_H * PAGE_W, SEEK_SET);
    n = fread(buf, 1, WORLD_H * PAGE_W, f);
    fclose(f);
    return n != WORLD_H * PAGE_W;
}

uint8_t course_spawns_read(uint8_t leg) {
    FILE *f = leg_open(leg, "spn");
    size_t n;
    n = fread(leg_spawns, sizeof(spawn_t), LEGS[leg].spawn_count, f);
    fclose(f);
    return n != LEGS[leg].spawn_count;
}

/* like main.c's run_start(): the leg from the checkpoint, the storm starts at once */
static void run_start(void) {
    uint8_t tx = hero_check_tx();
    entity_pool_clear(&game_entities);
    course_load(game.leg, tx);
    actors_reset();
    hero_spawn(((int16_t)tx - course_base) * 16, (int16_t)hero_check_ty() * 16 - 28);
    hero_go();
    course_go(1);
}

static void load_leg(uint8_t leg) {
    const leg_t *l = &LEGS[leg];
    game.leg = leg;
    hero_set_checkpoint(l->start_tx, l->start_ty);
    run_start();
}

int main(int argc, char **argv) {
    int leg, first = 0, count = LEG_COUNT, verbose = 0, i, total = 0, ok = 1;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) verbose = 1;
        else { first = atoi(argv[i]); count = 1; }
    }
    world = world_buf;
    tileflags_register(TILE_FLAGS);
    tileflags_set_outside(TF_NONE);
    sprite_entities_init(&game_entities, pool, POOL_SIZE, sizeof(*pool));
    for (leg = first; leg < first + count; leg++) {
        int frame;
        falls = stumbles = kicks = zips_ridden = chains = max_meter = slides = 0;
        game.meter = 0;
        load_leg(leg);
        for (frame = 0; frame < 60 * 180 && falls < 20; frame++) {
            game_entity_t *h = hero_entity();
            input_now = autopilot_input();
            input_pressed = input_now & ~last;
            last = input_now;
            hero_update();
            actors_update();
            course_frame();
            tilemap_stream_update(&vctx, &stream, cam.x, cam.y);
            if (verbose && h && frame % 10 == 0)
                printf("  f%4d col%4d x%5d y%4d vx%4d vy%4d st%02x meter %d screen %d\n", frame,
                       course_base + body_px(&h->entity.physics.body) / 16, body_px(&h->entity.physics.body), body_py(&h->entity.physics.body),
                       h->entity.physics.body.vx, h->entity.physics.body.vy, h->entity.physics.body.state, game.meter, body_px(&h->entity.physics.body) - cam.x);
            if (hero_done()) break;
            if (hero_out()) run_start();
        }
        printf("leg %2d %-15s %s in %5.1f s  outs %d  stumbles %d  climbs %d  slides %d  zips %d  chains %d  max meter %d  score %u\n",
               leg + 1, LEGS[leg].name, hero_done() ? "done" : "FAILED", frame / 60.0, falls, stumbles, kicks, slides,
               zips_ridden, chains, max_meter, game.score);
        if (!hero_done() || falls) ok = 0;
        total += frame;
    }
    printf("total %.1f s, %s\n", total / 60.0, ok ? "every leg clean" : "NOT every leg clean");
    return ok ? 0 : 1;
}
