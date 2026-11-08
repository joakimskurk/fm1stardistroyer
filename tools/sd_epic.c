/* SPDX-License-Identifier: GPL-3.0-only */
/* The second film cue. The user, 2026-11-05, asked for the whole instrument, much more extreme: a sci-fi
 * soundtrack that is immensely big, beautiful and hard and distorted at the same time, but melancholy and
 * beautiful.
 * On the first render they asked for "a proper filter on the melody at the start, and plenty of space". Then:
 * "drop the pew-pew laser, have more parts and higher intensity instead: we are after goosebumps and a wall of
 * sound".
 * Original material: E minor, 70 BPM, 37 bars (~127 s). The last climax goes up a third, to G major, the release.
 * (A key change a tone up, via a C#7 pivot, was tried and dropped: the user heard it as not working.)
 *
 *   sd_epic OUT.wav [STEMS_DIR] [CELLO_PARAMS]   (CELLO_PARAMS: key-value lines in place of STRINGS, for tuning)
 *
 * Fifteen parts, each its own engine as an FM-1 would play it, each on a stock patch (sd_patches.h: STRINGS and
 * the twelve after it are this cue's); only the notes, the macros' moves, the cello's opening and the mix are
 * this tool's:
 *   CELLO    STRINGS, the bowed cello (a saw through the Rider's body, HARM): the melody, the heart.
 *            At the start it is filtered (TAME and DESTROY's upper bands) and opens over eight bars, in a long
 *            hall with a dotted-quarter delay.
 *   CELLO 2  the same cello, a harmony under the melody (the nearest chord tone a third or more below)
 *   LOW STR  the same cello on the roots, a long note a chord: the section's floor
 *   PAD      HALO (NEBULA in a big hall), POLY: the chords, far back (DROWN)
 *   CHOIR    AURORA (GLASS, slow), POLY: the chords' tones an octave up, clean, far back
 *   HORNS    TITAN (DISTROY, swelling), POLY: the chords in the low middle, the top band destroyed, a wall from the
 *            first climax
 *   OSTINATO DRIVE (ION) on the sequencer: 16ths on the chord's tones
 *   BASS     UNDERTOW (ION, dry) on the sequencer: eighths on the root
 *   PERC     IMPACT (ORBITAL, dry) on the sequencer: taiko, denser in the climaxes, rolling 16ths at the last
 *   DRONE    BRAAM (DISTROY) an octave under the root: a drone at first, then a hit on every chord. A once-a-hit SAW on
 *            D SPLIT drops the destroyed band through the spectrum.
 *   SCREAM   SCREAM (BRAAM falling further) on the root and fifth, BREAK up
 *   LEAD     BEACON (TOUR) an octave over the cello in the climaxes, far back, its top tamed
 *   SUB      DEPTH, a sine an octave under the root: the depth
 *   ARP      SHARDS (GLASS, short) on the sequencer: 32nds up and down, high, in the release
 *   DARP     PRISM (SHARDS, ringing) on the sequencer: 16ths falling from two octaves up, in the release
 * The lows stay clean (the sub, ION's round low, the cellos' bodies); the distortion lives in the top bands.
 * Form, chords two bars each:
 *   0-7    Em C G D: the drone, the sub, the pad, the cello alone, closed and far, opening
 *   8-15   Em C Am B: the second cello, the low strings, the ostinato and the drums, the bass, then the choir
 *   16-23  Em C G D: the first climax, everything in, braams and horns on every chord
 *   24-31  G D Em C: the release. The D before it is G's V, so it lands as a cadence. A new melody a third up, all at
 *          its fullest, the arps on top.
 *   32-36  Am Em: one last hit, then the cello alone in the hall, down to the low E
 * Before each climax the rhythm and the drones drop out for the bar's last beat: the breath before the hit. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_params.h"

#define BPM 70u
#define BAR (SD_FS * 60u * 4u / BPM)            /* frames a bar */
#define BARS 37u
#define BLOCK 256u
enum { T_CELLO, T_CELLO2, T_LOWSTR, T_PAD, T_CHOIR, T_HORNS, T_OST, T_BASS, T_PERC, T_DRONE, T_SCREAM, T_LEAD,
       T_SUB, T_ARP, T_DARP, NT };
static const char *const TNAME[NT] = {"cello", "cello2", "lowstr", "pad", "choir", "horns", "ostinato", "bass",
                                      "perc", "drone", "scream", "lead", "sub", "arp", "darp"};

