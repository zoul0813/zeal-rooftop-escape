#include <stdint.h>
#include <zos_errors.h>
#include <zvb_gfx.h>
/* only the ZGDK headers used: <zgdk.h> pulls rect.h/ascii.h/music.h, whose non-inline static
 * functions get compiled into every .c that includes them */
#include "zgdk/input/button_map.h"
#include "zgdk/sprites.h"
#include "zgdk/tilemap.h"
#include "zgdk/utils/rand.h"
#include "zgdk/sound/sounds.h"
#include "zgdk/sound/tracker.h"
#include <zgdk/physics/body.h>
#include <zgdk/tilemap/flags.h>
#include <zgdk/sound/sfx.h>
#include <zgdk/input.h>
#include <zgdk/camera.h>
#include "stream.h"
#include "gfx.h"
#include "entities.h"

#ifndef GAME_H
#define GAME_H

#define SCREEN_W       320
#define SCREEN_H       240

#define SKY_LAYER      0   // skyline, half-speed parallax, loops every 80 tiles
#define PLAY_LAYER     1   // rooftops, streamed from the world buffer

/* A leg is up to 6 pages of 40 columns in rooftop.zar; the world buffer holds 3 pages and is rebased
 * (stream_shift) a page at a time as the camera scrolls, so legs can be longer than 2047 px. */
#define PAGE_W         40
#define WORLD_PAGES    3
#define WORLD_W        (PAGE_W * WORLD_PAGES)
#define WORLD_H        15
#define LEG_COUNT      10
#define MAX_ZIPS       4   // whole-leg rope cache; checked by tools/pack_maps.py

#define POOL_SIZE      20  // pigeons, stamps, beacons, mate, dust + the hero
#define HUD_SPRITES    12
#define STORM_ROWS     15
#define STORM_SPRITES  (STORM_ROWS * 2)   // the storm front at the left edge: cloud + ragged edge
#define TEXT_LINES     3
#define TEXT_LEN       20
#define ARENA_SIZE     (POOL_SIZE + HUD_SPRITES + STORM_SPRITES + TEXT_LINES * TEXT_LEN)

/* entity types */
#define E_HERO    0
#define E_PIGEON  1
#define E_STAMP   2
#define E_BEACON  3
#define E_MATE    4
#define E_DUST    5

/* spawn types in the level data (tools/pack_maps.py) */
#define SP_PIGEON 1
#define SP_STAMP  2
#define SP_BEACON 3
#define SP_MATE   4
#define SP_ZIP    5
#define SP_SPEED  6   // from this column on the storm scrolls at arg (12.4 px/frame)

/* input */
#define IN_LEFT   BUTTON_LEFT
#define IN_RIGHT  BUTTON_RIGHT
#define IN_JUMP   (BUTTON_B | BUTTON_UP)   // Z / Space / Up, SNES B
#define IN_SLIDE  (BUTTON_DOWN | BUTTON_A) // Down / X, SNES A
#define IN_START  BUTTON_START             // Enter, SNES Start
#define IN_QUIT   BUTTON_SELECT            // Right Shift, SNES Select

#define METER_MAX     4
#define LIVES_START   3
#define LIVES_MAX     9
#define STORM_REACH   14     // the storm catches a runner whose back is this close to the screen's left edge
#define AHEAD_MAX     200    // screen x past which the runner can't pull further ahead of the storm

typedef struct {
    uint8_t type, tx, ty, arg;
} spawn_t;

typedef struct {
    uint8_t spawn_count;      // separate legN.spn records in rooftop.zar, read into leg_spawns
    uint8_t pages;            // PAGE_W columns each
    uint8_t start_tx, start_ty;
    uint8_t speed;            // storm speed at the start, 12.4 px/frame
    uint8_t sky;              // sky palette
    const char *name;
} leg_t;

typedef struct {
    uint16_t score;
    uint8_t leg;          // 0 based
    uint8_t lives;
    uint8_t meter;        // momentum 0..METER_MAX
    uint8_t stamps;
    uint8_t state;        // GS_*
    uint16_t timer;
} game_t;

