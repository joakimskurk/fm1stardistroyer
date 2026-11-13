/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* A patch from "key value ..." lines (the bowed-string tuning's format, tools/sd_strings.c): shared by the host
 * tools that play a tuned patch (sd_strings, sd_epic). Include after sd_engine.h, <math.h>, <stdio.h>, <string.h>. */
#ifndef SD_PARAMS_H
#define SD_PARAMS_H
static inline uint16_t hz_m256(double hz) { return (uint16_t)lround(256.0 * (69.0 + 12.0 * log2(hz / 440.0))); }
static inline uint16_t db_lv(double db) { double v = 32768.0 * pow(10.0, db / 20.0); return (uint16_t)(v > 65535 ? 65535 : v); }

static inline int params(const char *path, SdPatch *p)   /* "key value ..." lines; # comments */
{
    char line[256], key[64];
    double v[8];
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 1; }
    while (fgets(line, sizeof line, f)) {
        int n, k;
        if (line[0] == '#' || sscanf(line, "%63s", key) != 1) continue;
        n = sscanf(line, "%*s %lf %lf %lf %lf %lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
        if (!strcmp(key, "warp")) p->warp = (uint16_t)v[0];
        else if (!strcmp(key, "xm")) p->xm = (uint16_t)v[0];
        else if (!strcmp(key, "dt")) p->dt = (uint16_t)v[0];
        else if (!strcmp(key, "env1") && n == 4) { p->e1_a = (uint8_t)v[0]; p->e1_d = (uint8_t)v[1]; p->e1_s = (uint8_t)v[2]; p->e1_r = (uint8_t)v[3]; }
        else if (!strcmp(key, "freqs") && n == 5) {   /* freqs STAGE f1 f2 f3 f4 (Hz) */
            for (k = 0; k < 4; k++) p->stage[(int)v[0]].pitch[k] = hz_m256(v[1 + k]);
        } else if (!strcmp(key, "levels") && n == 5) {   /* levels STAGE l1 l2 l3 l4 (dB, 0 = 1x) */
            for (k = 0; k < 4; k++) p->stage[(int)v[0]].level[k] = db_lv(v[1 + k]);
        } else if (!strcmp(key, "reactor") && n == 5) {   /* reactor MODEL RES MIX POS (0..65535; POS 0..1535) */
            p->r_model = (uint8_t)v[0]; p->r_res = (uint16_t)v[1]; p->r_mix = (uint16_t)v[2]; p->r_pos = (uint16_t)v[3];
            p->r_drive = (uint16_t)v[4];
        } else if (!strcmp(key, "reactor") && n == 4) {
            p->r_model = (uint8_t)v[0]; p->r_res = (uint16_t)v[1]; p->r_mix = (uint16_t)v[2]; p->r_pos = (uint16_t)v[3];
        } else if (!strcmp(key, "keypos")) p->mod[0] = (SdModSlot){SD_MS_KEY, SD_MD_RPOS, (int16_t)v[0]};
        else if (!strcmp(key, "keytame")) p->mod[2] = (SdModSlot){SD_MS_KEY, SD_MD_DTAME, (int16_t)v[0]};
        else if (!strcmp(key, "keyx")) p->mod[3] = (SdModSlot){SD_MS_KEY, SD_MD_X, (int16_t)v[0]};   /* A -> B */
        else if (!strcmp(key, "keyx2")) p->mod[4] = (SdModSlot){SD_MS_KEY, SD_MD_X, (int16_t)v[0]};   /* (twice: steeper) */
        else if (!strcmp(key, "nav_x")) p->x = (uint16_t)v[0];
        else if (!strcmp(key, "track")) p->r_track = (uint8_t)v[0];   /* 1 KEY, 2 HARM: the body onto the note's harmonics */
        else if (!strcmp(key, "keyx3")) p->mod[5] = (SdModSlot){SD_MS_KEY, SD_MD_X, (int16_t)v[0]};
        else if (!strcmp(key, "keyx4")) p->mod[6] = (SdModSlot){SD_MS_KEY, SD_MD_X, (int16_t)v[0]};
        else if (!strcmp(key, "osc") && n == 6) {   /* osc BANK IDX HARM FINE LEVEL ROLE(0 C, 1 S, 2 P) */
            SdOscPatch *o = &p->bank[(int)v[0] & 3].osc[(int)v[1] & 3];
            o->harm = (uint8_t)v[2]; o->fine = (int8_t)v[3]; o->level = (uint16_t)v[4];
            o->role = (uint8_t)((int)v[5] == 1 ? SD_ROLE_S : (int)v[5] == 2 ? SD_ROLE_P : SD_ROLE_C);
        } else if (!strcmp(key, "xm")) p->xm = (uint16_t)v[0];
        else if (!strcmp(key, "env2") && n == 4) { p->e2_a = (uint8_t)v[0]; p->e2_d = (uint8_t)v[1]; p->e2_s = (uint8_t)v[2]; p->e2_r = (uint8_t)v[3]; }
        else if (!strcmp(key, "crush") && n == 3) {   /* crush LEVEL_dB ALT GAIN: the top band's hash, the bow's hiss */
            p->d_type[3] = SD_DS_CRUSH; p->d_level[3] = db_lv(v[0]); p->d_alt[3] = (uint16_t)v[1]; p->d_gain[3] = (uint16_t)v[2];
        }
        else if (!strcmp(key, "vibrato") && n == 2) {   /* vibrato HZ CENTS */
            p->lfo_shape[1] = SD_LS_SIN; p->lfo_mode[1] = 0;
            p->lfo_rate[1] = (uint16_t)lround(65536.0 * log(v[0] / 0.02) / log(2000.0));
            p->mod[1] = (SdModSlot){SD_MS_LFO2, SD_MD_PITCH, (int16_t)lround(v[1] / 2400.0 * 32767.0)};
        } else if (!strcmp(key, "splits") && n == 3) {
            for (k = 0; k < 3; k++) p->d_split[k] = hz_m256(v[k]);
        } else if (!strcmp(key, "bands") && n == 4) {   /* bands dB dB dB dB: DISTROY's four levels, no shaping */
            for (k = 0; k < 4; k++) { p->d_level[k] = db_lv(v[k]); p->d_type[k] = SD_DS_NONE; p->d_gain[k] = 0; }
            p->d_mix = 65535; p->d_xover = SD_DX_CMP;
        } else if (!strcmp(key, "tame")) p->d_tame = (uint16_t)v[0];
        else if (!strcmp(key, "room") && n == 3) { p->s_size = (uint16_t)v[0]; p->s_decay = (uint16_t)v[1]; p->s_rlevel = (uint16_t)v[2]; }
        else if (!strcmp(key, "width")) p->width = (uint16_t)v[0];
    }
    fclose(f);
    return 0;
}

#endif
