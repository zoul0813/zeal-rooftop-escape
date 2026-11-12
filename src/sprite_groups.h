#ifndef ROOFTOP_SPRITE_GROUPS_H
#define ROOFTOP_SPRITE_GROUPS_H

#include <zgdk/sprites.h>

/* Lower slots draw on top. HUD/banners precede the storm, then actors. */
enum {
    SPR_GROUP_LIVES, SPR_GROUP_METER, SPR_GROUP_SCORE,
    SPR_GROUP_TEXT0, SPR_GROUP_TEXT1, SPR_GROUP_TEXT2,
    SPR_GROUP_CLOUD, SPR_GROUP_EDGE, SPR_GROUP_ACTORS,
    SPR_GROUP_COUNT
};

gfx_error rooftop_sprites_init(gfx_sprite *arena);
void rooftop_sprites_dirty(uint8_t group);
/* Run after entity sync, outside vblank. */
void rooftop_sprites_prepare(void);
/* Only called in vblank; unsuccessful groups stay dirty for retry. */
gfx_error rooftop_sprites_upload(gfx_context *ctx);

#endif
