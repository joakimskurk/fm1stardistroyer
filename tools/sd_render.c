/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound engine (firmware/src/sd_engine.h) on this computer, without the platform (Sound Plan v0.1 §1).
 *
 *   sd_render render OUT.wav [SECONDS] [BPM] [PATCH]   the 16-step loop -> a stereo 16-bit WAV, as the FM-1 renders
 *                                              it (256-frame interrupts, Dinghy's clip at +-32767). A PATCH is its
 *                                              name (SD_PATCH_NAME, any case: distroy) or its number from 0
 *   sd_render hold OUT.wav PATCH [NOTE] [cmp]  one note held 2 s and let ring 2 s (cmp: DESTROY's complementary
 *                                              crossover instead of the patch's, D1's A/B)
 *   sd_render field OUT.wav [SECONDS]          VOYAGE held, X/Y round the field's edge and across it (Gate D VOICE)
 *   sd_render mode poly|duo OUT.wav PATCH      POLY: a chord C E G B; DUO: two notes a fifth apart
 *   sd_render sweep wp|fd|xm|dt|pos OUT.wav [SECONDS] [NOTE]   a held note, one control 0 -> max: WP and FD on one
 *                                              oscillator, XM on a 1:1 pair, DT on four carriers, the Reactor's POS
 *                                              on VOWEL (stage 1 -> 6, its clock off)
 *   sd_render macros OUT.wav PATCH             C1: the loop at 110 BPM, each macro in turn (MUTATE MOVE BREAK MACRO 4)
 *                                              up 0 -> 99 over 4.8 s, held 1.2 s, back to 0 in 1.2 s: ~29 s
 *   sd_render pattern OUT.wav PATCH [MUTATE]   the sequencer at 110 BPM, ~35 s: a 32-step line in segments (A x2, B
 *                                              reversed, C ping-pong, D shuffled), ratchets, PROB, SLIDE and TIE;
 *                                              MUTATE (0..100, 0 off) rewrites it at each loop's one
 *   sd_render evolve OUT.wav PATCH [SECONDS]   the synth's essence (the user: "something that evolves … I want to
 *                                              hear the sonic spectrum"): a slow line in long tied notes at 76 BPM,
 *                                              quarters, while the four macros step through the patch's range: a
 *                                              random walk, a new place for each on its own bar count (1, 2, 3, 4
 *                                              bars), glided ~80 ms ("all modulation must add something rhythmic")
 *   sd_render tour DIR                         ten patches, each its own phrase (tempo, register, rhythm, DIV), its
 *                                              macros stepping their own way: DIR/tour_NAME.wav each (~22 s) and
 *                                              DIR/tour.wav, all in a row
 *   sd_render bench [SECONDS]                  the worst case this build has, timed: ns a frame, x real time
 *
 * tests/run_sd.sh builds and runs both. The bench times this computer, not the chip: it ranks the blocks and shows
 * a change's cost; fm1-emu (tools/emu_cpu.sh) and the CPU readout on the FM-1 give the chip's numbers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "sd_engine.h"

#define HALF 256u                               /* frames per audio interrupt on Dinghy */

static uint32_t patch_arg(const char *a)        /* a patch by name, any case, or by number */
{
    uint32_t i, k;
    for (i = 0; i < SD_PATCHES; i++) {
        for (k = 0; a[k] && SD_PATCH_NAME[i][k] && (a[k] & ~0x20) == SD_PATCH_NAME[i][k]; k++) {}
        if (!a[k] && !SD_PATCH_NAME[i][k]) return i;
    }
    if (a[0] < '0' || a[0] > '9') { fprintf(stderr, "sd_render: no patch %s\n", a); exit(2); }
    return (uint32_t)atoi(a) % SD_PATCHES;
}

static SdEngine eng;
static SdSpaceMem spmem;                        /* SPACE's memory (the pool, on the FM-1) */
#define SD_INIT(e) (sd_engine_init(e), memset(&spmem, 0, sizeof spmem), (e)->sm = &spmem)

static void put16(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFFu); put16(f, v >> 16); }

static FILE *wav_open(const char *path, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t bytes = frames * 4u;
    if (!f) { perror(path); return 0; }
    fwrite("RIFF", 1, 4, f); put32(f, 36u + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SD_FS); put32(f, SD_FS * 4u); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    return f;
}

static void wav_put(FILE *f, const int32_t *buf, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < 2u * n; i++) {
        int32_t s = buf[i] > 32767 ? 32767 : buf[i] < -32767 ? -32767 : buf[i];
        put16(f, (uint32_t)(uint16_t)(int16_t)s);
    }
}

