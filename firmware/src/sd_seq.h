/* SPDX-License-Identifier: GPL-3.0-only */
/* Star Distroyer's sequencer: the pattern and what edits it (Sequencer Plan v0.2, its v0.3 revisions).
 *
 * A step sequencer like the stock FM-1's, up to 64 steps, with segments: the pattern is cut into up to
 * six segments A-F, each played REPEAT 1-4 times in its ORDER (FWD REV P-P RND) or SKIPped. A step is a NOTE (with
 * SLIDE: a glide into it, no new attack), a TIE (the note before held on), a REST, or SKIPped (out of the pattern).
 * A NOTE can RATCHET (2 or 3 hits in its time, each softer, never closer than 50 ms: the user asked for ratchets
 * "not as extreme and fast") and has a PROB, the chance it plays at all (else a rest).
 * RANDOMIZE rewrites the steps now. TURING is not the pattern's (since 2026-11-09, the user: it changed the loop
 * "without me understanding why"): it is a modulation source in the patch (sd_engine.h), and its NOTES moves track
 * A's notes in the scale as they play, the pattern as it was written.
 *
 * Everything here is plain data and functions on it: the main loop edits a pattern with them, the audio plays a copy
 * (sd_engine.h: the hand-over, the traversal, the timing). The segment editing: the bounds, a cut or a
 * join (toggle start), EVEN, a segment's length, the rescale to a new pattern length. */
#ifndef SD_SEQ_H
#define SD_SEQ_H
#include <stdint.h>

#define SD_PSTEPS 64u                           /* steps a pattern at most: four banks of 16 on the white keys */
#define SD_SEGS 6u                              /* segments A-F */
enum { SD_ST_NOTE, SD_ST_SLIDE, SD_ST_TIE, SD_ST_REST, SD_ST_SKIP, SD_STYPES };   /* a step's type */
enum { SD_SO_FWD, SD_SO_REV, SD_SO_PP, SD_SO_RND, SD_SORDERS };                  /* a segment's order */
enum { SD_SC_MINOR, SD_SC_MAJOR, SD_SC_DORIAN, SD_SC_PHRYG, SD_SC_PMIN, SD_SC_PMAJ, SD_SC_CHROM, SD_SCALES };
#define SD_SDIVS 6u                             /* DIV: 1/32 1/16T 1/16 1/8T 1/8 1/4, of the tempo's beat */
static const uint8_t SD_SDIV_NUM[SD_SDIVS] = {1, 2, 1, 4, 2, 4}, SD_SDIV_DEN[SD_SDIVS] = {2, 3, 1, 3, 1, 1};   /* x a 16th */

typedef struct {
    uint8_t note;                               /* MIDI */
    uint8_t vel;                                /* 1..127 */
    uint8_t gate;                               /* 1..100 % of the step */
    uint8_t type;                               /* SD_ST_* */
    uint8_t ratchet;                            /* 1..3 hits in the step (0 reads as 1) */
    uint8_t prob;                               /* 0..100 %: the chance it plays (the default: 100) */
    int8_t chord[3];                            /* a chord (the user, 2026-11-06): up to three more notes, semitones
                                                   from note (0: none); they move with it (TURING's NOTES, RND, transpose) */
} SdStep;
#define SD_RATCHET_MAX 3u
#define SD_RATCHET_MIN_SAMPLES 2205u            /* 50 ms: hits never closer */

typedef struct {
    SdStep step[SD_PSTEPS];
    uint8_t len;                                /* 1..64 */
    uint8_t div;                                /* SD_SDIV index; 2 = 1/16 */
    uint8_t swing;                              /* 50..75 %: the first of each pair of steps that much of the pair */
    uint8_t old_change;                         /* (TURING's CHANGE before 2026-11-09: unread, its place kept so the
                                                   saved patterns still read) */
    uint8_t seg_count;                          /* 1..6 */
    uint8_t seg_split[SD_SEGS - 1u];            /* where segments 2.. start (steps, rising) */
    uint8_t seg_rep[SD_SEGS];                   /* 1..4 */
    uint8_t seg_order[SD_SEGS];                 /* SD_SO_* */
    uint8_t seg_skip;                           /* bit k: segment k is skipped */
    uint8_t scale;                              /* SD_SC_*: what RANDOMIZE and TURING's NOTES choose notes from; the
                                                   key is the first step's note */
    uint8_t old_tur, old_range;                 /* (the old TURING's switch, RESTS and RANGE: unread, as above) */
} SdPattern;

