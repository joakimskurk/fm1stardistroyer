/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host (tests/host.c through stardistroyer.c), then a look at what the script left: SAVE and PRESETS
 * while the sequencer plays (tests/scripts/sv1.txt, sv2.txt: PLAY, SEQ A, SAVE twice: P01; PRESETS +1, EDIT: P02).
 * 2026-11-19, the user: saving and loading patterns take time; seamless at the next one. SAVE no longer stops it, the
 * flash written once PLAY stops (an erase stalls the sound); P02 comes in at the one. sv1 ends playing: P01 saved, not
 * yet in the flash, P02 taken; sv2 stops first: P01 in the flash. Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void save_check(void)
{
    uint32_t in_flash = host_slot[1][0] && !memcmp(host_slot[1] + 4, &useq, USEQ_HALF);
    uint32_t ok = (useq.used & 1u) && set.pattern == 1u && !eng.t[0].pat_wait && !eng.orb_wait;
    if (eng.seq_run) ok = ok && (save_pending & 2u) && !host_slot[1][0];
    else ok = ok && !save_pending && in_flash;
    if (!ok) {
        printf("FAIL  SAVE and PRESETS while it plays: playing %u, P01 saved %u, waits to be written %u, in the flash "
               "%u, pattern P%02u, waiting %u %u\n", eng.seq_run, useq.used & 1u, save_pending, in_flash,
               set.pattern + 1u, eng.t[0].pat_wait, eng.orb_wait);
        _exit(1);
    }
    printf(eng.seq_run ? "ok    SAVE while it plays: P01 saved, it plays on, the flash waits for STOP; P02 in at the one\n"
                       : "ok    SAVE while it plays: P01 in the flash once PLAY stopped; P02 in at the one\n");
}
