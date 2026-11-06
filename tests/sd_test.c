/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound engine's contract (firmware/src/sd_engine.h), on this computer. tests/run_sd.sh runs it.
 *   - silence costs nothing and is silence
 *   - the render does not depend on how the frames are cut into calls (sub-blocks continue across calls)
 *   - a step starts within one sub-block (32 frames, 0.73 ms) of its exact time, at any tempo
 *   - no clicks: a note on, a note off, a step never jump further than the sine itself
 * V1, the operator (Sound Plan §3.1-3.4, §6 V1):
 *   - the table's sine is the sine; warp 0 and fold 0 leave it alone
 *   - warp: sine at 0, saw-like at 0.5, pulse-like at 1, and the waveform moves in small even steps across the whole
 *     range, the hand-over at 0.5 included (no crossfade, no jump)
 *   - fold: the spec's four regions, by folds per half-cycle; continuous too
 *   - extremes: everything at max, any note: no overflow, the output stays under the level
 *   - envelopes: attack and release take their times
 * V2, the bank, modelled on Vector Wave (§3.3):
 *   - warp passes Vector Wave's anchors: sine 0, triangle 20, saw 40, pulse 99, with no seam at the hand-overs
 *   - PM is the formula cos(phi + beta cos(phi)), beta up to 4 pi (level x ENV2 x XM)
 *   - the roles route as Vector Wave draws them, left to right (S the next one, P the nearest carrier)
 *   - every algorithm, all at max: no overflow, under the carriers' levels
 *   - the audio never takes a half-written patch
 * R1, the Rider (Night Rider): MIX 0 is no Rider at all; each filter peaks where it is set (VBP at 0 dB, BP6 louder as
 * it rings, BP12 steeper); POS between stages in log pitch, SLEW 0 jumps, SLEW glides; the stage clock's modes; at
 * every maximum nothing overflows
 * R2: KP rings at its pitch (within 5 cents) and longer with RES; AP1 and AP2 are flat at RES 0 and notch at RES
 * max, left and right apart; a comb that has rung out rests
 * R3: every model within 3 dB of the dry voice (the make-up); small signals clean, DRIVE and RES saturate in the
 * loops; the level law's anchors */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define N (SD_FS * 4u)
static int32_t a[2u * N], b[2u * N];
static SdEngine e1, e2;

static void run(SdEngine *e, int32_t *out, uint32_t frames, uint32_t chunk)
{
    uint32_t done = 0;
    memset(out, 0, frames * 2u * sizeof *out);
    while (done < frames) {
        uint32_t n = frames - done < chunk ? frames - done : chunk;
        sd_engine_render(e, out + 2u * done, n);
        done += n;
    }
}

static void test_silence(void)
{
    uint32_t i;
    sd_engine_init(&e1);
    run(&e1, a, SD_FS, 256);
    for (i = 0; i < 2u * SD_FS; i++)
        if (a[i]) break;
    CHECK(i == 2u * SD_FS, "silence: frame %u is %d", i / 2u, a[i]);
}

static void test_chunking(void)
{
    static const uint32_t CH[] = {1, 7, 31, 32, 33, 100, 256, 1000};
    uint32_t c, i, p;
    for (p = 0; p < SD_PATCHES; p++) {       /* every patch: the fold ramp, the Rider, DESTROY, SPACE continue
                                                   across calls too */
        static SdSpaceMem cm1, cm2;
        memset(&cm1, 0, sizeof cm1);
        sd_engine_init(&e1);
        e1.sm = &cm1;
        sd_engine_patch(&e1, &SD_PATCH[p]);
        e1.seq_run = 1;
        run(&e1, a, N, 256);
        for (c = 0; c < sizeof CH / sizeof CH[0]; c++) {
            memset(&cm2, 0, sizeof cm2);
            sd_engine_init(&e2);
            e2.sm = &cm2;
            sd_engine_patch(&e2, &SD_PATCH[p]);
            e2.seq_run = 1;
            run(&e2, b, N, CH[c]);
            for (i = 0; i < 2u * N; i++)
                if (a[i] != b[i]) break;
            CHECK(i == 2u * N, "%s, chunks of %u: differs from 256 at frame %u", SD_PATCH_NAME[p], CH[c], i / 2u);
        }
    }
}

static void test_timing(void)
{
    static const uint32_t BPM[] = {40, 97, 120, 133, 175, 240};
    uint32_t t, f;
    for (t = 0; t < sizeof BPM / sizeof BPM[0]; t++) {
        uint32_t starts = 0, worst = 0;
        uint8_t last = 0xFF;
        sd_engine_init(&e1);
        e1.bpm = (uint16_t)BPM[t];
        e1.seq_run = 1;
        for (f = 0; f < N; f++) {               /* frame by frame: a step's first frame is where seq_shown changes */
            int32_t o[2] = {0, 0};
            sd_engine_render(&e1, o, 1);
            if (e1.t[0].seq_shown != last || (f == 0)) {
                /* exact start of step k: k * FS * 60 / (bpm * 4) frames; seq_shown changed in the frame just rendered */
                double exact = (double)starts * SD_FS * 60.0 / (BPM[t] * 4.0);
                double late = (double)f - exact;
                uint32_t l = late < 0 ? 999u : (uint32_t)late;
                if (l > worst) worst = l;
                last = e1.t[0].seq_shown;
                starts++;
            }
        }
        CHECK(starts > 4u, "%u BPM: only %u steps", BPM[t], starts);
        CHECK(worst < SD_SUB, "%u BPM: a step %u frames late (more than a sub-block)", BPM[t], worst);
    }
}

static void test_clicks(void)
{
    uint32_t i;
    int32_t worst = 0;
    sd_engine_init(&e1);
    e1.bpm = 240;
    e1.seq_run = 1;
    run(&e1, a, N, 256);
    for (i = 1; i < N; i++) {
        int32_t d = a[2u * i] - a[2u * (i - 1u)];
        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }
    /* the highest note of the loop (root + 15 = C4, 261.6 Hz) at its level (120 * 128): slope 2 pi f / fs * A = 573 */
    CHECK(worst <= 640, "a jump of %d between two frames (the sine alone: <= 573)", worst);
}

/* ---- the operator alone: one cycle of 1024 frames (inc 2^22) at full level, no ramps */
#define CYC 1024u
static SdBank tb;                               /* a bank alone, without the engine: the oscillator's tests */
static SdVoice tv;

static void bank_cycle(const SdPatch *p, int32_t *buf)   /* two cycles of 1024 frames (note inc 2^22); the 2nd */
{
    uint32_t i, c;
    memset(&tb, 0, sizeof tb);
    memset(&tv, 0, sizeof tv);
    tb.sh.fg = tb.sh.fg_next = 4096;
    tb.sh.last_tr = -1;
    tv.inc = 1u << 22;
    tv.note = 60;                               /* below the tracking's knee */
    tv.env2.lvl = SD_ONE;                       /* ENV2 full: a modulator's index is its level x XM */
    sd_bank_params(&tb, p, &p->bank[0], &tv, 1, 65536u);   /* twice first: the level and fold ramps settle */
    for (c = 0; c < 2u; c++)
        for (i = 0; i < CYC; i += SD_SUB) {
            sd_bank_params(&tb, p, &p->bank[0], &tv, 1, 65536u);
            memset(tb.bus, 0, sizeof tb.bus);
            memset(tb.mix, 0, sizeof tb.mix);
            sd_bank_run(&tb, 0, SD_SUB);
            memcpy(buf + i, tb.mix, sizeof tb.mix);
        }
}

static int32_t op_cycle(uint32_t warp, uint32_t fold, int32_t *wave)   /* one oscillator; -> the largest |sample| */
{
    static int32_t buf[CYC];
    SdPatch p = SD_PATCH_SINE;
    int32_t peak = 0;
    uint32_t i;
    p.warp = (uint16_t)warp; p.fold = (uint16_t)fold;
    bank_cycle(&p, buf);
    for (i = 0; i < CYC; i++) {
        int32_t a = buf[i] < 0 ? -buf[i] : buf[i];
        if (wave) wave[i] = buf[i];
        if (a > peak) peak = a;
    }
    return peak;
}

static double harm(const int32_t *w, uint32_t h)   /* the h-th harmonic's amplitude, full scale 1 */
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < CYC; i++) {
        re += w[i] * cos(2 * M_PI * h * i / CYC);
        im += w[i] * sin(2 * M_PI * h * i / CYC);
    }
    return 2 * sqrt(re * re + im * im) / CYC / 32767.0;
}

static double rms_diff(const int32_t *a, const int32_t *b)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < CYC; i++) s += (double)(a[i] - b[i]) * (a[i] - b[i]);
    return sqrt(s / CYC) / 32767.0;
}

static uint32_t crossings(const int32_t *w)     /* sign changes in a cycle */
{
    uint32_t i, c = 0;
    for (i = 0; i < CYC; i++) c += (w[i] < 0) != (w[(i + 1u) % CYC] < 0);
    return c;
}

static void test_lut(void)
{
    uint32_t i;
    double worst = 0;
    for (i = 0; i < 65536u; i++) {
        uint32_t p = i * 65537u;
        double err = fabs(sd_lut(p) - 32767.0 * sin(2 * M_PI * p / 4294967296.0));
        if (err > worst) worst = err;
    }
    CHECK(worst <= 2.5, "the table's sine is %.1f LSB off", worst);
}

static void test_identity(void)
{
    static int32_t w[CYC];
    uint32_t i;
    int32_t worst = 0;
    op_cycle(0, 0, w);
    for (i = 0; i < CYC; i++) {
        int32_t d = w[i] - ((sd_lut(((i + 1u) << 22) + 0x40000000u) * 32767) >> 15);   /* level 65535 = 32767 */
        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }
    CHECK(worst == 0, "warp 0 fold 0 is not the plain cosine: %d LSB off", worst);
}

static double step_median(const double *step, uint32_t n)
{
    double t[257];
    uint32_t i, j;
    memcpy(t, step, n * sizeof *t);
    for (i = 0; i < n; i++)
        for (j = i + 1u; j < n; j++)
            if (t[j] < t[i]) { double x = t[i]; t[i] = t[j]; t[j] = x; }
    return t[n / 2u];
}

/* Vector Wave's warp: "0 = Sine, 40 = Saw, 99 = Pulse", the triangle on the way (20) */
static void test_warp(void)
{
    static const struct { uint32_t w; const char *name; double h2lo, h2hi, h3lo, h3hi; } A[] = {
        {0, "sine (0)", 0, 0.001, 0, 0.001},
        {SD_WARP_ROW_TRI * 512u, "triangle (20)", 0, 0.01, 0.09, 0.13},          /* 1/9 */
        {SD_WARP_ROW_SAW * 512u, "saw (40)", 0.42, 0.56, 0.27, 0.38},            /* 1/2, 1/3 */
        {65535, "pulse (99)", 0, 0.05, 0.25, 0.40}};                             /* 0, 1/3 */
    static int32_t w0[CYC], w1[CYC];
    double step[257], med, worst = 0, h1, h2, h3;
    uint32_t i, at = 0, n = 0, b;
    for (i = 0; i < 4u; i++) {
        op_cycle(A[i].w, 0, w0);
        h1 = harm(w0, 1); h2 = harm(w0, 2) / h1; h3 = harm(w0, 3) / h1;
        printf("warp %-14s h2/h1 %.3f h3/h1 %.3f\n", A[i].name, h2, h3);
        CHECK(h2 >= A[i].h2lo && h2 <= A[i].h2hi && h3 >= A[i].h3lo && h3 <= A[i].h3hi, "warp at %s is off", A[i].name);
    }
    /* continuity: consecutive steps of 1/256 move the waveform by similar, small amounts, the hand-overs included */
    op_cycle(0, 0, w0);
    for (i = 256; i <= 65536u; i += 256u, n++) {
        op_cycle(i > 65535u ? 65535u : i, 0, w1);
        step[n] = rms_diff(w0, w1);
        if (step[n] > worst) { worst = step[n]; at = i; }
        memcpy(w0, w1, sizeof w0);
    }
    med = step_median(step, n);
    printf("warp sweep: steps of 1/256 move the wave %.4f (median) .. %.4f (at %u) rms\n", med, worst, at);
    CHECK(worst < 0.04, "warp jumps %.3f rms at %u", worst, at);
    for (b = SD_WARP_ROW_TRI * 2u; b <= SD_WARP_ROW_SAW * 2u; b += (SD_WARP_ROW_SAW - SD_WARP_ROW_TRI) * 2u) {
        /* the steps that cross a hand-over, against the steps just before and after: a seam stands out */
        double around = step[b - 3u] > step[b + 1u] ? step[b - 3u] : step[b + 1u];
        printf("warp hand-over at %u/256: steps %.4f %.4f %.4f %.4f\n", b, step[b - 3u], step[b - 1u], step[b],
               step[b + 1u]);
        CHECK(step[b - 1u] < 2.0 * around && step[b] < 2.0 * around, "warp jumps at its hand-over %u/256", b);
    }
}

static void test_fold(void)
{
    static const uint32_t AT[] = {0, 19661, 42598, 55705, 65535};   /* 0, .30, .65, .85, 1 */
    static int32_t w0[CYC], w1[CYC];
    uint32_t i, c[5], n, at = 0;
    double step[257], worst = 0;
    for (i = 0; i < 5u; i++) { op_cycle(0, AT[i], w0); c[i] = crossings(w0); }
    printf("fold: crossings a cycle at 0 .30 .65 .85 1: %u %u %u %u %u\n", c[0], c[1], c[2], c[3], c[4]);
    CHECK(c[0] == 2u && c[1] == 2u, "fold up to .30 should only bend the peaks");
    CHECK(c[2] >= 4u && c[2] <= 6u, "fold at .65 should fold once or twice");
    CHECK(c[3] > c[2] && c[4] > c[3] && c[4] >= 14u, "fold should keep folding to the end");
    /* continuity: no step of 1/256 moves the wave much more than its neighbours do (the fold speeds up toward the
     * extreme region by design, so the test is local: a jump, not a fast region) */
    op_cycle(0, 0, w0);
    for (i = 256, n = 0; i <= 65536u; i += 256u, n++) {
        op_cycle(0, i > 65535u ? 65535u : i, w1);
        step[n] = rms_diff(w0, w1);
        memcpy(w0, w1, sizeof w0);
    }
    for (i = 1; i + 1u < n; i++) {
        double r = step[i] / (step[i - 1u] > step[i + 1u] ? step[i - 1u] : step[i + 1u]);
        if (r > worst) { worst = r; at = i; }
    }
    printf("fold sweep: steps of 1/256 move the wave %.4f (at .30) %.4f (.65) %.4f (.85) %.4f (1) rms; worst jump "
           "%.2f x its neighbours\n", step[76], step[166], step[217], step[n - 1u], worst);
    CHECK(worst < 2.0, "fold jumps at %u/256: %.2f x its neighbours", at + 1u, worst);
}

static void test_extremes(void)
{
    static const uint16_t V[] = {0, 32768, 65535};
    uint32_t x, y, al, md, note, k;
    int32_t worst = 0;
    for (x = 0; x < 3u; x++)
        for (y = 0; y < 3u; y++) {
            int32_t p = op_cycle(V[x], V[y], 0);
            if (p > worst) worst = p;
        }
    CHECK(worst <= 32767, "the operator reaches %d", worst);
    /* the whole engine: every algorithm in every bank, everything at max, the field's centre, every mode, lowest and
     * highest note. The bound: every carrier of every bank at full level on one side */
    for (al = 0; al < SD_ALGOS; al++)
        for (md = 0; md < SD_MODES; md++)
            for (note = 0; note < 128u; note += 127u) {
                SdPatch p = SD_PATCH_SINE;
                uint32_t i, b, carriers = 0;
                int32_t peak = 0;
                for (b = 0; b < SD_BANKS; b++) {
                    for (k = 0; k < SD_OPS; k++) {
                        p.bank[b].osc[k] = (SdOscPatch){(uint8_t)(32u - k - b), 99, sd_role_of(SD_ALGO[al][k]), 0, 65535};
                        carriers += SD_ALGO[al][k] == 'C';
                    }
                    p.bank[b].xm = p.bank[b].wp = p.bank[b].fd = p.bank[b].dt = 32767;
                }
                p.xm = p.warp = p.fold = p.dt = 65535;
                p.e2_warp = 32767; p.e2_fold = 32767; p.x = p.y = 32768; p.width = 65535;
                sd_engine_init(&e1);
                e1.mode = (uint8_t)md;
                sd_engine_patch(&e1, &p);
                for (i = 0; i < 4u; i++) sd_engine_note(&e1, note, 127);
                run(&e1, a, SD_FS / 4u, 256);
                for (i = 0; i < SD_FS / 2u; i++) { int32_t s = a[i] < 0 ? -a[i] : a[i]; if (s > peak) peak = s; }
                CHECK(peak <= (int32_t)carriers * (127 * 128 + 64), "%s mode %u note %u all at max: peak %d", SD_ALGO[al],
                      md, note, peak);
            }
}

