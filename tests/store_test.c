/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The store's two copies (firmware/src/store_io.h, 2026-11-20) on a simulated NOR flash (an erase sets a sector to
 * 0xFF, a write can only clear bits, in 256-byte pages as the driver does). The power is cut after every one of a
 * save's flash steps in turn: a cut erase leaves the sector half done, a cut page a part of it and a byte's bits
 * half set. After each cut, as at the next start: the load must give the save before or the new one, never nothing
 * and never a mix; the next save must work. Twice in a row too; and the saves from before the copies (no number),
 * a part that does not take a byte (read back: -7, the old one stays), the places (each its own sectors, all in the
 * app's allow-list), the sectors a save erases.
 *   cc -Ifirmware/hal -Ifirmware/src -o store_test tests/store_test.c && ./store_test   (-DBOARD_SMK37=1: the SMK's) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"
#if BOARD_SMK37
#define BOARD_NOTE ""
#else
#define BOARD_NOTE " or the stock firmware's patch area (0xEA000..0xFBFFF)"
#endif

static uint8_t nor[0x100000], snap[0x100000], snap2[0x100000];
static uint8_t flash_ok = 1;
static long ops_left = -1;                      /* flash steps till the cut (-1: none) */
static int cut;
static uint32_t ops, erases, stuck_at = 0xFFFFFFFFu, rng = 12345u;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off > sizeof nor || n > sizeof nor - off) return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int step(void)                           /* 1: this step happens whole; 0: the cut is now (or was) */
{
    if (cut) return 0;
    if (ops_left == 0) { cut = 1; return 0; }
    if (ops_left > 0) ops_left--;
    ops++;
    return 1;
}
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    uint32_t i;
    *took = 0;
    if (off & 0xFFFu || off >= sizeof nor || !FL_RANGE_OK(off, 0x1000u)) { printf("FAIL  an erase outside the store: %05X\n", off); exit(1); }
    if (cut) return 0;
    if (!step()) {                              /* cut in the middle: some of it erased, some not */
        for (i = 0; i < 4096u; i++) nor[off + i] = rnd() & 1u ? 0xFFu : (uint8_t)(nor[off + i] & rnd());
        return 0;
    }
    memset(nor + off, 0xFF, 4096u);
    erases++;
    return 0;
}
static int fl_write(uint32_t off, const uint8_t *src, uint32_t n)
{
    if (!FL_RANGE_OK(off, n)) { printf("FAIL  a write outside the store: %05X +%u\n", off, n); exit(1); }
    while (n) {
        uint32_t chunk = 256u - (off & 0xFFu), part, i;
        if (chunk > n) chunk = n;
        if (cut) return 0;
        part = step() ? chunk : rnd() % chunk;
        for (i = 0; i < part; i++) {
            nor[off + i] &= src[i];
            if (off + i == stuck_at) nor[off + i] |= 0x10u;   /* (a bit that will not program) */
        }
        if (cut) { nor[off + part] &= (uint8_t)(src[part] | rnd()); return 0; }   /* (a byte's bits half set) */
        off += chunk; src += chunk; n -= chunk;
    }
    return 0;
}
static void fm1_wdt_feed(void) {}
#include "store_io.h"

static int bad, runs;
static void check(const char *what, int ok) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); bad += !ok; }

static uint8_t want[8][SLOT_MAX];               /* the saves, by their seed */
static void fill(uint32_t seed, uint32_t n) { uint32_t i, r = seed * 2654435761u + 1u; for (i = 0; i < n; i++) { r = r * 1103515245u + 12345u; want[seed & 7u][i] = (uint8_t)(r >> 16); } }

