/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host, then a look at what tests/scripts/home1.txt left: MACROS' switches on the keys as the screen
 * draws them (2026-11-21, the user: "what is on top of the screen on black, what is at the bottom on white"): GLO + F3
 * ARP on, GLO + F#3 the FILTER off, GLO + D#4 SPACE off; F#5 and G5, RADAR's and the drums' keys before, switch
 * nothing now. Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void home_check(void)
{
    uint32_t arp = home_on(5u), filt = home_on(0u), space = home_on(4u), radar = home_on(10u), drums = home_on(11u);
    if (!arp || filt || space || radar || drums || home_sw(0u) != 5 || home_sw(1u) != 0 || home_sw(10u) != 4 ||
        home_sw(11u) != 11 || home_sw(12u) != -1 || home_sw(13u) != -1 || home_sw(26u) != -1) {
        printf("FAIL  HOME's switches: ARP %u (1), FILTER %u (0), SPACE %u (0), RADAR %u (0), DRUMS %u (0); keys F3 %d, "
               "F#3 %d, D#4 %d, E4 %d, F4 %d, F#4 %d, G5 %d\n", arp, filt, space, radar, drums, home_sw(0u),
               home_sw(1u), home_sw(10u), home_sw(11u), home_sw(12u), home_sw(13u), home_sw(26u));
        fflush(stdout);
        _exit(1);
    }
    printf("ok    HOME's switches: the chain on F#3..D#4, the players on F3..E4; the keys above play\n");
}
