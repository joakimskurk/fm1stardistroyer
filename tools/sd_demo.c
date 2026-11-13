/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* A demo of the sound engine (firmware/src/sd_engine.h) as music: ~105 s, 100 BPM, D minor.
 *
 *   sd_demo OUT.wav
 *
 * Five parts, each its own engine as the FM-1 would play it (the instrument plays one part at a time: this is five
 * takes layered, a mix as from a multitrack):
 *   PAD      POLY chords (Dm9 Bbmaj7 Gm9 A7sus4), the Reactor's AP2 phaser gliding
 *   PLUCKS   a 16th arpeggio pinging the Reactor's KP combs
 *   BASS     GROWL's chain low, its fold swept by a slow wave
 *   LEAD     VOWEL's melody, the Reactor's POS scanning vowels; later VOYAGE, the nav circling the field
 *   WEIRD    the break: DISTROY with sample-and-hold on XM, warp and fold, the nav in a fast Lissajous, the
 *            Reactor's stages at random, the harmonic generators re-rolling the banks on the beat
 * Every change the patterns and the "modulation" make goes through sd_engine_patch() once a 256-frame interrupt,
 * as the firmware's knobs and modulation would. Only the mix (gains, a soft master clip) is this tool's own and
 * uses floating point; the engine renders exactly as on the FM-1.
 *
 * Bars (16 sixteenths, 2.4 s): 0-5 pad; 6-11 + plucks; 12-19 + bass, vowel lead; 20-27 the weird break;
 * 28-37 everything, VOYAGE lead; 38-43 the pad and a flanged swell.
 *
 * SD_FIRST=1 renders it as the first demo was (2026-11-04, after R3): no SPACE, no DISTROY, the Reactor's MIX a plain
 * crossfade with no low keep and no HARM, the patches' Reactor values from then, POLY at -3 dB a voice. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"

#define BPM 100u
#define S16 (SD_FS * 60u / BPM / 4u)            /* frames a sixteenth: 6615 */
#define BARS 44u
#define BLOCK 256u
#define NTR 6u

typedef struct {
    SdEngine e;
    SdPatch p;
    uint8_t held[8];                            /* notes now on */
    uint32_t nheld;
} Track;

static Track tr[NTR];
enum { T_PAD, T_PLUCK, T_BASS, T_LEAD, T_WEIRD, T_SWELL };

static const uint8_t CHORD[4][4] = {{50, 53, 57, 64}, {46, 50, 53, 57}, {43, 46, 50, 57}, {45, 50, 52, 55}};
static const uint8_t ROOT[4] = {38, 34, 31, 33};
static const int8_t MEL[32] = {62, -1, 65, 67, 69, -1, 67, 65, 64, -1, 62, -1, 57, -1, -1, -1,
                               62, -1, 65, 69, 72, -1, 69, 67, 69, -1, -1, 65, 64, -1, 62, -1};   /* eighths */
static const int8_t TOURMEL[16] = {69, -1, -1, 72, -1, 74, -1, -1, 72, -1, 69, -1, 67, -1, 65, -1};   /* quarters */
static const uint8_t PENTA[10] = {50, 53, 55, 57, 60, 62, 65, 67, 69, 72};

static uint32_t rnd = 0x51ED;
static int first;                               /* SD_FIRST=1: as the first demo */

static void as_first(SdPatch *p)                /* what came after R3, off */
{
    p->s_dlevel = p->s_rlevel = 0;              /* S1 */
    p->d_mix = 0;                               /* D1..D3 */
    p->r_low = 0;                               /* R4 */
    p->r_track &= (uint8_t)~SD_RT_HARM;
}
static void old_mix(SdEngine *e)                /* R4's fill law -> the plain crossfade: dry (1 - MIX) + wet MIX */
{
    e->r.dg = 32768 - e->r.mix;
    e->r.wg = e->r.mix;
}
static uint32_t rand32(void) { rnd = rnd * 1103515245u + 12345u; return rnd >> 8; }

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

