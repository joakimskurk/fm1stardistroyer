/* SPDX-License-Identifier: GPL-3.0-only */
/* RANGE: a demo of what one FM-1 running Star Distroyer can do, played as one person would play it (2026-11-07, the
 * user: "a new demo, much more versatile, showing the instrument's span; not a long cinematic piece but something
 * that evolves through everything this machine can do; nothing that the synth on the FM-1 cannot do itself").
 *
 *   sd_range OUT.wav
 *
 * One engine, one patch at a time (stock patches, loaded with PRESETS), the app's controls only: a knob turns on the
 * page it belongs to and no faster than a hand turns an encoder (a detent at a time, at most ~60 a second), keys
 * among the panel's 27 at velocity 100, the sequencer's two tracks, TURING, the transpose (SCL + a key), the
 * arpeggiator, the macros, EDIT's switches, the tempo, GLO's modes. The app's voice rules hold (no ARP while SEQ A
 * plays in MONO, SEQ B only in DUO and POLY, ARP only where a voice is left, keys only where one is free): the tool
 * checks them every block and stops if a gesture could not be played. Patterns are ones the player saved before
 * (PRESETS on SEQ loads them). The output is what the FM-1 plays (and sends over USB): the engine's, through the
 * MASTER pot as platform.c applies it; the player rides MASTER in the bars between scenes (the patches' levels
 * differ by ~10 dB). No other mix.
 *
 * Seven scenes at 112 BPM (124 in the sixth), each a pattern and a patch, a bar between them to stop, load the next
 * patch and pattern, change the mode, and start again:
 *   1 NAV   VOYAGE, MONO: COURSE moving the nav on every note; WARP and XM up, FOLD in, a harmonic generator
 *              re-rolling bank A on each bar's one
 *   2 FILTER   GROWL, MONO: an acid line (SLIDE, accents, a ratchet, PROB, swing); the lowpass switched on, LP24,
 *              closed with RES, ENV 3 made short, then TURING on (the line evolves at each one) and the line
 *              transposed up a fourth and a minor third at the ones; the filter opens to end
 *   3 REACTOR    VOWEL, DUO: a 4-bar melody on SEQ A, a 1-bar bass on SEQ B transposed under it (D D Bb C) at its
 *              ones; the Reactor's POS through the vowels, its MODEL changed
 *   4 DESTROY  DISTROY, DUO: a riff in four segments (one in random order, one ping-pong), a ratchet, PROB; BREAK
 *              and MOVE up, then the top band's GAIN destroyed
 *   5 POLY     CHORDS, POLY: three-note chords on SEQ A, the arpeggiator on top (HOLD, two octaves), A transposed up a
 *              tone at the fifth bar with a new hand for the arp; UP-DN, three octaves, 1/32, RND
 *   6 TURING   ORBITAL, MONO: a 16th ostinato that TURING rewrites (CHANGE 35, RANGE 5), MOVE stepping the nav
 *              around the field's middle, MUTATE, the tempo pushed to 124, a bar at 1/32
 *   7 SPACE    BEACON, MONO: long notes into the delay and the room, FDBK and DECAY up; the sequence stops, a held
 *              key, the echoes left to fall */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"

