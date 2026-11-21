/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host (tests/host.c through stardistroyer.c) with ORBIT saved as it was till 2026-11-19 (slot 3:
 * 32 ORBITs, a pattern's each): P01 saved with the default ORBIT (nothing in it), P03 (E4s) with one that steps (on, 2
 * steps, RATE 2 loops, XPOSE A +5 on step 2). At the start it becomes song S01 (the only one with something in it):
 * its steps 2 loops each, step 1 SEQ A's P03. tests/scripts/sg1.txt: PLAY: the song loads P03 at START, SEQ A's page
 * with it. sg2.txt: SAVE on ORBIT (S01, then S02), STOP: the songs in slot 3 in their own size. Run by
 * tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((constructor)) static void song_old_saves(void)
{
    static UserPatterns up;
    static uint8_t old[UORB_V1];
    SdOrbitV1 *o = (SdOrbitV1 *)(old + 4u + 2u * sizeof(SdOrbitV1));
    uint32_t i;
    sd_pattern_default(&up.p[0]);
    sd_pattern_default(&up.p[2]);
    for (i = 0; i < SD_PSTEPS; i++) { up.p[2].step[i].note = 64; up.p[2].step[i].type = SD_ST_NOTE; }
    up.used = 1u | 4u;
    host_save(host_slot[1], SLOT_MAX, &up, USEQ_HALF, "slot", 1);
    host_save(host_slot[2], SLOT_MAX, (uint8_t *)&up + USEQ_HALF, sizeof up - USEQ_HALF, "slot", 2);
    old[0] = 1u | 4u;                           /* (used: P01's, P03's) */
    o->on = 1; o->len = 2; o->rate = 1; o->xa[1] = 5;
    host_save(host_slot[3], SLOT_MAX, old, UORB_V1, "slot", 3);
}
__attribute__((destructor)) static void song_check(void)
{
    const SdOrbit *s = &usong.o[0];
    uint32_t mig = s->on && s->len == 2u && s->loops[0] == 2u && s->loops[1] == 2u && s->xa[1] == 5 && s->pat[0][0] == 3u &&
                   !s->pat[1][0] && !s->pat[0][1];
    uint32_t played = eng.t[0].song_n == 3u && seqp.step[0].note == 64u && set.pattern == 2u;
    uint32_t n = (uint32_t)(host_slot[3][1] | host_slot[3][2] << 8 | host_slot[3][3] << 16);
    if (eng.seq_run) {
        if (!mig || usong.used != 1u || !played) {
            printf("FAIL  the old ORBITs as songs: S01 %s (used %x), SEQ A plays P%02u (step 1 %u, SEQ's P%02u)\n",
                   mig ? "right" : "wrong", usong.used, eng.t[0].song_n, seqp.step[0].note, set.pattern + 1u);
            _exit(1);
        }
        printf("ok    the old ORBITs as songs: S01 (P03's: 2 steps of 2 loops, +5 on step 2, P03 on step 1); PLAY: "
               "the song loads P03, SEQ A's page with it\n");
    } else {
        if (usong.used != 3u || n != sizeof usong || memcmp(host_slot[3] + 4, &usong, sizeof usong) ||
            memcmp(&usong.o[1], &orbp, sizeof orbp) || set.song != 1u) {
            printf("FAIL  SAVE on ORBIT: songs used %x, slot 3 %u B (want %u), S%02u\n", usong.used, n,
                   (uint32_t)sizeof usong, set.song + 1u);
            _exit(1);
        }
        printf("ok    SAVE on ORBIT: S01 and S02, in the flash in the songs' own size (%u B)\n", n);
    }
}
