/* SPDX-License-Identifier: GPL-3.0-only */
/* Where Dinghy keeps your data in flash, per board (platform.c: settings_*, blob_*, slot_*). Macros and compile-time
 * checks only, so the host tests include it (tests/smk_flash_test.c).
 *
 *              FM-1                          SMK-37 (Pro, Elite, MKE-P37)
 *   settings   0xFC000, 1 sector             0x9D000, 1 sector
 *   blobs      0xF4000.. NBLOB sectors       0x9E000.. NBLOB sectors
 *   slots      0x97000.. NSLOT x SLOT_SECT   0xC2000.. NSLOT x SLOT_SECT (USRFLASH, SLOOP's sample slots)
 *   owner tag  -                             0xAF000: "SMK37DATA" + DATA_FAMILY (smk_owner_check)
 *   copies     0xB7000, 3 sectors            0xE2000, 3 sectors (after the slots: the second copies, below)
 *   freeze log 0xBA000, 2 sectors            0xE5000, 2 sectors (after the copies: Star Distroyer's, stardistroyer.c)
 *
 * On the FM-1 the other firmwares of the family keep their data in 0x97000..0xFBFFF too: a new firmware finds the
 * magic or the length wrong and starts fresh (give your fork its own STORE_MAGIC). The SMK-37's store is shared by
 * the ports under the fm1-to-smk37 kit's convention: whose data it is says the owner tag, anything else is cleared
 * once at boot (the stock settings, another port's), never parsed. Calibration (0xC1000), BT pairing, the stock
 * resources and key_mac are never in reach: FL_RANGE_OK below is the app's allow-list, and the RAM flash driver
 * refuses every protected range on its own as well (FL_GUARD, hal/fm1_flash_map.h). */
#pragma once
#include <stdint.h>

#ifndef STORE_MAGIC
#define STORE_MAGIC 0x54534453u                 /* "SDST": settings and blobs (slots: "SLOT"); your fork: your own */
#endif
#define NBLOB 8u                                /* blobs: one 4 KiB sector each */
#define NSLOT 8u                                /* slots: SLOT_SECT sectors each, for something bigger (samples, songs) */
#define SLOT_SECT 4u
#define NCOPY 3u                                /* the copies' own sectors after the slots (below) */
#define LOG_SECT 2u                             /* the freeze log's sectors after the copies (Star Distroyer's, 2026-11-22:
                                                   what the last freezes and crashes left, stardistroyer.c's frz_*) */

#if BOARD_SMK37
#define SETTINGS_AT 0x0009D000u
#define BLOBS_AT 0x0009E000u
#define OWNER_AT 0x000AF000u                    /* (the kit's place for it) */
#define SLOTS_AT 0x000C2000u
#define COPIES_AT (SLOTS_AT + NSLOT * SLOT_SECT * 0x1000u)
#define LOG_AT (COPIES_AT + NCOPY * 0x1000u)    /* the freeze log, LOG_SECT sectors after the copies (Star Distroyer's) */
#ifndef DATA_FAMILY
#define DATA_FAMILY "DINGHY"                    /* the owner tag's family: 1..7 of A-Z 0-9 (tools/build.py DATA_FAMILY) */
#endif
#define FL_RANGE_OK(off, n) (FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || FL_IN(off, n, SETTINGS_AT, SETTINGS_AT + 0x1000u) || \
                             FL_IN(off, n, BLOBS_AT, BLOBS_AT + NBLOB * 0x1000u) || FL_IN(off, n, OWNER_AT, OWNER_AT + 0x1000u) || \
                             FL_IN(off, n, SLOTS_AT, LOG_AT + LOG_SECT * 0x1000u))
#else
#define SETTINGS_AT 0x000FC000u                 /* (Felucca's globals area) */
#define BLOBS_AT 0x000F4000u                    /* (NB: in the stock firmware's USR area, 0xEA000..0xFBFFF, whose
                                                   patch settings lie at 0xFA000..: blobs 6 and 7. Star Distroyer
                                                   writes no blob) */