static void no_dyn(SdPatch *p)                  /* SD_NODYN=1: DESTROY's dynamics off (D2's A/B) */
{
    const char *v = getenv("SD_NODYN");
    if (v && *v == '1') memset(p->d_shape, 0, sizeof p->d_shape);
    v = getenv("SD_NOFB");                      /* SD_NOFB=1: DESTROY's feedback and TAME off (D3's A/B) */
    if (v && *v == '1') p->d_fb = p->d_tame = 0;
}

static int render(const char *path, uint32_t secs, uint32_t bpm, uint32_t patch)
{
    uint32_t frames = secs * SD_FS, done = 0;
    int32_t buf[2u * HALF];
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    SD_INIT(&eng);
    {
        SdPatch p = SD_PATCH[patch % SD_PATCHES];
        no_dyn(&p);
        sd_engine_patch(&eng, &p);
    }
    eng.bpm = (uint16_t)bpm;
    eng.seq_run = 1;
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF;
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("render -> %s (%u s, %u BPM, %s)\n", path, secs, bpm, SD_PATCH_NAME[patch % SD_PATCHES]);
    return 0;
}

static int xover_cmp;                           /* hold ... cmp: DESTROY's complementary crossover (D1's A/B) */

static int hold(const char *path, uint32_t patch, uint32_t note)
{
    uint32_t frames = 4u * SD_FS, done = 0;
    int32_t buf[2u * HALF];
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    {
        SdPatch p = SD_PATCH[patch % SD_PATCHES];
        if (xover_cmp) p.d_xover = SD_DX_CMP;
        no_dyn(&p);
        SD_INIT(&eng);
        sd_engine_patch(&eng, &p);
    }
    sd_engine_note(&eng, note, 110);
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF;
        if (done < 2u * SD_FS && done + n >= 2u * SD_FS) sd_engine_note(&eng, note, 0);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("hold -> %s (%s, note %u)\n", path, SD_PATCH_NAME[patch % SD_PATCHES], note);
    return 0;
}

/* A held note with one parameter swept 0 -> max over the render (set once an interrupt, as the knobs would). The
 * envelopes are the clean patch's: what changes is the parameter alone. */
static int sweep(const char *what, const char *path, uint32_t secs, uint32_t note)
{
    uint32_t frames = secs * SD_FS, done = 0;
    int32_t buf[2u * HALF];
    FILE *f;
    SdPatch p = SD_PATCH_SINE;
    if (strcmp(what, "wp") && strcmp(what, "fd") && strcmp(what, "xm") && strcmp(what, "dt") && strcmp(what, "pos")) {
        fprintf(stderr, "sweep: wp, fd, xm, dt or pos\n");
        return 2;
    }
    if (!(f = wav_open(path, frames))) return 1;
    SD_INIT(&eng);
    if (!strcmp(what, "xm")) {                  /* a 1:1 pair: oscillator 1 modulates oscillator 2 */
        p.bank[0].osc[0] = (SdOscPatch){1, 0, SD_ROLE_S, 0, 65535};
        p.bank[0].osc[1] = (SdOscPatch){1, 0, SD_ROLE_C, 0, 65535};
    } else if (!strcmp(what, "pos")) {          /* VOWEL held, its clock off: POS from stage 1 through 6, a glide */
        p = SD_PATCH[SD_P_VOWEL];
        p.r_div = 0;
        p.r_slew = 20000;
        p.e1_s = 127;
    } else if (!strcmp(what, "dt")) {           /* four carriers on the fundamental: the spread beats */
        uint32_t k;
        for (k = 0; k < SD_OPS; k++) p.bank[0].osc[k] = (SdOscPatch){1, 0, SD_ROLE_C, 0, 16000};
    }
    sd_engine_note(&eng, note, 110);
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF;
        uint16_t x = (uint16_t)((uint64_t)done * 65535u / frames);
        if (!strcmp(what, "wp")) p.warp = x;
        else if (!strcmp(what, "fd")) p.fold = x;
        else if (!strcmp(what, "xm")) p.xm = x;
        else if (!strcmp(what, "pos")) p.r_pos = (uint16_t)((uint32_t)x * 1535u / 65535u);
        else p.dt = x;
        sd_engine_patch(&eng, &p);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("sweep %s -> %s (%u s, note %u)\n", what, path, secs, note);
    return 0;
}

