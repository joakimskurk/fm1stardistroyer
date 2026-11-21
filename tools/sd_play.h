/* SPDX-License-Identifier: GPL-3.0-only */
/* One FM-1 played by one person: the engine, the app's pages and controls, and the rules a performance must keep
 * (shared by tools/sd_range.c and tools/sd_song.c). A score calls these each block; whatever it does goes through
 * what the app does with a button, a knob, PRESETS or a key, and the tool stops (fail) at anything the FM-1 could
 * not do: a knob of another page, a knob turned faster than a hand turns it, two buttons closer than a hand presses
 * them, a key off the keyboard or with no voice, ARP or SEQ B where the voice rules forbid them, PLAY before a mode
 * change. The output is the engine's through MASTER as platform.c applies it. Include after sd_engine.h, sd_gen.h,
 * <math.h>, <stdio.h>, <stdlib.h>, <string.h>. */
#ifndef SD_PLAY_H
#define SD_PLAY_H

#define BLOCK 256u                              /* the firmware's interrupt: knobs reach the engine once a block */
#define SD_PLAY_SETTLE 40u                      /* the renders before the score (not heard) */
#ifdef SD_PLAY_REC                              /* recorded for the FM-1's demo (tools/sd_demo_rec.c): before each
                                                   render, what the score did to the engine; what the player says */
static void rec_render(int end);
static void rec_say(const char *what);
#define SD_PLAY_RENDER_HOOK(end) rec_render(end)
#else
#define SD_PLAY_RENDER_HOOK(end) ((void)0)
#endif
#ifndef SD_PLAY_NOTE
#define SD_PLAY_NOTE sd_engine_note             /* (the recorder logs each key) */
#endif
#define MAXRATE 60.0                            /* detents a second a hand turns at most */

static SdEngine e;
static SdSpaceMem sm;
static SdPatch pat;                             /* the patch as the knobs left it (the app's pat) */
static int dirty;
static uint32_t frame;                          /* frames rendered */
static double q = -1e-6;                        /* sixteenths since the start (the tempo's clock) */
static double pq;                               /* ... at the block before */
static uint32_t bpm = 112;
static uint32_t gen = 0xFFFFFFFFu, gen_seed = 1;
static uint8_t held[27];                        /* keys down: their notes */
static int oct;                                 /* OCT- / OCT+ */
static uint32_t mode = SD_MODE_MONO;            /* SEL's */
static int32_t master = 4092;                   /* the MASTER pot, Q12 (platform.c: (k10 x k10) >> 8, ~1 full) */

/* ---- the pages (only the page shown takes K1..K4, ALGORITHM, EDIT) */
enum { PG_NAV, PG_MACROS, PG_REACTOR, PG_DISTROY, PG_SPACE, PG_ENV, PG_FILTER, PG_ARP, PG_LFO1, PG_LFO2, PG_TURING,
       PG_MOD, PG_TIME, PG_LOOP, PG_SETUP, PG_SUB, PG_SEQA, PG_SEQB, PG_ORB, PG_DRUM };   /* (SEQ last: page >= PG_SEQA is SEQ,
                                                   ORBIT its third, DRUM its fourth) */
static const char *const PG_NAME[] = {"NAV", "MACROS", "REACTOR", "DISTROY", "SPACE", "ENV", "FILTER", "ARP", "LFO 1",
                                      "LFO 2", "RADAR", "MOD", "TIME", "LOOPER", "SETUP", "SUB", "SEQ A", "SEQ B", "ORBIT",
                                      "DRUM"};
static int page = PG_NAV;
static int fx_last = PG_REACTOR, env_last = PG_NAV, lfo_last = PG_LFO1, arp_last = PG_ARP, seq_last = PG_SEQA;   /* each
                                                   page button: the page it showed last, where it comes back to from another */

static void say(const char *what)
{
#ifdef SD_PLAY_REC
    rec_say(what);
#endif
    printf("%7.2f s  bar %5.2f  %-8s %s\n", frame / (double)SD_FS, q / 16.0, PG_NAME[page], what);
}
static void fail(const char *why)
{
    printf("NOT PLAYABLE at %.2f s (bar %.2f): %s\n", frame / (double)SD_FS, q / 16.0, why);
    exit(1);
}
static void on_page(int p, const char *what) { if (page != p) fail(what); }
static int at(double s) { return pq < s && q >= s; }   /* the clock crossed s (sixteenths) this block */