static float gain_of(uint32_t t, double bar)    /* each part's level through the arrangement, with fades */
{
    static const struct { double in0, in1, out0, out1; float g; } A[NTR] = {
        {0, 1.5, 42, 44, 0.85f},                 /* pad: the whole piece */
        {6, 7, 38, 39, 0.70f},                   /* plucks */
        {12, 12.5, 38, 39, 0.95f},               /* bass */
        {12, 13, 43, 44, 0.80f},                 /* lead (vowel, then VOYAGE) */
        {20, 20.3, 27.7, 28, 1.00f},             /* the weird break */
        {37, 39, 43, 44, 0.70f}};                /* the flanged swell */
    double f;
    if (bar < A[t].in0 || bar >= A[t].out1) return 0;
    f = bar < A[t].in1 ? (bar - A[t].in0) / (A[t].in1 - A[t].in0) : 1.0;
    if (bar > A[t].out0) f *= (A[t].out1 - bar) / (A[t].out1 - A[t].out0);
    if (t == T_PAD && bar >= 20 && bar < 28) f *= 0.55;   /* the pad steps back for the break */
    if (t == T_BASS && bar >= 20 && bar < 28) f *= 0.8;
    return (float)f * A[t].g;
}

/* ---- a sixteenth's events */
static void sixteenth(uint32_t s)
{
    uint32_t bar = s / 16u, pos = s % 16u, ch = (bar / 2u) % 4u, i;
    /* PAD: a chord every two bars */
    if (pos == 0 && bar % 2u == 0 && bar < 42u) {
        all_off(&tr[T_PAD]);
        for (i = 0; i < 4u; i++) note_on(&tr[T_PAD], CHORD[ch][i], 80 + i * 6);
    }
    if (s == 42u * 16u) all_off(&tr[T_PAD]);
    /* PLUCKS: up and down the chord, two octaves up, a sixteenth each */
    if (bar >= 6u && bar < 39u) {
        static const uint8_t ARP[16] = {0, 1, 2, 3, 2, 1, 0, 2, 1, 3, 2, 0, 3, 1, 2, 3};
        uint32_t n = CHORD[ch][ARP[pos]] + 12u + (pos >= 8u ? 12u : 0u);
        all_off(&tr[T_PLUCK]);
        if (!(pos == 7u || pos == 15u)) note_on(&tr[T_PLUCK], n, pos % 4u == 0 ? 110 : 75);
    }
    /* BASS: root, octave and fifth in eighths, a push on the last sixteenth */
    if (bar >= 12u && bar < 39u) {
        static const int8_t BL[16] = {0, -1, 0, 12, -1, 0, 7, -1, 0, -1, 10, 12, -1, 0, 7, 12};
        if (BL[pos] >= 0) { all_off(&tr[T_BASS]); note_on(&tr[T_BASS], ROOT[ch] + (uint32_t)BL[pos], pos == 0 ? 120 : 95); }
        else if (pos % 2u) all_off(&tr[T_BASS]);
    }
    /* LEAD: the vowel melody in eighths (bars 12-19), then VOYAGE's long notes (28-37) */
    if (bar >= 12u && bar < 20u && pos % 2u == 0) {
        int8_t n = MEL[((s - 12u * 16u) / 2u) % 32u];
        if (n >= 0) { all_off(&tr[T_LEAD]); note_on(&tr[T_LEAD], (uint32_t)n, 100); }
        else if (((s / 2u) & 1u)) all_off(&tr[T_LEAD]);
    }
    if (bar == 20u && pos == 0) all_off(&tr[T_LEAD]);
    if (bar >= 28u && bar < 42u && pos % 4u == 0) {
        int8_t n = TOURMEL[((s - 28u * 16u) / 4u) % 16u];
        if (n >= 0) { all_off(&tr[T_LEAD]); note_on(&tr[T_LEAD], (uint32_t)n, 105); }
    }
    if (bar == 42u && pos == 0) all_off(&tr[T_LEAD]);
    /* WEIRD: pentatonic sample-and-hold, gated at random, the banks re-rolled on each beat */
    if (bar >= 20u && bar < 28u) {
        Track *w = &tr[T_WEIRD];
        all_off(w);
        if (rand32() % 100u < 70u) note_on(w, PENTA[rand32() % 10u] + (rand32() % 4u == 0 ? 12u : 0u), 90 + rand32() % 37u);
        w->p.xm = (uint16_t)(20000u + rand32() % 45000u);
        if (pos % 4u == 0) {
            uint32_t b = rand32() % 4u;
            sd_gen(&w->p, b, rand32() % 2u ? 2u : 3u, &rnd);   /* RND or CLUST on one bank */
        }
        w->p.r_pos = (uint16_t)(rand32() % 1536u);
        w->p.r_slew = rand32() % 3u ? 0 : 30000;
    }
    if (s == 28u * 16u) all_off(&tr[T_WEIRD]);
    /* SWELL: one long low note under the outro */
    if (s == 37u * 16u) note_on(&tr[T_SWELL], 38, 100);
    if (s == 43u * 16u) all_off(&tr[T_SWELL]);
}

