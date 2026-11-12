/*
 * HUD and banners as sprites. Both tile layers scroll, so text that has to stay put is drawn
 * with the font tiles (at their ASCII codes) as sprites on top of everything.
 */
#include "game.h"
#include <zgdk/ascii.h>
#include <core.h>
#include <zgdk/hud/sprite.h>
#include "sprite_groups.h"

#define H_LIVES   0   // icon + digit (+ one spare)
#define H_PIPS    3   // 4 meter pips
#define H_SCORE   7   // 5 digits
#define HUD_Y     (6 + 16)
#define SPRITE_OFF 0

static gfx_sprite *hud;   // contiguous HUD sprites, followed by the banner rows
#define TEXT(line, i) (&hud[HUD_SPRITES + (line) * TEXT_LEN + (i)])
static uint8_t shown;
static uint16_t last_score = 0xFFFF;
static uint8_t last_lives = 0xFF;
static uint8_t last_meter = 0xFF;

static gfx_sprite *add(uint16_t x) {
    gfx_sprite s = { .y = SPRITE_OFF, .x = x, .tile = 0, .flags = SPRITE_NONE, .options = SPRITE_OPTION_NONE };
    return sprites_register(s);
}

zos_err_t hud_init(void) {
    uint8_t i, j;
    gfx_sprite *last;
    // This game's font occupies tiles 32-95; unsupported characters are blank.
    mem_set(ASCII_MAP, 32, sizeof(ASCII_MAP));
    ascii_map(' ', 64, 32);

    // the lives sit right of the storm front
    hud = add(52 + 16);
    add(68 + 16);
    add(0);
    for (i = 0; i < METER_MAX; i++) add(128 + i * 16 + 16);
    for (i = 0; i < 5; i++) add(236 + i * 12 + 16);
    for (j = 0; j < TEXT_LINES; j++) {
        for (i = 0; i < TEXT_LEN; i++) last = add(0);
    }
    if (!last) return ERR_NO_MORE_MEMORY;
    hud[H_LIVES].tile = SPR_LIFE;
    return ERR_SUCCESS;
}

void hud_show(uint8_t on) {
    uint8_t i;
    shown = on;
    for (i = 0; i < 12; i++) hud[i].y = on ? HUD_Y : SPRITE_OFF;
    hud[H_LIVES + 2].y = SPRITE_OFF;
    last_score = 0xFFFF;
    last_lives = 0xFF;
    last_meter = 0xFF;
    rooftop_sprites_dirty(SPR_GROUP_LIVES);
    rooftop_sprites_dirty(SPR_GROUP_METER);
    rooftop_sprites_dirty(SPR_GROUP_SCORE);
}

void hud_draw(void) {
    uint8_t i;
    if (!shown) return;
    if (game.lives != last_lives) {
        last_lives = game.lives;
        hud[H_LIVES + 1].tile = ascii_to('0' + game.lives);
        rooftop_sprites_dirty(SPR_GROUP_LIVES);
    }
    if (game.meter != last_meter) {
        last_meter = game.meter;
        rooftop_sprites_dirty(SPR_GROUP_METER);
        for (i = 0; i < METER_MAX; i++) {
            uint8_t t = SPR_PIP_OFF;
            if (i < game.meter) t = game.meter == METER_MAX ? SPR_PIP_MAX : SPR_PIP_ON;
            hud[H_PIPS + i].tile = t;
        }
    }
    if (game.score != last_score) {
        uint16_t v = game.score;
        last_score = v;
        rooftop_sprites_dirty(SPR_GROUP_SCORE);
        for (i = 5; i > 0; i--) {
            hud[H_SCORE + i - 1].tile = ascii_to('0' + v % 10);
            v /= 10;
        }
    }
}

void text_show(uint8_t line, const char *str, uint8_t y) {
    hud_sprite_text_t row = { TEXT(line, 0), TEXT_LEN, 12 };
    // Preserve this font's visual centering (12 px advance, 2 px correction).
    hud_sprite_put_s_centered(&row, str, 158, y);
    rooftop_sprites_dirty(SPR_GROUP_TEXT0 + line);
}

void text_hide(uint8_t line) {
    hud_sprite_hide(TEXT(line, 0), TEXT_LEN);
    rooftop_sprites_dirty(SPR_GROUP_TEXT0 + line);
}

void text_hide_all(void) {
    hud_sprite_hide(TEXT(0, 0), TEXT_LINES * TEXT_LEN);
    rooftop_sprites_dirty(SPR_GROUP_TEXT0);
    rooftop_sprites_dirty(SPR_GROUP_TEXT1);
    rooftop_sprites_dirty(SPR_GROUP_TEXT2);
}

void score_add(uint16_t points) {
    if ((uint16_t)(game.score + points) < game.score) game.score = 0xFFFF;
    else game.score += points;
}
