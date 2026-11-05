#include <zgdk/physics/platformer.h>
/*
 * The runner: SDK platformer physics (zgdk/physics/platformer.h) plus the Rooftop Escape moves.
 *
 * The runner never stops: they always run right, at the storm's speed. Right pushes ahead (up to
 * AHEAD_MAX on screen), Left eases off toward the storm. Jump height depends on how long jump is
 * held. Slide (Down) ducks under billboards and pipes. Jumping into a wall and pressing jump climbs
 * it. Momentum: a hop within a few frames of landing, a wall climb, a slide under something or a
 * jump off a clothesline raises the meter (0..4); each step adds a little speed and jump height.
 */
#include "game.h"

#define HERO_W        10
#define HERO_H        28
#define SLIDE_H       14
#define SLIDE_MIN     16    // frames a slide lasts at least
#define SLIDE_MAX     44    // and at most, unless something is overhead
#define HOP_WINDOW    8     // frames after landing that still count as a chained jump
#define DECAY_FRAMES  40    // grounded frames per meter step lost
#define WALL_GRACE    6     // frames a climb is still allowed after touching the wall
#define CLIMB_POSE    14
#define PUSH          12    // 12.4 px/frame added (Right) or taken (Left) from the storm's speed
#define ZIP_MAX       PHYSICS_FP(5)
#define ZIP_COOL      20
#define STUMBLE_TIME  24
#define FALL_LINE     (WORLD_H * 16 + 16)

#define M_READY    0
#define M_RUN      1
#define M_SLIDE    2
#define M_ZIP      3
#define M_STUMBLE  4
#define M_OUT      5
#define M_DONE     6

static const int16_t JUMP_VEL[METER_MAX + 1] = { -100, -101, -102, -104, -106 };

static platformer_config_t phys = {
    .gravity = 6,
    .max_fall = 100,
    .jump_vel = -100,
    .jump_cut = -40,
    .accel = 3,
    .decel = 5,
    .max_run = 36,
    .coyote = 5,
    .buffer = 6,
    .air_jumps = 0,
};

static const uint8_t F_STAND[] = { SPR_HERO_STAND };
static const uint8_t F_RUN[] = { SPR_HERO_RUN1, SPR_HERO_RUN2, SPR_HERO_RUN3, SPR_HERO_RUN4 };
static const uint8_t F_JUMP[] = { SPR_HERO_JUMP };
static const uint8_t F_FALL[] = { SPR_HERO_FALL };
static const uint8_t F_WALL[] = { SPR_HERO_WALL };
static const uint8_t F_ZIP[] = { SPR_HERO_ZIP };
static const uint8_t F_STUMBLE[] = { SPR_HERO_STUMBLE };
static const uint8_t F_SLIDE[] = { SPR_HERO_SLIDE };
static const uint8_t F_DUST[] = { SPR_DUST1, SPR_DUST2 };
static const anim_def A_STAND = { F_STAND, 1, 1, ANIM_LOOP };
static const anim_def A_RUN = { F_RUN, 4, 5, ANIM_LOOP };
static const anim_def A_RUN_FAST = { F_RUN, 4, 3, ANIM_LOOP };
static const anim_def A_JUMP = { F_JUMP, 1, 1, ANIM_LOOP };
static const anim_def A_FALL = { F_FALL, 1, 1, ANIM_LOOP };
static const anim_def A_WALL = { F_WALL, 1, 1, ANIM_LOOP };
static const anim_def A_ZIP = { F_ZIP, 1, 1, ANIM_LOOP };
static const anim_def A_STUMBLE = { F_STUMBLE, 1, 1, ANIM_LOOP };
static const anim_def A_SLIDE = { F_SLIDE, 1, 1, ANIM_LOOP };
static const anim_def A_DUST = { F_DUST, 2, 5, 0 };