/* ---- the modulation, once a 256-frame interrupt: slow waves, a Lissajous, sweeps */
static void modulate(double t, double bar)
{
    Track *p = &tr[T_PAD], *b = &tr[T_BASS], *l = &tr[T_LEAD], *w = &tr[T_WEIRD], *s = &tr[T_SWELL];
    p->p.r_pos = (uint16_t)(767.5 + 767.5 * sin(2 * M_PI * t / 19.0));      /* the phaser drifting, 19 s */
    p->p.warp = (uint16_t)(6000 + 6000 * sin(2 * M_PI * t / 11.0));
    b->p.fold = (uint16_t)(14000 + 14000 * sin(2 * M_PI * t / 4.8));        /* the growl's fold, two bars */
    b->p.xm = (uint16_t)(30000 + 12000 * sin(2 * M_PI * t / 7.2));
    if (bar < 20) {                             /* VOWEL: POS through the vowels a bar, a glide */
        l->p.r_pos = (uint16_t)fmod(t / 2.4 * 256.0 * 1.5, 1536.0);
    } else if (bar >= 28) {                     /* VOYAGE: the nav round the field, 7.2 s a lap; DT breathing */
        l->p.x = (uint16_t)(32767.5 + 32767 * cos(2 * M_PI * t / 7.2));
        l->p.y = (uint16_t)(32767.5 + 32767 * sin(2 * M_PI * t / 7.2));
        l->p.dt = (uint16_t)(9000 + 9000 * sin(2 * M_PI * t / 9.6));
    }
    w->p.x = (uint16_t)(32767.5 + 32767 * sin(2 * M_PI * t * 1.3));         /* a fast Lissajous */
    w->p.y = (uint16_t)(32767.5 + 32767 * sin(2 * M_PI * t * 0.83 + 1.0));
    w->p.warp = (uint16_t)(32767.5 + 32767 * sin(2 * M_PI * t * 0.6));
    w->p.fold = (uint16_t)(26000 + 26000 * sin(2 * M_PI * t * 0.37));
    s->p.r_pos = (uint16_t)fmod(t * 120.0, 1536.0);
    s->p.x = (uint16_t)(32767.5 + 32767 * sin(2 * M_PI * t / 5.0));
}

static void setup(void)
{
    uint32_t i;
    /* PAD: soft carriers 1 2 3 (+ a 2 a few cents off), toward triangle, slow ADSR; POLY; the Reactor's AP2 */
    SdPatch pad = SD_PATCH[SD_P_PHASER];
    for (i = 0; i < SD_BANKS; i++)
        pad.bank[i] = (SdBankPatch){{{1, 0, SD_ROLE_C, 0, 34000}, {2, 6, SD_ROLE_C, 0, 18000},
                                     {3, -4, SD_ROLE_C, 0, 9000}, {2, -7, SD_ROLE_C, 0, 12000}}, 0, 0, 0, 0};
    pad.dt = 14000; pad.width = 30000;
    pad.e1_a = 82; pad.e1_d = 100; pad.e1_s = 110; pad.e1_r = 98;
    pad.r_div = 0; pad.r_slew = 50000; pad.r_res = 52000; pad.r_mix = 52000;
    tr[T_PAD].p = pad;
    tr[T_PLUCK].p = SD_PATCH[SD_P_PLUCKS];                /* PLUCKS */
    tr[T_PLUCK].p.r_div = 0; tr[T_PLUCK].p.r_track = 1; tr[T_PLUCK].p.r_res = 56000;
    tr[T_BASS].p = SD_PATCH[SD_P_GROWL];                 /* GROWL */
    tr[T_BASS].p.e1_d = 50; tr[T_BASS].p.e1_s = 60; tr[T_BASS].p.e1_r = 30;
    tr[T_LEAD].p = SD_PATCH[SD_P_VOWEL];                 /* VOWEL; at bar 28 it becomes VOYAGE */
    tr[T_LEAD].p.r_div = 0; tr[T_LEAD].p.r_slew = 34000; tr[T_LEAD].p.e1_r = 70;
    tr[T_WEIRD].p = SD_PATCH[SD_P_DISTROY];                /* DISTROY */
    tr[T_WEIRD].p.r_div = 0; tr[T_WEIRD].p.e1_d = 40; tr[T_WEIRD].p.e1_s = 40; tr[T_WEIRD].p.e1_r = 25;
    tr[T_WEIRD].p.r_mix = 52000;
    tr[T_SWELL].p = SD_PATCH[SD_P_FLANGE];               /* FLANGE */
    tr[T_SWELL].p.r_div = 0; tr[T_SWELL].p.e1_a = 100; tr[T_SWELL].p.e1_r = 100;
    if (first) {                                /* the patches' Reactor values before R4 */
        tr[T_PLUCK].p.r_mix = 65535; tr[T_PLUCK].p.r_drive = 16384;
        tr[T_LEAD].p.r_res = 36000; tr[T_LEAD].p.r_drive = 26000; tr[T_LEAD].p.r_mix = 65535;
        tr[T_WEIRD].p.r_res = 44000; tr[T_WEIRD].p.r_drive = 14000;
        for (i = 0; i < NTR; i++) as_first(&tr[i].p);
    }
    for (i = 0; i < NTR; i++) {
        static SdSpaceMem sm[NTR];              /* each part its own SPACE, as each its own FM-1 */
        sd_engine_init(&tr[i].e);
        tr[i].e.sm = &sm[i];
        sd_engine_patch(&tr[i].e, &tr[i].p);
    }
    tr[T_PAD].e.mode = SD_MODE_POLY;
    {                                           /* the mode switches at a tick while quiet: before the first chord */
        int32_t z[2u * SD_SUB];
        memset(z, 0, sizeof z);
        sd_engine_render(&tr[T_PAD].e, z, SD_SUB);
    }
}

