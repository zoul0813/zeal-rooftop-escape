/* Real High Wire rope geometry must survive paging without reloads or ID changes. */
#include "game.h"
#include <assert.h>
#include <stdio.h>

game_t game;
uint8_t course_base;
const leg_t LEGS[LEG_COUNT] = { [5] = { .spawn_count = 3 } };
spawn_t leg_spawns[MAX_SPAWNS] = {
    { SP_ZIP, 22, 19, 15 },
    { SP_ZIP, 89, 19, 15 },
    { SP_ZIP, 177, 19, 15 }
};

int main(void) {
    uint8_t i, base;
    int16_t leg_x, start, end;
    game.leg = 5;
    course_base = 0;
    zips_load();
    assert(zip_count() == 3); /* includes the rope beyond the initial world buffer */
    for (base = 0; base <= 160; base += PAGE_W) {
        course_base = base; /* same-frame world/player rebase, with no rope reload */
        for (i = 0; i < zip_count(); i++) {
            start = (int16_t)leg_spawns[i].tx * 16;
            end = start + (int16_t)leg_spawns[i].arg * 16;
            assert(zip_line_y(i, start - base * 16 - 1) == -1);
            assert(zip_line_y(i, end - base * 16) == -1);
            for (leg_x = start; leg_x < end; leg_x++)
                assert(zip_line_y(i, leg_x - base * 16) == 76 + ((leg_x - start) >> 2));
        }
    }
    /* Loading a checkpoint at a different buffer origin keeps the same IDs. */
    course_base = 80;
    zips_load();
    assert(zip_count() == 3);
    assert(zip_line_y(1, 1520 - 80 * 16) == 100);
    /* A new leg cannot retain cached ropes from the previous leg. */
    game.leg = 0;
    zips_load();
    assert(zip_count() == 0);
    puts("ropes: stable coordinates and IDs across rebases/checkpoints passed");
    return 0;
}
