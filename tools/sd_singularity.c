/* SPDX-License-Identifier: GPL-3.0-only */
/* SINGULARITY: one take (2026-11-11, the user: "can the next be what one can get with only one take by maxing the
 * FM-1 itself? No layers, not more than one take, one stem. Just a lot of the firmware, standalone").
 *
 *   sd_singularity OUT.wav
 *
 * One FM-1 played by one person, one pass, RANGE's rules (tools/sd_play.h: one engine, one patch at a time, the
 * app's controls at a hand's speed, the voice rules, MASTER as platform.c applies it; nothing mixed). What sounds
 * over what is the FM-1's own: the looper holding a bar of one patch while the next plays, the plate's FREEZE holding
 * its tail across a patch change, the delay's echoes; under POLY's four voices SEQ A's chords, SEQ B and the
 * arpeggiator at once, ORBIT stepping both, TURING, the matrix, the macros. The looper's memory is the FM-1's own
 * (half the delay line, lent while L-LEVEL is above 0, and 24 KB: 5.1 s, mono mu-law): armed in the second bar,
 * the delay then 0.5 s. F minor (the arp, TURING's NOTES and the transposes, IN KEY as the app has them, write from it), 72 BPM,
 * POLY throughout, 33 bars:
 *   1-8    GLASS    AURORA (saved wet) on SEQ A, Fm Db Bbm Fm, the arpeggiator over it in eighths; LOOP its last bar,
 *                   SEQ A off; REVERSE, SCL + OCT- (a bar's glide down onto its own grid); SHIMMER and FREEZE on its
 *                   plate: the loop an octave down under the plate an octave up
 *   9-16   VOICES   VOWEL (saved wet) on SEQ A, long notes, and SEQ B under it, the frozen glass behind: TURING's NOTES
 *                   on A; LFO 2 onto the Reactor's POS (its vowels, slowly), LFO 1 onto the loop's SPEED (a tape's
 *                   wobble); FREEZE let go; the delay's FDBK and D-LEVEL up; GLIDE 2 BAR, REVERSE off, SCL + OCT+
 *                   twice: the loop two octaves up, onto its grid at +12; MOVE
 *   17-26  WALL     DISTROY (saved wet), POLY's four voices full: SEQ A's riff in octaves, SEQ B's bass, the arp in
 *                   16ths at random over two octaves; ORBIT stepping them both through F Db Ab Eb, its lanes on D
 *                   SPLIT and the nav's X, its BREAK and MOVE rising with them; BREAK and BODY up, TURING on the FOLD,
 *                   the top band CRUSHed; the loop at +12 over it all, then backwards
 *   27-33  FREEZE   the wall's plate frozen as it stood, the rest off (DISTROY's EDIT with it); the loop falling two
 *                   octaves; BEACON's last C in its echoes; the looper out; FREEZE let go */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"

#define BAR(b) (((b) - 1) * 16.0)               /* the start of bar b (from 1), in sixteenths */

/* ---- the harmony: a chord a bar, three notes (the step's note and two above it: the arp's voice left) */
typedef struct { uint8_t root; int8_t up[2]; } Chord;
static const Chord FM = {53, {7, 15}}, DB = {49, {7, 16}}, BBM = {46, {7, 15}};
static const Chord *const GLASSH[4] = {&FM, &DB, &BBM, &FM};   /* (its last bar the loop's) */

/* ---- the patterns the player saved (PRESETS on SEQ loads them; ORBIT with SEQ A's) */
static SdPattern GLASS, VA, VB, RIFF, BASS;
static SdOrbit ORB;
static SdPatch U01, U02, U03, U04;              /* the player's own (see patterns()) */

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
        if (n[i] & AC) s->vel = 124;
    }
}
static void wet(SdPatch *p, uint32_t rl, uint32_t dc, uint32_t sz, uint32_t dl, uint32_t fb)   /* SPACE opened on its
                                                   pages and saved: R-LEVEL DECAY SIZE (SPACE), D-LEVEL FDBK (TIME) */
{
    p->s_rlevel = u99((int32_t)rl); p->s_decay = u99((int32_t)dc); p->s_size = u99((int32_t)sz);
    p->s_dlevel = u99((int32_t)dl); p->s_fb = u99((int32_t)fb);
}

