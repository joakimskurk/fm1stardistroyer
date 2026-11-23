/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/dr4.txt: on DRUM, ALGORITHM turns the layers round (GLO held or not: never
 * the fourth's ENGINE), the pads pick the drum, KIT's K4 the fourth's ENGINE. Run by tests/run_tests.sh */
#include <unistd.h>
#define HOST_POLL_HOOK drum4_poll
#include "stardistroyer.c"
static uint32_t seen, lays, perc_early = 0xFFu;
static void drum4_poll(void)
{
    if (page == PAGE_DRUM) { seen |= 1u << (dr_cur % SD_DRS); lays |= 1u << pg_lay[PAGE_DRUM]; }
    if (fm1_ms > 2100u && fm1_ms < 2650u && set.dperc != 0u) perc_early = set.dperc;   /* (ALGORITHM, GLO held: never
                                                   the ENGINE) */
}
__attribute__((destructor)) static void drum4_check(void)
{
    if (page != PAGE_DRUM || seen != 15u || lays != 63u || pg_lay[PAGE_DRUM] != LK_SOUND || perc_early != 0xFFu ||
        set.dperc != SD_PC_DATA || dr_cur != SD_DR_HAT) {
        printf("FAIL  DRUM: page %u, drums picked %x (want f), layers seen %x (want 3f), the last %u (want SOUND), the "
               "ENGINE moved by ALGORITHM to %u (none), KIT's ENGINE %u (want %u), the drum last %u (want %u)\n",
               page, seen, lays, pg_lay[PAGE_DRUM], perc_early, set.dperc, SD_PC_DATA, dr_cur, SD_DR_HAT);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    DRUM: ALGORITHM the layers, round (GLO held too), the pads pick the drum, KIT's K4 the fourth's "
           "ENGINE (%s)\n", SD_PC_NAME[set.dperc]);
}
