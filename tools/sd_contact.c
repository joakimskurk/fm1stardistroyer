/* SPDX-License-Identifier: GPL-3.0-only */
/* FIRST CONTACT, a cue (2026-11-22, the user: "a demo where we think anew, not bound to run on the device as a demo";
 * of the ideas, "First contact is good"). A signal from far away, a ship that answers, a pulse, the contact, and what
 * is left after it. Original material: D minor with the stranger's G sharp (E over D), 72 BPM, 64 bars, ~3:35.
 *
 *   sd_contact OUT.wav [STEMS_DIR]
 *
 * Ten parts, each its own engine as an FM-1 would play it (a multitrack, as the CINEMATIC cue: every sound the
 * engine's own; this tool only times the notes and the knobs and mixes, a fader, a place in the stereo and one
 * master gain each):
 *   DEPTH     the sine under it all, the chords' roots two octaves down
 *   HALO      the pad, far back: the chords, its plate FROZEN at the end
 *   TITAN     the big chords of the contact
 *   BEACON    the ship's voice, far back: it answers the signal, then sings the two together
 *   PRISM     falling 16ths on the chord's tones
 *   UNDERTOW  the eighths on the root, dry and near
 *   BRAAM     the hits: the ship's arrival, the contact's chords, the last one
 *   DRUMS     a heartbeat, then the drive; the fourth a SWELL (a hit played backwards) into each act
 *   SONAR     the signal: its pings (the fourth drum's SONAR, its echoes on the eighths), the stranger's call
 *   DATA      bursts of blips (the fourth drum's DATA), the stranger thinking
 * The stranger's call is D A G#, a fifth up and a half step down, never resolved; the ship answers D A Bb A. At the
 * contact the ship sings them as one (D A G# A Bb A); at the end the signal answers in the ship's words.
 *   1-12   SIGNAL   the sine's low D; the call far off, the data's bursts; the pad from 5, E over D from 9
 *   13-24  SHIP     a hit: the ship; the chords Dm, E/D, Bb, A; the call, and the answer after it
 *   25-40  PULSE    a heartbeat, the eighths, the falling 16ths from 29; the ship's theme from 33, the snare
 *   41-52  CONTACT  the big chords, a hit on each, the drums driving; the call and the answer as one
 *   53-64  AFTER    one last hit; the pad's plate frozen; the signal answers in the ship's words; out
 * The macros step a bar at a time (a stepped walk, its range rising with the cue); the far parts DROWN high, the
 * eighths, the drums and the sine dry and near; the top kept down (the user's cinematic rule: ~-21 dB over 5 kHz). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_demo_patches.h"

#define BPM 72u
#define BAR (SD_FS * 60u * 4u / BPM)            /* frames a bar (147000) */
#define BARS 66u                                 /* 64 and the tails */
#define BLOCK SD_SUB
enum { T_DEPTH, T_HALO, T_TITAN, T_BEACON, T_PRISM, T_UNDER, T_BRAAM, T_DRUMS, T_SONAR, T_DATA, NT };
static const char *const TNAME[NT] = {"depth", "halo", "titan", "beacon", "prism", "undertow", "braam", "drums",
                                      "sonar", "data"};

typedef struct {
    SdEngine e;
    SdSpaceMem sm;
    SdPatch p;
    SdPattern q;
    int16_t ob[2u * SD_DR_BATCH * SD_SUB];
    uint8_t held[8];
    uint32_t nheld;
} Track;
static Track tr[NT];

