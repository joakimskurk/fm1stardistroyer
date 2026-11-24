/* SPDX-License-Identifier: GPL-3.0-only */
/* RANGE: the span of one FM-1 running Star Distroyer, in one take (2026-11-24, the user: "only SINGULARITY and a new
 * RANGE on the page, a much better RANGE"; the first, 2026-11-07: "much more versatile, showing the instrument's span;
 * not a long cinematic piece but something that evolves through everything this machine can do; nothing that the
 * synth on the FM-1 cannot do itself"). The first played seven scenes with a stop between each, on the patches of
 * then; this one never stops, and plays the factory collection as the FM-1 has it now (sd_patches.h).
 *
 *   sd_range OUT.wav
 *
 * One FM-1 played by one person, one pass, nothing layered (tools/sd_play.h: one engine, one patch at a time, the
 * app's controls at a hand's speed, the voice rules, MASTER as platform.c applies it). Each patch plays what it is
 * made for: the pluck the arp, the bass the line, the lead the tune, the chord lead the chords, the brass the long
 * chords, the soaring lead the last notes. What sounds under the next is the FM-1's own: the looper holding two bars of the bass
 * (never moved off its pitch: on the looper pitch is speed, and a loop a few semitones off is off the bar too), the
 * plate's FREEZE, the echoes. A minor, 104 BPM; the bass loop's two bars (A, F) are the harmony while it plays:
 *   1-8    PLUCK    STARDUST, MONO, no sequence: the arpeggiator over the hands, Am F C G, HOLD; MORE's DARK opened
 *                   from 30; UP-DN, 1/32, RND
 *   9-16   GROWL    GRAVITY (saved with TIME SYNC) on SEQ A, the drop (16ths, SLIDE, a ratchet, PROB, swing; A and
 *                   F); its filter on (LP24, RES), SUB up, HOME's CUTOFF down, MOVE stepping the nav across its four
 *                   growls, CUTOFF open again; the looper armed, bars 15 and 16 held: the bass under 17-32
 *   17-24  SIGNAL   SIGNAL (saved wet: its plate and echoes up), the lead on SEQ A over the bass loop, written on its
 *                   A and F; the echoes' FDBK up the second time
 *   25-32  CHORDS   POLY, NOVA: Am and F on SEQ A over the bass loop, NOVA's own pumping; AIR; SHIMMER on the plate
 *   33-44  WALL     the loop let go; COLOSSUS (TIME SYNC): the low brass in open fifths, each swelling over two bars,
 *                   ORBIT stepping them Am F C G Am (IN KEY), the nav, MOVE and BREAK with them; BREAK and BODY up,
 *                   RADAR on the FOLD (the steps inside the held chords), the top band CRUSHed, then back
 *   45-52  LAST     the wall's plate frozen on its A, the rest off; SOLAR, the soaring lead, over it: E, B, A (the
 *                   tune's first notes, landing), each note blooming; FREEZE let go */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sd_engine.h"
#include "sd_gen.h"
#include "sd_play.h"

#define BAR(b) (((b) - 1) * 16.0)               /* the start of bar b (from 1), in sixteenths */

/* ---- the patterns the player saved (PRESETS on SEQ loads them), the song, the places */
static SdPattern BASS, LEAD, CHORDS, BRASS;
static SdOrbit ORB;
static SdPatch U01, U02, U03;                   /* SIGNAL with its plate and echoes up; GRAVITY and COLOSSUS with TIME
                                                   SYNC: saved (see patterns()) */

