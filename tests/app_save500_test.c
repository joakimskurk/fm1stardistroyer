/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* A save from before the LOW CUTs (2026-11-26): each patch 500 bytes, the twelve slots' layout. It loads with every
 * field where it was, its last two (AIR, DROWN) too, and both LOW CUTs 0 (TIME's off, SPACE's its 200 Hz); a second
 * patch after it as well (the places moved back to front). Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((constructor)) static void s500_old_save(void)
{
    static uint8_t old[sizeof(UserPatches)];
    const uint32_t po = (uint32_t)__builtin_offsetof(UserPatches, p), osz = (uint32_t)__builtin_offsetof(SdPatch, s_dlow);
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        SdPatch p = SD_PATCH[k ? SD_P_AURORA : SD_P_DISTROY];
        p.lv |= 4u;                             /* (saved after 2026-11-23: its macros apart already, as any save of
                                                   pat is) */
        p.xm = (uint16_t)(4321u + k); p.mac_air = (uint8_t)(55u + k); p.mac_drown = (uint8_t)(77u + k);
        p.mod[11] = (SdModSlot){SD_MS_LFO1, SD_MD_XM, (int16_t)(999 + k)}; p.mod_off[11] = (int8_t)(-40 + (int)k);
        p.s_dlow = 0xAA; p.s_rlow = 0x55;       /* (not in the old layout: what lies past it must not come in) */
        memcpy(old + po + k * osz, &p, osz);
    }
    old[0] = 3;                                 /* (used: U01, U02) */
    memcpy(old + __builtin_offsetof(UserPatches, name), "OLD500A ", 8);
    memcpy(old + __builtin_offsetof(UserPatches, name) + 8, "OLD500B ", 8);
    host_save(host_slot[0], SLOT_MAX, old, po + UPATCHES * osz, "slot", 0);
}
__attribute__((destructor)) static void s500_check(void)
{
    uint32_t k, ok = (upat.used & 3u) == 3u;
    for (k = 0; k < 2u; k++) {
        SdPatch q;
        patch_load(SD_PATCHES + k, &q);
        ok &= q.xm == 4321u + k && q.mac_air == 55u + k && q.mac_drown == 77u + k && q.mod[11].src == SD_MS_LFO1 &&
              q.mod[11].dst == SD_MD_XM && q.mod[11].amt == (int16_t)(999 + k) && q.mod_off[11] == -40 + (int)k &&
              !q.s_dlow && !q.s_rlow;
        if (!ok) {
            printf("FAIL  a 500-byte save, U0%u: XM %u AIR %u DROWN %u slot 12 %u %u %d %d, LOW CUTs %u %d\n", k + 1u, q.xm,
                   q.mac_air, q.mac_drown, q.mod[11].src, q.mod[11].dst, q.mod[11].amt, q.mod_off[11], q.s_dlow, q.s_rlow);
            fflush(stdout);
            _exit(1);
        }
    }
    printf("ok    a save from before the LOW CUTs (500 bytes a patch): both patches where they were, the LOW CUTs 0\n");
}
