/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The app on the host with tests/scripts/rec3.txt: SEQ's REC ends from any page (2026-11-22, the user: left on, it was
 * stuck away from SEQ: DRUM's REC was its own, and ENV and LFO wrote a TIE and a REST wherever you were). At 2.0 s ENV
 * took you to its page (no TIE written); at the end, on DRUM, REC ended SEQ's recording and left the drums' off. Run by
 * tests/run_tests.sh */
#define HOST_POLL_HOOK recoff_poll
#include <unistd.h>
#include "stardistroyer.c"
static uint32_t at2000_page = 0xFFu, at2000_rec;
static void recoff_poll(void) { if (fm1_ms == 2000u) { at2000_page = page; at2000_rec = seq_rec; } }
__attribute__((destructor)) static void recoff_check(void)
{
    if (at2000_page != 0u || !at2000_rec || page != PAGE_DRUM || seq_rec || dr_rec) {
        printf("FAIL  REC away from SEQ: ENV led to page %u (NAV 0), REC on there %u (1); at the end page %u (DRUM %u), "
               "SEQ's REC %u (0), the drums' %u (0)\n", at2000_page, at2000_rec, page, PAGE_DRUM, seq_rec, dr_rec);
        fflush(stdout);
        _exit(1);
    }
    printf("ok    REC away from SEQ: ENV goes to its page, REC on DRUM ends SEQ's recording\n");
}
