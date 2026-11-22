/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The drums' kit and pattern (2026-11-19, the user: "the drums are just a bonus", always off at the start; a macro
 * each; the kick a sidechain source in MOD; a deeper snare, synthwave, a sci-fi soundtrack, cinematic). The sounds
 * were tuned on the drum lab's page (claude.ai Drum Lab, its SYNTHWAVE kit); sd_drums.h makes them. Each drum has
 * eight values, K1..K4 (K4 its macro, -100..100: 0 as set) and GLO's four, then its level and pan (MIX):
 *   KICK   TUNE DECAY PUNCH BOOM  | SWEEP CLICK DRIVE SUB
 *   SNARE  TUNE DECAY SNAPPY SIZE | N DECAY TONE BODY ROOM
 *   HAT    TONE DECAY OPEN AIR    | METAL RES DRIVE THIN    (an accent opens it; a hit chokes the last)
 *   CLAP   TONE DECAY SPREAD CROWD| BURSTS RES TAIL ROOM
 * A pitch is a MIDI note, a time an index of the envelopes' times (SD_ENV_MS: 1 ms .. 10 s, to -40 dB), the rest
 * percent. A step: 0 off, 1 on, 2 accent, 3 RADAR (it plays where RADAR's newest bit is 1); its second layer (MORE,
 * 2026-11-20): ROLL x1..x4 and PROB 100 75 50 25 %, beside the beat (SdDrumMore). The fourth drum has an ENGINE
 * (2026-11-20, the user: "the clap swappable with other engines, RADAR can play it"): CLAP, TOM, DATA, SONAR, ZAP,
 * SWELL (2026-11-22: RIM and COW gone), its eight values each its own (SD_PC_*), its TUNE a matrix destination (PERC
 * TUNE: RADAR plays it in semitones). */
#ifndef SD_DRUMKIT_H
#define SD_DRUMKIT_H

enum { SD_DR_KICK, SD_DR_SNARE, SD_DR_HAT, SD_DR_CLAP, SD_DRS };
enum { SD_DK_NOTE, SD_DK_TIME, SD_DK_SEMI, SD_DK_MAC, SD_DK_PCT, SD_DK_N, SD_DK_DIV, SD_DK_SST };   /* what a value is
                                                   (DIV: a division of the step, its place fastest first; SST:
                                                   semitones either way) */
enum { SD_DS_OFF, SD_DS_ON, SD_DS_ACC, SD_DS_RADAR };
#define SD_DR_STEPS 16u
#define SD_DR_PLACES 16u                        /* the beats' places, D01..D16 (PRESETS, SAVE, a song's step) */
#define SD_DR_SONG_OFF 17u                      /* a song's step: the drums off there (0: as they play) */
#define SD_DR_BATCH 8u                          /* the sub-blocks a batch (256 frames: an audio half on the FM-1) */
typedef struct {
    int16_t v[8];                               /* K1..K4, GLO's four (SD_DR_KIND) */
    uint8_t level;                              /* 0..100 */
    int8_t pan;                                 /* -60..60 */
} SdDrumP;
typedef struct { SdDrumP d[SD_DRS]; uint8_t accent, duck; } SdDrumKit;   /* (duck: MIX's DUCK 0..100, the synth
                                                   under the kick; the byte was padding before 2026-11-19: 0) */
typedef struct { uint8_t s[SD_DRS][SD_DR_STEPS / 4u]; } SdDrumPat;   /* 2 bits a step */
#define SD_DR_STEP(p, d, i) ((uint32_t)((p)->s[d][(i) >> 2] >> (((i) & 3u) * 2u)) & 3u)
static void sd_dr_step_set(SdDrumPat *p, uint32_t d, uint32_t i, uint32_t v)
{
    uint32_t sh = (i & 3u) * 2u;
    p->s[d][i >> 2] = (uint8_t)((p->s[d][i >> 2] & ~(3u << sh)) | (v & 3u) << sh);
}