/* PM against the formula: a 1:1 pair, x = cos(phi + beta cos(phi)), beta = 4 pi * level */
static void test_pm(void)
{
    static const uint16_t L[] = {5215, 16384, 32768, 65535};   /* beta ~ 1, pi, 2 pi, 4 pi rad */
    static int32_t w[CYC];
    uint32_t t, i;
    for (t = 0; t < 4u; t++) {
        SdPatch p = SD_PATCH_SINE;
        double worst = 0, beta = 4.0 * M_PI * (((L[t] >> 1) * 65535u) >> 16) / 32768.0;   /* level x XM */
        p.bank[0].osc[0] = (SdOscPatch){1, 0, SD_ROLE_S, 0, L[t]};
        p.bank[0].osc[1] = (SdOscPatch){1, 0, SD_ROLE_C, 0, 65535};
        p.xm = 65535;
        bank_cycle(&p, w);
        for (i = 0; i < CYC; i++) {
            double ph = 2 * M_PI * (i + 1u) / CYC, x = 32767.0 * 32767.0 / 32768.0 * cos(ph + beta * cos(ph));
            double d = fabs(w[i] - x) / 32767.0;
            if (d > worst) worst = d;
        }
        CHECK(worst < 0.005, "PM beta %.2f rad: %.4f off the formula", beta, worst);
    }
}

/* the roles, left to right: S feeds the oscillator just right of it, P the nearest carrier right of it */
static void test_routing(void)
{
    static const struct { const char *roles; uint8_t tgt[SD_OPS]; } R[] = {
        {"SSSC", {1, 2, 3, SD_CARRIER}}, {"PSSC", {3, 2, 3, SD_CARRIER}}, {"PPPC", {3, 3, 3, SD_CARRIER}},
        {"SCSC", {1, SD_CARRIER, 3, SD_CARRIER}}, {"PPCC", {2, 2, SD_CARRIER, SD_CARRIER}},
        {"CSSC", {SD_CARRIER, 2, 3, SD_CARRIER}}, {"SCCC", {1, SD_CARRIER, SD_CARRIER, SD_CARRIER}},
        {"CCCC", {SD_CARRIER, SD_CARRIER, SD_CARRIER, SD_CARRIER}},
        {"CCCS", {SD_CARRIER, SD_CARRIER, SD_CARRIER, SD_NOWHERE}}, {"PCCP", {1, SD_CARRIER, SD_CARRIER, SD_NOWHERE}}};
    uint32_t r, k;
    for (r = 0; r < sizeof R / sizeof R[0]; r++) {
        SdPatch p = SD_PATCH_SINE;
        for (k = 0; k < SD_OPS; k++) p.bank[0].osc[k].role = sd_role_of(R[r].roles[k]);
        memset(&tb, 0, sizeof tb);
        memset(&tv, 0, sizeof tv);
        sd_bank_params(&tb, &p, &p.bank[0], &tv, 1, 65536u);
        for (k = 0; k < SD_OPS; k++)
            CHECK(tb.osc[k].tgt == R[r].tgt[k], "%s: oscillator %u feeds %u, not %u", R[r].roles, k + 1u, tb.osc[k].tgt,
                  R[r].tgt[k]);
    }
}

/* the patch hand-over: the audio never takes a patch the main loop is halfway through */
static void test_patch_lock(void)
{
    int32_t o[2u * SD_SUB];
    sd_engine_init(&e1);
    e1.patch_seq = 1;                           /* the main loop is writing */
    e1.patch_in.bank[0].osc[0].level = 1234;
    run(&e1, o, SD_SUB, SD_SUB);
    CHECK(e1.patch.bank[0].osc[0].level == 65535, "the audio took a patch while it was being written");
    e1.patch_seq = 2;
    run(&e1, o, SD_SUB, SD_SUB);
    CHECK(e1.patch.bank[0].osc[0].level == 1234, "the audio did not take the finished patch");
}

static void test_env(void)
{
    /* attack 64 = 1 ms * 10000^(64/127) = 104 ms to full; release 64 = 104 ms to 1 % */
    uint32_t t, ticks;
    double ms;
    SdPatch p = SD_PATCH_SINE;
    p.e1_a = 64; p.e1_r = 64;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 60, 127);
    for (t = 0; t < 2000u && e1.voice[0].env1.stage != SD_ENV_DECAY; t++) { int32_t o[2u * SD_SUB]; run(&e1, o, SD_SUB, SD_SUB); }
    ms = t * SD_SUB * 1000.0 / SD_FS;
    CHECK(fabs(ms - 104.0) < 6.0, "attack 64 took %.1f ms (104)", ms);
    sd_engine_note(&e1, 60, 0);
    for (ticks = 0; ticks < 4000u && e1.voice[0].env1.lvl > SD_ONE / 100; ticks++) { int32_t o[2u * SD_SUB]; run(&e1, o, SD_SUB, SD_SUB); }
    ms = ticks * SD_SUB * 1000.0 / SD_FS;
    CHECK(fabs(ms - 104.0) < 6.0, "release 64 took %.1f ms to 1 %% (104)", ms);
}

/* ---- V3 */
static void test_vector(void)                   /* the weights: corners, centre, DUO, POLY */
{
    static const uint16_t CX[4] = {0, 65535, 65535, 0}, CY[4] = {65535, 65535, 0, 0};   /* A B C D */
    int32_t w[SD_BANKS];
    uint32_t c, b;
    for (c = 0; c < 4u; c++) {
        sd_engine_init(&e1);
        e1.patch.x = CX[c]; e1.patch.y = CY[c];
        sd_vector(&e1, w);
        for (b = 0; b < SD_BANKS; b++)
            CHECK(b == c ? w[b] >= 32700 : w[b] == 0, "corner %c: bank %c weighs %d", 'A' + c, 'A' + b, w[b]);
    }
    sd_engine_init(&e1);
    e1.patch.x = e1.patch.y = 32768;
    sd_vector(&e1, w);
    for (b = 0; b < SD_BANKS; b++)
        CHECK(w[b] > 11585 - 120 && w[b] < 11585 + 120, "centre: bank %c weighs %d, not 0.354", 'A' + b, w[b]);
    e1.mode_on = SD_MODE_DUO;
    sd_vector(&e1, w);
    CHECK(w[0] == w[2] && w[1] == w[3] && w[0] > 19483 - 120 && w[0] < 19483 + 120, "DUO at X 0.5: %d %d", w[0], w[1]);
    e1.mode_on = SD_MODE_POLY;
    sd_vector(&e1, w);
    CHECK(w[0] == SD_POLY_GAIN && w[3] == SD_POLY_GAIN, "POLY: %d", w[0]);
}

static void test_vec2(void)                     /* TOUR's VEC2: from the gate, A -> B -> C -> D, 1.5 s each, then held */
{
    static const struct { double s; uint16_t x, y; } AT[] = {{0.02, 0, 65535}, {1.5, 65535, 65535}, {3.0, 65535, 0},
                                                            {4.5, 0, 0}, {6.0, 0, 0}};
    uint32_t i, done = 0;
    SdPatch p = SD_PATCH[5];
    memset(p.mod, 0, sizeof p.mod);             /* VEC2 alone (TOUR's LFOs move X and Y on top: C1) */
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 45, 100);
    for (i = 0; i < 5u; i++) {
        uint32_t to = (uint32_t)(AT[i].s * SD_FS);
        int32_t o[2u * SD_SUB];
        while (done < to) { run(&e1, o, SD_SUB, SD_SUB); done += SD_SUB; }
        CHECK(abs((int)e1.vx - AT[i].x) < 1200 && abs((int)e1.vy - AT[i].y) < 1200, "VEC2 at %.2f s: %u %u, not %u %u",
              AT[i].s, e1.vx, e1.vy, AT[i].x, AT[i].y);
    }
}

static void test_modes(void)                    /* POLY and DUO allocate and steal; a mode waits for quiet */
{
    static const uint8_t CH[5] = {48, 52, 55, 59, 62};
    int32_t o[2u * SD_SUB];
    uint32_t i;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &SD_PATCH[1]);
    sd_engine_note(&e1, 60, 100);
    run(&e1, o, SD_SUB, SD_SUB);
    e1.mode = SD_MODE_POLY;
    run(&e1, o, SD_SUB, SD_SUB);
    CHECK(e1.mode_on == SD_MODE_MONO, "the mode changed while a voice still sounded");
    for (i = 0; i < 8000u && e1.mode_on != SD_MODE_POLY; i++) run(&e1, o, SD_SUB, SD_SUB);   /* BELL rings ~1.5 s */
    CHECK(e1.mode_on == SD_MODE_POLY && e1.nvoices == 4u, "POLY never came (mode %u)", e1.mode_on);
    for (i = 0; i < 4u; i++) { sd_engine_note(&e1, CH[i], 100); run(&e1, o, SD_SUB, SD_SUB); }
    {   /* every note on its own voice, held; a fifth takes the oldest (the first note's) */
        uint32_t seen = 0, k;
        for (i = 0; i < 4u; i++)
            for (k = 0; k < 4u; k++)
                if (e1.voice[i].note == CH[k] && e1.voice[i].gate) seen |= 1u << k;
        CHECK(seen == 15u, "POLY: the four notes are not on four held voices (%x)", seen);
        sd_engine_note(&e1, CH[4], 100);
        run(&e1, o, SD_SUB, SD_SUB);
        for (i = 0, seen = 0; i < 4u; i++)
            for (k = 1; k < 5u; k++)
                if (e1.voice[i].note == CH[k]) seen |= 1u << k;
        CHECK(seen == 30u, "a fifth note did not take the oldest voice (%x)", seen);
    }
    for (i = 0; i < SD_BANKS; i++) CHECK(e1.bank[i].voice == i && e1.bank[i].src == 0, "POLY bank %u map", i);
    e1.mode = SD_MODE_DUO;
    for (i = 0; i < 5u; i++) sd_engine_note(&e1, CH[i], 0);
    for (i = 0; i < 8000u && e1.mode_on != SD_MODE_DUO; i++) run(&e1, o, SD_SUB, SD_SUB);
    CHECK(e1.mode_on == SD_MODE_DUO && e1.nvoices == 2u && e1.bank[2].voice == 1 && e1.bank[3].src == 1, "DUO map");
}

static void test_stereo(void)                   /* WIDTH 0: both sides the same; corner A wide: A leans left */
{
    uint32_t i;
    double l = 0, r = 0;
    int same = 1;
    SdPatch p = SD_PATCH[5];
    p.vec = 0;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &SD_PATCH[0]);
    sd_engine_note(&e1, 57, 100);
    run(&e1, a, SD_FS / 4u, 256);
    for (i = 0; i < SD_FS / 4u; i++) same &= a[2u * i] == a[2u * i + 1u];
    CHECK(same, "WIDTH 0 is not mono");
    p.width = 32768;                            /* half: the right side of bank A at 0.5 */
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 57, 100);
    run(&e1, a, SD_FS / 4u, 256);
    for (i = SD_FS / 8u; i < SD_FS / 4u; i++) { l += (double)a[2u * i] * a[2u * i]; r += (double)a[2u * i + 1u] * a[2u * i + 1u]; }
    CHECK(fabs(sqrt(r / l) - 0.5) < 0.02, "bank A at WIDTH 0.5: right / left %.3f, not 0.5", sqrt(r / l));
}

/* the aliasing tracking: a saw at C7 aliases less with it, and below the knee nothing changes */
static double off_series_db(const int32_t *x, uint32_t n, double f0)   /* energy off the harmonics, dB of all */
{
    double on = 0, off = 0;
    uint32_t k, i;
    for (k = 1; k < n / 2u; k += 1u) {
        double re = 0, im = 0, f = (double)k * SD_FS / n, h = f / f0, dev = fabs(h - floor(h + 0.5)) * f0;
        for (i = 0; i < n; i++) {
            double w = 0.5 - 0.5 * cos(2 * M_PI * i / n);
            re += x[2u * i] * w * cos(2 * M_PI * k * i / n);
            im += x[2u * i] * w * sin(2 * M_PI * k * i / n);
        }
        if (dev < 2.0 * SD_FS / n) on += re * re + im * im;
        else off += re * re + im * im;
    }
    return 10 * log10(off / (on + off));
}

static void test_alias(void)
{
    static const uint32_t NOTE[2] = {45, 96};
    double db[2][2];
    uint32_t n, t;
    SdPatch p = SD_PATCH_SINE;
    p.warp = SD_WARP_ROW_SAW * 512u;            /* the saw */
    for (n = 0; n < 2u; n++)
        for (t = 0; t < 2u; t++) {
            sd_engine_init(&e1);
            e1.track = (uint8_t)t;
            sd_engine_patch(&e1, &p);
            sd_engine_note(&e1, NOTE[n], 127);
            run(&e1, n ? b : a, 8192u + 4096u, 256);
            db[n][t] = off_series_db((n ? b : a) + 2u * 4096u, 2048u, 440.0 * pow(2.0, (NOTE[n] - 69.0) / 12.0));
        }
    printf("aliasing (energy off the harmonics): A2 %.1f / %.1f dB, C7 %.1f / %.1f dB (tracking off / on)\n", db[0][0],
           db[0][1], db[1][0], db[1][1]);
    CHECK(fabs(db[0][0] - db[0][1]) < 0.01, "the tracking changed a note below its knee");
    CHECK(db[1][1] < db[1][0] - 6.0, "the tracking did not take 6 dB off C7's aliasing");
}

static void test_gen(void)                      /* the generators */
{
    SdPatch p = SD_PATCH[1];
    uint32_t seed = 1, g, k, i;
    sd_gen(&p, 1, 0, &seed);
    CHECK(p.bank[1].osc[0].harm == 1 && p.bank[1].osc[3].harm == 4, "ALL");
    CHECK(p.bank[1].osc[1].level == SD_PATCH[1].bank[1].osc[1].level, "ALL changed a level");
    sd_gen(&p, 1, 1, &seed);
    CHECK(p.bank[1].osc[1].harm == 3 && p.bank[1].osc[3].harm == 7, "ODD");
    for (i = 0; i < 200u; i++)
        for (g = 2; g < 4u; g++) {
            sd_gen(&p, 2, g, &seed);
            for (k = 0; k < SD_OPS; k++) CHECK(p.bank[2].osc[k].harm >= 1 && p.bank[2].osc[k].harm <= 32, "%s: %u", sd_gen_name(g), p.bank[2].osc[k].harm);
        }
    sd_gen(&p, 3, 4, &seed);
    CHECK(p.bank[3].osc[0].harm == 4 && p.bank[3].osc[1].harm == 5 && p.bank[3].osc[2].harm == 6, "MAJ");
    for (g = 4; g < 4u + SD_CHORDS; g++) {
        sd_gen(&p, 0, g, &seed);
        for (k = 0; k < SD_OPS; k++) CHECK(p.bank[0].osc[k].harm <= 32 && p.bank[0].osc[k].harm >= 1, "%s", sd_gen_name(g));
    }
    sd_gen(&p, 0, SD_GENS - 1u, &seed);
    for (k = 1; k < SD_BANKS; k++) CHECK(!memcmp(&p.bank[k], &p.bank[0], sizeof p.bank[0]), "CPY to bank %u", k);
}

/* ---- R1, the Rider */
static uint16_t midi256(double hz) { return (uint16_t)lround(256.0 * (69.0 + 12.0 * log2(hz / 440.0))); }

/* one filter of the Rider alone, its centre at hz: the gain (dB) of a sine at test_hz, after it settles */
static double rider_gain(uint32_t model, uint16_t res, double hz, double test_hz)
{
    static int32_t in[2u * SD_SUB], out[2u * SD_SUB];
    SdPatch p = SD_PATCH_SINE;
    uint32_t i, b, k;
    double ph = 0, a2 = 0, o2 = 0;
    for (k = 0; k < 6u; k++) p.stage[k].pitch[0] = midi256(hz), p.stage[k].level[0] = 32768;
    p.r_model = (uint8_t)model; p.r_res = res; p.r_drive = 16384; p.r_mix = 65535;
    sd_engine_init(&e1);
    e1.patch = p;
    sd_rider_params(&e1);
    for (b = 0; b < 3000u; b++) {               /* 2.2 s: the last 0.7 s measured */
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = (int32_t)lround(1000.0 * sin(ph));
            ph += 2 * M_PI * test_hz / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            out[2u * i] = out[2u * i + 1u] = 0;
        }
        sd_rider_run(&e1, in, out, SD_SUB, 0);
        if (b >= 2000u)
            for (i = 0; i < SD_SUB; i++) { a2 += (double)in[2u * i] * in[2u * i]; o2 += (double)out[2u * i] * out[2u * i]; }
    }
    return 10 * log10(o2 / a2) - 20 * log10(SD_RMAKEUP[model] / 4096.0);   /* the shape, without the make-up */
}

