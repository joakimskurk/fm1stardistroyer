/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* REC, playing, with tests/scripts/hold1.txt (2026-11-25; REC POLY, TOOLS' K4; hold2.txt the same in MONO, the default): a held chord's note changed writes a HOLD (the others go
 * on), a chord's key let go a HOLD of the rest, and keys 60 ms apart one chord wherever the step's middle falls (the
 * chord window). Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t notes_set(const SdStep *s)      /* its notes as bits from C4 (60) */
{
    uint8_t ns[4];
    uint32_t n = seq_notes_of(s, ns), i, b = 0;
    for (i = 0; i < n; i++) if (ns[i] >= 60u && ns[i] < 92u) b |= 1u << (ns[i] - 60u);
    return b;
}
#define B(n) (1u << ((n) - 60u))
__attribute__((destructor)) static void hold_check(void)
{
    uint32_t k, chg = 0, rel = 0, ce = 0, e_alone = 0, cegs = 0, d_alone = 0, cd = 0;
    if (!set.rec_poly) {                        /* REC MONO (hold2.txt): one note a step, nowhere a HOLD or a chord */
        uint32_t holds = 0, chords = 0, notes = 0;
        for (k = 0; k < seqp.len && k < SD_PSTEPS; k++) {
            const SdStep *s = &seqp.step[k];
            if (s->type != SD_ST_NOTE) continue;
            notes++;
            if (SD_IS_HOLD(s)) holds++;
            if (s->chord[0] || s->chord[1] || s->chord[2]) chords++;
        }
        if (holds || chords || notes < 8u) {
            printf("FAIL  REC MONO: %u HOLDs, %u chords (0, 0), %u notes (8 at least)\n", holds, chords, notes);
            fflush(stdout);
            _exit(1);
        }
        printf("ok    REC MONO: one note a step, no HOLD, no chord (%u notes)\n", notes);
        return;
    }
    for (k = 0; k < seqp.len && k < SD_PSTEPS; k++) {
        const SdStep *s = &seqp.step[k];
        uint32_t b = s->type == SD_ST_NOTE ? notes_set(s) : 0u, j;
        if (s->type != SD_ST_NOTE) continue;
        if (!SD_IS_HOLD(s) && b == (B(60) | B(64) | B(67))) {
            cegs++;
            for (j = k + 1u; j < seqp.len && seqp.step[j].type == SD_ST_TIE; j++) {}
            if (j < seqp.len && j > k + 1u && SD_IS_HOLD(&seqp.step[j])) {
                uint32_t h = notes_set(&seqp.step[j]);
                if (h == (B(60) | B(64) | B(69))) chg++;
                if (h == (B(60) | B(64))) rel++;
            }
        }
        if (!SD_IS_HOLD(s) && b == (B(60) | B(64))) ce++;
        if (b == B(64)) e_alone++;
        if (b == B(62)) d_alone++;
        if ((b & (B(60) | B(62))) == (B(60) | B(62))) cd++;
    }
    if (cegs != 3u || chg != 1u || rel != 1u || ce != 4u || e_alone || d_alone != 2u || cd) {
        printf("FAIL  REC's HOLD: C E G chords %u (3), then a HOLD C E A %u (1), a HOLD C E after G let go %u (1); "
               "C then E 60 ms after: chords %u (4), E alone %u (0); C to D, C let go 40 ms after D: D alone %u (2), "
               "C with D %u (0)\n", cegs, chg, rel, ce, e_alone, d_alone, cd);
        for (k = 0; k < seqp.len && k < 40u; k++)
            printf("  %2u %u%s %02X %d %d %d\n", k, seqp.step[k].type, SD_IS_HOLD(&seqp.step[k]) ? "H" : " ",
                   seqp.step[k].note, seqp.step[k].chord[0], seqp.step[k].chord[1], seqp.step[k].chord[2]);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    REC's HOLD: a held chord's note changed, the others go on (HOLD); a key let go, the rest hold on; "
           "keys 60 ms apart one chord; a tiny overlap trimmed, a short chord kept\n");
}