/* ---- the harmony: a chord two bars (from bar 13); its pad voicing, the big one, the root, the 16ths' tones */
enum { DM, ED, BB, AA, NCH };
static const uint8_t PAD[NCH][4] = {{50, 57, 62, 65}, {50, 56, 59, 64}, {46, 53, 62, 65}, {45, 52, 57, 61}};
static const uint8_t BIG[NCH][4] = {{50, 57, 65, 69}, {50, 59, 64, 68}, {46, 53, 62, 65}, {45, 52, 61, 64}};
static const uint8_t ROOT[NCH] = {38, 38, 34, 33};   /* (E over D: D's pedal) */
static const uint8_t TONE[NCH][4] = {{81, 77, 74, 69}, {83, 80, 76, 74}, {77, 74, 70, 65}, {76, 73, 69, 64}};
static uint32_t chord_at(uint32_t bar)           /* bar from 1 */
{
    if (bar < 13u) return bar < 9u ? DM : ED;
    if (bar >= 49u && bar < 53u) return bar < 51u ? BB : AA;
    if (bar >= 53u) return DM;
    return ((bar - 13u) / 2u) % NCH;
}

/* ---- the clock: sixteenths since the start; at(s): s crossed in this block */
static double q = -1e-9, pq = -1e-9;
static int at(double s) { return pq < s && q >= s; }
#define S(bar, six) (((bar) - 1) * 16.0 + (six))

static void note_on(Track *t, uint32_t n, uint32_t v)
{
    sd_engine_note(&t->e, n, v);
    if (t->nheld < 8u) t->held[t->nheld++] = (uint8_t)n;
}
static void all_off(Track *t)
{
    uint32_t i;
    for (i = 0; i < t->nheld; i++) sd_engine_note(&t->e, t->held[i], 0);
    t->nheld = 0;
}
static void chord(Track *t, const uint8_t *n, uint32_t vel) { uint32_t k; all_off(t); for (k = 0; k < 4u; k++) note_on(t, n[k], vel); }

/* ---- the fourth drum: a hit, its TUNE (a note) and macro set first (SONAR's echoes keep their own pitch: the next
   ping comes after them) */
static void hit4(Track *t, int32_t tune, int32_t macro, uint32_t acc)
{
    if (tune >= 0) t->e.dr.kit.d[SD_DR_CLAP].v[0] = (int16_t)tune;
    if (macro > -1000) t->e.dr.kit.d[SD_DR_CLAP].v[3] = (int16_t)macro;
    t->e.dr.req |= (uint8_t)(1u << SD_DR_CLAP | (acc ? 1u << (4u + SD_DR_CLAP) : 0u));
}
static int32_t ping(int32_t note, int32_t macro) { return note + macro * 12 / 100; }   /* SONAR's TUNE for a note: its
                                                   DEPTH lowers it by 12 semitones at 100 (sd_dr_values), taken back */
static void call(double s, int32_t n0, int32_t n1, int32_t n2, int32_t macro)   /* the stranger's call: three pings
                                                   six 16ths apart (1.25 s: the echoes of one gone before the next) */
{
    if (at(s)) hit4(&tr[T_SONAR], ping(n0, macro), macro, 1);
    if (at(s + 6)) hit4(&tr[T_SONAR], ping(n1, macro), macro, 0);
    if (at(s + 12)) hit4(&tr[T_SONAR], ping(n2, macro), macro, 0);
}
static void data(double s, int32_t chaos) { if (at(s)) hit4(&tr[T_DATA], -1, chaos, 0); }
static void swell(double s) { if (at(s)) hit4(&tr[T_DRUMS], -1, -1000, 1); }   /* its STEPS 16: the peak a bar on */

/* ---- a line: notes at sixteenths, each held to the next (0: a rest); its last held to 'end' */
typedef struct { double s; uint8_t n; } Nt;
static void line(Track *t, const Nt *l, uint32_t count, double end, uint32_t vel)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        if (at(l[i].s)) { all_off(t); if (l[i].n) note_on(t, l[i].n, vel); }
    }
    if (at(end)) all_off(t);
}

