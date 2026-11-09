/* SPDX-License-Identifier: GPL-3.0-only */
/* Star Distroyer's sound engine (Sound Plan v0.1). No platform calls: the firmware (stardistroyer.c) and the host
 * tools (tools/sd_render.c, tests/sd_test.c) include the same file.
 *
 * Every interrupt's 256 frames are rendered as sub-blocks of SD_SUB (32) frames. At the start of a sub-block the
 * control tick runs: notes from the main loop, the note driver, the envelopes, the nav, every bank's parameters
 * for the sub-block. Inside it only audio-rate work, with levels, gains and the fold gain ramped across it.
 *
 * V3: four banks of four oscillators (Sound Plan §3.0):
 *   oscillator  phase += inc;  p = phase + PM in;  shape(p) -> squash -> fold;  -> * level to its target
 *   frequency   harmonic 1..32 of the note, fine tune +-99 cents, plus the bank's detune spread (DT)
 *   roles       left to right: C carrier (to the bank's output), S series modulator (modulates the oscillator just
 *               right of it), P parallel modulator (the nearest carrier right of it); with nothing there, silent
 *   XM          the FM amount: a modulator's PM index is its level x ENV2 x XM; at all three full, 4 pi rad
 *   WP warp     sine -> triangle (20) -> saw (40) -> pulse (99): the cosine mixed toward the triangle (both odd
 *               harmonics only), the triangle skewed into a saw, the saw squashed by a cubic soft clip into a pulse
 *   FD fold     a triangle fold of every oscillator's waveform, closed form
 *   Q page      XM WP FD DT for all banks; each bank adds its own offsets (as a modulation slot's offset)
 *   ENV1, ENV2  per voice: ENV1 the level, ENV2 the FM amount (and, as a modulation slot would, WP and FD)
 *   modes       MONO: one voice, the four banks mixed by the nav X/Y (NAV); DUO: two voices, banks
 *               A B and their copies, mixed by X; POLY: four voices, each bank playing bank A
 *   nav      bilinear weights from X/Y, normalised by (sum w^2)^(1/4): +3 dB for coherent banks at the centre,
 *               -3 dB for unrelated ones, between equal gain and equal power. COURSE: four stored X/Y cells, played
 *               from the gate (triggered)
 *   stereo      a balance per bank, A and D left, B and C right, by WIDTH (a pan per bank); at WIDTH 0
 *               both outputs carry everything at full level
 *   aliasing    gentle tracking by the note above C5 (SD_TRACK): XM, FD and the squash fall 6 dB an octave, the
 *               saw's edge is never shorter than a sample of the note, an oscillator fades out from 16 to 20 kHz, a
 *               modulator from 4 to 12 kHz (its sidebands: the screech of FM up high)
 * The kernel has eight variants (shape sine / blend / triangle-saw, squash, fold), picked once a sub-block per bank:
 * no branch on a parameter inside the loop.
 *
 * R1, the Reactor (Sound Plan §4.0): the voice, summed to mono and DRIVEn, through four
 * band-pass filters in parallel, each panned, mixed with the dry voice by MIX (0: the Reactor is not there at all).
 *   filters     TPT state-variable filters in integers: BP6 (one, its band-pass output: louder as it rings), BP12 (two
 *               in series, steeper), VBP (one, normalised: RES sets the width, not the loudness). One model for all
 *   stages      six, each a frequency (log pitch) and a level for every filter; the pan is a filter's own
 *   POS         0.000 .. 5.999 between the stages, in log pitch; at SLEW 0 it jumps stage to stage ("an abrupt
 *               jump"), above it the filters glide toward the target, a one-pole on pitch and level
 *   clock       the stage advances with the note driver's steps: a division, a range, FWD BWD PING-PONG RANDOM
 *   frequency   a global offset in semitones, plus the last note's distance from C4 when key tracking is on
 *   level       up to 2x: past 1 a soft clip (a zone of saturation)
 * R2, the Reactor's other models, in parallel (KP) or in series (the allpasses):
 *   KP          a comb per filter: a delay line fs / f long (read between samples, linearly), Karplus-Strong's
 *               two-point average in the loop (its half sample taken off the length), feedback 0 .. 0.995 by RES
 *   AP1         four Schroeder allpasses in series on delay lines: as a filter's pitch moves its delay slides, and
 *               the pitch "skates" (kept on purpose)
 *   AP2         eight first-order IIR allpasses in series, two a filter: no skating
 *   AP outputs  the four levels' mean, no pan: left the wet minus RES x the dry, right the wet plus it (a
 *               subtracted and an added dry): RES 0 a flat allpass, RES up the notches
 * R3, the Reactor's nonlinear layer, before DISTROY's:
 *   in the loop  a soft saturation where each filter's energy turns back on itself: an SVF's band-pass before it
 *               updates its integrator (it rings to ~6 x full scale, then flattens), KP's line (to ~3 x): high
 *               RES and DRIVE ring into saturation, never away
 *   level law   SD_RLEVEL: the first half clean, 0 .. 1; the second 1 .. 4, into the soft clip at the output
 *   make-up     each model's own gain (SD_RMAKEUP), so a model change does not jump in level; BP6's and BP12's
 *               loudness growing with RES stays, by design
 *   dry path    stereo: MIX blends the voice's own stereo with the Reactor's panned wet
 *
 * D1, DISTROY (Sound Plan §5.0): four frequency bands, each with its distortion: a GAIN, a type, an ALT, and BIAS,
 * a DC offset before the distortion:
 *   crossover   two, a patch's choice (D1's A/B, Sound Plan §5.1):
 *               CMP  per channel three 2-pole lowpasses (TPT SVFs, Butterworth) at the splits; the bands LP1,
 *                    LP2 - LP1, LP3 - LP2, x - LP3 sum to the input exactly; a neighbour band passes -4 .. -7 dB
 *               LR4  a Linkwitz-Riley tree: split 2 first, then 1 on the low side and 3 on the high; each split two
 *                    Butterworth stages (low: lowpass twice, high: highpass twice), and each side an allpass at the
 *                    other side's split, so the bands sum to an allpass: flat in level; 11 SVFs a channel
 *   a band      GAIN (0 .. +36 dB) -> + BIAS -> its shaper (NONE SOFT HARD FOLD CRUSH, each with its ALT) -> a DC
 *               blocker -> LEVEL (the Reactor's level law) -> PAN (a balance)
 *   shapers     SOFT  a knee: linear to T, a cubic shoulder to the ceiling; the negative half's ceiling lower
 *                     (asymmetric: even harmonics); ALT = hardness (T from 0 to 0.6)
 *               HARD  a clip at full scale; ALT = the knee (0 a brick wall, up to half the range a shoulder)
 *               FOLD  the voice's triangle fold of x / t; ALT = the spacing (t from full scale to a quarter)
 *               CRUSH clipped, then bits taken off toward zero; ALT = the depth (15 bits .. 2)
 *   MIX         dry <-> wet; 0 is no DISTROY at all
 *
 * S1, SPACE (Sound Plan §5.5, the user's design): a delay into a reverb, last in the chain.
 *   delay       stereo, or ping-pong (the mono sum in on the left, the sides crossing back); TIME free 1..1000 ms or
 *               synced to the note driver's sixteenth (1/32 .. 1/2, triplets, dotted); a new time glides there (a tape,
 *               ~12 ms); in its loop TONE (a one-pole lowpass), FDBK (0 .. 0.95), a soft ceiling at ~1.9 x
 *   reverb      four Schroeder allpasses (diffusion), then a 4-line FDN mixed by a Hadamard matrix; SIZE scales the
 *               lines (0.25 .. 1 of 2903 3331 3713 4049 samples), DECAY their gain (SD_SP_G: T60 0.3 .. 20 s at
 *               3500), TONE a one-pole lowpass in each line; its input the dry and the echoes: the echoes ring too
 *   out         dry + D-LEVEL x the echoes + R-LEVEL x the reverb; both levels 0: no SPACE. It rests when nothing
 *               comes in and its tails have stayed under the floor for 2 s
 *   memory      ~213 KB (SdSpaceMem), the app's (the pool on the FM-1): the engine only points at it
 *
 * The output (2026-11-09, the user: "a little internal clipping with extreme modulations"; measured: 20 of the 29
 * patches flattened up to a fifth of their samples at the output with the macros and four slots at full, one even
 * as it is, a POLY chord at velocity 127): a limiter, 64 frames (1.45 ms) ahead, to 0.85 of full scale (-1.4 dBFS): a
 * sub-block's gain reaches what the next two need before they come out, so a peak is turned down, never squared off;
 * back up over ~0.2 s. The shoulder after it (linear to 0.85, a cubic one to full scale) stays, a last safety.
 *
 * Rules (docs/RULES.md): integers only, no 64-bit division, no loop whose length a parameter sets. */
#ifndef SD_ENGINE_H
#define SD_ENGINE_H
#include <stdint.h>
#include "sd_tables.h"
#include "sd_seq.h"

#define SD_FS 44100u
#define SD_SUB 32u                              /* frames per sub-block = the control period (1378 Hz) */
#define SD_LIM_D 64u                            /* the output limiter's look-ahead: two sub-blocks */
#define SD_LIM_T 27853                          /* its ceiling: 0.85 of full scale (where the shoulder starts) */
#define SD_SUB_SHIFT 5u
#define SD_QLEN 32u                             /* main loop -> audio note queue */
#define SD_HELD 12u                             /* the hand's keys remembered while held (more: the oldest goes) */
#define SD_OPS 4u                               /* oscillators in a bank */
#define SD_BANKS 4u
#define SD_VOICES 4u
#define SD_ONE (1 << 30)                        /* envelope full scale */
#define SD_PM_SHIFT 18u                         /* PM in (Q15, 1.0 = full level) -> phase: 2 cycles = 4 pi rad */
#define SD_CARRIER 0xFFu                        /* an oscillator's target: the output */
#define SD_NOWHERE 0xFEu                        /* a modulator with nothing to its right */
#define SD_DT_CENTS 7                           /* detune spread at DT max: the oscillators at -21 -7 +7 +21 cents */
#define SD_G_MIN 13u                            /* the squash's drive where it starts, Q8 (0.05: a no-op) */
#define SD_INC_4K 389565315u                    /* a modulator above 4 kHz fades, gone at 12 kHz: its sidebands are
                                                   the FM's screech up high (the user: "screaming high overtones") */
#define SD_INC_12K 1168695946u
#define SD_INC_16K 1558261261u                  /* 16 kHz and 20 kHz as phase increments: the fade-out above */
#define SD_INC_20K 1947826576u
#define SD_POLY_GAIN 16384                      /* a POLY voice's level, Q15: -6 dB, four voices share the output (R4: -3 dB
                                                   clipped CHORDS and FUSION in chords once the Reactor kept the dry) */

#if defined(__GNUC__)
#define SD_INLINE static inline __attribute__((always_inline))
#define SD_BARRIER() __asm__ volatile("" ::: "memory")
#else
#define SD_INLINE static inline
#define SD_BARRIER()
#endif

/* ---- numbers */
static int32_t sd_sine(uint32_t ph)             /* a full cycle over 2^32 -> -32767..32767 (Dinghy's refined parabola) */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

SD_INLINE int32_t sd_lut(uint32_t p)            /* the table's sine, a cycle over 2^32, linear interpolation */
{
    uint32_t i = p >> 22;
    int32_t a = SD_SIN[i], b = SD_SIN[i + 1u];
    return a + (((b - a) * (int32_t)(p >> 6 & 0xFFFFu)) >> 16);   /* |b - a| <= 202: fits */
}

