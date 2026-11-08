/* SPDX-License-Identifier: GPL-3.0-only */
/* The bowed-string match (the user, 2026-11-05: "can we get as close as possible to these scales with our
 * instrument? One and the same instrument"): the two scales of the user's recordings played with one patch.
 *
 *   sd_strings piece OUT.wav [PARAMS]   a short duo with the patch: an original cantabile in D minor, 66 BPM, eight
 *                                       bars (~32 s): a melody and a bass line, two engines, as two cellists
 *   sd_strings OUT_PREFIX [PARAMS]   -> OUT_PREFIX_cello.wav (C major, C2 .. C4 and down, 0.62 s a note) and
 *                                       OUT_PREFIX_violin.wav (A major, A3 .. A5 and down, 0.21 s a note)
 *
 * The patch: SD_PATCH's STRINGS (tools/cello_v4.txt, V4), or, with PARAMS (key value lines, see below), one built from them, for tuning.
 * The model: a bowed string is a sawtooth through a body. The source is the banks at the saw; the body is the
 * Reactor's four band-passes on fixed frequencies (no tracking), stages 1-3 a cello's body and 4-6 a violin's, KEY
 * moving POS so the low notes sound through the cello and the high ones through the violin; DESTROY with no
 * shaping is a four-band level (the tilt) and TAME the top's fall. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_params.h"

static SdEngine eng;
static SdSpaceMem sm;

static void put16(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFFu); put16(f, v >> 16); }
static int scale(const char *path, const SdPatch *p, const uint8_t *notes, uint32_t n, double note_s, double lead_s)
{
    uint32_t frames = (uint32_t)((lead_s + n * note_s + 1.5) * SD_FS), done = 0, k = 0, on = 0;
    int32_t buf[512];
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite("RIFF", 1, 4, f); put32(f, 36u + frames * 4u); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SD_FS); put32(f, SD_FS * 4u); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4u);
    sd_engine_init(&eng);
    memset(&sm, 0, sizeof sm);
    eng.sm = &sm;
    sd_engine_patch(&eng, p);
    while (done < frames) {
        uint32_t m = frames - done < 256u ? frames - done : 256u, i;
        double t = (double)done / SD_FS;
        if (k < n && t >= lead_s + k * note_s) {   /* legato: the next note, then the last one off */
            sd_engine_note(&eng, notes[k], 100);
            if (k) sd_engine_note(&eng, notes[k - 1], 0);
            on = notes[k];
            k++;
        } else if (k == n && on && t >= lead_s + n * note_s) {
            sd_engine_note(&eng, on, 0);
            on = 0;
        }
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, m);
        for (i = 0; i < 2u * m; i++) { int32_t v = buf[i]; put16(f, (uint32_t)(uint16_t)(int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v)); }
        done += m;
    }
    fclose(f);
    printf("strings -> %s\n", path);
    return 0;
}

/* the duo: (beat, beats, MIDI, velocity) */
typedef struct { float at, len; uint8_t note, vel; } Ev;
static const Ev MEL[] = {   /* Dm | Gm A | A | F | Gm | C7 A | A7 | Dm: up to the high D in bar 4, then home */
    {0, 2, 50, 78}, {2, 1, 53, 82}, {3, 1, 57, 86},
    {4, 1.5f, 55, 88}, {5.5f, 0.5f, 53, 80}, {6, 1, 52, 84}, {7, 1, 50, 82},
    {8, 2, 49, 90}, {10, 1, 52, 94}, {11, 1, 57, 100},
    {12, 3, 62, 127}, {15, 1, 60, 108},
    {16, 1.5f, 58, 98}, {17.5f, 0.5f, 57, 90}, {18, 1, 55, 92}, {19, 1, 53, 88},
    {20, 1, 52, 86}, {21, 1, 55, 88}, {22, 1, 58, 92}, {23, 1, 57, 86},
    {24, 1, 53, 78}, {25, 1, 52, 70}, {26, 1, 50, 64}, {27, 1, 49, 58},
    {28, 4, 50, 50}};
static const Ev BASS[] = {
    {0, 4, 38, 70}, {4, 2, 43, 72}, {6, 2, 45, 74}, {8, 4, 45, 76}, {12, 2, 41, 80}, {14, 2, 45, 78},
    {16, 4, 43, 76}, {20, 2, 48, 74}, {22, 2, 45, 70}, {24, 4, 45, 58}, {28, 4, 38, 46}};

