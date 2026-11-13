/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* DERELICT, an overture (2026-11-08, the user, once four patches had gone: a new demo, "2 minutes with the patches,
 * at most 3-4 overlapping tracks; a little noisy, a little cinematic, a little staccato, a little flowing, a little
 * beautiful, a little melancholic; use all the banks; the app's tools, not the typical classic FM things; nothing the
 * firmware could not do on an FM-1". Heard: "a beautiful sound, but only two parts. More an overture, its themes
 * closer together; the lead with much more reverb and delay, and lower"; then "one place a little disharmonic; it
 * can be shorter, less repetition": the breach's G7 under its Phrygian pedal went, and so did the repeats).
 *
 *   sd_derelict OUT.wav [STEMS_DIR]
 *
 * Four passes on one FM-1, each played by one person under RANGE's rules (tools/sd_play.h: one engine, one patch at a
 * time, the app's controls at a hand's speed, the voice rules, MASTER), recorded one after another to the same tempo
 * and layered at fixed faders: never more than four at once, nothing added to them. C minor, 84 BPM, 32 bars: six
 * sections of four, a chord a bar, then the coda's eight; four themes, the first back over the second:
 *   THE CALL (the opening)       BRAAM's hits on C, Ab, G under AURORA's chords swelling in
 *   THEME I (the derelict)       the cello's melody (U01: STRINGS saved with a big room and 1 s echoes)
 *   THEME II (the signal)        PULSAR's staccato 16ths (roots, fifths, ninths), its nav jumping across its four
 *                                banks, under the cello's answer
 *   THEME III (the aurora)       BEACON's high line far back (VOYAGE's banks, COURSE round the field), PULSAR's glints
 *   THEME I over THEME II        the cello's melody again, over the staccato
 *   THEME IV (the breach)        a pedal C in Phrygian that TURING's NOTES moves in its scale, over Fm Db Ab Fm (all in its
 *                                scale), its top band CRUSHed, BREAK and MOVE up; UNDERTOW in 16ths, its warp up
 *   the coda                     BRAAM, the cello down to a low C, the chords drowning
 * The layering: tools/sd_layer.h (each pass in its own process, a fresh FM-1; its performance printed in turn). The
 * stems: STEMS_DIR/derelict_{chords,pulse,low,lead}.wav (STEMS_DIR: OUT's own folder). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"
#include "sd_layer.h"

enum { CHORDS_, PULSE_, LOW_, LEAD_, PASSES };
static const char *const PASS_NAME[PASSES] = {"chords", "pulse", "low", "lead"};
static const double GAIN[PASSES] = {1.36, 1.90, 1.80, 0.55};   /* the faders (then one master to -1 dBFS) */
#define pass layer_pass

#define BAR(b) (((b) - 1) * 16.0)               /* the start of bar b (from 1), in sixteenths */
#define SEC(k) BAR(1 + 4 * (k))                 /* section k (0..6): four bars (the coda eight) */

/* ---- the harmony: a chord a bar, voiced for AURORA (the step's note and three above it) */
typedef struct { uint8_t root; int8_t up[3]; int8_t off; uint8_t low; } Chord;   /* off: the figures' shift from C;
                                                                                    low: the bass's root */
static const Chord CM = {48, {7, 14, 15}, 0, 36}, AB = {44, {11, 16, 19}, -4, 32}, EB = {51, {7, 11, 16}, 3, 39},
                   BB = {46, {7, 14, 16}, -2, 34}, FM = {41, {15, 19, 26}, -7, 29}, DB = {49, {7, 11, 16}, 1, 37},
                   G7 = {43, {16, 19, 22}, -5, 31};
static const Chord *const HARM[7][4] = {
    {&CM, &CM, &AB, &G7},                       /* 0 the call */
    {&CM, &AB, &EB, &BB},                       /* 1 theme I */
    {&CM, &AB, &EB, &BB},                       /* 2 theme II, the cello's answer */
    {&EB, &BB, &CM, &AB},                       /* 3 theme III */
    {&CM, &AB, &FM, &G7},                       /* 4 I over II */
    {&FM, &DB, &AB, &FM},                       /* 5 theme IV: all in C Phrygian, the pedal's scale (a G7 here
                                                   clashed with it, the user heard) */
    {&AB, &BB, &CM, &CM}};                      /* 6 the coda (its second half: C minor held) */

/* ---- the patterns (saved before: PRESETS on SEQ loads them; each loaded inside its section's last step) */
static SdPattern CH[7], PU[7], LO[7], LE[7], RESTS;
static SdPatch U01, U02, U03;                   /* the player's own: STRINGS with SPACE opened, saved in U01; AURORA
                                                   and BEACON with their delays at 1 s, U02 and U03 */

static SdStep *st(SdPattern *p, uint32_t k, uint32_t gate)
{
    SdStep *s = &p->step[k];
    s->gate = (uint8_t)gate; s->ratchet = 1; s->prob = 100; s->vel = 92;
    return s;
}
static void begin(SdPattern *p, uint32_t len, uint32_t div)   /* div: SD_SDIV's index (2 1/16, 4 1/8, 5 1/4) */
{
    sd_pattern_default(p);
    p->len = (uint8_t)len; p->div = (uint8_t)div; p->seg_count = 1;
}
static void put(SdStep *s, int32_t n)           /* a step from sd_play.h's notation: RS, TI, a note | SL | AC */
{
    if (n == RS) { s->type = SD_ST_REST; return; }
    if (n == TI) { s->type = SD_ST_TIE; return; }
    s->note = (uint8_t)(n & 0xFF);
    s->type = n & SL ? SD_ST_SLIDE : SD_ST_NOTE;
    if (n & AC) s->vel = 124;
}
static void chord_step(SdStep *s, const Chord *c, int note)
{
    s->vel = 96; s->note = c->root; memcpy(s->chord, c->up, 3);
    s->type = note ? SD_ST_NOTE : SD_ST_TIE;
}
static void chords(SdPattern *p, int k)         /* eighths: a chord a bar, a NOTE then ties */
{
    uint32_t i;
    begin(p, 32, 4);
    for (i = 0; i < 32u; i++) chord_step(st(p, i, 100), HARM[k][i / 8u], i % 8u == 0u);
}
static void figure(SdPattern *p, int k, const int16_t *f, uint32_t n, uint32_t div, uint32_t gate)   /* a bar's
    figure in C, shifted with each bar's chord */
{
    uint32_t b, i;
    begin(p, 4u * n, div);
    for (b = 0; b < 4u; b++)
        for (i = 0; i < n; i++) {
            int32_t v = f[i];
            if (v != RS && v != TI) v += HARM[k][b]->off;
            put(st(p, b * n + i, gate), v);
        }
}
static void bass(SdPattern *p, int k, int busy)  /* the roots, low: whole bars (0), eighths (1), 16ths (2) */
{
    static const int8_t E8[8] = {0, -1, 0, 12, -1, 0, 7, 0};
    static const int8_t E16[16] = {0, 0, 12, 0, -1, 0, 7, 0, 0, 0, 12, -1, 0, 7, 12, 0};
    uint32_t b, i, n = busy == 2 ? 16u : busy ? 8u : 4u;
    begin(p, 4u * n, busy == 2 ? 2u : busy ? 4u : 5u);
    for (b = 0; b < 4u; b++)
        for (i = 0; i < n; i++) {
            SdStep *s = st(p, b * n + i, busy ? 55 : 100);
            int32_t o = busy == 2 ? E16[i] : busy ? E8[i] : 0;
            if (!busy && i) { s->type = SD_ST_TIE; continue; }
            if (o < 0) { s->type = SD_ST_REST; continue; }
            s->note = (uint8_t)(HARM[k][b]->low + o);
            s->type = SD_ST_NOTE;
            s->vel = i == 0u ? 124 : 88;
        }
    p->step[p->len - 1u].type = SD_ST_REST;     /* (the last a rest: a clean hand-over) */
}
static void melody(SdPattern *p, const int16_t *n, uint32_t len)   /* the lead, in eighths */
{
    uint32_t i;
    begin(p, len, 4);
    for (i = 0; i < len; i++) put(st(p, i, 100), n[i]);
}

static void patterns(void)
{
    /* THEME II: a bar of 16ths in C (C G D: no third to clash with a major chord), staccato */
    static const int16_t sig[16] = {60 | AC, RS, 67, 72, 74, RS, 67, 72, 60, 67, RS, 74, 72, RS, 67, 62};
    /* THEME III's glints: high eighths, fifths and ninths */
    static const int16_t gli[8] = {72 | AC, RS, 79, RS, 74, 72, RS, 79};
    /* THEME IV: a pedal C, C Phrygian, TURING's NOTES moving it in the scale */
    static const int16_t ped[16] = {60 | AC, 60, 72, 60, 67, 60, 72, 68, 60 | AC, 60, 72, 60, 70, 60, 67, 63};
    /* the lead, eighths, a chord a bar */
    static const int16_t th1[32] = {            /* THEME I: Cm Ab Eb Bb */
        63 | AC, TI, TI, 62 | SL, 60, TI, 62 | SL, TI,   60, TI, TI, 58 | SL, 56, TI, 55 | SL, TI,
        58, TI, 63 | SL, TI, 65, TI, 67 | SL, TI,        65 | SL, TI, 62, TI, 60, TI, TI, TI};
    static const int16_t th1b[32] = {           /* its answer, over THEME II; done by the last bar's middle */
        67 | AC, TI, TI, TI, 65 | SL, 63, TI, TI,        63, TI, 60 | SL, TI, TI, TI, TI, TI,
        RS, RS, 58, TI, 63 | SL, TI, 62, TI,             TI, TI, TI, TI, RS, RS, RS, RS};
    static const int16_t th3[32] = {            /* THEME III, BEACON, high: Eb Bb Cm Ab */
        75 | AC, TI, TI, 74 | SL, 70, TI, TI, TI,        74, TI, TI, 72 | SL, 70, TI, 65, TI,
        67, TI, TI, TI, 70 | SL, TI, 72, TI,             72, TI, TI, TI, RS, RS, RS, RS};
    static const int16_t th1c[32] = {           /* THEME I again, over Cm Ab Fm G7 */
        63 | AC, TI, TI, 62 | SL, 60, TI, 62 | SL, TI,   60, TI, TI, 58 | SL, 56, TI, 55 | SL, TI,
        56, TI, 60 | SL, TI, 63, TI, 65 | SL, TI,        62, TI, TI, TI, 59 | SL, TI, TI, TI};
    static const int16_t coda[64] = {           /* the coda: Ab Bb Cm Cm, then C minor held, down to the low C */
        63 | AC, TI, TI, TI, 62 | SL, TI, 60, TI,        58, TI, TI, TI, 60 | SL, TI, 62, TI,
        60, TI, TI, TI, TI, TI, TI, TI,                  55 | SL, TI, TI, TI, TI, TI, TI, TI,
        48 | SL, TI, TI, TI, TI, TI, TI, TI,             TI, TI, TI, TI, TI, TI, TI, TI,
        TI, TI, TI, TI, TI, TI, TI, TI,                  TI, TI, TI, TI, TI, TI, RS, RS};
    uint32_t i, b;
    int k;
    for (k = 0; k < 6; k++) chords(&CH[k], k);
    CH[0].step[8].type = SD_ST_TIE;             /* the call: C minor for two bars */
    begin(&CH[6], 64, 4);                       /* the coda: its chords, then C minor four bars */
    for (i = 0; i < 64u; i++) chord_step(st(&CH[6], i, 100), i < 32u ? HARM[6][i / 8u] : &CM, i < 24u && i % 8u == 0u);
    figure(&PU[2], 2, sig, 16, 2, 28);
    figure(&PU[3], 3, gli, 8, 4, 60);
    figure(&PU[4], 4, sig, 16, 2, 28);
    for (k = 2; k <= 4; k += 2)                 /* THEME II's ratchet and chance, each bar */
        for (b = 0; b < 4u; b++) { PU[k].step[b * 16u + 7u].ratchet = 2; PU[k].step[b * 16u + 11u].prob = 70; }
    begin(&PU[5], 16, 2);                       /* THEME IV, saved in Phrygian (what TURING's NOTES moves in) */
    for (k = 0; k < 16; k++) put(st(&PU[5], (uint32_t)k, 30), ped[k]);
    PU[5].step[6].ratchet = 2; PU[5].step[14].ratchet = 3; PU[5].step[3].prob = 75;
    PU[5].scale = SD_SC_PHRYG;
    bass(&LO[1], 1, 0);
    bass(&LO[2], 2, 1);
    bass(&LO[3], 3, 1);
    bass(&LO[4], 4, 1);
    bass(&LO[5], 5, 2);
    melody(&LE[1], th1, 32);
    melody(&LE[2], th1b, 32);
    melody(&LE[3], th3, 32);
    melody(&LE[4], th1c, 32);
    melody(&LE[6], coda, 64);
    begin(&RESTS, 32, 4);
    for (i = 0; i < 32u; i++) st(&RESTS, i, 50)->type = SD_ST_REST;
    /* U01: STRINGS, its SPACE opened on the SPACE page and saved: FREE PP, TIME 1000 ms, FDBK 52, D-LEVEL 44, TONE 34;
       SIZE 90, DECAY 84, R-LEVEL 56, its TONE 36 */
    U01 = SD_PATCH[SD_P_STRINGS];
    U01.s_mode = SD_SP_PP; U01.s_time = 65535u;   /* (TIME's top: 1000 ms) */
    U01.s_fb = u99(52); U01.s_dlevel = u99(44); U01.s_dtone = u99(34);
    U01.s_size = u99(90); U01.s_decay = u99(84); U01.s_rlevel = u99(56); U01.s_rtone = u99(36);
    /* U02, U03: AURORA and BEACON, each with its SPACE MODE turned to FREE (PP kept) and TIME to 1000 ms, saved. Their
       1/4D is 1.07 s at 84 BPM, longer than the line: synced it plays at half, 1/8D (since 2026-11-10). DERELICT was
       made with 1 s echoes there, and the user, hearing both, kept them */
    U02 = SD_PATCH[SD_P_AURORA];
    U02.s_mode = SD_SP_PP; U02.s_time = 65535u;
    U03 = SD_PATCH[SD_P_BEACON];
    U03.s_mode = SD_SP_PP; U03.s_time = 65535u;
}

/* ---- gestures */
static void load(int k, const SdPattern *p, const char *name)   /* section k's pattern, inside the last step before */
{
    if (at(SEC(k) - 0.4)) pattern(0, p, name);
}

/* the chords: AURORA (U02) in POLY on SEQ A the whole way; MACRO 4 (DROWN) at the end */
static void chords_setup(void)
{
    glo(SD_MODE_POLY);
    preset_user(&U02, "U02 AURORA 1 S");
    page = PG_SEQA;
    pattern(0, &CH[0], "THE CALL (Cm Cm Ab G7)");
    master = 0;
}
static int chords_score(void)
{
    static const char *const NAME[7] = {"", "THEME I", "THEME II", "THEME III", "THEME I over II", "THEME IV", "CODA"};
    int k;
    if (at(0)) play(1);
    pot(__LINE__, 3000, 0, BAR(3));             /* MASTER: in from nothing */
    for (k = 1; k <= 6; k++) load(k, &CH[k], NAME[k]);
    pot(__LINE__, 3600, SEC(5), SEC(5) + 8);    /* (the breach a little up) */
    pot(__LINE__, 3000, SEC(6), SEC(6) + 8);
    if (at(SEC(6) + 2)) press("HOME", PG_NAV);
    if (at(SEC(6) + 3.5)) press("HOME", PG_MACROS);
    KNOB99(PG_MACROS, pat.mac[3], 80, SEC(6) + 4, BAR(32));   /* MACRO 4: DROWN */
    if (at(BAR(33) - 1)) play(0);
    pot(__LINE__, 0, BAR(33), BAR(36));         /* the hall let go of by hand */
    return q >= BAR(36);
}

/* the pulse: PULSAR in MONO; THEME II, III's glints, IV (TURING's NOTES, set in II's last section while off, on as
   IV starts); CRUSH on its top band, BREAK and MOVE */
static void pulse_setup(void)
{
    preset(SD_P_PULSAR);
    page = PG_SEQA;
    pattern(0, &PU[2], "THEME II (16ths)");
    master = 4092;
}
static int pulse_score(void)
{
    if (at(SEC(2))) play(1);
    load(3, &PU[3], "THEME III (glints)");
    pot(__LINE__, 2700, SEC(3) - 1, SEC(3));    /* MASTER: the glints under the high line */
    pot(__LINE__, 4092, SEC(4) - 1, SEC(4));
    load(4, &PU[4], "THEME II (16ths)");
    lfo_to_turing(SEC(4) + 2);                  /* TURING, set while off: NOTES, 33 % (+-4 degrees), CHANGE 30 */
    TUR_DEST_BY(-1, SEC(4) + 4.5, SEC(4) + 5.5);
    TUR_AMOUNT(33, SEC(4) + 6, SEC(4) + 12);
    TUR_CHANGE(30, SEC(4) + 12, SEC(4) + 16);
    if (at(SEC(4) + 18)) press("SEQ", PG_SEQA);
    load(5, &PU[5], "THEME IV (Phrygian)");
    lfo_to_turing(SEC(5) + 0.5);
    if (at(SEC(5) + 3.5)) tur_edit();           /* on: the pedal moves */
    if (at(SEC(5) + 5)) press("FX", PG_REACTOR);
    if (at(SEC(5) + 6.5)) press("FX", PG_DISTROY);
    if (at(SEC(5) + 8)) { act(); say("ALGORITHM: band 4 (the top)"); }
    if (at(SEC(5) + 9.5)) {                     /* SCL + ALGORITHM: the top band's shaper to CRUSH */
        on_page(PG_DISTROY, "a shaper off DISTROY");
        act();
        pat.d_type[3] = SD_DS_CRUSH;
        dirty = 1;
        say("SCL + ALGORITHM: SHAPE CRUSH (band 4)");
    }
    KNOB99(PG_DISTROY, pat.d_gain[3], 58, SEC(5) + 10, SEC(5) + 22);   /* K1: its GAIN */
    if (at(SEC(5) + 24)) press("HOME", PG_NAV);
    if (at(SEC(5) + 25.5)) press("HOME", PG_MACROS);
    KNOB99(PG_MACROS, pat.mac[2], 62, SEC(5) + 26, SEC(6) - 6);   /* BREAK */
    KNOB99(PG_MACROS, pat.mac[1], 65, SEC(5) + 26, SEC(6) - 6);   /* MOVE */
    if (at(SEC(6) - 0.4)) play(0);
    return q >= SEC(6);
}

/* the low: BRAAM's hits, UNDERTOW's roots, eighths and 16ths (its warp up in the breaches), BRAAM again; MONO, the
   keyboard two octaves down */
static void low_setup(void)
{
    preset(SD_P_BRAAM);
    page = PG_SEQA;
    pattern(0, &LO[1], "LOW (THEME I: whole bars)");
    page = PG_NAV;
    oct = -2;                                   /* (OCT- twice before: the keys from F1) */
    master = 3200;
}
static int low_score(void)
{
    int32_t wp = (int32_t)d99(SD_PATCH[SD_P_UNDERTOW].warp);   /* UNDERTOW's WARP, in detents */
    if (at(0.5)) key(36, 1);                    /* the call: C, Ab, G */
    if (at(BAR(2) + 10)) key(36, 0);
    if (at(BAR(3))) key(32, 1);
    if (at(BAR(3) + 12)) key(32, 0);
    if (at(BAR(4))) key(31, 1);
    if (at(BAR(4) + 12)) key(31, 0);
    if (at(SEC(1) - 1.2)) preset(SD_P_UNDERTOW);
    pot(__LINE__, 2100, SEC(1) - 1.1, SEC(1) - 0.2);   /* (THEME I's long roots, softer) */
    pot(__LINE__, 3500, SEC(2) - 2, SEC(2));
    if (at(SEC(1))) play(1);
    if (at(SEC(1) + 2)) press("SEQ", PG_SEQA);
    load(2, &LO[2], "LOW (eighths)");
    load(3, &LO[3], "LOW (eighths)");
    load(4, &LO[4], "LOW (eighths)");
    load(5, &LO[5], "LOW (16ths)");
    if (at(SEC(5) + 2)) press("HOME", PG_NAV);
    KNOB99(PG_NAV, pat.warp, wp + 24, SEC(5) + 4, SEC(6) - 10);   /* K2: WARP toward the saw */
    if (at(SEC(6) - 1.5)) play(0);
    if (at(SEC(6) - 0.6)) preset(SD_P_BRAAM);
    if (at(SEC(6))) key(32, 1);                 /* the coda: Ab, then C held */
    if (at(SEC(6) + 24)) key(32, 0);
    if (at(BAR(27))) key(36, 1);
    if (at(BAR(31))) key(36, 0);
    pot(__LINE__, 0, BAR(29), BAR(33));
    return q >= BAR(33);
}

/* the lead: U01 (STRINGS in a big room) in MONO, BEACON (U03) for THEME III; far back */
static void lead_setup(void)
{
    preset_user(&U01, "U01 CELLO HALL");
    page = PG_SEQA;
    pattern(0, &LE[1], "THEME I");
    master = 1500;                              /* (far back: the room and the echoes carry it) */
}
static int lead_score(void)
{
    if (at(SEC(1))) play(1);
    load(2, &LE[2], "THEME I (the answer)");
    if (at(SEC(3) - 4.6)) press("HOME", PG_NAV);   /* BEACON for THEME III, in the answer's last bar */
    if (at(SEC(3) - 3.6)) preset_user(&U03, "U03 BEACON 1 S");
    if (at(SEC(3) - 2.8)) press("SEQ", PG_SEQA);
    pot(__LINE__, 4092, SEC(3) - 3.4, SEC(3) - 1);   /* MASTER up for BEACON, far back already */
    load(3, &LE[3], "THEME III");
    if (at(SEC(4) - 4.6)) press("HOME", PG_NAV);   /* back to U01, in THEME III's last half bar */
    if (at(SEC(4) - 3.6)) preset_user(&U01, "U01 CELLO HALL");
    if (at(SEC(4) - 2.8)) press("SEQ", PG_SEQA);
    pot(__LINE__, 1500, SEC(4) - 3.4, SEC(4) - 1);
    load(4, &LE[4], "THEME I (over II)");
    load(5, &RESTS, "(rests)");
    load(6, &LE[6], "CODA");
    if (at(BAR(33) - 1)) play(0);
    return q >= BAR(33);
}

static void (*const SETUP[PASSES])(void) = {chords_setup, pulse_setup, low_setup, lead_setup};
static int (*const SCORE[PASSES])(void) = {chords_score, pulse_score, low_score, lead_score};
static void setup(void) { patterns(); SETUP[pass](); }
static int score(void) { return SCORE[pass](); }

int main(int argc, char **argv)
{
    bpm = 84;
    return layer_main(argc, argv, "derelict", PASS_NAME, GAIN, PASSES, setup, score);
}
