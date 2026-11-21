/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The button masher (2026-11-20; the user's FM-1 froze twice on fast menu clicks, EDIT and SEQ on DRUM, and the
 * watchdog restarted it). FUZZ_S simulated seconds (default 120) of random input from FUZZ_SEED (default 1), faster
 * than hands: taps of every button, held ones (GLO, HOME and the rest, for their combinations), the encoders turned
 * a few detents at a time, chords on the keys, MIDI (notes, CCs, program changes, a clock, start and stop) over USB
 * and the jack, and bursts of EDIT and SEQ 10..40 ms apart. Then everything let go, 8 s of nothing.
 * It fails on:
 *   a hang       the main loop not back in 10 s of the computer's time (on the FM-1 the watchdog restarts it after
 *                8 s): the input that led there is printed
 *   memory       anything out of bounds, a signed overflow (tests/run_tests.sh builds it with ASan and UBSan)
 *   a stuck note a voice still held by a key or MIDI after all of them were let go
 *   no reach     fewer than 12 of the 15 pages seen
 *   the sound    a sample past 32767 (the platform clips there: a hard clip), or no sound at all
 * The host runs the audio between the main loop's passes, so what an interrupt in the middle of one could do is the
 * FM-1's own (RESTART, README).
 *   cc -fsanitize=address,undefined -DDINGHY_HOST ... tests/app_fuzz_test.c && HOST_NO_FILES=1 ./app_fuzz_test x */
#define HOST_POLL_HOOK fuzz_poll
#define HOST_BLOCK_HOOK fuzz_block
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include "stardistroyer.c"

#define FZ_MAX 400000u
static host_ev_t fz[FZ_MAX];
static uint32_t fz_n, fz_rng = 1, fz_end, fz_pages, fz_blocks, fz_loud, fz_peak, fz_over, fz_demo_ms;
static volatile uint32_t wd_last, wd_stall;

static uint32_t rr(void) { fz_rng ^= fz_rng << 13; fz_rng ^= fz_rng >> 17; fz_rng ^= fz_rng << 5; return fz_rng; }
static uint32_t rn(uint32_t n) { return rr() % n; }
static void ev(uint32_t t, uint32_t keys, int btn, int enc, int encn, uint8_t st, uint8_t d1, uint8_t d2, uint8_t src)
{
    host_ev_t e = {t, keys, btn, st, d1, d2, (int8_t)enc, (int8_t)encn, 0, 0, 0, src};
    if (fz_n < FZ_MAX) fz[fz_n++] = e;
}
static void tap(uint32_t t, int b) { ev(t, 0, b, -1, 0, 0, 0, 0, 0); }
static void midi(uint32_t t, uint8_t st, uint8_t d1, uint8_t d2) { ev(t, 0, -1, -1, 0, st, d1, d2, (uint8_t)(1u + rn(2))); }
static int by_time(const void *a, const void *b)
{
    const host_ev_t *x = a, *y = b;
    return x->t < y->t ? -1 : x->t > y->t ? 1 : x < y ? -1 : x > y;
}

/* the hang: the computer's clock, once a second; the main loop's millisecond not moved for 10 */
static void fuzz_alarm(int sig)
{
    char b[160];
    int n;
    uint32_t i, from;
    (void)sig;
    if (fm1_ms != wd_last) { wd_last = fm1_ms; wd_stall = 0; return; }
    if (++wd_stall < 10u) return;
    n = snprintf(b, sizeof b, "FAIL  the masher: a hang at %u ms (page %u): the main loop did not come back; the input before it:\n",
                 fm1_ms, page);
    if (write(2, b, (size_t)n) < 0) _exit(2);
    for (from = 0; from < fz_n && fz[from].t + 400u < fm1_ms; from++) ;
    for (i = from; i < fz_n && fz[i].t <= fm1_ms; i++) {
        n = snprintf(b, sizeof b, "      %u ms btn %d keys %X enc %d %+d midi %02X %u %u\n", fz[i].t, fz[i].btn, fz[i].keys,
                     fz[i].enc, fz[i].encn, fz[i].st, fz[i].d1, fz[i].d2);
        if (write(2, b, (size_t)n) < 0) break;
    }
    _exit(2);
}

