/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The freeze log (2026-11-22) on the host: the inputs a run leaves (a button, a key, a knob from
 * tests/scripts/frz1.txt), kept oldest first in a record; 20 records into 16 places, the newest first, the oldest
 * written over. On the FM-1 the places are two flash sectors (fm1-live checked it: a freeze, the watchdog's restart,
 * the record on SETUP and over USB). Run by tests/run_tests.sh */
#include <unistd.h>
#include "stardistroyer.c"
__attribute__((destructor)) static void frz_check(void)
{
    FrzRec r;
    LastRun run = last_run;
    uint32_t i, k, bad = 0, kinds[3];
    frz_add(FRZ_FREEZE, &run, 0);               /* the script's run: its inputs, the oldest first */
    if (frz_get(0, &r) || r.ev_n < 3u) { printf("FAIL  the freeze log: the run's record (%u inputs)\n", r.ev_n); _exit(1); }
    for (i = k = 0; i < EV_N && k < 3u; i++) if (r.ev[i].kc >> 5 == EV_BTN || r.ev[i].kc >> 5 == EV_KEY || r.ev[i].kc >> 5 == EV_ENC) kinds[k++] = r.ev[i].kc >> 5;
    if (k < 3u || kinds[0] != EV_BTN || kinds[1] != EV_KEY || kinds[2] != EV_ENC) {
        printf("FAIL  the freeze log: the inputs out of order (%u: %u %u %u)\n", k, kinds[0], kinds[1], kinds[2]);
        _exit(1);
    }
    for (i = 1; i < 20u; i++) { run.ms = 1000u * i; frz_add(FRZ_FREEZE, &run, 0); }
    if (frz_n != FRZ_RECS || frz_top != 19u) bad = 1;
    for (k = 0; k < FRZ_RECS && !bad; k++) if (frz_get(k, &r) || r.seq != 19u - k || r.run.ms != 1000u * (19u - k)) bad = 1 + k;
    if (bad || !frz_get(FRZ_RECS, &r)) { printf("FAIL  the freeze log: 20 records in %u places (%u, top %u)\n", FRZ_RECS, bad, frz_top); _exit(1); }
    printf("ok    the freeze log: a run's inputs oldest first; 20 records, the newest 16 kept, the newest first\n");
}
