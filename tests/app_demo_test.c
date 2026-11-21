/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host (tests/host.c through stardistroyer.c), then a look at what the script left: the demo from
 * SETUP (tests/scripts/dm1.txt: ALGORITHM a detent, OCT+, still playing at the end: at its tempo, its renders on);
 * dm2.txt: PLAY stops it, and all the player had in the engine is back (the patch, the patterns, ORBIT, the tempo,
 * the mode, stopped); dm3.txt: ALGORITHM two detents, DEMO 2, ARMADA with its drums. Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void demo_check(void)
{
    if (demo_st) {
        const DemoSong *d = &DEMOS[demo_k & 1u];
        uint32_t want = (30000u - 2400u) * 441u / 100u / 256u * 9u / 10u;
        if (demo_st != DEMO_PLAY || demo_r < want || eng.bpm != d->bpm || !eng.seq_run || (demo_k && !eng.dr.on)) {
            printf("FAIL  the demo from SETUP: %s, state %u, %u renders (want %u or more), %u BPM, PLAY %u, drums %u\n",
                   d->name, demo_st, demo_r, want, eng.bpm, eng.seq_run, eng.dr.on);
            _exit(1);
        }
        printf("ok    the demo from SETUP: DEMO %u, %s plays (%u renders, %u BPM%s)\n", demo_k + 1u, d->name, demo_r,
               eng.bpm, eng.dr.on ? ", the drums on" : "");
        return;
    }
    if (eng.bpm != set.bpm || eng.mode != set.mode || eng.seq_run || memcmp(&eng.patch_in, &pat, sizeof pat) ||
        memcmp(&eng.t[seq_trk].pat_in, &seqp, sizeof seqp) || memcmp(&eng.t[seq_trk ^ 1u].pat_in, &seq_other, sizeof seq_other) ||
        memcmp(&eng.orb_in, &orbp, sizeof orbp) || eng.pats != useq.p) {
        printf("FAIL  after the demo: the player's not back (%u BPM of %u, mode %u of %u, PLAY %u, the patch %s, the "
               "patterns %s, ORBIT %s)\n", eng.bpm, set.bpm, eng.mode, set.mode, eng.seq_run,
               memcmp(&eng.patch_in, &pat, sizeof pat) ? "not" : "back",
               memcmp(&eng.t[seq_trk].pat_in, &seqp, sizeof seqp) ? "not" : "back",
               memcmp(&eng.orb_in, &orbp, sizeof orbp) ? "not" : "back");
        _exit(1);
    }
    printf("ok    the demo stopped by a button: the player's patch, patterns, ORBIT, tempo and mode back\n");
}