/* ---- the sequenced parts' patterns, a chord at a time */
static void under_pattern(uint32_t ch)          /* eighths on the root: an octave jump, a push before the bar */
{
    static const int8_t B[8] = {0, 0, 12, 0, 0, 12, 0, 7};
    SdPattern *p = &tr[T_UNDER].q;
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        p->step[k].note = (uint8_t)(ROOT[ch] + B[k]);
        p->step[k].type = k == 3u ? SD_ST_SLIDE : SD_ST_NOTE;
        p->step[k].vel = k % 2u == 0u ? 112u : 88u;
        p->step[k].gate = 70;
    }
    sd_engine_pattern(&tr[T_UNDER].e, p);
}
static void prism_pattern(uint32_t ch)          /* 16ths falling through the chord's tones, a group of four a beat, the
                                                   groups stepping down */
{
    static const int8_t OFF[16] = {0, 1, 2, 3, 1, 2, 3, -1, 0, 2, 3, -1, 1, 3, -1, -2};
    SdPattern *p = &tr[T_PRISM].q;
    uint32_t k;
    for (k = 0; k < 16u; k++) {
        int32_t i = OFF[k], n = i < 0 ? TONE[ch][(uint32_t)(4 + i) % 4u] - 12 : TONE[ch][i] - 12;
        p->step[k].note = (uint8_t)n;
        p->step[k].type = SD_ST_NOTE;
        p->step[k].vel = k % 4u == 0u ? 108u : 80u;
        p->step[k].gate = 40;
    }
    sd_engine_pattern(&tr[T_PRISM].e, p);
}

/* ---- the beats (a row a drum, a character a step: . off, x on, X accent) */
static void beat(SdDrumPat *b, const char *k, const char *s, const char *h)
{
    const char *row[3] = {k, s, h};
    uint32_t d, i;
    memset(b, 0, sizeof *b);
    for (d = 0; d < 3u; d++)
        for (i = 0; i < SD_DR_STEPS; i++)
            sd_dr_step_set(b, d, i, row[d][i] == 'X' ? SD_DS_ACC : row[d][i] == 'x' ? SD_DS_ON : SD_DS_OFF);
}
static SdDrumPat B_NONE, B_HEART, B_HALF, B_DRIVE, B_LAST;
static SdDrumMore M_NONE, M_ROLL;

/* ---- the macros: a step a bar, a walk in a range that rises with the cue */
static uint32_t rnd = 0xC0471u;
static double rnd01(void) { rnd = rnd * 1103515245u + 12345u; return (double)(rnd >> 8 & 0xFFFF) / 65535.0; }
static double build_at(uint32_t bar)
{
    return bar < 13u ? 0.05 : bar < 25u ? 0.2 : bar < 41u ? 0.2 + 0.4 * (bar - 25u) / 16.0 : bar < 53u ?
           0.65 + 0.35 * (bar - 41u) / 12.0 : 0.1;
}
static void macros(uint32_t bar)
{
    double b = build_at(bar);
    uint32_t k;
    for (k = 0; k < NT; k++) {
        SdPatch *p = &tr[k].p;
        if (k == T_DEPTH || k >= T_DRUMS) continue;
        p->mac[0] = (uint16_t)(65535 * fmin(1, 0.10 * rnd01() + 0.45 * b));   /* MUTATE */
        p->mac[1] = (uint16_t)(65535 * fmin(1, 0.15 + 0.45 * rnd01() * (0.4 + b)));   /* MOVE */
        p->mac[2] = (uint16_t)(65535 * fmin(1, (k == T_BRAAM ? 0.30 : 0.05) + 0.25 * b * rnd01()));   /* BREAK (the hits'
                                                   top kept down: the cue's ~-21 dB over 5 kHz) */
        if (k == T_HALO || k == T_BEACON || k == T_TITAN) {   /* far back, clean: DROWN high */
            p->mac[2] = 0;
            p->mac4 = SD_M4_DROWN;
            p->mac[3] = (uint16_t)(65535 * (k == T_TITAN ? 0.60 + 0.2 * rnd01() : 0.62 + 0.25 * rnd01()));
        }
        if (k == T_UNDER) { p->mac[1] = 0; p->mac[3] = 0; }
    }
    {                                           /* the data: its CHAOS with the cue */
        Track *t = &tr[T_DATA];
        t->e.dr.kit.d[SD_DR_CLAP].v[3] = (int16_t)(-40 + 120 * b);
    }
}