static void patterns(void)
{
    /* VOWEL's long notes on A, F first (the key): F, C, Db Bb, C; B under them: F, Ab, Bb, F */
    static const int16_t va[32] = {65 | AC, TI, TI, TI, TI, TI, TI, TI,   72, TI, TI, TI, TI, TI, TI, TI,
                                   73, TI, TI, TI, 70, TI, TI, TI,        72, TI, TI, TI, TI, TI, RS, RS};
    static const int16_t vb[32] = {41, TI, TI, TI, TI, TI, TI, TI,        44, TI, TI, TI, TI, TI, TI, TI,
                                   46, TI, TI, TI, TI, TI, TI, TI,        41, TI, TI, TI, TI, TI, RS, RS};
    /* the wall: A's riff in eighths, in octaves (F F - F Ab - G F); B's bass in 16ths */
    static const int16_t ri[8] = {53 | AC, 53, RS, 53, 56, RS, 55, 53};
    static const int16_t ba[16] = {41 | AC, 41, 53, 41, 41, 53, 41, 48, 41 | AC, 41, 53, 41, 44, 46, 48, 51};
    static const int8_t XA[4] = {0, -4, 3, -2};                  /* F Db Ab Eb (IN KEY: degrees of F minor) */
    static const int8_t L1[4] = {0, 40, -30, 60}, L2[4] = {-50, 50, -20, 70};   /* D SPLIT, the nav's X */
    static const int8_t BR[4] = {0, 8, 16, 24}, MV[4] = {0, 20, 0, 30};         /* BREAK, MOVE: on top of the knobs */
    uint32_t i, k;
    begin(&GLASS, 32, 4);                       /* AURORA's chords: a chord a bar, in eighths */
    for (i = 0; i < 32u; i++) {
        SdStep *s = st(&GLASS, i, 100);
        const Chord *c = GLASSH[i / 8u];
        s->vel = 90; s->note = c->root; s->chord[0] = c->up[0]; s->chord[1] = c->up[1]; s->chord[2] = 0;
        s->type = i % 8u == 0u ? SD_ST_NOTE : SD_ST_TIE;
    }
    notes(&VA, va, 32, 4, 100, 0);
    notes(&VB, vb, 32, 4, 100, 0);
    notes(&RIFF, ri, 8, 4, 70, 12);
    notes(&BASS, ba, 16, 2, 45, 0);
    BASS.step[7].ratchet = 2; BASS.step[13].prob = 70;
    memset(&ORB, 0, sizeof ORB);                /* ORBIT, saved with the riff: four steps, a bar each */
    ORB.len = 4; ORB.rate = 0; ORB.glide = SD_OG_L1 | SD_OG_L2 | SD_OG_MAC;
    ORB.dst[0] = SD_MD_DSPLIT; ORB.dst[1] = SD_MD_X;
    for (k = 0; k < 4u; k++) {
        ORB.xa[k] = XA[k]; ORB.xb[k] = XA[k];
        ORB.lane[0][k] = L1[k]; ORB.lane[1][k] = L2[k];
        ORB.mac[2][k] = BR[k]; ORB.mac[1][k] = MV[k];
    }
    /* the player's places: AURORA (U01), VOWEL (U02) and DISTROY (U03) with their plates and echoes up; BEACON (U04)
       with its MODE FREE (PP kept) and TIME at 1000 ms */
    U01 = SD_PATCH[SD_P_AURORA]; wet(&U01, 66, 92, 99, 40, 55);
    U02 = SD_PATCH[SD_P_VOWEL]; wet(&U02, 56, 88, 96, 38, 50);
    U03 = SD_PATCH[SD_P_DISTROY]; wet(&U03, 44, 84, 96, 40, 55);
    U04 = SD_PATCH[SD_P_BEACON];
    U04.s_mode = SD_SP_PP; U04.s_time = 65535u;
}