static void test_rider_response(void)
{
    static const double HZ[] = {100, 440, 2000, 8000, 13000};
    uint32_t h, m;
    for (h = 0; h < 5u; h++) {
        double best = -1e9, at = 0, f, c;
        for (f = HZ[h] * 0.8; f <= HZ[h] * 1.25; f *= 1.005) {   /* the peak, VBP at a high RES */
            double g = rider_gain(SD_RM_VBP, 52000, HZ[h], f);
            if (g > best) { best = g; at = f; }
        }
        c = rider_gain(SD_RM_VBP, 52000, HZ[h], HZ[h]);
        printf("rider VBP at %5.0f Hz: peak at %7.1f Hz (%+.2f dB); an octave off %+.1f dB\n", HZ[h], at, best,
               rider_gain(SD_RM_VBP, 52000, HZ[h], HZ[h] * (HZ[h] < 10000 ? 2.0 : 0.5)));
        CHECK(fabs(at / HZ[h] - 1) < 0.02, "rider VBP at %.0f Hz peaks at %.1f Hz", HZ[h], at);
        CHECK(fabs(c) < 0.5, "rider VBP at %.0f Hz: %.2f dB at its centre, not 0", HZ[h], c);
    }
    for (m = 0; m <= SD_RM_VBP; m++) {          /* each band-pass model: a band, not a shelf */
        double c = rider_gain(m, 30000, 1000, 1000), lo = rider_gain(m, 30000, 1000, 250), hi = rider_gain(m, 30000, 1000, 4000);
        printf("rider model %u at 1 kHz: centre %+.1f, 250 Hz %+.1f, 4 kHz %+.1f dB\n", m, c, lo, hi);
        CHECK(c > lo + 9 && c > hi + 9, "rider model %u is not a band-pass", m);
    }
    CHECK(rider_gain(SD_RM_BP12, 30000, 1000, 4000) < rider_gain(SD_RM_BP6, 30000, 1000, 4000) - 6,
          "BP12 is not steeper than BP6");
    CHECK(rider_gain(SD_RM_BP6, 60000, 1000, 1000) > rider_gain(SD_RM_BP6, 20000, 1000, 1000) + 10,
          "BP6 does not grow louder as it rings");
}

/* ---- R2: the delay models and the allpasses */
static SdPatch rider_patch(uint32_t model, uint16_t res, double hz)   /* all four filters at hz, level 1 */
{
    SdPatch p = SD_PATCH_SINE;
    uint32_t k, f;
    for (k = 0; k < 6u; k++)
        for (f = 0; f < 4u; f++) p.stage[k].pitch[f] = midi256(hz), p.stage[k].level[f] = 32768;
    p.r_model = (uint8_t)model; p.r_res = res; p.r_drive = 16384; p.r_mix = 65535;
    return p;
}

/* the Rider alone on a test signal: an impulse (sine_hz 0) or a sine; frames of output, left / right */
static void rider_feed(const SdPatch *p, double sine_hz, uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    double ph = 0;
    sd_engine_init(&e1);
    e1.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_rider_params(&e1);
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = sine_hz > 0 ? (int32_t)lround(1000.0 * sin(ph)) : (b == 0 && i == 0 ? 20000 : 0);
            ph += 2 * M_PI * sine_hz / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_rider_run(&e1, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
    }
}

static double ac_pitch(const int32_t *x, uint32_t n, double guess)   /* autocorrelation near a guessed period */
{
    double best = -1e300, at = 0;
    uint32_t lo = (uint32_t)(SD_FS / guess * 0.9), hi = (uint32_t)(SD_FS / guess * 1.1) + 1u, l, i;
    double r[4096];
    for (l = lo - 1u; l <= hi + 1u; l++) {
        double s = 0;
        for (i = 0; i + l < n; i++) s += (double)x[2u * i] * x[2u * (i + l)];
        r[l - lo + 1u] = s;
    }
    for (l = lo; l <= hi; l++)
        if (r[l - lo + 1u] > best) { best = r[l - lo + 1u]; at = l; }
    {                                           /* a parabola through the peak: the period between samples */
        double y0 = r[(uint32_t)at - lo], y1 = r[(uint32_t)at - lo + 1u], y2 = r[(uint32_t)at - lo + 2u];
        at += 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2);
    }
    return SD_FS / at;
}

static void test_rider_kp(void)
{
    static const double HZ[] = {55, 110, 220, 440, 880, 1760};
    uint32_t h;
    double worst = 0, t1, t2;
    for (h = 0; h < 6u; h++) {                  /* pinged: the comb's pitch, in cents off */
        SdPatch p = rider_patch(SD_RM_KP, 60000, HZ[h]);
        double f, c;
        rider_feed(&p, 0, SD_FS / 2u, a);
        f = ac_pitch(a + 2u * 2000u, SD_FS / 2u - 2000u, HZ[h]);
        c = 1200.0 * log2(f / HZ[h]);
        if (fabs(c) > worst) worst = fabs(c);
        printf("rider KP at %6.1f Hz: rings at %8.2f Hz (%+.1f cents)\n", HZ[h], f, c);
    }
    CHECK(worst < 5.0, "KP is %.1f cents off its pitch", worst);
    {                                           /* RES: a longer ring */
        SdPatch p = rider_patch(SD_RM_KP, 40000, 220);
        uint32_t i;
        double e0 = 0, e1v = 0;
        rider_feed(&p, 0, SD_FS, a);
        for (i = 0; i < 4410u; i++) e0 += (double)a[2u * i] * a[2u * i], e1v += (double)a[2u * (i + 22050u)] * a[2u * (i + 22050u)];
        t1 = 10 * log10(e1v / e0);
        p = rider_patch(SD_RM_KP, 62000, 220);
        rider_feed(&p, 0, SD_FS, a);
        e0 = e1v = 0;
        for (i = 0; i < 4410u; i++) e0 += (double)a[2u * i] * a[2u * i], e1v += (double)a[2u * (i + 22050u)] * a[2u * (i + 22050u)];
        t2 = 10 * log10(e1v / e0);
        printf("rider KP at 220 Hz: after 0.5 s %.1f dB (RES 61) / %.1f dB (RES 94)\n", t1, t2);
        CHECK(t2 > t1 + 10, "KP: RES does not lengthen the ring");
    }
}

static void test_rider_ap(void)                 /* RES 0: flat; RES max: notches, left and right apart */
{
    static const double HZ[] = {200, 700, 1500, 3000, 6000};
    uint32_t m, h, i;
    for (m = SD_RM_AP1; m <= SD_RM_AP2; m++) {
        double flat = 0, top = 0, lmin = 1e9, rmin = 1e9, lmax = -1e9;
        for (h = 0; h < 5u; h++) {
            SdPatch p = rider_patch(m, 0, 1000);
            double ein = 0, el = 0;
            rider_feed(&p, HZ[h], SD_FS / 2u, a);
            for (i = SD_FS / 4u; i < SD_FS / 2u; i++) { double x = 1000.0 * sin(2 * M_PI * HZ[h] * i / SD_FS); ein += x * x; el += (double)a[2u * i] * a[2u * i]; }
            double g = 10 * log10(el / ein) - 20 * log10(SD_RMAKEUP[m] / 4096.0);   /* without the make-up */
            if (h == 4u) top = g;               /* 6 kHz: AP1's linear reads in its loops dip there */
            else if (fabs(g) > flat) flat = fabs(g);
        }
        for (h = 0; h < 120u; h++) {            /* RES max: sweep 100 Hz .. 10 kHz */
            double hz = 100 * pow(100.0, h / 119.0), ein = 0, el = 0, er = 0, gl, gr;
            SdPatch p = rider_patch(m, 65535, 1000);
            p.stage[0].pitch[1] = midi256(1300); p.stage[0].pitch[2] = midi256(2100); p.stage[0].pitch[3] = midi256(3400);
            rider_feed(&p, hz, SD_FS / 4u, a);
            for (i = SD_FS / 8u; i < SD_FS / 4u; i++) {
                double x = 1000.0 * sin(2 * M_PI * hz * i / SD_FS);
                ein += x * x; el += (double)a[2u * i] * a[2u * i]; er += (double)a[2u * i + 1u] * a[2u * i + 1u];
            }
            gl = 10 * log10(el / ein + 1e-12); gr = 10 * log10(er / ein + 1e-12);
            if (gl < lmin) lmin = gl;
            if (gr < rmin) rmin = gr;
            if (gl > lmax) lmax = gl;
        }
        printf("rider AP%u: RES 0 within %.2f dB of flat to 3 kHz (6 kHz %+.1f dB); RES max: left %.1f .. %+.1f dB, "
               "right down to %.1f dB\n", m - SD_RM_AP1 + 1u, flat, top, lmin, lmax, rmin);
        CHECK(flat < 1.5, "AP%u at RES 0 is %.2f dB off flat", m - SD_RM_AP1 + 1u, flat);
        CHECK(lmin < -15 && rmin < -15 && lmax > 3, "AP%u at RES max has no notches", m - SD_RM_AP1 + 1u);
    }
}

static void test_rider_rest(void)               /* a comb rings out, then rests: no work while silent */
{
    SdPatch p = rider_patch(SD_RM_KP, 60000, 220);
    uint32_t t;
    int32_t o[2u * SD_SUB];
    p.e1_r = 30;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 57, 100);
    for (t = 0; t < 100u; t++) run(&e1, o, SD_SUB, SD_SUB);
    CHECK(!e1.r.rest, "the Rider rests while a note sounds");
    sd_engine_note(&e1, 57, 0);
    for (t = 0; t < 12000u && !e1.r.rest; t++) run(&e1, o, SD_SUB, SD_SUB);   /* RES 92: a long ring */
    printf("rider KP rests %.2f s after the release\n", t * SD_SUB / (double)SD_FS);
    CHECK(e1.r.rest, "the Rider never rests after a comb rang out");
}

/* ---- R3: each model's level against the dry voice: a saw through VOWEL's stage A, RES and level one half */
static double rider_vs_dry(uint32_t model)
{
    SdPatch p = SD_PATCH[7];
    uint32_t i;
    double d = 0, w = 0;
    p.r_div = 0; p.r_pos = 0; p.r_slew = 0; p.e1_s = 127;
    memset(p.mod, 0, sizeof p.mod);             /* the model alone (PLUCKS's RAND moves POS: C1) */
    for (i = 0; i < 4u; i++) p.stage[0].level[i] = 32768;
    p.r_model = (uint8_t)model; p.r_res = 32768; p.r_drive = 16384;
    p.r_mix = 0;
    sd_engine_init(&e1); sd_engine_patch(&e1, &p); sd_engine_note(&e1, 45, 100);
    run(&e1, a, SD_FS, 256);
    p.r_mix = 65535;
    sd_engine_init(&e2); sd_engine_patch(&e2, &p); sd_engine_note(&e2, 45, 100);
    run(&e2, b, SD_FS, 256);
    for (i = SD_FS / 2u; i < SD_FS; i++) {
        d += ((double)a[2u * i] * a[2u * i] + (double)a[2u * i + 1u] * a[2u * i + 1u]);
        w += ((double)b[2u * i] * b[2u * i] + (double)b[2u * i + 1u] * b[2u * i + 1u]);
    }
    return 10 * log10(w / d);
}

static void test_rider_makeup(void)             /* switching model keeps the level: each within 3 dB of the dry */
{
    static const char *const MN[SD_RMODELS] = {"BP6", "BP12", "VBP", "KP", "AP1", "AP2"};
    uint32_t m;
    printf("rider against the dry voice:");
    for (m = 0; m < SD_RMODELS; m++) {
        double g = rider_vs_dry(m);
        printf(" %s %+.1f", MN[m], g);
        CHECK(fabs(g) < 3.0, "rider %s is %+.1f dB from the dry voice", MN[m], g);
    }
    printf(" dB\n");
}

static double h3_of(const int32_t *x, uint32_t n, double f0)   /* the 3rd harmonic against the 1st (left) */
{
    double r1 = 0, i1 = 0, r3 = 0, i3 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n), v = x[2u * i] * w;
        r1 += v * cos(2 * M_PI * f0 * i / SD_FS); i1 += v * sin(2 * M_PI * f0 * i / SD_FS);
        r3 += v * cos(6 * M_PI * f0 * i / SD_FS); i3 += v * sin(6 * M_PI * f0 * i / SD_FS);
    }
    return sqrt(r3 * r3 + i3 * i3) / sqrt(r1 * r1 + i1 * i1);
}

static void test_rider_saturation(void)         /* clean while small; DRIVE and RES ring into the loop's ceiling */
{
    static int32_t in[2u * SD_SUB];
    SdPatch p = rider_patch(SD_RM_BP6, 52000, 1000);
    uint32_t b, i, d, k;
    double h[2], pk[2], ph;
    for (d = 0; d < 2u; d++) {
        ph = 0; pk[d] = 0;
        p.r_drive = d ? 65535 : 16384;
        for (k = 0; k < 6u; k++) p.stage[k].level[0] = 16384, p.stage[k].level[1] = p.stage[k].level[2] = p.stage[k].level[3] = 0;
        sd_engine_init(&e1);
        e1.patch = p;
        for (b = 0; b < 2000u; b++) {
            sd_rider_params(&e1);
            for (i = 0; i < SD_SUB; i++) {
                int32_t x = (int32_t)lround((d ? 8000.0 : 300.0) * sin(ph));
                ph += 2 * M_PI * 1000.0 / SD_FS;
                in[2u * i] = in[2u * i + 1u] = x;
                a[2u * (b * SD_SUB + i)] = a[2u * (b * SD_SUB + i) + 1u] = 0;
            }
            sd_rider_run(&e1, in, a + 2u * b * SD_SUB, SD_SUB, 0);
        }
        h[d] = h3_of(a + 2u * 40000u, 16384u, 1000.0);
        for (i = 40000u; i < 56384u; i++) if (abs(a[2u * i]) > pk[d]) pk[d] = abs(a[2u * i]);
        for (i = 0; i < 4u; i++) CHECK(abs(e1.r.ic1[0][i]) < (16 << 19), "an integrator past 16 x full scale");
    }
    printf("rider BP6 at RES 79: a quiet sine h3/h1 %.4f %%; DRIVE 4x on a loud one %.1f %% (peak %.0f)\n", 100 * h[0],
           100 * h[1], pk[1]);
    CHECK(h[0] < 0.001, "the Rider is not clean while small: h3/h1 %.4f", h[0]);
    CHECK(h[1] > 0.03, "the Rider does not saturate when driven: h3/h1 %.4f", h[1]);
    CHECK(SD_RLEVEL[64] == 4096 && SD_RLEVEL[128] == 16384 && SD_RLEVEL[0] == 0, "the level law's anchors");
    for (i = 1; i < 129u; i++) CHECK(SD_RLEVEL[i] > SD_RLEVEL[i - 1u], "the level law falls at %u", i);
}

/* ---- D1: DESTROY's crossover, bias, shapers */
static SdPatch destroy_patch(uint32_t type, uint16_t gain, uint16_t alt, int16_t bias)   /* every band alike */
{
    SdPatch p = SD_PATCH_SINE;
    uint32_t k;
    p.d_split[0] = midi256(150); p.d_split[1] = midi256(800); p.d_split[2] = midi256(4000);
    for (k = 0; k < 4u; k++) { p.d_type[k] = (uint8_t)type; p.d_gain[k] = gain; p.d_alt[k] = alt; p.d_bias[k] = bias; p.d_level[k] = 32768; }
    p.d_mix = 65535;
    return p;
}

/* DESTROY alone on a sine of amp at hz: frames of output (left / right), after a settle */
static void destroy_feed(const SdPatch *p, double hz, double amp, uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    double ph = 0;
    sd_engine_init(&e1);
    e1.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_destroy_params(&e1);
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = (int32_t)lround(amp * sin(ph));
            ph += 2 * M_PI * hz / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_destroy_run(&e1, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
    }
}