/* ---- the arrangement: each part's fader through the cue (in, out over bars, the fader) and its place */
static double fader(uint32_t t, double bar)      /* bar from 1, fractional */
{
    static const struct { double in0, in1, out0, out1, g; } A[NT] = {
        {1, 3, 62, 65, 0.42},                   /* depth */
        {5, 8, 62, 66, 0.55},                   /* halo */
        {41, 41.3, 53.5, 55, 0.50},             /* titan */
        {13, 13.2, 60, 62, 0.50},               /* beacon */
        {29, 31, 52.8, 53.2, 0.36},             /* prism */
        {25, 26, 52.8, 53.2, 0.62},             /* undertow */
        {13, 13.01, 60, 62, 1.00},              /* braam */
        {12, 12.01, 60, 62, 0.55},              /* drums (the swell into 13 first) */
        {1, 1.01, 62, 64, 0.62},                /* sonar */
        {1, 1.01, 62, 64, 0.70}};               /* data */
    double f;
    if (bar < A[t].in0 || bar >= A[t].out1) return 0;
    f = bar < A[t].in1 ? (bar - A[t].in0) / (A[t].in1 - A[t].in0) : 1.0;
    if (bar > A[t].out0) f *= (A[t].out1 - bar) / (A[t].out1 - A[t].out0);
    if (t == T_SONAR && bar < 13) f *= 0.55 + 0.45 * (bar - 1) / 12.0;   /* the signal nearing */
    return f * A[t].g;
}
static const double PAN[NT] = {0, 0, 0, -0.18, 0.22, 0, 0, 0, -0.25, 0.30};   /* (never hard: equal power within
                                                   +-0.3 of a turn's quarter) */

