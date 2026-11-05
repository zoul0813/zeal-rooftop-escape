/*
 * Everything on the rooftops besides the runner. The leg's spawn list is sorted by column, so
 * actors are spawned just before they scroll into view and freed once they are well behind.
 * Spawn columns are leg columns; the world buffer starts at leg column course_base.
 */
#include "game.h"

#define SPAWN_AHEAD   (320 + 32)
#define FREE_BEHIND   64
#define SCARE_X       56
#define SCARE_Y       64

static const uint8_t F_PIGEON_IDLE[] = { SPR_PIGEON_SIT, SPR_PIGEON_SIT, SPR_PIGEON_PECK, SPR_PIGEON_SIT };
static const uint8_t F_PIGEON_FLY[] = { SPR_PIGEON_FLY1, SPR_PIGEON_FLY2 };
static const uint8_t F_STAMP[] = { SPR_STAMP1, SPR_STAMP1, SPR_STAMP2 };
static const uint8_t F_BEACON_RED[] = { SPR_BEACON_RED };
static const uint8_t F_BEACON_GREEN[] = { SPR_BEACON_GREEN };
static const uint8_t F_MATE[] = { SPR_MATE_STAND, SPR_MATE_WAVE };
static const anim_def A_PIGEON_IDLE = { F_PIGEON_IDLE, 4, 20, ANIM_LOOP };
static const anim_def A_PIGEON_IDLE2 = { F_PIGEON_IDLE, 4, 27, ANIM_LOOP };
static const anim_def A_PIGEON_FLY = { F_PIGEON_FLY, 2, 5, ANIM_LOOP };
static const anim_def A_STAMP = { F_STAMP, 3, 10, ANIM_LOOP };
static const anim_def A_BEACON_RED = { F_BEACON_RED, 1, 1, ANIM_LOOP };
static const anim_def A_BEACON_GREEN = { F_BEACON_GREEN, 1, 1, ANIM_LOOP };
static const anim_def A_MATE = { F_MATE, 1, 1, ANIM_LOOP };
static const anim_def A_MATE_WAVE = { F_MATE, 2, 12, ANIM_LOOP };

static const leg_t *leg;
static uint8_t next_spawn;
static uint8_t coo_cool;

void actors_reset(void) {
    leg = &LEGS[game.leg];
    // nothing from the pages the buffer no longer holds
    for (next_spawn = 0; next_spawn < leg->spawn_count && leg_spawns[next_spawn].tx < course_base; next_spawn++) {}
    coo_cool = 0;
}

/* returns 0 when the pool is full and the spawn must be retried (only for the actors a leg needs) */
static uint8_t spawn(const spawn_t *s) {
    int16_t x = ((int16_t)s->tx - course_base) * 16;
    int16_t y = (int16_t)s->ty * 16;
    game_entity_t *e;
    switch (s->type) {
        case SP_PIGEON:
            e = physics_entities_spawn(&game_entities, E_PIGEON, x + 2, y + 6, 12, 10);
            if (!e) break;   // a missing pigeon doesn't matter
            e->entity.physics.visual.ox = -2;
            e->entity.physics.visual.oy = -6;
            e->entity.physics.visual.flip = s->arg ? SPRITE_FLIP_X : 0;
            animated_physics_entity_play(&e->entity, s->arg ? &A_PIGEON_IDLE2 : &A_PIGEON_IDLE);
            break;
        case SP_STAMP:
            e = physics_entities_spawn(&game_entities, E_STAMP, x + 2, y + 2, 12, 12);
            if (!e) break;
            e->entity.physics.visual.ox = -2;
            e->entity.physics.visual.oy = -2;
            animated_physics_entity_play(&e->entity, &A_STAMP);
            break;
        case SP_BEACON:
            // stands on the roof: the 32 px sprite ends on the roof top
            e = physics_entities_spawn(&game_entities, E_BEACON, x + 4, y - 32, 8, 32);
            if (!e) return 0;
            e->entity.physics.visual.tall = 1;
            e->entity.physics.visual.ox = -4;
            // already lit when the run restarts from it (or from a later one)
            e->timer = s->tx <= hero_check_tx();
            animated_physics_entity_play(&e->entity, e->timer ? &A_BEACON_GREEN : &A_BEACON_RED);
            break;
        case SP_MATE:
            e = physics_entities_spawn(&game_entities, E_MATE, x + 3, y - 28, 10, 28);
            if (!e) return 0;
            e->entity.physics.visual.tall = 1;
            e->entity.physics.visual.ox = -3;
            e->entity.physics.visual.oy = -4;
            e->entity.physics.visual.flip = SPRITE_FLIP_X;
            animated_physics_entity_play(&e->entity, &A_MATE);
            break;
    }
    return 1;
}