static double harm_db(const int32_t *x, uint32_t n, double f, uint32_t h)   /* the h-th harmonic, dB of the 1st */
{
    double r[2] = {0, 0}, q[2] = {0, 0};
    uint32_t i, j;
    for (i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n), v = x[2u * i] * w;
        for (j = 0; j < 2u; j++) {
            double hh = j ? h : 1;
            r[j] += v * cos(2 * M_PI * hh * f * i / SD_FS); q[j] += v * sin(2 * M_PI * hh * f * i / SD_FS);
        }
    }
    return 20 * log10(sqrt(r[1] * r[1] + q[1] * q[1]) / sqrt(r[0] * r[0] + q[0] * q[0]) + 1e-12);
}

static void test_destroy(void)
{
    static const double HZ[] = {60, 300, 1800, 9000};       /* a band's middle each, splits 150 800 4000 */
    uint32_t i, h, k, t, x;
    for (x = SD_DX_CMP; x <= SD_DX_LR4; x++) {   /* both crossovers: they sum back; how much the bands bleed */
    {   /* neutral: the bands sum to the input (the DC blockers aside) */
        double worst = 0;
        for (h = 0; h < 4u; h++) {
            SdPatch p = destroy_patch(SD_DS_NONE, 0, 0, 0);
            p.d_xover = (uint8_t)x;
            double ein = 0, eo = 0;
            destroy_feed(&p, HZ[h] < 100 ? 200 : HZ[h], 8000, SD_FS / 2u, a);
            for (i = SD_FS / 4u; i < SD_FS / 2u; i++) { double x = 8000 * sin(2 * M_PI * (HZ[h] < 100 ? 200 : HZ[h]) * i / SD_FS); ein += x * x; eo += (double)a[2u * i] * a[2u * i]; }
            if (fabs(10 * log10(eo / ein)) > worst) worst = fabs(10 * log10(eo / ein));
        }
        printf("destroy %s neutral: the bands sum to the input within %.3f dB\n", x ? "LR4" : "CMP", worst);
        CHECK(worst < 0.05, "DESTROY's bands do not sum back: %.3f dB", worst);
    }
    {   /* the slope: each band alone (the others' level 0), a sine at each band's middle: how much of it it passes */
        printf("destroy %s bands (rows: a sine at 60 300 1800 9000 Hz; columns: band 1..4 alone, dB)\n", x ? "LR4" : "CMP");
        for (h = 0; h < 4u; h++) {
            printf("   %5.0f Hz:", HZ[h]);
            for (k = 0; k < 4u; k++) {
                SdPatch p = destroy_patch(SD_DS_NONE, 0, 0, 0);
                double ein = 0, eo = 0;
                p.d_xover = (uint8_t)x;
                for (t = 0; t < 4u; t++) p.d_level[t] = t == k ? 32768 : 0;
                destroy_feed(&p, HZ[h], 8000, SD_FS / 2u, a);
                for (i = SD_FS / 4u; i < SD_FS / 2u; i++) { double x = 8000 * sin(2 * M_PI * HZ[h] * i / SD_FS); ein += x * x; eo += (double)a[2u * i] * a[2u * i]; }
                printf(" %6.1f", 10 * log10(eo / ein + 1e-12));
                if (k == h) CHECK(10 * log10(eo / ein) > -3.5, "band %u passes its own middle at %.1f dB", k + 1u, 10 * log10(eo / ein));
                else if (k + 1u < h || k > h + 1u) CHECK(10 * log10(eo / ein) < -15, "band %u leaks %.1f dB at %.0f Hz", k + 1u, 10 * log10(eo / ein), HZ[h]);
            }
            printf("\n");
        }
    }
    }
    {   /* the shapers at +24 dB on a 1 kHz sine of 0.25: each its own colour */
        static const char *const SN[SD_DSHAPES] = {"NONE", "SOFT", "HARD", "FOLD", "CRUSH"};
        for (t = SD_DS_SOFT; t < SD_DSHAPES; t++) {
            SdPatch p = destroy_patch(t, 43690, 32768, 0);
            double h2, h3;
            int32_t peak = 0;
            destroy_feed(&p, 1000, 8000, SD_FS / 2u, a);
            h2 = harm_db(a + 2u * 8192u, 8192u, 1000, 2); h3 = harm_db(a + 2u * 8192u, 8192u, 1000, 3);
            for (i = 8192u; i < 16384u; i++) if (abs(a[2u * i]) > peak) peak = abs(a[2u * i]);
            printf("destroy %-5s +24 dB ALT 50: h2 %6.1f h3 %6.1f dB, peak %d\n", SN[t], h2, h3, peak);
            CHECK(h3 > -30, "%s does not distort", SN[t]);
            CHECK(peak <= 4 * 32768, "%s reaches %d", SN[t], peak);
            if (t == SD_DS_SOFT) CHECK(h2 > -40, "SOFT is not asymmetric: h2 %.1f dB", h2);
            if (t == SD_DS_HARD) CHECK(h2 < -50, "HARD is not symmetric: h2 %.1f dB", h2);
        }
    }
    {   /* BIAS: before HARD, even harmonics; after it no DC */
        SdPatch p = destroy_patch(SD_DS_HARD, 43690, 0, 20000);
        double dc = 0;
        destroy_feed(&p, 1000, 8000, SD_FS, a);
        for (i = SD_FS / 2u; i < SD_FS; i++) dc += a[2u * i];
        dc /= SD_FS / 2u;
        printf("destroy HARD with BIAS +0.3: h2 %.1f dB, DC %.1f\n", harm_db(a + 2u * 24000u, 8192u, 1000, 2), dc);
        CHECK(harm_db(a + 2u * 24000u, 8192u, 1000, 2) > -25, "BIAS makes no even harmonics");
        CHECK(fabs(dc) < 30, "a DC of %.1f after the blocker", dc);
    }
    {   /* CRUSH at ALT max: a handful of levels */
        SdPatch p = destroy_patch(SD_DS_CRUSH, 0, 65535, 0);
        uint32_t n = 0;
        int32_t seen[64];
        p.d_level[0] = p.d_level[1] = p.d_level[2] = 0;   /* the top band alone: the blocker barely moves it */
        destroy_feed(&p, 9000, 30000, SD_FS / 4u, a);
        (void)seen;
        for (i = 0; i < 4u; i++) n += (uint32_t)(e1.d.crush[i] == 8192);
        CHECK(n == 4u, "CRUSH's step at ALT max is not 2^13");
    }
    {   /* MIX 0: no DESTROY at all, bit for bit */
        SdPatch p = SD_PATCH[5];
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[5]); sd_engine_note(&e1, 50, 100); run(&e1, a, SD_FS / 2u, 256);
        p.d_type[0] = SD_DS_FOLD; p.d_gain[0] = 65535; p.d_mix = 0;
        sd_engine_init(&e2); sd_engine_patch(&e2, &p); sd_engine_note(&e2, 50, 100); run(&e2, b, SD_FS / 2u, 256);
        for (i = 0; i < SD_FS; i++) if (a[i] != b[i]) break;
        CHECK(i == SD_FS, "DESTROY at MIX 0 changed the sound");
    }
    {   /* every shaper at every maximum, through the whole engine: bounded (and the sanitizers' second run) */
        for (t = 0; t < SD_DSHAPES; t++) {
            SdPatch p = SD_PATCH[SD_PATCH_WORST];
            int32_t peak = 0;
            for (k = 0; k < 4u; k++) { p.d_type[k] = (uint8_t)t; p.d_gain[k] = 65535; p.d_alt[k] = 65535; p.d_bias[k] = k & 1 ? 32767 : -32767; p.d_level[k] = 65535; p.d_pan[k] = 0; }
            p.d_split[0] = SD_R_LO * 256; p.d_split[1] = 70 * 256; p.d_split[2] = SD_R_HI * 256; p.d_mix = 65535;
            sd_engine_init(&e1); sd_engine_patch(&e1, &p); sd_engine_note(&e1, 40, 127);
            run(&e1, a, SD_FS / 2u, 256);
            for (i = 0; i < SD_FS; i++) if (abs(a[i]) > peak) peak = abs(a[i]);
            CHECK(peak <= 4 * 4 * 4 * 32768, "DESTROY shaper %u at max: peak %d", t, peak);   /* 4 bands x 4 FS x 4 */
        }
    }
}

/* ---- S1: SPACE */
static SdSpaceMem tsm;

static SdPatch space_patch(uint16_t time, uint8_t mode, uint16_t fb, uint16_t dlvl, uint16_t rlvl, uint16_t decay)
{
    SdPatch p = SD_PATCH_SINE;
    p.s_time = time; p.s_mode = mode; p.s_fb = fb; p.s_dtone = 65535; p.s_dlevel = dlvl;
    p.s_size = 65535; p.s_decay = decay; p.s_rtone = 50000; p.s_rlevel = rlvl;
    return p;
}

/* SPACE alone: an impulse (or a held sine of amp, hz > 0) at frame 0, frames of output */
static void space_feed(const SdPatch *p, double hz, double amp, uint32_t frames, int32_t *outbuf, uint32_t bpm)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    double ph = 0;
    memset(&tsm, 0, sizeof tsm);
    sd_engine_init(&e1);
    e1.sm = &tsm;
    e1.bpm = (uint16_t)bpm;
    sd_seq_retime(&e1);
    e1.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_space_params(&e1);
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = hz > 0 ? (int32_t)lround(amp * sin(ph)) : (b == 0 && i == 0 ? 20000 : 0);
            ph += 2 * M_PI * hz / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_space_run(&e1, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
    }
}

static uint32_t first_peak(const int32_t *x, uint32_t ch, uint32_t from, uint32_t to)   /* the loudest frame */
{
    uint32_t i, at = from;
    int32_t best = -1;
    for (i = from; i < to; i++) if (abs(x[2u * i + ch]) > best) { best = abs(x[2u * i + ch]); at = i; }
    return at;
}

