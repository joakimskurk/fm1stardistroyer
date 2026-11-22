/* SPDX-License-Identifier: GPL-3.0-only */
/* ARMADA: one take with the drums (2026-11-21, the user: "a demo much more worked out musically, using more of the
 * functions, incredibly melodic and big and massive and sci-fi epic; the drums in it too"; then: "it must run as a
 * demo on the FM-1 too"). The FM-1's second demo (SETUP's ALGORITHM), recorded as SINGULARITY is.
 *
 *   sd_armada OUT.wav
 *
 * One FM-1 played by one person, one pass (tools/sd_play.h: one engine, one patch at a time, the app's controls at a
 * hand's speed, the voice rules, MASTER as platform.c applies it; nothing mixed). What sounds over what is the
 * FM-1's own: the plate's FREEZE holding a chord across a patch change, the looper holding a bar of the ostinato under
 * every patch after it, the delay's echoes, the drums beside the synth; POLY's four voices for SEQ A, SEQ B and the
 * arpeggiator at once. D minor, then E minor (SETUP's XPOSE on CHROM: ORBIT's lift a whole step), 96 BPM, 54 bars
 * (~2:20), one theme told three times:
 *   1-8    HORIZON  AURORA (saved wet): SEQ A's chords Dm Bb F C Dm Bb Gm Asus4, a bar each, MASTER rising, SHIMMER on
 *                   the plate; FREEZE on the last chord's tail
 *   9-16   LAUNCH   DRIVE: a D-pedal ostinato in 16ths on SEQ A under the frozen chords; the looper armed (1 BAR)
 *                   and holding its last bar from 13, SEQ A silent from there; the drums in (PULSE: a heartbeat, the
 *                   hats, the fourth a ZAP on RADAR's steps, RADAR playing its PERC TUNE); FREEZE let go; SEQ B's
 *                   bass, D Bb F C; the drums half-time (HALF)
 *   17-24  THEME    STRINGS (saved wet): the theme on SEQ A, SEQ B's roots; the loop's ostinato under it; the drums
 *                   half-time, the fourth a TOM now (its ENGINE), a pickup each bar
 *   25-32  ASCENT   VOYAGE (saved wet): a counter-line on SEQ A, the arpeggiator over two octaves on a held D A E (the
 *                   pedal's colour over every chord), the FILTER (MACROS' K4) dipped and opened over the eight bars;
 *                   the drums four to the floor from 27, a snare fill rolling in the last bar
 *   33-40  ARMADA   TITAN (saved wet): the theme in octaves, SEQ B driving eighths, the arp on; ORBIT stepping both
 *                   (its lanes on D SPLIT and the nav's X, BREAK and MOVE gliding up); RADAR's ACCENTS on the drums;
 *                   MOVE and BREAK up; the drums driving
 *   41-48  LIFT     ORBIT's second step: A and B a whole step up (E minor), the arp's hand a step up with them
 *                   (E B F#), the loop out (its F against E); the drums with the toms rolling; MUTATE; the arp out
 *                   in the last bar
 *   49-54  DRIFT    the final chord in the new key, E, its plate frozen, the drums off after one hit; the looper
 *                   holding that bar, backwards and an octave down, fading; FREEZE and DISTROY let go */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"

#define BAR(b) (((b) - 1) * 16.0)               /* the start of bar b (from 1), in sixteenths */
#define T(b, s) (BAR(b) + (s))

/* ---- the patterns the player saved (PRESETS on SEQ loads them) */
static SdPattern CHORDS, OSTI, EMPTY, BASS4, BASS8, THEME, COUNTER, THEME8, DRIVEB, FINAL_A, FINAL_B;
static SdOrbit ORB;
static SdPatch U01, U02, U03, U04, U05;         /* the player's own places (see patterns()) */
static SdDrumPat PULSE, HALF_Z, HALF_T, BUILD, FILL, DRIVE, LIFT, END;
static SdDrumMore FILL_M;
static SdDrumKit KIT;