static game_entity_t *hero;
static uint8_t mode;
static uint8_t timer;
static uint8_t ground_frames;
static uint8_t air_frames;
static uint8_t hop_window;
static uint8_t wall_timer;
static uint8_t climbed;      // one climb per jump
static uint8_t climb_pose;
static uint8_t ducked;       // something was overhead during this slide
static uint8_t slide_buf;    // frames a slide press is remembered before landing
static uint8_t zip;
static int16_t zip_speed;
static uint8_t zip_cool;
static uint8_t safe_timer;   // no stumble right after one
static uint8_t check_tx, check_ty;

game_entity_t *hero_entity(void) {
    return (hero && hero->entity.physics.visual.type == E_HERO) ? hero : NULL;
}

uint8_t hero_done(void) {
    return mode == M_DONE;
}

uint8_t hero_out(void) {
    return mode == M_OUT;
}

void hero_go(void) {
    // off at the storm's speed, so it doesn't gain on a standing start
    if (mode == M_READY) {
        mode = M_RUN;
        hero->entity.physics.body.vx = course_speed;
    }
}

void hero_celebrate(void) {
    // into the hatch
    mode = M_DONE;
    hero->entity.physics.body.vx = 0;
    hero->entity.physics.visual.hidden = 1;
}

void hero_set_checkpoint(uint8_t tx, uint8_t ty) {
    check_tx = tx;
    check_ty = ty;
}

uint8_t hero_check_tx(void) {
    return check_tx;
}

uint8_t hero_check_ty(void) {
    return check_ty;
}

static void dust(void) {
    game_entity_t *d = physics_entities_spawn(&game_entities, E_DUST, PHYSICS_FP_INT(hero->entity.physics.body.x) - 3, PHYSICS_FP_INT(hero->entity.physics.body.y) + hero->entity.physics.body.h - 14, 1, 1);
    if (d == NULL) return;
    animated_physics_entity_play(&d->entity, &A_DUST);
    d->timer = 10;
}

void hero_spawn(int16_t px, int16_t py) {
    if (hero_entity() == NULL) {
        hero = physics_entities_spawn(&game_entities, E_HERO, px, py, HERO_W, HERO_H);
        hero->entity.physics.visual.tall = 1;
        hero->entity.physics.visual.ox = -3;
    } else {
        platformer_set_pos(&hero->entity.physics.body, &hero->platformer, px, py);
    }
    hero->entity.physics.body.h = HERO_H;
    hero->entity.physics.visual.oy = -4;
    hero->entity.physics.body.vx = 0;
    hero->entity.physics.body.vy = 0;
    hero->entity.physics.visual.hidden = 0;
    hero->entity.physics.visual.flip = 0;
    mode = M_READY;
    ground_frames = 0;
    air_frames = 0;
    hop_window = 0;
    wall_timer = 0;
    climbed = 0;
    climb_pose = 0;
    zip_cool = 0;
    slide_buf = 0;
    safe_timer = 0;
    game.meter = 0;
    animated_physics_entity_play(&hero->entity, &A_STAND);
}

static void meter_up(void) {
    if (game.meter < METER_MAX) game.meter++;
    game_sfx_chain(game.meter);
    score_add(10 * game.meter);
}

static void set_speed(int8_t push) {
    int16_t vx = hero->entity.physics.body.vx;
    int16_t top = course_speed + game.meter * 2;
    if (push > 0) top += PUSH;
    else if (push < 0) top -= PUSH;
    if (PHYSICS_FP_INT(hero->entity.physics.body.x) - cam.x > AHEAD_MAX && top > course_speed) top = course_speed;
    if (mode == M_STUMBLE) top = course_speed - PUSH;
    // slowing down eases the speed instead of braking at once
    if (vx > top + 2) top = vx - 2;
    phys.max_run = top;
    phys.jump_vel = JUMP_VEL[game.meter];
}

static void go_out(void) {
    mode = M_OUT;
    hero->entity.physics.visual.hidden = 1;
    game.meter = 0;
    game_sfx_fall();
}