/* ---- D2: DESTROY's dynamics. One band (2, 150 .. 800 Hz) alone, NONE, a 300 Hz sine whose peak follows env(t) */
static SdEngine dyn_e;
static void dyn_feed(const SdPatch *p, double (*env)(double), uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    sd_engine_init(&dyn_e);
    dyn_e.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_destroy_params(&dyn_e);
        for (i = 0; i < SD_SUB; i++) {
            double t = (double)(b * SD_SUB + i) / SD_FS;
            in[2u * i] = in[2u * i + 1u] = (int32_t)lround(env(t) * sin(2 * M_PI * 300 * t));
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_destroy_run(&dyn_e, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
    }
}
static double env_step(double t) { return t < 0.5 ? 328 : 10362; }            /* -40 dBFS, then -10 */
static double env_pluck(double t) { return 16384 * exp(-fmod(t, 0.25) / 0.06); }   /* -6 dBFS, 60 ms, 4 a second */
static double env_floor(double t) { (void)t; return 1; }                      /* -90 dBFS */
static double rms_db(const int32_t *a, uint32_t from, uint32_t to)
{
    double e = 0;
    uint32_t i;
    for (i = from; i < to; i++) e += (double)a[2u * i] * a[2u * i];
    return 10 * log10(e / (to - from) + 1e-9);
}
static SdPatch dyn_patch(int16_t shape, double t_db)
{
    SdPatch p = destroy_patch(SD_DS_NONE, 0, 0, 0);
    uint32_t k;
    p.d_xover = SD_DX_CMP;
    for (k = 0; k < 4u; k++) p.d_level[k] = k == 1u ? 32768 : 0;
    p.d_shape[1] = shape;
    p.d_body[1] = (uint16_t)lround((t_db + 54) / 48 * 65535);
    return p;
}

static void test_dynamics(void)
{
    static int32_t a[2u * (SD_FS + SD_SUB)];   /* dyn_feed writes whole sub-blocks */
    uint32_t k, n = SD_FS;
    double step0, step1, c0, c1, pk;
    {   /* SHAPE 0: the gain is GAIN's, bit for bit (D1's sound) */
        SdPatch p = dyn_patch(0, -30);
        dyn_feed(&p, env_step, n, a);
        for (k = 0; k < 4u; k++)
            CHECK(dyn_e.d.gs[k] == 0 && dyn_e.d.g0[k] >> 4 == dyn_e.d.gain[k], "SHAPE 0 moves band %u's gain", k + 1u);
    }
    {   /* PHAT: a 30 dB step from under T = -25 dBFS to over it comes out ~18.75 dB (-40 lifted 4:1 upward) */
        SdPatch p = dyn_patch(0, -25);
        dyn_feed(&p, env_step, n, a);
        step0 = rms_db(a, n * 8u / 10u, n) - rms_db(a, n * 3u / 10u, n / 2u);
        p = dyn_patch(-32767, -25);
        dyn_feed(&p, env_step, n, a);
        step1 = rms_db(a, n * 8u / 10u, n) - rms_db(a, n * 3u / 10u, n / 2u);
        printf("dynamics PHAT: a 30 dB step, T -25 dBFS: %.1f dB without, %.1f dB with PHAT; the gain at -10: %+.1f dB\n",
               step0, step1, dyn_e.d.dyn[1] / 256.0);
        CHECK(fabs(step0 - 30) < 0.5, "without PHAT the step is %.1f dB", step0);
        CHECK(step1 > 16 && step1 < 21, "PHAT's step is %.1f dB (4:1 upward: ~18.75)", step1);
        CHECK(abs(dyn_e.d.dyn[1]) < 64, "PHAT moves -10 dBFS (over T) by %+.1f dB", dyn_e.d.dyn[1] / 256.0);
    }
    {   /* SHARP: plucks come out spikier (peak over rms), the tails shorter */
        SdPatch p = dyn_patch(0, -54);
        dyn_feed(&p, env_pluck, n, a);
        pk = 0;
        for (k = n / 2u; k < n; k++) pk = fmax(pk, fabs((double)a[2u * k]));
        c0 = 20 * log10(pk) - rms_db(a, n / 2u, n);
        p = dyn_patch(32767, -54);
        dyn_feed(&p, env_pluck, n, a);
        pk = 0;
        for (k = n / 2u; k < n; k++) pk = fmax(pk, fabs((double)a[2u * k]));
        c1 = 20 * log10(pk) - rms_db(a, n / 2u, n);
        printf("dynamics SHARP: plucks' crest %.1f dB without, %.1f dB with SHARP; peak %.0f\n", c0, c1, pk);
        CHECK(c1 > c0 + 2, "SHARP raises the crest only %.1f -> %.1f dB", c0, c1);
        CHECK(pk < 5 * 32768, "SHARP's peak %.0f unbounded (NONE guards at 4 x full scale)", pk);
    }
    {   /* PHAT lifts nothing from under -72 dB: the noise floor stays down */
        SdPatch p = dyn_patch(-32767, -6);
        dyn_feed(&p, env_floor, n / 4u, a);
        printf("dynamics PHAT at -90 dBFS, T -6: the gain %+.1f dB\n", dyn_e.d.dyn[1] / 256.0);
        CHECK(dyn_e.d.dyn[1] <= 0, "PHAT lifts -90 dBFS by %+.1f dB", dyn_e.d.dyn[1] / 256.0);
    }
}

/* ---- D3: DESTROY's feedback, TAME, the limiter. Every band NONE at 0 dB, MIX full; input from a function */
static SdEngine d3_e;
static void d3_feed(const SdPatch *p, int32_t (*src)(uint32_t), uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    sd_engine_init(&d3_e);
    d3_e.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_destroy_params(&d3_e);
        for (i = 0; i < SD_SUB; i++) {
            in[2u * i] = in[2u * i + 1u] = src(b * SD_SUB + i);
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_destroy_run(&d3_e, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
    }
}
static int32_t src_burst(uint32_t i) { return i < 44u ? (int32_t)lround(8192 * sin(2 * M_PI * 1000 * i / SD_FS)) : 0; }
static int32_t src_noise(uint32_t i) { static uint32_t r = 1; r = r * 1664525u + 1013904223u; return i < SD_FS / 4u ? (int32_t)(r >> 17) - 16384 : 0; }
static double d3_hz;
static int32_t src_sine(uint32_t i) { return (int32_t)lround(8192 * sin(2 * M_PI * d3_hz * i / SD_FS)); }
static SdPatch d3_patch(void)
{
    SdPatch p = destroy_patch(SD_DS_NONE, 0, 0, 0);
    p.d_xover = SD_DX_CMP;
    return p;
}
static double win_db(const int32_t *a, uint32_t from, uint32_t to)
{
    double e = 0;
    uint32_t i;
    for (i = from; i < to; i++) e += (double)a[2u * i] * a[2u * i];
    return 10 * log10(e + 1e-9);
}

static void test_feedback(void)
{
    static int32_t a[2u * (SD_FS * 2u + SD_SUB)];
    uint32_t i, n;
    {   /* an echo at TIME: 10 ms (441 frames + 44), FB 0.5: the repeat ~6 dB down (the loop's lowpass a little more) */
        SdPatch p = d3_patch();
        int32_t dl;
        double e0, e1;
        p.d_fb = 36409;                          /* 0.5 / 0.9 */
        p.d_ftime = (uint16_t)((441 - 44) * 65536 / 838 + 1);
        d3_feed(&p, src_burst, SD_FS / 10u, a);
        dl = d3_e.d.fd[0];
        e0 = win_db(a, 0, 88);
        e1 = win_db(a, (uint32_t)dl, (uint32_t)dl + 88u);
        printf("feedback: delay %d frames, the first repeat %.1f dB under the burst\n", (int)dl, e1 - e0);
        CHECK(dl >= 440 && dl <= 442, "feedback's delay %d, not 441", (int)dl);
        CHECK(e1 - e0 < -4 && e1 - e0 > -9, "the repeat %.1f dB (FB 0.5: ~-6)", e1 - e0);
        CHECK(win_db(a, 2u * (uint32_t)dl, 2u * (uint32_t)dl + 88u) < e1 - 4, "no second repeat under the first");
    }
    {   /* a moderate loop dies out to nothing: no limit cycle */
        SdPatch p = d3_patch();
        int32_t mx = 0;
        p.d_fb = 58000;
        p.d_ftime = 20000;
        d3_feed(&p, src_burst, SD_FS * 2u, a);
        for (i = SD_FS * 3u / 2u; i < SD_FS * 2u; i++) mx = abs(a[2u * i]) > mx ? abs(a[2u * i]) : mx;
        printf("feedback FB 0.8: 1.5 .. 2 s after a burst, peak %d\n", (int)mx);
        CHECK(mx <= 2, "FB 0.8 leaves %d after 1.5 s (a limit cycle?)", (int)mx);   /* 2: the crossover's last
                                                   bit through the DC blocker's floor, -84 dB; DESTROY rests there */
    }
    {   /* everything at max: +36 dB in every band, FB 0.9, noise in: it screams, bounded */
        SdPatch p = destroy_patch(SD_DS_NONE, 65535, 0, 0);
        int32_t mx = 0;
        p.d_xover = SD_DX_CMP;
        p.d_fb = 65535;
        p.d_ftime = 0;
        p.d_spread = 65535;
        d3_feed(&p, src_noise, SD_FS * 2u, a);
        for (i = SD_FS / 2u; i < SD_FS * 2u; i++) mx = abs(a[2u * i]) > mx ? abs(a[2u * i]) : mx;
        printf("feedback at max (+36 dB, FB 0.9): peak %d (%.2f x full scale), the limiter at %.2f\n", (int)mx, mx / 32768.0, d3_e.d.lg / 32768.0);
        CHECK(mx < 2 * 32768, "feedback at max reaches %d", (int)mx);
        CHECK(d3_e.d.lg < 32768, "the limiter never acted at max");
    }
    {   /* the limiter alone: +36 dB NONE on a sine of 0.25 holds near 1.5 x full scale */
        SdPatch p = destroy_patch(SD_DS_NONE, 65535, 0, 0);
        int32_t mx = 0;
        p.d_xover = SD_DX_CMP;
        d3_hz = 300;
        d3_feed(&p, src_sine, SD_FS, a);
        for (i = SD_FS / 2u; i < SD_FS; i++) mx = abs(a[2u * i]) > mx ? abs(a[2u * i]) : mx;
        printf("limiter: +36 dB on 0.25 full scale holds at %.2f x full scale\n", mx / 32768.0);
        CHECK(mx < 1.6 * 32768 && mx > 1.3 * 32768, "the limiter holds at %.2f x full scale", mx / 32768.0);
    }
    {   /* TAME: off at 0; at max 10 kHz goes, 100 Hz stays; at its least 10 kHz stays */
        static const uint16_t TM[3] = {0, 1, 65535};
        double r[3][2];
        uint32_t t, h;
        n = SD_FS / 2u;
        for (t = 0; t < 3u; t++)
            for (h = 0; h < 2u; h++) {
                SdPatch p = d3_patch();
                p.d_tame = TM[t];
                d3_hz = h ? 10000 : 100;
                d3_feed(&p, src_sine, n, a);
                r[t][h] = win_db(a, n / 2u, n);
            }
        printf("TAME: 100 Hz %+.1f / %+.1f dB, 10 kHz %+.1f / %+.1f dB (at its least / its most, against off)\n",
               r[1][0] - r[0][0], r[2][0] - r[0][0], r[1][1] - r[0][1], r[2][1] - r[0][1]);
        CHECK(fabs(r[2][0] - r[0][0]) < 1, "TAME at max moves 100 Hz %.1f dB", r[2][0] - r[0][0]);
        CHECK(r[2][1] - r[0][1] < -20, "TAME at max cuts 10 kHz only %.1f dB", r[2][1] - r[0][1]);
        CHECK(fabs(r[1][1] - r[0][1]) < 1.5, "TAME at its least moves 10 kHz %.1f dB", r[1][1] - r[0][1]);
    }
}

static void test_space(void)
{
    uint32_t at, i;
    {   /* a free 250 ms: the echo 11025 frames on, its loudest within a frame or two (the tone's lowpass) */
        SdPatch p = space_patch((uint16_t)(249u * 65535u / 999u + 1u), 0, 0, 65535, 0, 0);
        space_feed(&p, 0, 0, SD_FS / 2u, a, 120);
        at = first_peak(a, 0, 100, SD_FS / 2u);
        printf("space: a free 250 ms echo at frame %u (11025)\n", at);
        CHECK(at >= 11024u && at <= 11027u, "the 250 ms echo is at frame %u", at);
    }
    {   /* synced: 1/8 at 120 BPM = 0.25 s; 1/4D at 100 BPM = 0.9 s */
        SdPatch p = space_patch((uint16_t)(5u * 65536u / 12u + 100u), SD_SP_SYNC, 0, 65535, 0, 0);
        space_feed(&p, 0, 0, SD_FS / 2u, a, 120);
        at = first_peak(a, 0, 100, SD_FS / 2u);
        CHECK(at >= 11024u && at <= 11027u, "1/8 at 120 BPM: the echo at frame %u, not 11025", at);
        p = space_patch((uint16_t)(10u * 65536u / 12u + 100u), SD_SP_SYNC, 0, 65535, 0, 0);
        space_feed(&p, 0, 0, SD_FS, b, 100);
        at = first_peak(b, 0, 100, SD_FS);
        printf("space: synced 1/8 at 120 BPM, 1/4D at 100 BPM: frame %u (39690)\n", at);
        CHECK(at >= 39689u && at <= 39692u, "1/4D at 100 BPM: the echo at frame %u, not 39690", at);
    }
    {   /* ping-pong: the first echo left, the second right */
        SdPatch p = space_patch((uint16_t)(99u * 65535u / 999u + 1u), SD_SP_PP, 40000, 65535, 0, 0);   /* 100 ms */
        uint32_t l1, r1;
        space_feed(&p, 0, 0, SD_FS / 2u, a, 120);
        l1 = first_peak(a, 0, 100, 6000);
        r1 = first_peak(a, 1, 100, 10000);
        printf("space: ping-pong, 100 ms: left at %u, right at %u\n", l1, r1);
        CHECK(l1 >= 4409u && l1 <= 4412u && r1 >= 8819u && r1 <= 8823u, "ping-pong: left %u right %u", l1, r1);
    }
    {   /* FDBK at max, a loud sine for 3 s: it sings, but stays under its ceiling */
        SdPatch p = space_patch(20000, 0, 65535, 65535, 65535, 65535);
        int32_t peak = 0;
        space_feed(&p, 220, 30000, 3u * SD_FS, a, 120);
        for (i = 0; i < 3u * SD_FS; i++) if (abs(a[2u * i]) > peak) peak = abs(a[2u * i]);
        printf("space: FDBK and DECAY at max on a loud sine: peak %d\n", peak);
        CHECK(peak < 6 * 32768, "SPACE runs away: peak %d", peak);
    }
    {   /* DECAY: a longer tail */
        double t[2];
        uint32_t d;
        for (d = 0; d < 2u; d++) {
            SdPatch p = space_patch(1000, 0, 0, 0, 65535, d ? 52000 : 26000);
            double e0 = 0, e2 = 0;
            space_feed(&p, 0, 0, 3u * SD_FS / 2u, a, 120);
            for (i = 0; i < 8820u; i++) e0 += (double)a[2u * i] * a[2u * i];
            for (i = SD_FS; i < SD_FS + 8820u; i++) e2 += (double)a[2u * i] * a[2u * i];
            t[d] = 10 * log10(e2 / e0 + 1e-15);
        }
        printf("space: the reverb 1 s after an impulse: %.1f dB (DECAY 40), %.1f dB (DECAY 79)\n", t[0], t[1]);
        CHECK(t[1] > t[0] + 10, "DECAY does not lengthen the tail");
    }
    {   /* both levels 0: no SPACE, bit for bit */
        SdPatch p = SD_PATCH[5];
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[5]); sd_engine_note(&e1, 50, 100); run(&e1, a, SD_FS / 2u, 256);
        p.s_time = 9000; p.s_fb = 50000; p.s_decay = 50000; p.s_dlevel = 0; p.s_rlevel = 0;
        memset(&tsm, 0, sizeof tsm);
        sd_engine_init(&e2); e2.sm = &tsm; sd_engine_patch(&e2, &p); sd_engine_note(&e2, 50, 100); run(&e2, b, SD_FS / 2u, 256);
        for (i = 0; i < SD_FS; i++) if (a[i] != b[i]) break;
        CHECK(i == SD_FS, "SPACE at levels 0 changed the sound");
    }
    {   /* a note, its tail, then rest */
        SdPatch p = space_patch(9000, 0, 30000, 40000, 40000, 30000);
        uint32_t t2;
        int32_t o[2u * SD_SUB];
        memset(&tsm, 0, sizeof tsm);
        sd_engine_init(&e1); e1.sm = &tsm; sd_engine_patch(&e1, &p);
        sd_engine_note(&e1, 57, 100);
        for (t2 = 0; t2 < 200u; t2++) run(&e1, o, SD_SUB, SD_SUB);
        CHECK(!e1.sp.rest, "SPACE rests while a note sounds");
        sd_engine_note(&e1, 57, 0);
        for (t2 = 0; t2 < 30000u && !e1.sp.rest; t2++) run(&e1, o, SD_SUB, SD_SUB);
        printf("space: rests %.1f s after the release\n", t2 * SD_SUB / (double)SD_FS);
        CHECK(e1.sp.rest, "SPACE never rests");
    }
}

/* ---- R4: MIX fills, the lows keep, HARM */
static void test_rider_r4(void)
{
    static int32_t a[2u * (SD_FS / 2u + SD_SUB)];
    uint32_t n = SD_FS / 2u, k, h, ok;
    double e_dry, e_mid, e_keep, e_nokeep;
    /* the law's table: the dry whole up to the middle (-3 dB there with the wet), the ends exact */
    CHECK(SD_RMIX[0][0] == 32767 && SD_RMIX[0][1] == 0 && SD_RMIX[128][0] == 0 && SD_RMIX[128][1] == 32767,
          "MIX's ends are not dry / wet");
    CHECK(abs((int)SD_RMIX[64][0] - 23170) < 2 && abs((int)SD_RMIX[64][1] - 23170) < 2, "MIX's middle is not -3 dB each");
    for (k = 1; k <= 64u; k++) CHECK(SD_RMIX[k][1] >= SD_RMIX[k - 1u][1], "MIX's wet falls at %u", k);
    {   /* a 1 kHz sine with the filters at 4 kHz (they pass little): at the middle the dry comes through at -3 dB,
           not half as before R4 */
        SdPatch p = rider_patch(SD_RM_BP12, 20000, 4000);
        p.r_mix = 1;                            /* (MIX 0 is no Rider: the dry by 1 LSB of MIX) */
        rider_feed(&p, 1000, n, a);
        e_dry = rms_db(a, n / 2u, n);
        p.r_mix = 32768;
        rider_feed(&p, 1000, n, a);
        e_mid = rms_db(a, n / 2u, n);
        printf("rider R4: MIX at the middle passes the dry at %+.1f dB (the old law: -6)\n", e_mid - e_dry);
        CHECK(e_mid - e_dry > -5 && e_mid - e_dry < -1, "MIX's middle passes the dry at %+.1f dB", e_mid - e_dry);   /* -3,
                                                   and a little of the wet against it */
    }
    {   /* the low keep: a 60 Hz sine, the filters at 2 kHz, MIX full: with r_low it stays, without it goes */
        SdPatch p = rider_patch(SD_RM_BP12, 20000, 2000);
        rider_feed(&p, 60, n, a);
        e_nokeep = rms_db(a, n / 2u, n);
        p.r_low = 40000;                        /* ~264 Hz */
        rider_feed(&p, 60, n, a);
        e_keep = rms_db(a, n / 2u, n);
        p.r_mix = 1;
        rider_feed(&p, 60, n, a);
        e_dry = rms_db(a, n / 2u, n);
        printf("rider R4: 60 Hz at MIX full: %+.1f dB without the low keep, %+.1f dB with it\n", e_nokeep - e_dry, e_keep - e_dry);
        CHECK(e_keep - e_dry > -2, "the low keep loses 60 Hz: %+.1f dB", e_keep - e_dry);
        CHECK(e_nokeep - e_dry < -15, "without the low keep 60 Hz still passes %+.1f dB", e_nokeep - e_dry);
    }
    {   /* HARM: every filter lands on a harmonic of the note (none under it) */
        SdPatch p = rider_patch(SD_RM_BP12, 20000, 1000);
        uint32_t f;
        p.r_track = SD_RT_HARM;
        for (f = 0; f < 4u; f++) p.stage[0].pitch[f] = (uint16_t)midi256(150.0 + 700.0 * f);   /* 150 .. 2250 Hz */
        p.r_slew = 0;
        sd_engine_init(&e1);
        e1.patch = p;
        e1.r.note = 50;                         /* D3, 146.8 Hz */
        sd_rider_params(&e1);
        ok = 1;
        for (f = 0; f < 4u; f++) {
            int32_t sv = (e1.r.pitch[f] >> 8) - 50 * 256, hit = 0;
            for (h = 0; h < 32u; h++) hit |= sv == (int32_t)SD_HARM_ST[h];
            ok &= (uint32_t)hit;
        }
        printf("rider R4 HARM: 150 850 1550 2250 Hz over D3 -> harmonics %.2f %.2f %.2f %.2f\n",
               pow(2, ((e1.r.pitch[0] >> 8) - 50 * 256) / 3072.0), pow(2, ((e1.r.pitch[1] >> 8) - 50 * 256) / 3072.0),
               pow(2, ((e1.r.pitch[2] >> 8) - 50 * 256) / 3072.0), pow(2, ((e1.r.pitch[3] >> 8) - 50 * 256) / 3072.0));
        CHECK(ok, "HARM leaves a filter off the harmonics");
    }
}

static void test_rider_bypass(void)             /* MIX 0: the Rider is not there, bit for bit */
{
    SdPatch p = SD_PATCH[5];
    uint32_t i;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &SD_PATCH[5]);
    sd_engine_note(&e1, 50, 100);
    run(&e1, a, SD_FS / 2u, 256);
    p.stage[0] = SD_PATCH[7].stage[0];
    p.r_model = SD_RM_BP12; p.r_res = 60000; p.r_drive = 65535; p.r_mix = 0;
    sd_engine_init(&e2);
    sd_engine_patch(&e2, &p);
    sd_engine_note(&e2, 50, 100);
    run(&e2, b, SD_FS / 2u, 256);
    for (i = 0; i < SD_FS; i++) if (a[i] != b[i]) break;
    CHECK(i == SD_FS, "MIX 0 changed the sound at frame %u", i / 2u);
}