static SdStep *st(SdPattern *p, uint32_t k, uint32_t gate)
{
    SdStep *s = &p->step[k];
    s->gate = (uint8_t)gate; s->ratchet = 1; s->prob = 100; s->vel = 92;
    return s;
}
static void begin(SdPattern *p, uint32_t len, uint32_t div)   /* div: SD_SDIV's index (2 1/16, 4 1/8) */
{
    sd_pattern_default(p);
    p->len = (uint8_t)len; p->div = (uint8_t)div; p->seg_count = 1;
    p->root = SD_ROOT_SET | 2u;                 /* (the key D, set: GLO + SELECT) */
    p->scale = SD_SC_MINOR;
}
static void notes(SdPattern *p, const int16_t *n, uint32_t len, uint32_t div, uint32_t gate, int8_t chord)   /* sd_play.h's
                                                   notation: RS, TI, a note | SL | AC; chord: a second note above */
{
    uint32_t i;
    begin(p, len, div);
    for (i = 0; i < len; i++) {
        SdStep *s = st(p, i, gate);
        if (n[i] == RS) { s->type = SD_ST_REST; continue; }
        if (n[i] == TI) { s->type = SD_ST_TIE; continue; }
        s->note = (uint8_t)(n[i] & 0xFF);
        s->type = n[i] & SL ? SD_ST_SLIDE : SD_ST_NOTE;
        s->chord[0] = chord; s->chord[1] = 0; s->chord[2] = 0;
        if (n[i] & AC) s->vel = 118;
    }
}
static void wet(SdPatch *p, uint32_t rl, uint32_t dc, uint32_t sz, uint32_t dl, uint32_t fb)   /* SPACE opened on its
                                                   pages and saved: R-LEVEL DECAY SIZE (SPACE), D-LEVEL FDBK (TIME) */
{
    p->s_rlevel = u99((int32_t)rl); p->s_decay = u99((int32_t)dc); p->s_size = u99((int32_t)sz);
    p->s_dlevel = u99((int32_t)dl); p->s_fb = u99((int32_t)fb);
}
static void beat(SdDrumPat *b, const char *const rows[SD_DRS])   /* a beat: a row a drum, a character a step
                                                   (. off, x on, X accent, r RADAR's) */
{
    uint32_t d, i;
    memset(b, 0, sizeof *b);
    for (d = 0; d < SD_DRS; d++)
        for (i = 0; i < SD_DR_STEPS; i++) {
            char c = rows[d][i];
            sd_dr_step_set(b, d, i, c == 'x' ? SD_DS_ON : c == 'X' ? SD_DS_ACC : c == 'r' ? SD_DS_RADAR : SD_DS_OFF);
        }
}