/* nothing solid where a standing runner's upper half would be */
static uint8_t headroom(void) {
    body_t *b = &hero->entity.physics.body;
    int16_t x = PHYSICS_FP_INT(b->x), top = PHYSICS_FP_INT(b->y) - (HERO_H - SLIDE_H);
    return !((tileflags_at(x, top) | tileflags_at(x + HERO_W - 1, top) | tileflags_at(x, top + 12) |
              tileflags_at(x + HERO_W - 1, top + 12)) & TF_SOLID);
}

static void slide_start(void) {
    body_t *b = &hero->entity.physics.body;
    mode = M_SLIDE;
    timer = 0;
    ducked = 0;
    b->y += PHYSICS_FP(HERO_H - SLIDE_H);
    b->h = SLIDE_H;
    hero->entity.physics.visual.oy = SLIDE_H - 32;
    animated_physics_entity_play(&hero->entity, &A_SLIDE);
    game_sfx_slide();
    dust();
}

static void slide_end(void) {
    body_t *b = &hero->entity.physics.body;
    mode = M_RUN;
    b->y -= PHYSICS_FP(HERO_H - SLIDE_H);
    b->h = HERO_H;
    hero->entity.physics.visual.oy = -4;
    if (ducked) meter_up();
}

static void update_zip(uint8_t jump_pressed) {
    body_t *b = &hero->entity.physics.body;
    int16_t cx, ly;

    if (zip_speed < ZIP_MAX) zip_speed += 1;
    if (PHYSICS_FP_INT(b->x) - cam.x > AHEAD_MAX + 40 && zip_speed > course_speed) zip_speed = course_speed;
    b->x += zip_speed;
    cx = PHYSICS_FP_INT(b->x) + HERO_W / 2;
    ly = zip_line_y(zip, cx);
    if (ly < 0 || jump_pressed) {
        mode = M_RUN;
        zip_cool = ZIP_COOL;
        b->vx = zip_speed;
        b->vy = 0;
        if (jump_pressed) {
            b->vy = phys.jump_vel;
            meter_up();
            game_sfx_jump();
        }
        return;
    }
    b->y = PHYSICS_FP(ly + 4);
    b->vx = zip_speed;
    b->vy = 0;
}

static void try_zip(void) {
    body_t *b = &hero->entity.physics.body;
    int16_t cx = PHYSICS_FP_INT(b->x) + HERO_W / 2;
    int16_t top = PHYSICS_FP_INT(b->y);
    uint8_t i;
    for (i = 0; i < zip_count(); i++) {
        int16_t ly = zip_line_y(i, cx);
        if (ly < 0) continue;
        if (top + 4 >= ly - 6 && top + 4 <= ly + 12) {
            mode = M_ZIP;
            zip = i;
            zip_speed = b->vx > (int16_t)course_speed ? b->vx : course_speed;
            if (zip_speed < PHYSICS_FP(2)) zip_speed = PHYSICS_FP(2);
            b->state &= ~BODY_ON_GROUND;
            animated_physics_entity_play(&hero->entity, &A_ZIP);
            game_sfx_zip();
            return;
        }
    }
}