static void line16(SdPattern *p, const int16_t *n, uint32_t len, uint32_t div, uint32_t gate)   /* sd_play.h's
                                                   notation (RS, TI, a note | SL | AC) at a division */
{
    line(p, n, len, gate);
    p->div = (uint8_t)div;
}
static void chords(SdPattern *p, uint32_t len, const uint8_t *root, const int8_t (*ch)[3], uint32_t every, uint32_t rest)
{                                               /* chords: struck every 'every' steps, tied, the last 'rest' of each rested */
    uint32_t k;
    sd_pattern_default(p);
    p->len = (uint8_t)len; p->seg_count = 1;
    for (k = 0; k < len; k++) {
        SdStep *st = &p->step[k];
        uint32_t c = k / every, i = k % every;
        st->gate = 100; st->ratchet = 1; st->prob = 100; st->vel = i ? 92 : 112;
        st->type = i == 0u ? SD_ST_NOTE : i >= every - rest ? SD_ST_REST : SD_ST_TIE;
        st->note = root[c];
        st->chord[0] = ch[c][0]; st->chord[1] = ch[c][1]; st->chord[2] = ch[c][2];
    }
}
static void wet(SdPatch *p, uint32_t rl, uint32_t dc, uint32_t sz, uint32_t dl, uint32_t fb)   /* SPACE opened on its
                                                   pages and saved: R-LEVEL DECAY SIZE (SPACE), D-LEVEL FDBK (TIME) */
{
    p->s_rlevel = u99((int32_t)rl); p->s_decay = u99((int32_t)dc); p->s_size = u99((int32_t)sz);
    p->s_dlevel = u99((int32_t)dl); p->s_fb = u99((int32_t)fb);
}
static void patterns(void)
{
    /* GRAVITY's line, two bars: A, then F (16ths) */
    static const int16_t ba[32] = {45 | AC, 45, 57, 45, RS, 45, 55 | SL, 57, 45 | AC, RS, 57, 45, 52, 45, 55 | SL, 57,
                                   41 | AC, 41, 53, 41, RS, 41, 52 | SL, 53, 41 | AC, RS, 53, 41, 48, 41, 52 | SL, 53};
    /* SIGNAL's lead, four bars in eighths over the loop's A F A F: (.) E D C B, C A G, (.) A C E G, F E C; its first
       eighth a rest: the patch is loaded on the one, after LOOP */
    static const int16_t le[32] = {RS, 76 | AC, TI, 74, 72, TI, 71, TI,   72, TI, TI, TI, 69, TI, 67, TI,
                                   RS, 69 | AC, TI, 72, 76, TI, 79, TI,   77, TI, 76, TI, 72, TI, RS, RS};
    static const uint8_t CR[2] = {57, 53};                       /* NOVA: Am, F (with the loop's A, F) */
    static const int8_t CC[2][3] = {{3, 7, 0}, {4, 7, 0}};
    static const uint8_t BR[1] = {45};                           /* COLOSSUS: A2 E3 A3 E4, open fifths (its metal bank
                                                   adds the ninth; a third under it rubs), two bars, ORBIT moves it */
    static const int8_t BC[1][3] = {{7, 12, 19}};
    static const int8_t XA[5] = {0, -4, 3, -2, 0};               /* Am F C G Am (IN KEY: degrees of A minor) */
    static const int8_t L1[5] = {-30, 25, -15, 40, 0};           /* the nav's X: a bank for each chord */
    static const int8_t MV[5] = {0, 15, 5, 25, 0}, BK[5] = {0, 6, 12, 18, 24};   /* MOVE, BREAK: on top of the knobs */
    uint32_t k;
    line16(&BASS, ba, 32, 2, 45);
    BASS.swing = 55;
    BASS.step[6].ratchet = 2; BASS.step[13].prob = 70; BASS.step[29].prob = 70;
    line16(&LEAD, le, 32, 4, 95);
    LEAD.root = SD_ROOT_SET | 9u;               /* (GLO + SELECT: the key A; its first note is a rest) */
    chords(&CHORDS, 32, CR, CC, 16, 2);           /* a chord a bar, a breath before the next */
    CHORDS.step[8].type = SD_ST_NOTE; CHORDS.step[24].type = SD_ST_NOTE;   /* (struck again on the half) */
    CHORDS.step[8].vel = CHORDS.step[24].vel = 96;
    chords(&BRASS, 32, BR, BC, 32, 2);            /* one chord in two bars: its swell */
    memset(&ORB, 0, sizeof ORB);                /* ORBIT: five steps, a loop of BRASS (two bars) each */
    ORB.len = 5; ORB.glide = SD_OG_L1;
    ORB.dst[0] = SD_MD_X;
    for (k = 0; k < 5u; k++) {
        ORB.xa[k] = XA[k]; ORB.xb[k] = XA[k];
        ORB.lane[0][k] = L1[k];
        ORB.mac[1][k] = MV[k]; ORB.mac[2][k] = BK[k];
    }
    U01 = SD_PATCH[SD_P_SIGNAL]; wet(&U01, 58, 88, 92, 44, 48);
    U02 = SD_PATCH[SD_P_GRAVITY]; U02.s_mode |= SD_SP_SYNC;   /* (TIME's MODE SYNC, its echoes still off: the
                                                   looper's LENGTH reads as TIME's does, so with FREE the held bars
                                                   would turn into 868 ms of them) */
    U03 = SD_PATCH[SD_P_COLOSSUS]; U03.s_mode |= SD_SP_SYNC;
}