typedef struct { uint8_t x[SD_DRS][SD_DR_STEPS / 2u]; } SdDrumMore;   /* 4 bits a step: ROLL (0..3: x1..x4), PROB
                                                   (0..3: 100 75 50 25 %) */
#define SD_DR_MORE(m, d, i) ((uint32_t)((m)->x[d][(i) >> 1] >> (((i) & 1u) * 4u)) & 15u)
#define SD_DR_ROLL(m, d, i) (SD_DR_MORE(m, d, i) & 3u)
#define SD_DR_PROB(m, d, i) (SD_DR_MORE(m, d, i) >> 2)
static void sd_dr_more_set(SdDrumMore *m, uint32_t d, uint32_t i, uint32_t roll, uint32_t prob)
{
    uint32_t sh = (i & 1u) * 4u;
    m->x[d][i >> 1] = (uint8_t)((m->x[d][i >> 1] & ~(15u << sh)) | ((roll & 3u) | (prob & 3u) << 2) << sh);
}

static const uint8_t SD_DR_KIND[SD_DRS][8] = {
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_SEMI, SD_DK_MAC, SD_DK_TIME, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_PCT, SD_DK_MAC, SD_DK_TIME, SD_DK_NOTE, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_TIME, SD_DK_MAC, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_TIME, SD_DK_MAC, SD_DK_N, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT}};
static const int16_t SD_DR_MIN[SD_DRS][8] = {   /* (notes 21 = 27.5 Hz; times as SD_ENV_MS: 34 ~ 12 ms, 81 ~ 350 ms) */
    {21, 50, 0, -100, 20, 0, 0, 0}, {40, 30, 0, -100, 30, 76, 0, 0},
    {102, 20, 55, -100, 0, 0, 0, 0}, {71, 40, 20, -100, 1, 0, 0, 0}};
static const int16_t SD_DR_MAX[SD_DRS][8] = {
    {45, 105, 48, 100, 90, 100, 100, 100}, {67, 95, 100, 100, 100, 130, 100, 100},
    {129, 75, 100, 100, 100, 100, 100, 100}, {111, 100, 52, 100, 6, 100, 100, 100}};
static const char *const SD_DR_NAME[SD_DRS] = {"KICK", "SNARE", "HAT", "CLAP"};
static const char *const SD_DR_SHORT[SD_DRS] = {"KCK", "SNR", "HAT", "CLP"};
static const char *const SD_DR_PNAME[SD_DRS][8] = {
    {"TUNE", "DECAY", "PUNCH", "BOOM", "SWEEP", "CLICK", "DRIVE", "SUB"},
    {"TUNE", "DECAY", "SNAPPY", "SIZE", "N DECAY", "TONE", "BODY", "ROOM"},
    {"TONE", "DECAY", "OPEN", "AIR", "METAL", "RES", "DRIVE", "THIN"},
    {"TONE", "DECAY", "SPREAD", "CROWD", "BURSTS", "RES", "TAIL", "ROOM"}};

/* the fourth drum's engines (SD_PC_CLAP: the clap above): each its eight values, ranges, names and its sound as
   chosen (an engine picked: these, its level and pan kept). Since 2026-11-22 (the user: "more experimental than a
   typical cowbell"): CLAP and TOM, then four of the ship's own. DATA: a burst of blips, each a new pitch SPREAD
   round TUNE (the same burst each hit), RATE a division of the step; SONAR: a ping and its REPEATS, TIME apart (a
   division), each STEP semitones on and FALL quieter; ZAP: a laser; SWELL: noise and a tone swelling for STEPS steps,
   their band rising with it (SWEEP), cut at the peak (TAIL): a hit played backwards, its peak on the beat STEPS on */
