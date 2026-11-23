/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/lp1.txt: the looper switched off keeps its loop (2026-11-21, the user: off
 * from HOME it crackled and the loop was gone), and its time (2026-11-23, the user: off and on again, the loop was
 * gone and out of time). At 8.6 s (off): held, the memory still lent, LOOP silent and nothing recorded (its write head
 * where it stopped), its head still going round as the clock goes; at 10.5 s (on again): the same loop (its end where
 * it was) playing; at 13.5 s (cleared, asked): the memory back to the delay. Run by tests/run_tests.sh */
#define HOST_POLL_HOOK loop_poll
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t lw_off, lw_later, ok_off, ok_on, ok_clr, seen, base0, pk_off, run_ok;
static void loop_poll(void)
{
    if (fm1_ms == 6900u && !(seen & 16u)) { seen |= 16u; base0 = eng.sp.lbase; }
    if (fm1_ms == 8600u && !(seen & 1u)) {
        seen |= 1u; lw_off = eng.sp.lw; pk_off = eng.sp.pk;
        ok_off = lp_hold && !lp_arm && eng.sp.larm && eng.sp.pon && eng.sp.plev == 0 && eng.sp.lbase == base0;
    }
    if (fm1_ms == 8900u && !(seen & 2u)) {      /* (300 ms on: its head 300 ms round, at x 1) */
        uint32_t ln = eng.sp.pln, d = ln ? (eng.sp.pk + ln - pk_off % ln) % ln : 0u, want = 300u * 22050u / 1000u * 256u % (ln ? ln : 1u);
        int32_t e = (int32_t)d - (int32_t)want;
        seen |= 2u; lw_later = eng.sp.lw;
        run_ok = ln && e < 40000 && e > -40000;   /* (the host's sound comes a block at a time: 128 samples) */
    }
    if (fm1_ms == 10500u && !(seen & 4u)) {
        seen |= 4u; ok_on = lp_arm && !lp_hold && eng.sp.larm && eng.sp.pon && eng.sp.plev > 0 && eng.sp.lbase == base0;
    }
    if (fm1_ms == 13500u && !(seen & 8u)) { seen |= 8u; ok_clr = !lp_arm && !lp_hold && !lp_on && !eng.sp.larm; }
}
__attribute__((destructor)) static void loop_check(void)
{
    if (seen != 31u || !ok_off || lw_off != lw_later || !run_ok || !ok_on || !ok_clr) {
        printf("FAIL  the looper off keeps its loop: off %u (held, lent, silent), recording while off %s, its head on "
               "time %u, on again %u (the same loop plays), cleared %u (the memory back) [seen %X]\n", ok_off,
               lw_off != lw_later ? "yes" : "no", run_ok, ok_on, ok_clr, seen);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    the looper off keeps its loop: held silent, not recording, its head on time; on again the same loop "
           "plays; TOOLS' CLEAR, OCT+ clears it\n");
}
