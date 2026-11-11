/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio): from Melodee (github.com/keremimo/melodee), fm1_core1_stop.h and
 *   fm1_flash_access.h's idle word, under MPL-2.0 by agreement (https://github.com/hugelton/felucca-bsp/issues/5)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments: Felucca 1.6.1's fm1_cpu1_halt
 *   (a53ecf4; https://github.com/hugelton/Felucca/pull/247)
 * Taken from felucca-bsp (hal/fm1_cpu1_stop.h, hal/fm1_flash_access.h) for Star Distroyer: the two in one file, and
 * the flash gate waits for the second core instead of refusing the transaction (Dinghy's flash calls cannot fail). */
/* Hold the second core (CPU1), safely, and keep the flash off only under an idle one.
 *
 * Holding it can abort a bus access it has outstanding, and that raises a delayed bus-invalid flag on CPU0: the bus
 * traps are masked, CPU1 held (C1_CON 0x1EEE004: bit 1 set, then bit 3 cleared), the flag given time to arrive and
 * cleared, then the debug enables and the write lock put back. Registers, and the gate's idle word (a word in .bss,
 * so harmless before the RAM is set up): also first thing at a start (a reset that left CPU1 running), and on the
 * way to a reset or an update (fm1_sys.h). Harmless when CPU1 never ran.
 *
 * The gate: CPU1's jobs run from the flash, and a flash transaction switches the flash off. fm1_flash_cpu1_idle is
 * 1 while CPU1 sits in its RAM loop (fm1_cpu1.h: it sets it after each job; CPU0 clears it before handing one over).
 * irq_save (fm1_flash.h) calls fm1_flash_cpu1_gate with CPU0's interrupts off, so no new job comes: it waits for the
 * one under way (a job is an audio block's share, about a millisecond), and holds CPU1 if it has not come back in
 * 8 ms (it faulted, or hangs). fm1_flash_cpu1_cut tells fm1_cpu1.h it was held there. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#if defined(FELUCCA_LOADER) && FELUCCA_LOADER
/* the update loader: the app held the second core before it handed over (fm1_sys.h), and the loader never starts it,
 * so nothing to hold or wait for (the loader stays as Dinghy's, byte for byte) */
FM1_INLINE void fm1_cpu1_stop(void) {}
FM1_INLINE void fm1_flash_cpu1_gate(void) {}
#else
static volatile uint32_t fm1_flash_cpu1_idle;
static volatile uint8_t fm1_flash_cpu1_cut;

FM1_INLINE void fm1_cpu1_stop(void)
{
    uint32_t locked = !(*(volatile uint32_t *)0x1EEE240u & 1u), en, bus = 0x003F0030u;
    volatile uint32_t n;
    __asm__("" : "+r"(bus));                    /* (not folded into an address that looks like the chip ROM's) */
    if (locked)
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;          /* DBG_WR_EN: writing 0xE7 toggles the unlock */
    en = *(volatile uint32_t *)0x1EEE340u;                 /* DBG_EN */
    *(volatile uint32_t *)0x1EEE340u = en & ~bus;
    __asm__ volatile("csync" ::: "memory");
    *(volatile uint32_t *)0x1EEE004u |= 2u;
    *(volatile uint32_t *)0x1EEE004u &= ~8u;
    __asm__ volatile("csync" ::: "memory");
    for (n = 0; n < 256u; n++)                  /* let the delayed bus flag arrive */
        ;
    *(volatile uint32_t *)0x1EEE248u = bus;                /* DBG_MSG_CLR */
    __asm__ volatile("csync" ::: "memory");
    *(volatile uint32_t *)0x1EEE340u = en;
    if (locked && (*(volatile uint32_t *)0x1EEE240u & 1u))
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;
    fm1_flash_cpu1_idle = 0;
    __asm__ volatile("csync" ::: "memory");
}

/* 1: the flash may go off under CPU1 (never started or held: C1_CON bit 3 clear or bit 1 set; or idle in its loop) */
FM1_INLINE int fm1_flash_cpu1_ok(void)
{
    uint32_t c;
    __asm__ volatile("csync" ::: "memory");
    c = *(volatile uint32_t *)0x1EEE004u;
    return !(c & 8u) || (c & 2u) || fm1_flash_cpu1_idle == 1u;
}

/* CPU0, interrupts off, before the flash goes off (irq_save) */
FM1_INLINE void fm1_flash_cpu1_gate(void)
{
    uint32_t t0;
    if (fm1_flash_cpu1_ok())
        return;
    t0 = *(volatile uint32_t *)0x10804u;                   /* TIMER4, 24 MHz */
    while (!fm1_flash_cpu1_ok())
        if ((uint32_t)(*(volatile uint32_t *)0x10804u - t0) >= 8000u * 24u) {
            fm1_cpu1_stop();
            fm1_flash_cpu1_cut = 1;
            __asm__ volatile("csync" ::: "memory");
            return;
        }
}
#endif