static void test_rider_pos(void)                /* between stages in log pitch; SLEW 0 jumps, SLEW glides */
{
    SdPatch p = SD_PATCH_SINE;
    uint32_t k, t;
    int32_t o[2u * SD_SUB];
    for (k = 0; k < 6u; k++)
        for (t = 0; t < 4u; t++) p.stage[k].pitch[t] = (uint16_t)((40u + 8u * k + 2u * t) * 256u), p.stage[k].level[t] = 30000;
    p.r_mix = 65535; p.r_drive = 16384;
    p.r_pos = 2u * 256u + 128u;                 /* 3.5: halfway from stage 3 to stage 4 */
    p.r_slew = 0;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    run(&e1, o, SD_SUB, SD_SUB);
    CHECK(e1.r.pitch[1] >> 16 == 40 + 16 + 2, "SLEW 0: POS 3.5 is not stage 3 (%d)", e1.r.pitch[1] >> 16);
    p.r_slew = 20000;                           /* a glide: the target is the midpoint, reached in time */
    sd_engine_patch(&e1, &p);
    for (t = 0; t < 2000u; t++) run(&e1, o, SD_SUB, SD_SUB);
    CHECK(abs((e1.r.pitch[1] >> 8) - (40 + 16 + 2 + 4) * 256) < 4, "SLEW: POS 3.5 is at %d, not halfway (%d)",
          e1.r.pitch[1] >> 8, (40 + 16 + 2 + 4) * 256);
    p.r_pos = 5u * 256u;                        /* to stage 6: 200 ms later still on the way, 2 s later there */
    p.r_slew = 50u << 9;
    sd_engine_patch(&e1, &p);
    for (t = 0; t < 30u; t++) run(&e1, o, SD_SUB, SD_SUB);
    CHECK((e1.r.pitch[1] >> 16) < 40 + 40 + 2 - 1, "SLEW: the jump to stage 6 did not glide");
    for (t = 0; t < 3000u; t++) run(&e1, o, SD_SUB, SD_SUB);
    CHECK(abs((e1.r.pitch[1] >> 8) - (40 + 40 + 2) * 256) < 4, "SLEW: stage 6 never reached");
}

static void test_rider_clock(void)              /* the stage clock: modes, range */
{
    static const struct { uint8_t mode, lo, hi; const char *want; } C[] = {
        {SD_RC_FWD, 0, 5, "234561234"}, {SD_RC_BWD, 0, 5, "654321654"}, {SD_RC_PP, 0, 3, "234321234"},
        {SD_RC_FWD, 2, 4, "453453453"}};               /* from stage lo (BWD: lo, so it wraps to hi first) */
    uint32_t c, k;
    for (c = 0; c < 4u; c++) {
        char got[10];
        sd_engine_init(&e1);
        e1.patch.r_div = 1; e1.patch.r_cmode = C[c].mode; e1.patch.r_min = C[c].lo; e1.patch.r_max = C[c].hi;
        e1.r.stage = C[c].mode == SD_RC_BWD ? 0 : C[c].lo;
        for (k = 0; k < 9u; k++) { sd_rider_clock(&e1); got[k] = (char)('1' + e1.r.stage); }
        got[9] = 0;
        CHECK(!strcmp(got, C[c].want), "rider clock mode %u range %u..%u: %s, not %s", C[c].mode, C[c].lo + 1, C[c].hi + 1,
              got, C[c].want);
    }
    sd_engine_init(&e1);
    e1.patch.r_div = 1; e1.patch.r_cmode = SD_RC_RND; e1.patch.r_min = 1; e1.patch.r_max = 3;
    for (k = 0; k < 200u; k++) { sd_rider_clock(&e1); CHECK(e1.r.stage >= 1 && e1.r.stage <= 3, "RND left its range"); }
}

static void test_rider_extremes(void)           /* every model, RES / DRIVE / levels at max, the range's ends */
{
    static const uint16_t P[] = {SD_R_LO * 256, 69 * 256, SD_R_HI * 256};
    uint32_t m, q, i, k;
    for (m = 0; m < SD_RMODELS; m++)
        for (q = 0; q < 3u; q++) {
            SdPatch p = SD_PATCH[SD_PATCH_WORST];
            int32_t peak = 0, st = 0;
            for (k = 0; k < 6u; k++)
                for (i = 0; i < 4u; i++) p.stage[k].pitch[i] = P[q], p.stage[k].level[i] = 65535;
            p.r_model = (uint8_t)m; p.r_res = 65535; p.r_drive = 65535; p.r_mix = 65535; p.r_div = 0;
            sd_engine_init(&e1);
            sd_engine_patch(&e1, &p);
            sd_engine_note(&e1, q == 2u ? 100 : 30, 127);
            run(&e1, a, SD_FS / 2u, 256);
            for (i = 0; i < SD_FS; i++) { int32_t s = a[i] < 0 ? -a[i] : a[i]; if (s > peak) peak = s; }
            for (i = 0; i < 4u; i++) { int32_t s = abs(e1.r.ic1[0][i]) > abs(e1.r.ic2[0][i]) ? abs(e1.r.ic1[0][i]) : abs(e1.r.ic2[0][i]); if (s > st) st = s; }
            CHECK(peak <= 4 * 32768 + 64, "rider model %u at %u x 256, all at max: peak %d", m, P[q] / 256u, peak);
            CHECK(st < (1 << 30), "rider model %u at %u x 256: an integrator at %d", m, P[q] / 256u, st);
        }
}

/* C1: the modulation. None: the patch exactly (the renders above). A slot moves its destination by its amount of
 * the range; the LFOs keep their rate and sync; KEY restarts; PITCH tunes; the macros stay in range everywhere */
static SdSpaceMem mod_sm;
static void mod_run(SdEngine *e, const SdPatch *p, uint32_t ticks)   /* that many ticks, the output dropped */
{
    static int32_t o[2u * SD_SUB];
    uint32_t t;
    for (t = 0; t < ticks; t++) { memset(o, 0, sizeof o); sd_engine_render(e, o, SD_SUB); }
    (void)p;
}
static void mod_init(SdEngine *e, const SdPatch *p)
{
    sd_engine_init(e);
    memset(&mod_sm, 0, sizeof mod_sm);
    e->sm = &mod_sm;
    sd_engine_patch(e, p);
}
static void test_mod(void)
{
    SdPatch p = SD_PATCH[0];
    uint32_t t, k, lo, hi, cyc, last, wraps;
    /* LFO1 (sine, ~1 Hz free) -> XM at +25 %: XM swings +-25 % of the range around the patch's (100 %: +-all) */
    p.xm = 32768;
    p.lfo_rate[0] = 34000;                      /* 0.02 x 2000^(34000/65536): ~1 Hz */
    p.mod[0] = (SdModSlot){SD_MS_LFO1, SD_MD_XM, 8192};
    mod_init(&e1, &p);
    sd_engine_note(&e1, 60, 100);
    lo = 65535; hi = 0;
    for (t = 0; t < 3000u; t++) {
        mod_run(&e1, &p, 1);
        if (e1.patch.xm < lo) lo = e1.patch.xm;
        if (e1.patch.xm > hi) hi = e1.patch.xm;
    }
    printf("mod: LFO1 -> XM +25 %%: XM %u .. %u around 32768\n", lo, hi);
    CHECK(lo < 32768u - 15000u && lo > 32768u - 17500u && hi > 32768u + 15000u && hi < 32768u + 17500u,
          "LFO1 -> XM swings %u .. %u", lo, hi);
    CHECK(e1.base.xm == 32768u, "the matrix wrote the base: %u", e1.base.xm);
    /* SYNC: 4 sixteenths (a beat) at 120 BPM is 0.5 s: 689 ticks a cycle */
    p = SD_PATCH[0];
    p.lfo_mode[0] = SD_LM_SYNC;
    p.lfo_rate[0] = (uint16_t)(3u * 65536u / SD_LFO_DIVS + 100u);   /* the 4th division: 4 sixteenths */
    p.lfo_shape[0] = SD_LS_RAMP;
    p.mod[0] = (SdModSlot){SD_MS_LFO1, SD_MD_XM, 100};
    mod_init(&e1, &p);
    e1.bpm = 120;
    wraps = 0; cyc = 0; last = 0;
    for (t = 0; t < 4000u; t++) {
        uint32_t ph0 = e1.lfo_ph[0];
        mod_run(&e1, &p, 1);
        if (e1.lfo_ph[0] < ph0) { if (wraps) cyc = t - last; last = t; wraps++; }
    }
    printf("mod: LFO SYNC a beat at 120 BPM: a cycle every %u ticks (689 expected)\n", cyc);
    CHECK(cyc >= 687u && cyc <= 691u, "SYNC's cycle is %u ticks", cyc);
    /* KEY: a note restarts the LFO */
    p.lfo_mode[0] = SD_LM_KEY;
    p.lfo_rate[0] = 20000;
    mod_init(&e1, &p);
    mod_run(&e1, &p, 333);
    sd_engine_note(&e1, 60, 100);
    mod_run(&e1, &p, 1);
    CHECK(e1.lfo_ph[0] == 0u, "KEY: the phase after a note is %u", e1.lfo_ph[0]);
    /* ONE: a SAW once a note: from the top down to the bottom over its cycle, held there, again at the next note */
    p = SD_PATCH[0];
    p.lfo_shape[0] = SD_LS_SAW; p.lfo_mode[0] = SD_LM_ONE; p.lfo_rate[0] = 34000;   /* ~1 Hz: a second down */
    p.mod[0] = (SdModSlot){SD_MS_LFO1, SD_MD_XM, 8192};
    mod_init(&e1, &p);
    sd_engine_note(&e1, 60, 100);
    mod_run(&e1, &p, 2);
    CHECK(e1.lfo_out[0] > 32000, "ONE starts at %d, not the top", e1.lfo_out[0]);
    mod_run(&e1, &p, 3000);                     /* 2.2 s: past its cycle */
    CHECK(e1.lfo_out[0] >= 0 && e1.lfo_out[0] < 200, "ONE ends at %d, not 0 (the patch's own value)", e1.lfo_out[0]);
    mod_run(&e1, &p, 500);
    CHECK(e1.lfo_out[0] >= 0 && e1.lfo_out[0] < 200, "ONE did not hold: %d", e1.lfo_out[0]);
    sd_engine_note(&e1, 62, 100);
    mod_run(&e1, &p, 2);
    CHECK(e1.lfo_out[0] > 32000, "ONE did not restart at the note: %d", e1.lfo_out[0]);
    /* the engines as sources: STEP walks the pattern; D LOW follows the low band; ECHO follows SPACE */
    {
        SdPattern q;
        int32_t st0 = 0, st1 = 0, dl = 0, ec = 0;
        uint32_t t;
        p = SD_PATCH[6];                        /* DISTROY: DESTROY and SPACE on */
        sd_pattern_default(&q);
        mod_init(&e1, &p);
        sd_engine_pattern(&e1, &q);
        e1.seq_run = 1;
        for (t = 0; t < 1400u; t++) {           /* ~1 s at 120 BPM: steps 0 .. 7 */
            int32_t s0;
            mod_run(&e1, &p, 1);
            s0 = e1.t[0].seq_shown;
            if (t == 20u) st0 = s0;
            if (t == 1300u) st1 = s0;
            if (e1.d.on && e1.d.ef[0] + 48 * 256 > dl) dl = e1.d.ef[0] + 48 * 256;
            if (e1.echo_env > ec) ec = e1.echo_env;
        }
        CHECK(st1 > st0, "the steps did not move (%d -> %d)", st0, st1);
        CHECK(dl > 12 * 256, "D LOW never rose: %d dB over -48", dl / 256);
        CHECK(ec > 300, "ECHO never rose: %d", ec);
    }
    /* PITCH: the wheel at full, +50 %: +12 semitones, an octave */
    p = SD_PATCH[0];
    p.mod[0] = (SdModSlot){SD_MS_WHEEL, SD_MD_PITCH, 16384};
    mod_init(&e1, &p);
    e1.wheel = 127;
    mod_run(&e1, &p, 2);
    printf("mod: wheel -> PITCH +50 %%: x %.4f (2 expected)\n", e1.pmul / 65536.0);
    CHECK(abs((int)e1.pmul - 131072) < 400, "PITCH at +12: x %.4f", e1.pmul / 65536.0);
    p.mod[0].amt = -16384;
    mod_init(&e1, &p);
    e1.wheel = 127;
    mod_run(&e1, &p, 2);
    CHECK(abs((int)e1.pmul - 32768) < 200, "PITCH at -12: x %.4f", e1.pmul / 65536.0);
    /* the restore knows every field the modulation writes: everything at full, then put back, is the base again */
    {
        uint32_t d, kind;
        for (kind = 0; kind < SD_M4S; kind++) {
            p = SD_PATCH[6];
            for (d = 1; d < SD_MDSTS && d - 1u < SD_MODS * 3u; d++)
                p.mod[(d - 1u) % SD_MODS] = (SdModSlot){(uint8_t)(1u + d % (SD_MSRCS - 1u)), (uint8_t)d, 32767};
            for (k = 0; k < 4u; k++) p.mac[k] = 65535;
            p.mac4 = (uint8_t)kind;
            mod_init(&e1, &p);
            sd_engine_note(&e1, 60, 127);
            e1.wheel = e1.after = 127;
            for (d = 1; d < SD_MDSTS; d++) {    /* each destination in turn in slot 1, so all get written */
                e1.base.mod[0] = (SdModSlot){SD_MS_WHEEL, (uint8_t)d, 32767};
                e1.patch.mod[0] = e1.base.mod[0];
                mod_run(&e1, &p, 1);
            }
            sd_mod_restore(&e1);
            CHECK(!memcmp(&e1.patch, &e1.base, sizeof e1.patch), "MACRO 4 kind %u: the restore misses a field", kind);
        }
    }
    /* every destination by every LFO shape at +-100 %, and every macro of every kind at full, on every patch: in
       range (the sanitizers watch), and never past 4 x full scale */
    {
        static int32_t o[2u * 2048u];
        uint32_t d, pi, worst = 0;
        for (pi = 0; pi < SD_PATCHES; pi++)
            for (d = 1; d < SD_MDSTS + 3u; d++) {
                int32_t pk = 0;
                uint32_t i;
                p = SD_PATCH[pi];
                if (d < SD_MDSTS) {
                    p.mod[0] = (SdModSlot){SD_MS_LFO1, (uint8_t)d, 32767};
                    p.mod[1] = (SdModSlot){SD_MS_LFO2, (uint8_t)d, -32767};
                    p.mod[2] = (SdModSlot){SD_MS_RAND, (uint8_t)d, 32767};
                    p.lfo_shape[0] = (uint8_t)(d % SD_LSHAPES);
                    p.lfo_shape[1] = SD_LS_SH;
                    p.lfo_rate[0] = 60000; p.lfo_rate[1] = 50000;
                } else {
                    for (k = 0; k < 4u; k++) p.mac[k] = 65535;
                    p.mac4 = (uint8_t)(d - SD_MDSTS);
                }
                mod_init(&e1, &p);
                sd_engine_note(&e1, 48, 127);
                for (t = 0; t < 20u; t++) {
                    memset(o, 0, sizeof o);
                    sd_engine_render(&e1, o, 2048u);
                    if (t == 10u) sd_engine_note(&e1, 72, 127);
                    for (i = 0; i < 2u * 2048u; i++) { int32_t v = abs(o[i]); if (v > pk) pk = v; }
                }
                if ((uint32_t)pk > worst) worst = (uint32_t)pk;
                CHECK(pk <= 4 * 32768, "%s, destination %u: the output reaches %d", SD_PATCH_NAME[pi], d, pk);
            }
        printf("mod: every destination at +-100 %% and every macro at full, all patches: peak %.2f x full scale\n",
               worst / 32768.0);
    }
}

/* The sequencer (sd_seq.h): the traversal through segments, a step's types, DIV and SWING, MUTATE only when on, at
 * the loop's one; the segment editing (SkurkyMello's) */