/* ---- the patterns the player saved (PRESETS on SEQ) */
static SdPattern A1, A2, A3, B3, A4, B4, A5, A6, A7;
static void patterns(void)
{
    static const int16_t a1[16] = {62 | AC, RS, 69, RS, 65, RS, 72, 70, 69 | AC, RS, 65, RS, 62, RS, 64, 65};
    static const int16_t a2[16] = {38 | AC, 38, 50, 38 | AC, 41, 43 | SL, 38 | AC, 50, 38, 36, 38 | AC, 48, 50 | SL,
                                   41 | AC, 45, 48 | SL};
    static const int16_t a3[64] = {
        69 | AC, TI, TI, TI, 65, TI, 67, TI, 69, TI, 72, TI, 69, TI, TI, TI,
        67 | AC, TI, TI, TI, 65, TI, 64, TI, 65, TI, TI, TI, TI, TI, RS, RS,
        74 | AC, TI, TI, TI, 72, TI, 69, TI, 70, TI, TI, TI, 69, TI, 67, TI,
        69 | AC, TI, TI, TI, TI, TI, TI, TI, RS, RS, RS, RS, 64, TI, 65, TI};
    static const int16_t b3[16] = {38 | AC, RS, 50, RS, 38, 38, RS, 48, 38 | AC, RS, 45, RS, 38, RS, 48, 50 | SL};
    static const int16_t a4[32] = {50 | AC, RS, 50, 62, RS, 50, 60, RS,    50 | AC, RS, 53, RS, 55, 57, RS, 60,
                                   62 | AC, 62, RS, 60, RS, 57, 55, RS,    50 | AC, RS, 48, RS, 46, RS, 45, RS};
    static const int16_t b4[16] = {38 | AC, RS, 38, 50, RS, 38, RS, 38, 38 | AC, RS, 50, RS, 36, RS, 41, RS};
    static const int16_t a6[16] = {62 | AC, 57, 65, 57, 62, 57, 69, 57, 62 | AC, 57, 65, 57, 64, 57, 60, 57};
    static const int16_t a7[32] = {62 | AC, TI, TI, TI, TI, TI, TI, TI, 65, TI, TI, TI, 64, TI, TI, TI,
                                   62, TI, TI, TI, TI, TI, TI, TI, 57, TI, TI, TI, TI, TI, RS, RS};
    static const uint8_t root[4] = {50, 46, 53, 48};          /* Dm Bb F C */
    static const int8_t third[4] = {3, 4, 4, 4};
    uint32_t b, k;
    line(&A1, a1, 16, 70);
    line(&A2, a2, 16, 45);
    A2.swing = 56;
    A2.step[9].ratchet = 2;
    A2.step[14].prob = 60;
    line(&A3, a3, 64, 92);
    line(&B3, b3, 16, 60);
    line(&A4, a4, 32, 50);
    A4.seg_count = 4; A4.seg_split[0] = 8; A4.seg_split[1] = 16; A4.seg_split[2] = 24;
    A4.seg_order[2] = SD_SO_RND; A4.seg_order[3] = SD_SO_PP;
    A4.step[16].ratchet = 3; A4.step[20].prob = 60;
    line(&B4, b4, 16, 40);
    sd_pattern_default(&A5);                    /* chords: a bar each, struck on the beat and the half */
    A5.len = 64; A5.seg_count = 1;
    for (b = 0; b < 4u; b++)
        for (k = 0; k < 16u; k++) {
            SdStep *s = &A5.step[b * 16u + k];
            s->gate = 95; s->vel = k ? 84 : 110; s->ratchet = 1; s->prob = 100;
            s->type = k == 0u || k == 8u ? SD_ST_NOTE : k == 7u || k == 15u ? SD_ST_REST : SD_ST_TIE;
            s->note = root[b];
            s->chord[0] = third[b]; s->chord[1] = 7; s->chord[2] = 0;
        }
    line(&A6, a6, 16, 40);
    line(&A7, a7, 32, 95);
}

/* ---- the scenes: s, the sixteenths since the scene's PLAY; a bar between scenes (s 128..143 of the one before) */
static void s1_nav(double s0)
{
#define S(x) (s0 + (x))
    if (at(S(0))) { play(1); }
    KNOB99(PG_NAV, pat.warp, 40, S(2), S(64));
    KNOB99(PG_NAV, pat.xm, 70, S(32), S(62));
    KNOB99(PG_NAV, pat.fold, 22, S(66), S(124));
    {                                           /* SCL + K4: a generator a detent on bank A, each bar's one */
        static const double G[4] = {64, 80, 96, 112};
        int i;
        for (i = 0; i < 4; i++)
            if (at(S(G[i]) - 0.5)) {
                char t[48];
                on_page(PG_NAV, "GEN off NAV");
                gen = gen >= SD_GENS ? 0u : (gen + 1u) % SD_GENS;
                gen_seed += frame;
                sd_gen(&pat, 0, gen, &gen_seed);
                dirty = 1;
                snprintf(t, sizeof t, "SCL + K4: GEN %s on bank A", sd_gen_name(gen));
                say(t);
            }
    }
    if (at(S(128))) play(0);
    /* the bar between (the patch last: a new patch's SPACE takes over the tail): the acid pattern, FILTER, GROWL */
    if (at(S(130))) press("SEQ", PG_SEQA);
    if (at(S(131))) pattern(0, &A2, "ACID (16, SLIDE, ratchet, PROB, swing 56)");
    if (at(S(137))) press("ENV", PG_ENV);
    if (at(S(138))) press("ENV", PG_FILTER);
    if (at(S(141))) preset(SD_P_GROWL);
    pot(__LINE__, 2740, S(139), S(143));
#undef S
}

