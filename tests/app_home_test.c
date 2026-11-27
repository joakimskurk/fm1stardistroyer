/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host, then a look at what tests/scripts/home1.txt left: MACROS' switches on the keys as the screen
 * draws them (2026-11-21, the user: "what is on top of the screen on black, what is at the bottom on white";
 * 2026-11-27 the white keys ARP SEQ A SEQ B ORBIT, then SUB FREEZE LOOP RADAR): GLO + F3 ARP on, GLO + F#3 the FILTER
 * off, GLO + D#4 SPACE off, GLO + C4 SUB on, GLO + D4 FREEZE on (SPACE off: kept), GLO + F4 RADAR on; G4 plays, and
 * F#5 and G5, RADAR's and the drums' keys before, switch nothing. Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void home_check(void)
{
    uint32_t arp = home_on(HOME_ARP), filt = home_on(0u), space = home_on(4u), sub = home_on(HOME_SUB),
             frz = home_on(HOME_FRZ), radar = home_on(HOME_RADAR);
    if (!arp || filt || space || !sub || !fx_on(SD_FX_SUB) || !frz || !sp_frz || !radar || !SD_TUR_ON(&pat) ||
        home_sw(0u) != HOME_ARP || home_sw(1u) != 0 || home_sw(2u) != HOME_SQA || home_sw(6u) != HOME_ORB ||
        home_sw(7u) != HOME_SUB || home_sw(9u) != HOME_FRZ || home_sw(10u) != 4 || home_sw(11u) != HOME_LOOP ||
        home_sw(12u) != HOME_RADAR || home_sw(13u) != -1 || home_sw(14u) != -1 || home_sw(26u) != -1) {
        printf("FAIL  HOME's switches: ARP %u (1), FILTER %u (0), SPACE %u (0), SUB %u (1), FREEZE %u (1), RADAR %u (1); "
               "keys F3 %d, F#3 %d, G3 %d, B3 %d, C4 %d, D4 %d, D#4 %d, E4 %d, F4 %d, F#4 %d, G4 %d, G5 %d\n", arp, filt,
               space, sub, frz, radar, home_sw(0u), home_sw(1u), home_sw(2u), home_sw(6u), home_sw(7u), home_sw(9u),
               home_sw(10u), home_sw(11u), home_sw(12u), home_sw(13u), home_sw(14u), home_sw(26u));
        fflush(stdout);
        _exit(1);
    }
    pat.d_xover = (uint8_t)(pat.d_xover | SD_DX_LAST);   /* DISTROY after SPACE (2026-11-27): drawn last, the key
                                                   under its box: A#3 TIME, C#4 SPACE, D#4 DISTROY */
    if (home_sw(5u) != 3 || home_sw(8u) != 4 || home_sw(10u) != 2 || home_sw(1u) != 0 || home_sw(3u) != 1) {
        printf("FAIL  HOME, DISTROY last: A#3 %d (3), C#4 %d (4), D#4 %d (2), F#3 %d (0), G#3 %d (1)\n", home_sw(5u),
               home_sw(8u), home_sw(10u), home_sw(1u), home_sw(3u));
        fflush(stdout);
        _exit(1);
    }
    printf("ok    HOME's switches: the chain on F#3..D#4 (DISTROY last: on D#4); ARP SEQ A SEQ B ORBIT, SUB FREEZE LOOP RADAR on F3..F4; the "
           "keys above play\n");
}