/* a button: the page it leads to, as the app's buttons do: on its pages the next, from another the one it showed last
   (HOME: MACROS) */
static int button_page(const char *b)
{
    if (!strcmp(b, "HOME")) return page == PG_MACROS ? PG_SETUP : PG_MACROS;   /* (MACROS, SETUP; NAV is ENV's since
                                                   2026-11-21) */
    if (!strcmp(b, "FX")) return page == PG_REACTOR ? PG_DISTROY : page == PG_DISTROY ? PG_TIME : page == PG_TIME ? PG_SPACE :
                                 page == PG_SPACE ? PG_REACTOR : fx_last;   /* (TIME before SPACE since 2026-11-21) */
    if (!strcmp(b, "ENV")) return page == PG_NAV ? PG_FILTER : page == PG_FILTER ? PG_ENV : page == PG_ENV ? PG_SUB :
                                  page == PG_SUB ? PG_NAV : env_last;   /* (SUB ENV's fourth, FILTER before ENV, since
                                                   2026-11-21) */
    if (!strcmp(b, "LFO")) return page == PG_LFO1 ? PG_LFO2 : page == PG_LFO2 ? PG_TURING : page == PG_TURING ? PG_MOD :
                                  page == PG_MOD ? PG_LFO1 : lfo_last;   /* (RADAR its own tab again since the user found it
                                                   gone as MOD's ninth, 2026-11-19) */
    if (!strcmp(b, "SEQ")) return page == PG_SEQA ? PG_SEQB : page == PG_SEQB ? PG_ORB : page == PG_ORB ? PG_DRUM :
                                  page == PG_DRUM ? PG_SEQA : seq_last;
    if (!strcmp(b, "ARP")) return page == PG_ARP ? PG_LOOP : page == PG_LOOP ? PG_ARP : arp_last;
    return page;
}
static void remember(void)                      /* the page buttons' memory, as the app keeps it */
{
    if (page == PG_REACTOR || page == PG_DISTROY || page == PG_SPACE || page == PG_TIME) fx_last = page;
    if (page == PG_ARP || page == PG_LOOP) arp_last = page;
    if (page == PG_NAV || page == PG_ENV || page == PG_FILTER || page == PG_SUB) env_last = page;
    if (page == PG_LFO1 || page == PG_LFO2 || page == PG_TURING || page == PG_MOD) lfo_last = page;
    if (page >= PG_SEQA) seq_last = page;
}
static uint32_t last_act;                       /* a button, an encoder's click, a PRESETS detent: one hand, in turn */
static void act(void)
{
    if (last_act && frame - last_act < (uint32_t)(SD_FS * 12u / 100u)) fail("two buttons closer than a hand presses them");
    last_act = frame ? frame : 1u;
}
static void press(const char *button, int to)
{
    char t[80];
    act();
    if (button_page(button) != to) {
        snprintf(t, sizeof t, "%s leads to %s from %s, not %s", button, PG_NAME[button_page(button)], PG_NAME[page], PG_NAME[to]);
        fail(t);
    }
    page = to;
    remember();
    say(button);
}
static void next_page(const char *button) { press(button, button_page(button)); }   /* a page button, where it leads */
/* a page button 'step' sixteenths apart from s0 till the page is 'to' (the app's way there: from another page first
   the one it showed last), then no more: each call its s0 (a new pass, the clock from 0 again: anew) */
static void go_every(const char *button, int to, double s0, double step)
{
    static double at_s[96], seen = -1.0;
    static int busy[96], n;
    int k, i;
    if (q < seen) n = 0;
    seen = q;
    for (k = 0; k < n && at_s[k] != s0; k++) {}
    if (k == n) { if (n == 96) fail("too many page moves"); at_s[n] = s0; busy[n++] = 1; }
    for (i = 0; i < 6; i++)
        if (busy[k] && at(s0 + i * step)) {
            if (page != to) next_page(button);
            if (page == to) busy[k] = 0;
        }
}
static void go_to(const char *button, int to, double s0) { go_every(button, to, s0, 1.0); }
static void arp_to(int to, double s0) { go_to("ARP", to, s0); }   /* ARP, a sixteenth apart, till ARP or LOOPER */

