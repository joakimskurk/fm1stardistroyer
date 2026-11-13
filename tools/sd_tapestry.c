/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* TAPESTRY: one FM-1 played by one person, the looper building the bed (2026-11-11, the user: "we implement reverse
 * too, and you make a demo with it in use, a little longer; this time with the reverb on too, so we get the carpet
 * of sound I wanted").
 *
 *   sd_tapestry OUT.wav
 *
 * RANGE's rules (tools/sd_play.h): one engine, one stock patch at a time, the app's controls on their pages at a
 * hand's speed, the voice rules, MASTER as platform.c applies it, no mix. The looper's memory is the FM-1's own (since
 * 2026-11-11: half the delay line, lent while L-LEVEL is above 0, and 24 KB: 5.1 s, mono mu-law): armed before the
 * LOOP, the delay then 0.5 s. D minor, 84 BPM.
 *
 *   1-8    PRISM, MONO: SEQ A's falling sixteenths (Dm9, Bbmaj7, C) cascading in its dotted-quarter echoes; the plate
 *          opened up (R-LEVEL, DECAY)
 *   9      LOOP: the last bar round; SEQ A off (EDIT): the loop carries it, PLAY running on
 *   10-12  REVERSE: the cascades backwards, swelling into each note; GLIDE 1 BAR, and SCL + OCT- an octave down,
 *          a tape slowing over the bar and landing on the loop's own grid
 *   13-20  AURORA, POLY: a slow line over the bed, in the plate; LFO 2 slow (0.7 Hz) onto the looper's SPEED, +2 %
 *          (+-0.24 semitones: a tape's wobble, the loop's grid with it); FREEZE holds the plate on its A, SHIMMER
 *          climbs
 *   21-24  SCL + OCT+: back up to pitch over a bar, on its grid; REVERSE off: the cascades forward again
 *   25-28  FREEZE off, SHIMMER down, the looper faded out (L-LEVEL), LOOP off; a last chord, MASTER down, the plate's
 *          tail */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"

static SdPattern ARP;

static void patterns(void)
{
    static const int16_t arp[32] = {
        62, 69, 72, 76, 77, 76, 72, 69, 62, 69, 72, 76, 77, 81, 76, 72,    /* Dm9 */
        58, 65, 69, 74, 76, 74, 69, 65, 60, 67, 70, 74, 77, 74, 70, 67};   /* Bbmaj7, C (add9) */
    line(&ARP, arp, 32, 40);
}

static void octave(int to)                      /* OCT- / OCT+ */
{
    act();
    oct = to;
    say(to < 0 ? "OCT-" : "OCT+");
}

/* a note of the line: down at s0, up just before s1 (a hand moving to the next key) */
static void note(uint32_t n, double s0, double s1)
{
    if (at(s0)) key(n, 1);
    if (at(s1 - 0.25)) key(n, 0);
}

static void opening(void)                       /* bars 1-8: the cascades, the plate opened */
{
    if (at(0)) play(1);
    KNOB99(PG_SPACE, pat.s_rlevel, 68, 8, 56);  /* K4: R-LEVEL 52 -> 68 */
    KNOB99(PG_SPACE, pat.s_decay, 93, 24, 88);  /* K2: DECAY 87 -> 93 */
    KNOB99(PG_SPACE, pat.s_size, 99, 40, 96);   /* K1: SIZE 98 -> 99 */
    if (at(100)) press("FX", PG_TIME);
    if (at(104)) lp_length(12);                 /* SCL + K2: LENGTH 1 BAR (two do not fit at 84 BPM) */
    LP_LEVEL(79, 106, 114);                     /* SCL + K4: L-LEVEL up: the looper armed (the delay 0.5 s) */
}

static void bed(void)                           /* bars 9-12: the loop, backwards, an octave down */
{
    if (at(128)) loop_toggle();                 /* the last bar, held */
    if (at(129)) press("SEQ", PG_SEQA);         /* EDIT: SEQ A off: the loop carries it (PLAY runs on: STOP would
                                                   park the loop) */
    if (at(130)) track_on(0, 0);
    if (at(131)) press("FX", PG_REACTOR);       /* back to TIME */
    if (at(132)) press("FX", PG_DISTROY);
    if (at(133)) press("FX", PG_SPACE);
    if (at(134)) press("FX", PG_TIME);
    if (at(144)) reverse_toggle();
    if (at(152)) lp_glide_to(13);               /* SCL + K1: GLIDE 1 BAR */
    if (at(160)) loop_octave(0);                /* SCL + OCT-: an octave down, over the bar */
}

