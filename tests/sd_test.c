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
 * V2, the bank (§3.3):
 *   - warp passes its anchors: sine 0, triangle 20, saw 40, pulse 99, with no seam at the hand-overs
 *   - PM is the formula cos(phi + beta cos(phi)), beta up to 4 pi (level x ENV2 x XM)
 *   - the roles route left to right (S the next one, P the nearest carrier)
 *   - every algorithm, all at max: no overflow, under the carriers' levels
 *   - the audio never takes a half-written patch
 * R1, the Reactor: MIX 0 is no Reactor at all; each filter peaks where it is set (VBP at 0 dB, BP6 louder as
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

/* GLASS and BELL as V2 made them, the plain test sounds (out of the bank for sounding like classic FM). GLASS: four
 * carriers on harmonics 1 2 3 4, no FM, AFTER -> XM and WHEEL
 * -> WARP and nothing else; BELL: two FM stacks, 7 on 2 and 5 on 1, its FM falling away, in a room */
#define C_ SD_ROLE_C
#define S_ SD_ROLE_S
static const SdPatch T_GLASS = {
    .bank = SD_SAME(BK_GLASS), SD_CORNER_A,
    .e1_a = 20, .e1_d = 80, .e1_s = 90, .e1_r = 60, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
    .mod = {{SD_MS_AFTER, SD_MD_XM, 9929}, {SD_MS_WHEEL, SD_MD_WARP, 13239}}, .mac4 = SD_M4_AIR};
static const SdPatch T_BELL = {
    .bank = SD_SAME(BK_BELL), SD_CORNER_A, .xm = 40000,
    .e1_a = 0, .e1_d = 110, .e1_s = 0, .e1_r = 90, .e2_a = 0, .e2_d = 85, .e2_s = 0, .e2_r = 80,
    .s_size = 52000, .s_decay = 46000, .s_rtone = 42000, .s_rlevel = 14000};
#undef C_
#undef S_

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
    for (p = 0; p < SD_PATCHES; p++) {       /* every patch: the fold ramp, the Reactor, DISTROY, SPACE continue
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

/* the warp: sine 0, triangle 20, saw 40, pulse 99 */
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
static void test_nav(void)                   /* the weights: corners, centre, DUO, POLY */
{
    static const uint16_t CX[4] = {0, 65535, 65535, 0}, CY[4] = {65535, 65535, 0, 0};   /* A B C D */
    int32_t w[SD_BANKS];
    uint32_t c, b;
    for (c = 0; c < 4u; c++) {
        sd_engine_init(&e1);
        e1.patch.x = CX[c]; e1.patch.y = CY[c];
        sd_nav(&e1, w);
        for (b = 0; b < SD_BANKS; b++)
            CHECK(b == c ? w[b] >= 32700 : w[b] == 0, "corner %c: bank %c weighs %d", 'A' + c, 'A' + b, w[b]);
    }
    sd_engine_init(&e1);
    e1.patch.x = e1.patch.y = 32768;
    sd_nav(&e1, w);
    for (b = 0; b < SD_BANKS; b++)
        CHECK(w[b] > 11585 - 120 && w[b] < 11585 + 120, "centre: bank %c weighs %d, not 0.354", 'A' + b, w[b]);
    e1.mode_on = SD_MODE_DUO;
    sd_nav(&e1, w);
    CHECK(w[0] == w[2] && w[1] == w[3] && w[0] > 19483 - 120 && w[0] < 19483 + 120, "DUO at X 0.5: %d %d", w[0], w[1]);
    e1.mode_on = SD_MODE_POLY;
    sd_nav(&e1, w);
    CHECK(w[0] == SD_POLY_GAIN && w[3] == SD_POLY_GAIN, "POLY: %d", w[0]);
}

static void test_course(void)                     /* VOYAGE's COURSE: from the gate, A -> B -> C -> D, 1.5 s each, then held */
{
    static const struct { double s; uint16_t x, y; } AT[] = {{0.02, 0, 65535}, {1.5, 65535, 65535}, {3.0, 65535, 0},
                                                            {4.5, 0, 0}, {6.0, 0, 0}};
    uint32_t i, done = 0;
    SdPatch p = SD_PATCH[SD_P_VOYAGE];
    memset(p.mod, 0, sizeof p.mod);             /* COURSE alone (VOYAGE's LFOs move X and Y on top: C1) */
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 45, 100);
    for (i = 0; i < 5u; i++) {
        uint32_t to = (uint32_t)(AT[i].s * SD_FS);
        int32_t o[2u * SD_SUB];
        while (done < to) { run(&e1, o, SD_SUB, SD_SUB); done += SD_SUB; }
        CHECK(abs((int)e1.nav_x - AT[i].x) < 1200 && abs((int)e1.nav_y - AT[i].y) < 1200, "COURSE at %.2f s: %u %u, not %u %u",
              AT[i].s, e1.nav_x, e1.nav_y, AT[i].x, AT[i].y);
    }
}

static void test_modes(void)                    /* POLY and DUO allocate and steal; a mode waits for quiet */
{
    static const uint8_t CH[5] = {48, 52, 55, 59, 62};
    int32_t o[2u * SD_SUB];
    uint32_t i;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &T_BELL);
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

/* the stereo (a patch may lean, never hard left or right): WIDTH 0 is mono; a corner's bank alone, at WIDTH's most,
 * plays in the middle; and no factory patch's note, MONO or POLY, leans more than 6.5 dB either way (WIDTH's 6 dB at
 * most, the rest the Reactor's own stereo), POLY's at WIDTH's most 2.6 dB at most */
static double st_lr(SdEngine *en, const uint8_t *notes, uint32_t n)   /* L - R, dB, from the note on for 0.25 s */
{
    double l = 0, r = 0;
    uint32_t i, f;
    for (i = 0; i < n; i++) sd_engine_note(en, notes[i], 100);
    for (f = 0; f < SD_FS / 4u; f += SD_SUB) {
        run(en, a, SD_SUB, SD_SUB);
        for (i = 0; i < SD_SUB; i++) { l += (double)a[2u * i] * a[2u * i]; r += (double)a[2u * i + 1u] * a[2u * i + 1u]; }
    }
    for (i = 0; i < n; i++) sd_engine_note(en, notes[i], 0);
    run(en, a, SD_FS / 2u, 256);
    return 10 * log10((l + 1) / (r + 1));
}
static void test_stereo(void)
{
    static const uint8_t NT[4] = {69, 72, 74, 76};
    uint32_t i, pi, md, k;
    double l = 0, r = 0, worst = 0, wpoly = 0, d;
    int same = 1;
    const char *wn = "";
    SdPatch p = SD_PATCH[SD_P_AURORA];
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &T_GLASS);
    sd_engine_note(&e1, 57, 100);
    run(&e1, a, SD_FS / 4u, 256);
    for (i = 0; i < SD_FS / 4u; i++) same &= a[2u * i] == a[2u * i + 1u];
    CHECK(same, "WIDTH 0 is not mono");
    p.width = 65535;                            /* corner A, WIDTH at its most: bank A alone, in the middle */
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 57, 100);
    run(&e1, a, SD_FS / 4u, 256);
    for (i = SD_FS / 8u; i < SD_FS / 4u; i++) { l += (double)a[2u * i] * a[2u * i]; r += (double)a[2u * i + 1u] * a[2u * i + 1u]; }
    CHECK(fabs(sqrt(r / l) - 1) < 0.02, "bank A alone at WIDTH 1: right / left %.3f, not 1", sqrt(r / l));
    for (pi = 0; pi < SD_PATCHES; pi++)
        for (md = 0; md < 2u; md++) {
            p = SD_PATCH[pi];
            p.s_dlevel = 0; p.s_rlevel = 0;
            sd_engine_init(&e1);
            e1.mode = md ? SD_MODE_POLY : SD_MODE_MONO;
            sd_engine_patch(&e1, &p);
            run(&e1, a, 4096u, 256);
            for (k = 0; k < 5u; k++) {
                d = fabs(k < 4u ? st_lr(&e1, &NT[k], 1) : st_lr(&e1, NT, 4));
                if (d > worst) { worst = d; wn = SD_PATCH_NAME[pi]; }
                if (md && k < 4u && d > wpoly && (pi == SD_P_AURORA || pi == SD_P_PRISM || pi == SD_P_SHARDS)) wpoly = d;
                                                /* (WIDTH at its most, no Reactor) */
            }
        }
    printf("stereo: a corner's bank alone at WIDTH 1 in the middle (right / left %.3f); the factory patches' notes "
           "lean %.1f dB at most (%s), POLY's at WIDTH 1 %.1f dB\n", sqrt(r / l), worst, wn, wpoly);
    CHECK(worst < 6.5, "%s leans %.1f dB: never hard left or right", wn, worst);
    CHECK(wpoly < 2.6, "a POLY note leans %.1f dB at WIDTH 1", wpoly);
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

/* An oscillator that turns into a modulator with nothing to its right (a patch, ALGORITHM or a generator changing a
 * bank's roles while it sounds) is silent at once. Its level used to fade from the old one for a sub-block, written
 * into a bus that does not exist (bus[SD_NOWHERE]: DERELICT's patch changes found it, the sanitizers on Linux) */
static void test_nowhere(void)
{
    SdPatch p = T_GLASS;                        /* four carriers */
    int32_t o[2u * SD_SUB];
    uint32_t i, b;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 57, 100);
    for (i = 0; i < 64u; i++) run(&e1, o, SD_SUB, SD_SUB);
    for (b = 0; b < SD_BANKS; b++) p.bank[b].osc[3].role = SD_ROLE_S;   /* the 4th: a series modulator, nothing right */
    sd_engine_patch(&e1, &p);
    for (i = 0; i < 64u; i++) run(&e1, o, SD_SUB, SD_SUB);
    for (b = 0; b < SD_BANKS; b++)
        CHECK(e1.bank[b].osc[3].lvl == 0 && e1.bank[b].osc[3].lvl_next == 0, "bank %u's 4th, feeding nothing, at %d",
              b, e1.bank[b].osc[3].lvl);
    printf("nowhere: a modulator with nothing to its right is silent at once, its bus never written\n");
}

static void test_gen(void)                      /* the generators */
{
    SdPatch p = T_BELL;
    uint32_t seed = 1, g, k, i;
    sd_gen(&p, 1, 0, &seed);
    CHECK(p.bank[1].osc[0].harm == 1 && p.bank[1].osc[3].harm == 4, "ALL");
    CHECK(p.bank[1].osc[1].level == T_BELL.bank[1].osc[1].level, "ALL changed a level");
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

/* ---- R1, the Reactor */
static uint16_t midi256(double hz) { return (uint16_t)lround(256.0 * (69.0 + 12.0 * log2(hz / 440.0))); }

/* one filter of the Reactor alone, its centre at hz: the gain (dB) of a sine at test_hz, after it settles */
static double reactor_gain(uint32_t model, uint16_t res, double hz, double test_hz)
{
    static int32_t in[2u * SD_SUB], out[2u * SD_SUB];
    SdPatch p = SD_PATCH_SINE;
    uint32_t i, b, k;
    double ph = 0, a2 = 0, o2 = 0;
    for (k = 0; k < 6u; k++) p.stage[k].pitch[0] = midi256(hz), p.stage[k].level[0] = 32768;
    p.r_model = (uint8_t)model; p.r_res = res; p.r_drive = 16384; p.r_mix = 65535;
    sd_engine_init(&e1);
    e1.patch = p;
    sd_reactor_params(&e1);
    for (b = 0; b < 3000u; b++) {               /* 2.2 s: the last 0.7 s measured */
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = (int32_t)lround(1000.0 * sin(ph));
            ph += 2 * M_PI * test_hz / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            out[2u * i] = out[2u * i + 1u] = 0;
        }
        sd_reactor_run(&e1, in, out, SD_SUB, 0);
        if (b >= 2000u)
            for (i = 0; i < SD_SUB; i++) { a2 += (double)in[2u * i] * in[2u * i]; o2 += (double)out[2u * i] * out[2u * i]; }
    }
    return 10 * log10(o2 / a2) - 20 * log10(SD_RMAKEUP[model] / 4096.0);   /* the shape, without the make-up */
}

static void test_reactor_response(void)
{
    static const double HZ[] = {100, 440, 2000, 8000, 13000};
    uint32_t h, m;
    for (h = 0; h < 5u; h++) {
        double best = -1e9, at = 0, f, c;
        for (f = HZ[h] * 0.8; f <= HZ[h] * 1.25; f *= 1.005) {   /* the peak, VBP at a high RES */
            double g = reactor_gain(SD_RM_VBP, 52000, HZ[h], f);
            if (g > best) { best = g; at = f; }
        }
        c = reactor_gain(SD_RM_VBP, 52000, HZ[h], HZ[h]);
        printf("reactor VBP at %5.0f Hz: peak at %7.1f Hz (%+.2f dB); an octave off %+.1f dB\n", HZ[h], at, best,
               reactor_gain(SD_RM_VBP, 52000, HZ[h], HZ[h] * (HZ[h] < 10000 ? 2.0 : 0.5)));
        CHECK(fabs(at / HZ[h] - 1) < 0.02, "reactor VBP at %.0f Hz peaks at %.1f Hz", HZ[h], at);
        CHECK(fabs(c) < 0.5, "reactor VBP at %.0f Hz: %.2f dB at its centre, not 0", HZ[h], c);
    }
    for (m = 0; m <= SD_RM_VBP; m++) {          /* each band-pass model: a band, not a shelf */
        double c = reactor_gain(m, 30000, 1000, 1000), lo = reactor_gain(m, 30000, 1000, 250), hi = reactor_gain(m, 30000, 1000, 4000);
        printf("reactor model %u at 1 kHz: centre %+.1f, 250 Hz %+.1f, 4 kHz %+.1f dB\n", m, c, lo, hi);
        CHECK(c > lo + 9 && c > hi + 9, "reactor model %u is not a band-pass", m);
    }
    CHECK(reactor_gain(SD_RM_BP12, 30000, 1000, 4000) < reactor_gain(SD_RM_BP6, 30000, 1000, 4000) - 6,
          "BP12 is not steeper than BP6");
    CHECK(reactor_gain(SD_RM_BP6, 60000, 1000, 1000) > reactor_gain(SD_RM_BP6, 20000, 1000, 1000) + 10,
          "BP6 does not grow louder as it rings");
}

/* ---- R2: the delay models and the allpasses */
static SdPatch reactor_patch(uint32_t model, uint16_t res, double hz)   /* all four filters at hz, level 1 */
{
    SdPatch p = SD_PATCH_SINE;
    uint32_t k, f;
    for (k = 0; k < 6u; k++)
        for (f = 0; f < 4u; f++) p.stage[k].pitch[f] = midi256(hz), p.stage[k].level[f] = 32768;
    p.r_model = (uint8_t)model; p.r_res = res; p.r_drive = 16384; p.r_mix = 65535;
    return p;
}