static void s2_filter(double s0)
{
#define S(x) (s0 + (x))
    int32_t v;
    if (at(S(0))) play(1);
    if (at(S(1))) {                             /* EDIT: the filter on (the app's first time: open, ENV 3 = ENV 1) */
        on_page(PG_FILTER, "EDIT off FILTER");
        pat.fx_off &= (uint8_t)~SD_FX_FILTER;
        pat.f_type = SD_FT_LP12;
        pat.f_cut = 52000;
        pat.e3_a = pat.e1_a; pat.e3_d = pat.e1_d; pat.e3_s = pat.e1_s; pat.e3_r = pat.e1_r;
        dirty = 1;
        say("EDIT: FILTER ON (LP12)");
    }
    if (at(S(2))) { pat.f_type = SD_FT_LP24; dirty = 1; say("ALGORITHM: LP24"); }
    on_ramp_page(PG_FILTER, S(2), S(30));
    if (ramp(__LINE__, pat.f_cut / 147, 33000 / 147, S(2), S(30), &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    KNOB99(PG_FILTER, pat.f_res, 62, S(2), S(30));
    if (q >= S(32) && pq <= S(48)) {            /* SCL + K3: ENV 3's SUSTAIN down (2 a detent) */
        on_page(PG_FILTER, "ENV 3 off FILTER");
        if (ramp(__LINE__, pat.e3_s / 2, 5, S(32), S(48), &v)) { pat.e3_s = (uint8_t)(v * 2); dirty = 1; }
    }
    if (q >= S(48) && pq <= S(64)) {            /* K3: ENV 3's depth */
        on_page(PG_FILTER, "ENV off FILTER");
        if (ramp(__LINE__, (pat.f_env * 99 + 16383) / 32767, 60, S(48), S(64), &v)) { pat.f_env = (int16_t)(v * 32767 / 99); dirty = 1; }
    }
    if (at(S(65))) press("SEQ", PG_SEQA);
    if (at(S(66))) press("SEQ", PG_SEQB);
    if (at(S(67))) press("SEQ", PG_TURING);
    if (at(S(68))) {                            /* EDIT on TURING: on, CHANGE 25 (the app's first) */
        on_page(PG_TURING, "TURING's EDIT off its page");
        pa_cur.tur |= 1u;
        pa_cur.mutate = 25;
        sd_engine_pattern_to(&e, 0, &pa_cur);
        say("EDIT: TURING ON (CHANGE 25)");
    }
    if (at(S(88))) xpose(0, 5);
    if (at(S(104))) xpose(0, 3);
    if (at(S(113))) xpose(0, 0);                /* (taken at the next PLAY's start) */
    if (at(S(114))) press("ENV", PG_ENV);
    if (at(S(115))) press("ENV", PG_FILTER);
    on_ramp_page(PG_FILTER, S(116), S(127));
    if (ramp(__LINE__, pat.f_cut / 147, 40000 / 147, S(116), S(127), &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    if (at(S(128))) play(0);
    /* the bar between: DUO, VOWEL, the melody on A, the bass on B */
    if (at(S(129))) glo(SD_MODE_DUO);
    if (at(S(130))) press("SEQ", PG_SEQA);
    if (at(S(131))) pattern(0, &A3, "MELODY (64)");
    if (at(S(133))) press("SEQ", PG_SEQB);
    if (at(S(134))) pattern(1, &B3, "BASS (16)");
    if (at(S(137))) press("FX", PG_REACTOR);
    if (at(S(141))) preset(SD_P_VOWEL);
    pot(__LINE__, 2380, S(139), S(143));
#undef S
}

static void s3_reactor(double s0)
{
#define S(x) (s0 + (x))
    int32_t v;
    static const double X[6] = {24, 40, 56, 88, 104, 120};
    static const int XV[6] = {-4, -2, 0, -4, -2, 0};   /* B under A: D D Bb C */
    int i;
    if (at(S(0))) play(1);
    on_ramp_page(PG_REACTOR, S(3), S(21));
    if (ramp(__LINE__, pat.r_pos / 26, 512 / 26, S(3), S(21), &v)) { pat.r_pos = (uint16_t)(v * 26); dirty = 1; }
    KNOB99(PG_REACTOR, pat.r_res, 60, S(6), S(20));
    if (at(S(22))) press("SEQ", PG_SEQA);
    if (at(S(23))) press("SEQ", PG_SEQB);
    for (i = 0; i < 6; i++) if (at(S(X[i]))) xpose(1, XV[i]);
    if (at(S(58))) press("FX", PG_REACTOR);
    on_ramp_page(PG_REACTOR, S(60), S(84));
    if (ramp(__LINE__, pat.r_pos / 26, 1000 / 26, S(60), S(84), &v)) { pat.r_pos = (uint16_t)(v * 26); dirty = 1; }
    if (at(S(72))) {                            /* SCL + K2: MODEL, VBP -> BP12 */
        on_page(PG_REACTOR, "MODEL off REACTOR");
        pat.r_model = SD_RM_BP12;
        dirty = 1;
        say("SCL + K2: MODEL BP12");
    }
    if (at(S(86))) press("SEQ", PG_SEQA);
    if (at(S(87))) press("SEQ", PG_SEQB);
    if (at(S(128))) play(0);
    /* the bar between: DISTROY, the riff on A, the bass on B */
    if (at(S(130))) pattern(1, &B4, "PULSE (16)");   /* (on SEQ B still) */
    if (at(S(131))) press("HOME", PG_NAV);
    if (at(S(132))) press("SEQ", PG_SEQA);
    if (at(S(133))) pattern(0, &A4, "RIFF (4 segments: RND, P-P)");
    if (at(S(135))) press("HOME", PG_NAV);
    if (at(S(136))) press("HOME", PG_MACROS);
    if (at(S(141))) preset(SD_P_DISTROY);
    pot(__LINE__, 4092, S(139), S(143));
#undef S
}

static void s4_destroy(double s0)
{
#define S(x) (s0 + (x))
    int32_t v;
    if (at(S(0))) play(1);
    KNOB99(PG_MACROS, pat.mac[2], 85, S(1), S(80));    /* BREAK */
    KNOB99(PG_MACROS, pat.mac[1], 60, S(48), S(96));   /* MOVE */
    if (at(S(97))) press("FX", PG_REACTOR);
    if (at(S(98))) press("FX", PG_DESTROY);
    if (at(S(100))) say("ALGORITHM: band 4 (the top)");
    on_ramp_page(PG_DESTROY, S(102), S(124));
    if (ramp(__LINE__, (int32_t)d99(pat.d_gain[3]), 95, S(102), S(124), &v)) { pat.d_gain[3] = u99(v); dirty = 1; }
    if (at(S(128))) play(0);
    /* the bar between: POLY, CHORDS, the chords on A, B off (the arp needs its voice), the ARP page */
    if (at(S(129))) glo(SD_MODE_POLY);
    if (at(S(130))) press("SEQ", PG_SEQA);
    if (at(S(131))) pattern(0, &A5, "CHORDS (Dm Bb F C)");
    if (at(S(133))) press("SEQ", PG_SEQB);
    if (at(S(134))) { e.t[1].mute = 1; say("EDIT: SEQ B OFF"); }
    if (at(S(136))) press("ARP", PG_ARP);
    if (at(S(141))) preset(SD_P_CHORDS);
    pot(__LINE__, 2130, S(139), S(143));
#undef S
}

static void s5_poly(double s0)
{
#define S(x) (s0 + (x))
    static const uint8_t H1[4] = {62, 65, 69, 74}, H2[4] = {64, 67, 71, 76};
    int i;
    if (at(S(0))) play(1);
    if (at(S(8))) {
        on_page(PG_ARP, "ARP's EDIT off its page");
        if (e.t[0].width + (e.t[1].mute ? 0u : e.t[1].width) + 1u > 4u) fail("no voice for ARP");
        e.arp_on = 1; e.arp_mode = SD_AM_UP; e.arp_div = 2; e.arp_oct = 1; e.arp_gate = 50;
        say("EDIT: ARP ON");
    }
    if (at(S(9))) { e.arp_hold = 1; say("ALGORITHM: HOLD"); }
    if (at(S(10))) { e.arp_oct = 2; say("K3: OCTAVES 2"); }
    if (at(S(11))) { e.arp_gate = 40; say("K4: GATE 40"); }
    for (i = 0; i < 4; i++) { if (at(S(12) + i * 0.1)) key(H1[i], 1); if (at(S(14) + i * 0.1)) key(H1[i], 0); }
    if (at(S(50))) press("SEQ", PG_SEQA);
    if (at(S(52))) xpose(0, 2);
    if (at(S(56))) press("ARP", PG_ARP);
    for (i = 0; i < 4; i++) { if (at(S(64) + i * 0.1)) key(H2[i], 1); if (at(S(66) + i * 0.1)) key(H2[i], 0); }
    if (at(S(68))) { e.arp_mode = SD_AM_UPDN; say("K1: MODE UP-DN"); }
    if (at(S(70))) { e.arp_oct = 3; say("K3: OCTAVES 3"); }
    if (at(S(96))) { e.arp_div = 0; say("K2: RATE 1/32"); }
    if (at(S(112))) { e.arp_mode = SD_AM_RND; say("K1: MODE RND"); }
    if (at(S(128))) play(0);
    /* the bar between: ARP off (MONO next), MONO, PULSAR, the transpose back, the ostinato, TURING on */
    if (at(S(129))) { e.arp_on = 0; say("EDIT: ARP OFF"); }
    if (at(S(130))) glo(SD_MODE_MONO);
    if (at(S(131))) press("SEQ", PG_SEQA);
    if (at(S(132))) xpose(0, 0);
    if (at(S(133))) pattern(0, &A6, "OSTINATO (16)");
    if (at(S(134))) press("SEQ", PG_SEQB);
    if (at(S(135))) press("SEQ", PG_TURING);
    if (at(S(136))) {
        pa_cur.tur |= 1u; pa_cur.mutate = 25;
        sd_engine_pattern_to(&e, 0, &pa_cur);
        say("EDIT: TURING ON (CHANGE 25)");
    }
    if (at(S(137))) { pa_cur.mutate = 35; sd_engine_pattern_to(&e, 0, &pa_cur); say("K1: CHANGE 35"); }
    if (at(S(138))) { pa_cur.range = 5; sd_engine_pattern_to(&e, 0, &pa_cur); say("K2: RANGE 5"); }
    if (at(S(139))) press("HOME", PG_NAV);
    if (at(S(141))) preset(SD_P_ORBITAL);
    pot(__LINE__, 4092, S(139), S(143));
#undef S
}

static void s6_turing(double s0)
{
#define S(x) (s0 + (x))
    int32_t v;
    if (at(S(0))) play(1);
    if (at(S(16))) press("HOME", PG_MACROS);
    KNOB99(PG_MACROS, pat.mac[1], 70, S(18), S(60));   /* MOVE */
    KNOB99(PG_MACROS, pat.mac[0], 40, S(40), S(70));   /* MUTATE */
    if (ramp(__LINE__, (int32_t)bpm, 124, S(32), S(112), &v)) { bpm = (uint32_t)v; e.bpm = bpm; }   /* SELECT: any page */
    if (at(S(96))) press("SEQ", PG_SEQA);
    if (at(S(100))) { pa_cur = e.t[0].pat; pa_cur.div = 0; sd_engine_pattern_to(&e, 0, &pa_cur); say("SCL + K2: DIV 1/32"); }
    if (at(S(112))) { pa_cur = e.t[0].pat; pa_cur.div = 2; sd_engine_pattern_to(&e, 0, &pa_cur); say("SCL + K2: DIV 1/16"); }
    if (at(S(128))) play(0);
    if (ramp(__LINE__, (int32_t)bpm, 112, S(129), S(131), &v)) { bpm = (uint32_t)v; e.bpm = bpm; }
    /* the bar between: BEACON, the long notes, the SPACE page */
    if (at(S(132))) { pa_cur = A7; pattern(0, &A7, "LONG NOTES (32)"); }   /* (on SEQ A still) */
    if (at(S(134))) press("FX", PG_REACTOR);
    if (at(S(135))) press("FX", PG_DESTROY);
    if (at(S(136))) press("FX", PG_SPACE);
    if (at(S(141))) preset(SD_P_BEACON);
#undef S
}

static int s7_space(double s0)                  /* 1: the end */
{
#define S(x) (s0 + (x))
    if (at(S(0))) play(1);
    KNOB99(PG_SPACE, pat.s_fb, 70, S(1), S(32));
    KNOB99(PG_SPACE, pat.s_decay, 85, S(16), S(48));
    KNOB99(PG_SPACE, pat.s_dlevel, 70, S(48), S(64));   /* SCL + K1: D-LEVEL */
    if (at(S(64))) play(0);
    if (at(S(66))) key(62, 1);
    if (at(S(96))) key(62, 0);
    KNOB99(PG_SPACE, pat.s_fb, 30, S(96), S(112));
    return q >= S(112);                         /* (then the render runs until it is quiet) */
#undef S
}

static void setup(void)
{
    patterns();
    preset(SD_P_VOYAGE);
    page = PG_SEQA;
    pattern(0, &A1, "LINE (16)");
    page = PG_NAV;
}
static int score(void)
{
    s1_nav(0);
    s2_filter(144);
    s3_reactor(288);
    s4_destroy(432);
    s5_poly(576);
    s6_turing(720);
    return s7_space(864);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sd_range OUT.wav\n"); return 2; }
    return play_run(argv[1], "range", setup, score);
}
