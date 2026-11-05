/*
 * Clotheslines: straight lines from a pole down to the right, 4 px per tile (1/4 slope).
 * They come from SP_ZIP entries in the leg's spawn list: start column, start height / 4, length.
 * Cache the whole leg in leg pixels so rope IDs stay stable across buffer rebases.
 */
#include "game.h"

static uint8_t zips;
static int16_t zip_x0[MAX_ZIPS], zip_y0[MAX_ZIPS], zip_x1[MAX_ZIPS];

void zips_load(void) {
    const leg_t *leg = &LEGS[game.leg];
    uint8_t i;
    zips = 0;
    for (i = 0; i < leg->spawn_count && zips < MAX_ZIPS; i++) {
        const spawn_t *s = &leg_spawns[i];
        if (s->type != SP_ZIP) continue;
        zip_x0[zips] = (int16_t)s->tx * 16;
        zip_y0[zips] = (int16_t)s->ty * 4;
        zip_x1[zips] = zip_x0[zips] + (int16_t)s->arg * 16;
        zips++;
    }
}

uint8_t zip_count(void) {
    return zips;
}

int16_t zip_line_y(uint8_t zip, int16_t px) {
    px += (int16_t)course_base * 16;
    if (px < zip_x0[zip] || px >= zip_x1[zip]) return -1;
    return zip_y0[zip] + ((px - zip_x0[zip]) >> 2);
}