typedef struct {
    SdEngine e;
    SdSpaceMem sm;
    SdPatch p;
    SdPattern q;
    uint8_t held[4];
    uint32_t nheld;
    uint16_t l2, l3;                            /* the cellos: DESTROY's upper band levels as programmed */
} Track;
static Track tr[NT];

/* the chords: 0 Em, 1 C, 2 G, 3 D, 4 Am, 5 B; voicings, roots, the ostinato's tones */
static const uint8_t CHORD[6][4] = {{52, 55, 59, 64}, {48, 55, 60, 64}, {50, 55, 59, 62}, {50, 54, 57, 62},
                                    {45, 52, 57, 60}, {47, 51, 54, 59}};
static const uint8_t ROOT[6] = {40, 36, 43, 38, 33, 35};
static const uint8_t TONES[6][3] = {{64, 67, 71}, {60, 64, 67}, {62, 67, 71}, {62, 66, 69}, {60, 64, 69},
                                    {63, 66, 71}};
static const uint8_t PROG[BARS] = {0, 0, 1, 1, 2, 2, 3, 3, 0, 0, 1, 1, 4, 4, 5, 5, 0, 0, 1, 1, 2, 2, 3, 3,
                                   2, 2, 3, 3, 0, 0, 1, 1, 4, 4, 0, 0, 0};
static uint32_t chord_at(uint32_t bar) { return PROG[bar < BARS ? bar : BARS - 1u]; }
static int chord_changes(uint32_t bar)          /* a new chord on this bar's one */
{
    return bar == 0u || chord_at(bar) != chord_at(bar - 1u);
}

/* the melody, in beats: legato */
typedef struct { uint16_t at, len; uint8_t note, vel; } Ev;
static const Ev MA[] = {   /* A, Em C G D: a question */
    {4, 3, 59, 84}, {7, 1, 62, 76}, {8, 4, 64, 96}, {12, 2, 67, 92}, {14, 2, 66, 82}, {16, 6, 62, 90},
    {22, 2, 59, 80}, {24, 4, 57, 88}, {28, 4, 54, 78},
    /* B, Em C Am B: it rises, the B major's D# aching */
    {32, 2, 55, 88}, {34, 2, 59, 94}, {36, 4, 64, 104}, {40, 3, 67, 110}, {43, 1, 66, 90}, {44, 4, 64, 100},
    {48, 4, 60, 100}, {52, 2, 64, 96}, {54, 2, 69, 116}, {56, 4, 66, 112}, {60, 2, 63, 100}, {62, 2, 66, 106}};
static const Ev MC[] = {   /* the first climax, Em C G D, at beat 64 */
    {0, 4, 67, 127}, {4, 2, 66, 112}, {6, 2, 64, 106}, {8, 6, 64, 116}, {14, 2, 67, 112}, {16, 4, 62, 116},
    {20, 2, 67, 122}, {22, 2, 66, 110}, {24, 6, 66, 120}, {30, 2, 62, 100}};
static const Ev MG[] = {   /* the release, G D Em C, at beat 96: the line a third up, at its fullest */
    {0, 4, 67, 127}, {4, 2, 69, 118}, {6, 2, 67, 112}, {8, 6, 66, 122}, {14, 2, 69, 116}, {16, 4, 71, 127},
    {20, 2, 66, 112}, {22, 2, 64, 108}, {24, 4, 67, 127}, {28, 2, 64, 110}, {30, 2, 62, 100}};
static const Ev ME[] = {   /* home, Am Em (at beat 128), down to the low tonic */
    {0, 4, 64, 92}, {4, 2, 60, 82}, {6, 2, 59, 78}, {8, 2, 55, 72}, {10, 8, 52, 64}};
#define NA (sizeof MA / sizeof MA[0])
#define NC (sizeof MC / sizeof MC[0])
#define NG (sizeof MG / sizeof MG[0])
#define NE (sizeof ME / sizeof ME[0])
#define NMAX 64u
static Ev MEL[NMAX], HAR[NMAX], LOW[NMAX], LEAD[NMAX];
static uint32_t nmel, nhar, nlow, nlead;