/* ---- knobs: a value from where it is to a target over [s0, s1], a detent at a time */
typedef struct { int line; int32_t from; int started; int32_t done; } RampSt;
static RampSt rs[96];
static RampSt *ramp_state(int line)
{
    uint32_t i;
    for (i = 0; i < 96u && rs[i].line; i++) if (rs[i].line == line) return &rs[i];
    if (i == 96u) fail("too many ramps");
    rs[i].line = line;
    return &rs[i];
}
/* the value a knob shows at the clock, in detents from 'from' toward 'to'; checks a hand can turn it so fast */
static int ramp(int line, int32_t cur, int32_t to, double s0, double s1, int32_t *out)
{
    RampSt *r;
    double f, secs;
    if (q < s0 || pq > s1) return 0;
    r = ramp_state(line);
    if (!r->started) {
        r->started = 1;
        r->from = cur;
        secs = (s1 - s0) * 15.0 / bpm;         /* a sixteenth: 15 / BPM seconds */
        if (abs(to - cur) / secs > MAXRATE) fail("a knob turned faster than a hand turns it");
    }
    f = q >= s1 ? 1.0 : (q - s0) / (s1 - s0);
    *out = r->from + (int32_t)lround((to - r->from) * f);
    return *out != cur;
}
static uint32_t d99(uint32_t v) { return (v * 99u + 32767u) / 65535u; }
static uint16_t u99(int32_t d) { d = d < 0 ? 0 : d > 99 ? 99 : d; return (uint16_t)((uint32_t)d * 65535u / 99u); }
#define KNOB99(pg, field, to, s0, s1) do { int32_t v_; on_ramp_page(pg, s0, s1); \
    if (ramp(__LINE__, (int32_t)d99(field), to, s0, s1, &v_)) { field = u99(v_); dirty = 1; } } while (0)
static void on_ramp_page(int pg, double s0, double s1) { if (q >= s0 && pq <= s1) on_page(pg, "a knob of another page"); }
#define HOME_FILTER(to, s0, s1) do { int32_t v_; on_ramp_page(PG_MACROS, s0, s1); \
    if (ramp(__LINE__, (int32_t)d99(65535u - e.filt), to, s0, s1, &v_)) e.filt = (uint16_t)(65535u - u99(v_)); } while (0)
                                                /* (MACROS' K4 FILTER, the player's: 99 open, every cutoff down
                                                   together below it; as the app turns home_filt) */

/* ---- the patch, the patterns */
static void preset(uint32_t i)                  /* PRESETS (not on SEQ) turned to a stock patch, then EDIT to load it
                                                   (since 2026-11-09; one gesture here) */
{
    char t[48];
    if (page >= PG_SEQA) fail("PRESETS on SEQ loads a pattern, not a patch");
    act();
    pat = SD_PATCH[i];
    sd_patch_levels(&pat);                      /* (as the app's patch_load: onto the make-ups) */
    gen = 0xFFFFFFFFu;
    dirty = 1;
    snprintf(t, sizeof t, "PRESETS: P%02u %s", i + 1u, SD_PATCH_NAME[i]);
    say(t);
}
static void preset_user(const SdPatch *p, const char *name)   /* PRESETS past the stock ones: a place of the player's */
{
    char t[48];
    if (page >= PG_SEQA) fail("PRESETS on SEQ loads a pattern, not a patch");
    act();
    pat = *p;
    sd_patch_levels(&pat);
    gen = 0xFFFFFFFFu;
    dirty = 1;
    snprintf(t, sizeof t, "PRESETS: %s", name);
    say(t);
}
static void push(void) { if (dirty) sd_engine_patch(&e, &pat); dirty = 0; }

