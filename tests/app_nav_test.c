/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* NAV on the host with tests/scripts/nav1.txt (2026-11-26, the user: "rethink NAV so it cannot be misunderstood",
 * "what is not available greyed, and why said when you try"): SELECT on the oscillator, a grey knob that still sets
 * and says why, LEVEL opening an oscillator, a routing heard again, ROUND kept in the COURSE byte as EDIT steps it,
 * COURSE's point, GEN on TOOLS, an engine that is off saying so. Run by tests/run_tests.sh */
#include <unistd.h>
#include <string.h>
#define HOST_POLL_HOOK nav_poll
#include "stardistroyer.c"
enum { NV_N = 10 };
static const uint32_t NV_AT[NV_N] = {2200, 2400, 2600, 2900, 3300, 3500, 3700, 3900, 4100, 4300};
static struct { uint32_t page, lay, bank, osc, grey1, harm, level, heard, course, cell, dl; uint8_t h[4]; char msg[40], k2[12]; } nv[NV_N];
static uint32_t off_seen, off_ok;
static char off_msg[40];
static void nav_poll(void)
{
    uint32_t i, k;
    if (fm1_ms == 4700u) {                      /* REACTOR: off, or on (then nothing to say) */
        off_seen = page_off();
        snprintf(off_msg, sizeof off_msg, "%s", msg_txt ? msg_txt : "");
        off_ok = !off_seen || !strcmp(off_msg, "REACTOR OFF: EDIT TURNS IT ON");
    }
    for (i = 0; i < NV_N; i++)
        if (fm1_ms == NV_AT[i]) {
            uint8_t lv[SD_OPS];
            knobs_of();
            nv[i].page = page; nv[i].lay = lay(); nv[i].bank = ebank; nv[i].osc = eosc; nv[i].grey1 = (kn_why[0] != 0) | (kn_why[1] != 0) << 1 | (kn_why[3] != 0) << 3;
            nv[i].harm = pat.bank[ebank].osc[eosc].harm; nv[i].level = pat.bank[ebank].osc[eosc].level;
            nv[i].heard = sd_bank_live(&pat.bank[ebank], 1u, lv); nv[i].course = pat.course; nv[i].cell = ecell; nv[i].dl = dlist_kind;
            for (k = 0; k < 4u; k++) nv[i].h[k] = pat.bank[ebank].osc[k].harm;
            snprintf(nv[i].msg, sizeof nv[i].msg, "%s", msg_txt ? msg_txt : "");
            snprintf(nv[i].k2, sizeof nv[i].k2, "%s", kn[2].nm ? kn[2].nm : "");
        }
}
#define NV_CHECK(c, ...) do { if (!(c)) { printf("FAIL  NAV: " __VA_ARGS__); printf("\n"); bad = 1; } } while (0)
__attribute__((destructor)) static void nav_check(void)
{
    uint32_t bad = 0;
    NV_CHECK(nv[0].page == 0u && nv[0].lay == LN_BANK && nv[0].bank == 0u && nv[0].osc == 1u && (nv[0].grey1 & 2u) && !nv[0].level,
             "SELECT +1 on BANK: A2 at 0, HARM grey (page %u layer %u, %c%u, grey %x)", nv[0].page, nv[0].lay, 'A' + nv[0].bank, nv[0].osc + 1u, nv[0].grey1);
    NV_CHECK(nv[1].harm == 2u && !strcmp(nv[1].msg, "A2 AT 0: TURN LEVEL UP"), "HARM on A2 at 0: x%u, said '%s'", nv[1].harm, nv[1].msg);
    NV_CHECK(nv[2].level > 0u && !(nv[2].grey1 & 2u), "LEVEL up: A2 %u, HARM grey %x", nv[2].level, nv[2].grey1);
    NV_CHECK(nv[3].osc == 0u && pat.bank[0].osc[0].role == SD_ROLE_S && nv[3].heard, "ROUTING SCCC with A2 open: heard %x", nv[3].heard);
    NV_CHECK(nv[4].lay == LN_XY && SD_COURSE_ROUND(&(SdPatch){.course = (uint8_t)nv[4].course}) == 2u && !SD_COURSE_MODE(&(SdPatch){.course = (uint8_t)nv[4].course}) && (nv[4].grey1 & 8u),
             "ROUND +2: 1 BAR, grey with COURSE off (course %02x, grey %x)", nv[4].course, nv[4].grey1);
    NV_CHECK(nv[5].course == (SD_COURSE_ON | 2u << 4) && (nv[5].grey1 & 1u), "EDIT: COURSE ON, ROUND kept, X grey (course %02x, grey %x)", nv[5].course, nv[5].grey1);
    NV_CHECK(!strcmp(nv[6].msg, "X/Y: COURSE MOVES IT (EDIT)"), "X turned while COURSE runs: said '%s'", nv[6].msg);
    NV_CHECK(nv[7].lay == LN_COURSE && !strcmp(nv[7].k2, "TIME") && nv[7].cell == 0u, "the COURSE layer: K3 '%s', point %u", nv[7].k2, nv[7].cell + 1u);
    NV_CHECK(nv[8].cell == 1u, "POINT +1: point %u", nv[8].cell + 1u);
    NV_CHECK(nv[9].lay == LN_TOOLS && nv[9].dl == DL_GEN && nv[9].h[0] == 1u && nv[9].h[1] == 2u && nv[9].h[2] == 3u && nv[9].h[3] == 4u,
             "TOOLS' GEN ALL: %u %u %u %u (list %u)", nv[9].h[0], nv[9].h[1], nv[9].h[2], nv[9].h[3], nv[9].dl);
    NV_CHECK(off_ok, "REACTOR off: a knob said '%s'", off_msg);
    {                                           /* ROUTING always plays (2026-11-26): every factory patch, every bank,
                                                   every routing from the bank as it was: heard, its carriers' power
                                                   kept (within 1 dB, the levels' ceiling aside), modulators open, XM
                                                   up where there are modulators; turning back gives the bank back */
        uint32_t pi, b, a, k, n = 0, quiet = 0, deaf = 0, back = 0;
        for (pi = 0; pi < SD_PATCHES; pi++)
            for (b = 0; b < SD_BANKS; b++)
                for (a = 0; a < SD_ALGOS; a++) {
                    uint8_t lv[SD_OPS];
                    uint32_t q0, q1, a0, x0;
                    pat = SD_PATCH[pi]; ebank = b; rt_snap = pat.bank[b]; rt_snap_b = b; rt_xm = x0 = pat.xm;
                    q0 = snap_q(); a0 = algo_of(&pat);
                    routing_apply(a);
                    q1 = car_q(&pat.bank[b]);
                    n++;
                    if (!sd_bank_live(&pat.bank[b], sd_clamp16((int32_t)pat.xm + 2 * (int32_t)pat.bank[b].xm) > 0, lv)) deaf++;
                    else for (k = 0; k < SD_OPS; k++)
                        if (pat.bank[b].osc[k].role != SD_ROLE_C && !lv[k]) { deaf++; break; }
                    if (q1 * 112u < q0 * 100u) {   /* (more than 1 dB under: only where a carrier sits at the ceiling) */
                        uint32_t top = 0;
                        for (k = 0; k < SD_OPS; k++) top |= pat.bank[b].osc[k].role == SD_ROLE_C && pat.bank[b].osc[k].level == 65535u;
                        if (!top) quiet++;
                    }
                    if (a0 < SD_ALGOS) {        /* (and back to where it began: the bank as it was) */
                        routing_apply(a0);
                        if (pat.xm != x0) back++;   /* (XM back too: 2026-11-26, a sound lost to XM 30) */
                        if (memcmp(&pat.bank[b], &rt_snap, sizeof rt_snap) && car_q(&rt_snap)) {
                            uint32_t same = 1;
                            for (k = 0; k < SD_OPS; k++) same &= pat.bank[b].osc[k].harm == rt_snap.osc[k].harm && pat.bank[b].osc[k].role == rt_snap.osc[k].role;
                            if (!same) back++;
                        }
                    }
                }
        NV_CHECK(!deaf && !quiet && !back, "ROUTING always plays: %u of %u silent or a modulator unheard, %u quieter, %u not back", deaf, n, quiet, back);
        {                                       /* GEN's chord on CORE's bank A (one oscillator on): all four open */
            pat = SD_PATCH[SD_P_CORE]; ebank = 0; rt_snap = pat.bank[0]; rt_snap_b = 0; rt_xm = pat.xm;
            gen_apply(4u);                      /* (MAJ: 4 5 6 8) */
            for (k = 0; k < SD_OPS; k++) if (!pat.bank[0].osc[k].level) break;
            NV_CHECK(k == SD_OPS && pat.bank[0].osc[0].harm == 4u && pat.bank[0].osc[3].harm == 8u, "GEN MAJ on CORE: oscillator %u still off", k + 1u);
        }
        if (!deaf && !quiet && !back) printf("ok    ROUTING plays every one of %u bank x routing: heard, its power kept, back where it began\n", n);
    }
    if (bad) { fflush(stdout); _exit(1); }
    printf("ok    NAV: the oscillator SELECT is on, grey knobs that set and say why, ROUND kept, COURSE's point, GEN on TOOLS%s\n",
           off_seen ? ", an engine off says so" : "");
}
