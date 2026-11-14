/* SPDX-License-Identifier: GPL-3.0-only */
/* The lab (web/lab/): the browser's firmware opened up for the patch, so a page can show and set every one of its
 * fields while the firmware plays it. stardistroyer.c includes this at its end, in the browser build only
 * (DINGHY_WEB): it reads and writes the app's own patch (pat), the one the knobs edit, and hands it to the engine as
 * the knobs do. Nothing here is in the FM-1's build; the sound is the firmware's, bit for bit.
 *
 *   web_pat(), web_pat_size()  the patch, its bytes (SdPatch)
 *   web_pat_apply()            the page changed it: to the engine (as a knob does), the screen with it
 *   web_pat_load(i)            a patch of the bank (or a place of the player's, from SD_PATCHES on) as PRESETS + EDIT
 *   web_pat_now(), web_pat_count(), web_pat_name(i)   which one is loaded; how many (the factory's, then the player's)
 *   web_fields(), web_nfields()  the patch's fields, each a LabField (below); built from SdPatch itself (offsetof), so
 *                              the page can never disagree with the struct
 *   web_enum(list, i)          a list's names (LAB_E_*), NULL past its end
 * The sequencer, beside the patch (the lab's SEQUENCER): the two tracks' patterns as SEQ holds them, ORBIT, and the
 * settings the tracks and the arpeggiator play by; edited through the firmware's own functions, handed over as SEQ's
 * keys hand them:
 *   web_seq(k), web_seq_size()  track k's pattern (0 A, 1 B), its bytes (SdPattern)
 *   web_seq_apply(k)           the page wrote it: to the engine (as a SEQ edit does)
 *   web_seq_op(k, op, a, b)    LENGTH (the segments scaled), a segment cut or joined, EVEN, a segment's length,
 *                              RANDOMIZE, CLEAR (LAB_SQ_*), as SEQ's keys do them
 *   web_seq_layout(i)          the offsets in SdStep, SdPattern and SdOrbit the page reads (LAB_SEQ_OFF's order)
 *   web_orb(), web_orb_size(), web_orb_apply()   ORBIT (SEQ A's), its bytes, to the engine
 *   web_lset(i), web_lset_put(i, v)   a setting (LAB_LS_*): the tempo, the voices, ARP, a track on or off; set as the
 *                              buttons set it (ARP on only when it has room, as EDIT on ARP)
 *   web_play(i)                the transport for the screen: running, then track A's and B's step now
 * And what the player sets beside the patch, with what the window draws of it: the looper's LOOP, REVERSE, LENGTH,
 * SPEED, GLIDE and L-LEVEL and SPACE's FREEZE are LAB_LS_* settings too; web_lp(i) the looper as it plays (its head in
 * its loop, its memory's two pieces, LAB_LP_*), web_tur(i) TURING's register and its steps, web_tur_dac(r) a value
 * (What the lab records is web_run's frames as they come out of the firmware: web/lab/lab-worklet.js.) */
#ifndef WEB_LAB_H
#define WEB_LAB_H
#include <stddef.h>

enum { LAB_U8 = 1, LAB_I8, LAB_U16, LAB_I16 };   /* a field's type */
enum {                                           /* how the page shows a value (the raw number always beside it) */
    LAB_RAW, LAB_99, LAB_PCT, LAB_PITCH, LAB_SEMI, LAB_ENUM, LAB_HEX, LAB_MS, LAB_CENTS, LAB_POS, LAB_PAN, LAB_LEVEL
};
enum { LAB_E_NONE, LAB_E_ROLE, LAB_E_SHAPER, LAB_E_RMODEL, LAB_E_RCLOCK, LAB_E_XOVER, LAB_E_LFO, LAB_E_SRC, LAB_E_DST,
       LAB_E_M4, LAB_E_FILTER, LAB_E_ONOFF,
       LAB_E_STYPE = 16, LAB_E_SORDER, LAB_E_SCALE, LAB_E_SDIV, LAB_E_AMODE, LAB_E_MODE, LAB_E_COURSE, LAB_ES };   /* (16 on: the
                                                   sequencer's) */
typedef struct {
    uint16_t off;                                /* in SdPatch */
    uint8_t type, show, list, group;             /* LAB_U8 ..; LAB_RAW ..; LAB_E_* (LAB_ENUM); the page's group */
    int32_t lo, hi;                              /* the range the firmware takes */
    const char *name, *path;                     /* the page's name; the C designator (".bank[0].osc[2].harm") */
} LabField;

#define LAB_NF 400
static LabField lab_f[LAB_NF];
static char lab_names[LAB_NF][20], lab_paths[LAB_NF][28];
static uint32_t lab_n;
static char lab_cp[28];                          /* the next field's C path (lab_path, lab_pathk) */