enum { SD_PC_CLAP, SD_PC_TOM, SD_PC_DATA, SD_PC_SONAR, SD_PC_ZAP, SD_PC_SWELL, SD_PCS };
static const char *const SD_PC_NAME[SD_PCS] = {"CLAP", "TOM", "DATA", "SONAR", "ZAP", "SWELL"};
static const char *const SD_PC_SHORT[SD_PCS] = {"CLP", "TOM", "DAT", "SON", "ZAP", "SWL"};
static const uint8_t SD_PC_KIND[SD_PCS][8] = {
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_TIME, SD_DK_MAC, SD_DK_N, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_SEMI, SD_DK_MAC, SD_DK_TIME, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_DIV, SD_DK_MAC, SD_DK_SEMI, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_DIV, SD_DK_MAC, SD_DK_N, SD_DK_SST, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_TIME, SD_DK_SEMI, SD_DK_MAC, SD_DK_TIME, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT},
    {SD_DK_NOTE, SD_DK_N, SD_DK_TIME, SD_DK_MAC, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT, SD_DK_PCT}};
static const int16_t SD_PC_MIN[SD_PCS][8] = {   /* (a DIV: its place fastest first, SD_SDIV_ORDER: 0 1/64 .. 11 1/2) */
    {71, 40, 20, -100, 1, 0, 0, 0}, {36, 50, 0, -100, 20, 0, 0, 0}, {48, 50, 0, -100, 0, 0, 0, 0},
    {60, 30, 0, -100, 1, -12, 0, 0}, {30, 30, 0, -100, 15, 0, 0, 0}, {48, 1, 20, -100, 0, 0, 0, 0}};
static const int16_t SD_PC_MAX[SD_PCS][8] = {
    {111, 100, 52, 100, 6, 100, 100, 100}, {72, 100, 36, 100, 90, 100, 100, 100}, {96, 100, 7, 100, 24, 100, 100, 100},
    {100, 90, 11, 100, 8, 12, 100, 100}, {84, 95, 72, 100, 85, 100, 100, 100}, {96, 16, 90, 100, 100, 100, 100, 100}};
static const char *const SD_PC_PNAME[SD_PCS][8] = {
    {"TONE", "DECAY", "SPREAD", "CROWD", "BURSTS", "RES", "TAIL", "ROOM"},
    {"TUNE", "DECAY", "BEND", "DEEP", "SWEEP", "NOISE", "DRIVE", "RING"},
    {"TUNE", "DECAY", "RATE", "CHAOS", "SPREAD", "GATE", "NOISE", "DRIVE"},
    {"TUNE", "DECAY", "TIME", "DEPTH", "REPEATS", "STEP", "FALL", "DRIVE"},
    {"TUNE", "DECAY", "RANGE", "LASER", "SWEEP", "SHAPE", "DRIVE", "RING"},
    {"TUNE", "STEPS", "TAIL", "SIZE", "NOISE", "RES", "SWEEP", "DRIVE"}};
static const int16_t SD_PC_DEF[SD_PCS][8] = {   /* (as chosen; CLAP the kit's own) */
    {88, 76, 34, 20, 4, 30, 60, 40}, {50, 78, 12, 0, 52, 15, 20, 18}, {76, 78, 0, 0, 12, 45, 15, 25},
    {81, 74, 6, 0, 3, 0, 45, 15}, {45, 72, 36, 0, 62, 30, 30, 0}, {80, 4, 40, 0, 70, 25, 60, 10}};