static void fuzz_poll(void)
{
    if (page < 32u) fz_pages |= 1u << page;
    if (demo_st) fz_demo_ms++;
}
static void fuzz_block(const int32_t *blk, uint32_t frames)
{
    uint32_t i, pk = 0;
    for (i = 0; i < 2u * frames; i++) {
        uint32_t a = (uint32_t)(blk[i] < 0 ? -blk[i] : blk[i]);
        if (a > pk) pk = a;
    }
    fz_blocks++;
    if (pk > fz_peak) fz_peak = pk;
    if (pk > 64u) fz_loud++;
    if (pk > 32767u) fz_over++;                 /* (the platform clips there: every ceiling soft before it) */
}

__attribute__((constructor)) static void fuzz_make(void)
{
    static const int BTN[] = {BTN_EDIT, BTN_SEQ, BTN_EDIT, BTN_SEQ, BTN_GLO, BTN_HOME, BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO,
                              BTN_ARP, BTN_PLAY, BTN_REC, BTN_SAVE, BTN_OCT_UP, BTN_OCT_DOWN};
    static const int HOLD[] = {BTN_GLO, BTN_GLO, BTN_HOME, BTN_SEL, BTN_EDIT, BTN_SEQ, BTN_FX, BTN_OCT_UP, BTN_OCT_DOWN, BTN_PLAY};
    const char *e = getenv("FUZZ_SEED");
    uint32_t t = 1800u, s = getenv("FUZZ_S") ? (uint32_t)atoi(getenv("FUZZ_S")) : 120u, i;
    struct itimerval it = {{1, 0}, {1, 0}};
    fz_rng = e ? (uint32_t)strtoul(e, 0, 0) * 2654435761u + 1u : 1u;
    if (!fz_rng) fz_rng = 1;
    fz_end = 1800u + s * 1000u;
    while (t < fz_end && fz_n + 64u < FZ_MAX) {
        uint32_t r = rn(100);
        t += 2u + rn(rn(4) ? 30u : 300u);       /* (mostly 2..32 ms apart, now and then a rest) */
        if (r < 40) tap(t, BTN[rn(sizeof BTN / sizeof BTN[0])]);
        else if (r < 50) {                      /* a button held, others pressed meanwhile */
            int b = HOLD[rn(sizeof HOLD / sizeof HOLD[0])];
            uint32_t h = 30u + rn(900), k, n = rn(4);
            ev(t, 0, b + 100, -1, 0, 0, 0, 0, 0);
            for (k = 0; k < n; k++) tap(t + 5u + rn(h), BTN[rn(sizeof BTN / sizeof BTN[0])]);
            if (rn(2)) ev(t + 5u + rn(h), 0, -1, (int)rn(HOST_NENC), (int)rn(9) - 4, 0, 0, 0, 0);
            ev(t + h, 0, b + 200, -1, 0, 0, 0, 0, 0);
        } else if (r < 70) {                    /* an encoder, -8..8 detents (never 0) */
            int d = (int)rn(16) - 8;
            ev(t, 0, -1, (int)rn(HOST_NENC), d >= 0 ? d + 1 : d, 0, 0, 0, 0);
        }
        else if (r < 82) {                      /* keys: a chord, held */
            uint32_t m = 0, k, n = 1u + rn(3);
            for (k = 0; k < n; k++) m |= 1u << rn(27);
            ev(t, m, -1, -1, 0, 0, 0, 0, 0);
            ev(t + 10u + rn(600), 0, -1, -1, 0, 0, 0, 0, 0);
        } else if (r < 90) {                    /* MIDI: a note on and its off */
            uint8_t ch = (uint8_t)rn(rn(4) ? 1u : 16u), n = (uint8_t)(24u + rn(84));
            uint32_t h = 5u + rn(800);
            midi(t, (uint8_t)(0x90u | ch), n, (uint8_t)(1u + rn(127)));
            if (rn(3)) midi(t + h, (uint8_t)(0x80u | ch), n, 0); else midi(t + h, (uint8_t)(0x90u | ch), n, 0);
        } else if (r < 94) {                    /* a CC (not the sustain pedal: held notes are its job), a program */
            uint8_t cc = (uint8_t)rn(120);
            if (cc == 64u) cc = 1u;
            if (rn(4)) midi(t, 0xB0u, cc, (uint8_t)rn(128)); else midi(t, 0xC0u, (uint8_t)rn(128), 0);
        } else if (r < 96) {                    /* a clock: start, 24..200 ticks at 60..240 BPM, stop */
            uint32_t bpm = 60u + rn(181), n = 24u + rn(177), k;
            midi(t, 0xFAu, 0, 0);
            for (k = 0; k < n; k++) midi(t + k * 2500u / bpm, 0xF8u, 0, 0);
            if (rn(2)) midi(t + n * 2500u / bpm, 0xFCu, 0, 0);
        } else {                                /* the report: EDIT and SEQ fast, 5..20 times */
            uint32_t n = 5u + rn(16), k, g = 10u + rn(31);
            for (k = 0; k < n; k++) tap(t + k * g, rn(3) ? (k & 1u ? BTN_SEQ : BTN_EDIT) : BTN[rn(sizeof BTN / sizeof BTN[0])]);
            t += n * g;
        }
    }
    for (i = 0; i < 14u; i++) ev(fz_end, 0, (int)i + 200, -1, 0, 0, 0, 0, 0);   /* all let go */
    ev(fz_end, 0, -1, -1, 0, 0, 0, 0, 0);
    midi(fz_end, 0xB0u, 64u, 0u);
    qsort(fz, fz_n, sizeof fz[0], by_time);
    script = fz;
    script_n = fz_n;
    end_ms = fz_end + 8000u;
    signal(SIGALRM, fuzz_alarm);
    setitimer(ITIMER_REAL, &it, 0);
}