static void setup(void)
{
    const SdPatch *const PATCH[NT] = {&SD_DPATCH[SD_D_DEPTH], &SD_DPATCH[SD_D_HALO], &SD_DPATCH[SD_D_TITAN],
                                      &SD_DPATCH[SD_D_BEACON], &SD_DPATCH[SD_D_PRISM], &SD_DPATCH[SD_D_UNDERTOW],
                                      &SD_DPATCH[SD_D_BRAAM], 0, 0, 0};
    uint32_t i, k;
    for (i = 0; i < NT; i++) {
        Track *t = &tr[i];
        sd_engine_init(&t->e);
        memset(&t->sm, 0, sizeof t->sm);
        t->e.sm = &t->sm;
        t->p = i >= T_DRUMS ? SD_PATCH_SINE : *PATCH[i];
        sd_pattern_default(&t->q);
        t->e.bpm = BPM;
    }
    tr[T_HALO].e.mode = SD_MODE_POLY;
    tr[T_TITAN].e.mode = SD_MODE_POLY;
    tr[T_DEPTH].p.e1_a = 80; tr[T_DEPTH].p.e1_r = 96;   /* the sine: slower still in and out */
    /* the sequenced parts: 8 eighths, 16 sixteenths */
    tr[T_UNDER].q.len = 8; tr[T_UNDER].q.div = 4; tr[T_UNDER].q.seg_count = 1;
    tr[T_PRISM].q.len = 16; tr[T_PRISM].q.div = 2; tr[T_PRISM].q.seg_count = 1;
    under_pattern(DM);
    prism_pattern(DM);
    /* the drums' engines: their synth silent (a pattern of rests, running for the grid), the drums on */
    for (i = T_DRUMS; i < NT; i++) {
        Track *t = &tr[i];
        t->q.len = 16; t->q.div = 2; t->q.seg_count = 1;
        for (k = 0; k < SD_PSTEPS; k++) t->q.step[k].type = SD_ST_REST;
        sd_engine_pattern(&t->e, &t->q);
        t->e.dr.ob = t->ob;
        t->e.dr.on = 1;
        t->e.dr.kit = SD_DR_KIT0;
        t->e.seq_run = 1;
    }
    beat(&B_NONE, "................", "................", "................");
    beat(&B_HEART, "X..x............", "................", "..x...x...x...x.");
    beat(&B_HALF, "X..x......x.....", "........X.......", "..x...x...x...xx");
    beat(&B_DRIVE, "X..x..x.X..x..x.", "....X.......X...", "xxXxxxXxxxXxxxXx");
    beat(&B_LAST, "X...............", "X...............", "................");
    memset(&M_NONE, 0, sizeof M_NONE);
    M_ROLL = M_NONE;
    sd_dr_more_set(&M_ROLL, SD_DR_SNARE, 14, 2, 0);   /* (a roll into the next bar: x3, x4) */
    sd_dr_more_set(&M_ROLL, SD_DR_SNARE, 15, 3, 0);
    {                                           /* DRUMS: the lab's kit, deeper, the fourth a SWELL a bar long */
        SdDrumKit *kt = &tr[T_DRUMS].e.dr.kit;
        static const int16_t SW[8] = {80, 16, 52, 30, 75, 22, 70, 10};   /* TUNE STEPS TAIL SIZE | NOISE RES SWEEP DRIVE */
        kt->d[SD_DR_KICK].level = 92; kt->d[SD_DR_SNARE].level = 70; kt->d[SD_DR_HAT].level = 18;
        kt->d[SD_DR_KICK].v[3] = 40;            /* BOOM */
        kt->d[SD_DR_SNARE].v[3] = 45;           /* SIZE */
        for (k = 0; k < 8u; k++) kt->d[SD_DR_CLAP].v[k] = SW[k];
        kt->d[SD_DR_CLAP].level = 62; kt->d[SD_DR_CLAP].pan = 0;
        kt->duck = 0;
        tr[T_DRUMS].e.dr.perc = SD_PC_SWELL;
        tr[T_DRUMS].e.dr.pat = B_NONE;
    }
    {                                           /* SONAR: its pings and three echoes on the eighths, falling */
        SdDrumKit *kt = &tr[T_SONAR].e.dr.kit;
        static const int16_t SO[8] = {74, 72, 7, 0, 3, 0, 52, 8};   /* TUNE DECAY TIME DEPTH | REPEATS STEP FALL DRIVE */
        for (k = 0; k < 3u; k++) kt->d[k].level = 0;
        for (k = 0; k < 8u; k++) kt->d[SD_DR_CLAP].v[k] = SO[k];
        kt->d[SD_DR_CLAP].level = 100; kt->d[SD_DR_CLAP].pan = 0;
        tr[T_SONAR].e.dr.perc = SD_PC_SONAR;
        tr[T_SONAR].e.dr.pat = B_NONE;
    }
    {                                           /* DATA: bursts on the 64ths, longer than as chosen */
        SdDrumKit *kt = &tr[T_DATA].e.dr.kit;
        static const int16_t DA[8] = {79, 86, 0, 0, 12, 40, 18, 20};   /* TUNE DECAY RATE CHAOS | SPREAD GATE NOISE DRIVE */
        for (k = 0; k < 3u; k++) kt->d[k].level = 0;
        for (k = 0; k < 8u; k++) kt->d[SD_DR_CLAP].v[k] = DA[k];
        kt->d[SD_DR_CLAP].level = 100; kt->d[SD_DR_CLAP].pan = 0;
        tr[T_DATA].e.dr.perc = SD_PC_DATA;
        tr[T_DATA].e.dr.pat = B_NONE;
    }
    for (i = 0; i < NT; i++) {
        int32_t z[2u * SD_SUB];
        sd_engine_patch(&tr[i].e, &tr[i].p);
        memset(z, 0, sizeof z);
        sd_engine_render(&tr[i].e, z, SD_SUB);  /* (the mode switches at a tick while quiet) */
    }
}

