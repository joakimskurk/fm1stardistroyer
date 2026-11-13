/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* CINEMATIC, a cue: the user asked for a soundtrack, "big, mighty and alien". Original material: D minor, 84 BPM,
 * Dm Bb Gm A, ~69 s. (Remade on 2026-11-08 as four passes on one FM-1, it lost what it had, the user heard: this one,
 * the first, stays.)
 *
 *   sd_cine OUT.wav [STEMS_DIR]
 *
 * Seven parts, each its own engine as the FM-1 would play it (a multitrack, layered; only the mix is this tool's):
 *   PAD      NEBULA, POLY: the chords, the combs on their tones
 *   ALIEN    VOYAGE: long notes high, the nav stepping through its field
 *   OSTINATO PULSAR on the sequencer: 16ths on the chord's tones, the spectral sequencer's steps
 *   BASS     ION on the sequencer: eighths on the root, the bass ducking its own grit
 *   PERC     ORBITAL on the sequencer: a taiko pattern, low
 *   BRAAM 1  MONOLITH: the climax's hits, an octave under the root
 *   BRAAM 2  DISTROY: the hits an octave over, BREAK up
 *   SUB      a sine an octave under the root, from the build on: the depth under it all
 * Depth (the user: "missing a little depth"): the pad and the alien far back (MACRO 4, DROWN, high), the bass and
 * the drums dry and near; the alien an octave lower than at first (its top partials screamed)
 * Sections (bars of 4 beats, 2.86 s): 0-3 the pad and the alien; 4-7 + the ostinato and a drone; 8-15 + the bass
 * and the drums, the macros stepping up a bar at a time; 16-21 the braams on every chord; 22-23 the tails. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"

#define BPM 84u
#define BAR (SD_FS * 60u * 4u / BPM)            /* frames a bar */
#define BARS 24u
#define BLOCK 256u
enum { T_PAD, T_ALIEN, T_OST, T_BASS, T_PERC, T_BR1, T_BR2, T_SUB, NT };
static const char *const TNAME[NT] = {"pad", "alien", "ostinato", "bass", "perc", "braam1", "braam2", "sub"};

typedef struct {
    SdEngine e;
    SdSpaceMem sm;
    SdPatch p;
    SdPattern q;
    uint8_t held[4];
    uint32_t nheld;
} Track;
static Track tr[NT];

/* the chords: Dm Bb Gm A, two bars each; their tones (pad voicing) and roots */
static const uint8_t CHORD[4][4] = {{50, 53, 57, 62}, {46, 50, 53, 58}, {43, 50, 55, 58}, {45, 49, 52, 57}};
static const uint8_t ROOT[4] = {38, 34, 31, 33};
static const uint8_t TONES[4][3] = {{62, 65, 69}, {58, 62, 65}, {55, 58, 62}, {57, 61, 64}};   /* the ostinato's */
/* the alien line: a note a bar or two, high (bar, note, bars long) */
static const struct { uint8_t bar, note, len; } ALIEN[] = {
    {0, 62, 2}, {2, 65, 2}, {4, 64, 1}, {5, 62, 1}, {6, 61, 2}, {8, 69, 2}, {10, 67, 1}, {11, 65, 1}, {12, 62, 2},
    {14, 61, 2}, {16, 74, 2}, {18, 70, 2}, {20, 67, 1}, {21, 69, 1}, {22, 62, 2}};   /* (an octave lower than at
                                                   first: up there the FM's top partials screamed) */

static void note_on(Track *t, uint32_t n, uint32_t v)
{
    sd_engine_note(&t->e, n, v);
    if (t->nheld < 4u) t->held[t->nheld++] = (uint8_t)n;
}
static void all_off(Track *t)
{
    uint32_t i;
    for (i = 0; i < t->nheld; i++) sd_engine_note(&t->e, t->held[i], 0);
    t->nheld = 0;
}

static void ost_pattern(uint32_t ch)            /* 16ths over the chord's tones: up, a skip, down; two octaves */
{
    static const int8_t IDX[16] = {0, 1, 2, 3, 1, 2, 4, 2, 0, 1, 2, 3, 2, 4, 3, 1};
    SdPattern *q = &tr[T_OST].q;
    uint32_t k;
    for (k = 0; k < 16u; k++) {
        int32_t i = IDX[k];
        q->step[k].note = (uint8_t)(TONES[ch][i % 3] + (i >= 3 ? 12 : 0));
        q->step[k].type = SD_ST_NOTE;
        q->step[k].vel = k % 4u == 0u ? 110u : 82u;
        q->step[k].gate = 45;
    }
    sd_engine_pattern(&tr[T_OST].e, q);
}
static void bass_pattern(uint32_t ch)           /* eighths on the root, an octave jump and a push before the bar */
{
    static const int8_t B[8] = {0, 0, 12, 0, 0, 12, 0, 7};
    SdPattern *q = &tr[T_BASS].q;
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        q->step[k].note = (uint8_t)(ROOT[ch] + B[k]);
        q->step[k].type = k == 3u ? SD_ST_SLIDE : SD_ST_NOTE;
        q->step[k].vel = k % 2u == 0u ? 115u : 90u;
        q->step[k].gate = 70;
    }
    sd_engine_pattern(&tr[T_BASS].e, q);
}