/* ORBIT (2026-11-09, the user: "a sub-sequencer: different parameters in 2-8 steps, so I can make a bigger change,
 * like transposing a sequence over time"): 2..8 steps, each RATE loops of SEQ A long (1, 2, 4: a step changes at A's
 * one), each a change from where the player is: SEQ A's and B's transpose, two free lanes (each its matrix DEST), the
 * four macros. 0 changes nothing. The lanes and the macros jump at a step's start or glide through it to the next
 * step's value (GLIDE, a lane each: lane 1, lane 2, the macros). The pattern's, saved with it (SEQ A's). */
#define SD_ORB_STEPS 8u
enum { SD_OG_L1 = 1, SD_OG_L2 = 2, SD_OG_MAC = 4 };   /* GLIDE's bits */
typedef struct {
    uint8_t on, len, rate, glide;               /* on; 2..8 (0 reads as 4); 0 1 2: 1, 2, 4 loops of A; SD_OG_* */
    uint8_t dst[2], pad[2];                     /* the free lanes' destinations (SD_MD_*, 0 none) */
    int8_t xa[SD_ORB_STEPS], xb[SD_ORB_STEPS];  /* XPOSE A, B: -24..24 semitones */
    int8_t lane[2][SD_ORB_STEPS];               /* the free lanes: -100..100 % */
    int8_t mac[4][SD_ORB_STEPS];                /* MUTATE MOVE BREAK MACRO 4: -99..99 */
} SdOrbit;
#define SD_ORB_LEN(o) ((o)->len ? (o)->len < 2u ? 2u : (o)->len > 8u ? 8u : (uint32_t)(o)->len : 4u)
#define SD_ORB_LOOPS(o) (1u << ((o)->rate > 2u ? 2u : (o)->rate))

/* the scales as the semitones they hold, a bit each from the key */
static const uint16_t SD_SCALE_MASK[SD_SCALES] = {0x5AD, 0xAB5, 0x6AD, 0x5AB, 0x4A9, 0x295, 0xFFF};

static uint32_t sd_seq_rand(uint32_t *s) { *s = *s * 1103515245u + 12345u; return *s >> 8; }   /* 24 bits */

/* the segments' boundaries: b[0] = 0 .. b[sc] = len, each at least one step (build_bounds) */
static uint32_t sd_seg_bounds(const SdPattern *p, uint8_t b[SD_SEGS + 1u])
{
    uint32_t n = p->len < 1u ? 1u : p->len > SD_PSTEPS ? SD_PSTEPS : p->len, sc = p->seg_count, i;
    if (sc < 1u) sc = 1u;
    if (sc > SD_SEGS) sc = SD_SEGS;
    if (sc > n) sc = n;
    b[0] = 0;
    for (i = 0; i + 1u < sc; i++) {
        uint32_t lo = b[i] + 1u, hi = n - (sc - 1u - i), v = p->seg_split[i];
        b[i + 1u] = (uint8_t)(v < lo ? lo : v > hi ? hi : v);
    }
    b[sc] = (uint8_t)n;
    return sc;
}

static void sd_seg_store(SdPattern *p, const uint8_t *b, uint32_t sc)
{
    uint32_t i;
    p->seg_count = (uint8_t)sc;
    for (i = 0; i + 1u < sc; i++) p->seg_split[i] = b[i + 1u];
}

static void sd_seg_copy(SdPattern *p, uint32_t j, uint32_t i)   /* segment i's settings to segment j */
{
    p->seg_rep[j] = p->seg_rep[i];
    p->seg_order[j] = p->seg_order[i];
    p->seg_skip = (uint8_t)((p->seg_skip & ~(1u << j)) | ((p->seg_skip >> i & 1u) << j));
}

static uint32_t sd_seg_of(const SdPattern *p, uint32_t step)   /* the segment a step is in */
{
    uint8_t b[SD_SEGS + 1u];
    uint32_t sc = sd_seg_bounds(p, b), k;
    for (k = 0; k + 1u < sc && b[k + 1u] <= step; k++) {}
    return k;
}

