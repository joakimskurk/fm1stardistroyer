/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* GAIN: the level each knob that shapes the sound moves (2026-11-21, the user: "huge differences in level when some
 * parameters change; it is heard in the volume, and as extreme ducking on the master").
 *
 *   sd_gain [PATCH|all] [PARAM]     a table: for each patch and knob, the loudness at five settings across the knob's
 *                                   range (0 25 50 75 100 %), the level span, and what the limiters took
 *
 * The same phrase on every patch (MONO, four notes held a second each, velocity 100, then a second of tail), its
 * loudness measured as a meter does (K-weighted, LUFS, no gate) on what goes INTO the output limiter (the output over
 * the limiter's gain at each sub-block), and the limiters' work: the output's (its mean and its deepest, dB) and
 * DISTROY's own safety (its deepest). A knob is set as its page sets it (all four bands for the DISTROY knobs that
 * have no band, each band alone for DRIVE); everything else as the patch has it. */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_patches.h"

static SdEngine eng;
static SdSpaceMem spmem;

typedef struct { const char *name; size_t off; int type, n; int32_t lo, hi; } Knob;   /* type 0 u16, 1 i16, 2 u8; n
                                                   fields in a row (the four bands) */
#define KU(nm, f, lo, hi) {nm, offsetof(SdPatch, f), 0, 1, lo, hi}
#define KI(nm, f, lo, hi) {nm, offsetof(SdPatch, f), 1, 1, lo, hi}
#define K8(nm, f, lo, hi) {nm, offsetof(SdPatch, f), 2, 1, lo, hi}
#define KU4(nm, f, lo, hi) {nm, offsetof(SdPatch, f), 0, 4, lo, hi}
#define KI4(nm, f, lo, hi) {nm, offsetof(SdPatch, f), 1, 4, lo, hi}
#define K84(nm, f, lo, hi) {nm, offsetof(SdPatch, f), 2, 4, lo, hi}
static const Knob KNOB[] = {
    KU("XM", xm, 0, 65535), KU("WARP", warp, 0, 65535), KU("FOLD", fold, 0, 65535), KU("DT", dt, 0, 65535),
    KU("NAV X", x, 0, 65535), KU("NAV Y", y, 0, 65535), KU("WIDTH", width, 0, 65535),
    KU("R RES", r_res, 0, 65535), KU("R DRIVE", r_drive, 0, 65535), KU("R MIX", r_mix, 0, 65535),
    KU("R POS", r_pos, 0, 1535), KI("R FREQ", r_freq, -24 * 256, 24 * 256), K8("R MODEL", r_model, 0, 5),
    KU("D DRIVE 1", d_gain[0], 0, 65535), KU("D DRIVE 2", d_gain[1], 0, 65535), KU("D DRIVE 3", d_gain[2], 0, 65535),
    KU("D DRIVE 4", d_gain[3], 0, 65535), KU4("D DRIVE all", d_gain, 0, 65535),
    K84("D TYPE all", d_type, 0, 4), KU4("D ALT all", d_alt, 0, 65535), KU4("D LEVEL all", d_level, 0, 65535),
    KI4("D BIAS all", d_bias, -32767, 32767), KI4("D SHAPE all", d_shape, -32767, 32767), KU4("D BODY all", d_body, 0, 65535),
    KU("D FDBK", d_fb, 0, 65535), KU("D TAME", d_tame, 0, 65535), KU("D MIX", d_mix, 0, 65535),
    KU("F CUTOFF", f_cut, 0, 65535), KU("F RES", f_res, 0, 65535),
    KU("S D-LEVEL", s_dlevel, 0, 65535), KU("S FDBK", s_fb, 0, 65535), KU("S R-LEVEL", s_rlevel, 0, 65535),
    KU("S DECAY", s_decay, 0, 65535), KU("S SIZE", s_size, 0, 65535),
    KU("MUTATE", mac[0], 0, 65535), KU("MOVE", mac[1], 0, 65535), KU("BREAK", mac[2], 0, 65535), KU("MACRO 4", mac[3], 0, 65535),
};
#define NKNOB (sizeof KNOB / sizeof KNOB[0])

static void knob_set(SdPatch *p, const Knob *k, double t)
{
    int32_t v = k->lo + (int32_t)lround((k->hi - k->lo) * t);
    int i;
    for (i = 0; i < k->n; i++) {
        uint8_t *a = (uint8_t *)p + k->off;
        if (k->type == 0) ((uint16_t *)a)[i] = (uint16_t)v;
        else if (k->type == 1) ((int16_t *)a)[i] = (int16_t)v;
        else a[i] = (uint8_t)v;
    }
}

#include "../tools/sd_loud.h"
static Meas play(const SdPatch *p) { return sd_loud(&eng, &spmem, p); }

static SdPatch base(uint32_t i)                  /* as the app loads it: the filter on at 12 kHz if the patch never set it */
{
    SdPatch p = SD_PATCH[i];
    if (p.f_type == SD_FT_OFF && !p.f_cut && !p.f_res && !p.f_env && !p.f_vel && !p.f_key) { p.f_type = SD_FT_LP12; p.f_cut = SD_FCUT_OPEN; }
#ifdef SD_MK_D
    sd_patch_levels(&p);                        /* (as the app loads it: onto the make-ups; a knob then turns that) */
#endif
    return p;
}

int main(int argc, char **argv)
{
    static const double AT[5] = {0, 0.25, 0.5, 0.75, 1.0};
    uint32_t i0 = 0, i1 = SD_PATCHES, i, k, j;
    if (argc > 1 && strcmp(argv[1], "all")) {
        for (i = 0; i < SD_PATCHES && strcasecmp(argv[1], SD_PATCH_NAME[i]); i++) ;
        if (i == SD_PATCHES) i = (uint32_t)atoi(argv[1]) % SD_PATCHES;
        i0 = i; i1 = i + 1u;
    }
    printf("patch\tknob\tas set\t0%%\t25%%\t50%%\t75%%\t100%%\tspan\tGR mean max\tGR deepest\tDISTROY lim deepest\tpeak out\n");
    for (i = i0; i < i1; i++) {
        SdPatch p0 = base(i);
        Meas m0 = play(&p0);
        printf("%s\t(as set)\t%.1f\t%.1f\t\t\t\t\t\t%.1f\t%.1f\t%.1f\t%.1f\n", SD_PATCH_NAME[i], m0.lufs, m0.mmax, m0.gr_mean, m0.gr_max, m0.dlim_max, m0.peak);
        for (k = 0; k < NKNOB; k++) {
            double lo = 1e9, hi = -1e9, grm = 0, grx = 0, dl = 0, pkx = -99;
            Meas m[5];
            if (argc > 2 && strncasecmp(argv[2], KNOB[k].name, strlen(argv[2]))) continue;   /* (a name, or its start) */
            for (j = 0; j < 5u; j++) {
                SdPatch p = p0;
                knob_set(&p, &KNOB[k], AT[j]);
                m[j] = play(&p);
                if (m[j].lufs < lo) lo = m[j].lufs;
                if (m[j].lufs > hi) hi = m[j].lufs;
                if (m[j].gr_mean > grm) grm = m[j].gr_mean;
                if (m[j].gr_max > grx) grx = m[j].gr_max;
                if (m[j].dlim_max > dl) dl = m[j].dlim_max;
                if (m[j].peak > pkx) pkx = m[j].peak;
            }
            printf("%s\t%s\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\n", SD_PATCH_NAME[i], KNOB[k].name, m0.lufs,
                   m[0].lufs, m[1].lufs, m[2].lufs, m[3].lufs, m[4].lufs, hi - lo, grm, grx, dl, pkx);
            fflush(stdout);
        }
    }
    return 0;
}