static void setup(void)
{
    uint32_t i, k;
    static const uint8_t PATCH[NT] = {SD_P_NEBULA, SD_P_VOYAGE, SD_P_PULSAR, SD_P_ION, SD_P_ORBITAL, SD_P_MONOLITH,
                                       SD_P_DISTROY, 0 /* T_SUB: the sine */};
    for (i = 0; i < NT; i++) {
        Track *t = &tr[i];
        sd_engine_init(&t->e);
        memset(&t->sm, 0, sizeof t->sm);
        t->e.sm = &t->sm;
        t->p = i == T_SUB ? SD_PATCH_SINE : SD_PATCH[PATCH[i]];
        sd_pattern_default(&t->q);
        t->e.bpm = BPM;
    }
    tr[T_PAD].e.mode = SD_MODE_POLY;
    tr[T_PAD].p.s_mode = SD_SP_PP;              /* NEBULA's delay FREE at 1000 ms (PP kept): its 1/4D, 1.07 s at 84
                                                   BPM, is longer than the line and synced plays at half, 1/8D (since
                                                   2026-11-10); the cue was made with 1 s echoes, and the user kept them */
    tr[T_PAD].p.s_time = 65535u;                /* (TIME's top) */
    tr[T_SUB].p.e1_a = 70; tr[T_SUB].p.e1_r = 80; tr[T_SUB].p.e1_s = 120;   /* the sub swells in and out */
    /* the ostinato: 16 steps of 16ths; the bass: 8 eighths; the drums: a taiko 16 of 16ths, low D */
    tr[T_OST].q.len = 16; tr[T_OST].q.div = 2; tr[T_OST].q.seg_count = 1;
    tr[T_BASS].q.len = 8; tr[T_BASS].q.div = 4; tr[T_BASS].q.seg_count = 1;
    {
        static const uint8_t HIT[16] = {1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1};
        static const uint8_t NOTE[16] = {38, 0, 0, 38, 0, 0, 45, 0, 38, 0, 43, 0, 0, 38, 41, 45};
        SdPattern *q = &tr[T_PERC].q;
        q->len = 16; q->div = 2; q->seg_count = 1;
        for (k = 0; k < 16u; k++) {
            q->step[k].type = HIT[k] ? SD_ST_NOTE : SD_ST_REST;
            q->step[k].note = NOTE[k] ? NOTE[k] : 38u;
            q->step[k].vel = k == 0u || k == 8u ? 127u : 96u;
            q->step[k].gate = 40;
            q->step[k].prob = k == 14u || k == 15u ? 70u : 100u;
            q->step[k].ratchet = k == 15u ? 2u : 1u;
        }
    }
    ost_pattern(0);
    bass_pattern(0);
    sd_engine_pattern(&tr[T_PERC].e, &tr[T_PERC].q);
    for (i = 0; i < NT; i++) {
        int32_t z[2u * SD_SUB];
        sd_engine_patch(&tr[i].e, &tr[i].p);
        memset(z, 0, sizeof z);
        sd_engine_render(&tr[i].e, z, SD_SUB);  /* (the mode switches at a tick while quiet) */
    }
}

