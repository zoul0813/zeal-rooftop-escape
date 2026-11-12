/*
 * Rooftop Escape (rooftop.bin): a free runner outruns a storm front across the city's rooftops.
 * Auto-scrolling side-scroller for Zeal 8-bit OS, built on ZGDK.
 */
#include <zos_vfs.h>
#include <stdint.h>
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_video.h>
#include "game.h"
#include "controls.h"
#include "entities.h"
#include "assets.h"
#include "zgdk/tilemap/scroll.h"
#include <zgdk/sprites.h>
#include "sky.h"
#include "sprite_groups.h"

#ifndef START_LEG
#define START_LEG 0
#endif
#define BANNER_FRAMES   150
#define READY_FRAMES    50     // the runner waits, the storm gathers
#define SAFE_FRAMES     200
#define CAUGHT_FRAMES   110
#define END_FRAMES      320

gfx_context vctx;
game_t game;
entity_pool_t game_entities;
uint16_t input_now;
uint16_t input_pressed;
static uint16_t input_last;
static game_entity_t pool[POOL_SIZE];
static gfx_sprite arena[ARENA_SIZE];
#ifdef AUTOPILOT
static uint8_t autopilot_on;
#endif

static void fatal(zos_err_t err, const char *what) {
    // printf would pull in ~2.6 KB of formatting code
    uint16_t len = 0;
    char code[4] = { ' ', '0' + (err / 10) % 10, '0' + err % 10, '\n' };
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
    while (what[len]) len++;
    write(DEV_STDOUT, what, &len);
    len = sizeof(code);
    write(DEV_STDOUT, code, &len);
    exit(err);
}

static void read_input(void) {
    input_now = input_read();
#ifdef AUTOPILOT
    if (autopilot_on) input_now = (input_now & (IN_START | IN_QUIT)) | autopilot_input();
#endif
    input_pressed = input_now & ~input_last;
    input_last = input_now;
}

static void frame_end(void) {
    storm_update();
    world_frame();
    physics_entities_sync(&game_entities);
    hud_draw();
    rooftop_sprites_prepare();
    gfx_wait_vblank(&vctx);
    world_scroll();
    (void)rooftop_sprites_upload(&vctx);   // retry dirty groups next frame on error
    gfx_wait_end_vblank(&vctx);
    bgm_tick();
    sfx_tick();
}

static void init(void) {
    zos_err_t err;

    err = input_init(1);
    if (err) fatal(err, "input");

    gfx_enable_screen(0);
    err = gfx_initialize(ZVB_CTRL_VID_MODE_GFX_320_8BIT, &vctx);
    if (err) fatal(err, "gfx");
    err = sprites_clear(&vctx);   // a previous program may have left sprites in the table
    if (err) fatal(err, "sprites");
    err = zar_file_open(PACK_FILE, &pack);   // rooftop.zar, next to the .bin
    if (err) fatal(err, PACK_FILE);
    err = load_palette(&vctx);
    if (err) fatal(err, "palette");
    err = load_tileset(&vctx);
    if (err) fatal(err, "tileset");
    tilemap_fill(&vctx, PLAY_LAYER, TILE_EMPTY, 0, 0, 80, 40);

    err = rooftop_sprites_init(arena);
    if (err) fatal(err, "sprites");
    err = hud_init();   // first: lower sprite slots draw on top, the HUD and banners stay over the actors
    if (err) fatal(err, "hud");
    storm_init();
    err = sprite_entities_init(&game_entities, pool, POOL_SIZE, sizeof(*pool));
    if (err) fatal(err, "sprites");
    err = world_init();
    if (err) fatal(err, "world");

    sound_init_hardware();       // resets every voice, so before the tracker and the effects
    err = bgm_init();   // after load_tileset(): the patterns reuse its load buffer
    if (err) fatal(err, "music");
    game_sfx_init();
    rand8_seed(0x7E1A);

    gfx_enable_screen(1);
}

static void deinit(void) {
    bgm_stop();
    sound_deinit();
    zar_file_close(&pack);
    tilemap_scroll(SKY_LAYER, 0, 0);
    tilemap_scroll(PLAY_LAYER, 0, 0);
    (void)sprites_clear(&vctx);   // best-effort cleanup before returning to ZOS
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}

static void wait_frames(uint16_t frames) {
    while (frames--) {
        read_input();
        actors_update();
        frame_end();
    }
}

/* Title over leg 1, the camera drifting along the rooftops. Returns 0 to quit. */
static uint8_t title(void) {
    uint16_t seed = 0;
    int8_t pan = 1;

    entity_pool_clear(&game_entities);
    game.leg = 0;
    game.state = GS_OVER;
    hero_set_checkpoint(0, 0);
    course_load(0, 0);
    actors_reset();
    world_set_sky(LEGS[0].sky);
    hud_show(0);
    storm_show(0);
    input_flush();
    controls_menu_begin();
    text_hide_all();
    text_show(0, "ROOFTOP ESCAPE", 40);
    text_show(1, "OUTRUN THE STORM", 64);
    text_show(2, PROMPT_START, 124);
    input_last = 0;
    bgm_play(BGM_TITLE);

    while (1) {
        read_input();
        if (controls_menu_update())
            text_show(2, (seed & 256) ? CONTROL_PROMPT("Z JUMP  DOWN SLIDE", PAD_B " JUMP  " PAD_DOWN " SLIDE") : PROMPT_START, 124);
        seed++;
        if (input_pressed & IN_START) break;
        if (input_pressed & IN_QUIT) return 0;
#ifdef AUTOPILOT
        if (seed == 120) break;
#endif
        // the controls, then back to the pitch
        if ((seed & 255) == 0) text_show(2, (seed & 256) ? CONTROL_PROMPT("Z JUMP  DOWN SLIDE", PAD_B " JUMP  " PAD_DOWN " SLIDE") : PROMPT_START, 124);
        // drift right and back so the parallax shows off
        cam.x += pan;
        if (cam.x >= cam.max_x) pan = -1;
        else if (cam.x <= 0) pan = 1;
        actors_update();
        frame_end();
    }
    text_hide_all();
    rand8_seed(seed | 1);
    return 1;
}