static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* VOYAGE held, COURSE off, X/Y walked round the field's edge A -> B -> C -> D -> A and through the centre */
static int field(const char *path, uint32_t secs)
{
    uint32_t frames = secs * SD_FS, done = 0;
    int32_t buf[2u * HALF];
    SdPatch p = SD_PATCH[SD_P_VOYAGE];
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    p.course = 0;
    p.e1_s = 127;
    SD_INIT(&eng);
    sd_engine_note(&eng, 45, 110);
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF, t = (uint32_t)((uint64_t)done * 5u * 65536u / frames);
        uint32_t leg = t >> 16, fr = t & 0xFFFFu;   /* five legs: A->B, B->C, C->D, D->A, A->C (the diagonal) */
        static const uint16_t X[6] = {0, 65535, 65535, 0, 0, 65535}, Y[6] = {65535, 65535, 0, 0, 65535, 0};
        p.x = (uint16_t)(X[leg] + (((int32_t)X[leg + 1u] - (int32_t)X[leg]) * (int32_t)fr >> 16));
        p.y = (uint16_t)(Y[leg] + (((int32_t)Y[leg + 1u] - (int32_t)Y[leg]) * (int32_t)fr >> 16));
        sd_engine_patch(&eng, &p);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("field -> %s (%u s: A B C D A, then the diagonal A -> C)\n", path, secs);
    return 0;
}

/* A mode: POLY plays a chord C E G B (bank A of PATCH on four voices); DUO two notes a fifth apart, X at 0.5 */
static int mode(const char *path, uint32_t m, uint32_t patch)
{
    static const uint8_t CH[4] = {48, 52, 55, 59};
    uint32_t frames = 5u * SD_FS, done = 0, i;
    int32_t buf[2u * HALF];
    SdPatch p = SD_PATCH[patch % SD_PATCHES];
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    SD_INIT(&eng);
    eng.mode = (uint8_t)m;
    p.x = 32768;
    sd_engine_patch(&eng, &p);
    memset(buf, 0, sizeof buf);
    sd_engine_render(&eng, buf, 64);            /* the mode switches at a tick, while quiet */
    for (i = 0; i < (m == SD_MODE_POLY ? 4u : 2u); i++) sd_engine_note(&eng, m == SD_MODE_POLY ? CH[i] : 48u + 7u * i, 100);
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF;
        if (done < 3u * SD_FS && done + n >= 3u * SD_FS)
            for (i = 0; i < 4u; i++) sd_engine_note(&eng, m == SD_MODE_POLY ? CH[i] : 48u + 7u * i, 0);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("mode %s -> %s (%s)\n", m == SD_MODE_POLY ? "POLY" : "DUO", path, SD_PATCH_NAME[patch % SD_PATCHES]);
    return 0;
}

static int bench(uint32_t secs)
{
    uint32_t frames = secs * SD_FS, done = 0;
    int32_t buf[2u * HALF];
    volatile int32_t sink = 0;
    double t0, dt;
    {                                           /* GROWL, every stage on (squash, fold) and held: the bank's worst */
        SdPatch p = SD_PATCH[SD_PATCH_WORST];
        p.e1_s = 127;
        SD_INIT(&eng);
        sd_engine_patch(&eng, &p);
    }
    sd_engine_note(&eng, 57, 100);              /* one held note: the worst case grows with V3..D3 */
    t0 = now_s();
    while (done < frames) {
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, HALF);
        sink += buf[0];
        done += HALF;
    }
    dt = now_s() - t0;
    printf("bench: %u s of audio in %.3f s: %.1f ns a frame, %.0f x real time (this computer)\n", secs, dt,
           dt * 1e9 / (double)frames, (double)secs / dt);
    return 0;
}

/* Scenes (R4): a patch beyond the 16-step loop, at 100 BPM (a sixteenth 6615 frames): a bass riff low (MONO), a
 * slow melody in the middle (MONO), four chords held (POLY), a fast arpeggio over two octaves (MONO); a bar's rest
 * between them. Events: the sixteenth it starts, the note, its length in sixteenths. */
typedef struct { uint16_t at; uint8_t note, len; } SdEv;
#define SC_SIX 6615u
static const SdEv SC_BASS[] = {{0,38,1},{2,38,1},{4,41,1},{6,43,1},{8,45,1},{10,43,1},{12,41,1},{14,36,1},
                               {16,38,1},{18,38,1},{20,41,1},{22,43,1},{24,45,1},{26,43,1},{28,41,1},{30,36,2}};