typedef struct { const char *name; int kind; uint32_t i, n; } Obj;   /* kind 0: a slot, 1: the settings */
static int save(const Obj *o, uint32_t seed) { fill(seed, o->n); return o->kind ? settings_save(want[seed & 7u], o->n) : slot_save(o->i, want[seed & 7u], o->n); }
static int load(const Obj *o, uint8_t *p, uint32_t n) { return o->kind ? settings_load(p, n) : slot_load(o->i, p, n); }
static int which(const Obj *o, uint32_t n, const uint32_t *seeds, int k)   /* the seed the load gave; -1: none, -2: rubbish */
{
    static uint8_t got[SLOT_MAX];
    int j;
    if (load(o, got, n)) return -1;
    for (j = 0; j < k; j++) { fill(seeds[j], n); if (!memcmp(got, want[seeds[j] & 7u], n)) return (int)seeds[j]; }
    return -2;
}
static void power_on(void) { cut = 0; ops_left = -1; }

/* a save of `next` cut after each of its steps, from the state in nor (whose load gives `last`); after each: the load
 * gives last or next, then a save goes through; with twice, each cut state gets a second cut save at every step too */
static void sweep(const Obj *o, uint32_t last, uint32_t next, int twice)
{
    uint32_t total, k, fails = 0, fails2 = 0, oldn = 0, newn = 0;
    const uint32_t seeds[3] = {last, next, next + 1u};
    char line[200];
    memcpy(snap, nor, sizeof nor);
    ops = 0;
    if (save(o, next)) { check("a save without a cut", 0); return; }
    total = ops;
    for (k = 0; k <= total; k++) {
        int got;
        memcpy(nor, snap, sizeof nor);
        ops_left = (long)k; cut = 0;
        save(o, next);
        power_on();
        runs++;
        got = which(o, o->n, seeds, 2);
        if (got == (int)last) oldn++; else if (got == (int)next) newn++; else fails++;
        if (k == total && got != (int)next) fails++;
        if (twice && got >= 0) {
            uint32_t j, total2;
            memcpy(snap2, nor, sizeof nor);
            ops = 0;
            save(o, next + 1u);
            total2 = ops;
            for (j = 0; j < total2; j += 1u + (total2 > 40u) * 2u) {   /* (every third step in a long one) */
                int g2;
                memcpy(nor, snap2, sizeof nor);
                ops_left = (long)j; cut = 0;
                save(o, next + 1u);
                power_on();
                runs++;
                g2 = which(o, o->n, seeds, 3);
                if (g2 != got && g2 != (int)(next + 1u)) fails2++;
            }
        }
        memcpy(nor, snap, sizeof nor);
        ops_left = (long)k; cut = 0;
        save(o, next);
        power_on();
        if (save(o, next + 1u) || which(o, o->n, seeds, 3) != (int)(next + 1u)) fails++;   /* the next save works */
    }
    memcpy(nor, snap, sizeof nor);
    snprintf(line, sizeof line, "%s (%u B): cut after each of a save's %u steps: the old save %u times, the new %u, never "
             "nothing or a mix; the next save works%s", o->name, o->n, total, oldn, newn, twice ? "; a second cut save too" : "");
    check(line, !fails && !fails2 && oldn && newn);
    if (fails || fails2) printf("      (%u bad after one cut, %u after two)\n", fails, fails2);
}

static void legacy(uint32_t at, uint32_t magic, const uint8_t *p, uint32_t n)   /* a save as before the copies */
{
    uint32_t h[3] = {magic, n, crc32(p, n)}, s;
    for (s = 0; s < (12u + n + 0xFFFu) >> 12; s++) memset(nor + at + s * 0x1000u, 0xFF, 4096u);
    memcpy(nor + at + 12u, p, n);
    memcpy(nor + at, h, 12u);
}

