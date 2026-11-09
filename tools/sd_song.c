/* SPDX-License-Identifier: GPL-3.0-only */
/* A song, played on one FM-1 by one person (2026-11-07, the user, after RANGE: "can we make it into a song; never
 * more than one patch at a time; no layering").
 *
 *   sd_song OUT.wav
 *
 * The rules are RANGE's (tools/sd_play.h): one engine, one stock patch at a time, the app's controls on their pages at
 * a hand's speed, the voice rules, MASTER as platform.c applies it, no mix. What makes it a song: one key (D minor,
 * the last chorus up a tone), one tempo (112 BPM), one hook, and a form. Within a section the melody and the bass (or
 * the chords) are the two tracks on the one patch; the harmony moves by the patterns and by the tracks' transposes at
 * their ones. Sections change without a stop where the patterns allow it (a pattern loaded inside the bar the old and
 * the new one share); they stop for a bar only where the mode must change (the sound quiet first): before each chorus
 * and before the bridge, a break.
 *
 *   INTRO    8   VOWEL, DUO: the bass alone (D D Bb C), the Reactor opening; the hook enters on A at bar 5
 *   VERSE    16  the verse melody on A, the bass on B; the Reactor's MODEL to BP12 halfway
 *   PRE      8   Bb Bb C C, a rising line; BREAK and MOVE up
 *   (break)      GLO to POLY, CHORDS
 *   CHORUS   16  CHORDS, POLY: Dm Bb F C on A (three voices), the hook on B (the fourth); MOVE in the second half
 *   (break)      GLO to MONO, ORBITAL
 *   BRIDGE   8   ORBITAL, MONO: an ostinato on D, TURING's NOTES moving it, MOVE stepping the nav, MUTATE
 *   BUILD    8   GROWL (the same line, still evolving): the lowpass on, closed, then opened all the way, RES up
 *   (break)      GLO to POLY, CHORDS, the last chorus's pattern
 *   CHORUS   16  again, BREAK's grit, MOVE; at bar 9 both tracks up a tone (E minor)
 *   OUTRO    8   SEQ B off, the arpeggiator on (HOLD, E G B E) over the chords, the echoes up, MASTER down; PLAY
 *                stops, the arp alone, then off */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"

enum { P_GROWL = SD_P_GROWL, P_VOWEL = SD_P_VOWEL, P_CHORDS = SD_P_CHORDS, P_ORBITAL = SD_P_ORBITAL };

/* ---- the patterns (saved before: PRESETS on SEQ loads them). A's are 64 steps, B's 64: a pattern loaded inside
   the last bar plays on from its own next step, so two that share their last bar change seamlessly there */
static SdPattern HOOK, VERSE, PRE, CHORDS, OST, BASS, BASS_PRE;

static void bass(SdPattern *p, const uint8_t root[4])   /* the bass figure on four roots, a bar each */
{
    static const int8_t OFS[16] = {0, -1, 12, -1, 0, 0, -1, 10, 0, -1, 7, -1, 0, -1, 10, 12};
    uint32_t b, k;
    sd_pattern_default(p);
    p->len = 64; p->seg_count = 1;
    for (b = 0; b < 4u; b++)
        for (k = 0; k < 16u; k++) {
            SdStep *s = &p->step[b * 16u + k];
            s->gate = 60; s->ratchet = 1; s->prob = 100;
            s->vel = k == 0u || k == 8u ? 124 : 92;
            if (OFS[k] < 0) { s->type = SD_ST_REST; continue; }
            s->note = (uint8_t)(root[b] + OFS[k]);
            s->type = k == 15u ? SD_ST_SLIDE : SD_ST_NOTE;
        }
}

