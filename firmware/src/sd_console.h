/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* CONSOLE (W1, 2026-11-23): the FM-1's state for the site's console page (web/console/), a frame at a time. Read
 * only: nothing is written, the sound never changes (the renders are the same with it on or off).
 *
 * Over USB MIDI (stardistroyer.c's sd_diag_service, the diagnosis's SysEx, 7D the non-commercial ID):
 *   F0 7D 53 44 07 01 F7   start, or go on: frames for 3 s more (the page asks again each second)
 *   F0 7D 53 44 07 00 F7   stop
 *   F0 7D 53 44 08 <the frame> F7, 25 a second, from the main loop (never the audio interrupt), and only when the
 *   last one has left: a host that stops reading stops the frames, the FM-1 never waits for it.
 * In a browser (web/app/web_host.c's web_console) the page's player hands over the same frames, 30 a second.
 *
 * A frame: 7-bit bytes, a 14-bit value as two (high, low), under SD_CON_MAX so the USB queue (64 packets of 3)
 * takes it whole. FAST every frame, then one SLOW part in turn (its number in FAST's byte 2), each six frames:
 * web/console/frame.js reads it, field for field in this order; the layout's version is byte 0.
 *
 *  FAST  version, count, slow part, flags A (PLAY, REC, ARP, ORBIT, RADAR, FREEZE, LOOP playing), flags B (the mode
 *        in bits 0-1, MIDI clock, MASTER B, SEQ A on, SEQ B on, looper armed), flags C (REACTOR DISTROY TIME SPACE
 *        FILTER SUB COURSE on), the tempo x 10 (14), CPU and its worst since the start (0.1 %, 14 each; 16383 not
 *        known), core 2's wait (0.1 %, 14), the page, the voices (4: the note, 0 silent; then each one's level),
 *        NAV X and Y (14 each), XM WARP FOLD DETUNE, the Reactor's place (14, x 256 across its stages) and stage,
 *        DISTROY's four bands' levels (dB + 64) and its three splits (MIDI), the echoes, the looper's head (14,
 *        0..4095 round its loop), the plate's level, the output's peaks L and R (14), the limiter's gain (0..127,
 *        127 open), the four macros, RADAR's 16 bits (7, 7, 2), its place and its value, LFO 1 and 2 (signed + 64),
 *        ENV 1-3 of the last voice, the twelve slots' value now (source x AMOUNT, signed + 64), SEQ A's and B's step
 *        now and note now, ORBIT's step and its loops so far, ARP's note now (0 none), the wheel, aftertouch
 *  SLOW  0 the sound: the patch's number and its name (8), ALGORITHM, HOME's DARK (0..127), COURSE, the engines off,
 *        the Reactor's model, RES, MIX and stages in use, DISTROY's shapers (4), the crossover, its loop's FDBK,
 *        TAME, MIX, the delay's TIME (14), its mode (SYNC, PP, SHIMMER), FDBK, LEVEL, the plate's SIZE, DECAY,
 *        LEVEL, FREEZE PITCH (+ 64), the looper's LENGTH (14), SPEED (+ 64), its level, the LFOs' shapes, modes
 *        and rates (14), ENV 1-3's A D S R, RADAR's LENGTH, DIV, SEARCH and OFFSET (+ 100), the filter's type,
 *        CUTOFF and RES, SUB's level and what it follows, WIDTH, the key (0-11), the scale
 *        1 MOD: twelve slots: SOURCE, DEST (a matrix one, or 110 + RADAR's own: 0x80 + n), AMOUNT (14, + 8192),
 *          OFFSET (14, + 100)
 *        2, 3 SEQ A, SEQ B: on, playing in, LENGTH, DIV, the bank shown (16 steps from bank x 16), the segments
 *          and where they start (5), then the bank's 16 steps: the note, the type | (VEL / 32) << 3
 *        4 ORBIT: on, LENGTH, END, then each step's LOOPS, SEQ A's pattern, SEQ B's pattern (16 x 3)
 *        5 ARP: its mode, OCTAVES, DIV, how many notes, the notes (8); then each bank's four oscillators: the
 *          ratio and the level (/ 512) (4 x 4 x 2)
 */
#define SD_CON_VER 2u                          /* (2: twelve MOD slots, RADAR's own DESTs at 110, 2026-11-25) */
#define SD_CON_MAX 180u                         /* (its bytes with F0 7D 53 44 08 and F7: 186 <= 64 packets x 3) */
#define SD_CON_SLOWS 6u
static uint32_t con_n;                          /* the frames made: the slow part in turn */
static volatile int32_t con_pk[2];              /* the output's peaks since the last frame (app_audio, while on) */
static volatile uint32_t con_until;             /* fm1_ms the frames stop at (0: off) */

static uint8_t *con_b(uint8_t *o, int32_t v) { *o++ = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v); return o; }
static uint8_t *con_w(uint8_t *o, int32_t v)
{
    v = v < 0 ? 0 : v > 16383 ? 16383 : v;
    *o++ = (uint8_t)(v >> 7); *o++ = (uint8_t)(v & 127);
    return o;
}
static uint8_t *con_s(uint8_t *o, int32_t v) { return con_b(o, v + 64); }   /* (a signed 7-bit) */
static int32_t con_db(int32_t q8) { return q8 / 256 + 64; }   /* (DISTROY's band levels, dB x 256: -64 .. +63) */
static void con_peak(const int32_t *out, uint32_t n)   /* app_audio, while the frames run */
{
    int32_t a = con_pk[0], b = con_pk[1];
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t l = out[2u * i], r = out[2u * i + 1u];
        l = l < 0 ? -l : l; r = r < 0 ? -r : r;
        if (l > a) a = l;
        if (r > b) b = r;
    }
    con_pk[0] = a; con_pk[1] = b;
}