static void lab_path(const char *member) { char *p = put(lab_cp, "."); p = put(p, member); *p = 0; }
static void lab_path2(const char *a, uint32_t i, const char *b, uint32_t j, const char *c)   /* ".a[i].b[j].c", or
                                                   with b 0 ".a[i].c" */
{
    char *p = put(lab_cp, ".");
    p = put(p, a); *p++ = '['; p = put_u(p, i); *p++ = ']';
    if (b) { *p++ = '.'; p = put(p, b); *p++ = '['; p = put_u(p, j); *p++ = ']'; }
    *p++ = '.'; p = put(p, c); *p = 0;
}
static void lab_cp_k(uint32_t k)                /* "[k]" after the path */
{
    char *p = lab_cp;
    while (*p) p++;
    *p++ = '['; p = put_u(p, k); *p++ = ']'; *p = 0;
}
static void lab_pathk(const char *member, uint32_t k)   /* ".member[k]" */
{
    char *p = put(lab_cp, ".");
    p = put(p, member); *p++ = '['; p = put_u(p, k); *p++ = ']'; *p = 0;
}
static void lab_add(uint32_t group, const char *name, uint32_t off, uint32_t type, uint32_t show, uint32_t list,
                    int32_t lo, int32_t hi)
{
    char *p;
    if (lab_n >= LAB_NF) return;
    p = put(lab_names[lab_n], name);
    *p = 0;
    p = put(lab_paths[lab_n], lab_cp);
    *p = 0;
    lab_f[lab_n] = (LabField){(uint16_t)off, (uint8_t)type, (uint8_t)show, (uint8_t)list, (uint8_t)group, lo, hi,
                              lab_names[lab_n], lab_paths[lab_n]};
    lab_n++;
}
static void lab_addk(uint32_t group, const char *pre, uint32_t k, const char *name, uint32_t off, uint32_t type,
                     uint32_t show, uint32_t list, int32_t lo, int32_t hi)   /* the k-th of something: "pre k name" */
{
    char t[20], *p = t;
    p = put(p, pre);
    p = put_u(p, k);
    *p++ = ' ';
    p = put(p, name);
    *p = 0;
    lab_add(group, t, off, type, show, list, lo, hi);
}
#define F(f) ((uint32_t)offsetof(SdPatch, f))
#define P(f) (lab_path(#f), F(f))                /* a member's offset, its C path set for the next field */
#define PK(f, k) (lab_pathk(#f, k), F(f))         /* an array member's start, its k-th's path set */
#define U8 LAB_U8, LAB_RAW, 0
#define U16 LAB_U16, LAB_RAW, 0

/* the groups, in the page's order (web/lab/lab.js has their titles) */
enum { G_Q, G_NAV, G_BANKA, G_BANKB, G_BANKC, G_BANKD, G_ENV1, G_ENV2, G_FILTER, G_ENV3, G_REACT, G_STAGES, G_DIST,
       G_BANDS, G_LOOP, G_TIME, G_SPACE, G_MOD, G_LFO, G_MACRO, G_TURING };

