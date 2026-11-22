/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host (tests/host.c through stardistroyer.c), then a look at what the script left: SEQ's DUB (TOOLS'
 * K4, 2026-11-22) adds a key to the step's chord and takes nothing (tests/scripts/dub1.txt); built with -DDUB_UNDO,
 * TOOLS' UNDO takes the take back (dub2.txt). Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void dub_check(void)
{
    const SdStep *a = &seqp.step[0], *b = &seqp.step[1];
#ifdef DUB_UNDO
    if (seq_trk || a->type != SD_ST_NOTE || a->note != 60u || a->chord[0] || b->type != SD_ST_REST) {
        printf("FAIL  UNDO after a DUB: step 1 %u (%u) chord %d, step 2 type %u\n", a->note, a->type, a->chord[0], b->type);
        _exit(1);
    }
    printf("ok    UNDO after a DUB: step 1 C4 alone again\n");
#else
    if (seq_trk || !seq_dub || seq_layer != SL_TOOLS || a->type != SD_ST_NOTE || a->note != 60u || a->chord[0] != 4 ||
        a->chord[1] || b->type != SD_ST_REST) {
        printf("FAIL  DUB: step 1 %u (%u) chord %d %d, step 2 type %u, dub %u layer %u\n", a->note, a->type, a->chord[0],
               a->chord[1], b->type, seq_dub, seq_layer);
        _exit(1);
    }
    printf("ok    DUB: E4 joined step 1's C4 (+4), step 2 untouched\n");
#endif
}
