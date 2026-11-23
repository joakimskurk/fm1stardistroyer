/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* RADAR 1 on MOD (2026-11-23, the user's "RADAR 1-4"), the app on the host with tests/scripts/rd1.txt: a slot from
 * RADAR 1 to REPEAT at the list's end, ON 0 on K4 (AMOUNT's sign); the lists (RADAR 1 after RADAR among the
 * sources; its six DESTs after the matrix's, RADAR's NOTES not among them); a SOURCE turned away from RADAR 1 lets go
 * of its DEST. Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t bad;
static void want(uint32_t ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) bad = 1;
}
__attribute__((destructor)) static void radar1_check(void)
{
    uint32_t k, slot = SD_MODS, all = 1, r;
    char nm[24];
    for (k = 0; k < SD_MODS; k++)
        if (pat.mod[k].src == SD_MS_TUR1 && pat.mod[k].dst == SD_TD_TRIG + SD_TG_LREP) slot = k;
    want(slot < SD_MODS && pat.mod[slot].amt < 0 && amt99(pat.mod[slot].amt) <= -20,
         "MOD: RADAR 1 on K1, REPEAT the DEST list's last, AMOUNT up, K4 ON 0 (a minus AMOUNT)");
    want(SD_TUR_ON(&pat), "RADAR on (EDIT on its page)");
    want(src_step(SD_MS_TUR, 1) == SD_MS_TUR1 && src_step(SD_MS_TUR1, -1) == SD_MS_TUR, "RADAR 1 right after RADAR among the sources");
    for (k = 0; k < SD_TGS; k++) {              /* RADAR 1's list: the matrix's, then its six */
        all &= t1dst_of(N_DST + k) == SD_TD_TRIG + k && t1dst_rank(SD_TD_TRIG + k) == N_DST + k;
        all &= strlen(TG_NAME[k]) <= 7u && strlen(TG_PARAM[k]) + 1u <= sizeof nm;
    }
    want(all && dst_step(SD_MD_OFF, -1, SD_MS_TUR1) == SD_TD_TRIG + SD_TGS - 1u &&
         dst_step(SD_TD_TRIG + SD_TGS - 1u, 1, SD_MS_TUR1) == SD_MD_OFF, "RADAR 1's DESTs: the matrix's, then RESTART .. REPEAT, round");
    want(dst_ok(SD_MS_TUR1, SD_TD_TRIG) && !dst_ok(SD_MS_TUR1, SD_TD_NOTES) && !dst_ok(SD_MS_TUR, SD_TD_TRIG) &&
         !dst_ok(SD_MS_LFO1, SD_TD_TRIG + 3u) && dst_ok(SD_MS_LFO1, SD_MD_XM), "each source its own DESTs only");
    dlist_kind = DL_TUR1;
    r = N_DST + SD_TG_SEQB;
    want(!strcmp(dl_item(r, nm), "SEQ B") && !strcmp(nm, "PLAYS ON 1"), "the list says SEQ B, PLAYS ON 1");
    want(!strcmp(slot_dname(&(SdModSlot){SD_MS_TUR1, SD_TD_TRIG + SD_TG_ENV, 1}), "RESTART"), "the knob says RESTART");
    fflush(stdout);
    if (bad) _exit(1);
}