enum { RS = -1, TI = -2 };                      /* a rest, a tie */
#define SL 0x100                                /* a SLIDE into the note */
#define AC 0x200                                /* an accent (velocity 124) */
static void line(SdPattern *p, const int16_t *n, uint32_t len, uint32_t gate)
{
    uint32_t k;
    sd_pattern_default(p);
    p->len = (uint8_t)len;
    p->seg_count = 1;
    for (k = 0; k < len; k++) {
        SdStep *s = &p->step[k];
        s->gate = (uint8_t)gate;
        s->vel = 92;
        s->ratchet = 1;
        s->prob = 100;
        if (n[k] == RS) { s->type = SD_ST_REST; continue; }
        if (n[k] == TI) { s->type = SD_ST_TIE; continue; }
        s->note = (uint8_t)(n[k] & 0xFF);
        s->type = n[k] & SL ? SD_ST_SLIDE : SD_ST_NOTE;
        if (n[k] & AC) s->vel = 124;
    }
}
static SdPattern pa_cur;                        /* track A's pattern as the player left it */
static void pattern(int trk, const SdPattern *p, const char *name)   /* PRESETS on SEQ: a saved pattern (and EDIT);
                                                   while it plays, in at the one from its step 1 (2026-11-19) */
{
    char t[48];
    on_page(trk ? PG_SEQB : PG_SEQA, "a pattern loaded off its track's page");
    act();
    if (e.seq_run) e.t[trk].pat_wait = 1;
    sd_engine_pattern_to(&e, (uint32_t)trk, p);
    if (!trk) pa_cur = *p;
    snprintf(t, sizeof t, "PRESETS: pattern %s", name);
    say(t);
}
static void xpose(int trk, int semis)           /* GLO held + a key on SEQ: the transpose, at the one (the keys as
                                                   printed: OCT's octave left out) */
{
    char t[48];
    uint32_t key = (uint32_t)(60 + semis), base = 53u;
    if (page != (trk ? PG_SEQB : PG_SEQA)) fail("a transpose off its track's page");
    if (key < base || key > base + 26u) fail("the transpose's key is not on the keyboard");
    act();
    e.t[trk].xpose_next = (int8_t)semis;
    snprintf(t, sizeof t, "GLO + key: SEQ %c transposed %+d at its one", 'A' + trk, semis);
    say(t);
}

/* ---- the keys */
static void key(uint32_t note, int down)
{
    uint32_t base = (uint32_t)(53 + 12 * oct), k = note - base;
    char t[48];
    if (note < base || note > base + 26u) fail("a key that is not on the keyboard");
    if (page >= PG_SEQA) fail("on SEQ the keys are the steps");
    if (down) {
        if (e.seq_run && !e.arp_on && !e.quota[SD_SRC_HAND]) fail("a key with no voice free");
        held[k] = (uint8_t)note;
        SD_PLAY_NOTE(&e, note, 100);
    } else {
        held[k] = 0;
        SD_PLAY_NOTE(&e, note, 0);
    }
    snprintf(t, sizeof t, "key %u %s", note, down ? "down" : "up");
    say(t);
}

/* ---- the transport and the rules */
static void pot(int line, int32_t to, double s0, double s1)   /* MASTER, turned (any time: a pot) */
{
    int32_t v;
    RampSt *r;
    if (q < s0 || pq > s1) return;
    r = ramp_state(line);
    if (!r->started) { r->started = 1; r->from = master; }
    v = r->from + (int32_t)lround((to - r->from) * (q >= s1 ? 1.0 : (q - s0) / (s1 - s0)));
    if (v != master && (q >= s1 || r->from == master)) {
        char t[32];
        snprintf(t, sizeof t, "MASTER %.0f %%", to * 100.0 / 4092.0);
        if (q >= s1) say(t);
    }
    master = v;
}
static void play(int on) { act(); e.seq_run = (uint8_t)on; say(on ? "PLAY" : "PLAY: stop"); }
static void scl(uint32_t to)
{
    static const char *const M[3] = {"MONO", "DUO", "POLY"};
    char t[32];
    act();
    while (mode != to) mode = (mode + 1u) % SD_MODES;   /* (SEL steps MONO DUO POLY: the presses it takes) */
    e.mode = (uint8_t)mode;
    snprintf(t, sizeof t, "SEL: %s", M[mode]);
    say(t);
}
static void rules(void)                         /* the app's voice rules, every block */
{
    uint32_t n = e.nvoices;
    if (e.seq_run && e.mode != e.mode_on) fail("PLAY before the mode changed (the sound must be quiet first)");
    if (e.seq_run && e.arp_on && mode == SD_MODE_MONO && !e.t[0].mute) fail("ARP while SEQ A plays in MONO");
    if (e.seq_run && e.arp_on && !e.quota[SD_SRC_ARP]) fail("ARP with no voice left");
    if (e.seq_run && !e.t[1].mute && e.t[1].width && mode != SD_MODE_MONO &&
        (e.t[0].mute ? 0u : e.t[0].width) >= n) fail("SEQ B with no voice left");
}

/* TURING (LFO's third page, RADAR on the screen, CHANGE its K1 SEARCH): its knobs, as the app turns them; EDIT on /
   off (the first time on, sent nowhere yet: a free MOD slot to XM, 30 %, CHANGE 25 unless set). Where it goes is MOD's since 2026-11-21 (a
   slot, SOURCE RADAR: tur_route); its own DEST and AMOUNT knobs are gone */