static const SdEv SC_LEAD[] = {{0,62,4},{4,65,2},{6,69,6},{12,67,2},{14,65,4},{18,64,2},{20,62,8}};
static const SdEv SC_PAD[] = {{0,50,7},{0,53,7},{0,57,7},{0,62,7},{8,46,7},{8,50,7},{8,53,7},{8,58,7},
                              {16,45,7},{16,48,7},{16,53,7},{16,57,7},{24,48,7},{24,52,7},{24,55,7},{24,60,7}};
static const SdEv SC_ARP[] = {{0,50,1},{1,53,1},{2,57,1},{3,60,1},{4,64,1},{5,62,1},{6,57,1},{7,53,1},
                              {8,50,1},{9,53,1},{10,57,1},{11,60,1},{12,64,1},{13,62,1},{14,57,1},{15,53,1},
                              {16,46,1},{17,50,1},{18,53,1},{19,58,1},{20,62,1},{21,58,1},{22,53,1},{23,50,1},
                              {24,45,1},{25,48,1},{26,53,1},{27,57,1},{28,60,1},{29,57,1},{30,53,1},{31,48,1}};
static int scene(const char *path, uint32_t patch)
{
    static const struct { const SdEv *ev; uint32_t n, mode, start; } SC[4] = {
        {SC_BASS, sizeof SC_BASS / sizeof SC_BASS[0], SD_MODE_MONO, 0},
        {SC_LEAD, sizeof SC_LEAD / sizeof SC_LEAD[0], SD_MODE_MONO, 48},
        {SC_PAD, sizeof SC_PAD / sizeof SC_PAD[0], SD_MODE_POLY, 96},
        {SC_ARP, sizeof SC_ARP / sizeof SC_ARP[0], SD_MODE_MONO, 144}};
    uint32_t frames = 192u * SC_SIX, done = 0, k, i;
    int32_t buf[2u * HALF];
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    {
        SdPatch p = SD_PATCH[patch % SD_PATCHES];
        no_dyn(&p);
        SD_INIT(&eng);
        sd_engine_patch(&eng, &p);
    }
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF;
        for (k = 0; k < 4u; k++) {              /* the events that fall in this interrupt, at its start */
            uint32_t base = SC[k].start * SC_SIX;
            if (done <= base && base < done + n) eng.mode = (uint8_t)SC[k].mode;
            for (i = 0; i < SC[k].n; i++) {
                uint32_t on = base + SC[k].ev[i].at * SC_SIX, off = on + SC[k].ev[i].len * SC_SIX - SC_SIX / 4u;
                if (done <= on && on < done + n) sd_engine_note(&eng, SC[k].ev[i].note, 100);
                if (done <= off && off < done + n) sd_engine_note(&eng, SC[k].ev[i].note, 0);
            }
        }
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("scene -> %s (%s)\n", path, SD_PATCH_NAME[patch % SD_PATCHES]);
    return 0;
}

static int macros(const char *path, uint32_t patch)
{
    const uint32_t seg = 7u * SD_FS + SD_FS / 5u, frames = 4u * seg;   /* 7.2 s a macro */
    uint32_t done = 0;
    int32_t buf[2u * HALF];
    SdPatch p = SD_PATCH[patch % SD_PATCHES];
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    SD_INIT(&eng);
    sd_engine_patch(&eng, &p);
    eng.bpm = 110;
    eng.seq_run = 1;
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF, k = done / seg, t = done % seg, v;
        v = t < 24u * SD_FS / 5u ? t * 65535u / (24u * SD_FS / 5u)                    /* up */
          : t < 6u * SD_FS ? 65535u                                                  /* held */
          : (seg - t) * 65535u / (seg - 6u * SD_FS);                                 /* down */
        p.mac[0] = p.mac[1] = p.mac[2] = p.mac[3] = 0;
        p.mac[k] = (uint16_t)(v > 65535u ? 65535u : v);
        sd_engine_patch(&eng, &p);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("macros -> %s (%s: MUTATE, MOVE, BREAK, %s)\n", path, SD_PATCH_NAME[patch % SD_PATCHES],
           p.mac4 == SD_M4_AIR ? "AIR" : p.mac4 == SD_M4_DROWN ? "DROWN" : "BODY");
    return 0;
}

