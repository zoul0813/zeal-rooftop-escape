/*
 * The course: a leg is up to 6 pages of PAGE_W columns in rooftop.zar (legN.ztm, authored in assets/legs/).
 * The world buffer holds WORLD_PAGES of them. The storm scrolls the camera right at the leg's speed;
 * once the camera is a page into the buffer, the next page is read (one frame) and the buffer is
 * rebased by a page (the next frame, stream_shift(): world, camera and entities move together), &game_entities,
 * so a leg can be longer than the 2047 px a 12.4 position reaches.
 */
#include <core.h>
#include "game.h"

uint8_t course_base;
uint8_t course_speed;
tilemap_stream_t stream;
static Tilemap tilemap;
static const leg_t *leg;
static uint8_t next_page;      // leg page that goes into the buffer's last page next
static uint8_t page_step;      // rebase in steps, one per frame: PS_*
static uint8_t scrolling;
static uint8_t frac;           // camera sub-pixel, 1/16 px
static uint8_t target_speed;
static uint8_t speed_spawn;    // next SP_SPEED entry to look at
static uint8_t page_buf[WORLD_H * PAGE_W];

#define PS_IDLE    0
#define PS_READ    1   // page_buf holds next_page
#define PS_PLACE   2   // rebased: page_buf goes into the last page next frame

/* copy page_buf into buffer page `slot` (row-major pages, row by row into the world rows) */
static void place_page(uint8_t slot) {
    uint8_t y;
    uint8_t *dst = world + (uint16_t)slot * PAGE_W, *src = page_buf;
    for (y = 0; y < WORLD_H; y++) {
        mem_cpy(dst, src, PAGE_W);
        dst += WORLD_W;
        src += PAGE_W;
    }
}

static void read_page(uint8_t page) {
    if (page >= leg->pages || course_page_read(game.leg, page, page_buf)) mem_set(page_buf, TILE_EMPTY, sizeof(page_buf));
}

static void set_limits(void) {
    // the camera stops at the end of the leg
    uint8_t cols = leg->pages - course_base / PAGE_W >= WORLD_PAGES ? WORLD_W : (uint8_t)((leg->pages * PAGE_W) - course_base);
    cam.max_x = (int16_t)cols * 16 - SCREEN_W;
}

void course_load(uint8_t l, uint8_t from_tx) {
    uint8_t i;
    int16_t cx;
    leg = &LEGS[l];
    if (course_spawns_read(l)) mem_set(leg_spawns, 0, sizeof(leg_spawns));
    // the page holding the camera's left edge (72 px left of from_tx) becomes buffer page 0
    i = from_tx > 5 ? (uint8_t)((from_tx - 5) / PAGE_W) : 0;
    course_base = i * PAGE_W;
    for (next_page = i; next_page < i + WORLD_PAGES; next_page++) {
        read_page(next_page);
        place_page(next_page - i);
    }
    page_step = PS_IDLE;
    camera_init_view(&cam, WORLD_W * 16, WORLD_H * 16, SCREEN_W, SCREEN_H);
    set_limits();
    cx = (int16_t)(from_tx - course_base) * 16 - 72;
    camera_set(&cam, cx, 0);
    frac = 0;
    scrolling = 0;
    // the storm's speed at from_tx: the leg's, then every speed mark before it
    course_speed = leg->speed;
    for (speed_spawn = 0; speed_spawn < leg->spawn_count; speed_spawn++) {
        const spawn_t *s = &leg_spawns[speed_spawn];
        if (s->tx > from_tx) break;
        if (s->type == SP_SPEED) course_speed = s->arg;
    }
    target_speed = course_speed;
    stream_init(&vctx, &stream, &tilemap, world, WORLD_W, WORLD_H, PLAY_LAYER, &cam);
    zips_load();
}

void course_go(uint8_t on) {
    scrolling = on;
}

void course_frame(void) {
    uint8_t cam_tx;
    if (!scrolling) return;
    // speed marks: ease toward the new speed a step every other frame
    cam_tx = course_base + (uint8_t)((cam.x + SCREEN_W / 2) >> 4);
    while (speed_spawn < leg->spawn_count) {
        const spawn_t *s = &leg_spawns[speed_spawn];
        if (s->tx > cam_tx) break;
        if (s->type == SP_SPEED) target_speed = s->arg;
        speed_spawn++;
    }
    if (course_speed != target_speed && (frac & 1)) course_speed += course_speed < target_speed ? 1 : -1;
    frac += course_speed;
    cam.x += frac >> 4;
    frac &= 15;
    if (cam.x > cam.max_x) cam.x = cam.max_x;

    // past the first page: read the next one, rebase by a page, then fill the last page, each in a
    // frame of its own (the last page is 60 columns right of the view, out of sight for a while)
    switch (page_step) {
        case PS_IDLE:
            if (cam.x >= PAGE_W * 16 - 64 && next_page < leg->pages) {
                read_page(next_page);
                page_step = PS_READ;
            }
            break;
        case PS_READ:
            // on a frame where the camera didn't cross a tile (the stream writes no column)
            if (cam.x >= PAGE_W * 16 && (cam.x & 15) >= 4) {
                stream_shift(&stream, &cam, &game_entities, PAGE_W, 0);
                course_base += PAGE_W;
                page_step = PS_PLACE;
            }
            break;
        case PS_PLACE:
            place_page(WORLD_PAGES - 1);
            next_page++;
            set_limits();
            page_step = PS_IDLE;
            break;
    }
}
