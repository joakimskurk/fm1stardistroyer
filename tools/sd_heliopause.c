/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* HELIOPAUSE, a cue (2026-11-11, the user: "write a new 1-2 min cinematic thing that uses everything that is in RANGE,
 * and use the looping tape and so on as texture where it fits"; TURING "at least one place"; then, having heard the
 * first: "not extreme enough in the sound image at the climax; the acid bass is off key; the part from one minute a
 * little too intense; I want to hear more of the plate and the delay, and both FREEZE, an octave down and an octave
 * up; it can be shorter, and more ambient before it gets intense"): where the sun's wind gives way to the dark
 * between the stars.
 *
 *   sd_heliopause OUT.wav [STEMS_DIR]
 *
 * Four passes on one FM-1, each played by one person under RANGE's rules (tools/sd_play.h: one engine, one patch at
 * a time, the app's controls at a hand's speed, the voice rules, MASTER), recorded one after another to the same
 * tempo and layered at fixed faders (tools/sd_layer.h): never more than four at once, nothing added to them. The
 * looper's memory is the FM-1's own (half the delay line, lent while L-LEVEL is above 0, and 24 KB: 5.1 s, mono
 * mu-law): armed a bar and a half before LOOP, the bed's delay then 0.5 s. E minor, every chord in it (the arp, TURING's NOTES and the
 * transposes, IN KEY as the app has them, write from it), 80 BPM, 30 bars: ambient for 16, a rhythm at a time, then
 * the wall, then its freeze:
 *   1-8    DRIFT    GROWL's low E, its lowpass opening; VOYAGE's figure (COURSE, ALL and ODD on bank A) and AURORA's
 *                   glass swells, both saved wet (their plates and echoes up); the bed (CHORDS, saved dark, in a
 *                   big hall) in at 3, the arpeggiator over it in eighths
 *   7-12   TAPE     LOOP: the bed's last bar held, the bed off; REVERSE; SCL + OCT-, a bar's glide down onto its own
 *                   grid; SHIMMER, then FREEZE on its plate: the loop an octave down under the plate an octave up;
 *                   LFO 2 onto its SPEED (a tape's wobble); under it the acid, dark, TURING on its CUTOFF, and
 *                   VOWEL's long notes through the Reactor's vowels
 *   13-16  RISE     FREEZE let go, REVERSE off, GLIDE 2 BAR, SCL + OCT+ twice: the loop rising two octaves over two
 *                   bars, onto its grid at +12; ORBITAL's ostinato, TURING's NOTES, MOVE
 *   17-24  WALL     DISTROY's slow riff, BREAK, MOVE, its top band CRUSHed and up; the bed's chords back, loud, the
 *                   loop over them at +12; BEACON's line far back, DROWN up; ORBITAL's glints, MUTATE, CHANGE 55
 *   25-30  FREEZE   all at once off but the wall's plate, frozen as it stood; the loop falling two octaves back down,
 *                   faded out; BEACON's last E in its echoes; FREEZE let go
 * The stems: STEMS_DIR/heliopause_{bed,low,motion,lead}.wav (STEMS_DIR: OUT's own folder). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"
#include "sd_layer.h"

enum { BED_, LOW_, MOTION_, LEAD_, PASSES };
static const char *const PASS_NAME[PASSES] = {"bed", "low", "motion", "lead"};
static const double GAIN[PASSES] = {1.00, 1.30, 0.90, 1.00};   /* the faders (then one master to -1 dBFS) */
#define pass layer_pass

#define BAR(b) (((b) - 1) * 16.0)               /* the start of bar b (from 1), in sixteenths */

/* ---- the harmony: a chord a bar, three notes (the step's note and two above it: the arp's voice left) */
typedef struct { uint8_t root; int8_t up[2]; } Chord;
static const Chord EM = {52, {7, 15}}, CM = {48, {7, 16}}, DM = {50, {7, 16}}, AM = {45, {7, 15}};
static const Chord *const DRIFT[4] = {&EM, &CM, &DM, &EM};                       /* 3-6 (its last bar the loop's) */
static const Chord *const WALL[8] = {&EM, &CM, &DM, &EM, &AM, &CM, &DM, &EM};   /* 17-24 */

/* ---- the patterns the player saved (PRESETS on SEQ loads them) */
static SdPattern BEDA, BEDB, FIG, ACID, PADS, OST, RIFF, LEAD;
static SdPatch U01, U02, U03, U04, U05;         /* the player's own (see patterns()) */

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
static void notes(SdPattern *p, const int16_t *n, uint32_t len, uint32_t div, uint32_t gate)   /* sd_play.h's
                                                   notation: RS, TI, a note | SL | AC */
{
    uint32_t i;
    begin(p, len, div);
    for (i = 0; i < len; i++) {
        SdStep *s = st(p, i, gate);
        if (n[i] == RS) { s->type = SD_ST_REST; continue; }
        if (n[i] == TI) { s->type = SD_ST_TIE; continue; }
        s->note = (uint8_t)(n[i] & 0xFF);
        s->type = n[i] & SL ? SD_ST_SLIDE : SD_ST_NOTE;
        if (n[i] & AC) s->vel = 124;
    }
}
static void chords(SdPattern *p, const Chord *const *h, uint32_t bars, uint32_t vel)   /* eighths: a chord a bar */
{
    uint32_t i;
    begin(p, 8u * bars, 4);
    for (i = 0; i < 8u * bars; i++) {
        SdStep *s = st(p, i, 100);
        const Chord *c = h[i / 8u];
        s->vel = (uint8_t)vel; s->note = c->root; s->chord[0] = c->up[0]; s->chord[1] = c->up[1]; s->chord[2] = 0;
        s->type = i % 8u == 0u ? SD_ST_NOTE : SD_ST_TIE;
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
    /* VOYAGE's figure, eighths: E B G F# | E - D B | E B G B | A G F# D */
    static const int16_t fi[32] = {64 | AC, TI, 59, TI, 67, TI, 66, TI,   64, TI, TI, TI, 62, TI, 59, TI,
                                   64 | AC, TI, 59, TI, 67, TI, 71, TI,   69, TI, 67, TI, 66, TI, 62, TI};
    /* the acid, 16ths on E, all in E minor (a ratchet, a chance, swing) */
    static const int16_t ac[16] = {40 | AC, 40, 52, 40 | AC, 43, 45 | SL, 40 | AC, 52, 40, 38, 40 | AC, 50,
                                   52 | SL, 43 | AC, 47, 50 | SL};
    /* VOWEL's long notes: B, C, A, B (whole bars) */
    static const int16_t pa[32] = {71, TI, TI, TI, TI, TI, TI, TI,   72, TI, TI, TI, TI, TI, TI, TI,
                                   69, TI, TI, TI, TI, TI, TI, TI,   71, TI, TI, TI, TI, TI, RS, RS};
    /* ORBITAL's ostinato, 16ths on E (its scale E minor: TURING's NOTES move it there) */
    static const int16_t os[16] = {64 | AC, 59, 67, 59, 64, 59, 71, 59, 64 | AC, 59, 67, 59, 66, 59, 62, 59};
    /* DISTROY's slow riff, eighths, two bars in four segments: E, the octave, G; D, E, B */
    static const int16_t ri[16] = {40 | AC, TI, 52, TI, 40, TI, 55, TI,   38 | AC, TI, 50, TI, 40, TI, 47, 52};
    /* BEACON's line over the wall: Em C D Em Am C D Em */
    static const int16_t le[64] = {71 | AC, TI, TI, TI, 76, TI, TI, TI,   79, TI, TI, TI, 76, TI, 74, 72,
                                   74, TI, TI, TI, 78, TI, 76, 74,        76, TI, TI, TI, TI, TI, TI, TI,
                                   72 | AC, TI, TI, TI, 76, TI, 81, TI,   79, TI, TI, TI, 76, TI, 72, TI,
                                   74, TI, 78, TI, 81, TI, 79, 78,        76, TI, TI, TI, TI, TI, RS, RS};
    chords(&BEDA, DRIFT, 4, 84);
    chords(&BEDB, WALL, 8, 118);
    notes(&FIG, fi, 32, 4, 100);
    notes(&ACID, ac, 16, 2, 45);
    ACID.swing = 56;
    ACID.step[9].ratchet = 2;
    ACID.step[14].prob = 60;
    notes(&PADS, pa, 32, 4, 100);
    notes(&OST, os, 16, 2, 40);
    notes(&RIFF, ri, 16, 4, 90);
    RIFF.seg_count = 4; RIFF.seg_split[0] = 4; RIFF.seg_split[1] = 8; RIFF.seg_split[2] = 12;
    RIFF.seg_order[3] = SD_SO_RND;
    notes(&LEAD, le, 64, 4, 100);
    /* U01: CHORDS, saved dark in a big hall: on NAV WARP 30 and XM 36; the FILTER on (LP12, CUTOFF 3.2 kHz, no KEY);
       SIZE 90, DECAY 82, R-LEVEL 46, TONE 34; D-LEVEL 30, FDBK 40 */
    U01 = SD_PATCH[SD_P_CHORDS];
    U01.warp = u99(30); U01.xm = u99(36);
    U01.fx_off &= (uint8_t)~SD_FX_FILTER; U01.f_type = SD_FT_LP12; U01.f_cut = 50000; U01.f_key = 0;
    U01.e3_a = U01.e1_a; U01.e3_d = U01.e1_d; U01.e3_s = U01.e1_s; U01.e3_r = U01.e1_r;
    wet(&U01, 46, 82, 90, 30, 40);
    U01.s_rtone = u99(34);
    /* U02 VOYAGE, U03 AURORA, U05 VOWEL: wet (the plate and the echoes up) */
    U02 = SD_PATCH[SD_P_VOYAGE]; wet(&U02, 52, 86, 99, 44, 56);
    U03 = SD_PATCH[SD_P_AURORA]; wet(&U03, 66, 92, 99, 40, 60);
    U05 = SD_PATCH[SD_P_VOWEL]; wet(&U05, 58, 88, 96, 36, 50);
    /* U04: BEACON with its MODE FREE (PP kept) and TIME at 1000 ms (its 1/4D is 1.13 s at 80 BPM: synced, half) */
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
static void gen_at(double s, uint32_t detents)  /* SCL + K4 on NAV: the harmonic generator on bank A */
{
    char t[64];
    uint32_t d;
    if (!at(s)) return;
    on_page(PG_NAV, "GEN off NAV");
    act();
    for (d = 0; d < detents; d++) gen = gen >= SD_GENS ? 0u : (gen + 1u) % SD_GENS;
    gen_seed += frame;
    sd_gen(&pat, 0, gen, &gen_seed);
    dirty = 1;
    snprintf(t, sizeof t, "SCL + K4: GEN %s on bank A", sd_gen_name(gen));
    say(t);
}
static void in_key(void) { e.xkey = 1; }       /* the transposes IN KEY: the app's default (SCL + EDIT on SEQ) */

/* the bed: CHORDS (U01) in POLY with the arp; then the looper (down, FREEZE and SHIMMER over it, the wobble; up two
   octaves); the wall's chords; its plate frozen, the loop down two octaves, out */
static void bed_setup(void)
{
    in_key();
    glo(SD_MODE_POLY);
    preset_user(&U01, "U01 CHORDS DARK HALL");
    page = PG_SEQA;
    pattern(0, &BEDA, "DRIFT (Em C D Em)");
    looper_mem();
    master = 0;
}
static int bed_score(void)
{
    static const uint8_t H[4] = {64, 67, 71, 76};   /* the arp's hand: E G B E */
    int i;
    if (at(BAR(3))) play(1);
    pot(__LINE__, 2200, BAR(3), BAR(5));        /* MASTER: in */
    if (at(BAR(3) + 2)) press("ARP", PG_ARP);   /* the arp in eighths over it */
    if (at(BAR(3) + 3)) {
        on_page(PG_ARP, "ARP's EDIT off its page");
        if (e.t[0].width + (e.t[1].mute ? 0u : e.t[1].width) + 1u > 4u) fail("no voice for ARP");
        e.arp_on = 1; e.arp_mode = SD_AM_UP; e.arp_div = 4; e.arp_oct = 1; e.arp_gate = 60;
        act();
        say("EDIT: ARP ON (K2: RATE 1/8)");
    }
    if (at(BAR(3) + 4)) { act(); e.arp_hold = 1; say("ALGORITHM: HOLD"); }
    if (at(BAR(3) + 5)) { act(); e.arp_oct = 2; say("K3: OCTAVES 2"); }
    for (i = 0; i < 4; i++) { if (at(BAR(3) + 6 + i * 0.1)) key(H[i], 1); if (at(BAR(3) + 8 + i * 0.1)) key(H[i], 0); }
    fx_to(4, BAR(5));                           /* TIME: LENGTH 1 BAR, GLIDE 1 BAR */
    if (at(BAR(5) + 5)) lp_length(12);
    if (at(BAR(5) + 7)) lp_glide_to(13);
    LP_LEVEL(79, BAR(5) + 9, BAR(6) + 8);      /* SCL + K4: L-LEVEL up: the looper armed (the delay 0.5 s) */
    /* the tape: the bed's last bar held, backwards, down an octave */
    if (at(BAR(7))) loop_toggle();
    if (at(BAR(7) + 1)) press("SEQ", PG_SEQA);  /* EDIT: the bed off, PLAY running on (STOP would park the loop) */
    if (at(BAR(7) + 2)) track_on(0, 0);
    if (at(BAR(7) + 3)) press("ARP", PG_ARP);
    if (at(BAR(7) + 4)) { act(); e.arp_on = 0; say("EDIT: ARP OFF"); }
    fx_to(4, BAR(7) + 5);
    if (at(BAR(7) + 10)) reverse_toggle();
    if (at(BAR(8))) loop_octave(0);             /* SCL + OCT-: down over the bar, onto its grid */
    /* SHIMMER and FREEZE on its plate: an octave up over the loop an octave down; the plate up */
    fx_to(3, BAR(9));
    SHIMMER(26, BAR(9) + 3, BAR(10));           /* SCL + ALGORITHM: SHIMMER 0 -> 41 % */
    KNOB99(PG_SPACE, pat.s_rlevel, 62, BAR(9) + 3, BAR(10));   /* K4: R-LEVEL */
    if (at(BAR(10))) freeze_toggle();
    if (at(BAR(11))) press("LFO", PG_LFO1);     /* LFO 2 onto its SPEED: the wobble */
    if (at(BAR(11) + 1)) press("LFO", PG_LFO2);
    KNOB99(PG_LFO2, pat.lfo_rate[1], 40, BAR(11) + 2, BAR(11) + 8);   /* K2: RATE ~0.4 Hz */
    if (at(BAR(11) + 9)) press("LFO", PG_TURING);
    if (at(BAR(11) + 10)) press("LFO", PG_MOD);
    if (at(BAR(11) + 11)) mod_slot(0, SD_MS_LFO2, SD_MD_LSPEED, 2);   /* LFO 2 -> L SPEED +2 % */
    /* the rise: FREEZE let go, forward, two octaves up over two bars */
    fx_to(3, BAR(11) + 13);
    if (at(BAR(12) + 2)) freeze_toggle();
    fx_to(1, BAR(12) + 4);
    if (at(BAR(12) + 6)) lp_glide_to(14);       /* SCL + K1: GLIDE 2 BAR */
    if (at(BAR(12) + 8)) reverse_toggle();
    if (at(BAR(13))) loop_octave(1);            /* SCL + OCT+: to its pitch ... */
    if (at(BAR(13) + 4)) loop_octave(1);        /* ... and on to +12 */
    /* the wall: the chords back, loud, the loop at +12 over them */
    if (at(BAR(16) + 4)) press("SEQ", PG_SEQA);
    if (at(BAR(16) + 6)) pattern(0, &BEDB, "WALL (Em C D Em Am C D Em)");
    pot(__LINE__, 4092, BAR(16) + 8, BAR(17));
    if (at(BAR(16) + 14)) play(0);              /* a breath: STOP parks the loop; the bed on while stopped; START: */
    if (at(BAR(16) + 15)) track_on(0, 1);       /* the wall from its step 1, the loop back in sync on the one */
    if (at(BAR(17))) play(1);
    /* its freeze: the plate held as the wall stood, the chords off; the loop down two octaves, out */
    fx_to(3, BAR(24) + 10);
    if (at(BAR(25))) freeze_toggle();
    if (at(BAR(25) + 1)) press("SEQ", PG_SEQA); /* EDIT: the chords off, PLAY running on under the loop */
    if (at(BAR(25) + 2)) track_on(0, 0);
    fx_to(4, BAR(25) + 3);
    if (at(BAR(25) + 7)) loop_octave(0);        /* SCL + OCT-: to its pitch ... */
    if (at(BAR(25) + 11)) loop_octave(0);       /* ... and on to -12 */
    LP_LEVEL(1, BAR(27) + 10, BAR(30));         /* SCL + K4: L-LEVEL out, to 1 (armed still: at 0 the delay's line
                                                   would change under its echoes) */
    if (at(BAR(30) + 2)) loop_toggle();
    if (at(BAR(30) + 3)) play(0);
    fx_to(3, BAR(30) + 4);
    if (at(BAR(31))) freeze_toggle();           /* the frozen wall let go */
    pot(__LINE__, 0, BAR(31), BAR(34));
    return q >= BAR(34);
}

/* the low: GROWL's low E, its lowpass opening; its acid, dark, TURING on the CUTOFF; DISTROY's wall (BREAK, MOVE,
   the top band CRUSHed and up) */
static void low_setup(void)
{
    in_key();
    preset(SD_P_GROWL);
    page = PG_SEQA;
    pattern(0, &ACID, "ACID (E minor, SLIDE, ratchet, PROB, swing 56)");
    page = PG_NAV;
    oct = -2;                                   /* (OCT- twice before: the keys from F1) */
    master = 0;
}
static int low_score(void)
{
    int32_t v;
    if (at(0.2)) press("ENV", PG_ENV);
    if (at(1.4)) press("ENV", PG_FILTER);
    if (at(2.6)) {                              /* EDIT: the filter on (the app's first time: open, ENV 3 = ENV 1) */
        on_page(PG_FILTER, "EDIT off FILTER");
        act();
        pat.fx_off &= (uint8_t)~SD_FX_FILTER;
        pat.f_type = SD_FT_LP12; pat.f_cut = 22050;
        pat.e3_a = pat.e1_a; pat.e3_d = pat.e1_d; pat.e3_s = pat.e1_s; pat.e3_r = pat.e1_r;
        dirty = 1;
        say("EDIT: FILTER ON (LP12)");
    }
    if (at(3.6)) { act(); pat.f_type = SD_FT_LP24; dirty = 1; say("ALGORITHM: LP24"); }
    if (at(4.5)) key(40, 1);                    /* the low E */
    pot(__LINE__, 3000, 4, BAR(3));
    on_ramp_page(PG_FILTER, 5, BAR(5) + 8);
    if (ramp(__LINE__, pat.f_cut / 147, 33000 / 147, 5, BAR(5) + 8, &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    /* TURING, set while it drones: to the CUTOFF (17 detents), 30 %, CHANGE 25 */
    lfo_to_turing(BAR(6));
    TUR_DEST_BY(17, BAR(6) + 2.2, BAR(6) + 4.6);
    TUR_AMOUNT(30, BAR(6) + 5, BAR(6) + 10);
    TUR_CHANGE(25, BAR(6) + 10, BAR(6) + 14);
    if (at(BAR(8) + 8)) key(40, 0);
    /* the acid, dark under the frozen plate, opening toward the rise */
    if (at(BAR(9))) play(1);
    if (at(BAR(9) + 4)) tur_edit();
    if (at(BAR(10))) press("ENV", PG_ENV);
    if (at(BAR(10) + 1)) press("ENV", PG_FILTER);
    on_ramp_page(PG_FILTER, BAR(10) + 2, BAR(10) + 12);
    if (ramp(__LINE__, pat.f_cut / 147, 29000 / 147, BAR(10) + 2, BAR(10) + 12, &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    KNOB99(PG_FILTER, pat.f_res, 46, BAR(10) + 2, BAR(10) + 12);
    on_ramp_page(PG_FILTER, BAR(11), BAR(12) + 14);
    if (ramp(__LINE__, pat.f_cut / 147, 39000 / 147, BAR(11), BAR(12) + 14, &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    KNOB99(PG_FILTER, pat.f_res, 54, BAR(11), BAR(12) + 14);
    if (at(BAR(13))) play(0);
    /* the rise, resting: the riff, DISTROY */
    if (at(BAR(13) + 2)) press("SEQ", PG_SEQA);
    if (at(BAR(13) + 3)) pattern(0, &RIFF, "WALL RIFF (E, eighths, 4 segments: RND)");
    home_to(2, BAR(14));
    if (at(BAR(14) + 4)) preset(SD_P_DISTROY);
    pot(__LINE__, 4092, BAR(14) + 4, BAR(15));
    /* the wall */
    if (at(BAR(17))) play(1);
    KNOB99(PG_MACROS, pat.mac[2], 62, BAR(17), BAR(19));   /* BREAK */
    KNOB99(PG_MACROS, pat.mac[1], 60, BAR(18), BAR(20));   /* MOVE */
    fx_to(2, BAR(20) + 4);
    if (at(BAR(20) + 7)) { act(); say("ALGORITHM: band 4 (the top)"); }
    if (at(BAR(20) + 9)) {                      /* SCL + ALGORITHM: the top band's shaper to CRUSH */
        on_page(PG_DISTROY, "a shaper off DISTROY");
        act();
        pat.d_type[3] = SD_DS_CRUSH;
        dirty = 1;
        say("SCL + ALGORITHM: SHAPE CRUSH (band 4)");
    }
    on_ramp_page(PG_DISTROY, BAR(20) + 10, BAR(23));
    if (ramp(__LINE__, (int32_t)d99(pat.d_gain[3]), 84, BAR(20) + 10, BAR(23), &v)) { pat.d_gain[3] = u99(v); dirty = 1; }
    if (at(BAR(25) + 1)) play(0);
    if (at(BAR(25) + 2)) {                      /* EDIT: DISTROY off, its loop with it (BREAK up keeps it ringing) */
        on_page(PG_DISTROY, "DISTROY's EDIT off its page");
        act();
        pat.fx_off |= SD_FX_DISTROY;
        dirty = 1;
        say("EDIT: DISTROY OFF");
    }
    pot(__LINE__, 0, BAR(25) + 1, BAR(25) + 2);
    return q >= BAR(27);
}

/* the motion: VOYAGE (U02, wet), its figure (COURSE, the generators); VOWEL (U05, wet), long notes through the
   Reactor; ORBITAL, its ostinato, TURING's NOTES, MOVE, MUTATE */
static void motion_setup(void)
{
    in_key();
    preset_user(&U02, "U02 VOYAGE WET");
    page = PG_SEQA;
    pattern(0, &FIG, "FIGURE (E B G F#)");
    page = PG_NAV;
    master = 0;
}
static int motion_score(void)
{
    int32_t v;
    if (at(0)) play(1);
    pot(__LINE__, 3000, 0, BAR(3));
    KNOB99(PG_NAV, pat.warp, 26, 2, BAR(5));    /* K2: WARP */
    KNOB99(PG_NAV, pat.xm, 36, BAR(2), BAR(6)); /* K1: XM */
    gen_at(BAR(3) - 0.5, 1);                    /* ALL */
    gen_at(BAR(5) - 0.5, 1);                    /* ODD */
    if (at(BAR(8))) play(0);
    /* VOWEL's long notes */
    if (at(BAR(8) + 2)) press("SEQ", PG_SEQA);
    if (at(BAR(8) + 3)) pattern(0, &PADS, "LONG NOTES (B C A B)");
    if (at(BAR(8) + 6)) press("FX", PG_REACTOR);
    if (at(BAR(8) + 10)) preset_user(&U05, "U05 VOWEL WET");
    pot(__LINE__, 2200, BAR(8) + 10, BAR(9));
    if (at(BAR(9))) play(1);
    on_ramp_page(PG_REACTOR, BAR(9) + 2, BAR(11));
    if (ramp(__LINE__, pat.r_pos / 26, 700 / 26, BAR(9) + 2, BAR(11), &v)) { pat.r_pos = (uint16_t)(v * 26); dirty = 1; }
    KNOB99(PG_REACTOR, pat.r_res, 56, BAR(9) + 4, BAR(11));
    if (at(BAR(11) + 1)) {                      /* SCL + K2: MODEL to BP12 */
        on_page(PG_REACTOR, "MODEL off REACTOR");
        act();
        pat.r_model = SD_RM_BP12;
        dirty = 1;
        say("SCL + K2: MODEL BP12");
    }
    on_ramp_page(PG_REACTOR, BAR(11) + 2, BAR(12) + 14);
    if (ramp(__LINE__, pat.r_pos / 26, 1400 / 26, BAR(11) + 2, BAR(12) + 14, &v)) { pat.r_pos = (uint16_t)(v * 26); dirty = 1; }
    if (at(BAR(13))) play(0);
    /* the rise: ORBITAL, its ostinato, TURING's NOTES (set while off), MOVE */
    if (at(BAR(13) + 1)) press("SEQ", PG_SEQA);
    if (at(BAR(13) + 2)) pattern(0, &OST, "OSTINATO (E)");
    if (at(BAR(13) + 4)) press("HOME", PG_NAV);
    if (at(BAR(13) + 6)) preset(SD_P_ORBITAL);
    pot(__LINE__, 2600, BAR(13) + 6, BAR(14));
    lfo_to_turing(BAR(13) + 8);                 /* NOTES, 36 %, CHANGE 30 */
    TUR_DEST_BY(-1, BAR(13) + 10.5, BAR(13) + 11.5);
    TUR_AMOUNT(36, BAR(13) + 12, BAR(14) + 2);
    TUR_CHANGE(30, BAR(14) + 2, BAR(14) + 6);
    if (at(BAR(14) + 8)) tur_edit();
    if (at(BAR(15))) play(1);
    home_to(2, BAR(15) + 2);
    KNOB99(PG_MACROS, pat.mac[1], 55, BAR(15) + 4, BAR(18));   /* MOVE */
    KNOB99(PG_MACROS, pat.mac[0], 35, BAR(19), BAR(22));       /* MUTATE */
    lfo_to_turing(BAR(22) + 2);
    TUR_CHANGE(55, BAR(22) + 5, BAR(22) + 10);
    if (at(BAR(25) + 1)) play(0);
    pot(__LINE__, 0, BAR(25) + 1, BAR(25) + 2);
    return q >= BAR(27);
}

/* the lead: AURORA (U03, wet) swelling far back in POLY; BEACON (U04) in MONO over the wall, DROWN up; its last E in
   its echoes, FDBK up */
static void lead_setup(void)
{
    in_key();
    glo(SD_MODE_POLY);
    preset_user(&U03, "U03 AURORA WET");
    page = PG_SEQA;
    pattern(0, &LEAD, "LEAD (over the wall)");
    page = PG_NAV;
    master = 2400;                              /* (far back: the hall carries it) */
}
static int lead_score(void)
{
    if (at(BAR(2) + 8)) key(71, 1);             /* B, E, D, B swelling over each other */
    if (at(BAR(3) + 8)) key(76, 1);
    if (at(BAR(4))) key(71, 0);
    if (at(BAR(5))) key(74, 1);
    if (at(BAR(5) + 8)) key(76, 0);
    if (at(BAR(6) + 8)) key(71, 1);
    if (at(BAR(7))) key(74, 0);
    if (at(BAR(8) + 8)) key(71, 0);
    /* BEACON for the wall */
    if (at(BAR(14))) preset_user(&U04, "U04 BEACON 1 S");
    if (at(BAR(14) + 2)) glo(SD_MODE_MONO);
    home_to(1, BAR(14) + 4);                    /* (MACROS: DROWN) */
    pot(__LINE__, 2600, BAR(14) + 4, BAR(16));
    if (at(BAR(17))) play(1);
    KNOB99(PG_MACROS, pat.mac[3], 72, BAR(19), BAR(24));   /* MACRO 4: DROWN */
    /* its last E, its echoes up */
    if (at(BAR(25))) play(0);
    fx_to(4, BAR(25) + 1);
    if (at(BAR(25) + 6)) key(76, 1);
    KNOB99(PG_TIME, pat.s_fb, 72, BAR(25) + 7, BAR(27));      /* K2: FDBK */
    if (at(BAR(28))) key(76, 0);
    KNOB99(PG_TIME, pat.s_fb, 30, BAR(29), BAR(31));
    return q >= BAR(34);
}

static void (*const SETUP[PASSES])(void) = {bed_setup, low_setup, motion_setup, lead_setup};
static int (*const SCORE[PASSES])(void) = {bed_score, low_score, motion_score, lead_score};
static void setup(void) { patterns(); SETUP[pass](); }
static int score(void) { return SCORE[pass](); }

int main(int argc, char **argv)
{
    bpm = 80;
    return layer_main(argc, argv, "heliopause", PASS_NAME, GAIN, PASSES, setup, score);
}
