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
 * S1, SPACE (Sound Plan §5.5): a delay into a reverb, last in the chain.
 *   delay       stereo, or ping-pong (the mono sum in on the left; the left's echo into the right whole, the right's
 *               back to the left x FDBK^2: the echoes in pairs, the right as loud as the left, each echo down by FDBK
 *               on average, as in stereo); TIME free 1..1000 ms or
 *               synced to the note driver's sixteenth (1/32 .. 1/2, triplets, dotted, each exact); a division longer
 *               than the line plays at half, or a quarter, so it stays on the beat (sd_space_sync_q8); a new time
 *               glides there (a tape, ~12 ms); in its loop TONE (a one-pole lowpass), FDBK (0 .. 0.95), a soft ceiling
 *               at ~1.9 x
 *   reverb      a plate, raw and long: at half rate,
 *               a half-band filter in and out (-57 dB past 15 kHz); four diffusers, then a tank of two branches in a
 *               figure of eight (a modulated allpass, a delay, the damping, the decay, an allpass, a delay, the
 *               decay, into the other), out from seven taps across both a side. SIZE stretches every line 0.35 ..
 *               1.75 x (gliding there: a tape); DECAY is its T60 (SD_PL_T60: 0.3 .. 60 s), whatever the SIZE; TONE
 *               the damping; SHIMMER (s_mode's top bits) an octave up of the tank back into it; FREEZE (the
 *               player's, not the patch's) closes the input and holds the tail as it is. Its input the dry and the
 *               echoes: the echoes ring too. It runs on the second core (the plate's jobs); on one core (a second
 *               core that misreads RAM) a 4-line room plays in its place on core 0
 *   out         dry + D-LEVEL x the echoes + R-LEVEL x the reverb; both levels 0: no SPACE. It rests when nothing
 *               comes in and its tails have stayed under the floor for 2 s
 *   memory      ~234 KB (SdSpaceMem: the delay 173 KB, the plate's lines 61 KB), the app's (the pool on the FM-1):
 *               the engine only points at it
 *
 * The output (measured: 20 of the 29
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
                                                   the FM's screech up high */
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
#define SD_SP_DMAX 44160u                       /* SPACE's delay line, a channel: 1 s and a little (1/2 at 120 BPM
                                                   fits, 44100 frames) */
#define SD_SP_DHALF (SD_SP_DMAX / 2u)           /* its first half: the delay while the looper borrows the second */
#define SD_SP_NDIV 12u                          /* TIME synced: 1/32 .. 1/2 */
#define SD_LP_XF 224u                           /* the looper's crossfade where it comes round, half-rate samples
                                                   (10 ms) */
#define SD_LP_NMAX (4u << 20)                   /* its memory's most, samples (bytes: mono, mu-law, half rate; the
                                                   app lends it: half the delay line and a little of the RAM) */
enum { SD_SP_SYNC = 1, SD_SP_PP = 2 };          /* s_mode's bits; 2..7: SHIMMER, 0..63 */
#define SD_SP_SHIM(p) ((uint32_t)(p)->s_mode >> 2)
SD_INLINE uint32_t sd_sp_div(uint32_t t) { return (t * SD_SP_NDIV) >> 16; }   /* TIME synced: its division */
#define SD_LP_NDIV 15u                          /* the looper's LENGTH synced: TIME's, then 1, 2 and 4 bars */
SD_INLINE uint32_t sd_lp_div(uint32_t t) { return (t * SD_LP_NDIV) >> 16; }
SD_INLINE uint32_t sd_sp_ms(uint32_t t) { return 1u + (t * 999u) / 65535u; }   /* TIME free: 1 .. 1000 ms */
#define SD_LP_NGLIDE 16u                        /* GLIDE synced: OFF, then LENGTH's divisions (1/32 .. 4 bars) */
SD_INLINE uint32_t sd_lp_glide_div(uint32_t t) { return (t * SD_LP_NGLIDE) >> 16; }   /* 0 OFF, else division - 1 */
SD_INLINE uint32_t sd_lp_glide_ms(uint32_t t) { return (t * 4000u) / 65535u; }        /* GLIDE free: 0 .. 4000 ms */
#define SD_SP_LIM ((SD_SP_DMAX - 4u) * 256u)    /* the longest TIME, samples x 256 */
#define SD_SP_LIM2 ((SD_SP_DHALF - 4u) * 256u)  /* ... while the looper borrows the line's second half */
/* the plate's lines at half rate (22050 Hz), at SIZE x 1: four diffusers, then each branch's allpass, delay,
   allpass, delay (left, right); the shifter's window last. Each line has room for 1.75 x (and its modulation) */
enum { SD_PL_IN0, SD_PL_A1 = 4, SD_PL_D1 = 6, SD_PL_A2 = 8, SD_PL_D2 = 10, SD_PL_SH = 12, SD_PL_LINES };
#define SD_PL_SHW 2048u                         /* the shifter's window: 93 ms */
#define SD_PL_MOD 48u                           /* the modulation's room in each branch's first allpass */
#define SD_PL_MEM (16665u * 7u / 4u + 12u * 4u + 2u * SD_PL_MOD + SD_PL_SHW + 24u)   /* every line, int16 */
#define SD_PL_SMIN 22938                        /* SIZE: 0.35 .. 1.75 x, Q16 */
#define SD_PL_SMAX 114688
#define SD_PL_BLK 128u                          /* a job: 128 plate samples (256 frames: an audio half on the FM-1);
                                                   its output heard a block later (5.8 ms on the wet) */
#define SD_PL_ZN 512u                           /* the outputs' ring (a power of two) */
#define SD_PL_CH 24u                            /* the chunks' ring: a tick's settings each, two blocks of them */
#define SD_SP_FLEN 2048u                        /* the room's lines (a power of two: masked), at half rate */
#define SD_SP_APLEN 256u                        /* its diffusers, at half rate */
enum { SD_FX_REACTOR = 1, SD_FX_DISTROY = 2, SD_FX_SPACE = 4, SD_FX_FILTER = 8, SD_FX_TIME = 16 };   /* fx_off's bits
                                                   (SPACE: the reverb; TIME: the delay) */

/* C1: the modulation (Sound Plan §7.x; the spec's "one global matrix"): sources, destinations, a slot */
enum { SD_MS_OFF, SD_MS_ENV1, SD_MS_ENV2, SD_MS_LFO1, SD_MS_LFO2, SD_MS_NAVX, SD_MS_NAVY, SD_MS_VEL, SD_MS_KEY,
       SD_MS_WHEEL, SD_MS_AFTER, SD_MS_RAND,
       /* the engines talking to each other: where the
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
       /* more destinations, after the others so a saved slot
          keeps its meaning: the three envelopes' times and sustain (a full source at 100 %: the whole 0..127), the
          filter's ENV depth, the Reactor's SLEW, DISTROY's loop TIME and SPREAD, SPACE's delay TIME, its TONE, the
          room's SIZE and TONE, the LFOs' RATE */
       SD_MD_E1A, SD_MD_E1D, SD_MD_E1S, SD_MD_E1R, SD_MD_E2A, SD_MD_E2D, SD_MD_E2S, SD_MD_E2R,
       SD_MD_E3A, SD_MD_E3D, SD_MD_E3S, SD_MD_E3R, SD_MD_FENV, SD_MD_RSLEW, SD_MD_DTIME, SD_MD_DSPREAD,
       SD_MD_STIME, SD_MD_SDTONE, SD_MD_SSIZE, SD_MD_SRTONE, SD_MD_L1RATE, SD_MD_L2RATE,
       /* the looper's SPEED, +-12 semitones at 100 %, free (a tape's wobble); TURING's in whole semitones, through
          GLIDE */
       SD_MD_LSPEED,
       /* each crossover: the lower and the middle beside D SPLIT (now SPLIT 3, the upper), +-24 semitones each; one
          moved past another goes through it (they are sorted each tick: the band between closes and opens again).
          Each band's GAIN beside D GAIN (all four) and D HIGH (now GAIN 4, the top) */
       SD_MD_DSPLIT1, SD_MD_DSPLIT2, SD_MD_DGAIN1, SD_MD_DGAIN2, SD_MD_DGAIN3,
       SD_MDSTS };                              /* (D HIGH: the top band's gain; D SPLIT: the upper crossover) */
/* the destinations in the chain's order, as the lists show them: the voice and its envelopes, the filter, the
   Reactor, DISTROY, SPACE, the LFOs */
static const uint8_t SD_MD_ORDER[SD_MDSTS] = {
    SD_MD_OFF, SD_MD_PITCH, SD_MD_XM, SD_MD_WARP, SD_MD_FOLD, SD_MD_DT, SD_MD_X, SD_MD_Y, SD_MD_WIDTH,
    SD_MD_E1A, SD_MD_E1D, SD_MD_E1S, SD_MD_E1R, SD_MD_E2A, SD_MD_E2D, SD_MD_E2S, SD_MD_E2R,
    SD_MD_FCUT, SD_MD_FRES, SD_MD_FENV, SD_MD_E3A, SD_MD_E3D, SD_MD_E3S, SD_MD_E3R,
    SD_MD_RPOS, SD_MD_RFREQ, SD_MD_RRES, SD_MD_RDRIVE, SD_MD_RMIX, SD_MD_RSLEW,
    SD_MD_DGAIN, SD_MD_DGAIN1, SD_MD_DGAIN2, SD_MD_DGAIN3, SD_MD_DHIGH, SD_MD_DBIAS, SD_MD_DSPLIT1, SD_MD_DSPLIT2,
    SD_MD_DSPLIT, SD_MD_DFB, SD_MD_DTIME, SD_MD_DSPREAD, SD_MD_DTAME,
    SD_MD_DMIX, SD_MD_STIME, SD_MD_SFB, SD_MD_SDTONE, SD_MD_SDLVL, SD_MD_SSIZE, SD_MD_SDECAY, SD_MD_SRTONE,
    SD_MD_SRLVL, SD_MD_LSPEED, SD_MD_L1RATE, SD_MD_L2RATE};
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
                                                   note */
#define SD_LFO_DIVS 12                          /* SYNC: a cycle of 1 .. 64 sixteenths */
#define SD_CLK_NB 8u                            /* MIDI clock: its tempo, the mean of this many beats */
static const uint8_t SD_LFO_DIV[SD_LFO_DIVS] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
enum { SD_M4_BODY, SD_M4_AIR, SD_M4_DROWN, SD_M4S };   /* MACRO 4's kind (the spec's options A, B and DROWN) */

typedef struct {
    SdBankPatch bank[SD_BANKS];
    uint16_t xm, warp, fold, dt;                /* the Q page: 0..65535 (0..99 on the screen), every bank */
    int16_t e2_warp, e2_fold;                   /* ENV2's depth on WP / FD, -32767..32767 = -1..+1 (a mod slot) */
    uint8_t e1_a, e1_d, e1_s, e1_r;             /* ENV1, the level: times 0..127 = 1 ms .. 10 s, sustain 0..127 */
    uint8_t e2_a, e2_d, e2_s, e2_r;             /* ENV2, the FM amount */
    uint16_t x, y, width;                       /* the nav (0 = left / bottom), the stereo width; 0..65535 */
    uint8_t course;                                /* COURSE: 0 off, 1 from each note (SD_COURSE_*) */
    uint8_t fx_off;                             /* SD_FX_*: engines switched off (EDIT), their settings
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
    /* the filter: a lowpass on each voice, on its banks' carriers before the nav's balance; ENV3 on its cutoff.
       All zero: off (every patch before it plays as it did) */
    uint8_t f_type, f_key;                      /* SD_FT_*; KEY 0..100 % (100: the cutoff follows the note) */
    uint16_t f_cut, f_res;                      /* 0..65535: CUTOFF MIDI 16 .. 130 (21 Hz .. 15 kHz); RES */
    int16_t f_env;                              /* ENV3's depth, -32767..32767 = -/+ 60 semitones */
    uint8_t f_vel, tur_ld;                      /* VEL: 0..100 % = up to +24 semitones at velocity 127; TURING's
                                                   LENGTH (bits 0..4: 2..16, 0 reads as 8) and DIV (bits 5..7: the
                                                   SD_SDIV index + 1, 0 reads as 1/16) */
    uint8_t e3_a, e3_d, e3_s, e3_r;             /* ENV3, the filter's: as ENV1 */
} SdPatch;
/* TURING, a modulation source: a looping shift register on the tempo, its four fields in the patch's four spare
   bytes (so a patch saved before reads as TURING off). DEST is a matrix destination (SD_MD_*), or NOTES: SEQ A's
   notes moved in its scale as they play, the pattern itself never rewritten. 0: none */
#define SD_TD_NOTES 0x80u
#define SD_TUR_ON(p) ((p)->tur_set >> 7)
#define SD_TUR_CHANGE(p) ((uint32_t)((p)->tur_set & 127u) > 100u ? 100u : (uint32_t)((p)->tur_set & 127u))
#define SD_TUR_LEN(p) (((p)->tur_ld & 31u) ? ((p)->tur_ld & 31u) < 2u ? 2u : ((p)->tur_ld & 31u) > 16u ? 16u : (uint32_t)((p)->tur_ld & 31u) : 8u)
#define SD_TUR_DIV(p) (((p)->tur_ld >> 5) ? (uint32_t)((p)->tur_ld >> 5) - 1u > SD_TUR_DIVS - 1u ? SD_TUR_DIVS - 1u : (uint32_t)((p)->tur_ld >> 5) - 1u : 2u)
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

/* Who plays a voice (rules for what can play together): the hand (the keys and MIDI), SEQ A, SEQ B, the
 * arpeggiator. Each has a quota of voices held at once (sd_plan: the tracks first, then ARP, the hand the rest); a
 * note off turns off only its own source's notes, and no source takes a held voice from another: past its quota it
 * steals its own oldest, without one it is silent. */
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
    int16_t dl[2u * SD_SP_DMAX];                /* the delay, its frames left and right in turn, Q14 (2 x full
                                                   scale): the line's second half one run of bytes (the looper's,
                                                   borrowed: sd_engine_looper_borrow) */
    union {                                     /* the reverb's: the plate's, or (one core) the room's */
        int16_t pl[SD_PL_MEM];                  /* the plate's lines, one after another, Q14 */
        struct { int16_t fdn[4][SD_SP_FLEN], ap[4][SD_SP_APLEN]; } rm;   /* the room's lines and diffusers */
    } u;
} SdSpaceMem;

typedef struct {                                /* the looper's head over a tick (16 of its samples), as core 0 planned
                                                   it: the job reads these (a sample i into the tick reads k0 + adv i
                                                   / 16, and the second head as far from k1) */
    uint32_t k0, k1, ln;                        /* where it reads at the tick's start, x 256 into the loop; the second
                                                   head's (a crossfade to the loop's own place); the loop's length x
                                                   256 */
    int32_t adv;                                /* both heads' way over the tick, x 256 (REVERSE: back, negative) */
    int16_t x0, xd;                             /* the crossfade: the second head's weight at the start, Q15, and its
                                                   step over the tick */
    int16_t lev0;                               /* the level's fade at the start, Q14 (in and out: LOOP on and off) */
    int8_t levd;                                /* and a sample: +64 in (12 ms), -16 out (46 ms), 0 */
    uint8_t on, bl, rst;                        /* it plays (LOOP, or its fade); the crossfade on; LOOP from here: its
                                                   end where it records now */
    uint32_t back;                              /* (with rst) its end that many samples before that: a line passed */
} SdLpTick;

typedef struct {                                /* the plate's settings for a chunk (a tick's, or the part of it in one
                                                   block), as core 0 left them: the job reads these, never SdSpace's */
    uint32_t di[12], ti[14], ma[2];             /* the lines' and the taps' lengths, whole; the allpasses' (x 256) */
    int32_t g, c, sg, d2, snext, sc0, sst2;     /* Q14; SIZE's scale at the tick's end, at the chunk's first sample,
                                                   and its step a sample (Q16) */
    uint32_t m0;                                /* the chunk's first sample */
    uint8_t n, G, frz, tick;                    /* its samples; SIZE moving; FREEZE; the tick it is (its low byte) */
    int32_t llvl, rlvl;                         /* the looper's level, Q15; R-LEVEL, Q15 (the job mixes the plate and
                                                   the looper) */
    SdLpTick lt;                                /* the looper's head over the tick, as core 0 planned it */
    uint8_t li0;                                /* the chunk's first sample, into its tick (a tick split by a block) */
} SdPlateP;