static int pattern(const char *path, uint32_t patch, uint32_t mut)
{
    static const int8_t LINE[32] = {0, 0, 12, 7, 10, 7, 3, 5, 0, 12, 15, 12, 10, 7, 5, 3,
                                    0, 7, 0, 10, 0, 12, 0, 15, 17, 15, 12, 10, 7, 10, 12, 15};
    uint32_t frames = 35u * SD_FS, done = 0, k;
    int32_t buf[2u * HALF];
    SdPattern q;
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    sd_pattern_default(&q);
    q.len = 32;
    for (k = 0; k < 32u; k++) {
        SdStep *st = &q.step[k];
        st->note = (uint8_t)(38 + LINE[k]);     /* D2 */
        st->type = SD_ST_NOTE;
        st->vel = k % 4u == 0u ? 118u : 88u;
        st->gate = 50;
    }
    q.step[3].type = SD_ST_REST; q.step[6].type = SD_ST_TIE; q.step[11].type = SD_ST_SLIDE;
    q.step[1].ratchet = 2; q.step[9].ratchet = 3; q.step[14].ratchet = 2; q.step[27].ratchet = 3;
    q.step[5].prob = 60; q.step[13].prob = 50; q.step[21].prob = 50; q.step[29].prob = 70;
    q.step[18].type = SD_ST_SLIDE; q.step[22].type = SD_ST_TIE; q.step[30].type = SD_ST_SLIDE;
    q.seg_count = 4; q.seg_split[0] = 8; q.seg_split[1] = 16; q.seg_split[2] = 24;
    q.seg_rep[0] = 2; q.seg_order[1] = SD_SO_REV; q.seg_order[2] = SD_SO_PP; q.seg_order[3] = SD_SO_RND;
    q.mutate = (uint8_t)(mut > 100u ? 100u : mut);
    if (q.mutate) q.tur |= 1u;                  /* TURING on */
    SD_INIT(&eng);
    sd_engine_patch(&eng, &SD_PATCH[patch % SD_PATCHES]);
    sd_engine_pattern(&eng, &q);
    eng.bpm = 110;
    eng.seq_run = 1;
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF;
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("pattern -> %s (%s, MUTATE %u %%)\n", path, SD_PATCH_NAME[patch % SD_PATCHES], q.mutate);
    return 0;
}

static int evolve(const char *path, uint32_t patch, uint32_t secs)
{   /* D minor, a step a quarter: notes held through ties and slid into, a rest now and then to breathe */
    static const int8_t LINE[16] = {0, 0, 0, 7, 3, 3, 10, 10, 5, 5, 5, 12, 7, 7, 3, 2};
    static const uint8_t TYPE[16] = {SD_ST_NOTE, SD_ST_TIE, SD_ST_TIE, SD_ST_SLIDE, SD_ST_NOTE, SD_ST_TIE, SD_ST_NOTE,
                                     SD_ST_TIE, SD_ST_NOTE, SD_ST_TIE, SD_ST_TIE, SD_ST_SLIDE, SD_ST_NOTE, SD_ST_TIE,
                                     SD_ST_SLIDE, SD_ST_REST};
    uint32_t frames = secs * SD_FS, done = 0, k, bar_len = SD_FS * 60u * 4u / 76u, rnd = 0x5EED5u;
    int32_t buf[2u * HALF];
    double tgt[4] = {0, 0.2, 0, 0.1}, cur[4] = {0, 0.2, 0, 0.1};
    static const uint32_t EVERY[4] = {1, 2, 3, 4};   /* bars between a macro's steps: they meet now and then */
    SdPattern q;
    SdPatch p = SD_PATCH[patch % SD_PATCHES];
    FILE *f = wav_open(path, frames);
    if (!f) return 1;
    sd_pattern_default(&q);
    for (k = 0; k < 16u; k++) {
        q.step[k].note = (uint8_t)(38 + LINE[k]);   /* D2 */
        q.step[k].type = TYPE[k];
        q.step[k].vel = 100;
        q.step[k].gate = 96;
    }
    q.div = 5;                                  /* a quarter */
    q.seg_count = 1;
    q.scale = SD_SC_MINOR;
    SD_INIT(&eng);
    sd_engine_pattern(&eng, &q);
    eng.bpm = 76;
    eng.seq_run = 1;
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF;
        double a = (double)done / frames, arc = sin(M_PI * fmin(1.0, a * 1.15));   /* rises to 2/3, settles */
        if (done % bar_len < n) {               /* a bar's one: the macros due take a step of a random walk */
            uint32_t bar = done / bar_len;
            for (k = 0; k < 4u; k++)
                if (bar % EVERY[k] == 0u) {
                    rnd = rnd * 1103515245u + 12345u;
                    tgt[k] = fmax(0, fmin(1, tgt[k] + ((double)(rnd >> 8 & 0xFFFF) / 65535.0 - 0.45) * 0.5));
                }
        }
        for (k = 0; k < 4u; k++) {              /* glided ~80 ms; MUTATE and BREAK under the arc */
            double want = k == 0u || k == 2u ? tgt[k] * arc : tgt[k];
            cur[k] += (want - cur[k]) * 0.07;
            p.mac[k] = (uint16_t)(65535 * fmax(0, fmin(1, cur[k])));
        }
        sd_engine_patch(&eng, &p);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        wav_put(f, buf, n);
        done += n;
    }
    fclose(f);
    printf("evolve -> %s (%s, %u s)\n", path, SD_PATCH_NAME[patch % SD_PATCHES], secs);
    return 0;
}

