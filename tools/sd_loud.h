/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* A patch's loudness as a meter reads it (2026-11-21, the gain staging): tools/sd_gain.c's table and tests/sd_test.c's
 * guard (test_gain_staging) play the same phrase the same way. MONO, four notes (A2 E3 A3 E4) held a second each at
 * velocity 100, a quarter second between, then a second of tail; K-weighted (ITU-R BS.1770: a high shelf +4 dB, a
 * high pass at 38 Hz), no gate. What goes INTO the output limiter (the output over its gain each sub-block):
 *   lufs      over the 6 s;  mmax  its loudest 400 ms (a meter's momentary);  out_lufs  after the limiter
 *   gr_mean, gr_max  the output limiter's work, dB (its mean, its deepest);  dlim_max  DISTROY's safety's deepest
 *   peak      the output's highest sample, dBFS
 * Include after sd_engine.h; it needs <math.h>, <string.h>, <stdlib.h>. */
#ifndef SD_LOUD_H
#define SD_LOUD_H
/* K-weighting (ITU-R BS.1770: a high shelf +4 dB, a high pass at 38 Hz), the RBJ forms at 44.1 kHz */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } Bq;
static double bq(Bq *f, double x) { double y = f->b0 * x + f->z1; f->z1 = f->b1 * x - f->a1 * y + f->z2; f->z2 = f->b2 * x - f->a2 * y; return y; }
static void kw_init(Bq *s, Bq *h)
{
    double A = pow(10.0, 3.99984385397 / 40.0), w = 2.0 * M_PI * 1681.9744509555319 / SD_FS, c = cos(w), al = sin(w) / (2.0 * 0.7071752369554193), a0;
    a0 = (A + 1) - (A - 1) * c + 2 * sqrt(A) * al;
    s->b0 = A * ((A + 1) + (A - 1) * c + 2 * sqrt(A) * al) / a0;
    s->b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
    s->b2 = A * ((A + 1) + (A - 1) * c - 2 * sqrt(A) * al) / a0;
    s->a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
    s->a2 = ((A + 1) - (A - 1) * c - 2 * sqrt(A) * al) / a0;
    w = 2.0 * M_PI * 38.13547087613982 / SD_FS; c = cos(w); al = sin(w) / (2.0 * 0.5003270373253953); a0 = 1 + al;
    h->b0 = (1 + c) / 2 / a0; h->b1 = -(1 + c) / a0; h->b2 = (1 + c) / 2 / a0; h->a1 = -2 * c / a0; h->a2 = (1 - al) / a0;
    s->z1 = s->z2 = h->z1 = h->z2 = 0;
}

typedef struct { double lufs, out_lufs, gr_mean, gr_max, dlim_max, peak, mmax; } Meas;

static Meas sd_loud(SdEngine *e, SdSpaceMem *mem, const SdPatch *pp)
{
    static const uint8_t NOTE[4] = {45, 52, 57, 64};
    Bq s[2], h[2];
    Meas r;
    double e_in = 0, e_out = 0, grs = 0, win[60] = {0}, mmax = -200;   /* (100 ms each: a meter's momentary, 400 ms) */
    uint32_t t, total = 6u * SD_FS, n = 0, subs = 0, i;
    int32_t buf[2 * SD_SUB], gmin = 32768, dmin = 32768;
    int32_t pk = 0;
    int on = -1;
    SdPatch p = *pp;
    sd_engine_init(e);
    memset(mem, 0, sizeof *mem);
    e->sm = mem;
    sd_engine_patch(e, &p);
    kw_init(&s[0], &h[0]); kw_init(&s[1], &h[1]);
    for (t = 0; t < total; t += SD_SUB) {
        uint32_t ms = t * 1000u / SD_FS, k = ms / 1250u;
        int want = k < 4u && ms % 1250u < 1000u ? (int)k : -1;
        int32_t g0 = e->lim_own, g1;           /* (the limiter's own: the gain it applies carries TRIM too) */
        if (want != on) {                       /* a note a second, a quarter second between */
            if (on >= 0) sd_engine_note(e, NOTE[on], 0);
            if (want >= 0) sd_engine_note(e, NOTE[want], 100);
            on = want;
        }
        memset(buf, 0, sizeof buf);             /* (the engine adds into it, as the FM-1's buffer comes zeroed) */
        sd_engine_render(e, buf, SD_SUB);
        g1 = e->lim_own;
        if (g1 < gmin) gmin = g1;
        if (e->d.on && e->d.lg < dmin) dmin = e->d.lg;
        for (i = 0; i < SD_SUB; i++) {
            double g = (g0 + (g1 - g0) * (double)i / SD_SUB) / 32768.0, l = buf[2 * i] / 32768.0, rr = buf[2 * i + 1] / 32768.0;
            double kl, kr;
            int32_t a = abs(buf[2 * i]) > abs(buf[2 * i + 1]) ? abs(buf[2 * i]) : abs(buf[2 * i + 1]);
            if (a > pk) pk = a;
            kl = bq(&h[0], bq(&s[0], l)); kr = bq(&h[1], bq(&s[1], rr));
            e_out += kl * kl + kr * kr;
            e_in += (kl * kl + kr * kr) / (g * g);
            win[(t + i) / 4410u % 60u] += (kl * kl + kr * kr) / (g * g);
            n++;
        }
        grs += -20.0 * log10((g0 + g1) / 65536.0);
        subs++;
    }
    for (i = 3; i < 60u; i++) {
        double m = -0.691 + 10.0 * log10((win[i] + win[i - 1] + win[i - 2] + win[i - 3]) / 17640.0 + 1e-20);
        if (m > mmax) mmax = m;
    }
    r.mmax = mmax;
    r.lufs = -0.691 + 10.0 * log10(e_in / n + 1e-20);
    r.out_lufs = -0.691 + 10.0 * log10(e_out / n + 1e-20);
    r.gr_mean = grs / subs;
    r.gr_max = -20.0 * log10(gmin / 32768.0);
    r.dlim_max = -20.0 * log10(dmin / 32768.0);
    r.peak = 20.0 * log10(pk / 32768.0 + 1e-9);
    return r;
}

#endif