void actors_update(void) {
    game_entity_t *e, *hero = hero_entity();
    int16_t hx = 0, hy = 0;

    // spawn what is about to scroll in
    while (next_spawn < leg->spawn_count) {
        const spawn_t *s = &leg_spawns[next_spawn];
        if (((int16_t)s->tx - course_base) * 16 > cam.x + SPAWN_AHEAD) break;
        if (s->type < SP_ZIP && !spawn(s)) break;
        next_spawn++;
    }
    if (coo_cool) coo_cool--;
    if (hero) {
        hx = PHYSICS_FP_INT(hero->entity.physics.body.x);
        hy = PHYSICS_FP_INT(hero->entity.physics.body.y);
    }

    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        int16_t ex = PHYSICS_FP_INT(e->entity.physics.body.x);
        int16_t dx;
        if (e->entity.physics.visual.type == E_HERO) continue;
        if (ex < cam.x - FREE_BEHIND) {
            entity_pool_free(&game_entities, e);
            continue;
        }
        dx = ex - hx;
        switch (e->entity.physics.visual.type) {
            case E_PIGEON:
                if (e->timer == 0) {
                    int16_t dy = PHYSICS_FP_INT(e->entity.physics.body.y) - hy;
                    if (hero && dx < SCARE_X && dx > -SCARE_X && dy < SCARE_Y && dy > -SCARE_Y) {
                        // scatter away from the courier, up and forward
                        e->timer = 1;
                        e->entity.physics.body.vx = (dx >= 0 ? 20 : -14) + (rand8() & 15);
                        e->entity.physics.body.vy = -12 - (rand8() & 15);
                        e->entity.physics.visual.flip = e->entity.physics.body.vx < 0 ? SPRITE_FLIP_X : 0;
                        animated_physics_entity_play(&e->entity, &A_PIGEON_FLY);
                        score_add(10);
                        if (!coo_cool) {
                            game_sfx_pigeon();
                            coo_cool = 20;
                        }
                    }
                } else {
                    e->entity.physics.body.x += e->entity.physics.body.vx;
                    e->entity.physics.body.y += e->entity.physics.body.vy;
                    if (e->entity.physics.body.vy > -40) e->entity.physics.body.vy -= 1;
                    if (PHYSICS_FP_INT(e->entity.physics.body.y) < -16) {
                        entity_pool_free(&game_entities, e);
                        continue;
                    }
                }
                break;
            case E_STAMP:
                if (hero && body_overlap(&e->entity.physics.body, &hero->entity.physics.body)) {
                    game.stamps++;
                    score_add(100);
                    game_sfx_stamp();
                    entity_pool_free(&game_entities, e);
                    continue;
                }
                break;
            case E_BEACON:
                if (e->timer == 0 && hero && dx < 0) {
                    e->timer = 1;
                    animated_physics_entity_play(&e->entity, &A_BEACON_GREEN);
                    hero_set_checkpoint(course_base + (uint8_t)(ex >> 4), (uint8_t)((PHYSICS_FP_INT(e->entity.physics.body.y) + 32) >> 4));
                    game_sfx_beacon();
                    score_add(50);
                }
                break;
            case E_MATE:
                if (hero && dx < 96 && dx > -96) animated_physics_entity_play(&e->entity, &A_MATE_WAVE);
                // reaching the hatch's column is enough, even mid-jump over the friend's head
                if (hero && !hero_done() && !hero_out() && dx <= 4) hero_celebrate();
                break;
            case E_DUST:
                if (--e->timer == 0) {
                    entity_pool_free(&game_entities, e);
                    continue;
                }
                break;
        }
        e->entity.physics.visual.sprite->tile = anim_tick(&e->entity.anim);
    }
}