/* ---- gestures */
static void fx_to(int to, double s0) { go_to("FX", to, s0); }
static void home_to(int to, double s0) { go_to("HOME", to, s0); }
static void seq_to(int to, double s0) { go_to("SEQ", to, s0); }
static void hand(const uint8_t *n, uint32_t k, double down, double up)   /* keys pressed and let go together */
{
    uint32_t i;
    for (i = 0; i < k; i++) { if (at(down + i * 0.1)) key(n[i], 1); if (at(up + i * 0.1)) key(n[i], 0); }
}
static void arp_on(double s, uint32_t mode, uint32_t div, uint32_t octs, uint32_t gate)   /* EDIT on ARP, its knobs,
                                                   HOLD: a sixteenth each */
{
    char t[64];
    if (at(s)) {
        on_page(PG_ARP, "ARP's EDIT off its page");
        if (e.t[0].width + (e.t[1].mute ? 0u : e.t[1].width) + 1u > 4u) fail("no voice for ARP");
        act();
        e.arp_on = 1; e.arp_mode = (uint8_t)mode; e.arp_div = (uint8_t)div; e.arp_gate = (uint8_t)gate; e.arp_oct = 1;
        snprintf(t, sizeof t, "EDIT: ARP ON (MODE %u, RATE %u, GATE %u)", mode, div, gate);
        say(t);
    }
    if (at(s + 1)) { act(); e.arp_hold = 1; say("ALGORITHM: HOLD"); }
    if (at(s + 2)) { act(); e.arp_oct = (uint8_t)octs; say("K3: OCTAVES"); }
}
static void arp_set(double s, const char *what, volatile uint8_t *field, uint8_t v)   /* a knob on ARP, a detent or a few */
{
    if (at(s)) { on_page(PG_ARP, "an ARP knob off its page"); act(); *field = v; say(what); }
}
static void arp_off(double s)
{
    if (at(s)) { on_page(PG_ARP, "ARP's EDIT off its page"); act(); e.arp_on = 0; e.arp_hold = 0; say("EDIT: ARP OFF"); }
}
static uint32_t free_slot(void)                 /* MOD: the first slot the patch leaves free */
{
    uint32_t i;
    for (i = 0; i < SD_MODS; i++) if (!pat.mod[i].src) return i;
    fail("no MOD slot free");
    return 0;
}

static void setup(void)
{
    patterns();
    e.xkey = 1;                                 /* the transposes IN KEY: the app's default */
    preset_factory(SD_P_STARDUST);
    page = PG_SEQB; remember();
    e.t[1].mute = 1; say("(SEQ B OFF, as left)");
    page = PG_SEQA; remember();
    pattern(0, &BASS, "GROWL (A F, 16ths)");
    e.t[0].mute = 1; say("(SEQ A OFF, as left)");
    page = PG_ARP; remember();
    e.filt = (uint16_t)(65535u - u99(30)); say("(MORE's DARK at 30, as left)");
    looper_mem();
    master = 1600;
}

