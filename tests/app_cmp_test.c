/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The workshop's (2026-11-27) on the host with tests/scripts/cmp1.txt: the lamp when the sound is changed, COMPARE
 * (GLO + SAVE: the saved one heard, a turn back to yours, OCT+ keeps the saved one), INIT from PRESETS (NEW: one
 * sine), CHOP on LOOPER's SLICE (a MOD slot RADAR -> L-START, RADAR on), PANIC (GLO + PLAY). Run by
 * tests/run_tests.sh */
#include <unistd.h>
#include <string.h>
#define HOST_POLL_HOOK cmp_poll
#include "stardistroyer.c"
enum { CP_N = 9 };
static const uint32_t CP_AT[CP_N] = {1850, 2050, 2200, 2450, 2600, 2750, 2900, 3400, 3900};
static struct { uint32_t ed, cmp, heard_ref, init, browse, chop, slot, radar, page, lay, run, s1, s2; char msg[40]; } cp[CP_N];
static uint32_t run_at_play;
static void cmp_poll(void)
{
    uint32_t i;
    if (fm1_ms == 3600u) run_at_play = eng.seq_run;
    for (i = 0; i < CP_N; i++)
        if (fm1_ms == CP_AT[i]) {
            cp[i].ed = pat_edited(); cp[i].cmp = cmp_on; cp[i].heard_ref = !memcmp(&eng.patch_in, &pat_ref, sizeof pat);
            cp[i].init = pat_init; cp[i].browse = browse_at ? browse_to : 0xFFFFu; cp[i].chop = chop_n();
            cp[i].slot = chop_slot(); cp[i].radar = SD_TUR_ON(&pat); cp[i].page = page; cp[i].lay = lay();
            cp[i].run = eng.seq_run;
            cp[i].s1 = pat.bank[0].osc[0].level; cp[i].s2 = pat.bank[0].osc[1].level;
            snprintf(cp[i].msg, sizeof cp[i].msg, "%s", msg_txt ? msg_txt : "");
        }
}
#define CP_CHECK(c, ...) do { if (!(c)) { printf("FAIL  CMP: " __VA_ARGS__); printf("\n"); bad = 1; } } while (0)
__attribute__((destructor)) static void cmp_check(void)
{
    uint32_t bad = 0;
    CP_CHECK(cp[0].ed && !cp[0].cmp, "MUTATE +5: the lamp %u, COMPARE %u", cp[0].ed, cp[0].cmp);
    CP_CHECK(cp[1].cmp && cp[1].heard_ref && cp[1].ed, "GLO + SAVE: COMPARE %u, the saved one heard %u", cp[1].cmp, cp[1].heard_ref);
    CP_CHECK(!cp[2].cmp && !cp[2].heard_ref && !strcmp(cp[2].msg, "COMPARE OFF: YOURS"), "a turn: COMPARE %u, said '%s'",
             cp[2].cmp, cp[2].msg);
    CP_CHECK(cp[3].cmp, "GLO + SAVE again: COMPARE %u", cp[3].cmp);
    CP_CHECK(!cp[4].cmp && !cp[4].ed && cp[4].heard_ref && !strcmp(cp[4].msg, "THE SAVED ONE KEPT"),
             "OCT+: the saved one kept (changed %u, said '%s')", cp[4].ed, cp[4].msg);
    CP_CHECK(cp[5].browse == INIT_P, "PRESETS -1 from P01: %u, not INIT (%u)", cp[5].browse, INIT_P);
    CP_CHECK(cp[6].init && cp[6].s1 == 65535u && !cp[6].s2 && !cp[6].ed, "INIT loaded: %u, A1 %u A2 %u, changed %u",
             cp[6].init, cp[6].s1, cp[6].s2, cp[6].ed);
    CP_CHECK(cp[7].page == PAGE_LOOP && cp[7].lay == LL_SLICE && cp[7].chop == 4u && cp[7].slot < SD_MODS && cp[7].radar,
             "SLICE's CHOP +4: page %u layer %u, CHOP %u, slot %u, RADAR %u", cp[7].page, cp[7].lay, cp[7].chop, cp[7].slot,
             cp[7].radar);
    CP_CHECK(run_at_play && !cp[8].run && !strcmp(cp[8].msg, "PANIC: ALL QUIET"), "GLO + PLAY: playing %u, then %u, said '%s'",
             run_at_play, cp[8].run, cp[8].msg);
    if (bad) { fflush(stdout); _exit(1); }
    printf("ok    COMPARE (the lamp, the saved one heard, a turn back to yours, OCT+ keeps it), INIT from PRESETS, "
           "SLICE's CHOP (RADAR to START), PANIC on GLO + PLAY\n");
}
