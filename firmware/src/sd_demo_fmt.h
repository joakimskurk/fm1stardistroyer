/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The demo's recording (2026-11-19, the user: SINGULARITY as a demo song on the FM-1). tools/sd_demo_rec.c writes it
 * into firmware/src/sd_demo.h (tools/make_demo.sh); the app plays it back from SETUP; tests/demo_test.c checks the
 * playback is the performance's render sample for sample. A performance (tools/sd_play.h: one FM-1 played by one
 * person) as what it does to the engine, render by render: a stream of events, each
 *   [the renders since the event before, varint] [op] [its bytes]
 * from render 0, the first of the performance's settling ones (SD_DEMO_SETTLE, not heard on the page's renders):
 *   SD_DO_PATCH   the patch handed over: the runs of its bytes that changed, [n] then n x [offset u16 LE, length u8,
 *                 the bytes]
 *   SD_DO_PAT     a track's pattern, the same way (bit 4 the track, bit 5 pat_wait: at the one)
 *   SD_DO_ORB     ORBIT, the same way (bit 5 orb_wait)
 *   SD_DO_SET     one of SD_DEMO_SCALARS: [its index] [the value, zigzag varint]
 *   SD_DO_NOTE    a key: [note] [velocity, 0 off]
 *   SD_DO_GAIN    MASTER as the player turned it (Q12, varint)
 *   SD_DO_SAY     what the player does, for the screen: [length] [text]
 *   SD_DO_END     the score's end (then on until it is quiet, as the render's)
 *   SD_DO_DKIT    the drums' kit, the bytes that changed (2026-11-21: a demo with the drums)
 *   SD_DO_DBEAT   the drums' beat and its ROLLs and PROBs (pat_in, then more_in, each as SD_DO_PATCH's runs; bit 5
 *                 pat_wait: at the bar's one)
 * The handovers keep their bytes in the engine's own buffers (patch_in, pat_in, orb_in): a run writes there and the
 * sequence numbers move as sd_engine_patch's do, so the player keeps no copy (no RAM). The structs' bytes are the
 * same on the computer that records and the FM-1 (sd_demo.h checks their sizes). */
#ifndef SD_DEMO_FMT_H
#define SD_DEMO_FMT_H

enum { SD_DO_PATCH = 1, SD_DO_PAT, SD_DO_ORB, SD_DO_SET, SD_DO_NOTE, SD_DO_GAIN, SD_DO_SAY, SD_DO_END, SD_DO_DKIT,
       SD_DO_DBEAT };
#define SD_DEMO_SCALARS(X) X(mode) X(bpm) X(xkey) X(seqb) X(master) X(seq_run) X(t[0].mute) X(t[1].mute) \
    X(t[0].xpose_next) X(t[1].xpose_next) X(arp_on) X(arp_mode) X(arp_div) X(arp_gate) X(arp_oct) X(arp_hold) \
    X(sp_freeze) X(sp_pitch) X(lp_on) X(lp_rev) X(lp_len) X(lp_speed) X(lp_glide) X(lp_lvl) X(lp_arm) X(lp_filt) \
    X(lp_rsend) X(lp_dsend) X(wheel) X(after) X(dr.on) X(dr.perc) X(dr.mute) X(filt)
typedef struct { uint16_t off; uint8_t size, sgn; } SdDemoField;
#define SD_DEMO_F(x) {(uint16_t)__builtin_offsetof(SdEngine, x), (uint8_t)sizeof(((SdEngine *)0)->x), \
                      (uint8_t)((__typeof__(((SdEngine *)0)->x))-1 < 0)},
static const SdDemoField SD_DEMO_FIELD[] = {SD_DEMO_SCALARS(SD_DEMO_F)};
#define SD_DEMO_NF ((uint32_t)(sizeof SD_DEMO_FIELD / sizeof SD_DEMO_FIELD[0]))