static void line_over(void)                     /* bars 13-20: AURORA over the bed; FREEZE, SHIMMER */
{
    if (at(192)) preset(SD_P_AURORA);
    if (at(194)) glo(SD_MODE_POLY);
    note(74, 196, 200); note(72, 200, 208);                         /* bar 13: D C */
    note(69, 208, 224);                                             /* 14: A */
    if (at(208)) press("LFO", PG_LFO1);
    if (at(209)) press("LFO", PG_LFO2);
    KNOB99(PG_LFO2, pat.lfo_rate[1], 46, 210, 218);   /* K2: RATE 0.02 -> 0.7 Hz */
    if (at(219)) press("LFO", PG_TURING);
    if (at(220)) press("LFO", PG_MOD);
    if (at(221)) mod_slot(3, SD_MS_LFO2, SD_MD_LSPEED, 2);   /* the 4th slot: LFO 2 -> L SPEED +2 % */
    note(70, 224, 232); note(69, 232, 236); note(67, 236, 240);     /* 15: Bb A G */
    if (at(240)) key(69, 1);                                        /* 16: A, held into the FREEZE */
    if (at(244)) press("FX", PG_REACTOR);
    if (at(245)) press("FX", PG_DISTROY);
    if (at(246)) press("FX", PG_SPACE);
    if (at(248)) freeze_toggle();
    if (at(254)) key(69, 0);
    SHIMMER(24, 256, 288);                      /* SCL + ALGORITHM: SHIMMER 0 -> 38 % */
    note(77, 256, 264); note(76, 264, 272);                         /* 17: F E */
    note(74, 272, 288);                                             /* 18: D */
    note(72, 288, 296); note(74, 296, 304);                         /* 19: C D */
    note(76, 304, 320);                                             /* 20: E */
}

static void return_up(void)                     /* bars 21-24: to pitch, forward */
{
    if (at(306)) press("FX", PG_TIME);
    if (at(320)) loop_octave(1);                /* SCL + OCT+: back to its pitch, over the bar */
    note(69, 320, 336); note(74, 336, 352);                         /* 21, 22: A D */
    if (at(352)) reverse_toggle();
    note(72, 352, 360); note(70, 360, 368); note(69, 368, 384);     /* 23, 24: C Bb A */
}

static int ending(void)                         /* bars 25-30: FREEZE off, the looper out, a last chord */
{
    if (at(356)) press("FX", PG_REACTOR);
    if (at(357)) press("FX", PG_DISTROY);
    if (at(358)) press("FX", PG_SPACE);
    if (at(384)) freeze_toggle();
    SHIMMER(0, 386, 410);
    if (at(412)) press("FX", PG_TIME);
    LP_LEVEL(1, 416, 456);                      /* SCL + K4: L-LEVEL 79 -> 1 (armed still: at 0 the delay's line
                                                   would change under its echoes) */
    if (at(416)) { key(62, 1); key(65, 1); key(69, 1); key(76, 1); }   /* Dm(add9), held */
    if (at(458)) loop_toggle();
    if (at(448)) { key(62, 0); key(65, 0); key(69, 0); key(76, 0); }
    pot(__LINE__, 2400, 448, 480);
    return q >= 480;
}

static void setup(void)                         /* before the first note: PRISM, MONO, the pattern; the looper's memory */
{
    patterns();
    preset(SD_P_PRISM);
    page = PG_SEQA;
    pattern(0, &ARP, "ARP (Dm9 Bbmaj7 C)");
    page = PG_SPACE;
    looper_mem();
    master = 4092;                              /* (MASTER at its top) */
}

static int score(void)
{
    opening();
    bed();
    line_over();
    return_up();
    return ending();
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sd_tapestry OUT.wav\n"); return 2; }
    (void)octave;
    bpm = 84;
    return play_run(argv[1], "tapestry", setup, score);
}