static float gain_of(uint32_t t, double bar)    /* each part through the arrangement, with fades */
{
    static const struct { double in0, in1, out0, out1; float g; } A[NT] = {
        {0, 2.5, 22, 24, 0.62f},                /* pad */
        {0.5, 2, 22.5, 24, 0.62f},              /* alien */
        {4, 7, 21.5, 22, 1.30f},                /* ostinato */
        {8, 13, 21.8, 22, 0.55f},               /* bass: in over five bars */
        {8, 12, 22, 22.3, 0.55f},               /* drums: in over four */
        {4, 6, 23, 24, 0.80f},                  /* braam 1: the drone from bar 4 (at a third), the hits from 16 */
        {16, 16.02, 23, 24, 0.85f},             /* braam 2 */
        {8, 12, 22.5, 24, 0.26f}};              /* sub */
    double f;
    if (bar < A[t].in0 || bar >= A[t].out1) return 0;
    f = bar < A[t].in1 ? (bar - A[t].in0) / (A[t].in1 - A[t].in0) : 1.0;
    if (bar > A[t].out0) f *= (A[t].out1 - bar) / (A[t].out1 - A[t].out0);
    if (t == T_BR1 && bar < 16) f *= 0.33;      /* the drone, under it all */
    if ((t == T_BASS || t == T_PERC || t == T_PAD) && bar >= 16 && bar < 22) f *= 1.25;   /* the climax opens */
    return (float)f * A[t].g;
}

static uint32_t rnd = 0x57A8u;
static double rnd01(void) { rnd = rnd * 1103515245u + 12345u; return (double)(rnd >> 8 & 0xFFFF) / 65535.0; }