static uint32_t seq_trace(SdEngine *e, const SdPattern *p, uint32_t steps, uint8_t *out, uint32_t *starts)
{   /* the steps as they start, and the frame each starts at */
    static int32_t o[2u * SD_SUB];
    uint32_t n = 0, last = 0xFFFFFFFFu, f = 0;
    sd_engine_init(e);
    sd_engine_patch(e, &SD_PATCH[0]);
    sd_engine_pattern(e, p);
    e->bpm = 120;
    e->seq_run = 1;
    while (n < steps && f < 60u * SD_FS) {
        memset(o, 0, sizeof o);
        sd_engine_render(e, o, SD_SUB);
        if (e->t[0].seq_on && e->t[0].seq_count != last) {
            last = e->t[0].seq_count;
            if (starts) starts[n] = f;
            out[n++] = e->t[0].seq_shown;
        }
        f += SD_SUB;
    }
    return n;
}
static int seq_is(const uint8_t *got, const char *want)   /* "0 1 2 ..." */
{
    uint32_t i = 0;
    const char *c = want;
    while (*c) {
        uint32_t v = (uint32_t)strtoul(c, (char **)&c, 10);
        if (got[i++] != v) return 0;
        while (*c == ' ') c++;
    }
    return 1;
}
static void test_seq(void)
{
    static SdPattern p;
    static uint8_t tr[256];
    static uint32_t at[256];
    uint32_t k, n;
    /* the default: 16 steps, four segments of four, forwards */
    sd_pattern_default(&p);
    n = seq_trace(&e1, &p, 20, tr, at);
    CHECK(n == 20u && seq_is(tr, "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 0 1 2 3"), "default order");
    CHECK(at[1] - at[0] == 5512u || at[1] - at[0] == 5536u, "a 16th at 120 BPM is %u frames", at[1] - at[0]);
    /* orders and repeats: A x2, B reversed, C ping-pong, D skipped */
    p.seg_rep[0] = 2; p.seg_order[1] = SD_SO_REV; p.seg_order[2] = SD_SO_PP; p.seg_skip = 1u << 3;
    n = seq_trace(&e1, &p, 22, tr, 0);
    CHECK(seq_is(tr, "0 1 2 3 0 1 2 3 7 6 5 4 8 9 10 11 10 9 0 1 2 3"), "segments: got %u %u %u %u %u %u %u %u %u %u %u %u",
          tr[0], tr[1], tr[2], tr[3], tr[8], tr[9], tr[10], tr[11], tr[12], tr[13], tr[16], tr[18]);
    /* RND: each pass a shuffle of its steps */
    sd_pattern_default(&p);
    p.seg_count = 1; p.seg_order[0] = SD_SO_RND;
    n = seq_trace(&e1, &p, 48, tr, 0);
    {
        uint32_t pass, seen, same = 1;
        for (pass = 0; pass < 3u; pass++) {
            seen = 0;
            for (k = 0; k < 16u; k++) seen |= 1u << tr[pass * 16u + k];
            CHECK(seen == 0xFFFFu, "RND pass %u misses steps (%04x)", pass, seen);
        }
        for (k = 0; k < 16u; k++) same &= tr[k] == tr[16u + k];
        CHECK(!same, "RND plays the same order twice");
    }
    /* SKIP steps are out; a pattern of skips is silent, not stuck */
    sd_pattern_default(&p);
    p.step[1].type = SD_ST_SKIP; p.step[5].type = SD_ST_SKIP;
    n = seq_trace(&e1, &p, 8, tr, 0);
    CHECK(seq_is(tr, "0 2 3 4 6 7 8 9"), "SKIP: %u %u %u %u %u", tr[0], tr[1], tr[2], tr[3], tr[4]);
    for (k = 0; k < SD_PSTEPS; k++) p.step[k].type = SD_ST_SKIP;
    n = seq_trace(&e1, &p, 4, tr, 0);
    CHECK(n == 0u && sd_engine_silent(&e1), "all skipped: %u steps, silent %d", n, sd_engine_silent(&e1));
    /* DIV 1/8: twice a 16th; SWING 66: the pair splits 66 / 34 */
    sd_pattern_default(&p);
    p.div = 4;
    seq_trace(&e1, &p, 3, tr, at);
    CHECK(at[1] - at[0] >= 11008u && at[1] - at[0] <= 11072u, "DIV 1/8: %u frames", at[1] - at[0]);
    p.div = 2; p.swing = 66;
    seq_trace(&e1, &p, 3, tr, at);
    CHECK(at[1] - at[0] > at[2] - at[1] + 3000u, "SWING: %u then %u frames", at[1] - at[0], at[2] - at[1]);
    /* TIE holds the note through (no release); SLIDE glides (no new attack) */
    sd_pattern_default(&p);
    for (k = 0; k < 16u; k++) { p.step[k].type = SD_ST_TIE; p.step[k].gate = 50; }
    p.step[0].type = SD_ST_NOTE; p.step[0].note = 45;
    seq_trace(&e1, &p, 6, tr, 0);
    CHECK(e1.voice[0].gate && e1.voice[0].note == 45, "TIE: the note is not held (gate %u)", e1.voice[0].gate);
    sd_pattern_default(&p);
    for (k = 0; k < 16u; k++) { p.step[k].type = SD_ST_SLIDE; p.step[k].note = (uint8_t)(45 + (k & 1u) * 12u); }
    p.step[0].type = SD_ST_NOTE;
    seq_trace(&e1, &p, 2, tr, 0);
    {
        uint32_t ages = e1.ages;
        static int32_t o[2u * 2048u];
        run(&e1, o, 2048, 32);
        CHECK(e1.ages == ages && e1.voice[0].gate, "SLIDE retriggers (ages %u -> %u)", ages, e1.ages);
        CHECK(abs((int)(e1.voice[0].inc - e1.voice[0].inc_tgt)) < (int)(e1.voice[0].inc_tgt / 1000u),
              "SLIDE has not arrived after 46 ms");
    }
    /* TURING CHANGE 0: the pattern never changes; CHANGE 50 (on): once a loop, at its one */
    sd_pattern_default(&p);
    seq_trace(&e1, &p, 16u * 6u, tr, 0);
    CHECK(e1.t[0].mut_count == 0u && !memcmp(&e1.t[0].pat, &p, sizeof p), "MUTATE 0 changed the pattern");
    p.mutate = 50;                              /* CHANGE 50, TURING still off: nothing */
    seq_trace(&e1, &p, 16u * 6u + 1u, tr, 0);
    CHECK(e1.t[0].mut_count == 0u, "TURING off rewrote the pattern (%u)", e1.t[0].mut_count);
    p.tur |= 1u;
    seq_trace(&e1, &p, 16u * 6u + 1u, tr, 0);
    CHECK(e1.t[0].mut_count == 6u, "MUTATE 50: %u rewrites in 6 loops", e1.t[0].mut_count);
    {
        uint32_t diff = 0;
        for (k = 0; k < 16u; k++) diff += memcmp(&e1.t[0].pat.step[k], &p.step[k], sizeof p.step[k]) != 0;
        CHECK(diff > 2u, "MUTATE 50 left the pattern almost alone (%u steps)", diff);
        for (k = 0; k < 16u; k++)
            CHECK(e1.t[0].pat.step[k].note >= 33u && e1.t[0].pat.step[k].note <= 69u, "MUTATE wandered to %u", e1.t[0].pat.step[k].note);
    }
    /* the segment editing (SkurkyMello's): a cut, a join, EVEN, a length, a new pattern length */
    sd_pattern_default(&p);
    {
        uint8_t b[SD_SEGS + 1u];
        CHECK(sd_seg_toggle(&p, 6) == 2 && sd_seg_bounds(&p, b) == 5u && b[2] == 6u, "cut at 7");
        CHECK(sd_seg_toggle(&p, 6) == 1 && sd_seg_bounds(&p, b) == 4u, "join at 7");
        CHECK(sd_seg_toggle(&p, 0) == -1, "step 1 cut");
        sd_seg_set_len(&p, 0, 6);
        sd_seg_bounds(&p, b);
        CHECK(b[1] == 6u && b[2] == 10u && b[3] == 14u, "A to 6: %u %u %u", b[1], b[2], b[3]);
        sd_seg_even(&p);
        sd_seg_bounds(&p, b);
        CHECK(b[1] == 4u && b[2] == 8u && b[3] == 12u, "EVEN: %u %u %u", b[1], b[2], b[3]);
        p.len = 8;
        sd_seg_rescale(&p, 16);
        sd_seg_bounds(&p, b);
        CHECK(b[1] == 2u && b[2] == 4u && b[3] == 6u && b[4] == 8u, "16 -> 8: %u %u %u", b[1], b[2], b[3]);
    }
    /* RANDOMIZE keeps step 1 (the key) and the scale */
    sd_pattern_default(&p);
    {
        uint32_t r = 7, ok = 1;
        p.scale = SD_SC_PMIN;
        sd_pattern_randomize(&p, &r);
        for (k = 0; k < 16u; k++)
            ok &= (SD_SCALE_MASK[SD_SC_PMIN] >> ((p.step[k].note + 12u - 45u) % 12u)) & 1u;
        CHECK(p.step[0].note == 45u && ok, "RANDOMIZE left the scale or the key");
    }
    /* RATCHET: 3 hits in a 16th at 100 BPM (150 ms: 50 ms apart), each softer; at 240 BPM a 16th is 62 ms: one hit */
    {
        static int32_t o[2u * SD_SUB];
        uint32_t f, ons = 0, last_age, bpm, t;
        uint8_t vels[8];
        for (t = 0; t < 2u; t++) {
            bpm = t ? 240u : 100u;
            sd_pattern_default(&p);
            for (k = 0; k < 16u; k++) { p.step[k].type = SD_ST_REST; }
            p.step[0].type = SD_ST_NOTE; p.step[0].ratchet = 3; p.step[0].gate = 40; p.step[0].vel = 120;
            sd_engine_init(&e1);
            sd_engine_patch(&e1, &SD_PATCH[0]);
            sd_engine_pattern(&e1, &p);
            e1.bpm = (uint16_t)bpm;
            e1.seq_run = 1;
            ons = 0; last_age = 0;
            for (f = 0; f < SD_FS * 60u / bpm / 4u; f += SD_SUB) {   /* one 16th */
                memset(o, 0, sizeof o);
                sd_engine_render(&e1, o, SD_SUB);
                if (e1.ages != last_age) { last_age = e1.ages; if (ons < 8u) vels[ons] = e1.voice[0].vel; ons++; }
            }
            if (!t) {
                CHECK(ons == 3u && vels[1] < vels[0] && vels[2] < vels[1] && vels[2] >= 60u,
                      "RATCHET 3 at 100 BPM: %u hits, velocities %u %u %u", ons, vels[0], vels[1], vels[2]);
            } else {
                CHECK(ons == 1u, "RATCHET 3 at 240 BPM (62 ms a 16th): %u hits, not 1", ons);
            }
        }
    }
    /* PROB 50: about half the steps play; 100: all; 0: none */
    {
        uint32_t played = 0, i;
        static int32_t o[2u * SD_SUB];
        sd_pattern_default(&p);
        for (k = 0; k < 16u; k++) { p.step[k].type = SD_ST_NOTE; p.step[k].prob = 50; p.step[k].gate = 30; }
        sd_engine_init(&e1);
        sd_engine_patch(&e1, &SD_PATCH[0]);
        sd_engine_pattern(&e1, &p);
        e1.seq_run = 1;
        for (i = 0; i < 64u * 5512u / SD_SUB; i++) {   /* 64 steps at 120 BPM */
            uint32_t a0 = e1.ages;
            memset(o, 0, sizeof o);
            sd_engine_render(&e1, o, SD_SUB);
            played += e1.ages != a0;
        }
        CHECK(played > 20u && played < 44u, "PROB 50: %u of 64 steps played", played);
    }
    printf("seq: segments, orders, skips, DIV, SWING, TIE, SLIDE, RATCHET, PROB, MUTATE at the one, the editing\n");
}

/* 2026-11-06: the filter, the chords, track B, the arpeggiator */
static uint32_t gates_on(const SdEngine *e, uint8_t *notes)   /* the voices with a gate, their notes (sorted) */
{
    uint32_t i, n = 0, k;
    for (i = 0; i < e->nvoices; i++)
        if (e->voice[i].gate) notes[n++] = e->voice[i].note;
    for (i = 1; i < n; i++)
        for (k = i; k > 0u && notes[k - 1u] > notes[k]; k--) { uint8_t x = notes[k]; notes[k] = notes[k - 1u]; notes[k - 1u] = x; }
    return n;
}
static void run_ms(SdEngine *e, uint32_t ms)
{
    static int32_t o[2u * SD_SUB];
    uint32_t f;
    for (f = 0; f < ms * 441u / 10u; f += SD_SUB) sd_engine_render(e, o, SD_SUB);
}
static double band_db(const int32_t *x, uint32_t n, double f0, double f1)   /* left channel, a Hann DFT's bins */
{
    double s = 0;
    uint32_t k, i;
    for (k = (uint32_t)(f0 * n / SD_FS); k <= (uint32_t)(f1 * n / SD_FS); k++) {
        double re = 0, im = 0;
        for (i = 0; i < n; i++) {
            double h = 0.5 - 0.5 * cos(2 * M_PI * i / n), w = 2 * M_PI * k * i / n;
            re += h * x[2u * i] * cos(w);
            im -= h * x[2u * i] * sin(w);
        }
        s += re * re + im * im;
    }
    return 10 * log10(s + 1e-9);
}
static void test_filter_seq_arp(void)
{
    static int32_t o[2u * 4096u];
    SdPatch p = SD_PATCH_SINE;
    SdPattern q;
    uint8_t nt[8];
    double hi0, hi12, hi24, lo0, lo24, he;
    uint32_t n, i, ok;
    /* the filter: off is the voice as it was; LP12 and LP24 at ~400 Hz take a saw's 2-5 kHz down ~30 and ~60 dB, the
       lows kept; RES at full stays in range (the render's shoulder) */
    p.warp = 26000;
    for (i = 0; i < 4u; i++) {
        p.f_type = (uint8_t)(i == 3u ? SD_FT_LP24 : i);
        p.f_cut = 30000; p.f_res = i == 3u ? 65535u : 0u;
        p.e3_s = 127;
        sd_engine_init(&e1); sd_engine_patch(&e1, &p);
        run_ms(&e1, 2); sd_engine_note(&e1, 45, 110); run_ms(&e1, 300);
        memset(o, 0, sizeof o); sd_engine_render(&e1, o, 4096u);
        if (i == 0u) { hi0 = band_db(o, 4096u, 2000, 5000); lo0 = band_db(o, 4096u, 50, 500); }
        if (i == 1u) hi12 = band_db(o, 4096u, 2000, 5000);
        if (i == 2u) { hi24 = band_db(o, 4096u, 2000, 5000); lo24 = band_db(o, 4096u, 50, 500); }
        if (i == 3u) {
            int32_t pk = 0;
            for (n = 0; n < 2u * 4096u; n++) pk = o[n] > pk ? o[n] : -o[n] > pk ? -o[n] : pk;
            CHECK(pk <= 32767, "the filter at full RES: %d", pk);
        }
    }
    CHECK(hi0 - hi12 > 25 && hi0 - hi24 > 55 && fabs(lo0 - lo24) < 3, "the filter: 2-5 kHz -%.1f (LP12) -%.1f dB (LP24), "
          "the lows %+.1f dB", hi0 - hi12, hi0 - hi24, lo24 - lo0);
    /* ENV3 opens it: the same note brighter with depth +100 % and a decay */
    p.f_type = SD_FT_LP24; p.f_res = 0; p.f_cut = 10000; p.f_env = 32767; p.e3_a = 0; p.e3_d = 90; p.e3_s = 0;
    sd_engine_init(&e1); sd_engine_patch(&e1, &p);
    run_ms(&e1, 2); sd_engine_note(&e1, 45, 110); run_ms(&e1, 20);
    memset(o, 0, sizeof o); sd_engine_render(&e1, o, 2048u);
    he = band_db(o, 2048u, 1000, 4000);
    run_ms(&e1, 1500);
    memset(o, 0, sizeof o); sd_engine_render(&e1, o, 2048u);
    he -= band_db(o, 2048u, 1000, 4000);
    CHECK(he > 20, "ENV3 did not open the filter (%.1f dB)", he);
    /* a chord in a step: POLY, the step's note and its three offsets sound together, and stop at the gate */
    sd_pattern_default(&q);
    for (i = 0; i < q.len; i++) { q.step[i].type = SD_ST_NOTE; q.step[i].note = 48; q.step[i].gate = 50; }
    q.step[0].chord[0] = 4; q.step[0].chord[1] = 7; q.step[0].chord[2] = 12;
    sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[0]);
    e1.mode = SD_MODE_POLY; run_ms(&e1, 5);
    sd_engine_pattern(&e1, &q); e1.bpm = 120; e1.seq_run = 1;
    run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 4u && nt[0] == 48u && nt[1] == 52u && nt[2] == 55u && nt[3] == 60u, "a chord step: %u notes (%u %u %u %u)",
          n, nt[0], nt[1], nt[2], nt[3]);
    run_ms(&e1, 80);                            /* past the gate (a 16th at 120: 125 ms, 50 %) */
    CHECK(gates_on(&e1, nt) == 0u, "the chord's notes held past the gate");
    /* track B: its own pattern and length, at once with A, on the same sound */
    {
        SdPattern qb;
        sd_pattern_default(&q); sd_pattern_default(&qb);
        for (i = 0; i < SD_PSTEPS; i++) {
            q.step[i].type = qb.step[i].type = SD_ST_NOTE;
            q.step[i].note = 72; qb.step[i].note = 36; q.step[i].gate = qb.step[i].gate = 90;
        }
        q.len = 16; qb.len = 3; qb.seg_count = 1;
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[0]);
        e1.mode = SD_MODE_DUO; run_ms(&e1, 5);
        sd_engine_pattern_to(&e1, 0, &q); sd_engine_pattern_to(&e1, 1, &qb);
        e1.seqb = 1; e1.bpm = 120; e1.seq_run = 1;
        run_ms(&e1, 30);
        n = gates_on(&e1, nt);
        CHECK(n == 2u && nt[0] == 36u && nt[1] == 72u, "A and B together: %u notes", n);
        run_ms(&e1, 125u * 3u);                 /* B's length 3: its step 0 again at step 3 */
        CHECK(e1.t[1].seq_shown == 0u && e1.t[0].seq_shown == 3u, "B's own length: A at %u, B at %u",
              e1.t[0].seq_shown, e1.t[1].seq_shown);
    }
    /* the arpeggiator: C E G held, UP over two octaves on 16ths: 48 52 55 60 64 67 48 ...; DOWN; HOLD keeps them */
    {
        static const uint8_t UP[8] = {48, 52, 55, 60, 64, 67, 48, 52};
        uint8_t got[8];
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[0]);
        e1.arp_on = 1; e1.arp_oct = 2; e1.arp_mode = SD_AM_UP; e1.bpm = 120;
        sd_engine_note(&e1, 55, 100); sd_engine_note(&e1, 48, 100); sd_engine_note(&e1, 52, 100);
        for (i = 0; i < 8u; i++) {
            run_ms(&e1, i ? 125 : 10);
            got[i] = e1.arp_cur;
        }
        for (ok = 1, i = 0; i < 8u; i++) ok &= got[i] == UP[i];
        CHECK(ok, "ARP UP: %u %u %u %u %u %u %u %u", got[0], got[1], got[2], got[3], got[4], got[5], got[6], got[7]);
        e1.arp_mode = SD_AM_DOWN;
        run_ms(&e1, 125); i = e1.arp_cur;
        run_ms(&e1, 125);
        CHECK(e1.arp_cur + 12u == i || e1.arp_cur + 3u == i || e1.arp_cur + 4u == i || e1.arp_cur + 5u == i ||
              e1.arp_cur == 67u, "ARP DOWN does not fall: %u then %u", i, e1.arp_cur);
        sd_engine_note(&e1, 48, 0); sd_engine_note(&e1, 52, 0); sd_engine_note(&e1, 55, 0);
        run_ms(&e1, 300);
        CHECK(!e1.arp_n && gates_on(&e1, nt) == 0u, "ARP: the keys lifted, the run did not stop");
        e1.arp_hold = 1;
        sd_engine_note(&e1, 50, 100); sd_engine_note(&e1, 50, 0);
        run_ms(&e1, 300);
        CHECK(e1.arp_n == 1u && (e1.arp_cur == 50u || e1.arp_cur == 62u), "ARP HOLD did not keep the note (%u notes, %u)", e1.arp_n, e1.arp_cur);
        e1.arp_on = 0;
        run_ms(&e1, 10);
        CHECK(gates_on(&e1, nt) == 0u && !e1.arp_n, "ARP off left a note");
    }
    printf("filter, chords, track B, ARP: LP12 -%.0f dB, LP24 -%.0f dB at 2-5 kHz (cut ~400 Hz); ENV3 +%.0f dB\n", hi0 - hi12, hi0 - hi24, he);
}