static void patterns(void)
{
    static const int16_t hook[64] = {           /* Dm Bb F C (the verse's bass: D D Bb C) */
        69 | AC, TI, TI, TI, 65, TI, 67, TI, 69, TI, 72, TI, 69, TI, TI, TI,
        67 | AC, TI, TI, TI, 65, TI, 62, TI, 65, TI, TI, TI, TI, TI, RS, RS,
        74 | AC, TI, TI, TI, 72, TI, 69, TI, 70, TI, TI, TI, 69, TI, 67, TI,
        69 | AC, TI, TI, TI, TI, TI, TI, TI, RS, RS, RS, RS, 64, TI, 65, TI};
    static const int16_t verse[64] = {          /* over D D Bb C, lower: a call, an answer */
        62 | AC, RS, 65, RS, 69, TI, TI, 67, 65, RS, 64, RS, 62, TI, TI, TI,
        RS, RS, 62, RS, 65, RS, 69, RS, 72 | AC, TI, TI, TI, 69, TI, TI, TI,
        70 | AC, TI, TI, TI, 69, RS, 65, RS, 62, RS, 65, RS, 69, TI, TI, TI,
        67 | AC, TI, TI, TI, 64, RS, 60, RS, 64, RS, 67, TI, RS, RS, RS, RS};
    static const int16_t pre[64] = {            /* over Bb Bb C C, climbing; its last bar the verse's */
        62, RS, 65, RS, 70 | AC, RS, 65, RS, 62, RS, 65, RS, 70, RS, 72, RS,
        74 | AC, TI, TI, TI, 72, TI, 70, TI, 69, TI, TI, TI, RS, RS, RS, RS,
        64, RS, 67, RS, 72 | AC, RS, 67, RS, 64, RS, 67, RS, 72, RS, 74, RS,
        67 | AC, TI, TI, TI, 64, RS, 60, RS, 64, RS, 67, TI, RS, RS, RS, RS};
    static const int16_t ost[16] = {62 | AC, 57, 65, 57, 62, 57, 69, 57, 62 | AC, 57, 65, 57, 64, 57, 60, 57};
    static const uint8_t R_VERSE[4] = {38, 38, 34, 36}, R_PRE[4] = {34, 34, 36, 36};   /* D D Bb C; Bb Bb C C */
    static const uint8_t ROOT[4] = {38, 34, 41, 36};             /* Dm Bb F C: the root low (the chorus's bass), */
    static const int8_t THIRD[4] = {15, 16, 16, 16};             /* the third and the fifth an octave up */
    uint32_t b, k;
    line(&HOOK, hook, 64, 92);
    line(&VERSE, verse, 64, 85);
    line(&PRE, pre, 64, 80);
    line(&OST, ost, 16, 40);
    bass(&BASS, R_VERSE);
    bass(&BASS_PRE, R_PRE);
    sd_pattern_default(&CHORDS);                /* a bar a chord, pushed: on 1, the and of 2, 4 */
    CHORDS.len = 64; CHORDS.seg_count = 1;
    for (b = 0; b < 4u; b++)
        for (k = 0; k < 16u; k++) {
            SdStep *s = &CHORDS.step[b * 16u + k];
            s->gate = 95; s->ratchet = 1; s->prob = 100;
            s->vel = k == 0u ? 112 : 92;
            s->type = k == 0u || k == 6u || k == 12u ? SD_ST_NOTE : k == 15u ? SD_ST_REST : SD_ST_TIE;
            s->note = ROOT[b];
            s->chord[0] = THIRD[b]; s->chord[1] = 19; s->chord[2] = 0;
        }
}

/* ---- the sections; s: sixteenths from the section's start */
#define S(x) (s0 + (x))
static void intro(double s0)                    /* VOWEL, DUO: the bass, the Reactor opening; the hook from bar 5 */
{
    int32_t v;
    if (at(S(0))) play(1);
    on_ramp_page(PG_REACTOR, S(2), S(56));
    if (ramp(__LINE__, pat.r_pos / 26, 512 / 26, S(2), S(56), &v)) { pat.r_pos = (uint16_t)(v * 26); dirty = 1; }
    KNOB99(PG_REACTOR, pat.r_res, 58, S(8), S(48));
    if (at(S(57))) press("SEQ", PG_SEQA);
    if (at(S(60))) track_on(0, 1);              /* the hook's pickup, then the hook */
    if (at(S(122))) pattern(0, &VERSE, "VERSE");
}