/* ---- the tour: a phrase a patch. A line is notes as semitones over a root (-99 a rest, +100 a tie, +200 a slide
 * into it); POLY parts play chords from the render instead of the sequencer */
#define TIE_ 100
#define SLD_ 200
#define RST_ (-99)
typedef struct {
    const char *name;
    uint32_t patch, bpm, div, root, len, mode;  /* mode: SD_MODE_*; POLY: chords */
    int16_t line[32];
    uint8_t rat[32], prob[32];                  /* ratchets, chances (0: 100) */
    uint8_t gate;
    uint8_t mac_every[4];                       /* bars between a macro's steps (0: it stays at mac_lo) */
    uint8_t mac_lo[4], mac_hi[4];               /* its range, 0..99 */
} TourPart;
static const TourPart TOUR_PARTS[] = {
    {"voyage", SD_P_VOYAGE, 76, 5, 38, 16, SD_MODE_MONO, {0, TIE_, TIE_, SLD_ + 7, 3, TIE_, 10, TIE_, 5, TIE_, TIE_, SLD_ + 12, 7, TIE_, SLD_ + 3, RST_},
     {0}, {0}, 96, {3, 1, 0, 2}, {0, 20, 0, 10}, {30, 80, 0, 60}},
    {"comet", SD_P_COMET, 112, 2, 38, 16, SD_MODE_MONO, {0, RST_, 12, 0, SLD_ + 3, RST_, 0, 10, 0, RST_, 15, SLD_ + 12, 7, RST_, 5, SLD_ + 3},
     {0}, {0}, 60, {2, 0, 1, 4}, {0, 0, 0, 0}, {60, 0, 70, 50}},
    {"nebula", SD_P_NEBULA, 70, 5, 50, 0, SD_MODE_POLY, {0}, {0}, {0}, 100, {0, 1, 0, 2}, {0, 10, 0, 20}, {0, 70, 0, 80}},
    {"pulsar", SD_P_PULSAR, 100, 4, 50, 16, SD_MODE_MONO, {0, 7, 12, 15, 19, 15, 12, TIE_, 3, 10, 15, 22, 24, 22, 15, TIE_},
     {0}, {0}, 80, {2, 1, 4, 3}, {0, 0, 0, 0}, {45, 60, 40, 70}},
    {"ion", SD_P_ION, 124, 2, 33, 16, SD_MODE_MONO, {0, 0, 12, RST_, 0, 10, RST_, 0, 3, RST_, 0, 12, 0, RST_, 7, 5},
     {1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1}, {0, 70, 0, 0, 0, 60, 0, 80, 0, 0, 0, 50, 0, 0, 0, 70}, 55,
     {2, 0, 1, 0}, {0, 0, 10, 0}, {50, 0, 80, 0}},
    {"orbital", SD_P_ORBITAL, 96, 4, 45, 16, SD_MODE_MONO, {0, RST_, 7, 0, RST_, 12, 3, RST_, 0, 10, RST_, 7, 15, RST_, 0, 5},
     {0}, {0, 0, 70, 0, 0, 60, 80, 0, 0, 70, 0, 50, 60, 0, 0, 80}, 70, {1, 0, 2, 3}, {10, 0, 0, 10}, {70, 0, 50, 70}},
    {"monolith", SD_P_MONOLITH, 64, 6, 31, 8, SD_MODE_MONO, {0, TIE_, 3, TIE_, SLD_ + 7, TIE_, 5, RST_}, {0}, {0}, 98,
     {2, 0, 2, 4}, {0, 0, 0, 0}, {60, 0, 50, 50}},
    {"distroy", SD_P_DISTROY, 100, 4, 38, 16, SD_MODE_MONO, {0, 0, 12, SLD_ + 10, 0, RST_, 7, SLD_ + 5, 0, 0, 15, SLD_ + 12, 3, RST_, 10, 7},
     {0}, {0}, 70, {1, 2, 1, 4}, {0, 0, 0, 0}, {80, 70, 60, 60}},
    {"split", SD_P_SPLIT, 118, 2, 40, 16, SD_MODE_MONO, {0, RST_, 0, 12, RST_, 0, 10, 0, RST_, 7, 0, RST_, 3, 5, 0, RST_},
     {0}, {0}, 50, {2, 1, 1, 0}, {0, 0, 0, 0}, {50, 60, 80, 0}},
    {"growl", SD_P_GROWL, 92, 4, 31, 16, SD_MODE_MONO, {0, 0, SLD_ + 12, 0, RST_, 0, SLD_ + 7, 5, 0, 0, SLD_ + 15, 12, RST_, 10, SLD_ + 7, 3},
     {0}, {0}, 75, {2, 0, 1, 3}, {0, 0, 0, 0}, {50, 0, 60, 60}},
};
#define TOUR_N (sizeof TOUR_PARTS / sizeof TOUR_PARTS[0])

