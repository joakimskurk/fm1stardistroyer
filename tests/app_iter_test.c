/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* What the user asked for on 2026-11-23, after playing the FM-1, each checked where it is made (the app on the host,
 * after tests/run_host.sh's default script, so the engine is the app's):
 *   TIME and LENGTH synced turn from what plays: down always shorter, up always longer, never past the line
 *   SEQ A and B as MOD sources: to FRZ PITCH and to PITCH at 100 % the track's note from its root, exactly
 *   a SLIDE step's TIME: 1 as before (~30 ms), 8 about a second
 *   RADAR's DIV turned while it plays: its next step on the new division's grid
 *   CLICK: a tick from the frame the beat falls on, none when it is off
 * Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t bad;
static void want(uint32_t ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) bad = 1;
}
static uint32_t played(uint32_t d, uint32_t lim)  /* a synced division's length as the engine plays it */
{
    uint32_t f;
    return sd_space_sync_q8(eng.tempo_q8, d, &f, lim);
}
__attribute__((destructor)) static void iter_check(void)
{
    static SdEngine e2;
    const uint32_t L = 1411200u;                /* (a sixteenth at 120 BPM, samples x 256) */
    uint32_t d, ok, k, t1, t8;
    eng.tempo_q8 = L;
    {                                           /* TIME, the looper armed (a 0.5 s line): from 1/2 down, from 1/32 up */
        uint32_t lim = SD_SP_LIM2, was = played(11u, lim);
        ok = 1;
        for (d = div_turn(11u, -1, SD_SP_NDIV, lim), k = 0; k < 11u; k++) {
            uint32_t now = played(d, lim);
            if (now >= was) ok = 0;
            was = now;
            if (!d) break;
            d = div_turn(d, -1, SD_SP_NDIV, lim);
        }
        want(ok, "TIME turned down from 1/2 (it plays 1/4 on the looper's half line): shorter every detent");
        ok = 1;
        for (d = 0, was = played(0u, lim), k = 0; k < 14u; k++) {
            uint32_t n = div_turn(d, 1, SD_SP_NDIV, lim), f;
            sd_space_sync_q8(eng.tempo_q8, n, &f, lim);
            if (f || (n != d && played(n, lim) <= was)) ok = 0;
            if (n == d) break;
            was = played(n, lim);
            d = n;
        }
        want(ok && d == 8u, "TIME turned up from 1/32: longer every detent, stopping at 1/4 (the longest that fits 0.5 s)");
    }
    {                                           /* SEQ A, B: the note from the key's root, exact at 100 % */
        e2 = eng;
        memset(e2.patch.mod, 0, sizeof e2.patch.mod);
        memset(e2.patch.mod_off, 0, sizeof e2.patch.mod_off);
        e2.t[0].seq_on = 1; e2.t[0].seq_note[0] = 67; e2.t[0].pat.step[0].note = 60; e2.t[0].pat.root = 0;
        e2.t[1].seq_on = 1; e2.t[1].seq_note[0] = 43; e2.t[1].pat.step[0].note = 48; e2.t[1].pat.root = 0;
        e2.patch.mod[0].src = SD_MS_SEQA; e2.patch.mod[0].dst = SD_MD_SPITCH; e2.patch.mod[0].amt = 32767;
        e2.patch.mod[1].src = SD_MS_SEQB; e2.patch.mod[1].dst = SD_MD_PITCH; e2.patch.mod[1].amt = 32767;
        sd_mod_tick(&e2);
        want(e2.sp_mod == 7 && e2.psemis == -5 * 256, "SEQ A to FRZ PITCH and SEQ B to PITCH at 100 %: +7 and -5 semitones, exactly");
        e2.patch.mod[0].amt = 16384;
        sd_mod_tick(&e2);
        want(e2.sp_mod == 4 || e2.sp_mod == 3, "... at 50 %: half as far (3.5, whole semitones)");
    }
    {                                           /* SLIDE's TIME: ticks from an octave below to within 16 of it */
        uint32_t g;
        for (g = 1; g <= 8u; g += 7u) {
            uint32_t n = 0;
            e2 = eng;
            e2.voice[0].inc = 20000000u; e2.voice[0].inc_tgt = 40000000u; e2.voice[0].glide = (uint8_t)g;
            while (e2.voice[0].glide && n < 5000u) { sd_control_tick(&e2); n++; }   /* (the engine's own tick) */
            if (g == 1u) t1 = n; else t8 = n;
        }
        printf("      SLIDE 30 MS: %u ticks (%.0f ms to the last 16), 1 S: %u ticks (%.0f ms)\n", t1, t1 * 0.7256, t8, t8 * 0.7256);
        want(t1 * 0.7256 < 120.0 && t8 * 0.7256 > 900.0, "a SLIDE's TIME: 1 fast as before, 8 about a second");
    }
    {                                           /* RADAR: DIV 1/16 -> 1/8 at a step's middle, its place from the grid */
        e2 = eng;
        e2.patch.tur_set = 0x80u | 50u;
        e2.patch.tur_ld = (uint8_t)((2u + 1u) << 5 | 8u);   /* (1/16, LENGTH 8) */
        e2.seq_run = 1; e2.seq_len_q8 = L; e2.clk_adv = SD_SUB << 8;
        e2.tur_div_was = 2u; e2.tur_pos_q8 = 0;
        sd_tur_tick(&e2, 0);
        e2.patch.tur_ld = (uint8_t)((4u + 1u) << 5 | 8u);   /* (1/8) */
        e2.grid_q8 = 5u * L + 300u * 256u; e2.grid_adv = 0;
        sd_tur_tick(&e2, 0);
        want(e2.tur_pos_q8 == (5u * L + 300u * 256u) % (2u * L) + (SD_SUB << 8),
             "RADAR's DIV turned while it plays: its place in the new step from the grid (on the beat)");
    }
    {                                           /* CLICK: a beat 100 frames into the block, the bar's one */
        static int32_t out[512];
        uint32_t i, before = 0, after = 0;
        e2 = eng;
        memset(out, 0, sizeof out);
        click_want = 1; eng.seq_run = 1; eng.seq_len_q8 = L; eng.grid_adv = SD_SUB << 8;
        eng.grid_q8 = 4u * L - 100u * 256u + 256u * 256u - (SD_SUB << 8);   /* (the block's start 100 frames before beat 4) */
        demo_st = 0; clk_left = 0;
        click_add(out, 256u);
        for (i = 0; i < 100u; i++) before |= (uint32_t)(out[2u * i] != 0);
        for (i = 101; i < 256u; i++) after |= (uint32_t)(out[2u * i] != 0 && out[2u * i] == out[2u * i + 1u]);
        want(!before && after, "CLICK: silent before the beat, a tick from its frame, both sides");
        memset(out, 0, sizeof out);
        click_want = 0; clk_left = 0;
        click_add(out, 256u);
        for (i = 0, after = 0; i < 512u; i++) after |= (uint32_t)(out[i] != 0);
        want(!after, "CLICK off: nothing");
        eng = e2;
    }
    fflush(stdout);
    if (bad) _exit(1);
}