static void verse(double s0)                    /* the verse melody on A, the bass on B */
{
    int32_t v;
    if (at(S(2))) press("FX", PG_REACTOR);
    on_ramp_page(PG_REACTOR, S(4), S(120));
    if (ramp(__LINE__, pat.r_pos / 26, 900 / 26, S(4), S(120), &v)) { pat.r_pos = (uint16_t)(v * 26); dirty = 1; }
    if (at(S(126))) { on_page(PG_REACTOR, "MODEL off REACTOR"); act(); pat.r_model = SD_RM_BP12; dirty = 1; say("SCL + K2: MODEL BP12"); }
    on_ramp_page(PG_REACTOR, S(132), S(232));
    if (ramp(__LINE__, pat.r_pos / 26, 520 / 26, S(132), S(232), &v)) { pat.r_pos = (uint16_t)(v * 26); dirty = 1; }
    KNOB99(PG_REACTOR, pat.r_res, 66, S(140), S(220));
    if (at(S(242))) press("SEQ", PG_SEQA);      /* inside the last bar, which PRE shares */
    if (at(S(244))) pattern(0, &PRE, "PRE");
    if (at(S(246))) press("SEQ", PG_SEQB);
    if (at(S(248))) pattern(1, &BASS_PRE, "BASS (Bb Bb C C)");
}

static void prechorus(double s0)                /* Bb Bb C C twice; BREAK and MOVE build */
{
    if (at(S(2))) press("HOME", PG_NAV);
    if (at(S(4))) press("HOME", PG_MACROS);
    pot(__LINE__, 2350, S(0), S(16));           /* (a little down: the chorus lifts) */
    KNOB99(PG_MACROS, pat.mac[2], 35, S(16), S(120));   /* BREAK */
    KNOB99(PG_MACROS, pat.mac[1], 30, S(64), S(120));   /* MOVE */
    if (at(S(128))) play(0);
    /* the break: POLY, the chords on A, the hook on B, CHORDS last (its SPACE takes VOWEL's tail) */
    if (at(S(129.5))) glo(SD_MODE_POLY);
    if (at(S(131))) press("SEQ", PG_SEQA);
    if (at(S(132.5))) pattern(0, &CHORDS, "CHORDS (Dm Bb F C)");
    if (at(S(134))) press("SEQ", PG_SEQB);
    if (at(S(135.5))) pattern(1, &HOOK, "HOOK");
    if (at(S(137))) press("HOME", PG_NAV);
    if (at(S(139.5))) preset(P_CHORDS);
    KNOB99(PG_NAV, pat.warp, 42, S(140.5), S(143.5));   /* K2: WARP down a little, warmer */
    pot(__LINE__, 3300, S(140), S(143));
}

static void chorus1(double s0)
{
    if (at(S(0))) play(1);
    if (at(S(124))) press("HOME", PG_MACROS);
    KNOB99(PG_MACROS, pat.mac[1], 35, S(128), S(240));  /* MOVE: the second half wider, moving */
    if (at(S(256))) play(0);
    /* the break: MONO, the ostinato, ORBITAL */
    if (at(S(257.5))) glo(SD_MODE_MONO);
    if (at(S(259))) press("SEQ", PG_SEQA);
    if (at(S(260.5))) pattern(0, &OST, "OSTINATO (16)");
    if (at(S(262))) press("HOME", PG_NAV);
    if (at(S(267.5))) preset(P_ORBITAL);
    pot(__LINE__, 2900, S(268), S(271));
}