int main(void)
{
    static uint8_t got[SLOT_MAX];
    const Obj PAT = {"slot 0 (the patches)", 0, 0, 15364u}, SEQ = {"slot 1 (half the patterns)", 0, 1, 9634u},
              SONG = {"slot 3 (the songs; the copy one sector)", 0, 3, 3204u}, DRUM = {"slot 4 (the beats; the copy one sector)", 0, 4, 772u},
              SET = {"the settings", 1, 0, 142u};
    const Obj *all[] = {&PAT, &SEQ, &SONG, &DRUM, &SET};
    uint32_t i, j, e, places[2 * 6], sizes[2 * 6], np = 0, ok;
    const uint32_t s012[3] = {1u, 2u, 3u};
    char line[200];

    memset(nor, 0xFF, sizeof nor);
    printf("%s\n", BOARD_SMK37 ? "SMK-37" : "FM-1");

    /* the places: A and B of each, in the allow-list, none sharing a sector */
    for (i = 0; i < 5u; i++) { places[np] = SLOTS_AT + i * SLOT_SECT * 0x1000u; sizes[np++] = SLOT_SECT * 0x1000u;
                               places[np] = SLOT_B_AT(i); sizes[np++] = (SLOT_B_MAX(i) + 12u + 0xFFFu) & ~0xFFFu; }
    places[np] = SETTINGS_AT; sizes[np++] = 0x1000u; places[np] = SETTINGS_B_AT; sizes[np++] = 0x1000u;
    for (ok = 1, i = 0; i < np; i++) {
        ok &= FL_RANGE_OK(places[i], sizes[i]) && places[i] % 0x1000u == 0u;
        for (j = 0; j < np; j++) if (i != j && places[i] < places[j] + sizes[j] && places[j] < places[i] + sizes[i]) ok = 0;
    }
    for (i = 0; i < 8u; i++) ok &= SLOT_OWN(i) == (i < 5u) && (SLOT_B_AT(i) != 0u) == (i < 5u);
    for (i = 0; i < np; i++) ok &= places[i] + sizes[i] <= BLOBS_AT || places[i] >= BLOBS_AT + NBLOB * 0x1000u;
#if !BOARD_SMK37
    for (i = 0; i < np; i++) ok &= places[i] + sizes[i] <= 0xEA000u || places[i] >= 0xFC000u;   /* (the stock's USR area) */
#endif
    check("the places: slots 0..4 and the settings, each with a second copy, all in the store's allow-list, none sharing a "
          "sector, none in a blob" BOARD_NOTE, ok);
    check("slots 5..7 hold copies: their own saves and loads refused; the blobs as before (one copy)",
          slot_save(5, got, 16) == -1 && slot_save(7, got, 16) == -1 && slot_load(6, got, 16) == -1 &&
          blob_save(0, got, 16) == 0 && !blob_load(0, got, 16));

    /* sizes */
    check("a save leaves room for its number: a slot takes SLOT_MAX - 4, not - 3; slots 3, 4 a sector's (their copy one)",
          slot_save(0, want[0], SLOT_MAX - 4u) == 0 && slot_save(0, want[0], SLOT_MAX - 3u) == -1 &&
          slot_save(3, want[0], SECTOR_MAX - 4u) == 0 && slot_save(3, want[0], SECTOR_MAX - 3u) == -1 &&
          settings_save(want[0], SECTOR_MAX - 3u) == -1);
    memset(nor, 0xFF, sizeof nor);

    /* fresh, then saves in turn: each load the last, the copies alternate, a save erases only what it needs */
    for (ok = 1, e = 0, i = 0; i < 5u; i++) {
        uint32_t s;
        ok &= which(all[i], all[i]->n, s012, 0) == -1;
        for (s = 1; s <= 3u; s++) {
            uint32_t e0 = erases;
            ok &= save(all[i], s) == 0 && which(all[i], all[i]->n, s012, 3) == (int)s;
            e = erases - e0;
            ok &= e == (12u + all[i]->n + 4u + 0xFFFu) >> 12;
        }
    }
    {
        uint32_t h[2];
        fill(3, PAT.n);
        memcpy(h, nor + SLOTS_AT, 8);           /* saves 1 and 3 in A, 2 in B */
        ok &= h[1] == PAT.n && nor[SLOTS_AT + 12u] == want[3][0];
    }
    check("fresh: no load; then 3 saves each: each load the last, A and B in turn, the sectors erased only as many as "
          "the save needs (the beats 1, the patches 4)", ok);

    /* the cuts: from one copy, from two, twice in a row */
    for (i = 0; i < 5u; i++) {
        memset(nor, 0xFF, sizeof nor);
        save(all[i], 1u);
        sweep(all[i], 1u, 2u, 0);                /* from A alone */
        save(all[i], 2u);
        sweep(all[i], 2u, 3u, i >= 3u || i == 0u);   /* from both (twice: the small ones and the patches) */
    }

    /* the saves from before the copies: a load takes them; a cut save over them keeps one; a new length beside them */
    memset(nor, 0xFF, sizeof nor);
    fill(5, PAT.n);
    legacy(SLOTS_AT, SLOT_MAGIC ^ STORE_MAGIC, want[5], PAT.n);
    {
        const uint32_t s5[2] = {5u, 6u};
        ok = which(&PAT, PAT.n, s5, 2) == 5;
        check("a save from before the copies (no number) loads", ok);
        sweep(&PAT, 5u, 6u, 0);
        memset(nor, 0xFF, sizeof nor);
        fill(5, 9000u);
        legacy(SLOTS_AT, SLOT_MAGIC ^ STORE_MAGIC, want[5], 9000u);   /* an older, shorter record */
        save(&PAT, 6u);
        ok = which(&PAT, PAT.n, s5, 2) == 6 && which(&PAT, 9000u, s5, 1) == 5;
        check("a new length saved beside an old one: the new loads, the old still there (in A; the new in B)", ok);
    }

    /* a part that does not take a byte: the read-back says -7, the copy is dropped, the old one loads */
    memset(nor, 0xFF, sizeof nor);
    save(&PAT, 1u);
    stuck_at = SLOT_B_AT(0) + 12u + 5000u;
    fill(2, PAT.n);
    want[2][5000] &= ~0x10u;                    /* (the bit that must clear) */
    i = (uint32_t)slot_save(0, want[2], PAT.n);
    stuck_at = 0xFFFFFFFFu;
    {
        const uint32_t s1[1] = {1u};
        ok = (int)i == -7 && which(&PAT, PAT.n, s1, 1) == 1 && save(&PAT, 3u) == 0;
        fill(3, PAT.n);
        ok &= !slot_load(0, got, PAT.n) && !memcmp(got, want[3], PAT.n);
        check("a byte the flash did not take: the save reads back -7, the old save loads, the next save works", ok);
    }

    /* the newest whole one is kept even when the newer number's bytes are bad (a torn header that reads right) */
    memset(nor, 0xFF, sizeof nor);
    save(&DRUM, 1u);
    save(&DRUM, 2u);                            /* 1 in A, 2 in B */
    nor[SLOT_B_AT(4) + 12u + 100u] ^= 0x01u;   /* B's bytes go bad (its header and number fine) */
    {
        const uint32_t s13[3] = {1u, 2u, 3u};
        ok = which(&DRUM, DRUM.n, s13, 3) == 1;
        save(&DRUM, 3u);                        /* must go over B, the bad one, not A */
        ok &= which(&DRUM, DRUM.n, s13, 3) == 3 && nor[SLOTS_AT + 4u * SLOT_SECT * 0x1000u + 12u] == (fill(1, DRUM.n), want[1][0]);
        check("a copy whose bytes went bad: the load takes the other; the next save goes over the bad one", ok);
    }
    snprintf(line, sizeof line, "%d starts after a cut, each loaded", runs);
    check(line, runs > 1000);
    if (bad) { printf("store_test: %d FAILED\n", bad); return 1; }
    printf("store_test: all ok\n");
    return 0;
}