typedef struct {                                /* SPACE's state */
    uint32_t dw;                                /* the delay's write position */
    uint32_t dmax;                              /* the delay line's frames: SD_SP_DMAX, or SD_SP_DHALF lent */
    int32_t dfade;                              /* the echoes' level through a change of it, Q15 */
    uint8_t larm, dsw;                          /* the looper armed; the change: 1 fading out, 3 clearing, 2 in */
    uint32_t dclr;                              /* the clearing: where it has got to (the line, then the looper's
                                                   other piece) */
    int32_t dq, dnext, dtgt;                    /* the delay, samples x 256: now (this sub-block's start), next, target */
    int32_t lpd[2];                             /* the delay's tone lowpasses */
    int32_t fb, dtone, dlvl, rlvl;              /* Q15 */
    int32_t fb2;                                /* ping-pong's way back: FDBK^2, Q15 */
    uint8_t on, rest, pp, ph;                   /* ph: the plate's half rate, 0 gathers, 1 runs */
    int32_t peak;
    uint32_t quiet;
    int32_t dcz[2];                             /* the input's DC, Q8 (~7 Hz): kept out of the echoes and the plate */
    /* the plate */
    uint32_t pw[SD_PL_LINES];                   /* where each line writes next (not 16-bit: the lines' own type, every
                                                   write would make the compiler read them again) */
    int32_t sc, snext, stgt;                    /* SIZE: the lines' scale now (this sub-block's start), next, where
                                                   it goes; Q16 */
    uint32_t di[SD_PL_SH], ti[14];              /* the lines' lengths and the taps' at it, whole samples (still) */
    uint32_t ma[2];                             /* each branch's first allpass, its length with the LFO's (x 256) */
    int32_t md;                                 /* the LFOs' depth, 0..256: FREEZE glides it to 0 (~0.2 s), and the
                                                   allpass to a whole sample, so the held tail loses nothing between
                                                   samples (1.9 dB in 7 s, else); back the same way */
    int32_t g, c, sg, d2;                       /* the decay (each pass), the damping lowpass, SHIMMER's share of the
                                                   way round, the second allpasses'; Q14 (16384: 1) */
    int32_t ig;                                 /* the input, Q14: FREEZE fades it out (and back) over ~10 ms */
    int32_t lpi, lp[2], fbk[2], shy;            /* the input's lowpass; the branches' damping, their ends; the shifter */
    uint32_t lfo[2], shp;                       /* the two LFOs (2^32 a cycle, a step a tick); the shifter's place */
    int32_t hx[32];                             /* the half-band's way in (the last frames) */
    uint8_t hxw, frz, tick, room;               /* its write place; FREEZE as it plays; ticks (low byte); the room in
                                                   the plate's place (one core) */
    /* the plate's queue: its inputs (two blocks), how many so far, the blocks handed over and done, the chunks; its
       outputs (the job writes them, a block at a time; heard a block later) */
    int16_t qfl[4u * SD_PL_BLK], qfr[4u * SD_PL_BLK];   /* the queue: core 0's frames, two blocks of them, the
                                                   looper's left and right (x 1/2: 16 bits) and the plate's mono (as
                                                   it was: the plate as before, bit for bit); the job brings them down
                                                   to half rate */
    int32_t qfm[4u * SD_PL_BLK];
    uint32_t qn, qpost, qdone, qpw, qb0[2], qb1[2];   /* (qb: each block in flight, its chunks qb0 .. qb1) */
    SdPlateP qp[SD_PL_CH];
    int32_t zl[SD_PL_ZN], zr[SD_PL_ZN];
    /* the room (one core): its write places, the tone lowpasses, the lines' lengths moving to their targets, the
       gain, the tone, the gathered input and the taps before and now */
    uint32_t fw, apw[4];
    int32_t flp[4], flen[4], ftgt[4], rg, rtone, racc, a0[2], a1[2];
    /* the looper (TIME's): it records what SPACE plays before the reverb (the dry and the echoes at D-LEVEL), each
       side at half rate (core 0 queues the frames, the job brings them down; on one core core 0 does, and the queue's
       count is the looper's ring's). The plate's out goes to zl and zr as before (core 0 puts R-LEVEL on it as it
       hears it), the looper's to yl and yr, which core 0 reads only while it plays. Its settings now (core 0's: LOOP,
       the length, the speed gliding, the level); its state (the job's, or core 0's on one core): where it records,
       LOOP's end, length and place x 256, its level fading in and out */
    int16_t yl[SD_PL_ZN], yr[SD_PL_ZN];          /* the looper's out (x 1/2, at its level), a block late as the
                                                   plate's in zl and zr (on one core its own ring, a few samples late) */
    uint32_t lhear;                             /* core 0: frames it still reads them (while it loops, and its fade) */
    int32_t hxr[32];
    int32_t hxm[32];                            /* the plate's way in, as before the looper: the dry and the echoes
                                                   whole (D-LEVEL is the echoes' level out, not into the reverb).
                                                   hx, hxr and hxm: the half-bands' (the job's; core 0's on one core) */
    int32_t llen, llvl;                         /* core 0's: LENGTH as set (half-rate samples x 256), the level */
    uint8_t loop, lfold, lrev, lti;             /* LOOP, the halvings, REVERSE as set; (one core) the sample into
                                                   the tick */
    uint8_t qti;                                /* the samples of this tick queued so far */
    uint32_t lw, lbase;                         /* the job's (core 0's on one core): where it records, LOOP's end */
    SdLpTick lt;                                /* core 0's plan of this tick, for its chunks */
    /* core 0's plan (sd_lp_plan): the head and the crossfade's second; the loop's length; its grid (where it would
       read at x 0.5, 1 and 2 since LOOP, x 256) and where that starts (forward: its start; back: where REVERSE
       turned it); the speed now (Q16), its glide (from, to, the ticks, done, its own way so far, the bend, all and
       so far), a nudge; the speed set last, the crossfade's weight and step, the level; playing, crossfading, LOOP
       from here to hand over, backwards, waiting (1 an octave at once, 2 REVERSE's turn) */
    uint32_t pk, pk1, pln, pG[3], panc[2];
    int32_t pr, pg0, pgt, pgP, pgC, pgCd, pnud, ppr;
    int32_t pW;                                 /* the matrix's free wobble: how far it has taken the head (x 256;
                                                   the grid follows it while a slot sends, then takes it back) */
    uint32_t pgN, pgn;
    int16_t psp, px, pxd, plev;
    uint8_t pon, pbl, prst, pdir, ppend;
    /* the transport (PLAY, a MIDI clock): the half-rate samples since START, and at LOOP; running last tick, the loop
       parked by STOP (silent, its memory kept), and its place against the master's one known (LOOP while running) */
    uint32_t pT, pT0;
    uint8_t prun, ppark, psync;
    /* LOOP synced while the tracks play: its lock on the grid (sd_lp_snap): waiting for a line ahead, the clock time
       still to go (samples x 256); the samples back to a line passed, for the job */
    uint8_t pqw;
    uint32_t pq, pback;
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
    volatile uint8_t in;                        /* let in: ON (B: not MONO), and the master's one came since, if the
                                                   master played (the master itself: at once) */
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
    uint32_t lfo_ct[2], lfo_cyc[2], lfo_inv[2]; /* SYNC: clock time into the cycle, the cycle (0: not synced last
                                                   tick), 2^32 / the cycle */
    int32_t lfo_sh[2];                          /* S&H's held value */
    volatile int16_t lfo_out[2];                /* the LFOs now, Q15 (for the screen) */
    int32_t lfo_shg[2];                         /* S&H's output gliding to the held value (~10 ms) */
    uint32_t move_t;                            /* MOVE: samples x 256 into the beat */
    int32_t move_tgt[3], move_cur[3];           /* its X, Y, POS: drawn on each beat, glided to (Q15) */
    uint32_t pmul;                              /* PITCH: the note's increment x pmul, Q16 */
    int32_t psemis;                             /* the same as semitones x 256 */
    int32_t tq;                                 /* TURING to PITCH, semitones x 256: quantized to SEQ A's key for
                                                   each voice (sd_tq_snap), on top of pmul (0: none) */
    int32_t mvx, mvy;                           /* the nav's offset (the matrix's X / Y, MOVE's orbit), Q16 */
    int32_t echo_env;                           /* ECHO: SPACE's wet level, followed (~60 ms release), Q15 */
    /* ORBIT (sd_seq.h): handed over as a pattern is (orb_in, odd orb_seq while the main loop writes it); the step,
       the loops of A in it, the sub-blocks into it and in A's last loop (the glide's span) */
    volatile uint32_t orb_seq;
    SdOrbit orb_in, orb;
    uint32_t orb_taken;
    uint8_t orb_step, orb_loops;
    volatile uint8_t orb_shown;                 /* the step playing (the screen) */
    volatile uint8_t orb_live;                  /* on and in: switched on while the master played, at its one */
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

    /* the sequencer (sd_seq.h): two tracks on the one sound (a melody and a bass line at once). A always; B with
       seqb. One tempo, one transport (seq_run) */
    volatile uint8_t seq_run;                   /* the main loop sets it; the audio starts and stops on it */
    volatile uint8_t seqb;                      /* track B on (the main loop sets it) */
    volatile uint8_t xkey;                      /* the transposes IN KEY: in degrees of SEQ A's scale from its key (B
                                                   keeps to A's key and root), a chord tone by tone; 0: in
                                                   semitones (CHROM) */
    uint32_t seq_len_q8;                        /* a 16th, samples x 256: the clock time's unit (below) */
    /* MIDI clock: the main loop counts the pulses (24 a quarter) in clk_in and a START in clk_start. Everything
       on the tempo (the tracks, ARP, TURING, the synced LFOs, MOVE) moves by the clock time, clk_adv a sub-block:
       SD_SUB << 8 on the FM-1's own tempo; while pulses come, as far as they have gone (a pulse a 6th of a 16th),
       carried between them at their tempo and never past the next one, so nothing drifts from them. Their tempo is
       measured (the mean of the last SD_CLK_NB beats), and while the tracks play, seq_len_q8 holds: the pulses carry
       a new tempo, so a step stays 6 pulses */
    volatile uint32_t clk_in, clk_start;        /* pulses received; STARTs (and CONTINUEs), at their first pulse */
    uint32_t clk_seen, clk_start_seen;          /* taken */
    volatile uint8_t clk_on;                    /* following a clock: a pulse in the last 0.5 s */
    uint8_t clk_nb, clk_bi, clk_dev, clk_k;     /* beats in the mean, the next place; beats off it; pulses mod 6 */
    uint32_t clk_bn, clk_bf, clk_since;         /* pulses and frames since the beat's first; frames since the last */
    uint32_t clk_beats[SD_CLK_NB];              /* the last beats, frames */
    volatile uint32_t clk_beat;                 /* the clock's beat, frames (0: not measured yet) */
    uint32_t clk_pos, clk_done;                 /* clock time (samples x 256): at the last pulse; ours */
    uint32_t clk_adv;                           /* this sub-block's clock time */
    uint32_t tempo_q8;                          /* a 16th as heard (SPACE's synced time): seq_len_q8, or the clock's */
    uint8_t seq_started;                        /* A started this sub-block: the synced LFOs and MOVE from the one */
    volatile uint8_t sp_freeze;                 /* SPACE's FREEZE (the main loop's, ALGORITHM on SPACE): the plate's
                                                   input closed, its tail held */
    volatile uint8_t sp_room;                   /* the app's: the room in the plate's place, and the delay on this
                                                   core in the tape's (one core: the second cannot run, or gave up) */
    /* the looper (TIME's page, the player's, not the patch's): LOOP (it stops recording and its last LENGTH goes
       round), LENGTH (as TIME: free 1 .. 1000 ms, or a division with SD_SP_SYNC), its speed (semitones x 256,
       -12 .. +12: the pitch and the length together, a tape's), its level (0 .. 65535: above 0 it is armed and
       records); its memory, mono at half rate, a byte (mu-law) a sample, in two pieces of one ring (the app's: lpa
       NULL, none; sd_engine_looper, sd_engine_looper_borrow) */
    volatile uint8_t lp_on, lp_rev;             /* (lp_rev: REVERSE, its loop backwards) */
    volatile uint16_t lp_len, lp_lvl;
    volatile int16_t lp_speed;
    int32_t lp_mfree, lp_mtq;                   /* the matrix to the looper's SPEED (semitones x 256): free (on the
                                                   rate as it plays, its glide's after), TURING's in whole semitones
                                                   (on the speed set: through GLIDE, and to the grid at an octave) */
    uint8_t lp_mon;                             /* a slot sends to it free (the grid then follows its wobble) */
    volatile uint16_t lp_glide;                 /* GLIDE: how long a new speed takes; 0 at once, else a
                                                   division synced (sd_lp_glide_div), or free 0 .. 4000 ms */
    uint8_t *lpa, *lpb;                         /* its memory's two pieces (lpb may be NULL) */
    uint32_t lpa_n, lpb_n;
    uint8_t lp_borrow;                          /* lpa is the delay line's second half: armed, the delay has the
                                                   first (0.5 s); the change faded and the line cleared */
    /* the plate's jobs (on the second core, never on core 0, where the room is the fallback): the app's hooks.
       pl_post hands block b over and returns 1, or 0 when it could not (the block is dropped: sd_plate_drop); pl_join
       waits for the block handed over: 0 done, -1 dropped. NULL (a computer): the engine runs each block here, at
       once. The job is sd_plate_job */
    int (*pl_post)(void *e, uint32_t b);
    int (*pl_join)(void *e);
    /* the master: the track whose one leads, A (0) or B (1). The other track and ORBIT switched on while
       it plays come in at its one, from their step 1 (at once while it plays no steps; stopped, START starts them
       all); ORBIT counts its loops and the Reactor its steps. SEQ A's key (IN KEY) and TURING's NOTES stay A's */
    volatile uint8_t master;                    /* the main loop's choice */
    uint8_t lead;                               /* the master this sub-block (B only while it is there: seqb) */
    uint8_t a_wait, a_one;                      /* the master plays steps; its one this sub-block */
    uint32_t a_rem;                             /* its time past the one's start (the other's step 1 lines up on it) */
    SdSeqT t[2];

    /* the arpeggiator: the keys' notes (and MIDI's), played one at a
       time on the tempo. The main loop sets these; the audio keeps the held notes and plays them */
    volatile uint8_t arp_on, arp_mode, arp_div, arp_oct, arp_gate, arp_hold;   /* SD_AM_*; SD_SDIV index; 1..4
                                                   octaves; 1..100 %; HOLD: the notes stay after the keys lift */
    uint8_t arp_n, arp_keys, arp_was;           /* notes in the list; keys down now; arp_on last tick */
    uint8_t arp_note[8], arp_vel[8];            /* the notes, in the order played */
    /* the hand's keys held now, the latest last (in MONO, a second key let go goes back to the first, still held):
       sd_hand_key keeps it, sd_hand_off gives a voice back to it */
    uint8_t hand_note[SD_HELD], hand_vel[SD_HELD], hand_n;
    uint8_t arp_i, arp_dir, arp_playing, arp_cur;   /* the place in the run; UPDN's way; a note sounds; which */
    uint32_t arp_pos_q8, arp_step_q8;           /* samples x 256 into the step; its length */
    /* the grid (ARP's steps on SEQ's beat, not from the moment the keys went down): clock time since the master's
       START, within
       48 sixteenths (every division fits it a whole number of times). While the patterns play, ARP's steps fall on
       it; stopped, ARP runs from the key as before */
    uint32_t grid_q8, grid_adv, arp_ph;         /* the grid now (this sub-block's start); the last sub-block's clock
                                                   time, added at the next; ARP's place in its step there */
    uint32_t arp_rng;
    /* what ARP played, for KEEP (REC on ARP): its last SD_PSTEPS steps, a ring (a note, 0 a rest; its velocity),
       from when it came on; the
       time since the last step began (clock time: a MIDI clock's too) */
    uint8_t arp_hn[SD_PSTEPS], arp_hv[SD_PSTEPS], arp_hw, arp_hc;
    uint32_t arp_since_q8;
    /* the envelopes' times and the LFOs' rates as the matrix left them (sd_mod_snap): they tick before it, so they
       take its last tick's (0.73 ms on); the patch's own when nothing moves them */
    uint8_t envp[12];
    uint16_t lfor[2];

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
    uint32_t len = (SD_FS * 60u * 256u) / (bpm * 4u);           /* 677,376,000 / (bpm*4): fits in 32 bits */
    if (e->clk_on && e->clk_beat) {             /* a MIDI clock leads: its tempo (40..240 BPM: 66150..11025 frames) */
        uint32_t b = e->clk_beat < 11025u ? 11025u : e->clk_beat > 66150u ? 66150u : e->clk_beat;
        len = b * 64u;                          /* a beat's frames x 256 / 4 */
    }
    e->tempo_q8 = len;
    if (!e->clk_on || !(e->t[0].seq_on || e->t[1].seq_on)) e->seq_len_q8 = len;   /* (following while the tracks
                                                   play: the unit holds, the pulses carry the tempo) */
}

/* The tempo of a MIDI clock: a beat (24 pulses) measured in frames. The mean of the last SD_CLK_NB, taken when it
 * moves by over 0.4 % (the pulses' jitter, a few ms through USB and the main loop, must not move SPACE's time); two
 * beats in a row 6 % away are a new tempo, taken at once (one late beat is not). */
static void sd_clk_beat(SdEngine *e, uint32_t b)
{
    uint32_t i, sum = 0, m, c = e->clk_beat;
    if (b < 5000u || b > 132300u) return;       /* (20..530 BPM: anything else is not a clock) */
    if (e->clk_nb && c && (b > c + c / 16u || b + c / 16u < c)) {
        if (++e->clk_dev < 2u) return;
        e->clk_nb = 0;
    }
    e->clk_dev = 0;
    e->clk_beats[e->clk_bi] = b;
    e->clk_bi = (uint8_t)((e->clk_bi + 1u) % SD_CLK_NB);
    if (e->clk_nb < SD_CLK_NB) e->clk_nb++;
    for (i = 0; i < e->clk_nb; i++) sum += e->clk_beats[(e->clk_bi + SD_CLK_NB - 1u - i) % SD_CLK_NB];
    m = sum / e->clk_nb;                        /* (8 x 132300: fits) */
    if (!c || e->clk_nb < SD_CLK_NB || m > c + c / 256u || m + c / 256u < c) e->clk_beat = m;
}

/* MIDI clock, once a sub-block before anything on the tempo: the pulses taken, then this sub-block's clock time.
 * The pulses count as come at this sub-block's start (the main loop has just read them); the clock time goes to the
 * last pulse's place plus the time since it at their tempo, never as far as the next pulse; behind, it catches up a
 * pulse at once and the rest at twice the tempo (a stall's burst plays no steps at once). 0.5 s without a pulse: the
 * FM-1's own tempo again. */