static int32_t sd_demo_get(const SdEngine *e, uint32_t i)   /* scalar i as it stands (little endian, as both are) */
{
    const SdDemoField *f = &SD_DEMO_FIELD[i];
    const uint8_t *p = (const uint8_t *)e + f->off;
    uint32_t v = f->size == 1u ? p[0] : f->size == 2u ? (uint32_t)p[0] | (uint32_t)p[1] << 8
               : (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    if (f->sgn && f->size == 1u) v = (uint32_t)(int32_t)(int8_t)v;
    if (f->sgn && f->size == 2u) v = (uint32_t)(int32_t)(int16_t)v;
    return (int32_t)v;
}
static void sd_demo_put(SdEngine *e, uint32_t i, int32_t v)
{
    const SdDemoField *f = &SD_DEMO_FIELD[i];
    uint8_t *p = (uint8_t *)e + f->off;
    uint32_t k, u = (uint32_t)v;
    for (k = 0; k < f->size; k++) p[k] = (uint8_t)(u >> (8u * k));
}

typedef struct {                                /* a playback: where in the stream, the next event's render */
    const uint8_t *s;
    uint32_t n, pos, next;
    int32_t gain;                               /* MASTER, Q12 */
    uint32_t say_off, say_len, says;            /* the last thing said (its place in the stream), how many so far */
} SdDemoPlay;
static uint32_t sd_demo_var(SdDemoPlay *d)
{
    uint32_t v = 0, sh = 0, b;
    do { b = d->pos < d->n ? d->s[d->pos++] : 0u; v |= (b & 127u) << sh; sh += 7u; } while ((b & 128u) && sh < 32u);
    return v;
}
static void sd_demo_begin(SdDemoPlay *d, const uint8_t *s, uint32_t n)
{
    d->s = s; d->n = n; d->pos = 0; d->gain = 4092; d->say_off = d->say_len = d->says = 0;
    d->next = n ? sd_demo_var(d) : 0xFFFFFFFFu;
}
static void sd_demo_runs(SdDemoPlay *d, uint8_t *dst, uint32_t size)
{
    uint32_t r = d->pos < d->n ? d->s[d->pos++] : 0u;
    while (r-- && d->pos + 3u <= d->n) {
        uint32_t off = (uint32_t)d->s[d->pos] | (uint32_t)d->s[d->pos + 1u] << 8, len = d->s[d->pos + 2u], k;
        d->pos += 3u;
        for (k = 0; k < len && d->pos + k < d->n; k++) if (off + k < size) dst[off + k] = d->s[d->pos + k];
        d->pos += len;
    }
}
/* render r's events (before render r; r from 0, each once): 1 when the score ended there */
static uint32_t sd_demo_events(SdDemoPlay *d, SdEngine *e, uint32_t r)
{
    uint32_t end = 0;
    while (d->pos < d->n && d->next == r) {
        uint32_t op = d->s[d->pos++], k = op >> 4 & 1u, w = op >> 5 & 1u;
        switch (op & 15u) {
        case SD_DO_PATCH:
            e->patch_seq++;
            SD_BARRIER();
            sd_demo_runs(d, (uint8_t *)&e->patch_in, (uint32_t)sizeof e->patch_in);
            SD_BARRIER();
            e->patch_seq++;
            break;
        case SD_DO_PAT:
            e->t[k].pat_wait = (uint8_t)w;
            e->t[k].pat_seq++;
            SD_BARRIER();
            sd_demo_runs(d, (uint8_t *)&e->t[k].pat_in, (uint32_t)sizeof e->t[k].pat_in);
            SD_BARRIER();
            e->t[k].pat_seq++;
            break;
        case SD_DO_ORB:
            e->orb_wait = (uint8_t)w;
            e->orb_seq++;
            SD_BARRIER();
            sd_demo_runs(d, (uint8_t *)&e->orb_in, (uint32_t)sizeof e->orb_in);
            SD_BARRIER();
            e->orb_seq++;
            break;
        case SD_DO_SET: {
            uint32_t i = d->pos < d->n ? d->s[d->pos++] : 0xFFu, z = sd_demo_var(d);
            if (i < SD_DEMO_NF) sd_demo_put(e, i, (int32_t)(z >> 1) ^ -(int32_t)(z & 1u));
            break;
        }
        case SD_DO_NOTE:
            if (d->pos + 2u <= d->n) sd_engine_note(e, d->s[d->pos], d->s[d->pos + 1u]);
            d->pos += 2u;
            break;
        case SD_DO_GAIN: d->gain = (int32_t)sd_demo_var(d); break;
        case SD_DO_SAY:
            d->say_len = d->pos < d->n ? d->s[d->pos++] : 0u;
            d->say_off = d->pos;
            d->pos += d->say_len;
            d->says++;
            break;
        case SD_DO_END: end = 1; break;
        case SD_DO_DKIT: sd_demo_runs(d, (uint8_t *)&e->dr.kit, (uint32_t)sizeof e->dr.kit); break;
        case SD_DO_DBEAT:
            e->dr.pat_wait = (uint8_t)w;
            e->dr.pat_seq++;
            SD_BARRIER();
            sd_demo_runs(d, (uint8_t *)&e->dr.pat_in, (uint32_t)sizeof e->dr.pat_in);
            sd_demo_runs(d, (uint8_t *)&e->dr.more_in, (uint32_t)sizeof e->dr.more_in);
            SD_BARRIER();
            e->dr.pat_seq++;
            break;
        default: d->pos = d->n; break;         /* (not an event: the stream ends there) */
        }
        d->next = d->pos < d->n ? d->next + sd_demo_var(d) : 0xFFFFFFFFu;
    }
    return end;
}
#endif