/* ---- gestures */
static void fx_to(int n, double s0)             /* FX n times, a sixteenth apart: REACTOR DISTROY SPACE TIME, as the
                                                   app's button goes (from another page: REACTOR) */
{
    int i;
    for (i = 0; i < n; i++)
        if (at(s0 + i))
            press("FX", page == PG_REACTOR ? PG_DISTROY : page == PG_DISTROY ? PG_SPACE : page == PG_SPACE ? PG_TIME : PG_REACTOR);
}
static void home_to(int n, double s0)           /* HOME n times: NAV, MACROS */
{
    int i;
    for (i = 0; i < n; i++) if (at(s0 + i)) press("HOME", page == PG_NAV ? PG_MACROS : PG_NAV);
}
static void seq_to(int n, double s0)            /* SEQ n times: SEQ A, SEQ B, ORBIT */
{
    int i;
    for (i = 0; i < n; i++) if (at(s0 + i)) press("SEQ", page == PG_SEQA ? PG_SEQB : page == PG_SEQB ? PG_ORB : PG_SEQA);
}
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
    if (at(s + 2)) { act(); e.arp_oct = (uint8_t)octs; say(octs == 2u ? "K3: OCTAVES 2" : "K3: OCTAVES"); }
}
static void arp_off(double s)
{
    if (at(s)) press("ARP", PG_ARP);
    if (at(s + 1)) { act(); e.arp_on = 0; e.arp_hold = 0; say("EDIT: ARP OFF"); }
}
static void hand(const uint8_t *n, uint32_t k, double down, double up)   /* keys pressed and let go together */
{
    uint32_t i;
    for (i = 0; i < k; i++) { if (at(down + i * 0.1)) key(n[i], 1); if (at(up + i * 0.1)) key(n[i], 0); }
}

static void setup(void)
{
    patterns();
    e.xkey = 1;                                 /* the transposes IN KEY: the app's default */
    glo(SD_MODE_POLY);
    preset_user(&U01, "U01 AURORA WET");
    page = PG_SEQA;
    pattern(0, &GLASS, "GLASS (Fm Db Bbm Fm)");
    looper_mem();
    master = 0;
}