static void lab_build(void)
{
    static const char *const BANK = "ABCD";
    uint32_t b, o, k, s;
    lab_n = 0;
    /* the Q page, every bank; ENV2 on WP and FD */
    lab_add(G_Q, "XM", P(xm), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_Q, "WARP", P(warp), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_Q, "FOLD", P(fold), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_Q, "DETUNE", P(dt), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_Q, "ENV2 > WARP", P(e2_warp), LAB_I16, LAB_PCT, 0, -32767, 32767);
    lab_add(G_Q, "ENV2 > FOLD", P(e2_fold), LAB_I16, LAB_PCT, 0, -32767, 32767);
    /* the nav, its width, COURSE */
    lab_add(G_NAV, "X", P(x), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_NAV, "Y", P(y), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_NAV, "WIDTH", P(width), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_NAV, "COURSE", P(course), LAB_U8, LAB_ENUM, LAB_E_COURSE, 0, 2);   /* OFF, ON (from each note), LOOP */
    for (k = 0; k < 4u; k++) {
        lab_addk(G_NAV, "CELL ", k + 1u, "X", PK(cell_x, k) + 2u * k, LAB_U16, LAB_99, 0, 0, 65535);
        lab_addk(G_NAV, "CELL ", k + 1u, "Y", PK(cell_y, k) + 2u * k, LAB_U16, LAB_99, 0, 0, 65535);
    }
    for (k = 0; k < 3u; k++) lab_addk(G_NAV, "CELL ", k + 1u, "MS", PK(cell_ms, k) + 2u * k, LAB_U16, LAB_MS, 0, 1, 65535);
    /* the banks: four oscillators each (harmonic, fine, role, level), and the bank's own offsets on the Q page */
    for (b = 0; b < SD_BANKS; b++) {
        uint32_t base = F(bank) + b * (uint32_t)sizeof(SdBankPatch);
        char pre[4] = {BANK[b], 0};
        for (o = 0; o < SD_OPS; o++) {
            uint32_t ob = base + (uint32_t)offsetof(SdBankPatch, osc) + o * (uint32_t)sizeof(SdOscPatch);
            char t[8], *p = t;
            p = put(p, pre);
            p = put_u(p, o + 1u);
            *p++ = ' ';
            *p = 0;
            {
                char n1[20], *q;
                q = put(n1, t); q = put(q, "HARM"); *q = 0;
                lab_path2("bank", b, "osc", o, "harm");
                lab_add(G_BANKA + b, n1, ob + (uint32_t)offsetof(SdOscPatch, harm), LAB_U8, LAB_RAW, 0, 1, 32);
                q = put(n1, t); q = put(q, "FINE"); *q = 0;
                lab_path2("bank", b, "osc", o, "fine");
                lab_add(G_BANKA + b, n1, ob + (uint32_t)offsetof(SdOscPatch, fine), LAB_I8, LAB_CENTS, 0, -99, 99);
                q = put(n1, t); q = put(q, "ROLE"); *q = 0;
                lab_path2("bank", b, "osc", o, "role");
                lab_add(G_BANKA + b, n1, ob + (uint32_t)offsetof(SdOscPatch, role), LAB_U8, LAB_ENUM, LAB_E_ROLE, 0, 2);
                q = put(n1, t); q = put(q, "LEVEL"); *q = 0;
                lab_path2("bank", b, "osc", o, "level");
                lab_add(G_BANKA + b, n1, ob + (uint32_t)offsetof(SdOscPatch, level), LAB_U16, LAB_99, 0, 0, 65535);
            }
        }
        {
            char n1[20], *q;
            static const char *const QN[4] = {"XM", "WARP", "FOLD", "DETUNE"}, *const QP[4] = {"xm", "wp", "fd", "dt"};
            static const uint8_t QO[4] = {offsetof(SdBankPatch, xm), offsetof(SdBankPatch, wp), offsetof(SdBankPatch, fd),
                                          offsetof(SdBankPatch, dt)};
            for (k = 0; k < 4u; k++) {
                q = put(n1, pre); q = put(q, " "); q = put(q, QN[k]); q = put(q, " +"); *q = 0;
                lab_path2("bank", b, 0, 0, QP[k]);
                lab_add(G_BANKA + b, n1, base + QO[k], LAB_I16, LAB_PCT, 0, -32767, 32767);
            }
        }
    }
    /* the envelopes: times 0..127 (1 ms .. 10 s), sustain 0..127 */
    lab_add(G_ENV1, "ATTACK", P(e1_a), U8, 0, 127); lab_add(G_ENV1, "DECAY", P(e1_d), U8, 0, 127);
    lab_add(G_ENV1, "SUSTAIN", P(e1_s), U8, 0, 127); lab_add(G_ENV1, "RELEASE", P(e1_r), U8, 0, 127);
    lab_add(G_ENV2, "ATTACK", P(e2_a), U8, 0, 127); lab_add(G_ENV2, "DECAY", P(e2_d), U8, 0, 127);
    lab_add(G_ENV2, "SUSTAIN", P(e2_s), U8, 0, 127); lab_add(G_ENV2, "RELEASE", P(e2_r), U8, 0, 127);
    lab_add(G_ENV3, "ATTACK", P(e3_a), U8, 0, 127); lab_add(G_ENV3, "DECAY", P(e3_d), U8, 0, 127);
    lab_add(G_ENV3, "SUSTAIN", P(e3_s), U8, 0, 127); lab_add(G_ENV3, "RELEASE", P(e3_r), U8, 0, 127);
    /* the filter */
    lab_add(G_FILTER, "TYPE", P(f_type), LAB_U8, LAB_ENUM, LAB_E_FILTER, 0, 2);
    lab_add(G_FILTER, "CUTOFF", P(f_cut), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_FILTER, "RES", P(f_res), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_FILTER, "ENV", P(f_env), LAB_I16, LAB_PCT, 0, -32767, 32767);
    lab_add(G_FILTER, "KEY %", P(f_key), U8, 0, 100);
    lab_add(G_FILTER, "VEL %", P(f_vel), U8, 0, 100);
    /* the Reactor */
    lab_add(G_REACT, "MODEL", P(r_model), LAB_U8, LAB_ENUM, LAB_E_RMODEL, 0, 5);
    lab_add(G_REACT, "POS", P(r_pos), LAB_U16, LAB_POS, 0, 0, 6 * 256 - 1);
    lab_add(G_REACT, "RES", P(r_res), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_REACT, "SLEW", P(r_slew), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_REACT, "DRIVE", P(r_drive), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_REACT, "MIX", P(r_mix), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_REACT, "FREQ", P(r_freq), LAB_I16, LAB_SEMI, 0, -48 * 256, 48 * 256);
    lab_add(G_REACT, "TRACK", P(r_track), LAB_U8, LAB_HEX, 0, 0, 3);
    lab_add(G_REACT, "LOW", P(r_low), U16, 0, 65535);   /* 0 none, else 50 .. 400 Hz */
    lab_add(G_REACT, "CLOCK DIV", P(r_div), U8, 0, 16);
    lab_add(G_REACT, "CLOCK MODE", P(r_cmode), LAB_U8, LAB_ENUM, LAB_E_RCLOCK, 0, 3);
    lab_add(G_REACT, "CLOCK MIN", P(r_min), U8, 0, 5);
    lab_add(G_REACT, "CLOCK MAX", P(r_max), U8, 0, 5);
    for (k = 0; k < 4u; k++) lab_addk(G_REACT, "PAN ", k + 1u, "", PK(r_pan, k) + 2u * k, LAB_I16, LAB_PAN, 0, -32767, 32767);
    for (s = 0; s < 6u; s++)
        for (k = 0; k < 4u; k++) {
            uint32_t sb = F(stage) + s * (uint32_t)sizeof(SdReactorStage);
            char t[20], *p = t;
            p = put(p, "S"); p = put_u(p, s + 1u); p = put(p, " F"); p = put_u(p, k + 1u); *p = 0;
            {
                char n1[20], *q;
                q = put(n1, t); q = put(q, " PITCH"); *q = 0;
                lab_path2("stage", s, 0, 0, "pitch"); lab_cp_k(k);
                lab_add(G_STAGES, n1, sb + (uint32_t)offsetof(SdReactorStage, pitch) + 2u * k, LAB_U16, LAB_PITCH, 0,
                        SD_R_LO * 256, SD_R_HI * 256);
                q = put(n1, t); q = put(q, " LEVEL"); *q = 0;
                lab_path2("stage", s, 0, 0, "level"); lab_cp_k(k);
                lab_add(G_STAGES, n1, sb + (uint32_t)offsetof(SdReactorStage, level) + 2u * k, LAB_U16, LAB_LEVEL, 0,
                        0, 65535);
            }
        }
    /* DISTROY: the crossovers, the bands, the loop */
    lab_add(G_DIST, "XOVER", P(d_xover), LAB_U8, LAB_ENUM, LAB_E_XOVER, 0, 1);
    for (k = 0; k < 3u; k++) lab_addk(G_DIST, "SPLIT ", k + 1u, "", PK(d_split, k) + 2u * k, LAB_U16, LAB_PITCH, 0,
                                      SD_R_LO * 256, SD_R_HI * 256);
    lab_add(G_DIST, "MIX", P(d_mix), LAB_U16, LAB_99, 0, 0, 65535);
    for (k = 0; k < 4u; k++) {
        lab_addk(G_BANDS, "B", k + 1u, "TYPE", PK(d_type, k) + k, LAB_U8, LAB_ENUM, LAB_E_SHAPER, 0, 4);
        lab_addk(G_BANDS, "B", k + 1u, "DRIVE", PK(d_gain, k) + 2u * k, LAB_U16, LAB_99, 0, 0, 65535);
        lab_addk(G_BANDS, "B", k + 1u, "ALT", PK(d_alt, k) + 2u * k, LAB_U16, LAB_99, 0, 0, 65535);
        lab_addk(G_BANDS, "B", k + 1u, "LEVEL", PK(d_level, k) + 2u * k, LAB_U16, LAB_LEVEL, 0, 0, 65535);
        lab_addk(G_BANDS, "B", k + 1u, "BIAS", PK(d_bias, k) + 2u * k, LAB_I16, LAB_PCT, 0, -32767, 32767);
        lab_addk(G_BANDS, "B", k + 1u, "PAN", PK(d_pan, k) + 2u * k, LAB_I16, LAB_PAN, 0, -32767, 32767);
        lab_addk(G_BANDS, "B", k + 1u, "DYNAMIC", PK(d_shape, k) + 2u * k, LAB_I16, LAB_PCT, 0, -32767, 32767);
        lab_addk(G_BANDS, "B", k + 1u, "BODY", PK(d_body, k) + 2u * k, LAB_U16, LAB_99, 0, 0, 65535);
    }
    lab_add(G_LOOP, "FDBK", P(d_fb), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_LOOP, "TIME", P(d_ftime), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_LOOP, "SPREAD", P(d_spread), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_LOOP, "TAME", P(d_tame), LAB_U16, LAB_99, 0, 0, 65535);
    /* TIME (the delay) and SPACE (the plate) */
    lab_add(G_TIME, "TIME", P(s_time), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_TIME, "FDBK", P(s_fb), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_TIME, "TONE", P(s_dtone), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_TIME, "D-LEVEL", P(s_dlevel), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_TIME, "MODE", P(s_mode), LAB_U8, LAB_HEX, 0, 0, 255);
    lab_add(G_SPACE, "SIZE", P(s_size), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_SPACE, "DECAY", P(s_decay), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_SPACE, "TONE", P(s_rtone), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_SPACE, "R-LEVEL", P(s_rlevel), LAB_U16, LAB_99, 0, 0, 65535);
    /* the matrix */
    for (k = 0; k < SD_MODS; k++) {
        uint32_t mb = PK(mod, k) + k * (uint32_t)sizeof(SdModSlot);
        lab_path2("mod", k, 0, 0, "src");
        lab_addk(G_MOD, "", k + 1u, "SOURCE", mb + (uint32_t)offsetof(SdModSlot, src), LAB_U8, LAB_ENUM, LAB_E_SRC, 0,
                 SD_MSRCS - 1);
        lab_path2("mod", k, 0, 0, "dst");
        lab_addk(G_MOD, "", k + 1u, "DEST", mb + (uint32_t)offsetof(SdModSlot, dst), LAB_U8, LAB_ENUM, LAB_E_DST, 0,
                 SD_MDSTS - 1);
        lab_path2("mod", k, 0, 0, "amt");
        lab_addk(G_MOD, "", k + 1u, "AMOUNT", mb + (uint32_t)offsetof(SdModSlot, amt), LAB_I16, LAB_PCT, 0, -32767, 32767);
    }
    for (k = 0; k < 2u; k++) {
        lab_addk(G_LFO, "LFO ", k + 1u, "SHAPE", PK(lfo_shape, k) + k, LAB_U8, LAB_ENUM, LAB_E_LFO, 0, SD_LSHAPES - 1);
        lab_addk(G_LFO, "LFO ", k + 1u, "MODE", PK(lfo_mode, k) + k, LAB_U8, LAB_HEX, 0, 0, 7);
        lab_addk(G_LFO, "LFO ", k + 1u, "RATE", PK(lfo_rate, k) + 2u * k, LAB_U16, LAB_99, 0, 0, 65535);
    }
    lab_add(G_MACRO, "MUTATE", P(mac[0]), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_MACRO, "MOVE", P(mac[1]), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_MACRO, "BREAK", P(mac[2]), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_MACRO, "MACRO 4", P(mac[3]), LAB_U16, LAB_99, 0, 0, 65535);
    lab_add(G_MACRO, "MACRO 4 KIND", P(mac4), LAB_U8, LAB_ENUM, LAB_E_M4, 0, SD_M4S - 1);
    lab_add(G_MACRO, "FX OFF", P(fx_off), LAB_U8, LAB_HEX, 0, 0, 31);
    lab_add(G_TURING, "ON + CHANGE", P(tur_set), LAB_U8, LAB_HEX, 0, 0, 255);
    lab_add(G_TURING, "LENGTH + DIV", P(tur_ld), LAB_U8, LAB_HEX, 0, 0, 255);
    lab_add(G_TURING, "DEST", P(tur_dst), LAB_U8, LAB_RAW, 0, 0, 255);
    lab_add(G_TURING, "AMOUNT %", P(tur_amt), LAB_I8, LAB_RAW, 0, -100, 100);
}
#undef F
#undef P
#undef PK
#undef U8
#undef U16

