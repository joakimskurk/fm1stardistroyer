/* SPDX-License-Identifier: GPL-3.0-only */
/* The patches (Sound Plan §7, V3): four banks of four oscillators, each a harmonic 1..32 of
 * the note with a fine tune, a level and a role, left to right; each bank's offsets on the Q page; the Q page's XM WP
 * FD DT for all banks; ENV1 on the level, ENV2 on the FM amount; the vector X/Y, VEC2's cells, the stereo width.
 * Included by sd_engine.h after the patch's types.
 *
 * A one-bank patch (GROWL, AURORA, ...) has its bank in all four and the vector at corner A: in DUO and POLY every
 * voice plays the same bank. */
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
/* the Rider: a stage from a vowel (sd_tables.h, its four formants) at levels falling from F1, or a chord on a root */
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
 * without fold: two of four banks in the costliest kernel instead of four (the V3 fallback, the user 2026-11-05) */
#define BK_BELL_LIGHT SD_BANK_OFS(SD_OSC(7, 0, 34000, S_), SD_OSC(2, 0, 30000, C_), SD_OSC(5, 4, 26000, S_), \
                                  SD_OSC(1, 0, 40000, C_), 0, -17000, -15000, 0)
#define BK_EPIANO_LIGHT SD_BANK_OFS(SD_OSC(14, 0, 9000, S_), SD_OSC(1, 0, 30000, C_), SD_OSC(1, 0, 30000, S_), \
                                    SD_OSC(1, 3, 36000, C_), 0, -17000, -15000, 0)
#define BK_ORGAN SD_BANK(SD_OSC(1, 0, 40000, C_), SD_OSC(3, 0, 30800, C_), SD_OFF, SD_OFF)

/* SPACE's TIME as a synced division: SD_SP_DIV's index (1/32 1/16T 1/16 1/8T 1/16D 1/8 1/4T 1/8D 1/4 1/2T 1/4D 1/2) */
#define SD_SPDIV(i) ((uint16_t)((i) * 65536u / 12u + 100u))
#define SD_SPMS(ms) ((uint16_t)(((ms) - 1u) * 65535u / 999u + 1u))

/* GLASS, BELL, EPIANO and ORGAN are gone (the user, 2026-11-08: they still sounded like classic FM, edge or not);
 * their banks live on inside TOUR, DISTROY and the cue's patches. The code names a patch by SD_P_*, never by number:
 * tests/sd_test.c checks each against SD_PATCH_NAME */
enum {
    SD_P_GROWL, SD_P_TOUR, SD_P_DISTROY, SD_P_VOWEL, SD_P_CHORDS, SD_P_PLUCKS, SD_P_FLANGE, SD_P_PHASER, SD_P_RIDE,
    SD_P_SPLIT, SD_P_COMET, SD_P_NEBULA, SD_P_PULSAR, SD_P_ION, SD_P_ORBITAL, SD_P_MONOLITH, SD_P_STRINGS,
    SD_P_HALO, SD_P_AURORA, SD_P_TITAN, SD_P_DRIVE, SD_P_UNDERTOW, SD_P_IMPACT, SD_P_BRAAM, SD_P_SCREAM, SD_P_BEACON,
    SD_P_DEPTH, SD_P_SHARDS, SD_P_PRISM, SD_P_COUNT
};
#define SD_PATCHES 29u
static const char *const SD_PATCH_NAME[SD_PATCHES] = {"GROWL", "TOUR", "DISTROY", "VOWEL", "CHORDS", "PLUCKS", "FLANGE",
                                                      "PHASER", "RIDE", "SPLIT", "COMET", "NEBULA", "PULSAR", "ION",
                                                      "ORBITAL", "MONOLITH", "STRINGS",
                                                      /* cue 2's (tools/sd_epic.c) */
                                                      "HALO", "AURORA", "TITAN", "DRIVE", "UNDERTOW", "IMPACT", "BRAAM",
                                                      "SCREAM", "BEACON", "DEPTH", "SHARDS", "PRISM"};
/* DESTROY's splits as MIDI x 256: the pitch of hz, rounded (sd_tables.h's macros are for vowels) */
#define SD_DHZ_200 14170u                       /* MIDI 55.35 */
#define SD_DHZ_1200 22111u                      /* MIDI 86.37 */
#define SD_DHZ_5000 28436u                      /* MIDI 111.08 */
#define SD_DHZ_2500 25362u                      /* MIDI 99.07 */
/* The Star Distroyer edge (the user, 2026-11-08: a plain FM patch must sound as it should, with an edge that makes it
 * Star Distroyer's; GROWL's and AURORA's): under the top split DESTROY passes the sound as it was (NONE
 * through LR4, so the top's shaper sees the top alone: CMP's gentler slopes let the body in, +5 .. +7 dB at 1.2 ..
 * 5 kHz); the top band alone is destroyed (its shaper, gain, ALT and level); and something steps on the beat (S&H, in
 * the patch). LR4's bands sum to an allpass, flat in level but a few dB higher in peaks: BODY, the level under the
 * top (32768 = 1x), gives a hot patch its headroom back */
#define SD_EDGE(split3, type, gain, alt, level, body) .d_split = {SD_DHZ_200, SD_DHZ_1200, split3}, \
    .d_xover = SD_DX_LR4, .d_type = {SD_DS_NONE, SD_DS_NONE, SD_DS_NONE, type}, .d_gain = {0, 0, 0, gain}, \
    .d_alt = {0, 0, 0, alt}, .d_level = {body, body, body, level}, .d_mix = 65535
