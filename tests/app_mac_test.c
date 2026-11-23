/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* HOME's macros apart (2026-11-23, the user: "MACRO 4's place is odd, and instead of a global cutoff the synth's
 * cutoff on the front"). A patch saved before (its size less AIR and DROWN, MACRO 4 as DROWN) loads with DROWN on its
 * own knob, nothing on BODY, its other fields where they were. With tests/scripts/mac1.txt: MORE's BODY AIR DROWN
 * DARK turned, then MACROS' K4 the patch's own CUTOFF; the engine has them. Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
static uint16_t cut0;
__attribute__((constructor)) static void mac_old_save(void)
{
    static uint8_t old[sizeof(UserPatches)];
    const uint32_t po = (uint32_t)__builtin_offsetof(UserPatches, p), osz = (uint32_t)sizeof(SdPatch) - 2u;
    SdPatch p = SD_PATCH[SD_P_DISTROY];
    p.lv = (uint8_t)(p.lv & ~4u); p.mac4 = SD_M4_DROWN; p.mac[3] = 40000u; p.xm = 12345u; p.sub_src = SD_SUB_B;
    old[0] = 1;                                 /* (used: U01) */
    memcpy(old + __builtin_offsetof(UserPatches, name), "OLDSAVE ", 8);
    memcpy(old + po, &p, osz);
    host_save(host_slot[0], SLOT_MAX, old, po + UPATCHES * osz, "slot", 0);
}
__attribute__((destructor)) static void mac_check(void)
{
    SdPatch q, b;
    uint32_t ok;
    patch_load(SD_PATCHES, &q);
    ok = q.mac_drown == sd_mac8(40000u) && !q.mac[3] && !q.mac_air && (q.lv & 4u) && q.mac4 == SD_M4_BODY &&
         q.xm == 12345u && q.sub_src == SD_SUB_B;
    if (!ok) {
        printf("FAIL  an old save: DROWN %u (want %u), BODY %u, AIR %u, mark %u, XM %u, SUB %u\n", q.mac_drown,
               sd_mac8(40000u), q.mac[3], q.mac_air, q.lv & 4u, q.xm, q.sub_src);
        _exit(1);
    }
    patch_load(set.patch, &b);
    cut0 = b.f_cut;
    ok = q99(pat.mac[3]) == 7u && q99(pat.mac_air * 257u) == 10u && q99(pat.mac_drown * 257u) == 5u &&
         q99(home_filt) == 20u && eng.filt == home_filt && pat.f_cut < cut0 &&
         eng.base.mac[3] == pat.mac[3] && eng.base.mac_air == pat.mac_air && eng.base.mac_drown == pat.mac_drown &&
         eng.base.f_cut == pat.f_cut;
    if (!ok) {
        printf("FAIL  HOME's MORE and CUTOFF: BODY %u AIR %u DROWN %u DARK %u (eng %u), CUTOFF %u (was %u), engine %u %u %u %u\n",
               q99(pat.mac[3]), q99(pat.mac_air * 257u), q99(pat.mac_drown * 257u), q99(home_filt), eng.filt, pat.f_cut,
               cut0, eng.base.mac[3], eng.base.mac_air, eng.base.mac_drown, eng.base.f_cut);
        _exit(1);
    }
    printf("ok    HOME: an old MACRO 4 (DROWN) on DROWN; MORE BODY 7 AIR 10 DROWN 5 DARK 20; MACROS' CUTOFF %u -> %u\n",
           cut0, pat.f_cut);
}