WEB_EXPORT(web_pat) SdPatch *web_pat(void) { return &pat; }
WEB_EXPORT(web_pat_size) uint32_t web_pat_size(void) { return (uint32_t)sizeof pat; }
WEB_EXPORT(web_pat_apply) void web_pat_apply(void) { gen = 0xFFFFFFFFu; sd_engine_patch(&eng, &pat); }
WEB_EXPORT(web_pat_load) void web_pat_load(uint32_t i)
{
    if (!patch_ok(i)) return;
    set.patch = (uint8_t)i;
    patch_load(set.patch, &pat);
    gen = 0xFFFFFFFFu;
    sd_engine_patch(&eng, &pat);
}
WEB_EXPORT(web_pat_now) uint32_t web_pat_now(void) { return set.patch; }
WEB_EXPORT(web_pat_count) uint32_t web_pat_count(void) { return NPATCHES; }
WEB_EXPORT(web_pat_ok) uint32_t web_pat_ok(uint32_t i) { return patch_ok(i) ? 1u : 0u; }
WEB_EXPORT(web_pat_name) const char *web_pat_name(uint32_t i) { return patch_ok(i) ? patch_name(i) : 0; }
WEB_EXPORT(web_fields) const LabField *web_fields(void) { if (!lab_n) lab_build(); return lab_f; }
WEB_EXPORT(web_nfields) uint32_t web_nfields(void) { if (!lab_n) lab_build(); return lab_n; }
WEB_EXPORT(web_field_size) uint32_t web_field_size(void) { return (uint32_t)sizeof(LabField); }
static const char *lab_dst_full(uint32_t d)     /* a destination as the FM-1's list names it: its page, its knob */
{
    static char full[SD_MDSTS][24];
    char *p = full[d];
    if (!*p) { if (DST_GRP[d] != DG_NONE) { p = put(p, DG_NAME[DST_GRP[d]]); *p++ = ' '; } p = put(p, DST_PARAM[d]); *p = 0; }
    return full[d];
}
static const char *lab_src_full(uint32_t m)     /* a source as the FM-1's list names it: where it comes from, its name */
{
    static char full[SD_MSRCS][24];
    char *p = full[m];
    if (!*p) { if (*SRC_GRP[m]) { p = put(p, SRC_GRP[m]); *p++ = ' '; } p = put(p, SRC_PARAM[m]); *p = 0; }
    return full[m];
}
/* the destinations and the sources in the lists' order (SD_MD_ORDER, SRC_ORDER), for the lab's menus; 255 past the
   last */