/* the AP modes' stages: four pitches close together, high up (where the allpasses work best), at a level for all */
#define SD_RAP(a, b, c, d) {{a * 256u, b * 256u, c * 256u, d * 256u}, {32768u, 32768u, 32768u, 32768u}}
static const SdPatch SD_PATCH[SD_PATCHES] = {
    {   /* GROWL: a chain 1 -> 2 -> 3 -> 4 between triangle and saw, folded a little, low. (Past the saw toward pulse
         * with more fold and FM it was, the user: "so noisy it is nearly unusable": the chain's aliasing, -14 dB of it
         * off the harmonics on a held D2; now -24) */
        .bank = SD_SAME(BK_GROWL), SD_CORNER_A, .xm = 24000, .warp = 20000, .fold = 8000, .e2_fold = 6000,
        .e1_a = 5, .e1_d = 70, .e1_s = 80, .e1_r = 40, .e2_a = 0, .e2_d = 48, .e2_s = 40, .e2_r = 40,
        /* the edge: the bottom clean, the top destroyed in time: over 5 kHz folded, its drive a new step each eighth
         * with the voice's fold (no SHARP: that fold spits already) */
        SD_EDGE(SD_DHZ_5000, SD_DS_FOLD, 16000, 36000, 10000, 31000u),
        /* the fold stepping on eighths (S&H: a rhythm, not a wobble), the top's gain with it; harder notes growl more */
        .mod = {{SD_MS_LFO1, SD_MD_FOLD, 6620}, {SD_MS_VEL, SD_MD_XM, 6620}, {SD_MS_LFO1, SD_MD_DHIGH, 2600}},
        .lfo_shape = {SD_LS_SH}, .lfo_mode = {SD_LM_SYNC}, .lfo_rate = {5561}, .mac4 = SD_M4_BODY},
    {   /* TOUR (§7 VECTOR TOUR): clean additive (A) -> FM bell (B) -> folded growl (C) -> organ (D), VEC2 from the gate
         * around the field, the banks spread in stereo. C warps and folds by its own offsets, as a mod slot would */
        .bank = {BK_GLASS, BK_BELL, SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 30000, S_),
                                                SD_OSC(1, 0, 60000, C_), 0, 22000, 12000, 0), BK_ORGAN},
        .xm = 40000, .width = 40000, SD_CORNER_A,
        .vec = 1, .cell_x = {0, 65535, 65535, 0}, .cell_y = {65535, 65535, 0, 0}, .cell_ms = {1500, 1500, 1500},
        .e1_a = 10, .e1_d = 90, .e1_s = 100, .e1_r = 60, .e2_a = 0, .e2_d = 70, .e2_s = 60, .e2_r = 60,
        .s_time = SD_SPDIV(8), .s_mode = SD_SP_SYNC, .s_fb = 22000, .s_dtone = 36000, .s_dlevel = 14000,
        .s_size = 65535, .s_decay = 48000, .s_rtone = 40000, .s_rlevel = 18000,
        /* the vector stepping round the field: X a new place each quarter, Y each half (S&H, glided ~10 ms). (Two slow
         * free LFOs drifted it as a random vowel, the user: stepped random is "much better") */
        .mod = {{SD_MS_LFO1, SD_MD_X, 9929}, {SD_MS_LFO2, SD_MD_Y, 9929},
                /* the engines talking: Y walks the loop, the echoes bend the warp */
                {SD_MS_STEP, SD_MD_Y, 8274}, {SD_MS_ECHO, SD_MD_WARP, 3310}}, .lfo_shape = {SD_LS_SH, SD_LS_SH},
        .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC}, .lfo_rate = {16484, 27406}, .mac4 = SD_M4_DROWN},
    {   /* DISTROY (§7 STAR DISTROYER, the voice's part): four FM chains at the field's centre, the outer two (GROWL)
         * folded and squashed in the costliest kernel, the middle two (BELL, EPIANO) plain since C1: the CPU's worst
         * case all the same, with the whole chain after it */
        .bank = {BK_GROWL, BK_BELL_LIGHT, BK_EPIANO_LIGHT, BK_GROWL}, .x = 32768, .y = 32768, .width = 30000,
        .xm = 50000, .warp = 50000, .fold = 30000, .dt = 20000,
        .e1_a = 5, .e1_d = 70, .e1_s = 100, .e1_r = 40, .e2_a = 0, .e2_d = 60, .e2_s = 80, .e2_r = 40,
        /* and the Rider in its costliest model, BP12, half wet, its stages at random */
        .stage = {SD_RST_VOWEL(SD_RV_A), SD_RST_VOWEL(SD_RV_ER), SD_RCH(48, 10000u, 0, 7, 12, 19), SD_RST_VOWEL(SD_RV_OO),
                  SD_RCH(48, 10000u, 0, 3, 10, 15), SD_RST_VOWEL(SD_RV_EE)},
        .r_pan = {-24000, 24000, -8000, 8000}, .r_model = SD_RM_BP12, .r_res = 36000, .r_slew = 24000,
        .r_drive = 9000, .r_mix = 26000, .r_track = SD_RT_HARM, .r_low = 20000, .r_div = 2, .r_cmode = SD_RC_RND,
        .r_max = 5,
        /* and DESTROY, CMP (the user's choice at S1: LR4 here with SPACE reads 52 %), a shaper in every band */
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
        /* the Rider and the warp sampled and held a sixteenth; the top band's gain a new step each quarter; the
         * wheel feeds the loop */
        .mod = {{SD_MS_LFO1, SD_MD_RPOS, 8274}, {SD_MS_LFO1, SD_MD_WARP, 4965}, {SD_MS_LFO2, SD_MD_DHIGH, 9929},
                {SD_MS_WHEEL, SD_MD_DFB, 13239},
                /* the engines talking: the bass pushes the stages, the split sweeps the loop */
                {SD_MS_DLOW, SD_MD_RPOS, 6620}, {SD_MS_STEP, SD_MD_DSPLIT, 6620}}, .lfo_shape = {SD_LS_SH, SD_LS_SH},
                        .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC},
        .lfo_rate = {100, 16484}, .mac4 = SD_M4_BODY},
    {   /* VOWEL (§7): a saw-like voice through six vowels, A E EE AW OO ER, a stage a beat with a glide between; VBP,
         * so RES sets the formants' width and not their loudness */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OSC(2, 0, 14000, C_), SD_OFF, SD_OFF)), SD_CORNER_A,
        .warp = 26624,
        .e1_a = 5, .e1_d = 60, .e1_s = 110, .e1_r = 50, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RST_VBP(SD_RV_A), SD_RST_VBP(SD_RV_E), SD_RST_VBP(SD_RV_EE), SD_RST_VBP(SD_RV_AW),
                  SD_RST_VBP(SD_RV_OO), SD_RST_VBP(SD_RV_ER)},
        .r_pan = {-6000, 6000, -12000, 12000}, .r_model = SD_RM_VBP, .r_res = 30000, .r_slew = 30720,
        .r_drive = 14000, .r_mix = 36000, .r_track = SD_RT_HARM, .r_low = 24000, .r_div = 4, .r_cmode = SD_RC_FWD,
        .r_max = 5,
        .s_size = 60000, .s_decay = 42000, .s_rtone = 45000, .s_rlevel = 16000,
        /* C1: harder notes ring more */
        .mod = {{SD_MS_VEL, SD_MD_RRES, 4965}, {SD_MS_AFTER, SD_MD_RDRIVE, 9929}}, .mac4 = SD_M4_AIR},
    {   /* CHORDS (§7, the chord half of VOWEL): a buzzy chain through narrow BP12 filters on chords that follow the
         * key, a stage every half bar, ping-pong */
        .bank = SD_SAME(BK_GROWL), SD_CORNER_A, .xm = 30000, .warp = 40000,
        .e1_a = 5, .e1_d = 80, .e1_s = 100, .e1_r = 60, .e2_a = 0, .e2_d = 60, .e2_s = 60, .e2_r = 50,
        .stage = {SD_RCH(60, 4500u, 0, 4, 7, 12), SD_RCH(60, 4500u, -3, 0, 4, 7), SD_RCH(60, 4500u, 0, 5, 7, 12),
                  SD_RCH(60, 4500u, -2, 2, 5, 9), SD_RCH(60, 4500u, 0, 4, 7, 11), SD_RCH(60, 4500u, 0, 2, 7, 14)},
        .r_pan = {-20000, -7000, 7000, 20000}, .r_model = SD_RM_BP12, .r_res = 38000, .r_slew = 6000,
        .r_drive = 12000, .r_mix = 26000, .r_track = SD_RT_KEY | SD_RT_HARM, .r_low = 24000, .r_div = 8,
        .r_cmode = SD_RC_PP, .r_max = 5,
        .s_time = SD_SPDIV(10), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 26000, .s_dtone = 30000, .s_dlevel = 16000,
        .s_size = 50000, .s_decay = 40000, .s_rtone = 36000, .s_rlevel = 10000},
    {   /* PLUCKS (§7 PLUCK CHOIR): a short bright pluck pinging four KP combs on chord pitches that follow the key,
         * a chord a beat, each comb panned */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 40000, C_), SD_OSC(3, 0, 20000, C_), SD_OSC(5, 0, 12000, C_), SD_OFF)),
        SD_CORNER_A, .warp = 20000,
        .e1_a = 0, .e1_d = 25, .e1_s = 0, .e1_r = 20, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RCH(48, SD_RLV_KP, 0, 7, 12, 16), SD_RCH(48, SD_RLV_KP, -3, 4, 9, 12),
                  SD_RCH(48, SD_RLV_KP, -5, 2, 7, 11), SD_RCH(48, SD_RLV_KP, 0, 4, 7, 12),
                  SD_RCH(48, SD_RLV_KP, -7, 0, 5, 9), SD_RCH(48, SD_RLV_KP, -2, 5, 10, 14)},
        .r_pan = {-26000, 26000, -9000, 9000}, .r_model = SD_RM_KP, .r_res = 60000,
        .r_drive = 16384, .r_mix = 40000, .r_track = SD_RT_KEY | SD_RT_HARM, .r_low = 20000, .r_div = 4,
        .r_cmode = SD_RC_FWD, .r_max = 5,
        .s_time = SD_SPDIV(7), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 31000, .s_dtone = 40000, .s_dlevel = 22000,
        .s_size = 50000, .s_decay = 36000, .s_rtone = 40000, .s_rlevel = 14000,
        /* C1: each pluck a little elsewhere in the stages */
        .mod = {{SD_MS_VEL, SD_MD_RRES, 6620}, {SD_MS_RAND, SD_MD_RPOS, 6620}}, .mac4 = SD_M4_DROWN},
    {   /* FLANGE: AP1 on a held saw, its four delays high and close, sliding slowly stage to stage: the pitch skates */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OSC(2, 3, 18000, C_), SD_OFF, SD_OFF)), SD_CORNER_A,
        .warp = 26624,
        .e1_a = 10, .e1_d = 60, .e1_s = 120, .e1_r = 60, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RAP(96, 98, 100, 103), SD_RAP(84, 87, 90, 92), SD_RAP(100, 101, 104, 106), SD_RAP(90, 93, 95, 97),
                  SD_RAP(103, 105, 107, 110), SD_RAP(88, 90, 94, 96)},
        .r_model = SD_RM_AP1, .r_res = 62000, .r_slew = 52000,
        .r_drive = 16384, .r_mix = 65535, .r_div = 8, .r_cmode = SD_RC_PP, .r_max = 5},
    {   /* PHASER (a 70's phaser sweep): AP2 on the same saw, eight stages of phase gliding low to high */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OSC(2, 3, 18000, C_), SD_OFF, SD_OFF)), SD_CORNER_A,
        .warp = 26624,
        .e1_a = 10, .e1_d = 60, .e1_s = 120, .e1_r = 60, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RAP(60, 67, 74, 81), SD_RAP(70, 77, 84, 91), SD_RAP(80, 87, 94, 101), SD_RAP(88, 95, 102, 108),
                  SD_RAP(78, 85, 92, 99), SD_RAP(66, 73, 80, 87)},
        .r_model = SD_RM_AP2, .r_res = 56000, .r_slew = 54000,
        .r_drive = 16384, .r_mix = 65535, .r_div = 8, .r_cmode = SD_RC_FWD, .r_max = 5,
        .s_size = 50000, .s_decay = 40000, .s_rtone = 42000, .s_rlevel = 14000},
    {   /* RIDE (Gate D RIDER): one patch, its stages a phoneme, a chord, a phoneme, a single high ring, a phoneme, a
         * chord; BP12 high, DRIVE into the loops' saturation, ping-pong with a glide between */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 50000, C_), SD_OSC(2, 0, 16000, C_), SD_OSC(1, 7, 20000, C_), SD_OFF)),
        SD_CORNER_A, .warp = 30000, .dt = 8000,
        .e1_a = 8, .e1_d = 80, .e1_s = 110, .e1_r = 60, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        .stage = {SD_RST_VOWEL(SD_RV_A), SD_RCH(48, 26000u, 0, 7, 12, 16), SD_RST_VOWEL(SD_RV_OO),
                  {{91 * 256u, 98 * 256u, 103 * 256u, 108 * 256u}, {40000u, 14000u, 9000u, 6000u}},
                  SD_RST_VOWEL(SD_RV_EE), SD_RCH(48, 26000u, -2, 5, 10, 15)},
        .r_pan = {-14000, 14000, -26000, 26000}, .r_model = SD_RM_BP12, .r_res = 40000, .r_slew = 36000,
        .r_drive = 5500, .r_mix = 24000, .r_track = SD_RT_HARM, .r_low = 22000, .r_div = 4, .r_cmode = SD_RC_PP,
        .r_max = 5,
        .s_time = SD_SPMS(340u), .s_fb = 20000, .s_dtone = 30000, .s_dlevel = 12000,
        .s_size = 46000, .s_decay = 40000, .s_rtone = 36000, .s_rlevel = 16000,
        /* C1: the filters drifting over two bars */
        .mod = {{SD_MS_LFO1, SD_MD_RFREQ, 2648}}, .lfo_shape = {SD_LS_SIN}, .lfo_mode = {SD_LM_SYNC},
        .lfo_rate = {49252}, .mac4 = SD_M4_BODY},
    {   /* SPLIT (§7 SPLIT DAMAGE): clean lows, crushed mids, gritty biased upper mids, folded highs; LR4, the bands
         * panned apart. (PHAT on the low band comes with D2) */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 46000, C_), SD_OSC(2, 0, 20000, C_), SD_OSC(3, 0, 12000, C_),
                                SD_OSC(1, 0, 14000, S_))), SD_CORNER_A, .warp = 30000, .xm = 20000,
        .e1_a = 5, .e1_d = 80, .e1_s = 100, .e1_r = 50, .e2_a = 0, .e2_d = 70, .e2_s = 60, .e2_r = 50,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_LR4,
        .d_type = {SD_DS_NONE, SD_DS_CRUSH, SD_DS_SOFT, SD_DS_FOLD}, .d_gain = {0, 12000, 22000, 26000},
        .d_alt = {0, 46000, 36000, 34000}, .d_bias = {0, 0, 14000, 0}, .d_level = {20000, 17000, 13500, 12000},
        .d_pan = {0, -16000, 16000, 0}, .d_mix = 65535,
        .d_shape = {-22937, 0, 0, 0}, .d_body = {57344, 0, 0, 0},
        /* the bias follows the level; the top band and the upper split step on eighths and halves */
        .mod = {{SD_MS_ENV1, SD_MD_DBIAS, 6620}, {SD_MS_LFO1, SD_MD_DHIGH, 8274},
                {SD_MS_LFO2, SD_MD_DSPLIT, 6620}}, .lfo_shape = {SD_LS_SH, SD_LS_SH},
                        .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC},
        .lfo_rate = {5561, 27406}, .mac4 = SD_M4_BODY},   /* D2: PHAT on the low band (§7), T -12 dBFS */

    /* ---- after C1 (the user, 2026-11-05: "more variation … ramp-down curves that modulate within a note … more
     * patches"): movement inside the note (LFOs in ONE: a SAW once a note is a ramp down), stepped S&H on the
     * tempo, the destruction on the top band; warps under the saw (no aliasing noise) */
    {   /* COMET: an FM pluck, 2:1 and 3:1 into two carriers; inside each note the FM and the top band's grit fall
         * away over a beat, the warp steps on eighths */
        .bank = SD_SAME(SD_BANK(SD_OSC(2, 0, 30000, S_), SD_OSC(1, 0, 50000, C_), SD_OSC(3, 0, 16000, S_),
                                SD_OSC(1, 7, 30000, C_))), SD_CORNER_A, .xm = 18000, .warp = 9000, .width = 20000,
        .e1_a = 2, .e1_d = 80, .e1_s = 60, .e1_r = 50, .e2_a = 0, .e2_d = 60, .e2_s = 30, .e2_r = 50,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_CMP,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_SOFT, SD_DS_HARD}, .d_gain = {0, 6000, 14000, 22000},
        .d_alt = {0, 12000, 20000, 26000}, .d_level = {32768, 30000, 24000, 16000}, .d_mix = 52000, .d_tame = 14000,
        .s_time = SD_SPDIV(7), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 26000, .s_dtone = 30000, .s_dlevel = 12000,
        .s_size = 40000, .s_decay = 34000, .s_rtone = 34000, .s_rlevel = 9000,
        .mod = {{SD_MS_LFO1, SD_MD_XM, 5958}, {SD_MS_LFO1, SD_MD_DHIGH, 4634}, {SD_MS_LFO2, SD_MD_WARP, 3972},
                /* the engines talking: the grit feeds the echoes */
                {SD_MS_DTOP, SD_MD_SDLVL, 9929}},
        .lfo_shape = {SD_LS_SAW, SD_LS_SH}, .lfo_mode = {SD_LM_SYNC | SD_LM_ONE, SD_LM_SYNC},
        .lfo_rate = {16484, 5561}, .mac4 = SD_M4_BODY},
    {   /* NEBULA: a pad (four voices: POLY) whose character is the Rider: four KP combs tuned to the chord's
         * tones and kept on the note's harmonics (KEY+H), bowed by a soft triangle stack; their chord steps once a bar
         * (S&H on POS), the fold grows inside each note; the top band breathes, SOFT, on halves; the room open */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 34000, C_), SD_OSC(2, 5, 16000, C_), SD_OSC(4, -6, 9000, C_),
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
    {   /* PULSAR: a spectral sequencer: four banks as far apart as they go (A glass, B an odd
         * cluster on harmonics 5 7 9 11, C a chain toward the triangle (its offset counts x 2 on the Q page), D the
         * organ's 1 and 3), X stepping on 16ths and Y on eighths (S&H), so each step lands in another part of the
         * field; inside each note a little fold falls away; only SOFT on the top */
        .bank = {BK_GLASS, SD_BANK(SD_OSC(5, 0, 26000, C_), SD_OSC(7, 0, 20000, C_), SD_OSC(9, 0, 14000, C_),
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
    {   /* ION: a clean round bass (a triangle and its octave) under a destroyed top: the upper split jumps on 16ths,
         * the top band's grit falls inside each note */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OSC(2, 0, 22000, C_), SD_OSC(3, 0, 16000, S_),
                                SD_OSC(1, 0, 14000, S_))), SD_CORNER_A, .warp = 14000, .xm = 26000,
        .e1_a = 1, .e1_d = 70, .e1_s = 90, .e1_r = 30, .e2_a = 0, .e2_d = 55, .e2_s = 40, .e2_r = 30,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_CMP,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_SOFT}, .d_gain = {0, 9000, 16000, 22000},
        .d_alt = {0, 14000, 20000, 22000}, .d_level = {32768, 28000, 20000, 18000}, .d_mix = 56000,
        .d_pan = {0, -10000, 12000, -12000}, .d_tame = 20000,   /* (CRUSH there was hash: a 6.5 kHz centroid) */
        .d_shape = {-14000, 0, 0, 20000}, .d_body = {40000, 0, 0, 24000},
        .mod = {{SD_MS_LFO1, SD_MD_DSPLIT, 9929}, {SD_MS_LFO2, SD_MD_DHIGH, 11584}, {SD_MS_VEL, SD_MD_XM, 6620},
                /* the engines talking: the bass ducks the grit */
                {SD_MS_DLOW, SD_MD_DHIGH, -6620}},
        .lfo_shape = {SD_LS_SH, SD_LS_SAW}, .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC | SD_LM_ONE},
        .lfo_rate = {100, 16484}, .mac4 = SD_M4_BODY},
    {   /* ORBITAL: percussion with resonance: a short FM hit, its pitch falling ~150 ms, landing (RAND) anywhere in
         * a field of a bell, a chain, glass and the odd cluster; four BP6 filters on the note's harmonics ring after it,
         * their place a new step each 16th; the mid band crunches (HARD under 5 kHz: no fold-back)
         */
        .bank = {BK_BELL, SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 30000, S_),
                                      SD_OSC(1, 0, 60000, C_), 0, 8000, 0, 0),
                 BK_GLASS, SD_BANK(SD_OSC(5, 0, 26000, C_), SD_OSC(7, 0, 20000, C_), SD_OSC(9, 0, 14000, C_),
                                   SD_OSC(11, 0, 10000, C_))},
        .x = 32768, .y = 32768, .xm = 30000, .width = 40000,
        .e1_a = 0, .e1_d = 86, .e1_s = 0, .e1_r = 70, .e2_a = 0, .e2_d = 55, .e2_s = 0, .e2_r = 50,
        .stage = {SD_RCH(48, 22000u, 0, 12, 19, 24), SD_RCH(48, 22000u, 7, 12, 24, 31),
                  SD_RCH(48, 22000u, 0, 7, 19, 28), SD_RCH(48, 22000u, 12, 19, 24, 36),
                  SD_RCH(48, 22000u, 0, 12, 24, 34), SD_RCH(48, 22000u, 7, 19, 28, 36)},
        .r_pan = {-24000, 24000, -10000, 10000}, .r_model = SD_RM_BP6, .r_res = 52000, .r_slew = 0,
        .r_drive = 14000, .r_mix = 30000, .r_track = SD_RT_KEY | SD_RT_HARM, .r_max = 5,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_CMP,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_NONE}, .d_gain = {0, 6000, 9000, 0},
        .d_alt = {0, 10000, 20000, 0}, .d_level = {32768, 30000, 20000, 26000}, .d_mix = 44000,
        .s_time = SD_SPDIV(4), .s_mode = SD_SP_SYNC | SD_SP_PP, .s_fb = 20000, .s_dtone = 26000, .s_dlevel = 12000,
        .s_size = 46000, .s_decay = 40000, .s_rtone = 34000, .s_rlevel = 12000,
        .mod = {{SD_MS_LFO1, SD_MD_PITCH, 1324}, {SD_MS_LFO1, SD_MD_XM, 7282}, {SD_MS_RAND, SD_MD_X, 14894},
                {SD_MS_RAND, SD_MD_Y, 14894}, {SD_MS_LFO2, SD_MD_RPOS, 13239},
                /* the engines talking: the stage picks where it lands */
                {SD_MS_RSTAGE, SD_MD_X, 9929}},
        .lfo_shape = {SD_LS_SAW, SD_LS_SH}, .lfo_mode = {SD_LM_ONE, SD_LM_SYNC}, .lfo_rate = {50087, 100},
        .mac4 = SD_M4_DROWN},
    {   /* MONOLITH: long notes, dry: a chain toward the triangle whose FM grows inside each note (a RAMP once a
         * note) and whose fold rises across the loop (SEQ STEP); the warp steps on quarters; a little SOFT on the
         * top, a small room. (Drowned in the room and a fast delay with its top gated on eighths it was, the user:
         * "completely unusable … drowned in reverb / fast delay / ring modulator") */
        .bank = SD_SAME(BK_GROWL), SD_CORNER_A, .xm = 14000, .warp = 15000, .fold = 4000, .width = 24000,
        .e1_a = 30, .e1_d = 90, .e1_s = 112, .e1_r = 60, .e2_a = 30, .e2_d = 80, .e2_s = 90, .e2_r = 60,
        .d_split = {SD_DHZ_200, SD_DHZ_1200, SD_DHZ_5000}, .d_xover = SD_DX_CMP,
        .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_SOFT, SD_DS_SOFT}, .d_gain = {0, 4000, 8000, 10000},
        .d_alt = {0, 8000, 12000, 12000}, .d_level = {28000, 26000, 20000, 14000}, .d_mix = 40000, .d_tame = 16000,
        .s_size = 30000, .s_decay = 30000, .s_rtone = 30000, .s_rlevel = 9000,
        .mod = {{SD_MS_LFO1, SD_MD_XM, 6620}, {SD_MS_STEP, SD_MD_FOLD, 3972}, {SD_MS_LFO2, SD_MD_WARP, 2648}},
        .lfo_shape = {SD_LS_RAMP, SD_LS_SH}, .lfo_mode = {SD_LM_ONE, SD_LM_SYNC}, .lfo_rate = {23349, 16484},
        .mac4 = SD_M4_BODY},
    {   /* STRINGS (the user, 2026-11-05: a cello, "as close as possible" to the recorded C major, then V4 by ear in
         * the duo, tools/cello_v4.txt): a bowed string is a saw through a body.
         * - The source: a plain saw (WARP 25544), slow in (70), a long bow (sustain 118), 8 cents of vibrato at 5.5 Hz.
         * - The body: the Rider's four BP6s snapped to the note's harmonics (HARM), stages 1-3 a cello's
         *   (~100 200 320 Hz, 1 kHz), 4-6 brighter (~280 420 Hz, 1.05 1.65 kHz); POS on stage 2.
         * - The tilt: DESTROY unshaped as four levels (-22 -4.8 -0.1 +6.8 dB at 262 750 2380 Hz) and TAME; a small
         *   room, mono (the duo's and cue 2's strings pan by the part).
         * The earlier STRINGS (matched to a cello's and a violin's scale, KEY moving it from the one to the other) is
         * in the docs' history; sd_strings PARAMS still builds a patch from key-value lines for tuning, and
         * tests/run_sd.sh checks this one renders as tools/cello_v4.txt does. */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OFF, SD_OFF, SD_OFF)),
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
        .d_split = {15366, 20028, 25146}, .d_level = {2602, 18856, 32392, 65535}, .d_tame = 22000, .d_mix = 65535,
        .s_size = 30000, .s_decay = 30000, .s_rlevel = 8000,
        .mod = {[1] = {SD_MS_LFO2, SD_MD_PITCH, 109}}, .lfo_rate = {0, 48429}},
    {   /* HALO (cue 2's pad): NEBULA in a big dark hall and a ping-pong delay of dotted quarters, DROWN on MACRO 4
         * (POLY in the cue) */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 34000, C_), SD_OSC(2, 5, 16000, C_), SD_OSC(4, -6, 9000, C_),
                SD_OSC(1, -9, 22000, C_))),
        .warp = 12000, .dt = 12000, .e1_a = 70, .e1_d = 100, .e1_s = 110, .e1_r = 96, .e2_a = 20, .e2_d = 20,
        .e2_s = 127, .e2_r = 45,
        SD_CORNER_A, .width = 46000,
        .stage = {{{12288u, 14080u, 15360u, 16384u}, {20000u, 20000u, 20000u, 20000u}},
                  {{11520u, 13312u, 14592u, 15360u}, {20000u, 20000u, 20000u, 20000u}},
                  {{11008u, 12800u, 14080u, 15872u}, {20000u, 20000u, 20000u, 20000u}},
                  {{12288u, 13312u, 14080u, 17152u}, {20000u, 20000u, 20000u, 20000u}},
                  {{10496u, 12288u, 13568u, 15360u}, {20000u, 20000u, 20000u, 20000u}},
                  {{11776u, 13568u, 14848u, 16640u}, {20000u, 20000u, 20000u, 20000u}}},
        .r_pan = {-26000, 26000, -12000, 12000}, .r_res = 50000, .r_slew = 30000, .r_drive = 9000, .r_mix = 24000,
        .r_model = SD_RM_KP, .r_track = SD_RT_KEY | SD_RT_HARM, .r_max = 5, .r_low = 24000,
        .d_split = {14170, 22111, 28436}, .d_type = {SD_DS_NONE, SD_DS_NONE, SD_DS_SOFT, SD_DS_SOFT},
        .d_gain = {0, 0, 10000, 14000}, .d_alt = {0, 0, 14000, 16000}, .d_level = {32768, 32768, 26000, 18000},
        .d_tame = 12000, .d_mix = 36000,
        .s_time = 56000, .s_fb = 38000, .s_dtone = 22000, .s_dlevel = 19661, .s_size = 64000, .s_decay = 57000,
        .s_rtone = 30000, .s_rlevel = 34000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {{SD_MS_LFO1, SD_MD_RPOS, 11584}, {SD_MS_LFO2, SD_MD_FOLD, 4634}, {SD_MS_LFO1, SD_MD_DHIGH, 4965},
                {SD_MS_ECHO, SD_MD_FOLD, 3310}},
        .lfo_shape = {SD_LS_SH, SD_LS_RAMP}, .lfo_mode = {SD_LM_SYNC, SD_LM_ONE}, .lfo_rate = {38329, 21777},
        .mac4 = SD_M4_DROWN},
    {   /* AURORA (cue 2): the GLASS bank (harmonics 1 2 3 4) slow in and out (85 / 90), wide, in the big hall, its
         * delay on dotted quarters; DROWN on MACRO 4 (POLY in the cue) */
        .bank = SD_SAME(BK_GLASS),
        .e1_a = 85, .e1_d = 80, .e1_s = 120, .e1_r = 90, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        SD_CORNER_A, .width = 65535,
        .s_time = 56000, .s_fb = 38000, .s_dtone = 22000, .s_dlevel = 22937, .s_size = 64000, .s_decay = 57000,
        .s_rtone = 30000, .s_rlevel = 34000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        /* the edge: its top (over 2.5 kHz) folded to a sheen, the fold a new depth each bar */
        SD_EDGE(SD_DHZ_2500, SD_DS_FOLD, 18000, 32000, 16000, 32768u),
        .mod = {{SD_MS_AFTER, SD_MD_XM, 9929}, {SD_MS_WHEEL, SD_MD_WARP, 13239}, {SD_MS_LFO1, SD_MD_FOLD, 2600}},
        .lfo_shape = {SD_LS_SH}, .lfo_mode = {SD_LM_SYNC}, .lfo_rate = {38300}, .mac4 = SD_M4_DROWN},
    {   /* TITAN (cue 2): DISTROY's chords swelling in (55) and out long (80), its top tamed (POLY in the cue) */
        .bank = {BK_GROWL,
                 BK_BELL_LIGHT,
                 BK_EPIANO_LIGHT,
                 BK_GROWL},
        .xm = 50000, .warp = 50000, .fold = 30000, .dt = 20000, .e1_a = 55, .e1_d = 70, .e1_s = 100, .e1_r = 80,
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
        .d_spread = 22000, .d_tame = 26000, .d_mix = 36000,
        .s_time = 11022, .s_fb = 40000, .s_dtone = 30000, .s_dlevel = 18000, .s_size = 65535, .s_decay = 50000,
        .s_rtone = 36000, .s_rlevel = 16000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {{SD_MS_LFO1, SD_MD_RPOS, 8274}, {SD_MS_LFO1, SD_MD_WARP, 4965}, {SD_MS_LFO2, SD_MD_DHIGH, 9929},
                {SD_MS_WHEEL, SD_MD_DFB, 13239}, {SD_MS_DLOW, SD_MD_RPOS, 6620}, {SD_MS_STEP, SD_MD_DSPLIT, 6620}},
        .lfo_shape = {SD_LS_SH, SD_LS_SH}, .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC}, .lfo_rate = {100, 16484}},
    {   /* DRIVE (cue 2's 16ths): ION without the grit split jumping on the step (the bass has it), in the big
         * hall with a dotted-eighth delay: it answers itself (PULSAR's vector jumping between banks stood out from
         * the rest, the user said) */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OSC(2, 0, 22000, C_), SD_OSC(3, 0, 16000, S_),
                SD_OSC(1, 0, 14000, S_))),
        .xm = 26000, .warp = 14000, .e1_a = 1, .e1_d = 70, .e1_s = 90, .e1_r = 30, .e2_a = 0, .e2_d = 55, .e2_s = 40,
        .e2_r = 30,
        SD_CORNER_A,
        .d_split = {14170, 22111, 28436}, .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_SOFT},
        .d_gain = {0, 9000, 16000, 22000}, .d_alt = {0, 14000, 20000, 22000},
        .d_level = {32768, 28000, 20000, 18000}, .d_pan = {0, -10000, 12000, -12000},
        .d_shape = {-14000, 0, 0, 20000}, .d_body = {40000, 0, 0, 24000}, .d_tame = 20000, .d_mix = 56000,
        .s_time = 40000, .s_fb = 38000, .s_dtone = 22000, .s_dlevel = 22937, .s_size = 64000, .s_decay = 57000,
        .s_rtone = 30000, .s_rlevel = 34000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {[1] = {SD_MS_LFO2, SD_MD_DHIGH, 11584}, {SD_MS_VEL, SD_MD_XM, 6620},
                {SD_MS_DLOW, SD_MD_DHIGH, -6620}},
        .lfo_shape = {SD_LS_SH, SD_LS_SAW}, .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC | SD_LM_ONE}, .lfo_rate = {100, 16484}},
    {   /* UNDERTOW (cue 2's eighths): ION dry: no delay, no room (under a cue they only blur the low end) */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 52000, C_), SD_OSC(2, 0, 22000, C_), SD_OSC(3, 0, 16000, S_),
                SD_OSC(1, 0, 14000, S_))),
        .xm = 26000, .warp = 14000, .e1_a = 1, .e1_d = 70, .e1_s = 90, .e1_r = 30, .e2_a = 0, .e2_d = 55, .e2_s = 40,
        .e2_r = 30,
        SD_CORNER_A,
        .d_split = {14170, 22111, 28436}, .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_SOFT},
        .d_gain = {0, 9000, 16000, 22000}, .d_alt = {0, 14000, 20000, 22000},
        .d_level = {32768, 28000, 20000, 18000}, .d_pan = {0, -10000, 12000, -12000},
        .d_shape = {-14000, 0, 0, 20000}, .d_body = {40000, 0, 0, 24000}, .d_tame = 20000, .d_mix = 56000,
        .mod = {{SD_MS_LFO1, SD_MD_DSPLIT, 9929}, {SD_MS_LFO2, SD_MD_DHIGH, 11584}, {SD_MS_VEL, SD_MD_XM, 6620},
                {SD_MS_DLOW, SD_MD_DHIGH, -6620}},
        .lfo_shape = {SD_LS_SH, SD_LS_SAW}, .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC | SD_LM_ONE}, .lfo_rate = {100, 16484}},
    {   /* IMPACT (cue 2's drums): ORBITAL dry: its hits ring in the Rider, not in SPACE */
        .bank = {BK_BELL,
                 SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 30000, S_),
                     SD_OSC(1, 0, 60000, C_), 0, 8000, 0, 0),
                 BK_GLASS,
                 SD_BANK(SD_OSC(5, 0, 26000, C_), SD_OSC(7, 0, 20000, C_), SD_OSC(9, 0, 14000, C_),
                     SD_OSC(11, 0, 10000, C_))},
        .xm = 30000, .e1_a = 0, .e1_d = 86, .e1_s = 0, .e1_r = 70, .e2_a = 0, .e2_d = 55, .e2_s = 0, .e2_r = 50,
        .x = 32768, .y = 32768, .width = 40000,
        .stage = {{{12288u, 15360u, 17152u, 18432u}, {22000u, 22000u, 22000u, 22000u}},
                  {{14080u, 15360u, 18432u, 20224u}, {22000u, 22000u, 22000u, 22000u}},
                  {{12288u, 14080u, 17152u, 19456u}, {22000u, 22000u, 22000u, 22000u}},
                  {{15360u, 17152u, 18432u, 21504u}, {22000u, 22000u, 22000u, 22000u}},
                  {{12288u, 15360u, 18432u, 20992u}, {22000u, 22000u, 22000u, 22000u}},
                  {{14080u, 17152u, 19456u, 21504u}, {22000u, 22000u, 22000u, 22000u}}},
        .r_pan = {-24000, 24000, -10000, 10000}, .r_res = 52000, .r_drive = 14000, .r_mix = 30000,
        .r_track = SD_RT_KEY | SD_RT_HARM, .r_max = 5,
        .d_split = {14170, 22111, 28436}, .d_type = {SD_DS_NONE, SD_DS_SOFT, SD_DS_HARD, SD_DS_NONE},
        .d_gain = {0, 6000, 9000, 0}, .d_alt = {0, 10000, 20000, 0}, .d_level = {32768, 30000, 20000, 26000},
        .d_mix = 44000,
        .s_time = 21945, .s_fb = 20000, .s_dtone = 26000, .s_size = 46000, .s_decay = 40000, .s_rtone = 34000,
        .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {{SD_MS_LFO1, SD_MD_PITCH, 1324}, {SD_MS_LFO1, SD_MD_XM, 7282}, {SD_MS_RAND, SD_MD_X, 14894},
                {SD_MS_RAND, SD_MD_Y, 14894}, {SD_MS_LFO2, SD_MD_RPOS, 13239}, {SD_MS_RSTAGE, SD_MD_X, 9929}},
        .lfo_shape = {SD_LS_SAW, SD_LS_SH}, .lfo_mode = {SD_LM_ONE, SD_LM_SYNC}, .lfo_rate = {50087, 100},
        .mac4 = SD_M4_DROWN},
    {   /* BRAAM (cue 2's hits, the low one): DISTROY whose destroyed band falls through the spectrum in a hit: LFO2 a
         * SAW once a note (SYNC KEY ONE, half a bar) on D SPLIT and XM; its top tamed: hard, not screaming */
        .bank = {BK_GROWL,
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
    {   /* SCREAM (cue 2's hits, the high one): BRAAM falling further (D SPLIT 45 % against 30 %) */
        .bank = {BK_GROWL,
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
                {SD_MS_LFO2, SD_MD_DSPLIT, 14745}, {SD_MS_LFO2, SD_MD_XM, 8192}},
        .lfo_shape = {SD_LS_SH, SD_LS_SAW}, .lfo_mode = {SD_LM_SYNC, SD_LM_SYNC | SD_LM_KEY | SD_LM_ONE},
        .lfo_rate = {100, 30000}},
    {   /* BEACON (cue 2's high line): TOUR far back: a slow 10-cent vibrato (5 Hz), its top tamed (the user heard
         * TOUR's high partials scream), in the big hall with dotted-quarter delays; DROWN on MACRO 4 */
        .bank = {BK_GLASS,
                 BK_BELL,
                 SD_BANK_OFS(SD_OSC(1, 0, 20000, S_), SD_OSC(2, 0, 16000, S_), SD_OSC(1, 0, 30000, S_),
                     SD_OSC(1, 0, 60000, C_), 0, 22000, 12000, 0),
                 BK_ORGAN},
        .xm = 40000, .e1_a = 60, .e1_d = 90, .e1_s = 100, .e1_r = 75, .e2_a = 0, .e2_d = 70, .e2_s = 60, .e2_r = 60,
        SD_CORNER_A, .width = 40000, .vec = 1, .cell_x = {0, 65535, 65535, 0}, .cell_y = {65535, 65535, 0, 0},
        .cell_ms = {1500, 1500, 1500},
        .d_split = {12288, 18432, 24576}, .d_level = {32768, 32768, 32768, 32768}, .d_tame = 30000, .d_mix = 65535,
        .s_time = 56000, .s_fb = 38000, .s_dtone = 22000, .s_dlevel = 26214, .s_size = 64000, .s_decay = 57000,
        .s_rtone = 30000, .s_rlevel = 34000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {{SD_MS_LFO1, SD_MD_X, 9929}, {SD_MS_LFO2, SD_MD_Y, 9929}, {SD_MS_STEP, SD_MD_Y, 8274},
                {SD_MS_ECHO, SD_MD_WARP, 3310}, {SD_MS_LFO2, SD_MD_PITCH, 137}},
        .lfo_shape = {SD_LS_SH, SD_LS_SIN}, .lfo_mode = {SD_LM_SYNC, 0}, .lfo_rate = {16484, 47607},
        .mac4 = SD_M4_DROWN},
    {   /* DEPTH (cue 2's depth): a sine, slow in and out */
        .bank = SD_SAME(SD_BANK(SD_OSC(1, 0, 65535, C_), SD_OFF, SD_OFF, SD_OFF)),
        .e1_a = 70, .e1_d = 20, .e1_s = 120, .e1_r = 85, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        SD_CORNER_A},
    {   /* SHARDS (cue 2's 32nd arp): the GLASS bank short (no sustain), wide, without detune (up there any reads as
         * out of tune), its top tamed, in the big hall with dotted-eighth delays; DROWN on MACRO 4 */
        .bank = SD_SAME(BK_GLASS),
        .e1_a = 0, .e1_d = 50, .e1_s = 0, .e1_r = 45, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        SD_CORNER_A, .width = 65535,
        .d_split = {12288, 18432, 24576}, .d_level = {32768, 32768, 32768, 32768}, .d_tame = 26000, .d_mix = 65535,
        .s_time = 40000, .s_fb = 38000, .s_dtone = 22000, .s_dlevel = 29491, .s_size = 64000, .s_decay = 57000,
        .s_rtone = 30000, .s_rlevel = 34000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {{SD_MS_AFTER, SD_MD_XM, 9929}, {SD_MS_WHEEL, SD_MD_WARP, 13239}}, .mac4 = SD_M4_DROWN},
    {   /* PRISM (cue 2's falling 16ths): SHARDS ringing longer (70 / 60), darker, its delays on dotted quarters:
         * it cascades */
        .bank = SD_SAME(BK_GLASS),
        .e1_a = 0, .e1_d = 70, .e1_s = 0, .e1_r = 60, .e2_a = 20, .e2_d = 20, .e2_s = 127, .e2_r = 45,
        SD_CORNER_A, .width = 65535,
        .d_split = {12288, 18432, 24576}, .d_level = {32768, 32768, 32768, 32768}, .d_tame = 22000, .d_mix = 65535,
        .s_time = 56000, .s_fb = 38000, .s_dtone = 22000, .s_dlevel = 26214, .s_size = 64000, .s_decay = 57000,
        .s_rtone = 30000, .s_rlevel = 34000, .s_mode = SD_SP_SYNC | SD_SP_PP,
        .mod = {{SD_MS_AFTER, SD_MD_XM, 9929}, {SD_MS_WHEEL, SD_MD_WARP, 13239}}, .mac4 = SD_M4_DROWN},

};
#define SD_PATCH_WORST SD_P_DISTROY                       /* the boot patch: fm1-emu measures the worst case */

/* The algorithms the ALGORITHM encoder steps through: roles of oscillators 1..4, left to right. PPCC: 1 and 2
 * in parallel on 3, 4 unmodulated; SCSC: two chains of two. */
#define SD_ALGOS 8u
static const char *const SD_ALGO[SD_ALGOS] = {"SSSC", "PSSC", "PPPC", "SCSC", "PPCC", "CSSC", "SCCC", "CCCC"};

static uint8_t sd_role_of(char c) { return (uint8_t)(c == 'S' ? SD_ROLE_S : c == 'P' ? SD_ROLE_P : SD_ROLE_C); }

#undef C_
#undef S_
#undef P_
#endif