#define TUR_CHANGE(to, s0, s1) do { int32_t v_; on_ramp_page(PG_TURING, s0, s1); \
    if (ramp(__LINE__, (int32_t)SD_TUR_CHANGE(&pat), to, s0, s1, &v_)) { \
        pat.tur_set = (uint8_t)((pat.tur_set & 0x80u) | (uint32_t)v_); dirty = 1; } } while (0)
#define TUR_LENGTH(to, s0, s1) do { int32_t v_; on_ramp_page(PG_TURING, s0, s1); \
    if (ramp(__LINE__, (int32_t)SD_TUR_LEN(&pat), to, s0, s1, &v_)) { \
        pat.tur_ld = (uint8_t)((pat.tur_ld & 0xE0u) | (uint32_t)v_); dirty = 1; } } while (0)
static void mod_slot(uint32_t k, uint8_t src, uint8_t dst, int32_t pct);
static void tur_route(uint32_t k, uint8_t dst, int32_t pct)   /* RADAR's route, on MOD: slot k + 1 from RADAR (its
                                                   DEST list: the matrix's, ACCENTS, ROLLS, NOTES) */
{
    mod_slot(k, SD_MS_TUR, dst, pct);
}
static void tur_edit(void)
{
    char t[48];
    on_page(PG_TURING, "TURING's EDIT off its page");
    act();
    {
        uint32_t i, to = pat.tur_dst != 0u;
        pat.tur_set ^= 0x80u;
        for (i = 0; i < SD_MODS; i++) if (pat.mod[i].src == SD_MS_TUR && pat.mod[i].dst) to = 1;
        if (SD_TUR_ON(&pat) && !to) {
            if (!(pat.tur_set & 127u)) pat.tur_set |= 25u;
            for (i = 0; i < SD_MODS && pat.mod[i].src; i++) ;
            if (i < SD_MODS) { pat.mod[i] = (SdModSlot){SD_MS_TUR, SD_MD_XM, (int16_t)(30 * 32767 / 99)}; pat.mod_off[i] = 0; }
        }
    }
    dirty = 1;
    snprintf(t, sizeof t, "EDIT: TURING %s (CHANGE %u)", SD_TUR_ON(&pat) ? "ON" : "OFF", SD_TUR_CHANGE(&pat));
    say(t);
}
static void lfo_to_turing(double s0)            /* LFO till TURING, a sixteenth apart (from another page the one it
                                                   showed last, then LFO 1, LFO 2, TURING, MOD round) */
{
    go_to("LFO", PG_TURING, s0);
}

/* the edit switches, as EDIT on their pages */
/* ORBIT (SEQ's third page): a saved song, PRESETS there (orbit_load; off till EDIT), EDIT on there: in at the
   master's one while it plays (till 2026-11-19 it came with SEQ A's pattern) */
static SdOrbit orb;
static void orbit_load(const SdOrbit *o)
{
    on_page(PG_ORB, "a song loaded off ORBIT's page");
    act();
    orb = *o;
    orb.on = 0;
    if (e.seq_run) e.orb_wait = 1;
    sd_engine_orbit(&e, &orb);
    say("PRESETS: the song");
}
static void orbit_edit(int on)
{
    on_page(PG_ORB, "ORBIT's EDIT off its page");
    act();
    orb.on = (uint8_t)on;
    sd_engine_orbit(&e, &orb);
    say(on ? (e.seq_run ? "EDIT: ORBIT ON (in at A's one)" : "EDIT: ORBIT ON") : "EDIT: ORBIT OFF");
}
static void track_on(int trk, int on)
{
    char t[32];
    on_page(trk ? PG_SEQB : PG_SEQA, "a track's EDIT off its page");
    act();
    e.t[trk].mute = (uint8_t)!on;
    snprintf(t, sizeof t, "EDIT: SEQ %c %s", 'A' + trk, on ? "ON" : "OFF");
    say(t);
}

/* The drums (SEQ's fourth page, DRUM; 2026-11-21 in the performances): the kit, the fourth drum's ENGINE and DUCK as
   the player left them before the take (the app keeps them: drums_ready, the preparation); while it plays a beat from
   PRESETS there (in at the bar's one), EDIT (in at the master's one), the four MUTE keys (F#4 G#4 A#4 C#5, no GLO, on
   DRUM), GLO + ALGORITHM the fourth's ENGINE (its values as chosen: SD_PC_DEF), MIX (GLO + B10): K1..K4 the levels.
   Their batch (the app's, e.dr.ob) only once a performance has drums: the others sound as they did */