WEB_EXPORT(web_dst_order) uint32_t web_dst_order(uint32_t i) { return i < SD_MDSTS ? SD_MD_ORDER[i] : 255u; }
WEB_EXPORT(web_src_order) uint32_t web_src_order(uint32_t i) { return i < SD_MSRCS ? SRC_ORDER[i] : 255u; }
WEB_EXPORT(web_enum) const char *web_enum(uint32_t list, uint32_t i)
{
    static const char *const ROLE[] = {"C", "S", "P"};
    static const char *const SHAPER[] = {"NONE", "SOFT", "HARD", "FOLD", "CRUSH"};
    static const char *const RMODEL[] = {"BP6", "BP12", "VBP", "KP", "AP1", "AP2"};
    static const char *const RCLOCK[] = {"FWD", "BWD", "P-P", "RND"};
    static const char *const XOVER[] = {"CMP", "LR4"};
    static const char *const LFO[] = {"SINE", "TRI", "SAW", "RAMP", "SQUARE", "S&H"};
    static const char *const M4[] = {"BODY", "AIR", "DROWN"};
    static const char *const FILT[] = {"OFF", "LP12", "LP24"};
    static const char *const ONOFF[] = {"OFF", "ON"};
    static const char *const COURSE_N[] = {"OFF", "ON", "LOOP"};
    switch (list) {
    case LAB_E_ROLE: return i < 3u ? ROLE[i] : 0;
    case LAB_E_SHAPER: return i < 5u ? SHAPER[i] : 0;
    case LAB_E_RMODEL: return i < 6u ? RMODEL[i] : 0;
    case LAB_E_RCLOCK: return i < 4u ? RCLOCK[i] : 0;
    case LAB_E_XOVER: return i < 2u ? XOVER[i] : 0;
    case LAB_E_LFO: return i < 6u ? LFO[i] : 0;
    case LAB_E_SRC: return i < SD_MSRCS ? lab_src_full(i) : 0;
    case LAB_E_DST: return i < SD_MDSTS ? lab_dst_full(i) : 0;
    case LAB_E_M4: return i < 3u ? M4[i] : 0;
    case LAB_E_FILTER: return i < 3u ? FILT[i] : 0;
    case LAB_E_ONOFF: return i < 2u ? ONOFF[i] : 0;
    case LAB_E_STYPE: return i < SD_STYPES ? STYPE_NAME[i] : 0;
    case LAB_E_SORDER: return i < SD_SORDERS ? SORDER_NAME[i] : 0;
    case LAB_E_SCALE: return i < SD_SCALES ? SCALE_NAME[i] : 0;
    case LAB_E_SDIV: return i < SD_SDIVS ? SDIV_NAME[i] : 0;
    case LAB_E_AMODE: return i < SD_AMODES ? AMODE_NAME[i] : 0;
    case LAB_E_MODE: return i < SD_MODES ? MODE_NAME[i] : 0;
    case LAB_E_COURSE: return i < 3u ? COURSE_N[i] : 0;
    default: return 0;
    }
}