/* the fourth's tables by its engine, the others' as they are */
static uint32_t sd_dr_kind(uint32_t d, uint32_t pc, uint32_t i) { return d == SD_DR_CLAP ? SD_PC_KIND[pc % SD_PCS][i & 7u] : SD_DR_KIND[d % SD_DRS][i & 7u]; }
static int32_t sd_dr_min(uint32_t d, uint32_t pc, uint32_t i) { return d == SD_DR_CLAP ? SD_PC_MIN[pc % SD_PCS][i & 7u] : SD_DR_MIN[d % SD_DRS][i & 7u]; }
static int32_t sd_dr_max(uint32_t d, uint32_t pc, uint32_t i) { return d == SD_DR_CLAP ? SD_PC_MAX[pc % SD_PCS][i & 7u] : SD_DR_MAX[d % SD_DRS][i & 7u]; }
static const char *sd_dr_pname(uint32_t d, uint32_t pc, uint32_t i) { return d == SD_DR_CLAP ? SD_PC_PNAME[pc % SD_PCS][i & 7u] : SD_DR_PNAME[d % SD_DRS][i & 7u]; }
static const char *sd_dr_name(uint32_t d, uint32_t pc) { return d == SD_DR_CLAP ? SD_PC_NAME[pc % SD_PCS] : SD_DR_NAME[d % SD_DRS]; }
static const char *sd_dr_short(uint32_t d, uint32_t pc) { return d == SD_DR_CLAP ? SD_PC_SHORT[pc % SD_PCS] : SD_DR_SHORT[d % SD_DRS]; }

/* SYNTHWAVE, the lab's (its ms to -60 dB as these times to -40 dB: two thirds) */
static const SdDrumKit SD_DR_KIT0 = {{
    {{30, 81, 31, 20, 73, 18, 22, 25}, 88, 0},
    {{50, 72, 58, 35, 76, 104, 70, 45}, 76, 0},
    {{122, 46, 81, 0, 35, 22, 6, 30}, 46, 16},
    {{88, 76, 34, 20, 4, 30, 60, 40}, 52, -14}}, 40, 0};
