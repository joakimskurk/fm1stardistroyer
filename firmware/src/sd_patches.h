/* SPDX-License-Identifier: GPL-3.0-only */
/* The patches (Sound Plan §7, V3): four banks of four oscillators, each a harmonic 1..32 of
 * the note with a fine tune, a level and a role, left to right; each bank's offsets on the Q page; the Q page's XM WP
 * FD DT for all banks; ENV1 on the level, ENV2 on the FM amount; the nav X/Y, COURSE's cells, the stereo width.
 * Included by sd_engine.h after the patch's types.
 *
 * A one-bank patch (GROWL, AURORA, ...) has its bank in all four and the nav at corner A: in DUO and POLY every
 * voice plays the same bank. In POLY each voice keeps the bank heaviest at its note-on (sd_voice_banks), so
 * the pads start in their richest bank and move in the filter, the vowels and the combs; the journeys between the
 * banks are MONO's.
 *
 * TRIM (2026-11-21): each patch's level evened to the others' (tools/sd_gain.c: the same phrase on each, its loudest
 * 400 ms at -11 LUFS before the output's limiter; they spread from -31 to -2 before, 30 dB). The patches here are
 * written as they were before the make-ups (lv 0): sd_patch_levels moves them onto them as they load. */
#ifndef SD_PATCHES_H
#define SD_PATCHES_H

#define SD_OSC(h, fine, lvl, role) {h, fine, role, 0, lvl}
#define SD_BANK(a, b, c, d) {{a, b, c, d}, 0, 0, 0, 0}
#define SD_BANK_OFS(a, b, c, d, xm, wp, fd, dt) {{a, b, c, d}, xm, wp, fd, dt}
#define SD_SAME(bk) {bk, bk, bk, bk}
#define C_ SD_ROLE_C
#define S_ SD_ROLE_S
#define P_ SD_ROLE_P
#define SD_OFF SD_OSC(1, 0, 0, C_)
#define SD_CORNER_A .x = 0, .y = 65535
/* the Reactor: a stage from a vowel (sd_tables.h, its four formants) at levels falling from F1, or a chord on a root */
#define SD_RLV_VOWEL {47800u, 43700u, 37500u, 27900u}   /* the level law's 1.89 1.59 1.22 0.85 x */
#define SD_RST_VOWEL(v) {v, SD_RLV_VOWEL}
/* VOWEL's in VBP: the same gains as before the make-up (VBP's is 4.62 x): 0.41 0.34 0.26 0.18; PLUCKS' combs:
 * 0.67 / KP's 0.176 = 3.8 x, near the level law's top */
#define SD_RLV_KP 64441u
#define SD_RST_VBP(v) {v, {13400u, 11270u, 8650u, 6030u}}
#define SD_RCH(root, lv, a, b, c, d) {{(root + a) * 256u, (root + b) * 256u, (root + c) * 256u, (root + d) * 256u}, \
                                      {lv, lv, lv, lv}}   /* lv low: a narrow BP12 on a harmonic rings ~+25 dB */

/* The init patch: oscillator 1 of bank A alone, a sine. P0's and V1's contract (click, timing) hold on it. */
static const SdPatch SD_PATCH_SINE = {
    .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 65535, C_), SD_OFF, SD_OFF, SD_OFF)), SD_CORNER_A,
    .e1_a = 20, .e1_d = 20, .e1_s = 127, .e1_r = 45, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45};

/* banks, by sound: the ones the patches below are made of */
#define BK_GLASS SD_BANK(SD_OSC(1, 0, 36000, C_), SD_OSC(2, 0, 16000, C_), SD_OSC(3, 0, 9000, C_), SD_OSC(4, 0, 6000, C_))
#define BK_BELL SD_BANK(SD_OSC(7, 0, 34000, S_), SD_OSC(2, 0, 30000, C_), SD_OSC(5, 4, 26000, S_), SD_OSC(1, 0, 40000, C_))
#define BK_GROWL SD_BANK(SD_OSC(1, 0, 20000, S_), SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 30000, S_), SD_OSC(1, 0, 60000, C_))
/* DISTROY's middle banks (C1): BELL and EPIANO under the squash (WP -17000: at most the saw with its LFO) and
 * without fold: two of four banks in the costliest kernel instead of four (the V3 fallback) */
#define BK_BELL_LIGHT SD_BANK_OFS(SD_OSC(7, 0, 34000, S_), SD_OSC(2, 0, 30000, C_), SD_OSC(5, 4, 26000, S_), \
                                  SD_OSC(1, 0, 40000, C_), 0, -17000, -15000, 0)
#define BK_EPIANO_LIGHT SD_BANK_OFS(SD_OSC(14, 0, 9000, S_), SD_OSC(1, 0, 30000, C_), SD_OSC(1, 0, 30000, S_), \
                                    SD_OSC(1, 3, 36000, C_), 0, -17000, -15000, 0)
#define BK_ORGAN SD_BANK(SD_OSC(1, 0, 40000, C_), SD_OSC(3, 0, 30800, C_), SD_OFF, SD_OFF)

/* SPACE's TIME as a synced division: SD_SP_DIV's index (1/32 1/16T 1/16 1/8T 1/16D 1/8 1/4T 1/8D 1/4 1/2T 1/4D 1/2) */
#define SD_SPDIV(i) ((uint16_t)((i) * 65536u / 12u + 100u))
#define SD_SPMS(ms) ((uint16_t)((ms) >= 1000u ? 65535u : ((ms) - 1u) * 65535u / 999u + 1u))   /* (1000 ms: TIME's top,
                                                   65535, not 65536) */

/* The collection (2026-11-22, the user: the stepping on the beat was in too many; keep the most important, build a
 * wide set: pads, leads, basses, film; big, sci-fi, epic). DISTROY and VOYAGE first, then the basses, the leads,
 * the pads, the plucks and the film. Eight from before (DISTROY, VOYAGE, GROWL, AURORA, NEBULA, STRINGS, PULSAR,
 * BRAAM); the sixteen new ones each have four banks of their own, moved through inside the note (COURSE, the
 * envelopes) or by the hands (the wheel, aftertouch, velocity), the Reactor swept inside the note, a snap at the
 * attack and grit in the mids, and no S&H: steps on the beat come from MOVE and RADAR when the player wants them.
 * The film's and FX's last five (2026-11-23): COLOSSUS RISER DRONE TICKER WARP. SKURK last (2026-11-27): the
 * user's own, played on their FM-1 and read over USB. The
 * demos play the set as it was before (tools/sd_demo_patches.h). The code names a patch by SD_P_*, never by number:
 * tests/sd_test.c checks each against SD_PATCH_NAME */
enum {
    SD_P_DISTROY, SD_P_VOYAGE, SD_P_GROWL, SD_P_GRAVITY, SD_P_CHROME, SD_P_ABYSS, SD_P_NEON, SD_P_CORE, SD_P_NOVA,
    SD_P_SOLAR, SD_P_LASER, SD_P_SIGNAL, SD_P_AURORA, SD_P_NEBULA, SD_P_MASS, SD_P_OVERTONE, SD_P_HORIZON, SD_P_CHOIR,
    SD_P_STRINGS, SD_P_STARDUST, SD_P_CIRCUIT, SD_P_PLUCKS, SD_P_PULSAR, SD_P_BRAAM, SD_P_COLOSSUS, SD_P_RISER,
    SD_P_DRONE, SD_P_TICKER, SD_P_WARP, SD_P_SKURK, SD_P_COUNT
};
#define SD_PATCHES 30u
static const char *const SD_PATCH_NAME[SD_PATCHES] = {"DISTROY", "VOYAGE", "GROWL", "GRAVITY", "CHROME", "ABYSS",
                                                      "NEON", "CORE", "NOVA", "SOLAR", "LASER", "SIGNAL", "AURORA",
                                                      "NEBULA", "MASS", "OVERTONE", "HORIZON", "CHOIR", "STRINGS",
                                                      "STARDUST", "CIRCUIT", "PLUCKS", "PULSAR", "BRAAM", "COLOSSUS",
                                                      "RISER", "DRONE", "TICKER", "WARP", "SKURK"};
/* DISTROY's splits as MIDI x 256: the pitch of hz, rounded (sd_tables.h's macros are for vowels) */
#define SD_DHZ_200 14170u                       /* MIDI 55.35 */
#define SD_DHZ_1200 22111u                      /* MIDI 86.37 */
#define SD_DHZ_5000 28436u                      /* MIDI 111.08 */
#define SD_DHZ_2500 25362u                      /* MIDI 99.07 */
/* The Star Distroyer edge (a plain FM patch sounds as it should, with an edge that makes it Star Distroyer's:
 * GROWL's and AURORA's): under the top split DISTROY passes the sound as it was (NONE
 * through LR4, so the top's shaper sees the top alone: CMP's gentler slopes let the body in, +5 .. +7 dB at 1.2 ..
 * 5 kHz); the top band alone is destroyed (its shaper, gain, ALT and level); and something steps on the beat (S&H, in
 * the patch). LR4's bands sum to an allpass, flat in level but a few dB higher in peaks: BODY, the level under the
 * top (32768 = 1x), gives a hot patch its headroom back */
#define SD_EDGE(split3, type, gain, alt, level, body) .d_split = {SD_DHZ_200, SD_DHZ_1200, split3}, \
    .d_xover = SD_DX_LR4, .d_type = {SD_DS_NONE, SD_DS_NONE, SD_DS_NONE, type}, .d_gain = {0, 0, 0, gain}, \
    .d_alt = {0, 0, 0, alt}, .d_level = {body, body, body, level}, .d_mix = 65535