static void patterns(void)
{
    /* the chords, a bar each (the step's note and two above it): Dm Bb F C Dm Bb Gm Asus4 */
    static const uint8_t CR[8] = {50, 46, 53, 48, 50, 46, 43, 45};
    static const int8_t CU[8][2] = {{7, 15}, {7, 16}, {7, 16}, {7, 16}, {7, 15}, {7, 16}, {7, 15}, {7, 17}};
    /* the theme, in eighths: D--- A--- | Bb--A G-F- | A----- F G | G--F E--- | D-F-A-D'- | D'--- C'-Bb- |
       Bb--A G-Bb- | A------ */
    static const int16_t th[64] = {
        62 | AC, TI, TI, TI, 69, TI, TI, TI,    70, TI, TI, 69, 67, TI, 65, TI,
        69, TI, TI, TI, TI, TI, 65, 67,         67, TI, TI, 65, 64, TI, TI, TI,
        62 | AC, TI, 65, TI, 69, TI, 74, TI,    74 | AC, TI, TI, TI, 72, TI, 70, TI,
        70, TI, TI, 69, 67, TI, 70, TI,         69, TI, TI, TI, TI, TI, TI, RS};
    /* the counter-line under the arp, half notes: F A | Bb D | C A | G C | D F | D Bb | Bb D | E */
    static const int16_t co[64] = {
        53, TI, TI, TI, 57, TI, TI, TI,   58, TI, TI, TI, 62, TI, TI, TI,
        60, TI, TI, TI, 57, TI, TI, TI,   55, TI, TI, TI, 60, TI, TI, TI,
        62, TI, TI, TI, 65, TI, TI, TI,   62, TI, TI, TI, 58, TI, TI, TI,
        58, TI, TI, TI, 62, TI, TI, TI,   64, TI, TI, TI, TI, TI, TI, RS};
    /* the D pedal in 16ths: D3 D4 A3 D4, F4 (G4 the second time) D4 A3 D4 */
    static const int16_t os[16] = {50 | AC, 62, 57, 62, 65, 62, 57, 62, 50 | AC, 62, 57, 62, 67, 62, 57, 62};
    static const uint8_t BR[8] = {38, 34, 41, 36, 38, 34, 31, 33};   /* the roots: D Bb F C D Bb G A */
    int16_t b8[64], b4[32], dv[64], fa[32], fb[32], em[16];
    uint32_t i, k;
    begin(&CHORDS, 64, 4);
    for (i = 0; i < 64u; i++) {
        SdStep *s = st(&CHORDS, i, 100);
        s->vel = 84; s->note = CR[i / 8u]; s->chord[0] = CU[i / 8u][0]; s->chord[1] = CU[i / 8u][1]; s->chord[2] = 0;
        s->type = i % 8u == 0u ? SD_ST_NOTE : SD_ST_TIE;
    }
    for (i = 0; i < 16u; i++) em[i] = RS;
    notes(&EMPTY, em, 16, 2, 50, 0);
    notes(&OSTI, os, 16, 2, 45, 0);
    for (i = 0; i < 64u; i++) {                 /* the roots, a bar each, held (a breath before the next) */
        uint32_t j = i % 8u;
        b8[i] = j == 0u ? BR[i / 8u] : j == 7u ? RS : TI;
        dv[i] = (int16_t)(BR[i / 8u] + (j == 2u || j == 6u ? 12 : 0));   /* the drive: R R R' R R R R' R */
        if (j == 0u || j == 4u) dv[i] |= AC;
    }
    for (i = 0; i < 32u; i++) b4[i] = b8[i];
    notes(&BASS8, b8, 64, 4, 100, 0);
    notes(&BASS4, b4, 32, 4, 100, 0);
    notes(&DRIVEB, dv, 64, 4, 60, 0);
    notes(&THEME, th, 64, 4, 100, 0);
    notes(&THEME8, th, 64, 4, 100, 12);         /* the theme in octaves */
    notes(&COUNTER, co, 64, 4, 100, 0);
    for (i = 0; i < 32u; i++) { fa[i] = i == 0u ? 64 | AC : i < 8u ? TI : RS; fb[i] = i == 0u ? 40 | AC : i < 8u ? TI : RS; }
    notes(&FINAL_A, fa, 32, 4, 100, 7);         /* E B over the bass's E, a bar: the new key's; three voices, the
                                                   FREEZE's room beside the plate has the CPU's room (the strict
                                                   emulator: 90 % with four held two bars) */
    notes(&FINAL_B, fb, 32, 4, 100, 0);
    memset(&ORB, 0, sizeof ORB);                /* ORBIT, saved: two steps, a theme's eight bars each */
    ORB.len = 2; ORB.end = SD_OE_LOOP; ORB.glide = SD_OG_L1 | SD_OG_L2 | SD_OG_MAC;
    ORB.dst[0] = SD_MD_DSPLIT; ORB.dst[1] = SD_MD_X;
    for (k = 0; k < 2u; k++) {
        ORB.xa[k] = (int8_t)(k ? 2 : 0); ORB.xb[k] = (int8_t)(k ? 2 : 0);   /* (CHROM: a whole step, E minor) */
        ORB.lane[0][k] = (int8_t)(k ? 40 : -10); ORB.lane[1][k] = (int8_t)(k ? 35 : 0);
        ORB.mac[1][k] = (int8_t)(k ? 15 : 0); ORB.mac[2][k] = (int8_t)(k ? 18 : 6);
        ORB.loops[k] = 1;
    }
    /* the player's places: AURORA (U01), DRIVE (U02), STRINGS (U03), VOYAGE (U04) and TITAN (U05), each with its
       plate and echoes as the player opened them */
    U01 = SD_DPATCH[SD_D_AURORA]; wet(&U01, 66, 92, 99, 40, 55);
    U02 = SD_DPATCH[SD_D_DRIVE]; wet(&U02, 34, 76, 88, 30, 38);
    U03 = SD_DPATCH[SD_D_STRINGS]; wet(&U03, 60, 90, 99, 36, 48);
    U04 = SD_DPATCH[SD_D_VOYAGE]; wet(&U04, 52, 86, 96, 34, 45);
    U05 = SD_DPATCH[SD_D_TITAN]; wet(&U05, 44, 84, 96, 32, 45);
    U01.trim = U02.trim = U04.trim = U05.trim = 0;   /* (saved before the factory's TRIMs, 2026-11-21: each at its own
                                                   level, as the player mixed it; STRINGS' at what it sounded, its
                                                   DISTROY levels 14 dB down since: +3 dB) */
    U03.trim = 6;
    {                                           /* the beats (D01.., saved): kick, snare, hat, the fourth */
        static const char *const P[4] = {"X...............", "................", "xxXxxxXxxxXxxxXx", "...r...r...r...r"};
        static const char *const HZ[4] = {"X......x..x.....", "........X.......", "x.x.X.x.x.x.X.x.", "...r.......r...r"};
        static const char *const HT[4] = {"X......x..x.....", "........X.......", "x.x.X.x.x.x.X.x.", "..............xX"};
        static const char *const BU[4] = {"X...x...x...x...", "....X.......X...", "xxXxxxXxxxXxxxXx", ".......x.......x"};
        static const char *const FI[4] = {"X...x...x...x...", "xxxxxxxxxxxxXXXX", "................", "......X...X...X."};
        static const char *const DR[4] = {"X..x..x.X..x..x.", "....X.......X...", "xxXxxxXxxxXxxxXx", ".......x.......x"};
        static const char *const LI[4] = {"X..x..x.X..x..x.", "....X.......X..x", "xxXxxxXxxxXxxxXx", ".......x.....xxX"};
        static const char *const EN[4] = {"X...............", "X...............", "................", "X..............."};
        beat(&PULSE, P); beat(&HALF_Z, HZ); beat(&HALF_T, HT); beat(&BUILD, BU); beat(&FILL, FI); beat(&DRIVE, DR);
        beat(&LIFT, LI); beat(&END, EN);
        memset(&FILL_M, 0, sizeof FILL_M);      /* the fill's snare rolling: x2 in the third beat, x3 in the fourth */
        for (i = 8; i < 12u; i++) sd_dr_more_set(&FILL_M, SD_DR_SNARE, i, 1, 0);
        for (i = 12; i < 16u; i++) sd_dr_more_set(&FILL_M, SD_DR_SNARE, i, 2, 0);
    }
    KIT = SD_DR_KIT0;                           /* the lab's kit, its levels as the player left them, DUCK a little */
    KIT.d[SD_DR_KICK].level = 90; KIT.d[SD_DR_SNARE].level = 78; KIT.d[SD_DR_HAT].level = 22;
    KIT.d[SD_DR_CLAP].level = 56; KIT.duck = 22;
    for (k = 0; k < 8u; k++) KIT.d[SD_DR_CLAP].v[k] = SD_PC_DEF[SD_PC_ZAP][k];   /* (the fourth a ZAP, as chosen) */
}