static void put16(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFFu); put16(f, v >> 16); }

int main(int argc, char **argv)
{
    uint32_t frames = BARS * 16u * S16, done = 0, next16 = 0, i, k, clipped = 0, toured = 0;
    static int32_t buf[NTR][2u * BLOCK];
    double peak = 0;
    FILE *f;
    if (argc < 2) { fprintf(stderr, "usage: sd_demo OUT.wav\n"); return 2; }
    if (!(f = fopen(argv[1], "wb"))) { perror(argv[1]); return 1; }
    fwrite("RIFF", 1, 4, f); put32(f, 36u + frames * 4u); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SD_FS); put32(f, SD_FS * 4u); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4u);
    first = getenv("SD_FIRST") && *getenv("SD_FIRST") == '1';
    setup();
    while (done < frames) {
        uint32_t n = frames - done < BLOCK ? frames - done : BLOCK;
        double t = (double)done / SD_FS, bar = (double)done / (16.0 * S16);
        while (next16 * S16 <= done) sixteenth(next16++);
        if (bar >= 28.0 && !toured) {           /* the lead becomes VOYAGE, once */
            toured = 1;
            tr[T_LEAD].p = SD_PATCH[SD_P_VOYAGE];
            tr[T_LEAD].p.course = 0; tr[T_LEAD].p.e1_r = 80; tr[T_LEAD].p.e1_s = 110;
            tr[T_LEAD].p.r_mix = 30000; tr[T_LEAD].p.stage[0] = SD_PATCH[SD_P_VOWEL].stage[2];
            tr[T_LEAD].p.r_model = SD_RM_VBP; tr[T_LEAD].p.r_res = 30000; tr[T_LEAD].p.r_drive = 20000;
            for (k = 0; k < 6u; k++) tr[T_LEAD].p.stage[k] = SD_PATCH[SD_P_VOWEL].stage[k];
            if (first) as_first(&tr[T_LEAD].p);
        }
        modulate(t, bar);
        for (k = 0; k < NTR; k++) {
            sd_engine_patch(&tr[k].e, &tr[k].p);
            if (first) old_mix(&tr[k].e);
            memset(buf[k], 0, sizeof buf[k]);
            sd_engine_render(&tr[k].e, buf[k], n);
        }
        for (i = 0; i < 2u * n; i++) {
            double s = 0;
            for (k = 0; k < NTR; k++) s += gain_of(k, bar) * buf[k][i] * (first && k == T_PAD ? 1.41421 : 1.0);   /* POLY
                                                   was -3 dB a voice */
            s *= 1.05 / 32768.0;                /* the mix, then a soft master clip */
            if (fabs(s) > peak) peak = fabs(s);
            s = s / (1.0 + 0.25 * s * s);       /* gentle above -6 dB */
            if (s > 0.999) s = 0.999, clipped++;
            if (s < -0.999) s = -0.999, clipped++;
            put16(f, (uint32_t)(uint16_t)(int16_t)lround(s * 32767.0));
        }
        done += n;
    }
    fclose(f);
    printf("demo -> %s (%.1f s, %u BPM; mix peak %.2f before the soft clip, %u samples at the ceiling)\n", argv[1],
           frames / (double)SD_FS, BPM, peak, clipped);
    return 0;
}