/* A white key on SEGMENT: a cut where none starts (the new half takes the old one's settings), a join where one
 * does (it joins the one before). Returns the segment to pick, or -1 (step 1, past the end, a seventh). */
static int32_t sd_seg_toggle(SdPattern *p, uint32_t step)
{
    uint8_t b[SD_SEGS + 2u];
    uint32_t sc, j, k;
    if (step == 0u || step >= p->len) return -1;
    sc = sd_seg_bounds(p, b);
    for (j = 1; j < sc; j++)
        if (b[j] == step) {
            for (k = j; k < sc; k++) b[k] = b[k + 1u];
            for (k = j; k + 1u < sc; k++) sd_seg_copy(p, k, k + 1u);
            sd_seg_store(p, b, sc - 1u);
            return (int32_t)(j - 1u);
        }
    if (sc >= SD_SEGS) return (int32_t)sd_seg_of(p, step);
    for (j = 0; j + 1u < sc && b[j + 1u] <= step; j++) {}
    for (k = sc + 1u; k > j + 1u; k--) b[k] = b[k - 1u];
    b[j + 1u] = (uint8_t)step;
    for (k = sc; k > j + 1u; k--) sd_seg_copy(p, k, k - 1u);
    sd_seg_copy(p, j + 1u, j);
    sd_seg_store(p, b, sc + 1u);
    return (int32_t)(j + 1u);
}

static void sd_seg_even(SdPattern *p)           /* the segments as near equal as they go */
{
    uint8_t b[SD_SEGS + 1u];
    uint32_t sc = sd_seg_bounds(p, b), i;
    for (i = 1; i < sc; i++) b[i] = (uint8_t)((i * p->len + sc / 2u) / sc);
    sd_seg_store(p, b, sc);
}

/* LENGTH of a segment: its end moves, the ones after keep theirs as far as they can; the last one's start moves */
static void sd_seg_set_len(SdPattern *p, uint32_t seg, uint32_t len)
{
    uint8_t b[SD_SEGS + 1u];
    uint32_t sc = sd_seg_bounds(p, b), n = b[sc], k;
    int32_t d, end;
    if (seg >= sc || sc < 2u) return;
    if (seg + 1u == sc) {
        int32_t start = (int32_t)n - (int32_t)len;
        if (start < (int32_t)b[seg - 1u] + 1) start = (int32_t)b[seg - 1u] + 1;
        if (start > (int32_t)n - 1) start = (int32_t)n - 1;
        b[seg] = (uint8_t)start;
        sd_seg_store(p, b, sc);
        return;
    }
    end = (int32_t)b[seg] + (int32_t)(len ? len : 1u);
    if (end > (int32_t)n - (int32_t)(sc - 1u - seg)) end = (int32_t)n - (int32_t)(sc - 1u - seg);
    d = end - (int32_t)b[seg + 1u];
    for (k = seg + 1u; k < sc; k++) b[k] = (uint8_t)((int32_t)b[k] + d);
    for (k = sc - 1u; k > seg + 1u; k--)
        if (b[k] > b[k + 1u] - 1u) b[k] = (uint8_t)(b[k + 1u] - 1u);
    sd_seg_store(p, b, sc);
}

static void sd_seg_rescale(SdPattern *p, uint32_t old_len)   /* a new LENGTH scales the segments with it */
{
    uint8_t b[SD_SEGS + 1u];
    uint32_t sc, i, n = p->len, keep = p->len;
    if (!old_len || old_len == n) return;
    p->len = (uint8_t)old_len;
    sc = sd_seg_bounds(p, b);
    p->len = (uint8_t)keep;
    if (sc > n) sc = n;
    for (i = 1; i < sc; i++) {
        uint32_t v = (b[i] * n + old_len / 2u) / old_len;
        if (v < (uint32_t)b[i - 1u] + 1u) v = b[i - 1u] + 1u;
        if (v > n - (sc - i)) v = n - (sc - i);
        b[i] = (uint8_t)v;
    }
    b[sc] = (uint8_t)n;
    sd_seg_store(p, b, sc);
}

