/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The drums (sd_drumkit.h's kit): four synthesized voices, nothing sampled, and no memory of their own (the snare's
 * and the clap's gated room is shaped noise, not echoes: 28 KB the FM-1 has not got). Their own cost only while they
 * sound: off (the default) or silent, one test a sub-block. On the master's grid (ARP's: its 16ths since START, the
 * master's SWING), in at its one when switched on while it plays. After every engine, before the output's limiter
 * (soft at 0.8 of full scale: a hit never ducks the synth through it). The matrix reaches them (the macros, the
 * level: sd_mod_tick's dr.mod) and they reach it (KICK and SNARE, their envelopes as sources: a sidechain). Included
 * by sd_engine.h after its helpers, before the control tick. */
#ifndef SD_DRUMS_H
#define SD_DRUMS_H

/* a TPT SVF's coefficients from a pitch (MIDI x 256: SD_RG's warp) and its damping k (Q24), as the filter's */
static void sd_dr_fset(SdDrF *f, int32_t c, int32_t k)
{
    int32_t s, fr, g, d;
    c = c < SD_R_LO * 256 ? SD_R_LO * 256 : c > SD_R_HI * 256 - 1 ? SD_R_HI * 256 - 1 : c;
    s = (c >> 8) - SD_R_LO; fr = c & 255;
    g = (int32_t)SD_RG[s] + (int32_t)((((int64_t)SD_RG[s + 1] - SD_RG[s]) * fr) >> 8);
    d = (1 << 24) + (int32_t)(((int64_t)g * (g + k)) >> 24);
    f->a1 = sd_recip30(d);
    f->a2 = (int32_t)(((int64_t)g * f->a1) >> 24);
    f->a3 = (int32_t)(((int64_t)g * f->a2) >> 24);
    f->k = k;
}
SD_INLINE int32_t sd_dr_svf(const SdDrF *f, int32_t *ic, int32_t x, int32_t *hp)   /* x Q19: its band (its high) */
{
    int32_t v3 = x - ic[1];
    int32_t v1 = (int32_t)(((int64_t)f->a1 * ic[0] + (int64_t)f->a2 * v3) >> 30);
    int32_t v2 = ic[1] + (int32_t)(((int64_t)f->a2 * ic[0] + (int64_t)f->a3 * v3) >> 30);
    ic[0] = 2 * v1 - ic[0];
    ic[1] = 2 * v2 - ic[1];
    *hp = x - (int32_t)(((int64_t)f->k * v1) >> 24) - v2;
    return v1;
}
SD_INLINE uint32_t sd_dr_rnd(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }
static uint32_t sd_dr_inc(int32_t cents)  /* a phase increment for a pitch in cents (MIDI x 100) */
{
    uint32_t n, f;
    cents = cents < 0 ? 0 : cents > 12700 ? 12700 : cents;
    n = (uint32_t)cents / 100u; f = (uint32_t)cents % 100u;
    return (uint32_t)(((uint64_t)sd_note_inc(n) * SD_CENTS[99u + f]) >> 16);
}
SD_INLINE int32_t sd_dr_clip(int32_t y) { return (sd_cubic(y * 171) * 3) >> 9; }   /* 1.5 cubic(y / 1.5), Q15 */
static int32_t sd_dr_cl(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
SD_INLINE int32_t sd_dr_env(int32_t *v, int32_t *st, uint32_t idx)   /* a tick of a decay: the per-frame step */
{
    int32_t to = sd_toward(*v, 0, SD_ENV_DEC[idx > 127u ? 127u : idx]);
    *st = (to - *v) >> SD_SUB_SHIFT;
    return to;
}

/* each drum's values now: the kit's, its macro and the matrix's move on it (-100..100 together) over them, kept in
   its ranges. BOOM: longer, lower, more PUNCH and SUB; SIZE: lower, longer, the room up; AIR: longer, higher, more
   metal; CROWD: more hands, wider, a longer tail and room */
static void sd_dr_values(const SdEngine *e, uint32_t d, int32_t *v)
{
    const SdDrumP *p = &e->dr.kit.d[d];
    int32_t m, k;
    for (k = 0; k < 8; k++) v[k] = p->v[k];
    m = sd_dr_cl(p->v[3] + e->dr.mod[d] * 100 / 32767, -100, 100);
    switch (d) {
    case SD_DR_KICK:
        v[0] -= m * 5 / 100; v[1] += m * 14 / 100; v[2] += m * 7 / 100; v[4] += m * 10 / 100;
        v[5] = v[5] * (200 - m) / 200; v[6] += m * 15 / 100; v[7] += m * 40 / 100;
        break;
    case SD_DR_SNARE:
        v[0] -= m * 6 / 100; v[1] += m * 12 / 100; v[4] += m * 10 / 100; v[5] -= m * 5 / 100;
        v[6] += m * 20 / 100; v[7] += m * 50 / 100;
        break;
    case SD_DR_HAT:
        v[0] += m * 4 / 100; v[1] += m * 13 / 100; v[2] += m * 8 / 100; v[4] += m * 30 / 100; v[5] += m * 20 / 100;
        break;
    default:                                    /* the fourth: its engine's macro (CROWD, DEEP, CHAOS, DEPTH, LASER,
                                                   SIZE). CHAOS: faster, wider, noisier, longer; DEPTH: lower, longer,
                                                   slower, more REPEATS fading less; SIZE: more STEPS, a longer TAIL,
                                                   more NOISE and SWEEP */
        switch (e->dr.perc) {
        case SD_PC_TOM: v[0] -= m * 6 / 100; v[1] += m * 12 / 100; v[2] += m * 8 / 100; v[4] += m * 10 / 100; break;
        case SD_PC_DATA: v[1] += m * 10 / 100; v[2] -= m * 3 / 100; v[4] += m * 12 / 100; v[6] += m * 40 / 100; break;
        case SD_PC_SONAR:
            v[0] -= m * 12 / 100; v[1] += m * 12 / 100; v[2] += m * 2 / 100; v[4] += m * 4 / 100; v[6] -= m * 30 / 100;
            break;
        case SD_PC_ZAP: v[2] += m * 24 / 100; v[4] += m * 12 / 100; v[5] += m * 30 / 100; break;
        case SD_PC_SWELL: v[1] += m * 8 / 100; v[2] += m * 15 / 100; v[4] += m * 30 / 100; v[6] += m * 40 / 100; break;
        default:
            v[0] -= m * 4 / 100; v[1] += m * 10 / 100; v[2] += m * 6 / 100; v[4] += (m * 2 + (m < 0 ? -50 : 50)) / 100;
            v[6] += m * 40 / 100; v[7] += m * 40 / 100;
            break;
        }
        v[0] += e->dr.p_tn;                     /* (PERC TUNE, as it was at the hit: RADAR in semitones) */
        break;
    }
    for (k = 0; k < 8; k++)
        if (k != 3) {                           /* (a count or a division: never past its own range) */
            uint32_t kd = sd_dr_kind(d, e->dr.perc, (uint32_t)k), x = (k == 1 || k == 2) && kd != SD_DK_N && kd != SD_DK_DIV;
            v[k] = sd_dr_cl(v[k], sd_dr_min(d, e->dr.perc, (uint32_t)k) - (k == 0 ? (d == SD_DR_CLAP ? 24 : 6) : 0),
                            sd_dr_max(d, e->dr.perc, (uint32_t)k) + (x ? 15 : 0) + (k == 0 && d == SD_DR_CLAP ? 24 : 0));
        }
}

static void sd_dr_hit(SdEngine *e, uint32_t d, uint32_t acc)
{
    SdDrums *r = &e->dr;
    int32_t v[8], vel = acc ? SD_ONE : SD_ONE - (int32_t)(((int64_t)SD_ONE * r->kit.accent * 55) / 10000);
    if (d == SD_DR_CLAP) {                      /* PERC TUNE now, kept for the hit (whole semitones) */
        int32_t t = r->mod[5];
        r->p_tn = (t * 24 + (t < 0 ? -16383 : 16383)) / 32767;
    }
    sd_dr_values(e, d, v);
    r->live |= 1u << d;
    r->hitm |= (uint8_t)(1u << d);
    if (d == SD_DR_CLAP && r->perc != SD_PC_CLAP) {   /* the fourth's other engines: their envelopes from the top
                                                   (SWELL's from nothing), their time and count (DATA's blips,
                                                   SONAR's pings) from 0 */
        r->p_env = vel; r->p_ne = vel; r->p_pe = SD_ONE; r->p_ph = r->p_ph2 = 0;
        r->c_t = 0; r->c_b = 0; r->c_v = vel;
        if (r->perc == SD_PC_SWELL) { r->p_env = 0; r->p_pe = 0; r->p_ne = SD_ONE; }
        return;
    }
    switch (d) {
    case SD_DR_KICK: r->k_env = r->k_sub = r->k_cl = vel; r->k_pe = SD_ONE; r->k_ph = r->k_ph2 = 0; break;
    case SD_DR_SNARE:
        r->s_eb = r->s_en = vel; r->s_bend = SD_ONE; r->s_th = vel; r->s_ph[0] = r->s_ph[2] = 0; r->s_ph[1] = 0x40000000u;
        if (v[7]) { r->g_hold = SD_ENV_MS[v[4] & 127] * 2067u / 1000u; r->g_t = 0; r->g_lvl = (int32_t)((int64_t)vel * v[7] / 100); r->g_note = v[5] - 24; }
        break;
    case SD_DR_HAT: r->h_env = vel; r->h_open = (uint8_t)acc; break;
    default:
        r->c_t = 0; r->c_b = 0; r->c_v = vel; r->c_env = vel;
        if (v[7]) {
            uint32_t h = SD_ENV_MS[v[1] & 127] * 2067u / 1000u;   /* (ticks: its time to -60 dB, as the lab's hold) */
            if (r->g_t >= r->g_hold || h > r->g_hold) { r->g_hold = h; r->g_t = 0; r->g_note = v[0] - 16; }
            r->g_lvl = (int32_t)((int64_t)vel * v[7] / 100);
        }
        break;
    }
}

/* the step at grid g: the grid's 16ths, the master's SWING (two 16ths a pair, the second late) */
static uint32_t sd_dr_at(const SdEngine *e, uint32_t g)
{
    const SdPattern *mp = &e->t[e->lead].pat;
    uint32_t pl = 2u * e->seq_len_q8, sw = mp->swing < 50u ? 50u : mp->swing > 75u ? 75u : mp->swing;
    uint32_t pos = g % pl;
    return (2u * (g / pl) + (pos >= pl / 100u * sw ? 1u : 0u)) % SD_DR_STEPS;
}

static uint32_t sd_dr_frac(const SdEngine *e, uint32_t g)   /* where grid g is in its step, 0..255 (SWING's pairs) */
{
    const SdPattern *mp = &e->t[e->lead].pat;
    uint32_t pl = 2u * e->seq_len_q8, sw = mp->swing < 50u ? 50u : mp->swing > 75u ? 75u : mp->swing;
    uint32_t pos = pl ? g % pl : 0u, a = pl / 100u * sw, st = pos >= a ? a : 0u, len = pos >= a ? pl - a : a;
    return len ? (((pos - st) << 8) / len) & 255u : 0u;   /* (32 bits: a step's frames x 256 x 256 under 2^31 at
                                                   40 BPM; no 64-bit divide in the audio) */
}
static uint32_t sd_dr_chance(SdDrums *r, uint32_t pct)   /* 1 pct % of the time */
{
    r->prng = r->prng * 1664525u + 1013904223u;
    return (r->prng >> 16) % 100u < pct;
}
static void sd_dr_step_hits(SdEngine *e, uint32_t s)   /* step s comes: who plays, how (accent, ROLL), who not
                                                   (MUTE, PROB, a REC hit heard already) */
{
    SdDrums *r = &e->dr;
    uint32_t d, rad = SD_TUR_ON(&e->patch) && (e->tur_reg & 1u);
    int32_t aa = rad ? sd_tur_pct(&e->patch, SD_TD_DACC) : 0, ar = rad ? sd_tur_pct(&e->patch, SD_TD_DROLL) : 0;   /* (its
                                                   own DEST's or its MOD slots') */
    uint32_t racc = aa && sd_dr_chance(r, (uint32_t)(aa < 0 ? -aa : aa)), rroll = ar && sd_dr_chance(r, (uint32_t)(ar < 0 ? -ar : ar));
    for (d = 0; d < SD_DRS; d++) {
        uint32_t st = SD_DR_STEP(&r->pat, d, s), roll = SD_DR_ROLL(&r->more, d, s), pb = SD_DR_PROB(&r->more, d, s);
        uint32_t hit = st == SD_DS_RADAR ? rad : st != SD_DS_OFF, acc = st == SD_DS_ACC || racc;
        r->roll_n[d] = 0;
        if (!hit || (r->mute >> d & 1u)) continue;
        if (r->skip[d] == s + 1u) { r->skip[d] = 0; continue; }
        if (pb) { r->prng = r->prng * 1664525u + 1013904223u; if ((r->prng >> 30) >= 4u - pb) continue; }
        if (rroll && !roll) roll = 1u;
        sd_dr_hit(e, d, acc);
        r->roll_n[d] = (uint8_t)(roll + 1u); r->roll_k[d] = 1; r->roll_a[d] = (uint8_t)acc;
    }
}

/* the fourth's timing: a division of the step (its place, fastest first: SD_SDIV_ORDER) in frames, at the tempo
   heard (120 BPM before there is one); DATA's i-th blip, semitones from TUNE within SPREAD (the first on it; the
   same burst each hit) */
static uint32_t sd_dr_16th(const SdEngine *e)
{
    uint32_t q = e->tempo_q8 ? e->tempo_q8 : e->seq_len_q8 ? e->seq_len_q8 : 1411200u;
    return q >> 8;
}
static uint32_t sd_dr_div(const SdEngine *e, int32_t rank)
{
    uint32_t d = SD_SDIV_ORDER[rank < 0 ? 0 : rank > (int32_t)SD_SDIVS - 1 ? SD_SDIVS - 1u : (uint32_t)rank];
    return sd_dr_16th(e) * SD_SDIV_NUM[d] / SD_SDIV_DEN[d];
}
static int32_t sd_dr_blip(uint32_t i, int32_t spread)
{
    uint32_t h = i * 0x9E3779B9u ^ 0x5BD1E995u;
    if (!i || spread <= 0) return 0;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (int32_t)((h >> 8) % (uint32_t)(2 * spread + 1)) - spread;
}

/* a sub-block of the batch (its grid g, the master running): the steps on the grid, the auditions, each sounding
   drum's coefficients and envelopes for its frames */
static void sd_drum_tick(SdEngine *e, uint32_t g, uint32_t run)
{
    static const int16_t MET[6] = {-6341, -5659, -5323, -4723, -4667, -3986};   /* the six squares, cents from TONE */
    SdDrums *r = &e->dr;
    uint32_t req = r->req, d, k;
    int32_t v[8];
    uint32_t play = r->on && r->in && run && e->seq_len_q8, s = 0;
    if (req) { r->req = 0; for (d = 0; d < SD_DRS; d++) if (req >> d & 1u) sd_dr_hit(e, d, req >> (4u + d) & 1u); }
    if (play) s = sd_dr_at(e, g);
    {                                           /* the main loop's beat: at once, or at the bar's one; the song's
                                                   (its step's beat, or off) at the bar's one, or at once while they
                                                   are not playing */
        uint32_t sq = r->pat_seq, one = !play || (s == 0u && s != r->step);
        uint32_t late = play && s == 0u && r->step == 0u;   /* (still in the bar's first step: a song's beat set at
                                                   the one after this batch was made comes in now) */
        if (!(sq & 1u) && sq != r->pat_taken && (!r->pat_wait || one)) {
            r->pat = r->pat_in;
            r->more = r->more_in;
            r->pat_taken = (uint8_t)sq;
            r->pat_wait = 0;
        }
        if (r->song_due && (one || late)) {
            uint32_t v = r->song_due;
            r->song_due = 0;
            if (v == SD_DR_SONG_OFF) r->on = 0;
            else if (r->pats) {
                SdDrumPat was = r->pat;
                r->pat = r->pats[v - 1u];
                if (r->mores) r->more = r->mores[v - 1u];
                r->pat_taken = r->pat_seq;      /* (an edit of the one before, waiting: not now) */
                r->on = 1;
                if (late && !one)               /* (its step 1 now, where the beat before had nothing: never twice) */
                    for (d = 0; d < SD_DRS; d++) {
                        uint32_t st = SD_DR_STEP(&r->pat, d, 0u);
                        if (SD_DR_STEP(&was, d, 0u) || !st) continue;
                        if (st == SD_DS_RADAR) { if (SD_TUR_ON(&e->patch) && (e->tur_reg & 1u)) sd_dr_hit(e, d, 0); }
                        else sd_dr_hit(e, d, st == SD_DS_ACC);
                    }
            }
            r->song_n = (uint8_t)v;
            r->song_seq++;
            play = r->on && r->in && run && e->seq_len_q8;
        }
    }
    if (!r->on) r->in = 0;
    else if (!r->in && (!e->a_wait || !run || !e->seq_len_q8 || sd_dr_at(e, g) == 0u)) r->in = 1;   /* (on while
                                                   the master plays: in at the bar's one) */
    if (r->on && r->in && run && e->seq_len_q8) {
        if (!play) s = sd_dr_at(e, g);          /* (in just now: its step) */
        if (s != r->step) {
            r->step = (uint8_t)s;
            r->shown = (uint8_t)s;
            sd_dr_step_hits(e, s);
        }
        {                                       /* ROLL: the step's other hits, evenly through it */
            uint32_t fr = sd_dr_frac(e, g);
            r->frac = (uint8_t)fr;
            for (d = 0; d < SD_DRS; d++)
                if (r->roll_n[d] > 1u && r->roll_k[d] < r->roll_n[d] && ((fr * r->roll_n[d]) >> 8) >= r->roll_k[d]) {
                    r->roll_k[d]++;
                    if (!(r->mute >> d & 1u)) sd_dr_hit(e, d, r->roll_a[d]);
                }
        }
    } else {
        r->step = 0xFFu;
        r->shown = 0xFFu;
        for (d = 0; d < SD_DRS; d++) r->roll_n[d] = 0;
    }
    if (!r->live) return;
    {                                           /* the level: MIX's, the matrix's DRUMS LEVEL over it */
        int32_t ml = 32767 + e->dr.mod[4];
        r->bus = ml < 0 ? 0 : ml > 49151 ? 49151 : ml;
    }
    for (d = 0; d < SD_DRS; d++) {             /* each drum's pan and level, equal power (never hard L/R) */
        const SdDrumP *p = &r->kit.d[d];
        uint32_t ph = (uint32_t)((int32_t)sd_dr_cl(p->pan, -60, 60) + 100) * 5368709u;   /* (pan + 100) / 800 of a turn */
        int32_t g = (int32_t)(((int64_t)(p->level > 100u ? 100u : p->level) * 327 * r->bus) >> 15);
        r->gl[d] = (int32_t)(((((int64_t)sd_lut(ph + 0x40000000u) * 46341) >> 15) * g) >> 15);
        r->gr[d] = (int32_t)(((((int64_t)sd_lut(ph) * 46341) >> 15) * g) >> 15);
    }
    if (r->live & 1u << SD_DR_KICK) {          /* KICK: a sine falling PUNCH semitones over SWEEP, its SUB an octave
                                                   down, a click; DRIVE */
        int32_t kd;
        sd_dr_values(e, SD_DR_KICK, v);
        r->k_inc = sd_dr_inc(v[0] * 100 + (int32_t)(((int64_t)v[2] * 100 * r->k_pe) >> 30));
        r->k_pe = sd_toward(r->k_pe, 0, SD_ENV_DEC[v[4] & 127]);
        sd_dr_env(&r->k_env, &r->k_envs, (uint32_t)v[1]);
        sd_dr_env(&r->k_sub, &r->k_subs, (uint32_t)v[1] + 5u);
        sd_dr_env(&r->k_cl, &r->k_cls, 18u);
        r->k_subg = v[7] * 295;
        r->k_clg = v[5] * 229;
        kd = 256 + v[6] * 1536 / 100;
        r->k_drv = kd;
        r->k_nrm = (int32_t)(((int64_t)32767 << 12) / (sd_dr_clip(32767 * kd >> 8) | 1));   /* (Q12: a full sine as
                                                   loud at any DRIVE) */
        if (r->k_env < 1024 && r->k_sub < 1024 && r->k_cl < 1024) r->live &= ~(1u << SD_DR_KICK);
    }
    if (r->live & 1u << SD_DR_SNARE) {         /* SNARE: a body falling into its TUNE, a low thump, noise through TONE */
        int32_t c, b;
        sd_dr_values(e, SD_DR_SNARE, v);
        c = v[0] * 100 + (int32_t)(((int64_t)700 * r->s_bend) >> 30);
        r->s_inc[0] = sd_dr_inc(c); r->s_inc[1] = sd_dr_inc(c + 667); r->s_inc[2] = sd_dr_inc(c - 1200);
        sd_dr_env(&r->s_bend, &r->s_bends, 41u);   /* (the fall and the second tone's level with it, a frame at a time:
                                                   a level stepped a tick buzzed at 1.4 kHz) */
        sd_dr_env(&r->s_eb, &r->s_ebs, (uint32_t)v[1]);
        sd_dr_env(&r->s_en, &r->s_ens, (uint32_t)v[4]);
        sd_dr_env(&r->s_th, &r->s_ths, 50u);
        r->s_body = v[6] * 327;
        r->s_snap = v[2] * 327;
        sd_dr_fset(&r->s_f, v[5] * 256, 20971520);   /* (Q 0.8) */
        b = r->s_eb | r->s_en;
        if (b < 1024 && r->s_eb < 1024 && r->s_en < 1024) r->live &= ~(1u << SD_DR_SNARE);
    }
    if (r->live & 1u << SD_DR_HAT) {           /* HAT: noise and six squares (METAL) through TONE, RES, THIN */
        sd_dr_values(e, SD_DR_HAT, v);
        for (k = 0; k < 6u; k++) r->h_inc[k] = sd_dr_inc(v[0] * 100 + MET[k]);
        sd_dr_env(&r->h_env, &r->h_envs, (uint32_t)(r->h_open ? v[2] : v[1]));
        r->h_met = v[4] * 327;
        sd_dr_fset(&r->h_f1, v[0] * 256, (int32_t)(1677721600 / (70 + v[5] * 7)));   /* (Q 0.7 .. 7.7) */
        sd_dr_fset(&r->h_f2, v[0] * 256 - 1800 + v[7] * 18, 23726566);   /* (THIN: the highpass, 7 semitones
                                                   below TONE .. up to it) */
        r->h_drv = 256 + v[6] * 1280 / 100;
        r->h_nrm = (int32_t)(((int64_t)32767 << 12) / (sd_dr_clip(32767 * r->h_drv >> 8) | 1));
        if (r->h_env < 1024) r->live &= ~(1u << SD_DR_HAT);
    }
    if ((r->live & 1u << SD_DR_CLAP) && r->perc != SD_PC_CLAP) {   /* the fourth's engine: two oscillators (sine to
                                                   a soft square), a pitch falling BEND / RANGE over SWEEP, noise,
                                                   a band filter, DRIVE; DATA, SONAR and SWELL time their own (c_t the
                                                   frames since the hit, c_b the blips, the pings, SWELL's peak) */
        int32_t n0, bend = 0, rb = 0, lvb = 0, sq = 0, nl = 0, nidx = 30, sw = 40, cut = 0, kq = 0, sh = 0, drv;
        int32_t lva = 24576, eidx, keep = 0, own = 0, pdec = 1;
        uint32_t fm = 0;
        sd_dr_values(e, SD_DR_CLAP, v);
        n0 = v[0] * 100;
        eidx = v[1];
        switch (r->perc) {
        case SD_PC_TOM:                         /* a sine falling BEND into TUNE, a fifth above it (RING), a stick */
            bend = v[2] * 100; sw = v[4]; rb = 702; lvb = v[7] * 160; nl = v[5] * 200; nidx = 26;
            break;
        case SD_PC_DATA: {                      /* a blip each RATE, its pitch SPREAD round TUNE, at the burst's level
                                                   (falling over DECAY), each GATE long; NOISE through the blip's band */
            uint32_t per = sd_dr_div(e, v[2]);
            if (r->c_b < 255u && r->c_t >= (r->c_b + 1u) * per) {
                r->c_b++;
                r->p_env = r->p_ne = (int32_t)(((int64_t)r->c_v * r->p_pe) >> 30);
            }
            n0 += sd_dr_blip(r->c_b, v[4]) * 100;
            sw = v[1];                          /* (p_pe: the burst's level) */
            eidx = nidx = 22 + v[5] / 2;
            lva = 32767; rb = 1200; lvb = 5461; sq = v[7] * 200; nl = v[6] * 250;
            fm = 1u; cut = n0 * 256 / 100; kq = 5592405;   /* (Q 3) */
            keep = r->p_pe > SD_ONE / 128;
            break;
        }
        case SD_PC_SONAR: {                     /* a ping, its REPEATS TIME apart, each STEP semitones on and FALL
                                                   quieter (p_pe: its level), a faint partial, a tick of noise */
            uint32_t per = sd_dr_div(e, v[2]);
            if (r->c_b + 1u < (uint32_t)v[4] && r->c_t >= (r->c_b + 1u) * per) {
                r->c_b++;
                r->p_pe = (int32_t)(((int64_t)r->p_pe * ((100 - v[6] * 9 / 10) * 327)) >> 15);
                r->p_env = r->p_ne = (int32_t)(((int64_t)r->c_v * r->p_pe) >> 30);
                r->p_ph = r->p_ph2 = 0;
            }
            n0 += (int32_t)r->c_b * v[5] * 100;
            rb = 1757; lvb = 2600; sq = v[7] * 120; nl = 2400; nidx = 18;
            keep = r->c_b + 1u < (uint32_t)v[4];
            pdec = 0;
            break;
        }
        case SD_PC_SWELL: {                     /* up for STEPS steps as a cube (slow, then fast: a hit played
                                                   backwards), the band rising SWEEP with it, NOISE against the tone
                                                   and its fifth; at the peak the TAIL (p_pe: how far up, 0..1) */
            int32_t q, to;
            if (!r->c_b) {
                uint32_t n = sd_dr_16th(e) * (uint32_t)v[1] / SD_SUB;
                r->p_pe += n ? (int32_t)((uint32_t)SD_ONE / n) : SD_ONE;
                if (r->p_pe >= SD_ONE) { r->p_pe = SD_ONE; r->c_b = 1; }
            }
            q = r->p_pe >> 15;
            to = (int32_t)(((int64_t)r->c_v * ((((q * q) >> 15) * q) >> 15)) >> 15);
            if (!r->c_b) { r->p_envs = (to - r->p_env) >> SD_SUB_SHIFT; own = 1; keep = 1; }
            else eidx = v[2];
            r->p_ne = SD_ONE; r->p_nes = 0;
            lva = (100 - v[4]) * 245; lvb = lva / 2; rb = 702; nl = v[4] * 600;   /* (the noise twice: a band of it
                                                   is far less than the tone at its peak) */
            fm = 1u; cut = v[0] * 256 - (int32_t)(((int64_t)(SD_ONE - r->p_pe) * v[6] * 123) >> 30);
            kq = (int32_t)(1677721600 / (60 + v[5] * 8));
            pdec = 0;
            break;
        }
        default:                                /* ZAP: a laser, RANGE down over SWEEP, SHAPE to a square, RING */
            bend = v[2] * 100; sw = v[4]; rb = 1902; lvb = v[7] * 160; sq = v[5] * 327;
            break;
        }
        {
            int32_t b = (int32_t)(((int64_t)bend * r->p_pe) >> 30);
            r->p_inc = sd_dr_inc(n0 + b);
            r->p_inc2 = sd_dr_inc(n0 + b + rb);
        }
        if (pdec) r->p_pe = sd_toward(r->p_pe, 0, SD_ENV_DEC[sw & 127]);
        if (!own) sd_dr_env(&r->p_env, &r->p_envs, (uint32_t)eidx);
        if (r->perc != SD_PC_SWELL) sd_dr_env(&r->p_ne, &r->p_nes, (uint32_t)nidx);
        r->p_lva = lva;
        r->p_lvb = lvb; r->p_sqa = sq; r->p_nl = nl; r->p_short = sh; r->p_fm = (uint8_t)fm;
        if (fm) sd_dr_fset(&r->c_f, cut, kq);
        drv = 256 + v[6] * 1536 / 100;
        if (r->perc == SD_PC_DATA || r->perc == SD_PC_SONAR || r->perc == SD_PC_SWELL) drv = 256 + v[7] * 1536 / 100;
        r->p_drv = drv;
        r->p_nrm = (int32_t)(((int64_t)32767 << 12) / (sd_dr_clip(32767 * drv >> 8) | 1));
        r->c_t += SD_SUB;
        if (!keep && r->p_env < 1024 && (r->p_ne < 1024 || r->perc == SD_PC_SWELL)) r->live &= ~(1u << SD_DR_CLAP);   /* (SWELL's
                                                   noise under its one envelope) */
    } else if (r->live & 1u << SD_DR_CLAP) {   /* CLAP: BURSTS of noise SPREAD apart, its TAIL over DECAY, through TONE */
        uint32_t sp;
        sd_dr_values(e, SD_DR_CLAP, v);
        sp = (uint32_t)SD_ENV_MS[v[2] & 127] * 441u / 10u;   /* (frames between hands) */
        if (r->c_b + 1u < (uint32_t)v[4] && r->c_t >= (r->c_b + 1u) * sp) { r->c_b++; r->c_env = r->c_v; }
        r->c_last = (uint8_t)(r->c_b + 1u >= (uint32_t)v[4]);
        sd_dr_env(&r->c_env, &r->c_envs, r->c_last ? (uint32_t)v[1] : (uint32_t)v[2]);
        r->c_g = r->c_last ? 11469 + v[6] * 213 : 32767;
        sd_dr_fset(&r->c_f, v[0] * 256, (int32_t)(1677721600 / (90 + v[5] * 6)));
        r->c_t += SD_SUB;
        if (r->c_last && r->c_env < 1024) r->live &= ~(1u << SD_DR_CLAP);
    }
    if ((r->g_t != 0xFFFFFFFFu && r->g_t < r->g_hold + 60u) || r->g_g > 0) {   /* the gated room: open, held, shut in
                                                   ~25 ms */
        int32_t to = r->g_t < r->g_hold ? r->g_lvl : 0;
        int32_t nx = r->g_t < r->g_hold ? to : sd_toward(r->g_g, 0, SD_ENV_DEC[44]);
        if (r->g_t < 2u) nx = (r->g_g + to) / 2;
        r->g_gs = (nx - r->g_g) >> SD_SUB_SHIFT;
        r->g_t += 1u;
        sd_dr_fset(&r->g_f, r->g_note * 256, 25165824);   /* (wide, Q 0.67: a room, not a tone) */
        if (nx < 1024 && r->g_t >= r->g_hold) { r->g_g = 0; r->g_gs = 0; r->g_t = 0xFFFFFFFFu; r->g_hold = 0; }
        else r->live |= 1u << SD_DRS;
    } else {
        r->live &= ~(1u << SD_DRS);
    }
    r->kenv = r->k_env >> 15;                   /* (the matrix's sources: KICK, SNARE) */
    r->senv = r->s_eb >> 15;
}

/* the frames: each drum in its own loop (its state in registers, into the bus's two sums), then the bus. A sine is
   a parabola refined (no table: the flash's cache is the synth's), the products in 32 bits where they fit (the
   envelopes as Q15 there); the filters' in 64 */
SD_INLINE int32_t sd_dr_sin(uint32_t ph)        /* sin(2 pi ph / 2^32), Q15 (within 0.1 %) */
{
    int32_t x = (int32_t)ph >> 16, a = x < 0 ? -x : x, y = (x * (32768 - a)) >> 13;   /* 4 x (1 - |x|) */
    return y + ((((y * (y < 0 ? -y : y)) >> 15) - y) * 7373 >> 15);   /* + 0.225 (y |y| - y) */
}
#define SD_DR_FMAX 1500000                      /* a filter's output kept to ~3 x full scale (Q19): its products fit
                                                   (not 2^21: x 2 or x 3 of it is a ROM address, build.py's check) */
static void sd_drum_run(SdEngine *e, int16_t *out, uint32_t m)
{
    SdDrums *r = &e->dr;
    int32_t bl[SD_SUB], br[SD_SUB];
    uint32_t i, live = r->live, rnd;
    m = m > SD_SUB ? SD_SUB : m;
    for (i = 0; i < m; i++) bl[i] = br[i] = 0;
    if (live & 1u << SD_DR_KICK) {              /* a sine falling into its pitch, its SUB an octave down, a click */
        uint32_t ph = r->k_ph, ph2 = r->k_ph2, inc = r->k_inc;
        int32_t env = r->k_env, envs = r->k_envs, sub = r->k_sub, subs = r->k_subs, cl = r->k_cl, cls = r->k_cls;
        int32_t subg = r->k_subg, clg = r->k_clg, drv = r->k_drv, nrm = r->k_nrm;
        int32_t gl = r->gl[SD_DR_KICK] >> 2, gr = r->gr[SD_DR_KICK] >> 2;
        rnd = r->rn[SD_DR_KICK];
        for (i = 0; i < m; i++) {
            int32_t x, n;
            rnd = rnd * 1664525u + 1013904223u;
            n = (int32_t)rnd >> 16;
            x = ((sd_dr_sin(ph) * (env >> 15)) >> 15) + ((((sd_dr_sin(ph2) * (sub >> 15)) >> 15) * subg) >> 15) +
                ((((n * (cl >> 15)) >> 15) * clg) >> 15);
            ph += inc; ph2 += inc >> 1;
            env += envs; sub += subs; cl += cls;
            x = (sd_dr_clip(sd_dr_cl(x, -98304, 98304) * drv >> 8) * nrm) >> 12;
            bl[i] += (x * gl) >> 13; br[i] += (x * gr) >> 13;
        }
        r->k_ph = ph; r->k_ph2 = ph2; r->k_env = env; r->k_sub = sub; r->k_cl = cl;
        r->rn[SD_DR_KICK] = rnd;
    }
    if (live & 1u << SD_DR_SNARE) {             /* a body falling into its tune, a low thump, noise through TONE */
        uint32_t p0 = r->s_ph[0], p1 = r->s_ph[1], p2 = r->s_ph[2], i0 = r->s_inc[0], i1 = r->s_inc[1], i2 = r->s_inc[2];
        int32_t eb = r->s_eb, ebs = r->s_ebs, en = r->s_en, ens = r->s_ens, th = r->s_th, ths = r->s_ths;
        int32_t bend = r->s_bend, bends = r->s_bends, sbody = r->s_body, snap = r->s_snap;
        int32_t gl = r->gl[SD_DR_SNARE] >> 2, gr = r->gr[SD_DR_SNARE] >> 2, hp;
        rnd = r->rn[SD_DR_SNARE];
        for (i = 0; i < m; i++) {
            int32_t body, bp, x;
            rnd = rnd * 1664525u + 1013904223u;
            body = (sd_dr_sin(p0) * 22938 + sd_dr_sin(p1) * ((9830 * (bend >> 15)) >> 15) +
                    ((sd_dr_sin(p2) * 19661) >> 15) * (th >> 15)) >> 15;
            p0 += i0; p1 += i1; p2 += i2;
            bp = sd_dr_cl(sd_dr_svf(&r->s_f, r->s_fi, (int32_t)rnd >> 12, &hp), -SD_DR_FMAX, SD_DR_FMAX);
            hp = sd_dr_cl(hp, -SD_DR_FMAX, SD_DR_FMAX);
            x = (((body * (eb >> 15)) >> 15) * sbody) >> 15;
            x += (((((bp * 3 + hp * 2) >> 7) * (en >> 15)) >> 14 >> 1) * snap) >> 14;
            eb += ebs; en += ens; th += ths; bend += bends;
            bl[i] += (x * gl) >> 13; br[i] += (x * gr) >> 13;
        }
        r->s_ph[0] = p0; r->s_ph[1] = p1; r->s_ph[2] = p2;
        r->s_eb = eb; r->s_en = en; r->s_th = th; r->s_bend = bend;
        r->rn[SD_DR_SNARE] = rnd;
    }
    if (live & 1u << SD_DR_HAT) {               /* six squares and noise, METAL between them, two filters */
        uint32_t q0 = r->h_ph[0], q1 = r->h_ph[1], q2 = r->h_ph[2], q3 = r->h_ph[3], q4 = r->h_ph[4], q5 = r->h_ph[5];
        int32_t env = r->h_env, envs = r->h_envs, met = r->h_met, drv = r->h_drv, nrm = r->h_nrm;
        int32_t gl = r->gl[SD_DR_HAT] >> 2, gr = r->gr[SD_DR_HAT] >> 2, hp, h2;
        rnd = r->rn[SD_DR_HAT];
        for (i = 0; i < m; i++) {
            int32_t sq, src, b1, x;
            q0 += r->h_inc[0]; q1 += r->h_inc[1]; q2 += r->h_inc[2]; q3 += r->h_inc[3]; q4 += r->h_inc[4]; q5 += r->h_inc[5];
            sq = 5461 * (6 - 2 * (int32_t)((q0 >> 31) + (q1 >> 31) + (q2 >> 31) + (q3 >> 31) + (q4 >> 31) + (q5 >> 31)));
            rnd = rnd * 1664525u + 1013904223u;
            src = ((((int32_t)rnd >> 16) * (32767 - met)) >> 11) + ((sq * 13 / 8 * met) >> 11);   /* Q19 */
            b1 = sd_dr_svf(&r->h_f1, r->h_fi1, src, &hp);
            (void)sd_dr_svf(&r->h_f2, r->h_fi2, sd_dr_cl(b1 + hp, -SD_DR_FMAX, SD_DR_FMAX) * 5 / 8, &h2);
            x = ((sd_dr_cl(h2, -SD_DR_FMAX, SD_DR_FMAX) >> 6) * (env >> 15)) >> 13;
            x = (sd_dr_clip(sd_dr_cl(x, -98304, 98304) * drv >> 8) * nrm) >> 12;
            env += envs;
            bl[i] += (x * gl) >> 13; br[i] += (x * gr) >> 13;
        }
        r->h_ph[0] = q0; r->h_ph[1] = q1; r->h_ph[2] = q2; r->h_ph[3] = q3; r->h_ph[4] = q4; r->h_ph[5] = q5;
        r->h_env = env;
        r->rn[SD_DR_HAT] = rnd;
    }
    if ((live & 1u << SD_DR_CLAP) && r->perc != SD_PC_CLAP) {   /* the fourth's engine */
        uint32_t pa = r->p_ph, pb = r->p_ph2, ia = r->p_inc, ib = r->p_inc2, fm = r->p_fm;
        int32_t env = r->p_env, envs = r->p_envs, ne = r->p_ne, nes = r->p_nes, lva = r->p_lva, lvb = r->p_lvb;
        int32_t sq = r->p_sqa, nl = r->p_nl, sh = r->p_short, drv = r->p_drv, nrm = r->p_nrm, kq = r->c_f.k;
        int32_t gl = r->gl[SD_DR_CLAP] >> 2, gr = r->gr[SD_DR_CLAP] >> 2, hp;
        rnd = r->rn[SD_DR_CLAP];
        for (i = 0; i < m; i++) {
            int32_t a = sd_dr_sin(pa), b = sd_dr_sin(pb), x, g;
            if (sq) {                           /* (a soft square: the sine driven into the clip, few highs to fold) */
                a += ((sd_dr_clip(sd_dr_cl(a * 3, -98304, 98304)) - a) * sq) >> 15;
                b += ((sd_dr_clip(sd_dr_cl(b * 3, -98304, 98304)) - b) * sq) >> 15;
            }
            pa += ia; pb += ib;
            rnd = rnd * 1664525u + 1013904223u;
            x = ((a * lva) >> 15) + ((b * lvb) >> 15) + (((((int32_t)rnd >> 16) * (ne >> 15)) >> 15) * nl >> 15);
            if (fm) {                           /* the band, its peak at unity */
                int32_t bp = sd_dr_cl(sd_dr_svf(&r->c_f, r->c_fi, sd_dr_cl(x, -98304, 98304) * 16, &hp), -SD_DR_FMAX, SD_DR_FMAX);
                x = (int32_t)(((int64_t)bp * kq) >> 28);
            }
            g = sh ? (((env >> 15) * (32767 - sh)) >> 15) + (((ne >> 15) * sh) >> 15) : env >> 15;
            x = (sd_dr_cl(x, -98304, 98304) * g) >> 15;
            x = (sd_dr_clip(x * drv >> 8) * nrm) >> 12;
            env += envs; ne += nes;
            bl[i] += (x * gl) >> 13; br[i] += (x * gr) >> 13;
        }
        r->p_ph = pa; r->p_ph2 = pb; r->p_env = env; r->p_ne = ne;
        r->rn[SD_DR_CLAP] = rnd;
    } else if (live & 1u << SD_DR_CLAP) {       /* noise through its filter, the bursts in the envelope */
        int32_t env = r->c_env, envs = r->c_envs, cg = r->c_g, gl = r->gl[SD_DR_CLAP] >> 2, gr = r->gr[SD_DR_CLAP] >> 2, hp;
        rnd = r->rn[SD_DR_CLAP];
        for (i = 0; i < m; i++) {
            int32_t bp, x;
            rnd = rnd * 1664525u + 1013904223u;
            bp = sd_dr_cl(sd_dr_svf(&r->c_f, r->c_fi, (int32_t)rnd >> 12, &hp), -SD_DR_FMAX, SD_DR_FMAX);
            x = (((((bp >> 6) * (env >> 15)) >> 15) * cg) >> 13) * 3 / 2;
            env += envs;
            bl[i] += (x * gl) >> 13; br[i] += (x * gr) >> 13;
        }
        r->c_env = env;
        r->rn[SD_DR_CLAP] = rnd;
    }
    if (live & 1u << SD_DRS) {                  /* the gated room: two noises (its width) through one wide filter, its
                                                   low and its band (dark, as a room's tail is), at the snare's place */
        uint32_t rnd2 = r->rnd2;
        int32_t g = r->g_g, gs = r->g_gs, gl = r->gl[SD_DR_SNARE] >> 2, gr = r->gr[SD_DR_SNARE] >> 2, hp;
        rnd = r->rnd;
        for (i = 0; i < m; i++) {
            int32_t a, b, gq = g >> 15;
            rnd = rnd * 1664525u + 1013904223u;
            rnd2 = rnd2 * 1664525u + 1013904223u;
            a = sd_dr_svf(&r->g_f, r->g_fl, (int32_t)rnd >> 13, &hp); a = a * 2 / 5 + r->g_fl[1] * 3 / 10;   /* (ic2:
                                                   twice the low) */
            b = sd_dr_svf(&r->g_f, r->g_fr, (int32_t)rnd2 >> 13, &hp); b = b * 2 / 5 + r->g_fr[1] * 3 / 10;
            a = ((sd_dr_cl(a, -SD_DR_FMAX, SD_DR_FMAX) >> 5) * gq) >> 15;
            b = ((sd_dr_cl(b, -SD_DR_FMAX, SD_DR_FMAX) >> 5) * gq) >> 15;
            bl[i] += (a * gl) >> 13; br[i] += (b * gr) >> 13;
            g += gs;
        }
        r->rnd = rnd; r->rnd2 = rnd2;
        r->g_g = g;
    }
    for (i = 0; i < m; i++) {                   /* the drums' bus: soft at 0.8 of full scale */
        out[2u * i] = (int16_t)(sd_dr_clip(sd_dr_cl(bl[i], -524287, 524287)) * 26214 >> 15);
        out[2u * i + 1u] = (int16_t)(sd_dr_clip(sd_dr_cl(br[i], -524287, 524287)) * 26214 >> 15);
    }
}

/* The batch: the next SD_DR_BATCH sub-blocks, from where the engine stands after the last one heard (its grid on by
 * the grid's step a sub-block; a START due: from 0). On the FM-1 the second core's job after each audio block (the
 * app's sd_c1_job), else at the batch's first control tick, here (a computer: the same sound). What the grid does
 * meanwhile that a step could not foresee (a MIDI START, a pattern's resync) the next batch takes up: a step at most
 * a block late, never twice. sd_drum_begin, then sd_drum_part over the sub-blocks, in order */
#define SD_DR_DONE(n, j) (((n) & 0x0FFFFFFFu) << 4 | (j))
SD_NOINLINE static void sd_drum_begin(SdEngine *e, uint32_t n)
{
    SdDrums *r = &e->dr;
    r->b_n = n;
    const SdSeqT *a = &e->t[e->lead];
    r->b_span = e->seq_len_q8 * 48u;
    r->b_adv = e->grid_adv;
    r->b_start = (uint8_t)(e->seq_run && !a->seq_on && !e->clk_on);   /* (the master starts at the next sub-block)
                                                   */
    r->b_run = (uint8_t)(e->seq_run && (a->seq_on || r->b_start));
    r->b_g = r->b_start ? 0u : e->grid_q8;
    r->has = (uint8_t)(r->ob && (r->on || r->live || r->req || r->song_due || r->pat_seq != r->pat_taken));
}
SD_NOINLINE static void sd_drum_part(SdEngine *e, uint32_t j0, uint32_t j1)
{
    SdDrums *r = &e->dr;
    uint32_t j;
    for (j = j0; j < j1 && j < SD_DR_BATCH; j++) {
        if (r->has) {
            uint32_t g = r->b_g + (r->b_start ? j : j + 1u) * r->b_adv;
            if (r->b_span && g >= r->b_span) g %= r->b_span;
            sd_drum_tick(e, g, r->b_run);
            sd_drum_run(e, r->ob + 2u * SD_SUB * j, SD_SUB);
            r->kj[j] = r->kenv; r->sj[j] = r->senv;
            r->hj[j] = r->hitm; r->hitm = 0;   /* (who was hit: the LFOs' triggers) */
        } else {
            r->kj[j] = r->sj[j] = 0;
            r->hj[j] = 0;
        }
        SD_BARRIER();
        r->done = SD_DR_DONE(r->b_n, j + 1u);
    }
}
#endif