static void build_lines(void)
{
    uint32_t i, k;
    for (i = 0; i < NA; i++) MEL[nmel++] = MA[i];
    for (k = 0; k < 2u; k++)
        for (i = 0; i < (k ? NG : NC); i++) {
            Ev e = k ? MG[i] : MC[i];
            e.at = (uint16_t)(e.at + 64u + 32u * k);
            MEL[nmel++] = e;
            e.note = (uint8_t)(e.note + 12u);   /* the lead: an octave over */
            e.vel = 100;
            LEAD[nlead++] = e;
        }
    for (i = 0; i < NE; i++) { Ev e = ME[i]; e.at = (uint16_t)(e.at + 128u); MEL[nmel++] = e; }
    for (i = 0; i < nmel; i++) {                /* the harmony: from bar 8, the nearest chord tone 3+ semitones under */
        uint32_t bar = MEL[i].at / 4u, ch = chord_at(bar), n, best = 0;
        if (bar < 8u) continue;
        for (n = 45u; n + 3u <= MEL[i].note; n++) {
            uint32_t j;
            for (j = 0; j < 4u; j++) if (CHORD[ch][j] % 12u == n % 12u) best = n;
        }
        if (best) { Ev e = MEL[i]; e.note = (uint8_t)best; e.vel = (uint8_t)(e.vel > 20u ? e.vel - 12u : e.vel); HAR[nhar++] = e; }
    }
    for (k = 8u; k < 34u; k++) {                /* the low strings: the root a chord, in the cello's range */
        uint32_t r = ROOT[chord_at(k)];
        Ev e = {(uint16_t)(4u * k), 4, (uint8_t)(r < 36u ? r + 12u : r), (uint8_t)(k >= 16u ? 118u : 96u)};
        if (!chord_changes(k) && nlow) { LOW[nlow - 1u].len = (uint16_t)(LOW[nlow - 1u].len + 4u); continue; }
        LOW[nlow++] = e;
    }
    LOW[nlow - 1u].len = (uint16_t)(LOW[nlow - 1u].len + 8u);   /* (the last, held into the ending) */
}

