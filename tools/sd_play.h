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
static uint32_t mode = SD_MODE_MONO;            /* GLO's */
static int32_t master = 4092;                   /* the MASTER pot, Q12 (platform.c: (k10 x k10) >> 8, ~1 full) */

/* ---- the pages (only the page shown takes K1..K4, ALGORITHM, EDIT) */
enum { PG_NAV, PG_MACROS, PG_REACTOR, PG_DESTROY, PG_SPACE, PG_ENV, PG_FILTER, PG_ARP, PG_LFO1, PG_LFO2, PG_TURING,
       PG_MOD, PG_SEQA, PG_SEQB };            /* (SEQ last: page >= PG_SEQA is SEQ) */
static const char *const PG_NAME[] = {"NAV", "MACROS", "REACTOR", "DESTROY", "SPACE", "ENV", "FILTER", "ARP", "LFO 1",
                                      "LFO 2", "TURING", "MOD", "SEQ A", "SEQ B"};
static int page = PG_NAV;

static void say(const char *what)
{
    printf("%7.2f s  bar %5.2f  %-8s %s\n", frame / (double)SD_FS, q / 16.0, PG_NAME[page], what);
}
static void fail(const char *why)
{
    printf("NOT PLAYABLE at %.2f s (bar %.2f): %s\n", frame / (double)SD_FS, q / 16.0, why);
    exit(1);
}
static void on_page(int p, const char *what) { if (page != p) fail(what); }
static int at(double s) { return pq < s && q >= s; }   /* the clock crossed s (sixteenths) this block */

/* a button: the page it leads to, as the app's buttons do */
static uint32_t last_act;                       /* a button, an encoder's click, a PRESETS detent: one hand, in turn */
static void act(void)
{
    if (last_act && frame - last_act < (uint32_t)(SD_FS * 12u / 100u)) fail("two buttons closer than a hand presses them");
    last_act = frame ? frame : 1u;
}
static void press(const char *button, int to) { act(); page = to; say(button); }

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

/* ---- the patch, the patterns */
static void preset(uint32_t i)                  /* PRESETS (not on SEQ): a stock patch */
{
    char t[48];
    if (page >= PG_SEQA) fail("PRESETS on SEQ loads a pattern, not a patch");
    act();
    pat = SD_PATCH[i];
    gen = 0xFFFFFFFFu;
    dirty = 1;
    snprintf(t, sizeof t, "PRESETS: P%02u %s", i + 1u, SD_PATCH_NAME[i]);
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
static void pattern(int trk, const SdPattern *p, const char *name)   /* PRESETS on SEQ: a saved pattern */
{
    char t[48];
    on_page(trk ? PG_SEQB : PG_SEQA, "a pattern loaded off its track's page");
    act();
    sd_engine_pattern_to(&e, (uint32_t)trk, p);
    if (!trk) pa_cur = *p;
    snprintf(t, sizeof t, "PRESETS: pattern %s", name);
    say(t);
}
static void xpose(int trk, int semis)           /* SCL held + a key on SEQ: the transpose, at the one */
{
    char t[48];
    uint32_t key = (uint32_t)(60 + semis), base = (uint32_t)(53 + 12 * oct);
    if (page != (trk ? PG_SEQB : PG_SEQA)) fail("a transpose off its track's page");
    if (key < base || key > base + 26u) fail("the transpose's key is not on the keyboard");
    act();
    e.t[trk].xpose_next = (int8_t)semis;
    snprintf(t, sizeof t, "SCL + key: SEQ %c transposed %+d at its one", 'A' + trk, semis);
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
        sd_engine_note(&e, note, 100);
    } else {
        held[k] = 0;
        sd_engine_note(&e, note, 0);
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
static void glo(uint32_t to)
{
    static const char *const M[3] = {"MONO", "DUO", "POLY"};
    char t[32];
    act();
    while (mode != to) mode = (mode + 1u) % SD_MODES;   /* (GLO steps MONO DUO POLY: the presses it takes) */
    e.mode = (uint8_t)mode;
    snprintf(t, sizeof t, "GLO: %s", M[mode]);
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

/* TURING (LFO's third page): its knobs, as the app turns them; EDIT on / off (the first time on: to XM, 30 %,
   CHANGE 25, unless they are set). DEST turns round its list: n detents from where it is (OFF, then -1: NOTES) */
#define TUR_CHANGE(to, s0, s1) do { int32_t v_; on_ramp_page(PG_TURING, s0, s1); \
    if (ramp(__LINE__, (int32_t)SD_TUR_CHANGE(&pat), to, s0, s1, &v_)) { \
        pat.tur_set = (uint8_t)((pat.tur_set & 0x80u) | (uint32_t)v_); dirty = 1; } } while (0)
#define TUR_LENGTH(to, s0, s1) do { int32_t v_; on_ramp_page(PG_TURING, s0, s1); \
    if (ramp(__LINE__, (int32_t)SD_TUR_LEN(&pat), to, s0, s1, &v_)) { \
        pat.tur_ld = (uint8_t)((pat.tur_ld & 0xE0u) | (uint32_t)v_); dirty = 1; } } while (0)
#define TUR_AMOUNT(to, s0, s1) do { int32_t v_; on_ramp_page(PG_TURING, s0, s1); \
    if (ramp(__LINE__, (int32_t)pat.tur_amt, to, s0, s1, &v_)) { pat.tur_amt = (int8_t)v_; dirty = 1; } } while (0)
