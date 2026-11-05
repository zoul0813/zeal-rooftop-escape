/*
 * Background music: ZMT tracks (tools/make_music.py) played with ZGDK's tracker.
 *
 * One track is loaded at a time. A pattern_t takes 772 bytes of RAM, so all tracks share four
 * pattern buffers (the start of asset_work, after the tileset has gone through it), and a track is
 * read from rooftop.zar into zmt_buf and parsed by zmt_rom_load() when it changes. ZGDK's device
 * loader is private (zmt_file_load() only takes a path); a public zmt_dev_load(track, dev) would
 * stream it from the archive instead. Tracks use voices 0-2; voice 3 belongs to the effects.
 */
#include "game.h"
#include "assets.h"

#define MAX_PATTERNS MUSIC_PATTERNS

#define ZMT_MAX 640   // the largest track, run.zmt, is 592 bytes

typedef struct {
    const char *name;   // entry in rooftop.zar
    uint8_t loop;
} song_t;

static const song_t SONGS[] = {
    [BGM_TITLE]   = { "title.zmt", 1 },
    [BGM_RUN]     = { "run.zmt", 1 },
    [BGM_SAFE]    = { "safe.zmt", 0 },
    [BGM_CAUGHT]  = { "caught.zmt", 0 },
    [BGM_WIN]     = { "win.zmt", 0 },
};

static uint8_t zmt_buf[ZMT_MAX];
static track_t track;
static uint8_t current = BGM_NONE;
static uint8_t playing;

/* zmt_sound_off() mutes every voice, the effects voice too */
static void sound_off(void) {
    zmt_sound_off();
    sfx_restore();
}

/* Call after load_tileset(): the patterns are the start of its load buffer */
zos_err_t bgm_init(void) {
    uint8_t i;
    pattern_t *patterns = (pattern_t *)asset_work;
    if (MUSIC_RAM != MAX_PATTERNS * sizeof(pattern_t)) return ERR_INVALID_PARAMETER;
    for (i = 0; i < MAX_PATTERNS; i++) track.patterns[i] = &patterns[i];
    zmt_reset(VOL_75);
    return ERR_SUCCESS;
}

void bgm_play(uint8_t song) {
    const song_t *s = &SONGS[song];
    sound_off();
    if (song != current) {
        // loading reinitialises every pattern; it only happens between screens
        uint16_t size = sizeof(zmt_buf);
        if (zar_entry_load(&pack, s->name, zmt_buf, &size) != ERR_SUCCESS ||
            zmt_rom_load(&track, zmt_buf, size) != ERR_SUCCESS) {
            current = BGM_NONE;
            playing = 0;
            return;
        }
        current = song;
    }
    zmt_track_reset(&track, 1);
    playing = 1;
}

void bgm_stop(void) {
    playing = 0;
    sound_off();
}

/* Pause keeps the playhead; bgm_resume() carries on from the next step */
void bgm_pause(void) {
    if (!playing) return;
    playing = 2;
    sound_off();
}

void bgm_resume(void) {
    if (playing == 2) playing = 1;
}

uint8_t bgm_busy(void) {
    return playing == 1;
}

/* Once per frame */
void bgm_tick(void) {
    if (playing != 1) return;
    // a one-shot ends when the last step has played and the playhead wraps to step 0
    if (zmt_tick(&track, 1) == 0 && !SONGS[current].loop) bgm_stop();
}
