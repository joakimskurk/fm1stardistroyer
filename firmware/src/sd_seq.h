/* SPDX-License-Identifier: GPL-3.0-only */
/* Star Distroyer's sequencer: the pattern and what edits it (Sequencer Plan, v0.3).
 *
 * A step sequencer like the stock FM-1's, up to 64 steps, with segments: the pattern is cut into up to
 * six segments A-F, each played REPEAT 1-4 times in its ORDER (FWD REV P-P RND) or SKIPped. A step is a NOTE (with
 * SLIDE: a glide into it, no new attack), a TIE (the note before held on), a REST, or SKIPped (out of the pattern).
 * A NOTE can RATCHET (2 or 3 hits in its time, each softer, never closer than 50 ms: not too extreme and fast) and
 * has a PROB, the chance it plays at all (else a rest).
 * RANDOMIZE rewrites the steps now. RADAR is not the pattern's (rewriting the loop, it changed it in ways hard to
 * follow): it is a modulation source in the patch (sd_engine.h), and its NOTES moves track A's notes in the scale as
 * they play, the pattern as it was written.
 *
 * Everything here is plain data and functions on it: the main loop edits a pattern with them, the audio plays a copy
 * (sd_engine.h: the hand-over, the traversal, the timing). The segment editing: the bounds, a cut or a
 * join (toggle start), EVEN, a segment's length, the rescale to a new pattern length. */
#ifndef SD_SEQ_H
#define SD_SEQ_H
#include <stdint.h>
#ifndef SD_HOT
#define SD_HOT                                  /* (sd_engine.h's: the audio's code kept in one place) */
#endif

