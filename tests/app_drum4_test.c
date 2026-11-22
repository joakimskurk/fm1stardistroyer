/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/dr4.txt: on DRUM, GLO held, ALGORITHM picks the drum and goes round (the
 * user, 2026-11-22: with GLO it had been the fourth's ENGINE and held the row at the fourth), the ENGINE only on
 * GLO + SELECT. Run by tests/run_tests.sh */
#include <unistd.h>
#define HOST_POLL_HOOK drum4_poll
#include "stardistroyer.c"
static uint32_t seen, perc_early = 0xFFu;
static void drum4_poll(void)
{
    if (page == PAGE_DRUM) seen |= 1u << (dr_cur % SD_DRS);
    if (fm1_ms > 2100u && fm1_ms < 2650u && set.dperc != 0u) perc_early = set.dperc;   /* (ALGORITHM, GLO held: never
                                                   the ENGINE) */
}
__attribute__((destructor)) static void drum4_check(void)
{
    if (page != PAGE_DRUM || seen != 15u || perc_early != 0xFFu || set.dperc != SD_PC_DATA || dr_cur != SD_DR_HAT) {
        printf("FAIL  DRUM, GLO held: page %u, rows seen %x (want f), the ENGINE moved by ALGORITHM to %u (none), "
               "GLO + SELECT's ENGINE %u (want %u), the row last %u (want %u)\n", page, seen, perc_early, set.dperc,
               SD_PC_DATA, dr_cur, SD_DR_HAT);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    DRUM, GLO held: ALGORITHM picks the drum and goes round, GLO + SELECT the fourth's ENGINE (%s)\n",
           SD_PC_NAME[set.dperc]);
}