/* ---- the sequencer: the patterns are SEQ's own (seqp is the track SEQ edits now, seq_other the other one), so the
   FM-1's SEQ page and the lab edit the same two */
static SdPattern *lab_seq(uint32_t k) { return (k & 1u) == seq_trk ? &seqp : &seq_other; }
WEB_EXPORT(web_seq) SdPattern *web_seq(uint32_t k) { return lab_seq(k); }
WEB_EXPORT(web_seq_size) uint32_t web_seq_size(void) { return (uint32_t)sizeof(SdPattern); }
WEB_EXPORT(web_seq_apply) void web_seq_apply(uint32_t k)
{
    SdPattern *p = lab_seq(k);
    if (p->len < 1u) p->len = 1u;
    if (p->len > SD_PSTEPS) p->len = SD_PSTEPS;
    sd_engine_pattern_to(&eng, k & 1u, p);
    if ((k & 1u) == seq_trk) {                   /* SEQ's cursor and bank inside it (as LENGTH keeps them) */
        if (seq_cur >= seqp.len) seq_cur = seqp.len - 1u;
        if (seq_bank * 16u >= seqp.len) seq_bank = (seqp.len - 1u) / 16u;
    }
}
enum { LAB_SQ_LEN = 1, LAB_SQ_CUT, LAB_SQ_EVEN, LAB_SQ_SEGLEN, LAB_SQ_RND, LAB_SQ_CLEAR };
WEB_EXPORT(web_seq_op) void web_seq_op(uint32_t k, uint32_t op, uint32_t a, uint32_t b)
{
    SdPattern *p = lab_seq(k);
    uint32_t i;
    switch (op) {
    case LAB_SQ_LEN: {                           /* LENGTH: the segments scale with it */
        uint32_t old = p->len;
        p->len = (uint8_t)(a < 1u ? 1u : a > SD_PSTEPS ? SD_PSTEPS : a);
        sd_seg_rescale(p, old);
        break;
    }
    case LAB_SQ_CUT: if (a < p->len) (void)sd_seg_toggle(p, a); break;   /* a segment starts at step a, or no more */
    case LAB_SQ_EVEN: sd_seg_even(p); break;
    case LAB_SQ_SEGLEN: if (a < SD_SEGS) sd_seg_set_len(p, a, b < 1u ? 1u : b); break;
    case LAB_SQ_RND: sd_pattern_randomize(p, &seq_rng); break;
    case LAB_SQ_CLEAR:                           /* every step a rest, as CLR */
        for (i = 0; i < SD_PSTEPS; i++) {
            p->step[i].type = SD_ST_REST;
            p->step[i].chord[0] = p->step[i].chord[1] = p->step[i].chord[2] = 0;
        }
        break;
    default: return;
    }
    web_seq_apply(k);
}
static const uint16_t LAB_SEQ_OFF[] = {         /* the page reads these in this order (web/lab/seq.js: LAYOUT) */
    sizeof(SdStep), offsetof(SdStep, note), offsetof(SdStep, vel), offsetof(SdStep, gate), offsetof(SdStep, type),
    offsetof(SdStep, ratchet), offsetof(SdStep, prob), offsetof(SdStep, chord),
    offsetof(SdPattern, step), offsetof(SdPattern, len), offsetof(SdPattern, div), offsetof(SdPattern, swing),
    offsetof(SdPattern, seg_count), offsetof(SdPattern, seg_split), offsetof(SdPattern, seg_rep),
    offsetof(SdPattern, seg_order), offsetof(SdPattern, seg_skip), offsetof(SdPattern, scale), sizeof(SdPattern),
    offsetof(SdOrbit, on), offsetof(SdOrbit, len), offsetof(SdOrbit, rate), offsetof(SdOrbit, glide),
    offsetof(SdOrbit, dst), offsetof(SdOrbit, xa), offsetof(SdOrbit, xb), offsetof(SdOrbit, lane),
    offsetof(SdOrbit, mac), sizeof(SdOrbit)};