void hero_update(void) {
    body_t *b = &hero->entity.physics.body;
    int8_t push = 0;
    uint8_t jump_held = (input_now & IN_JUMP) != 0;
    uint8_t jump_pressed = (input_pressed & IN_JUMP) != 0;
    uint8_t slide_held = (input_now & IN_SLIDE) != 0;
    uint8_t was_ground = b->state & BODY_ON_GROUND;
    uint8_t jumped;

    if (input_now & IN_RIGHT) push = 1;
    else if (input_now & IN_LEFT) push = -1;
    if (zip_cool) zip_cool--;
    if (safe_timer) safe_timer--;
    if (input_pressed & IN_SLIDE) slide_buf = 8;
    else if (slide_buf) slide_buf--;

    switch (mode) {
        case M_READY:
            platformer_move(b, &phys);
            hero->entity.physics.visual.sprite->tile = anim_tick(&hero->entity.anim);
            return;
        case M_DONE:
        case M_OUT:
            return;
        case M_ZIP:
            update_zip(jump_pressed);
            if (PHYSICS_FP_INT(b->x) < cam.x + STORM_REACH) go_out();
            return;
        case M_STUMBLE:
            jump_pressed = 0;
            jump_held = 0;
            if (--timer == 0) mode = M_RUN;
            break;
        case M_SLIDE:
            timer++;
            if (!headroom()) {
                ducked = 1;
            } else if (jump_pressed || (timer >= SLIDE_MIN && (!slide_held || timer >= SLIDE_MAX)) || !was_ground) {
                slide_end();
            }
            if (mode == M_SLIDE) jump_pressed = 0;
            break;
    }

    set_speed(push);
    jumped = platformer_update(b, &phys, &hero->platformer, DIRECTION_RIGHT, jump_held, jump_pressed);
    if (jumped) {
        if (hop_window) meter_up();
        else game_sfx_jump();
        hop_window = 0;
    }
    // climb: jump into a wall (or press jump against it) and run up it, once per jump
    if (!was_ground && wall_timer && hero->platformer.buffer && !climbed) {
        b->vy = JUMP_VEL[game.meter] - 24;
        hero->platformer.buffer = 0;
        climbed = 1;
        climb_pose = CLIMB_POSE;
        wall_timer = 0;
        meter_up();
        game_sfx_climb();
        dust();
    }
    // Down in the air drops faster
    if (!was_ground && slide_held && mode != M_SLIDE && b->vy < phys.max_fall) b->vy += 4;

    platformer_move(b, &phys);

    if (b->state & BODY_ON_GROUND) {
        if (!was_ground && air_frames > 8) {
            hop_window = HOP_WINDOW;
            game_sfx_land();
            if (air_frames > 24) dust();
        }
        air_frames = 0;
        wall_timer = 0;
        climbed = 0;
        climb_pose = 0;
        if (hop_window) hop_window--;
        if (++ground_frames >= DECAY_FRAMES) {
            ground_frames = 0;
            if (game.meter) game.meter--;
        }
        if (b->state & BODY_HIT_WALL) game.meter = 0;
        if (mode == M_RUN && slide_held && slide_buf) {
            slide_buf = 0;
            slide_start();
        }
    } else {
        ground_frames = 0;
        if (air_frames < 255) air_frames++;
        if (b->state & BODY_HIT_WALL) wall_timer = WALL_GRACE;
        else if (wall_timer) wall_timer--;
        if (climb_pose) climb_pose--;
        if (!zip_cool && b->vy > -32 && mode != M_SLIDE) try_zip();
        if (mode == M_ZIP) return;
    }

    if ((b->state & BODY_TOUCHING_TRIGGER) && !safe_timer && mode == M_RUN) {
        mode = M_STUMBLE;
        timer = STUMBLE_TIME;
        safe_timer = 90;
        b->vy = -40;
        game.meter = 0;
        game_sfx_stumble();
    }

    // the last roof ends at the edge of the world
    if (PHYSICS_FP_INT(b->x) > WORLD_W * 16 - HERO_W) b->x = PHYSICS_FP(WORLD_W * 16 - HERO_W);
    if (PHYSICS_FP_INT(b->y) > FALL_LINE || PHYSICS_FP_INT(b->x) < cam.x + STORM_REACH) {
        go_out();
        return;
    }

    // animation
    if (mode == M_STUMBLE) {
        animated_physics_entity_play(&hero->entity, &A_STUMBLE);
    } else if (mode == M_SLIDE) {
        animated_physics_entity_play(&hero->entity, &A_SLIDE);
    } else if (b->state & BODY_ON_GROUND) {
        animated_physics_entity_play(&hero->entity, b->vx < 48 ? &A_RUN : &A_RUN_FAST);
    } else if (climb_pose || (wall_timer && !climbed)) {
        animated_physics_entity_play(&hero->entity, &A_WALL);
    } else if (b->vy < 0) {
        animated_physics_entity_play(&hero->entity, &A_JUMP);
    } else {
        animated_physics_entity_play(&hero->entity, &A_FALL);
    }
    hero->entity.physics.visual.sprite->tile = anim_tick(&hero->entity.anim);
}
