/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The FM-1's demo played back (firmware/src/sd_demo.h through sd_demo_fmt.h's player, as the app does from SETUP)
 * on an engine set up as the performance's (tools/sd_play.h's play_run), against the performance's render
 * (build/audio/singularity/star_distroyer_singularity.wav): sample for sample, MASTER as the player turned it.
 *   demo_test RENDER.wav            (run by tests/run_sd.sh) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_demo_fmt.h"
#define SD_DEMO_SECTION
#ifdef DEMO2                                    /* the second (2026-11-21, ARMADA): sd_demo2.h's, the same names */
#include "sd_demo2.h"
#define SD_DEMO SD_DEMO2
#define SD_DEMO_NAME SD_DEMO2_NAME
#define SD_DEMO_LEN SD_DEMO2_LEN
#define SD_DEMO_SETTLE SD_DEMO2_SETTLE
#define SD_DEMO_END SD_DEMO2_END
#define SD_DEMO_RENDERS SD_DEMO2_RENDERS
#else
#include "sd_demo.h"
#endif

int main(int argc, char **argv)
{
    static SdEngine e;
    static SdSpaceMem sm;
    static SdRoomMem frm;
    static uint8_t lp[24576];
    static int16_t dob[2u * SD_DR_BATCH * SD_SUB];
    static int32_t buf[2u * 256u];
    SdDemoPlay d;
    FILE *f;
    int16_t *w;
    long size;
    uint32_t r, i, ns, at = 0, differ = 0, first = 0xFFFFFFFFu, ended = 0, end_r = 0;
    int32_t worst = 0;
    if (argc < 2) { fprintf(stderr, "usage: demo_test RENDER.wav\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 44, SEEK_SET);
    ns = (uint32_t)((size - 44) / 2);
    w = malloc((size_t)ns * 2u);
    if (!w || fread(w, 2, ns, f) != ns) { fprintf(stderr, "demo_test: %s unreadable\n", argv[1]); return 1; }
    fclose(f);
    sd_engine_init(&e);
    e.sm = &sm;
    e.frm = &frm;
    sd_engine_looper_borrow(&e, lp, sizeof lp);
    e.dr.ob = dob;                              /* (the drums' batch, as the app has it) */
    sd_demo_begin(&d, SD_DEMO, SD_DEMO_LEN);
    for (r = 0; at < ns; r++) {
        if (sd_demo_events(&d, &e, r) && !ended) { ended = 1; end_r = r; }
        memset(buf, 0, sizeof buf);
        sd_engine_render(&e, buf, 256u);
        if (r < SD_DEMO_SETTLE) continue;
        for (i = 0; i < 512u && at < ns; i++, at++) {
            int32_t m = (buf[i] * d.gain) >> 12, s = m > 32767 ? 32767 : m < -32767 ? -32767 : m, dv = s - w[at];
            if (dv) { differ++; if (first == 0xFFFFFFFFu) first = at; if (abs(dv) > worst) worst = abs(dv); }
        }
    }
    if (differ || !ended || end_r != SD_DEMO_END || r != SD_DEMO_RENDERS) {
        printf("FAIL  the demo played back: %u of %u samples differ (the first at %.3f s, by up to %d); the score's end "
               "%s at render %u (want %u); %u renders (want %u)\n", differ, ns, first / 2.0 / SD_FS, worst,
               ended ? "seen" : "not seen", end_r, SD_DEMO_END, r, SD_DEMO_RENDERS);
        return 1;
    }
    printf("demo: %s played back from its %u B of events, %.1f s, the render sample for sample (%u said)\n",
           SD_DEMO_NAME, SD_DEMO_LEN, ns / 2.0 / SD_FS, d.says);
    return 0;
}
