/*
 * AUTOPILOT builds only (-DAUTOPILOT=ON): a bot that plays the real game, used to record
 * gameplay videos and measure the frame rate in the emulator, and to prove the legs in the host
 * sim (tests/run_sim.sh). It returns the buttons a player would hold this frame; everything else
 * (physics, rules, sound) is the normal game.
 *
 * It never pushes (no Right or Left) unless built with AUTOPILOT_PUSH, so a clean run shows a leg
 * can be done at the storm's speed. On the ground it looks ahead one tile column at a time:
 * it jumps at the last moment before an edge, an obstacle or glass, jumps early at a tall wall and
 * climbs it, slides under billboards and pipes, and hops again right after landing when the hop
 * lands on the same roof (that builds the momentum meter).
 */
#include "game.h"

#define HERO_W 10
#define HERO_H 28

#ifdef AUTOPILOT_PUSH
#define BASE IN_RIGHT
#else
#define BASE 0
#endif

static uint8_t hold;        // frames left holding jump
static uint8_t release;     // frames to keep jump released (so the next press is a new press)
static uint8_t ground;      // frames on the ground
static uint8_t was_air;
static uint8_t held;        // jump was down last frame
static uint8_t climbed;     // pressed for the climb in this jump

/* ground within 3 tiles below the feet in tile column tx: a step down onto a lower roof (or off an
 * AC unit) isn't an edge. Whole tiles, so the look-ahead costs a few lookups per column. */
static uint8_t solid_below(uint8_t tx, uint8_t ty) {
    return (tileflags_at_tile(tx, ty) | tileflags_at_tile(tx, ty + 1) | tileflags_at_tile(tx, ty + 2) |
            tileflags_at_tile(tx, ty + 3)) & (TF_SOLID | TF_ONEWAY);
}

/* start a jump held for `frames`; a jump only starts on a new press, so let go for a frame first */
static uint16_t press(uint8_t frames) {
    if (held) return BASE;
    hold = frames;
    return BASE | IN_JUMP;
}

static uint16_t decide(void) {
    game_entity_t *hero = hero_entity();
    body_t *b;
    int16_t x, feet, front, speed, reach, d;
    uint8_t edge = 0, wall = 0, tall = 0, glass = 0, over = 0, overhead = 0;
    uint8_t sliding;

    if (hero == NULL || hero->entity.physics.visual.hidden) return 0;
    b = &hero->entity.physics.body;
    x = PHYSICS_FP_INT(b->x);
    feet = PHYSICS_FP_INT(b->y) + b->h - 1;
    front = x + HERO_W;
    speed = b->vx > 0 ? PHYSICS_FP_INT(b->vx + 8) : 0;
    if (speed < 2) speed = 2;
    sliding = b->h < HERO_H;

    if (hold) {
        hold--;
        return BASE | IN_JUMP;
    }

    if (!(b->state & BODY_ON_GROUND)) {
        ground = 0;
        was_air = 1;
        if (release) {
            release--;
            return BASE;
        }
        // against a wall in the air: climb it
        if (!climbed && ((b->state & BODY_HIT_WALL) || (tileflags_at(front + 2, feet - 8) & TF_SOLID))) {
            climbed = 1;
            release = 3;
            return press(14);
        }
        return BASE;
    }
    climbed = 0;

    if (ground < 255) ground++;
    // look ahead along the roof, one tile column at a time
    reach = front + speed * 34 + 8;         // about one full hop
    {
        uint8_t tx = (uint8_t)((front + 2) >> 4);
        uint8_t below = (uint8_t)((feet + 1) >> 4);
        uint8_t body = (uint8_t)((feet - 4) >> 4);
        uint8_t head = (uint8_t)((feet - 20) >> 4);
        // under something already (sliding): keep low
        if (sliding && (tileflags_at_tile((uint8_t)(x >> 4), head) | tileflags_at_tile((uint8_t)((x + HERO_W - 1) >> 4), head)) & TF_SOLID)
            over = 1;
        for (d = 2; front + d < reach; tx++, d = (int16_t)tx * 16 - front) {
            uint8_t at_body = tileflags_at_tile(tx, body), at_head = tileflags_at_tile(tx, head);
            if (!edge && !solid_below(tx, below)) edge = (uint8_t)(d > 255 ? 255 : d);
            if (d < 40 && (at_body & TF_TRIGGER)) glass = 1;
            if (at_body & TF_SOLID) {
                // a wall: low obstacles are jumped at the last moment, tall walls early (to climb them)
                uint8_t h = (tileflags_at_tile(tx, body - 2) & TF_SOLID) != 0;
                if (h && d <= speed * 10 + 8) tall = 1;
                if (d < 24 + speed * 2) wall = 1;
                break;
            }
            if (at_head & TF_SOLID) {
                overhead = 1;
                if (d <= speed * 3 + 8) over = 1;
            }
            if (edge && d >= 40) break;
        }
    }

    if (over) return BASE | IN_SLIDE;   // slide (or keep sliding) under it
    // jump at the last moment: the jump button was released last frame (hold ran out), so a
    // press now is a new press even inside the release window
    if (tall || wall || glass || (edge && edge <= speed * 2 + 4)) {
        release = 2;
        return press(18);
    }
    if (release) {
        release--;
        return BASE;
    }
    // chained hop: only when it lands on this roof
    if (was_air && ground <= 2 && !edge && !sliding && !overhead) {
        was_air = 0;
        release = 2;
        return press(10);
    }
    if (ground > 2) was_air = 0;
    return BASE;
}

uint16_t autopilot_input(void) {
    uint16_t in = decide();
    held = (in & IN_JUMP) != 0;
    return in;
}