/* ---- gestures */
static void fx_to(int to, double s0) { go_to("FX", to, s0); }
static void home_to(int to, double s0) { go_to("HOME", to, s0); }
static void seq_to(int to, double s0) { go_to("SEQ", to, s0); }
static void arp_on(double s, uint32_t mode, uint32_t div, uint32_t octs, uint32_t gate)   /* EDIT on ARP, its knobs,
                                                   HOLD: a sixteenth each */
{
    static const char *const M[5] = {"UP", "DOWN", "UP-DN", "RND", "ORDER"};
    char t[64];
    if (at(s)) {
        on_page(PG_ARP, "ARP's EDIT off its page");
        if (e.t[0].width + (e.t[1].mute ? 0u : e.t[1].width) + 1u > 4u) fail("no voice for ARP");
        act();
        e.arp_on = 1; e.arp_mode = (uint8_t)mode; e.arp_div = (uint8_t)div; e.arp_gate = (uint8_t)gate; e.arp_oct = 1;
        snprintf(t, sizeof t, "EDIT: ARP ON (MODE %s, RATE %s, GATE %u)", M[mode % 5u], div == 2u ? "1/16" : "1/8", gate);
        say(t);
    }
    if (at(s + 1)) { act(); e.arp_hold = 1; say("ALGORITHM: HOLD"); }
    if (at(s + 2)) { act(); e.arp_oct = (uint8_t)octs; say("K3: OCTAVES 2"); }
}
static void hand(const uint8_t *n, uint32_t k, double down, double up)   /* keys pressed and let go together */
{
    uint32_t i;
    for (i = 0; i < k; i++) { if (at(down + i * 0.1)) key(n[i], 1); if (at(up + i * 0.1)) key(n[i], 0); }
}