static int16_t dr_ob[2u * SD_DR_BATCH * SD_SUB];
static void drums_ready(const SdDrumKit *k, uint32_t perc)
{
    e.dr.ob = dr_ob;
    e.dr.kit = *k;
    e.dr.perc = (uint8_t)(perc % SD_PCS);
    say("(the drums: the kit, the fourth's engine and DUCK as saved)");
}
static void drum_beat(const SdDrumPat *p, const SdDrumMore *m, const char *name)   /* PRESETS on DRUM, a saved beat
                                                   (and EDIT): while it plays, in at the bar's one */
{
    char t[48];
    on_page(PG_DRUM, "a beat loaded off DRUM");
    act();
    e.dr.pat_seq++;
    e.dr.pat_in = *p;
    if (m) e.dr.more_in = *m; else memset(&e.dr.more_in, 0, sizeof e.dr.more_in);
    e.dr.pat_wait = (uint8_t)(e.seq_run != 0);
    e.dr.pat_seq++;
    snprintf(t, sizeof t, "PRESETS: beat %s", name);
    say(t);
}
static void drums_edit(int on)
{
    on_page(PG_DRUM, "the drums' EDIT off DRUM");
    act();
    e.dr.on = (uint8_t)on;
    say(on ? (e.seq_run ? "EDIT: DRUMS ON (in at the master's one)" : "EDIT: DRUMS ON") : "EDIT: DRUMS OFF");
}
static void drum_mute(uint32_t d, int mute)    /* a MUTE key on DRUM */
{
    static const char *const N[SD_DRS] = {"KICK", "SNARE", "HAT", "4TH"};
    char t[40];
    on_page(PG_DRUM, "a drum's MUTE off DRUM");
    act();
    e.dr.mute = (uint8_t)(mute ? e.dr.mute | 1u << d : e.dr.mute & ~(1u << d));
    snprintf(t, sizeof t, "MUTE key: %s %s", N[d % SD_DRS], mute ? "OFF" : "ON");
    say(t);
}
static void drum_engine(uint32_t pc)            /* GLO + ALGORITHM on DRUM: the fourth's ENGINE, its values as chosen */
{
    static const char *const N[SD_PCS] = {"CLAP", "TOM", "RIM", "COW", "ZAP"};
    uint32_t k;
    char t[40];
    on_page(PG_DRUM, "the fourth's ENGINE off DRUM");
    act();
    e.dr.perc = (uint8_t)(pc % SD_PCS);
    for (k = 0; k < 8u; k++) e.dr.kit.d[SD_DR_CLAP].v[k] = SD_PC_DEF[e.dr.perc][k];
    snprintf(t, sizeof t, "GLO + ALGORITHM: PERC %s", N[e.dr.perc]);
    say(t);
}
#define DR_LEVEL(d, to, s0, s1) do { int32_t v_; on_ramp_page(PG_DRUM, s0, s1); \
    if (ramp(__LINE__, (int32_t)e.dr.kit.d[d].level, to, s0, s1, &v_)) e.dr.kit.d[d].level = (uint8_t)v_; } while (0)   /* (MIX's
                                                   K1..K4, a percent a detent) */

/* The looper (2026-11-11; its own page, LOOPER, since 2026-11-14; ARP's second since 2026-11-15), the player's as the app's: LOOP
   (ALGORITHM), REVERSE (GLO + ALGORITHM), GLIDE (K1, OFF or a division a detent), LENGTH (K2, a division a detent),
   SPEED (K3, an eighth of a semitone a detent; GLO + OCT, the next octave, there), L-LEVEL (K4: above 0
   the looper armed; 0 as the app starts).
   Its memory only when a performance asks for it (looper_mem: the FM-1's own, as the app has it since 2026-11-11:
   half the delay line, lent while the looper is armed, and 24 KB, mono mu-law at half rate, 5.1 s); the performances
   before it have none, and sound as they did */