static void note_on(Track *t, uint32_t n, uint32_t v)
{
    sd_engine_note(&t->e, n, v);
    if (t->nheld < 4u) t->held[t->nheld++] = (uint8_t)n;
}
static void all_off(Track *t)
{
    uint32_t i;
    for (i = 0; i < t->nheld; i++) sd_engine_note(&t->e, t->held[i], 0);
    t->nheld = 0;
}
static void ost_pattern(uint32_t ch)   /* 16ths over the chord's tones: up, a skip, down */
{
    static const int8_t IDX[16] = {0, 1, 2, 3, 1, 2, 4, 2, 0, 1, 2, 3, 2, 4, 3, 1};
    SdPattern *q = &tr[T_OST].q;
    uint32_t k;
    for (k = 0; k < 16u; k++) {
        int32_t i = IDX[k];
        q->step[k].note = (uint8_t)(TONES[ch][i % 3] - 12 + (i >= 3 ? 12 : 0));
        q->step[k].type = SD_ST_NOTE;
        q->step[k].vel = k % 4u == 0u ? 112u : 82u;
        q->step[k].gate = 45;
    }
    sd_engine_pattern(&tr[T_OST].e, q);
}
static void bass_pattern(uint32_t ch)  /* eighths on the root, an octave jump and a push */
{
    static const int8_t B[8] = {0, 0, 12, 0, 0, 12, 0, 7};
    SdPattern *q = &tr[T_BASS].q;
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        q->step[k].note = (uint8_t)(ROOT[ch] + B[k]);
        q->step[k].type = k == 3u ? SD_ST_SLIDE : SD_ST_NOTE;
        q->step[k].vel = k % 2u == 0u ? 118u : 92u;
        q->step[k].gate = 70;
    }
    sd_engine_pattern(&tr[T_BASS].e, q);
}
static void perc_pattern(uint32_t level)        /* taiko; 1: the climax's (more hits), 2: the release's, 3: its peak */
{
    static const uint8_t HIT[4][16] = {{1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1},
                                       {1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1},
                                       {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1},
                                       {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
    static const uint8_t NOTE[16] = {40, 0, 45, 40, 43, 0, 47, 45, 40, 0, 45, 43, 43, 40, 43, 47};
    SdPattern *q = &tr[T_PERC].q;
    uint32_t k;
    q->len = 16; q->div = 2; q->seg_count = 1;
    for (k = 0; k < 16u; k++) {
        q->step[k].type = HIT[level][k] ? SD_ST_NOTE : SD_ST_REST;
        q->step[k].note = NOTE[k] ? NOTE[k] : 40u;
        q->step[k].vel = k == 0u || k == 8u ? 127u : k % 4u == 0u ? 108u : 90u;
        q->step[k].gate = 40;
        q->step[k].prob = k == 14u || k == 15u ? (uint8_t)(70u + 15u * level) : 100u;
        q->step[k].ratchet = k == 15u || (level >= 2u && k == 7u) || (level == 3u && k % 4u == 3u) ? 2u : 1u;
        if (level == 3u && k % 2u) q->step[k].vel = 84u;   /* (the 16ths between: a roll under the hits) */
    }
    sd_engine_pattern(&tr[T_PERC].e, q);
}

static void arp_pattern(uint32_t ch)   /* 32nds up and down the chord's tones, two octaves up high */
{
    static const uint8_t UPDN[12] = {0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1};
    SdPattern *q = &tr[T_ARP].q;
    uint32_t k;
    for (k = 0; k < 32u; k++) {
        uint32_t i = UPDN[k % 12u];
        q->step[k].note = (uint8_t)(TONES[ch][i % 3u] + 12u * (1u + i / 3u));
        q->step[k].type = SD_ST_NOTE;
        q->step[k].vel = k % 8u == 0u ? 112u : k % 2u == 0u ? 88u : 72u;
        q->step[k].gate = 50;
    }
    sd_engine_pattern(&tr[T_ARP].e, q);
}

static void darp_pattern(uint32_t ch)  /* 16ths down the chord's tones from two octaves up: a clear falling line */
{
    SdPattern *q = &tr[T_DARP].q;
    uint32_t k;
    for (k = 0; k < 16u; k++) {
        uint32_t i = 5u - k % 6u;               /* 5 4 3 2 1 0 5 4 ...: the top tone of the upper octave down */
        q->step[k].note = (uint8_t)(TONES[ch][i % 3u] + 12u * (1u + i / 3u));
        q->step[k].type = SD_ST_NOTE;
        q->step[k].vel = k % 6u == 0u ? 124u : 100u - 4u * (k % 6u);
        q->step[k].gate = 70;
    }
    sd_engine_pattern(&tr[T_DARP].e, q);
}

static const SdPatch *patch_named(const char *name)   /* every part plays a stock patch (sd_patches.h) */
{
    uint32_t i;
    for (i = 0; i < SD_PATCHES && strcmp(SD_PATCH_NAME[i], name); i++) {}
    if (i == SD_PATCHES) { fprintf(stderr, "sd_epic: no patch %s\n", name); exit(1); }
    return &SD_PATCH[i];
}

static void setup(const char *cello_params)
{
    uint32_t i;
    static const char *const PATCH[NT] = {"STRINGS", "STRINGS", "STRINGS", "HALO", "AURORA", "TITAN", "DRIVE",
                                          "UNDERTOW", "IMPACT", "BRAAM", "SCREAM", "BEACON", "DEPTH", "SHARDS", "PRISM"};
    SdPatch cello = *patch_named("STRINGS");
    if (cello_params) {                         /* tuning: a cello from key-value lines on a plain saw, as sd_strings
                                                   builds it (tools/cello_v4.txt is STRINGS) */
        cello = SD_PATCH_SINE;
        cello.bank[0] = cello.bank[1] = cello.bank[2] = cello.bank[3] =
            (SdBankPatch){{{1, 0, SD_ROLE_C, 0, 52000}, {1, 0, SD_ROLE_C, 0, 0}, {1, 0, SD_ROLE_C, 0, 0},
                           {1, 0, SD_ROLE_C, 0, 0}}, 0, 0, 0, 0};
        cello.r_max = 5;
        if (params(cello_params, &cello)) exit(1);
    }
    for (i = 0; i < NT; i++) {
        Track *t = &tr[i];
        sd_engine_init(&t->e);
        memset(&t->sm, 0, sizeof t->sm);
        t->e.sm = &t->sm;
        t->p = i <= T_LOWSTR ? cello : *patch_named(PATCH[i]);
        t->l2 = t->p.d_level[2]; t->l3 = t->p.d_level[3];
        sd_pattern_default(&t->q);
        t->e.bpm = BPM;
    }
    tr[T_PAD].e.mode = SD_MODE_POLY;            /* (the voice mode is the player's, not the patch's) */
    tr[T_CHOIR].e.mode = SD_MODE_POLY;
    tr[T_HORNS].e.mode = SD_MODE_POLY;
    tr[T_ARP].q.len = 32; tr[T_ARP].q.div = 0; tr[T_ARP].q.seg_count = 1;
    tr[T_DARP].q.len = 16; tr[T_DARP].q.div = 2; tr[T_DARP].q.seg_count = 1;
    tr[T_OST].q.len = 16; tr[T_OST].q.div = 2; tr[T_OST].q.seg_count = 1;
    tr[T_BASS].q.len = 8; tr[T_BASS].q.div = 4; tr[T_BASS].q.seg_count = 1;
    ost_pattern(0);
    bass_pattern(0);
    perc_pattern(0);
    arp_pattern(0);
    darp_pattern(0);
    for (i = 0; i < NT; i++) {
        int32_t z[2u * SD_SUB];
        sd_engine_patch(&tr[i].e, &tr[i].p);
        memset(z, 0, sizeof z);
        sd_engine_render(&tr[i].e, z, SD_SUB);
    }
    build_lines();
}

static float gain_of(uint32_t t, double bar)    /* each part through the form, with fades */
{
    static const struct { double in0, in1, out0, out1; float g; } A[NT] = {
        {0, 0.1, 36, 37, 0.30f},                /* cello (its patch is hot: -10 dB) */
        {8, 10, 34, 36, 0.20f},                 /* cello 2 */
        {8, 10, 34, 36.5, 0.22f},               /* low strings */
        {2, 4, 35, 37, 0.80f},                  /* pad */
        {12, 16, 33, 36, 0.75f},                /* choir */
        {16, 16.02, 33, 35, 0.85f},             /* horns */
        {8, 11, 31.8, 32, 0.45f},               /* ostinato (ION: much hotter than PULSAR was) */
        {12, 14, 31.8, 32, 0.70f},              /* bass */
        {8, 12, 32, 32.3, 0.85f},               /* drums */
        {0, 3, 33, 35, 0.95f},                  /* drone: a third till the climax */
        {16, 16.02, 33, 35, 0.95f},             /* scream */
        {16, 17, 31.5, 32.5, 0.75f},            /* lead */
        {0, 3, 35, 37, 0.36f},                  /* sub */
        {24, 26, 33, 35, 2.20f},                /* arp: the last part (GLASS short: quiet) */
        {26, 26.5, 33, 35, 2.60f}};             /* the falling arp: from the release's second chord, clear */
    double f;
    if (bar < A[t].in0 || bar >= A[t].out1) return 0;
    f = bar < A[t].in1 ? (bar - A[t].in0) / (A[t].in1 - A[t].in0) : 1.0;
    if (bar > A[t].out0) f *= (A[t].out1 - bar) / (A[t].out1 - A[t].out0);
    if (t == T_DRONE && bar < 16) f *= 0.30 + 0.25 * (bar >= 8 ? (bar - 8) / 8.0 : 0);
    if (t == T_CELLO) f *= bar < 8 ? 0.8 : bar < 16 ? 1.15 : bar < 32 ? 1.4 : 0.9;   /* it rises with the build */
    if ((t == T_BASS || t == T_PERC || t == T_PAD) && bar >= 16 && bar < 32) f *= 1.2;
    if (bar >= 24 && bar < 32 && (t == T_HORNS || t == T_CHOIR || t == T_SCREAM || t == T_PERC || t == T_CELLO2 ||
                                  t == T_LEAD || t == T_LOWSTR || t == T_DRONE || t == T_OST || t == T_BASS))
        f *= 1.1 + 0.6 * (bar - 24) / 8;        /* the release grows to its last bar (+5 dB over eight) */
    if ((t == T_OST || t == T_BASS || t == T_PERC || t == T_DRONE || t == T_SUB || t == T_SCREAM || t == T_HORNS) &&
        ((bar >= 15.75 && bar < 16) || (bar >= 27.75 && bar < 28)))
        f = 0;                                  /* the breath before the first climax and before the release's
                                                   peak (the release itself lands as a cadence, no break) */
    return (float)f * A[t].g;
}

static uint32_t rnd = 0xE51Cu;
static double rnd01(void) { rnd = rnd * 1103515245u + 12345u; return (double)(rnd >> 8 & 0xFFFF) / 65535.0; }

static void on_bar(uint32_t bar)
{
    uint32_t ch = chord_at(bar), k;
    int climax = bar >= 16u && bar < 32u;
    double build = bar < 8u ? 0.1 * bar / 8.0 : bar < 16u ? 0.1 + 0.6 * (bar - 8u) / 8.0 : bar < 24u ? 0.9
                 : bar < 32u ? 1.0 + 0.4 * (bar - 24u) / 8.0 : 0.35;   /* (past 1 in the release: BREAK grows) */
    if (chord_changes(bar) && bar < 35u) {      /* PAD from bar 2, CHOIR from 12, HORNS in the climaxes and the last hit */
        if (bar >= 2u) { all_off(&tr[T_PAD]); for (k = 0; k < 4u; k++) note_on(&tr[T_PAD], CHORD[ch][k], 72u + 10u * (uint32_t)climax); }
        if (bar >= 12u) { all_off(&tr[T_CHOIR]); for (k = 0; k < 3u; k++) note_on(&tr[T_CHOIR], TONES[ch][k] + 12u, 90u); }
        all_off(&tr[T_HORNS]);
        if (climax || bar == 32u) for (k = 0; k < 4u; k++) note_on(&tr[T_HORNS], CHORD[ch][k] - 12u, 120u);
    }
    if (bar == 35u) { all_off(&tr[T_PAD]); all_off(&tr[T_CHOIR]); }
    if (chord_changes(bar)) { ost_pattern(ch); bass_pattern(ch); arp_pattern(ch); darp_pattern(ch); }
    if (bar == 26u) tr[T_DARP].e.seq_run = 1;
    if (bar == 34u) tr[T_DARP].e.seq_run = 0;
    if (bar == 24u) tr[T_ARP].e.seq_run = 1;
    if (bar == 34u) tr[T_ARP].e.seq_run = 0;
    if (bar == 8u) { tr[T_OST].e.seq_run = 1; tr[T_PERC].e.seq_run = 1; }
    if (bar == 12u) tr[T_BASS].e.seq_run = 1;
    if (bar == 16u) perc_pattern(1);
    if (bar == 24u) perc_pattern(2);
    if (bar == 28u) perc_pattern(3);
    if (bar == 32u) { tr[T_OST].e.seq_run = 0; tr[T_BASS].e.seq_run = 0; tr[T_PERC].e.seq_run = 0; }
    /* the drone: E1 held from the start, then a hit on every chord; the scream on the root and fifth */
    if (bar == 0u) note_on(&tr[T_DRONE], 28, 80);
    if ((climax && chord_changes(bar)) || bar == 32u) {
        all_off(&tr[T_DRONE]); all_off(&tr[T_SCREAM]);
        note_on(&tr[T_DRONE], ROOT[ch] - 12u + (ROOT[ch] < 36u ? 12u : 0u), 127);
        note_on(&tr[T_SCREAM], ROOT[ch], 127);
        note_on(&tr[T_SCREAM], ROOT[ch] + 7u, 118);
    }
    if (bar == 33u) all_off(&tr[T_SCREAM]);
    if (bar == 34u) all_off(&tr[T_DRONE]);
    if (chord_changes(bar) && bar < 35u) {
        uint32_t r = ROOT[ch];
        all_off(&tr[T_SUB]);
        note_on(&tr[T_SUB], r - 12u + (r < 36u ? 12u : 0u), 110);
    }
    /* the macros: a step a bar, each part's own, rising with the build */
    for (k = 0; k < NT; k++) {
        SdPatch *p = &tr[k].p;
        if (k <= T_LOWSTR || k == T_SUB) continue;   /* the strings and the depth stay as they are */
        p->mac[0] = (uint16_t)(65535 * fmin(1, 0.10 * rnd01() + 0.55 * build));   /* MUTATE */
        p->mac[1] = (uint16_t)(65535 * fmin(1, 0.15 + 0.45 * rnd01()));            /* MOVE */
        p->mac[2] = (uint16_t)(65535 * fmin(1, (k == T_DRONE || k == T_SCREAM || k == T_HORNS ? 0.45 : 0.08)
                                               + 0.50 * build * rnd01()));       /* BREAK */
        p->mac[3] = (uint16_t)(65535 * fmin(1, 0.25 + 0.4 * rnd01()));            /* MACRO 4 */
        if (k == T_PAD || k == T_LEAD || k == T_CHOIR || k == T_ARP || k == T_DARP) {   /* far back, clean (their
                                                                                          MACRO 4 is DROWN), high */
            p->mac[2] = 0;
            p->mac[3] = (uint16_t)(65535 * (0.60 + 0.25 * rnd01()));
        }
        if (k == T_BASS || k == T_PERC) { p->mac[1] = 0; p->mac[3] = 0; }
        if (k == T_OST || k == T_BASS || k == T_ARP || k == T_DARP)   /* the pitched patterns: MUTATE's detune kept small */
            p->mac[0] = (uint16_t)(p->mac[0] / 4u);
        if (k == T_ARP || k == T_DARP) p->mac[0] = 0;          /* (up there any detune reads as out of tune: it measured +-30 cents) */
    }
}

static void play(Track *t, const Ev *ev, uint32_t n, double b0, double b1)   /* a line, beats b0..b1, legato */
{
    uint32_t i;
    for (i = 0; i < n; i++)                     /* the ons first: the next note on before the last one off */
        if (ev[i].at >= b0 && ev[i].at < b1) sd_engine_note(&t->e, ev[i].note, ev[i].vel);
    for (i = 0; i < n; i++) {
        double off = ev[i].at + ev[i].len;
        int again = i + 1 < n && ev[i + 1].at == off && ev[i + 1].note == ev[i].note;
        if (off >= b0 && off < b1 && !again) sd_engine_note(&t->e, ev[i].note, 0);
    }
}

/* The cellos' filter and space through the form (the user: "at the start the melody lacks a proper filter, and
   plenty of space"). TAME (1.2 kHz, resonant) and DESTROY's upper bands close the cello to ~700 Hz at the start
   and open it to its own 22000 over the first eight bars. The space is a long dark hall with a dotted-quarter
   ping-pong delay in A; it narrows to the patch's own hall through B, stays there for the climaxes, then opens
   again for the ending. */
static void cello_shape(Track *t, double bar)
{
    SdPatch *p = &t->p;
    double open = fmin(1, bar / 8.0), wet = bar < 8 ? 1 : bar < 16 ? 1 - 0.45 * (bar - 8) / 8 : bar < 32 ? 0.55 : fmin(1, 0.55 + 0.45 * (bar - 32) / 2);
    open = open * open * (3 - 2 * open);        /* (an S: it lingers closed, then blooms) */
    p->d_tame = (uint16_t)lround(65535 - (65535 - 22000) * open);
    p->d_level[2] = (uint16_t)lround(t->l2 * pow(10, -14 * (1 - open) / 20));   /* 750 Hz .. 2.4 kHz */
    p->d_level[3] = (uint16_t)lround(t->l3 * pow(10, -25 * (1 - open) / 20));   /* past 2.4 kHz */
    p->s_size = (uint16_t)lround(52000 + 13535 * wet);
    p->s_decay = (uint16_t)lround(44000 + 16000 * wet);
    p->s_rlevel = (uint16_t)lround(16000 + 22000 * wet);
    p->s_rtone = (uint16_t)lround(40000 - 16000 * wet);   /* darker when big */
    p->s_mode = SD_SP_SYNC | SD_SP_PP;
    p->s_time = 56000;                          /* a dotted quarter */
    p->s_fb = 36000; p->s_dtone = 18000;
    p->s_dlevel = (uint16_t)lround(26000 * wet);
}

static void put16(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)(v >> 8 & 255u), f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFFu); put16(f, v >> 16); }
static void header(FILE *f, uint32_t frames)
{
    fwrite("RIFF", 1, 4, f); put32(f, 36u + frames * 4u); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SD_FS); put32(f, SD_FS * 4u); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4u);
}

