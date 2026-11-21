/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* REC while it plays (2026-11-20, the user: "the notes often land a little behind where I expect"): a key, or a pad,
 * goes to the step nearest where the player heard it, the engine's place less how far it runs ahead of the speaker
 * (rec_ahead); one past the middle into the next step, written as that step begins (it sounds once).
 *   tests/scripts/rec1.txt  SEQ A: 16 keys round the steps twice; steps 4 and 15 keep what they had (their keys came
 *                           past the middle: into 5 and 0)
 *   tests/scripts/rec2.txt  DRUM: the fourth's pad 16 times; steps 0 and 4 stay empty the same way
 * Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void rec_check(void)
{
    static const uint8_t WANT[16] = {60, 61, 62, 63, 0, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 0};
    char got[160], *p = got;
    uint32_t i, ok = 1;
    if (page == PAGE_DRUM) {
        for (i = 0; i < SD_DR_STEPS; i++) {
            uint32_t st = SD_DR_STEP(&set.dpat, 3, i), want = i != 0u && i != 4u;
            ok &= (st != 0u) == want;
            *p++ = st ? 'x' : '.';
        }
        *p = 0;
        if (!ok) { printf("FAIL  REC on DRUM: the fourth's row %s (want .xxx.xxxxxxxxxxx)\n", got); fflush(stdout); _exit(1); }
        printf("ok    REC on DRUM: each pad on the step nearest where it was heard (%s)\n", got);
        return;
    }
    for (i = 0; i < 16u; i++) {
        const SdStep *s = &seqp.step[i];
        uint32_t n = s->type == SD_ST_NOTE ? s->note : 0u;
        if (WANT[i] ? n != WANT[i] : (s->type == SD_ST_NOTE && n >= 60u)) ok = 0;
        p += sprintf(p, " %u", n);
    }
    if (!ok) { printf("FAIL  REC on SEQ: the steps%s (want 60 61 62 63 - 62 63 .. 71 -)\n", got); fflush(stdout); _exit(1); }
    printf("ok    REC on SEQ: each key on the step nearest where it was heard; the next step's as it begins\n");
}
