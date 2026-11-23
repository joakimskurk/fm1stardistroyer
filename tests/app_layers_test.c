/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/lay1.txt (2026-11-22: the knobs' layers on ALGORITHM on every page, GLO's
 * hidden ones gone; the user: "make sure no function disappears"): every page, every layer ALGORITHM gives it, each
 * knob a detent up and back. Fails where a layer was never shown or a knob it names never moved what it shows (its
 * value, its bar, its lamp). Run by tests/run_tests.sh */
#include <unistd.h>
#define HOST_POLL_HOOK lay_poll
#include "stardistroyer.c"
enum { LK_PAGES = 20, LK_LAYS = 8 };
static const char *const LK_NAME[LK_PAGES] = {"NAV", "REACTOR", "DISTROY", "SPACE", "", "MACROS", "SEQ", "ENV", "FILTER",
    "ARP", "ORBIT", "TIME", "LOOPER", "SETUP", "DRUM", "SUB", "LFO 1", "LFO 2", "RADAR", "MOD"};
static uint32_t first[LK_PAGES][LK_LAYS][4], moved[LK_PAGES][LK_LAYS], named[LK_PAGES][LK_LAYS], seen[LK_PAGES];
static char nm0[LK_PAGES][LK_LAYS][4][12];
static uint32_t lay_key(void)                   /* the page, LFO's four apart */
{
    if (page != 4u) return page;
    return dmod == DMOD_TUR ? 18u : dmod >= SD_MODS ? 16u + (dmod - SD_MODS) : 19u;
}
static uint32_t kn_hash(const Kn *k)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; k->v[i] && i < sizeof k->v; i++) h = (h ^ (uint8_t)k->v[i]) * 16777619u;
    h = (h ^ (uint32_t)(uint16_t)k->bar) * 16777619u;
    h = (h ^ (uint32_t)k->hot << 8 ^ (uint32_t)k->kind << 16) * 16777619u;
    return h | 0x80000000u;                     /* (never 0: 0 is "not yet seen") */
}
static void lay_poll(void)
{
    uint32_t p = lay_key(), l = lay(), i;
    if (p >= LK_PAGES || l >= LK_LAYS || fm1_ms < 1700u) return;
    seen[p] |= 1u << l;
    knobs_of();
    for (i = 0; i < 4u; i++) {
        uint32_t h;
        if (!kn[i].nm) continue;
        h = kn_hash(&kn[i]);
        named[p][l] |= 1u << i;
        if (!first[p][l][i]) { first[p][l][i] = h; snprintf(nm0[p][l][i], sizeof nm0[p][l][i], "%s", kn[i].nm); }
        else if (h != first[p][l][i]) moved[p][l] |= 1u << i;
    }
}
__attribute__((destructor)) static void lay_check(void)
{
    static const uint8_t PG[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    uint32_t i, l, k, bad = 0, layers = 0, knobs = 0;
    for (i = 0; i < sizeof PG; i++) {
        uint32_t p = PG[i], n, want;
        const char *const *ln;
        page = p < 16u ? p : 4u;
        dmod = p == 18u ? DMOD_TUR : p == 19u ? 0u : p >= 16u ? SD_MODS + (p - 16u) : dmod;
        n = lay_names(&ln);
        want = (1u << n) - 1u;
        if ((seen[p] & want) != want) { printf("FAIL  %s: layers shown %x of %x\n", LK_NAME[p], seen[p], want); bad = 1; }
        for (l = 0; l < n; l++) {
            layers++;
            if (p == 13u && l == LU_TOOLS) continue;   /* (SETUP's TOOLS: drawn by its picture, they ask; the demo
                                                   test plays them) */
            if (!named[p][l]) { printf("FAIL  %s %s: no knob\n", LK_NAME[p], ln ? ln[l] : "-"); bad = 1; }
            for (k = 0; k < 4u; k++) {
                if (!(named[p][l] >> k & 1u)) continue;
                knobs++;
                if (!(moved[p][l] >> k & 1u)) {
                    printf("FAIL  %s %s K%u %s: a turn showed no change\n", LK_NAME[p], ln ? ln[l] : "-", k + 1u, nm0[p][l][k]);
                    bad = 1;
                }
            }
        }
    }
    if (bad) { fflush(stdout); _exit(1); }
    printf("ok    every page's every layer (%u) shown, every knob named there (%u) moved by a turn\n", layers, knobs);
}
