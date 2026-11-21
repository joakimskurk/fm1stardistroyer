/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 zvenson (the store, from OberVave's: platform.c's until 2026-11-20); joakimskurk (the two copies) */
/* The store's reads and writes on the device (platform.c; tests/store_test.c runs them on a simulated flash). Star
 * Distroyer's, 2026-11-20: before, each save had one place, and a cut in the middle of one (the power, a restart) lost
 * all of it, every patch or every pattern.
 *
 * A copy: [magic][length][crc32], the bytes, then its number as ~n (an erased word reads 0: a save from before the
 * copies, which still loads). Each save has two places, A and B (store.h). A save erases and writes the place that does
 * not hold the newest whole copy, the header last (a cut save stays invalid), then reads it back (a part that did not
 * take it never says done: its header is erased again, -7). A load takes the newest copy whose bytes check, else the
 * other. So at any cut one whole copy stays: the old save or the new one.
 *
 * It needs, before the #include: st_read, fl_erase4k, fl_write (its source in RAM), fm1_wdt_feed, flash_ok, memcmp,
 * and store.h. */
#define ST_HEAD 12u

static uint32_t crc32_more(uint32_t c, const uint8_t *p, uint32_t n)
{
    uint32_t k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return c;
}
static uint32_t crc32(const uint8_t *p, uint32_t n) { return ~crc32_more(0xFFFFFFFFu, p, n); }
static uint32_t st_foot(uint32_t at, uint32_t n) { return at + ST_HEAD + ((n + 3u) & ~3u); }

/* the copy at `at` (0: none), its data up to max bytes (max: its room less the header, as SLOT_MAX): 0 if its header
 * reads right (want: that length; 0: any), its length and number out; with check its bytes' crc too */
static int st_copy(uint32_t at, uint32_t max, uint32_t magic, uint32_t want, int check, uint32_t *len, uint32_t *num)
{
    uint32_t h[3], f, c = 0xFFFFFFFFu, o;
    uint8_t b[256];
    if (!at || st_read(at, h, sizeof h) || h[0] != magic || h[1] + STORE_FOOT > max || (want && h[1] != want) ||
        st_read(st_foot(at, h[1]), &f, sizeof f))
        return -1;
    *len = h[1];
    *num = ~f;
    if (!check)
        return 0;
    for (o = 0; o < h[1]; o += sizeof b) {
        uint32_t k = h[1] - o > sizeof b ? sizeof b : h[1] - o;
        if (st_read(at + ST_HEAD + o, b, k))
            return -1;
        c = crc32_more(c, b, k);
    }
    return ~c == h[2] ? 0 : -1;
}

static int st_take(uint32_t at, void *p, uint32_t n)   /* the copy's bytes into p, their crc checked */
{
    uint32_t h[3];
    if (st_read(at, h, sizeof h) || st_read(at + ST_HEAD, p, n))
        return -1;
    return crc32(p, n) == h[2] ? 0 : -1;
}

/* n bytes into p from the newest copy that checks (A's room amax, B's bmax; b 0: one place). On -1 p may hold rubbish */
static int area_load(uint32_t a, uint32_t amax, uint32_t b, uint32_t bmax, uint32_t magic, void *p, uint32_t n)
{
    uint32_t l, na = 0, nb = 0;
    int va, vb;
    if (!flash_ok || !n)
        return -1;
    va = !st_copy(a, amax, magic, n, 0, &l, &na);
    vb = !st_copy(b, bmax, magic, n, 0, &l, &nb);
    if (vb && (!va || nb > na)) {               /* B the newer */
        if (!st_take(b, p, n))
            return 0;
        return va ? st_take(a, p, n) : -1;
    }
    if (va && !st_take(a, p, n))
        return 0;
    return vb ? st_take(b, p, n) : -1;
}