static void sd_clk_tick(SdEngine *e)
{
    uint32_t n = e->clk_in - e->clk_seen, L = e->seq_len_q8, pc = L / 6u, iv, s, d, nom;
    e->clk_seen += n;
    if (n) {
        if (!e->clk_on || e->clk_since > SD_FS / 2u) {   /* the first pulse (after a gap): the clock time from here */
            e->clk_on = 1;
            e->clk_pos = e->clk_done;
            e->clk_k = 0;
            e->clk_bn = e->clk_bf = 0;
            e->clk_nb = e->clk_dev = 0;
            n--;
        } else
            e->clk_bf += e->clk_since;
        e->clk_since = 0;
        if (n > 96u) n = 96u;                   /* (a stall's burst: 4 beats at most) */
        {                                       /* n pulses on: a 6th of a 16th each, the 6th takes the remainder */
            uint32_t k = e->clk_k + n;
            e->clk_pos += n * pc + (k / 6u) * (L - 6u * pc);
            e->clk_k = (uint8_t)(k % 6u);
        }
        e->clk_bn += n;
        if (e->clk_bn >= 24u) {                 /* a beat: the tempo */
            sd_clk_beat(e, e->clk_bf * 24u / e->clk_bn);
            e->clk_bn = e->clk_bf = 0;
        }
        if (e->clk_start != e->clk_start_seen) e->clk_done = e->clk_pos;   /* START: from its pulse */
    }
    if (e->clk_since < SD_FS) e->clk_since += SD_SUB;   /* (to this sub-block's end) */
    if (e->clk_bf > 4u * SD_FS) e->clk_bf = 4u * SD_FS;
    if (e->clk_on && e->clk_since > SD_FS / 2u) e->clk_on = 0;
    if (!e->clk_on) {
        e->clk_adv = SD_SUB << 8;
        e->clk_done += SD_SUB << 8;
        return;
    }
    iv = e->clk_beat ? e->clk_beat / 24u : e->clk_bn >= 3u ? e->clk_bf / e->clk_bn : (L >> 8) / 6u;   /* frames a
                                                   pulse: the clock's, a first guess, or ours */
    iv = iv < 400u ? 400u : iv > 3000u ? 3000u : iv;
    s = e->clk_since < iv ? e->clk_since : iv;
    d = s * pc / iv;                            /* (3000 x 705600: fits) */
    if (d >= pc) d = pc - 1u;
    d = e->clk_pos + d - e->clk_done;
    if ((int32_t)d < 0) d = 0;                  /* ahead (the clock slowed): wait for it */
    nom = SD_SUB * pc / iv;
    if (d > 2u * nom + pc) d = 2u * nom + pc;   /* (a pulse behind, as a faster tempo leaves it: caught at once) */
    e->clk_adv = d;
    e->clk_done += d;
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

/* the looper's memory (armed while L-LEVEL is above 0, mono mu-law at half rate, ~5 s): a and b, two pieces of one
 * ring of bytes
 * (b may be NULL); or, borrowing, the delay line's second half (e->sm's: set e->sm first) and b. Set before SPACE runs */
SD_INLINE uint32_t sd_lp_total(const SdEngine *e)
{
    uint32_t n = e->lpa ? e->lpa_n + (e->lpb ? e->lpb_n : 0u) : 0u;
    return n < SD_LP_NMAX ? n : SD_LP_NMAX;
}
static void sd_engine_looper(SdEngine *e, uint8_t *a, uint32_t na, uint8_t *b, uint32_t nb)
{
    e->lpa = a; e->lpa_n = a ? na : 0u; e->lpb = b; e->lpb_n = b ? nb : 0u; e->lp_borrow = 0;
}
static void sd_engine_looper_borrow(SdEngine *e, uint8_t *b, uint32_t nb)
{
    e->lpa = e->sm ? (uint8_t *)&e->sm->dl[2u * SD_SP_DHALF] : 0;
    e->lpa_n = e->sm ? SD_SP_DHALF * 4u : 0u;   /* (its frames' two channels' bytes) */
    e->lpb = b; e->lpb_n = b ? nb : 0u;
    e->lp_borrow = e->sm != 0;
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
    e->clk_adv = SD_SUB << 8;                   /* the FM-1's own tempo until a MIDI clock comes */
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

/* COURSE (MONO only): ON from each note, once round, A -> B -> C -> D held at D; LOOP round and round on its own,
   A B C D and back to A (that leg as long as A -> B), never from a note: from the master's START (in time with the
   tracks), else from where it is */
#define SD_COURSE_ON 1u
#define SD_COURSE_LOOP 2u
static void sd_course_start(SdEngine *e)           /* COURSE from its first cell */
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
    if (e->mode_on == SD_MODE_MONO && e->patch.course == SD_COURSE_ON) sd_course_start(e);   /* (LOOP: not from a
                                                   note) */
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
 * inside it (nothing tries to play more than is possible), so a quota cut here is only a chord's top notes. */
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
/* TIME synced: its division of a sixteenth, NUM / DEN (1/32 1/16T 1/16 1/8T 1/16D 1/8 1/4T 1/8D 1/4 1/2T 1/4D 1/2) */
static const uint8_t SD_SP_NUM[SD_LP_NDIV] = {1, 2, 1, 4, 3, 2, 8, 3, 4, 16, 6, 8, 16, 32, 64},
                     SD_SP_DEN[SD_LP_NDIV] = {2, 3, 1, 3, 2, 1, 3, 1, 1, 3, 1, 1, 1, 1, 1};   /* (the last three
                                                   the looper's: 1, 2 and 4 bars) */
static uint32_t sd_sp_half(uint32_t d)          /* the division half as long as d (or d, the shortest) */
{
    uint32_t k;
    for (k = 0; k < d; k++) if (2u * SD_SP_NUM[k] * SD_SP_DEN[d] == SD_SP_NUM[d] * SD_SP_DEN[k]) return k;
    return d;
}
/* Division d of the sixteenth t (samples x 256), as the delay plays it: exact (the triplets were 0.2 % long or
 * short, a millisecond off by the fourth echo), and halved until it fits the line, so it stays on the beat
 * (held at the line's end instead, 1/2 below 120 BPM, 1/4D below 90 and 1/2T below 80 fell off the beat). A halving
 * is three places down the list (1/2 -> 1/4,
 * 1/4D -> 1/8D, 1/2T -> 1/4T); at 40 BPM 1/2 takes two. *fold: how many (the screen says so) */
static uint32_t sd_space_sync_q8(uint32_t t, uint32_t d, uint32_t *fold, uint32_t lim)   /* lim: the line's
                                                   (SD_SP_LIM), samples x 256 */
{
    uint32_t q, f = 0;
    d = d >= SD_LP_NDIV ? SD_LP_NDIV - 1u : d;  /* (TIME's to 1/2; the looper's to 4 bars) */
    q = t * SD_SP_NUM[d] / SD_SP_DEN[d];        /* (t <= 66150 x 64, x 64: fits) */
    while (q > lim && f < 8u) { q >>= 1; f++; }
    if (fold) *fold = f;
    return q;
}
/* The reverb runs at half rate (22.05 kHz: it costs half, and its tail is darker under 11 kHz), so its lengths are
 * in half-rate samples: 66 .. 92 ms lines, 2.4 .. 8.7 ms diffusers */
static uint32_t sd_semis_mul(int32_t semis)    /* 2^(semis / 12), semis x 256 (+-60 semitones): Q16 */
{
    int32_t n = semis >> 8, c = ((semis & 255) * 100) >> 8, o = n >= 0 ? n / 12 : -((11 - n) / 12);
    uint32_t r = (uint32_t)(((uint64_t)SD_SEMI[n - o * 12] * SD_CENTS[99 + c]) >> 16);   /* Q16, 1 .. 2.1 */
    return o >= 0 ? r << o : r >> -o;
}
/* the plate's lines at SIZE x 1, half rate (a published plate's, from 29761 Hz), and its taps: the left's from the
   right branch mostly, the right's from the left */
static const uint16_t SD_PL_BASE[SD_PL_SH] = {105, 79, 281, 205, 498, 673, 3299, 3124, 1334, 1968, 2756, 2343};
static const uint16_t SD_PL_TL[7] = {197, 2203, 1417, 1479, 1474, 139, 790}, SD_PL_TR[7] = {262, 2687, 910, 1980, 1564, 248, 90};
/* each line's room (1.75 x its length, 4 more, the first allpasses 48 more for the LFOs; the shifter 2048 + 8) and its
   place in pl[]: constants, in the flash (the second core reads RAM wrong on some units, never the flash) */
static const uint16_t SD_PL_PN[SD_PL_LINES] = {187, 142, 495, 362, 923, 1229, 5777, 5471, 2338, 3448, 4827, 4104, 2056},
                      SD_PL_PO[SD_PL_LINES] = {0, 187, 329, 824, 1186, 2109, 3338, 9115, 14586, 16924, 20372, 25199, 29303};
static const uint16_t SD_SP_FMAX[4] = {1451, 1667, 1861, 2027};   /* the room's lines' longest, primes */
static const uint16_t SD_SP_APL[4] = {71, 53, 191, 139};          /* its diffusers, primes */
#define SD_PL_BRANCH_US 362721                  /* a branch's way at x 1 (7998 samples), us: the decay's span */
static int32_t sd_pl_sin(uint32_t ph)           /* a sine from a parabola, 4 u (1 - |u|): -32767 .. 32767 */
{
    int32_t u = (int32_t)(ph >> 16) - 32768, a = u < 0 ? -u : u;
    int32_t y = (int32_t)(((int64_t)4 * u * (32768 - a)) >> 15);
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

/* ---- the looper's head, planned here on core 0 a tick (16 of its samples) at a time: an octave finds the loop's
 * grid again, over GLIDE or at once (SCL + OCT the octave, GLIDE on SCL + K1: a glide bent a little to land on the
 * beats, then a crossfade into its own place). The loop's grid
 * had it played at that speed (x 0.5, 1 or 2) since LOOP (pG), from its start (panc[0]; backwards, from where
 * REVERSE turned it: panc[1]). Between the octaves it runs free. A new speed glides there over GLIDE (a smoothstep in
 * the rate; OFF: at once). One at an octave is bent on the way, at most a sixth of the glide's own change (so it
 * stays the glide), to land where its grid has it, else on its grid's beats (a 32nd of the loop); what it misses
 * after, it crossfades over (the two heads on the beats, as long as GLIDE, at least a 16th of the loop), or nudges
 * (a few samples). An octave at once waits for its grid's next 16th of the loop (the two heads on its beats then),
 * REVERSE's turn for its next 64th (the turn on them). The job reads where this says */
SD_INLINE uint32_t sd_lp_add(uint32_t k, int32_t d, uint32_t ln)   /* k + d round the loop (k < ln < 2^31) */
{
    uint32_t m;
    if (d >= 0) { k += (uint32_t)d; return k < ln ? k : k - ln < ln ? k - ln : k % ln; }
    m = (uint32_t)-d;
    if (m >= ln) m %= ln;
    return k >= m ? k - m : k + ln - m;
}
static int32_t sd_lp_near(uint32_t to, uint32_t from, uint32_t s)   /* to - from the nearest way, round s */
{
    uint32_t d;
    if (!s) return 0;
    d = to >= from ? (to - from) % s : (s - (from - to) % s) % s;
    return d >= s / 2u ? (int32_t)d - (int32_t)s : (int32_t)d;
}
SD_INLINE int sd_lp_oct(int32_t r) { return r == 32768 ? 0 : r == 65536 ? 1 : r == 131072 ? 2 : -1; }
static uint32_t sd_lp_grid(const SdSpace *s, uint32_t gi, uint32_t ahead)   /* where its grid has the head at x 0.5,
                                                   1 or 2, ahead ticks on */
{
    uint32_t ln = s->pln, g = ahead ? (s->pG[gi] + ((ahead * (2048u << gi)) % ln)) % ln : s->pG[gi];   /* (ahead <
                                                   2^17: fits) */
    return s->pdir ? sd_lp_add(s->panc[1], -(int32_t)g, ln) : sd_lp_add(s->panc[0], (int32_t)g, ln);
}
static uint32_t sd_lp_glide_ticks(const SdEngine *e)
{
    uint32_t v = e->lp_glide, d;
    if (e->patch.s_mode & SD_SP_SYNC) {
        d = sd_lp_glide_div(v);
        return d ? sd_space_sync_q8(e->tempo_q8, d - 1u, 0, 0xFFFFFFFFu) >> 13 : 0u;   /* frames x 256 / 32 x 256 */
    }
    return sd_lp_glide_ms(v) * 1378u / 1000u;
}
static void sd_lp_glide_to(SdSpace *s, int32_t to, uint32_t ticks)   /* a glide from the speed now; at an octave,
                                                   its bend */
{
    int gi = sd_lp_oct(to);
    s->pg0 = s->pr; s->pgt = to; s->pgN = ticks; s->pgn = 0; s->pgP = 0; s->pgC = 0; s->pgCd = 0;
    s->ppend &= (uint8_t)~1u;
    if (s->pon && gi >= 0 && ticks) {
        uint32_t ln = s->pln, S = gi ? ln : ln / 2u, way = (uint32_t)(((int64_t)ticks * (s->pg0 + to)) >> 5) % ln;
        uint32_t land = sd_lp_add(s->pbl ? s->pk1 : s->pk, s->pdir ? -(int32_t)way : (int32_t)way, ln);
        uint32_t at = sd_lp_add(sd_lp_grid(s, (uint32_t)gi, ticks), s->pW, ln);
        uint32_t dr = (uint32_t)(to > s->pg0 ? to - s->pg0 : s->pg0 - to);
        int32_t R = sd_lp_near(at, land, S), cm = (int32_t)(((dr >> 5) * ticks) / 3u), f;   /* (a sixth of the
                                                   change's way: dr / 96 x ticks; 3072 x 33075 fits) */
        if (R <= cm && R >= -cm) s->pgC = R;                    /* it lands where its grid has it */
        else {
            f = sd_lp_near(at, land, ln / 32u);                 /* else on the grid's beats */
            s->pgC = f > cm ? cm : f < -cm ? -cm : f;
        }
    }
}
/* START with a loop (the user, of the looper and PLAY: "the loop stops when I stop playback, and starts again in
 * sync"): its head where it stood against the master's one. Looped while the transport ran, T samples after START,
 * its grid at the new START is where it would be T before LOOP, at x 0.5, 1 and 2 (-T x 128 << k round the loop: in
 * 32 bits, T taken round the loop first); looped while stopped, from its start on the one. Silent (parked), it is
 * there at once and fades in; playing, it crossfades there (~12 ms) */
static void sd_lp_sync(SdSpace *s)
{
    uint32_t ln = s->pln, a, r, k, at;
    int gi = sd_lp_oct(s->pr);
    if (!s->psync) { s->pT0 = 0; s->psync = 1; }
    a = s->pT0 % ln;
    for (k = 0; k < 3u; k++) {
        r = (a * (8u << k)) % ln * 16u % ln;    /* (a x 32 < 2^30, ln x 16 < 2^29) */
        s->pG[k] = r ? ln - r : 0u;
    }
    s->panc[0] = s->panc[1] = 0;
    k = gi >= 0 ? (uint32_t)gi : 1u;
    at = s->pdir ? sd_lp_add(0u, -(int32_t)s->pG[k], ln) : s->pG[k];
    s->pW = 0; s->pnud = 0;
    if (s->ppark || s->plev <= 0) { s->pk = at; s->pbl = 0; s->px = 0; }
    else if (!s->pbl) {                         /* playing: a crossfade there, or, in sync already, a nudge (the
                                                   same place crossfaded would swell) */
        int32_t R = sd_lp_near(at, s->pk, ln);
        if (R > 1024 || R < -1024) { s->pk1 = at; s->pbl = 1; s->px = 0; s->pxd = 2048; }
        else s->pnud = R;
    }
    s->ppark = 0;
}
/* LOOP synced while the tracks play: where it locks, as a player means it. The lines are the loop's LENGTH under a
 * bar, else the bars, from the master's one (the grid's clock time: a MIDI clock's too); the bars and the half bars
 * among them too when the LENGTH is shorter. The press means the line nearest it, weighted: a line passed counts 2/3
 * of one ahead (a press comes a little late, and one ahead holds the loop back while it waits), a bar line half of
 * an ordinary one and a half bar 3/4, so the one draws it. The looper records all the while, so a line passed is
 * there to loop from at once: its end on the line, the head on as far as the line is back. Returns the clock time
 * back to the line (negative), on to it (positive), 0 on it. Free, or stopped: 0 (it locks where it is pressed) */
static int32_t sd_lp_snap(const SdEngine *e, uint32_t llen)
{
    uint32_t u = e->seq_len_q8, bar = 16u * u, g = llen * 2u, t = e->grid_q8, k, best = 0xFFFFFFFFu;
    int32_t pick = 0;
    if (!u || g < 256u || !e->seq_run || !(e->patch.s_mode & SD_SP_SYNC)) return 0;
    if (g > bar) g = bar;
    for (k = 0; k < 3u; k++) {                  /* the loop's lines, the half bars, the bars */
        uint32_t st = k == 0u ? g : k == 1u ? bar / 2u : bar, w = k == 0u ? 4u : k == 1u ? 3u : 2u, b, a, sb, sa;
        if (k && st < g) continue;              /* (a half bar is a line of a loop as long) */
        b = t % st;                             /* since the line before */
        a = st - b;                             /* to the line after */
        if (b == 0u) return 0;
        sb = (b >> 8) * w * 2u;                 /* (samples: under 2^22 at 40 BPM, x 8 fits) */
        sa = (a >> 8) * w * 3u;
        if (sb < best) { best = sb; pick = -(int32_t)b; }
        if (sa < best) { best = sa; pick = (int32_t)a; }
    }
    return pick;
}

static void sd_lp_plan(SdEngine *e, SdSpace *s)
{
    int32_t sp = e->lp_speed + e->lp_mtq, to, adv, bend = 0, A, gi;   /* (TURING's semitones on the speed set) */
    uint32_t ticks = sd_lp_glide_ticks(e), ln, k, tnow;
    int run = e->seq_run != 0, start = (run && !s->prun) || e->seq_started, on;
    if (start) s->pT = 0;                       /* the transport: STOP parks the loop, START plays it in sync */
    tnow = s->pT;
    if (run) s->pT += 16u;
    if (s->pon && s->prun && !run && s->loop) s->ppark = 1;
    if (!s->loop) s->ppark = 0;                 /* (LOOP off, or L-LEVEL 0: out, and it records) */
    s->prun = (uint8_t)run;
    if (start && s->pon) sd_lp_sync(s);
    on = s->loop && !s->ppark;
    if (!on || s->pon || !run) s->pqw = 0;      /* (a lock waiting: LOOP off, or STOP, takes it back) */
    s->pback = 0;
    if (on && !s->pon) {                        /* LOOP: on the grid, synced while the tracks play (sd_lp_snap) */
        if (s->pqw) {                           /* waiting for its line: there in this sub-block, it locks */
            if (s->pq > e->clk_adv) { s->pq -= e->clk_adv; on = 0; }
            else s->pqw = 0;
        } else {
            int32_t d = sd_lp_snap(e, (uint32_t)s->llen);
            if (d > (int32_t)e->clk_adv) { s->pq = (uint32_t)d; s->pqw = 1; on = 0; }
            else if (d < 0) s->pback = (uint32_t)(-d) / 512u;   /* (clock time to half-rate samples) */
        }
    }
    sp = sp > 3072 ? 3072 : sp < -3072 ? -3072 : sp;
    to = (int32_t)sd_semis_mul(sp);
    if (s->pr < 32768 || s->pr > 131072) { s->pr = to; s->psp = (int16_t)sp; }   /* (the first tick) */
    if (on && !s->pon) {                        /* LOOP: from its start, its grid with it */
        uint32_t tot = sd_lp_total(e), lm = tot > SD_LP_XF + 64u ? (tot - SD_LP_XF - 32u) * 256u : 512u;
        s->pln = (uint32_t)s->llen < 512u ? 512u : (uint32_t)s->llen > lm ? lm : (uint32_t)s->llen;
        s->pon = 1; s->plev = 0; s->prst = 1; s->pk = s->pk1 = 0; s->pbl = 0; s->px = 0; s->pnud = 0; s->pW = 0;
        s->pG[0] = s->pG[1] = s->pG[2] = 0; s->panc[0] = s->panc[1] = 0;
        s->pdir = s->lrev; s->ppend = 0;
        s->pT0 = tnow; s->psync = (uint8_t)run;  /* (its place against the master's one, for the next START) */
        if (s->pback) {                         /* its end on a line passed: the head on as far, at its speed, as if
                                                   LOOP had come there (its grid with it); if its memory holds that */
            uint32_t bk = s->pback, k3;
            if (bk * 256u + s->pln > lm) bk = 0;
            s->pback = bk;
            if (bk) {
                s->pk = (bk * (uint32_t)(s->pr >> 4) >> 4) % s->pln;   /* (samples x the rate's way a sample x 256) */
                if (s->pdir) s->pk = (s->pln - s->pk) % s->pln;   /* (REVERSE set before: as far back from its end) */
                for (k3 = 0; k3 < 3u; k3++) s->pG[k3] = (bk * (128u << k3)) % s->pln;
                s->pT0 = tnow > bk ? tnow - bk : 0u;
            }
        }
        if (s->pgn < s->pgN) sd_lp_glide_to(s, s->pgt, s->pgN - s->pgn);   /* (a glide on its way: bent now) */
    }
    if (sp != s->psp) {                         /* a new speed */
        s->psp = (int16_t)sp;
        if (ticks) sd_lp_glide_to(s, to, ticks);
        else if (s->pon && sd_lp_oct(to) >= 0 && to != s->pr) { s->ppend |= 1u; s->ppr = to; s->pgN = s->pgn = 0; }
        else { s->pr = to; s->pgN = s->pgn = 0; s->ppend &= (uint8_t)~1u; }
    }
    ln = s->pon ? s->pln : 512u;
    adv = s->pr >> 4;                           /* 16 samples at the speed, x 256 */
    if (s->pgn < s->pgN) {                      /* gliding: its way and its bend to the tick's end (each a sum from
                                                   its start, so the whole is exact) */
        uint32_t n = s->pgn + 1u, u = (n << 16) / s->pgN;   /* (n < 2^16) */
        int64_t u2 = ((int64_t)u * u) >> 16, u3 = (u2 * u) >> 16, u4 = (u3 * u) >> 16;
        int32_t F = (int32_t)(u3 - u4 / 2), Sq = (int32_t)(3 * u2 - 2 * u3), P, C;   /* the smoothstep's integral;
                                                   it, Q16 */
        P = (int32_t)(((int64_t)n * s->pg0 * 65536 + (int64_t)(s->pgt - s->pg0) * s->pgN * F) >> 20);
        C = (int32_t)(((int64_t)s->pgC * Sq) >> 16);
        adv = P - s->pgP; s->pgP = P;
        bend = C - s->pgCd; s->pgCd = C;
        s->pgn = n;
        s->pr = s->pg0 + (int32_t)(((int64_t)(s->pgt - s->pg0) * Sq) >> 16);
        if (n >= s->pgN) { s->pr = s->pgt; s->pgN = s->pgn = 0; }
    } else if (s->ppend & 1u) {                 /* an octave at once: at its grid's next 16th of the loop */
        uint32_t q = ln / 16u, at = q ? (q - s->pG[1] % q) % q : 0u, s1;
        if (at < 4096u) {                       /* (in this tick: rate 1's way over it) */
            s1 = (at + 255u) >> 8;
            adv = (int32_t)(((int64_t)s1 * s->pr + (int64_t)(16u - s1) * s->ppr) >> 8);
            s->pr = s->ppr;
            s->ppend &= (uint8_t)~1u;
        }
    }
    gi = sd_lp_oct(s->pr);
    if (s->pon && s->lrev != s->pdir) {         /* REVERSE: the turn at its grid's next 64th (free: at once) */
        uint32_t q = ln / 64u, g = gi >= 0 ? (uint32_t)gi : 1u, h = 1024u << g, at = q ? (q - s->pG[g] % q) % q : 0u;
        if (gi < 0 || at < h || at + h >= q) {
            s->pdir = s->lrev;
            if (s->pdir) s->panc[1] = sd_lp_add(sd_lp_add(s->pbl ? s->pk1 : s->pk, -s->pW, ln), (int32_t)s->pG[g], ln);
                                                /* (from here, its wobble aside) */
        }
    }
    if (!e->lp_mon) s->pW = 0;                  /* (no slot sends free: the grid takes its wobble back) */
    if (e->lp_mfree) {                          /* the matrix's free speed: on the rate as it plays, +-12 at most */
        int32_t m = e->lp_mfree > 3072 ? 3072 : e->lp_mfree < -3072 ? -3072 : e->lp_mfree, w;
        w = (int32_t)(((int64_t)adv * sd_semis_mul(m)) >> 16);
        w = w < 1024 ? 1024 : w > 16384 ? 16384 : w;   /* (x 0.25 .. 4: SPEED's +-12 and its own, so a wobble at
                                                   -12 or +12 still goes both ways) */
        if (s->pon) s->pW += s->pdir ? adv - w : w - adv;
        if (s->pW > (int32_t)ln || s->pW < -(int32_t)ln) s->pW %= (int32_t)ln;   /* (round the loop) */
        adv = w;
    }
    A = (s->pdir ? -adv : adv) + bend;
    if (s->pnud) { int32_t d = s->pnud > 16 ? 16 : s->pnud < -16 ? -16 : s->pnud; A += d; s->pnud -= d; }
    /* this tick's plan, for its chunks */
    s->lt.k0 = s->pk; s->lt.k1 = s->pk1; s->lt.ln = ln; s->lt.adv = A;
    s->lt.x0 = s->px; s->lt.xd = s->pbl ? s->pxd : 0;
    s->lt.levd = (int8_t)(on ? (s->plev < 16384 ? 64 : 0) : s->pon ? -16 : 0);
    s->lt.lev0 = s->plev;
    s->lt.on = s->pon; s->lt.bl = s->pbl; s->lt.rst = s->prst; s->lt.back = s->prst ? s->pback : 0u;
    s->lti = 0;
    if (!s->pon) return;
    /* on to the next tick */
    s->pk = sd_lp_add(s->pk, A, ln);
    if (s->pbl) s->pk1 = sd_lp_add(s->pk1, A, ln);
    for (k = 0; k < 3u; k++) s->pG[k] = sd_lp_add(s->pG[k], (int32_t)(2048u << k), ln);
    {
        int32_t lv = s->plev + 16 * s->lt.levd;
        s->plev = (int16_t)(lv > 16384 ? 16384 : lv < 0 ? 0 : lv);
        if (!on && s->plev <= 0 && !s->ppark) { /* out: it records (an octave waiting: at once now); parked, it
                                                   waits silent for START */
            if (s->ppend & 1u) s->pr = s->ppr;
            s->pon = 0; s->pbl = 0; s->ppend = 0;
            return;
        }
    }
    if (s->pbl && (s->px = (int16_t)(s->px + s->pxd > 32767 ? 32767 : s->px + s->pxd)) >= 32767) {
        s->pk = s->pk1; s->pbl = 0; s->px = 0;  /* the crossfade done: the second head is the head */
    }
    if (!s->pbl && s->pgn >= s->pgN && !s->ppend && gi >= 0 && s->lrev == s->pdir) {   /* at an octave, settled:
                                                   its grid's place */
        int32_t R = sd_lp_near(sd_lp_add(sd_lp_grid(s, (uint32_t)gi, 0), s->pW, ln), s->pk, gi ? ln : ln / 2u);
        if (R > 1024 || R < -1024) {            /* a crossfade over: GLIDE long, at least a 16th of the loop */
            uint32_t xf = ln / 16u / 4096u;
            xf = xf > ticks ? xf : ticks;
            xf = xf < 28u ? 28u : xf;
            s->pk1 = sd_lp_add(s->pk, R, ln);
            s->pbl = 1; s->px = 0;
            s->pxd = (int16_t)(32767u / xf ? 32767u / xf : 1u);
        } else s->pnud = R;                     /* a few samples: nudged (16 a tick at most: 0.4 %) */
    }
}

static void sd_plate_join(SdEngine *e);
static void sd_space_params(SdEngine *e)
{
    const SdPatch *p = &e->patch;
    SdSpace *s = &e->sp;
    uint32_t k, gi = p->s_decay >> 9, gf = p->s_decay & 511u, t60, us;
    int32_t semis, step, rt;
    uint32_t lim = SD_SP_LIM, q;
    {   /* the looper armed (a level and its memory). Borrowing, the delay gives it the line's second half: the change
           (armed or let go) fades the echoes out over ~23 ms, clears the line and the looper's memory over ~80 ms
           (1024 a tick: at once it took the interrupt from 52 % to 79 %), the delay writing silence meanwhile, and
           fades what goes in again; letting go waits for the looper to be quiet */
        int want = sd_lp_total(e) > SD_LP_XF + 64u && e->lp_lvl != 0;
        if (!s->dmax) { s->dmax = SD_SP_DMAX; s->dfade = 32767; }
        if (!e->lp_borrow) { s->larm = (uint8_t)want; s->dmax = SD_SP_DMAX; s->dsw = 0; s->dfade = 32767; }
        else {
            if (!s->dsw && want != s->larm && (want || !s->pon)) s->dsw = 1;
            if (s->dsw == 1 && (s->dfade -= 1024) <= 0) { s->dfade = 0; s->dsw = 3; s->dclr = 0; }   /* out */
            if (s->dsw == 3) {                  /* the line, then the looper's other piece, cleared a little a tick */
                uint32_t i = s->dclr, end = i + 1024u, n0 = 2u * SD_SP_DMAX, to;
                uint32_t n1 = e->lpb ? (e->lpb_n < SD_LP_NMAX ? e->lpb_n : SD_LP_NMAX) : 0u;
                int16_t *dl = e->sm->dl;
                for (to = end < n0 ? end : n0; i < to; i++) dl[i] = 0;
                for (to = end < n0 + n1 ? end : n0 + n1; i < to; i++) e->lpb[i - n0] = 0;
                s->dclr = i;
                if (i >= n0 + n1) {             /* cleared: the change */
                    s->larm = (uint8_t)want;
                    s->dmax = want ? SD_SP_DHALF : SD_SP_DMAX;
                    s->dw = 0; s->lw = 0; s->lbase = 0;
                    s->lpd[0] = s->lpd[1] = 0;
                    s->dsw = 2;
                }
            }
            if (s->dsw == 2 && (s->dfade += 1024) >= 32767) { s->dfade = 32767; s->dsw = 0; }
        }
        lim = s->dmax == SD_SP_DHALF ? SD_SP_LIM2 : SD_SP_LIM;
    }
    if (p->s_mode & SD_SP_SYNC)                 /* a division of the note driver's sixteenth (samples x 256) */
        q = sd_space_sync_q8(e->tempo_q8, sd_sp_div(p->s_time), 0, lim);   /* (the tempo heard: a MIDI clock's too) */
    else                                        /* ms x 44.1 x 256 (longer than the line: halved, as synced) */
        for (q = sd_sp_ms(p->s_time) * 11290u; q > lim; q >>= 1) {}
    s->dtgt = (int32_t)q;
    if (s->dtgt > (int32_t)lim) s->dtgt = (int32_t)lim;
    if (s->dtgt < 4 * 256) s->dtgt = 4 * 256;
    if (!s->on || s->dsw == 2) s->dq = s->dnext = s->dtgt;   /* off until now, or the line just changed: at the
                                                   time, no glide */
    s->dq = s->dnext;
    s->dnext = s->dq + (s->dtgt - s->dq) / 16;  /* a tape: ~12 ms to most of the way */
    s->fb = (int32_t)((p->s_fb * 31130u) >> 16);                 /* 0 .. 0.95 */
    s->fb2 = (s->fb * s->fb) >> 15;                              /* (31130^2: fits) */
    s->dtone = 2621 + (int32_t)((p->s_dtone * 30146u) >> 16);    /* the lowpass's coefficient, 0.08 .. 1 */
    {                                           /* the looper: its length (as TIME: a division halved until it fits
                                                   its memory, or ms), its level; its head and its speed (a tape's:
                                                   the pitch and the length together), sd_lp_plan */
        uint32_t tot = sd_lp_total(e), lm = tot > SD_LP_XF + 64u ? (tot - SD_LP_XF - 32u) * 512u : 512u, lq, f = 0;
                                                /* (frames x 256: twice its half-rate samples) */
        if (p->s_mode & SD_SP_SYNC) lq = sd_space_sync_q8(e->tempo_q8, sd_lp_div(e->lp_len), &f, lm);
        else for (lq = sd_sp_ms(e->lp_len) * 11290u; lq > lm; lq >>= 1) f++;
        if (lq > lm) lq = lm;
        s->llen = (int32_t)(lq / 2u);           /* (half-rate samples x 256) */
        s->lfold = (uint8_t)f;
        s->llvl = s->larm ? e->lp_lvl >> 1 : 0;   /* (armed: recording, or playing) */
        s->loop = e->lp_on && s->llvl != 0;
        s->lrev = e->lp_rev != 0;
        sd_lp_plan(e, s);
    }
    s->dlvl = p->s_dlevel >> 1;
    s->rlvl = p->s_rlevel >> 1;
    s->pp = (p->s_mode & SD_SP_PP) != 0;
    s->tick++;
    if (e->sp_room && !s->room) {               /* one core from now: the room, its memory cleared (the plate's was
                                                   there), its lines at their lengths */
        uint32_t i;
        sd_plate_join(e);                       /* (a block still handed over: back first, or dropped) */
        if (e->sm) for (i = 0; i < sizeof e->sm->u.rm / 2u; i++) ((int16_t *)&e->sm->u.rm)[i] = 0;
        s->fw = 0;
        for (k = 0; k < 4u; k++) { s->apw[k] = 0; s->flp[k] = 0; s->flen[k] = 0; }
        s->racc = s->a0[0] = s->a0[1] = s->a1[0] = s->a1[1] = 0;
        s->room = 1;
    }
    if (s->room) {                              /* the room (§5.5, before the plate): SIZE its lines 0.25 .. 1 of
                                                   their longest, DECAY their gain, TONE a lowpass in each */
        int32_t sz = 8192 + (int32_t)((p->s_size * 24575u) >> 16);   /* 0.25 .. 1, Q15 */
        s->rtone = 2621 + (int32_t)((p->s_rtone * 30146u) >> 16);
        s->rtone = 2 * s->rtone - ((s->rtone * s->rtone) >> 15);   /* 1 - (1 - c)^2: the same corner at half rate */
        s->rg = (int32_t)SD_SP_G[gi] + ((((int32_t)SD_SP_G[gi + 1u] - (int32_t)SD_SP_G[gi]) * (int32_t)gf) >> 9);
        for (k = 0; k < 4u; k++) {              /* SIZE: the lines move a sample a sub-block toward it */
            s->ftgt[k] = (SD_SP_FMAX[k] * sz) >> 15;
            if (!s->on || !s->flen[k]) s->flen[k] = s->ftgt[k];
            else s->flen[k] += s->flen[k] < s->ftgt[k] ? 1 : s->flen[k] > s->ftgt[k] ? -1 : 0;
        }
        s->on = 1;
        return;
    }
    /* SIZE: 0.35 .. 1.75 x, gliding there (a 256th of x 1 a tick at most: the whole way in a quarter second) */
    s->stgt = SD_PL_SMIN + (int32_t)(((uint32_t)p->s_size * (uint32_t)(SD_PL_SMAX - SD_PL_SMIN)) >> 16);
    if (!s->on) s->sc = s->snext = s->stgt;
    s->sc = s->snext;
    step = (s->stgt - s->sc) / 8;
    step = step > 256 ? 256 : step < -256 ? -256 : step;
    s->snext = s->sc + (step ? step : s->stgt > s->sc ? 1 : s->stgt < s->sc ? -1 : 0);
    /* DECAY: the T60, whatever the SIZE: each pass of a branch (its span x SIZE) down by 10^(-1.5 k span / T60), as
       semitones down from 1 (sd_semis_mul's octaves). k 1.07, measured at 700 Hz on a burst at -7 dBFS: the law to
       within a fifth up to 9 s; the longest a little short (~30 s of 53): a tail's last LSBs go to the rounding
       toward zero, as to a noise floor */
    t60 = SD_PL_T60[gi] + (uint32_t)((((int32_t)SD_PL_T60[gi + 1u] - (int32_t)SD_PL_T60[gi]) * (int32_t)gf) >> 9);
    us = (uint32_t)(((uint64_t)SD_PL_BRANCH_US * (uint32_t)s->snext) >> 16);   /* (<= 635 ms) */
    semis = (int32_t)(us * 16u / t60);          /* 59.8 x 1.07 / 1000 x 256 = 16.4 a us a ms (<= 33900) */
    semis = semis > 30720 ? 30720 : semis;      /* (10 octaves down: 0.001) */
    s->g = (int32_t)(sd_semis_mul(-semis) >> 2);
    s->g = s->g > 16383 ? 16383 : s->g;
    rt = 2621 + (int32_t)((p->s_rtone * 30146u) >> 16);   /* TONE: the damping, 0.08 .. 1 */
    s->c = (2 * rt - ((rt * rt) >> 15)) >> 1;   /* 1 - (1 - c)^2: the same corner at half rate */
    s->sg = (int32_t)SD_SP_SHIM(p) * 52;        /* SHIMMER: 0 .. 0.2 of the way round an octave up (the pitch played
                                                   loses (1 - sg) a pass: at 0.5 it was gone in seconds) */
    if (s->sg) {                                /* the octave and the rest share the way round as (1 - sg) and sg:
                                                   never more than the decay lets through, whatever their phases
                                                   (an equal-power mix ran away where they agreed). Uncorrelated
                                                   they lose power, sqrt((1 - sg)^2 + 2/3 sg^2) (the shifter's
                                                   windows: 2/3): the decay is raised by as much, to 1 at most */
        uint32_t q = 0, bit = 1u << 14, r = (uint32_t)(16384 - s->sg), v = r * r + (uint32_t)(s->sg * s->sg) * 2u / 3u;
        for (k = 0; k < 15u; k++, bit >>= 1) if ((q + bit) * (q + bit) <= v) q += bit;   /* (15 steps: its root) */
        v = (uint32_t)s->g * 16384u / (q ? q : 1u);
        s->g = (int32_t)(v > 16383u ? 16383u : v);
    }
    s->frz = e->sp_freeze;
    if (s->frz) { s->g = 16384; s->c = 16384; s->sg = 0; }   /* FREEZE: the tail as it is, for ever (x 1 exactly:
                                                   nothing rounded off) */
    s->d2 = s->g + 2458;                        /* the second allpasses: DECAY + 0.15, 0.25 .. 0.5 */
    s->d2 = s->d2 < 4096 ? 4096 : s->d2 > 8192 ? 8192 : s->d2;
    for (k = 0; k < SD_PL_SH; k++) s->di[k] = (SD_PL_BASE[k] * (uint32_t)s->snext + 32768u) >> 16;
    for (k = 0; k < 7u; k++) {
        s->ti[k] = (SD_PL_TL[k] * (uint32_t)s->snext + 32768u) >> 16;
        s->ti[7u + k] = (SD_PL_TR[k] * (uint32_t)s->snext + 32768u) >> 16;
    }
    s->lfo[0] += 136348u * 16u;                 /* ~0.7 Hz and ~0.87 Hz, each its own phase, a step a tick (16 plate
                                                   samples: the length moves 0.04 of a sample a tick at most) */
    s->lfo[1] += 169460u * 16u;
    s->md += s->frz ? (s->md > 0 ? -1 : 0) : (s->md < 256 ? 1 : 0);
    for (k = 0; k < 2u; k++) {                  /* up to 12 samples x SIZE, x the depth; at depth 0 a whole sample */
        uint32_t q = SD_PL_BASE[SD_PL_A1 + k] * (uint32_t)s->snext, w = (q + 32768u) >> 16;
        int32_t f = (int32_t)(q >> 8) - (int32_t)(w << 8) +   /* (the fraction off the whole, -128..127, and the LFO) */
                    (int32_t)(((int64_t)(sd_pl_sin(s->lfo[k]) + 32768) * ((3072 * (int64_t)s->snext) >> 16)) >> 16);
        f *= s->md;                             /* (|f| < 2^14, x 256: fits) */
        s->ma[k] = (w << 8) + (uint32_t)(f < 0 ? -((-f) >> 8) : f >> 8);
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

static uint32_t sd_seq_xn(const SdEngine *e, const SdSeqT *t, uint32_t n)   /* a note of a step, TURING's NOTES
                                                   on it, transposed */
{
    int32_t v, x = t->xpose + t->orb_x;
    if (t->tur_deg) {                           /* NOTES: degrees of the pattern's scale, from its key (step 1) */
        uint32_t key = t->pat.step[0].note, sc = t->pat.scale;
        n = sd_scale_note(key, sc, sd_scale_degree(key, sc, n) + t->tur_deg);
    }
    if (x && e->xkey) {                         /* IN KEY: as many degrees of A's scale as its key is from the key
                                                   the transpose names; a note off the scale keeps its step off it */
        const SdPattern *a = &e->t[0].pat;
        uint32_t key = a->step[0].note, sc = a->scale;
        int32_t to = (int32_t)key + x, d = sd_scale_degree(key, sc, n);
        int32_t off = (int32_t)n - (int32_t)sd_scale_note(key, sc, d);
        to = to < 0 ? 0 : to > 127 ? 127 : to;
        v = (int32_t)sd_scale_note(key, sc, d + sd_scale_degree(key, sc, (uint32_t)to)) + off;
    } else {
        v = (int32_t)n + x;
    }
    return (uint32_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

/* REC writes what is heard: the note n a key plays,
 * back by the track's transpose (SCL + a key, ORBIT's XPOSE), so sd_seq_xn plays n again. IN KEY: back by as many
 * degrees of A's scale (a note off the scale keeps its step off it, as there); CHROM: by the semitones. TURING's NOTES
 * is a modulation, not taken back */
static uint32_t sd_seq_rec_note(const SdEngine *e, const SdSeqT *t, uint32_t n)
{
    int32_t v, x = t->xpose + t->orb_x;
    if (!x) return n;
    if (e->xkey) {
        const SdPattern *a = &e->t[0].pat;
        uint32_t key = a->step[0].note, sc = a->scale;
        int32_t to = (int32_t)key + x, d = sd_scale_degree(key, sc, n);
        int32_t off = (int32_t)n - (int32_t)sd_scale_note(key, sc, d);
        to = to < 0 ? 0 : to > 127 ? 127 : to;
        v = (int32_t)sd_scale_note(key, sc, d - sd_scale_degree(key, sc, (uint32_t)to)) + off;
    } else {
        v = (int32_t)n - x;
    }
    return (uint32_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

/* ORBIT's transposes on both tracks, from its step (0 when it is off) */
static void sd_orb_xpose(SdEngine *e)
{
    const SdOrbit *o = &e->orb;
    uint32_t s = e->orb_step % SD_ORB_STEPS;
    e->t[0].orb_x = o->on && e->orb_live ? o->xa[s] : 0;
    e->t[1].orb_x = o->on && e->orb_live ? o->xb[s] : 0;
}
/* at the master's one: its loop measured (the glide's span), and after RATE loops the next step */
static void sd_orb_one(SdEngine *e)
{
    const SdOrbit *o = &e->orb;
    e->orb_loop_sb = e->orb_loop_t;
    e->orb_loop_t = 0;
    if (o->on && !e->orb_live) {                /* switched on while the master played: in now, from its step 1 */
        e->orb_live = 1;
        e->orb_step = e->orb_loops = 0;
        e->orb_t = 0;
    } else if (o->on && ++e->orb_loops >= SD_ORB_LOOPS(o)) {
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
    e->tur_pos_q8 += e->clk_adv;
}

static void sd_seq_on(SdEngine *e, SdSeqT *t, const SdStep *st, uint32_t vel)   /* the step's note and its chord */
{
    uint32_t i, n = 1, root = sd_seq_xn(e, t, st->note);
    if (t->quiet) return;                       /* OFF (or B in MONO): it keeps time, silent */
    t->seq_note[0] = (uint8_t)root;
    for (i = 0; i < 3u; i++)                    /* the chord: offsets from the step's note (0: none), each tone
                                                   transposed on its own (IN KEY: a minor chord may come out major) */
        if (st->chord[i]) {
            int32_t c = (int32_t)st->note + st->chord[i];
            if (c >= 0 && c <= 127) t->seq_note[n++] = (uint8_t)sd_seq_xn(e, t, (uint32_t)c);
        }
    if (n > e->quota[t->src]) n = e->quota[t->src];   /* (a chord past the track's voices: its top notes go) */
    for (i = 0; i < n; i++) sd_voice_on(e, t->seq_note[i], vel, t->src);
    t->seq_nn = (uint8_t)n;
    t->seq_note_on = 1;
}

static void sd_seq_take(SdEngine *e, SdSeqT *t, uint32_t sq)   /* the main loop's pattern, the audio's now */
{
    t->pat_taken = sq;
    t->pat = t->pat_in;
    t->width = (uint8_t)sd_pattern_width(&t->pat);
    sd_plan(e);                                 /* (its width may have changed the quotas) */
}
static void sd_seq_restart(SdEngine *e, SdSeqT *t)   /* at the master's one: from step 1 now, lined up on its */
{
    if (t->seq_note_on) sd_seq_off(e, t);
    sd_seq_start(t);
    t->sq_pos = 0xFEu;                          /* (the next step: step 1, at the one) */
    t->seq_count = 0xFFFFFFFFu;
    t->seq_step_q8 = e->seq_len_q8;
    t->seq_pos_q8 = t->seq_step_q8 + e->a_rem;
}

static void sd_seq_track(SdEngine *e, SdSeqT *t, uint32_t run, uint32_t k)   /* track k (0 A, 1 B), once a
                                                   sub-block: start, stop, steps, gates */
{
    uint32_t sq = t->pat_seq, lead = k == e->lead, back = 0;
    if (t->mute || (k && e->mode_on == SD_MODE_MONO)) t->in = 0;   /* (B needs DUO or POLY) */
    else if (lead) t->in = 1;                   /* the master: at once */
    else if (!t->in && (!e->a_wait || e->a_one)) { t->in = 1; back = e->a_one; }   /* the other: at the master's one,
                                                   from its step 1, when switched on while the master plays */
    t->quiet = (uint8_t)!t->in;
    if (t->quiet && t->seq_note_on) sd_seq_off(e, t);
    if (!(sq & 1u) && sq != t->pat_taken) {     /* a new pattern: from the next step on (the traversal keeps its
                                                   place; a pass past the new end restarts at A) */
        sd_seq_take(e, t, sq);
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
            if (lead) {                         /* ORBIT: its first step, the master's loop as its length says
                                                   (until one) */
                uint32_t d = t->pat.div < SD_SDIVS ? t->pat.div : 2u;
                e->seq_started = 1;             /* (the synced LFOs and MOVE: their cycles from this one) */
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
    if (back) sd_seq_restart(e, t);
    if (t->seq_pos_q8 >= t->seq_step_q8) {      /* a new step: it starts at this sub-block's first frame */
        const SdStep *st;
        uint32_t one, next;
        t->seq_pos_q8 -= t->seq_step_q8;
        if (t->sq_pos == 0xFFu) { t->sq_pos = 0; one = 0; }
        else if (t->sq_pos == 0xFEu) { t->sq_pos = 0; one = 1; }   /* (in at the master's one: step 1) */
        else one = sd_seq_advance(t);
        if (one && lead) { e->a_one = 1; e->a_rem = t->seq_pos_q8; }   /* the master's one: what waits comes in */
        if (!t->sq_n[t->sq_cur]) {              /* every step skipped: silence */
            if (t->seq_note_on) sd_seq_off(e, t);
            t->seq_step_q8 = e->seq_len_q8;
            t->seq_pos_q8 += e->clk_adv;
            return;
        }
        if (one) t->xpose = t->xpose_next;      /* SCL + a key: the transpose at the one */
        if (one && lead) sd_orb_one(e);         /* ORBIT: its step at the master's one (the other's notes take it
                                                   from its next step) */
        t->tur_deg = (int8_t)(!k ? sd_tur_degrees(e) : 0);   /* TURING's NOTES: this step's place (A) */
        t->seq_count++;
        t->seq_step = t->sq_ord[t->sq_cur][t->sq_pos];
        t->seq_shown = t->seq_step;
        t->seq_seg_shown = t->sq_seg;
        sd_seq_steplen(e, t);
        if (t->seq_pos_q8 >= t->seq_step_q8) t->seq_pos_q8 = 0;   /* (the tempo jumped: no catching up) */
        if (lead) sd_reactor_clock(e);
        st = &t->pat.step[t->seq_step];
        t->rat_q8 = 0;
        if (st->type <= SD_ST_SLIDE && st->prob < 100u && sd_seq_rand(&t->sq_rng) % 100u >= st->prob) {
            if (t->seq_note_on) sd_seq_off(e, t);   /* PROB said no: a rest this time */
            t->seq_pos_q8 += e->clk_adv;
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
            sd_seq_glide(e, t, sd_seq_xn(e, t, st->note), st->vel);   /* (a single note glides; a chord plays as
                                                   a NOTE) */
            t->seq_note[0] = (uint8_t)sd_seq_xn(e, t, st->note);
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
    t->seq_pos_q8 += e->clk_adv;
}

static void sd_seq_tick(SdEngine *e)            /* once a sub-block: both tracks, the master first */
{
    sd_clk_tick(e);                             /* first: the MIDI clock's pulses, this sub-block's clock time */
    e->seq_started = 0;                         /* (set by the master's START below; sd_mod_tick reads it after) */
    if (e->clk_start != e->clk_start_seen) {    /* a MIDI START (or CONTINUE) at its first pulse: from step 1 */
        uint32_t i;
        e->clk_start_seen = e->clk_start;
        for (i = 0; i < 2u; i++) {
            if (e->t[i].seq_note_on) sd_seq_off(e, &e->t[i]);
            e->t[i].seq_on = 0;
        }
    }
    sd_seq_retime(e);                           /* then: START measures step 1 in this tempo */
    {                                           /* the master: what waits for its one, while it plays steps */
        const SdSeqT *a;
        e->lead = (uint8_t)(e->master && e->seqb ? 1u : 0u);
        a = &e->t[e->lead];
        e->a_wait = (uint8_t)(e->seq_run && a->seq_on && a->sq_n[a->sq_cur]);
        e->a_one = 0;
    }
    sd_tur_tick(e, e->seq_run && !e->t[0].seq_on);   /* TURING first: a step at the same time sees its new place */
    {                                           /* ORBIT: a new one from the main loop (its step kept), its clocks */
        uint32_t sq = e->orb_seq;
        if (!(sq & 1u) && sq != e->orb_taken) {
            e->orb_taken = sq;
            e->orb = e->orb_in;
            if (e->orb_step >= SD_ORB_LEN(&e->orb)) e->orb_step = 0;
            sd_orb_xpose(e);
        }
        if (!e->orb.on) e->orb_live = 0;        /* (off: at once; on: at A's one, sd_orb_one) */
        else if (!e->orb_live && !e->a_wait) { e->orb_live = 1; sd_orb_xpose(e); }
        e->orb_t++;
        e->orb_loop_t++;
    }
    if (e->lead) sd_seq_track(e, &e->t[1], e->seq_run && e->seqb, 1);   /* the master first: its one is known
                                                   when the other comes */
    sd_seq_track(e, &e->t[0], e->seq_run, 0);
    if (!e->lead && (e->seqb || e->t[1].seq_on)) sd_seq_track(e, &e->t[1], e->seq_run && e->seqb, 1);
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
    if (e->mode_on == SD_MODE_MONO && e->patch.course == SD_COURSE_LOOP && (e->seq_started || !e->course_run))
        sd_course_start(e);                     /* LOOP: from the master's START, or the first time it runs */
    if (e->course_run && e->patch.course && e->mode_on == SD_MODE_MONO) {   /* COURSE: between two cells */
        const SdPatch *p = &e->patch;
        uint32_t a = e->course_seg & 3u, nx = (a + 1u) & 3u, f = e->course_ph >> 5;   /* Q15 */
        int loop = p->course == SD_COURSE_LOOP;
        if (loop && !e->course_step) {          /* (ON held at D, now LOOP: on round from there) */
            e->course_seg = 2; e->course_ph = (1u << 20) - 1u; e->course_step = 1;
        }
        x = (uint32_t)((int32_t)p->cell_x[a] + (((int32_t)p->cell_x[nx] - (int32_t)p->cell_x[a]) * (int32_t)f >> 15));
        y = (uint32_t)((int32_t)p->cell_y[a] + (((int32_t)p->cell_y[nx] - (int32_t)p->cell_y[a]) * (int32_t)f >> 15));
        e->course_ph += e->course_step;
        if (e->course_ph >= 1u << 20) {            /* the next cell: ON holds the last one, LOOP goes on round */
            if (e->course_seg < 2u || (loop && e->course_seg == 2u)) {
                uint32_t ms;
                e->course_seg++;
                e->course_ph = 0;
                ms = p->cell_ms[e->course_seg < 3u ? e->course_seg : 0u];
                e->course_step = 335544320u / ((ms ? ms : 1u) * 441u);
            } else if (loop) {                  /* D -> A done: round again */
                uint32_t ms = p->cell_ms[0] ? p->cell_ms[0] : 1u;
                e->course_seg = 0;
                e->course_ph = 0;
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
    uint32_t inc, old = e->lfo_ph[k], wrap;
    if (p->lfo_mode[k] & SD_LM_SYNC) {          /* a division of the sixteenth, on the clock time: exact (it keeps to
                                                   the steps for good), from the one at START, with a MIDI clock its
                                                   pulses' */
        uint32_t cyc = e->seq_len_q8 * SD_LFO_DIV[(e->lfor[k] * SD_LFO_DIVS) >> 16], ct;   /* (<= 4.3 M x 64) */
        if (cyc != e->lfo_cyc[k]) {             /* a new division or tempo (or SYNC just now): the same place */
            e->lfo_ct[k] = (uint32_t)(((uint64_t)old * cyc) >> 32);
            e->lfo_cyc[k] = cyc;
            e->lfo_inv[k] = 0xFFFFFFFFu / cyc;
        }
        ct = e->lfo_ct[k];
        wrap = e->seq_started || (e->m_trig && (p->lfo_mode[k] & (SD_LM_KEY | SD_LM_ONE)));
        if (wrap) ct = 0;                       /* restart: this tick on 0 */
        else if ((ct += e->clk_adv) >= cyc) {
            if (p->lfo_mode[k] & SD_LM_ONE) ct = cyc - 1u;   /* ONE: held at the end */
            else {
                ct -= cyc;
                if (ct >= cyc) ct = 0;
                wrap = 1;
            }
        }
        e->lfo_ct[k] = ct;
        e->lfo_ph[k] = ct * e->lfo_inv[k];      /* (ct < cyc: under 2^32) */
    } else {
        uint32_t i = e->lfor[k] >> 9, f = e->lfor[k] & 511u;
        inc = SD_LFO_INC[i] + (uint32_t)(((uint64_t)(SD_LFO_INC[i + 1u] - SD_LFO_INC[i]) * f) >> 9);
        e->lfo_cyc[k] = 0;
        if (e->m_trig && (p->lfo_mode[k] & (SD_LM_KEY | SD_LM_ONE))) old = 0u - inc;   /* restart: this tick lands on 0 */
        else if ((p->lfo_mode[k] & SD_LM_ONE) && old + inc < old) inc = 0xFFFFFFFFu - old;   /* ONE: held at the end */
        e->lfo_ph[k] = old + inc;
        wrap = e->lfo_ph[k] < old || (e->m_trig && (p->lfo_mode[k] & (SD_LM_KEY | SD_LM_ONE)));
    }
    if (wrap) {                                 /* a new cycle: S&H draws */
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
static void sd_add7(uint8_t *f, int32_t a)     /* an envelope's 0..127, by a (Q15: 32767 the whole range) */
{
    int32_t v = (int32_t)*f + ((a * 127) >> 15);
    *f = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
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
    p->e1_a = b->e1_a; p->e1_d = b->e1_d; p->e1_s = b->e1_s; p->e1_r = b->e1_r;
    p->e2_a = b->e2_a; p->e2_d = b->e2_d; p->e2_s = b->e2_s; p->e2_r = b->e2_r;
    p->e3_a = b->e3_a; p->e3_d = b->e3_d; p->e3_s = b->e3_s; p->e3_r = b->e3_r;
    p->f_env = b->f_env; p->d_ftime = b->d_ftime; p->d_spread = b->d_spread; p->s_time = b->s_time;
    p->lfo_rate[0] = b->lfo_rate[0]; p->lfo_rate[1] = b->lfo_rate[1];
    e->m_dirty = 0;
}

/* TURING to PITCH, quantized first to the key and root: note n moved by t (semitones x 256) lands on the nearest
 * note of SEQ A's scale from its key (the
 * lower of two as near; CHROM: the nearest semitone) -> the semitones from n. Each voice from its own note, so all
 * of them stay in the key; the other sources to PITCH (a vibrato) on top, as they are */
static int32_t sd_tq_snap(const SdEngine *e, uint32_t n, int32_t t)
{
    const SdPattern *a = &e->t[0].pat;
    uint32_t m = SD_SCALE_MASK[a->scale < SD_SCALES ? a->scale : SD_SC_CHROM];
    int32_t x = (int32_t)n * 256 + t, c = (x + 128) >> 8, q = c - 6, best = c, bd = 0x7FFFFFFF;
    int32_t r = (q - (int32_t)(a->step[0].note % 12u)) % 12;   /* (the place in the scale: once, then counted) */
    if (r < 0) r += 12;
    for (; q <= c + 6; q++) {                   /* (a scale's widest gap is 4: 13 places find it) */
        int32_t d = q * 256 - x;
        d = d < 0 ? -d : d;
        if ((m >> r & 1u) && d < bd) { bd = d; best = q; }
        if (++r == 12) r = 0;
    }
    return best - (int32_t)n;
}

static void sd_mod_tick(SdEngine *e)
{
    SdPatch *p = &e->patch;
    int32_t acc[SD_MDSTS], src[SD_MSRCS], m[4], semis = 0, tq = 0, lq = 0;
    int32_t tst = SD_TUR_ON(p) ? e->tur_out : 0;   /* TURING's step, unglided: to PITCH it is quantized */
    uint32_t i, k, any = 0, mon = 0;
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
        if (s->src == SD_MS_TUR && s->dst == SD_MD_PITCH) tq += (tst * s->amt) >> 15;   /* (quantized, below) */
        else if (s->src == SD_MS_TUR && s->dst == SD_MD_LSPEED) lq += (tst * s->amt) >> 15;
        else acc[s->dst] += (src[s->src] * s->amt) >> 15;
        if (s->dst == SD_MD_LSPEED && s->src != SD_MS_TUR) mon = 1;
        any = 1;
    }
    if (SD_TUR_ON(p) && p->tur_dst && p->tur_dst < SD_MDSTS && p->tur_amt) {   /* TURING's own DEST and AMOUNT */
        if (p->tur_dst == SD_MD_PITCH) tq += (tst * ((int32_t)p->tur_amt * 327)) >> 15;
        else if (p->tur_dst == SD_MD_LSPEED) lq += (tst * ((int32_t)p->tur_amt * 327)) >> 15;
        else acc[p->tur_dst] += (src[SD_MS_TUR] * ((int32_t)p->tur_amt * 327)) >> 15;
        any = 1;
    }
    tq = tq < -32767 ? -32767 : tq > 32767 ? 32767 : tq;
    e->tq = (tq * 24 * 256) >> 15;              /* semitones x 256, +-24, as PITCH's */
    {                                           /* ORBIT, while A plays: the lanes, the macros' changes (gliding:
                                                   from this step's value to the next's through the step) */
        const SdOrbit *o = &e->orb;
        int32_t om[4] = {0, 0, 0, 0};
        if (o->on && e->orb_live && e->t[e->lead].seq_on) {
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
    if (!any) { e->pmul = 65536u; e->psemis = 0; e->lp_mfree = e->lp_mtq = 0; e->lp_mon = 0; return; }   /* nothing:
                                                   the patch as it is */
    e->m_dirty = 1;                             /* (sd_mod_restore must know every field written below) */
    /* the macros: offsets around the patch as programmed (spec §8); 0 is the patch itself */
    if (m[0]) {                                 /* MUTATE: the voice's complexity */
        acc[SD_MD_XM] += sd_mw(m[0], 12000); acc[SD_MD_WARP] += sd_mw(m[0], 6000);   /* (C1 first: XM 20000, WP 13000,
                                                   FD 16000: a chain at full went half to aliasing noise) */
        acc[SD_MD_FOLD] += sd_mw(m[0], 7000); acc[SD_MD_DT] += sd_mw(m[0], 6000);
    }
    if (m[1]) {                                 /* MOVE: animation in steps on the beat (stepped random, something
                                                   rhythmic, over smooth sines):
                                                   the nav and the Reactor's POS jump to new places each quarter,
                                                   glided ~10 ms */
        if (e->seq_started) e->move_t = 4u * e->seq_len_q8;   /* START: a new place on the one */
        else e->move_t += e->clk_adv;
        if (e->move_t >= 4u * e->seq_len_q8) {
            e->move_t -= 4u * e->seq_len_q8;    /* (the remainder kept: on the beat for good) */
            if (e->move_t >= 4u * e->seq_len_q8) e->move_t = 0;
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
                                                   bands first, the lows nearly clean (it works best on the
                                                   top) */
        acc[SD_MD_RDRIVE] += sd_mw(m[2], 6000);   /* (12000: FUSION, its drive low, +3 dB louder) */
        acc[SD_MD_DMIX] += sd_mw(m[2], 15000);   /* (no feedback: on a patch without its own, BREAK's 1 ms loop
                                                   howled on COMET and ION) */
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
    {                                           /* the looper's SPEED: free (the slots, ORBIT's lanes), and TURING's
                                                   to the nearest semitone */
        int32_t a = acc[SD_MD_LSPEED], q;
        const SdOrbit *o = &e->orb;
        if (o->on && (o->dst[0] == SD_MD_LSPEED || o->dst[1] == SD_MD_LSPEED)) mon = 1;
        a = a < -32767 ? -32767 : a > 32767 ? 32767 : a;
        lq = lq < -32767 ? -32767 : lq > 32767 ? 32767 : lq;
        q = (lq * 12 * 256) >> 15;
        e->lp_mfree = (a * 12 * 256) >> 15;
        e->lp_mtq = (q < 0 ? -((-q + 128) >> 8) : (q + 128) >> 8) * 256;
        e->lp_mon = (uint8_t)mon;
        acc[SD_MD_LSPEED] = 0;
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
        case SD_MD_DHIGH: sd_add16(&p->d_gain[3], d); break;   /* (GAIN 4, the top band's) */
        case SD_MD_DGAIN1: case SD_MD_DGAIN2: case SD_MD_DGAIN3: sd_add16(&p->d_gain[i - SD_MD_DGAIN1], d); break;
        case SD_MD_DSPLIT1: case SD_MD_DSPLIT2: case SD_MD_DSPLIT: {   /* a crossover, +-24 semitones (D SPLIT the
                                                   upper), within DISTROY's range; past another, through it (below) */
            uint32_t s = i == SD_MD_DSPLIT ? 2u : i - SD_MD_DSPLIT1;
            int32_t v = (int32_t)p->d_split[s] + ((a * 24 * 256) >> 15), lo = SD_R_LO * 256 + 256, hi = SD_R_HI * 256 - 256;
            p->d_split[s] = (uint16_t)(v < lo ? lo : v > hi ? hi : v);
            break;
        }
        case SD_MD_FCUT: sd_add16(&p->f_cut, (int32_t)(((int64_t)a * 34492) >> 15)); break;   /* +-60 semitones of the 114 at 100 % */
        case SD_MD_FRES: sd_add16(&p->f_res, d); break;
        case SD_MD_E1A: sd_add7(&p->e1_a, a); break;
        case SD_MD_E1D: sd_add7(&p->e1_d, a); break;
        case SD_MD_E1S: sd_add7(&p->e1_s, a); break;
        case SD_MD_E1R: sd_add7(&p->e1_r, a); break;
        case SD_MD_E2A: sd_add7(&p->e2_a, a); break;
        case SD_MD_E2D: sd_add7(&p->e2_d, a); break;
        case SD_MD_E2S: sd_add7(&p->e2_s, a); break;
        case SD_MD_E2R: sd_add7(&p->e2_r, a); break;
        case SD_MD_E3A: sd_add7(&p->e3_a, a); break;
        case SD_MD_E3D: sd_add7(&p->e3_d, a); break;
        case SD_MD_E3S: sd_add7(&p->e3_s, a); break;
        case SD_MD_E3R: sd_add7(&p->e3_r, a); break;
        case SD_MD_FENV: sd_adds16(&p->f_env, a); break;   /* +-60 semitones of depth at 100 % */
        case SD_MD_RSLEW: sd_add16(&p->r_slew, d); break;
        case SD_MD_DTIME: sd_add16(&p->d_ftime, d); break;
        case SD_MD_DSPREAD: sd_add16(&p->d_spread, d); break;
        case SD_MD_STIME: sd_add16(&p->s_time, d); break;   /* (free: it glides, a tape; synced: a division) */
        case SD_MD_SDTONE: sd_add16(&p->s_dtone, d); break;
        case SD_MD_SSIZE: sd_add16(&p->s_size, d); break;
        case SD_MD_SRTONE: sd_add16(&p->s_rtone, d); break;
        case SD_MD_L1RATE: sd_add16(&p->lfo_rate[0], d); break;
        case SD_MD_L2RATE: sd_add16(&p->lfo_rate[1], d); break;
        default: break;                         /* (SD_MD_LSPEED: above) */
        }
    }
    {                                           /* the crossovers sorted: one moved past another goes through it, the
                                                   band between them closing to nothing and opening again on its other
                                                   side; each band keeps its place and its destruction (unmoved, they
                                                   are in order already: the patch keeps them so) */
        uint16_t *s = p->d_split, t;
        if (s[0] > s[1]) { t = s[0]; s[0] = s[1]; s[1] = t; }
        if (s[1] > s[2]) { t = s[1]; s[1] = s[2]; s[2] = t; }
        if (s[0] > s[1]) { t = s[0]; s[0] = s[1]; s[1] = t; }
    }
    e->psemis = semis;
    e->pmul = semis ? sd_semis_mul(semis) : 65536u;   /* 2^(semis / 12): octaves, semitones, cents */
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

static void sd_arp_hpush(SdEngine *e, uint32_t note, uint32_t vel)   /* a step into KEEP's ring */
{
    e->arp_hn[e->arp_hw] = (uint8_t)note;
    e->arp_hv[e->arp_hw] = (uint8_t)vel;
    e->arp_hw = (uint8_t)((e->arp_hw + 1u) % SD_PSTEPS);
    if (e->arp_hc < SD_PSTEPS) e->arp_hc++;
}

static void sd_arp_tick(SdEngine *e)            /* once a sub-block */
{
    uint32_t n = e->arp_n, oct = e->arp_oct < 1u ? 1u : e->arp_oct > 4u ? 4u : e->arp_oct, len, k, i;
    uint32_t span = e->seq_len_q8 * 48u;        /* (<= 48 x 4.3 M at 40 BPM: fits) */
    uint8_t s[8], v[8];
    if (e->seq_started) e->grid_q8 = 0;         /* the master started this sub-block: the grid's one */
    else {
        e->grid_q8 += e->grid_adv;
        if (span && e->grid_q8 >= span) e->grid_q8 %= span;
    }
    e->grid_adv = e->clk_adv;
    if (!e->arp_on) {
        if (e->arp_was) {                       /* off: its note off, the list gone */
            if (e->arp_playing) sd_voice_off(e, e->arp_cur, SD_SRC_ARP);
            e->arp_playing = 0; e->arp_n = 0; e->arp_keys = 0; e->arp_was = 0;
        }
        return;
    }
    if (!e->arp_was) { e->arp_hc = 0; e->arp_since_q8 = 0; }   /* on: KEEP's ring from now */
    e->arp_was = 1;
    if (e->arp_since_q8 < 0x40000000u) e->arp_since_q8 += e->clk_adv;
    if (!n) {
        if (e->arp_playing) { sd_voice_off(e, e->arp_cur, SD_SRC_ARP); e->arp_playing = 0; }
        return;
    }
    {
        uint32_t d = e->arp_div < SD_SDIVS ? e->arp_div : 2u, g = e->arp_gate < 1u ? 1u : e->arp_gate > 100u ? 100u : e->arp_gate;
        uint32_t gate;
        e->arp_step_q8 = e->seq_len_q8 * SD_SDIV_NUM[d] / SD_SDIV_DEN[d];
        gate = (e->arp_step_q8 >> 7) * g / 100u << 7;
        if (e->seq_run && e->arp_step_q8) {     /* on the grid: its place in the step, exactly (a 32-bit divide a
                                                   tick); a step where the place came round */
            uint32_t ph = e->grid_q8 * SD_SDIV_DEN[d] % (e->seq_len_q8 * SD_SDIV_NUM[d]) / SD_SDIV_DEN[d];
            uint32_t round = ph < e->arp_ph;
            e->arp_ph = ph;
            if (e->arp_playing && ph >= gate && e->arp_pos_q8 != 0xFFFFFFFFu) {
                sd_voice_off(e, e->arp_cur, SD_SRC_ARP);    /* the gate, from the grid's point */
                e->arp_playing = 0;
            }
            if (e->arp_pos_q8 == 0xFFFFFFFFu) { /* a new hand: on the grid. Just after a point (under a quarter of
                                                   a step): now, the next on the grid; later: at the next point */
                e->arp_pos_q8 = ph;
                if (ph >= e->arp_step_q8 / 4u) return;
            } else if (!round) {
                e->arp_pos_q8 = ph;
                return;
            }
            e->arp_pos_q8 = ph;
        } else {                                /* stopped: from the key, on its own */
            if (e->arp_pos_q8 != 0xFFFFFFFFu && e->arp_playing && e->arp_pos_q8 >= gate) {
                sd_voice_off(e, e->arp_cur, SD_SRC_ARP);    /* the gate */
                e->arp_playing = 0;
            }
            if (e->arp_pos_q8 != 0xFFFFFFFFu && e->arp_pos_q8 < e->arp_step_q8) { e->arp_pos_q8 += e->clk_adv; return; }
            e->arp_pos_q8 = e->arp_pos_q8 == 0xFFFFFFFFu ? 0u : e->arp_pos_q8 - e->arp_step_q8;
            if (e->arp_pos_q8 >= e->arp_step_q8) e->arp_pos_q8 = 0;   /* (the tempo jumped) */
        }
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
        uint32_t st = e->arp_step_q8 ? e->arp_step_q8 : 1u, r = e->arp_hc ? (e->arp_since_q8 + st / 2u) / st : 1u;
        e->arp_cur = (uint8_t)(note > 127u ? 127u : note);
        sd_voice_on(e, e->arp_cur, v[k % n], SD_SRC_ARP);
        e->arp_playing = 1;
        if (r > SD_PSTEPS) r = SD_PSTEPS;
        for (; r > 1u; r--) sd_arp_hpush(e, 0, 0);   /* KEEP's ring: the steps of a gap as rests, then this one */
        sd_arp_hpush(e, e->arp_cur, v[k % n]);
        e->arp_since_q8 = 0;
    }
    if (!e->seq_run) e->arp_pos_q8 += e->clk_adv;   /* (on the grid the place is the grid's) */
}

/* KEEP (REC on ARP): how many steps at division d it keeps: the fewest bars (1..4) that hold a whole number of
 * them, 8 at least (1/16: a bar of 16; 1/64: 64; 1/16T: 24; 1/8D: three bars, 16; 1/2: four bars, 8) */
static uint32_t sd_arp_keep_len(uint32_t d)
{
    uint32_t b, num, den;
    d = d < SD_SDIVS ? d : 2u;
    num = SD_SDIV_NUM[d]; den = SD_SDIV_DEN[d];
    for (b = 1; b < 4u; b++)
        if (16u * b * den % num == 0u && 16u * b * den / num >= 8u) break;
    return 16u * b * den / num > SD_PSTEPS ? SD_PSTEPS : 16u * b * den / num;
}
/* KEEP: the last n steps ARP played, oldest first (a note, 0 a rest; its velocity), the window ending at the step
 * boundary nearest now: a step begun under half a step ago is not in it (REC pressed on the one keeps the bar that
 * ended), and the time since the last one counts as rests; fewer played than n: rests before them. Returns how many
 * notes it holds. (The main loop reads the ring the audio writes: a step coming in meanwhile moves it by one.) */
static uint32_t sd_arp_keep(const SdEngine *e, uint32_t n, uint8_t *notes, uint8_t *vels)
{
    uint32_t st = e->arp_step_q8 ? e->arp_step_q8 : 1u, k = (e->arp_since_q8 + st / 2u) / st, hc = e->arp_hc;
    uint32_t tail = k ? k - 1u : 0u, len, i, got = 0, old = (e->arp_hw + SD_PSTEPS - e->arp_hc) % SD_PSTEPS;
    if (!k && hc) hc--;
    if (tail > n) tail = n;
    len = hc + tail;
    for (i = 0; i < n; i++) {
        int32_t t = (int32_t)len - (int32_t)n + (int32_t)i;   /* its place on the line, the oldest 0 */
        notes[i] = vels[i] = 0;
        if (t < 0 || (uint32_t)t >= hc) continue;   /* before ARP played, or after its last step */
        notes[i] = e->arp_hn[(old + (uint32_t)t) % SD_PSTEPS];
        vels[i] = e->arp_hv[(old + (uint32_t)t) % SD_PSTEPS];
        got += notes[i] != 0u;
    }
    return got;
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

static void sd_mod_snap(SdEngine *e)            /* the envelopes' times and the LFOs' rates, as the matrix left them */
{
    const SdPatch *q = &e->patch;
    e->envp[0] = q->e1_a; e->envp[1] = q->e1_d; e->envp[2] = q->e1_s; e->envp[3] = q->e1_r;
    e->envp[4] = q->e2_a; e->envp[5] = q->e2_d; e->envp[6] = q->e2_s; e->envp[7] = q->e2_r;
    e->envp[8] = q->e3_a; e->envp[9] = q->e3_d; e->envp[10] = q->e3_s; e->envp[11] = q->e3_r;
    e->lfor[0] = q->lfo_rate[0]; e->lfor[1] = q->lfo_rate[1];
}

static void sd_control_tick(SdEngine *e)        /* once a sub-block, before its frames */
{
    int32_t w[SD_BANKS], mid;
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
            sd_mod_snap(e);
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
        sd_env_tick(&v->env1, v->gate, e->envp[0], e->envp[1], e->envp[2], e->envp[3]);   /* (the matrix's, a tick on) */
        sd_env_tick(&v->env2, v->gate, e->envp[4], e->envp[5], e->envp[6], e->envp[7]);
        sd_env_tick(&v->env3, v->gate, e->envp[8], e->envp[9], e->envp[10], e->envp[11]);
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
    sd_mod_snap(e);
    if (p->fx_off) {                            /* EDIT's OFF: after the matrix and the macros, so none reopens it */
        SdPatch *q = &e->patch;
        if (q->fx_off & SD_FX_REACTOR) q->r_mix = 0;
        if (q->fx_off & SD_FX_DISTROY) q->d_mix = 0;
        if (q->fx_off & SD_FX_SPACE) q->s_rlevel = 0;
        if (q->fx_off & SD_FX_TIME) q->s_dlevel = 0;
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
    if (e->sm && (p->s_dlevel || p->s_rlevel || e->lp_lvl)) {   /* (the looper records while it has a level) */
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
    {   /* the balance's middle (a patch may lean, never hard left or right): the nav
           mix's own lean, its weights' energy, Q15 (-32768 all left .. 32768 all right; POLY's equal weights: 0) */
        int32_t sw = 0, sc = 0, q;
        for (b = 0; b < SD_BANKS; b++) {
            q = (w[b] * w[b]) >> 17;            /* (32767^2 >> 17 < 2^13: four fit, and x 2^15) */
            sw += q;
            sc += b == 1u || b == 2u ? q : -q;
        }
        mid = sw ? sc * 32768 / sw : 0;         /* (|sc| <= 2^15: x 2^15 fits) */
    }
    for (b = 0; b < SD_BANKS; b++) {
        SdBank *bk = &e->bank[b];
        const SdVoice *v = &e->voice[bk->voice];
        int32_t g = (v->amp_next * w[b]) >> 15, pan, far;   /* 16256 * 32767: fits */
        bk->gl = bk->gl_next;
        bk->gr = bk->gr_next;
        /* the balance: A and D lean left, B and C right, by WIDTH, from the mix's middle (a corner's bank alone plays
           in the middle; banks mixed spread round it), the far side down to a half at the most (6 dB); in POLY, a
           bank a voice, half that (each note near the middle, a chord a little wide). WIDTH 0: both sides carry
           everything, as before */
        pan = (b == 1u || b == 2u ? 32768 : -32768) - mid;   /* -65536 .. 65536 */
        pan = (pan * (int32_t)(p->width >> 2)) >> 14;       /* (65536 x 16383: fits) */
        pan = pan > 32768 ? 32768 : pan < -32768 ? -32768 : pan;
        if (e->mode_on == SD_MODE_POLY) pan /= 2;
        far = 32768 - (pan < 0 ? -pan : pan) / 2;           /* 16384 .. 32768 */
        bk->gl_next = pan > 0 ? (g * far) >> 15 : g;        /* 16256 * 32768: fits */
        bk->gr_next = pan < 0 ? (g * far) >> 15 : g;
        if (!v->amp && !v->amp_next) continue; /* silent: the render only moves the phases */
        sd_bank_params(bk, p, &p->bank[bk->src], v, e->track,   /* (TURING to PITCH: from the voice's note) */
                       e->tq ? sd_semis_mul(e->psemis + 256 * sd_tq_snap(e, v->note, e->tq)) : e->pmul);
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

SD_INLINE int32_t sd_sp_read(const int16_t *dl, uint32_t w, int32_t dq, uint32_t n)   /* dq samples back (Q8),
                                                   linear: a channel of a line of n frames (dl its first sample) */
{
    uint32_t i = (uint32_t)dq >> 8, r0 = w >= i ? w - i : w + n - i, r1 = r0 ? r0 - 1u : n - 1u;
    int32_t a = dl[2u * r0], b = dl[2u * r1];
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

/* ---- the plate (SPACE's reverb): its lines in pl[] at po[k], pn[k] long, written at pw[k]. Its costs
 * on the chip (fm1-emu): the first version, a cubic between samples in every line, 64-bit products and a call for
 * each, took 20 % of the chip; still, the lines are read at whole samples and only a moving SIZE reads between them
 * (a straight line: a glide is short), and each branch's modulated allpass (a straight line too: a cubic took 70
 * instructions a read; FREEZE glides the LFOs out to a whole sample, so the held tail loses nothing there); the
 * products are 32-bit with Q14 coefficients (every operand under 2^17); the LFOs move once a tick */
SD_INLINE int32_t sd_pl_rd(const int16_t *b, uint32_t n, uint32_t w, uint32_t d)   /* d back (1: the last written) */
{
    return (int32_t)b[w >= d ? w - d : w + n - d] * 2;   /* Q14 -> Q15 */
}
SD_INLINE int32_t sd_pl_rdq(const int16_t *b, uint32_t n, uint32_t w, uint32_t dq)   /* dq x 256 back, linear */
{
    uint32_t d = dq >> 8 < n ? dq >> 8 : n - 1u, i = w >= d ? w - d : w + n - d, j = i ? i - 1u : n - 1u;   /* (d
                                                   read wrong on the second core: kept inside the line) */
    int32_t a = b[i], c = b[j];
    return (a + sd_tz((c - a) * (int32_t)(dq & 255u), 8)) * 2;   /* (|c - a| < 2^16: x 255 fits) */
}
SD_INLINE void sd_pl_put(int16_t *b, uint32_t n, uint32_t *w, int32_t v)
{
    b[*w] = sd_sp_store(v);
    *w = *w + 1u < n ? *w + 1u : 0u;
}
SD_INLINE int32_t sd_pq(int32_t a, int32_t g) { return sd_tz(a * g, 14); }   /* a (|a| < 2^17) x g (Q14, <= 1) */
SD_INLINE int32_t sd_pl_clip(int32_t v) { return v > 98304 ? 98304 : v < -98304 ? -98304 : v; }   /* (the store's
                                                   ceiling, ~1.9 x: kept the same before a product) */
SD_INLINE int32_t sd_pl_ap(int16_t *b, uint32_t n, uint32_t *w, int32_t z, int32_t x, int32_t g)   /* an allpass, z
                                                   its line d back: v = x + g z in, out z - g v */
{
    int32_t v = sd_pl_clip(x + sd_pq(z, g));
    sd_pl_put(b, n, w, v);
    return z - sd_pq(v, g);
}
/* the half rate's way in and out: a 23-tap half-band (Q14; every other tap 0, the centre a half): flat to 8 kHz,
   -25 dB past 13.5 kHz, -57 past 15 (a straight line between the outputs let images through, a fizz;
   31 taps cost more than the top they kept, where the damping has taken most of it) */
static const int16_t SD_HB[6] = {5087, -1386, 545, -193, 49, -4};
SD_INLINE int32_t sd_hb_down(const int32_t *x, uint32_t w)   /* the last 23 frames (w: the next place), |x| < 2^16 */
{
    int32_t a = x[(w - 12u) & 31u] * 8192;
    uint32_t k;
    for (k = 0; k < 6u; k++) a += SD_HB[k] * (x[(w - 11u + 2u * k) & 31u] + x[(w - 13u - 2u * k) & 31u]);
    return sd_tz(a, 14);                        /* (|sum| < (8192 + 2 x 7264) x 2^16: fits) */
}
SD_INLINE int32_t sd_hb_mid16(const int16_t *z, uint32_t c)  /* as sd_hb_mid, a ring of 16 bits (x 2 out) */
{
    int32_t a = 0;
    uint32_t k;
    for (k = 0; k < 6u; k++) a += SD_HB[k] * (z[(c - k) & (SD_PL_ZN - 1u)] + z[(c + 1u + k) & (SD_PL_ZN - 1u)]);
    return sd_tz(a, 12);
}
SD_INLINE int32_t sd_hb_mid(const int32_t *z, uint32_t c)    /* half way between the plate's c and c + 1 (z its
                                                   ring, SD_PL_ZN: c + 6 must be there) */
{
    int32_t a = 0;
    uint32_t k;
    for (k = 0; k < 6u; k++) a += SD_HB[k] * (z[(c - k) & (SD_PL_ZN - 1u)] + z[(c + 1u + k) & (SD_PL_ZN - 1u)]);
    return sd_tz(a, 13);                        /* (2 x the taps; |z| <= 98304: the sum fits) */
}
/* the plate, a chunk of its samples (at most a tick's: 16), stage by stage, each stage's line in locals for the
 * chunk (a sample at a time through every line it was 13 M instructions/s more on the chip). Every line
 * is longer than a chunk, so no stage needs a sample another makes later in the same chunk; the branches' ends come
 * from their last delays, 820 samples back at least. Its inputs from the queue (mono, |x| < 2^16), its settings pp
 * (a tick's, as core 0 left them), its outputs to the ring. G: SIZE moving (read between samples), else whole
 * samples. The input's lowpass, four diffusers; the tank's two branches, each from the other's end: an allpass its
 * length moved by the LFOs, a delay, the damping, the decay, an allpass, a delay; SHIMMER's share an octave up; the
 * decay; seven taps a side. It may run on the second core, which on some units reads RAM wrong now and then: what
 * it reads from RAM to place a write is kept inside its line, the lines' places are constants (the flash) */
#define SD_PL_IDX(w, d, n) ((w) >= (d) ? (w) - (d) : (w) + (n) - (d))
#define SD_PL_LEN(base, sc) (((uint32_t)(base) * (uint32_t)(sc)) >> 8)   /* a length at scale sc, x 256 */
SD_INLINE uint32_t sd_pl_w(const SdSpace *s, uint32_t ln) { uint32_t w = s->pw[ln]; return w < SD_PL_PN[ln] ? w : 0u; }
SD_INLINE int32_t sd_pl_q14(int32_t v, int32_t lo) { return v > 16384 ? 16384 : v < lo ? lo : v; }   /* a gain, lo .. 1:
                                                   one read wrong (2^21 more) would blow the tank up for its T60 */
SD_INLINE uint32_t sd_pl_d(uint32_t d, uint32_t ln) { return d - 1u < SD_PL_PN[ln] - 3u ? d : 1u; }   /* (1 .. room - 3) */
SD_INLINE void sd_plate_blk(SdSpace *s, int16_t *pm, const SdPlateP *pp, const int G, const int32_t *pin)
{
    static const int16_t DIF[4] = {12288, 12288, 10240, 10240};   /* the diffusers: 0.75 0.75 0.625 0.625 */
    int32_t v[16], bk[2][16], fk[2][16], sch[16];
    uint32_t i, k, j, w0[SD_PL_SH], n = pp->n <= 16u ? pp->n : 16u, m0 = pp->m0;
    int32_t g = sd_pl_q14(pp->g, 0), sg = sd_pl_q14(pp->sg, 0);   /* (what it reads in range: below) */
    if (G) for (i = 0; i < n; i++) sch[i] = pp->sc0 + pp->sst2 * (int32_t)i;
    {                                           /* the input: its lowpass, FREEZE's fade */
        int32_t lpi = sd_pl_clip(s->lpi), ig = sd_pl_q14(s->ig, 0), frz = pp->frz;
        for (i = 0; i < n; i++) {
            if (frz ? ig : ig < 16384) ig += frz ? (ig > 75 ? -75 : -ig) : (ig < 16384 - 75 ? 75 : 16384 - ig);
            lpi += sd_pq(pin[i] - lpi, 14500);  /* (toward zero: no DC parked) */
            v[i] = ig == 16384 ? lpi : sd_pq(lpi, ig);
        }
        s->lpi = lpi;
        s->ig = ig;
    }
    for (k = 0; k < 4u; k++) {                  /* the diffusers */
        uint32_t ln = SD_PL_IN0 + k, nn = SD_PL_PN[ln], w = sd_pl_w(s, ln), d = sd_pl_d(pp->di[ln], ln);
        int16_t *b = pm + SD_PL_PO[ln];
        int32_t gd = DIF[k];
        for (i = 0; i < n; i++) {
            int32_t z = G ? sd_pl_rdq(b, nn, w, SD_PL_LEN(SD_PL_BASE[ln], sch[i])) : (int32_t)b[SD_PL_IDX(w, d, nn)] * 2;
            int32_t vv = sd_pl_clip(v[i] + sd_pq(z, gd));
            b[w] = sd_sp_store(vv);
            w = w + 1u < nn ? w + 1u : 0u;
            v[i] = z - sd_pq(vv, gd);
        }
        s->pw[ln] = w;
    }
    for (j = 0; j < SD_PL_SH; j++) w0[j] = sd_pl_w(s, j);   /* (the tank's lines as the chunk starts: the taps' base) */
    for (k = 0; k < 2u; k++) {                  /* the branches' ends this chunk: their last delay, read ahead */
        uint32_t ln = SD_PL_D2 + k, nn = SD_PL_PN[ln], w = w0[ln], d = sd_pl_d(pp->di[ln], ln);
        const int16_t *b = pm + SD_PL_PO[ln];
        for (i = 0; i < n; i++) {
            w = w + 1u < nn ? w + 1u : 0u;      /* (as the sample's write has moved it) */
            bk[k][i] = G ? sd_pl_rdq(b, nn, w, SD_PL_LEN(SD_PL_BASE[ln], sch[i])) : (int32_t)b[SD_PL_IDX(w, d, nn)] * 2;
        }
    }
    if (sg) {                                   /* SHIMMER: the ends, mono, an octave up (two heads sweeping the
                                                   shifter's window, down a sample a sample, crossfaded by triangles,
                                                   zero where each jumps); the ends and the octave share the way
                                                   round, (1 - sg) and sg: never more than the decay lets through */
        uint32_t nn = SD_PL_PN[SD_PL_SH], w = sd_pl_w(s, SD_PL_SH), shp = s->shp & 65535u;
        int16_t *sb = pm + SD_PL_PO[SD_PL_SH];
        for (i = 0; i < n; i++) {
            uint32_t f0 = shp, f1 = (shp + 32768u) & 65535u;
            int32_t g0 = 16383 - (int32_t)((f0 > 32768u ? f0 - 32768u : 32768u - f0) >> 1), y;
            sb[w] = sd_sp_store((bk[0][i] + bk[1][i]) >> 1);
            w = w + 1u < nn ? w + 1u : 0u;
            y = sd_pq(sd_pl_rdq(sb, nn, w, ((65536u - f0) << 3) + 512u), g0) +
                sd_pq(sd_pl_rdq(sb, nn, w, ((65536u - f1) << 3) + 512u), 16383 - g0);
            shp = (shp + 32u) & 65535u;
            for (k = 0; k < 2u; k++) fk[k][i] = sd_pq(bk[k][i] + sd_pq(y - bk[k][i], sg), g);
            s->shy = y;
        }
        s->pw[SD_PL_SH] = w;
        s->shp = shp;
    } else {
        for (i = 0; i < n; i++) { fk[0][i] = sd_pq(bk[0][i], g); fk[1][i] = sd_pq(bk[1][i], g); }
        s->shy = 0;
    }
    for (k = 0; k < 2u; k++) {                  /* each branch, its input the other's end a sample before */
        uint32_t a1 = SD_PL_A1 + k, d1 = SD_PL_D1 + k, a2 = SD_PL_A2 + k, d2 = SD_PL_D2 + k;
        uint32_t na1 = SD_PL_PN[a1], nd1 = SD_PL_PN[d1], na2 = SD_PL_PN[a2], nd2 = SD_PL_PN[d2];
        uint32_t wa1 = w0[a1], wd1 = w0[d1], wa2 = w0[a2], wd2 = w0[d2];
        uint32_t ld1 = sd_pl_d(pp->di[d1], d1), la2 = sd_pl_d(pp->di[a2], a2);
        uint32_t ma = pp->ma[k] >> 8 < na1 - 3u ? pp->ma[k] : 256u, mb = G ? ma - SD_PL_LEN(SD_PL_BASE[a1], pp->snext) : 0u;
        int16_t *ba1 = pm + SD_PL_PO[a1], *bd1 = pm + SD_PL_PO[d1], *ba2 = pm + SD_PL_PO[a2], *bd2 = pm + SD_PL_PO[d2];
        int32_t lp = sd_pl_clip(s->lp[k]), c = sd_pl_q14(pp->c, 0), gd2 = sd_pl_q14(pp->d2, -16384);
        int32_t prev = sd_pl_clip(s->fbk[k ^ 1u]);
        for (i = 0; i < n; i++) {
            int32_t t = sd_pl_clip(v[i] + prev), z, vv;
            prev = fk[k ^ 1u][i];
            z = sd_pl_rdq(ba1, na1, wa1, G ? SD_PL_LEN(SD_PL_BASE[a1], sch[i]) + mb : ma);
            vv = sd_pl_clip(t + sd_pq(z, -11469));   /* -0.70: the published plate's sign */
            ba1[wa1] = sd_sp_store(vv);
            wa1 = wa1 + 1u < na1 ? wa1 + 1u : 0u;
            t = z - sd_pq(vv, -11469);
            bd1[wd1] = sd_sp_store(t);          /* the delay */
            wd1 = wd1 + 1u < nd1 ? wd1 + 1u : 0u;
            t = G ? sd_pl_rdq(bd1, nd1, wd1, SD_PL_LEN(SD_PL_BASE[d1], sch[i])) : (int32_t)bd1[SD_PL_IDX(wd1, ld1, nd1)] * 2;
            lp += sd_pq(t - lp, c);             /* the damping: a lowpass, toward zero (a feedback's) */
            t = sd_pq(lp, g);
            z = G ? sd_pl_rdq(ba2, na2, wa2, SD_PL_LEN(SD_PL_BASE[a2], sch[i])) : (int32_t)ba2[SD_PL_IDX(wa2, la2, na2)] * 2;
            vv = sd_pl_clip(t + sd_pq(z, gd2));
            ba2[wa2] = sd_sp_store(vv);
            wa2 = wa2 + 1u < na2 ? wa2 + 1u : 0u;
            t = z - sd_pq(vv, gd2);
            bd2[wd2] = sd_sp_store(t);          /* the last delay (its end read ahead, above) */
            wd2 = wd2 + 1u < nd2 ? wd2 + 1u : 0u;
        }
        s->lp[k] = lp;
        s->pw[a1] = wa1; s->pw[d1] = wd1; s->pw[a2] = wa2; s->pw[d2] = wd2;
    }
    if (n) { s->fbk[0] = fk[0][n - 1u]; s->fbk[1] = fk[1][n - 1u]; }
    {                                           /* the taps: L from the right branch mostly, R from the left */
        static const uint8_t TLN[14] = {SD_PL_D1 + 1, SD_PL_D1 + 1, SD_PL_A2 + 1, SD_PL_D2 + 1, SD_PL_D1, SD_PL_A2, SD_PL_D2,
                                        SD_PL_D1, SD_PL_D1, SD_PL_A2, SD_PL_D2, SD_PL_D1 + 1, SD_PL_A2 + 1, SD_PL_D2 + 1};
        static const int8_t TSG[7] = {1, 1, -1, 1, -1, -1, -1};
        int32_t acc[2][16];
        for (i = 0; i < n; i++) acc[0][i] = acc[1][i] = 0;
        for (j = 0; j < 14u; j++) {
            uint32_t ln = TLN[j], nn = SD_PL_PN[ln], w = w0[ln], d = sd_pl_d(pp->ti[j], ln);
            const int16_t *b = pm + SD_PL_PO[ln];
            int32_t *a = acc[j / 7u], sgn = TSG[j % 7u];
            uint32_t base = j < 7u ? SD_PL_TL[j] : SD_PL_TR[j - 7u];
            for (i = 0; i < n; i++) {
                int32_t x;
                w = w + 1u < nn ? w + 1u : 0u;  /* (as the sample's write has moved it) */
                x = G ? sd_pl_rdq(b, nn, w, SD_PL_LEN(base, sch[i])) : (int32_t)b[SD_PL_IDX(w, d, nn)] * 2;
                a[i] += sgn > 0 ? x : -x;
            }
        }
        for (i = 0; i < n; i++) {               /* x 1.11: R-LEVEL as loud as the room before it was (the factory
                                                   patches' wets, a chord and its tail: the median of 22) */
            s->zl[(m0 + i) & (SD_PL_ZN - 1u)] = sd_pl_clip(sd_pq(sd_pl_clip(acc[0][i]), 18225));
            s->zr[(m0 + i) & (SD_PL_ZN - 1u)] = sd_pl_clip(sd_pq(sd_pl_clip(acc[1][i]), 18225));
        }
    }
}

/* the looper (it plays the delay's layer back, pitched, into the reverb as the synth does; the delay goes on):
 * armed (L-LEVEL above 0), it records what SPACE hears before the reverb, the dry and the echoes, mono at half
 * rate, always, a byte a sample (mu-law: 8 bits, a light tape), into a ring of two pieces (half the delay line and a
 * little of the RAM). LOOP: it stops recording and its last LENGTH goes round, crossfaded where it comes round (the
 * 10 ms before its start fade in: they run on into it), faded in over 12 ms, at its speed (the pitch and the length
 * together, a tape's), held; off, it fades out over 46 ms and records again. A sample at a time, on the second core in
 * the plate's job, on core 0 with the room (one core). What it reads to place a read or a write is kept inside its
 * memory (the second core reads RAM wrong on some units) */
typedef struct { uint8_t *a, *b; uint32_t na, n; } SdLpMem;   /* the ring: a (na bytes), then b; n in all */
SD_INLINE int32_t sd_td_clip(int32_t v) { return v > 65535 ? 65535 : v < -65535 ? -65535 : v; }
SD_INLINE uint32_t sd_mu_enc(int32_t q)         /* Q14 (int16) -> mu-law: a sign, 3 bits of octave, 4 of step; 0 -> 0 */
{
    uint32_t sg = q < 0 ? 128u : 0u, m = (uint32_t)(q < 0 ? -q : q), ex;
    m = (m >> 2) + 33u;
    m = m > 8191u ? 8191u : m;
    ex = m >= 4096u ? 7u : m >= 2048u ? 6u : m >= 1024u ? 5u : m >= 512u ? 4u : m >= 256u ? 3u : m >= 128u ? 2u :
         m >= 64u ? 1u : 0u;
    return sg | ex << 4 | ((m >> (ex + 1u)) & 15u);
}
SD_INLINE int32_t sd_mu_dec(uint32_t c)         /* mu-law -> Q14, the middle of its step */
{
    uint32_t ex = (c >> 4) & 7u;
    int32_t m = (int32_t)((((16u + (c & 15u)) << (ex + 1u)) + (1u << ex) - 33u) << 2);
    return c & 128u ? -m : m;
}
SD_INLINE int32_t sd_lp_get(const SdLpMem *L, uint32_t k) { return sd_mu_dec(k < L->na ? L->a[k] : L->b[k - L->na]); }
SD_INLINE int32_t sd_lp_read(const SdLpMem *L, uint32_t w, uint32_t dq)   /* dq x 256 back from w, a straight line */
{
    uint32_t n = L->n, i = dq >> 8, r0, r1;
    int32_t a, b;
    i = i < n - 2u ? i : n - 2u;
    r0 = w >= i ? w - i : w + n - i;
    r1 = r0 ? r0 - 1u : n - 1u;
    a = sd_lp_get(L, r0);
    b = sd_lp_get(L, r1);
    return (a + sd_tz((b - a) * (int32_t)(dq & 255u), 8)) * 2;   /* Q14 -> Q15 */
}
SD_INLINE int32_t sd_lp_lread(const SdLpMem *L, uint32_t base, uint32_t ln, uint32_t k)   /* the loop (ln x 256,
                                                   ending at base), k x 256 into it */
{
    uint32_t back = ln - k, x = SD_LP_XF * 256u;
    int32_t a;
    back = back < 256u ? 256u : back;
    a = sd_lp_read(L, base, back);
    if (k + x > ln) {
        int32_t u = (int32_t)((k + x - ln) >> 4);   /* 0 .. x / 16 */
        a += (sd_lp_read(L, base, back + ln) - a) * u / (int32_t)(x >> 4);   /* (|d| < 2^17, x 3584: fits) */
    }
    return a;
}
/* the head i samples into its tick: k0 + adv i / 16 round the loop (a floor, as core 0's sum over the tick) */
SD_INLINE uint32_t sd_lp_at(uint32_t k0, int32_t adv, uint32_t i, uint32_t ln)
{
    int32_t d = (adv * (int32_t)i) >> 4;        /* (|adv| < 2^15) */
    uint32_t k = k0 < ln ? k0 : k0 % ln, m;
    if (d >= 0) { k += (uint32_t)d; if (k >= ln) k -= ln; }
    else { m = (uint32_t)-d; k = k >= m ? k - m : k + ln - m; }
    return k < ln ? k : k % ln;                 /* (a loop shorter than a tick's way) */
}
/* a sample: in (the dry and the echoes, mono, |x| < 2^16), t (its tick's plan: LOOP playing, where it reads, the
   crossfade, the fade), i (the sample into the tick), rst (LOOP from here), lvl (Q15): out, at its level (mono: both
   sides the same). REVERSE's head comes round its start in at its end through the same crossfade, one curve either
   way */
SD_INLINE void sd_lp_step(SdSpace *s, const SdLpMem *L, int32_t xm, const SdLpTick *t, uint32_t i, int rst, int32_t lvl,
                          int32_t *yl, int32_t *yr)
{
    uint32_t nb = L->n, w = s->lw < nb ? s->lw : 0u, lm = (nb - SD_LP_XF - 32u) * 256u;
    *yl = *yr = 0;
    if (rst) s->lbase = t->back ? (w + nb - t->back % nb) % nb : w;   /* LOOP: its last LENGTH, ending here (or on
                                                   the line it passed: sd_lp_snap) */
    if (t->on) {
        uint32_t ln = t->ln < 512u ? 512u : t->ln > lm ? lm : t->ln, base = s->lbase < nb ? s->lbase : 0u;
        uint32_t k = sd_lp_at(t->k0, t->adv, i, ln);
        int32_t lv = t->lev0 + t->levd * (int32_t)i, g = lvl < 0 ? 0 : lvl > 32767 ? 32767 : lvl;
        int32_t a = sd_lp_lread(L, base, ln, k);
        lv = lv < 0 ? 0 : lv > 16384 ? 16384 : lv;
        if (t->bl) {                            /* the crossfade to its own place (two heads on its beats): x (2 -
                                                   x) in, 1 - x^2 out, near equal power */
            uint32_t k1 = sd_lp_at(t->k1, t->adv, i, ln);
            int32_t x = t->x0 + ((t->xd * (int32_t)i) >> 4), g0, g1;
            x = x < 0 ? 0 : x > 32767 ? 32767 : x;
            g1 = (x * (65536 - x)) >> 16;       /* Q14 */
            g0 = ((32768 - x) * (32768 + x)) >> 16;
            a = ((a * g0) >> 14) + ((sd_lp_lread(L, base, ln, k1) * g1) >> 14);   /* (|a| < 2^16, x 2^14) */
        }
        a = sd_tz(sd_td_clip(a) * (lv >> 1), 13);
        *yl = *yr = sd_td_clip(a) * g >> 15;    /* (< 2^31; a floor: not a loop) */
        return;
    }
    {                                           /* armed, recording (Q15 -> Q14, past 1.5 x a shoulder; mu-law) */
        uint8_t c = (uint8_t)sd_mu_enc(sd_sp_store(sd_td_clip(xm) * 2));
        if (w < L->na) L->a[w] = c; else L->b[w - L->na] = c;
    }
    s->lw = w + 1u < nb ? w + 1u : 0u;
}

/* the plate's job: block b of its queue, its chunks in order (on the second core when the app has one: a job between
 * the audio's interrupts, read a block later; else here). What it reads to count is kept in range */
static void sd_plate_job_at(SdEngine *e, int16_t *pm, uint8_t *la, uint32_t na, uint8_t *lb, uint32_t nbb, uint32_t b)
                                                /* pm: e->sm->u.pl; la na, lb nbb: the looper's memory's two pieces
                                                   (the second core's caller passes them as constants: a pointer read
                                                   wrong would write anywhere; kept to SD_LP_NMAX) */
{
    SdSpace *s = &e->sp;
    uint32_t c = s->qb0[b & 1u], end = s->qb1[b & 1u], k, qm = 4u * SD_PL_BLK - 1u;
    int32_t pin[16], yo[32];
    SdLpMem L;
    int lp;
    L.a = la; L.na = la ? na : 0u; L.b = lb; L.n = L.na + (lb ? nbb : 0u);
    lp = la && L.n > SD_LP_XF + 64u && L.n <= SD_LP_NMAX;
    if (end - c > SD_PL_CH) c = end - SD_PL_CH; /* (a count read wrong: at most the ring) */
    for (k = c; k != end; k++) {
        const SdPlateP *pp = &s->qp[k % SD_PL_CH];
        uint32_t i, n = pp->n <= 16u ? pp->n : 16u, z = SD_PL_ZN - 1u;
        int lq = lp && (pp->llvl || pp->lt.on);
        for (i = 0; i < n; i++) {               /* the frames of sample m (2m, 2m + 1) through the half-bands in; the
                                                   looper; the plate's input: the dry, the echoes, the looper (as the
                                                   synth goes in: mono, x 1/8) */
            uint32_t f = (2u * (pp->m0 + i)) & qm, j, w = s->hxw & 31u;
            int32_t xl = 0, xm;
            for (j = 0; j < 2u; j++, f = (f + 1u) & qm, w = (w + 1u) & 31u) {
                s->hx[w] = s->qfl[f] + s->qfr[f];   /* (the looper's: mono, the two sides' mean) */
                s->hxm[w] = sd_td_clip(s->qfm[f]);
            }
            s->hxw = (uint8_t)w;
            xm = sd_hb_down(s->hxm, w);
            if (lq) xl = sd_hb_down(s->hx, w);
            yo[2u * i] = yo[2u * i + 1u] = 0;
            if (lp) sd_lp_step(s, &L, xl, &pp->lt, (uint32_t)pp->li0 + i, pp->lt.rst && !i, pp->llvl,
                               &yo[2u * i], &yo[2u * i + 1u]);
            pin[i] = sd_td_clip(xm) + ((sd_td_clip(yo[2u * i]) + sd_td_clip(yo[2u * i + 1u])) >> 3);
        }
        if (pp->G) sd_plate_blk(s, pm, pp, 1, pin);
        else sd_plate_blk(s, pm, pp, 0, pin);
        for (i = 0; i < n; i++) {               /* the looper's out to its ring (x 1/2: 16 bits) */
            uint32_t q = (pp->m0 + i) & z;
            s->yl[q] = (int16_t)(sd_td_clip(yo[2u * i]) >> 1);
            s->yr[q] = (int16_t)(sd_td_clip(yo[2u * i + 1u]) >> 1);
        }
    }
}
static void sd_plate_job(SdEngine *e, uint32_t b) { sd_plate_job_at(e, e->sm->u.pl, e->lpa, e->lpa_n, e->lpb, e->lpb_n, b); }

/* block b not run (the second core gave up, or there is none: on the FM-1 the plate never runs on the first core):
 * its outputs fade from the last one heard to silence over the block, and the room plays from the
 * next tick (the app sets sp_room) */
static void sd_plate_drop(SdEngine *e, uint32_t b)
{
    SdSpace *s = &e->sp;
    uint32_t i, m0 = b * SD_PL_BLK, z = SD_PL_ZN - 1u;
    int32_t l = sd_pl_clip(s->zl[(m0 - 1u) & z]), r = sd_pl_clip(s->zr[(m0 - 1u) & z]);
    for (i = 0; i < SD_PL_BLK; i++) {           /* (x 127 at most: |l| < 2^17 fits; the echoes are in the mix) */
        int32_t g = (int32_t)(SD_PL_BLK - 1u - i);
        s->zl[(m0 + i) & z] = l * g / (int32_t)SD_PL_BLK;
        s->zr[(m0 + i) & z] = r * g / (int32_t)SD_PL_BLK;
    }
}
/* the plate's jobs, in order: block b waits for b - 1 (they share its lines). Without hooks (a computer: the host,
 * the page's player) each runs here at once; with them (the FM-1) a block not handed over or not back is dropped */
static void sd_plate_join(SdEngine *e)
{
    SdSpace *s = &e->sp;
    if (s->qdone == s->qpost) return;
    if (e->pl_join && e->pl_join(e) != 0) sd_plate_drop(e, s->qpost - 1u);
    s->qdone = s->qpost;
}
static void sd_plate_post(SdEngine *e, uint32_t b)
{
    SdSpace *s = &e->sp;
    sd_plate_join(e);
    s->qpost = b + 1u;
    if (!e->pl_post) { sd_plate_job(e, b); s->qdone = b + 1u; }
    else if (!e->pl_post(e, b)) { sd_plate_drop(e, b); s->qdone = b + 1u; }
}
/* a plate sample queued (the decimated input x, at frame scale sc, SIZE's step sstep): into the tick's chunk (a new
 * one at a tick or a block), and at a block's end the block handed over */
SD_INLINE void sd_plate_queue(SdEngine *e, int32_t sc, int32_t sstep)
{
    SdSpace *s = &e->sp;
    uint32_t m = s->qn, k;
    SdPlateP *pp = &s->qp[(s->qpw - 1u) % SD_PL_CH];
    int nt = !s->qpw || pp->tick != s->tick;    /* (a new tick) */
    if (nt || !(m % SD_PL_BLK) || pp->n >= 16u) {   /* a new chunk: the tick's settings */
        if (!(m % SD_PL_BLK)) s->qb0[(m / SD_PL_BLK) & 1u] = s->qpw;
        pp = &s->qp[s->qpw % SD_PL_CH];
        s->qpw++;
        for (k = 0; k < SD_PL_SH; k++) pp->di[k] = s->di[k];
        for (k = 0; k < 14u; k++) pp->ti[k] = s->ti[k];
        pp->ma[0] = s->ma[0]; pp->ma[1] = s->ma[1];
        pp->g = s->g; pp->c = s->c; pp->sg = s->sg; pp->d2 = s->d2; pp->snext = s->snext;
        pp->sc0 = sc; pp->sst2 = 2 * sstep; pp->G = sstep != 0; pp->frz = s->frz; pp->tick = s->tick;
        pp->m0 = m; pp->n = 0;
        pp->llvl = s->llvl; pp->rlvl = s->rlvl;
        if (nt) s->qti = 0;                     /* the looper's tick (a part of it after a block's end: from there) */
        pp->lt = s->lt; pp->li0 = s->qti;
        if (s->lt.rst) s->lt.rst = s->prst = 0; /* (LOOP's start handed over once) */
    }
    pp->n++;
    s->qti++;
    s->qn = m + 1u;
    if (!((m + 1u) % SD_PL_BLK)) {              /* a block's end: handed over */
        s->qb1[(m / SD_PL_BLK) & 1u] = s->qpw;
        sd_plate_post(e, m / SD_PL_BLK);
    }
}
/* the room (one core): a sample at half rate of the dry and the echoes, through four diffusers into a 4-line FDN
 * (Hadamard); between its outputs a straight line, a frame late (§5.5, before the plate) */
SD_INLINE void sd_room(SdSpace *s, SdSpaceMem *mem, int32_t r, int32_t *al, int32_t *ar)
{
    if (!s->ph) {
        s->racc = r;
        *al = s->a1[0];
        *ar = s->a1[1];
    } else {
        int32_t h[4], z[4], v[4];
        uint32_t k, fw = s->fw;
        r = (s->racc + r) >> 1;
        for (k = 0; k < 4u; k++) {              /* Schroeder allpasses, g 0.625 */
            uint32_t aw = s->apw[k], rp = (aw + SD_SP_APLEN - SD_SP_APL[k]) & (SD_SP_APLEN - 1u);
            int32_t zz = mem->u.rm.ap[k][rp] * 2, vv = r + sd_tz(zz * 20480, 15);
            mem->u.rm.ap[k][aw] = sd_sp_store(vv);
            r = zz - sd_tz(vv * 20480, 15);
            s->apw[k] = (aw + 1u) & (SD_SP_APLEN - 1u);
        }
        for (k = 0; k < 4u; k++) {              /* each line: read, the tone, the decay */
            z[k] = mem->u.rm.fdn[k][(fw - (uint32_t)s->flen[k]) & (SD_SP_FLEN - 1u)] * 2;
            s->flp[k] += ((z[k] - s->flp[k]) * s->rtone) >> 15;   /* as the delay's tone: the decay rounds */
            v[k] = sd_tz(s->flp[k] * s->rg, 15);
        }
        h[0] = sd_tz((v[0] + v[1] + v[2] + v[3]), 1);   /* the Hadamard matrix / 2: orthonormal, energy kept */
        h[1] = sd_tz((v[0] - v[1] + v[2] - v[3]), 1);
        h[2] = sd_tz((v[0] + v[1] - v[2] - v[3]), 1);
        h[3] = sd_tz((v[0] - v[1] - v[2] + v[3]), 1);
        mem->u.rm.fdn[0][fw] = sd_sp_store(h[0] + r);
        mem->u.rm.fdn[1][fw] = sd_sp_store(h[1] - r);
        mem->u.rm.fdn[2][fw] = sd_sp_store(h[2] + r);
        mem->u.rm.fdn[3][fw] = sd_sp_store(h[3] - r);
        s->fw = (fw + 1u) & (SD_SP_FLEN - 1u);
        s->a0[0] = s->a1[0];
        s->a0[1] = s->a1[1];
        s->a1[0] = (z[0] - z[3]) >> 1;          /* two decorrelated taps */
        s->a1[1] = (z[1] - z[2]) >> 1;
        *al = (s->a0[0] + s->a1[0]) >> 1;
        *ar = (s->a0[1] + s->a1[1]) >> 1;
    }
}

/* SPACE's frames, PP constant: the delay, the looper's and the reverb's way in (pass A), then out (pass C, as the
 * rings stood frame by frame). Its state in locals for the call (C1): out[] could alias it, and the compiler reloaded
 * it every sample. The delay is here on core 0 always; the
 * plate and the looper on the second core, or (one core) the room and the looper here */
SD_INLINE void sd_space_loop(SdEngine *e, const int32_t *in, int32_t *out, uint32_t m, uint32_t pos, const int PP)
{
    SdSpace *s = &e->sp;
    SdSpaceMem *mem = e->sm;
    uint32_t i, dw = s->dw, ph0 = s->ph, ph = ph0, ml, room = s->room, z = SD_PL_ZN - 1u;
    int32_t dstep = (s->dnext - s->dq) >> SD_SUB_SHIFT, dq = s->dq + dstep * (int32_t)pos;
    int32_t sstep = (s->snext - s->sc) >> SD_SUB_SHIFT, sc = s->sc + sstep * (int32_t)pos;
    int32_t lpd0 = s->lpd[0], lpd1 = s->lpd[1], fb = PP ? s->fb2 : s->fb, dtone = s->dtone, dlvl = s->dlvl, rlvl = s->rlvl;
    int32_t peak = s->peak, ev[2u * SD_SUB], rv[2u * SD_SUB], lo[2u * SD_SUB];
    int32_t dz0 = s->dcz[0], dz1 = s->dcz[1];
    int16_t *dl = mem->dl;
    uint32_t dmax = s->dmax == SD_SP_DHALF ? SD_SP_DHALF : SD_SP_DMAX;
    int32_t dg = s->dfade, dsw = s->dsw;
    SdLpMem L;
    int lp;
    L.a = e->lpa; L.na = e->lpa ? e->lpa_n : 0u; L.b = e->lpb; L.n = sd_lp_total(e);
    lp = room && e->lpa && L.n > SD_LP_XF + 64u;   /* (one core: the looper here) */
    if (!room) sd_plate_join(e);                /* (the block heard here: done) */
    if (s->loop) s->lhear = 24u * SD_PL_BLK;    /* (looping: on, and 3072 frames after: its fade, 2048, and the ring's
                                                   block) */
    ml = s->qn - 1u;
    for (i = 0; i < m; i++) {                   /* the delay; the looper's and the reverb's way in */
        int32_t xl = in[2u * i], xr = in[2u * i + 1u], el, er;
        /* the input without its DC (a warp toward pulse has some): the delay's feedback and the reverb's lines
           multiply a DC up to 20 .. 100 x (C1: DROWN on VOYAGE built a rumble). The dry, below, keeps it */
        dz0 += ((xl * 256 - dz0) >> 10);        /* |x| <= 2^18: x 256 fits */
        dz1 += ((xr * 256 - dz1) >> 10);
        xl -= dz0 >> 8;
        xr -= dz1 >> 8;
        /* the delay: read, the tone, the feedback back in (crossed in ping-pong) */
        el = sd_sp_read(dl, dw, dq, dmax);
        er = sd_sp_read(dl + 1, dw, dq, dmax);
        lpd0 += ((el - lpd0) * dtone) >> 15;    /* |difference| <= 2^17: x 2^15 fits; a floor here */
        lpd1 += ((er - lpd1) * dtone) >> 15;    /* parks at most at -1, which FDBK's toward-zero drops */
        el = lpd0;
        er = lpd1;
        if (dg < 32767 && (dsw == 1 || dsw == 3)) { el = (el * dg) >> 15; er = (er * dg) >> 15; }   /* (the line about
                                                   to change: its echoes out) */
        {
            int32_t wl = xl, wr = xr;
            if (dg < 32767 && dsw != 1) { wl = (xl * dg) >> 15; wr = (xr * dg) >> 15; }   /* (clearing: silence in;
                                                   the line new: what goes in faded in, so its first echo comes in as
                                                   smooth) */
            if (PP) {                           /* (fb is FDBK^2 here: the whole way round) */
                dl[2u * dw] = sd_sp_store(((wl + wr) >> 1) + sd_tz(er * fb, 15));
                dl[2u * dw + 1u] = sd_sp_store(el);   /* the left's echo, whole: the pong as loud as the ping */
            } else {
                dl[2u * dw] = sd_sp_store(wl + sd_tz(el * fb, 15));
                dl[2u * dw + 1u] = sd_sp_store(wr + sd_tz(er * fb, 15));
            }
        }
        dw = dw + 1u < dmax ? dw + 1u : 0u;
        ev[2u * i] = el;
        ev[2u * i + 1u] = er;
        sc += sstep;                            /* (SIZE as the frame queued: before the queue, as it always was) */
        /* what SPACE plays before the reverb, the dry and the echoes at D-LEVEL, each side (x 0.5: |x| < 2^16), for the
           looper; the dry and the echoes whole, mono (x 1/8: room in the tank), for the plate: through the half-band
           in to half rate, here on one core, else on the second core from the queue (a frame each, 16 bits) */
        if (room) {
            s->hx[s->hxw] = sd_td_clip(((xl + (el * dlvl >> 15)) >> 2) + ((xr + (er * dlvl >> 15)) >> 2));   /* (the
                                                   looper's: mono, the two sides' mean) */
            s->hxw = (uint8_t)((s->hxw + 1u) & 31u);
        } else {
            uint32_t q = (2u * s->qn + ph) & (4u * SD_PL_BLK - 1u);
            s->qfl[q] = (int16_t)(sd_td_clip((xl + (el * dlvl >> 15)) >> 1) >> 1);
            s->qfr[q] = (int16_t)(sd_td_clip((xr + (er * dlvl >> 15)) >> 1) >> 1);
            s->qfm[q] = (xl + xr + el + er) >> 3;
        }
        if (room) {                             /* one core: the looper here (as on the second, into its ring, heard 6
                                                   samples on through the half-band out), the room on all three */
            int32_t ll = 0, lr = 0;
            if (lp && (s->llvl || s->lt.on)) {  /* (the looper off: not its half-bands, nor it) */
                if (ph) {
                    int32_t yl, yr;
                    sd_lp_step(s, &L, sd_hb_down(s->hx, s->hxw), &s->lt, s->lti, s->lt.rst, s->llvl, &yl, &yr);
                    s->lti++;
                    if (s->lt.rst) s->lt.rst = s->prst = 0;
                    s->yl[s->qn & z] = (int16_t)(sd_td_clip(yl) >> 1);
                    s->yr[s->qn & z] = (int16_t)(sd_td_clip(yr) >> 1);
                    s->qn++;
                    ll = s->yl[(s->qn - 7u) & z] * 2;
                    lr = s->yr[(s->qn - 7u) & z] * 2;
                } else {
                    ll = sd_hb_mid16(s->yl, s->qn - 7u);
                    lr = sd_hb_mid16(s->yr, s->qn - 7u);
                }
                ll = sd_td_clip(ll);
                lr = sd_td_clip(lr);
            }
            lo[2u * i] = ll;
            lo[2u * i + 1u] = lr;
            s->ph = (uint8_t)ph;
            sd_room(s, mem, (xl + xr + el + er + ll + lr) >> 2, &rv[2u * i], &rv[2u * i + 1u]);   /* (the looper in as
                                                   the synth goes in) */
        } else if (ph) {
            sd_plate_queue(e, sc, sstep);
        }
        dq += dstep;
        ph ^= 1u;
    }
    ph = ph0;
    for (i = 0; i < m; i++) {                   /* out: the dry, the echoes; the second core's mix (the plate, the
                                                   looper, at their levels) a block late, a sample, then the
                                                   half-band's guess half way to the next (0.6 ms); or the room and
                                                   the looper here */
        int32_t el = ev[2u * i], er = ev[2u * i + 1u], al, ar, ll = 0, lr = 0;
        if (!room) {
            if (!ph) {
                uint32_t c = ml - 6u - SD_PL_BLK;
                al = sd_hb_mid(s->zl, c);
                ar = sd_hb_mid(s->zr, c);
                if (s->lhear) { ll = sd_hb_mid16(s->yl, c); lr = sd_hb_mid16(s->yr, c); }
            } else {
                uint32_t c;
                ml++;
                c = (ml - 6u - SD_PL_BLK) & z;
                al = s->zl[c];
                ar = s->zr[c];
                if (s->lhear) { ll = s->yl[c] * 2; lr = s->yr[c] * 2; }
            }
            if (s->lhear) s->lhear--;
        } else {
            al = rv[2u * i];
            ar = rv[2u * i + 1u];
            ll = lo[2u * i];
            lr = lo[2u * i + 1u];
        }
        ph ^= 1u;
        al = al > 65535 ? 65535 : al < -65535 ? -65535 : al;   /* (2 x full scale: x R-LEVEL stays under 2^31) */
        ar = ar > 65535 ? 65535 : ar < -65535 ? -65535 : ar;
        {
            int32_t pl = (el < 0 ? -el : el) | (al < 0 ? -al : al) | (ll < 0 ? -ll : ll);
            int32_t pr = (er < 0 ? -er : er) | (ar < 0 ? -ar : ar) | (lr < 0 ? -lr : lr);
            if (pl > peak) peak = pl;
            if (pr > peak) peak = pr;
        }
        out[2u * i] += in[2u * i] + (el * dlvl >> 15) + (al * rlvl >> 15) + ll;   /* the dry with its DC, as it
                                                   came; not a loop: a floor is fine; each product < 2^31 */
        out[2u * i + 1u] += in[2u * i + 1u] + (er * dlvl >> 15) + (ar * rlvl >> 15) + lr;
    }
    s->dw = dw;
    s->ph = (uint8_t)ph;
    s->lpd[0] = lpd0;
    s->lpd[1] = lpd1;
    s->peak = peak;
    s->dcz[0] = dz0;
    s->dcz[1] = dz1;
}

static void sd_space_run(SdEngine *e, const int32_t *in, int32_t *out, uint32_t m, uint32_t pos)
{
    if (e->sp.pp) sd_space_loop(e, in, out, m, pos, 1);
    else sd_space_loop(e, in, out, m, pos, 0);
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
