/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* SINGULARITY recorded for the FM-1's demo (2026-11-19, the user: the performance as a demo song on the device):
 * tools/sd_singularity.c played once, as tests/run_sd.sh renders it, and before each render what its score did to the
 * engine, as firmware/src/sd_demo_fmt.h's events: the patch, the patterns and ORBIT handed over (the bytes that
 * changed; at render 0 all of them), the scalars, the keys, MASTER and what the player says. Writes the header the app
 * plays back from SETUP (firmware/src/sd_demo.h); tests/demo_test.c plays it back against the render.
 *   tools/make_demo.sh                 build, run, write firmware/src/sd_demo.h
 *   sd_demo_rec OUT.h RENDER.wav       (the render, as sd_singularity writes it, too) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_demo_fmt.h"
#define SD_PLAY_REC 1
static void rec_note(SdEngine *en, uint32_t note, uint32_t vel);
#define SD_PLAY_NOTE rec_note
#ifndef DEMO_SRC                                /* which performance (2026-11-21: a second, ARMADA, with the drums) */
#define DEMO_SRC "sd_singularity.c"
#define DEMO_NAME "SINGULARITY"
#define DEMO_PFX "SD_DEMO"
#endif
#define main sing_main
#include DEMO_SRC
#undef main

static uint8_t *ob;                             /* the stream */
static uint32_t on, ocap;
static uint32_t rnow, rlast, r_end;             /* the render about to run, the last event's, the score's end */
static void put8(uint32_t b)
{
    if (on == ocap) { ocap = ocap ? 2u * ocap : 65536u; ob = realloc(ob, ocap); if (!ob) { perror("rec"); exit(1); } }
    ob[on++] = (uint8_t)b;
}
static void putv(uint32_t v) { while (v >= 128u) { put8((v & 127u) | 128u); v >>= 7; } put8(v); }
static void ev(uint32_t op) { putv(rnow - rlast); rlast = rnow; put8(op); }
static void runs(const void *now_, void *last_, uint32_t n, uint32_t op)   /* the bytes that changed, as runs (gaps
                                                   of 3 or fewer equal bytes inside one) */
{
    const uint8_t *a = now_;
    uint8_t *b = last_;
    static uint32_t ro[4096], rl[4096];
    uint32_t i = 0, nr = 0, k, done = 0;
    while (i < n) {
        uint32_t s, last;
        if (a[i] == b[i]) { i++; continue; }
        s = last = i;
        while (i < n && i - s < 255u) { if (a[i] != b[i]) last = i; else if (i - last > 3u) break; i++; }
        ro[nr] = s; rl[nr] = last - s + 1u; nr++;
        i = s + rl[nr - 1u];
    }
    do {                                        /* (255 runs an event at most: more, another handover) */
        uint32_t m = nr - done > 255u ? 255u : nr - done;
        ev(op);
        put8(m);
        for (k = done; k < done + m; k++) {
            uint32_t j;
            put8(ro[k] & 255u); put8(ro[k] >> 8); put8(rl[k]);
            for (j = 0; j < rl[k]; j++) put8(a[ro[k] + j]);
        }
        done += m;
    } while (done < nr);
    memcpy(b, a, n);
}

static void runs_body(const void *now_, void *last_, uint32_t n)   /* runs (as runs()) without the event's head:
                                                   at most 255 (a beat's and its MORE's fit) */
{
    const uint8_t *a = now_;
    uint8_t *b = last_;
    static uint32_t ro[256], rl[256];
    uint32_t i = 0, nr = 0, k;
    while (i < n && nr < 255u) {
        uint32_t s, last;
        if (a[i] == b[i]) { i++; continue; }
        s = last = i;
        while (i < n && i - s < 255u) { if (a[i] != b[i]) last = i; else if (i - last > 3u) break; i++; }
        ro[nr] = s; rl[nr] = last - s + 1u; nr++;
        i = s + rl[nr - 1u];
    }
    if (i < n) { fprintf(stderr, "rec: a beat with more than 255 runs\n"); exit(1); }
    put8(nr);
    for (k = 0; k < nr; k++) {
        uint32_t j;
        put8(ro[k] & 255u); put8(ro[k] >> 8); put8(rl[k]);
        for (j = 0; j < rl[k]; j++) put8(a[ro[k] + j]);
    }
    memcpy(b, a, n);
}
static void beat_runs(SdDrumPat *lp_, SdDrumMore *lm, uint32_t wait)
{
    ev(SD_DO_DBEAT | wait);
    runs_body(&e.dr.pat_in, lp_, sizeof *lp_);
    runs_body(&e.dr.more_in, lm, sizeof *lm);
}