WEB_EXPORT(web_seq_layout) uint32_t web_seq_layout(uint32_t i)
{
    return i < sizeof LAB_SEQ_OFF / sizeof LAB_SEQ_OFF[0] ? LAB_SEQ_OFF[i] : 0xFFFFu;
}
WEB_EXPORT(web_orb) SdOrbit *web_orb(void) { return &orbp; }
WEB_EXPORT(web_orb_size) uint32_t web_orb_size(void) { return (uint32_t)sizeof(SdOrbit); }
WEB_EXPORT(web_orb_apply) void web_orb_apply(void)
{
    if (orbp.len > 8u) orbp.len = 8u;
    if (orbp.rate > 2u) orbp.rate = 2u;
    orb_push();
}
enum { LAB_LS_BPM, LAB_LS_MODE, LAB_LS_ARP, LAB_LS_AMODE, LAB_LS_ADIV, LAB_LS_AOCT, LAB_LS_AGATE, LAB_LS_AHOLD,
       LAB_LS_ON_A, LAB_LS_ON_B, LAB_LS_MASTER, LAB_LS_CHROM,
       LAB_LS_LOOP, LAB_LS_REV, LAB_LS_LLEN, LAB_LS_LLVL, LAB_LS_LSPEED, LAB_LS_LGLIDE, LAB_LS_FREEZE, LAB_LSN };
