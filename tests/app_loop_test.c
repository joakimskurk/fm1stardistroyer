/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/lp1.txt: the looper switched off keeps its loop (2026-11-21, the user: off
 * from HOME it crackled and the loop was gone). At 8.6 s (off): held, the memory still lent, neither playing nor
 * recording, its write head where it stopped; at 10.5 s (on again): LOOP playing; at 13.5 s (cleared, asked): the
 * memory back to the delay. Run by tests/run_tests.sh */
#define HOST_POLL_HOOK loop_poll
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t lw_off, lw_later, ok_off, ok_on, ok_clr, seen;
static void loop_poll(void)
{
    if (fm1_ms == 8600u && !(seen & 1u)) {
        seen |= 1u; lw_off = eng.sp.lw;
        ok_off = lp_hold && !lp_arm && eng.sp.larm && !eng.sp.lact && !eng.sp.pon;
    }
    if (fm1_ms == 8900u && !(seen & 2u)) { seen |= 2u; lw_later = eng.sp.lw; }
    if (fm1_ms == 10500u && !(seen & 4u)) { seen |= 4u; ok_on = lp_arm && !lp_hold && eng.sp.larm && eng.sp.pon; }
    if (fm1_ms == 13500u && !(seen & 8u)) { seen |= 8u; ok_clr = !lp_arm && !lp_hold && !lp_on && !eng.sp.larm; }
}
__attribute__((destructor)) static void loop_check(void)
{
    if (seen != 15u || !ok_off || lw_off != lw_later || !ok_on || !ok_clr) {
        printf("FAIL  the looper off keeps its loop: off %u (held, lent, silent), recording while off %s, on again %u "
               "(LOOP plays), cleared %u (the memory back) [seen %X]\n", ok_off, lw_off != lw_later ? "yes" : "no",
               ok_on, ok_clr, seen);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    the looper off keeps its loop: held silent, not recording; on again the loop plays; GLO + EDIT, OCT+ "
           "clears it\n");
}