/* ---- the score */
static void score(void)
{
    uint32_t b;
    /* SIGNAL: the low D; the call far off; the data; the pad from 5, E over D from 9 */
    if (at(S(1, 0))) note_on(&tr[T_DEPTH], 26, 92);
    call(S(2, 0), 74, 81, 80, -20);
    call(S(4, 4), 74, 81, 80, -20);
    call(S(7, 0), 74, 81, 80, 0);
    call(S(9, 4), 74, 81, 80, 0);
    call(S(11, 0), 74, 81, 80, 20);
    data(S(3, 10), -40); data(S(6, 6), -30); data(S(8, 13), -20); data(S(10, 3), -10); data(S(12, 8), 0);
    if (at(S(5, 0))) chord(&tr[T_HALO], PAD[DM], 70);
    if (at(S(9, 0))) chord(&tr[T_HALO], PAD[ED], 72);
    swell(S(12, 0));
    /* SHIP: the hit; the chords; the call and the answer */
    if (at(S(13, 0))) { note_on(&tr[T_BRAAM], 38, 127); }
    if (at(S(15, 0))) all_off(&tr[T_BRAAM]);
    for (b = 13; b <= 52; b += 2) {             /* the chords, the sine's roots (13 .. 52) */
        uint32_t ch = chord_at(b);
        if (at(S(b, 0))) {
            if (b < 41u) chord(&tr[T_HALO], PAD[ch], b < 25u ? 74u : 80u);
            else chord(&tr[T_HALO], PAD[ch], 84);
            all_off(&tr[T_DEPTH]); note_on(&tr[T_DEPTH], ROOT[ch] - 12u, 100);
        }
    }
    call(S(13, 4), 74, 81, 80, 0);
    {
        static const Nt A1[] = {{S(14, 4), 62}, {S(14, 8), 69}, {S(14, 12), 70}, {S(15, 0), 69}, {S(16, 0), 0},
                                {S(18, 0), 62}, {S(18, 4), 69}, {S(18, 8), 70}, {S(18, 12), 72}, {S(19, 0), 69},
                                {S(20, 8), 0},
                                {S(21, 8), 74}, {S(22, 8), 72}, {S(23, 0), 71}, {S(23, 8), 68}, {S(24, 0), 69}};
        line(&tr[T_BEACON], A1, sizeof A1 / sizeof A1[0], S(24, 14), 96);
    }
    call(S(15, 4), 74, 81, 80, 10);
    call(S(17, 0), 74, 81, 80, 10);
    call(S(19, 4), 74, 81, 76, 20);             /* (the call changes: D A E) */
    call(S(21, 0), 74, 81, 80, 30);
    call(S(22, 8), 74, 81, 80, 30);
    data(S(17, 10), -10); data(S(19, 14), 0); data(S(21, 6), 10); data(S(22, 14), 10); data(S(23, 6), 20);
    data(S(23, 14), 20);
    swell(S(24, 0));
    /* PULSE: the heartbeat, the eighths; the falling 16ths from 29; the theme and the snare from 33 */
    if (at(S(25, 0))) { tr[T_DRUMS].e.dr.pat = B_HEART; tr[T_UNDER].e.seq_run = 1; }
    if (at(S(29, 0))) tr[T_PRISM].e.seq_run = 1;
    if (at(S(33, 0))) tr[T_DRUMS].e.dr.pat = B_HALF;
    for (b = 25; b <= 52; b++)
        if (b % 2u == 1u && at(S(b, 0))) { under_pattern(chord_at(b)); prism_pattern(chord_at(b)); }
    call(S(26, 8), 74, 81, 80, 30);
    call(S(30, 8), 74, 81, 80, 30);
    call(S(34, 8), 74, 81, 80, 40);
    call(S(38, 8), 74, 81, 80, 40);
    for (b = 25; b <= 39; b += 2) data(S(b + 1, 12), -1000);   /* (CHAOS as the bar has it) */
    {
        static const Nt TH[] = {{S(33, 0), 69}, {S(33, 8), 74}, {S(34, 0), 72}, {S(34, 12), 69},
                                {S(35, 0), 71}, {S(35, 8), 68}, {S(36, 0), 76},
                                {S(37, 0), 74}, {S(37, 8), 77}, {S(38, 0), 70},
                                {S(39, 0), 69}, {S(39, 8), 73}, {S(40, 0), 76}};
        line(&tr[T_BEACON], TH, sizeof TH / sizeof TH[0], S(40, 14), 104);
    }
    swell(S(40, 0));
    /* CONTACT: the big chords, a hit on each, the drums driving; the call and the answer as one */
    if (at(S(41, 0))) tr[T_DRUMS].e.dr.pat = B_DRIVE;
    for (b = 41; b <= 51; b += 2) {
        uint32_t ch = chord_at(b);
        if (at(S(b, 0))) {
            chord(&tr[T_TITAN], BIG[ch], 112);
            all_off(&tr[T_BRAAM]); note_on(&tr[T_BRAAM], ROOT[ch], 127);
        }
        if (at(S(b + 1, 8))) all_off(&tr[T_BRAAM]);
        if (at(S(b + 1, 0))) tr[T_DRUMS].e.dr.more = (b + 1u) % 4u == 0u ? M_ROLL : M_NONE;
        if (at(S(b + 2, 0))) tr[T_DRUMS].e.dr.more = M_NONE;
    }
    {
        static const Nt UN[] = {{S(41, 0), 62}, {S(41, 4), 69}, {S(41, 8), 68}, {S(41, 12), 69}, {S(42, 0), 70},
                                {S(42, 8), 69}, {S(43, 8), 0},
                                {S(45, 0), 74}, {S(45, 4), 81}, {S(45, 8), 80}, {S(45, 12), 81}, {S(46, 0), 82},
                                {S(46, 8), 81}, {S(47, 8), 76}, {S(48, 0), 73},
                                {S(49, 0), 74}, {S(49, 8), 77}, {S(50, 0), 74}, {S(51, 0), 73}, {S(51, 8), 76}};
        line(&tr[T_BEACON], UN, sizeof UN / sizeof UN[0], S(52, 14), 112);
    }
    call(S(41, 0), 74, 81, 80, 50);
    call(S(45, 0), 86, 93, 92, 50);             /* (the octave over, with the ship) */
    for (b = 41; b <= 52; b++) data(S(b, 12), -1000);
    swell(S(52, 0));
    /* AFTER: one last hit; the pad frozen; the signal in the ship's words; out */
    if (at(S(53, 0))) {
        all_off(&tr[T_TITAN]); chord(&tr[T_TITAN], BIG[DM], 120);
        all_off(&tr[T_BRAAM]); note_on(&tr[T_BRAAM], 38, 127);
        chord(&tr[T_HALO], PAD[DM], 84);
        all_off(&tr[T_DEPTH]); note_on(&tr[T_DEPTH], 26, 100);
        tr[T_DRUMS].e.dr.pat = B_LAST;
        tr[T_DRUMS].e.dr.more = M_NONE;
        all_off(&tr[T_BEACON]);
    }
    if (at(S(53, 2))) { tr[T_UNDER].e.seq_run = 0; tr[T_PRISM].e.seq_run = 0; }
    if (at(S(53, 4))) tr[T_DRUMS].e.dr.pat = B_NONE;
    if (at(S(54, 0))) { all_off(&tr[T_TITAN]); all_off(&tr[T_BRAAM]); }
    if (at(S(54, 8))) tr[T_HALO].e.sp_freeze = 1;   /* the plate held as the chord stood */
    if (at(S(55, 0))) all_off(&tr[T_HALO]);
    call(S(56, 0), 74, 81, 82, -20);            /* the signal answers in the ship's words: D A Bb ... */
    if (at(S(57, 2))) hit4(&tr[T_SONAR], ping(81, -20), -20, 0);   /* ... A */
    {
        static const Nt LA[] = {{S(58, 0), 62}, {S(58, 6), 69}, {S(58, 12), 68}, {S(60, 0), 0}};   /* the ship in
                                                   the stranger's: D A G# */
        line(&tr[T_BEACON], LA, sizeof LA / sizeof LA[0], S(60, 0), 84);
    }
    data(S(60, 8), -60);
    if (at(S(61, 0))) hit4(&tr[T_SONAR], ping(74, -40), -40, 0);
    if (at(S(62, 0))) tr[T_HALO].e.sp_freeze = 0;
    if (at(S(63, 0))) all_off(&tr[T_DEPTH]);
}