static uint32_t tour_part(const TourPart *t, FILE *one, FILE *all, uint32_t secs)
{
    static const uint8_t CH[4][4] = {{0, 3, 7, 14}, {-4 + 12, 0 + 12, 3 + 12, 7 + 12}, {-7 + 12, -4 + 12, 0 + 12, 9 + 12}, {-5 + 12, 0 + 12, 2 + 12, 7 + 12}};
    uint32_t frames = secs * SD_FS, fade = 2u * SD_FS, done = 0, k, bar_len = SD_FS * 60u * 4u / t->bpm, rnd = 0xC0FFEEu ^ t->patch;
    uint32_t chord = 99, held[4] = {0};
    int32_t buf[2u * HALF];
    double tgt[4], cur[4];
    SdPattern q;
    SdPatch p = SD_PATCH[t->patch % SD_PATCHES];
    sd_pattern_default(&q);
    if (t->len) {
        for (k = 0; k < t->len; k++) {
            int32_t v = t->line[k];
            SdStep *st = &q.step[k];
            st->vel = k % 4u == 0u ? 112u : 92u;
            st->gate = t->gate;
            st->ratchet = t->rat[k] ? t->rat[k] : 1u;
            st->prob = t->prob[k] ? t->prob[k] : 100u;
            if (v == RST_) { st->type = SD_ST_REST; continue; }
            if (v == TIE_) { st->type = SD_ST_TIE; continue; }
            st->type = v >= SLD_ - 50 ? SD_ST_SLIDE : SD_ST_NOTE;
            st->note = (uint8_t)((int32_t)t->root + (v >= SLD_ - 50 ? v - SLD_ : v));
        }
        q.len = (uint8_t)t->len;
        q.div = (uint8_t)t->div;
        q.seg_count = t->len >= 8u ? 4u : 1u;   /* four segments, forwards once each: SEG has something to say */
        sd_seg_even(&q);
    }
    for (k = 0; k < 4u; k++) tgt[k] = cur[k] = t->mac_lo[k] / 99.0;
    SD_INIT(&eng);
    eng.mode = (uint8_t)t->mode;
    sd_engine_pattern(&eng, &q);
    sd_engine_patch(&eng, &p);
    eng.bpm = (uint16_t)t->bpm;
    {                                           /* the mode switches at a tick while quiet: before the first chord */
        int32_t z[2u * SD_SUB];
        memset(z, 0, sizeof z);
        sd_engine_render(&eng, z, SD_SUB);
    }
    eng.seq_run = t->len ? 1u : 0u;
    while (done < frames) {
        uint32_t n = frames - done < HALF ? frames - done : HALF, i;
        if (done % bar_len < n) {               /* a bar's one: the macros due step, a random walk in their range */
            uint32_t bar = done / bar_len;
            for (k = 0; k < 4u; k++)
                if (t->mac_every[k] && bar % t->mac_every[k] == 0u && bar) {
                    double lo = t->mac_lo[k] / 99.0, hi = t->mac_hi[k] / 99.0;
                    rnd = rnd * 1103515245u + 12345u;
                    tgt[k] = lo + (hi - lo) * ((double)(rnd >> 8 & 0xFFFF) / 65535.0);
                }
            if (!t->len && (bar / 2u) % 4u != chord) {   /* POLY: a chord every two bars */
                chord = (bar / 2u) % 4u;
                for (k = 0; k < 4u; k++) if (held[k]) sd_engine_note(&eng, held[k], 0);
                for (k = 0; k < 4u; k++) { held[k] = t->root + CH[chord][k]; sd_engine_note(&eng, held[k], 90); }
            }
        }
        for (k = 0; k < 4u; k++) {
            cur[k] += (tgt[k] - cur[k]) * 0.07;
            p.mac[k] = (uint16_t)(65535 * fmax(0, fmin(1, cur[k])));
        }
        sd_engine_patch(&eng, &p);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&eng, buf, n);
        if (done + n > frames - fade) {         /* the last two seconds fade */
            for (i = 0; i < n; i++) {
                uint32_t left = frames - done - i;
                buf[2u * i] = (int32_t)((int64_t)buf[2u * i] * left / fade);
                buf[2u * i + 1u] = (int32_t)((int64_t)buf[2u * i + 1u] * left / fade);
            }
        }
        if (one) wav_put(one, buf, n);
        if (all) wav_put(all, buf, n);
        done += n;
    }
    for (k = 0; k < 4u; k++) if (held[k]) sd_engine_note(&eng, held[k], 0);
    return frames;
}