static uint8_t nq[512][2];
static uint32_t nn;
static char sq[64][64];
static uint32_t ns;
static void rec_note(SdEngine *en, uint32_t note, uint32_t vel)
{
    if (nn < 512u) { nq[nn][0] = (uint8_t)note; nq[nn][1] = (uint8_t)vel; nn++; }
    sd_engine_note(en, note, vel);
}
static void rec_say(const char *what)
{
    if (ns < 64u) { strncpy(sq[ns], what, 63); sq[ns][63] = 0; ns++; }
}
static void rec_render(int end)
{
    static SdPatch lp;
    static SdPattern lt[2];
    static SdOrbit lo;
    static SdDrumKit lk;
    static SdDrumPat ldp;
    static SdDrumMore ldm;
    static int32_t ls[SD_DEMO_NF], lg;
    static uint32_t pseq, tseq[2], oseq, dseq;
    static int first = 1, ended;
    uint32_t i, k;
    if (first) {                                /* render 0: everything, as it stands after the player's setup */
        for (i = 0; i < sizeof lp; i++) ((uint8_t *)&lp)[i] = (uint8_t)~((uint8_t *)&e.patch_in)[i];
        for (k = 0; k < 2u; k++) for (i = 0; i < sizeof lt[k]; i++) ((uint8_t *)&lt[k])[i] = (uint8_t)~((uint8_t *)&e.t[k].pat_in)[i];
        for (i = 0; i < sizeof lo; i++) ((uint8_t *)&lo)[i] = (uint8_t)~((uint8_t *)&e.orb_in)[i];
        for (i = 0; i < sizeof lk; i++) ((uint8_t *)&lk)[i] = (uint8_t)~((uint8_t *)&e.dr.kit)[i];
        for (i = 0; i < sizeof ldp; i++) ((uint8_t *)&ldp)[i] = (uint8_t)~((uint8_t *)&e.dr.pat_in)[i];
        for (i = 0; i < sizeof ldm; i++) ((uint8_t *)&ldm)[i] = (uint8_t)~((uint8_t *)&e.dr.more_in)[i];
        for (i = 0; i < SD_DEMO_NF; i++) ls[i] = sd_demo_get(&e, i) ^ 1;
        lg = -1;
    }
    for (i = 0; i < SD_DEMO_NF; i++) {
        int32_t v = sd_demo_get(&e, i);
        if (v == ls[i]) continue;
        ev(SD_DO_SET); put8(i); putv(((uint32_t)v << 1) ^ (uint32_t)(v >> 31));
        ls[i] = v;
    }
    if (first || e.patch_seq != pseq) { pseq = e.patch_seq; runs(&e.patch_in, &lp, sizeof lp, SD_DO_PATCH); }
    for (k = 0; k < 2u; k++)
        if (first || e.t[k].pat_seq != tseq[k]) {
            tseq[k] = e.t[k].pat_seq;
            runs(&e.t[k].pat_in, &lt[k], sizeof lt[k], SD_DO_PAT | k << 4 | (e.t[k].pat_wait ? 32u : 0u));
        }
    if (first || e.orb_seq != oseq) { oseq = e.orb_seq; runs(&e.orb_in, &lo, sizeof lo, SD_DO_ORB | (e.orb_wait ? 32u : 0u)); }
    if (first || memcmp(&e.dr.kit, &lk, sizeof lk)) runs(&e.dr.kit, &lk, sizeof lk, SD_DO_DKIT);   /* (the drums) */
    if (first || e.dr.pat_seq != dseq) {        /* (their beat: the beat's runs, then its MORE's, in one event) */
        dseq = e.dr.pat_seq;
        beat_runs(&ldp, &ldm, e.dr.pat_wait ? 32u : 0u);
    }
    for (i = 0; i < nn; i++) { ev(SD_DO_NOTE); put8(nq[i][0]); put8(nq[i][1]); }
    nn = 0;
    if (master != lg) { ev(SD_DO_GAIN); putv((uint32_t)master); lg = master; }
    for (i = 0; i < ns; i++) {
        uint32_t L = (uint32_t)strlen(sq[i]), j;
        ev(SD_DO_SAY); put8(L);
        for (j = 0; j < L; j++) put8((uint8_t)sq[i][j]);
    }
    ns = 0;
    if (end && !ended) { ended = 1; r_end = rnow; ev(SD_DO_END); }
    first = 0;
    rnow++;
}

int main(int argc, char **argv)
{
    char *av[2];
    FILE *f;
    uint32_t i;
    int r;
    if (argc < 3) { fprintf(stderr, "usage: sd_demo_rec OUT.h RENDER.wav\n"); return 2; }
    av[0] = (char *)"sd_demo_perf"; av[1] = argv[2];
    r = sing_main(2, av);
    if (r) return r;
    f = fopen(argv[1], "w");
    if (!f) { perror(argv[1]); return 1; }
    fprintf(f, "/* SPDX-License-Identifier: GPL-3.0-only */\n"
               "/* Generated by tools/sd_demo_rec.c (tools/make_demo.sh) from tools/%s: never edit. %s\n"
               " * as the FM-1 plays it back from SETUP: %u renders of %u frames (%.1f s with its tail), the score's end at\n"
               " * render %u; firmware/src/sd_demo_fmt.h's events, %u B. The includer defines SD_DEMO_SECTION */\n",
            DEMO_SRC, DEMO_NAME, rnow, BLOCK, rnow * (double)BLOCK / SD_FS, r_end, on);
    fprintf(f, "#define %s_NAME \"%s\"\n#define %s_BPM %uu\n#define %s_SETTLE %uu\n"
               "#define %s_RENDERS %uu\n#define %s_END %uu\n#define %s_LEN %uu\n",
            DEMO_PFX, DEMO_NAME, DEMO_PFX, bpm, DEMO_PFX, SD_PLAY_SETTLE, DEMO_PFX, rnow, DEMO_PFX, r_end, DEMO_PFX, on);
    fprintf(f, "_Static_assert(sizeof(SdPatch) == %uu && sizeof(SdPattern) == %uu && sizeof(SdOrbit) == %uu &&\n"
               "               SD_DEMO_NF == %uu, \"the recording's structs and scalars are these\");\n",
            (unsigned)sizeof(SdPatch), (unsigned)sizeof(SdPattern), (unsigned)sizeof(SdOrbit), SD_DEMO_NF);
    fprintf(f, "static const uint8_t %s[%s_LEN] SD_DEMO_SECTION = {", DEMO_PFX, DEMO_PFX);
    for (i = 0; i < on; i++) fprintf(f, "%s%u,", i % 24u ? "" : "\n    ", ob[i]);
    fprintf(f, "\n};\n");
    fclose(f);
    printf("demo: %u renders (the score's end at %u), %u B of events -> %s\n", rnow, r_end, on, argv[1]);
    return 0;
}