static int score(void)
{
    static const uint8_t AM[3] = {57, 60, 64}, F[3] = {57, 60, 65}, C[3] = {60, 64, 67}, G[3] = {59, 62, 67},
                         J[3] = {76, 71, 69};
    int32_t v;
    /* ---- PLUCK: the arp over the hands (Am F C G, two bars each), DARK opened */
    if (at(0)) play(1);
    arp_on(1, SD_AM_UP, 2, 2, 50);
    hand(AM, 3, 4, 6);
    home_to(PG_MACROS, 7);
    HOME_FILTER(86, 8, BAR(3));                 /* MORE's K4: DARK opened, 30 -> 86 */
    hand(F, 3, BAR(3) - 0.4, BAR(3) + 1);
    arp_to(PG_ARP, BAR(4) + 2);                 /* (ARP: ARP, the one it showed last) */
    hand(C, 3, BAR(5) - 0.4, BAR(5) + 1);
    arp_set(BAR(5) + 4, "K1: MODE UP-DN", &e.arp_mode, SD_AM_UPDN);
    hand(G, 3, BAR(7) - 0.4, BAR(7) + 1);
    arp_set(BAR(7) + 1, "K2: RATE 1/32", &e.arp_div, 0);
    arp_set(BAR(7) + 8, "K1: MODE RND", &e.arp_mode, SD_AM_RND);
    arp_set(BAR(8) + 2, "K2: RATE 1/16", &e.arp_div, 2);
    arp_off(BAR(8) + 8);
    /* ---- GROWL: GRAVITY on A, the drop */
    if (at(BAR(8) + 11)) preset_user(&U02, "U02 GRAVITY SYNC");
    pot(__LINE__, 4092, BAR(8) + 10, BAR(9));
    seq_to(PG_SEQA, BAR(8) + 13);
    if (at(BAR(9))) track_on(0, 1);
    go_to("ENV", PG_FILTER, BAR(9) + 2);        /* (ENV: NAV, the one it showed last; FILTER) */
    if (at(BAR(9) + 5)) {                       /* EDIT: the filter on (the app's first time: ENV 3 = ENV 1), LP24 */
        on_page(PG_FILTER, "EDIT off FILTER");
        act();
        pat.fx_off &= (uint8_t)~SD_FX_FILTER;
        pat.f_type = SD_FT_LP24;
        pat.f_cut = 52000;
        pat.e3_a = pat.e1_a; pat.e3_d = pat.e1_d; pat.e3_s = pat.e1_s; pat.e3_r = pat.e1_r;
        dirty = 1;
        say("EDIT: FILTER ON (LP24)");
    }
    KNOB99(PG_FILTER, pat.f_res, 58, BAR(9) + 7, BAR(10) + 2);       /* K2: RES */
    go_to("ENV", PG_SUB, BAR(10) + 3);          /* (ENV, SUB) */
    KNOB99(PG_SUB, pat.sub_lvl, 85, BAR(10) + 5, BAR(10) + 13);      /* SUB's K1: LEVEL, its sine under */
    home_to(PG_MACROS, BAR(10) + 13);
    on_ramp_page(PG_MACROS, BAR(10) + 15, BAR(11) + 12);              /* K4: CUTOFF, the synth's, down ... */
    if (ramp(__LINE__, pat.f_cut / 147, 42000 / 147, BAR(10) + 15, BAR(11) + 12, &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    KNOB99(PG_MACROS, pat.mac[1], 55, BAR(11) + 12, BAR(12) + 10);    /* MOVE: the nav stepping its growls */
    on_ramp_page(PG_MACROS, BAR(12) + 10, BAR(14) + 4);               /* ... and open again, past where it was */
    if (ramp(__LINE__, pat.f_cut / 147, 56000 / 147, BAR(12) + 10, BAR(14) + 4, &v)) { pat.f_cut = (uint16_t)(v * 147); dirty = 1; }
    arp_to(PG_LOOP, BAR(14) + 6);               /* (ARP, LOOPER) */
    if (at(BAR(14) + 9)) lp_length(13);         /* K2: LENGTH 2 BAR */
    LP_LEVEL(40, BAR(14) + 11, BAR(15));        /* K4: L-LEVEL to 40: armed, bars 15 and 16 recorded (it records whatever
                                                   it plays at; 40 for what comes after: each patch's TRIM is in the
                                                   output's gain, the loop's too: it comes back louder under SIGNAL) */
    seq_to(PG_SEQA, BAR(15) + 2);
    if (at(BAR(15) + 4)) pattern(0, &LEAD, "LEAD (A minor, four bars)");   /* (in at A's one: bar 17) */
    arp_to(PG_LOOP, BAR(16) + 8);               /* (ARP: LOOPER, the one it showed last) */
    pot(__LINE__, 1700, BAR(16) + 12, BAR(17) + 1);
    /* ---- SIGNAL: the lead over the bass loop */
    if (at(BAR(17))) loop_toggle();             /* bars 15 and 16 held: A, F */
    if (at(BAR(17) + 1)) preset_user(&U01, "U01 SIGNAL WET");          /* (the lead's first eighth a rest) */
    seq_to(PG_SEQA, BAR(21));
    if (at(BAR(21) + 2)) pattern(0, &CHORDS, "CHORDS (Am F)");         /* (in at A's one: bar 25) */
    fx_to(PG_TIME, BAR(21) + 4);                /* (FX: REACTOR, DISTROY, TIME) */
    KNOB99(PG_TIME, pat.s_fb, 68, BAR(21) + 8, BAR(22) + 8);         /* K2: FDBK */
    KNOB99(PG_TIME, pat.s_fb, 40, BAR(24) + 1, BAR(24) + 9);
    /* ---- CHORDS: POLY, NOVA over the loop */
    if (at(BAR(24) + 13)) scl(SD_MODE_POLY);    /* (the lead's last note gone) */
    if (at(BAR(24) + 14)) preset_factory(SD_P_NOVA);
    pot(__LINE__, 2400, BAR(24) + 14, BAR(25));
    home_to(PG_MACROS, BAR(25) + 2);
    KNOB99B(PG_MACROS, pat.mac_air, 55, BAR(25) + 4, BAR(26) + 12);  /* MORE's K2: AIR */
    fx_to(PG_SPACE, BAR(28));                   /* (FX: TIME, SPACE) */
    SHIMMER(36, BAR(28) + 4, BAR(30));          /* MORE's K1: SHIMMER */
    /* the wall's chords and its song: in at A's one, bar 33 */
    seq_to(PG_ORB, BAR(31));                    /* (SEQ A, SEQ B, ORBIT) */
    if (at(BAR(31) + 4)) orbit_load(&ORB);
    if (at(BAR(31) + 6)) orbit_edit(1);
    seq_to(PG_SEQA, BAR(31) + 8);               /* (DRUM, SEQ A) */
    if (at(BAR(31) + 11)) pattern(0, &BRASS, "BRASS (Am, two bars)");
    arp_to(PG_LOOP, BAR(32) + 2);               /* (ARP: LOOPER) */
    if (at(BAR(32) + 14)) preset_user(&U03, "U03 COLOSSUS SYNC");      /* (the chords' breath) */
    pot(__LINE__, 3200, BAR(32) + 14, BAR(33) + 4);
    /* ---- WALL: the loop let go; COLOSSUS's long chords, ORBIT (bar 33 Am, 35 F, 37 C, 39 G, 41 Am, 43 Am) */
    if (at(BAR(33))) loop_toggle();
    home_to(PG_MACROS, BAR(33) + 2);
    KNOB99(PG_MACROS, pat.mac[2], 40, BAR(33) + 4, BAR(35));         /* BREAK */
    KNOB99(PG_MACROS, pat.mac[3], 45, BAR(35) + 2, BAR(36) + 8);     /* BODY (MORE) */
    go_to("LFO", PG_MOD, BAR(36) + 10);         /* RADAR on the FOLD, 40 %, CHANGE 40 (LFO 1, LFO 2, RADAR, MOD) */
    if (at(BAR(36) + 14.5)) tur_route(free_slot(), SD_MD_FOLD, 40);
    go_to("LFO", PG_RADAR, BAR(37));
    TUR_CHANGE(40, BAR(37) + 4, BAR(37) + 12);
    TUR_LENGTH(8, BAR(37) + 12.5, BAR(37) + 14);
    if (at(BAR(38))) tur_edit();
    fx_to(PG_DISTROY, BAR(39));                 /* (FX: SPACE, the one it showed last; REACTOR, DISTROY) */
    if (at(BAR(39) + 4)) { act(); say("SELECT: band 4 (the top)"); }
    if (at(BAR(39) + 6)) {                      /* BAND's K1: the top band's TYPE to CRUSH */
        on_page(PG_DISTROY, "a shaper off DISTROY");
        act();
        pat.d_type[3] = SD_DS_CRUSH;
        dirty = 1;
        say("K1: TYPE CRUSH (band 4)");
    }
    on_ramp_page(PG_DISTROY, BAR(39) + 7, BAR(41));
    if (ramp(__LINE__, (int32_t)d99(pat.d_gain[3]), 80, BAR(39) + 7, BAR(41), &v)) { pat.d_gain[3] = u99(v); dirty = 1; }
    on_ramp_page(PG_DISTROY, BAR(42), BAR(44) + 2);                   /* ... and back, for the freeze: a clean Am */
    if (ramp(__LINE__, (int32_t)d99(pat.d_gain[3]), 10, BAR(42), BAR(44) + 2, &v)) { pat.d_gain[3] = u99(v); dirty = 1; }
    fx_to(PG_SPACE, BAR(44) + 6);               /* (FX: TIME, SPACE) */
    /* ---- LAST: the wall frozen on its A (before ORBIT's next step), the rest off; SOLAR over it */
    if (at(BAR(44) + 10)) freeze_toggle();
    seq_to(PG_SEQA, BAR(44) + 11);              /* (SEQ: SEQ A, the one it showed last) */
    if (at(BAR(44) + 12)) track_on(0, 0);       /* (the chord's breath: ORBIT's F never heard) */
    seq_to(PG_ORB, BAR(45));                    /* (SEQ B, ORBIT) */
    if (at(BAR(45) + 2)) orbit_edit(0);
    home_to(PG_MACROS, BAR(45) + 9);
    if (at(BAR(45) + 11)) preset_factory(SD_P_SOLAR);
    pot(__LINE__, 2300, BAR(45) + 11, BAR(46));
    hand(J, 1, BAR(46), BAR(46) + 7.5);         /* E5 ... */
    hand(J + 1, 1, BAR(46) + 8, BAR(46) + 15.5);   /* ... B4 ... */
    hand(J + 2, 1, BAR(47), BAR(50));           /* ... A4, held */
    fx_to(PG_SPACE, BAR(49) + 8);
    if (at(BAR(50) + 2)) freeze_toggle();       /* the frozen wall let go with the last note */
    if (at(BAR(50) + 8)) play(0);
    pot(__LINE__, 0, BAR(51), BAR(53));
    return q >= BAR(53);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: sd_range OUT.wav\n"); return 2; }
    bpm = 104;
    return play_run(argv[1], "range", setup, score);
}