WEB_EXPORT(web_lset) uint32_t web_lset(uint32_t i)
{
    switch (i) {
    case LAB_LS_BPM: return set.bpm;
    case LAB_LS_MODE: return set.mode;
    case LAB_LS_ARP: return eng.arp_on;
    case LAB_LS_AMODE: return set.amode;
    case LAB_LS_ADIV: return set.adiv;
    case LAB_LS_AOCT: return set.aoct;
    case LAB_LS_AGATE: return set.agate;
    case LAB_LS_AHOLD: return set.ahold;
    case LAB_LS_ON_A: return !eng.t[0].mute;
    case LAB_LS_ON_B: return !eng.t[1].mute;
    case LAB_LS_MASTER: return set.master;
    case LAB_LS_CHROM: return set.xchrom;
    case LAB_LS_LOOP: return lp_on;
    case LAB_LS_REV: return lp_rev;
    case LAB_LS_LLEN: return lp_len;
    case LAB_LS_LLVL: return lp_lvl;
    case LAB_LS_LSPEED: return (uint32_t)(int32_t)lp_speed;   /* (semitones x 256, -3072 .. 3072: the page reads it signed) */
    case LAB_LS_LGLIDE: return lp_glide;
    case LAB_LS_FREEZE: return sp_frz;
    default: return 0xFFFFFFFFu;
    }
}
WEB_EXPORT(web_lset_n) uint32_t web_lset_n(void) { return LAB_LSN; }
WEB_EXPORT(web_lset_put) void web_lset_put(uint32_t i, uint32_t v)   /* (the main loop hands ARP's, MASTER and the
                                                   transposes to the engine each pass, as the buttons leave them) */
{
    switch (i) {
    case LAB_LS_BPM: set.bpm = (uint8_t)(v < 40u ? 40u : v > 240u ? 240u : v); eng.bpm = set.bpm; break;
    case LAB_LS_MODE: if (v < SD_MODES) { set.mode = (uint8_t)v; eng.mode = set.mode; } break;
    case LAB_LS_ARP: if (!v) eng.arp_on = 0; else if (arp_room()) eng.arp_on = 1; break;
    case LAB_LS_AMODE: if (v < SD_AMODES) set.amode = (uint8_t)v; break;
    case LAB_LS_ADIV: if (v < SD_SDIVS) set.adiv = (uint8_t)v; break;
    case LAB_LS_AOCT: set.aoct = (uint8_t)(v < 1u ? 1u : v > 4u ? 4u : v); break;
    case LAB_LS_AGATE: set.agate = (uint8_t)(v < 5u ? 5u : v > 100u ? 100u : v); break;   /* (as K3 on ARP) */
    case LAB_LS_AHOLD: set.ahold = (uint8_t)(v != 0u); break;
    case LAB_LS_ON_A: eng.t[0].mute = (uint8_t)(v == 0u); break;
    case LAB_LS_ON_B: eng.t[1].mute = (uint8_t)(v == 0u); break;
    case LAB_LS_MASTER: set.master = (uint8_t)(v & 1u); break;
    case LAB_LS_CHROM: set.xchrom = (uint8_t)(v != 0u); break;
    case LAB_LS_LOOP: lp_on = (uint8_t)(v != 0u); break;   /* (the main loop hands the looper's to the engine each pass) */
    case LAB_LS_REV: lp_rev = (uint8_t)(v != 0u); break;
    case LAB_LS_LLEN: lp_len = (uint16_t)(v > 65535u ? 65535u : v); break;
    case LAB_LS_LLVL: lp_lvl = (uint16_t)(v > 65535u ? 65535u : v); break;
    case LAB_LS_LSPEED: { int32_t q = (int32_t)v; lp_speed = (int16_t)(q < -3072 ? -3072 : q > 3072 ? 3072 : q); break; }
    case LAB_LS_LGLIDE: lp_glide = (uint16_t)(v > 65535u ? 65535u : v); break;
    case LAB_LS_FREEZE: sp_frz = (uint8_t)(v != 0u); break;
    default: break;
    }
}
WEB_EXPORT(web_play) uint32_t web_play(uint32_t i)   /* running; track A's step now; track B's */
{
    return i == 0u ? eng.seq_run : i < 3u ? eng.t[i - 1u].seq_shown : 0u;
}
/* the looper as it plays: looping (its loop playing or fading), parked by STOP, its loop's length and the head in it
   (half-rate samples x 256), where it records, LOOP's end (its loop the length before it, in the ring), the ring's
   length, LENGTH as set (x 256), backwards, its level now (0 .. 16384), its rate (Q16), its memory's two pieces */
enum { LAB_LP_ON, LAB_LP_PARK, LAB_LP_LN, LAB_LP_K, LAB_LP_W, LAB_LP_BASE, LAB_LP_TOTAL, LAB_LP_LEN, LAB_LP_REV,
       LAB_LP_LEV, LAB_LP_RATE, LAB_LP_A, LAB_LP_NA, LAB_LP_B, LAB_LP_NB, LAB_LP_WAIT };   /* (WAIT: LOOP waiting
                                                   for its line, sd_lp_snap) */
WEB_EXPORT(web_lp) uint32_t web_lp(uint32_t i)
{
    const SdSpace *s = &eng.sp;
    switch (i) {
    case LAB_LP_ON: return s->pon;
    case LAB_LP_PARK: return s->ppark;
    case LAB_LP_LN: return s->pln;
    case LAB_LP_K: return s->pk;
    case LAB_LP_W: return s->lw;
    case LAB_LP_BASE: return s->lbase;
    case LAB_LP_TOTAL: return sd_lp_total(&eng);
    case LAB_LP_LEN: return (uint32_t)s->llen;
    case LAB_LP_REV: return s->pdir;
    case LAB_LP_LEV: return (uint32_t)(s->plev < 0 ? 0 : s->plev);
    case LAB_LP_RATE: return (uint32_t)s->pr;
    case LAB_LP_A: return (uint32_t)(uintptr_t)eng.lpa;
    case LAB_LP_NA: return eng.lpa ? eng.lpa_n : 0u;
    case LAB_LP_B: return (uint32_t)(uintptr_t)eng.lpb;
    case LAB_LP_NB: return eng.lpb ? eng.lpb_n : 0u;
    case LAB_LP_WAIT: return s->pqw;
    default: return 0;
    }
}
WEB_EXPORT(web_tur) uint32_t web_tur(uint32_t i) { return i ? eng.tur_count : eng.tur_reg; }   /* the register; its steps */
WEB_EXPORT(web_tur_dac) int32_t web_tur_dac(uint32_t r) { return sd_tur_dac(r); }   /* a value, as the matrix reads it */
#endif