static void put16(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFFu); put16(f, v >> 16); }
static void header(FILE *f, uint32_t frames)
{
    fwrite("RIFF", 1, 4, f); put32(f, 36u + frames * 4u); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SD_FS); put32(f, SD_FS * 4u); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4u);
}

int main(int argc, char **argv)
{
    uint32_t frames = BARS * BAR, done = 0, k, i, bar_done = 0, len;
    static int32_t buf[NT][2u * BLOCK];
    float *mix, *stem[NT] = {0};
    double peak = 0, g, e2[NT] = {0};
    FILE *f;
    if (argc < 2) { fprintf(stderr, "usage: sd_contact OUT.wav [STEMS_DIR]\n"); return 2; }
    mix = calloc(2u * frames, sizeof *mix);
    if (!mix) return 1;
    if (argc > 2) for (k = 0; k < NT; k++) stem[k] = calloc(2u * frames, sizeof *stem[k]);
    setup();
    while (done < frames) {
        uint32_t n = frames - done < BLOCK ? frames - done : BLOCK, bar = done / BAR + 1u;
        double bf = 1.0 + (double)done / BAR;
        pq = q;
        q = (double)(done + n) * 16.0 / BAR;
        if (bar != bar_done) { bar_done = bar; macros(bar); }
        score();
        for (k = 0; k < NT; k++) {
            double fg = fader(k, bf), pan = PAN[k], gl = cos((pan + 1.0) * M_PI / 4.0) * M_SQRT2, gr = sin((pan + 1.0) * M_PI / 4.0) * M_SQRT2;
            sd_engine_patch(&tr[k].e, &tr[k].p);
            memset(buf[k], 0, sizeof buf[k]);
            sd_engine_render(&tr[k].e, buf[k], n);
            for (i = 0; i < n; i++) {
                double l = fg * gl * buf[k][2u * i] / 32768.0, r = fg * gr * buf[k][2u * i + 1u] / 32768.0;
                mix[2u * (done + i)] += (float)l; mix[2u * (done + i) + 1u] += (float)r;
                e2[k] += l * l + r * r;
                if (stem[k]) { stem[k][2u * (done + i)] = (float)l; stem[k][2u * (done + i) + 1u] = (float)r; }
            }
        }
        done += n;
    }
    for (i = 0; i < 2u * frames; i++) if (fabs(mix[i]) > peak) peak = fabs(mix[i]);
    g = pow(10.0, -1.0 / 20.0) / (peak > 1e-9 ? peak : 1.0);
    for (len = frames; len > SD_FS && fabs(mix[2u * len - 1u] * g) < 1e-4 && fabs(mix[2u * len - 2u] * g) < 1e-4; len--) {}
    if (!(f = fopen(argv[1], "wb"))) { perror(argv[1]); return 1; }
    header(f, len);
    for (i = 0; i < 2u * len; i++) put16(f, (uint32_t)(uint16_t)(int16_t)lrint(mix[i] * g * 32767.0));
    fclose(f);
    printf("contact -> %s (%.1f s, %u BPM; the master %+.1f dB to a -1 dBFS peak)\n", argv[1], len / (double)SD_FS,
           BPM, 20.0 * log10(g));
    for (k = 0; k < NT; k++) {
        printf("  %-9s %6.1f dB RMS (the whole cue, after its fader)\n", TNAME[k], 10 * log10(e2[k] / (2.0 * frames) + 1e-12));
        if (stem[k]) {
            char path[512];
            FILE *sf;
            snprintf(path, sizeof path, "%s/contact_%s.wav", argv[2], TNAME[k]);
            if ((sf = fopen(path, "wb"))) {
                header(sf, len);
                for (i = 0; i < 2u * len; i++) put16(sf, (uint32_t)(uint16_t)(int16_t)lrint(fmax(-1, fmin(1, stem[k][i] * g)) * 32767.0));
                fclose(sf);
            }
            free(stem[k]);
        }
    }
    free(mix);
    return 0;
}
