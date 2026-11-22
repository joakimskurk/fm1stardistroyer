/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/recg.txt: REC while it plays writes each note's GATE from how long its key was
 * held (2026-11-22, the user): C4 held 60 ms of a 125 ms step, its GATE about half; D4 held 280 ms, its TIEs and the
 * last one's GATE together about 2.24 steps (as long as it was held). Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void recgate_check(void)
{
    uint32_t k, gc = 0, nc = 0, nd = 0, total = 0;
    for (k = 0; k < seqp.len && k < SD_PSTEPS; k++) {
        const SdStep *s = &seqp.step[k];
        if (s->type == SD_ST_NOTE && s->note == 60u) { gc = s->gate; nc++; }
        if (s->type == SD_ST_NOTE && s->note == 62u) {   /* its length: a step a TIE, then the last one's GATE */
            uint32_t n = k, ties = 0;
            nd++;
            while (seqp.step[(n + 1u) % seqp.len].type == SD_ST_TIE && ties < 8u) { n = (n + 1u) % seqp.len; ties++; }
            total = ties * 100u + seqp.step[n].gate;
        }
    }
    if (nc != 1u || nd != 1u || gc < 30u || gc > 70u || total < 190u || total > 260u) {
        printf("FAIL  REC's GATE: C4 %u time(s), its GATE %u (about 48); D4 %u time(s), its length %u %% of a step "
               "(about 224)\n", nc, gc, nd, total);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    REC's GATE: C4 held 60 ms, GATE %u %%; D4 held 280 ms, %u %% of a step with its TIEs\n", gc, total);
}