/* a note of the scale: degree d (any integer) from the key */
static uint32_t sd_scale_note(uint32_t key, uint32_t scale, int32_t d)
{
    uint32_t m = SD_SCALE_MASK[scale < SD_SCALES ? scale : SD_SC_CHROM], deg[12], n = 0, k;
    int32_t o, i, v;
    for (k = 0; k < 12u; k++) if (m >> k & 1u) deg[n++] = k;
    o = d >= 0 ? d / (int32_t)n : -(((int32_t)n - 1 - d) / (int32_t)n);
    i = d - o * (int32_t)n;
    v = (int32_t)key + o * 12 + (int32_t)deg[i];
    return (uint32_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

static int32_t sd_scale_degree(uint32_t key, uint32_t scale, uint32_t note)   /* the nearest degree at or under */
{
    int32_t d = 0;
    while (d > -64 && sd_scale_note(key, scale, d) > note) d--;
    while (d < 64 && sd_scale_note(key, scale, d + 1) <= note) d++;
    return d;
}

static uint32_t sd_pattern_width(const SdPattern *p)   /* the most notes a step plays at once (0: no notes) */
{
    uint32_t k, w = 0, n = p->len < SD_PSTEPS ? p->len : SD_PSTEPS;
    for (k = 0; k < n; k++) {
        const SdStep *s = &p->step[k];
        uint32_t c = 1u + (s->chord[0] != 0) + (s->chord[1] != 0) + (s->chord[2] != 0);
        if (s->type <= SD_ST_SLIDE && c > w) w = c;
    }
    return w;
}

static void sd_pattern_randomize(SdPattern *p, uint32_t *rng)   /* RANDOMIZE: a new line in the scale, now */
{
    uint32_t k, key = p->step[0].note;
    for (k = 0; k < p->len && k < SD_PSTEPS; k++) {
        SdStep *s = &p->step[k];
        uint32_t r = sd_seq_rand(rng);
        if (s->type == SD_ST_SKIP) continue;
        s->note = k ? (uint8_t)sd_scale_note(key, p->scale, (int32_t)(r % 15u) - 3) : (uint8_t)key;   /* step 1: the key */
        s->type = k && ((r >> 4) & 3u) == 0u ? SD_ST_REST : k && (r >> 8) % 10u == 0u ? SD_ST_SLIDE : SD_ST_NOTE;
        if (k && s->type == SD_ST_NOTE && (r >> 12) % 10u == 0u) s->type = SD_ST_TIE;
        s->vel = (uint8_t)(k % 4u == 0u ? 120u : 80u + (r >> 16) % 30u);
        s->ratchet = (uint8_t)(s->type == SD_ST_NOTE && (r >> 20) % 16u == 0u ? 2u : 1u);   /* now and then a double */
    }
}

/* A new pattern: the loop the engine played before the sequencer (SD_SEQ_LINE: A2, rests, 55 %), four segments of
 * four, forwards once each: it plays as a plain sequencer and is ready to change */
static void sd_pattern_default(SdPattern *p)
{
    static const int8_t LINE[16] = {0, 12, -1, 7, 0, 10, -1, 3, 0, 12, 15, -1, 7, 5, 3, -1};
    uint32_t k;
    uint8_t *z = (uint8_t *)p;
    for (k = 0; k < sizeof *p; k++) z[k] = 0;
    for (k = 0; k < SD_PSTEPS; k++) {
        int8_t s = LINE[k % 16u];
        p->step[k].note = (uint8_t)(45 + (s < 0 ? 0 : s));
        p->step[k].vel = k % 4u == 0u ? 120u : 90u;
        p->step[k].gate = 55;
        p->step[k].type = s < 0 ? SD_ST_REST : SD_ST_NOTE;
        p->step[k].ratchet = 1;
        p->step[k].prob = 100;
    }
    p->len = 16;
    p->div = 2;
    p->swing = 50;
    p->seg_count = 4;
    p->seg_split[0] = 4; p->seg_split[1] = 8; p->seg_split[2] = 12;
    for (k = 0; k < SD_SEGS; k++) p->seg_rep[k] = 1;
    p->scale = SD_SC_MINOR;
}
static void sd_pattern_empty(SdPattern *p)      /* the default, every step a rest: track B's start */
{
    uint32_t k;
    sd_pattern_default(p);
    for (k = 0; k < SD_PSTEPS; k++) p->step[k].type = SD_ST_REST;
}
#endif