#define TUR_DEST_BY(n, s0, s1) tur_dest_by(__LINE__, n, s0, s1)
static void tur_dest_by(int line, int32_t n, double s0, double s1)
{
    RampSt *r;
    int32_t v;
    uint32_t i;
    on_ramp_page(PG_TURING, s0, s1);
    if (q < s0 || pq > s1) return;
    r = ramp_state(line);
    if (!ramp(line, r->done, n, s0, s1, &v)) return;
    i = pat.tur_dst == SD_TD_NOTES ? SD_MDSTS : pat.tur_dst < SD_MDSTS ? pat.tur_dst : 0u;
    i = (uint32_t)(((int32_t)i + (v - r->done) % (int32_t)(SD_MDSTS + 1u) + (int32_t)(SD_MDSTS + 1u)) % (int32_t)(SD_MDSTS + 1u));
    pat.tur_dst = (uint8_t)(i == SD_MDSTS ? SD_TD_NOTES : i);
    r->done = v;
    dirty = 1;
}
static void tur_edit(void)
{
    char t[48];
    on_page(PG_TURING, "TURING's EDIT off its page");
    act();
    pat.tur_set ^= 0x80u;
    if (SD_TUR_ON(&pat) && !pat.tur_dst) {
        pat.tur_dst = SD_MD_XM;
        if (!pat.tur_amt) pat.tur_amt = 30;
        if (!(pat.tur_set & 127u)) pat.tur_set |= 25u;
    }
    dirty = 1;
    snprintf(t, sizeof t, "EDIT: TURING %s (CHANGE %u, AMOUNT %+d)", SD_TUR_ON(&pat) ? "ON" : "OFF", SD_TUR_CHANGE(&pat), pat.tur_amt);
    say(t);
}
static void lfo_to_turing(double s0)            /* LFO three times: LFO 1, LFO 2, TURING (a sixteenth apart) */
{
    if (at(s0)) press("LFO", PG_LFO1);
    if (at(s0 + 1)) press("LFO", PG_LFO2);
    if (at(s0 + 2)) press("LFO", PG_TURING);
}

/* the edit switches, as EDIT on their pages */
static void track_on(int trk, int on)
{
    char t[32];
    on_page(trk ? PG_SEQB : PG_SEQA, "a track's EDIT off its page");
    act();
    e.t[trk].mute = (uint8_t)!on;
    snprintf(t, sizeof t, "EDIT: SEQ %c %s", 'A' + trk, on ? "ON" : "OFF");
    say(t);
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
    e.seqb = 1;                                 /* the app: B there, silent until it has notes */
    e.bpm = (uint8_t)bpm;
    e.mode = SD_MODE_MONO;
    setup();
    last_act = 0;
    push();
    for (i = 0; i < 40u; i++) { memset(buf, 0, sizeof buf); sd_engine_render(&e, buf, BLOCK); }   /* (settles) */
    for (;;) {
        double step = BLOCK * bpm * 4.0 / (60.0 * SD_FS);
        int end;
        uint32_t pk = 0;
        pq = q;
        q += step;
        end = score();
        push();
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