#define LP_BYTES 24576u
static uint8_t lp_mem[LP_BYTES];
static uint8_t lp_on, lp_rev;
static uint16_t lp_len = (uint16_t)(5u * 65536u / SD_LP_NDIV + 100u), lp_lvl;
static int16_t lp_speed;
static uint16_t lp_glide;
static void looper_mem(void) { sd_engine_looper_borrow(&e, lp_mem, LP_BYTES); say("(the looper: half the delay line and 24 KB, 5.1 s)"); }
static void loop_toggle(void)
{
    on_page(PG_LOOP, "LOOP off LOOPER");
    act();
    lp_on = (uint8_t)!lp_on;
    say(lp_on ? "LOOPER ALGORITHM: LOOP ON (the last LENGTH held)" : "LOOPER ALGORITHM: LOOP OFF");
}
static void reverse_toggle(void)
{
    on_page(PG_LOOP, "REVERSE off LOOPER");
    act();
    lp_rev = (uint8_t)!lp_rev;
    say(lp_rev ? "GLO + ALGORITHM: REVERSE ON" : "GLO + ALGORITHM: REVERSE OFF");
}
static void lp_length(uint32_t d)               /* K2 on LOOPER turned to division d (a few detents: one gesture) */
{
    static const char *const N[SD_LP_NDIV] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4",
                                               "1/2T", "1/4D", "1/2", "1 BAR", "2 BAR", "4 BAR"};
    char t[48];
    on_page(PG_LOOP, "LENGTH off LOOPER");
    act();
    lp_len = (uint16_t)(d * 65536u / SD_LP_NDIV + 100u);
    snprintf(t, sizeof t, "LOOPER K2: LENGTH %s", N[d < SD_LP_NDIV ? d : 0]);
    say(t);
}
static void lp_glide_to(uint32_t d)             /* K1 on LOOPER turned to GLIDE's place d (0 OFF, then LENGTH's divisions) */
{
    static const char *const N[SD_LP_NGLIDE] = {"OFF", "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D",
                                                 "1/4", "1/2T", "1/4D", "1/2", "1 BAR", "2 BAR", "4 BAR"};
    char t[48];
    on_page(PG_LOOP, "GLIDE off LOOPER");
    act();
    lp_glide = (uint16_t)(d * 65536u / SD_LP_NGLIDE + (d ? 100u : 0u));
    snprintf(t, sizeof t, "LOOPER K1: GLIDE %s", N[d < SD_LP_NGLIDE ? d : 0]);
    say(t);
}
static void loop_octave(int up)                 /* GLO + OCT- / OCT+ on LOOPER: SPEED to the next octave */
{
    int32_t v = lp_speed;
    on_page(PG_LOOP, "GLO + OCT off LOOPER");
    act();
    v = up ? (v < 0 ? 0 : 3072) : (v > 0 ? 0 : -3072);
    lp_speed = (int16_t)v;
    say(up ? "GLO + OCT+: SPEED AN OCTAVE UP" : "GLO + OCT-: SPEED AN OCTAVE DOWN");
}
#define LP_SPEED(to8, s0, s1) do { int32_t v_; on_ramp_page(PG_LOOP, s0, s1); \
    if (ramp(__LINE__, lp_speed / 32, to8, s0, s1, &v_)) lp_speed = (int16_t)(v_ * 32); } while (0)   /* (eighths) */
#define LP_LEVEL(to, s0, s1) do { int32_t v_; on_ramp_page(PG_LOOP, s0, s1); \
    if (ramp(__LINE__, (int32_t)d99(lp_lvl), to, s0, s1, &v_)) lp_lvl = u99(v_); } while (0)
static void time_div(uint32_t d)                /* K1 on TIME, synced, turned to division d */
{
    on_page(PG_TIME, "TIME off its page");
    act();
    pat.s_time = (uint16_t)(d * 65536u / SD_SP_NDIV + 100u);
    dirty = 1;
    say("K1: TIME");
}
static void freeze_toggle(void)                 /* ALGORITHM on SPACE */
{
    on_page(PG_SPACE, "FREEZE off SPACE");
    act();
    e.sp_freeze = (uint8_t)!e.sp_freeze;
    say(e.sp_freeze ? "ALGORITHM: FREEZE ON" : "ALGORITHM: FREEZE OFF");
}
#define SHIMMER(to, s0, s1) do { int32_t v_; on_ramp_page(PG_SPACE, s0, s1); \
    if (ramp(__LINE__, (int32_t)SD_SP_SHIM(&pat), to, s0, s1, &v_)) { \
        pat.s_mode = (uint8_t)((pat.s_mode & 3u) | (uint32_t)v_ << 2); dirty = 1; } } while (0)   /* (GLO + K1, 0..63) */
