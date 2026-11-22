/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/ob2.txt: an ORBIT lane with no destination picks one on its own knob (GLO's
 * layer, K3), then the knob is its amount (2026-11-21, the user could not find the destinations in SETUP). Run by
 * tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void lane_check(void)
{
    uint32_t d = orbp.dst[0], s = orb_cur % SD_ORB_STEPS;
    int32_t a = orbp.lane[0][s];
    if (page != PAGE_ORB || !d || !a || orbp.dst[1]) {
        printf("FAIL  ORBIT's lane 1: page %u, its destination %u (picked?), its amount %d (moved?), lane 2's %u (0)\n",
               page, d, a, orbp.dst[1]);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    ORBIT's lane 1 picked its destination on its knob (%s), then its amount %+d %%\n", DST_NAME[d % SD_MDSTS], a);
}