static uint32_t sd_isqrt(uint32_t x)            /* floor(sqrt(x)), 16 fixed steps */
{
    uint32_t r = 0, b = 1u << 30, i;
    for (i = 0; i < 16u; i++) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

static const uint32_t SD_INC_C9[12] = {         /* MIDI 120..131: f * 2^32 / 44100; lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t sd_note_inc(uint32_t note) { return note > 127u ? 0u : SD_INC_C9[note % 12u] >> (10u - note / 12u); }

/* ---- the patch */
enum { SD_ROLE_C, SD_ROLE_S, SD_ROLE_P };
enum { SD_MODE_MONO, SD_MODE_DUO, SD_MODE_POLY, SD_MODES };

typedef struct {
    uint8_t harm;                               /* 1..32 x the note */
    int8_t fine;                                /* -99..99 cents */
    uint8_t role, pad;
    uint16_t level;                             /* 0..65535 */
} SdOscPatch;

typedef struct {
    SdOscPatch osc[SD_OPS];
    int16_t xm, wp, fd, dt;                     /* this bank's offsets on the Q page, -32767..32767 = -1..+1 */
} SdBankPatch;

typedef struct {
    uint16_t pitch[4];                          /* each filter: MIDI x 256 (log pitch), SD_R_LO..SD_R_HI */
    uint16_t level[4];                          /* 0..65535 = 0..2 (past 1: the soft clip) */
} SdReactorStage;
enum { SD_RM_BP6, SD_RM_BP12, SD_RM_VBP, SD_RM_KP, SD_RM_AP1, SD_RM_AP2, SD_RMODELS };
enum { SD_RT_KEY = 1, SD_RT_HARM = 2 };          /* r_track's bits: follow the key; snap to its harmonics (R4) */
#define SD_DL 2048u                             /* a Reactor delay line, samples: 21.5 Hz the lowest comb */
/* each model's make-up, Q12: measured (tests/sd_test.c, test_reactor_makeup) on a saw through VOWEL's stage A at RES
 * and level one half, so a change of model keeps the level (within the test's tolerance) */
static const int32_t SD_RMAKEUP[6] = {2769, 4240, 18939, 720, 3736, 3779};   /* measured: BP6 +3.4 BP12 -0.3 VBP -13.3
                                                   KP +15.1 AP1 +0.8 AP2 +0.7 dB from the dry, taken off */
enum { SD_RC_FWD, SD_RC_BWD, SD_RC_PP, SD_RC_RND, SD_RCMODES };
enum { SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_FOLD, SD_DS_CRUSH, SD_DSHAPES };
enum { SD_DX_CMP, SD_DX_LR4 };
#define SD_DFLEN 1024u                          /* DISTROY's feedback line, a channel (a power of two: masked) */
#define SD_DFMIN 44                             /* its shortest delay, 1 ms: longer than a call (32 frames), so a
                                                   call's feedback is read before the call writes any */
#define SD_SP_DMAX 44100u                       /* SPACE's delay line, a channel: 1 s */
#define SD_SP_FLEN 2048u                        /* its reverb's lines (a power of two: masked), at half rate */
#define SD_SP_APLEN 256u                        /* its diffusers, at half rate */
enum { SD_SP_SYNC = 1, SD_SP_PP = 2 };          /* s_mode's bits */
enum { SD_FX_REACTOR = 1, SD_FX_DISTROY = 2, SD_FX_SPACE = 4, SD_FX_FILTER = 8 };   /* fx_off's bits */

/* C1: the modulation (Sound Plan §7.x; the spec's "one global matrix"): sources, destinations, a slot */
enum { SD_MS_OFF, SD_MS_ENV1, SD_MS_ENV2, SD_MS_LFO1, SD_MS_LFO2, SD_MS_NAVX, SD_MS_NAVY, SD_MS_VEL, SD_MS_KEY,
       SD_MS_WHEEL, SD_MS_AFTER, SD_MS_RAND,
       /* the engines talking to each other (the user: "everything talking together, in unorthodox ways"): where the
          sequencer is in its pattern (STEP, -1 .. +1 across it) and which segment (SEG); DISTROY's low and top bands'
          energy (D LOW, D TOP: -48 .. 0 dBFS as 0 .. 1); the Reactor's stage (R STAGE, 1..6 as -1 .. +1); SPACE's
          echoes (ECHO: their level, followed) */
       SD_MS_STEP, SD_MS_SEG, SD_MS_DLOW, SD_MS_DTOP, SD_MS_RSTAGE, SD_MS_ECHO,
       SD_MS_ENV3,                              /* the filter's envelope */
       SD_MS_TUR,                               /* TURING: the shift register's 8 newest bits, a step on its clock */
       SD_MSRCS };
enum { SD_MD_OFF, SD_MD_PITCH, SD_MD_XM, SD_MD_WARP, SD_MD_FOLD, SD_MD_DT, SD_MD_X, SD_MD_Y, SD_MD_WIDTH,
       SD_MD_RPOS, SD_MD_RFREQ, SD_MD_RRES, SD_MD_RDRIVE, SD_MD_RMIX, SD_MD_DGAIN, SD_MD_DBIAS, SD_MD_DFB,
       SD_MD_DTAME, SD_MD_DMIX, SD_MD_SFB, SD_MD_SDLVL, SD_MD_SRLVL, SD_MD_SDECAY, SD_MD_DHIGH, SD_MD_DSPLIT,
       SD_MD_FCUT, SD_MD_FRES,                  /* the filter's CUTOFF (+-60 semitones at 100 %) and RES */
       SD_MDSTS };                              /* (D HIGH: the top band's gain; D SPLIT: the upper crossover) */
#define SD_MODS 8                               /* slots a patch */
typedef struct {
    uint8_t src, dst;                           /* SD_MS_*, SD_MD_* */
    int16_t amt;                                /* -32767..32767 = -100 .. +100 %: at a full source, the
                                                   destination's whole range (PITCH +-24 semitones) */
} SdModSlot;
enum { SD_LS_SIN, SD_LS_TRI, SD_LS_SAW, SD_LS_RAMP, SD_LS_SQR, SD_LS_SH, SD_LSHAPES };   /* SAW falls, RAMP rises */
enum { SD_LM_SYNC = 1, SD_LM_KEY = 2, SD_LM_ONE = 4 };   /* lfo_mode's bits: to the note driver's tempo; restart on
                                                   a note; once a note (with KEY): one cycle, then held at its end, and
                                                   unipolar (0..1): a SAW is a ramp down to the patch's value inside each
                                                   note (the user: "ramp-down curves that modulate within a note") */
#define SD_LFO_DIVS 12                          /* SYNC: a cycle of 1 .. 64 sixteenths */
static const uint8_t SD_LFO_DIV[SD_LFO_DIVS] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
enum { SD_M4_BODY, SD_M4_AIR, SD_M4_DROWN, SD_M4S };   /* MACRO 4's kind (the spec's options A, B and DROWN) */

typedef struct {
    SdBankPatch bank[SD_BANKS];
    uint16_t xm, warp, fold, dt;                /* the Q page: 0..65535 (0..99 on the screen), every bank */
    int16_t e2_warp, e2_fold;                   /* ENV2's depth on WP / FD, -32767..32767 = -1..+1 (a mod slot) */
    uint8_t e1_a, e1_d, e1_s, e1_r;             /* ENV1, the level: times 0..127 = 1 ms .. 10 s, sustain 0..127 */
    uint8_t e2_a, e2_d, e2_s, e2_r;             /* ENV2, the FM amount */
    uint16_t x, y, width;                       /* the nav (0 = left / bottom), the stereo width; 0..65535 */
    uint8_t course;                                /* COURSE on the gate: 0 off, 1 on */
    uint8_t fx_off;                             /* SD_FX_*: engines switched off (EDIT, 2026-11-07), their settings
                                                   kept; 0 in every factory patch */
    uint16_t cell_x[4], cell_y[4];              /* COURSE's four cells */
    uint16_t cell_ms[3];                        /* the times between them, 1..65535 ms */
    /* the Reactor */
    SdReactorStage stage[6];
    int16_t r_pan[4];                           /* each filter, -32767 (left) .. 32767 (right) */
    uint16_t r_pos;                             /* 0 .. 6 x 256 - 1: stage 1 .. 6.999 */
    uint16_t r_res, r_slew, r_drive, r_mix;     /* 0..65535; DRIVE 0..4x (16384 = 1x); MIX 0 = no Reactor */
    int16_t r_freq;                             /* the global offset, semitones x 256 */
    uint8_t r_model, r_track;                   /* SD_RM_*; key tracking on (1) */
    uint8_t r_div, r_cmode, r_min, r_max;       /* the stage clock: steps a stage (0 off), SD_RC_*, the range 0..5 */
    uint16_t r_low;                             /* R4: the lows that skip the Reactor: 0 none, else 50 .. 400 Hz */
    /* DISTROY */
    uint16_t d_split[3];                        /* the crossover, MIDI x 256 (log pitch), rising */
    uint8_t d_type[4];                          /* each band's shaper, SD_DS_* */
    uint16_t d_gain[4], d_alt[4], d_level[4];   /* 0..65535: 0 .. +36 dB; ALT; the level law (32768 = 1x) */
    int16_t d_bias[4], d_pan[4];                /* -32767..32767: -0.5 .. +0.5 full scale; left .. right */
    int16_t d_shape[4];                         /* the dynamics: -32767 PHAT .. 0 none .. 32767 SHARP */
    uint16_t d_body[4];                         /* their threshold, 0..65535: -54 .. -6 dBFS (the band's level) */
    uint16_t d_fb, d_ftime, d_spread;           /* feedback: FB 0 .. 0.9 (0 = off), TIME 1 .. 20 ms, SPREAD (the
                                                   right line up to 30 % shorter) */
    uint16_t d_tame;                            /* TAME: 0 off, then a lowpass 15 kHz .. 1.2 kHz, resonant at the top */
    uint16_t d_mix;                             /* 0 = no DISTROY */
    uint8_t d_xover, tur_dst;                   /* SD_DX_CMP / SD_DX_LR4; TURING's DEST (below) */
    /* SPACE */
    uint16_t s_time, s_fb, s_dtone, s_dlevel;   /* 0..65535: TIME (free 1..1000 ms, or a division), FDBK, TONE, LEVEL */
    uint16_t s_size, s_decay, s_rtone, s_rlevel;
    uint8_t s_mode;                             /* SD_SP_SYNC | SD_SP_PP */
    int8_t tur_amt;                             /* TURING's AMOUNT, -100 .. +100 % */
    /* C1: the modulation. All zero: none (every patch before C1 plays as it did) */
    SdModSlot mod[SD_MODS];
    uint8_t lfo_shape[2], lfo_mode[2];          /* SD_LS_*; SD_LM_* */
    uint16_t lfo_rate[2];                       /* free: 0.02 .. 40 Hz (SD_LFO_INC); SYNC: a division, 0..65535 */
    uint16_t mac[4];                            /* MUTATE MOVE BREAK MACRO 4, 0..65535: 0 = the patch as programmed */
    uint8_t mac4, tur_set;                      /* MACRO 4's kind, SD_M4_*; TURING: bit 7 on, bits 0..6 CHANGE % */
    /* the filter (the user, 2026-11-06: "I miss a lowpass filter"): a lowpass on each voice, on its banks' carriers
       before the nav's balance; ENV3 on its cutoff. All zero: off (every patch before it plays as it did) */
    uint8_t f_type, f_key;                      /* SD_FT_*; KEY 0..100 % (100: the cutoff follows the note) */
    uint16_t f_cut, f_res;                      /* 0..65535: CUTOFF MIDI 16 .. 130 (21 Hz .. 15 kHz); RES */
    int16_t f_env;                              /* ENV3's depth, -32767..32767 = -/+ 60 semitones */
    uint8_t f_vel, tur_ld;                      /* VEL: 0..100 % = up to +24 semitones at velocity 127; TURING's
                                                   LENGTH (bits 0..4: 2..16, 0 reads as 8) and DIV (bits 5..7: the
                                                   SD_SDIV index + 1, 0 reads as 1/16) */
    uint8_t e3_a, e3_d, e3_s, e3_r;             /* ENV3, the filter's: as ENV1 */
} SdPatch;
/* TURING (the user, 2026-11-09: "it should work more like a modulation source, to parameters in the synth, the
   filter"): a looping shift register on the tempo, its four fields in the patch's four spare bytes (so a patch saved
   before reads as TURING off). DEST is a matrix destination (SD_MD_*), or NOTES: SEQ A's notes moved in its scale as
   they play, the pattern itself never rewritten. 0: none */
#define SD_TD_NOTES 0x80u
#define SD_TUR_ON(p) ((p)->tur_set >> 7)
#define SD_TUR_CHANGE(p) ((uint32_t)((p)->tur_set & 127u) > 100u ? 100u : (uint32_t)((p)->tur_set & 127u))
#define SD_TUR_LEN(p) (((p)->tur_ld & 31u) ? ((p)->tur_ld & 31u) < 2u ? 2u : ((p)->tur_ld & 31u) > 16u ? 16u : (uint32_t)((p)->tur_ld & 31u) : 8u)
#define SD_TUR_DIV(p) (((p)->tur_ld >> 5) ? (uint32_t)((p)->tur_ld >> 5) - 1u > SD_SDIVS - 1u ? SD_SDIVS - 1u : (uint32_t)((p)->tur_ld >> 5) - 1u : 2u)
enum { SD_FT_OFF, SD_FT_LP12, SD_FT_LP24, SD_FTYPES };
enum { SD_AM_UP, SD_AM_DOWN, SD_AM_UPDN, SD_AM_RND, SD_AM_ORDER, SD_AMODES };   /* the arpeggiator's orders */

/* ---- envelopes: exponential segments, a coefficient per control tick */
enum { SD_ENV_OFF, SD_ENV_ATTACK, SD_ENV_DECAY, SD_ENV_RELEASE };
typedef struct {
    int32_t lvl;                                /* 0..SD_ONE */
    uint8_t stage;
} SdEnv;

static int32_t sd_toward(int32_t lvl, int32_t target, uint32_t c)   /* |lvl - target| < 2^31: Q31 coefficient */
{
    return target + (int32_t)(((int64_t)(lvl - target) * (int64_t)c) >> 31);
}

static void sd_env_tick(SdEnv *v, uint32_t gate, uint32_t a, uint32_t d, uint32_t s, uint32_t r)
{
    if (!gate && (v->stage == SD_ENV_ATTACK || v->stage == SD_ENV_DECAY)) v->stage = SD_ENV_RELEASE;
    switch (v->stage) {
    case SD_ENV_ATTACK:                         /* toward 1.25, cut at 1.0: the analog curve, done in the time */
        v->lvl = sd_toward(v->lvl, SD_ONE + SD_ONE / 4, SD_ENV_ATK[a & 127u]);
        if (v->lvl >= SD_ONE) { v->lvl = SD_ONE; v->stage = SD_ENV_DECAY; }
        break;
    case SD_ENV_DECAY:
        v->lvl = sd_toward(v->lvl, (int32_t)((s & 127u) * (SD_ONE / 127)), SD_ENV_DEC[d & 127u]);
        break;
    case SD_ENV_RELEASE:
        v->lvl = sd_toward(v->lvl, 0, SD_ENV_DEC[r & 127u]);
        if (v->lvl < (SD_ONE >> 14)) { v->lvl = 0; v->stage = SD_ENV_OFF; }   /* -84 dB: below one step of level */
        break;
    default:
        v->lvl = 0;
        break;
    }
}

/* ---- state */
typedef struct {
    uint32_t ph, inc;
    int32_t lvl, lvl_next;                      /* level, Q15 <= 32767, ramped across the sub-block */
    uint8_t tgt;                                /* SD_CARRIER, SD_NOWHERE or an oscillator to the right */
} SdOsc;

typedef struct {                                /* a bank's waveform for this sub-block: one for its four oscillators */
    uint32_t t;                                 /* the mix toward the triangle, Q15 */
    uint32_t d, k1, k2;                         /* skew: breakpoint (of 2^32), slopes before / after it (Q16) */
    uint32_t sq_g, sq_norm;                     /* squash: drive G (Q8), 1 / cubic(G) (Q7) */
    int32_t fg, fg_next;                        /* fold gain (Q12), ramped across the sub-block */
    uint8_t shape, sq, fold;                    /* the kernel's variant: shape SD_SH_* */
    uint32_t last_w, last_dmin;                 /* the inputs the warp's part was last computed from */
    int32_t last_tr;                            /* (-1: none yet) */
} SdShape;
enum { SD_SH_SINE, SD_SH_BLEND, SD_SH_TRI };    /* warp 0; sine -> triangle; triangle -> saw (and on, squashed) */

/* Who plays a voice (2026-11-07, the user: "notes get cancelled, especially with ARP: we need rules for what can
 * play together"): the hand (the keys and MIDI), SEQ A, SEQ B, the arpeggiator. Each has a quota of voices held at
 * once (sd_plan: the tracks first, then ARP, the hand the rest); a note off turns off only its own source's notes,
 * and no source takes a held voice from another: past its quota it steals its own oldest, without one it is silent. */
enum { SD_SRC_HAND, SD_SRC_A, SD_SRC_B, SD_SRC_ARP, SD_SRCS };

typedef struct {
    uint32_t inc;                               /* the note's (gliding toward inc_tgt while glide) */
    uint32_t inc_tgt;                           /* SLIDE: where the glide goes */
    uint8_t note, vel, gate, glide;
    uint8_t src;                                /* who played it: SD_SRC_* (only it turns it off) */
    SdEnv env1, env2, env3;
    int32_t fa1[2], fa2[2], fa3[2];             /* the filter's two stages: coefficients (Q30), this sub-block */
    int32_t amp, amp_next;                      /* Q15 level at this sub-block's start, at the next one's */
    uint32_t age;                               /* when it last started: the allocator steals the oldest */
} SdVoice;

typedef struct {
    SdOsc osc[SD_OPS];
    SdShape sh;
    int32_t gl, gl_next, gr, gr_next;           /* to the outputs: voice level x nav weight x balance, Q15 */
    uint8_t voice, src;                         /* the voice that plays it; the bank patch it plays */
    int32_t bus[SD_OPS][SD_SUB];                /* PM into each oscillator, this sub-block, Q15 */
    int32_t mix[SD_SUB];                        /* the carriers' sum, Q15 (up to 4 x full scale) */
    int32_t fic1[2], fic2[2];                   /* the filter's states (its two stages), the mix x 8 */
} SdBank;

typedef struct {                                /* the Reactor's state */
    int32_t pitch[4], level[4];                 /* where each filter is (slewed): MIDI x 256 << 8, level << 8 */
    int32_t ic1[2][4], ic2[2][4];               /* each filter's two integrators, two stages (BP12), Q19 */
    int32_t a1[4], a2[4], a3[4], kn[4];         /* coefficients (Q30); kn = k (Q24): BP12's and VBP's scale */
    int32_t lvl[4], pl[4], pr[4];               /* level x make-up (Q14, <= 8), pan gains (Q15) */
    int32_t drive, mix;                         /* Q14, Q15 */
    uint8_t model, on, stage, dir, count, note;
    uint8_t rest;
    int32_t dg, wg, lowc, lowz[2];              /* R4: MIX's dry and wet gains (Q15); the low keep's one-pole */                               /* the voice silent and the filters rung out: no work, no output */
    /* the delay models (KP, AP1): four lines, each read d samples back (Q8), d ramped across the sub-block */
    int16_t dl[4][SD_DL];                       /* Q13: 4 x full scale */
    uint32_t wp;
    int32_t dq[4], dnext[4], prev[4];
    int32_t fbk, dry, aplvl;                    /* KP's feedback (Q15); the AP modes' dry (Q15) and level (Q14) */
    int32_t apa[4], ax1[8], ay1[8];             /* AP2: each filter's coefficient (Q30), eight stages' states (Q15) */
    int32_t peak;                               /* the largest |wet| this sub-block (the delay models' rest) */
    uint32_t quiet;                             /* sub-blocks in a row the voice was silent and the wet under 4 */
    uint32_t rnd;
    volatile uint16_t shown_pos;                /* where it is, x 256: for the screen */
} SdReactor;

typedef struct {                                /* DISTROY's state */
    int32_t ic1[2][11], ic2[2][11];             /* the crossover's SVFs, a channel, Q19 (CMP uses 3, LR4 11) */
    int32_t a1[3], a2[3], a3[3];                /* their coefficients, Q30 */
    int32_t dx[2], dy[2];                       /* the DC blocker, a channel, after the bands' sum: its output x 256 */
    int32_t gain[4], bias[4], lvl[4], pl[4], pr[4];   /* Q8; Q15; Q14; Q15 */
    int32_t g2[2][4];                           /* level x pan, a channel and band (Q14) */
    int32_t bb[4][2][SD_SUB];                   /* the bands, this call's frames, Q19 */
    int32_t w[2][SD_SUB];                       /* their sum, Q15 */
    int32_t t[4], s[4], inv[4], tn[4], sn[4], invn[4];  /* the shapers' knees (SOFT, HARD): positive, negative */
    int32_t fold[4], crush[4];                  /* FOLD's 1 / t (Q12); CRUSH's step */
    int32_t pk[4];                              /* the dynamics: each band's peak since the last tick (Q19) */
    int32_t ef[4], es[4];                       /* its fast and slow envelopes, dB x 256 */
    int32_t g0[4], gs[4];                       /* the gain before the shaper (GAIN x the dynamics), Q12: at the
                                                   sub-block's start, and its step a frame */
    int32_t dyn[4];                             /* the dynamics' gain now, dB x 256 (for the screen) */
    /* D3: the feedback (§5.3), TAME (§5.4), the safety limiter */
    int16_t fl[2][SD_DFLEN];                    /* the wet out, Q14 (2 x full scale) */
    uint32_t fw;                                /* the line's write position */
    int32_t fd[2], ftgt[2];                     /* the delays now (moving a sample a tick) and their targets */
    int32_t flp[2], fb, fT, fS, finv;           /* the loop's lowpass; FB (Q15); its soft clip's knee */
    int32_t ta1, ta2, ta3, tic1[2], tic2[2];    /* TAME's SVF: coefficients (Q30), states */
    int32_t lg, lpk, fpk;                       /* the limiter's gain (Q15); the peak before it; the line's peak */
    uint8_t tame;                               /* TAME on */
    uint8_t type[4], on, rest, xover;           /* rest: nothing in, the blockers settled: no work, no output */
    int32_t mix;
} SdDistroy;

typedef struct {                                /* SPACE's memory: the app's (the pool) */
    int16_t dl[2][SD_SP_DMAX];                  /* the delay, a channel, Q14 (2 x full scale) */
    int16_t fdn[4][SD_SP_FLEN];                 /* the reverb's lines, Q14 */
    int16_t ap[4][SD_SP_APLEN];                 /* its diffusers, Q14 */
} SdSpaceMem;

typedef struct {                                /* SPACE's state */
    uint32_t dw, fw, apw[4];                    /* write positions */
    int32_t dq, dnext, dtgt;                    /* the delay, samples x 256: now (this sub-block's start), next, target */
    int32_t lpd[2], flp[4];                     /* the tone lowpasses: the delay's, each line's */
    int32_t flen[4], ftgt[4];                   /* the lines' lengths, moving a sample a sub-block toward their target */
    int32_t fb, dtone, g, rtone, dlvl, rlvl;    /* Q15 */
    uint8_t on, rest, pp, pad;
    int32_t peak;
    uint32_t quiet;
    uint32_t ph;                                /* the reverb's half rate: 0 gathers, 1 runs */
    int32_t racc, a0[2], a1[2];                 /* the gathered input; the taps before and now (L R) */
    int32_t dcz[2];                             /* the input's DC, Q8 (~7 Hz): kept out of the echoes and the room */
} SdSpace;

typedef struct {
    uint8_t note, vel;                          /* vel 0 = note off */
} SdNoteEv;

typedef struct {                                /* a sequencer track: its pattern, handed over as the patch is, and
                                                   where it plays */
    volatile uint32_t pat_seq;                  /* odd while the main loop writes pat_in */
    SdPattern pat_in;                           /* written by the main loop (sd_engine_pattern) */
    SdPattern pat;                              /* the audio's copy */
    uint32_t pat_taken;
    int8_t tur_deg;                             /* TURING's NOTES on the step playing: degrees (A only) */
    int8_t orb_x;                               /* ORBIT's transpose of the track, semitones (with xpose) */
    uint8_t seq_on, seq_step, seq_note_on, seq_nn;   /* running; the step playing; notes held; how many */
    uint8_t seq_note[4];                        /* the notes it holds: the step's, then its chord's */
    uint32_t seq_pos_q8, seq_gate_q8;           /* samples x 256 into the step; the step's gate */
    uint32_t seq_step_q8;                       /* this step's length (DIV, SWING) */
    uint32_t seq_count;                         /* steps played since START (SWING's pairs) */
    uint32_t rat_q8;                            /* RATCHET: a hit's length (samples x 256); 0 none */
    uint8_t rat_n, rat_i, rat_vel;              /* the hits, the one playing, its velocity */
    uint8_t sq_seg, sq_rep, sq_pos, sq_cur;     /* the segment, its pass, the place in its order, which buffer */
    uint8_t sq_n[2], sq_nseg, sq_nrep;          /* the passes' lengths; the next pass's segment and repeat */
    uint8_t sq_ord[2][SD_PSTEPS * 2u];          /* this pass's steps in order, and the next one's (P-P: ~2 x) */
    uint32_t sq_rng;
    volatile uint8_t mute;                      /* the main loop's ON / OFF (EDIT): the track keeps time, silent */
    uint8_t src;                                /* SD_SRC_A / SD_SRC_B */
    volatile uint8_t width;                     /* the most notes a step of its pattern plays at once (0: none) */
    volatile int8_t xpose_next;                 /* the main loop's transpose (SCL + a key), semitones: taken at the
                                                   track's next one (or START) */
    int8_t xpose;                               /* the transpose playing */
    uint8_t quiet;                              /* silent this sub-block: muted, or B in MONO */
    volatile uint8_t seq_shown;                 /* the step that last started, for the screen (0..63) */
    volatile uint8_t seq_seg_shown;             /* its segment */
} SdSeqT;

typedef struct {
    /* main loop -> audio: one writer each side */
    SdNoteEv q[SD_QLEN];
    volatile uint32_t q_w;                      /* written by the main loop */
    uint32_t q_r;                               /* read by the audio */
    volatile uint8_t mode;                      /* SD_MODE_*: taken once every voice is silent */
    volatile uint8_t track;                     /* the aliasing tracking on (1, default) or off (0, for tests) */
    volatile uint16_t bpm;                      /* 40..240 */
    volatile uint32_t patch_seq;                /* odd while the main loop writes patch_in */
    SdPatch patch_in;                           /* written by the main loop (sd_engine_patch) */
    SdPatch base;                               /* the audio's copy, taken when patch_seq is even */
    SdPatch patch;                              /* what plays: base + the modulation, each tick (C1) */
    uint32_t seq_taken;                         /* the patch_seq base was taken at: a new one, a new copy */
    uint8_t m_dirty;                            /* the modulation wrote patch last tick: its fields back first */

    /* C1: the modulation's sources and state */
    volatile uint8_t wheel, after;              /* MIDI CC 1, channel pressure: 0..127 (the main loop writes) */
    uint8_t m_vel, m_note, m_last, m_trig;      /* the last note: velocity, key, its voice; a note came (KEY) */
    int32_t m_rand;                             /* RAND: drawn at each note, Q15 */
    uint32_t m_seed;
    uint32_t lfo_ph[2];                         /* 2^32 a cycle */
    int32_t lfo_sh[2];                          /* S&H's held value */
    volatile int16_t lfo_out[2];                /* the LFOs now, Q15 (for the screen) */
    int32_t lfo_shg[2];                         /* S&H's output gliding to the held value (~10 ms) */
    uint32_t move_t;                            /* MOVE: samples x 256 into the beat */
    int32_t move_tgt[3], move_cur[3];           /* its X, Y, POS: drawn on each beat, glided to (Q15) */
    uint32_t pmul;                              /* PITCH: the note's increment x pmul, Q16 */
    int32_t mvx, mvy;                           /* the nav's offset (the matrix's X / Y, MOVE's orbit), Q16 */
    int32_t echo_env;                           /* ECHO: SPACE's wet level, followed (~60 ms release), Q15 */
    /* ORBIT (sd_seq.h): handed over as a pattern is (orb_in, odd orb_seq while the main loop writes it); the step,
       the loops of A in it, the sub-blocks into it and in A's last loop (the glide's span) */
    volatile uint32_t orb_seq;
    SdOrbit orb_in, orb;
    uint32_t orb_taken;
    uint8_t orb_step, orb_loops;
    volatile uint8_t orb_shown;                 /* the step playing (the screen) */
    uint32_t orb_t, orb_loop_t, orb_loop_sb;
    /* TURING: the register (bit 0 the newest), its clock, its output and where it glides (Q15) */
    volatile uint16_t tur_reg;
    uint32_t tur_pos_q8, tur_rng;
    int32_t tur_out, tur_g;
    volatile uint32_t tur_count;                /* its steps so far (the screen draws its history from them) */

    uint8_t mode_on, nvoices;
    uint32_t ages;
    SdVoice voice[SD_VOICES];
    volatile uint8_t quota[SD_SRCS];            /* sd_plan: the voices each source may hold at once (the screen reads
                                                   it) */
    volatile uint8_t rec_trk;                   /* REC (the main loop): the track it writes, 0 / 1, else 0xFF: the hand
                                                   may then take that track's voice (you hear what you record) */
    SdBank bank[SD_BANKS];

    /* the nav: COURSE's playback, and where it is (for the screen) */
    uint8_t course_seg, course_run;
    uint32_t course_ph, course_step;                  /* Q20 through the current segment */
    volatile uint16_t nav_x, nav_y;
    uint32_t w_x, w_y, w_mode;                  /* the X / Y / mode the weights below were computed for */
    uint8_t w_ok;
    int32_t w[SD_BANKS];

    /* the sequencer (sd_seq.h): two tracks on the one sound (the user, 2026-11-06: "a melody and a bass line with
       the same patch, at the same time"). A always; B with seqb. One tempo, one transport (seq_run) */
    volatile uint8_t seq_run;                   /* the main loop sets it; the audio starts and stops on it */
    volatile uint8_t seqb;                      /* track B on (the main loop sets it) */
    uint32_t seq_len_q8;                        /* a 16th, samples x 256 */
    SdSeqT t[2];

    /* the arpeggiator (the user, 2026-11-06: "ARP does not work"): the keys' notes (and MIDI's), played one at a
       time on the tempo. The main loop sets these; the audio keeps the held notes and plays them */
    volatile uint8_t arp_on, arp_mode, arp_div, arp_oct, arp_gate, arp_hold;   /* SD_AM_*; SD_SDIV index; 1..4
                                                   octaves; 1..100 %; HOLD: the notes stay after the keys lift */
    uint8_t arp_n, arp_keys, arp_was;           /* notes in the list; keys down now; arp_on last tick */
    uint8_t arp_note[8], arp_vel[8];            /* the notes, in the order played */
    /* the hand's keys held now, the latest last (the user, 2026-11-06: in MONO, a second key let go must go back to
       the first, still held, not cut it): sd_hand_key keeps it, sd_hand_off gives a voice back to it */
    uint8_t hand_note[SD_HELD], hand_vel[SD_HELD], hand_n;
    uint8_t arp_i, arp_dir, arp_playing, arp_cur;   /* the place in the run; UPDN's way; a note sounds; which */
    uint32_t arp_pos_q8, arp_step_q8;           /* samples x 256 into the step; its length */
    uint32_t arp_rng;

    uint32_t sub_left;                          /* frames left in the current sub-block */
    uint32_t frames;                            /* frames rendered since start (wraps) */
    /* the output's limiter: the line (64 frames, L R), its gain (Q15, the sub-block's ramp), the peaks in (the last
       block's, this one's so far) */
    int32_t lim_buf[2u * SD_LIM_D];
    int32_t lim_g, lim_step, lim_end;
    int32_t lim_pk1, lim_acc;
    SdReactor r;
    int32_t vbuf[2u * SD_SUB];                  /* the voice, before the Reactor */
    SdDistroy d;
    int32_t dbuf[2u * SD_SUB];                  /* the voice and the Reactor, before DISTROY */
    SdSpace sp;
    SdSpaceMem *sm;                             /* SPACE's memory (0: no SPACE); set after sd_engine_init */
    int32_t sbuf[2u * SD_SUB];                  /* everything, before SPACE */
} SdEngine;

#include "sd_patches.h"

/* The loop: semitones over the root, -1 a rest. A test line, not a pattern to keep. */

static void sd_seq_retime(SdEngine *e)          /* the step's length from the tempo, in samples x 256 (32-bit only) */
{
    uint32_t bpm = e->bpm < 40u ? 40u : e->bpm > 240u ? 240u : e->bpm;
    e->seq_len_q8 = (SD_FS * 60u * 256u) / (bpm * 4u);          /* 677,376,000 / (bpm*4): fits in 32 bits */
}

/* main loop: a whole patch. The audio takes it at its next control tick; it never sees half of one (it keeps the
 * last whole one while patch_seq is odd, and the main loop never interrupts the audio). */
static void sd_engine_pattern_to(SdEngine *e, uint32_t k, const SdPattern *p)   /* the main loop hands track k
                                                   (0 A, 1 B) a pattern */
{
    SdSeqT *t = &e->t[k & 1u];
    t->pat_seq++;
    SD_BARRIER();
    t->pat_in = *p;
    SD_BARRIER();
    t->pat_seq++;
}
static void sd_engine_pattern(SdEngine *e, const SdPattern *p) { sd_engine_pattern_to(e, 0, p); }
static void sd_engine_orbit(SdEngine *e, const SdOrbit *o)   /* the main loop hands over ORBIT */
{
    e->orb_seq++;
    SD_BARRIER();
    e->orb_in = *o;
    SD_BARRIER();
    e->orb_seq++;
}

static void sd_engine_patch(SdEngine *e, const SdPatch *p)
{
    e->patch_seq++;
    SD_BARRIER();
    e->patch_in = *p;
    SD_BARRIER();
    e->patch_seq++;
}

static void sd_map_banks(SdEngine *e)           /* which voice plays which bank, which bank patch it plays */
{
    uint32_t b;
    e->nvoices = e->mode_on == SD_MODE_POLY ? 4u : e->mode_on == SD_MODE_DUO ? 2u : 1u;
    for (b = 0; b < SD_BANKS; b++) {
        e->bank[b].voice = (uint8_t)(e->mode_on == SD_MODE_POLY ? b : e->mode_on == SD_MODE_DUO ? b >> 1 : 0u);
        e->bank[b].src = (uint8_t)(e->mode_on == SD_MODE_POLY ? 0u : e->mode_on == SD_MODE_DUO ? b & 1u : b);
    }
}

/* TURING's DAC: the register's 8 newest bits, the newest the top one (the last coin weighs most), as -1 .. +1 */
static int32_t sd_tur_dac(uint32_t reg)
{
    uint32_t v = 0, k;
    for (k = 0; k < 8u; k++) v |= ((reg >> k) & 1u) << (7u - k);
    {
        int32_t o = (int32_t)(v * 257u) - 32767;   /* 0 .. 255: -32767 .. +32768 */
        return o > 32767 ? 32767 : o;
    }
}

static void sd_engine_init(SdEngine *e)
{
    uint8_t *p = (uint8_t *)e;
    uint32_t i;
    for (i = 0; i < sizeof *e; i++) p[i] = 0;
    for (i = 0; i < SD_BANKS; i++) e->bank[i].sh.last_tr = -1;
    e->bpm = 120;
    e->arp_div = 2; e->arp_oct = 1; e->arp_gate = 50; e->arp_rng = 0xA7Fu;
    e->track = 1;
    e->patch_in = e->base = e->patch = SD_PATCH_SINE;
    for (i = 0; i < 2u; i++) {
        if (i) sd_pattern_empty(&e->t[i].pat_in);   /* B: silent until it has notes */
        else sd_pattern_default(&e->t[i].pat_in);
        e->t[i].pat = e->t[i].pat_in;
        e->t[i].sq_rng = 0x5E9u + 0x1F3u * i;
        e->t[i].src = (uint8_t)(SD_SRC_A + i);
        e->t[i].width = (uint8_t)sd_pattern_width(&e->t[i].pat);
    }
    e->rec_trk = 0xFF;
    e->pmul = 65536;
    e->m_seed = 0x5EED1u;
    e->lim_g = e->lim_end = 32768;              /* the limiter open */
    e->tur_reg = 0x9A5Cu;                       /* TURING's first loop: any bits, as at power-on */
    e->tur_rng = 0x7E57u;
    e->tur_out = sd_tur_dac(e->tur_reg);
    for (i = 0; i < SD_BANKS; i++) e->bank[i].sh.fg = e->bank[i].sh.fg_next = 4096;
    sd_map_banks(e);
    sd_seq_retime(e);
}

static int sd_voice_silent(const SdVoice *v) { return v->env1.stage == SD_ENV_OFF && !v->amp && !v->amp_next; }
static int sd_engine_silent(const SdEngine *e)
{
    uint32_t i;
    for (i = 0; i < SD_VOICES; i++)
        if (!sd_voice_silent(&e->voice[i])) return 0;
    return 1;
}

/* main loop: a note on (vel 1..127) or off (vel 0). Dropped when the queue is full. */
static void sd_engine_note(SdEngine *e, uint32_t note, uint32_t vel)
{
    uint32_t w = e->q_w;
    if (w - e->q_r >= SD_QLEN || note > 127u) return;
    e->q[w % SD_QLEN].note = (uint8_t)note;
    e->q[w % SD_QLEN].vel = (uint8_t)(vel > 127u ? 127u : vel);
    e->q_w = w + 1u;                            /* published last */
}

static void sd_course_start(SdEngine *e)           /* COURSE from its first cell (on the gate, MONO only) */
{
    uint32_t ms = e->patch.cell_ms[0] ? e->patch.cell_ms[0] : 1u;
    e->course_seg = 0;
    e->course_ph = 0;
    e->course_step = 335544320u / (ms * 441u);     /* 32 frames x 2^20 / (ms x 44.1): Q20 a tick */
    e->course_run = 1;
}

static uint32_t sd_voice_on(SdEngine *e, uint32_t note, uint32_t vel, uint32_t src)   /* 1: it sounds */
{
    uint32_t i, pick = 0xFFu, held = 0, best = 0xFFFFFFFFu, rank, q = e->quota[src % SD_SRCS];
    SdVoice *v;
    for (i = 0; i < e->nvoices; i++) held += e->voice[i].gate && e->voice[i].src == src;
    for (i = 0; i < e->nvoices; i++) {          /* its own note again; else, under its quota, the oldest silent, then
                                                   released; at its quota, its own oldest held. Never another's held */
        SdVoice *c = &e->voice[i];
        if (c->gate && c->src == src && c->note == note) rank = 0u;
        else if (c->gate && c->src != src) {
            if (!(src == SD_SRC_HAND && e->rec_trk < 2u && c->src == SD_SRC_A + e->rec_trk && !q)) continue;
            rank = 3u;                          /* (REC with no voice of its own: the recorded track's) */
        } else if (c->gate) { if (held < q) continue; rank = 3u; }
        else if (held >= q && !(src == SD_SRC_HAND && e->rec_trk < 2u && !q)) continue;
        else rank = sd_voice_silent(c) ? 1u : 2u;
        rank = rank << 28 | (c->age & 0x0FFFFFFFu);
        if (rank < best) { best = rank; pick = i; }
    }
    if (pick == 0xFFu) return 0;                /* no voice for it: silent */
    v = &e->voice[pick];
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->src = (uint8_t)src;
    v->inc = v->inc_tgt = sd_note_inc(note);
    v->glide = 0;
    v->gate = 1;
    v->age = ++e->ages;
    e->r.note = (uint8_t)note;                  /* the Reactor's key tracking follows the last note */
    e->m_vel = (uint8_t)vel;                    /* C1: the matrix's VEL, KEY, ENV (this voice's), RAND */
    e->m_note = (uint8_t)note;
    e->m_last = (uint8_t)pick;
    e->m_trig = 1;
    e->m_seed = e->m_seed * 1103515245u + 12345u;
    e->m_rand = (int32_t)(e->m_seed >> 16) - 32768;
    v->env1.stage = SD_ENV_ATTACK;              /* from where the level is: a retrigger never clicks */
    v->env2.stage = SD_ENV_ATTACK;
    v->env3.stage = SD_ENV_ATTACK;
    if (e->mode_on == SD_MODE_MONO && e->patch.course) sd_course_start(e);
    return 1;
}
static void sd_voice_off(SdEngine *e, uint32_t note, uint32_t src)   /* src's notes only */
{
    uint32_t i;
    for (i = 0; i < e->nvoices; i++)
        if (e->voice[i].note == note && e->voice[i].src == src) e->voice[i].gate = 0;
}

/* The hand's held keys, the latest last: a key down goes on top (again: moved there), a key up leaves. Kept with
 * ARP on as well, so the list is right when it goes off. */
static void sd_hand_key(SdEngine *e, uint32_t note, uint32_t vel)
{
    uint32_t i, j;
    for (i = 0; i < e->hand_n; i++)
        if (e->hand_note[i] == note) {
            for (j = i; j + 1u < e->hand_n; j++) { e->hand_note[j] = e->hand_note[j + 1u]; e->hand_vel[j] = e->hand_vel[j + 1u]; }
            e->hand_n--;
            break;
        }
    if (!vel) return;
    if (e->hand_n == SD_HELD) {                 /* full: the oldest goes */
        for (j = 0; j + 1u < SD_HELD; j++) { e->hand_note[j] = e->hand_note[j + 1u]; e->hand_vel[j] = e->hand_vel[j + 1u]; }
        e->hand_n--;
    }
    e->hand_note[e->hand_n] = (uint8_t)note;
    e->hand_vel[e->hand_n] = (uint8_t)vel;
    e->hand_n++;
}
static int sd_hand_sounds(const SdEngine *e, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < e->nvoices; i++)
        if (e->voice[i].gate && e->voice[i].src == SD_SRC_HAND && e->voice[i].note == note) return 1;
    return 0;
}
/* A key up (after sd_hand_key took it off the list): its voice goes back to the latest key still held that has
 * none (it lost it to a later key: in MONO always), legato: the pitch jumps there, the envelope goes on, COURSE is not
 * restarted, as a mono synth goes back to the key still down. With none such, the voice is let go. */
static void sd_hand_off(SdEngine *e, uint32_t note)
{
    uint32_t i, k;
    for (i = 0; i < e->nvoices; i++) {
        SdVoice *v = &e->voice[i];
        if (v->note != note || v->src != SD_SRC_HAND) continue;
        if (v->gate) {
            for (k = e->hand_n; k-- > 0u;)
                if (!sd_hand_sounds(e, e->hand_note[k])) break;
            if (k < e->hand_n) {                /* (none: k wrapped past 0) */
                v->note = e->hand_note[k];
                v->vel = e->hand_vel[k];
                v->inc = v->inc_tgt = sd_note_inc(v->note);
                v->glide = 0;
                e->r.note = v->note;            /* the Reactor's key tracking and the matrix's KEY follow */
                e->m_note = v->note;
                continue;
            }
        }
        v->gate = 0;
    }
}

/* The plan: each source's quota from the mode's voices. A playing track holds its widest step (its chords), A first;
 * ARP one; the hand the rest. Stopped, the tracks hold none. B never in MONO. The main loop keeps what is switched on
 * inside it (the user: "no trying to play more than is possible"), so a quota cut here is only a chord's top notes. */
static void sd_plan(SdEngine *e)
{
    uint32_t n = e->nvoices, a, b, r, run = e->seq_run;
    a = run && !e->t[0].mute ? e->t[0].width : 0u;
    a = a < n ? a : n;
    b = run && e->seqb && !e->t[1].mute && e->mode_on != SD_MODE_MONO ? e->t[1].width : 0u;
    b = b < n - a ? b : n - a;
    r = e->arp_on && n - a - b ? 1u : 0u;
    e->quota[SD_SRC_A] = (uint8_t)a;
    e->quota[SD_SRC_B] = (uint8_t)b;
    e->quota[SD_SRC_ARP] = (uint8_t)r;
    e->quota[SD_SRC_HAND] = (uint8_t)(n - a - b - r);
}

/* ---- the Reactor: control */
static void sd_reactor_clock(SdEngine *e)         /* a step of the note driver: the stage clock's advance */
{
    const SdPatch *p = &e->patch;
    SdReactor *r = &e->r;
    uint32_t lo = p->r_min > 5u ? 5u : p->r_min, hi = p->r_max > 5u ? 5u : p->r_max, st;
    if (!p->r_div || ++r->count < p->r_div) return;
    r->count = 0;
    if (hi < lo) { st = lo; lo = hi; hi = st; }
    st = r->stage < lo || r->stage > hi ? lo : r->stage;
    switch (p->r_cmode) {
    case SD_RC_BWD: st = st == lo ? hi : st - 1u; break;
    case SD_RC_PP:
        if (lo == hi) break;
        if (r->dir) { if (st == lo) { r->dir = 0; st++; } else st--; }
        else { if (st == hi) { r->dir = 1; st--; } else st++; }
        break;
    case SD_RC_RND:
        r->rnd = r->rnd * 1103515245u + 12345u;
        st = lo + (r->rnd >> 16) % (hi - lo + 1u);
        break;
    default: st = st == hi ? lo : st + 1u; break;
    }
    r->stage = (uint8_t)st;
}

/* 1 / D for D in Q24 (1 .. ~60), in Q30: a 32-bit division to 15 bits, one Newton step to ~28 */
static int32_t sd_recip30(int32_t d)
{
    int32_t x = (int32_t)(0x80000000u / ((uint32_t)d >> 8)) << 15;   /* 2^15 / D, in Q30 */
    int32_t e = (int32_t)(((int64_t)d * x) >> 24);                   /* D x, Q30 */
    return (int32_t)(((int64_t)x * ((int64_t)(1 << 30) * 2 - e)) >> 30);
}

static void sd_reactor_params(SdEngine *e)        /* once a sub-block: position, slew, coefficients, gains */
{
    const SdPatch *p = &e->patch;
    SdReactor *r = &e->r;
    uint32_t pos = ((uint32_t)p->r_pos + (p->r_div ? (uint32_t)r->stage << 8 : 0u)) % 1536u, i, f, j, k;
    uint32_t slew = p->r_slew >> 9, c = slew ? SD_ENV_DEC[slew] : 0u;   /* 0: no glide */
    int32_t off = p->r_freq + (p->r_track & SD_RT_KEY ? ((int32_t)r->note - 60) * 256 : 0);
    int32_t kk = (int32_t)SD_RK[p->r_res >> 9] + ((((int32_t)SD_RK[(p->r_res >> 9) + 1u] -
                 (int32_t)SD_RK[p->r_res >> 9]) * (int32_t)(p->r_res & 511u)) >> 9);
    if (!slew) pos &= ~255u;                    /* no slew: stage to stage */
    r->shown_pos = (uint16_t)pos;
    i = pos >> 8; f = pos & 255u; j = i == 5u ? 0u : i + 1u;   /* stage 6 glides on to stage 1 */
    r->model = p->r_model < SD_RMODELS ? p->r_model : SD_RM_BP6;
    r->drive = p->r_drive;                      /* Q14 as it is: 16384 = 1x, 65535 = 4x */
    r->mix = p->r_mix >> 1;
    /* R4: MIX fills. Up to the middle the dry stays whole and the wet comes in on top; past it the dry fades. The
     * lows under r_low skip the Reactor: what the dry loses of them comes back from a one-pole */
    {                                           /* (SD_RMIX: the loudness held, -3 dB each at the middle) */
        uint32_t mi = p->r_mix >> 9, mf = p->r_mix & 511u;
        r->dg = (int32_t)SD_RMIX[mi][0] + ((((int32_t)SD_RMIX[mi + 1u][0] - (int32_t)SD_RMIX[mi][0]) * (int32_t)mf) >> 9);
        r->wg = (int32_t)SD_RMIX[mi][1] + ((((int32_t)SD_RMIX[mi + 1u][1] - (int32_t)SD_RMIX[mi][1]) * (int32_t)mf) >> 9);
    }
    r->lowc = p->r_low ? (int32_t)((50u + ((p->r_low * 350u) >> 16)) * 4669u >> 10) : 0;   /* 2 pi f / fs, Q15 */
    for (k = 0; k < 4u; k++) {
        int32_t tp = (int32_t)p->stage[i].pitch[k] + ((((int32_t)p->stage[j].pitch[k] - (int32_t)p->stage[i].pitch[k]) *
                     (int32_t)f) >> 8) + off, tl, n, g, d;
        uint32_t s, fr;
        tl = (int32_t)p->stage[i].level[k] + ((((int32_t)p->stage[j].level[k] - (int32_t)p->stage[i].level[k]) *
             (int32_t)f) >> 8);
        if (p->r_track & SD_RT_HARM) {          /* R4: onto the nearest harmonic of the note (none under it) */
            int32_t sv = tp - (int32_t)r->note * 256;
            uint32_t lo = 0, st;                /* the last harmonic at or under sv: five fixed halvings (a scan of
                                                   all 32 cost ~0.5 % at the worst interrupt) */
            for (st = 16u; st; st >>= 1)
                if (lo + st < 32u && (int32_t)SD_HARM_ST[lo + st] <= sv) lo += st;
            if (lo < 31u && (int32_t)SD_HARM_ST[lo + 1u] - sv < sv - (int32_t)SD_HARM_ST[lo]) lo++;   /* or the next */
            tp = (int32_t)r->note * 256 + (int32_t)SD_HARM_ST[lo];
        }
        tp = tp < SD_R_LO * 256 ? SD_R_LO * 256 : tp > SD_R_HI * 256 ? SD_R_HI * 256 : tp;
        if (c && r->on) {                       /* glide: a one-pole on the target, in log pitch */
            r->pitch[k] = sd_toward(r->pitch[k], tp << 8, c);
            r->level[k] = sd_toward(r->level[k], tl << 8, c);
        } else {
            r->pitch[k] = tp << 8;
            r->level[k] = tl << 8;
        }
        n = r->pitch[k] >> 8;                   /* MIDI x 256 */
        s = (uint32_t)(n >> 8) - SD_R_LO; fr = (uint32_t)n & 255u;
        g = (int32_t)SD_RG[s] + (int32_t)((((int64_t)SD_RG[s + 1u] - SD_RG[s]) * fr) >> 8);   /* Q24 */
        d = (1 << 24) + (int32_t)(((int64_t)g * (g + kk)) >> 24);
        r->a1[k] = sd_recip30(d);
        r->a2[k] = (int32_t)(((int64_t)g * r->a1[k]) >> 24);
        r->a3[k] = (int32_t)(((int64_t)g * r->a2[k]) >> 24);
        r->kn[k] = kk;
        if (r->model >= SD_RM_KP) {             /* the delay a filter's pitch makes, and AP2's coefficient */
            uint32_t note = (uint32_t)n >> 8, cents = ((uint32_t)n & 255u) * 100u >> 8, up = note > 119u;
            uint32_t inc = (uint32_t)(((uint64_t)sd_note_inc(note - 12u * up) * SD_CENTS[99u + cents]) >> 16) << up;
                                                /* (MIDI 120..130: an octave down, doubled; the table ends at 127) */
            int32_t dq = (int32_t)(0xFFFFFFFFu / (inc >> 8));   /* fs / f in Q8: 2^32 / inc */
            if (r->model == SD_RM_KP) dq -= 128;  /* the two-point average delays half a sample */
            dq = dq < 2 * 256 ? 2 * 256 : dq > (int32_t)(SD_DL - 4u) * 256 ? (int32_t)(SD_DL - 4u) * 256 : dq;
            r->dq[k] = r->on ? r->dnext[k] : dq;
            r->dnext[k] = dq;
            r->apa[k] = (int32_t)(((int64_t)(g - (1 << 24)) * sd_recip30((1 << 24) + g)) >> 24);   /* (g-1)/(g+1):
                                                   the phase turns through -pi around the filter's frequency */
        }
        {                                       /* the level law (Q12) x the model's make-up (Q12) -> Q14 */
            uint32_t lv = (uint32_t)(r->level[k] >> 8), li = lv >> 9, lf = lv & 511u;
            int32_t g = (int32_t)SD_RLEVEL[li] + ((((int32_t)SD_RLEVEL[li + 1u] - (int32_t)SD_RLEVEL[li]) * (int32_t)lf) >> 9);
            r->lvl[k] = (g * SD_RMAKEUP[r->model]) >> 10;   /* 16384 x 4 (law) x <= 2 (make-up): fits */
            if (p->r_track & SD_RT_HARM) r->lvl[k] = (r->lvl[k] >> 5) * 19;   /* R4: on the harmonics a filter
                                                   passes more (+1.4 .. +7.4 dB measured on five patches): x 0.59
                                                   (lvl reaches 2^18: shifted first, so it fits) */
        }
        r->pl[k] = p->r_pan[k] > 0 ? 32768 - p->r_pan[k] : 32768;   /* the balance: centre both full */
        r->pr[k] = p->r_pan[k] < 0 ? 32768 + p->r_pan[k] : 32768;
    }
    r->fbk = 32768 - (kk >> 10);                /* KP: 1 - k / 2, 0 .. 0.995 */
    r->dry = p->r_res >> 1;                     /* the AP modes: RES is the dry's share */
    r->aplvl = (r->lvl[0] + r->lvl[1] + r->lvl[2] + r->lvl[3]) >> 2;
    r->on = 1;
}

/* ---- DISTROY: control */
static void sd_knee_params(int32_t t, int32_t l, int32_t *T, int32_t *S, int32_t *inv)   /* a knee at t, a ceiling at l */
{
    *T = t;
    *S = l - t;
    *inv = *S > 0 ? (int32_t)(((1u << 31) / (uint32_t)(3 * *S)) << 1) : 0;   /* 2^23 x 2^8 / (1.5 S): Q8 */
}

/* ---- DISTROY's dynamics (§5.2: SHAPE and BODY): at control rate, from each band's peak */
static int32_t sd_db(int32_t x)                 /* a Q19 magnitude in dB x 256 against full scale (2^19); 0: -120 dB */
{
    uint32_t u = (uint32_t)x, e = 0;
    if (x <= 0) return -120 * 256;
    if (u >> 16) { u >>= 16; e += 16u; }        /* the top bit: five fixed steps, no loop */
    if (u >> 8) { u >>= 8; e += 8u; }
    if (u >> 4) { u >>= 4; e += 4u; }
    if (u >> 2) { u >>= 2; e += 2u; }
    if (u >> 1) e += 1u;
    u = e >= 6u ? (uint32_t)x >> (e - 6u) : (uint32_t)x << (6u - e);   /* 1.xxxxxx */
    return ((int32_t)e - 19) * 1541 + (int32_t)SD_DLOG[u & 63u];
}

/* The gain the dynamics add, dB x 256. BODY is the threshold T (-54 .. -6 dBFS).
 *   PHAT (SHAPE < 0): a low-level compressor: under T, up to 4:1 upward (x 0.75 of the
 *   distance); above T nothing. Quiet parts and tails come up: it thickens. Below -72 dB it lifts no more (no noise
 *   pumped up); +24 dB at most. Its level is the slow envelope or this tick's peak - 3 dB, whichever is higher: an
 *   attack is caught at once, not lifted.
 *   SHARP (SHAPE > 0): an expander that spits transients: + 2 x (fast - slow) (the attack up, the tail down),
 *   and below T down by the distance (a gentle gate). -30 .. +12 dB.
 * The envelopes: fast holds the peak and falls 60 dB in 80 ms; slow is a one-pole, 15 ms up, 150 ms down. */
static int32_t sd_dyn(SdDistroy *d, const SdPatch *p, uint32_t k)
{
    int32_t lv = sd_db(d->pk[k]), a = p->d_shape[k] < 0 ? -p->d_shape[k] : p->d_shape[k], g, T;
    d->pk[k] = 0;
    if (!d->on) d->ef[k] = d->es[k] = lv;
    d->ef[k] = lv > d->ef[k] ? lv : d->ef[k] - 139 > lv ? d->ef[k] - 139 : lv;
    d->es[k] += (int32_t)(((int64_t)(lv - d->es[k]) * (lv > d->es[k] ? 3172 : 317)) >> 16);
    if (!a) return d->dyn[k] = 0;
    T = -54 * 256 + (int32_t)((p->d_body[k] * 12288u) >> 16);
    if (p->d_shape[k] < 0) {
        int32_t l = lv - 3 * 256 > d->es[k] ? lv - 3 * 256 : d->es[k];
        g = (int32_t)(((int64_t)(T - l) * a * 3) >> 17);                   /* a x 0.75 x (T - level) */
        if (g > l + 72 * 256) g = l + 72 * 256;                            /* no lift from under -72 dB */
        g = g > 24 * 256 ? 24 * 256 : g < 0 ? 0 : g;
    } else {
        g = (int32_t)(((int64_t)(d->ef[k] - d->es[k]) * a) >> 14);         /* a x 2 x (fast - slow) */
        if (d->es[k] < T) g -= (int32_t)(((int64_t)(T - d->es[k]) * a) >> 15);
        g = g > 12 * 256 ? 12 * 256 : g < -30 * 256 ? -30 * 256 : g;
    }
    return d->dyn[k] = g;
}

static void sd_distroy_params(SdEngine *e)      /* once a sub-block */
{
    const SdPatch *p = &e->patch;
    SdDistroy *d = &e->d;
    uint32_t k;
    for (k = 0; k < 3u; k++) {                  /* the splits: Butterworth lowpasses (k = sqrt 2) */
        int32_t n = p->d_split[k] < SD_R_LO * 256 ? SD_R_LO * 256 : p->d_split[k] > SD_R_HI * 256 ? SD_R_HI * 256 : p->d_split[k];
        uint32_t st = (uint32_t)(n >> 8) - SD_R_LO, fr = (uint32_t)n & 255u;
        int32_t g = (int32_t)SD_RG[st] + (int32_t)((((int64_t)SD_RG[st + 1u] - SD_RG[st]) * fr) >> 8);
        int32_t dd = (1 << 24) + (int32_t)(((int64_t)g * (g + 23726566)) >> 24);   /* 1 + g (g + 1.414) */
        d->a1[k] = sd_recip30(dd);
        d->a2[k] = (int32_t)(((int64_t)g * d->a1[k]) >> 24);
        d->a3[k] = (int32_t)(((int64_t)g * d->a2[k]) >> 24);
    }
    for (k = 0; k < 4u; k++) {
        uint32_t gi = p->d_gain[k] >> 9, gf = p->d_gain[k] & 511u, lv = p->d_level[k], li = lv >> 9, lf = lv & 511u;
        int32_t alt = p->d_alt[k] >> 1;         /* Q15 */
        d->type[k] = p->d_type[k] < SD_DSHAPES ? p->d_type[k] : SD_DS_NONE;
        d->gain[k] = (int32_t)SD_DGAIN[gi] + ((((int32_t)SD_DGAIN[gi + 1u] - (int32_t)SD_DGAIN[gi]) * (int32_t)gf) >> 9);
        d->bias[k] = p->d_bias[k] >> 1;         /* +-0.5 full scale */
        d->lvl[k] = ((int32_t)SD_RLEVEL[li] + ((((int32_t)SD_RLEVEL[li + 1u] - (int32_t)SD_RLEVEL[li]) * (int32_t)lf) >> 9)) << 2;
        d->pl[k] = p->d_pan[k] > 0 ? 32768 - p->d_pan[k] : 32768;
        d->pr[k] = p->d_pan[k] < 0 ? 32768 + p->d_pan[k] : 32768;
        if (d->type[k] == SD_DS_SOFT) {         /* hardness: the knee from 0 to 0.6; the negative side's ceiling 0.8 */
            sd_knee_params((alt * 19661) >> 15, 32767, &d->t[k], &d->s[k], &d->inv[k]);
            sd_knee_params((alt * 15729) >> 15, 26214, &d->tn[k], &d->sn[k], &d->invn[k]);
        } else if (d->type[k] == SD_DS_HARD) {  /* the knee: a brick wall at 0, a shoulder over the top half at 1 */
            sd_knee_params(32767 - ((alt * 16384) >> 15), 32767, &d->t[k], &d->s[k], &d->inv[k]);
            d->tn[k] = d->t[k]; d->sn[k] = d->s[k]; d->invn[k] = d->inv[k];
        }
        d->fold[k] = (int32_t)((uint32_t)(32768u << 12) / (uint32_t)(32767 - ((alt * 24576) >> 15)));   /* 1 / t, Q12 */
        d->crush[k] = 1 << ((uint32_t)(alt * 14) >> 15);   /* step: 1 (15 bits) .. 2^13 (2 bits) */
    }
    for (k = 0; k < 4u; k++) {                  /* the dynamics (§5.2), then the gain's ramp over the sub-block */
        int32_t tgt = d->gain[k] << 4, db = sd_dyn(d, p, k);   /* GAIN, Q12 */
        if (!d->on) d->g0[k] = tgt;
        else d->g0[k] += d->gs[k] * (int32_t)SD_SUB;           /* where the last ramp ended */
        if (db) {
            int32_t l = (db * 10885) >> 16, n = l >> 8, f = (l & 255) >> 2;   /* dB -> log2 x 256 (1 / 6.0206) */
            int32_t lin = SD_DEXP[f];                           /* Q14 */
            lin = n >= 0 ? lin << n : lin >> -n;                /* n in -6 .. +3 */
            tgt = (int32_t)(((int64_t)tgt * lin) >> 14);
            tgt = tgt > 16383 * 16 ? 16383 * 16 : tgt;          /* at most GAIN's +36 dB */
        }
        d->gs[k] = tgt - d->g0[k] < 0 ? -((d->g0[k] - tgt) >> SD_SUB_SHIFT) : (tgt - d->g0[k]) >> SD_SUB_SHIFT;
        if (d->gs[k] == 0) d->g0[k] = tgt;      /* within a step a frame: there (the same gain as D1, bit for bit) */
    }
    for (k = 0; k < 4u; k++) {                  /* the level and the pan in one gain a channel */
        d->g2[0][k] = (int32_t)(((int64_t)d->lvl[k] * d->pl[k]) >> 15);
        d->g2[1][k] = (int32_t)(((int64_t)d->lvl[k] * d->pr[k]) >> 15);
    }
    d->mix = p->d_mix >> 1;
    d->xover = p->d_xover == SD_DX_LR4 ? SD_DX_LR4 : SD_DX_CMP;
    {                                           /* D3: the feedback, TAME, the limiter */
        int32_t dl = SD_DFMIN + (int32_t)((p->d_ftime * 838u) >> 16), dr = dl - (int32_t)((dl * (p->d_spread * 19661u >> 16)) >> 16);
        d->ftgt[0] = dl;
        d->ftgt[1] = dr < SD_DFMIN ? SD_DFMIN : dr;
        for (k = 0; k < 2u; k++) {              /* TIME and SPREAD glide a sample a tick: the loop's pitch bends */
            if (!d->on || !d->fd[k]) d->fd[k] = d->ftgt[k];
            else d->fd[k] += d->fd[k] < d->ftgt[k] ? 1 : d->fd[k] > d->ftgt[k] ? -1 : 0;
        }
        d->fb = (int32_t)((p->d_fb * 29491u) >> 16);   /* 0 .. 0.9 */
        if (d->fb && !d->fS) sd_knee_params(24576, 34406, &d->fT, &d->fS, &d->finv);   /* 0.75 .. 1.05 full scale */
        d->tame = p->d_tame != 0;
        if (d->tame) {                          /* 130 .. 86 (14.9 kHz .. 1.2 kHz); k 1.41 .. 0.81 */
            int32_t n = (130 << 8) - (int32_t)((p->d_tame * (44u << 8)) >> 16), kk = 23726566 - (int32_t)((p->d_tame * 10066329u) >> 16);
            uint32_t st, fr;
            int32_t g, dd;
            n = n > SD_R_HI * 256 ? SD_R_HI * 256 : n;
            st = (uint32_t)(n >> 8) - SD_R_LO;
            fr = (uint32_t)n & 255u;
            g = (int32_t)SD_RG[st] + (int32_t)((((int64_t)SD_RG[st + 1u] - SD_RG[st]) * fr) >> 8);
            dd = (1 << 24) + (int32_t)(((int64_t)g * (g + kk)) >> 24);
            d->ta1 = sd_recip30(dd);
            d->ta2 = (int32_t)(((int64_t)g * d->ta1) >> 24);
            d->ta3 = (int32_t)(((int64_t)g * d->ta2) >> 24);
        }
        /* the limiter: past 1.5 x full scale at once (a tick late: it is a safety), back over ~0.75 s */
        if (!d->on) d->lg = 32768;
        d->lg += (32768 - d->lg) >> 10;
        if (d->lpk > 49152) {
            int32_t t = (int32_t)((49152u << 15) / (uint32_t)d->lpk);
            if (t < d->lg) d->lg = t;
        }
        d->lpk = 0;
    }
    d->on = 1;
}

/* ---- SPACE: control */
static const uint16_t SD_SP_DIV[12] = {128, 171, 256, 341, 384, 512, 683, 768, 1024, 1365, 1536, 2048};   /* a
   sixteenth x 256: 1/32 1/16T 1/16 1/8T 1/16D 1/8 1/4T 1/8D 1/4 1/2T 1/4D 1/2 */
/* The reverb runs at half rate (22.05 kHz: it costs half, and its tail is darker under 11 kHz), so its lengths are
 * in half-rate samples: 66 .. 92 ms lines, 2.4 .. 8.7 ms diffusers */
static const uint16_t SD_SP_FMAX[4] = {1451, 1667, 1861, 2027};   /* the lines' longest, primes */
static const uint16_t SD_SP_APL[4] = {71, 53, 191, 139};          /* the diffusers, primes */

static void sd_space_params(SdEngine *e)
{
    const SdPatch *p = &e->patch;
    SdSpace *s = &e->sp;
    uint32_t k, gi = p->s_decay >> 9, gf = p->s_decay & 511u;
    int32_t sz = 8192 + (int32_t)((p->s_size * 24575u) >> 16);   /* 0.25 .. 1, Q15 */
    if (p->s_mode & SD_SP_SYNC)                 /* a division of the note driver's sixteenth (samples x 256) */
        s->dtgt = (int32_t)(((uint64_t)e->seq_len_q8 * SD_SP_DIV[(p->s_time * 12u) >> 16]) >> 8);
    else
        s->dtgt = (int32_t)(1u + (p->s_time * 999u) / 65535u) * 11290;   /* ms x 44.1 x 256 */
    if (s->dtgt > (int32_t)(SD_SP_DMAX - 4u) * 256) s->dtgt = (int32_t)(SD_SP_DMAX - 4u) * 256;
    if (s->dtgt < 4 * 256) s->dtgt = 4 * 256;
    if (!s->on) s->dq = s->dnext = s->dtgt;     /* off until now: at the time, no glide */
    s->dq = s->dnext;
    s->dnext = s->dq + (s->dtgt - s->dq) / 16;  /* a tape: ~12 ms to most of the way */
    s->fb = (int32_t)((p->s_fb * 31130u) >> 16);                 /* 0 .. 0.95 */
    s->dtone = 2621 + (int32_t)((p->s_dtone * 30146u) >> 16);    /* the lowpass's coefficient, 0.08 .. 1 */
    s->rtone = 2621 + (int32_t)((p->s_rtone * 30146u) >> 16);
    s->rtone = 2 * s->rtone - ((s->rtone * s->rtone) >> 15);   /* 1 - (1 - c)^2: the same corner at half rate */
    s->g = (int32_t)SD_SP_G[gi] + ((((int32_t)SD_SP_G[gi + 1u] - (int32_t)SD_SP_G[gi]) * (int32_t)gf) >> 9);
    s->dlvl = p->s_dlevel >> 1;
    s->rlvl = p->s_rlevel >> 1;
    s->pp = (p->s_mode & SD_SP_PP) != 0;
    for (k = 0; k < 4u; k++) {                  /* SIZE: the lines move a sample a sub-block toward it */
        s->ftgt[k] = (SD_SP_FMAX[k] * sz) >> 15;
        if (!s->on || !s->flen[k]) s->flen[k] = s->ftgt[k];
        else s->flen[k] += s->flen[k] < s->ftgt[k] ? 1 : s->flen[k] > s->ftgt[k] ? -1 : 0;
    }
    s->on = 1;
}

/* The traversal: a pass is one play of a segment in its order; the next pass is built when this one starts, so
 * the step after this one is always known (a TIE ahead keeps the note on past its gate). */
static uint32_t sd_seq_build(SdSeqT *t, uint32_t seg, uint8_t *ord)   /* a pass of segment seg -> its length */
{
    const SdPattern *p = &t->pat;
    uint8_t b[SD_SEGS + 1u];
    uint32_t sc = sd_seg_bounds(p, b), n = 0, k, o;
    if (seg >= sc || (p->seg_skip >> seg & 1u)) return 0;
    o = p->seg_order[seg] % SD_SORDERS;
    for (k = b[seg]; k < b[seg + 1u]; k++)
        if (p->step[k].type != SD_ST_SKIP) ord[n++] = (uint8_t)k;
    if (o == SD_SO_REV)
        for (k = 0; k < n / 2u; k++) { uint8_t t = ord[k]; ord[k] = ord[n - 1u - k]; ord[n - 1u - k] = t; }
    if (o == SD_SO_PP && n > 2u) {              /* there and back, the ends once: 1 2 3 4 3 2 */
        uint32_t m = n;
        for (k = m - 2u; k >= 1u; k--) ord[n++] = ord[k];
    } else if (o == SD_SO_RND && n > 1u) {       /* shuffled each pass */
        for (k = n - 1u; k > 0u; k--) {
            uint32_t j = sd_seq_rand(&t->sq_rng) % (k + 1u);
            uint8_t t = ord[k]; ord[k] = ord[j]; ord[j] = t;
        }
    }
    return n;
}

static void sd_seq_next_pass(SdSeqT *t)       /* after (sq_seg, sq_rep): the next pass with steps, built */
{
    uint8_t b[SD_SEGS + 1u];
    uint32_t sc = sd_seg_bounds(&t->pat, b), seg = t->sq_seg, rep = t->sq_rep, tries, n = 0, nb = t->sq_cur ^ 1u;
    for (tries = 0; tries < 2u * SD_SEGS * 4u && !n; tries++) {
        if (++rep >= (t->pat.seg_rep[seg] < 1u ? 1u : t->pat.seg_rep[seg] > 4u ? 4u : t->pat.seg_rep[seg])) {
            rep = 0;
            seg = seg + 1u >= sc ? 0u : seg + 1u;
        }
        n = sd_seq_build(t, seg, t->sq_ord[nb]);
    }
    t->sq_nseg = (uint8_t)seg;
    t->sq_nrep = (uint8_t)rep;
    t->sq_n[nb] = (uint8_t)n;                   /* 0: every step skipped */
}

static void sd_seq_start(SdSeqT *t)           /* START: the first pass with steps, from segment A */
{
    t->sq_cur = 0;
    t->sq_seg = 0;
    t->sq_rep = 0;
    t->sq_pos = 0;
    t->sq_n[0] = (uint8_t)sd_seq_build(t, 0, t->sq_ord[0]);
    if (!t->sq_n[0]) {                          /* A skipped or empty: the next with steps */
        t->sq_rep = 0xFF;                       /* (any: past the repeats) */
        sd_seq_next_pass(t);
        t->sq_cur ^= 1u;
        t->sq_seg = t->sq_nseg;
        t->sq_rep = t->sq_nrep;
    }
    sd_seq_next_pass(t);
}

static uint32_t sd_seq_advance(SdSeqT *t)     /* to the next step of the traversal -> 1 at the loop's one */
{
    uint32_t one = 0;
    if (++t->sq_pos < t->sq_n[t->sq_cur]) return 0;
    t->sq_pos = 0;
    one = t->sq_nseg < t->sq_seg || (t->sq_nseg == t->sq_seg && t->sq_nrep <= t->sq_rep);   /* the passes go
                                                   forward through (segment, repeat) but at the loop's one */
    t->sq_cur ^= 1u;
    t->sq_seg = t->sq_nseg;
    t->sq_rep = t->sq_nrep;
    sd_seq_next_pass(t);
    return one;
}

static uint32_t sd_seq_peek(const SdSeqT *t)  /* the step after this one */
{
    uint32_t c = t->sq_cur;
    if (t->sq_pos + 1u < t->sq_n[c]) return t->sq_ord[c][t->sq_pos + 1u];
    return t->sq_n[c ^ 1u] ? t->sq_ord[c ^ 1u][0] : 0xFFu;
}

static void sd_seq_steplen(SdEngine *e, SdSeqT *t)         /* this step's length (DIV, SWING) and gate, in samples x 256 */
{
    const SdPattern *p = &t->pat;
    uint32_t d = p->div < SD_SDIVS ? p->div : 2u, len = e->seq_len_q8 * SD_SDIV_NUM[d] / SD_SDIV_DEN[d];
    uint32_t sw = p->swing < 50u ? 50u : p->swing > 75u ? 75u : p->swing;
    const SdStep *st = &p->step[t->seq_step];
    if (sw != 50u) len = (len >> 7) * (t->seq_count & 1u ? 100u - sw : sw) / 50u << 7;   /* a pair's 2 x len split */
    t->seq_step_q8 = len;
    t->seq_gate_q8 = (len >> 7) * (st->gate < 1u ? 1u : st->gate > 100u ? 100u : st->gate) / 100u << 7;
}

static void sd_seq_glide(SdEngine *e, SdSeqT *t, uint32_t note, uint32_t vel)   /* SLIDE: the held note glides to note */
{
    uint32_t i;
    for (i = 0; i < e->nvoices; i++) {
        SdVoice *v = &e->voice[i];
        if (v->gate && v->note == t->seq_note[0] && v->src == t->src) {
            v->note = (uint8_t)note;
            v->inc_tgt = sd_note_inc(note);
            v->glide = 1;
            e->r.note = (uint8_t)note;
            e->m_note = (uint8_t)note;
            (void)vel;
            return;
        }
    }
    sd_voice_on(e, note, vel, t->src);          /* (nothing held: a new note) */
}

static void sd_seq_off(SdEngine *e, SdSeqT *t)  /* every note the track holds, off */
{
    uint32_t i;
    for (i = 0; i < t->seq_nn && i < 4u; i++) sd_voice_off(e, t->seq_note[i], t->src);
    t->seq_note_on = 0;
    t->seq_nn = 0;
}

static uint32_t sd_seq_xn(const SdSeqT *t, uint32_t n)   /* a step's note, TURING's NOTES on it, transposed */
{
    int32_t v;
    if (t->tur_deg) {                           /* NOTES: degrees of the pattern's scale, from its key (step 1) */
        uint32_t key = t->pat.step[0].note, sc = t->pat.scale;
        n = sd_scale_note(key, sc, sd_scale_degree(key, sc, n) + t->tur_deg);
    }
    v = (int32_t)n + t->xpose + t->orb_x;
    return (uint32_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

/* ORBIT's transposes on both tracks, from its step (0 when it is off) */
static void sd_orb_xpose(SdEngine *e)
{
    const SdOrbit *o = &e->orb;
    uint32_t s = e->orb_step % SD_ORB_STEPS;
    e->t[0].orb_x = o->on ? o->xa[s] : 0;
    e->t[1].orb_x = o->on ? o->xb[s] : 0;
}
/* at SEQ A's one: A's loop measured (the glide's span), and after RATE loops the next step */
static void sd_orb_one(SdEngine *e)
{
    const SdOrbit *o = &e->orb;
    e->orb_loop_sb = e->orb_loop_t;
    e->orb_loop_t = 0;
    if (o->on && ++e->orb_loops >= SD_ORB_LOOPS(o)) {
        e->orb_loops = 0;
        e->orb_step = (uint8_t)((e->orb_step + 1u) % SD_ORB_LEN(o));
        e->orb_t = 0;
    }
    if (e->orb_step >= SD_ORB_LEN(o)) e->orb_step = 0;   /* (LENGTH made shorter meanwhile) */
    e->orb_shown = e->orb_step;
    sd_orb_xpose(e);
}

/* TURING's NOTES now: its output x AMOUNT as degrees, +-12 at 100 % (0: not NOTES, or off) */
static int32_t sd_tur_degrees(const SdEngine *e)
{
    const SdPatch *p = &e->patch;
    int32_t d;
    if (!SD_TUR_ON(p) || p->tur_dst != SD_TD_NOTES || !p->tur_amt) return 0;
    d = e->tur_out * p->tur_amt * 12;           /* 32767 x 100 x 12: fits */
    d = d < 0 ? -((-d + 1638350) / 3276700) : (d + 1638350) / 3276700;   /* (once a step: not the audio's path) */
    return d;
}

/* TURING's clock, once a sub-block before the tracks: a step every DIV of the tempo. The bit leaving the loop
 * (LENGTH bits) comes back in at the newest place, flipped CHANGE % of the time: CHANGE 0 a locked loop, 100 its
 * inverse every pass (a loop twice as long), between them it drifts. START lines it up with step 1. */
static void sd_tur_tick(SdEngine *e, uint32_t start)
{
    const SdPatch *p = &e->patch;
    uint32_t d = SD_TUR_DIV(p), len = e->seq_len_q8 * SD_SDIV_NUM[d] / SD_SDIV_DEN[d];
    if (!SD_TUR_ON(p)) return;
    if (start) e->tur_pos_q8 = len;
    if (e->tur_pos_q8 >= len) {
        uint32_t n = SD_TUR_LEN(p), b = (e->tur_reg >> (n - 1u)) & 1u;
        e->tur_pos_q8 -= len;
        if (e->tur_pos_q8 >= len) e->tur_pos_q8 = 0;   /* (the tempo jumped: no catching up) */
        if (sd_seq_rand(&e->tur_rng) % 100u < SD_TUR_CHANGE(p)) b ^= 1u;
        e->tur_reg = (uint16_t)(e->tur_reg << 1 | b);
        e->tur_out = sd_tur_dac(e->tur_reg);
        e->tur_count++;
    }
    e->tur_pos_q8 += SD_SUB << 8;
}

static void sd_seq_on(SdEngine *e, SdSeqT *t, const SdStep *st, uint32_t vel)   /* the step's note and its chord */
{
    uint32_t i, n = 1, root = sd_seq_xn(t, st->note);
    if (t->quiet) return;                       /* OFF (or B in MONO): it keeps time, silent */
    t->seq_note[0] = (uint8_t)root;
    for (i = 0; i < 3u; i++)                    /* the chord: offsets from the step's note (0: none) */
        if (st->chord[i]) {
            int32_t c = (int32_t)root + st->chord[i];
            if (c >= 0 && c <= 127) t->seq_note[n++] = (uint8_t)c;
        }
    if (n > e->quota[t->src]) n = e->quota[t->src];   /* (a chord past the track's voices: its top notes go) */
    for (i = 0; i < n; i++) sd_voice_on(e, t->seq_note[i], vel, t->src);
    t->seq_nn = (uint8_t)n;
    t->seq_note_on = 1;
}

static void sd_seq_track(SdEngine *e, SdSeqT *t, uint32_t run, uint32_t main)   /* a track, once a sub-block:
                                                   start, stop, steps, gates; main: track A (the Reactor's clock) */
{
    uint32_t sq = t->pat_seq;
    t->quiet = (uint8_t)(t->mute || (!main && e->mode_on == SD_MODE_MONO));   /* (B needs DUO or POLY) */
    if (t->quiet && t->seq_note_on) sd_seq_off(e, t);
    if (!(sq & 1u) && sq != t->pat_taken) {     /* a new pattern: from the next step on (the traversal keeps its
                                                   place; a pass past the new end restarts at A) */
        t->pat_taken = sq;
        t->pat = t->pat_in;
        t->width = (uint8_t)sd_pattern_width(&t->pat);
        sd_plan(e);                             /* (its width may have changed the quotas) */
        if (t->seq_on) {
            uint8_t b[SD_SEGS + 1u];
            if (t->sq_seg >= sd_seg_bounds(&t->pat, b)) { t->sq_seg = 0; t->sq_rep = 0; }
            t->sq_n[t->sq_cur] = (uint8_t)sd_seq_build(t, t->sq_seg, t->sq_ord[t->sq_cur]);
            if (t->sq_pos >= t->sq_n[t->sq_cur]) t->sq_pos = t->sq_n[t->sq_cur] ? t->sq_n[t->sq_cur] - 1u : 0u;
            sd_seq_next_pass(t);
        }
    }
    if (run != t->seq_on) {
        t->seq_on = (uint8_t)run;
        if (t->seq_on) {                        /* START: step 1 now */
            sd_seq_start(t);
            t->xpose = t->xpose_next;           /* (a transpose waiting: now) */
            t->sq_pos = 0xFF;                   /* (the first advance lands on 0) */
            t->seq_count = 0xFFFFFFFFu;
            t->seq_step_q8 = e->seq_len_q8;
            t->seq_pos_q8 = t->seq_step_q8;
            if (main) {                         /* ORBIT: its first step, A's loop as its length says (until one) */
                uint32_t d = t->pat.div < SD_SDIVS ? t->pat.div : 2u;
                uint32_t step = e->seq_len_q8 * SD_SDIV_NUM[d] / SD_SDIV_DEN[d];   /* (32-bit: <= 17 M) */
                e->orb_step = e->orb_loops = 0;
                e->orb_t = e->orb_loop_t = 0;
                e->orb_loop_sb = ((step >> 8) * t->pat.len) >> SD_SUB_SHIFT;   /* (<= 66150 x 64: fits) */
                e->orb_shown = 0;
                sd_orb_xpose(e);
            }
        } else if (t->seq_note_on) {
            sd_seq_off(e, t);
        }
    }
    if (!t->seq_on) return;
    if (t->seq_pos_q8 >= t->seq_step_q8) {      /* a new step: it starts at this sub-block's first frame */
        const SdStep *st;
        uint32_t one, next;
        t->seq_pos_q8 -= t->seq_step_q8;
        if (t->sq_pos == 0xFFu) { t->sq_pos = 0; one = 0; }
        else one = sd_seq_advance(t);
        if (!t->sq_n[t->sq_cur]) {              /* every step skipped: silence */
            if (t->seq_note_on) sd_seq_off(e, t);
            t->seq_step_q8 = e->seq_len_q8;
            t->seq_pos_q8 += SD_SUB << 8;
            return;
        }
        if (one) t->xpose = t->xpose_next;      /* SCL + a key: the transpose at the one */
        if (one && main) sd_orb_one(e);         /* ORBIT: its step at A's one (B's notes take it from now on) */
        t->tur_deg = (int8_t)(main ? sd_tur_degrees(e) : 0);   /* TURING's NOTES: this step's place (A) */
        t->seq_count++;
        t->seq_step = t->sq_ord[t->sq_cur][t->sq_pos];
        t->seq_shown = t->seq_step;
        t->seq_seg_shown = t->sq_seg;
        sd_seq_steplen(e, t);
        if (t->seq_pos_q8 >= t->seq_step_q8) t->seq_pos_q8 = 0;   /* (the tempo jumped: no catching up) */
        if (main) sd_reactor_clock(e);
        st = &t->pat.step[t->seq_step];
        t->rat_q8 = 0;
        if (st->type <= SD_ST_SLIDE && st->prob < 100u && sd_seq_rand(&t->sq_rng) % 100u >= st->prob) {
            if (t->seq_note_on) sd_seq_off(e, t);   /* PROB said no: a rest this time */
            t->seq_pos_q8 += SD_SUB << 8;
            return;
        }
        if (st->type == SD_ST_NOTE && st->ratchet > 1u) {   /* RATCHET: the step in hits, 50 ms apart at least */
            uint32_t n = st->ratchet > SD_RATCHET_MAX ? SD_RATCHET_MAX : st->ratchet;
            while (n > 1u && t->seq_step_q8 / n < SD_RATCHET_MIN_SAMPLES << 8) n--;
            if (n > 1u) {
                t->rat_n = (uint8_t)n;
                t->rat_i = 0;
                t->rat_q8 = t->seq_step_q8 / n;
                t->rat_vel = st->vel;
                t->seq_gate_q8 = (t->rat_q8 >> 7) * (st->gate < 1u ? 1u : st->gate > 100u ? 100u : st->gate) / 100u << 7;
            }
        }
        if (st->type == SD_ST_TIE && t->seq_note_on) {
            /* held on: nothing new */
        } else if (st->type == SD_ST_SLIDE && t->seq_note_on && t->seq_nn == 1u && !st->chord[0]) {
            sd_seq_glide(e, t, sd_seq_xn(t, st->note), st->vel);   /* (a single note glides; a chord plays as
                                                   a NOTE) */
            t->seq_note[0] = (uint8_t)sd_seq_xn(t, st->note);
        } else {
            if (t->seq_note_on) sd_seq_off(e, t);
            if (st->type == SD_ST_NOTE || st->type == SD_ST_SLIDE) sd_seq_on(e, t, st, st->vel);
        }
        next = sd_seq_peek(t);
        if (!t->rat_q8 && next < SD_PSTEPS &&
            (t->pat.step[next].type == SD_ST_TIE || t->pat.step[next].type == SD_ST_SLIDE))
            t->seq_gate_q8 = 0xFFFFFFFFu;       /* a TIE or SLIDE next: held to it */
    } else if (t->rat_q8 && t->rat_i + 1u < t->rat_n && t->seq_pos_q8 >= (t->rat_i + 1u) * t->rat_q8) {
        const SdStep *st = &t->pat.step[t->seq_step];
        t->rat_i++;                             /* the next hit, softer: x 0.75 each */
        t->rat_vel = (uint8_t)(t->rat_vel * 3u / 4u < 1u ? 1u : t->rat_vel * 3u / 4u);
        if (t->seq_note_on) sd_seq_off(e, t);
        sd_seq_on(e, t, st, t->rat_vel);
        {                                       /* this hit's gate: GATE of a hit, from its start */
            uint32_t g = st->gate;
            g = g < 1u ? 1u : g > 100u ? 100u : g;
            t->seq_gate_q8 = t->rat_i * t->rat_q8 + ((t->rat_q8 >> 7) * g / 100u << 7);
        }
    } else if (t->seq_note_on && t->seq_pos_q8 >= t->seq_gate_q8) {
        sd_seq_off(e, t);
    }
    t->seq_pos_q8 += SD_SUB << 8;
}

static void sd_seq_tick(SdEngine *e)            /* once a sub-block: both tracks, A first */
{
    sd_seq_retime(e);                           /* first: START measures step 1 in this tempo */
    sd_tur_tick(e, e->seq_run && !e->t[0].seq_on);   /* TURING first: a step at the same time sees its new place */
    {                                           /* ORBIT: a new one from the main loop (its step kept), its clocks */
        uint32_t sq = e->orb_seq;
        if (!(sq & 1u) && sq != e->orb_taken) {
            e->orb_taken = sq;
            e->orb = e->orb_in;
            if (e->orb_step >= SD_ORB_LEN(&e->orb)) e->orb_step = 0;
            sd_orb_xpose(e);
        }
        e->orb_t++;
        e->orb_loop_t++;
    }
    sd_seq_track(e, &e->t[0], e->seq_run, 1);
    if (e->seqb || e->t[1].seq_on) sd_seq_track(e, &e->t[1], e->seq_run && e->seqb, 0);
}

/* >> n toward zero: C's / 2^n without the divide (the compiler emits the chip's divide for a signed / 2: C1 took
 * them out of every per-sample path, as the rules say) */
SD_INLINE int32_t sd_tz(int32_t t, uint32_t n) { return t < 0 ? -((-t) >> n) : t >> n; }   /* >> n toward zero */
static int32_t sd_iabs(int32_t x) { return x < 0 ? -x : x; }   /* (no libc in the firmware) */
static int32_t sd_clamp16(int32_t x) { return x < 0 ? 0 : x > 65535 ? 65535 : x; }
static int32_t sd_mod(int32_t base, int32_t env_q15, int32_t depth)   /* base + env * depth, 0..65535 */
{
    return sd_clamp16(base + ((env_q15 * depth) >> 14));       /* 32767 * 32767 >> 14: +-65534 */
}

static int32_t sd_fold_gain(uint32_t f)         /* the fold law, Q12, 0..65535 */
{
    uint32_t i = f >> 9, fr = f & 511u;
    return (int32_t)SD_FOLD_G[i] + ((((int32_t)SD_FOLD_G[i + 1u] - (int32_t)SD_FOLD_G[i]) * (int32_t)fr) >> 9);
}

SD_INLINE int32_t sd_cubic(int32_t u)           /* u - u^3 / 3 of u in Q23, clamped at +-1: the squash's soft clip */
{
    int32_t u15;
    u = u > 8388608 ? 8388608 : u < -8388608 ? -8388608 : u;
    u15 = u >> 8;
    return u - (((((((u15 * u15) >> 15) * u15) >> 15) * 10923) >> 15) * 256);   /* 32768^2: fits at each step */
}

/* The warp law at w, the fold law at f; tr the tracking (Q15), dmin the shortest saw edge (Q16 of a cycle). */
static void sd_shape_params(SdShape *sh, uint32_t w, uint32_t f, int32_t tr, uint32_t dmin)
{
    uint32_t i = w >> 9, fr = w & 511u, d16, g;
    const SdWarpRow *r0 = &SD_WARP[i], *r1 = &SD_WARP[i + 1u];
    int32_t fg;
    if (w == sh->last_w && tr == sh->last_tr && dmin == sh->last_dmin) goto fold;   /* as the last tick: no divisions */
    sh->last_w = w;
    sh->last_tr = tr;
    sh->last_dmin = dmin;
    sh->t = (uint32_t)((int32_t)r0->t_q15 + (((int32_t)r1->t_q15 - (int32_t)r0->t_q15) * (int32_t)fr >> 9));
    d16 = (uint32_t)((int32_t)r0->d16 + (((int32_t)r1->d16 - (int32_t)r0->d16) * (int32_t)fr >> 9));
    if (d16 < dmin) d16 = dmin > 32768u ? 32768u : dmin;
    sh->d = d16 << 16;
    sh->k1 = 0x80000000u / d16;                 /* 0.5 / d in Q16: the first half-cycle into [0, d) */
    sh->k2 = 0x80000000u / (65536u - d16);
    g = (uint32_t)((int32_t)r0->g_q8 + (((int32_t)r1->g_q8 - (int32_t)r0->g_q8) * (int32_t)fr >> 9));
    g = SD_G_MIN + (uint32_t)(((int32_t)g - (int32_t)SD_G_MIN) * tr >> 15);
    sh->sq_g = g;
    sh->sq_norm = 0x40000000u / (uint32_t)sd_cubic((int32_t)(g << 15));   /* Q30 / Q23 -> Q7: 1 / cubic(G) */
    sh->shape = (uint8_t)(!w ? SD_SH_SINE : sh->t < 32767u ? SD_SH_BLEND : SD_SH_TRI);
    sh->sq = sh->shape == SD_SH_TRI && g > SD_G_MIN;
fold:
    fg = sd_fold_gain(f);
    sh->fg = sh->fg_next;
    sh->fg_next = 4096 + ((fg - 4096) * tr >> 15);   /* (40960 - 4096) * 32767: fits */
    sh->fold = sh->fg != 4096 || sh->fg_next != 4096;
}

/* A bank's parameters for a sub-block, played by voice v from bank patch bp: its waveform, every oscillator's
 * frequency and level, the routing. */
static void sd_bank_params(SdBank *bk, const SdPatch *p, const SdBankPatch *bp, const SdVoice *v, uint32_t track,
                           uint32_t pmul)
{
    int32_t e2 = v->env2.lvl >> 15;             /* Q15 */
    int32_t tr = track ? (int32_t)SD_TRACK[v->note & 127u] : 32767;
    uint32_t k = SD_OPS, carrier = SD_NOWHERE;
    uint32_t xm = (uint32_t)sd_clamp16(p->xm + 2 * bp->xm), dt = (uint32_t)sd_clamp16(p->dt + 2 * bp->dt);
    uint64_t vp = ((uint64_t)v->inc * pmul) >> 16;   /* C1: PITCH; past 0x7FFFFFFF (~22 kHz) it is silent anyway */
    uint32_t vinc = vp > 0x7FFFFFFFu ? 0x7FFFFFFFu : (uint32_t)vp;
    sd_shape_params(&bk->sh, (uint32_t)sd_mod(sd_clamp16(p->warp + 2 * bp->wp), e2, p->e2_warp),
                    (uint32_t)sd_mod(sd_clamp16(p->fold + 2 * bp->fd), e2, p->e2_fold), tr,
                    track ? vinc >> 16 : 0u);  /* the saw's edge: at least one sample of the note */
    xm = xm * (uint32_t)tr >> 15;
    while (k--) {                               /* right to left: the nearest carrier to the right is known */
        const SdOscPatch *q = &bp->osc[k];
        SdOsc *o = &bk->osc[k];
        int32_t c = q->fine + ((((int32_t)k * 2 - 3) * (int32_t)dt * SD_DT_CENTS) >> 16);
        uint32_t ratio = (q->harm < 1u ? 1u : q->harm > 32u ? 32u : q->harm) *
                         SD_CENTS[(c < -99 ? -99 : c > 99 ? 99 : c) + 99];   /* Q16, <= 32 x 1.06: fits */
        uint64_t inc = (uint64_t)vinc * ratio >> 16;
        int32_t l = q->level >> 1;              /* Q15 */
        if (q->role == SD_ROLE_C) { o->tgt = SD_CARRIER; carrier = k; }
        else if (q->role == SD_ROLE_S) o->tgt = (uint8_t)(k + 1u < SD_OPS ? k + 1u : SD_NOWHERE);
        else o->tgt = (uint8_t)carrier;
        if (o->tgt != SD_CARRIER)               /* a modulator: level x ENV2 x XM */
            l = (int32_t)((uint32_t)((l * e2) >> 15) * xm >> 16);
        if (o->tgt != SD_CARRIER && inc > SD_INC_4K)   /* a modulator past 4 kHz: its index fades out by 12 kHz */
            l = inc >= SD_INC_12K ? 0 : (int32_t)((uint32_t)l * ((SD_INC_12K - (uint32_t)inc) >> 12) /
                                                  ((SD_INC_12K - SD_INC_4K) >> 12));
        if (inc >= SD_INC_20K || o->tgt == SD_NOWHERE) l = 0;    /* near Nyquist, or feeding nothing: silent */
        else if (inc > SD_INC_16K)              /* 16..20 kHz: fading out */
            l = (int32_t)((uint32_t)l * ((SD_INC_20K - (uint32_t)inc) >> 12) / ((SD_INC_20K - SD_INC_16K) >> 12));
        o->inc = (uint32_t)inc;
        o->lvl = o->tgt == SD_NOWHERE ? 0 : o->lvl_next;   /* feeding nothing: silent at once, not faded from the
                                                   old level (sd_bank_run would write that into bus[SD_NOWHERE]) */
        o->lvl_next = l;
    }
}

static void sd_nav(SdEngine *e, int32_t w[SD_BANKS])   /* each bank's weight, Q15 */
{
    uint32_t x, y, b, s = 0, r;
    if (e->mode_on == SD_MODE_POLY) {
        for (b = 0; b < SD_BANKS; b++) w[b] = SD_POLY_GAIN;
        return;
    }
    if (e->course_run && e->patch.course && e->mode_on == SD_MODE_MONO) {   /* COURSE: between two cells */
        const SdPatch *p = &e->patch;
        uint32_t a = e->course_seg, f = e->course_ph >> 5;             /* Q15 */
        x = (uint32_t)((int32_t)p->cell_x[a] + (((int32_t)p->cell_x[a + 1u] - (int32_t)p->cell_x[a]) * (int32_t)f >> 15));
        y = (uint32_t)((int32_t)p->cell_y[a] + (((int32_t)p->cell_y[a + 1u] - (int32_t)p->cell_y[a]) * (int32_t)f >> 15));
        e->course_ph += e->course_step;
        if (e->course_ph >= 1u << 20) {            /* the next cell: hold the last one */
            if (e->course_seg < 2u) {
                uint32_t ms;
                e->course_seg++;
                e->course_ph = 0;
                ms = p->cell_ms[e->course_seg] ? p->cell_ms[e->course_seg] : 1u;
                e->course_step = 335544320u / (ms * 441u);
            } else {
                e->course_ph = (1u << 20) - 1u;
                e->course_step = 0;
            }
        }
    } else {
        x = e->patch.x;
        y = e->patch.y;
    }
    if (e->mvx | e->mvy) {                      /* C1: the matrix's X / Y and MOVE's orbit, on top of either */
        x = (uint32_t)sd_clamp16((int32_t)x + e->mvx);
        y = (uint32_t)sd_clamp16((int32_t)y + e->mvy);
    }
    e->nav_x = (uint16_t)x;
    e->nav_y = (uint16_t)y;
    if (e->w_ok && x == e->w_x && y == e->w_y && e->mode_on == e->w_mode) {   /* unmoved: the last weights */
        for (b = 0; b < SD_BANKS; b++) w[b] = e->w[b];
        return;
    }
    e->w_ok = 1;
    e->w_x = x;
    e->w_y = y;
    e->w_mode = e->mode_on;
    x >>= 1; y >>= 1;                           /* Q15 */
    if (e->mode_on == SD_MODE_DUO) {            /* X only, inside each voice: A <-> B */
        w[0] = w[2] = (int32_t)(32767u - x);
        w[1] = w[3] = (int32_t)x;
    } else {                                    /* A top left, B top right, C bottom right, D bottom left */
        w[0] = (int32_t)((32767u - x) * y >> 15);
        w[1] = (int32_t)(x * y >> 15);
        w[2] = (int32_t)(x * (32767u - y) >> 15);
        w[3] = (int32_t)((32767u - x) * (32767u - y) >> 15);
    }
    /* normalised by (sum w^2)^(1/4): a corner is 1, the centre's four are 0.354 (between 0.25 and 0.5) */
    for (b = 0; b < SD_BANKS; b++) s += (uint32_t)(w[b] * w[b]) >> 2;   /* Q28 */
    if (e->mode_on == SD_MODE_DUO) s >>= 1;     /* two pairs counted: one voice's */
    r = sd_isqrt(sd_isqrt(s) << 14);            /* (s)^(1/4) in Q14 */
    if (r)
        for (b = 0; b < SD_BANKS; b++) {
            int32_t g = (w[b] << 14) / (int32_t)r;
            w[b] = g > 32767 ? 32767 : g;
        }
    for (b = 0; b < SD_BANKS; b++) e->w[b] = w[b];
}

/* ---- C1: the modulation, once a tick: two LFOs, the matrix, the four macros, onto e->patch (a copy of the base).
 * Everything at control rate: a destination moves as its knob would (the engines ramp what needs ramping). */
SD_INLINE int32_t sd_lfo_wave(uint32_t shape, uint32_t ph, int32_t sh)   /* Q15, -32767..32767 */
{
    int32_t x = (int32_t)(ph >> 16) - 32768;    /* the ramp, -32768..32767 */
    switch (shape) {
    case SD_LS_SIN: return sd_sine(ph);
    case SD_LS_TRI: { int32_t t = (int32_t)((ph < 0x80000000u ? ph : ~ph) >> 15) - 32768;   /* up, then down */
                      return t < -32767 ? -32767 : t; }
    case SD_LS_SAW: return x <= -32767 ? 32767 : -x;
    case SD_LS_RAMP: return x < -32767 ? -32767 : x;
    case SD_LS_SQR: return ph < 0x80000000u ? 32767 : -32767;
    default: return sh;                          /* S&H: drawn at each cycle's start (glided by the caller) */
    }
}

static int32_t sd_lfo_tick(SdEngine *e, const SdPatch *p, uint32_t k)
{
    uint32_t inc, old = e->lfo_ph[k];
    if (p->lfo_mode[k] & SD_LM_SYNC) {          /* a division of the sixteenth: its period in ticks */
        uint32_t per = (e->seq_len_q8 * SD_LFO_DIV[(p->lfo_rate[k] * SD_LFO_DIVS) >> 16]) >> 13;
        inc = 0xFFFFFFFFu / (per ? per : 1u);
    } else {
        uint32_t i = p->lfo_rate[k] >> 9, f = p->lfo_rate[k] & 511u;
        inc = SD_LFO_INC[i] + (uint32_t)(((uint64_t)(SD_LFO_INC[i + 1u] - SD_LFO_INC[i]) * f) >> 9);
    }
    if (e->m_trig && (p->lfo_mode[k] & (SD_LM_KEY | SD_LM_ONE))) old = 0u - inc;   /* restart: this tick lands on 0 */
    else if ((p->lfo_mode[k] & SD_LM_ONE) && old + inc < old) inc = 0xFFFFFFFFu - old;   /* ONE: held at the end */
    e->lfo_ph[k] = old + inc;
    if (e->lfo_ph[k] < old || (e->m_trig && (p->lfo_mode[k] & (SD_LM_KEY | SD_LM_ONE)))) {   /* a new cycle: S&H draws */
        e->m_seed = e->m_seed * 1103515245u + 12345u;
        e->lfo_sh[k] = (int32_t)(e->m_seed >> 16) - 32768;
    }
    if (p->lfo_shape[k] == SD_LS_SH) {          /* the step, glided an eighth a tick: no click, still a step */
        int32_t d = e->lfo_sh[k] - e->lfo_shg[k];
        e->lfo_shg[k] += d / 8 + (d > 0) - (d < 0);
        return e->lfo_shg[k];
    }
    if (p->lfo_mode[k] & SD_LM_ONE)              /* ONE: unipolar, so it starts or ends at the patch's own value: a
                                                   SAW falls from full to 0, a RAMP grows from 0 (a pitch drop ends in
                                                   tune) */
        return (sd_lfo_wave(p->lfo_shape[k], e->lfo_ph[k], e->lfo_sh[k]) + 32767) >> 1;
    return sd_lfo_wave(p->lfo_shape[k], e->lfo_ph[k], e->lfo_sh[k]);
}

static void sd_add16(uint16_t *f, int32_t d) { *f = (uint16_t)sd_clamp16((int32_t)*f + d); }
static void sd_adds16(int16_t *f, int32_t d)
{
    int32_t v = (int32_t)*f + d;
    *f = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
}
SD_INLINE int32_t sd_mw(int32_t m, int32_t w) { return (m * w) >> 15; }   /* a macro (Q15) x its depth */

static void sd_mod_restore(SdEngine *e)         /* every field the matrix and the macros write, from the base */
{
    SdPatch *p = &e->patch;
    const SdPatch *b = &e->base;
    uint32_t k;
    p->xm = b->xm; p->warp = b->warp; p->fold = b->fold; p->dt = b->dt; p->width = b->width;
    p->r_pos = b->r_pos; p->r_freq = b->r_freq; p->r_res = b->r_res; p->r_drive = b->r_drive; p->r_mix = b->r_mix;
    p->r_slew = b->r_slew; p->r_low = b->r_low;
    for (k = 0; k < 4u; k++) {
        p->d_gain[k] = b->d_gain[k]; p->d_alt[k] = b->d_alt[k]; p->d_bias[k] = b->d_bias[k];
        p->d_level[k] = b->d_level[k]; p->d_shape[k] = b->d_shape[k];
    }
    p->d_fb = b->d_fb; p->d_tame = b->d_tame; p->d_mix = b->d_mix; p->d_xover = b->d_xover;
    for (k = 0; k < 3u; k++) p->d_split[k] = b->d_split[k];
    for (k = 0; k < 4u; k++) p->d_type[k] = b->d_type[k];
    p->s_fb = b->s_fb; p->s_dlevel = b->s_dlevel; p->s_rlevel = b->s_rlevel; p->s_decay = b->s_decay;
    p->s_size = b->s_size; p->s_rtone = b->s_rtone; p->s_dtone = b->s_dtone;
    p->f_cut = b->f_cut; p->f_res = b->f_res;
    e->m_dirty = 0;
}

static void sd_mod_tick(SdEngine *e)
{
    SdPatch *p = &e->patch;
    int32_t acc[SD_MDSTS], src[SD_MSRCS], m[4], semis = 0;
    uint32_t i, k, any = 0;
    for (i = 0; i < SD_MDSTS; i++) acc[i] = 0;
    e->mvx = e->mvy = 0;
    /* the sources, Q15 */
    {
        const SdVoice *v = &e->voice[e->m_last < SD_VOICES ? e->m_last : 0u];
        int32_t e1 = v->env1.lvl >> 15, e2 = v->env2.lvl >> 15;
        int32_t key = ((int32_t)e->m_note - 60) * 546;
        src[SD_MS_OFF] = 0;
        src[SD_MS_ENV1] = e1 > 32767 ? 32767 : e1;
        src[SD_MS_ENV2] = e2 > 32767 ? 32767 : e2;
        src[SD_MS_LFO1] = sd_lfo_tick(e, p, 0);
        src[SD_MS_LFO2] = sd_lfo_tick(e, p, 1);
        src[SD_MS_NAVX] = (int32_t)e->nav_x - 32768;
        src[SD_MS_NAVY] = (int32_t)e->nav_y - 32768;
        src[SD_MS_VEL] = (int32_t)e->m_vel * 258;
        src[SD_MS_KEY] = key > 32767 ? 32767 : key < -32767 ? -32767 : key;
        src[SD_MS_WHEEL] = (int32_t)(e->wheel & 127u) * 258;
        src[SD_MS_AFTER] = (int32_t)(e->after & 127u) * 258;
        src[SD_MS_RAND] = e->m_rand;
        {
            int32_t e3 = v->env3.lvl >> 15;
            src[SD_MS_ENV3] = e3 > 32767 ? 32767 : e3;
        }
        {   /* the engines as sources: each from the last tick (their parts of this tick come after) */
            uint8_t bd[SD_SEGS + 1u];
            uint32_t sc = sd_seg_bounds(&e->t[0].pat, bd), len = e->t[0].pat.len;
            int32_t dl = e->d.on ? e->d.ef[0] + 48 * 256 : 0, dt = e->d.on ? e->d.ef[3] + 48 * 256 : 0;
            int32_t ep = e->sp.on ? (e->sp.peak > 32767 ? 32767 : e->sp.peak) : 0;
            src[SD_MS_STEP] = e->t[0].seq_on && len > 1u ? (int32_t)(e->t[0].seq_shown * 65534u / (len - 1u)) - 32767 : 0;
            src[SD_MS_SEG] = e->t[0].seq_on && sc > 1u ? (int32_t)(e->t[0].seq_seg_shown * 65534u / (sc - 1u)) - 32767 : 0;
            src[SD_MS_DLOW] = dl <= 0 ? 0 : dl >= 48 * 256 ? 32767 : dl * 32767 / (48 * 256);
            src[SD_MS_DTOP] = dt <= 0 ? 0 : dt >= 48 * 256 ? 32767 : dt * 32767 / (48 * 256);
            src[SD_MS_RSTAGE] = e->r.on ? (int32_t)(e->r.stage > 5u ? 5u : e->r.stage) * 13107 - 32767 : 0;
            e->echo_env = ep > e->echo_env ? ep : e->echo_env - (e->echo_env >> 6) - 1;
            if (e->echo_env < 0) e->echo_env = 0;
            src[SD_MS_ECHO] = e->echo_env * 2 > 32767 ? 32767 : e->echo_env * 2;   /* (echoes sit at half scale) */
        }
        {                                       /* TURING: its step, glided an eighth a tick (no click) */
            int32_t d = e->tur_out - e->tur_g;
            e->tur_g += d / 8 + (d > 0) - (d < 0);
            src[SD_MS_TUR] = SD_TUR_ON(p) ? e->tur_g : 0;
        }
        e->lfo_out[0] = (int16_t)src[SD_MS_LFO1];
        e->lfo_out[1] = (int16_t)src[SD_MS_LFO2];
        e->m_trig = 0;
    }
    /* the matrix: each slot's source x amount, summed per destination (Q15: a full source at 100 % is 32767) */
    for (k = 0; k < SD_MODS; k++) {
        const SdModSlot *s = &p->mod[k];
        if (!s->src || !s->dst || !s->amt || s->src >= SD_MSRCS || s->dst >= SD_MDSTS) continue;
        acc[s->dst] += (src[s->src] * s->amt) >> 15;
        any = 1;
    }
    if (SD_TUR_ON(p) && p->tur_dst && p->tur_dst < SD_MDSTS && p->tur_amt) {   /* TURING's own DEST and AMOUNT */
        acc[p->tur_dst] += (src[SD_MS_TUR] * ((int32_t)p->tur_amt * 327)) >> 15;
        any = 1;
    }
    {                                           /* ORBIT, while A plays: the lanes, the macros' changes (gliding:
                                                   from this step's value to the next's through the step) */
        const SdOrbit *o = &e->orb;
        int32_t om[4] = {0, 0, 0, 0};
        if (o->on && e->t[0].seq_on) {
            uint32_t s = e->orb_step % SD_ORB_STEPS, nx = (e->orb_step + 1u) % SD_ORB_LEN(o);
            uint32_t span = e->orb_loop_sb * SD_ORB_LOOPS(o);
            uint32_t sh = span > 65535u ? 8u : 0u;   /* (a long step: both scaled, so the product fits) */
            int32_t f = !span || e->orb_t >= span ? 32767 :
                        (int32_t)(((e->orb_t >> sh) * 32767u) / ((span >> sh) ? (span >> sh) : 1u));   /* (a tick's
                                                   one 32-bit divide) */
            for (k = 0; k < 2u; k++) {
                int32_t v = o->lane[k][s];
                if (!o->dst[k] || o->dst[k] >= SD_MDSTS) continue;
                if (o->glide & (1u << k)) v += ((o->lane[k][nx] - v) * f) >> 15;   /* (200 x 32767: fits) */
                acc[o->dst[k]] += v * 327;
                any = 1;
            }
            for (k = 0; k < 4u; k++) {
                int32_t v = o->mac[k][s];
                if (o->glide & SD_OG_MAC) v += ((o->mac[k][nx] - v) * f) >> 15;
                om[k] = v * 331;                /* 99: 32769 */
            }
        }
        for (k = 0; k < 4u; k++) {
            int32_t v = (p->mac[k] >> 1) + om[k];
            m[k] = v < 0 ? 0 : v > 32767 ? 32767 : v;
            any |= (uint32_t)m[k];
        }
    }
    if (!any) { e->pmul = 65536u; return; }    /* nothing: the patch as it is */
    e->m_dirty = 1;                             /* (sd_mod_restore must know every field written below) */
    /* the macros: offsets around the patch as programmed (spec §8); 0 is the patch itself */
    if (m[0]) {                                 /* MUTATE: the voice's complexity */
        acc[SD_MD_XM] += sd_mw(m[0], 12000); acc[SD_MD_WARP] += sd_mw(m[0], 6000);   /* (C1 first: XM 20000, WP 13000,
                                                   FD 16000: a chain at full went half to aliasing noise) */
        acc[SD_MD_FOLD] += sd_mw(m[0], 7000); acc[SD_MD_DT] += sd_mw(m[0], 6000);
    }
    if (m[1]) {                                 /* MOVE: animation in steps on the beat (the user: "all modulation
                                                   must add something rhythmic"; stepped random over smooth sines):
                                                   the nav and the Reactor's POS jump to new places each quarter,
                                                   glided ~10 ms */
        e->move_t += SD_SUB << 8;
        if (e->move_t >= 4u * e->seq_len_q8) {
            e->move_t = 0;
            for (k = 0; k < 3u; k++) {
                e->m_seed = e->m_seed * 1103515245u + 12345u;
                e->move_tgt[k] = (int32_t)(e->m_seed >> 16) - 32768;
            }
        }
        for (k = 0; k < 3u; k++) {
            int32_t d = e->move_tgt[k] - e->move_cur[k];
            e->move_cur[k] += d / 8 + (d > 0) - (d < 0);
        }
        acc[SD_MD_X] += sd_mw(e->move_cur[0], sd_mw(m[1], 11000));
        acc[SD_MD_Y] += sd_mw(e->move_cur[1], sd_mw(m[1], 11000));
        acc[SD_MD_RPOS] += sd_mw(e->move_cur[2], sd_mw(m[1], 16384));
        acc[SD_MD_WIDTH] += sd_mw(m[1], 13000);
    }
    if (m[2] && !e->base.d_mix) {               /* BREAK on a patch without DISTROY: one of its own, the lows clean,
                                                   SOFT and HARD on the top two bands, at unity (else BREAK only
                                                   faded the voice into an empty DISTROY: -5 dB, nothing gained) */
        static const uint8_t DT[4] = {SD_DS_NONE, SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD};
        p->d_split[0] = 14170u; p->d_split[1] = 22111u; p->d_split[2] = 28436u;   /* 200 Hz, 1.2 kHz, 5 kHz */
        p->d_xover = SD_DX_CMP;
        for (k = 0; k < 4u; k++) { p->d_type[k] = DT[k]; p->d_level[k] = 32768u; }
    }
    if (m[2]) {                                 /* BREAK: destruction */
        static const int16_t BG[4] = {4000, 12000, 20000, 26000}, BA[4] = {4000, 10000, 16000, 20000};   /* the top
                                                   bands first, the lows nearly clean (the user: "the best
                                                   results with it on the top frequencies") */
        acc[SD_MD_RDRIVE] += sd_mw(m[2], 6000);   /* (12000: FUSION, its drive low, +3 dB louder) */
        acc[SD_MD_DMIX] += sd_mw(m[2], 15000);   /* (no feedback: on a patch without its own, BREAK's 1 ms loop
                                                   howled, the user: "full internal feedback" on COMET and ION) */
        for (k = 0; k < 4u; k++) {
            sd_add16(&p->d_gain[k], sd_mw(m[2], BG[k]));
            sd_add16(&p->d_alt[k], sd_mw(m[2], BA[k]));
            sd_adds16(&p->d_bias[k], (p->d_bias[k] * m[2] * 3) >> 16);   /* the bias's range, up to 2.5 x */
            p->d_level[k] = (uint16_t)(p->d_level[k] - ((uint64_t)p->d_level[k] * (uint32_t)m[2] * 5u >> 18));   /* the
                                                   gain's level back: -8.5 dB at full: BREAK destroys, not louder */
        }
    }
    if (m[3]) {                                 /* MACRO 4 */
        if (p->mac4 == SD_M4_AIR) {             /* brightness and spread */
            acc[SD_MD_RFREQ] += sd_mw(m[3], 8192);   /* +12 semitones */
            acc[SD_MD_WIDTH] += sd_mw(m[3], 15000);
            acc[SD_MD_DTAME] -= sd_mw(m[3], 15000);
            sd_add16(&p->s_rtone, sd_mw(m[3], 20000));
            sd_add16(&p->s_dtone, sd_mw(m[3], 20000));
        } else if (p->mac4 == SD_M4_DROWN) {    /* the room */
            acc[SD_MD_SDLVL] += sd_mw(m[3], 12000); acc[SD_MD_SRLVL] += sd_mw(m[3], 14000);
            acc[SD_MD_SDECAY] += sd_mw(m[3], 8000); acc[SD_MD_SFB] += sd_mw(m[3], 3000);   /* (8000 built up)*/
            sd_add16(&p->s_size, sd_mw(m[3], 20000));
        } else {                                /* BODY (the spec's MASS): weight and density */
            acc[SD_MD_RRES] += sd_mw(m[3], 4000); acc[SD_MD_DTAME] += sd_mw(m[3], 12000);   /* (RES 8000 rang)*/
            sd_add16(&p->r_low, sd_mw(m[3], 30000));
            for (k = 0; k < 4u; k++) sd_adds16(&p->d_shape[k], -sd_mw(m[3], 20000));   /* toward PHAT */
        }
    }
    /* onto the patch: a 0..65535 field moves by 2 x acc (a full source at 100 %: the whole range) */
    for (i = 1; i < SD_MDSTS; i++) {
        int32_t a = acc[i], d = 2 * a;
        if (!a) continue;
        switch (i) {
        case SD_MD_PITCH: semis = (a * 24 * 256) >> 15; break;   /* semitones x 256, +-24 */
        case SD_MD_XM: sd_add16(&p->xm, d); break;
        case SD_MD_WARP: sd_add16(&p->warp, d); break;
        case SD_MD_FOLD: sd_add16(&p->fold, d); break;
        case SD_MD_DT: sd_add16(&p->dt, d); break;
        case SD_MD_X: e->mvx = d; break;
        case SD_MD_Y: e->mvy = d; break;
        case SD_MD_WIDTH: sd_add16(&p->width, d); break;
        case SD_MD_RPOS: {
            int32_t q = (int32_t)p->r_pos + ((a * 1535) >> 15);
            p->r_pos = (uint16_t)(q < 0 ? 0 : q > 1535 ? 1535 : q);
            break;
        }
        case SD_MD_RFREQ: {
            int32_t f = p->r_freq + ((a * 48 * 256) >> 15);
            p->r_freq = (int16_t)(f < -48 * 256 ? -48 * 256 : f > 48 * 256 ? 48 * 256 : f);
            break;
        }
        case SD_MD_RRES: sd_add16(&p->r_res, d); break;
        case SD_MD_RDRIVE: sd_add16(&p->r_drive, d); break;
        case SD_MD_RMIX: sd_add16(&p->r_mix, d); break;
        case SD_MD_DGAIN: for (k = 0; k < 4u; k++) sd_add16(&p->d_gain[k], d); break;
        case SD_MD_DBIAS: for (k = 0; k < 4u; k++) sd_adds16(&p->d_bias[k], a); break;
        case SD_MD_DFB: sd_add16(&p->d_fb, d); break;
        case SD_MD_DTAME: sd_add16(&p->d_tame, d); break;
        case SD_MD_DMIX: sd_add16(&p->d_mix, d); break;
        case SD_MD_SFB: sd_add16(&p->s_fb, d); break;
        case SD_MD_SDLVL: sd_add16(&p->s_dlevel, d); break;
        case SD_MD_SRLVL: sd_add16(&p->s_rlevel, d); break;
        case SD_MD_SDECAY: sd_add16(&p->s_decay, d); break;
        case SD_MD_DHIGH: sd_add16(&p->d_gain[3], d); break;
        case SD_MD_DSPLIT: {                    /* D SPLIT: the upper crossover, +-24 semitones, between its neighbours */
            int32_t v = (int32_t)p->d_split[2] + ((a * 24 * 256) >> 15), lo = p->d_split[1] + 256, hi = SD_R_HI * 256 - 256;
            p->d_split[2] = (uint16_t)(v < lo ? lo : v > hi ? hi : v);
            break;
        }
        case SD_MD_FCUT: sd_add16(&p->f_cut, (int32_t)(((int64_t)a * 34492) >> 15)); break;   /* +-60 semitones of the 114 at 100 % */
        default: sd_add16(&p->f_res, d); break;   /* SD_MD_FRES */
        }
    }
    if (semis) {                                /* 2^(semis / 12): octaves, semitones, cents */
        int32_t n = semis >> 8, c = ((semis & 255) * 100) >> 8, o = n >= 0 ? n / 12 : -((11 - n) / 12);
        uint32_t r = (uint32_t)(((uint64_t)SD_SEMI[n - o * 12] * SD_CENTS[99 + c]) >> 16);   /* Q16, 1 .. 2.1 */
        e->pmul = o >= 0 ? r << o : r >> -o;
    } else {
        e->pmul = 65536u;
    }
}

/* The arpeggiator. A key adds its note to the list (in the order played), a lift takes it out; with HOLD the list
 * stays once every key is up, and the next key starts a new one. A run: the notes sorted (ORDER: as played), over
 * 1..4 octaves, UP, DOWN, UP-DOWN (the ends once), RND. Each step DIV long on the tempo, its gate GATE of it. */
static void sd_arp_key(SdEngine *e, uint32_t note, uint32_t vel)
{
    uint32_t i, j;
    if (vel) {
        if (!e->arp_keys && e->arp_hold) e->arp_n = 0;   /* HOLD: a new hand, a new list */
        e->arp_keys = (uint8_t)(e->arp_keys < 255u ? e->arp_keys + 1u : 255u);
        for (i = 0; i < e->arp_n; i++)
            if (e->arp_note[i] == note) return; /* (already in it) */
        if (!e->arp_n) {                        /* the first note: the run starts now, on its first step */
            e->arp_i = 0; e->arp_dir = 0;
            e->arp_pos_q8 = 0xFFFFFFFFu;
        }
        if (e->arp_n == 8u) {                   /* full: the oldest goes */
            for (i = 0; i + 1u < 8u; i++) { e->arp_note[i] = e->arp_note[i + 1u]; e->arp_vel[i] = e->arp_vel[i + 1u]; }
            e->arp_n = 7;
        }
        e->arp_note[e->arp_n] = (uint8_t)note;
        e->arp_vel[e->arp_n] = (uint8_t)vel;
        e->arp_n++;
    } else {
        if (e->arp_keys) e->arp_keys--;
        sd_voice_off(e, note, SD_SRC_HAND);   /* (a key held as ARP came on: its
                                                   note played straight, so it lifts straight) */
        if (e->arp_hold) return;
        for (i = 0; i < e->arp_n; i++)
            if (e->arp_note[i] == note) {
                for (j = i; j + 1u < e->arp_n; j++) { e->arp_note[j] = e->arp_note[j + 1u]; e->arp_vel[j] = e->arp_vel[j + 1u]; }
                e->arp_n--;
                break;
            }
    }
}

static void sd_arp_tick(SdEngine *e)            /* once a sub-block */
{
    uint32_t n = e->arp_n, oct = e->arp_oct < 1u ? 1u : e->arp_oct > 4u ? 4u : e->arp_oct, len, k, i;
    uint8_t s[8], v[8];
    if (!e->arp_on) {
        if (e->arp_was) {                       /* off: its note off, the list gone */
            if (e->arp_playing) sd_voice_off(e, e->arp_cur, SD_SRC_ARP);
            e->arp_playing = 0; e->arp_n = 0; e->arp_keys = 0; e->arp_was = 0;
        }
        return;
    }
    e->arp_was = 1;
    if (!n) {
        if (e->arp_playing) { sd_voice_off(e, e->arp_cur, SD_SRC_ARP); e->arp_playing = 0; }
        return;
    }
    {
        uint32_t d = e->arp_div < SD_SDIVS ? e->arp_div : 2u, g = e->arp_gate < 1u ? 1u : e->arp_gate > 100u ? 100u : e->arp_gate;
        e->arp_step_q8 = e->seq_len_q8 * SD_SDIV_NUM[d] / SD_SDIV_DEN[d];
        if (e->arp_pos_q8 != 0xFFFFFFFFu && e->arp_playing && e->arp_pos_q8 >= (e->arp_step_q8 >> 7) * g / 100u << 7) {
            sd_voice_off(e, e->arp_cur, SD_SRC_ARP);        /* the gate */
            e->arp_playing = 0;
        }
        if (e->arp_pos_q8 != 0xFFFFFFFFu && e->arp_pos_q8 < e->arp_step_q8) { e->arp_pos_q8 += SD_SUB << 8; return; }
        e->arp_pos_q8 = e->arp_pos_q8 == 0xFFFFFFFFu ? 0u : e->arp_pos_q8 - e->arp_step_q8;
        if (e->arp_pos_q8 >= e->arp_step_q8) e->arp_pos_q8 = 0;   /* (the tempo jumped) */
    }
    for (i = 0; i < n; i++) { s[i] = e->arp_note[i]; v[i] = e->arp_vel[i]; }
    if (e->arp_mode != SD_AM_ORDER)             /* sorted, low to high (8 at most: an insertion sort) */
        for (i = 1; i < n; i++)
            for (k = i; k > 0u && s[k - 1u] > s[k]; k--) {
                uint8_t a = s[k]; s[k] = s[k - 1u]; s[k - 1u] = a;
                a = v[k]; v[k] = v[k - 1u]; v[k - 1u] = a;
            }
    len = n * oct;                              /* the run's notes: index j -> s[j % n] + 12 (j / n) */
    if (e->arp_mode == SD_AM_RND) {
        e->arp_rng = e->arp_rng * 1103515245u + 12345u;
        k = (e->arp_rng >> 16) % len;
    } else if (e->arp_mode == SD_AM_UPDN && len > 1u) {   /* 0 1 2 3 2 1: a cycle of 2 len - 2 */
        uint32_t c = 2u * len - 2u, j = e->arp_i % c;
        k = j < len ? j : c - j;
        e->arp_i = (uint8_t)((j + 1u) % c);
    } else {
        k = e->arp_i % len;
        e->arp_i = (uint8_t)((k + 1u) % len);
        if (e->arp_mode == SD_AM_DOWN) k = len - 1u - k;
    }
    if (e->arp_playing) sd_voice_off(e, e->arp_cur, SD_SRC_ARP);
    {
        uint32_t note = s[k % n] + 12u * (k / n);
        e->arp_cur = (uint8_t)(note > 127u ? 127u : note);
        sd_voice_on(e, e->arp_cur, v[k % n], SD_SRC_ARP);
        e->arp_playing = 1;
    }
    e->arp_pos_q8 += SD_SUB << 8;
}

/* The filter: each voice's cutoff (MIDI x 256) from CUTOFF, KEY, VEL and ENV3, then two TPT SVF stages' coefficients
 * as the Reactor's (SD_RG's warp; SD_FK's damping by RES). LP12: one stage, k 1.41 (flat) .. 0.1 (+23 dB) by RES.
 * LP24: a Butterworth pair, k 1.85 and 0.77 .. 0.054 (+25 dB). A silent voice's banks have their states cleared. */
static void sd_filter_params(SdEngine *e)
{
    const SdPatch *p = &e->patch;
    uint32_t i, b, two = p->f_type == SD_FT_LP24, ri = p->f_res >> 9, rf = p->f_res & 511u;
    int32_t kf = (int32_t)SD_FK[ri] + ((((int32_t)SD_FK[ri + 1u] - (int32_t)SD_FK[ri]) * (int32_t)rf) >> 9);   /* 1 .. 0.07 */
    int32_t kb = (int32_t)(((int64_t)kf * (two ? 12839501 : 23726566)) >> 24);   /* 0.765 / 1.414 at RES 0 */
    for (i = 0; i < e->nvoices; i++) {
        SdVoice *v = &e->voice[i];
        int32_t c, e3, g, d, s, fr, kn[2];
        uint32_t st;
        if (sd_voice_silent(v)) {
            for (b = 0; b < SD_BANKS; b++)
                if (e->bank[b].voice == i) e->bank[b].fic1[0] = e->bank[b].fic1[1] = e->bank[b].fic2[0] = e->bank[b].fic2[1] = 0;
            continue;
        }
        e3 = v->env3.lvl >> 15;
        e3 = e3 > 32767 ? 32767 : e3;
        c = 16 * 256 + (int32_t)(((uint32_t)p->f_cut * 114u) >> 8)          /* 0..65535 -> 16 .. 130 x 256 */
            + ((((int32_t)v->note - 60) * 256 * (int32_t)(p->f_key > 100u ? 100u : p->f_key)) / 100)
            + ((((e3 * p->f_env) >> 15) * 60 * 256) >> 15)
            + (int32_t)((uint32_t)v->vel * (p->f_vel > 100u ? 100u : p->f_vel) * 6144u / 12700u);   /* +24 at 127 */
        c = c < SD_R_LO * 256 ? SD_R_LO * 256 : c > SD_R_HI * 256 ? SD_R_HI * 256 : c;
        s = (c >> 8) - SD_R_LO; fr = c & 255;
        st = (uint32_t)(s >= SD_R_HI - SD_R_LO ? SD_R_HI - SD_R_LO - 1 : s);
        if (s >= SD_R_HI - SD_R_LO) fr = 255;
        g = (int32_t)SD_RG[st] + (int32_t)((((int64_t)SD_RG[st + 1u] - SD_RG[st]) * fr) >> 8);   /* Q24 */
        kn[0] = two ? 31004295 : kb;            /* 1.848 (Q24): the pair's first stage */
        kn[1] = kb;
        for (b = 0; b < 1u + two; b++) {
            d = (1 << 24) + (int32_t)(((int64_t)g * (g + kn[b])) >> 24);
            v->fa1[b] = sd_recip30(d);
            v->fa2[b] = (int32_t)(((int64_t)g * v->fa1[b]) >> 24);
            v->fa3[b] = (int32_t)(((int64_t)g * v->fa2[b]) >> 24);
        }
    }
}

static void sd_control_tick(SdEngine *e)        /* once a sub-block, before its frames */
{
    int32_t w[SD_BANKS];
    uint32_t b, i, k;
    const SdPatch *p;
    {   /* a new whole patch: taken once (a copy is ~400 bytes, a byte at a time with -fno-builtin: C1 measured ~0.7 %
           of the chip for one a tick); otherwise only what the modulation wrote goes back to the base */
        uint32_t sq = e->patch_seq;
        if (!(sq & 1u) && sq != e->seq_taken) {
            e->seq_taken = sq;
            e->base = e->patch_in;
            e->patch = e->base;
            e->m_dirty = 0;
        } else if (e->m_dirty) {
            sd_mod_restore(e);
        }
    }
    p = &e->patch;
    sd_plan(e);                                 /* who may hold how many voices, now */
    while (e->q_r != e->q_w) {                  /* notes from the keys and MIDI */
        SdNoteEv ev = e->q[e->q_r % SD_QLEN];
        e->q_r++;
        sd_hand_key(e, ev.note, ev.vel);
        if (e->arp_on) sd_arp_key(e, ev.note, ev.vel);
        else if (ev.vel) sd_voice_on(e, ev.note, ev.vel, SD_SRC_HAND);
        else sd_hand_off(e, ev.note);
    }
    if (e->mode != e->mode_on) {                /* a new mode: release everything, switch once it is quiet */
        for (i = 0; i < SD_VOICES; i++) e->voice[i].gate = 0;
        if (sd_engine_silent(e) && e->mode < SD_MODES) { e->mode_on = e->mode; sd_map_banks(e); }
    }
    sd_seq_tick(e);
    sd_arp_tick(e);
    for (i = 0; i < SD_VOICES; i++) {
        SdVoice *v = &e->voice[i];
        sd_env_tick(&v->env1, v->gate, p->e1_a, p->e1_d, p->e1_s, p->e1_r);
        sd_env_tick(&v->env2, v->gate, p->e2_a, p->e2_d, p->e2_s, p->e2_r);
        sd_env_tick(&v->env3, v->gate, p->e3_a, p->e3_d, p->e3_s, p->e3_r);
        v->amp = v->amp_next;
        v->amp_next = ((v->env1.lvl >> 15) * (int32_t)(v->vel * 128u)) >> 15;   /* 32767 * 16256: fits; half scale */
    }
    for (i = 0; i < SD_VOICES; i++) {           /* SLIDE: the increment an eighth of the way a tick (~6 ms: most
                                                   of an octave in ~30 ms, a 303's slide) */
        SdVoice *v = &e->voice[i];
        if (v->glide) {
            int32_t d = (int32_t)(v->inc_tgt - v->inc);
            if (d > -16 && d < 16) { v->inc = v->inc_tgt; v->glide = 0; }
            else v->inc += (uint32_t)(d / 8 + (d > 0 ? 1 : -1));
        }
    }
    sd_mod_tick(e);
    if (p->fx_off) {                            /* EDIT's OFF: after the matrix and the macros, so none reopens it */
        SdPatch *q = &e->patch;
        if (q->fx_off & SD_FX_REACTOR) q->r_mix = 0;
        if (q->fx_off & SD_FX_DISTROY) q->d_mix = 0;
        if (q->fx_off & SD_FX_SPACE) q->s_dlevel = q->s_rlevel = 0;
        if (q->fx_off & SD_FX_FILTER) q->f_type = SD_FT_OFF;
    }
    if (p->f_type) sd_filter_params(e);
    sd_nav(e, w);
    if (p->r_mix) {
        sd_reactor_params(e);
        e->r.rest = 0;
        if (sd_engine_silent(e) && e->r.model > SD_RM_VBP) {   /* the delay models: rest once their output has been
                                                                  under 4 LSB for 100 sub-blocks (73 ms) */
            e->r.quiet = e->r.peak < 4 ? e->r.quiet + 1u : 0u;
            if (e->r.quiet >= 100u) {
                if (e->r.quiet == 100u) {       /* once: the lines and states to zero */
                    for (k = 0; k < 4u * SD_DL; k++) e->r.dl[k / SD_DL][k % SD_DL] = 0;
                    for (k = 0; k < 8u; k++) e->r.ax1[k] = e->r.ay1[k] = 0;
                    for (k = 0; k < 4u; k++) e->r.prev[k] = 0;
                }
                e->r.rest = 1;
                e->r.quiet = 101u;
            }
        } else if (!sd_engine_silent(e)) {
            e->r.quiet = 0;
        }
        e->r.peak = 0;
        if (sd_engine_silent(e) && e->r.model <= SD_RM_VBP) {   /* nothing in: rest once every integrator is near
                                                                  zero */
            int32_t big = 0;
            for (k = 0; k < 8u; k++) {
                int32_t a1 = e->r.ic1[k >> 2][k & 3u], a2 = e->r.ic2[k >> 2][k & 3u];
                big |= (a1 < 0 ? -a1 : a1) | (a2 < 0 ? -a2 : a2);
            }
            e->d.fpk = 0;
            if (big < 64) {                     /* Q19: under 1/8 LSB of the output */
                for (k = 0; k < 8u; k++) e->r.ic1[k >> 2][k & 3u] = e->r.ic2[k >> 2][k & 3u] = 0;
                e->r.rest = 1;
            }
        }
    } else {
        e->r.on = 0;                            /* off: its states keep, its coefficients refresh when it returns */
    }
    if (p->d_mix) {
        sd_distroy_params(e);
        e->d.rest = 0;
        if (sd_engine_silent(e) && (!e->r.on || e->r.rest)) {   /* nothing comes in: rest once the blockers and
                                                                   the crossover have settled */
            int32_t big = 0;
            for (k = 0; k < 2u; k++) big |= sd_iabs(e->d.dy[k]) >> 8;
            for (k = 0; k < 22u; k++) big |= sd_iabs(e->d.ic1[k / 11u][k % 11u]) | sd_iabs(e->d.ic2[k / 11u][k % 11u]);
            big |= e->d.fpk | (sd_iabs(e->d.tic2[0]) >> 4) | (sd_iabs(e->d.tic2[1]) >> 4);   /* D3: the loop, TAME */
            if (big < 64) {
                for (k = 0; k < 2u; k++) e->d.dy[k] = e->d.dx[k] = 0;
                for (k = 0; k < 22u; k++) e->d.ic1[k / 11u][k % 11u] = e->d.ic2[k / 11u][k % 11u] = 0;
                for (k = 0; k < 2u; k++) e->d.tic1[k] = e->d.tic2[k] = e->d.flp[k] = 0;
                e->d.rest = 1;
            }
        }
    } else {
        e->d.on = 0;
    }
    if (e->sm && (p->s_dlevel || p->s_rlevel)) {
        sd_space_params(e);
        e->sp.rest = 0;
        if (sd_engine_silent(e) && (!e->r.on || e->r.rest) && (!e->d.on || e->d.rest)) {   /* nothing comes in:
                                                   rest once the tails have stayed under the floor for 2 s */
            e->sp.quiet = e->sp.peak < 4 ? e->sp.quiet + 1u : 0u;
            if (e->sp.quiet >= 2756u) { e->sp.rest = 1; e->sp.quiet = 2756u; }
        } else {
            e->sp.quiet = 0;
        }
        e->sp.peak = 0;
    } else {
        e->sp.on = 0;
    }
    for (b = 0; b < SD_BANKS; b++) {
        SdBank *bk = &e->bank[b];
        const SdVoice *v = &e->voice[bk->voice];
        int32_t g = (v->amp_next * w[b]) >> 15, wd = (int32_t)(p->width >> 1);   /* 16256 * 32767: fits */
        bk->gl = bk->gl_next;
        bk->gr = bk->gr_next;
        /* the balance: A and D lean left, B and C right, by WIDTH; at 0 both sides carry everything */
        bk->gl_next = b == 1u || b == 2u ? (g * (32768 - wd)) >> 15 : g;   /* 16256 * 32768: fits */
        bk->gr_next = b == 0u || b == 3u ? (g * (32768 - wd)) >> 15 : g;
        if (!v->amp && !v->amp_next) continue; /* silent: the render only moves the phases */
        sd_bank_params(bk, p, &p->bank[bk->src], v, e->track, e->pmul);
        for (k = 0; k < SD_OPS; k++)
            for (i = 0; i < SD_SUB; i++) bk->bus[k][i] = 0;
        for (i = 0; i < SD_SUB; i++) bk->mix[i] = 0;
    }
}

/* One oscillator, m frames: PM from in, its output * level added to dst. SH, SQ, FD: constants, so each variant
 * compiles without the stages it does not have. Every stage keeps s in -32767..32768 (no clamps: each next product
 * is written for 32768), and the loop walks pointers: the costliest variant is what the field's worst case pays,
 * 16 times a frame. */
SD_INLINE void sd_osc_run(SdOsc *o, const SdShape *sh, const int32_t *in, int32_t *dst, uint32_t m, int32_t lvl,
                          int32_t lstep, int32_t fg, int32_t fstep, const int SH, const int SQ, const int FD)
{
    const int32_t *end = in + m;
    uint32_t ph = o->ph, inc = o->inc;
    while (in < end) {
        uint32_t p;
        int32_t s;
        ph += inc;
        p = ph + ((uint32_t)*in++ << SD_PM_SHIFT);              /* wraps: the phase is modular */
        if (SH == SD_SH_SINE) {
            s = sd_lut(p + 0x40000000u);                        /* the cosine: warp's stages start from it */
        } else if (SH == SD_SH_BLEND) {                         /* the cosine, toward the triangle */
            int32_t x = (int32_t)(p - 0x80000000u), c = sd_lut(p + 0x40000000u);
            int32_t tr = (int32_t)((uint32_t)(x ^ (x >> 31)) >> 15) - 32767;   /* 4 |p - 1/2| - 1: -32767..32768 */
            s = c + (((tr - c) * (int32_t)sh->t) >> 15);        /* between c and tr; 65535 * 32767 fits */
        } else {                                                /* the triangle, skewed: the saw at d -> 0 */
            uint32_t q = p < sh->d ? (uint32_t)(((uint64_t)p * sh->k1) >> 16)   /* < 2^31 */
                                   : 0x80000000u + (uint32_t)(((uint64_t)(p - sh->d) * sh->k2) >> 16);
            int32_t x = (int32_t)(q - 0x80000000u);
            s = (int32_t)((uint32_t)(x ^ (x >> 31)) >> 15) - 32767;    /* 4 |q - 1/2| - 1, no table: -32767..32768 */
        }
        if (SQ)                                 /* cubic(G s) / cubic(G): G s in Q23, 32768 * 3072 fits; |cubic(Gs)|
                                                   <= cubic(G), so the Q23 * Q7 product <= 2^30 and s <= 32768 */
            s = (sd_cubic(s * (int32_t)sh->sq_g) * (int32_t)sh->sq_norm) >> 15;
        if (FD) {                               /* the triangle fold, period 4 (2^17): -32767..32768 */
            int32_t tr = (((s * fg) >> 12) + 32768) & 0x1FFFF, y = tr - 65536;   /* 32768 * 40960 fits */
            s = 32768 - (y < 0 ? -y : y);
            fg += fstep;
        }
        *dst++ += (s * lvl) >> 15;              /* 32768 * 32767: fits */
        lvl += lstep;
    }
    o->ph = ph;
}

#define SD_RUN(SH, SQ, FD) sd_osc_run(o, sh, in, dst, m, lvl, lstep, fg, fstep, SH, SQ, FD)

/* A bank, frames pos..pos+m of the sub-block, into its mix: oscillators 1 -> 4, left to right. */
static void sd_bank_run(SdBank *bk, uint32_t pos, uint32_t m)
{
    const SdShape *sh = &bk->sh;
    uint32_t k, variant = (uint32_t)sh->shape << 2 | (uint32_t)sh->sq << 1 | sh->fold;
    int32_t fstep = (sh->fg_next - sh->fg) >> SD_SUB_SHIFT, fg = sh->fg + fstep * (int32_t)pos;
    for (k = 0; k < SD_OPS; k++) {
        SdOsc *o = &bk->osc[k];
        int32_t lstep, lvl, *dst;
        const int32_t *in = bk->bus[k] + pos;
        if (!o->lvl && !o->lvl_next) {          /* silent: costs nothing, the phase stays continuous */
            o->ph += o->inc * m;
            continue;
        }
        lstep = (o->lvl_next - o->lvl) >> SD_SUB_SHIFT;
        lvl = o->lvl + lstep * (int32_t)pos;
        dst = (o->tgt == SD_CARRIER ? bk->mix : bk->bus[o->tgt]) + pos;
        switch (variant) {
        case 0: SD_RUN(SD_SH_SINE, 0, 0); break;
        case 1: SD_RUN(SD_SH_SINE, 0, 1); break;
        case 4: SD_RUN(SD_SH_BLEND, 0, 0); break;
        case 5: SD_RUN(SD_SH_BLEND, 0, 1); break;
        case 8: SD_RUN(SD_SH_TRI, 0, 0); break;
        case 9: SD_RUN(SD_SH_TRI, 0, 1); break;
        case 10: SD_RUN(SD_SH_TRI, 1, 0); break;
        default: SD_RUN(SD_SH_TRI, 1, 1); break;
        }
    }
}

/* ---- the Reactor: audio. in: the voice (stereo, m frames); out: added to. */
/* The loops' saturation: exactly linear up to a knee, then a cubic shoulder to a ceiling; on the magnitude, the
 * sign put back, so it adds no bias (a bias recirculating in a ringing filter damps it, or holds it at a DC) */
SD_INLINE int32_t sd_sat19(int32_t v)            /* Q19: linear to 2 x full scale, a ceiling at 6 x */
{
    int32_t a = v < 0 ? -v : v, e;
    if (a <= 2 << 19) return v;
    e = a - (2 << 19);                          /* past the knee, of a 4 FS shoulder: e / (1.5 x 4 FS) in Q23 */
    e = e > (6 << 20) ? 6 << 20 : e;            /* (to keep e x 683 in range: past 1.5 x the shoulder it is flat) */
    a = (2 << 19) + ((sd_cubic((int32_t)(((int64_t)e * 683) >> 8)) * 3) >> 3);   /* x 1.5 x 4 FS / 2^23 = 3/8 */
    return v < 0 ? -a : a;
}

SD_INLINE int32_t sd_sat13(int32_t w)            /* KP's line, Q13: linear to 1.5 x full scale, a ceiling at 3 x */
{
    int32_t a = w < 0 ? -w : w, e;
    if (a <= 12288) return w;
    e = a - 12288;                              /* of a 1.5 FS shoulder: e / (1.5 x 1.5 FS) in Q23 = e x 455 */
    e = e > 18432 ? 18432 : e;
    a = 12288 + ((sd_cubic(e * 455) * 9) >> 12);   /* x 2.25 FS / 2^23 = 9 / 4096: at most 24576 */
    return w < 0 ? -a : a;
}

SD_INLINE int32_t sd_svf(int32_t x, int32_t *ic1, int32_t *ic2, int32_t a1, int32_t a2, int32_t a3)
{                                               /* one TPT state-variable filter step: its band-pass, Q19 */
    int32_t v3 = x - *ic2;
    int32_t v1 = sd_sat19((int32_t)(((int64_t)a1 * *ic1 + (int64_t)a2 * v3) >> 30));   /* saturates in the loop */
    int32_t v2 = *ic2 + (int32_t)(((int64_t)a2 * *ic1 + (int64_t)a3 * v3) >> 30);
    *ic1 = 2 * v1 - *ic1;                       /* |v1| <= 6 FS (Q19, 3.1e6); |v2| <= the input / k: fits */
    *ic2 = 2 * v2 - *ic2;
    return v1;
}

SD_INLINE int32_t sd_rclip(int32_t y)           /* 1.5 cubic(y / 1.5), Q15: clean while small, flat at full scale */
{
    return (sd_cubic(y * 171) * 3) >> 9;        /* |y| <= 2^19: * 171 fits; sd_cubic clamps */
}

SD_INLINE int32_t sd_dl_read(const int16_t *dl, uint32_t wp, int32_t dq)   /* dq samples back (Q8), linear */
{
    uint32_t i = (uint32_t)dq >> 8;
    int32_t a = dl[(wp - i) & (SD_DL - 1u)], b = dl[(wp - i - 1u) & (SD_DL - 1u)];
    return a + (((b - a) * (dq & 255)) >> 8);
}

/* pos: where in the sub-block these m frames start (the delays' ramps) */
/* The Reactor's frames, MODEL and FILL constants: each variant compiles without the others' branches (C1: they
 * were tested every sample) */
SD_INLINE void sd_reactor_loop(SdReactor *r, const int32_t *in, int32_t *out, uint32_t m, int32_t *d, const int32_t *ds,
                             const int MODEL, const int FILL)
{
    uint32_t i, k;
    int32_t drive = r->drive, peak = r->peak, dg = r->dg, wg = r->wg, wg2 = r->wg >> 2, lowc = r->lowc;
    int32_t lz0 = r->lowz[0], lz1 = r->lowz[1];
    uint32_t wp = r->wp;
    for (i = 0; i < m; i++) {
        int32_t x = (in[2u * i] + in[2u * i + 1u]) >> 1, wl = 0, wr = 0;   /* mono */
        x = (int32_t)(((int64_t)x * drive) >> 14);                       /* x DRIVE, Q15 */
        x = x > (1 << 18) ? 1 << 18 : x < -(1 << 18) ? -(1 << 18) : x;      /* 8 x full scale at most.
           (clamped before any shift: -(1 << 22) as a constant is a ROM address to the build's check, RULES.md) */
        if (MODEL <= SD_RM_VBP) {
            int32_t x19 = x * 16;
            for (k = 0; k < 4u; k++) {
                int32_t y = sd_svf(x19, &r->ic1[0][k], &r->ic2[0][k], r->a1[k], r->a2[k], r->a3[k]), v;
                if (MODEL != SD_RM_BP6) y = (int32_t)(((int64_t)y * r->kn[k]) >> 24);   /* unit peak */
                if (MODEL == SD_RM_BP12) y = sd_svf(y, &r->ic1[1][k], &r->ic2[1][k], r->a1[k], r->a2[k], r->a3[k]);
                y = (int32_t)(((int64_t)(y >> 4) * r->lvl[k]) >> 14);       /* Q15, x level (<= 2) */
                v = sd_rclip(y);
                wl += (v * r->pl[k]) >> 15;
                wr += (v * r->pr[k]) >> 15;
            }
        } else {
            int32_t x13 = x >> 2, s, wet;              /* the lines hold Q13: 4 x full scale */
            x13 = x13 > 32767 ? 32767 : x13 < -32767 ? -32767 : x13;
            if (MODEL == SD_RM_KP) {         /* four combs in parallel, each panned */
                for (k = 0; k < 4u; k++) {
                    int32_t z = sd_dl_read(r->dl[k], wp, d[k]), avg = sd_tz(z + r->prev[k], 1), w, v, t;
                    r->prev[k] = z;
                    t = avg * r->fbk;           /* toward zero, not down: a shift's floor would hold the line at
                                                   -1 for ever, a DC that never rings out */
                    w = x13 + (t < 0 ? -((-t) >> 15) : t >> 15);
                    w = sd_sat13(w);            /* the line rings into a soft ceiling, never wraps */
                    r->dl[k][wp] = (int16_t)w;
                    v = sd_rclip((int32_t)(((int64_t)w * 4 * r->lvl[k]) >> 14));
                    wl += (v * r->pl[k]) >> 15;
                    wr += (v * r->pr[k]) >> 15;
                }
            } else {
                s = x13;
                if (MODEL == SD_RM_AP1) {    /* four Schroeder allpasses in series, g 0.6 */
                    for (k = 0; k < 4u; k++) {
                        int32_t z = sd_dl_read(r->dl[k], wp, d[k]), v = s + ((z * 19661) >> 15);
                        v = v > 32767 ? 32767 : v < -32767 ? -32767 : v;
                        r->dl[k][wp] = (int16_t)v;
                        s = z - ((v * 19661) >> 15);
                    }
                } else {                        /* AP2: eight first-order allpasses in series */
                    for (k = 0; k < 8u; k++) {
                        int64_t t = (int64_t)r->apa[k >> 1] * (s - r->ay1[k]);
                        int32_t y = (int32_t)(t < 0 ? -((-t) >> 30) : t >> 30) + r->ax1[k];   /* toward zero: a
                                                   floor would hold a stage at a small DC for ever */
                        r->ax1[k] = s;
                        r->ay1[k] = y;
                        s = y;
                    }
                }
                wet = s * 4;                    /* Q15 */
                wl = sd_rclip((int32_t)(((int64_t)(wet - ((x * r->dry) >> 15)) * r->aplvl) >> 14));
                wr = sd_rclip((int32_t)(((int64_t)(wet + ((x * r->dry) >> 15)) * r->aplvl) >> 14));
            }
            wp = (wp + 1u) & (SD_DL - 1u);
            for (k = 0; k < 4u; k++) d[k] += ds[k];
        }
        {
            int32_t al = wl < 0 ? -wl : wl, ar = wr < 0 ? -wr : wr;
            if (al > peak) peak = al;
            if (ar > peak) peak = ar;
        }
        if (FILL) {                   /* R4, MIX to the middle: the dry whole, the wet on top (the lows
                                                   need nothing); 32-bit: |wet| <= 2^17 x 2^13 */
            out[2u * i] += in[2u * i] + ((wl * wg2) >> 13);
            out[2u * i + 1u] += in[2u * i + 1u] + ((wr * wg2) >> 13);
        } else {                                /* past it: dry x dg + the lows x (1 - dg) + wet x wg, 64-bit (wet
                                                   and dry past 2 x full scale apart) */
            int32_t xl = in[2u * i], xr = in[2u * i + 1u], ll = 0, lr = 0;
            if (lowc) {
                lz0 += ((xl - lz0) * lowc) >> 15;   /* |x - z| <= 2^18 x lowc <= 2^11: fits */
                lz1 += ((xr - lz1) * lowc) >> 15;
                ll = lz0;
                lr = lz1;
            }
            out[2u * i] += (int32_t)(((int64_t)xl * dg + (int64_t)ll * (32768 - dg) + (int64_t)wl * wg) >> 15);
            out[2u * i + 1u] += (int32_t)(((int64_t)xr * dg + (int64_t)lr * (32768 - dg) + (int64_t)wr * wg) >> 15);
        }
    }
    r->peak = peak;
    r->lowz[0] = lz0;
    r->lowz[1] = lz1;
    r->wp = wp;
}

#define SD_RLOOP(M) (fill ? sd_reactor_loop(r, in, out, m, d, ds, M, 1) : sd_reactor_loop(r, in, out, m, d, ds, M, 0))

static void sd_reactor_run(SdEngine *e, const int32_t *in, int32_t *out, uint32_t m, uint32_t pos)
{
    SdReactor *r = &e->r;
    uint32_t k;
    int32_t d[4], ds[4], fill = r->dg >= 32767;  /* MIX to the middle: the dry whole, the wet on top */
    for (k = 0; k < 4u; k++) {
        ds[k] = (r->dnext[k] - r->dq[k]) >> SD_SUB_SHIFT;
        d[k] = r->dq[k] + ds[k] * (int32_t)pos;
    }
    switch (r->model) {
    case SD_RM_BP6: SD_RLOOP(SD_RM_BP6); break;
    case SD_RM_BP12: SD_RLOOP(SD_RM_BP12); break;
    case SD_RM_VBP: SD_RLOOP(SD_RM_VBP); break;
    case SD_RM_KP: SD_RLOOP(SD_RM_KP); break;
    case SD_RM_AP1: SD_RLOOP(SD_RM_AP1); break;
    default: SD_RLOOP(SD_RM_AP2); break;
    }
}

/* ---- DISTROY: audio */
SD_INLINE int32_t sd_lp(int32_t x, int32_t *ic1, int32_t *ic2, int32_t a1, int32_t a2, int32_t a3)
{                                               /* a TPT SVF's lowpass, Q19, no saturation (k = 1.414: no ring) */
    int32_t v3 = x - *ic2;
    int32_t v1 = (int32_t)(((int64_t)a1 * *ic1 + (int64_t)a2 * v3) >> 30);
    int32_t v2 = *ic2 + (int32_t)(((int64_t)a2 * *ic1 + (int64_t)a3 * v3) >> 30);
    *ic1 = 2 * v1 - *ic1;
    *ic2 = 2 * v2 - *ic2;
    return v2;
}

/* a TPT SVF (k = 1.414, Butterworth), all its outputs: lowpass, band, highpass (x - k band - low), Q19 */
SD_INLINE void sd_svf3(int32_t x, int32_t *ic1, int32_t *ic2, int32_t a1, int32_t a2, int32_t a3, int32_t *lp,
                       int32_t *bp, int32_t *hp)
{
    int32_t v3 = x - *ic2;
    int32_t v1 = (int32_t)(((int64_t)a1 * *ic1 + (int64_t)a2 * v3) >> 30);
    int32_t v2 = *ic2 + (int32_t)(((int64_t)a2 * *ic1 + (int64_t)a3 * v3) >> 30);
    *ic1 = 2 * v1 - *ic1;
    *ic2 = 2 * v2 - *ic2;
    *lp = v2;
    *bp = v1;
    *hp = x - (int32_t)(((int64_t)v1 * 23726566) >> 24) - v2;
}

/* LR4 at one split: low = the lowpass twice, high = the highpass twice (in phase: they sum to an allpass) */
SD_INLINE void sd_lr4(int32_t x, int32_t *ic1, int32_t *ic2, int32_t a1, int32_t a2, int32_t a3, int32_t *lo,
                      int32_t *hi)
{
    int32_t l, b, h, l2, b2, h2;
    sd_svf3(x, &ic1[0], &ic2[0], a1, a2, a3, &l, &b, &h);
    sd_svf3(l, &ic1[1], &ic2[1], a1, a2, a3, lo, &b2, &h2);
    sd_svf3(h, &ic1[2], &ic2[2], a1, a2, a3, &l2, &b, hi);
}

SD_INLINE int32_t sd_ap2(int32_t x, int32_t *ic1, int32_t *ic2, int32_t a1, int32_t a2, int32_t a3)
{                                               /* the 2nd-order allpass LR4's two halves sum to: x - 2 k band */
    int32_t l, b, h;
    sd_svf3(x, ic1, ic2, a1, a2, a3, &l, &b, &h);
    return x - (int32_t)(((int64_t)b * 47453133) >> 24);   /* 2 x 1.414 in Q24 */
}

SD_INLINE int32_t sd_knee(int32_t a, int32_t T, int32_t S, int32_t inv)   /* a magnitude through a knee */
{
    int32_t e;
    if (a <= T) return a;
    if (S <= 0) return T;                       /* a brick wall */
    e = a - T;
    e = e > S + (S >> 1) ? S + (S >> 1) : e;   /* (S > 0 here) */          /* past 1.5 S the shoulder is flat */
    return T + (int32_t)(((int64_t)sd_cubic((int32_t)(((int64_t)e * inv) >> 8)) * S * 3) >> 24);   /* x 1.5 S / 2^23 */
}

/* One band through its shaper, both channels, m frames: the type picked once, not a sample (as the voice's kernel) */
#define SD_DLOOP(...)                                                                                \
    for (c = 0; c < 2u; c++) {                                                                       \
        const int32_t *bin = d->bb[k][c];                                                            \
        int32_t *wo = d->w[c], gch = d->g2[c][k], gn = g0;                                          \
        for (i = 0; i < m; i++) {                                                                    \
            int32_t b0 = bin[i], v = (((b0 >> 6) * (gn >> 4)) >> 6) + bias, a = v < 0 ? -v : v, y;   \
            b0 ^= b0 >> 31;                     /* ~|band|, for the dynamics' peak */                \
            pk = b0 > pk ? b0 : pk;                                                                  \
            gn += gs;                                                                                \
            __VA_ARGS__;                                                                             \
            wo[i] += (int32_t)(((int64_t)y * gch) >> 14);                                            \
        }                                                                                            \
    }

static void sd_dband(SdDistroy *d, uint32_t k, uint32_t m, uint32_t pos)
{
    uint32_t c, i;
    int32_t gs = d->gs[k], g0 = d->g0[k] + gs * (int32_t)pos, pk = d->pk[k];   /* the gain ramps through the sub-block */
    int32_t bias = d->bias[k];                  /* (|band| >> 6 <= 2^16) x (gain Q12 >> 4 <= 2^14): fits */
    int32_t T = d->t[k], S = d->s[k], inv = d->inv[k], Tn = d->tn[k], Sn = d->sn[k], invn = d->invn[k];
    int32_t fold = d->fold[k], mask = ~(d->crush[k] - 1);
    switch (d->type[k]) {
    case SD_DS_SOFT:
    case SD_DS_HARD:
        SD_DLOOP(y = v < 0 ? -sd_knee(a, Tn, Sn, invn) : sd_knee(a, T, S, inv));
        break;
    case SD_DS_FOLD:                            /* the triangle, period 4 t: x / t in Q15 (|v| <= 2^21, x 4: fits) */
        SD_DLOOP(int32_t x = (int32_t)(((int64_t)v * fold) >> 12), tr = (x + 32768) & 0x1FFFF, z = tr - 65536;
                 (void)a; y = 32768 - (z < 0 ? -z : z));
        break;
    case SD_DS_CRUSH:                           /* clipped, then bits off toward zero: no bias */
        SD_DLOOP(a = (a > 32767 ? 32767 : a) & mask; y = v < 0 ? -a : a);
        break;
    default:                                    /* NONE: no distortion, but a guard at 4 x full scale */
        SD_DLOOP((void)a; y = v > 4 * 32768 ? 4 * 32768 : v < -4 * 32768 ? -4 * 32768 : v);
        break;
    }
    d->pk[k] = pk;
}

/* The crossover, one channel, m frames, into the band buffers; LR4 and FB constants: each variant compiles without
 * the other's branches (C1: they were tested every sample), the coefficients in locals (the band buffers' stores
 * could alias them) */
SD_INLINE void sd_dx_loop(SdDistroy *d, const int32_t *in, uint32_t m, uint32_t c, const int LR4, const int FB)
{
    uint32_t i, k;
    int32_t a1[3], a2[3], a3[3];
    for (k = 0; k < 3u; k++) { a1[k] = d->a1[k]; a2[k] = d->a2[k]; a3[k] = d->a3[k]; }
    {
        int32_t *i1 = d->ic1[c], *i2 = d->ic2[c];
        const int16_t *fl = d->fl[c];           /* D3: the feedback, read here (the delay > a call: nothing this
                                                   call writes is read) */
        int32_t flp = d->flp[c], fb = d->fb, fT = d->fT, fS = d->fS, finv = d->finv;
        uint32_t fr = d->fw - (uint32_t)d->fd[c];
        for (i = 0; i < m; i++) {
            int32_t x = in[2u * i + c], x19, lp[3];
            if (FB) {
                int32_t v = fl[(fr + i) & (SD_DFLEN - 1u)] * 2, a;
                flp += ((v - flp) * 5735) >> 13;    /* ~7 kHz: the loop darkens. |v - lp| <= 2^17: x 2^12.5 fits
                                                   (x 22938 >> 15 overflowed and latched the loop on a DC). A
                                                   floor here: the toward-zero below drops it */
                a = flp < 0 ? -flp : flp;
                a = sd_knee(a, fT, fS, finv);       /* the soft clip: linear to 0.75, a ceiling ~1.05 */
                v = (flp < 0 ? -a : a) * fb;        /* <= 2^16 x 2^15 */
                x += v < 0 ? -((-v) >> 15) : v >> 15;   /* toward zero: no limit cycle */
            }
            x = x > (1 << 18) ? 1 << 18 : x < -(1 << 18) ? -(1 << 18) : x;   /* 8 x full scale (RULES: no
                                                   -(1 << 22) as a constant, so clamped before the shift) */
            x19 = x * 16;
            if (LR4) {                          /* split 2; 1 below it, 3 above; each side the other's allpass */
                int32_t lo, hi;
                sd_lr4(x19, &i1[0], &i2[0], a1[1], a2[1], a3[1], &lo, &hi);
                lo = sd_ap2(lo, &i1[3], &i2[3], a1[2], a2[2], a3[2]);
                hi = sd_ap2(hi, &i1[4], &i2[4], a1[0], a2[0], a3[0]);
                sd_lr4(lo, &i1[5], &i2[5], a1[0], a2[0], a3[0], &d->bb[0][c][i], &d->bb[1][c][i]);
                sd_lr4(hi, &i1[8], &i2[8], a1[2], a2[2], a3[2], &d->bb[2][c][i], &d->bb[3][c][i]);
            } else {
                for (k = 0; k < 3u; k++) lp[k] = sd_lp(x19, &i1[k], &i2[k], a1[k], a2[k], a3[k]);
                d->bb[0][c][i] = lp[0]; d->bb[1][c][i] = lp[1] - lp[0];
                d->bb[2][c][i] = lp[2] - lp[1]; d->bb[3][c][i] = x19 - lp[2];
            }
            d->w[c][i] = 0;
        }
        d->flp[c] = flp;
    }
}

static void sd_distroy_run(SdEngine *e, const int32_t *in, int32_t *out, uint32_t m, uint32_t pos)
{
    SdDistroy *d = &e->d;
    uint32_t i, c, k;
    for (c = 0; c < 2u; c++) {
        if (d->xover == SD_DX_LR4) {
            if (d->fb) sd_dx_loop(d, in, m, c, 1, 1); else sd_dx_loop(d, in, m, c, 1, 0);
        } else {
            if (d->fb) sd_dx_loop(d, in, m, c, 0, 1); else sd_dx_loop(d, in, m, c, 0, 0);
        }
    }
    for (k = 0; k < 4u; k++) sd_dband(d, k, m, pos);
    for (c = 0; c < 2u; c++) {                  /* the DC blocker on the sum (linear: as on each band, 2 not 8),
                                                   TAME, the limiter, into the feedback's line, then MIX */
        int16_t *fl = d->fl[c];
        const int32_t *w = d->w[c];             /* the state in locals: out[] and the line could alias it, and
                                                   the compiler would reload it every sample */
        int32_t pk = d->lpk, lg = d->lg, dx = d->dx[c], dy = d->dy[c], mix = d->mix;
        int32_t tame = d->tame, fb = d->fb, a1 = d->ta1, a2 = d->ta2, a3 = d->ta3, t1 = d->tic1[c], t2 = d->tic2[c];
        uint32_t fw = d->fw;
        for (i = 0; i < m; i++) {
            int32_t y = w[i], t = dy - sd_tz(dy, 9) + (y - dx) * 256, a;   /* y - y[-1] + 0.998 out[-1] (~14 Hz),
                                                   x 256 (no dead zone), toward zero (C's /); |y| <= 2^21: fits */
            dx = y;
            dy = t;
            y = sd_tz(t, 8);
            if (tame) y = sd_lp(y, &t1, &t2, a1, a2, a3);
            a = y < 0 ? -y : y;
            pk = a > pk ? a : pk;
            if (lg < 32768) y = (int32_t)(((int64_t)y * lg) >> 15);   /* (a 32-bit version with its clamps
                                                   measured slower: the chip's 64-bit multiply is cheap) */
            y = y > 65534 ? 65534 : y < -65534 ? -65534 : y;   /* the ceiling, 2 x full scale: the limiter is a
                                                   tick late, a surge inside a call stops here */
            if (fb) fl[(fw + i) & (SD_DFLEN - 1u)] = (int16_t)sd_tz(y, 1);   /* Q14; toward zero */
            out[2u * i + c] += in[2u * i + c] + (int32_t)(((int64_t)(y - in[2u * i + c]) * mix) >> 15);
        }
        d->dx[c] = dx;
        d->dy[c] = dy;
        d->tic1[c] = t1;
        d->tic2[c] = t2;
        d->lpk = pk;
    }
    if (d->fb) {
        d->fpk |= d->lpk;
        d->fw = (d->fw + m) & (SD_DFLEN - 1u);
    }
}

/* ---- SPACE: audio */

SD_INLINE int32_t sd_sp_read(const int16_t *dl, uint32_t w, int32_t dq)   /* dq samples back (Q8), linear */
{
    uint32_t i = (uint32_t)dq >> 8, r0 = w >= i ? w - i : w + SD_SP_DMAX - i, r1 = r0 ? r0 - 1u : SD_SP_DMAX - 1u;
    int32_t a = dl[r0], b = dl[r1];
    return (a + (((b - a) * (dq & 255)) >> 8)) * 2;   /* Q14 -> Q15 */
}

SD_INLINE int16_t sd_sp_store(int32_t v)        /* Q15 -> Q14, past 1.5 x full scale a shoulder to ~1.9 x */
{
    int32_t a;
    if ((uint32_t)(v + 49152) < 98305u) return (int16_t)sd_tz(v, 1);   /* under the knee: linear (the common case) */
    a = v < 0 ? -v : v;
    a = sd_knee(a, 49152, 12848, 111431);       /* T 1.5 FS, S 0.39 FS; inv = 2^31 / (1.5 S) */
    return (int16_t)(v < 0 ? -(a >> 1) : a >> 1);
}

/* SPACE's frames, PP constant. Its state in locals for the call (C1): out[] could alias it, and the compiler
 * reloaded it every sample */
SD_INLINE void sd_space_loop(SdSpace *s, SdSpaceMem *mem, const int32_t *in, int32_t *out, uint32_t m, uint32_t pos,
                             const int PP)
{
    uint32_t i, k, dw = s->dw, fw = s->fw, ph = s->ph;
    int32_t dstep = (s->dnext - s->dq) >> SD_SUB_SHIFT, dq = s->dq + dstep * (int32_t)pos;
    int32_t lpd0 = s->lpd[0], lpd1 = s->lpd[1], fb = s->fb, dtone = s->dtone, dlvl = s->dlvl, rlvl = s->rlvl;
    int32_t racc = s->racc, a00 = s->a0[0], a01 = s->a0[1], a10 = s->a1[0], a11 = s->a1[1], peak = s->peak;
    int32_t dz0 = s->dcz[0], dz1 = s->dcz[1];
    int16_t *dl0 = mem->dl[0], *dl1 = mem->dl[1];
    for (i = 0; i < m; i++) {
        int32_t xl = in[2u * i], xr = in[2u * i + 1u], el, er, r, al, ar, dl = xl, dr = xr;
        /* the input without its DC (a warp toward pulse has some): the delay's feedback and the room's lines
           multiply a DC up to 20 .. 100 x (C1: DROWN on VOYAGE built a rumble). The dry, below, keeps it */
        dz0 += ((xl * 256 - dz0) >> 10);        /* |x| <= 2^18: x 256 fits */
        dz1 += ((xr * 256 - dz1) >> 10);
        xl -= dz0 >> 8;
        xr -= dz1 >> 8;
        /* the delay: read, the tone, the feedback back in (crossed in ping-pong) */
        el = sd_sp_read(dl0, dw, dq);
        er = sd_sp_read(dl1, dw, dq);
        lpd0 += ((el - lpd0) * dtone) >> 15;    /* |difference| <= 2^17: x 2^15 fits; a floor here */
        lpd1 += ((er - lpd1) * dtone) >> 15;    /* parks at most at -1, which FDBK's toward-zero drops */
        el = lpd0;
        er = lpd1;
        if (PP) {
            dl0[dw] = sd_sp_store(((xl + xr) >> 1) + sd_tz(er * fb, 15));
            dl1[dw] = sd_sp_store(sd_tz(el * fb, 15));
        } else {
            dl0[dw] = sd_sp_store(xl + sd_tz(el * fb, 15));
            dl1[dw] = sd_sp_store(xr + sd_tz(er * fb, 15));
        }
        dw = dw + 1u < SD_SP_DMAX ? dw + 1u : 0u;
        dq += dstep;
        /* the reverb, at half rate: the dry and the echoes, mono, two frames averaged, through four diffusers
         * into the FDN; between its outputs a straight line (a frame late) */
        r = (xl + xr + el + er) >> 2;
        if (!ph) {
            ph = 1u;
            racc = r;
            al = a10;
            ar = a11;
        } else {
            int32_t h[4], z[4], v[4];
            ph = 0u;
            r = (racc + r) >> 1;
            for (k = 0; k < 4u; k++) {              /* Schroeder allpasses, g 0.625 */
                uint32_t aw = s->apw[k], rp = (aw + SD_SP_APLEN - SD_SP_APL[k]) & (SD_SP_APLEN - 1u);
                int32_t zz = mem->ap[k][rp] * 2, vv = r + sd_tz(zz * 20480, 15);
                mem->ap[k][aw] = sd_sp_store(vv);
                r = zz - sd_tz(vv * 20480, 15);
                s->apw[k] = (aw + 1u) & (SD_SP_APLEN - 1u);
            }
            for (k = 0; k < 4u; k++) {              /* each line: read, the tone, the decay */
                z[k] = mem->fdn[k][(fw - (uint32_t)s->flen[k]) & (SD_SP_FLEN - 1u)] * 2;
                s->flp[k] += ((z[k] - s->flp[k]) * s->rtone) >> 15;   /* as the delay's tone: the decay rounds */
                v[k] = sd_tz(s->flp[k] * s->g, 15);
            }
            h[0] = sd_tz((v[0] + v[1] + v[2] + v[3]), 1);   /* the Hadamard matrix / 2: orthonormal, energy kept */
            h[1] = sd_tz((v[0] - v[1] + v[2] - v[3]), 1);
            h[2] = sd_tz((v[0] + v[1] - v[2] - v[3]), 1);
            h[3] = sd_tz((v[0] - v[1] - v[2] + v[3]), 1);
            mem->fdn[0][fw] = sd_sp_store(h[0] + r);
            mem->fdn[1][fw] = sd_sp_store(h[1] - r);
            mem->fdn[2][fw] = sd_sp_store(h[2] + r);
            mem->fdn[3][fw] = sd_sp_store(h[3] - r);
            fw = (fw + 1u) & (SD_SP_FLEN - 1u);
            a00 = a10;
            a01 = a11;
            a10 = (z[0] - z[3]) >> 1;               /* two decorrelated taps */
            a11 = (z[1] - z[2]) >> 1;
            al = (a00 + a10) >> 1;
            ar = (a01 + a11) >> 1;
        }
        {
            int32_t pl = (el < 0 ? -el : el) | (al < 0 ? -al : al), pr = (er < 0 ? -er : er) | (ar < 0 ? -ar : ar);
            if (pl > peak) peak = pl;
            if (pr > peak) peak = pr;
        }
        out[2u * i] += dl + (el * dlvl >> 15) + (al * rlvl >> 15);   /* not a loop: a floor is fine; */
        out[2u * i + 1u] += dr + (er * dlvl >> 15) + (ar * rlvl >> 15);   /* each product < 2^31 */
    }
    s->dw = dw;
    s->fw = fw;
    s->ph = ph;
    s->lpd[0] = lpd0;
    s->lpd[1] = lpd1;
    s->racc = racc;
    s->a0[0] = a00;
    s->a0[1] = a01;
    s->a1[0] = a10;
    s->a1[1] = a11;
    s->peak = peak;
    s->dcz[0] = dz0;
    s->dcz[1] = dz1;
}

static void sd_space_run(SdEngine *e, const int32_t *in, int32_t *out, uint32_t m, uint32_t pos)
{
    if (e->sp.pp) sd_space_loop(&e->sp, e->sm, in, out, m, pos, 1);
    else sd_space_loop(&e->sp, e->sm, in, out, m, pos, 0);
}

/* Render n stereo frames into out (L R L R ..., Q15 full scale +-32767, already zero: add to it).
 * n is any count; sub-blocks continue across calls. */
/* The filter on a bank's mix, in place: one or two TPT SVF lowpass stages (as DISTROY's sd_lp), the mix x 8 inside
 * (the states' resolution at low cutoffs); out at most 4 x full scale, where the render's sum is written for it */
static void sd_filter_run(SdBank *bk, const SdVoice *v, uint32_t pos, uint32_t m, uint32_t two)
{
    int32_t *x = bk->mix + pos, *end = x + m;
    int32_t i1 = bk->fic1[0], i2 = bk->fic2[0], j1 = bk->fic1[1], j2 = bk->fic2[1];
    int32_t a1 = v->fa1[0], a2 = v->fa2[0], a3 = v->fa3[0], b1 = v->fa1[1], b2 = v->fa2[1], b3 = v->fa3[1];
    if (two)                                    /* (two loops: -Os does not take the test out of one) */
        while (x < end) {
            int32_t y = sd_tz(sd_lp(sd_lp(*x * 8, &i1, &i2, a1, a2, a3), &j1, &j2, b1, b2, b3), 3);
            *x++ = y > 131068 ? 131068 : y < -131068 ? -131068 : y;
        }
    else
        while (x < end) {
            int32_t y = sd_tz(sd_lp(*x * 8, &i1, &i2, a1, a2, a3), 3);
            *x++ = y > 131068 ? 131068 : y < -131068 ? -131068 : y;
        }
    bk->fic1[0] = i1; bk->fic2[0] = i2; bk->fic1[1] = j1; bk->fic2[1] = j2;
}

/* The limiter at a sub-block's start: the block coming out now (two back) and the one after it (just in) are
 * measured, so the gain ramps over this block to what both need, and never stands above what this one needs (it did
 * reach that by the end of the block before). Down within a block; up an 1/256 of the way a block (~0.19 s). */
static void sd_lim_block(SdEngine *e)
{
    int32_t pk = e->lim_pk1 > e->lim_acc ? e->lim_pk1 : e->lim_acc, tgt = 32768, g = e->lim_end;
    if (pk > SD_LIM_T) tgt = (int32_t)(((uint32_t)SD_LIM_T << 15) / (uint32_t)pk);   /* (32-bit: one a block) */
    e->lim_pk1 = e->lim_acc;
    e->lim_acc = 0;
    e->lim_g = g;                               /* (where the last ramp meant to end) */
    e->lim_end = tgt < g ? tgt : g + ((tgt - g) >> 8);
    e->lim_step = sd_tz(e->lim_end - g, SD_SUB_SHIFT);
    if (e->lim_end < g && e->lim_step * (int32_t)SD_SUB > e->lim_end - g) e->lim_step--;   /* (down: never short of it) */
}

static void sd_engine_render(SdEngine *e, int32_t *out, uint32_t n)
{
    while (n) {
        uint32_t m, pos, i, b, k;
        int32_t *dst;
        if (!e->sub_left) { sd_control_tick(e); sd_lim_block(e); e->sub_left = SD_SUB; }
        m = n < e->sub_left ? n : e->sub_left;
        pos = SD_SUB - e->sub_left;             /* where in the sub-block this call starts: ramps continue */
        {                                       /* voice -> (Reactor) -> (DISTROY) -> (SPACE) -> out */
            int32_t *last = e->sp.on ? e->sbuf : out, *post = e->d.on ? e->dbuf : last;
            if (e->sp.on)
                for (i = 0; i < 2u * m; i++) e->sbuf[i] = 0;
            if (e->d.on)
                for (i = 0; i < 2u * m; i++) e->dbuf[i] = 0;
            dst = e->r.on ? e->vbuf : post;
            if (e->r.on)
                for (i = 0; i < 2u * m; i++) e->vbuf[i] = 0;
        }
        for (b = 0; b < SD_BANKS; b++) {
            SdBank *bk = &e->bank[b];
            const SdVoice *v = &e->voice[bk->voice];
            if ((!v->amp && !v->amp_next) || (!bk->gl && !bk->gl_next && !bk->gr && !bk->gr_next)) {
                for (k = 0; k < SD_OPS; k++) bk->osc[k].ph += bk->osc[k].inc * m;   /* silent or weighted out */
                continue;
            }
            sd_bank_run(bk, pos, m);
            if (e->patch.f_type) sd_filter_run(bk, v, pos, m, e->patch.f_type == SD_FT_LP24);
            {
                int32_t lstep = (bk->gl_next - bk->gl) >> SD_SUB_SHIFT, gl = bk->gl + lstep * (int32_t)pos;
                int32_t rstep = (bk->gr_next - bk->gr) >> SD_SUB_SHIFT, gr = bk->gr + rstep * (int32_t)pos;
                const int32_t *mix = bk->mix + pos;
                for (i = 0; i < m; i++) {       /* the sum <= 4 x 32767, a gain <= 16256: fits */
                    dst[2u * i] += (mix[i] * gl) >> 15;
                    dst[2u * i + 1u] += (mix[i] * gr) >> 15;
                    gl += lstep;
                    gr += rstep;
                }
            }
        }
        if (e->r.on && !e->r.rest)              /* resting: the voice is silent too */
            sd_reactor_run(e, e->vbuf, e->d.on ? e->dbuf : e->sp.on ? e->sbuf : out, m, pos);
        if (e->d.on && !e->d.rest) sd_distroy_run(e, e->dbuf, e->sp.on ? e->sbuf : out, m, pos);
        if (e->sp.on && !e->sp.rest) sd_space_run(e, e->sbuf, out, m, pos);
        {                                       /* the limiter: each frame in, the one 64 back out, at the gain */
            int32_t g = e->lim_g, st = e->lim_step, acc = e->lim_acc;
            int32_t *lb = e->lim_buf;
            for (i = 0; i < m; i++) {
                uint32_t r = ((e->frames + i) & (SD_LIM_D - 1u)) * 2u;
                int32_t l = out[2u * i], rr = out[2u * i + 1u], al = l < 0 ? -l : l, ar = rr < 0 ? -rr : rr;
                acc = al > acc ? al : acc;
                acc = ar > acc ? ar : acc;
                out[2u * i] = (int32_t)(((int64_t)lb[r] * g) >> 15);
                out[2u * i + 1u] = (int32_t)(((int64_t)lb[r + 1u] * g) >> 15);
                lb[r] = l;
                lb[r + 1u] = rr;
                g += st;
            }
            e->lim_g = g;
            e->lim_acc = acc;
        }
        for (i = 0; i < 2u * m; i++) {          /* the output's shoulder: linear to 0.85 of full scale, then a cubic
                                                   one to it (the macros together, long tied notes, could clip hard) */
            int32_t v = out[i];
            if ((uint32_t)(v + 27853) > 55706u) {
                int32_t a = sd_knee(v < 0 ? -v : v, 27853, 4914, 291354);   /* inv = 2^31 / (1.5 S) */
                out[i] = v < 0 ? -a : a;
            }
        }
        out += 2u * m;
        n -= m;
        e->sub_left -= m;
        e->frames += m;
    }
}

#endif