#define SLOTS_AT 0x00097000u                    /* (Felucca's main store, FL_DATA) */
#define COPIES_AT (SLOTS_AT + NSLOT * SLOT_SECT * 0x1000u)
#define LOG_AT (COPIES_AT + NCOPY * 0x1000u)    /* the freeze log, LOG_SECT sectors after the copies (Star Distroyer's) */
#define FL_RANGE_OK(off, n) (FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || FL_IN(off, n, SETTINGS_AT, SETTINGS_AT + 0x1000u) || \
                             FL_IN(off, n, BLOBS_AT, BLOBS_AT + NBLOB * 0x1000u) || \
                             FL_IN(off, n, SLOTS_AT, LOG_AT + LOG_SECT * 0x1000u))
#endif
#include "fm1_flash_map.h"
#define SECTOR_MAX (4096u - 12u)                /* settings, a blob: the sector minus the header */
#define SLOT_MAX (SLOT_SECT * 0x1000u - 12u)

/* Each save in two copies on the device (store_io.h, Star Distroyer's, 2026-11-20): a save goes over the older one, so
 * a cut in the middle (the power, a restart) leaves the other whole. A copy is the save plus its number (STORE_FOOT
 * bytes after the data: a save must leave room for it). The second copies' places: slots 0..2's in slots 5..7 (those
 * hold copies, not their own: SLOT_OWN); slots 3 and 4's (a sector's worth of data at most) and the settings' in the
 * NCOPY sectors after the slots. The blobs keep one copy. The host's and the browser's stores keep one (the same sizes) */
#define STORE_FOOT 4u
#define SETTINGS_B_AT (COPIES_AT + 2u * 0x1000u)
#define SLOT_B_AT(i) ((i) < 3u ? SLOTS_AT + ((i) + 5u) * SLOT_SECT * 0x1000u : (i) < 5u ? COPIES_AT + ((i) - 3u) * 0x1000u : 0u)
#define SLOT_B_MAX(i) ((i) < 3u ? SLOT_MAX : (i) < 5u ? SECTOR_MAX : 0u)   /* (its header's room, as SLOT_MAX's) */
#define SLOT_OWN(i) ((i) < 5u)
_Static_assert(NSLOT == 8u && SLOT_SECT == 4u && NCOPY == 3u, "store.h: the copies' places assume 8 slots of 4, 3 sectors after");

#if BOARD_SMK37
_Static_assert(SETTINGS_AT >= FL_DATA_LO && SETTINGS_AT + 0x1000u <= BLOBS_AT, "SMK store: settings outside FL_DATA");
_Static_assert(BLOBS_AT + NBLOB * 0x1000u <= OWNER_AT && OWNER_AT + 0x1000u <= FL_DATA_HI, "SMK store: blobs / owner tag");
_Static_assert(SLOTS_AT >= FL_SMP_LO && LOG_AT + LOG_SECT * 0x1000u <= FL_SMP_HI, "SMK store: slots, copies, log outside FL_SMP");
_Static_assert(FL_WIN_SAFE(SLOTS_AT, LOG_AT + LOG_SECT * 0x1000u), "SMK store: the slots overlap a protected range");
_Static_assert(FL_WIN_SAFE(SETTINGS_AT, OWNER_AT + 0x1000u), "SMK store: overlaps a protected range");
_Static_assert(sizeof DATA_FAMILY - 1u >= 1u && sizeof DATA_FAMILY - 1u <= 7u, "SMK store: family 1..7 characters");

/* the 16-byte owner tag: "SMK37DATA" + the family, zero padded (a 7-letter family fills it, no NUL) */
static void smk_owner_tag(uint8_t tag[16])
{
    static const char FAM[] = DATA_FAMILY;
    uint32_t i;
    for (i = 0; i < 16u; i++)
        tag[i] = i < 9u ? (uint8_t)"SMK37DATA"[i] : i - 9u < sizeof FAM - 1u ? (uint8_t)FAM[i - 9u] : 0u;
}
#else
_Static_assert(SLOTS_AT >= FL_DATA_LO && LOG_AT + LOG_SECT * 0x1000u <= FL_DATA_HI, "FM-1 store: slots, copies, log outside FL_DATA");
_Static_assert(BLOBS_AT >= 0xEA000u && BLOBS_AT + NBLOB * 0x1000u <= SETTINGS_AT, "FM-1 store: blobs (free, below the settings)");
#endif