__attribute__((destructor)) static void fuzz_check(void)
{
    uint32_t i, held = 0, pages = 0, bad = 0;
    struct itimerval off = {{0, 0}, {0, 0}};
    setitimer(ITIMER_REAL, &off, 0);
    for (i = 0; i < SD_VOICES; i++) held += eng.voice[i].gate && eng.voice[i].src == SD_SRC_HAND;
    for (i = 0; i < 15u; i++) pages += fz_pages >> i & 1u;
    fprintf(stderr, "      the masher: %u events over %u s, %u pages seen, the demo %u s, peak %u, %u of %u blocks sounding\n",
            fz_n, (fz_end - 1800u) / 1000u, pages, fz_demo_ms / 1000u, fz_peak, fz_loud, fz_blocks);
    if (fm1_ms < end_ms) { fprintf(stderr, "FAIL  the masher: it ended at %u ms, not %u\n", fm1_ms, end_ms); bad++; }
    if (held && !demo_st) { fprintf(stderr, "FAIL  the masher: %u voices still held by a key or MIDI after all were let go\n", held); bad++; }
    if (pages < 12u) { fprintf(stderr, "FAIL  the masher: only %u pages seen\n", pages); bad++; }
    if (fz_over) { fprintf(stderr, "FAIL  the masher: %u blocks past the ceiling (peak %u)\n", fz_over, fz_peak); bad++; }
    if (!fz_loud) { fprintf(stderr, "FAIL  the masher: no sound at all\n"); bad++; }
    if (bad) { fflush(stderr); _exit(1); }
    fprintf(stderr, "ok    the masher: no hang, no stuck note, every page reached\n");
}
