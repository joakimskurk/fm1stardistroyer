/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* The harmonic generators (Sound Plan §3.7), run by the main loop on a patch it then
 * hands to the engine (sd_engine_patch). They set the harmonics (and clear the fine tune) of a bank's four
 * oscillators; levels and roles stay as they were.
 *   ALL    1 2 3 4                 ODD    1 3 5 7
 *   RND    four random harmonics 1..32
 *   CLUST  a random formant cluster: four neighbours around a random centre
 *   CHRD   one of 14 chords, in just intonation: the harmonics' own ratios (4:5:6 is a major triad)
 *   CPY    this bank to the other three
 * The chords are fixed harmonic sets, as the harmonics 1..32 allow. */
#ifndef SD_GEN_H
#define SD_GEN_H
#include "sd_engine.h"

#define SD_CHORDS 14u
static const char *const SD_CHORD_NAME[SD_CHORDS] = {"MAJ", "MIN", "SUS2", "SUS4", "DOM7", "MAJ7", "MIN7", "MAJ6",
                                                     "MIN6", "ADD9", "FIFTH", "OCT", "DIM", "AUG"};
static const uint8_t SD_CHORD[SD_CHORDS][SD_OPS] = {
    {4, 5, 6, 8}, {10, 12, 15, 20}, {8, 9, 12, 16}, {6, 8, 9, 12}, {4, 5, 6, 7}, {8, 10, 12, 15}, {10, 12, 15, 18},
    {12, 15, 18, 20}, {10, 12, 15, 17}, {8, 10, 12, 18}, {2, 3, 4, 6}, {1, 2, 4, 8}, {10, 12, 14, 20}, {16, 20, 25, 32}};

/* the list the player steps through: ALL ODD RND CLUST, the chords, CPY */
#define SD_GENS (4u + SD_CHORDS + 1u)
static const char *sd_gen_name(uint32_t g)
{
    static const char *const FIRST[4] = {"ALL", "ODD", "RND", "CLUST"};
    return g < 4u ? FIRST[g] : g < 4u + SD_CHORDS ? SD_CHORD_NAME[g - 4u] : "CPY";
}

static uint32_t sd_rand(uint32_t *s)            /* xorshift32: never 0 once seeded non-0 */
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}

static void sd_set_harms(SdBankPatch *bp, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    bp->osc[0].harm = (uint8_t)a; bp->osc[1].harm = (uint8_t)b; bp->osc[2].harm = (uint8_t)c; bp->osc[3].harm = (uint8_t)d;
    bp->osc[0].fine = bp->osc[1].fine = bp->osc[2].fine = bp->osc[3].fine = 0;
}

/* generator g (0..SD_GENS-1) on bank b of p */
static void sd_gen(SdPatch *p, uint32_t b, uint32_t g, uint32_t *seed)
{
    SdBankPatch *bp = &p->bank[b & 3u];
    uint32_t k;
    if (g == 0u) sd_set_harms(bp, 1, 2, 3, 4);
    else if (g == 1u) sd_set_harms(bp, 1, 3, 5, 7);
    else if (g == 2u) {
        sd_set_harms(bp, 1, 1, 1, 1);
        for (k = 0; k < SD_OPS; k++) bp->osc[k].harm = (uint8_t)(1u + sd_rand(seed) % 32u);
    } else if (g == 3u) {                       /* a centre 2..30, its neighbours -1 0 +1 +2, inside 1..32 */
        uint32_t c = 2u + sd_rand(seed) % 29u;
        sd_set_harms(bp, c - 1u, c, c + 1u, c + 2u > 32u ? c - 2u : c + 2u);
    } else if (g < 4u + SD_CHORDS) {
        const uint8_t *h = SD_CHORD[g - 4u];
        sd_set_harms(bp, h[0], h[1], h[2], h[3]);
    } else {
        for (k = 0; k < SD_BANKS; k++)
            if (k != (b & 3u)) p->bank[k] = *bp;
    }
}

#endif