/* the engine's side: the voices' states (sd_drums.h), ~0.5 KB */
typedef struct { int32_t a1, a2, a3, k; } SdDrF;   /* a TPT SVF's coefficients (Q30, k Q24) */
typedef struct {
    SdDrumKit kit;                              /* the main loop's (it copies its own in) */
    SdDrumPat pat;                              /* the main loop's */
    SdDrumMore more;                            /* the steps' ROLL and PROB (with the beat) */
    uint8_t perc;                               /* the fourth's engine, SD_PC_* (the main loop's) */
    volatile uint8_t mute;                      /* bit d: drum d's steps silent (the main loop's: its keys) */
    volatile uint8_t frac;                      /* where in its step the grid is, 0..255 (the main loop's REC) */
    volatile uint8_t skip[SD_DRS];              /* REC: a step written just ahead, heard then: not again (its
                                                   step + 1, once) */
    uint8_t roll_n[SD_DRS], roll_k[SD_DRS], roll_a[SD_DRS], hitm;   /* a step's ROLL: its hits, those fired, accent;
                                                   the hits this sub-block (bit d) */
    uint8_t hj[8];                              /* the hits each sub-block of the batch (the LFOs' triggers) */
    uint32_t prng;                              /* PROB's and RADAR's chances */
    uint32_t p_ph, p_ph2, p_inc, p_inc2;        /* the fourth's engines (not CLAP): two oscillators, */
    int32_t p_env, p_envs, p_ne, p_nes, p_pe, p_lva, p_lvb, p_sqa, p_nl, p_short, p_drv, p_nrm, p_tn;   /* their
                                                   envelopes, levels, the noise's, DRIVE; PERC TUNE at the hit */
    uint8_t p_fm;                               /* their filter: 0 none, 1 band, 2 low, 3 high */
    volatile uint8_t on;                        /* EDIT: off at every start */
    volatile uint8_t req;                       /* the main loop's auditions: bit d a hit, bit 4 + d accented */
    volatile uint8_t shown;                     /* the step played last (the screen), 0xFF none */
    uint8_t in, step, live, h_open, c_b, c_last;   /* in (at the master's one); the step; who sounds (bit 4 the room) */
    int16_t mod[6];                             /* the matrix's: the four macros, the level, PERC TUNE (Q15) */
    int32_t bus, gl[SD_DRS], gr[SD_DRS];        /* the level (Q15, x 1.5 at most); each drum's pan and level */
    uint32_t rnd, rnd2, rn[SD_DRS];             /* the noises: the room's two, each drum's own */
    int32_t kenv, senv;                         /* the matrix's sources, KICK and SNARE (Q15) */
    uint32_t k_ph, k_ph2, k_inc;
    int32_t k_env, k_envs, k_sub, k_subs, k_cl, k_cls, k_pe, k_subg, k_clg, k_drv, k_nrm;
    uint32_t s_ph[3], s_inc[3];
    int32_t s_eb, s_ebs, s_en, s_ens, s_th, s_ths, s_bend, s_bends, s_body, s_snap, s_fi[2];
    SdDrF s_f;
    uint32_t h_ph[6], h_inc[6];
    int32_t h_env, h_envs, h_met, h_drv, h_nrm, h_fi1[2], h_fi2[2];
    SdDrF h_f1, h_f2;
    uint32_t c_t;
    int32_t c_v, c_env, c_envs, c_g, c_fi[2];
    SdDrF c_f;
    uint32_t g_t, g_hold;                       /* the gated room: ticks since it opened, how long it holds */
    int32_t g_lvl, g_g, g_gs, g_note, g_fl[2], g_fr[2];
    SdDrF g_f;
    SdDrumPat pat_in;                           /* the main loop's beat: taken once pat_seq moves (and is even), at
                                                   once, or (pat_wait: PRESETS while they play) at the bar's one */
    volatile uint8_t pat_seq, pat_wait;
    uint8_t pat_taken;
    uint8_t song_due, song_v, song_n;           /* the song's: due at the bar's one (a place 1..16, or
                                                   SD_DR_SONG_OFF), its step's last, the last it loaded */
    volatile uint8_t song_seq;                  /* (the main loop follows what the song loaded) */
    volatile uint16_t pats_has;                 /* which places hold a beat (the main loop's) */
    const SdDrumPat *pats;                      /* the places, D01..D16 (the main loop's; 0: none) */
    const SdDrumMore *mores;                    /* their ROLLs and PROBs (0: none) */
    SdDrumMore more_in;                         /* (with pat_in) */
    /* the batch (sd_drums.h): the drums a block ahead, SD_DR_BATCH sub-blocks at once (on the FM-1 the second core's
       job, so their code leaves the flash's cache to the synth), heard sub-block by sub-block */
    int16_t *ob;                                /* its sound, stereo, after the bus (the app's memory; 0: no drums) */
    int32_t kj[8], sj[8];                       /* KICK, SNARE (the matrix's sources) each of its sub-blocks */
    uint32_t grp;                               /* the batch heard now */
    volatile uint32_t ext;                      /* the batch the second core renders (the app's), else ~0 */
    volatile uint32_t done;                     /* its progress: (the batch << 4) | sub-blocks done */
    uint32_t b_n, b_g, b_adv, b_span;           /* the batch made now (its number); its grid: where it starts, a
                                                   sub-block's step, the wrap */
    uint8_t gj, has, b_run, b_start;            /* the sub-block heard in it; it sounds; the master runs; it starts */
    uint8_t hn;                                 /* has, as this sub-block began (the mix's: the next batch may begin
                                                   on the second core before it) */
    uint8_t dup;                                /* DUCK's attack: its ticks left (in the padding before dfol) */
    int32_t dfol;                               /* DUCK's follower of the kick (the mod tick's) */
} SdDrums;

static void sd_dr_pat0(SdDrumPat *p)            /* the lab's beat */
{
    static const uint8_t B[SD_DRS][SD_DR_STEPS] = {
        {2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 1},
        {1, 0, 1, 0, 1, 0, 2, 0, 1, 0, 1, 0, 1, 0, 2, 1}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}};
    uint32_t d, i;
    for (d = 0; d < SD_DRS; d++) for (i = 0; i < SD_DR_STEPS; i++) sd_dr_step_set(p, d, i, B[d][i]);
}
#endif