/* the new collection's: WARP at the saw (13312 the triangle, toward the pulse above it); a cutoff at a MIDI note */
#define SD_WP_SAW 26624u
#define SD_FC(midi) ((uint16_t)(((midi) - 16) * 65535 / 114))
#define SD_LDIV(i) ((uint16_t)((i) * 65536u / 12u + 100u))   /* an LFO synced: 1 2 3 4 6 8 12 16 24 32 48 64 16ths */
static const SdPatch SD_PATCH[SD_PATCHES] = {
    {   /* DISTROY (§7 STAR DISTROYER, the voice's part): four FM chains at the field's centre, the outer two (GROWL)
         * folded and squashed in the costliest kernel, the middle two (BELL, EPIANO) plain since C1: the CPU's worst
         * case all the same, with the whole chain after it */
        .trim = -3, .bank = {BK_GROWL, BK_BELL_LIGHT, BK_EPIANO_LIGHT, BK_GROWL}, .x = 32768, .y = 32768, .width = 30000,
        .xm = 50000, .warp = 50000, .fold = 30000, .dt = 20000,
        .e1_a = 5, .e1_d = 70, .e1_s = 100, .e1_r = 40, .e2_a = 0, .e2_d = 60, .e2_s = 80, .e2_r = 40,
        /* and the Reactor in its costliest model, BP12, half wet, its stages at random */
        .stage = {SD_RST_VOWEL(SD_RV_A), SD_RST_VOWEL(SD_RV_ER), SD_RCH(48, 10000u, 0, 7, 12, 19), SD_RST_VOWEL(SD_RV_OO),
                  SD_RCH(48, 10000u, 0, 3, 10, 15), SD_RST_VOWEL(SD_RV_EE)},
        .r_pan = {-24000, 24000, -8000, 8000}, .r_model = SD_RM_BP12, .r_res = 36000, .r_slew = 24000,
        .r_drive = 9000, .r_mix = 26000, .r_track = SD_RT_HARM, .r_low = 20000, .r_div = 2, .r_cmode = SD_RC_RND,
        .r_max = 5,
        /* and DISTROY, CMP (chosen at S1: LR4 here with SPACE reads 52 %), a shaper in every band */
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_CMP,
        .d_type = {SD_DS_SOFT, SD_DS_CRUSH, SD_DS_FOLD, SD_DS_HARD}, .d_gain = {5000, 14000, 22000, 24000},
        .d_alt = {20000, 34000, 34000, 24000}, .d_bias = {4000, 0, -6000, 0}, .d_level = {26000, 15500, 15500, 10000},
        .d_pan = {0, -12000, 12000, 0}, .d_mix = 36000,   /* the lows nearly clean, the top destroyed (C1+) */
        /* D2: PHAT thickens the low band (T -20 dBFS); SHARP spits the crushed and the hard bands' attacks */
        .d_shape = {-16384, 16384, 0, 26214}, .d_body = {46421, 24576, 0, 19114},
        /* D3: the feedback on (§7), FB 0.19 (from ~0.27 this loop holds itself up after release: the bands' gain is
         * in it), 6 ms with the right a fifth shorter, TAME a third of the way */
        .d_fb = 14000, .d_ftime = 16000, .d_spread = 22000, .d_tame = 22000,
        /* and SPACE, all of it: the worst case */
        .s_time = SD_SPDIV(2), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 40000, .s_dtone = 30000, .s_dlevel = 18000,
        .s_size = 65535, .s_decay = 50000, .s_rtone = 36000, .s_rlevel = 16000,
        /* the Reactor and the warp sampled and held a sixteenth; the top band's gain a new step each quarter; the
         * wheel feeds the loop */
        .mod = {{SD_MS_LFO1, SD_MD_RPOS, 8274}, {SD_MS_LFO1, SD_MD_WARP, 4965}, {SD_MS_LFO2, SD_MD_DHIGH, 9929},
                {SD_MS_WHEEL, SD_MD_DFB, 13239},
                /* the engines talking: the bass pushes the stages, the split sweeps the loop */
                {SD_MS_DLOW, SD_MD_RPOS, 6620}, {SD_MS_STEP, SD_MD_DSPLIT, 6620}}, .lfo_shape = {SD_LS_SH, SD_LS_SH},
                        .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC},
        .lfo_rate = {100, 16484}, .mac4 = SD_M4_BODY},
    {   /* VOYAGE (§7 VOYAGE): clean additive (A) -> FM bell (B) -> folded growl (C) -> organ (D), COURSE from the gate
         * around the field, the banks spread in stereo. C warps and folds by its own offsets, as a mod slot would */
        .trim = 5, .bank = {BK_GLASS, BK_BELL, SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 30000, S_),
                                                SD_OSC(1, 0, 60000, C_), 0, 22000, 12000, 0), BK_ORGAN},
        .xm = 40000, .width = 40000, SD_CORNER_A,
        .course = 1, .cell_x = {0, 65535, 65535, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {1500, 1500, 1500},
        .e1_a = 10, .e1_d = 90, .e1_s = 100, .e1_r = 60, .e2_a = 0, .e2_d = 70, .e2_s = 60, .e2_r = 60,
        .s_time = SD_SPDIV(8), .s_mode = SD_SP_SYNC, .s_fb = 22000, .s_dtone = 36000, .s_dlevel = 14000,
        .s_size = 65535, .s_decay = 48000, .s_rtone = 40000, .s_rlevel = 18000,
        /* the nav stepping round the field: X a new place each quarter, Y each half (S&H, glided ~10 ms). (Two slow
         * free LFOs drifted it as a random vowel; stepped random is much better) */
        .mod = {{SD_MS_LFO1, SD_MD_X, 9929}, {SD_MS_LFO2, SD_MD_Y, 9929},
                /* the engines talking: Y walks the loop, the echoes bend the warp */
                {SD_MS_STEP, SD_MD_Y, 8274}, {SD_MS_ECHO, SD_MD_WARP, 3310}}, .lfo_shape = {SD_LS_SH, SD_LS_SH},
        .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC}, .lfo_rate = {16484, 27406}, .mac4 = SD_M4_DROWN},
    {   /* GROWL: a chain 1 -> 2 -> 3 -> 4 between triangle and saw, folded a little, low. (Past the saw toward pulse
         * with more fold and FM it was so noisy it was nearly unusable: the chain's aliasing, -14 dB of it off the
         * harmonics on a held D2; now -24) */
        .trim = 6, .bank = SD_SAME(BK_GROWL), SD_CORNER_A, .xm = 24000, .warp = 20000, .fold = 8000, .e2_fold = 6000,
        .e1_a = 5, .e1_d = 70, .e1_s = 80, .e1_r = 40, .e2_a = 0, .e2_d = 48, .e2_s = 40, .e2_r = 40,
        /* the edge: the bottom clean, the top destroyed in time: over 5 kHz folded, its drive a new step each eighth
         * with the voice's fold (no SHARP: that fold spits already) */
        SD_EDGE(SD_DHZ_5000, SD_DS_FOLD, 16000, 36000, 10000, 31000u),
        /* the fold stepping on eighths (S&H: a rhythm, not a wobble), the top's gain with it; harder notes growl more */
        .mod = {{SD_MS_LFO1, SD_MD_FOLD, 6620}, {SD_MS_VEL, SD_MD_XM, 6620}, {SD_MS_LFO1, SD_MD_DHIGH, 2600}},
        .lfo_shape = {SD_LS_SH}, .lfo_mode = {SD_LM_SYNC}, .lfo_rate = {5561}, .mac4 = SD_M4_BODY},
    {   /* GRAVITY: the growl bass, four growls: A a sine body under a light FM, B the 2:1 "yoi", C a folded saw, D
         * metal (3:1 and 5). A wobble on the tempo sweeps the Reactor between OO and A and the FM with it; the wheel
         * speeds the wobble up (a half note .. 16ths) and moves toward B; aftertouch toward D; each note says "yow"
         * (ENV2 on FREQ); velocity folds; the sub's own band ducks the top's grit. SUB under, dry */
        .trim = -21, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 44000, C_), SD_OSC(1, 0, 18000, S_), SD_OSC(1, 0, 30000, C_),
                                          SD_OSC(2, 0, 10000, C_), 0, 0, 0, 0),
                              SD_BANK_OFS(SD_OSC(2, 0, 30000, S_), SD_OSC(1, 0, 48000, C_), SD_OSC(3, 0, 16000, S_),
                                          SD_OSC(1, 0, 30000, C_), 12000, 0, 0, 0),
                              SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(1, 7, 30000, C_), SD_OSC(2, 0, 14000, C_),
                                          SD_OFF, 0, 10000, 14000, 0),
                              SD_BANK_OFS(SD_OSC(3, 0, 24000, S_), SD_OSC(1, 0, 44000, C_), SD_OSC(5, 3, 10000, S_),
                                          SD_OSC(1, 0, 26000, C_), 8000, 0, 6000, 0)},
        .x = 0, .y = 65535, .xm = 24000, .warp = 16000, .fold = 4000, .width = 16000,
        .e1_a = 0, .e1_d = 80, .e1_s = 110, .e1_r = 30, .e2_a = 0, .e2_d = 50, .e2_s = 30, .e2_r = 30,
        .stage = {SD_RST_VBP(SD_RV_OO), SD_RST_VBP(SD_RV_A)},
        .r_pan = {-8000, 8000, -4000, 4000}, .r_model = SD_RM_VBP, .r_res = 30000, .r_slew = 6000, .r_drive = 16000,
        .r_mix = 42000, .r_track = 2u << 4, .r_low = 24000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_HARD, SD_DS_FOLD, SD_DS_FOLD}, .d_gain = {0, 26000, 30000, 16000},
        .d_alt = {0, 26000, 34000, 30000}, .d_bias = {0, 6000, 12000, 0}, .d_level = {30000, 36000, 26000, 8000},
        .d_shape = {-20000, 0, 0, 0}, .d_body = {48000, 0, 0, 0}, .d_tame = 32000, .d_fb = 8000, .d_ftime = 12000,
        .d_mix = 64000,
        .mod = {{SD_MS_LFO1, SD_MD_RPOS, 14000}, {SD_MS_LFO1, SD_MD_XM, 6000}, {SD_MS_WHEEL, SD_MD_L1RATE, -26000},
                {SD_MS_WHEEL, SD_MD_X, 30000}, {SD_MS_AFTER, SD_MD_Y, -30000}, {SD_MS_ENV2, SD_MD_RFREQ, 14000},
                {SD_MS_ENV2, SD_MD_PITCH, 700}, {SD_MS_DLOW, SD_MD_DHIGH, -6000}},
        .lfo_shape = {SD_LS_SIN}, .lfo_mode = {SD_LM_SYNC}, .lfo_rate = {30000},
        .mac4 = SD_M4_BODY, .sub_lvl = 15000, .sub_src = SD_SUB_A},
    {   /* CHROME: glossy, rubbery, synthetic: each note a journey (COURSE): a metallic 3:1 ping (B) -> bright 2:1 (D)
         * -> a square (C) -> a pure sine (A), in 350 ms; its pitch bending down into it (ENV2: a boing); a resonant
         * LP24 snapping shut (the squelch); velocity the ping's FM, the wheel the squelch's resonance, aftertouch
         * folds the body; hard edges in the low mids; dry, tight */
        .trim = -18, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 52000, C_), SD_OFF, SD_OFF, SD_OFF, 0, 0, 0, 0),
                              SD_BANK_OFS(SD_OSC(3, 7, 34000, S_), SD_OSC(1, 0, 46000, C_), SD_OSC(7, 0, 8000, S_),
                                          SD_OSC(1, 0, 20000, C_), 16000, 0, 0, 0),
                              SD_BANK_OFS(SD_OSC(1, 0, 44000, C_), SD_OSC(3, 0, 8000, C_), SD_OFF, SD_OFF,
                                          0, 22000, 0, 0),
                              SD_BANK_OFS(SD_OSC(2, 0, 30000, S_), SD_OSC(1, 0, 46000, C_), SD_OFF, SD_OFF,
                                          10000, 0, 8000, 0)},
        .x = 65535, .y = 65535, .xm = 22000, .warp = 24000,
        .course = 1, .cell_x = {65535, 0, 65535, 0}, .cell_y = {65535, 0, 0, 65535}, .cell_ms = {40, 90, 220},
        .e1_a = 0, .e1_d = 64, .e1_s = 96, .e1_r = 22, .e2_a = 0, .e2_d = 56, .e2_s = 0, .e2_r = 20,
        .f_type = SD_FT_LP24, .f_cut = SD_FC(78), .f_res = 50000, .f_env = 24000, .f_key = 40,
        .e3_a = 0, .e3_d = 58, .e3_s = 20, .e3_r = 24,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_HARD, SD_DS_FOLD, SD_DS_SOFT}, .d_gain = {0, 26000, 18000, 8000},
        .d_alt = {0, 28000, 24000, 12000}, .d_level = {32768, 24000, 22000, 16000}, .d_tame = 26000, .d_mix = 60000,
        .mod = {{SD_MS_ENV2, SD_MD_PITCH, 9000}, {SD_MS_VEL, SD_MD_XM, 9000}, {SD_MS_WHEEL, SD_MD_FRES, 12000},
                {SD_MS_AFTER, SD_MD_FOLD, 14000}, {SD_MS_KEY, SD_MD_FCUT, 6000}},
        .mac4 = SD_M4_BODY, .sub_lvl = 10000, .sub_src = SD_SUB_A},
    {   /* ABYSS: the wide dark bass: A three saws a few cents apart and a sine, B saws an octave up, C a square, D a
         * folded saw; a held note travels A -> B -> D -> C over five seconds (it rises, grits, hollows); the detune
         * breathing slowly; the wheel opens the LP24, aftertouch drives the low mids, higher notes brighter; SUB
         * under */
        .trim = -16, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 30000, C_), SD_OSC(1, 9, 30000, C_), SD_OSC(1, -11, 30000, C_),
                                         SD_OSC(1, 0, 24000, C_), 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(2, 5, 26000, C_), SD_OSC(2, -6, 26000, C_), SD_OSC(1, 0, 30000, C_),
                                         SD_OFF, 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 36000, C_), SD_OSC(1, 7, 30000, C_), SD_OFF, SD_OFF,
                                         0, 20000, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 36000, C_), SD_OSC(1, -8, 30000, C_), SD_OFF, SD_OFF,
                                         0, 0, 18000, 0)},
        .x = 0, .y = 65535, .warp = SD_WP_SAW, .dt = 20000, .width = 34000,
        .course = 1, .cell_x = {0, 65535, 0, 65535}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {1200, 1600, 2200},
        .e1_a = 2, .e1_d = 80, .e1_s = 118, .e1_r = 36, .e2_a = 0, .e2_d = 46, .e2_s = 0, .e2_r = 20,
        .f_type = SD_FT_LP24, .f_cut = SD_FC(90), .f_res = 30000, .f_env = 18000, .f_key = 30,
        .e3_a = 0, .e3_d = 60, .e3_s = 50, .e3_r = 40,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_HARD, SD_DS_FOLD, SD_DS_SOFT}, .d_gain = {0, 26000, 22000, 8000},
        .d_alt = {0, 22000, 26000, 12000}, .d_level = {32768, 26000, 20000, 12000}, .d_shape = {-14000, 0, 0, 0},
        .d_body = {44000, 0, 0, 0}, .d_tame = 28000, .d_mix = 62000,
        .s_size = 30000, .s_decay = 26000, .s_rtone = 30000, .s_rlevel = 6000,
        .mod = {{SD_MS_WHEEL, SD_MD_FCUT, 12000}, {SD_MS_AFTER, SD_MD_DGAIN, 9000}, {SD_MS_LFO2, SD_MD_DT, 4000},
                {SD_MS_KEY, SD_MD_FCUT, 5000}, {SD_MS_ENV2, SD_MD_PITCH, 500}, {SD_MS_VEL, SD_MD_FENV, 8000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_TRI}, .lfo_rate = {0, 17372 /* 0.15 Hz */},
        .mac4 = SD_M4_BODY, .sub_lvl = 18000, .sub_src = SD_SUB_A},
    {   /* NEON: the retro bass, played: soft notes two saws and their octave (A), hard ones a square (B), high ones
         * hollow (D: odd harmonics); a pluck through LP24 that velocity opens further; aftertouch thickens the
         * detune, the wheel opens it; a slow width */
        .trim = -12, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(1, 7, 30000, C_), SD_OSC(2, 0, 12000, C_),
                                        SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 44000, C_), SD_OSC(2, 0, 14000, C_), SD_OFF, SD_OFF,
                                        0, 20000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(3, 0, 9000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(3, 0, 13000, C_), SD_OSC(5, 0, 8000, C_),
                                        SD_OFF, 0, -12000, 0, 0)},
        .x = 0, .y = 65535, .warp = SD_WP_SAW, .dt = 9000, .width = 24000,
        .e1_a = 0, .e1_d = 66, .e1_s = 90, .e1_r = 24, .e2_a = 0, .e2_d = 44, .e2_s = 0, .e2_r = 20,
        .f_type = SD_FT_LP24, .f_cut = SD_FC(56), .f_res = 34000, .f_env = 28000, .f_key = 50,
        .e3_a = 0, .e3_d = 52, .e3_s = 10, .e3_r = 30,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_SOFT}, .d_gain = {0, 18000, 16000, 6000},
        .d_alt = {0, 16000, 20000, 10000}, .d_level = {32768, 28000, 22000, 14000}, .d_tame = 26000, .d_mix = 56000,
        .s_size = 24000, .s_decay = 22000, .s_rtone = 34000, .s_rlevel = 4000,
        .mod = {{SD_MS_VEL, SD_MD_X, 26000}, {SD_MS_KEY, SD_MD_Y, -20000}, {SD_MS_VEL, SD_MD_FENV, 9000},
                {SD_MS_AFTER, SD_MD_DT, 12000}, {SD_MS_WHEEL, SD_MD_FCUT, 9000}, {SD_MS_LFO2, SD_MD_WIDTH, 6000},
                {SD_MS_ENV2, SD_MD_PITCH, 600}, {SD_MS_VEL, SD_MD_DGAIN, 7000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_TRI}, .lfo_rate = {0, 21777 /* 0.25 Hz */},
        .mac4 = SD_M4_CUTOFF},
    {   /* CORE: the sub that can growl: A a pure sine, B the sine and its octave (warmer), C its third harmonic
         * (hollow), D folded; aftertouch warms it toward B, the wheel folds it toward D; velocity drives the low
         * band; soft at the start */
        .trim = 0, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 56000, C_), SD_OFF, SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 50000, C_), SD_OSC(2, 0, 18000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 50000, C_), SD_OSC(3, 0, 14000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 52000, C_), SD_OFF, SD_OFF, SD_OFF, 0, 0, 26000, 0)},
        .x = 0, .y = 65535, .warp = 3000,
        .e1_a = 4, .e1_d = 80, .e1_s = 122, .e1_r = 30, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_SOFT, SD_DS_NONE, SD_DS_NONE, SD_DS_NONE}, .d_gain = {8000, 0, 0, 0},
        .d_level = {32768, 32768, 20000, 12000}, .d_mix = 40000,
        .mod = {{SD_MS_AFTER, SD_MD_X, 32767}, {SD_MS_WHEEL, SD_MD_Y, -32767}, {SD_MS_VEL, SD_MD_DGAIN, 7000}},
        .mac4 = SD_M4_BODY},
    {   /* NOVA: the chord lead: A four saws wide, B saws an octave up, C a square stack, D hollow; in POLY each voice
         * its first bank (A; played MONO a held note opens to B and drifts on to D); the level pumping on the
         * quarters and the LP12 breathing with it (a SAW on VOLUME and CUTOFF: the side-chain feel); velocity
         * opens the attack, the wheel the filter, aftertouch toward the square; the echoes fold it a little; dotted
         * eighths, a plate */
        .trim = -2, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 30000, C_), SD_OSC(1, 11, 28000, C_), SD_OSC(1, -9, 28000, C_),
                                        SD_OSC(2, 4, 12000, C_), 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(2, 7, 26000, C_), SD_OSC(2, -8, 26000, C_), SD_OSC(1, 0, 22000, C_),
                                        SD_OSC(4, 0, 8000, C_), 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 30000, C_), SD_OSC(1, 9, 28000, C_), SD_OSC(2, 0, 14000, C_),
                                        SD_OFF, 0, 20000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 34000, C_), SD_OSC(3, 0, 14000, C_), SD_OSC(5, 0, 9000, C_),
                                        SD_OSC(7, 0, 5000, C_), 0, -14000, 0, 0)},
        .x = 0, .y = 65535, .warp = SD_WP_SAW, .dt = 18000, .width = 56000,
        .course = 1, .cell_x = {0, 65535, 0, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {250, 2500, 4000},
        .e1_a = 0, .e1_d = 74, .e1_s = 100, .e1_r = 44, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .f_type = SD_FT_LP12, .f_cut = SD_FC(86), .f_res = 28000, .f_env = 20000, .f_key = 40,
        .e3_a = 0, .e3_d = 56, .e3_s = 55, .e3_r = 40,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_SOFT, SD_DS_NONE}, .d_gain = {0, 16000, 14000, 0},
        .d_alt = {0, 14000, 16000, 0}, .d_level = {32768, 28000, 24000, 16000}, .d_tame = 26000, .d_mix = 52000,
        .s_time = SD_SPDIV(6), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 26000, .s_dtone = 28000, .s_dlevel = 12000,
        .s_size = 46000, .s_decay = 40000, .s_rtone = 34000, .s_rlevel = 12000,
        .mod = {{SD_MS_LFO1, SD_MD_VOL, -16000}, {SD_MS_LFO1, SD_MD_FCUT, 5000}, {SD_MS_VEL, SD_MD_FENV, 10000},
                {SD_MS_WHEEL, SD_MD_FCUT, 10000}, {SD_MS_AFTER, SD_MD_X, 20000}, {SD_MS_ECHO, SD_MD_FOLD, 3000}},
        .lfo_shape = {SD_LS_SAW}, .lfo_mode = {SD_LM_SYNC}, .lfo_rate = {19000},
        .mac4 = SD_M4_CUTOFF},
    {   /* SOLAR: the soaring lead: each note blooms: a soft glassy start (C: sines 1..4) -> two saws (A) in half a
         * second -> a saw and its fifth (B) by three seconds -> a buzz (D: 1:1 FM) when held long; a 5 Hz vibrato
         * from each note; aftertouch bends it up a whole tone, the wheel opens LP24; long ping-pong, a big plate */
        .trim = -12, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 36000, C_), SD_OSC(1, 6, 30000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 38000, C_), SD_OSC(3, 0, 12000, C_), SD_OSC(1, -5, 24000, C_),
                                        SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 36000, C_), SD_OSC(2, 0, 16000, C_), SD_OSC(3, 0, 9000, C_),
                                        SD_OSC(4, 0, 6000, C_), 0, -26624, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(1, 0, 40000, C_), SD_OSC(1, 4, 20000, C_),
                                        SD_OFF, 6000, 0, 0, 0)},
        .x = 65535, .y = 0, .warp = SD_WP_SAW, .xm = 10000, .dt = 8000, .width = 30000,
        .course = 1, .cell_x = {65535, 0, 65535, 0}, .cell_y = {0, 65535, 65535, 0}, .cell_ms = {500, 2500, 4000},
        .e1_a = 18, .e1_d = 80, .e1_s = 116, .e1_r = 60, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .f_type = SD_FT_LP24, .f_cut = SD_FC(84), .f_res = 30000, .f_env = 16000, .f_key = 30,
        .e3_a = 0, .e3_d = 58, .e3_s = 70, .e3_r = 50,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_SOFT}, .d_gain = {0, 18000, 18000, 6000},
        .d_alt = {0, 16000, 22000, 10000}, .d_level = {32768, 28000, 22000, 12000}, .d_tame = 30000, .d_mix = 58000,
        .s_time = SD_SPDIV(10), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 34000, .s_dtone = 26000, .s_dlevel = 16000,
        .s_size = 60000, .s_decay = 52000, .s_rtone = 32000, .s_rlevel = 18000,
        .mod = {{SD_MS_LFO2, SD_MD_PITCH, 200}, {SD_MS_AFTER, SD_MD_PITCH, 2730}, {SD_MS_WHEEL, SD_MD_FCUT, 9000},
                {SD_MS_VEL, SD_MD_FENV, 6000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_SIN}, .lfo_mode = {0, SD_LM_KEY}, .lfo_rate = {0, 47600},
        .mac4 = SD_M4_DROWN},
    {   /* LASER: the 80s pulse, alive: A a pulse, B its octave, C a saw, D a thin pulse; each note a chirp (ENV2 on
         * PITCH: it falls a semitone into the note); its width drifting slowly (PWM); velocity toward the octave,
         * aftertouch thinner, the wheel opens LP24; short echoes */
        .trim = -5, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(2, 0, 8000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(2, 0, 34000, C_), SD_OSC(1, 0, 16000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(2, 0, 10000, C_), SD_OFF, SD_OFF, 0, -9000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OFF, SD_OFF, SD_OFF, 0, 14000, 0, 0)},
        .x = 0, .y = 65535, .warp = 36000, .width = 20000,
        .e1_a = 0, .e1_d = 70, .e1_s = 100, .e1_r = 30, .e2_a = 0, .e2_d = 50, .e2_s = 0, .e2_r = 30,
        .f_type = SD_FT_LP24, .f_cut = SD_FC(86), .f_res = 36000, .f_env = 20000, .f_key = 60,
        .e3_a = 0, .e3_d = 54, .e3_s = 45, .e3_r = 30,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_HARD, SD_DS_SOFT, SD_DS_NONE}, .d_gain = {0, 16000, 12000, 0},
        .d_alt = {0, 18000, 14000, 0}, .d_level = {32768, 26000, 22000, 14000}, .d_tame = 28000, .d_mix = 52000,
        .s_time = SD_SPDIV(4), .s_mode = SD_SP_SYNC, .s_fb = 22000, .s_dtone = 30000, .s_dlevel = 12000,
        .s_size = 34000, .s_decay = 30000, .s_rtone = 34000, .s_rlevel = 8000,
        .mod = {{SD_MS_LFO2, SD_MD_WARP, 5000}, {SD_MS_ENV2, SD_MD_PITCH, 1400}, {SD_MS_VEL, SD_MD_X, 24000},
                {SD_MS_AFTER, SD_MD_Y, -24000}, {SD_MS_WHEEL, SD_MD_FCUT, 9000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_TRI}, .lfo_rate = {0, 23349 /* 0.3 Hz */},
        .mac4 = SD_M4_CUTOFF},
    {   /* SIGNAL: the sci-fi lead: FM tones (A 2:1, B 3:1, C 1:1 folded, D a sine and its fifth harmonic) through
         * four vowels, OO A E EE; inside each note ENV2 sweeps them ("wow-yeah"), a 6 Hz flutter on the formants; the
         * Reactor's stage moves the nav (the engines talking: the vowel picks the tone); the wheel pushes the vowels
         * on, aftertouch adds FM; ping-pong, a plate */
        .trim = 3, .bank = {SD_BANK_OFS(SD_OSC(2, 0, 20000, S_), SD_OSC(1, 0, 42000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(3, 0, 18000, S_), SD_OSC(1, 0, 42000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 24000, S_), SD_OSC(1, 0, 42000, C_), SD_OFF, SD_OFF, 0, 0, 14000, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 42000, C_), SD_OSC(5, 0, 7000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0)},
        .x = 0, .y = 65535, .xm = 22000, .warp = 14000,
        .e1_a = 6, .e1_d = 80, .e1_s = 110, .e1_r = 50, .e2_a = 20, .e2_d = 68, .e2_s = 25, .e2_r = 50,
        .stage = {SD_RST_VBP(SD_RV_OO), SD_RST_VBP(SD_RV_A), SD_RST_VBP(SD_RV_E), SD_RST_VBP(SD_RV_EE)},
        .r_pan = {-10000, 10000, -4000, 4000}, .r_model = SD_RM_VBP, .r_res = 34000, .r_slew = 12000,
        .r_drive = 18000, .r_mix = 48000, .r_track = 4u << 4, .r_low = 24000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_FOLD, SD_DS_SOFT}, .d_gain = {0, 14000, 18000, 6000},
        .d_alt = {0, 14000, 24000, 10000}, .d_level = {32768, 28000, 22000, 12000}, .d_tame = 28000, .d_mix = 54000,
        .s_time = SD_SPDIV(8), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 30000, .s_dtone = 26000, .s_dlevel = 14000,
        .s_size = 52000, .s_decay = 46000, .s_rtone = 32000, .s_rlevel = 14000,
        .mod = {{SD_MS_ENV2, SD_MD_RPOS, 18000}, {SD_MS_LFO1, SD_MD_RFREQ, 1500}, {SD_MS_RSTAGE, SD_MD_X, 20000},
                {SD_MS_WHEEL, SD_MD_RPOS, 12000}, {SD_MS_AFTER, SD_MD_XM, 9000}},
        .lfo_shape = {SD_LS_SIN}, .lfo_mode = {SD_LM_KEY}, .lfo_rate = {49178 /* 6.0 Hz */},
        .mac4 = SD_M4_DROWN},
    {   /* AURORA (cue 2): the GLASS bank (harmonics 1 2 3 4) slow in and out (85 / 90), wide, in the big hall, its
         * delay on dotted quarters; DROWN on MACRO 4 (POLY in the cue) */
        .trim = 1, .bank = SD_SAME(BK_GLASS),
        .e1_a = 85, .e1_d = 80, .e1_s = 120, .e1_r = 90, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        SD_CORNER_A, .width = 65535,
        .s_time = 56000, .s_fb = 38000, .s_dtone = 22000, .s_dlevel = 22937, .s_size = 64000, .s_decay = 57000,
        .s_rtone = 30000, .s_rlevel = 34000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        /* the edge: its top (over 2.5 kHz) folded to a sheen, the fold a new depth each bar */
        SD_EDGE(SD_DHZ_2500, SD_DS_FOLD, 18000, 32000, 16000, 32768u),
        .mod = {{SD_MS_AFTER, SD_MD_XM, 9929}, {SD_MS_WHEEL, SD_MD_WARP, 13239}, {SD_MS_LFO1, SD_MD_FOLD, 2600}},
        .lfo_shape = {SD_LS_SH}, .lfo_mode = {SD_LM_SYNC}, .lfo_rate = {38300}, .mac4 = SD_M4_DROWN},
    {   /* NEBULA: a pad (four voices: POLY) whose character is the Reactor: four KP combs tuned to the chord's
         * tones and kept on the note's harmonics (KEY+H), bowed by a soft triangle stack; their chord steps once a bar
         * (S&H on POS), the fold grows inside each note; the top band breathes, SOFT, on halves; the room open */
        .trim = -9, .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 34000, C_), SD_OSC(2, 5, 16000, C_), SD_OSC(4, -6, 9000, C_),
                                SD_OSC(1, -9, 22000, C_))), SD_CORNER_A, .warp = 12000, .dt = 12000, .width = 46000,
        .e1_a = 70, .e1_d = 100, .e1_s = 110, .e1_r = 96, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RCH(48, 20000u, 0, 7, 12, 16), SD_RCH(48, 20000u, -3, 4, 9, 12),
                  SD_RCH(48, 20000u, -5, 2, 7, 14), SD_RCH(48, 20000u, 0, 4, 7, 19),
                  SD_RCH(48, 20000u, -7, 0, 5, 12), SD_RCH(48, 20000u, -2, 5, 10, 17)},
        .r_pan = {-26000, 26000, -12000, 12000}, .r_model = SD_RM_KP, .r_res = 50000, .r_slew = 30000,
        .r_drive = 9000, .r_mix = 24000, .r_track = SD_RT_KEY | SD_RT_HARM, .r_low = 24000, .r_max = 5,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_CMP,
        .d_type = {SD_DS_NONE, SD_DS_NONE, SD_DS_SOFT, SD_DS_SOFT}, .d_gain = {0, 0, 10000, 14000},
        .d_alt = {0, 0, 14000, 16000}, .d_level = {32768, 32768, 26000, 18000}, .d_mix = 36000, .d_tame = 12000,
        .s_time = SD_SPDIV(10), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 28000, .s_dtone = 26000, .s_dlevel = 12000,
        .s_size = 65535, .s_decay = 54000, .s_rtone = 32000, .s_rlevel = 22000,
        .mod = {{SD_MS_LFO1, SD_MD_RPOS, 11584}, {SD_MS_LFO2, SD_MD_FOLD, 4634}, {SD_MS_LFO1, SD_MD_DHIGH, 4965},
                /* the engines talking: the echoes fold it */
                {SD_MS_ECHO, SD_MD_FOLD, 3310}},
        .lfo_shape = {SD_LS_SH, SD_LS_RAMP}, .lfo_mode = {SD_LM_SYNC, SD_LM_ONE}, .lfo_rate = {38329, 21777},
        .mac4 = SD_M4_DROWN},
    {   /* MASS: the fullest pad: A the note's octaves and fifth, detuned; B four saws wide; C hollow (odd harmonics);
         * D a soft 2:1 glow; a held chord travels A -> B -> C -> D over eight seconds; four KP combs ringing on the
         * octave and the fifth (one stage, KEY+H); the wheel opens it toward B, aftertouch makes the combs ring
         * longer and drives the mids; the echoes fold the tail; SUB under; the plate at its largest */
        .trim = -11, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 34000, C_), SD_OSC(2, 5, 24000, C_), SD_OSC(3, -5, 16000, C_),
                                        SD_OSC(4, 7, 10000, C_), 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 28000, C_), SD_OSC(1, 11, 26000, C_), SD_OSC(1, -9, 26000, C_),
                                        SD_OSC(2, 4, 14000, C_), 0, 12000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 36000, C_), SD_OSC(3, 0, 14000, C_), SD_OSC(5, 0, 9000, C_),
                                        SD_OSC(7, 0, 5000, C_), 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 40000, C_), SD_OSC(1, 6, 24000, C_),
                                        SD_OFF, 10000, 0, 0, 0)},
        .x = 0, .y = 65535, .warp = 18000, .fold = 4000, .dt = 16000, .width = 60000,
        .course = 1, .cell_x = {0, 65535, 65535, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {1500, 3000, 3500},
        .e1_a = 84, .e1_d = 100, .e1_s = 120, .e1_r = 100, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RCH(48, 20000u, 0, 7, 12, 19)},
        .r_pan = {-26000, 26000, -12000, 12000}, .r_model = SD_RM_KP, .r_res = 48000, .r_slew = 30000,
        .r_drive = 9000, .r_mix = 26000, .r_track = SD_RT_KEY | SD_RT_HARM | 1u << 4, .r_low = 24000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_SOFT, SD_DS_NONE}, .d_gain = {0, 10000, 12000, 0},
        .d_alt = {0, 10000, 14000, 0}, .d_level = {32768, 30000, 24000, 16000}, .d_tame = 24000, .d_mix = 40000,
        .s_time = SD_SPDIV(10), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 30000, .s_dtone = 22000, .s_dlevel = 12000,
        .s_size = 65535, .s_decay = 58000, .s_rtone = 30000, .s_rlevel = 26000,
        .mod = {{SD_MS_WHEEL, SD_MD_X, 24000}, {SD_MS_AFTER, SD_MD_RRES, 9000}, {SD_MS_AFTER, SD_MD_DGAIN, 8000},
                {SD_MS_ECHO, SD_MD_FOLD, 4000}, {SD_MS_VEL, SD_MD_RMIX, 6000}},
        .mac4 = SD_M4_DROWN, .sub_lvl = 24000, .sub_src = SD_SUB_A},
    {   /* OVERTONE: the overtones sing: A a stack toward the saw, B brighter (its 3rd and 5th up), C a soft 1:1 FM, D
         * hollow; one stage of narrow BP12 peaks on the note's 5th to 8th harmonics (KEY+H), spread wide; a slow
         * drift moves them, and as they snap onto the harmonics the overtones step by themselves, free of the beat;
         * aftertouch lifts them up the series, the wheel sharpens them; a held chord A -> B -> D over six seconds */
        .trim = 0, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(2, 3, 20000, C_), SD_OSC(3, -4, 14000, C_),
                                        SD_OSC(1, -7, 26000, C_), 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 34000, C_), SD_OSC(3, 0, 20000, C_), SD_OSC(5, 0, 14000, C_),
                                        SD_OSC(1, 6, 24000, C_), 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(1, 0, 42000, C_), SD_OSC(1, 5, 22000, C_),
                                        SD_OFF, 8000, -12000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 38000, C_), SD_OSC(3, 0, 16000, C_), SD_OSC(5, 0, 11000, C_),
                                        SD_OSC(7, 0, 8000, C_), 0, -8000, 0, 0)},
        .x = 0, .y = 65535, .warp = 26000, .dt = 9000, .width = 54000,
        .course = 1, .cell_x = {0, 65535, 0, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {2000, 4000, 4000},
        .e1_a = 84, .e1_d = 100, .e1_s = 115, .e1_r = 98, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {{{22492u, 23301u, 23985u, 24576u}, {4200u, 4200u, 4200u, 4200u}}},
        .r_pan = {-22000, -8000, 8000, 22000}, .r_model = SD_RM_BP12, .r_res = 50000, .r_slew = 20000,
        .r_drive = 9000, .r_mix = 52000, .r_track = SD_RT_KEY | SD_RT_HARM | 1u << 4, .r_low = 24000,
        .s_time = SD_SPDIV(10), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 26000, .s_dtone = 26000, .s_dlevel = 10000,
        .s_size = 62000, .s_decay = 54000, .s_rtone = 32000, .s_rlevel = 22000,
        .mod = {{SD_MS_LFO2, SD_MD_RFREQ, 5000}, {SD_MS_AFTER, SD_MD_RFREQ, 9000}, {SD_MS_WHEEL, SD_MD_RRES, 8000},
                {SD_MS_VEL, SD_MD_RDRIVE, 6000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_TRI}, .lfo_mode = {0, SD_LM_KEY}, .lfo_rate = {0, 15000 /* ~0.1 Hz */},
        .mac4 = SD_M4_DROWN},
    {   /* HORIZON: the warm retro pad: A two saws a little apart, B saws and their octave, C a soft triangle, D a
         * hollow square; each chord starts on A (in POLY a voice keeps its first bank; played MONO a held note opens
         * toward B); LP24 swelling open on its own envelope; aftertouch thickens the detune, the wheel opens the
         * filter, velocity the swell; a slow width; dotted quarters, a big plate */
        .trim = 3, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 34000, C_), SD_OSC(1, 8, 32000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 30000, C_), SD_OSC(1, -7, 28000, C_), SD_OSC(2, 5, 18000, C_),
                                        SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(1, 4, 30000, C_), SD_OFF, SD_OFF,
                                        0, -14000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 36000, C_), SD_OSC(1, 6, 30000, C_), SD_OFF, SD_OFF,
                                        0, 16000, 0, 0)},
        .x = 0, .y = 65535, .warp = SD_WP_SAW, .dt = 14000, .width = 58000,
        .course = 1, .cell_x = {0, 65535, 65535, 65535}, .cell_y = {65535, 65535, 65535, 65535}, .cell_ms = {1200, 4000, 4000},
        .e1_a = 80, .e1_d = 100, .e1_s = 118, .e1_r = 96, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .f_type = SD_FT_LP24, .f_cut = SD_FC(88), .f_res = 18000, .f_env = 12000, .f_key = 40,
        .e3_a = 84, .e3_d = 100, .e3_s = 100, .e3_r = 90,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_NONE, SD_DS_NONE}, .d_gain = {0, 8000, 0, 0},
        .d_alt = {0, 8000, 0, 0}, .d_level = {32768, 30000, 26000, 18000}, .d_mix = 30000,
        .s_time = SD_SPDIV(10), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 30000, .s_dtone = 24000, .s_dlevel = 12000,
        .s_size = 62000, .s_decay = 54000, .s_rtone = 30000, .s_rlevel = 22000,
        .mod = {{SD_MS_AFTER, SD_MD_DT, 12000}, {SD_MS_WHEEL, SD_MD_FCUT, 10000}, {SD_MS_VEL, SD_MD_FENV, 9000},
                {SD_MS_LFO2, SD_MD_WIDTH, 6000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_TRI}, .lfo_rate = {0, 21776 /* 0.25 Hz */},
        .mac4 = SD_M4_DROWN},
    {   /* CHOIR: voices: A three saws apart, B their octave (the high voices), C a soft triangle, D a saw and its
         * fifth; through two vowels, OO and AH: each chord opens from OO to AH as it swells (ENV2 on POS), aftertouch
         * shapes the vowel further; in POLY each voice its first bank, A (MONO: a held note travels on to B); the
         * formants a little wider with velocity; a big hall */
        .trim = -3, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 30000, C_), SD_OSC(1, 7, 28000, C_), SD_OSC(1, -8, 28000, C_),
                                        SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(2, 0, 26000, C_), SD_OSC(2, 9, 24000, C_), SD_OSC(1, -5, 20000, C_),
                                        SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(1, 6, 30000, C_), SD_OFF, SD_OFF,
                                        0, -14000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 32000, C_), SD_OSC(3, 0, 12000, C_), SD_OSC(1, -6, 26000, C_),
                                        SD_OFF, 0, 0, 0, 0)},
        .x = 0, .y = 65535, .warp = 24000, .dt = 20000, .width = 60000,
        .course = 1, .cell_x = {0, 65535, 65535, 65535}, .cell_y = {65535, 65535, 65535, 65535}, .cell_ms = {1500, 4000, 4000},
        .e1_a = 88, .e1_d = 100, .e1_s = 118, .e1_r = 98, .e2_a = 88, .e2_d = 100, .e2_s = 127, .e2_r = 90,
        .stage = {SD_RST_VBP(SD_RV_OO), SD_RST_VBP(SD_RV_A)},
        .r_pan = {-14000, 14000, -6000, 6000}, .r_model = SD_RM_VBP, .r_res = 16000, .r_slew = 30000,
        .r_drive = 12000, .r_mix = 32000, .r_track = 2u << 4, .r_low = 24000,
        .s_time = SD_SPDIV(10), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 20000, .s_dtone = 24000, .s_dlevel = 6000,
        .s_size = 64000, .s_decay = 56000, .s_rtone = 30000, .s_rlevel = 26000,
        .mod = {{SD_MS_ENV2, SD_MD_RPOS, 16000}, {SD_MS_AFTER, SD_MD_RFREQ, 5000}, {SD_MS_VEL, SD_MD_RRES, -6000}},
        .mac4 = SD_M4_DROWN},
    {   /* STRINGS (a cello, as close as possible to a recorded C major, then V4 by ear in
         * the duo, tools/cello_v4.txt): a bowed string is a saw through a body.
         * - The source: a plain saw (WARP 25544), slow in (70), a long bow (sustain 118), 8 cents of vibrato at 5.5 Hz.
         * - The body: the Reactor's four BP6s snapped to the note's harmonics (HARM), stages 1-3 a cello's
         *   (~100 200 320 Hz, 1 kHz), 4-6 brighter (~280 420 Hz, 1.05 1.65 kHz); POS on stage 2.
         * - The tilt: DISTROY unshaped as four levels (-22 -4.8 -0.1 +12 dB at 262 750 2380 Hz: the top's +6.8 meant, its
         *   field at the top of the level law, 4x) and TAME; a small
         *   room, mono (the duo's and cue 2's strings pan by the part). The four levels 14 dB down since 2026-11-21
         *   (their sum held DISTROY's safety limiter 10 dB down: a duck on every bow), the level back by TRIM.
         * The earlier STRINGS (matched to a cello's and a violin's scale, KEY moving it from the one to the other) is
         * in the docs' history; sd_strings PARAMS still builds a patch from key-value lines for tuning, and
         * tests/run_sd.sh checks this one renders as tools/cello_v4.txt does. */
        .trim = -14, .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OFF, SD_OFF, SD_OFF)),
        .warp = 25544, .e1_a = 70, .e1_d = 100, .e1_s = 118, .e1_r = 62, .e2_a = 20, .e2_d = 20, .e2_s = 127,
        .e2_r = 45,
        SD_CORNER_A,
        .stage = {{{11098u, 14170u, 16253u, 21303u}, {19074u, 65535u, 65535u, 26028u}},
                  {{11314u, 14386u, 16389u, 21390u}, {19074u, 65535u, 65535u, 26028u}},
                  {{11520u, 14592u, 16521u, 21519u}, {19074u, 65535u, 65535u, 26028u}},
                  {{15661u, 17458u, 21519u, 23522u}, {51933u, 41252u, 56944u, 32768u}},
                  {{15739u, 17562u, 21561u, 23602u}, {51933u, 41252u, 56944u, 32768u}},
                  {{15816u, 17664u, 21602u, 23654u}, {51933u, 41252u, 56944u, 32768u}}},
        .r_pos = 256, .r_res = 45360, .r_drive = 16384, .r_mix = 22000, .r_track = SD_RT_HARM, .r_max = 5,
        .d_split = {15366, 20028, 25146}, .d_level = {519, 3762, 6463, 26028}, .d_tame = 22000, .d_mix = 65535,
        .s_size = 30000, .s_decay = 30000, .s_rlevel = 8000,
        .mod = {[1] = {SD_MS_LFO2, SD_MD_PITCH, 109}}, .lfo_rate = {0, 48429}},
    {   /* STARDUST: the wide pluck for arpeggios: A a sine and its octave, B a soft saw, C glassy partials (1 2 3 5:
         * a sheen, no bell), D a soft 2:1 FM; velocity brightens it (toward C), higher notes lean to D; each pluck a
         * resonant snap of LP12; a sparkle at the attack (ENV2 folds it); dotted-eighth ping-pong, a plate */
        .trim = 17, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(2, 0, 16000, C_), SD_OFF, SD_OFF, 0, -26000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 38000, C_), SD_OSC(1, 6, 26000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 34000, C_), SD_OSC(2, 0, 16000, C_), SD_OSC(3, 0, 10000, C_),
                                        SD_OSC(5, 0, 6000, C_), 0, -26000, 0, 0),
                            SD_BANK_OFS(SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 40000, C_), SD_OFF, SD_OFF, 10000, -20000, 0, 0)},
        .x = 0, .y = 65535, .warp = SD_WP_SAW, .dt = 8000, .width = 60000,
        .e1_a = 0, .e1_d = 70, .e1_s = 40, .e1_r = 70, .e2_a = 0, .e2_d = 40, .e2_s = 0, .e2_r = 30,
        .f_type = SD_FT_LP12, .f_cut = SD_FC(84), .f_res = 30000, .f_env = 18000, .f_key = 50,
        .e3_a = 0, .e3_d = 50, .e3_s = 30, .e3_r = 60,
        .s_time = SD_SPDIV(6), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 32000, .s_dtone = 26000, .s_dlevel = 16000,
        .s_size = 52000, .s_decay = 46000, .s_rtone = 34000, .s_rlevel = 14000,
        .mod = {{SD_MS_VEL, SD_MD_X, 26000}, {SD_MS_KEY, SD_MD_Y, -18000}, {SD_MS_ENV2, SD_MD_FOLD, 6000},
                {SD_MS_WHEEL, SD_MD_FCUT, 9000}, {SD_MS_AFTER, SD_MD_SDLVL, 9000}},
        .mac4 = SD_M4_AIR},
    {   /* CIRCUIT: the sci-fi blip: each note a 200 ms transformation, a 5:1 metal (B) -> a 3:1 FM (A) -> a square
         * (C) -> a sine (D); it chirps down into the note (ENV2 on PITCH); a resonant LP24 snaps; velocity the FM,
         * the wheel the resonance, aftertouch folds it; dotted sixteenths, a little room; dry enough to stay sharp */
        .trim = 15, .bank = {SD_BANK_OFS(SD_OSC(3, 0, 26000, S_), SD_OSC(1, 0, 44000, C_), SD_OFF, SD_OFF, 10000, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(5, 0, 26000, S_), SD_OSC(1, 0, 44000, C_), SD_OSC(7, 3, 8000, S_),
                                        SD_OSC(1, 0, 20000, C_), 16000, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 44000, C_), SD_OSC(3, 0, 8000, C_), SD_OFF, SD_OFF, 0, 22000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 50000, C_), SD_OFF, SD_OFF, SD_OFF, 0, -26000, 0, 0)},
        .x = 65535, .y = 65535, .xm = 22000, .warp = 20000,
        .course = 1, .cell_x = {65535, 0, 65535, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {30, 60, 110},
        .e1_a = 0, .e1_d = 56, .e1_s = 30, .e1_r = 40, .e2_a = 0, .e2_d = 44, .e2_s = 0, .e2_r = 20,
        .f_type = SD_FT_LP24, .f_cut = SD_FC(84), .f_res = 44000, .f_env = 22000, .f_key = 50,
        .e3_a = 0, .e3_d = 50, .e3_s = 30, .e3_r = 30,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_HARD, SD_DS_SOFT, SD_DS_NONE}, .d_gain = {0, 14000, 10000, 0},
        .d_alt = {0, 16000, 12000, 0}, .d_level = {32768, 26000, 22000, 14000}, .d_tame = 26000, .d_mix = 50000,
        .s_time = SD_SPDIV(3), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 24000, .s_dtone = 28000, .s_dlevel = 12000,
        .s_size = 30000, .s_decay = 28000, .s_rtone = 34000, .s_rlevel = 7000,
        .mod = {{SD_MS_ENV2, SD_MD_PITCH, 2800}, {SD_MS_VEL, SD_MD_XM, 9000}, {SD_MS_WHEEL, SD_MD_FRES, 9000},
                {SD_MS_AFTER, SD_MD_FOLD, 12000}},
        .mac4 = SD_M4_CUTOFF},
    {   /* PLUCKS: a pluck that rings a chord: a bright short excitation (A a high FM, B a saw, C a sine, D folded)
         * pinging four KP combs on the note, its fifth, octave and tenth (one stage, KEY+H): each note sounds a chord
         * that rings out; velocity the excitation's brightness and the combs' ring; aftertouch turns the chord (FREQ:
         * the combs move up a fourth, snapping to the harmonics); a twang at the attack; ping-pong, a plate */
        .trim = -22, .bank = {SD_BANK_OFS(SD_OSC(5, 0, 24000, S_), SD_OSC(1, 0, 40000, C_), SD_OFF, SD_OFF, 14000, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(1, 7, 26000, C_), SD_OFF, SD_OFF, 0, 0, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 46000, C_), SD_OFF, SD_OFF, SD_OFF, 0, -26000, 0, 0),
                            SD_BANK_OFS(SD_OSC(1, 0, 42000, C_), SD_OSC(2, 0, 12000, C_), SD_OFF, SD_OFF, 0, 0, 16000, 0)},
        .x = 0, .y = 65535, .xm = 12000, .warp = 24000,
        .e1_a = 0, .e1_d = 64, .e1_s = 20, .e1_r = 72, .e2_a = 0, .e2_d = 40, .e2_s = 0, .e2_r = 30,
        .stage = {SD_RCH(60, SD_RLV_KP, 0, 7, 12, 16)},
        .r_pan = {-20000, -7000, 7000, 20000}, .r_model = SD_RM_KP, .r_res = 54000, .r_slew = 6000,
        .r_drive = 10000, .r_mix = 40000, .r_track = SD_RT_KEY | SD_RT_HARM | 1u << 4, .r_low = 24000,
        .s_time = SD_SPDIV(6), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 26000, .s_dtone = 28000, .s_dlevel = 12000,
        .s_size = 48000, .s_decay = 42000, .s_rtone = 34000, .s_rlevel = 12000,
        .mod = {{SD_MS_VEL, SD_MD_X, 26000}, {SD_MS_VEL, SD_MD_RRES, 6000}, {SD_MS_AFTER, SD_MD_RFREQ, 3400},
                {SD_MS_ENV2, SD_MD_PITCH, 500}, {SD_MS_WHEEL, SD_MD_RMIX, 9000}},
        .mac4 = SD_M4_AIR},
    {   /* PULSAR: a spectral sequencer: four banks as far apart as they go (A glass, B an odd
         * cluster on harmonics 5 7 9 11, C a chain toward the triangle (its offset counts x 2 on the Q page), D the
         * organ's 1 and 3), X stepping on 16ths and Y on eighths (S&H), so each step lands in another part of the
         * field; inside each note a little fold falls away; only SOFT on the top */
        .trim = 7, .bank = {BK_GLASS, SD_BANK(SD_OSC(5, 0, 26000, C_), SD_OSC(7, 0, 20000, C_), SD_OSC(9, 0, 14000, C_),
                                   SD_OSC(11, 0, 10000, C_)),
                 SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 30000, S_),
                             SD_OSC(1, 0, 60000, C_), 0, 8000, 0, 0), BK_ORGAN},
        .x = 32768, .y = 32768, .xm = 22000, .width = 44000,
        .e1_a = 2, .e1_d = 80, .e1_s = 80, .e1_r = 55, .e2_a = 0, .e2_d = 70, .e2_s = 40, .e2_r = 55,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_CMP,
        .d_type = {SD_DS_NONE, SD_DS_NONE, SD_DS_SOFT, SD_DS_SOFT}, .d_gain = {0, 0, 12000, 10000},
        .d_alt = {0, 0, 16000, 12000}, .d_level = {32768, 32768, 24000, 18000}, .d_mix = 40000,
        .s_time = SD_SPDIV(5), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 24000, .s_dtone = 30000, .s_dlevel = 12000,
        .s_size = 52000, .s_decay = 42000, .s_rtone = 36000, .s_rlevel = 12000,
        .mod = {{SD_MS_LFO1, SD_MD_X, 15887}, {SD_MS_LFO2, SD_MD_Y, 15887}, {SD_MS_ENV2, SD_MD_FOLD, 3972},
                /* the engines talking: each segment its own fold */
                {SD_MS_SEG, SD_MD_FOLD, 4634}},
        .lfo_shape = {SD_LS_SH, SD_LS_SH}, .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC}, .lfo_rate = {100, 5561},
        .mac4 = SD_M4_AIR},
    {   /* BRAAM (cue 2's hits, the low one): DISTROY whose destroyed band falls through the spectrum in a hit: LFO2 a
         * SAW once a note (SYNC KEY ONE, half a bar) on D SPLIT and XM; its top tamed: hard, not screaming */
        .trim = -3, .bank = {BK_GROWL,
                 BK_BELL_LIGHT,
                 BK_EPIANO_LIGHT,
                 BK_GROWL},
        .xm = 50000, .warp = 50000, .fold = 30000, .dt = 20000, .e1_a = 5, .e1_d = 70, .e1_s = 100, .e1_r = 40,
        .e2_a = 0, .e2_d = 60, .e2_s = 80, .e2_r = 40,
        .x = 32768, .y = 32768, .width = 30000,
        .stage = {{{19908u, 21684u, 25256u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{18141u, 22633u, 23628u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{12288u, 14080u, 15360u, 17152u}, {10000u, 10000u, 10000u, 10000u}},
                  {{15967u, 20685u, 24877u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{12288u, 13056u, 14848u, 16128u}, {10000u, 10000u, 10000u, 10000u}},
                  {{15500u, 24975u, 26186u, 26855u}, {47800u, 43700u, 37500u, 27900u}}},
        .r_pan = {-24000, 24000, -8000, 8000}, .r_res = 36000, .r_slew = 24000, .r_drive = 9000, .r_mix = 26000,
        .r_model = SD_RM_BP12, .r_track = SD_RT_HARM, .r_div = 2, .r_cmode = SD_RC_RND, .r_max = 5, .r_low = 20000,
        .d_split = {14170, 22111, 28436}, .d_type = {SD_DS_SOFT, SD_DS_CRUSH, SD_DS_FOLD, SD_DS_HARD},
        .d_gain = {5000, 14000, 22000, 24000}, .d_alt = {20000, 34000, 34000, 24000},
        .d_level = {26000, 15500, 15500, 10000}, .d_bias = {4000, 0, -6000, 0}, .d_pan = {0, -12000, 12000, 0},
        .d_shape = {-16384, 16384, 0, 26214}, .d_body = {46421, 24576, 0, 19114}, .d_fb = 14000, .d_ftime = 16000,
        .d_spread = 22000, .d_tame = 24000, .d_mix = 36000,
        .s_time = 11022, .s_fb = 40000, .s_dtone = 30000, .s_dlevel = 18000, .s_size = 65535, .s_decay = 50000,
        .s_rtone = 36000, .s_rlevel = 16000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {{SD_MS_LFO1, SD_MD_RPOS, 8274}, {SD_MS_LFO1, SD_MD_WARP, 4965}, {SD_MS_LFO2, SD_MD_DHIGH, 9929},
                {SD_MS_WHEEL, SD_MD_DFB, 13239}, {SD_MS_DLOW, SD_MD_RPOS, 6620}, {SD_MS_STEP, SD_MD_DSPLIT, 6620},
                {SD_MS_LFO2, SD_MD_DSPLIT, 9830}, {SD_MS_LFO2, SD_MD_XM, 8192}},
        .lfo_shape = {SD_LS_SH, SD_LS_SAW}, .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC | SD_LM_KEY | SD_LM_ONE},
        .lfo_rate = {100, 30000}},
    {   /* COLOSSUS: the wall of low brass. A a 1:1 brass FM whose index swells (ENV2 over 1.5 s), B four saws wide,
         * C a folded growl an octave under, D a 3:1 metal brass. The Reactor three vowels OO -> AW -> A, opened by
         * the same swell (a "waah"), the lows past it; DISTROY's low mids HARD and PHAT, the mids SOFT, the top
         * tamed; the plate large. Velocity drives the mids, the wheel the loop's roar, aftertouch opens the vowel
         * and moves toward B; SUB under. In MONO the note travels A -> B over three seconds; in POLY each voice A */
        .trim = -16, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 30000, S_), SD_OSC(1, 0, 44000, C_), SD_OSC(1, 6, 22000, C_),
                                         SD_OSC(2, -5, 12000, C_), 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 30000, C_), SD_OSC(1, 9, 28000, C_), SD_OSC(1, -11, 28000, C_),
                                         SD_OSC(2, 5, 16000, C_), 0, 14000, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 24000, S_), SD_OSC(1, 0, 40000, C_), SD_OSC(1, 4, 24000, C_),
                                         SD_OFF, 6000, 0, 12000, 0),
                             SD_BANK_OFS(SD_OSC(3, 0, 18000, S_), SD_OSC(1, 0, 42000, C_), SD_OSC(2, 3, 18000, C_),
                                         SD_OFF, 4000, 0, 0, 0)},
        .x = 0, .y = 65535, .xm = 22000, .warp = 14000, .fold = 3000, .dt = 14000, .width = 46000,
        .course = 1, .cell_x = {0, 0, 65535, 65535}, .cell_y = {65535, 65535, 65535, 65535}, .cell_ms = {400, 2600, 4000},
        .e1_a = 52, .e1_d = 100, .e1_s = 120, .e1_r = 92, .e2_a = 100, .e2_d = 110, .e2_s = 96, .e2_r = 90,
        .stage = {SD_RST_VBP(SD_RV_OO), SD_RST_VBP(SD_RV_AW), SD_RST_VBP(SD_RV_A)},
        .r_pan = {-10000, 10000, -4000, 4000}, .r_model = SD_RM_VBP, .r_res = 24000, .r_slew = 30000,
        .r_drive = 14000, .r_mix = 34000, .r_track = 3u << 4, .r_low = 26000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_SOFT, SD_DS_HARD, SD_DS_SOFT, SD_DS_NONE}, .d_gain = {4000, 20000, 12000, 0},
        .d_alt = {8000, 24000, 14000, 0}, .d_level = {32000, 30000, 22000, 7000}, .d_shape = {-12000, -16000, 0, 0},
        .d_body = {44000, 40000, 0, 0}, .d_fb = 6000, .d_ftime = 20000, .d_spread = 16000, .d_tame = 30000,
        .d_mix = 56000,
        .s_size = 64000, .s_decay = 52000, .s_rtone = 28000, .s_rlevel = 20000,
        .f_type = SD_FT_LP12, .f_cut = SD_FC(100), .f_key = 50,
        .mod = {{SD_MS_ENV2, SD_MD_RPOS, 16000}, {SD_MS_VEL, SD_MD_DGAIN, 8000}, {SD_MS_WHEEL, SD_MD_DFB, 18000},
                {SD_MS_AFTER, SD_MD_RPOS, 10000}, {SD_MS_AFTER, SD_MD_X, 20000}, {SD_MS_LFO1, SD_MD_DT, 4000},
                {SD_MS_ENV2, SD_MD_FCUT, 6000}},
        .lfo_shape = {SD_LS_SIN}, .lfo_rate = {17373 /* 0.15 Hz */},
        .mac4 = SD_M4_BODY, .sub_lvl = 20000, .sub_src = SD_SUB_A},
    {   /* RISER: one held note becomes the build: over four bars (a RAMP once a note, on the tempo) its pitch climbs
         * an octave, the fold and the mids' drive with it, the Reactor's peaks sweep up and a pulse on the level
         * steps faster on the beat, half notes to 16ths (8 6 4 3 2 1); the FM grows (ENV2); COURSE goes A (a sine and
         * its octave) -> B (saws) -> C (high partials) -> D (bright 1:1); the echoes ping-pong in dotted eighths into a large plate. The wheel feeds the loop, aftertouch
         * drives the Reactor. Let go: the tail falls into the room */
        .trim = -11, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(2, 0, 20000, C_), SD_OSC(1, 7, 18000, C_),
                                         SD_OFF, 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 28000, C_), SD_OSC(1, 12, 26000, C_), SD_OSC(1, -12, 26000, C_),
                                         SD_OSC(2, 0, 14000, C_), 0, 16000, 0, 0),
                             SD_BANK_OFS(SD_OSC(7, 0, 12000, S_), SD_OSC(1, 0, 36000, C_), SD_OSC(4, 5, 14000, C_),
                                         SD_OSC(6, -4, 9000, C_), 6000, 0, 6000, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 30000, S_), SD_OSC(1, 0, 40000, C_), SD_OSC(1, 8, 20000, C_),
                                         SD_OFF, 14000, 0, 0, 0)},
        .x = 0, .y = 65535, .xm = 14000, .warp = 12000, .dt = 12000, .width = 52000,
        .course = 1, .cell_x = {0, 65535, 65535, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {2600, 3000, 2600},
        .e1_a = 96, .e1_d = 100, .e1_s = 127, .e1_r = 104, .e2_a = 110, .e2_d = 100, .e2_s = 127, .e2_r = 100,
        .stage = {{{14000u, 15000u, 16000u, 17000u}, {9000u, 9000u, 9000u, 9000u}}},
        .r_pan = {-8000, 8000, -3000, 3000}, .r_model = SD_RM_BP12, .r_res = 40000, .r_slew = 20000,
        .r_drive = 12000, .r_mix = 30000, .r_track = 1u << 4, .r_low = 24000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_NONE}, .d_gain = {0, 10000, 14000, 0},
        .d_alt = {0, 12000, 18000, 0}, .d_level = {32768, 30000, 22000, 8000}, .d_ftime = 8000,
        .d_spread = 20000, .d_tame = 28000, .d_mix = 48000,
        .s_time = SD_SPDIV(7), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 30000, .s_dtone = 24000, .s_dlevel = 12000,
        .s_size = 64000, .s_decay = 54000, .s_rtone = 28000, .s_rlevel = 22000,
        .mod = {{SD_MS_LFO2, SD_MD_PITCH, 16384}, {SD_MS_LFO2, SD_MD_RFREQ, 20000}, {SD_MS_LFO2, SD_MD_L1RATE, -13653},
                {SD_MS_LFO1, SD_MD_VOL, -22000}, {SD_MS_LFO2, SD_MD_DGAIN, 8000}, {SD_MS_LFO2, SD_MD_FOLD, 10000},
                {SD_MS_WHEEL, SD_MD_DFB, 14000}, {SD_MS_AFTER, SD_MD_RDRIVE, 14000}},
        .lfo_shape = {SD_LS_SQR, SD_LS_RAMP}, .lfo_mode = {SD_LM_SYNC | SD_LM_KEY, SD_LM_SYNC | SD_LM_KEY | SD_LM_ONE},
        .lfo_rate = {SD_LDIV(5), SD_LDIV(11)},
        .mac4 = SD_M4_DROWN, .sub_lvl = 10000, .sub_src = SD_SUB_A},
    {   /* DRONE: one note for minutes. COURSE round on its own (LOOP), half a minute a turn: A a soft stack, B a low
         * folded 1:1, C hollow odd harmonics, D a distant 5:1 metal; KP combs on the octave and the fifth drifting
         * (an LFO under a tenth of a hertz), the other drifting the fold;
         * the lows PHAT and clean, the top dark; the plate at its largest and darkest. The wheel opens it toward C,
         * aftertouch folds and drives it; the echoes fold the tail; SUB deep under */
        .trim = -3, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 40000, C_), SD_OSC(2, 3, 20000, C_), SD_OSC(3, -4, 12000, C_),
                                         SD_OSC(4, 6, 6000, C_), 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(1, 0, 42000, C_), SD_OSC(1, 5, 22000, C_),
                                         SD_OFF, 6000, 0, 10000, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 36000, C_), SD_OSC(3, 0, 14000, C_), SD_OSC(5, 0, 9000, C_),
                                         SD_OSC(7, 0, 6000, C_), 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(5, 3, 10000, S_), SD_OSC(1, 0, 38000, C_), SD_OSC(2, -6, 16000, C_),
                                         SD_OFF, 4000, 0, 0, 0)},
        .x = 0, .y = 65535, .xm = 12000, .warp = 8000, .fold = 2000, .dt = 10000, .width = 50000,
        .course = 2, .cell_x = {0, 65535, 65535, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {9000, 11000, 10000},
        .e1_a = 100, .e1_d = 100, .e1_s = 127, .e1_r = 110, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RCH(36, 18000u, 0, 7, 12, 19)},
        .r_pan = {-22000, 22000, -10000, 10000}, .r_model = SD_RM_KP, .r_res = 44000, .r_slew = 30000,
        .r_drive = 9000, .r_mix = 24000, .r_track = SD_RT_KEY | SD_RT_HARM | 1u << 4, .r_low = 24000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_SOFT, SD_DS_NONE, SD_DS_SOFT, SD_DS_NONE}, .d_gain = {3000, 0, 10000, 0},
        .d_alt = {6000, 10000, 12000, 0}, .d_level = {32768, 28000, 20000, 6000}, .d_shape = {-14000, 0, 0, 0},
        .d_body = {44000, 0, 0, 0}, .d_tame = 24000,
        .d_mix = 44000,
        .s_time = SD_SPDIV(11), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 26000, .s_dtone = 18000, .s_dlevel = 9000,
        .s_size = 65535, .s_decay = 60000, .s_rtone = 22000, .s_rlevel = 26000,
        .mod = {{SD_MS_LFO1, SD_MD_RFREQ, 3000}, {SD_MS_LFO2, SD_MD_FOLD, 3000}, {SD_MS_WHEEL, SD_MD_X, 26000},
                {SD_MS_AFTER, SD_MD_FOLD, 12000}, {SD_MS_AFTER, SD_MD_DGAIN, 9000}, {SD_MS_ECHO, SD_MD_FOLD, 4000},
                {SD_MS_WHEEL, SD_MD_RRES, 8000}},
        .lfo_shape = {SD_LS_TRI, SD_LS_SIN}, .lfo_rate = {7900 /* 0.05 Hz */, 11953 /* 0.08 Hz */},
        .mac4 = SD_M4_DROWN, .sub_lvl = 26000, .sub_src = SD_SUB_A},
    {   /* TICKER: the clock under a scene: a short, dry, woody-metal tick (a 7:1 and an 11:1 FM falling inside ~30
         * ms) through one narrow resonance an octave over the note; dotted-eighth echoes, ping-pong, low. Velocity
         * brightens the tick and rounds it (toward C, a 1:1 pulse); the wheel lets the echoes build, aftertouch
         * rings the resonance longer. Played as 16ths with accents. Corners: A the tick, B wood, C a pulse, D a
         * crack */
        .trim = 11, .bank = {SD_BANK_OFS(SD_OSC(7, 0, 26000, S_), SD_OSC(1, 0, 44000, C_), SD_OSC(11, 3, 9000, S_),
                                         SD_OSC(2, 0, 20000, C_), 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(3, 0, 22000, S_), SD_OSC(2, 0, 40000, C_), SD_OFF, SD_OFF,
                                         0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 52000, C_), SD_OSC(2, 0, 10000, S_), SD_OSC(1, 0, 20000, C_),
                                         SD_OFF, 0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(13, 5, 18000, S_), SD_OSC(1, 0, 40000, C_), SD_OFF, SD_OFF,
                                         4000, 0, 6000, 0)},
        .x = 0, .y = 65535, .xm = 20000, .width = 30000,
        .e1_a = 0, .e1_d = 54, .e1_s = 0, .e1_r = 44, .e2_a = 0, .e2_d = 36, .e2_s = 0, .e2_r = 30,
        .stage = {{{72u * 256u, 0u, 0u, 0u}, {22000u, 0u, 0u, 0u}}},   /* (an octave over C4: KEY moves it with the note) */
        .r_pan = {-6000, 6000, 0, 0}, .r_model = SD_RM_BP12, .r_res = 44000, .r_slew = 0,
        .r_drive = 12000, .r_mix = 26000, .r_track = SD_RT_KEY | 1u << 4, .r_low = 26000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_SOFT, SD_DS_HARD, SD_DS_SOFT, SD_DS_NONE}, .d_gain = {6000, 14000, 8000, 0},
        .d_alt = {10000, 16000, 10000, 0}, .d_level = {32768, 30000, 24000, 10000}, .d_tame = 26000, .d_mix = 52000,
        .s_time = SD_SPDIV(7), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 24000, .s_dtone = 26000, .s_dlevel = 14000,
        .s_size = 30000, .s_decay = 30000, .s_rtone = 30000, .s_rlevel = 8000,
        .mod = {{SD_MS_VEL, SD_MD_XM, 14000}, {SD_MS_VEL, SD_MD_Y, -22000}, {SD_MS_WHEEL, SD_MD_SFB, 20000},
                {SD_MS_AFTER, SD_MD_RRES, 12000}, {SD_MS_VEL, SD_MD_E1D, 4000}, {SD_MS_VEL, SD_MD_X, 22000}},
        .mac4 = SD_M4_AIR},
    {   /* WARP: the jump. Each note falls from two octaves up into itself over a second and a half (ENV2) while
         * DISTROY's loop sweeps (its TIME with it: a jet's comb) and the Reactor's allpasses (AP1) phase the other way; a
         * saw once a note drives the nav A -> C (a sine opening into metal); the echoes ping-pong in eighths into a
         * large plate. The wheel feeds the loop, aftertouch the echoes. Short notes zap, long ones land */
        .trim = 6, .bank = {SD_BANK_OFS(SD_OSC(1, 0, 46000, C_), SD_OSC(2, 0, 14000, C_), SD_OFF, SD_OFF,
                                         0, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(1, 0, 26000, S_), SD_OSC(1, 0, 40000, C_), SD_OSC(1, 9, 22000, C_),
                                         SD_OFF, 8000, 0, 0, 0),
                             SD_BANK_OFS(SD_OSC(5, 0, 16000, S_), SD_OSC(1, 0, 38000, C_), SD_OSC(3, 4, 12000, C_),
                                         SD_OFF, 10000, 0, 10000, 0),
                             SD_BANK_OFS(SD_OSC(2, 0, 22000, S_), SD_OSC(1, 0, 42000, C_), SD_OFF, SD_OFF,
                                         6000, 0, 0, 0)},
        .x = 0, .y = 65535, .xm = 16000, .warp = 10000, .dt = 10000, .width = 50000,
        .e1_a = 0, .e1_d = 104, .e1_s = 100, .e1_r = 96, .e2_a = 0, .e2_d = 100, .e2_s = 0, .e2_r = 80,
        .stage = {{{14000u, 15500u, 17000u, 18500u}, {30000u, 30000u, 30000u, 30000u}}},
        .r_pan = {-6000, 6000, -3000, 3000}, .r_model = SD_RM_AP1, .r_res = 30000, .r_slew = 0,
        .r_drive = 12000, .r_mix = 30000, .r_track = 1u << 4, .r_low = 24000,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_NONE, SD_DS_HARD, SD_DS_NONE}, .d_gain = {0, 0, 14000, 0},
        .d_alt = {0, 0, 16000, 0}, .d_level = {32768, 30000, 22000, 8000}, .d_fb = 22000, .d_ftime = 6000,
        .d_spread = 20000, .d_tame = 28000, .d_mix = 50000,
        .s_time = SD_SPDIV(5), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 26000, .s_dtone = 24000, .s_dlevel = 11000,
        .s_size = 60000, .s_decay = 50000, .s_rtone = 28000, .s_rlevel = 18000,
        .mod = {{SD_MS_ENV2, SD_MD_PITCH, 32767}, {SD_MS_ENV2, SD_MD_DTIME, -20000}, {SD_MS_ENV2, SD_MD_RFREQ, 22000},
                {SD_MS_LFO2, SD_MD_X, 30000}, {SD_MS_LFO2, SD_MD_Y, -30000}, {SD_MS_WHEEL, SD_MD_DFB, 16000},
                {SD_MS_AFTER, SD_MD_SFB, 18000}, {SD_MS_ENV2, SD_MD_FOLD, 8000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_RAMP}, .lfo_mode = {0, SD_LM_SYNC | SD_LM_KEY | SD_LM_ONE},
        .lfo_rate = {19853 /* 0.2 Hz */, SD_LDIV(7)},
        .mac4 = SD_M4_AIR},
    {   /* SKURK (2026-11-27, the user's own: played on their FM-1 and read over USB as the knobs left it; SAVE there
         * names a place after the patch it came from, so its name is the factory's). A an FM stack (6 8 9 12) over
         * three plain banks (C toward the triangle, D toward the pulse); COURSE round and round at the top of Y; LP24
         * with RES, ENV3 opening it; the Reactor's KP combs driven hard and keyed; DISTROY LAST, after SPACE (the
         * echoes, the plate and the looper destroyed too): LR4, LOW and TOP crushed, LO-MID soft, HI-MID hard but
         * nearly out, TAME; ping-pong echoes on the tempo with SHIMMER and LO CUT into a long plate; MUTATE and BREAK
         * up. LFO 2 on WARP, ENV2 on the pitch, velocity on X, aftertouch on Y (down), the wheel on CUTOFF. Its
         * levels already the make-ups' (lv 5: as it played) */
        .trim = -5, .lv = 5,
        .bank[0].osc[0].harm = 6, .bank[0].osc[0].role = 1, .bank[0].osc[0].level = 40000, .bank[0].osc[1].harm = 8,
        .bank[0].osc[1].level = 28842, .bank[0].osc[2].harm = 9, .bank[0].osc[2].role = 1, .bank[0].osc[2].level = 19661,
        .bank[0].osc[3].harm = 12, .bank[0].osc[3].level = 28842,
        .bank[1].osc[0].harm = 2, .bank[1].osc[0].level = 34000, .bank[1].osc[1].harm = 1, .bank[1].osc[1].level = 16000,
        .bank[1].osc[2].harm = 1, .bank[1].osc[3].harm = 1,
        .bank[2].osc[0].harm = 1, .bank[2].osc[0].level = 40000, .bank[2].osc[1].harm = 2, .bank[2].osc[1].level = 10000,
        .bank[2].osc[2].harm = 1, .bank[2].osc[3].harm = 1, .bank[2].wp = -9000,
        .bank[3].osc[0].harm = 1, .bank[3].osc[0].level = 40000, .bank[3].osc[1].harm = 1, .bank[3].osc[2].harm = 1,
        .bank[3].osc[3].harm = 1, .bank[3].wp = 14000,
        .xm = 46999, .warp = 52957, .fold = 27802, .dt = 60239, .y = 65535, .width = 20000, .course = 2,
        .cell_x = {35746}, .cell_y = {45013}, .cell_ms = {539, 163, 79},
        .e1_d = 70, .e1_s = 100, .e1_r = 10, .e2_d = 50, .e2_r = 30, .e3_d = 54, .e3_s = 45, .e3_r = 30,
        .f_type = SD_FT_LP24, .f_cut = 40387, .f_res = 31774, .f_env = 21182, .f_key = 40,
        .r_model = SD_RM_KP, .r_res = 41042, .r_slew = 43028, .r_drive = 64211, .r_mix = 46337, .r_freq = 256,
        .r_track = 1, .r_max = 5, .r_pan = {-24000, 24000, -8000, 8000},
        .stage = {{{19908u, 21684u, 25256u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{18489u, 24005u, 25328u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{15500u, 24975u, 26186u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{18811u, 20530u, 25201u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{15967u, 20685u, 24877u, 26855u}, {47800u, 43700u, 37500u, 27900u}},
                  {{18141u, 22633u, 23628u, 26855u}, {47800u, 43700u, 37500u, 27900u}}},
        .d_xover = SD_DX_LR4 | SD_DX_LAST, .d_split = {14170, 20255, 27668}, .d_mix = 52000,
        .d_type = {SD_DS_CRUSH, SD_DS_SOFT, SD_DS_HARD, SD_DS_CRUSH}, .d_gain = {13901, 0, 5295, 14563},
        .d_alt = {38394, 15887, 14000, 0}, .d_level = {30450, 19859, 2647, 47661}, .d_bias = {0, 0, -11790, 0},
        .d_pan = {0, 0, -689, 0}, .d_shape = {0, 0, 993, 0}, .d_body = {0, 0, 33098, 0}, .d_tame = 28000,
        .s_time = 54713, .s_mode = SD_SP_SYNC | SD_SP_PP | 41u << 2, .s_fb = 31774, .s_dtone = 35084, .s_dlevel = 12000,
        .s_dlow = 80, .s_size = 52295, .s_decay = 58253, .s_rtone = 49647, .s_rlevel = 22506,
        .mod = {{SD_MS_LFO2, SD_MD_WARP, 5000}, {SD_MS_ENV2, SD_MD_PITCH, 1400}, {SD_MS_VEL, SD_MD_X, 24000},
                {SD_MS_AFTER, SD_MD_Y, -24000}, {SD_MS_WHEEL, SD_MD_FCUT, 9000}},
        .lfo_shape = {SD_LS_SIN, SD_LS_TRI}, .lfo_rate = {0, 23349}, .mac = {50971, 0, 26478, 0}}
};
#ifndef SD_PATCH_WORST                           /* (a test build: -DSD_PATCH_WORST=SD_P_NOVA measures another) */
#define SD_PATCH_WORST SD_P_DISTROY                       /* the boot patch: fm1-emu measures the worst case */
#endif

/* The algorithms the ALGORITHM encoder steps through: roles of oscillators 1..4, left to right. PPCC: 1 and 2
 * in parallel on 3, 4 unmodulated; SCSC: two chains of two. */
#define SD_ALGOS 8u
static const char *const SD_ALGO[SD_ALGOS] = {"SSSC", "PSSC", "PPPC", "SCSC", "PPCC", "CSSC", "SCCC", "CCCC"};

static uint8_t sd_role_of(char c) { return (uint8_t)(c == 'S' ? SD_ROLE_S : c == 'P' ? SD_ROLE_P : SD_ROLE_C); }

#undef C_
#undef S_
#undef P_
#endif