/* n bytes from p (in RAM) to the place that does not hold the newest whole copy; 0: done and read back */
static int area_save(uint32_t a, uint32_t amax, uint32_t b, uint32_t bmax, uint32_t magic, const void *p, uint32_t n)
{
    uint32_t h[3] = {magic, n, 0}, l, na = 0, nb = 0, x, num = 0, to, k, sectors, took, f, o;
    uint8_t buf[256];
    int ga, gb, rc;
    if (!flash_ok || !n || n + STORE_FOOT > amax || (b && n + STORE_FOOT > bmax))
        return -1;
    if (!st_copy(a, amax, magic, 0, 0, &l, &x) && x > num) num = x;   /* (past every copy whose header reads) */
    if (!st_copy(b, bmax, magic, 0, 0, &l, &x) && x > num) num = x;
    ga = !st_copy(a, amax, magic, 0, 1, &l, &na);
    gb = !st_copy(b, bmax, magic, 0, 1, &l, &nb);
    to = b && ga && (!gb || na >= nb) ? b : a;  /* over the older (A newest on a tie, or none whole: A) */
    h[2] = crc32(p, n);
    f = ~(num + 1u);
    sectors = (ST_HEAD + ((n + 3u) & ~3u) + STORE_FOOT + 0xFFFu) >> 12;
    for (k = 0; k < sectors; k++) {
        fm1_wdt_feed();                         /* (~50 ms an erase) */
        if ((rc = fl_erase4k(to + k * 0x1000u, &took)) != 0)
            return rc;
    }
    if ((rc = fl_write(to + ST_HEAD, p, n)) != 0 || (rc = fl_write(st_foot(to, n), (const uint8_t *)&f, sizeof f)) != 0 ||
        (rc = fl_write(to, (const uint8_t *)h, sizeof h)) != 0)   /* the header last */
        return rc;
    for (o = 0; o < n; o += sizeof buf) {       /* read back */
        k = n - o > sizeof buf ? sizeof buf : n - o;
        if (st_read(to + ST_HEAD + o, buf, k) || memcmp(buf, (const uint8_t *)p + o, k))
            break;
    }
    if (o >= n && !st_copy(to, to == a ? amax : bmax, magic, n, 0, &l, &x) && x == num + 1u)
        return 0;
    fl_erase4k(to, &took);                      /* (its header gone: the other stays the newest) */
    return -7;
}

/* settings, blobs: a sector each, up to SECTOR_MAX - STORE_FOOT bytes; a load that fails leaves p as it was */
static int sector_load(uint32_t a, uint32_t b, void *p, uint32_t n)
{
    static uint8_t buf[SECTOR_MAX];
    if (n > sizeof buf || area_load(a, SECTOR_MAX, b, SECTOR_MAX, STORE_MAGIC, buf, n))
        return -1;
    memcpy(p, buf, n);
    return 0;
}
static int settings_load(void *p, uint32_t n) { return sector_load(SETTINGS_AT, SETTINGS_B_AT, p, n); }
static int settings_save(const void *p, uint32_t n) { return area_save(SETTINGS_AT, SECTOR_MAX, SETTINGS_B_AT, SECTOR_MAX, STORE_MAGIC, p, n); }
static int blob_load(uint32_t i, void *p, uint32_t n) { return i < NBLOB ? sector_load(BLOBS_AT + i * 0x1000u, 0, p, n) : -1; }
static int blob_save(uint32_t i, const void *p, uint32_t n) { return i < NBLOB ? area_save(BLOBS_AT + i * 0x1000u, SECTOR_MAX, 0, 0, STORE_MAGIC, p, n) : -1; }
/* slot i: SLOT_SECT sectors, up to SLOT_MAX - STORE_FOOT bytes (slots 3, 4: SECTOR_MAX - STORE_FOOT, their second
 * copy a sector after the slots). slot_load reads straight into p (no buffer that big): on -1 p holds rubbish, start afresh */
#define SLOT_MAGIC 0x544F4C53u                  /* "SLOT" */
static int slot_load(uint32_t i, void *p, uint32_t n)
{
    return i < NSLOT && SLOT_OWN(i) ? area_load(SLOTS_AT + i * SLOT_SECT * 0x1000u, SLOT_MAX, SLOT_B_AT(i), SLOT_B_MAX(i),
                                                SLOT_MAGIC ^ STORE_MAGIC, p, n) : -1;
}
static int slot_save(uint32_t i, const void *p, uint32_t n)
{
    return i < NSLOT && SLOT_OWN(i) ? area_save(SLOTS_AT + i * SLOT_SECT * 0x1000u, SLOT_MAX, SLOT_B_AT(i), SLOT_B_MAX(i),
                                                SLOT_MAGIC ^ STORE_MAGIC, p, n) : -1;
}