static void on_bar(uint32_t bar)                /* a bar's one: chords, lines, hits, the macros' steps */
{
    uint32_t ch = (bar / 2u) % 4u, k;
    double build = bar < 8u ? 0.0 : bar < 16u ? (bar - 8u) / 8.0 : bar < 22u ? 1.0 : 0.4;
    if (bar % 2u == 0u && bar < 22u) {          /* PAD: a chord every two bars */
        all_off(&tr[T_PAD]);
        for (k = 0; k < 4u; k++) note_on(&tr[T_PAD], CHORD[ch][k], 70u + 8u * (bar >= 16u));
    }
    if (bar == 22u) all_off(&tr[T_PAD]);
    for (k = 0; k < sizeof ALIEN / sizeof ALIEN[0]; k++) {   /* ALIEN */
        if (ALIEN[k].bar == bar) { all_off(&tr[T_ALIEN]); note_on(&tr[T_ALIEN], ALIEN[k].note, 100); }
        if (ALIEN[k].bar + ALIEN[k].len == bar && bar == 24u) all_off(&tr[T_ALIEN]);
    }
    if (bar % 2u == 0u) { ost_pattern(ch); bass_pattern(ch); }
    if (bar == 4u) tr[T_OST].e.seq_run = 1;
    if (bar == 8u) { tr[T_BASS].e.seq_run = 1; tr[T_PERC].e.seq_run = 1; }
    if (bar == 22u) { tr[T_OST].e.seq_run = 0; tr[T_BASS].e.seq_run = 0; tr[T_PERC].e.seq_run = 0; }
    /* BRAAM 1: a drone on D1 from bar 4 to 15, then a hit every chord; BRAAM 2 the hits an octave over the root */
    if (bar == 4u) note_on(&tr[T_BR1], 26, 90);
    if (bar >= 16u && bar < 22u && bar % 2u == 0u) {
        all_off(&tr[T_BR1]); all_off(&tr[T_BR2]);
        note_on(&tr[T_BR1], ROOT[ch] - 12u, 127);
        note_on(&tr[T_BR2], ROOT[ch], 127);
        note_on(&tr[T_BR2], ROOT[ch] + 7u, 120);
    }
    if (bar == 22u) { all_off(&tr[T_BR1]); all_off(&tr[T_BR2]); }
    if (bar >= 8u && bar < 22u && bar % 2u == 0u) { all_off(&tr[T_SUB]); note_on(&tr[T_SUB], ROOT[ch] - 12u, 110); }
    if (bar == 22u) { all_off(&tr[T_SUB]); note_on(&tr[T_SUB], 26, 90); }   /* (the tail's D, held) */
    /* the macros: a step a bar, each part's own, rising with the build (stepped random walks in a range) */
    for (k = 0; k < NT; k++) {
        SdPatch *p = &tr[k].p;
        double r = rnd01();
        p->mac[0] = (uint16_t)(65535 * fmin(1, 0.15 * r + 0.55 * build));   /* MUTATE */
        p->mac[1] = (uint16_t)(65535 * fmin(1, 0.20 + 0.50 * rnd01()));     /* MOVE */
        p->mac[2] = (uint16_t)(65535 * fmin(1, (k >= T_BR1 ? 0.55 : 0.10) + 0.35 * build * rnd01()));   /* BREAK */
        p->mac[3] = (uint16_t)(65535 * fmin(1, 0.25 + 0.4 * rnd01()));      /* MACRO 4 */
        if (k == T_PAD || k == T_ALIEN) {       /* the strings stay clean and far back (DROWN, high) */
            p->mac[2] = 0;
            p->mac[3] = (uint16_t)(65535 * (0.65 + 0.25 * rnd01()));
            p->mac4 = SD_M4_DROWN;
        }
        if (k == T_BASS || k == T_PERC) { p->mac[1] = 0; p->mac[3] = 0; p->s_dlevel = 0; p->s_rlevel /= 3u; }   /* near */
        if (k == T_SUB) { uint32_t z; for (z = 0; z < 4u; z++) p->mac[z] = 0; }
    }
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
    uint32_t frames = BARS * BAR, done = 0, k, i, bar_done = 0xFFFFFFFFu, clipped = 0;
    static int32_t buf[NT][2u * BLOCK];
    FILE *f, *stem[NT] = {0};
    double peak = 0, rms[NT] = {0};
    if (argc < 2) { fprintf(stderr, "usage: sd_cine OUT.wav [STEMS_DIR]\n"); return 2; }
    if (!(f = fopen(argv[1], "wb"))) { perror(argv[1]); return 1; }
    header(f, frames);
    if (argc > 2)
        for (k = 0; k < NT; k++) {
            char path[512];
            snprintf(path, sizeof path, "%s/cine_%s.wav", argv[2], TNAME[k]);
            if ((stem[k] = fopen(path, "wb"))) header(stem[k], frames);
        }
    setup();
    while (done < frames) {
        uint32_t n = frames - done < BLOCK ? frames - done : BLOCK, bar = done / BAR;
        double b = (double)done / BAR;
        if (bar != bar_done) { bar_done = bar; on_bar(bar); }
        for (k = 0; k < NT; k++) {
            sd_engine_patch(&tr[k].e, &tr[k].p);
            memset(buf[k], 0, sizeof buf[k]);
            sd_engine_render(&tr[k].e, buf[k], n);
        }
        for (i = 0; i < 2u * n; i++) {
            double s = 0;
            for (k = 0; k < NT; k++) {
                double v = gain_of(k, b) * buf[k][i] / 32768.0;
                s += v;
                rms[k] += v * v;
                if (stem[k]) put16(stem[k], (uint32_t)(uint16_t)(int16_t)lround(fmax(-1, fmin(1, v)) * 32767.0));
            }
            s *= 0.72;                          /* the mix, then a soft master clip */
            if (fabs(s) > peak) peak = fabs(s);
            s = s / (1.0 + 0.22 * s * s);
            if (s > 0.999) s = 0.999, clipped++;
            if (s < -0.999) s = -0.999, clipped++;
            put16(f, (uint32_t)(uint16_t)(int16_t)lround(s * 32767.0));
        }
        done += n;
    }
    fclose(f);
    for (k = 0; k < NT; k++) if (stem[k]) fclose(stem[k]);
    printf("cine -> %s (%.1f s, %u BPM; mix peak %.2f before the soft clip, %u at the ceiling)\n", argv[1],
           frames / (double)SD_FS, BPM, peak, clipped);
    for (k = 0; k < NT; k++) printf("  %-9s %6.1f dB RMS (whole piece)\n", TNAME[k], 10 * log10(rms[k] / (2.0 * frames) + 1e-12));
    return 0;
}