/* (re)start the run at the checkpoint: the leg's start, or the last beacon */
static void run_start(uint8_t banner) {
    const leg_t *l = &LEGS[game.leg];
    uint8_t tx = hero_check_tx();
    char head[] = "LEG 00";

    entity_pool_clear(&game_entities);
    course_load(game.leg, tx);
    actors_reset();
    world_set_sky(l->sky);
    hero_spawn(((int16_t)tx - course_base) * 16, (int16_t)hero_check_ty() * 16 - 28);
    text_hide_all();
    if (banner) {
        if (game.leg >= 9) {
            head[4] = '0' + (game.leg + 1) / 10;
            head[5] = '0' + (game.leg + 1) % 10;
        } else {
            head[4] = '1' + game.leg;
            head[5] = 0;
        }
        text_show(0, head, 64);
        text_show(1, l->name, 84);
    }
    text_show(2, "READY", 124);
    game.state = GS_PLAY;
    game.timer = READY_FRAMES;
    hud_show(1);
    storm_show(1);
    bgm_play(BGM_RUN);
}

static void leg_start(void) {
    const leg_t *l = &LEGS[game.leg];
    hero_set_checkpoint(l->start_tx, l->start_ty);
    run_start(1);
}

static void pause(void) {
    text_show(2, "PAUSED", 124);
    bgm_pause();
    input_flush();
    input_last = 0;
    do {
        read_input();
        frame_end();
    } while (!(input_pressed & IN_START));
    text_hide(2);
    bgm_resume();
}

static void play(void) {
    uint16_t banner = 0;
    game.score = 0;
    game.leg = START_LEG;
    game.lives = LIVES_START;
    game.stamps = 0;
#ifdef AUTOPILOT
    autopilot_on = 1;
#endif
    leg_start();

    while (1) {
        read_input();
        if (input_pressed & IN_START) pause();
        if (input_pressed & IN_QUIT) break;

        hero_update();
        actors_update();

        switch (game.state) {
            case GS_PLAY:
                if (game.timer && --game.timer == 0) {
                    // go: the storm starts moving
                    hero_go();
                    course_go(1);
                    text_show(2, "RUN!", 124);
                    banner = BANNER_FRAMES;
                }
                if (banner) {
                    if (--banner == BANNER_FRAMES - 40) text_hide(2);
                    if (banner == 0) {
                        text_hide(0);
                        text_hide(1);
                    }
                }
                if (hero_done()) {
                    course_go(0);
                    game.state = GS_SAFE;
                    game.timer = SAFE_FRAMES;
                    score_add(250 + (uint16_t)game.lives * 50);
                    text_show(0, game.leg == LEG_COUNT - 1 ? "YOU MADE IT!" : "SAFE!", 64);
                    text_hide(1);
                    text_hide(2);
                    banner = 0;
                    bgm_play(game.leg == LEG_COUNT - 1 ? BGM_WIN : BGM_SAFE);
                } else if (hero_out()) {
                    game_entity_t *h = hero_entity();
                    course_go(0);
                    game.state = GS_CAUGHT;
                    game.timer = CAUGHT_FRAMES;
                    if (game.lives) game.lives--;
                    banner = 0;
                    text_hide_all();
                    text_show(0, PHYSICS_FP_INT(h->entity.physics.body.y) > WORLD_H * 16 ? "YOU FELL" : "THE STORM GOT YOU", 64);
                    if (game.lives == 0) text_show(1, "GAME OVER", 84);
                    bgm_play(BGM_CAUGHT);
                }
                break;
            case GS_CAUGHT:
                if (--game.timer == 0) {
                    if (game.lives == 0) goto done;
                    run_start(0);
                }
                break;
            case GS_SAFE:
                if (--game.timer == 0) {
                    if (++game.leg >= LEG_COUNT) {
                        game.state = GS_WIN;
                        game.timer = END_FRAMES;
                        storm_show(0);
                        text_show(0, "THE STORM PASSES", 64);
                        text_show(1, "SUNRISE", 84);
                        world_set_sky(SKY_STEPS - 1);
                    } else {
                        if (game.lives < LIVES_MAX) game.lives++;
                        leg_start();
                    }
                }
                break;
            case GS_WIN:
                if (--game.timer == 0) goto done;
                break;
        }
        frame_end();
    }
done:
    entity_pool_clear(&game_entities);
    text_hide_all();
    hud_show(0);
    storm_show(0);
    game.state = GS_OVER;
    bgm_stop();
#ifdef AUTOPILOT
    autopilot_on = 0;
#endif
    frame_end();
    input_flush();
}

int main(void) {
    init();
    while (title()) play();
    deinit();
    return 0;
}