#define SD_PSTEPS 64u                           /* steps a pattern at most: four banks of 16 on the white keys */
#define SD_SEGS 6u                              /* segments A-F */
enum { SD_ST_NOTE, SD_ST_SLIDE, SD_ST_TIE, SD_ST_REST, SD_ST_SKIP, SD_STYPES };   /* a step's type */
enum { SD_SO_FWD, SD_SO_REV, SD_SO_PP, SD_SO_RND, SD_SORDERS };                  /* a segment's order */
enum { SD_SC_MINOR, SD_SC_MAJOR, SD_SC_DORIAN, SD_SC_PHRYG, SD_SC_PMIN, SD_SC_PMAJ, SD_SC_CHROM, SD_SCALES };
#define SD_SDIVS 12u                            /* DIV: 1/32 1/16T 1/16 1/8T 1/8 1/4, then 1/64 1/32T 1/16D 1/4T 1/8D 1/2
                                                   (added after the six, so a pattern's index keeps its meaning) */
static const uint8_t SD_SDIV_NUM[SD_SDIVS] = {1, 2, 1, 4, 2, 4, 1, 1, 3, 8, 3, 8},
                     SD_SDIV_DEN[SD_SDIVS] = {2, 3, 1, 3, 1, 1, 4, 3, 2, 3, 1, 1};   /* x a 16th */
static const uint8_t SD_SDIV_ORDER[SD_SDIVS] = {6, 7, 0, 1, 2, 3, 8, 4, 9, 10, 5, 11};   /* fastest to slowest: the
                                                   knobs step them so */
#define SD_TUR_DIVS 6u                          /* RADAR's DIV: the first six (its field holds 3 bits) */

typedef struct {
    uint8_t note;                               /* MIDI */
    uint8_t vel;                                /* 1..127 */
    uint8_t gate;                               /* 1..SD_GATE_MAX % of the step; 100: the whole step, up to the next
                                                   (2026-11-22, the user: "GATE should not go past 100 %, then it is
                                                   right up to the next note"; 101..200, 2026-11-18's "a little longer",
                                                   read as 100) */
    uint8_t type;                               /* SD_ST_* */
    uint8_t ratchet;                            /* 1..3 hits in the step (0 reads as 1) */
    uint8_t prob;                               /* 0..100 %: the chance it plays (the default: 100) */
    int8_t chord[3];                            /* a chord: up to three more notes, semitones
                                                   from note (0: none); they move with it (RADAR's NOTES, RND, transpose) */
} SdStep;
#define SD_GATE_MAX 100u
#define SD_RATCHET_MAX 3u
#define SD_RATCHET_MIN_SAMPLES 2205u            /* 50 ms: hits never closer */

typedef struct {
    SdStep step[SD_PSTEPS];
    uint8_t len;                                /* 1..64 */
    uint8_t div;                                /* SD_SDIV index; 2 = 1/16 */
    uint8_t swing;                              /* 50..75 %: the first of each pair of steps that much of the pair */
    uint8_t root;                               /* the key's root when set (SD_ROOT_SET | 0..11, C..B: GLO + SELECT
                                                   on SEQ, since 2026-11-16); without the bit, step 1's note is the root,
                                                   as for every pattern before (the low bits then RADAR's CHANGE of
                                                   when it was the pattern's: unread). sd_pat_key */
    uint8_t seg_count;                          /* 1..6 */
    uint8_t seg_split[SD_SEGS - 1u];            /* where segments 2.. start (steps, rising) */
    uint8_t seg_rep[SD_SEGS];                   /* 1..4 */
    uint8_t seg_order[SD_SEGS];                 /* SD_SO_* */
    uint8_t seg_skip;                           /* bit k: segment k is skipped */
    uint8_t scale;                              /* SD_SC_*: what RANDOMIZE and RADAR's NOTES choose notes from; the
                                                   key's root: root, or the first step's note */
    uint8_t old_tur, old_range;                 /* (the old RADAR's switch, RESTS and RANGE: unread, as above) */
} SdPattern;

#define SD_ROOT_SET 0x80u
/* the key a pattern is in, as a note: its root (set) at or below step 1's note, so the octave stays the line's (RND
   writes round it); unset, step 1's note. Only its place in the octave matters to the scales */
SD_HOT static uint32_t sd_pat_key(const SdPattern *p)
{
    uint32_t n0 = p->step[0].note, r;
    if (!(p->root & SD_ROOT_SET)) return n0;
    r = (n0 % 12u + 12u - (p->root & 15u) % 12u) % 12u;
    return n0 >= r ? n0 - r : n0 + 12u - r;
}

/* ORBIT, the song (a sequencer over the sequencer, 2026-11-19: the user, "a song builder"): 1..16 steps, each LOOPS
 * loops of the master long (1..8: a step changes at its one), each naming the pattern SEQ A and SEQ B play there (0:
 * as they play, the player's; 1..32 the saved P01..P32, loaded at the one from their step 1) and the drums' beat (a
 * place D01..D16, the drums on; or off; at the bar's one), and a change from where
 * the player is: SEQ A's and B's transpose, the LOOPER's and the frozen tail's pitch (2026-11-22, the user: "I miss
 * pitch on the loop and FREEZE in ORBIT"), two free lanes (each its matrix DEST), the four macros (0 changes
 * nothing). The lanes and the macros jump at a step's start or glide through it to the next step's value (GLIDE, a
 * lane each: lane 1, lane 2, the macros). At the end: back to step 1, or PLAY stops (END). Saved as a song of its
 * own (16 places; until 2026-11-19 with SEQ A's pattern, 8 steps, one RATE for all) */
#define SD_ORB_STEPS 16u
#define SD_ORB_LOOPS_MAX 8u
enum { SD_OG_L1 = 1, SD_OG_L2 = 2, SD_OG_MAC = 4 };   /* GLIDE's bits */
enum { SD_OE_LOOP, SD_OE_STOP, SD_OES };        /* END */
typedef struct {
    uint8_t on, len, end, glide;                /* on; 1..16 (0 reads as 4); SD_OE_*; SD_OG_* */
    uint8_t dst[2], pad[2];                     /* the free lanes' destinations (SD_MD_*, 0 none) */
    int8_t xa[SD_ORB_STEPS], xb[SD_ORB_STEPS];  /* XPOSE A, B: -24..24 semitones */
    int8_t lane[2][SD_ORB_STEPS];               /* the free lanes: -100..100 % */
    int8_t mac[4][SD_ORB_STEPS];                /* MUTATE MOVE BREAK MACRO 4: -99..99 */
    uint8_t pat[2][SD_ORB_STEPS];               /* SEQ A's, B's pattern: 0 as it plays, 1..32 P01..P32 */
    uint8_t loops[SD_ORB_STEPS];                /* 1..8 loops of the master (0 reads as 1) */
    uint8_t drm[SD_ORB_STEPS];                  /* the drums' beat: 0 as they play, 1..16 D01..D16 (on), 17 off */
    uint8_t tail[20];                           /* LOOP's and FREEZE's pitch at each step, -12..12 semitones, 5 bits
                                                   each (two's complement), LOOP's sixteen then FREEZE's (sd_orb_tail;
                                                   packed: the songs' RAM, 2026-11-22) */
} SdOrbit;
#define SD_ORB_TAIL_B 20u                       /* (the songs saved before it: their size less this, the rest 0) */
static int32_t sd_orb_tail(const SdOrbit *o, uint32_t w, uint32_t s)   /* w 0 LOOP, 1 FREEZE: step s's semitones */
{
    uint32_t b = ((w & 1u) * SD_ORB_STEPS + s % SD_ORB_STEPS) * 5u, i = b >> 3, sh = b & 7u, v = (uint32_t)o->tail[i] >> sh;
    int32_t x;
    if (sh > 3u) v |= (uint32_t)o->tail[i + 1u] << (8u - sh);
    v &= 31u;
    x = v >= 16u ? (int32_t)v - 32 : (int32_t)v;
    return x < -12 ? -12 : x > 12 ? 12 : x;
}
static void sd_orb_tail_set(SdOrbit *o, uint32_t w, uint32_t s, int32_t x)
{
    uint32_t b = ((w & 1u) * SD_ORB_STEPS + s % SD_ORB_STEPS) * 5u, i = b >> 3, sh = b & 7u, k;
    uint32_t v = (uint32_t)(x < -12 ? -12 : x > 12 ? 12 : x) & 31u;
    for (k = 0; k < 5u; k++, sh++) {            /* (a bit at a time: no byte past the end touched) */
        uint32_t at = i + (sh >> 3), bit = sh & 7u;
        o->tail[at] = (uint8_t)((o->tail[at] & ~(1u << bit)) | (v >> k & 1u) << bit);
    }
}
#define SD_ORB_LEN(o) ((o)->len ? (o)->len > SD_ORB_STEPS ? SD_ORB_STEPS : (uint32_t)(o)->len : 4u)
#define SD_ORB_LOOPS(o, s) ((o)->loops[(s) % SD_ORB_STEPS] ? (o)->loops[(s) % SD_ORB_STEPS] > SD_ORB_LOOPS_MAX ? \
                            SD_ORB_LOOPS_MAX : (uint32_t)(o)->loops[(s) % SD_ORB_STEPS] : 1u)