#define GS_PLAY     0
#define GS_SAFE     1     // reached the hatch
#define GS_CAUGHT   2     // fell or the storm caught up, waiting to restart at the beacon
#define GS_OVER     3
#define GS_WIN      4

extern gfx_context vctx;
extern game_t game;
extern camera_t cam;
extern uint16_t input_now;
extern uint16_t input_pressed;
extern uint8_t *world;               // WORLD_W * WORLD_H tiles, inside asset_work (assets.h)

/* map_data.c (generated in the build directory) */
#define MAX_SPAWNS 64                // the most spawns in a leg (tools/pack_maps.py checks it)
extern const leg_t LEGS[LEG_COUNT];
extern spawn_t leg_spawns[MAX_SPAWNS];   // the current leg's, read by course_load()

/* course.c: the leg's pages in the world buffer, the storm's auto-scroll */
extern uint8_t course_base;       // leg column at world column 0
extern uint8_t course_speed;      // storm scroll speed now, 12.4 px/frame
extern tilemap_stream_t stream;
void course_load(uint8_t leg, uint8_t from_tx);   // world + camera with leg column from_tx in view
void course_go(uint8_t on);       // start / stop the storm's scrolling
void course_frame(void);          // scroll, read and rebase pages; before tilemap_stream_update()
uint8_t course_page_read(uint8_t leg, uint8_t page, uint8_t *buf);   // world.c / the host sim: 0 = ok
uint8_t course_spawns_read(uint8_t leg);                             // world.c / the host sim: 0 = ok

/* world.c */
zos_err_t world_init(void);
void world_frame(void);           // camera + streaming, before the vblank
void world_scroll(void);          // hardware scroll registers, inside the vblank
void world_set_sky(uint8_t step);
void world_flash(void);           // lightning: the sky lights up for a few frames

/* zips.c */
void zips_load(void);             // cache all clotheslines once per leg load
uint8_t zip_count(void);
int16_t zip_line_y(uint8_t zip, int16_t px);   // buffer-space px, stable rope ID; height or -1 off the line

/* hero.c */
void hero_spawn(int16_t px, int16_t py);
void hero_update(void);
void hero_go(void);                // the countdown is over: run
void hero_set_checkpoint(uint8_t tx, uint8_t ty);   // leg tiles: the roof under a beacon
uint8_t hero_check_tx(void);
uint8_t hero_check_ty(void);
game_entity_t *hero_entity(void);
uint8_t hero_done(void);           // reached the hatch
uint8_t hero_out(void);            // fell or caught by the storm
void hero_celebrate(void);

/* actors.c */
void actors_reset(void);
void actors_update(void);

/* storm.c: the storm front sprites at the left edge, lightning */
void storm_init(void);
void storm_show(uint8_t on);
void storm_update(void);

/* hud.c */
zos_err_t hud_init(void);
void hud_draw(void);
void hud_show(uint8_t on);
void text_show(uint8_t line, const char *str, uint8_t y);
void text_hide(uint8_t line);
void text_hide_all(void);
void score_add(uint16_t points);

/* audio: the music's pattern buffers sit at the start of asset_work (assets.h) */
#define MUSIC_PATTERNS 4
#define MUSIC_RAM      (MUSIC_PATTERNS * 772)   // sizeof(pattern_t)
#define BGM_NONE     0xFF
#define BGM_TITLE    0
#define BGM_RUN      1
#define BGM_SAFE     2
#define BGM_CAUGHT   3
#define BGM_WIN      4
zos_err_t bgm_init(void);
void bgm_play(uint8_t song);
void bgm_stop(void);
void bgm_tick(void);
uint8_t bgm_busy(void);
void bgm_pause(void);
void bgm_resume(void);
void game_sfx_init(void);
void game_sfx_jump(void);
void game_sfx_chain(uint8_t level);
void game_sfx_climb(void);
void game_sfx_land(void);
void game_sfx_zip(void);
void game_sfx_pigeon(void);
void game_sfx_stamp(void);
void game_sfx_stumble(void);
void game_sfx_fall(void);
void game_sfx_beacon(void);
void game_sfx_slide(void);
void game_sfx_thunder(void);

#ifdef AUTOPILOT
uint16_t autopilot_input(void);
#endif

#endif