static int piece(const char *path, const SdPatch *p)
{
    static SdEngine e2;
    static SdSpaceMem sm2;
    const double beat = 60.0 / 66.0;
    uint32_t frames = (uint32_t)((32.0 * beat + 3.0) * SD_FS), done = 0, i;
    int32_t a[512], b[512];
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite("RIFF", 1, 4, f); put32(f, 36u + frames * 4u); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SD_FS); put32(f, SD_FS * 4u); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4u);
    sd_engine_init(&eng); memset(&sm, 0, sizeof sm); eng.sm = &sm; sd_engine_patch(&eng, p);
    sd_engine_init(&e2); memset(&sm2, 0, sizeof sm2); e2.sm = &sm2; sd_engine_patch(&e2, p);
    while (done < frames) {
        uint32_t m = frames - done < 256u ? frames - done : 256u;
        double t = (double)done / SD_FS, t1 = (double)(done + m) / SD_FS;
        for (i = 0; i < sizeof MEL / sizeof MEL[0]; i++) {   /* legato: the next note on, then the last one off */
            double on = MEL[i].at * beat, off = (MEL[i].at + MEL[i].len) * beat;
            if (on >= t && on < t1) sd_engine_note(&eng, MEL[i].note, MEL[i].vel);
            if (off >= t && off < t1 && !(i + 1 < sizeof MEL / sizeof MEL[0] && MEL[i + 1].note == MEL[i].note))
                sd_engine_note(&eng, MEL[i].note, 0);
        }
        for (i = 0; i < sizeof BASS / sizeof BASS[0]; i++) {
            double on = BASS[i].at * beat, off = (BASS[i].at + BASS[i].len) * beat - 0.02;
            if (on >= t && on < t1) sd_engine_note(&e2, BASS[i].note, BASS[i].vel);
            if (off >= t && off < t1) sd_engine_note(&e2, BASS[i].note, 0);
        }
        memset(a, 0, sizeof a); memset(b, 0, sizeof b);
        sd_engine_render(&eng, a, m);
        sd_engine_render(&e2, b, m);
        {   /* the phrase's arc in the mix (the Reactor's resonances keep the velocities close): held back at the start,
               full at the high D of bar 4, a diminuendo home; the last bar fades */
            double bt = t / beat, g = bt < 12 ? 0.70 + 0.30 * bt / 12 : bt < 16 ? 1.0 : bt < 28 ? 1.0 - 0.35 * (bt - 16) / 12
                      : 0.65 * fmax(0, 1 - (bt - 28) / 6);
            for (i = 0; i < 2u * m; i++) { a[i] = (int32_t)(a[i] * g); b[i] = (int32_t)(b[i] * g); }
        }
        for (i = 0; i < 2u * m; i++) {         /* the melody a little right, the bass a little left, the bass -4 dB */
            int32_t v = (int32_t)(a[i] * (i & 1u ? 0.62 : 0.50) + b[i] * (i & 1u ? 0.30 : 0.38));
            put16(f, (uint32_t)(uint16_t)(int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v));
        }
        done += m;
    }
    fclose(f);
    printf("piece -> %s (%.1f s)\n", path, frames / (double)SD_FS);
    return 0;
}

int main(int argc, char **argv)
{
    static const uint8_t CELLO[] = {36, 38, 40, 41, 43, 45, 47, 48, 50, 52, 53, 55, 57, 59, 60,
                                    59, 57, 55, 53, 52, 50, 48, 47, 45, 43, 41, 40, 38, 36};
    static const uint8_t VIOLIN[] = {57, 59, 61, 62, 64, 66, 68, 69, 71, 73, 74, 76, 78, 80, 81,
                                     80, 78, 76, 74, 73, 71, 69, 68, 66, 64, 62, 61, 59, 57};
    char path[512];
    SdPatch p;
    uint32_t i;
    if (argc < 2) { fprintf(stderr, "usage: sd_strings OUT_PREFIX [PARAMS] | piece OUT.wav [PARAMS]\n"); return 2; }
    for (i = 0; i < SD_PATCHES && strcmp(SD_PATCH_NAME[i], "STRINGS"); i++) {}
    p = i < SD_PATCHES ? SD_PATCH[i] : SD_PATCH_SINE;
    if (argc > (strcmp(argv[1], "piece") ? 2 : 3)) {
        p = SD_PATCH_SINE;
        p.bank[0] = p.bank[1] = p.bank[2] = p.bank[3] =
            (SdBankPatch){{{1, 0, SD_ROLE_C, 0, 52000}, {1, 0, SD_ROLE_C, 0, 0}, {1, 0, SD_ROLE_C, 0, 0}, {1, 0, SD_ROLE_C, 0, 0}}, 0, 0, 0, 0};
        p.r_max = 5;
        if (params(argv[strcmp(argv[1], "piece") ? 2 : 3], &p)) return 1;
    }
    if (!strcmp(argv[1], "piece")) return argc > 2 ? piece(argv[2], &p) : 2;
    snprintf(path, sizeof path, "%s_cello.wav", argv[1]);
    if (scale(path, &p, CELLO, sizeof CELLO, 0.62, 0.8)) return 1;
    snprintf(path, sizeof path, "%s_violin.wav", argv[1]);
    return scale(path, &p, VIOLIN, sizeof VIOLIN, 0.21, 0.5);
}