/* the scales as the semitones they hold, a bit each from the key */
static const uint16_t SD_SCALE_MASK[SD_SCALES] = {0x5AD, 0xAB5, 0x6AD, 0x5AB, 0x4A9, 0x295, 0xFFF};

SD_HOT static uint32_t sd_seq_rand(uint32_t *s) { *s = *s * 1103515245u + 12345u; return *s >> 8; }   /* 24 bits */

/* the segments' boundaries: b[0] = 0 .. b[sc] = len, each at least one step (build_bounds) */
SD_HOT static uint32_t sd_seg_bounds(const SdPattern *p, uint8_t b[SD_SEGS + 1u])
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
    uint32_t k, key = sd_pat_key(p);
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

/* A note on the steps: its NOTE (or SLIDE) and the TIEs after it (STEP's MORE: MOVE and LENGTH). The one heard at
 * step s: its first step, or -1 (a REST, a SKIP, a TIE after nothing) */
SD_HOT static int32_t sd_note_at(const SdPattern *p, uint32_t s)
{
    if (s >= p->len || s >= SD_PSTEPS) return -1;
    while (p->step[s].type == SD_ST_TIE) { if (!s) return -1; s--; }
    return p->step[s].type <= SD_ST_SLIDE ? (int32_t)s : -1;
}
static uint32_t sd_note_len(const SdPattern *p, uint32_t s)   /* its steps: its own and the TIEs after it */
{
    uint32_t n = 1;
    while (s + n < p->len && p->step[s + n].type == SD_ST_TIE) n++;
    return n;
}
/* MOVE: the note at s a step earlier (d < 0) or later, inside the pattern (no wrap). A join stays a join: played
 * legato into the next note it keeps its end and only its start moves (it gets longer or shorter); else the whole
 * note moves. The step it leaves: a TIE where the note before was held into it, else a REST. Another note's first
 * step is never taken. Returns the note's new first step, or -1 (no room) */
static int32_t sd_note_move(SdPattern *p, uint32_t s, int32_t d)
{
    uint32_t n = p->len < SD_PSTEPS ? p->len : SD_PSTEPS, l = sd_note_len(p, s), i;
    uint32_t in = s > 0u && p->step[s - 1u].type <= SD_ST_TIE, out = s + l < n && p->step[s + l].type <= SD_ST_SLIDE;
    if (d > 0) {
        if (s + l >= n || p->step[s + l].type == SD_ST_SKIP) return -1;
        if (out) {                              /* legato out: the start moves, the end stays */
            if (l < 2u) return -1;
            p->step[s + 1u] = p->step[s];
        } else {                                /* a REST after it: the whole note */
            for (i = s + l; i > s; i--) p->step[i] = p->step[i - 1u];
        }
        p->step[s].type = (uint8_t)(in ? SD_ST_TIE : SD_ST_REST);
        return (int32_t)s + 1;
    }
    if (!s || p->step[s - 1u].type <= SD_ST_SLIDE || p->step[s - 1u].type == SD_ST_SKIP) return -1;
    for (i = s - 1u; i + 1u < s + l; i++) p->step[i] = p->step[i + 1u];
    p->step[s + l - 1u].type = (uint8_t)(out ? SD_ST_TIE : SD_ST_REST);   /* (legato out: the end stays) */
    return (int32_t)s - 1;
}
/* LENGTH: the note at s a step longer (over a REST: never into another note, nor past the pattern's end) or shorter
 * (its last TIE a REST; one step at least). Returns its new length, or 0 (no room) */
static uint32_t sd_note_resize(SdPattern *p, uint32_t s, int32_t d)
{
    uint32_t l = sd_note_len(p, s), e = s + l;
    if (d > 0) {
        if (e >= p->len || e >= SD_PSTEPS || p->step[e].type != SD_ST_REST) return 0;
        p->step[e].type = SD_ST_TIE;
        return l + 1u;
    }
    if (l < 2u) return 0;
    p->step[e - 1u].type = SD_ST_REST;
    return l - 1u;
}
#endif