/* the Reactor alone on a test signal: an impulse (sine_hz 0) or a sine; frames of output, left / right */
static void reactor_feed(const SdPatch *p, double sine_hz, uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    double ph = 0;
    sd_engine_init(&e1);
    e1.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_reactor_params(&e1);
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = sine_hz > 0 ? (int32_t)lround(1000.0 * sin(ph)) : (b == 0 && i == 0 ? 20000 : 0);
            ph += 2 * M_PI * sine_hz / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_reactor_run(&e1, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
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

static void test_reactor_kp(void)
{
    static const double HZ[] = {55, 110, 220, 440, 880, 1760};
    uint32_t h;
    double worst = 0, t1, t2;
    for (h = 0; h < 6u; h++) {                  /* pinged: the comb's pitch, in cents off */
        SdPatch p = reactor_patch(SD_RM_KP, 60000, HZ[h]);
        double f, c;
        reactor_feed(&p, 0, SD_FS / 2u, a);
        f = ac_pitch(a + 2u * 2000u, SD_FS / 2u - 2000u, HZ[h]);
        c = 1200.0 * log2(f / HZ[h]);
        if (fabs(c) > worst) worst = fabs(c);
        printf("reactor KP at %6.1f Hz: rings at %8.2f Hz (%+.1f cents)\n", HZ[h], f, c);
    }
    CHECK(worst < 5.0, "KP is %.1f cents off its pitch", worst);
    {                                           /* RES: a longer ring */
        SdPatch p = reactor_patch(SD_RM_KP, 40000, 220);
        uint32_t i;
        double e0 = 0, e1v = 0;
        reactor_feed(&p, 0, SD_FS, a);
        for (i = 0; i < 4410u; i++) e0 += (double)a[2u * i] * a[2u * i], e1v += (double)a[2u * (i + 22050u)] * a[2u * (i + 22050u)];
        t1 = 10 * log10(e1v / e0);
        p = reactor_patch(SD_RM_KP, 62000, 220);
        reactor_feed(&p, 0, SD_FS, a);
        e0 = e1v = 0;
        for (i = 0; i < 4410u; i++) e0 += (double)a[2u * i] * a[2u * i], e1v += (double)a[2u * (i + 22050u)] * a[2u * (i + 22050u)];
        t2 = 10 * log10(e1v / e0);
        printf("reactor KP at 220 Hz: after 0.5 s %.1f dB (RES 61) / %.1f dB (RES 94)\n", t1, t2);
        CHECK(t2 > t1 + 10, "KP: RES does not lengthen the ring");
    }
}

static void test_reactor_ap(void)                 /* RES 0: flat; RES max: notches, left and right apart */
{
    static const double HZ[] = {200, 700, 1500, 3000, 6000};
    uint32_t m, h, i;
    for (m = SD_RM_AP1; m <= SD_RM_AP2; m++) {
        double flat = 0, top = 0, lmin = 1e9, rmin = 1e9, lmax = -1e9;
        for (h = 0; h < 5u; h++) {
            SdPatch p = reactor_patch(m, 0, 1000);
            double ein = 0, el = 0;
            reactor_feed(&p, HZ[h], SD_FS / 2u, a);
            for (i = SD_FS / 4u; i < SD_FS / 2u; i++) { double x = 1000.0 * sin(2 * M_PI * HZ[h] * i / SD_FS); ein += x * x; el += (double)a[2u * i] * a[2u * i]; }
            double g = 10 * log10(el / ein) - 20 * log10(SD_RMAKEUP[m] / 4096.0);   /* without the make-up */
            if (h == 4u) top = g;               /* 6 kHz: AP1's linear reads in its loops dip there */
            else if (fabs(g) > flat) flat = fabs(g);
        }
        for (h = 0; h < 120u; h++) {            /* RES max: sweep 100 Hz .. 10 kHz */
            double hz = 100 * pow(100.0, h / 119.0), ein = 0, el = 0, er = 0, gl, gr;
            SdPatch p = reactor_patch(m, 65535, 1000);
            p.stage[0].pitch[1] = midi256(1300); p.stage[0].pitch[2] = midi256(2100); p.stage[0].pitch[3] = midi256(3400);
            reactor_feed(&p, hz, SD_FS / 4u, a);
            for (i = SD_FS / 8u; i < SD_FS / 4u; i++) {
                double x = 1000.0 * sin(2 * M_PI * hz * i / SD_FS);
                ein += x * x; el += (double)a[2u * i] * a[2u * i]; er += (double)a[2u * i + 1u] * a[2u * i + 1u];
            }
            gl = 10 * log10(el / ein + 1e-12); gr = 10 * log10(er / ein + 1e-12);
            if (gl < lmin) lmin = gl;
            if (gr < rmin) rmin = gr;
            if (gl > lmax) lmax = gl;
        }
        printf("reactor AP%u: RES 0 within %.2f dB of flat to 3 kHz (6 kHz %+.1f dB); RES max: left %.1f .. %+.1f dB, "
               "right down to %.1f dB\n", m - SD_RM_AP1 + 1u, flat, top, lmin, lmax, rmin);
        CHECK(flat < 1.5, "AP%u at RES 0 is %.2f dB off flat", m - SD_RM_AP1 + 1u, flat);
        CHECK(lmin < -15 && rmin < -15 && lmax > 3, "AP%u at RES max has no notches", m - SD_RM_AP1 + 1u);
    }
}

static void test_reactor_rest(void)               /* a comb rings out, then rests: no work while silent */
{
    SdPatch p = reactor_patch(SD_RM_KP, 60000, 220);
    uint32_t t;
    int32_t o[2u * SD_SUB];
    p.e1_r = 30;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &p);
    sd_engine_note(&e1, 57, 100);
    for (t = 0; t < 100u; t++) run(&e1, o, SD_SUB, SD_SUB);
    CHECK(!e1.r.rest, "the Reactor rests while a note sounds");
    sd_engine_note(&e1, 57, 0);
    for (t = 0; t < 12000u && !e1.r.rest; t++) run(&e1, o, SD_SUB, SD_SUB);   /* RES 92: a long ring */
    printf("reactor KP rests %.2f s after the release\n", t * SD_SUB / (double)SD_FS);
    CHECK(e1.r.rest, "the Reactor never rests after a comb rang out");
}

/* ---- R3: each model's level against the dry voice: a saw through VOWEL's stage A, RES and level one half */
static double reactor_vs_dry(uint32_t model)
{
    SdPatch p = SD_PATCH[SD_P_VOWEL];
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

static void test_reactor_makeup(void)             /* switching model keeps the level: each within 3 dB of the dry */
{
    static const char *const MN[SD_RMODELS] = {"BP6", "BP12", "VBP", "KP", "AP1", "AP2"};
    uint32_t m;
    printf("reactor against the dry voice:");
    for (m = 0; m < SD_RMODELS; m++) {
        double g = reactor_vs_dry(m);
        printf(" %s %+.1f", MN[m], g);
        CHECK(fabs(g) < 3.0, "reactor %s is %+.1f dB from the dry voice", MN[m], g);
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

static void test_reactor_saturation(void)         /* clean while small; DRIVE and RES ring into the loop's ceiling */
{
    static int32_t in[2u * SD_SUB];
    SdPatch p = reactor_patch(SD_RM_BP6, 52000, 1000);
    uint32_t b, i, d, k;
    double h[2], pk[2], ph;
    for (d = 0; d < 2u; d++) {
        ph = 0; pk[d] = 0;
        p.r_drive = d ? 65535 : 16384;
        for (k = 0; k < 6u; k++) p.stage[k].level[0] = 16384, p.stage[k].level[1] = p.stage[k].level[2] = p.stage[k].level[3] = 0;
        sd_engine_init(&e1);
        e1.patch = p;
        for (b = 0; b < 2000u; b++) {
            sd_reactor_params(&e1);
            for (i = 0; i < SD_SUB; i++) {
                int32_t x = (int32_t)lround((d ? 8000.0 : 300.0) * sin(ph));
                ph += 2 * M_PI * 1000.0 / SD_FS;
                in[2u * i] = in[2u * i + 1u] = x;
                a[2u * (b * SD_SUB + i)] = a[2u * (b * SD_SUB + i) + 1u] = 0;
            }
            sd_reactor_run(&e1, in, a + 2u * b * SD_SUB, SD_SUB, 0);
        }
        h[d] = h3_of(a + 2u * 40000u, 16384u, 1000.0);
        for (i = 40000u; i < 56384u; i++) if (abs(a[2u * i]) > pk[d]) pk[d] = abs(a[2u * i]);
        for (i = 0; i < 4u; i++) CHECK(abs(e1.r.ic1[0][i]) < (16 << 19), "an integrator past 16 x full scale");
    }
    printf("reactor BP6 at RES 79: a quiet sine h3/h1 %.4f %%; DRIVE 4x on a loud one %.1f %% (peak %.0f)\n", 100 * h[0],
           100 * h[1], pk[1]);
    CHECK(h[0] < 0.001, "the Reactor is not clean while small: h3/h1 %.4f", h[0]);
    CHECK(h[1] > 0.03, "the Reactor does not saturate when driven: h3/h1 %.4f", h[1]);
    CHECK(SD_RLEVEL[64] == 4096 && SD_RLEVEL[128] == 16384 && SD_RLEVEL[0] == 0, "the level law's anchors");
    for (i = 1; i < 129u; i++) CHECK(SD_RLEVEL[i] > SD_RLEVEL[i - 1u], "the level law falls at %u", i);
}

/* ---- D1: DISTROY's crossover, bias, shapers */
static SdPatch distroy_patch(uint32_t type, uint16_t gain, uint16_t alt, int16_t bias)   /* every band alike */
{
    SdPatch p = SD_PATCH_SINE;
    uint32_t k;
    p.d_split[0] = midi256(150); p.d_split[1] = midi256(800); p.d_split[2] = midi256(4000);
    for (k = 0; k < 4u; k++) { p.d_type[k] = (uint8_t)type; p.d_gain[k] = gain; p.d_alt[k] = alt; p.d_bias[k] = bias; p.d_level[k] = 32768; }
    p.d_mix = 65535;
    return p;
}

/* DISTROY alone on a sine of amp at hz: frames of output (left / right), after a settle */
static void distroy_feed(const SdPatch *p, double hz, double amp, uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    double ph = 0;
    sd_engine_init(&e1);
    e1.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_distroy_params(&e1);
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = (int32_t)lround(amp * sin(ph));
            ph += 2 * M_PI * hz / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_distroy_run(&e1, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
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

static void test_distroy(void)
{
    static const double HZ[] = {60, 300, 1800, 9000};       /* a band's middle each, splits 150 800 4000 */
    uint32_t i, h, k, t, x;
    for (x = SD_DX_CMP; x <= SD_DX_LR4; x++) {   /* both crossovers: they sum back; how much the bands bleed */
    {   /* neutral: the bands sum to the input (the DC blockers aside) */
        double worst = 0;
        for (h = 0; h < 4u; h++) {
            SdPatch p = distroy_patch(SD_DS_NONE, 0, 0, 0);
            p.d_xover = (uint8_t)x;
            double ein = 0, eo = 0;
            distroy_feed(&p, HZ[h] < 100 ? 200 : HZ[h], 8000, SD_FS / 2u, a);
            for (i = SD_FS / 4u; i < SD_FS / 2u; i++) { double x = 8000 * sin(2 * M_PI * (HZ[h] < 100 ? 200 : HZ[h]) * i / SD_FS); ein += x * x; eo += (double)a[2u * i] * a[2u * i]; }
            if (fabs(10 * log10(eo / ein)) > worst) worst = fabs(10 * log10(eo / ein));
        }
        printf("distroy %s neutral: the bands sum to the input within %.3f dB\n", x ? "LR4" : "CMP", worst);
        CHECK(worst < 0.05, "DISTROY's bands do not sum back: %.3f dB", worst);
    }
    {   /* the slope: each band alone (the others' level 0), a sine at each band's middle: how much of it it passes */
        printf("distroy %s bands (rows: a sine at 60 300 1800 9000 Hz; columns: band 1..4 alone, dB)\n", x ? "LR4" : "CMP");
        for (h = 0; h < 4u; h++) {
            printf("   %5.0f Hz:", HZ[h]);
            for (k = 0; k < 4u; k++) {
                SdPatch p = distroy_patch(SD_DS_NONE, 0, 0, 0);
                double ein = 0, eo = 0;
                p.d_xover = (uint8_t)x;
                for (t = 0; t < 4u; t++) p.d_level[t] = t == k ? 32768 : 0;
                distroy_feed(&p, HZ[h], 8000, SD_FS / 2u, a);
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
            SdPatch p = distroy_patch(t, 43690, 32768, 0);
            double h2, h3;
            int32_t peak = 0;
            distroy_feed(&p, 1000, 8000, SD_FS / 2u, a);
            h2 = harm_db(a + 2u * 8192u, 8192u, 1000, 2); h3 = harm_db(a + 2u * 8192u, 8192u, 1000, 3);
            for (i = 8192u; i < 16384u; i++) if (abs(a[2u * i]) > peak) peak = abs(a[2u * i]);
            printf("distroy %-5s +24 dB ALT 50: h2 %6.1f h3 %6.1f dB, peak %d\n", SN[t], h2, h3, peak);
            CHECK(h3 > -30, "%s does not distort", SN[t]);
            CHECK(peak <= 4 * 32768, "%s reaches %d", SN[t], peak);
            if (t == SD_DS_SOFT) CHECK(h2 > -40, "SOFT is not asymmetric: h2 %.1f dB", h2);
            if (t == SD_DS_HARD) CHECK(h2 < -50, "HARD is not symmetric: h2 %.1f dB", h2);
        }
    }
    {   /* BIAS: before HARD, even harmonics; after it no DC */
        SdPatch p = distroy_patch(SD_DS_HARD, 43690, 0, 20000);
        double dc = 0;
        distroy_feed(&p, 1000, 8000, SD_FS, a);
        for (i = SD_FS / 2u; i < SD_FS; i++) dc += a[2u * i];
        dc /= SD_FS / 2u;
        printf("distroy HARD with BIAS +0.3: h2 %.1f dB, DC %.1f\n", harm_db(a + 2u * 24000u, 8192u, 1000, 2), dc);
        CHECK(harm_db(a + 2u * 24000u, 8192u, 1000, 2) > -25, "BIAS makes no even harmonics");
        CHECK(fabs(dc) < 30, "a DC of %.1f after the blocker", dc);
    }
    {   /* CRUSH at ALT max: a handful of levels */
        SdPatch p = distroy_patch(SD_DS_CRUSH, 0, 65535, 0);
        uint32_t n = 0;
        int32_t seen[64];
        p.d_level[0] = p.d_level[1] = p.d_level[2] = 0;   /* the top band alone: the blocker barely moves it */
        distroy_feed(&p, 9000, 30000, SD_FS / 4u, a);
        (void)seen;
        for (i = 0; i < 4u; i++) n += (uint32_t)(e1.d.crush[i] == 8192);
        CHECK(n == 4u, "CRUSH's step at ALT max is not 2^13");
    }
    {   /* MIX 0: no DISTROY at all, bit for bit */
        SdPatch p = SD_PATCH[SD_P_VOYAGE];
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[SD_P_VOYAGE]); sd_engine_note(&e1, 50, 100); run(&e1, a, SD_FS / 2u, 256);
        p.d_type[0] = SD_DS_FOLD; p.d_gain[0] = 65535; p.d_mix = 0;
        sd_engine_init(&e2); sd_engine_patch(&e2, &p); sd_engine_note(&e2, 50, 100); run(&e2, b, SD_FS / 2u, 256);
        for (i = 0; i < SD_FS; i++) if (a[i] != b[i]) break;
        CHECK(i == SD_FS, "DISTROY at MIX 0 changed the sound");
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
            CHECK(peak <= 4 * 4 * 4 * 32768, "DISTROY shaper %u at max: peak %d", t, peak);   /* 4 bands x 4 FS x 4 */
        }
    }
}

/* ---- S1: SPACE */
static SdSpaceMem tsm;
static uint8_t space_one;                       /* SPACE on one core (the delay, the room, the looper), else the
                                                   delay here and the second core's plate and looper (here run in the
                                                   render, no hooks) */

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
    e1.sp_room = space_one;
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

/* ---- D2: DISTROY's dynamics. One band (2, 150 .. 800 Hz) alone, NONE, a 300 Hz sine whose peak follows env(t) */
static SdEngine dyn_e;
static void dyn_feed(const SdPatch *p, double (*env)(double), uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    sd_engine_init(&dyn_e);
    dyn_e.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_distroy_params(&dyn_e);
        for (i = 0; i < SD_SUB; i++) {
            double t = (double)(b * SD_SUB + i) / SD_FS;
            in[2u * i] = in[2u * i + 1u] = (int32_t)lround(env(t) * sin(2 * M_PI * 300 * t));
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_distroy_run(&dyn_e, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
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
    SdPatch p = distroy_patch(SD_DS_NONE, 0, 0, 0);
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

/* ---- D3: DISTROY's feedback, TAME, the limiter. Every band NONE at 0 dB, MIX full; input from a function */
static SdEngine d3_e;
static void d3_feed(const SdPatch *p, int32_t (*src)(uint32_t), uint32_t frames, int32_t *outbuf)
{
    static int32_t in[2u * SD_SUB];
    uint32_t i, b;
    sd_engine_init(&d3_e);
    d3_e.patch = *p;
    for (b = 0; b * SD_SUB < frames; b++) {
        sd_distroy_params(&d3_e);
        for (i = 0; i < SD_SUB; i++) {
            in[2u * i] = in[2u * i + 1u] = src(b * SD_SUB + i);
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_distroy_run(&d3_e, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
    }
}
static int32_t src_burst(uint32_t i) { return i < 44u ? (int32_t)lround(8192 * sin(2 * M_PI * 1000 * i / SD_FS)) : 0; }
static int32_t src_noise(uint32_t i) { static uint32_t r = 1; r = r * 1664525u + 1013904223u; return i < SD_FS / 4u ? (int32_t)(r >> 17) - 16384 : 0; }
static double d3_hz;
static int32_t src_sine(uint32_t i) { return (int32_t)lround(8192 * sin(2 * M_PI * d3_hz * i / SD_FS)); }
static SdPatch d3_patch(void)
{
    SdPatch p = distroy_patch(SD_DS_NONE, 0, 0, 0);
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
                                                   bit through the DC blocker's floor, -84 dB; DISTROY rests there */
    }
    {   /* everything at max: +36 dB in every band, FB 0.9, noise in: it screams, bounded */
        SdPatch p = distroy_patch(SD_DS_NONE, 65535, 0, 0);
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
        SdPatch p = distroy_patch(SD_DS_NONE, 65535, 0, 0);
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

static void test_space_mode(uint32_t one)       /* one: the delay and the room on one core; else the delay and the plate */
{
    uint32_t at, i;
    const char *mode = one ? "one core" : "two cores";
    space_one = (uint8_t)one;
    {   /* a free 250 ms: the echo 11025 frames on, its loudest within a frame or two (the tone's lowpass) */
        SdPatch p = space_patch(SD_SPMS(250u), 0, 0, 65535, 0, 0);
        space_feed(&p, 0, 0, SD_FS / 2u, a, 120);
        at = first_peak(a, 0, 100, SD_FS / 2u);
        printf("space (%s): a free 250 ms echo at frame %u (11025)\n", mode, at);
        CHECK(at >= 11024u && at <= 11027u, "the 250 ms echo is at frame %u", at);
    }
    {   /* synced: 1/8 at 120 BPM = 0.25 s; 1/4D at 100 BPM = 0.9 s */
        SdPatch p = space_patch(SD_SPDIV(5), SD_SP_SYNC, 0, 65535, 0, 0);
        space_feed(&p, 0, 0, SD_FS / 2u, a, 120);
        at = first_peak(a, 0, 100, SD_FS / 2u);
        CHECK(at >= 11024u && at <= 11027u, "1/8 at 120 BPM: the echo at frame %u, not 11025", at);
        p = space_patch(SD_SPDIV(10), SD_SP_SYNC, 0, 65535, 0, 0);
        space_feed(&p, 0, 0, SD_FS, b, 100);
        at = first_peak(b, 0, 100, SD_FS);
        printf("space (%s): synced 1/8 at 120 BPM, 1/4D at 100 BPM: frame %u (39690)\n", mode, at);
        CHECK(at >= 39689u && at <= 39692u, "1/4D at 100 BPM: the echo at frame %u, not 39690", at);
    }
    {   /* on the beat, always: a triplet exact to
           its fourth echo; a division longer than the line halved (1/2 at 100 BPM plays 1/4, 1/4D at 84 plays 1/8D,
           1/2 at 40 plays 1/8), never held at 1 s; 1/2 at 120 BPM fits as it is */
        static const struct { uint32_t d, bpm, at, fold; } C[5] = {   /* (the delay: its line 1 s, on either) */
            {6, 120, 14700, 0}, {11, 100, 26460, 1}, {10, 84, 23625, 1}, {11, 40, 33075, 2}, {11, 120, 44100, 0}};
        uint32_t c, f, l4;
        for (c = 0; c < 5u; c++) {
            SdPatch p = space_patch(SD_SPDIV(C[c].d), SD_SP_SYNC, c ? 0 : 50000, 65535, 0, 0);
            space_feed(&p, 0, 0, c ? C[c].at + 2000u : 4u * C[c].at + 2000u, a, C[c].bpm);
            at = first_peak(a, 0, 100, C[c].at + 2000u);
            sd_space_sync_q8(e1.tempo_q8, C[c].d, &f, SD_SP_LIM);
            CHECK(at >= C[c].at - 1u && at <= C[c].at + 2u && f == C[c].fold,
                  "synced case %u: the echo at frame %u (%u), %u halvings (%u)", c, at, C[c].at, f, C[c].fold);
            if (!c) {
                l4 = first_peak(a, 0, 3u * C[c].at + 2000u, 4u * C[c].at + 2000u);
                printf("space (%s): 1/4T at 120 BPM: the 1st echo at %u, the 4th at %u (14700, 58800); 1/2 at 100 BPM, "
                       "1/4D at 84 and 1/2 at 40 halved onto the beat\n", mode, at, l4);
                CHECK(l4 >= 58799u && l4 <= 58803u, "1/4T's 4th echo at frame %u, not 58800", l4);
            }
        }
    }
    {   /* ping-pong: the first echo left, the second right */
        SdPatch p = space_patch(SD_SPMS(100u), SD_SP_PP, 40000, 65535, 0, 0);   /* 100 ms */
        uint32_t l1, r1;
        space_feed(&p, 0, 0, SD_FS / 2u, a, 120);
        l1 = first_peak(a, 0, 100, 6000);
        r1 = first_peak(a, 1, 100, 10000);
        printf("space (%s): ping-pong, 100 ms: left at %u, right at %u\n", mode, l1, r1);
        CHECK(l1 >= 4409u && l1 <= 4412u && r1 >= 8819u && r1 <= 8823u, "ping-pong: left %u right %u", l1, r1);
    }
    {   /* FDBK at max, a loud sine for 3 s: it sings, but stays under its ceiling */
        SdPatch p = space_patch(20000, 0, 65535, 65535, 65535, 65535);
        int32_t peak = 0;
        space_feed(&p, 220, 30000, 3u * SD_FS, a, 120);
        for (i = 0; i < 3u * SD_FS; i++) if (abs(a[2u * i]) > peak) peak = abs(a[2u * i]);
        printf("space (%s): FDBK and DECAY at max on a loud sine: peak %d\n", mode, peak);
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
        SdPatch p = SD_PATCH[SD_P_VOYAGE];
        sd_engine_init(&e1); sd_engine_patch(&e1, &SD_PATCH[SD_P_VOYAGE]); sd_engine_note(&e1, 50, 100); run(&e1, a, SD_FS / 2u, 256);
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
    space_one = 0;
}
static void test_space(void) { test_space_mode(0); test_space_mode(1); }

/* TIME's looper (the delay's layer looped and pitched, into the reverb as the synth goes, the delay going on): a
 * 440 Hz sine 0 .. 1 s,
 * LOOP at 1 s with LENGTH 500 ms (2 s of memory here). It holds (the same level at 2 and at 9 s), at 440 Hz; +12 and
 * -12 at its SPEED play it an octave up and down; where it comes round and where it is let go, no step bigger than
 * the sine's own; the delay goes on while it loops (an impulse at 5 s echoes on TIME); it goes into the reverb as
 * the synth does (R-LEVEL up: more there); on one core the same, into the room */
static double tl_freq(const int32_t *x, uint32_t from, uint32_t to)   /* Hz, the left: its rising zero crossings,
                                                   between samples, first to last */
{
    double t0 = -1, t1 = -1;
    uint32_t i, n = 0;
    for (i = from + 1u; i < to; i++)
        if (x[2u * i - 2u] < 0 && x[2u * i] >= 0) {
            double t = i - 1 + (double)-x[2u * i - 2u] / (x[2u * i] - x[2u * i - 2u]);
            if (t0 < 0) t0 = t; else n++;
            t1 = t;
        }
    return n && t1 > t0 ? n * SD_FS / (t1 - t0) : 0;
}
static double tl_cross(const int32_t *x, uint32_t from, uint32_t to)   /* zero crossings a second, the left */
{
    uint32_t i, n = 0;
    for (i = from + 1u; i < to; i++) n += (x[2u * i - 2u] < 0) != (x[2u * i] < 0);
    return n * (double)SD_FS / (to - from);
}
static double tl_rms(const int32_t *x, uint32_t from, uint32_t to)
{
    double e = 0;
    uint32_t i;
    for (i = from; i < to; i++) e += (double)x[2u * i] * x[2u * i];
    return sqrt(e / (to - from));
}
static int32_t tl_step(const int32_t *x, uint32_t from, uint32_t to)
{
    uint32_t i;
    int32_t m = 0;
    for (i = from + 1u; i < to; i++) { int32_t d = abs(x[2u * i] - x[2u * i - 2u]); if (d > m) m = d; }
    return m;
}
static uint8_t tl_mem[24576];                   /* the looper's memory here, as the app's: half the delay line and
                                                   this, 5.1 s */
static double tl_rev_at = 99, tl_rev_off = 99;  /* REVERSE from .. to (s) */
static int tl_ramp;                             /* the sine rising from 0.5 to 1 s (0 .. 12000), else flat */
static double tl_t[4];                          /* (tl_nv of them) the speed set at tl_t[i] to tl_v[i]; GLIDE */
static int16_t tl_v[4];
static uint32_t tl_nv;
static uint16_t tl_glide;
static double tl_wob, tl_wob_at = 99, tl_wob_off = 99;   /* the matrix's free SPEED: a 2 Hz sine of tl_wob semitones */
static int32_t tl_tq;                           /* TURING's SPEED (semitones x 256) from tl_wob_at to tl_wob_off */
static void tl_feed(int32_t *outbuf, uint32_t frames, uint16_t rlvl, uint16_t dlvl, double loop_at, double loop_off,
                    double up_at, int16_t speed, double imp_at)
{
    static int32_t in[2u * SD_SUB];
    SdPatch p = space_patch(SD_SPMS(300u), 0, 0, dlvl, rlvl, 30000);
    uint32_t i, b;
    double ph = 0;
    memset(&tsm, 0, sizeof tsm);
    memset(tl_mem, 0, sizeof tl_mem);
    sd_engine_init(&e1);
    e1.sm = &tsm;
    e1.sp_room = space_one;
    sd_engine_looper_borrow(&e1, tl_mem, sizeof tl_mem);
    e1.lp_len = SD_SPMS(500u); e1.lp_lvl = 65535;
    e1.bpm = 120;
    sd_seq_retime(&e1);
    e1.patch = p;
    for (b = 0; b * SD_SUB < frames; b++) {
        double t = (double)b * SD_SUB / SD_FS;
        e1.lp_on = t >= loop_at && t < loop_off;
        e1.lp_rev = t >= tl_rev_at && t < tl_rev_off;
        e1.lp_speed = t >= up_at ? speed : 0;
        for (i = 0; i < tl_nv; i++) if (t >= tl_t[i]) e1.lp_speed = tl_v[i];
        e1.lp_glide = tl_glide;
        {
            int w = t >= tl_wob_at && t < tl_wob_off;
            e1.lp_mfree = w && tl_wob != 0 ? (int32_t)lround(tl_wob * 256 * sin(2 * M_PI * 2 * (t - tl_wob_at))) : 0;
            e1.lp_mon = (uint8_t)(w && tl_wob != 0);
            e1.lp_mtq = w ? tl_tq : 0;
        }
        sd_space_params(&e1);
        for (i = 0; i < SD_SUB; i++) {
            double tt = (double)(b * SD_SUB + i) / SD_FS, am = !tl_ramp ? 12000 : tt < 0.5 ? 0 : (tt - 0.5) * 24000;
            int32_t x = t < loop_at ? (int32_t)lround(am * sin(ph)) : 0;
            if (imp_at > 0 && b == (uint32_t)(imp_at * SD_FS / SD_SUB) && !i) x = 20000;
            ph += 2 * M_PI * 440 / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            outbuf[2u * (b * SD_SUB + i)] = outbuf[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_space_run(&e1, in, outbuf + 2u * b * SD_SUB, SD_SUB, 0);
    }
}
static void test_looper_mode(uint32_t one)
{
    static int32_t o[2u * (12u * SD_FS + SD_SUB)], o2[2u * (12u * SD_FS + SD_SUB)];
    uint32_t F = SD_FS, i, at;
    double l2, l9, c2, cu, cd, w0, w1;
    int32_t sw, sr, s0;
    const char *mode = one ? "one core" : "two cores";
    space_one = (uint8_t)one;
    /* held, at its pitch; then let go at 10 s (the delay off: the looper alone) */
    tl_feed(o, 12u * F, 0, 0, 1.0, 10.0, 99, 0, 0);
    l2 = tl_rms(o, 2u * F, 3u * F); l9 = tl_rms(o, 9u * F, 10u * F);
    c2 = tl_cross(o, 2u * F, 3u * F);
    s0 = tl_step(o, F / 2u, F);                 /* the sine playing: its own steps */
    sw = tl_step(o, 2u * F, 9u * F);            /* round and round (the wrap each 0.5 s) */
    sr = tl_step(o, 10u * F - F / 10u, 10u * F + F / 2u);   /* let go */
    printf("looper (%s): held %.2f dB from 2 to 9 s, at %.0f Hz (440); the largest step round %d, let go %d (the sine "
           "playing: %d)\n", mode, 20 * log10(l9 / l2), c2 / 2, sw, sr, s0);
    CHECK(fabs(20 * log10(l9 / l2)) < 0.5 && l2 > 1000, "the looper does not hold: %.1f then %.1f", l2, l9);
    CHECK(fabs(c2 / 2 - 440) < 5, "the looper plays at %.0f Hz, not 440", c2 / 2);
    CHECK(sw <= s0 * 3 / 2 && sr <= s0 * 3 / 2, "the looper clicks: round %d, let go %d (the sine's own %d)", sw, sr, s0);
    CHECK(tl_rms(o, 11u * F, 12u * F) < 1, "let go, the looper still plays");
    /* its speed: +12 and -12 from 3 s */
    tl_feed(o, 6u * F, 0, 0, 1.0, 99, 3.0, 3072, 0);
    cu = tl_cross(o, 4u * F, 6u * F) / 2;
    tl_feed(o, 6u * F, 0, 0, 1.0, 99, 3.0, -3072, 0);
    cd = tl_cross(o, 4u * F, 6u * F) / 2;
    printf("looper (%s): its speed +12 plays %.0f Hz (880), -12 %.0f Hz (220)\n", mode, cu, cd);
    CHECK(fabs(cu - 880) < 10 && fabs(cd - 220) < 4, "the looper's speed: %.0f and %.0f Hz", cu, cd);
    /* the delay goes on: an impulse at 5 s while it loops, its echo 300 ms on (o: with the impulse, o2 without) */
    tl_feed(o, 6u * F, 0, 65535, 1.0, 99, 99, 0, 5.0);
    tl_feed(o2, 6u * F, 0, 65535, 1.0, 99, 99, 0, 0);
    for (i = 0; i < 2u * 6u * F; i++) o[i] -= o2[i];
    at = first_peak(o, 0, 5u * F + 4000u, 5u * F + 20000u);
    printf("looper (%s): the delay goes on while it loops: an echo %u frames after the impulse (13230)\n", mode,
           at - (uint32_t)(5.0 * F / SD_SUB) * SD_SUB);
    CHECK(abs((int)(at - (uint32_t)(5.0 * F / SD_SUB) * SD_SUB) - 13230) <= 3, "the delay while looping: the echo at %u", at);
    /* into the reverb as the synth goes: the wet (R-LEVEL 76 against 0) from 2 to 3 s, the looper alone playing; and
       the synth's own way in kept: its tail at 1 .. 1.2 s the same with the looper's level at 0 */
    tl_feed(o2, 3u * F, 0, 0, 1.0, 99, 99, 0, 0);
    w0 = tl_rms(o2, 2u * F, 3u * F);
    tl_feed(o, 3u * F, 50000, 0, 1.0, 99, 99, 0, 0);
    for (i = 0; i < 2u * 3u * F; i++) o[i] -= o2[i];
    w1 = tl_rms(o, 2u * F, 3u * F);
    printf("looper (%s): into the %s as the synth: its wet %.1f dB under it (R-LEVEL 76)\n", mode, one ? "room" : "plate",
           20 * log10(w0 / w1));
    CHECK(w1 > w0 * 0.03, "the looper does not reach the reverb: its wet %.0f against %.0f", w1, w0);
    {   /* REVERSE: the sine rising through its 500 ms plays rising forward and falling backward (the 10 ms pieces'
           steps, mostly up or mostly down); round its start, and at the turn (3 s), no step bigger than its own */
        int up[2] = {0, 0}, dn[2] = {0, 0};
        uint32_t r, j;
        tl_ramp = 1;
        for (r = 0; r < 2u; r++) {
            double prev = -1;
            tl_rev_at = r ? 1.0 : 99;
            tl_feed(o, 4u * F, 0, 0, 1.0, 99, 99, 0, 0);
            for (j = 2u * F; j < 4u * F; j += F / 100u) {
                double v = tl_rms(o, j, j + F / 100u);
                if (prev >= 0) { if (v > prev * 1.01) up[r]++; else if (v < prev * 0.99) dn[r]++; }
                prev = v;
            }
        }
        tl_ramp = 0;
        tl_rev_at = 1.0;
        tl_feed(o, 4u * F, 0, 0, 1.0, 99, 99, 0, 0);
        sw = tl_step(o, 2u * F, 4u * F);
        tl_rev_at = 3.0;
        tl_feed(o, 4u * F, 0, 0, 1.0, 99, 99, 0, 0);
        sr = tl_step(o, 3u * F - F / 10u, 3u * F + F / 10u);
        tl_rev_at = 99;
        printf("looper (%s): REVERSE: forward %d pieces up, %d down; backward %d up, %d down; round its start %d, the "
               "turn %d (its own %d)\n", mode, up[0], dn[0], up[1], dn[1], sw, sr, s0);
        CHECK(up[0] > 3 * dn[0] && dn[1] > 3 * up[1], "REVERSE does not turn it round: %d/%d, %d/%d", up[0], dn[0], up[1], dn[1]);
        CHECK(sw <= s0 * 3 / 2 && sr <= s0 * 3 / 2, "REVERSE clicks: round %d, the turn %d (its own %d)", sw, sr, s0);
    }
    space_one = 0;
}
/* the looper's grid (an octave finds it again, over GLIDE or at once): an octave down at an odd time and back, gliding
 * (300 ms) or at once, and REVERSE there and back: settled at -12 the head is where its grid has it, and back at its
 * pitch the loop plays bit for bit as it would have never moved; no step bigger than the sine's own on the way */
static void test_looper_grid_mode(uint32_t one)
{
    static int32_t o[2u * (8u * SD_FS + SD_SUB)], r0[2u * (8u * SD_FS + SD_SUB)], o2g[2u * (4u * SD_FS + SD_SUB)];
    static const struct { uint16_t glide; int16_t to; double rev; const char *what; } C[4] = {
        {4915, -3072, 0, "gliding 300 ms"}, {0, -3072, 0, "at once"}, {14746, 3072, 0, "+12 gliding 900 ms"},
        {0, 0, 2.213, "REVERSE"}};
    uint32_t F = SD_FS, c, i, s0, sw;
    int32_t R, md;
    const char *mode = one ? "one core" : "two cores";
    space_one = (uint8_t)one;
    tl_feed(r0, 8u * F, 0, 0, 1.0, 99, 99, 0, 0);
    s0 = (uint32_t)tl_step(r0, 2u * F, 3u * F);
    for (c = 0; c < 4u; c++) {
        tl_nv = C[c].to ? 2u : 0u; tl_t[0] = 2.137; tl_t[1] = 4.311; tl_v[0] = C[c].to; tl_v[1] = 0;
        tl_glide = C[c].glide;
        if (C[c].rev > 0) { tl_rev_at = C[c].rev; tl_rev_off = 3.377; }
        tl_feed(o, (uint32_t)(3.9 * F), 0, 0, 1.0, 99, 99, 0, 0);   /* settled there */
        R = C[c].to ? sd_lp_near(sd_lp_grid(&e1.sp, (uint32_t)sd_lp_oct(e1.sp.pr), 0), e1.sp.pk,
                                 C[c].to < 0 ? e1.sp.pln / 2u : e1.sp.pln) : 0;
        CHECK(!e1.sp.pbl && R == 0 && (!C[c].to || e1.sp.pr == (C[c].to < 0 ? 32768 : 131072)),
              "the looper %s: at 3.9 s off its grid by %d (crossfading %d, speed %d)", C[c].what, R, e1.sp.pbl, e1.sp.pr);
        tl_feed(o, 8u * F, 0, 0, 1.0, 99, 99, 0, 0);
        for (i = 13u * F / 2u, md = 0; i < 8u * F; i++) { int32_t d = abs(o[2u * i] - r0[2u * i]); if (d > md) md = d; }
        sw = (uint32_t)tl_step(o, 2u * F, 13u * F / 2u);
        printf("looper (%s): %s there and back: on its grid there; from 6.5 s %s; the largest step %u (the sine's "
               "own %u)\n", mode, C[c].what, md ? "NOT as before" : "bit for bit as it would have been", sw, s0);
        CHECK(md == 0, "the looper %s: back, %d off the loop never moved", C[c].what, md);
        CHECK(sw <= 3u * s0 / 2u * (C[c].to > 0 ? 2u : 1u), "the looper %s clicks: %u (its own %u)", C[c].what, sw, s0);
        tl_rev_at = tl_rev_off = 99;
    }
    tl_nv = 0; tl_glide = 0;
    {   /* the matrix to SPEED (an LFO a slight wobble; TURING in semitones): a 2 Hz wobble of
           0.3 semitones from 2.1 to 4.37 s, the grid following it, then taken back; TURING's +7 at once from 2.1 s */
        double lo = 1e9, hi = 0, f7;
        uint32_t j;
        tl_wob = 0.3; tl_wob_at = 2.1; tl_wob_off = 4.37;
        tl_feed(o, 8u * F, 0, 0, 1.0, 99, 99, 0, 0);
        for (j = 0; j < 8u; j++) {              /* its pitch in 50 ms pieces, 2.2 .. 2.7 s (a cycle) */
            double c = tl_freq(o, 2u * F + F / 5u + j * F / 16u, 2u * F + F / 5u + j * F / 16u + F / 20u);
            lo = c < lo ? c : lo; hi = c > hi ? c : hi;
        }
        for (i = 13u * F / 2u, md = 0; i < 8u * F; i++) { int32_t d = abs(o[2u * i] - r0[2u * i]); if (d > md) md = d; }
        sw = (uint32_t)tl_step(o, 2u * F, 13u * F / 2u);
        tl_wob = 0; tl_tq = 7 * 256;
        tl_feed(o2g, 4u * F, 0, 0, 1.0, 99, 99, 0, 0);
        f7 = tl_cross(o2g, 3u * F, 4u * F) / 2;
        tl_tq = 0; tl_wob_at = tl_wob_off = 99;
        printf("looper (%s): the matrix's SPEED: a wobble of 0.3 semitones plays %.1f .. %.1f Hz (432.4 .. 447.7 at its ends), the "
               "grid with it; let go, from 6.5 s %s; the largest step %u; TURING's +7 plays %.0f Hz (659)\n", mode, lo, hi,
               md ? "NOT as before" : "bit for bit as it would have been", sw, f7);
        CHECK(lo > 430 && lo < 435 && hi > 445 && hi < 450, "the looper's wobble: %.1f .. %.1f Hz", lo, hi);
        CHECK(md == 0, "the looper after its wobble: %d off the loop never moved", md);
        CHECK(sw <= 3u * s0 / 2u, "the looper's wobble clicks: %u (its own %u)", sw, s0);
        CHECK(fabs(f7 - 659.3) < 5, "TURING's +7 on the looper plays %.0f Hz", f7);
    }
    space_one = 0;
}
/* the looper's memory, borrowed (armed while L-LEVEL is above 0, mono mu-law, ~5 s): armed, a free 800 ms echo plays
 * at 400 ms (the delay has its line's first half), let go at 800 again; arming
 * while echoes ring fades them out and back, no step bigger than the sine's own; a 440 Hz sine looped through the
 * mu-law bytes keeps its noise 30 dB under it; the memory 5.1 s */
static int32_t lb_out[2u * (4u * SD_FS + SD_SUB)];
static void lb_feed(uint32_t ms, double arm_at, double imp_at, double sine_amp, double loop_at, uint32_t frames)
{
    static int32_t in[2u * SD_SUB];
    SdPatch p = space_patch(SD_SPMS(ms), 0, sine_amp > 0 ? 30000 : 0, loop_at < 99 ? 0 : 65535, 0, 30000);   /* (the
                                                   loop's: the dry alone) */
    uint32_t i, b;
    double ph = 0;
    memset(&tsm, 0, sizeof tsm);
    memset(tl_mem, 0, sizeof tl_mem);
    sd_engine_init(&e1);
    e1.sm = &tsm;
    e1.sp_room = space_one;
    sd_engine_looper_borrow(&e1, tl_mem, sizeof tl_mem);
    e1.lp_len = SD_SPMS(500u);
    e1.bpm = 120;
    sd_seq_retime(&e1);
    e1.patch = p;
    for (b = 0; b * SD_SUB < frames; b++) {
        double t = (double)b * SD_SUB / SD_FS;
        e1.lp_lvl = t >= arm_at ? 65535 : 0;
        e1.lp_on = t >= loop_at;
        sd_space_params(&e1);
        for (i = 0; i < SD_SUB; i++) {
            int32_t x = sine_amp > 0 && t < loop_at ? (int32_t)lround(sine_amp * sin(ph)) : 0;
            if (imp_at >= 0 && b == (uint32_t)(imp_at * SD_FS / SD_SUB) && !i) x = 20000;
            ph += 2 * M_PI * 440 / SD_FS;
            in[2u * i] = in[2u * i + 1u] = x;
            lb_out[2u * (b * SD_SUB + i)] = lb_out[2u * (b * SD_SUB + i) + 1u] = 0;
        }
        sd_space_run(&e1, in, lb_out + 2u * b * SD_SUB, SD_SUB, 0);
    }
}
static void test_looper_borrow_mode(uint32_t one)
{
    uint32_t F = SD_FS, i0 = (uint32_t)(0.5 * F / SD_SUB) * SD_SUB, a_off, a_on, sw, s0, i;
    double re = 0, im = 0, e = 0, snr;
    const char *mode = one ? "one core" : "two cores";
    space_one = (uint8_t)one;
    lb_feed(800u, 99, 0.5, 0, 99, 2u * F);      /* not armed: 800 ms */
    a_off = first_peak(lb_out, 0, i0 + 4000u, 2u * F) - i0;
    lb_feed(800u, 0, 0.5, 0, 99, 2u * F);       /* armed from the start: 400 ms */
    a_on = first_peak(lb_out, 0, i0 + 4000u, 2u * F) - i0;
    lb_feed(300u, 1.0, -1, 12000, 99, 2u * F);  /* armed at 1 s, a sine in the delay's echoes */
    s0 = (uint32_t)tl_step(lb_out, F / 2u, 9u * F / 10u);
    sw = (uint32_t)tl_step(lb_out, 9u * F / 10u, 3u * F / 2u);
    lb_feed(300u, 0, -1, 12000, 1.0, 3u * F);   /* a sine looped (the dry alone), its noise */
    for (i = 2u * F; i < 3u * F; i++) {         /* the loop's strongest line, and what is not it */
        double w = 2 * M_PI * 440 * i / SD_FS;
        re += lb_out[2u * i] * cos(w); im += lb_out[2u * i] * sin(w); e += (double)lb_out[2u * i] * lb_out[2u * i];
    }
    {
        double tone = 2 * (re * re + im * im) / F;
        snr = 10 * log10((e - tone) / tone + 1e-12);
    }
    printf("looper (%s): its memory %.2f s (half the delay line and 24 KB); a free 800 ms echo at %u frames not armed "
           "(35280), %u armed (17640); armed over ringing echoes: the largest step %u (the sine's own %u); a looped "
           "sine's noise %.1f dB under it\n", mode, (sd_lp_total(&e1) - SD_LP_XF - 32u) / 22050.0, a_off, a_on, sw, s0, snr);
    CHECK(abs((int)a_off - 35280) <= 3 && abs((int)a_on - 17640) <= 3, "the delay, lent: %u and %u", a_off, a_on);
    CHECK(sw <= 3u * s0 / 2u, "arming the looper clicks: %u (its own %u)", sw, s0);
    CHECK(snr < -30, "the mu-law loop's noise %.1f dB under it", snr);
    CHECK(sd_lp_total(&e1) > 5u * 22050u, "the looper's memory %u samples", sd_lp_total(&e1));
    space_one = 0;
}
static void test_looper(void)
{
    test_looper_mode(0); test_looper_mode(1); test_looper_grid_mode(0); test_looper_grid_mode(1);
    test_looper_borrow_mode(0); test_looper_borrow_mode(1);
}

/* SPACE's plate (raw and long; a prototype's LFO stuttered and clicked): its T60 at 700 Hz follows DECAY's law,
 * whatever the SIZE; SIZE swept the whole way and back
 * while a chord rings (a tape) leaves no click in the wet; FREEZE holds the tail's level and keeps new notes out;
 * SHIMMER at its most on the longest DECAY still dies away */
static int32_t pl_out[2u * 32u * SD_FS], pl_in[2u * 32u * SD_FS];
static void plate_feed(SdPatch *p, uint32_t frames, uint32_t sweep, double frz_on, double frz_off)
{
    uint32_t b, i;
    memset(&tsm, 0, sizeof tsm);
    sd_engine_init(&e1); e1.sm = &tsm; e1.bpm = 120; sd_seq_retime(&e1);
    for (b = 0; b * SD_SUB < frames; b++) {
        double t = (double)b * SD_SUB / SD_FS;
        if (sweep) p->s_size = (uint16_t)(32767.5 + 32767.5 * sin(2 * M_PI * t / 4.0));   /* the whole way in 2 s */
        e1.sp_freeze = (uint8_t)(frz_on >= 0 && t >= frz_on && (frz_off < 0 || t < frz_off));
        e1.patch = *p;
        sd_space_params(&e1);
        for (i = 0; i < SD_SUB; i++) pl_out[2u * (b * SD_SUB + i)] = pl_out[2u * (b * SD_SUB + i) + 1u] = 0;
        sd_space_run(&e1, pl_in + 2u * b * SD_SUB, pl_out + 2u * b * SD_SUB, SD_SUB, 0);
        for (i = 0; i < 2u * SD_SUB; i++) pl_out[2u * b * SD_SUB + i] -= pl_in[2u * b * SD_SUB + i];   /* the wet */
    }
}
static double pl_db(uint32_t f0, uint32_t f1)  /* the wet's left, its RMS in dB, frames f0..f1 */
{
    double s = 0;
    uint32_t i;
    for (i = f0; i < f1; i++) s += (double)pl_out[2u * i] * pl_out[2u * i];
    return 10 * log10(s / (f1 - f0) + 1e-9);
}
static void test_plate(void)
{
    SdPatch p = SD_PATCH_SINE;
    uint32_t i, k, f, clicks = 0, seed = 7;
    double worst = 1, h0, h1, z0, z1, a, b2, top, sh3, sh30, rt[4];
    p.s_dlevel = 0; p.s_rlevel = 65535; p.s_rtone = 50000; p.s_mode = 0;
    /* T60 at 700 Hz (a burst of noise, 50 ms at -7 dBFS): DECAY 36000 and 48000, SIZE 0 and 65535: within 25 % */
    for (k = 0; k < 4u; k++) {
        static const uint16_t D[2] = {36000, 48000};
        double w = 2 * M_PI * 700 / SD_FS, al = sin(w) / 1.4, a0 = 1 + al, x1 = 0, x2 = 0, y1 = 0, y2 = 0, env, tp = -1e9, t5 = -1, t35 = -1, law;
        uint32_t n = 14u * SD_FS;
        p.s_decay = D[k & 1u]; p.s_size = k & 2u ? 65535 : 0;
        for (i = 0; i < n; i++) {
            int32_t x = 0;
            if (i < 2205u) { seed = seed * 1664525u + 1013904223u; x = ((int32_t)(seed >> 20) - 2048) * 12; }
            pl_in[2u * i] = pl_in[2u * i + 1u] = x;
        }
        plate_feed(&p, n, 0, -1, -1);
        for (i = 0; i < n; i++) {               /* (a bandpass round 700 Hz, in place) */
            double x = pl_out[2u * i], y = (al / a0) * x - (al / a0) * x2 - (-2 * cos(w) / a0) * y1 - ((1 - al) / a0) * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            pl_out[2u * i] = (int32_t)lrint(y);
        }
        for (f = 0; f + 2205u <= n; f += 2205u) {
            env = pl_db(f, f + 2205u);
            if (env > tp) tp = env;
            if (t5 < 0 && env < tp - 5) t5 = f / (double)SD_FS;
            if (t35 < 0 && env < tp - 35) { t35 = f / (double)SD_FS; break; }
        }
        law = SD_PL_T60[p.s_decay >> 9] / 1000.0;
        a = t35 > 0 ? 2 * (t35 - t5) / law : 0;
        rt[k] = a;
        if (fabs(a - 1) > fabs(worst - 1)) worst = a;
        CHECK(a > 0.75 && a < 1.25, "the plate's T60 (DECAY %u, SIZE %u): %.1f s, the law %.1f s", p.s_decay, p.s_size,
              2 * (t35 - t5), law);
    }
    /* clicks: a chord of three (A3 C#4 E4 at -12 dBFS, its own slow swell) while SIZE sweeps 0 .. 65535 and back */
    p.s_decay = 52000;
    for (i = 0; i < 12u * SD_FS; i++) {
        double t = (double)i / SD_FS, v = 8000 * (sin(2 * M_PI * 220 * t) + sin(2 * M_PI * 277.2 * t) + sin(2 * M_PI * 329.6 * t)) *
                   (0.6 + 0.4 * sin(2 * M_PI * 0.3 * t)) * (t < 8 ? 1 : 0);
        pl_in[2u * i] = pl_in[2u * i + 1u] = (int32_t)lrint(v);
    }
    plate_feed(&p, 12u * SD_FS, 1, -1, -1);
    for (i = 3u * SD_FS; i + 2u < 12u * SD_FS; i++) {   /* the second difference 8 x over its 10 ms round it */
        static double r2[441];
        double hh = pl_out[2u * (i + 2u)] - 2.0 * pl_out[2u * (i + 1u)] + pl_out[2u * i], loc = 0;
        uint32_t j;
        if (i % 441u == 0u || i == 3u * SD_FS) {
            for (j = 0; j < 441u && i + j + 2u < 12u * SD_FS; j++) {
                double q = pl_out[2u * (i + j + 2u)] - 2.0 * pl_out[2u * (i + j + 1u)] + pl_out[2u * (i + j)];
                r2[j] = q * q;
            }
        }
        for (j = 0; j < 441u; j++) loc += r2[j];
        loc = sqrt(loc / 441) + 1;
        if (fabs(hh) > 8 * loc && pl_db(i, i + 441u) > 30) { clicks++; i += 441u; }
    }
    CHECK(!clicks, "the plate clicks %u times while SIZE sweeps", clicks);
    /* FREEZE at 1 s on a second of noise: the tail's level 2..3 s and 9..10 s within 1.5 dB; a loud tone from 4 s
       stays out (the wet as without it); thawed at 11 s, it dies away */
    p.s_size = 49000; p.s_decay = 30000;
    for (i = 0; i < 16u * SD_FS; i++) {
        int32_t x = 0;
        if (i < SD_FS) { seed = seed * 1664525u + 1013904223u; x = ((int32_t)(seed >> 20) - 2048) * 6; }
        pl_in[2u * i] = pl_in[2u * i + 1u] = x;
    }
    plate_feed(&p, 16u * SD_FS, 0, 1.0, 11.0);
    z0 = pl_db(2u * SD_FS, 3u * SD_FS); z1 = pl_db(9u * SD_FS, 10u * SD_FS);
    h0 = pl_db(12u * SD_FS, 13u * SD_FS); h1 = pl_db(15u * SD_FS, 16u * SD_FS);
    for (i = 4u * SD_FS; i < 8u * SD_FS; i++) pl_in[2u * i] = pl_in[2u * i + 1u] = (int32_t)lrint(20000 * sin(2 * M_PI * 1000 * i / SD_FS));
    a = pl_db(5u * SD_FS, 8u * SD_FS);
    plate_feed(&p, 16u * SD_FS, 0, 1.0, 11.0);
    for (i = 4u * SD_FS; i < 8u * SD_FS; i++) pl_out[2u * i] += 0;   /* (the wet, the dry already taken off) */
    b2 = pl_db(5u * SD_FS, 8u * SD_FS);
    CHECK(fabs(z1 - z0) < 1.5 && fabs(b2 - a) < 1.0 && h1 < h0 - 10,
          "FREEZE: held %.1f -> %.1f dB, a tone in moved it %.1f dB, thawed %.1f -> %.1f dB", z0, z1, b2 - a, h0, h1);
    /* SHIMMER at its most on DECAY's most: 3 s of the chord, then 30 s: still there at 5 s, under its top at the end */
    p.s_size = 65535; p.s_decay = 65535; p.s_mode = 63u << 2;
    for (i = 0; i < 32u * SD_FS; i++) {
        double t = (double)i / SD_FS, v = t < 3 ? 6000 * (sin(2 * M_PI * 220 * t) + sin(2 * M_PI * 329.6 * t)) : 0;
        pl_in[2u * i] = pl_in[2u * i + 1u] = (int32_t)lrint(v);
    }
    plate_feed(&p, 32u * SD_FS, 0, -1, -1);
    top = -1e9;
    for (f = 0; f + 2205u <= 6u * SD_FS; f += 2205u) if (pl_db(f, f + 2205u) > top) top = pl_db(f, f + 2205u);
    sh3 = pl_db(5u * SD_FS, 6u * SD_FS); sh30 = pl_db(30u * SD_FS, 31u * SD_FS);
    CHECK(sh30 < top - 6, "SHIMMER at its most runs on: %.1f dB at 30 s, %.1f at its top", sh30, top);
    CHECK(sh3 > top - 20, "SHIMMER at its most cuts the tail short: %.1f dB at 5 s, %.1f at its top", sh3, top);
    printf("plate: T60 at 700 Hz x %.2f %.2f (SIZE 0, DECAY 36000 48000) x %.2f %.2f (SIZE 65535) of the law; SIZE swept the whole way: %u clicks; FREEZE held %.1f -> %.1f "
           "dB, a tone kept out (%.1f dB), thawed %.1f -> %.1f dB; SHIMMER at its most: %.0f dB under its top at 30 s "
           "(%.0f at 5 s)\n", rt[0], rt[1], rt[2], rt[3], clicks, z0, z1, b2 - a, h0, h1, top - sh30, top - sh3);
}

/* the plate's jobs (on the second core, the room the fallback, never on core 0): handed over and run later (as on
 * the second core: a block at a time, read a
 * block on), the sound is the one the engine makes alone, bit for bit; a block not back is dropped, never run here,
 * and the room takes over clean, as it does put in at a run's middle */
static uint32_t pj_pending, pj_have, pj_drop, pj_mis, pj_mis_n, pj_seed = 11, pj_runs, pj_drop_at = 0xFFFFFFFFu;
/* a second core that reads RAM wrong (on some FM-1s bit 21 now and then): one word the job reads, flipped, every job
   from 1.5 to 2 s. Its settings (a chunk's) as it read them, then right again (the RAM held them); its state written
   back from what it read, as the core would */
static void pj_misread(SdEngine *e, uint32_t b)
{
    SdSpace *s = &e->sp;
    SdPlateP *pp, keep;
    uint32_t r, k, c = s->qb0[b & 1u];
    double t = (double)b * SD_PL_BLK / (SD_FS / 2.0);
    pj_runs++;
    if (!pj_mis || t < 1.5 || t >= 2.0) { sd_plate_job(e, b); return; }
    pj_seed = pj_seed * 1664525u + 1013904223u;
    r = pj_seed >> 8;
    pp = &s->qp[(c + (r >> 8) % 4u) % SD_PL_CH];
    keep = *pp;
    k = r % 40u;
    if (k < 12u) pp->di[k] ^= 1u << 21;
    else if (k < 26u) pp->ti[k - 12u] ^= 1u << 21;
    else switch (k) {
        case 26: pp->g ^= 1 << 21; break;           case 27: pp->c ^= 1 << 21; break;
        case 28: pp->sg ^= 1 << 21; break;          case 29: pp->d2 ^= 1 << 21; break;
        case 30: pp->ma[r & 1u] ^= 1u << 21; break; case 31: pp->sc0 ^= 1 << 21; pp->G = 1; break;
        case 32: pp->snext ^= 1 << 21; pp->G = 1; break;
        case 33: s->pw[(r >> 9) % 13u] ^= 1u << 21; break;
        case 34: s->lpi ^= 1 << 21; break;          case 35: s->ig ^= 1 << 21; break;
        case 36: s->lp[r & 1u] ^= 1 << 21; break;   case 37: s->fbk[r & 1u] ^= 1 << 21; break;
        case 38: s->qb0[b & 1u] ^= 1u << 21; break; default: s->qfm[(r >> 9) & 511u] ^= 1 << 21; break;
    }
    pj_mis_n++;
    sd_plate_job(e, b);
    *pp = keep;
    if (k == 38) s->qb0[b & 1u] ^= 1u << 21;
}
static int pj_post(void *ev, uint32_t b)
{
    if (pj_drop && b > pj_drop_at) { ((SdEngine *)ev)->sp_room = 1; return 0; }   /* (off: not handed over) */
    pj_pending = b; pj_have = 1; return 1;
}
static int pj_join(void *ev)
{
    SdEngine *e = (SdEngine *)ev;
    if (!pj_have) return 0;
    pj_have = 0;
    if (pj_drop && pj_pending >= pj_drop_at) { e->sp_room = 1; return -1; }   /* (the second core gave up: the
                                                   block dropped, and the room, as the app does) */
    pj_misread(e, pj_pending);
    return 0;
}
static void pj_feed(SdEngine *e, int32_t *outbuf, uint32_t frames, uint32_t hooks, double room_at)
{
    static int32_t in[2u * SD_SUB];
    SdPatch p = SD_PATCH_SINE;
    uint32_t b, i, seed = 5;
    p.s_dlevel = 12000; p.s_rlevel = 40000; p.s_size = 45000; p.s_decay = 50000; p.s_rtone = 45000; p.s_time = 12000;
    memset(&tsm, 0, sizeof tsm);
    sd_engine_init(e); e->sm = &tsm; e->bpm = 120; sd_seq_retime(e);
    if (hooks) { e->pl_post = pj_post; e->pl_join = pj_join; }
    pj_have = 0;
    for (b = 0; b * SD_SUB < frames; b++) {
        double t = (double)b * SD_SUB / SD_FS;
        uint32_t m1 = 1u + (b * 5u) % 31u;
        if (t > 1.5 && t < 2.5) p.s_size = (uint16_t)(32767.5 + 32767.5 * sin(t * 4));
        if (room_at >= 0) e->sp_room = t >= room_at;
        e->patch = p;
        sd_space_params(e);
        for (i = 0; i < SD_SUB; i++) {
            uint32_t f = b * SD_SUB + i;
            double v = t < 1 || (t > 3 && t < 3.3) ? 8000 * (sin(2 * M_PI * 220 * f / SD_FS) + sin(2 * M_PI * 277 * f / SD_FS)) : 0;
            seed = seed * 1664525u + 1013904223u;
            if (t > 1.2 && t < 1.25) v += ((int32_t)(seed >> 20) - 2048) * 6;
            in[2u * i] = (int32_t)v; in[2u * i + 1u] = (int32_t)(v * 0.8);
            outbuf[2u * f] = outbuf[2u * f + 1u] = 0;
        }
        sd_space_run(e, in, outbuf + 2u * b * SD_SUB, m1, 0);   /* (a call split in two, as the render may) */
        sd_space_run(e, in + 2u * m1, outbuf + 2u * (b * SD_SUB + m1), SD_SUB - m1, m1);
    }
}
static uint32_t pj_drop_at_frames(double t) { return (uint32_t)(t * SD_FS / 2.0) / SD_PL_BLK * SD_PL_BLK * 2u; }
static void test_plate_jobs(void)
{
    static SdEngine ej;
    static int32_t o1[2u * (5u * SD_FS + SD_SUB)], o2[2u * (5u * SD_FS + SD_SUB)];   /* (pj_feed: whole sub-blocks) */
    uint32_t n = 5u * SD_FS, i;
    int32_t pk0 = 0, pk1 = 0;
    pj_feed(&ej, o1, n, 0, -1);
    pj_drop = 0;
    pj_feed(&ej, o2, n, 1, -1);
    CHECK(!memcmp(o1, o2, sizeof o1), "the plate's jobs run later differ from the engine's own");
    {   /* the second core gives up at 2.5 s (a block not back): never the plate here, the block's
           tail fades out, the room from the next tick; until then the same bit for bit */
        uint32_t f0, f1, j, runs_at = 0;
        int32_t step = 0, pkr = 0;
        double er = 0;
        pj_drop = 1; pj_drop_at = (uint32_t)(2.5 * SD_FS / 2.0) / SD_PL_BLK; pj_runs = 0;
        pj_feed(&ej, o2, n, 1, -1);
        runs_at = pj_runs;
        pj_drop = 0; pj_drop_at = 0xFFFFFFFFu;
        f0 = pj_drop_at_frames(2.5);
        CHECK(!memcmp(o1, o2, sizeof(int32_t) * 2u * (f0 - 2u * SD_PL_BLK * 2u)), "before the second core gave up: not the same");
        CHECK(ej.sp.room && runs_at <= (uint32_t)(2.5 * SD_FS / 2.0) / SD_PL_BLK + 1u,
              "the plate ran after the second core gave up (%u jobs)", runs_at);
        for (j = f0 - 4u * SD_PL_BLK; j < f0 + 8u * SD_PL_BLK; j++) {   /* the wet's step, sample to sample, there */
            int32_t d = abs(o2[2u * (j + 1u)] - o2[2u * j]);
            if (d > step) step = d;
        }
        f1 = (uint32_t)(3.4 * SD_FS);
        for (j = f1; j < f1 + SD_FS / 5u; j++) { er += (double)o2[2u * j] * o2[2u * j]; if (abs(o2[2u * j]) > pkr) pkr = abs(o2[2u * j]); }
        printf("plate jobs: the second core gave up at 2.5 s: %u jobs before, none after; the largest step there %d; "
               "the room after (peak %d)\n", runs_at, step, pkr);
        CHECK(step < 6000 && er > 1e6, "the hand-over to the room: a step of %d, the room %.3g", step, er);
    }
    pj_feed(&ej, o2, n, 0, 2.8);                /* the room from 2.8 s: clean, and it rings */
    for (i = 2u * SD_FS; i < (uint32_t)(2.8 * SD_FS); i++) if (abs(o2[2u * i]) > pk0) pk0 = abs(o2[2u * i]);
    for (i = (uint32_t)(2.8 * SD_FS); i < n; i++) if (abs(o2[2u * i]) > pk1) pk1 = abs(o2[2u * i]);
    CHECK(ej.sp.room && pk1 < 2 * (pk0 > 20000 ? pk0 : 20000), "the room put in mid-run: peak %d after, %d before", pk1, pk0);
    {
        double t = 0;
        for (i = (uint32_t)(3.4 * SD_FS); i < (uint32_t)(3.6 * SD_FS); i++) t += (double)o2[2u * i] * o2[2u * i];
        CHECK(t > 1e6, "the room put in mid-run is silent");
    }
    printf("plate jobs: run later, the same sound bit for bit; the room put in mid-run takes over clean (peak %d, %d "
           "before)\n", pk1, pk0);
    {   /* misread (a word a job, 1.5 .. 2 s; under ASan in the second run: nothing out of its lines): what it reads is
           kept in range, so the tail stays bounded and dies away as the clean one does */
        double c1 = 0, c2 = 0, m1 = 0, m2 = 0;
        int32_t pk = 0;
        pj_drop = 0; pj_mis = 1; pj_mis_n = 0;
        pj_feed(&ej, o2, n, 1, -1);
        pj_mis = 0;
        for (i = 0; i < 2u * n; i++) if (abs(o2[i]) > pk) pk = abs(o2[i]);
        for (i = (uint32_t)(4.0 * SD_FS); i < (uint32_t)(4.4 * SD_FS); i++) c1 += (double)o1[2u * i] * o1[2u * i], m1 += (double)o2[2u * i] * o2[2u * i];
        for (i = (uint32_t)(4.6 * SD_FS); i < n; i++) c2 += (double)o1[2u * i] * o1[2u * i], m2 += (double)o2[2u * i] * o2[2u * i];
        printf("plate jobs: %u words read wrong (bit 21) in 0.5 s: the peak %d, the tail at 4 s %+.1f dB and 4.6 s %+.1f dB "
               "against the clean one's\n", pj_mis_n, pk, 10 * log10((m1 + 1) / (c1 + 1)), 10 * log10((m2 + 1) / (c2 + 1)));
        CHECK(pj_mis_n > 50 && pk <= 4 * 32767, "misreads: %u, the peak %d", pj_mis_n, pk);
        CHECK(m2 < m1 && m2 < 4 * c2 + 1e6, "misreads: the tail does not die away (%.3g then %.3g; clean %.3g)", m1, m2, c2);
    }
}

/* ---- R4: MIX fills, the lows keep, HARM */
static void test_reactor_r4(void)
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
        SdPatch p = reactor_patch(SD_RM_BP12, 20000, 4000);
        p.r_mix = 1;                            /* (MIX 0 is no Reactor: the dry by 1 LSB of MIX) */
        reactor_feed(&p, 1000, n, a);
        e_dry = rms_db(a, n / 2u, n);
        p.r_mix = 32768;
        reactor_feed(&p, 1000, n, a);
        e_mid = rms_db(a, n / 2u, n);
        printf("reactor R4: MIX at the middle passes the dry at %+.1f dB (the old law: -6)\n", e_mid - e_dry);
        CHECK(e_mid - e_dry > -5 && e_mid - e_dry < -1, "MIX's middle passes the dry at %+.1f dB", e_mid - e_dry);   /* -3,
                                                   and a little of the wet against it */
    }
    {   /* the low keep: a 60 Hz sine, the filters at 2 kHz, MIX full: with r_low it stays, without it goes */
        SdPatch p = reactor_patch(SD_RM_BP12, 20000, 2000);
        reactor_feed(&p, 60, n, a);
        e_nokeep = rms_db(a, n / 2u, n);
        p.r_low = 40000;                        /* ~264 Hz */
        reactor_feed(&p, 60, n, a);
        e_keep = rms_db(a, n / 2u, n);
        p.r_mix = 1;
        reactor_feed(&p, 60, n, a);
        e_dry = rms_db(a, n / 2u, n);
        printf("reactor R4: 60 Hz at MIX full: %+.1f dB without the low keep, %+.1f dB with it\n", e_nokeep - e_dry, e_keep - e_dry);
        CHECK(e_keep - e_dry > -2, "the low keep loses 60 Hz: %+.1f dB", e_keep - e_dry);
        CHECK(e_nokeep - e_dry < -15, "without the low keep 60 Hz still passes %+.1f dB", e_nokeep - e_dry);
    }
    {   /* HARM: every filter lands on a harmonic of the note (none under it) */
        SdPatch p = reactor_patch(SD_RM_BP12, 20000, 1000);
        uint32_t f;
        p.r_track = SD_RT_HARM;
        for (f = 0; f < 4u; f++) p.stage[0].pitch[f] = (uint16_t)midi256(150.0 + 700.0 * f);   /* 150 .. 2250 Hz */
        p.r_slew = 0;
        sd_engine_init(&e1);
        e1.patch = p;
        e1.r.note = 50;                         /* D3, 146.8 Hz */
        sd_reactor_params(&e1);
        ok = 1;
        for (f = 0; f < 4u; f++) {
            int32_t sv = (e1.r.pitch[f] >> 8) - 50 * 256, hit = 0;
            for (h = 0; h < 32u; h++) hit |= sv == (int32_t)SD_HARM_ST[h];
            ok &= (uint32_t)hit;
        }
        printf("reactor R4 HARM: 150 850 1550 2250 Hz over D3 -> harmonics %.2f %.2f %.2f %.2f\n",
               pow(2, ((e1.r.pitch[0] >> 8) - 50 * 256) / 3072.0), pow(2, ((e1.r.pitch[1] >> 8) - 50 * 256) / 3072.0),
               pow(2, ((e1.r.pitch[2] >> 8) - 50 * 256) / 3072.0), pow(2, ((e1.r.pitch[3] >> 8) - 50 * 256) / 3072.0));
        CHECK(ok, "HARM leaves a filter off the harmonics");
    }
}

static void test_reactor_bypass(void)             /* MIX 0: the Reactor is not there, bit for bit */
{
    SdPatch p = SD_PATCH[SD_P_VOYAGE];
    uint32_t i;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &SD_PATCH[SD_P_VOYAGE]);
    sd_engine_note(&e1, 50, 100);
    run(&e1, a, SD_FS / 2u, 256);
    p.stage[0] = SD_PATCH[SD_P_VOWEL].stage[0];
    p.r_model = SD_RM_BP12; p.r_res = 60000; p.r_drive = 65535; p.r_mix = 0;
    sd_engine_init(&e2);
    sd_engine_patch(&e2, &p);
    sd_engine_note(&e2, 50, 100);
    run(&e2, b, SD_FS / 2u, 256);
    for (i = 0; i < SD_FS; i++) if (a[i] != b[i]) break;
    CHECK(i == SD_FS, "MIX 0 changed the sound at frame %u", i / 2u);
}

static void test_reactor_pos(void)                /* between stages in log pitch; SLEW 0 jumps, SLEW glides */
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

static void test_reactor_clock(void)              /* the stage clock: modes, range */
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
        for (k = 0; k < 9u; k++) { sd_reactor_clock(&e1); got[k] = (char)('1' + e1.r.stage); }
        got[9] = 0;
        CHECK(!strcmp(got, C[c].want), "reactor clock mode %u range %u..%u: %s, not %s", C[c].mode, C[c].lo + 1, C[c].hi + 1,
              got, C[c].want);
    }
    sd_engine_init(&e1);
    e1.patch.r_div = 1; e1.patch.r_cmode = SD_RC_RND; e1.patch.r_min = 1; e1.patch.r_max = 3;
    for (k = 0; k < 200u; k++) { sd_reactor_clock(&e1); CHECK(e1.r.stage >= 1 && e1.r.stage <= 3, "RND left its range"); }
}

static void test_reactor_extremes(void)           /* every model, RES / DRIVE / levels at max, the range's ends */
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
            CHECK(peak <= 4 * 32768 + 64, "reactor model %u at %u x 256, all at max: peak %d", m, P[q] / 256u, peak);
            CHECK(st < (1 << 30), "reactor model %u at %u x 256: an integrator at %d", m, P[q] / 256u, st);
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
/* The crossovers as destinations: D SPLIT1, D SPLIT2 and D SPLIT (the upper) each move their own; one
 * moved past another goes through it (sorted each tick). Then the wheel sweeps the lower one up through the middle
 * one, every band driven: no step at the crossing larger than the sound's own about it (the sweep's largest are
 * elsewhere: where the lower crossover passes the note itself) */
static const char *const DST_NAMES_T[3] = {"D SPLIT1", "D SPLIT2", "D SPLIT"};
static void test_split_through(void)
{
    static const struct { uint8_t d; int16_t amt; double hz[3]; } C[] = {
        {SD_MD_DSPLIT1, 32767, {800, 150.0 * 16, 4000}},    /* +48: the lower one up through the middle one */
        {SD_MD_DSPLIT2, 16384, {150, 800.0 * 4, 4000}},     /* +24: the middle one, in place */
        {SD_MD_DSPLIT, -32767, {150, 4000.0 / 16, 800}},    /* -48: the upper one down through the middle one */
    };
    SdPatch p = distroy_patch(SD_DS_SOFT, 30000, 16384, 0);
    uint32_t c, k, t, i;
    int32_t worst = 0;
    for (c = 0; c < 3u; c++) {
        SdPatch q = p;
        q.mod[0] = q.mod[1] = (SdModSlot){SD_MS_WHEEL, C[c].d, C[c].amt};
        mod_init(&e1, &q);
        e1.wheel = 127;
        mod_run(&e1, &q, 2);
        for (k = 0; k < 3u; k++) {
            int32_t want = (int32_t)midi256(C[c].hz[k]), got = e1.patch.d_split[k];
            if (abs(got - want) > worst) worst = abs(got - want);
            CHECK(abs(got - want) <= 8, "%s: crossover %u at %.1f Hz, not %.1f", DST_NAMES_T[c], k + 1u,
                  440.0 * pow(2.0, (got / 256.0 - 69.0) / 12.0), C[c].hz[k]);
        }
        CHECK(e1.patch.d_split[0] <= e1.patch.d_split[1] && e1.patch.d_split[1] <= e1.patch.d_split[2], "unsorted");
    }
    printf("split: D SPLIT1 +48 through the middle one, D SPLIT2 +24, D SPLIT -48 through it: each where it should be "
           "(within %d of 256 a semitone)\n", worst);
    for (c = 0; c < 4u; c++) {                  /* each band's GAIN (D HIGH the top's): its band alone, by 2 x the slot */
        static const uint8_t G[4] = {SD_MD_DGAIN1, SD_MD_DGAIN2, SD_MD_DGAIN3, SD_MD_DHIGH};
        SdPatch q = p;
        q.mod[0] = (SdModSlot){SD_MS_WHEEL, G[c], 8192};
        mod_init(&e1, &q);
        e1.wheel = 127;
        mod_run(&e1, &q, 2);
        for (k = 0; k < 4u; k++) {
            int32_t want = k == c ? 30000 + 2 * ((8192 * 127 * 258) >> 15) : 30000;
            CHECK(abs((int32_t)e1.patch.d_gain[k] - want) <= 2, "GAIN %u by slot %u: band %u at %u, not %d", c + 1u,
                  c + 1u, k + 1u, e1.patch.d_gain[k], want);
        }
    }
    printf("gain: D GAIN1..3 and D HIGH (GAIN 4) each move their own band's GAIN, the others' kept\n");
    {   /* the sweep: the wheel 0 -> 127 over 2 s (SPLIT1 150 Hz -> 2.4 kHz, through 800 Hz), a 300 Hz note */
        static int32_t o[2u * SD_SUB];
        double big[2] = {0, 0}, prof[16] = {0};
        int32_t prev = 0;
        uint32_t ticks = 2u * SD_FS / SD_SUB;
        for (k = 0; k < 2u; k++) {
            SdPatch q = p;
            q.mod[0] = q.mod[1] = (SdModSlot){SD_MS_WHEEL, SD_MD_DSPLIT1, 32767};
            mod_init(&e1, &q);
            e1.wheel = k ? 0 : 77;              /* (still: at the crossing; swept: from 0) */
            sd_engine_note(&e1, 62, 100);
            mod_run(&e1, &q, 400);
            for (t = 0; t < ticks; t++) {
                if (k) e1.wheel = (uint8_t)(t * 128u / ticks);
                memset(o, 0, sizeof o);
                sd_engine_render(&e1, o, SD_SUB);
                for (i = 0; i < SD_SUB; i++) {
                    double st = fabs((double)o[2u * i] - prev);
                    if ((t || i) && st > big[k]) big[k] = st;
                    if (k && (t || i) && st > prof[e1.wheel / 8u]) prof[e1.wheel / 8u] = st;
                    prev = o[2u * i];
                }
            }
        }
        {   /* at the crossing (the wheel 64..95: 800 Hz at 77) against either side of it and held there */
            double at = prof[8] > prof[9] ? prof[8] : prof[9], at2 = prof[10] > prof[11] ? prof[10] : prof[11];
            double side = prof[6] > prof[7] ? prof[6] : prof[7], side2 = prof[12] > prof[13] ? prof[12] : prof[13];
            double ref = side > side2 ? side : side2;
            at = at > at2 ? at : at2;
            ref = ref > big[0] ? ref : big[0];
            printf("split: the lower crossover swept up through the middle one (150 Hz -> 2.4 kHz, every band driven, a "
                   "D4): the largest step at the crossing %.0f, either side %.0f, held there %.0f\n", at,
                   side > side2 ? side : side2, big[0]);
            CHECK(at < 1.25 * ref, "a step as one crossover goes through another: %.0f (%.0f about it)", at, ref);
        }
    }
}

static void test_mod(void)
{
    SdPatch p = T_GLASS;
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
    {   /* the looper's SPEED: LFO1 at +3 % free (+-0.36 semitones: the wobble, a slot sending all the
           while); TURING at +50 % in whole semitones (+-6) */
        int32_t mlo = 1 << 30, mhi = -(1 << 30), qn = 0, qbad = 0, qlo = 1 << 30, qhi = -(1 << 30);
        uint32_t mon = 1;
        p = T_GLASS;
        p.lfo_rate[0] = 34000;
        p.mod[0] = (SdModSlot){SD_MS_LFO1, SD_MD_LSPEED, 983};
        p.mod[1] = (SdModSlot){SD_MS_TUR, SD_MD_LSPEED, 16384};
        p.tur_set = 0x80u | 50u; p.tur_ld = 8u;
        mod_init(&e1, &p);
        e1.bpm = 120; sd_seq_retime(&e1);
        e1.t[0].seq_on = 1;
        sd_engine_note(&e1, 60, 100);
        for (t = 0; t < 6000u; t++) {
            mod_run(&e1, &p, 1);
            if (e1.lp_mfree < mlo) mlo = e1.lp_mfree;
            if (e1.lp_mfree > mhi) mhi = e1.lp_mfree;
            mon &= e1.lp_mon;
            qbad |= e1.lp_mtq & 255;
            if (e1.lp_mtq < qlo) qlo = e1.lp_mtq;
            if (e1.lp_mtq > qhi) qhi = e1.lp_mtq;
            qn += e1.lp_mtq != 0;
        }
        printf("mod: LFO1 -> L SPEED +3 %%: %.2f .. %.2f semitones, free; TURING -> L SPEED +50 %%: %d .. %d, whole "
               "semitones%s\n", mlo / 256.0, mhi / 256.0, qlo / 256, qhi / 256, qbad ? " NOT" : "");
        CHECK(mlo < -80 && mlo > -100 && mhi > 80 && mhi < 100 && mon, "LFO1 -> L SPEED: %d .. %d (x 256)", mlo, mhi);
        CHECK(!qbad && qn > 0 && qlo >= -6 * 256 && qhi <= 6 * 256, "TURING -> L SPEED: %d .. %d, not whole semitones", qlo, qhi);
    }
    /* SYNC: 4 sixteenths (a beat) at 120 BPM is 0.5 s: 689 ticks a cycle */
    p = T_GLASS;
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
    p = T_GLASS;
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
        p = SD_PATCH[SD_P_DISTROY];                        /* DISTROY: DISTROY and SPACE on */
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
    p = T_GLASS;
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
            p = SD_PATCH[SD_P_DISTROY];
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
 * the loop's one; the segment editing */
static uint32_t seq_trace(SdEngine *e, const SdPattern *p, uint32_t steps, uint8_t *out, uint32_t *starts)
{   /* the steps as they start, and the frame each starts at */
    static int32_t o[2u * SD_SUB];
    uint32_t n = 0, last = 0xFFFFFFFFu, f = 0;
    sd_engine_init(e);
    sd_engine_patch(e, &T_GLASS);
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
    /* the segment editing: a cut, a join, EVEN, a length, a new pattern length */
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
            sd_engine_patch(&e1, &T_GLASS);
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
        sd_engine_patch(&e1, &T_GLASS);
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
    printf("seq: segments, orders, skips, DIV, SWING, TIE, SLIDE, RATCHET, PROB, the editing\n");
}

/* the filter, the chords, track B, the arpeggiator */
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
    for (f = 0; f < ms * 441u / 10u; f += SD_SUB) { memset(o, 0, sizeof o); sd_engine_render(e, o, SD_SUB); }   /* (zeroed
                                                   each time, as the platform hands it: the limiter's line keeps it) */
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
    sd_engine_init(&e1); sd_engine_patch(&e1, &T_GLASS);
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
        sd_engine_init(&e1); sd_engine_patch(&e1, &T_GLASS);
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
        sd_engine_init(&e1); sd_engine_patch(&e1, &T_GLASS);
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

/* EDIT's OFF (fx_off) is the engine as if its level were 0, bit for bit; a track OFF is silent and keeps
   time; B in MONO is silent; SCL's transpose waits for the track's one */
static void render_patch(const SdPatch *p, int32_t *o, uint32_t frames)
{
    sd_engine_init(&e1); sd_engine_patch(&e1, p);
    run_ms(&e1, 2); sd_engine_note(&e1, 45, 110); run_ms(&e1, 200);
    memset(o, 0, frames * 2u * sizeof *o); sd_engine_render(&e1, o, frames);
}
/* The hand's held keys (in MONO, hold a key, play a second, let it go: back to the first): a key let go gives its
 * voice back to the latest key still held without one, legato (no new note-on:
 * the voice's age stays). Keys let go out of order, three keys, and DUO's stolen voice too. */
static void hand_keys(SdEngine *e, uint32_t mode)
{
    sd_engine_init(e); sd_engine_patch(e, &T_GLASS);
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

/* The edge (a plain FM patch sounds as it should, with an edge that makes it Star Distroyer's).
 * GROWL and AURORA, an A3 in MONO and a C major chord in POLY, each with the engine the edge gave it, DISTROY, on and
 * off (fx_off): the body (30 Hz .. 1.2 kHz) within 2 dB, the top (over 5 kHz) up by less than 12 dB, no sample at the
 * ceiling */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } EdgeBq;
static EdgeBq edge_bq(double hz, int high)       /* RBJ's Butterworth biquad, Q 0.707 */
{
    double w = 2.0 * 3.14159265358979 * hz / SD_FS, c = cos(w), al = sin(w) / (2.0 * 0.70710678), a0 = 1.0 + al;
    EdgeBq q = {0};
    q.b0 = (high ? (1.0 + c) : (1.0 - c)) / 2.0 / a0;
    q.b1 = (high ? -(1.0 + c) : (1.0 - c)) / a0;
    q.b2 = q.b0;
    q.a1 = -2.0 * c / a0;
    q.a2 = (1.0 - al) / a0;
    return q;
}
static double edge_db(const int32_t *x, uint32_t n, double lo, double hi)   /* the mid's band, dB (two of each) */
{
    EdgeBq f[4];
    uint32_t i, k, m = 0;
    double e = 0.0;
    f[0] = f[1] = edge_bq(lo, 1);
    f[2] = f[3] = edge_bq(hi, 0);
    for (i = 0; i < n; i++) {
        double v = 0.5 * ((double)x[2u * i] + (double)x[2u * i + 1u]);
        for (k = 0; k < 4u; k++) {
            double y = f[k].b0 * v + f[k].z1;
            f[k].z1 = f[k].b1 * v - f[k].a1 * y + f[k].z2;
            f[k].z2 = f[k].b2 * v - f[k].a2 * y;
            v = y;
        }
        if (i >= 2048u) { e += v * v; m++; }
    }
    return 10.0 * log10(e / (double)m + 1e-9);
}
static void edge_play(const SdPatch *p, uint32_t mode, int32_t *out, uint32_t frames)
{
    static const uint8_t CHORD[4] = {48, 52, 55, 60};
    uint32_t k;
    mod_init(&e1, p);
    e1.mode = (uint8_t)mode;
    mod_run(&e1, p, 4);
    if (mode == SD_MODE_POLY) for (k = 0; k < 4u; k++) sd_engine_note(&e1, CHORD[k], 100);
    else sd_engine_note(&e1, 57, 100);
    run(&e1, out, frames, 256);
}
static void test_edge(void)
{
    static const uint8_t EDGE[2] = {SD_P_GROWL, SD_P_AURORA};
    const uint32_t frames = SD_FS * 3u / 2u;
    uint32_t i, m, k;
    double worst_body = 0.0, worst_top = -99.0;
    for (i = 0; i < 2u; i++)
        for (m = 0; m < 2u; m++) {
            SdPatch on = SD_PATCH[EDGE[i]], off = on;
            uint32_t mode = m ? SD_MODE_POLY : SD_MODE_MONO;
            int32_t peak = 0;
            double body, top;
            off.fx_off = SD_FX_DISTROY;
            edge_play(&on, mode, a, frames);
            edge_play(&off, mode, b, frames);
            for (k = 0; k < 2u * frames; k++) if ((a[k] < 0 ? -a[k] : a[k]) > peak) peak = a[k] < 0 ? -a[k] : a[k];
            body = edge_db(a, frames, 30.0, 1200.0) - edge_db(b, frames, 30.0, 1200.0);
            top = edge_db(a, frames, 5000.0, 20000.0) - edge_db(b, frames, 5000.0, 20000.0);
            if (fabs(body) > fabs(worst_body)) worst_body = body;
            if (top > worst_top) worst_top = top;
            CHECK(fabs(body) < 2.0, "%s %s: the body moved %+.1f dB", SD_PATCH_NAME[EDGE[i]], m ? "POLY" : "MONO", body);
            CHECK(top < 12.0, "%s %s: the top rose %+.1f dB", SD_PATCH_NAME[EDGE[i]], m ? "POLY" : "MONO", top);
            CHECK(peak < 32767, "%s %s: a sample at the ceiling (%d)", SD_PATCH_NAME[EDGE[i]], m ? "POLY" : "MONO", peak);
        }
    printf("edge: GROWL and AURORA keep their body (worst %+.1f dB), the top up at most %+.1f dB\n",
           worst_body, worst_top);
}

/* The code names a patch by SD_P_* (sd_patches.h): each the patch of its name */
static void test_names(void)
{
#define PN(x) {SD_P_##x, #x}
    static const struct { uint32_t i; const char *name; } P[] = {
        PN(GROWL), PN(VOYAGE), PN(DISTROY), PN(VOWEL), PN(CHORDS), PN(PLUCKS), PN(FLANGE), PN(PHASER), PN(FUSION),
        PN(SPLIT), PN(COMET), PN(NEBULA), PN(PULSAR), PN(ION), PN(ORBITAL), PN(MONOLITH), PN(STRINGS), PN(HALO),
        PN(AURORA), PN(TITAN), PN(DRIVE), PN(UNDERTOW), PN(IMPACT), PN(BRAAM), PN(SCREAM), PN(BEACON), PN(DEPTH),
        PN(SHARDS), PN(PRISM)};
#undef PN
    uint32_t k;
    CHECK(SD_P_COUNT == SD_PATCHES && sizeof P / sizeof P[0] == SD_PATCHES, "SD_P_* has %u names, the bank %u patches",
          (unsigned)SD_P_COUNT, SD_PATCHES);
    for (k = 0; k < sizeof P / sizeof P[0]; k++)
        CHECK(P[k].i < SD_PATCHES && !strcmp(SD_PATCH_NAME[P[k].i], P[k].name), "SD_P_%s is patch %u, %s", P[k].name,
              P[k].i, P[k].i < SD_PATCHES ? SD_PATCH_NAME[P[k].i] : "none");
}

static void test_switches(void)
{
    static int32_t a[2u * 2048u], b[2u * 2048u];
    static const uint8_t BITS[4] = {SD_FX_REACTOR, SD_FX_DISTROY, SD_FX_SPACE, SD_FX_FILTER};
    SdPatch p, q;
    SdPattern pa, pb;
    uint8_t nt[8];
    uint32_t i, k, n;
    for (k = 0; k < 4u; k++) {
        for (i = 0; i < SD_PATCHES; i++) {      /* a stock patch with that engine in */
            const SdPatch *s = &SD_PATCH[i];
            if ((k == 0u && s->r_mix) || (k == 1u && s->d_mix) || (k == 2u && (s->s_dlevel || s->s_rlevel)) || (k == 3u && s->f_type)) break;
        }
        if (i == SD_PATCHES) { p = T_GLASS; p.f_type = SD_FT_LP12; p.f_cut = 30000; p.e3_s = 127; }   /* (no stock filter) */
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
    sd_engine_init(&e1); sd_engine_patch(&e1, &T_GLASS);
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
        sd_engine_init(&e1); sd_engine_patch(&e1, &T_GLASS);
        e1.mode = SD_MODE_POLY; run_ms(&e1, 5);
        sd_engine_pattern_to(&e1, 0, &pa); e1.bpm = 120; e1.seq_run = 1;
        e1.arp_on = 1; e1.arp_gate = 20; e1.arp_div = 0;   /* ARP on 60 too, short gates, 1/32 */
        sd_engine_note(&e1, 60, 100);
        for (sub = 0; sub < 2000u; sub++) {
            int32_t oo[64] = {0};
            uint32_t v, s60 = 0;
            sd_engine_render(&e1, oo, 32);
            for (v = 0; v < e1.nvoices; v++) s60 += e1.voice[v].gate && e1.voice[v].note == 60u && e1.voice[v].src == SD_SRC_A;
            if (e1.t[0].seq_note_on) { onA++; cutA += !s60; }
        }
        CHECK(onA > 100u && cutA * 50u < onA, "POLY: ARP's note offs cut A's same note (%u of %u)", cutA, onA);
        for (k = 0; k < 2u; k++) {              /* MONO, then DUO: A plays with ARP on */
            sd_engine_init(&e1); sd_engine_patch(&e1, &T_GLASS);
            e1.mode = k ? SD_MODE_DUO : SD_MODE_MONO; run_ms(&e1, 5);
            sd_engine_pattern_to(&e1, 0, &pa); e1.bpm = 120; e1.seq_run = 1;
            e1.arp_on = 1; sd_engine_note(&e1, 64, 100); sd_engine_note(&e1, 67, 100);
            cutA = onA = arpSounds = 0;
            for (sub = 0; sub < 2000u; sub++) {
                int32_t oo[64] = {0};
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
        sd_engine_init(&e1); sd_engine_patch(&e1, &T_GLASS);   /* MONO, A playing: the hand has no voice */
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
    printf("switches: EDIT OFF = level 0 (REACTOR DISTROY SPACE FILTER), track OFF, B in MONO, XPOSE at the one; "
           "the voices' owners and quotas\n");
}

/* SPACE's ping-pong (both sides, never one): the first echo on the left, the second on the
 * right as loud, at FDBK 0 too; the third down by FDBK^2, as the stereo mode's after two */
static void test_pingpong(void)
{
    static SdEngine e5;
    static int32_t o[2u * 512u];
    static const uint16_t FB[2] = {0, 32000};
    uint32_t f, t, k, n, per = (uint32_t)(0.300 * SD_FS);
    for (f = 0; f < 2u; f++) {
        SdPatch p = T_GLASS;
        double el[4] = {0, 0, 0, 0}, er[4] = {0, 0, 0, 0};
        p.s_mode = SD_SP_PP; p.s_time = SD_SPMS(300u); p.s_fb = FB[f]; p.s_dlevel = 50000; p.s_rlevel = 0;
        p.s_dtone = 65535;
        sd_engine_init(&e5);
        memset(&tsm, 0, sizeof tsm);
        e5.sm = &tsm;
        sd_engine_patch(&e5, &p);
        run_ms(&e5, 20);
        sd_engine_note(&e5, 60, 127);
        for (t = 0, n = 0; t < 4u * per / 512u + 1u; t++) {
            memset(o, 0, sizeof o);
            if (t == 4u) sd_engine_note(&e5, 60, 0);
            sd_engine_render(&e5, o, 512u);
            for (k = 0; k < 512u; k++, n++)
                if (n / per < 4u) { el[n / per] += (double)o[2u * k] * o[2u * k]; er[n / per] += (double)o[2u * k + 1u] * o[2u * k + 1u]; }
        }
        CHECK(el[1] > 100.0 * er[1] && er[2] > 100.0 * el[2] && fabs(10.0 * log10(er[2] / el[1])) < 1.0,
              "ping-pong at FDBK %u: the 1st echo L %.1f dB (R %.1f), the 2nd R %.1f dB (L %.1f)", FB[f],
              10.0 * log10(el[1] + 1), 10.0 * log10(er[1] + 1), 10.0 * log10(er[2] + 1), 10.0 * log10(el[2] + 1));
    }
    printf("ping-pong: the first echo left, the second right as loud (FDBK 0 and 49 %%)\n");
}

/* MIDI clock (the modulation and the sequencer on another device's clock). Pulses at 100 BPM against the FM-1's own
 * 120, each up to 4 ms late at random (USB, the main loop), START
 * at a pulse; from 60 s 140 BPM; STOP and START again; from 100 s none. Each step starts no sooner than its 6th
 * pulse and within the lateness and two sub-blocks after it, for the whole run and through the change; a synced LFO
 * of a beat starts its cycles with every 24th pulse; the tempo is measured; 0.5 s without pulses: the FM-1's own */
static void test_clock(void)
{
    static SdEngine e6;
    static int32_t o[2u * SD_SUB];
    static double pt[16384];                    /* each pulse's time, frames */
    static uint32_t st_f[4096];                 /* each step's first frame (the last START's on) */
    SdPatch p = T_GLASS;
    const uint32_t LAT = 176u, END = 102u * SD_FS, STOP_F = 100u * SD_FS;
    double t = 1000.0, d_prev = 0, late_lo = 1e9, late_hi = -1e9, lfo_lo = 1e9, lfo_hi = -1e9, b100 = 0, b140 = 0;
    uint32_t np = 0, nd = 0, f, k, rng = 0x1234567u, start_p = 48u, last_cnt = 0xFFFFFFFFu, nst = 0, cyc = 0;
    uint32_t ph_last = 0, steps_on = 0, own_ok = 0, stopped = 0, restarted = 0, lfo0 = 0;
    double dl[16384];
    p.lfo_mode[0] = SD_LM_SYNC;
    p.lfo_rate[0] = 16384u + 100u;              /* (12 divisions: the 4th, a beat) */
    sd_engine_init(&e6);
    sd_engine_patch(&e6, &p);
    e6.bpm = 120;
    while (np < 16384u && t < (double)STOP_F) { /* the pulses and when the main loop hands them over */
        double d;
        pt[np] = t;
        rng = rng * 1103515245u + 12345u;
        d = t + (double)((rng >> 16) % (LAT + 1u));
        dl[np] = d < d_prev ? d_prev : d;       /* (in order) */
        d_prev = dl[np];
        t += SD_FS * 60.0 / ((t < 60.0 * SD_FS ? 100.0 : 140.0) * 24.0);
        np++;
    }
    for (f = 0; f < END; f += SD_SUB) {
        while (nd < np && dl[nd] <= (double)f) {   /* what the main loop read since the last sub-block (clk_rt) */
            e6.clk_in++;
            if (nd == start_p) { e6.seq_run = 1; e6.clk_start++; nst = 0; last_cnt = 0xFFFFFFFFu; cyc = 0; }
            nd++;
        }
        if (f <= 80u * SD_FS && 80u * SD_FS < f + SD_SUB) {   /* STOP; START again a beat after the next one */
            e6.seq_run = 0;
            stopped = 1;
            for (k = nd; k < np && (k - start_p) % 24u; k++) {}
            start_p = k + 24u;
        }
        memset(o, 0, sizeof o);
        sd_engine_render(&e6, o, SD_SUB);
        if (e6.seq_run && e6.t[0].seq_count != last_cnt) {   /* a step began at this sub-block's first frame */
            last_cnt = e6.t[0].seq_count;
            if (nst < 4096u) st_f[nst] = f;
            if (start_p + 6u * nst < np && f < STOP_F) {
                double late = (double)f - pt[start_p + 6u * nst];
                if (late < late_lo) late_lo = late;
                if (late > late_hi) late_hi = late;
                steps_on++;
                if (stopped && nst == 0u) restarted = 1;
            }
            nst++;
        }
        if (e6.seq_run && e6.lfo_ph[0] < ph_last) {   /* the LFO's cycle began */
            if (start_p + 24u * cyc < np && f < STOP_F) {
                double late = (double)f - pt[start_p + 24u * cyc];
                if (late < lfo_lo) lfo_lo = late;
                if (late > lfo_hi) lfo_hi = late;
            }
            if (cyc == 0u && e6.lfo_ph[0] == 0u) lfo0 = 1;
            cyc++;
        }
        ph_last = e6.lfo_ph[0];
        if (f <= 55u * SD_FS && 55u * SD_FS < f + SD_SUB) b100 = e6.clk_beat;
        if (f <= 78u * SD_FS && 78u * SD_FS < f + SD_SUB) b140 = e6.clk_beat;
    }
    if (nst > 8u && !e6.clk_on) {               /* no pulses: the steps at 120 BPM's 16th (5512.5 frames) again */
        double sp = (double)(st_f[nst - 1u] - st_f[nst - 5u]) / 4.0;
        own_ok = fabs(sp - 5512.5) < SD_SUB;
    }
    CHECK(steps_on > 700u && late_lo >= 0.0 && late_hi <= LAT + 2.0 * SD_SUB,
          "MIDI clock: %u steps from %.0f to %.0f frames after their pulse (0 .. %u)", steps_on, late_lo, late_hi,
          LAT + 2u * SD_SUB);
    CHECK(lfo0 && cyc > 20u && lfo_lo >= 0.0 && lfo_hi <= LAT + 2.0 * SD_SUB,
          "MIDI clock: the synced LFO's cycles from %.0f to %.0f frames after every 24th pulse (from 0 at START: %u)",
          lfo_lo, lfo_hi, lfo0);
    CHECK(fabs(b100 - 26460.0) < 26460.0 * 0.006 && fabs(b140 - 18900.0) < 18900.0 * 0.006,
          "MIDI clock: the tempo measured %.0f and %.0f frames a beat (26460, 18900)", b100, b140);
    CHECK(restarted && own_ok && e6.tempo_q8 == e6.seq_len_q8,
          "MIDI clock: START again %u, the FM-1's own tempo after the pulses %u", restarted, own_ok);
    printf("MIDI clock: %u steps %.0f..%.0f frames after their pulse (lateness to 4 ms), the LFO's beat %.0f..%.0f, "
           "the tempo 100 -> 140 BPM, START again, its own after\n", steps_on, late_lo, late_hi, lfo_lo, lfo_hi);
}

/* IN KEY (SEQ B transposed keeps to SEQ A's key and root): the transposes in
 * degrees of A's scale from its key, a chord tone by tone; CHROM in semitones, as before */
static uint32_t xkey_notes(SdEngine *e, const SdPattern *a, const SdPattern *b, int32_t xa, int32_t xb, uint32_t key,
                           uint32_t steps, uint8_t *na, uint8_t *nb)
{   /* the notes A and B start on their first steps: A's lowest held, B's */
    static int32_t out[2u * SD_SUB];
    uint32_t n = 0, f = 0, last = 0xFFFFFFFFu, v;
    sd_engine_init(e);
    sd_engine_patch(e, &T_GLASS);
    e->mode = SD_MODE_POLY;
    sd_engine_pattern_to(e, 0, a);
    sd_engine_pattern_to(e, 1, b);
    e->seqb = 1;
    e->xkey = (uint8_t)key;
    e->t[0].xpose_next = (int8_t)xa;
    e->t[1].xpose_next = (int8_t)xb;
    e->bpm = 120;
    run_ms(e, 10);
    e->seq_run = 1;
    while (n < steps && f < 10u * SD_FS) {
        memset(out, 0, sizeof out);
        sd_engine_render(e, out, SD_SUB);
        f += SD_SUB;
        if (e->t[0].seq_on && e->t[0].seq_count != last) {
            uint8_t lo = 127, bn = 0;
            last = e->t[0].seq_count;
            for (v = 0; v < e->nvoices; v++) {
                if (!e->voice[v].gate) continue;
                if (e->voice[v].src == SD_SRC_A && e->voice[v].note < lo) lo = e->voice[v].note;
                if (e->voice[v].src == SD_SRC_B) bn = e->voice[v].note;
            }
            na[n] = lo; nb[n++] = bn;
        }
    }
    return n;
}
static void test_xkey(void)
{
    static SdEngine e4;
    static const uint8_t LINE[4] = {60, 63, 67, 62};   /* C minor: C Eb G D */
    SdPattern a, b;
    uint8_t na[8], nb[8];
    uint32_t k, v, chord[3];
    sd_pattern_default(&a);
    a.len = 4; a.seg_count = 1; a.scale = SD_SC_MINOR;
    for (k = 0; k < 4u; k++) { a.step[k].note = LINE[k]; a.step[k].type = SD_ST_NOTE; a.step[k].gate = 90; }
    sd_pattern_default(&b);                     /* B: C3 held, its own scale MAJOR (IN KEY follows A's) */
    b.len = 4; b.seg_count = 1; b.scale = SD_SC_MAJOR;
    for (k = 0; k < 4u; k++) { b.step[k].note = 48; b.step[k].type = SD_ST_NOTE; b.step[k].gate = 90; }
    xkey_notes(&e4, &a, &b, 2, 4, 1, 4, na, nb);
    CHECK(na[0] == 62 && na[1] == 65 && na[2] == 68 && na[3] == 63, "IN KEY, A +2: %u %u %u %u (D F Ab Eb)", na[0], na[1], na[2], na[3]);
    CHECK(nb[0] == 51, "IN KEY, B +4 in A's C minor: %u (Eb3)", nb[0]);
    xkey_notes(&e4, &a, &b, 2, 4, 0, 4, na, nb);
    CHECK(na[0] == 62 && na[1] == 65 && na[2] == 69 && na[3] == 64 && nb[0] == 52, "CHROM, A +2: %u %u %u %u, B +4: %u",
          na[0], na[1], na[2], na[3], nb[0]);
    a.step[0].chord[0] = 3; a.step[0].chord[1] = 7;   /* C minor on step 1, up to Eb in key: Eb G Bb, major */
    for (k = 1; k < 4u; k++) a.step[k].type = SD_ST_TIE;
    xkey_notes(&e4, &a, &b, 3, 0, 1, 1, na, nb);
    for (k = 0, v = 0; v < e4.nvoices && k < 3u; v++) if (e4.voice[v].gate && e4.voice[v].src == SD_SRC_A) chord[k++] = e4.voice[v].note;
    CHECK(k == 3u && chord[0] + chord[1] + chord[2] == 63u + 67u + 70u, "IN KEY, the chord: %u %u %u (Eb G Bb)",
          chord[0], k > 1u ? chord[1] : 0u, k > 2u ? chord[2] : 0u);
    printf("in key: A +2 in C minor D F Ab Eb (CHROM D F A E), B +4 to Eb in A's key, Cm +3 to Eb major\n");
}

/* the destinations in the lists' order (each once); ENV 1's DECAY and S TIME moved by the
 * matrix; the divisions fastest to slowest; ARP's KEEP: how many steps, what it played in order, a gap as rests,
 * the window ending at the nearest step, the ring from when ARP came on */
static void test_more(void)
{
    static SdEngine e6;
    static int32_t o[2u * 8820u];
    uint8_t seen[SD_MDSTS], nt[SD_PSTEPS], vl[SD_PSTEPS];
    uint32_t i, ok, got;
    double d0, d1;
    memset(seen, 0, sizeof seen);
    for (i = 0; i < SD_MDSTS; i++) if (SD_MD_ORDER[i] < SD_MDSTS) seen[SD_MD_ORDER[i]]++;
    for (ok = 1, i = 0; i < SD_MDSTS; i++) ok &= seen[i] == 1u;
    CHECK(ok, "SD_MD_ORDER holds each of the %u destinations once", SD_MDSTS);
    for (ok = 1, i = 1; i < SD_SDIVS; i++) {
        uint32_t a = SD_SDIV_ORDER[i - 1u], b = SD_SDIV_ORDER[i];
        ok &= SD_SDIV_NUM[a] * SD_SDIV_DEN[b] < SD_SDIV_NUM[b] * SD_SDIV_DEN[a];
    }
    CHECK(ok, "SD_SDIV_ORDER: the divisions not fastest to slowest");
    CHECK(sd_arp_keep_len(2) == 16u && sd_arp_keep_len(6) == 64u && sd_arp_keep_len(1) == 24u &&
          sd_arp_keep_len(10) == 16u && sd_arp_keep_len(11) == 8u && sd_arp_keep_len(8) == 32u &&
          sd_arp_keep_len(9) == 12u && sd_arp_keep_len(5) == 8u,
          "KEEP's lengths: 1/16 %u, 1/64 %u, 1/16T %u, 1/8D %u, 1/2 %u, 1/16D %u, 1/4T %u, 1/4 %u", sd_arp_keep_len(2),
          sd_arp_keep_len(6), sd_arp_keep_len(1), sd_arp_keep_len(10), sd_arp_keep_len(11), sd_arp_keep_len(8),
          sd_arp_keep_len(9), sd_arp_keep_len(5));
    {   /* ENV 1's DECAY: a short one, then the wheel at full through +100 % to E1 DECAY: the note 200 ms on louder */
        SdPatch p = T_GLASS;
        p.e1_a = 0; p.e1_d = 40; p.e1_s = 0; p.e1_r = 40;
        p.mod[0] = (SdModSlot){SD_MS_WHEEL, SD_MD_E1D, 32767}; p.mod[1] = (SdModSlot){0, 0, 0};
        mod_init(&e6, &p);
        e6.wheel = 0;
        run_ms(&e6, 5);
        sd_engine_note(&e6, 60, 120);
        run(&e6, o, 8820u, SD_SUB);
        d0 = rms_db(o, 6615u, 8820u);
        mod_init(&e6, &p);
        e6.wheel = 127;
        run_ms(&e6, 5);
        sd_engine_note(&e6, 60, 120);
        run(&e6, o, 8820u, SD_SUB);
        d1 = rms_db(o, 6615u, 8820u);
        CHECK(d1 > d0 + 12.0, "E1 DECAY by the wheel: 150..200 ms %.1f dB, without %.1f dB", d1, d0);
        sd_mod_restore(&e6);
        CHECK(e6.patch.e1_d == 40u, "E1 DECAY: the restore misses it (%u)", e6.patch.e1_d);
    }
    {   /* L1 RATE: LFO 1 slow (free), the wheel at full through +50 % to its RATE: it turns many times as often */
        SdPatch p = T_GLASS;
        uint32_t w, flips[2], t;
        p.lfo_shape[0] = SD_LS_SQR; p.lfo_mode[0] = 0; p.lfo_rate[0] = 20000;
        p.mod[0] = (SdModSlot){SD_MS_WHEEL, SD_MD_L1RATE, 16384}; p.mod[1] = (SdModSlot){SD_MS_LFO1, SD_MD_XM, 100};
        for (w = 0; w < 2u; w++) {
            int32_t last = 0;
            mod_init(&e6, &p);
            e6.wheel = w ? 127u : 0u;
            for (flips[w] = 0, t = 0; t < 1378u; t++) {   /* a second of ticks */
                run_ms(&e6, 0); mod_run(&e6, &p, 1);
                if (e6.lfo_out[0] != last) flips[w]++;
                last = e6.lfo_out[0];
            }
        }
        CHECK(flips[1] > 4u * flips[0] + 4u, "L1 RATE by the wheel: LFO 1 turned %u times, without %u", flips[1], flips[0]);
        printf("more: L1 RATE by the wheel, LFO 1's square %u flips a second (%u without)\n", flips[1], flips[0]);
    }
    /* KEEP: C E G UP on 16ths at 120 BPM (125 ms), a bar of it; then a gap of four steps, two more notes */
    sd_engine_init(&e6); sd_engine_patch(&e6, &T_GLASS);
    e6.arp_on = 1; e6.arp_oct = 1; e6.arp_mode = SD_AM_UP; e6.arp_div = 2; e6.bpm = 120;
    run_ms(&e6, 5);
    sd_engine_note(&e6, 48, 100); sd_engine_note(&e6, 52, 100); sd_engine_note(&e6, 55, 100);
    run_ms(&e6, 16u * 125u - 20u);              /* 16 steps begun, the last 105 ms ago: the window ends after it */
    got = sd_arp_keep(&e6, 16, nt, vl);
    for (ok = 1, i = 0; i < 16u; i++) ok &= nt[i] == (i % 3u == 0u ? 48u : i % 3u == 1u ? 52u : 55u);
    CHECK(ok && got == 16u, "KEEP a bar: %u notes, %u %u %u .. %u", got, nt[0], nt[1], nt[2], nt[15]);
    run_ms(&e6, 30);                            /* step 17 begun 10 ms ago: not in it yet */
    got = sd_arp_keep(&e6, 16, nt, vl);
    CHECK(nt[0] == 48u && nt[15] == 48u && got == 16u, "KEEP at the one: the bar that ended (%u .. %u)", nt[0], nt[15]);
    sd_engine_note(&e6, 48, 0); sd_engine_note(&e6, 52, 0); sd_engine_note(&e6, 55, 0);
    run_ms(&e6, 4u * 125u + 100u);              /* a gap */
    sd_engine_note(&e6, 60, 90);
    run_ms(&e6, 2u * 125u - 60u);               /* two more steps of C4 */
    sd_engine_note(&e6, 60, 0);
    got = sd_arp_keep(&e6, 16, nt, vl);
    CHECK(nt[15] == 60u && nt[14] == 60u && nt[13] == 0u && nt[10] == 0u && nt[9] != 0u && vl[15] == 90u,
          "KEEP across a gap: ..%u %u %u %u | %u %u (vel %u)", nt[9], nt[10], nt[12], nt[13], nt[14], nt[15], vl[15]);
    e6.arp_on = 0; run_ms(&e6, 10); e6.arp_on = 1; run_ms(&e6, 10);
    CHECK(sd_arp_keep(&e6, 16, nt, vl) == 0u, "KEEP: ARP on again starts a new ring");
    printf("more: %u destinations in the chain's order, E1 DECAY by the wheel: %.0f dB at 150..200 ms (%.0f without); "
           "%u divisions; KEEP a bar, at the one, a gap as rests\n", SD_MDSTS, d1, d0, SD_SDIVS);
}

/* what is switched on while the master plays comes in at its one (or the master's choice: A or B): B ON at A's
 * step 2 silent till A's one, then
 * its step 1 on the same sub-block; ORBIT on there plays its step 1 from the one, not before; MASTER B: A ON waits for
 * B's one, ORBIT steps at B's ones */
static uint32_t cue_note(const SdEngine *e, uint32_t lo, uint32_t hi)   /* a voice with a gate on lo..hi: its note */
{
    uint32_t i;
    for (i = 0; i < e->nvoices; i++)
        if (e->voice[i].gate && e->voice[i].note >= lo && e->voice[i].note <= hi) return e->voice[i].note;
    return 0;
}
static void test_cue(void)
{
    static SdEngine e4;
    static int32_t o[2u * SD_SUB];
    SdPattern pa, pb;
    SdOrbit ob;
    uint32_t i, f, a_was, one_f, b_f, early, a_n[4], m;
    sd_pattern_default(&pa); sd_pattern_default(&pb);
    for (i = 0; i < SD_PSTEPS; i++) {
        pa.step[i].type = pb.step[i].type = SD_ST_NOTE;
        pa.step[i].note = 72; pb.step[i].note = 36; pa.step[i].gate = pb.step[i].gate = 50;
    }
    pa.len = 4; pb.len = 3; pa.seg_count = pb.seg_count = 1;
    sd_engine_init(&e4); sd_engine_patch(&e4, &T_GLASS);
    e4.mode = SD_MODE_DUO;
    run_ms(&e4, 5);
    sd_engine_pattern_to(&e4, 0, &pa); sd_engine_pattern_to(&e4, 1, &pb);
    e4.t[1].mute = 1;
    e4.seqb = 1; e4.bpm = 120; e4.seq_run = 1;
    run_ms(&e4, 125 + 60);                      /* A on its step 2 (125 ms a 16th) */
    CHECK(e4.t[0].seq_shown == 1u, "A not on step 2 (%u)", e4.t[0].seq_shown);
    e4.t[1].mute = 0;                           /* B ON: in at A's one, 315 ms on */
    a_was = e4.t[0].seq_shown; one_f = b_f = 0; early = 0;
    for (f = 0; f < SD_FS && !(one_f && b_f); f += SD_SUB) {
        memset(o, 0, sizeof o);
        sd_engine_render(&e4, o, SD_SUB);
        if (!one_f && a_was && !e4.t[0].seq_shown) one_f = f + 1u;
        if (cue_note(&e4, 30, 40)) { if (!b_f) b_f = f + 1u; if (!one_f) early = 1; }
        a_was = e4.t[0].seq_shown;
    }
    CHECK(!early && one_f && b_f == one_f && e4.t[1].seq_shown == 0u && e4.t[1].in,
          "SEQ B ON: in at A's one (%u frames on), its first note at %u, B's step %u", one_f, b_f, e4.t[1].seq_shown);
    CHECK(one_f > 5000u, "SEQ B ON: A's one came too soon (%u frames)", one_f);
    /* ORBIT on at A's step 2: A's notes as they were till the one, then ORBIT's step 1 (+7), a loop on its step 2 (+5) */
    memset(&ob, 0, sizeof ob);
    ob.len = 2; ob.xa[0] = 7; ob.xa[1] = 5;
    run_ms(&e4, 125 + 60);
    CHECK(e4.t[0].seq_shown == 1u, "ORBIT: A not on step 2 (%u)", e4.t[0].seq_shown);
    ob.on = 1;
    sd_engine_orbit(&e4, &ob);
    m = 0; a_was = e4.t[0].seq_shown;
    for (f = 0; f < 2u * SD_FS && m < 4u; f += SD_SUB) {   /* A's notes at steps 3, 4, 1 and 1 a loop on */
        static const uint8_t WANT[4] = {2, 3, 0, 0};
        memset(o, 0, sizeof o);
        sd_engine_render(&e4, o, SD_SUB);
        if (e4.t[0].seq_shown != a_was) {
            a_was = e4.t[0].seq_shown;
            if (a_was == WANT[m]) a_n[m++] = cue_note(&e4, 60, 90);
        }
    }
    CHECK(m == 4u && a_n[0] == 72u && a_n[1] == 72u && a_n[2] == 79u && a_n[3] == 77u && e4.orb_live,
          "ORBIT on: in at A's one (steps 3, 4, 1, 1 a loop on: %u %u %u %u)", a_n[0], a_n[1], a_n[2], a_n[3]);
    ob.on = 0;
    sd_engine_orbit(&e4, &ob);
    run_ms(&e4, 5);
    CHECK(!e4.orb_live && !e4.t[0].orb_x, "ORBIT off: not at once");
    /* MASTER B (3 steps, 375 ms a loop): A ON at B's step 2 silent till B's one, then its step 1 there; ORBIT's steps
       at B's ones; back to A */
    run_ms(&e4, 500);
    e4.master = 1;
    e4.t[0].mute = 1;
    run_ms(&e4, 20);
    while (e4.t[1].seq_shown != 1u) run_ms(&e4, 5);
    run_ms(&e4, 30);
    e4.t[0].mute = 0;
    a_was = e4.t[1].seq_shown; one_f = b_f = 0; early = 0;
    for (f = 0; f < SD_FS && !(one_f && b_f); f += SD_SUB) {
        memset(o, 0, sizeof o);
        sd_engine_render(&e4, o, SD_SUB);
        if (!one_f && a_was && !e4.t[1].seq_shown) one_f = f + 1u;
        if (cue_note(&e4, 60, 90)) { if (!b_f) b_f = f + 1u; if (!one_f) early = 1; }
        a_was = e4.t[1].seq_shown;
    }
    CHECK(!early && one_f && b_f == one_f && e4.t[0].seq_shown == 0u && e4.t[0].in && e4.lead == 1u,
          "MASTER B, SEQ A ON: in at B's one (%u frames on), its first note at %u, A's step %u", one_f, b_f, e4.t[0].seq_shown);
    ob.on = 1;
    sd_engine_orbit(&e4, &ob);
    m = 0; a_was = e4.orb_shown;
    for (f = 0; f < 2u * SD_FS && m < 3u; f += SD_SUB) {   /* ORBIT's next steps: each at B's one */
        memset(o, 0, sizeof o);
        sd_engine_render(&e4, o, SD_SUB);
        if (e4.orb_live && e4.orb_shown != a_was) {
            a_was = e4.orb_shown;
            a_n[m++] = e4.t[1].seq_shown;
        }
        if (!e4.orb_live) a_was = 0xFFu;
    }
    CHECK(m == 3u && !a_n[0] && !a_n[1] && !a_n[2] && f < SD_FS * 3u / 2u,
          "MASTER B: ORBIT's steps at B's ones (B's step %u %u %u, %u frames for three)", a_n[0], a_n[1], a_n[2], f);
    e4.master = 0;
    run_ms(&e4, 5);
    CHECK(e4.lead == 0u && e4.t[0].in && e4.t[1].in, "MASTER A again: not at once");
    printf("cue: SEQ B ON in at A's one, its step 1 there; ORBIT on from A's one, its step 1; MASTER B: A ON in at B's "
           "one (%u frames on), ORBIT's steps at B's ones\n", one_f);
}

/* TURING to PITCH, quantized to SEQ A's key first: a held A3 under TURING (CHANGE 50, its own
 * DEST PITCH at 60 %, and a MOD slot TURING -> PITCH): every tick the oscillators sit a whole number of semitones from
 * the same engine without it, on a note of A minor (A's default pattern), several of them; an off-scale key (A#3)
 * lands in the scale too; CHROM: whole semitones */
static void test_tq(void)
{
    static SdEngine eq, er;
    static int32_t o[2u * SD_SUB];
    SdPatch pt = T_GLASS, pr;
    uint32_t f, k, seen = 0, ticks = 0, out = 0, sc, notes;
    double worst = 0;
    for (sc = 0; sc < 2u; sc++) {               /* A minor; then CHROM */
        for (k = 0; k < 2u; k++) {              /* A3, then A#3 */
            uint32_t key = k ? 58u : 57u;
            SdPattern a;
            sd_pattern_default(&a);
            if (sc) a.scale = SD_SC_CHROM;
            pt = T_GLASS;
            memset(pt.mod, 0, sizeof pt.mod);
            pt.tur_set = 0x80u | 50u; pt.tur_ld = 8u; pt.tur_dst = SD_MD_PITCH; pt.tur_amt = 60;
            pt.mod[0] = (SdModSlot){SD_MS_TUR, SD_MD_PITCH, 8000};
            pr = pt; pr.tur_amt = 0; pr.mod[0].amt = 0;   /* (the same TURING, nothing to PITCH) */
            sd_engine_init(&eq); sd_engine_init(&er);
            sd_engine_patch(&eq, &pt); sd_engine_patch(&er, &pr);
            sd_engine_pattern(&eq, &a); sd_engine_pattern(&er, &a);
            run_ms(&eq, 5); run_ms(&er, 5);
            sd_engine_note(&eq, key, 100); sd_engine_note(&er, key, 100);
            for (f = 0; f < 3u * SD_FS; f += SD_SUB) {
                double st, d;
                int32_t n;
                memset(o, 0, sizeof o); sd_engine_render(&eq, o, SD_SUB);
                memset(o, 0, sizeof o); sd_engine_render(&er, o, SD_SUB);
                if (!eq.bank[0].osc[0].inc || !er.bank[0].osc[0].inc) continue;
                st = 12.0 * log2((double)eq.bank[0].osc[0].inc / er.bank[0].osc[0].inc);
                n = (int32_t)lround(st);
                d = fabs(st - n);
                if (d > worst) worst = d;
                n += (int32_t)key;
                if (!sc && !(SD_SCALE_MASK[SD_SC_MINOR] >> (uint32_t)((n - 45 + 120) % 12) & 1u)) out++;
                if (n >= 30 && n < 62) seen |= 1u << (n - 30);
                ticks++;
            }
        }
    }
    for (notes = 0, k = 0; k < 32u; k++) notes += seen >> k & 1u;
    CHECK(ticks > 10000u && worst < 0.01 && !out && notes >= 5u,
          "TURING to PITCH: %u ticks, %.4f semitones off a whole one at worst, %u off A minor, %u notes", ticks, worst, out, notes);
    printf("tq: TURING to PITCH quantized: %u ticks on whole semitones (%.4f off at worst), all in A minor (off-scale A#3 "
           "too), %u notes; CHROM whole semitones\n", ticks, worst, notes);
}

/* ARP on the grid (not from the moment the keys went down, up to half a step off the beat): while SEQ A plays,
 * every ARP note within a sub-block of the grid from START at 1/16,
 * 1/16T, 1/16D and 1/64, after 60 s too and after a new hand (whose first may come late, as played, by under a
 * quarter of a step); a hand just after a point plays at once, a later one waits for the next point; at 1/16 each
 * of SEQ A's steps has ARP's note in the same sub-block */
static void test_arp_grid(void)
{
    static SdEngine ea;
    static int32_t o[2u * SD_SUB];
    static const uint8_t DV[4] = {2, 1, 8, 6};
    uint32_t c, f, worst = 0, first_now = 0, first_wait = 0, a_hits = 0, a_steps = 0;
    for (c = 0; c < 5u; c++) {
        uint32_t d = DV[c < 4u ? c : 0u], was = 0, wasa = 0, first = 1,
                 press = (c == 4u ? 22050u + 3000u : 22050u + 600u) / SD_SUB * SD_SUB,   /* (600 frames after a
                                                   16th: under a quarter of one; 3000: over) */
                 again = (20u * SD_FS + 7777u) / SD_SUB * SD_SUB;
        uint64_t step = (uint64_t)1411200u * SD_SDIV_NUM[d] / SD_SDIV_DEN[d];   /* x 256 frames: 120 BPM (5512.5 a 16th) */
        SdPattern p;
        sd_engine_init(&ea); sd_engine_patch(&ea, &T_GLASS);
        ea.mode = SD_MODE_POLY; ea.bpm = 120;
        sd_pattern_default(&p); sd_engine_pattern(&ea, &p);
        ea.arp_div = d; ea.arp_on = 1; ea.arp_gate = 50;
        run_ms(&ea, 20);
        ea.seq_run = 1;
        for (f = 0; f < 62u * SD_FS; f += SD_SUB) {
            uint32_t k;
            if (f == press || f == again) for (k = 0; k < 3u; k++) sd_engine_note(&ea, 60 + 4 * k, 100);
            if (f == (again - SD_FS / 2u) / SD_SUB * SD_SUB) for (k = 0; k < 3u; k++) sd_engine_note(&ea, 60 + 4 * k, 0);
            memset(o, 0, sizeof o);
            sd_engine_render(&ea, o, SD_SUB);
            {
                uint32_t on = ea.arp_playing && !was;
                if (on) {                       /* a note on: where on the grid (START at this run's frame 0) */
                    uint64_t r = ((uint64_t)f * 256u) % step;
                    uint32_t off = (uint32_t)((r > step / 2u ? step - r : r) >> 8);
                    if (first) { if (c == 4u) first_wait = f - press; else if (!c) first_now = f - press; first = 0; }   /* (1/16:
                                                   600 frames is under a quarter step; at 1/64 it is not) */
                    else if (f != again) { if (off > worst) worst = off; }   /* (a new hand's first may come late,
                                                   as played, by under a quarter of a step: by design) */
                }
                if (c == 0u && ea.t[0].seq_count != wasa) {   /* A's step: ARP's note in the same sub-block */
                    wasa = ea.t[0].seq_count;
                    if (f >= press + 2u * SD_SUB && f < again - SD_FS / 2u) { a_steps++; a_hits += on; }
                }
                was = ea.arp_playing;
            }
        }
    }
    CHECK(worst <= SD_SUB, "ARP off the grid by %u frames", worst);
    CHECK(first_now <= SD_SUB, "a hand just after a grid point waited %u frames", first_now);
    CHECK(first_wait > 1000u, "a hand far from the grid played at once (%u frames)", first_wait);
    CHECK(a_hits * 10u > a_steps * 9u, "ARP's notes not with A's steps: %u of %u", a_hits, a_steps);
    printf("arp grid: every note within %u frames of the grid (1/16, 1/16T, 1/16D, 1/64, 60 s, a new hand); a hand just "
           "after a point at once (%u frames), a later one at the next point (%u frames on); %u of A's %u steps with ARP's\n",
           worst, first_now, first_wait, a_hits, a_steps);
}

/* REC writes what is heard (never back in another octave): a key under a transpose
 * (SCL's and ORBIT's, -24..+24 in all) goes in so the step plays that key again; CHROM every note, IN KEY every note
 * of A's scale (C minor), on both tracks; no transpose: the key as it is */
static void test_rec_note(void)
{
    static SdEngine e5;                         /* (zeroed: only the tracks' transposes and A's pattern are read) */
    static const int32_t SPLIT[3][2] = {{1, 0}, {0, 1}, {2, -1}};   /* x as SCL's, ORBIT's, both */
    uint32_t trk, key, n, bad = 0, tried = 0;
    int32_t x, s;
    sd_pattern_default(&e5.t[0].pat);
    e5.t[0].pat.step[0].note = 60; e5.t[0].pat.scale = SD_SC_MINOR;
    for (key = 0; key < 2u; key++)
        for (trk = 0; trk < 2u; trk++)
            for (x = -24; x <= 24; x++)
                for (s = 0; s < 3; s++) {
                    SdSeqT *t = &e5.t[trk];
                    int32_t xa = s == 2 ? x / 2 : x * SPLIT[s][0], xb = x - xa;
                    e5.xkey = (uint8_t)key;
                    t->xpose = (int8_t)xa; t->orb_x = (int8_t)xb; t->tur_deg = 0;
                    for (n = 36; n <= 96u; n++) {
                        if (key && sd_scale_note(60, SD_SC_MINOR, sd_scale_degree(60, SD_SC_MINOR, n)) != n) continue;
                        tried++;
                        if (sd_seq_xn(&e5, t, sd_seq_rec_note(&e5, t, n)) != n) bad++;
                    }
                }
    e5.t[0].xpose = 0; e5.t[0].orb_x = 0;
    CHECK(bad == 0, "REC under a transpose: %u of %u notes play back as another", bad, tried);
    CHECK(sd_seq_rec_note(&e5, &e5.t[0], 61) == 61, "REC, no transpose: the key as it is");
    e5.xkey = 0; e5.t[1].xpose = 12; e5.t[1].orb_x = 0;
    CHECK(sd_seq_rec_note(&e5, &e5.t[1], 72) == 60, "REC on B an octave up (CHROM): C5 goes in as C4");
    printf("rec: under every transpose -24..+24 (SCL's, ORBIT's, both; IN KEY and CHROM; A and B) the step plays the "
           "key (%u notes)\n", tried);
}

/* ORBIT: the steps on SEQ A's ones, XPOSE A moving the line, a lane to XM jumping or gliding, a macro's
 * change, nothing when it is off, the pattern never rewritten */
static void orbit_run(SdEngine *e, const SdPattern *p, const SdOrbit *o, uint32_t loops, uint8_t *notes, int32_t *xm,
                      int32_t *xm_mid)
{   /* each loop of A (4 steps): the note at its first step, XM there and half way through it */
    static int32_t out[2u * SD_SUB];
    uint32_t n = 0, f = 0, last = 0xFFFFFFFFu;
    SdPatch pt = T_GLASS;
    pt.xm = 30000;
    sd_engine_init(e);
    sd_engine_patch(e, &pt);
    sd_engine_pattern(e, p);
    if (o) sd_engine_orbit(e, o);
    e->bpm = 120;
    e->seq_run = 1;
    while (n < loops && f < 60u * SD_FS) {
        memset(out, 0, sizeof out);
        sd_engine_render(e, out, SD_SUB);
        f += SD_SUB;
        if (e->t[0].seq_on && e->t[0].seq_count != last) {
            last = e->t[0].seq_count;
            if (e->t[0].seq_shown == 0u) { notes[n] = e->voice[e->m_last].note; xm[n] = e->patch.xm; }
            if (e->t[0].seq_shown == 2u) xm_mid[n++] = e->patch.xm;
        }
    }
}
static void test_orbit(void)
{
    static SdEngine e3;
    SdPattern p, p0;
    SdOrbit o;
    uint8_t nt[16];
    int32_t xm[16], xmm[16];
    uint32_t k;
    sd_pattern_default(&p);
    p.len = 4;
    p.seg_count = 1;
    for (k = 0; k < 4u; k++) { p.step[k].note = 60; p.step[k].type = SD_ST_NOTE; }
    p0 = p;
    memset(&o, 0, sizeof o);
    CHECK(!o.on && SD_ORB_LEN(&o) == 4u && SD_ORB_LOOPS(&o) == 1u, "ORBIT's zero: off, 4 steps, a loop each");
    o.len = 3;                                  /* off: the line as it is */
    o.xa[1] = 5; o.xa[2] = 7;
    orbit_run(&e3, &p, &o, 7, nt, xm, xmm);
    CHECK(nt[0] == 60 && nt[1] == 60 && nt[2] == 60 && nt[3] == 60, "ORBIT off moved the notes (%u %u %u)", nt[0], nt[1], nt[2]);
    o.on = 1;                                   /* on: C, F, G, C ... a loop each */
    orbit_run(&e3, &p, &o, 7, nt, xm, xmm);
    CHECK(nt[0] == 60 && nt[1] == 65 && nt[2] == 67 && nt[3] == 60 && nt[4] == 65 && nt[6] == 60,
          "ORBIT XPOSE A: %u %u %u %u %u %u %u", nt[0], nt[1], nt[2], nt[3], nt[4], nt[5], nt[6]);
    CHECK(!memcmp(&e3.t[0].pat, &p0, sizeof p0), "ORBIT rewrote the pattern");
    o.rate = 1;                                 /* two loops a step */
    orbit_run(&e3, &p, &o, 6, nt, xm, xmm);
    CHECK(nt[0] == 60 && nt[1] == 60 && nt[2] == 65 && nt[3] == 65 && nt[4] == 67, "ORBIT RATE 2: %u %u %u %u %u",
          nt[0], nt[1], nt[2], nt[3], nt[4]);
    o.rate = 0;                                 /* a lane to XM, jumping: 0, +50 %, -50 % */
    o.xa[1] = o.xa[2] = 0;
    o.dst[0] = SD_MD_XM; o.lane[0][1] = 50; o.lane[0][2] = -50;
    orbit_run(&e3, &p, &o, 4, nt, xm, xmm);
    CHECK(xm[0] == 30000 && xm[1] > 30000 + 10000 && xm[2] < 30000 - 10000 && xmm[0] == xm[0],
          "ORBIT lane, jumping: XM %d %d %d (mid %d)", xm[0], xm[1], xm[2], xmm[0]);
    o.glide = SD_OG_L1;                         /* gliding: half way through step 1, half way to +50 % */
    orbit_run(&e3, &p, &o, 4, nt, xm, xmm);
    CHECK(xmm[0] > 30000 + 4000 && xmm[0] < xm[1], "ORBIT lane, gliding: XM %d, half way %d, then %d", xm[0], xmm[0], xm[1]);
    o.glide = 0; o.dst[0] = 0;                  /* MUTATE +99 on step 2: XM up there */
    o.mac[0][1] = 99;
    orbit_run(&e3, &p, &o, 4, nt, xm, xmm);
    CHECK(xm[0] == 30000 && xm[1] > 30000 + 5000 && xm[2] == 30000, "ORBIT MUTATE: XM %d %d %d", xm[0], xm[1], xm[2]);
    printf("orbit: off by default; XPOSE A C F G a loop each, two with RATE 2; a lane jumping and gliding; MUTATE a "
           "step's; the pattern untouched\n");
}

/* The output's limiter (STRINGS' POLY chord at velocity 127 flattened 9 % of its samples, the macros and
 * slots at full up to a fifth of them on 20 patches): the chord now never past its ceiling, a quiet note untouched (the
 * gain open, the output the input 64 frames later) */
static void test_limiter(void)
{
    static SdEngine e2;
    static int32_t o[2u * 2048u];
    uint32_t t, i, flat = 0;
    int32_t pk = 0, gmin = 32768, g1;
    sd_engine_init(&e2);
    sd_engine_patch(&e2, &SD_PATCH[SD_P_STRINGS]);
    e2.mode = SD_MODE_POLY;
    run_ms(&e2, 20);
    sd_engine_note(&e2, 48, 127); sd_engine_note(&e2, 55, 127); sd_engine_note(&e2, 60, 127); sd_engine_note(&e2, 63, 127);
    for (t = 0; t < 60u; t++) {
        memset(o, 0, sizeof o);
        sd_engine_render(&e2, o, 2048u);
        for (i = 0; i < 2u * 2048u; i++) { int32_t a = abs(o[i]); if (a > pk) pk = a; flat += a >= 32767; }
        if (e2.lim_g < gmin) gmin = e2.lim_g;
    }
    CHECK(pk <= SD_LIM_T + SD_LIM_T / 100 && !flat && gmin < 32768, "the limiter: STRINGS' chord at %d (ceiling %d), %u "
          "samples flat, its gain down to %d", pk, SD_LIM_T, flat, gmin);
    g1 = gmin;
    sd_engine_init(&e2);
    sd_engine_patch(&e2, &T_GLASS);
    run_ms(&e2, 20);
    sd_engine_note(&e2, 60, 60);
    for (t = 0, gmin = 32768; t < 20u; t++) {
        memset(o, 0, sizeof o);
        sd_engine_render(&e2, o, 2048u);
        if (e2.lim_g < gmin) gmin = e2.lim_g;
    }
    CHECK(gmin == 32768, "the limiter acted on a quiet note (gain %d)", gmin);
    printf("limiter: STRINGS' POLY chord at velocity 127 peaks %.3f x full scale, none flat (the gain down to %.1f dB); "
           "a quiet note untouched\n", pk / 32768.0, 20.0 * log10(g1 / 32768.0));
}

/* TURING: a modulation source in the patch. Off by default; CHANGE 0 a locked loop of LENGTH steps;
 * DEST XM moves the patch's XM in steps; NOTES moves SEQ A's notes in the scale and never rewrites the pattern */
static void test_turing(void)
{
    static SdEngine e1;
    static int32_t o[2u * SD_SUB];
    SdPatch pt = T_GLASS;
    SdPattern p, p0;
    uint32_t k, f, last, xmax = 0, xmin = 65535;
    int32_t vals[64];
    uint8_t notes[64];
    CHECK(!SD_TUR_ON(&pt) && SD_TUR_LEN(&pt) == 8u && SD_TUR_DIV(&pt) == 2u, "TURING's zero: off, LENGTH 8, 1/16");
    /* a locked loop: CHANGE 0, LENGTH 5, every 16th: the output repeats every 5 steps (once the 8 bits it reads are
       all the loop's: 3 steps in) */
    pt.tur_set = 0x80u; pt.tur_ld = 5u; pt.tur_dst = SD_MD_XM; pt.tur_amt = 50; pt.xm = 32768;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &pt);
    e1.bpm = 120;
    for (k = 0, f = 0, last = e1.tur_count; k < 40u && f < 20u * SD_FS; f += SD_SUB) {
        memset(o, 0, sizeof o);
        sd_engine_render(&e1, o, SD_SUB);
        if (e1.tur_count != last) { last = e1.tur_count; vals[k++] = e1.tur_out; }
        if (e1.patch.xm > xmax) xmax = e1.patch.xm;
        if (e1.patch.xm < xmin) xmin = e1.patch.xm;
    }
    CHECK(k == 40u, "TURING stepped %u times in 20 s", k);
    {
        uint32_t same = 1, varied = 0;
        for (k = 8; k < 40u; k++) same &= vals[k] == vals[k - 5u];
        for (k = 4; k < 8u; k++) varied |= vals[k] != vals[3];
        CHECK(same && varied, "CHANGE 0: not a loop of 5 (same %u, varied %u)", same, varied);
    }
    CHECK(xmax > 32768u + 3000u && xmin < 32768u - 3000u, "DEST XM at 50 %%: XM moved only %u .. %u", xmin, xmax);
    /* CHANGE 100: each pass the inverse, so a loop of twice LENGTH */
    pt.tur_set = 0x80u | 100u;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &pt);
    for (k = 0, f = 0, last = e1.tur_count; k < 40u && f < 20u * SD_FS; f += SD_SUB) {
        memset(o, 0, sizeof o);
        sd_engine_render(&e1, o, SD_SUB);
        if (e1.tur_count != last) { last = e1.tur_count; vals[k++] = e1.tur_out; }
    }
    {
        uint32_t same = 1, half = 1;
        for (k = 13; k < 40u; k++) same &= vals[k] == vals[k - 10u];
        for (k = 13; k < 40u; k++) half &= vals[k] == vals[k - 5u];
        CHECK(same && !half, "CHANGE 100: not a loop of 10 (same %u, of 5 %u)", same, half);
    }
    /* off: nothing moves */
    pt.tur_set = 25u;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &pt);
    last = e1.tur_count;
    for (f = 0; f < 2u * SD_FS; f += SD_SUB) { memset(o, 0, sizeof o); sd_engine_render(&e1, o, SD_SUB); }
    CHECK(e1.tur_count == last && e1.patch.xm == e1.base.xm, "TURING off still stepped or moved XM");
    /* NOTES: SEQ A's notes move in the scale (C minor), the pattern stays as written */
    sd_pattern_default(&p);
    for (k = 0; k < 16u; k++) { p.step[k].note = 60; p.step[k].type = SD_ST_NOTE; }
    p.scale = SD_SC_MINOR;
    p0 = p;
    pt.tur_set = 0x80u | 30u; pt.tur_ld = 16u; pt.tur_dst = SD_TD_NOTES; pt.tur_amt = 50;
    sd_engine_init(&e1);
    sd_engine_patch(&e1, &pt);
    sd_engine_pattern(&e1, &p);
    e1.bpm = 120;
    e1.seq_run = 1;
    for (k = 0, f = 0, last = 0xFFFFFFFFu; k < 48u && f < 30u * SD_FS; f += SD_SUB) {
        memset(o, 0, sizeof o);
        sd_engine_render(&e1, o, SD_SUB);
        if (e1.t[0].seq_on && e1.t[0].seq_count != last) { last = e1.t[0].seq_count; notes[k++] = e1.voice[e1.m_last].note; }
    }
    {
        uint32_t moved = 0, inscale = 1;
        for (k = 0; k < 48u; k++) {
            moved += notes[k] != 60u;
            inscale &= (SD_SCALE_MASK[SD_SC_MINOR] >> ((notes[k] + 12u - 60u % 12u) % 12u)) & 1u;
            CHECK(notes[k] >= 60u - 14u && notes[k] <= 60u + 14u, "NOTES at 50 %%: %u is past +-6 degrees", notes[k]);
        }
        CHECK(moved > 10u && inscale, "NOTES: %u of 48 steps moved, in the scale %u", moved, inscale);
    }
    CHECK(!memcmp(&e1.t[0].pat, &p0, sizeof p0), "NOTES rewrote the pattern");
    printf("turing: off by default; CHANGE 0 a loop of LENGTH, 100 of twice it; DEST XM %u .. %u; NOTES in the scale, "
           "the pattern untouched\n", xmin, xmax);
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
    test_nav();
    test_course();
    test_modes();
    test_stereo();
    test_alias();
    test_nowhere();
    test_turing();
    test_limiter();
    test_orbit();
    test_xkey();
    test_rec_note();
    test_more();
    test_cue();
    test_tq();
    test_plate();
    test_plate_jobs();
    test_arp_grid();
    test_pingpong();
    test_clock();
    test_gen();
    test_reactor_bypass();
    test_reactor_response();
    test_reactor_pos();
    test_reactor_clock();
    test_reactor_extremes();
    test_reactor_kp();
    test_reactor_ap();
    test_reactor_rest();
    test_reactor_makeup();
    test_reactor_saturation();
    test_distroy();
    test_dynamics();
    test_reactor_r4();
    test_feedback();
    test_space();
    test_looper();
    test_mod();
    test_split_through();
    test_seq();
    test_filter_seq_arp();
    test_names();
    test_switches();
    test_hand();
    test_edge();
    test_env();
    test_silence();
    test_chunking();
    test_timing();
    test_clicks();
    printf(fails ? "sd_test: %d FAILED\n" : "sd_test: all passed\n", fails);
    return fails ? 1 : 0;
}