static void setup(void)
{
    patterns();
    e.xkey = 0;                                 /* SETUP's XPOSE on CHROM (the player's, before the take): ORBIT's
                                                   +2 a whole step, every chord as it was (IN KEY made one diminished) */
    scl(SD_MODE_POLY);
    page = PG_MACROS; remember();
    preset_user(&U01, "U01 AURORA WET");
    page = PG_SEQA; remember();
    pattern(0, &CHORDS, "CHORDS (Dm Bb F C Dm Bb Gm Asus4)");
    looper_mem();
    drums_ready(&KIT, SD_PC_ZAP);
    master = 0;
}

static int score(void)
{
    static const uint8_t PED[3] = {62, 69, 76};   /* the arp's hand: D A E, the pedal's colour */
    static const uint8_t PED2[3] = {64, 71, 78};  /* ... and a whole step up with the lift: E B F# */
    /* HORIZON: AURORA's chords, SHIMMER, FREEZE */
    if (at(0)) play(1);
    pot(__LINE__, 1800, 0, BAR(4));                /* MASTER: the horizon quiet, then up section by section */
    pot(__LINE__, 2100, BAR(5), BAR(9));
    fx_to(PG_SPACE, T(1, 4));                   /* (FX: REACTOR, DISTROY, SPACE) */
    SHIMMER(24, T(2, 0), T(5, 0));              /* GLO + K1: SHIMMER up */
    seq_to(PG_SEQA, T(7, 0));
    if (at(T(7, 2))) pattern(0, &OSTI, "OSTINATO (the D pedal, 16ths)");   /* (in at A's one: bar 9) */
    fx_to(PG_SPACE, T(7, 4));
    if (at(T(8, 12))) freeze_toggle();          /* the Asus4's tail held */
    home_to(PG_MACROS, T(8, 13));
    /* LAUNCH: DRIVE's ostinato under the frozen plate; the looper; the drums; RADAR's ZAP */
    if (at(BAR(9))) preset_user(&U02, "U02 DRIVE");
    arp_to(PG_LOOP, T(9, 2));                   /* (ARP: ARP, LOOPER) */
    if (at(T(9, 5))) lp_length(12);             /* K2: LENGTH 1 BAR */
    if (at(T(9, 7))) lp_glide_to(13);           /* K1: GLIDE 1 BAR */
    LP_LEVEL(80, T(9, 8), T(10, 4));            /* K4: L-LEVEL up: armed (the delay 0.5 s) */
    seq_to(PG_DRUM, T(10, 6));                  /* (SEQ: A, B, ORBIT, DRUM) */
    if (at(T(10, 11))) drum_beat(&PULSE, 0, "PULSE (a heartbeat, the hats, RADAR's ZAP)");
    if (at(T(10, 13))) drums_edit(1);           /* (in at the master's one: bar 11) */
    go_to("LFO", PG_MOD, T(11, 0));             /* (LFO: LFO 1, LFO 2, RADAR, MOD) */
    if (at(T(11, 5))) tur_route(4, SD_MD_PTUNE, 18);   /* MOD slot 5: RADAR to PERC TUNE (the ZAP's pitch) */
    go_to("LFO", PG_RADAR, T(11, 7));          /* (LFO: LFO 1, LFO 2, RADAR) */
    TUR_LENGTH(8, T(11, 10), T(11, 11));        /* K2: LENGTH 8 */
    TUR_CHANGE(20, T(11, 12), T(11, 15));       /* K1: SEARCH 20 */
    if (at(T(12, 0))) tur_edit();               /* RADAR on: the ZAPs where its bit is 1 */
    seq_to(PG_SEQA, T(12, 2));                  /* (SEQ: DRUM, SEQ A) */
    if (at(T(12, 5))) pattern(0, &EMPTY, "EMPTY (A keeps the time)");   /* (in at bar 13) */
    seq_to(PG_SEQB, T(12, 7));
    if (at(T(12, 9))) pattern(1, &BASS4, "ROOTS (D Bb F C)");          /* (in at bar 13) */
    arp_to(PG_LOOP, T(12, 11));
    if (at(BAR(13))) loop_toggle();             /* the ostinato's last bar, held */
    fx_to(PG_SPACE, T(13, 2));
    if (at(T(13, 4))) freeze_toggle();          /* AURORA's chords let go */
    seq_to(PG_DRUM, T(13, 6));                  /* (SEQ: B, ORBIT, DRUM) */
    if (at(T(13, 9))) drum_beat(&HALF_Z, 0, "HALF (half-time, the ZAPs)");   /* (in at bar 14) */
    /* THEME: STRINGS, the theme, its roots, the drums half-time with a TOM */
    seq_to(PG_SEQA, T(16, 0));                  /* (SEQ: DRUM, SEQ A) */
    if (at(T(16, 2))) pattern(0, &THEME, "THEME");
    seq_to(PG_SEQB, T(16, 4));
    if (at(T(16, 6))) pattern(1, &BASS8, "ROOTS (D Bb F C D Bb G A)");
    seq_to(PG_DRUM, T(16, 8));                  /* (SEQ: ORBIT, DRUM) */
    if (at(T(16, 11))) drum_engine(SD_PC_TOM);
    if (at(T(16, 12))) drum_beat(&HALF_T, 0, "HALF (half-time, the TOM's pickup)");
    home_to(PG_MACROS, T(16, 14));
    pot(__LINE__, 2500, T(16, 0), BAR(17));
    if (at(BAR(17))) preset_user(&U03, "U03 STRINGS WET");
    /* ASCENT: VOYAGE, the counter-line, the arp on D A E, the FILTER, the drums four to the floor, the fill */
    seq_to(PG_SEQA, T(24, 0));                  /* (SEQ: DRUM, SEQ A) */
    if (at(T(24, 3))) pattern(0, &COUNTER, "COUNTER-LINE");
    arp_to(PG_ARP, T(24, 5));                   /* (ARP: LOOPER, the one it showed last, then ARP) */
    arp_on(T(24, 8), SD_AM_UPDN, 2, 2, 55);
    hand(PED, 3, T(24, 12), T(24, 14));
    home_to(PG_MACROS, T(24, 15));
    if (at(BAR(25))) preset_user(&U04, "U04 VOYAGE WET");
    HOME_FILTER(40, T(25, 0.5), T(25, 8));      /* K4: FILTER dipped ... */
    HOME_FILTER(60, T(25, 10), T(26, 4));       /* ... and opening */
    seq_to(PG_DRUM, T(26, 6));                  /* (SEQ: A, B, ORBIT, DRUM) */
    if (at(T(26, 11))) drum_beat(&BUILD, 0, "BUILD (four to the floor)");   /* (in at bar 27) */
    home_to(PG_MACROS, T(26, 13));
    HOME_FILTER(99, T(26, 15), T(31, 6));       /* K4: FILTER open */
    pot(__LINE__, 2900, BAR(27), BAR(32));
    seq_to(PG_DRUM, T(31, 8));
    if (at(T(31, 10))) drum_beat(&FILL, &FILL_M, "FILL (the snare rolling)");   /* (in at bar 32) */
    seq_to(PG_SEQA, T(31, 12));                 /* (SEQ: DRUM, SEQ A) */
    if (at(T(31, 14))) pattern(0, &THEME8, "THEME IN OCTAVES");             /* (in at bar 33) */
    seq_to(PG_SEQB, T(32, 0));
    if (at(T(32, 2))) pattern(1, &DRIVEB, "DRIVE (the roots in eighths)");
    seq_to(PG_ORB, T(32, 4));
    if (at(T(32, 6))) orbit_load(&ORB);
    if (at(T(32, 7))) orbit_edit(1);            /* (in at A's one: bar 33) */
    seq_to(PG_DRUM, T(32, 9));
    if (at(T(32, 11))) drum_beat(&DRIVE, 0, "DRIVE");                       /* (in at bar 33) */
    home_to(PG_MACROS, T(32, 13));
    pot(__LINE__, 3700, T(32, 0), T(32, 12));       /* (one hand on MASTER, the other on the keys; then K4) */
    HOME_FILTER(84, T(32, 14), T(33, 1));       /* K4: FILTER back a little (TITAN's top darker) */
    /* ARMADA: TITAN, the theme in octaves, ORBIT, RADAR's ACCENTS, MOVE and BREAK */
    if (at(BAR(33))) preset_user(&U05, "U05 TITAN WET");
    go_to("LFO", PG_MOD, T(33, 2));             /* (LFO: RADAR, the one it showed last, then MOD) */
    if (at(T(33, 5))) tur_route(6, SD_TD_DACC, 40);   /* MOD slot 7: RADAR to ACCENTS */
    go_to("LFO", PG_RADAR, T(33, 7));          /* (LFO: LFO 1, LFO 2, RADAR) */
    TUR_CHANGE(30, T(33, 10), T(33, 14));       /* K1: SEARCH 30 */
    if (at(T(34, 0))) tur_edit();               /* RADAR on */
    home_to(PG_MACROS, T(34, 2));
    KNOB99(PG_MACROS, pat.mac[1], 30, T(35, 0), T(39, 0));   /* MOVE */
    KNOB99(PG_MACROS, pat.mac[2], 24, T(37, 0), T(40, 4));   /* BREAK */
    pot(__LINE__, 4092, T(40, 0), T(40, 7));
    seq_to(PG_DRUM, T(40, 8));                  /* (SEQ: SEQ B, the one it showed last, ORBIT, DRUM) */
    if (at(T(40, 11))) drum_beat(&LIFT, 0, "LIFT (the toms rolling)");      /* (in at bar 41) */
    arp_to(PG_LOOP, T(40, 13));                 /* (ARP: ARP, the one it showed last, then LOOPER) */
    hand(PED2, 3, T(40, 15), T(41, 1));         /* the arp's new hand (HOLD: a new hand, a new list) */
    if (at(BAR(41))) loop_toggle();             /* the ostinato's loop out: the looper records again */
    home_to(PG_MACROS, T(41, 2));
    /* LIFT: ORBIT's second step (A and B a whole step up), MUTATE */
    KNOB99(PG_MACROS, pat.mac[0], 16, T(41, 4), T(45, 0));   /* MUTATE */
    seq_to(PG_SEQA, T(47, 0));                  /* (SEQ: DRUM, SEQ A) */
    if (at(T(47, 3))) pattern(0, &FINAL_A, "FINAL (E, held)");              /* (in at bar 49) */
    seq_to(PG_SEQB, T(47, 4));
    if (at(T(47, 6))) pattern(1, &FINAL_B, "FINAL (E, held)");
    arp_to(PG_ARP, T(47, 8));                   /* (ARP: LOOPER, the one it showed last, then ARP) */
    if (at(T(47, 12))) { act(); e.arp_on = 0; e.arp_hold = 0; say("EDIT: ARP OFF"); }
    seq_to(PG_DRUM, T(48, 0));                  /* (SEQ: SEQ B, ORBIT, DRUM) */
    if (at(T(48, 4))) drum_beat(&END, 0, "END (one hit)");                  /* (in at bar 49) */
    fx_to(PG_SPACE, T(48, 6));
    /* DRIFT: the final E, frozen; the drums off; the looper holding the bar, backwards, down an octave */
    if (at(T(49, 3))) freeze_toggle();
    seq_to(PG_DRUM, T(49, 5));
    if (at(T(49, 7))) drums_edit(0);
    arp_to(PG_LOOP, T(49, 9));
    if (at(BAR(50))) loop_toggle();             /* the final bar held */
    if (at(T(50, 2))) reverse_toggle();
    if (at(T(50, 4))) loop_octave(0);           /* GLO + OCT-: an octave down, over a bar */
    LP_LEVEL(1, T(51, 0), T(52, 14));           /* K4: L-LEVEL out, to 1 (armed still) */
    pot(__LINE__, 2400, T(50, 0), BAR(53));
    if (at(BAR(53))) loop_toggle();             /* the loop let go */
    if (at(T(53, 1))) play(0);
    fx_to(PG_SPACE, T(53, 3));
    if (at(T(53, 5))) freeze_toggle();          /* the frozen wall let go */
    fx_to(PG_DISTROY, T(53, 7));                /* (FX: TIME, REACTOR, DISTROY) */
    if (at(T(53, 11))) {                        /* EDIT: DISTROY off, its loop with it (BREAK up keeps it ringing) */
        on_page(PG_DISTROY, "DISTROY's EDIT off its page");
        act();
        pat.fx_off |= SD_FX_DISTROY;
        dirty = 1;
        say("EDIT: DISTROY OFF");
    }
    pot(__LINE__, 0, BAR(53), BAR(55));
    return q >= BAR(55);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sd_armada OUT.wav\n"); return 2; }
    bpm = 96;
    return play_run(argv[1], "armada", setup, score);
}