static void mod_slot(uint32_t k, uint8_t src, uint8_t dst, int32_t pct)   /* MOD: ALGORITHM to slot k + 1, K1
                                                   SOURCE, K2 DEST (its list), K3 AMOUNT (a percent a detent) */
{
    char t[64];
    on_page(PG_MOD, "a slot off MOD");
    act();
    pat.mod[k] = (SdModSlot){src, dst, (int16_t)(pct * 32767 / 99)};
    dirty = 1;
    snprintf(t, sizeof t, "MOD: SLOT %u, SOURCE, DEST, AMOUNT %+d %%", k + 1u, pct);
    say(t);
}
static void looper_push(void)
{
    e.lp_on = lp_on; e.lp_rev = lp_rev; e.lp_len = lp_len; e.lp_speed = lp_speed; e.lp_glide = lp_glide;
    e.lp_lvl = sd_lp_total(&e) ? lp_lvl : 0u;
}

/* the performance: setup() before the first block (the player's preparation: not heard), score() each block (1: the
 * end, then the render runs until it is quiet) */
static int play_run(const char *path, const char *name, void (*setup)(void), int (*score)(void))
{
    static int32_t buf[2u * BLOCK];
    FILE *f;
    uint32_t i, peak = 0, quiet = 0;
    long data;
    f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0", 1, 24, f);
    {
        uint32_t fs = SD_FS, br = SD_FS * 4u;
        fwrite(&fs, 4, 1, f); fwrite(&br, 4, 1, f);
        fwrite("\x04\0\x10\0data\0\0\0\0", 1, 12, f);
    }
    data = ftell(f);
    sd_engine_init(&e);
    memset(&sm, 0, sizeof sm);
    e.sm = &sm;
    {                                           /* the room beside the frozen plate, as the app has it */
        static SdRoomMem frm;
        e.frm = &frm;
    }
    e.seqb = 1;                                 /* the app: B there, silent until it has notes */
    e.xkey = 0;                                 /* the transposes CHROM (SETUP's XPOSE, set before): these were played
                                                   so, before IN KEY (the app's default since 2026-11-10) */
    e.bpm = (uint8_t)bpm;
    e.mode = SD_MODE_MONO;
    setup();
    last_act = 0;
    push();
    looper_push();
    for (i = 0; i < SD_PLAY_SETTLE; i++) {      /* (settles) */
        SD_PLAY_RENDER_HOOK(0);
        memset(buf, 0, sizeof buf);
        sd_engine_render(&e, buf, BLOCK);
    }
    for (;;) {
        double step = BLOCK * bpm * 4.0 / (60.0 * SD_FS);
        int end;
        uint32_t pk = 0;
        pq = q;
        q += step;
        end = score();
        push();
        looper_push();
        SD_PLAY_RENDER_HOOK(end);
        memset(buf, 0, sizeof buf);             /* (the engine adds into its buffer) */
        sd_engine_render(&e, buf, BLOCK);
        rules();
        for (i = 0; i < 2u * BLOCK; i++) {     /* MASTER as platform.c applies it, then Q15 */
            int32_t m = (buf[i] * master) >> 12, s = m > 32767 ? 32767 : m < -32767 ? -32767 : m;
            int16_t o = (int16_t)s;
            uint32_t a = (uint32_t)(s < 0 ? -s : s), b = (uint32_t)(buf[i] < 0 ? -buf[i] : buf[i]);
            if (a > peak) peak = a;
            if (b > pk) pk = b;
            fwrite(&o, 2, 1, f);
        }
        frame += BLOCK;
        quiet = end && pk < 16u ? quiet + BLOCK : 0u;   /* the end: under -66 dBFS for half a second */
        if (quiet >= SD_FS / 2u) break;
    }
    {
        uint32_t bytes = (uint32_t)(ftell(f) - data), riff = bytes + 36u;
        fseek(f, 4, SEEK_SET); fwrite(&riff, 4, 1, f);
        fseek(f, data - 4, SEEK_SET); fwrite(&bytes, 4, 1, f);
    }
    fclose(f);
    printf("%s -> %s (%.1f s, peak %.1f dBFS)\n", name, path, frame / (double)SD_FS, 20.0 * log10(peak / 32767.0 + 1e-9));
    return 0;
}
#endif
