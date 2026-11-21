/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host (tests/host.c through stardistroyer.c), then a look at what the script left: the track that is
 * not the master writes its RND line in the master's key (tests/scripts/k2.txt: A's root B, minor; B's RND). 2026-11-19,
 * the user: B did not follow A's root and key (its RND took its own, never set). Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void key_check(void)
{
    const SdPattern *kp = key_pat();
    uint32_t k, key = sd_pat_key(kp) % 12u, mask = SD_SCALE_MASK[kp->scale % SD_SCALES], off = 0, n = 0;
    for (k = 0; k < seqp.len && k < SD_PSTEPS; k++) {
        const SdStep *s = &seqp.step[k];
        if (s->type >= SD_ST_REST) continue;
        n++;
        if (!(mask >> ((s->note + 12u - key) % 12u) & 1u)) off++;
    }
    if (seq_trk != 1u || key_trk() != 0u || key != 11u || n < 4u || off || seqp.step[0].note % 12u != key) {
        printf("FAIL  SEQ B's RND in A's key (root %u, the line's step 1 %u): %u of %u notes off it\n", key,
               seqp.step[0].note % 12u, off, n);
        _exit(1);
    }
    printf("ok    SEQ B's RND in A's key: root B, %u notes, none off it, step 1 the root\n", n);
}
