/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host (tests/host.c through stardistroyer.c) with what was saved before the drums (2026-11-19): the
 * settings in their old size (120 -> 100 BPM), a song in the songs' old size (S01: on, 2 steps, P03 on step 1, its
 * steps 2 loops). At the start: the settings kept, the drums' kit and beat the lab's, S01 as it was with no beats
 * (0: as they play), the drums off.
 *   tests/scripts/dr1.txt  DRUM: steps, MIX, a sound, PRESETS D03 (at the bar's one), SAVE to D09: in slot 4
 *   tests/scripts/dr2.txt  ORBIT: step 1 the beat D03, step 2 OFF; PLAY: on with D03 (ends in step 1)
 *   tests/scripts/dr3.txt  the same, on into step 2: the drums off there
 * Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t beat_is(const SdDrumPat *p, uint32_t f)   /* the factory's beat f */
{
    uint32_t d, i;
    for (d = 0; d < SD_DRS; d++)
        for (i = 0; i < SD_DR_STEPS; i++)
            if (SD_DR_STEP(p, d, i) != (uint32_t)(DR_FBEAT[f][d][i] - '0')) return 0;
    return 1;
}
__attribute__((constructor)) static void drum_old_saves(void)
{
    static uint8_t old[UORB_V2];
    const uint32_t osz = (uint32_t)sizeof(SdOrbit) - SD_ORB_STEPS - SD_ORB_TAIL_B;   /* (before the drums, and
                                                   LOOP's and FREEZE's pitch after them) */
    SdOrbit o;
    uint8_t s16[16];
    memset(&o, 0, sizeof o);
    o.on = 1; o.len = 2; o.pat[0][0] = 3; o.loops[0] = o.loops[1] = 2;
    old[0] = 1;                                 /* (used: S01) */
    memcpy(old + 4, &o, osz);                   /* (its bytes, as the struct was: the beats were its end) */
    host_save(host_slot[3], SLOT_MAX, old, UORB_V2, "slot", 3);
    memcpy(s16, &set, 16);                      /* the settings before the drums: their first 16 B */
    s16[1] = 100;
    host_save(host_set, SECTOR_MAX, s16, 16, "settings", 0);
}
__attribute__((destructor)) static void drum_check(void)
{
    const SdOrbit *s = &usong.o[0];
    uint32_t i, n4 = (uint32_t)(host_slot[4][1] | host_slot[4][2] << 8 | host_slot[4][3] << 16), drm = 0;
    for (i = 0; i < SD_ORB_STEPS; i++) drm |= s->drm[i];
    if (set.bpm != 100u || !s->on || s->len != 2u || s->pat[0][0] != 3u || s->loops[1] != 2u ||
        (fm1_ms < 3800u && drm)) {
        printf("FAIL  the saves before the drums: %u BPM (want 100), S01 %s\n", set.bpm,
               s->on && s->len == 2u && s->pat[0][0] == 3u ? "moved, its beats wrong" : "lost");
        fflush(stdout); _exit(1);
    }
    if (page == PAGE_DRUM) {                    /* dr1 */
        if (!eng.dr.on || set.drum != 8u || !(udr.used >> 8 & 1u) || n4 != sizeof udr || !beat_is(&udr.p[8], 2) ||
            !beat_is(&set.dpat, 2) || memcmp(&eng.dr.pat, &set.dpat, sizeof set.dpat) || set.dkit.d[0].level != 78u ||
            set.dkit.d[1].v[3] != 55 || memcmp(&eng.dr.kit, &set.dkit, sizeof set.dkit)) {
            printf("FAIL  DRUM: on %u, D%02u (used %x, slot 4 %u B of %u), FOUR %s/%s, the kick %u%%, the snare's "
                   "SIZE %d\n", eng.dr.on, set.drum + 1u, udr.used, n4, (uint32_t)sizeof udr,
                   beat_is(&udr.p[8], 2) ? "saved" : "not saved", beat_is(&set.dpat, 2) ? "loaded" : "not loaded",
                   set.dkit.d[0].level, set.dkit.d[1].v[3]);
            fflush(stdout); _exit(1);
        }
        printf("ok    DRUM: steps, MIX, a sound; D03 FOUR loaded at the bar's one, saved to D09 (slot 4, %u B); the "
               "saves before the drums kept (%u BPM, S01)\n", n4, set.bpm);
    } else if (fm1_ms < 8000u) {                /* dr2: in the song's step 1 */
        if (!eng.dr.on || !beat_is(&eng.dr.pat, 2) || !beat_is(&set.dpat, 2) || set.drum != 2u || orbp.drm[0] != 3u ||
            orbp.drm[1] != SD_DR_SONG_OFF) {
            printf("FAIL  ORBIT's DRUMS: on %u, the engine's beat %s, DRUM's %s (D%02u), the steps %u %u\n", eng.dr.on,
                   beat_is(&eng.dr.pat, 2) ? "FOUR" : "not FOUR", beat_is(&set.dpat, 2) ? "FOUR" : "not FOUR",
                   set.drum + 1u, orbp.drm[0], orbp.drm[1]);
            fflush(stdout); _exit(1);
        }
        printf("ok    ORBIT's DRUMS: step 1 D03, PLAY: the drums on with FOUR, DRUM's page with it\n");
    } else {                                    /* dr3: step 2 */
        if (eng.dr.on || !eng.seq_run || eng.orb_step != 1u) {
            printf("FAIL  ORBIT's DRUMS OFF: on %u, PLAY %u, step %u\n", eng.dr.on, eng.seq_run, eng.orb_step + 1u);
            fflush(stdout); _exit(1);
        }
        printf("ok    ORBIT's DRUMS: step 2 OFF, the drums off there, PLAY on\n");
    }
}