/* 2026-11-07: EDIT's OFF (fx_off) is the engine as if its level were 0, bit for bit; a track OFF is silent and keeps
   time; B in MONO is silent; SCL's transpose waits for the track's one */
static void render_patch(const SdPatch *p, int32_t *o, uint32_t frames)
{
    sd_engine_init(&e1); sd_engine_patch(&e1, p);
    run_ms(&e1, 2); sd_engine_note(&e1, 45, 110); run_ms(&e1, 200);
    memset(o, 0, frames * 2u * sizeof *o); sd_engine_render(&e1, o, frames);
}
/* The hand's held keys (the user, 2026-11-06: "in MONO, hold a key, play a second, let it go: back to the first; now
 * it is cut"): a key let go gives its voice back to the latest key still held without one, legato (no new note-on:
 * the voice's age stays). Keys let go out of order, three keys, and DUO's stolen voice too. */
static void hand_keys(SdEngine *e, uint32_t mode)
{
    sd_engine_init(e); sd_engine_patch(e, &SD_PATCH[0]);
    e->mode = mode; run_ms(e, 5);
}
static void test_hand(void)
{
    uint8_t nt[8];
    uint32_t n, age;
    hand_keys(&e1, SD_MODE_MONO);
    sd_engine_note(&e1, 60, 100); run_ms(&e1, 20);
    sd_engine_note(&e1, 64, 100); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 64u, "MONO: the second key does not sound (%u notes, %u)", n, nt[0]);
    age = e1.voice[0].age;
    sd_engine_note(&e1, 64, 0); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 60u, "MONO: the second key let go did not go back to the first (%u notes, %u)", n, nt[0]);
    CHECK(e1.voice[0].age == age && e1.voice[0].inc == sd_note_inc(60), "MONO: back to the first key is not legato");
    sd_engine_note(&e1, 60, 0); run_ms(&e1, 20);
    CHECK(gates_on(&e1, nt) == 0u, "MONO: the last key let go still sounds");
    /* the first key let go first: the second goes on; then nothing */
    sd_engine_note(&e1, 60, 100); sd_engine_note(&e1, 64, 100); run_ms(&e1, 20);
    sd_engine_note(&e1, 60, 0); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 64u, "MONO: the first key let go cut the second (%u notes, %u)", n, nt[0]);
    sd_engine_note(&e1, 64, 0); run_ms(&e1, 20);
    CHECK(gates_on(&e1, nt) == 0u, "MONO: a key let go went back to one no longer held");
    /* three keys: 67 up -> 64, 64 up -> 60; out of order: 64 up (silent) changes nothing, 67 up -> 60 */
    sd_engine_note(&e1, 60, 100); sd_engine_note(&e1, 64, 100); sd_engine_note(&e1, 67, 100); run_ms(&e1, 20);
    sd_engine_note(&e1, 67, 0); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 64u, "MONO: three keys, the last up: not back to the middle one (%u)", nt[0]);
    sd_engine_note(&e1, 64, 0); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 60u, "MONO: three keys, then the middle up: not back to the first (%u)", nt[0]);
    sd_engine_note(&e1, 60, 0);
    sd_engine_note(&e1, 60, 100); sd_engine_note(&e1, 64, 100); sd_engine_note(&e1, 67, 100); run_ms(&e1, 20);
    sd_engine_note(&e1, 64, 0); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 67u, "MONO: a silent held key let go changed the note (%u)", nt[0]);
    sd_engine_note(&e1, 67, 0); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 60u, "MONO: out of order, not back to the first key (%u)", nt[0]);
    sd_engine_note(&e1, 60, 0); run_ms(&e1, 20);
    CHECK(gates_on(&e1, nt) == 0u, "MONO: all keys up, still sounding");
    /* DUO: a third key takes the oldest voice; let go, that voice goes back to the first key */
    hand_keys(&e1, SD_MODE_DUO);
    sd_engine_note(&e1, 60, 100); run_ms(&e1, 5); sd_engine_note(&e1, 64, 100); run_ms(&e1, 5);
    sd_engine_note(&e1, 67, 100); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 2u && nt[0] == 64u && nt[1] == 67u, "DUO: the third key did not take the oldest voice (%u notes)", n);
    sd_engine_note(&e1, 67, 0); run_ms(&e1, 20);
    n = gates_on(&e1, nt);
    CHECK(n == 2u && nt[0] == 60u && nt[1] == 64u, "DUO: the third key let go, not back to the first (%u notes, %u %u)",
          n, nt[0], nt[1]);
    /* ARP on with keys down and off after they lifted: the list is empty, nothing comes back */
    hand_keys(&e1, SD_MODE_MONO);
    sd_engine_note(&e1, 60, 100); run_ms(&e1, 5);
    e1.arp_on = 1; run_ms(&e1, 5);
    sd_engine_note(&e1, 60, 0); run_ms(&e1, 5);
    e1.arp_on = 0; run_ms(&e1, 20);
    sd_engine_note(&e1, 64, 100); run_ms(&e1, 5); sd_engine_note(&e1, 64, 0); run_ms(&e1, 20);
    CHECK(e1.hand_n == 0u && gates_on(&e1, nt) == 0u, "the hand's list kept a key lifted under ARP (%u)", e1.hand_n);
    printf("hand: MONO back to the key still held (legato), out of order, three keys; DUO's taken voice back\n");
}

static void test_switches(void)
{
    static int32_t a[2u * 2048u], b[2u * 2048u];
    static const uint8_t BITS[4] = {SD_FX_RIDER, SD_FX_DESTROY, SD_FX_SPACE, SD_FX_FILTER};
    SdPatch p, q;
    SdPattern pa, pb;
    uint8_t nt[8];
    uint32_t i, k, n;
    for (k = 0; k < 4u; k++) {
        for (i = 0; i < SD_PATCHES; i++) {      /* a stock patch with that engine in */
            const SdPatch *s = &SD_PATCH[i];
            if ((k == 0u && s->r_mix) || (k == 1u && s->d_mix) || (k == 2u && (s->s_dlevel || s->s_rlevel)) || (k == 3u && s->f_type)) break;
        }
        if (i == SD_PATCHES) { p = SD_PATCH[0]; p.f_type = SD_FT_LP12; p.f_cut = 30000; p.e3_s = 127; }   /* (no stock filter) */
        else p = SD_PATCH[i];
        q = p;
        p.fx_off = BITS[k];
        if (k == 0u) q.r_mix = 0;
        if (k == 1u) q.d_mix = 0;
        if (k == 2u) q.s_dlevel = q.s_rlevel = 0;
        if (k == 3u) q.f_type = SD_FT_OFF;
        render_patch(&p, a, 2048u);
        render_patch(&q, b, 2048u);
        CHECK(!memcmp(a, b, sizeof a), "EDIT OFF (engine %u, patch %u) is not the engine at level 0", k, i);
    }
    sd_pattern_default(&pa); sd_pattern_default(&pb);
    for (i = 0; i < SD_PSTEPS; i++) {
        pa.step[i].type = pb.step[i].type = SD_ST_NOTE;
        pa.step[i].note = 72; pb.step[i].note = 36; pa.step[i].gate = pb.step[i].gate = 90;
    }
    pa.len = pb.len = 4; pa.seg_count = pb.seg_count = 1;
    /* B in MONO: silent (A plays); in DUO both */
    sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[0]);
    run_ms(&e1, 5);
    sd_engine_pattern_to(&e1, 0, &pa); sd_engine_pattern_to(&e1, 1, &pb);
    e1.seqb = 1; e1.bpm = 120; e1.seq_run = 1;
    run_ms(&e1, 30);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 72u, "MONO: B sounded (%u notes, %u)", n, nt[0]);
    /* a track OFF: silent, still in time */
    e1.t[0].mute = 1;
    run_ms(&e1, 125);
    CHECK(gates_on(&e1, nt) == 0u, "track A OFF still sounds");
    CHECK(e1.t[0].seq_shown == 1u, "track A OFF stopped its steps (%u)", e1.t[0].seq_shown);
    e1.t[0].mute = 0;
    /* the transpose: +5 asked at step 2, heard from the loop's one (step 1 again) */
    run_ms(&e1, 125);
    e1.t[0].xpose_next = 5;
    run_ms(&e1, 30);
    n = gates_on(&e1, nt);
    CHECK(n == 1u && nt[0] == 72u, "the transpose came before the one (%u)", nt[0]);
    run_ms(&e1, 125u * 2u);                     /* step 4 -> step 1 */
    n = gates_on(&e1, nt);
    CHECK(e1.t[0].seq_shown == 0u && n == 1u && nt[0] == 77u, "the transpose at the one: step %u, note %u", e1.t[0].seq_shown, nt[0]);
    /* the voices' owners and quotas: a note off turns off only its own source's notes; no source takes another's held
       voice; MONO with A playing has no voice for ARP or the hand (REC: the hand takes A's) */
    {
        uint32_t sub, cutA = 0, onA = 0, arpSounds = 0;
        for (i = 0; i < SD_PSTEPS; i++) { pa.step[i].note = 60; pa.step[i].gate = 95; }
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[0]);
        e1.mode = SD_MODE_POLY; run_ms(&e1, 5);
        sd_engine_pattern_to(&e1, 0, &pa); e1.bpm = 120; e1.seq_run = 1;
        e1.arp_on = 1; e1.arp_gate = 20; e1.arp_div = 0;   /* ARP on 60 too, short gates, 1/32 */
        sd_engine_note(&e1, 60, 100);
        for (sub = 0; sub < 2000u; sub++) {
            int32_t oo[64];
            uint32_t v, s60 = 0;
            sd_engine_render(&e1, oo, 32);
            for (v = 0; v < e1.nvoices; v++) s60 += e1.voice[v].gate && e1.voice[v].note == 60u && e1.voice[v].src == SD_SRC_A;
            if (e1.t[0].seq_note_on) { onA++; cutA += !s60; }
        }
        CHECK(onA > 100u && cutA * 50u < onA, "POLY: ARP's note offs cut A's same note (%u of %u)", cutA, onA);
        for (k = 0; k < 2u; k++) {              /* MONO, then DUO: A plays with ARP on */
            sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[0]);
            e1.mode = k ? SD_MODE_DUO : SD_MODE_MONO; run_ms(&e1, 5);
            sd_engine_pattern_to(&e1, 0, &pa); e1.bpm = 120; e1.seq_run = 1;
            e1.arp_on = 1; sd_engine_note(&e1, 64, 100); sd_engine_note(&e1, 67, 100);
            cutA = onA = arpSounds = 0;
            for (sub = 0; sub < 2000u; sub++) {
                int32_t oo[64];
                uint32_t v, sa = 0;
                sd_engine_render(&e1, oo, 32);
                for (v = 0; v < e1.nvoices; v++) {
                    sa += e1.voice[v].gate && e1.voice[v].src == SD_SRC_A;
                    arpSounds += e1.voice[v].gate && e1.voice[v].src == SD_SRC_ARP;
                }
                if (e1.t[0].seq_note_on) { onA++; cutA += !sa; }
            }
            CHECK(onA > 100u && !cutA, "%s: A cut by ARP %u of %u times", k ? "DUO" : "MONO", cutA, onA);
            CHECK(k ? arpSounds > 100u : !arpSounds, "%s: ARP %s (%u)", k ? "DUO" : "MONO", k ? "silent" : "took the voice", arpSounds);
        }
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[0]);   /* MONO, A playing: the hand has no voice */
        run_ms(&e1, 5);
        sd_engine_pattern_to(&e1, 0, &pa); e1.bpm = 120; e1.seq_run = 1;
        run_ms(&e1, 30);
        sd_engine_note(&e1, 72, 100); run_ms(&e1, 2);
        CHECK(gates_on(&e1, nt) == 1u && nt[0] == 60u, "MONO: a key took A's voice (%u)", nt[0]);
        sd_engine_note(&e1, 72, 0);
        e1.rec_trk = 0;                         /* REC on A: the key is heard */
        sd_engine_note(&e1, 74, 100); run_ms(&e1, 2);
        CHECK(gates_on(&e1, nt) == 1u && nt[0] == 74u, "MONO REC: the key is not heard (%u)", nt[0]);
        sd_engine_note(&e1, 74, 0); run_ms(&e1, 2);
        CHECK(e1.voice[0].note != 74u || !e1.voice[0].gate, "MONO REC: the key's note stuck");
    }
    printf("switches: EDIT OFF = level 0 (RIDER DESTROY SPACE FILTER), track OFF, B in MONO, XPOSE at the one; "
           "the voices' owners and quotas\n");
}

int main(void)
{
    test_lut();
    test_identity();
    test_warp();
    test_fold();
    test_extremes();
    test_pm();
    test_routing();
    test_patch_lock();
    test_vector();
    test_vec2();
    test_modes();
    test_stereo();
    test_alias();
    test_gen();
    test_rider_bypass();
    test_rider_response();
    test_rider_pos();
    test_rider_clock();
    test_rider_extremes();
    test_rider_kp();
    test_rider_ap();
    test_rider_rest();
    test_rider_makeup();
    test_rider_saturation();
    test_destroy();
    test_dynamics();
    test_rider_r4();
    test_feedback();
    test_space();
    test_mod();
    test_seq();
    test_filter_seq_arp();
    test_switches();
    test_hand();
    test_env();
    test_silence();
    test_chunking();
    test_timing();
    test_clicks();
    printf(fails ? "sd_test: %d FAILED\n" : "sd_test: all passed\n", fails);
    return fails ? 1 : 0;
}
