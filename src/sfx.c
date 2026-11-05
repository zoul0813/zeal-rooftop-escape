/*
 * Sound effects: tables for ZGDK's sequencer (zgdk/sound/sfx.h) on voice 3.
 * Voices 0-2 belong to the ZMT music (music.c), which never writes voice 3.
 * Priorities keep a footstep-level sound from cutting a chain or a fall (sfx_play_prio).
 */
#include "game.h"

#define SFX_VOLUME VOL_100

static const sfx_seg S_JUMP[]    = { SFX_SEG(WAV_SQUARE, 300, 620, 7), SFX_END };
static const sfx_seg S_LAND[]    = { SFX_SEG(WAV_NOISE, 900, 300, 3), SFX_END };
static const sfx_seg S_KICK[]    = { SFX_SEG(WAV_NOISE, 2400, 1200, 2), SFX_SEG(WAV_SQUARE, 500, 1000, 6), SFX_END };
static const sfx_seg S_ZIP[]     = { SFX_SEG(WAV_SAWTOOTH, 900, 1400, 6), SFX_SEG(WAV_SAWTOOTH, 1400, 1300, 18), SFX_END };
static const sfx_seg S_PIGEON[]  = { SFX_SEG(WAV_NOISE, 5000, 3000, 3), SFX_SEG(WAV_NOISE, 4000, 2500, 3), SFX_SEG(WAV_NOISE, 5000, 3000, 3), SFX_END };
static const sfx_seg S_STAMP[]   = { SFX_SEG(WAV_TRIANGLE, 1319, 1319, 3), SFX_SEG(WAV_TRIANGLE, 1976, 1976, 6), SFX_END };
static const sfx_seg S_STUMBLE[] = { SFX_SEG(WAV_NOISE, 3000, 800, 5), SFX_SEG(WAV_SQUARE, 300, 150, 10), SFX_END };
static const sfx_seg S_FALL[]    = { SFX_SEG(WAV_SQUARE, 900, 200, 36), SFX_SEG(WAV_NOISE, 400, 100, 8), SFX_END };
static const sfx_seg S_BEACON[]  = { SFX_SEG(WAV_SQUARE, 784, 784, 4), SFX_SEG(WAV_SQUARE, 1175, 1175, 4), SFX_SEG(WAV_SQUARE, 1568, 1568, 8), SFX_END };
static const sfx_seg S_SLIDE[]   = { SFX_SEG(WAV_NOISE, 1800, 900, 10), SFX_END };
static const sfx_seg S_THUNDER[] = { SFX_SEG(WAV_NOISE, 300, 120, 12), SFX_SEG(WAV_NOISE, 160, 60, 30), SFX_END };
/* the chain rises a step per meter level */
static const sfx_seg S_CHAIN1[]  = { SFX_SEG(WAV_SQUARE, 330, 660, 4), SFX_SEG(WAV_SQUARE, 659, 659, 3), SFX_END };
static const sfx_seg S_CHAIN2[]  = { SFX_SEG(WAV_SQUARE, 392, 784, 4), SFX_SEG(WAV_SQUARE, 784, 784, 3), SFX_END };
static const sfx_seg S_CHAIN3[]  = { SFX_SEG(WAV_SQUARE, 494, 988, 4), SFX_SEG(WAV_SQUARE, 988, 988, 3), SFX_END };
static const sfx_seg S_CHAIN4[]  = { SFX_SEG(WAV_SQUARE, 659, 1319, 4), SFX_SEG(WAV_SQUARE, 1319, 1319, 2), SFX_SEG(WAV_SQUARE, 1568, 1568, 4), SFX_END };
static const sfx_seg *const CHAIN[] = { S_CHAIN1, S_CHAIN1, S_CHAIN2, S_CHAIN3, S_CHAIN4 };

void game_sfx_init(void) {
    sfx_init(VOICE3, SFX_VOLUME);
}

void game_sfx_jump(void)    { sfx_play_prio(S_JUMP, 0); }
void game_sfx_land(void)    { sfx_play_prio(S_LAND, 0); }
void game_sfx_chain(uint8_t level) { sfx_play_prio(CHAIN[level > METER_MAX ? METER_MAX : level], 1); }
void game_sfx_climb(void)   { sfx_play_prio(S_KICK, 1); }
void game_sfx_zip(void)     { sfx_play_prio(S_ZIP, 1); }
void game_sfx_pigeon(void)  { sfx_play_prio(S_PIGEON, 1); }
void game_sfx_stamp(void)   { sfx_play_prio(S_STAMP, 2); }
void game_sfx_slide(void)   { sfx_play_prio(S_SLIDE, 0); }
void game_sfx_thunder(void) { sfx_play_prio(S_THUNDER, 1); }
void game_sfx_beacon(void)  { sfx_play_prio(S_BEACON, 2); }
void game_sfx_stumble(void) { sfx_play_prio(S_STUMBLE, 3); }
void game_sfx_fall(void)    { sfx_play_prio(S_FALL, 3); }
