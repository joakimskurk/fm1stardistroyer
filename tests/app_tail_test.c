/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* ORBIT's LOOP and FREEZE pitch at each step (2026-11-22, the user: "I miss pitch on the loop and FREEZE in ORBIT").
 * Packed five bits a value (sd_orb_tail): every step's either value set to every pitch reads back, the others
 * untouched. The songs saved before it (their size less SD_ORB_TAIL_B) load as they were, with no pitch. With
 * tests/scripts/ot1.txt: on ORBIT's PITCH layer a step's LOOP +5 and FREEZE -7, while the song plays the engine takes
 * them (the loop's speed, the frozen tail's pitch). Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t pack_ok = 1;
__attribute__((constructor)) static void tail_old_saves(void)
{
    static uint8_t old[4u + USONGS * (sizeof(SdOrbit) - SD_ORB_TAIL_B)];
    const uint32_t osz = (uint32_t)sizeof(SdOrbit) - SD_ORB_TAIL_B;
    SdOrbit o, q;
    uint32_t w, s, w2, s2;
    int32_t v;
    memset(&o, 0, sizeof o);
    o.on = 1; o.len = 2; o.loops[0] = o.loops[1] = 1; o.drm[0] = 3; o.xa[1] = 4;
    old[0] = 1;                                 /* (used: S01) */
    memcpy(old + 4, &o, osz);
    host_save(host_slot[3], SLOT_MAX, old, sizeof old, "slot", 3);
    for (w = 0; w < 2u; w++)                    /* every value at every place, the others kept */
        for (s = 0; s < SD_ORB_STEPS; s++)
            for (v = -12; v <= 12; v++) {
                memset(&q, 0, sizeof q);
                for (w2 = 0; w2 < 2u; w2++) for (s2 = 0; s2 < SD_ORB_STEPS; s2++) sd_orb_tail_set(&q, w2, s2, (int32_t)((w2 * 7u + s2 * 3u) % 25u) - 12);
                sd_orb_tail_set(&q, w, s, v);
                for (w2 = 0; w2 < 2u; w2++)
                    for (s2 = 0; s2 < SD_ORB_STEPS; s2++) {
                        int32_t want = w2 == w && s2 == s ? v : (int32_t)((w2 * 7u + s2 * 3u) % 25u) - 12;
                        if (sd_orb_tail(&q, w2, s2) != want) pack_ok = 0;
                    }
            }
}
__attribute__((destructor)) static void tail_check(void)
{
    const SdOrbit *s0 = &usong.o[0];
    uint32_t k, none = 1;
    for (k = 0; k < SD_ORB_TAIL_B; k++) none &= s0->tail[k] == 0;
    if (!pack_ok || !s0->on || s0->len != 2u || s0->drm[0] != 3u || s0->xa[1] != 4 || !none ||
        sd_orb_tail(&orbp, 0, 0) != 5 || sd_orb_tail(&orbp, 1, 0) != -7 || eng.lp_orb != 5 * 256 || eng.sp_orb != -7) {
        printf("FAIL  ORBIT's LOOP and FREEZE pitch: packed %s, the old song %s, step 1's LOOP %d FREEZE %d (5, -7), "
               "the engine's %d %d (1280, -7)\n", pack_ok ? "ok" : "WRONG",
               s0->on && s0->len == 2u && s0->drm[0] == 3u && s0->xa[1] == 4 && none ? "as saved" : "CHANGED",
               (int)sd_orb_tail(&orbp, 0, 0), (int)sd_orb_tail(&orbp, 1, 0), eng.lp_orb, eng.sp_orb);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    ORBIT's LOOP and FREEZE pitch: every value at every step packed and read back; a song saved before "
           "loads with none; step 1's +5 and -7 in the engine as the song plays\n");
}