static int tour(const char *dir)
{
    char path[512];
    uint32_t k, secs = 22;
    FILE *all;
    snprintf(path, sizeof path, "%s/tour.wav", dir);
    all = wav_open(path, (uint32_t)TOUR_N * secs * SD_FS);
    if (!all) return 1;
    for (k = 0; k < TOUR_N; k++) {
        FILE *one;
        snprintf(path, sizeof path, "%s/tour_%02u_%s.wav", dir, k + 1u, TOUR_PARTS[k].name);
        one = wav_open(path, secs * SD_FS);
        if (!one) return 1;
        tour_part(&TOUR_PARTS[k], one, all, secs);
        fclose(one);
        printf("tour -> %s (%s, %u BPM)\n", path, SD_PATCH_NAME[TOUR_PARTS[k].patch], TOUR_PARTS[k].bpm);
    }
    fclose(all);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "tour")) return tour(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "evolve"))
        return evolve(argv[2], patch_arg(argv[3]), argc > 4 ? (uint32_t)atoi(argv[4]) : 90u);
    if (argc >= 4 && !strcmp(argv[1], "pattern"))
        return pattern(argv[2], patch_arg(argv[3]), argc > 4 ? (uint32_t)atoi(argv[4]) : 0u);
    if (argc >= 4 && !strcmp(argv[1], "macros")) return macros(argv[2], patch_arg(argv[3]));
    if (argc >= 4 && !strcmp(argv[1], "scene")) return scene(argv[2], patch_arg(argv[3]));
    if (argc >= 3 && !strcmp(argv[1], "render"))
        return render(argv[2], argc > 3 ? (uint32_t)atoi(argv[3]) : 8u, argc > 4 ? (uint32_t)atoi(argv[4]) : 120u,
                      argc > 5 ? patch_arg(argv[5]) : SD_P_DISTROY);
    if (argc >= 3 && !strcmp(argv[1], "field"))
        return field(argv[2], argc > 3 ? (uint32_t)atoi(argv[3]) : 15u);
    if (argc >= 5 && !strcmp(argv[1], "mode"))
        return mode(argv[3], !strcmp(argv[2], "poly") ? SD_MODE_POLY : SD_MODE_DUO, patch_arg(argv[4]));
    if (argc >= 4 && !strcmp(argv[1], "hold")) {
        xover_cmp = argc > 5 && !strcmp(argv[5], "cmp");
        return hold(argv[2], patch_arg(argv[3]), argc > 4 ? (uint32_t)atoi(argv[4]) : 57u);
    }
    if (argc >= 4 && !strcmp(argv[1], "sweep"))
        return sweep(argv[2], argv[3], argc > 4 ? (uint32_t)atoi(argv[4]) : 8u, argc > 5 ? (uint32_t)atoi(argv[5]) : 45u);
    if (argc >= 2 && !strcmp(argv[1], "bench"))
        return bench(argc > 2 ? (uint32_t)atoi(argv[2]) : 60u);
    fprintf(stderr, "usage: sd_render render OUT.wav [SECONDS] [BPM] [PATCH] | hold OUT.wav PATCH [NOTE] | field OUT.wav [S] | "
                    "mode poly|duo OUT.wav PATCH | "
                    "sweep wp|fd|xm|dt|pos OUT.wav [SECONDS] [NOTE] | "
                    "bench [SECONDS]\n");
    return 2;
}