static int score(void)
{
    static const uint8_t H1[4] = {65, 68, 72, 75}, H2[3] = {65, 68, 72};   /* the arp's hands: Fm7, Fm */
    int32_t v;
    /* GLASS */
    if (at(0)) play(1);
    pot(__LINE__, 2200, 0, BAR(3));                /* (low: the wall is MASTER's top) */
    if (at(BAR(2) + 12)) press("ARP", PG_ARP);
    arp_on(BAR(2) + 13, SD_AM_UPDN, 4, 2, 70);
    hand(H1, 4, BAR(3), BAR(3) + 2);
    fx_to(4, BAR(3) + 4);                       /* TIME: LENGTH 1 BAR, GLIDE 1 BAR */
    if (at(BAR(3) + 9)) lp_length(12);
    if (at(BAR(3) + 11)) lp_glide_to(13);
    LP_LEVEL(79, BAR(3) + 13, BAR(4) + 12);          /* SCL + K4: L-LEVEL up: the looper armed (the delay 0.5 s) */
    if (at(BAR(5))) loop_toggle();              /* its last bar (Fm and the arp), held */
    seq_to(1, BAR(5) + 1);                      /* EDIT: SEQ A off, PLAY running on (STOP would park the loop) */
    if (at(BAR(5) + 2)) track_on(0, 0);
    arp_off(BAR(5) + 3);
    fx_to(4, BAR(5) + 5);
    if (at(BAR(5) + 10)) reverse_toggle();
    if (at(BAR(6))) loop_octave(0);             /* SCL + OCT-: down over the bar, onto its grid */
    fx_to(3, BAR(7));
    SHIMMER(30, BAR(7) + 3, BAR(8));            /* SCL + ALGORITHM: SHIMMER 0 -> 48 % */
    KNOB99(PG_SPACE, pat.s_rlevel, 74, BAR(7) + 3, BAR(8));   /* K4: R-LEVEL */
    if (at(BAR(8))) freeze_toggle();
    /* VOICES: VOWEL over the frozen glass, A and B */
    if (at(BAR(8) + 4)) preset_user(&U02, "U02 VOWEL WET");
    seq_to(1, BAR(8) + 6);
    if (at(BAR(8) + 7)) pattern(0, &VA, "VOICES A (F C Db Bb C)");
    seq_to(1, BAR(8) + 9);
    if (at(BAR(8) + 10)) pattern(1, &VB, "VOICES B (F Ab Bb F)");
    if (at(BAR(8) + 11)) track_on(1, 1);
    pot(__LINE__, 1900, BAR(8) + 4, BAR(9));
    seq_to(2, BAR(8) + 12);                     /* SEQ A on at the one (B comes in there by itself) */
    if (at(BAR(9))) track_on(0, 1);
    if (at(BAR(9) + 2)) press("LFO", PG_LFO1);
    KNOB99(PG_LFO1, pat.lfo_rate[0], 42, BAR(9) + 3, BAR(9) + 9);    /* K2: RATE ~0.5 Hz (the wobble's) */
    if (at(BAR(9) + 10)) press("LFO", PG_LFO2);
    KNOB99(PG_LFO2, pat.lfo_rate[1], 20, BAR(9) + 11, BAR(9) + 15);  /* K2: RATE ~0.1 Hz (the vowels') */
    if (at(BAR(10))) press("LFO", PG_TURING);  /* TURING's NOTES on A: 30 %, CHANGE 20 */
    TUR_DEST_BY(-1, BAR(10) + 0.5, BAR(10) + 1.5);
    TUR_AMOUNT(30, BAR(10) + 2, BAR(10) + 8);
    TUR_CHANGE(20, BAR(10) + 8, BAR(10) + 12);
    if (at(BAR(10) + 13)) tur_edit();
    if (at(BAR(11))) press("LFO", PG_MOD);
    if (at(BAR(11) + 2)) mod_slot(2, SD_MS_LFO2, SD_MD_RPOS, 45);     /* the Reactor's vowels */
    if (at(BAR(11) + 4)) mod_slot(3, SD_MS_LFO1, SD_MD_LSPEED, 2);    /* the loop's wobble */
    fx_to(3, BAR(11) + 8);
    if (at(BAR(12))) freeze_toggle();           /* the glass let go */
    fx_to(1, BAR(12) + 2);
    KNOB99(PG_TIME, pat.s_fb, 70, BAR(12) + 3, BAR(12) + 10);        /* K2: FDBK */
    KNOB99(PG_TIME, pat.s_dlevel, 56, BAR(12) + 11, BAR(13) + 2);    /* K4: D-LEVEL */
    if (at(BAR(13) + 4)) lp_glide_to(14);       /* SCL + K1: GLIDE 2 BAR */
    if (at(BAR(13) + 6)) reverse_toggle();
    if (at(BAR(14))) loop_octave(1);            /* SCL + OCT+: to its pitch ... */
    if (at(BAR(14) + 4)) loop_octave(1);        /* ... and on to +12 */
    home_to(2, BAR(15));
    KNOB99(PG_MACROS, pat.mac[1], 50, BAR(15) + 2, BAR(17));         /* MOVE */
    /* the wall: A and B off (PLAY running on: the loop carries it), DISTROY, A's riff (its ORBIT with it), B's bass;
       B and ORBIT on in bar 18 (in at A's one, bar 19), the arp; A on at the one */
    seq_to(1, BAR(17));
    if (at(BAR(17) + 1)) track_on(0, 0);
    seq_to(1, BAR(17) + 2);
    if (at(BAR(17) + 3)) track_on(1, 0);
    home_to(1, BAR(17) + 4);
    if (at(BAR(17) + 5)) preset_user(&U03, "U03 DISTROY WET");
    seq_to(1, BAR(17) + 6);
    if (at(BAR(17) + 7)) { pattern(0, &RIFF, "WALL A (F, octaves) and its ORBIT"); orbit_load(&ORB); }
    seq_to(1, BAR(17) + 8);
    if (at(BAR(17) + 9)) pattern(1, &BASS, "WALL B (F, 16ths)");
    if (at(BAR(18) + 1)) track_on(1, 1);
    seq_to(1, BAR(18) + 2);
    if (at(BAR(18) + 3)) orbit_edit(1);
    if (at(BAR(18) + 4)) press("ARP", PG_ARP);
    arp_on(BAR(18) + 5, SD_AM_RND, 2, 2, 35);
    seq_to(1, BAR(18) + 12);
    pot(__LINE__, 4092, BAR(18), BAR(19));
    if (at(BAR(19))) track_on(0, 1);
    home_to(2, BAR(19) + 1);
    hand(H2, 3, BAR(19) + 3, BAR(19) + 5);
    KNOB99(PG_MACROS, pat.mac[2], 58, BAR(19) + 6, BAR(21));         /* BREAK */
    KNOB99(PG_MACROS, pat.mac[3], 55, BAR(21) + 1, BAR(22) + 8);     /* MACRO 4: BODY */
    lfo_to_turing(BAR(22) + 10);                /* TURING on the FOLD: 40 %, CHANGE 40 */
    TUR_DEST_BY(4, BAR(22) + 12.5, BAR(22) + 14.5);
    TUR_AMOUNT(40, BAR(22) + 15, BAR(23) + 4);
    TUR_CHANGE(40, BAR(23) + 4, BAR(23) + 8);
    if (at(BAR(23) + 9)) tur_edit();
    fx_to(2, BAR(23) + 11);
    if (at(BAR(23) + 14)) { act(); say("ALGORITHM: band 4 (the top)"); }
    if (at(BAR(24))) {                          /* SCL + ALGORITHM: the top band's shaper to CRUSH */
        on_page(PG_DISTROY, "a shaper off DISTROY");
        act();
        pat.d_type[3] = SD_DS_CRUSH;
        dirty = 1;
        say("SCL + ALGORITHM: SHAPE CRUSH (band 4)");
    }
    on_ramp_page(PG_DISTROY, BAR(24) + 1, BAR(25) + 8);
    if (ramp(__LINE__, (int32_t)d99(pat.d_gain[3]), 84, BAR(24) + 1, BAR(25) + 8, &v)) { pat.d_gain[3] = u99(v); dirty = 1; }
    fx_to(2, BAR(25) + 10);
    if (at(BAR(25) + 13)) reverse_toggle();     /* the loop at +12 backwards */
    /* FREEZE: the wall held, the rest off; the loop down two octaves; BEACON's last C */
    fx_to(3, BAR(26) + 10);
    if (at(BAR(27))) freeze_toggle();
    seq_to(1, BAR(27) + 1);                     /* A, B and ORBIT off, PLAY running on under the loop */
    if (at(BAR(27) + 2)) track_on(0, 0);
    seq_to(1, BAR(27) + 3);
    if (at(BAR(27) + 4)) track_on(1, 0);
    seq_to(1, BAR(27) + 5);
    if (at(BAR(27) + 6)) orbit_edit(0);
    arp_off(BAR(27) + 7);
    fx_to(2, BAR(27) + 9);
    if (at(BAR(27) + 11)) {                     /* EDIT: DISTROY off, its loop with it (BREAK up keeps it ringing) */
        on_page(PG_DISTROY, "DISTROY's EDIT off its page");
        act();
        pat.fx_off |= SD_FX_DISTROY;
        dirty = 1;
        say("EDIT: DISTROY OFF");
    }
    fx_to(2, BAR(27) + 12);
    if (at(BAR(27) + 14)) loop_octave(0);       /* SCL + OCT-: to its pitch ... */
    if (at(BAR(28) + 4)) loop_octave(0);        /* ... and on to -12 */
    if (at(BAR(28) + 8)) preset_user(&U04, "U04 BEACON 1 S");
    pot(__LINE__, 3400, BAR(28) + 8, BAR(29));
    if (at(BAR(29))) key(72, 1);
    KNOB99(PG_TIME, pat.s_fb, 72, BAR(29) + 1, BAR(29) + 8);         /* K2: FDBK */
    LP_LEVEL(1, BAR(29) + 10, BAR(32));         /* SCL + K4: L-LEVEL out, to 1 (armed still: at 0 the delay's line
                                                   would change under BEACON's echoes) */
    if (at(BAR(31))) key(72, 0);
    if (at(BAR(32) + 2)) loop_toggle();
    if (at(BAR(32) + 3)) play(0);
    fx_to(3, BAR(32) + 4);
    if (at(BAR(33))) freeze_toggle();           /* the frozen wall let go */
    pot(__LINE__, 0, BAR(33), BAR(36));
    return q >= BAR(36);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sd_singularity OUT.wav\n"); return 2; }
    bpm = 72;
    return play_run(argv[1], "singularity", setup, score);
}