int main(int argc, char **argv)
{
    uint32_t frames = BARS * BAR, done = 0, k, i, bar_done = 0xFFFFFFFFu, clipped = 0;
    static int32_t buf[NT][2u * BLOCK];
    FILE *f, *stem[NT] = {0};
    double peak = 0, rms[NT] = {0}, env = 0, gmin = 1;
    if (argc < 2) { fprintf(stderr, "usage: sd_epic OUT.wav [STEMS_DIR] [CELLO_PARAMS]\n"); return 2; }
    if (!(f = fopen(argv[1], "wb"))) { perror(argv[1]); return 1; }
    header(f, frames);
    if (argc > 2 && argv[2][0])
        for (k = 0; k < NT; k++) {
            char path[512];
            snprintf(path, sizeof path, "%s/epic_%s.wav", argv[2], TNAME[k]);
            if ((stem[k] = fopen(path, "wb"))) header(stem[k], frames);
        }
    setup(argc > 3 ? argv[3] : NULL);
    while (done < frames) {
        uint32_t n = frames - done < BLOCK ? frames - done : BLOCK, bar = done / BAR;
        double b = (double)done / BAR, b0 = b * 4.0, b1 = (double)(done + n) / BAR * 4.0;
        if (bar != bar_done) { bar_done = bar; on_bar(bar); }
        play(&tr[T_CELLO], MEL, nmel, b0, b1);
        play(&tr[T_CELLO2], HAR, nhar, b0, b1);
        play(&tr[T_LOWSTR], LOW, nlow, b0, b1);
        play(&tr[T_LEAD], LEAD, nlead, b0, b1);
        for (k = T_CELLO; k <= T_LOWSTR; k++) cello_shape(&tr[k], b);
        for (k = 0; k < NT; k++) {
            sd_engine_patch(&tr[k].e, &tr[k].p);
            memset(buf[k], 0, sizeof buf[k]);
            sd_engine_render(&tr[k].e, buf[k], n);
        }
        for (i = 0; i < n; i++) {
            double s[2] = {0, 0}, x;
            uint32_t c;
            for (c = 0; c < 2u; c++)
                for (k = 0; k < NT; k++) {
                    double v = gain_of(k, b) * buf[k][2u * i + c] / 32768.0;
                    if (k == T_CELLO) v *= c ? 1.0 : 0.85;    /* (a little right of centre) */
                    if (k == T_CELLO2) v *= c ? 0.80 : 1.0;   /* (a little left) */
                    s[c] += v;
                    rms[k] += v * v;
                    if (stem[k]) put16(stem[k], (uint32_t)(uint16_t)(int16_t)lround(fmax(-1, fmin(1, v)) * 32767.0));
                }
            /* the bus: the master fader rides the form (A held back, rising through B, the second climax over the
               first), then a compressor on the peaks (over -5 dBFS at 4:1, 8 ms in, 250 ms out: the wall), +3 dB,
               and a tanh */
            {
                double ride = b < 8 ? 0.62 : b < 16 ? 0.62 + 0.16 * (b - 8) / 8 : b < 24 ? 0.78 : b < 32 ? 0.90 + 0.50 * (b - 24) / 8 : 0.75;
                s[0] *= ride; s[1] *= ride;
            }
            x = fmax(fabs(s[0]), fabs(s[1])) * 0.62;
            env += (x - env) * (x > env ? 1.0 - exp(-1.0 / (0.008 * SD_FS)) : 1.0 - exp(-1.0 / (0.25 * SD_FS)));
            {
                double over = 20 * log10(env + 1e-9) + 5, g = over > 0 ? pow(10, -over * (1 - 1 / 4.0) / 20) : 1;
                if (g < gmin) gmin = g;
                for (c = 0; c < 2u; c++) {
                    double y = s[c] * 0.62 * g * 1.41;
                    if (fabs(y) > peak) peak = fabs(y);
                    y = tanh(y);                /* (the drums' transients pass the compressor: rounded, never flat) */
                    if (y > 0.999) y = 0.999, clipped++;
                    if (y < -0.999) y = -0.999, clipped++;
                    put16(f, (uint32_t)(uint16_t)(int16_t)lround(y * 32767.0));
                }
            }
        }
        done += n;
    }
    if (ferror(f) | fclose(f)) { fprintf(stderr, "sd_epic: %s: a write failed (the disk full?)\n", argv[1]); return 1; }
    for (k = 0; k < NT; k++)
        if (stem[k] && (ferror(stem[k]) | fclose(stem[k]))) { fprintf(stderr, "sd_epic: a stem's write failed\n"); return 1; }
    printf("epic -> %s (%.1f s, %u BPM; mix peak %.2f before the soft clip, %u at the ceiling; the bus %.1f dB at most)\n",
           argv[1], frames / (double)SD_FS, BPM, peak, clipped, 20 * log10(gmin));
    for (k = 0; k < NT; k++) printf("  %-9s %6.1f dB RMS (whole piece)\n", TNAME[k], 10 * log10(rms[k] / (2.0 * frames) + 1e-12));
    return 0;
}