static void bridge(double s0)                   /* ORBITAL: TURING's NOTES moves the line; MOVE, MUTATE */
{
    if (at(S(0))) play(1);
    lfo_to_turing(S(8));                        /* TURING, set while off: NOTES, 42 %, CHANGE 32; then on */
    TUR_DEST_BY(-1, S(10.5), S(11.5));
    TUR_AMOUNT(42, S(12), S(18));
    TUR_CHANGE(32, S(18), S(23));
    if (at(S(24))) tur_edit();
    if (at(S(32))) press("HOME", PG_NAV);
    if (at(S(34))) press("HOME", PG_MACROS);
    KNOB99(PG_MACROS, pat.mac[1], 60, S(36), S(120));   /* MOVE */
    KNOB99(PG_MACROS, pat.mac[0], 30, S(64), S(120));   /* MUTATE */
    if (at(S(127.8))) preset(P_GROWL);          /* the same line, GROWL from the next bar (turned on the bar line:
                                                   ORBITAL's tail stops there, as on the FM-1) */
    pot(__LINE__, 3500, S(126.5), S(127.8));
}

static void build(double s0)                    /* GROWL: the lowpass closes, then opens all the way */
{
    int32_t v;
    if (at(S(1))) press("ENV", PG_ENV);
    if (at(S(2.5))) press("ENV", PG_FILTER);
    if (at(S(4))) {                             /* EDIT: the filter on (the app's first time: open, ENV 3 = ENV 1) */
        on_page(PG_FILTER, "EDIT off FILTER");
        act();
        pat.fx_off &= (uint8_t)~SD_FX_FILTER;
        pat.f_type = SD_FT_LP12; pat.f_cut = 52000;
        pat.e3_a = pat.e1_a; pat.e3_d = pat.e1_d; pat.e3_s = pat.e1_s; pat.e3_r = pat.e1_r;
        dirty = 1;
        say("EDIT: FILTER ON (LP12)");
    }
    if (at(S(5.5))) { act(); pat.f_type = SD_FT_LP24; dirty = 1; say("ALGORITHM: LP24"); }
    on_ramp_page(PG_FILTER, S(6), S(34));
    if (ramp(__LINE__, pat.f_cut / 147, 22000 / 147, S(6), S(34), &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    KNOB99(PG_FILTER, pat.f_res, 45, S(6), S(34));
    if (q >= S(36) && pq <= S(50)) {            /* K3: ENV 3's depth */
        on_page(PG_FILTER, "ENV off FILTER");
        if (ramp(__LINE__, (pat.f_env * 99 + 16383) / 32767, 40, S(36), S(50), &v)) { pat.f_env = (int16_t)(v * 32767 / 99); dirty = 1; }
    }
    on_ramp_page(PG_FILTER, S(52), S(124));
    if (ramp(__LINE__, pat.f_cut / 147, 56000 / 147, S(52), S(124), &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    KNOB99(PG_FILTER, pat.f_res, 72, S(80), S(124));
    if (at(S(128))) play(0);
    /* the break (silent: GROWL has no SPACE): POLY, the chords, CHORDS (B still has the hook) */
    if (at(S(129.5))) glo(SD_MODE_DUO);
    if (at(S(131))) glo(SD_MODE_POLY);
    if (at(S(132.5))) press("SEQ", PG_SEQA);
    if (at(S(134))) pattern(0, &CHORDS, "CHORDS (Dm Bb F C)");
    if (at(S(135.5))) press("HOME", PG_NAV);
    if (at(S(137))) preset(P_CHORDS);
    KNOB99(PG_NAV, pat.warp, 42, S(138.5), S(141.5));
    pot(__LINE__, 3300, S(138), S(141));
}

static void chorus2(double s0)                  /* again; BREAK's grit, MOVE; at bar 9 up a tone */
{
    if (at(S(0))) play(1);
    if (at(S(4))) press("HOME", PG_MACROS);
    KNOB99(PG_MACROS, pat.mac[2], 35, S(8), S(60));     /* BREAK */
    KNOB99(PG_MACROS, pat.mac[1], 40, S(32), S(100));   /* MOVE */
    if (at(S(118))) press("SEQ", PG_SEQA);
    if (at(S(120))) xpose(0, 2);
    if (at(S(122))) press("SEQ", PG_SEQB);
    if (at(S(124))) xpose(1, 2);
    if (at(S(130))) press("HOME", PG_NAV);
    if (at(S(132))) press("HOME", PG_MACROS);
    KNOB99(PG_MACROS, pat.mac[1], 60, S(136), S(240));  /* MOVE */
    pot(__LINE__, 3700, S(128), S(160));        /* (a touch louder for the last half) */
    if (at(S(244))) press("SEQ", PG_SEQA);
    if (at(S(246))) press("SEQ", PG_SEQB);
    if (at(S(250))) track_on(1, 0);             /* the hook ends: B off (the arp needs its voice) */
    if (at(S(252))) press("ARP", PG_ARP);
    if (at(S(254))) {
        on_page(PG_ARP, "ARP's EDIT off its page");
        if (e.t[0].width + (e.t[1].mute ? 0u : e.t[1].width) + 1u > 4u) fail("no voice for ARP");
        act();
        e.arp_on = 1; e.arp_mode = SD_AM_UP; e.arp_div = 2; e.arp_oct = 1; e.arp_gate = 50;
        say("EDIT: ARP ON");
    }
}

static int outro(double s0)                     /* the arp over the chords (E minor now), the echoes, the end */
{
    static const uint8_t HAND[4] = {64, 67, 71, 76};
    int i;
    if (at(S(0))) { act(); e.arp_hold = 1; say("ALGORITHM: HOLD"); }
    if (at(S(1.5))) { act(); e.arp_oct = 2; say("K3: OCTAVES 2"); }
    for (i = 0; i < 4; i++) { if (at(S(3) + i * 0.1)) key(HAND[i], 1); if (at(S(5) + i * 0.1)) key(HAND[i], 0); }
    if (at(S(16))) { act(); e.arp_mode = SD_AM_UPDN; say("K1: MODE UP-DN"); }
    if (at(S(32))) press("FX", PG_REACTOR);
    if (at(S(34))) press("FX", PG_DESTROY);
    if (at(S(36))) press("FX", PG_SPACE);
    KNOB99(PG_SPACE, pat.s_fb, 62, S(38), S(80));
    KNOB99(PG_SPACE, pat.s_dlevel, 60, S(80), S(100));  /* SCL + K1: D-LEVEL */
    pot(__LINE__, 1500, S(64), S(150));
    KNOB99(PG_SPACE, pat.s_fb, 22, S(156), S(172));    /* the echoes let go */
    if (at(S(128))) play(0);                    /* the chords stop; the arp plays on, alone */
    if (at(S(174))) press("ARP", PG_ARP);
    if (at(S(176))) { act(); e.arp_on = 0; say("EDIT: ARP OFF"); }
    return q >= S(176);
}
#undef S

static void setup(void)                         /* before the first note: DUO, VOWEL, the patterns; A off */
{
    patterns();
    glo(SD_MODE_DUO);                           /* (quiet: the engine switches while it settles) */
    preset(P_VOWEL);
    page = PG_SEQA;
    pattern(0, &HOOK, "HOOK");
    e.t[0].mute = 1;
    say("EDIT: SEQ A OFF");
    page = PG_SEQB;
    pattern(1, &BASS, "BASS (D D Bb C)");
    page = PG_REACTOR;
    master = 2600;
}

static int score(void)
{
    intro(0);
    verse(128);
    prechorus(384);
    chorus1(528);
    bridge(800);
    build(928);
    chorus2(1072);
    return outro(1328);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sd_song OUT.wav\n"); return 2; }
    return play_run(argv[1], "song", setup, score);
}