static uint32_t sd_con_frame(uint8_t *o0)       /* the next frame into o0 (SD_CON_MAX): its length */
{
    uint8_t *o = o0;
    const SdPatch *p = &eng.patch;
    uint32_t sl = con_n % SD_CON_SLOWS, i, k, tempo;
    const SdVoice *lv = &eng.voice[eng.m_last < SD_VOICES ? eng.m_last : 0u];
    con_n++;
    /* FAST */
    o = con_b(o, SD_CON_VER);
    o = con_b(o, (int32_t)(con_n & 127u));
    o = con_b(o, (int32_t)sl);
    o = con_b(o, (int32_t)((eng.seq_run ? 1u : 0u) | (seq_rec ? 2u : 0u) | (eng.arp_on ? 4u : 0u) |
                           (eng.orb.on && eng.orb_live ? 8u : 0u) | (SD_TUR_ON(p) ? 16u : 0u) | (sp_frz ? 32u : 0u) |
                           (eng.sp.pon ? 64u : 0u)));
    o = con_b(o, (int32_t)((set.mode & 3u) | (eng.clk_on ? 4u : 0u) | (set.master ? 8u : 0u) |
                           (eng.t[0].seq_on ? 16u : 0u) | (eng.t[1].seq_on ? 32u : 0u) | (eng.lp_arm ? 64u : 0u)));
    o = con_b(o, (int32_t)((eng.r.on ? 1u : 0u) | (eng.d.on ? 2u : 0u) | (!(p->fx_off & SD_FX_TIME) && p->s_dlevel ? 4u : 0u) |
                           (eng.sp.on ? 8u : 0u) | (p->f_type && !(p->fx_off & SD_FX_FILTER) ? 16u : 0u) |
                           (p->sub_lvl && !(p->fx_off & SD_FX_SUB) ? 32u : 0u) | (SD_COURSE_MODE(p) ? 64u : 0u)));
    tempo = eng.clk_on ? (eng.clk_beat ? (SD_FS * 600u + eng.clk_beat / 2u) / eng.clk_beat : 0u) : 10u * (demo_st ? eng.bpm : set.bpm);
    o = con_w(o, (int32_t)tempo);
    o = con_w(o, cpu_permille == 0xFFFFFFFFu ? 16383 : (int32_t)cpu_permille);
    o = con_w(o, (int32_t)cpu_max);
#ifndef DINGHY_WEB
    o = con_w(o, (int32_t)wait_permille);
#else
    o = con_w(o, 16383);
#endif
    o = con_b(o, (int32_t)page);
    for (i = 0; i < SD_VOICES; i++) {
        const SdVoice *v = &eng.voice[i];
        o = con_b(o, v->env1.stage != SD_ENV_OFF ? v->note : 0);
    }
    for (i = 0; i < SD_VOICES; i++) o = con_b(o, eng.voice[i].env1.lvl >> 23);
    o = con_w(o, eng.nav_x >> 2);
    o = con_w(o, eng.nav_y >> 2);
    o = con_b(o, p->xm >> 9); o = con_b(o, p->warp >> 9); o = con_b(o, p->fold >> 9); o = con_b(o, p->dt >> 9);
    o = con_w(o, eng.r.shown_pos);
    o = con_b(o, eng.r.stage);
    for (i = 0; i < 4u; i++) o = con_b(o, eng.d.on ? con_db(eng.d.ef[i]) : 0);
    for (i = 0; i < 3u; i++) o = con_b(o, p->d_split[i] >> 8);
    o = con_b(o, eng.echo_env >> 8);
    o = con_w(o, eng.lp_at);
    o = con_b(o, eng.sp.peak >> 8);
    for (i = 0; i < 2u; i++) { o = con_w(o, con_pk[i] >> 1); con_pk[i] = 0; }
    o = con_b(o, eng.lim_g >> 8);
    for (i = 0; i < 4u; i++) o = con_b(o, p->mac[i] >> 9);
    o = con_b(o, eng.tur_reg & 127u); o = con_b(o, eng.tur_reg >> 7 & 127u); o = con_b(o, eng.tur_reg >> 14 & 3u);
    o = con_b(o, eng.tur_place == 0xFFu ? 127 : eng.tur_place);
    o = con_b(o, (eng.tur_out + 32768) >> 9);
    o = con_s(o, eng.lfo_out[0] >> 9); o = con_s(o, eng.lfo_out[1] >> 9);
    o = con_b(o, lv->env1.lvl >> 23); o = con_b(o, lv->env2.lvl >> 23); o = con_b(o, lv->env3.lvl >> 23);
    for (i = 0; i < SD_MODS; i++) o = con_s(o, eng.m_shown[i] / 2);
    for (k = 0; k < 2u; k++) {
        const SdSeqT *t = &eng.t[k];
        o = con_b(o, t->seq_shown);
        o = con_b(o, t->seq_note_on ? t->seq_note[0] : 0);
    }
    o = con_b(o, eng.orb_shown);
    o = con_b(o, eng.orb_loops);
    o = con_b(o, eng.arp_on && eng.arp_playing ? eng.arp_cur : 0);
    o = con_b(o, eng.wheel & 127u); o = con_b(o, eng.after & 127u);
    /* SLOW, in turn */
    o = con_b(o, (int32_t)sl);
    if (sl == 0u) {
        const char *nm = patch_name(set.patch);
        o = con_b(o, set.patch);
        for (i = 0; i < 8u; i++) { o = con_b(o, *nm ? *nm : ' '); if (*nm) nm++; }
        o = con_b(o, (int32_t)algo_of(p));
        o = con_b(o, eng.filt >> 9); o = con_b(o, p->course); o = con_b(o, p->fx_off);
        o = con_b(o, p->r_model); o = con_b(o, p->r_res >> 9); o = con_b(o, p->r_mix >> 9); o = con_b(o, SD_R_NST(p));
        for (i = 0; i < 4u; i++) o = con_b(o, p->d_type[i]);
        o = con_b(o, p->d_xover); o = con_b(o, p->d_fb >> 9); o = con_b(o, p->d_tame >> 9); o = con_b(o, p->d_mix >> 9);
        o = con_w(o, p->s_time >> 2); o = con_b(o, p->s_mode); o = con_b(o, p->s_fb >> 9); o = con_b(o, p->s_dlevel >> 9);
        o = con_b(o, p->s_size >> 9); o = con_b(o, p->s_decay >> 9); o = con_b(o, p->s_rlevel >> 9);
        o = con_s(o, eng.sp_mod + eng.sp_orb);
        o = con_w(o, eng.lp_len); o = con_s(o, eng.lp_speed / 64); o = con_b(o, eng.lp_lvl >> 9);
        for (i = 0; i < 2u; i++) { o = con_b(o, p->lfo_shape[i]); o = con_b(o, p->lfo_mode[i]); o = con_w(o, p->lfo_rate[i] >> 2); }
        o = con_b(o, p->e1_a); o = con_b(o, p->e1_d); o = con_b(o, p->e1_s); o = con_b(o, p->e1_r);
        o = con_b(o, p->e2_a); o = con_b(o, p->e2_d); o = con_b(o, p->e2_s); o = con_b(o, p->e2_r);
        o = con_b(o, p->e3_a); o = con_b(o, p->e3_d); o = con_b(o, p->e3_s); o = con_b(o, p->e3_r);
        o = con_b(o, (int32_t)SD_TUR_LEN(p)); o = con_b(o, (int32_t)SD_TUR_DIV(p)); o = con_b(o, (int32_t)SD_TUR_CHANGE(p));
        o = con_b(o, p->tur_off + 100);
        o = con_b(o, p->f_type); o = con_b(o, p->f_cut >> 9); o = con_b(o, p->f_res >> 9);
        o = con_b(o, p->sub_lvl >> 9); o = con_b(o, p->sub_src); o = con_b(o, p->width >> 9);
        o = con_b(o, (int32_t)(sd_pat_key(&eng.t[0].pat) % 12u)); o = con_b(o, eng.t[0].pat.scale);
    } else if (sl == 1u) {
        for (i = 0; i < SD_MODS; i++) {
            const SdModSlot *m = &p->mod[i];
            o = con_b(o, m->src);
            o = con_b(o, m->dst >= 0x80u ? 110 + (m->dst - 0x80) : m->dst);
            o = con_w(o, m->amt / 4 + 8192);
            o = con_w(o, p->mod_off[i] + 100);
        }
    } else if (sl <= 3u) {
        const SdSeqT *t = &eng.t[sl - 2u];
        const SdPattern *pt = &t->pat;
        uint32_t bank = t->seq_shown / 16u;
        o = con_b(o, t->seq_on); o = con_b(o, t->in); o = con_b(o, pt->len); o = con_b(o, pt->div);
        o = con_b(o, (int32_t)bank); o = con_b(o, pt->seg_count);
        for (i = 0; i < SD_SEGS - 1u; i++) o = con_b(o, pt->seg_split[i]);
        for (i = 0; i < 16u; i++) {
            const SdStep *s = &pt->step[(bank * 16u + i) % SD_PSTEPS];
            o = con_b(o, s->note);
            o = con_b(o, (int32_t)((s->type & 7u) | (uint32_t)(s->vel >> 5) << 3));
        }
    } else if (sl == 4u) {
        const SdOrbit *ob = &eng.orb;
        o = con_b(o, ob->on); o = con_b(o, (int32_t)SD_ORB_LEN(ob)); o = con_b(o, ob->end);
        for (i = 0; i < SD_ORB_STEPS; i++) { o = con_b(o, ob->loops[i]); o = con_b(o, ob->pat[0][i]); o = con_b(o, ob->pat[1][i]); }
    } else {
        o = con_b(o, eng.arp_mode); o = con_b(o, eng.arp_oct); o = con_b(o, eng.arp_div); o = con_b(o, eng.arp_n);
        for (i = 0; i < 8u; i++) o = con_b(o, i < eng.arp_n ? eng.arp_note[i] : 0);
        for (k = 0; k < SD_BANKS; k++)
            for (i = 0; i < SD_OPS; i++) {
                o = con_b(o, p->bank[k].osc[i].harm);
                o = con_b(o, p->bank[k].osc[i].level >> 9);
            }
    }
    return (uint32_t)(o - o0);
}
