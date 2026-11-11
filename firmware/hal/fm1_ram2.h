/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* The FM-1's second RAM bank, a test (Star Distroyer's RAM2 build, 2026-11-11). The AC79 maps SDRAM at 0x04000000
 * (the AC79 SDK's linker script: "sdram"), and the FM-1's AC7911B comes with 2 MB (BA) or 8 MB (BB) of it inside. But
 * no firmware for the FM-1 is known to use it, M-VAVE's included (its start-up copies an overlay to 0x04000120 of
 * length 0), and its loader seems to set the clocks up without it: whether it answers at all, set up or not, only an
 * FM-1 can tell. Nothing here sets it up: this only looks.
 *
 *   fm1_ram2_probe()   its size in bytes (0: none, or it did not answer as RAM), from 64 KiB to 8 MiB
 *   fm1_ram2_clear(n)  its first n bytes to 0
 *
 * A word at each 4 KiB of 8 MiB, a value of its own each (its index ^ a constant), all written before any is read
 * back (so no cache, however it is set up, can answer from what it holds): where the bank ends, a read gives
 * another index's value (the bank's addresses wrap: that index's distance is its size) or no value of ours. Then
 * three patterns through its first 64 KiB. The debug unit's bus traps are masked meanwhile (as fm1_cpu1_stop masks
 * them), so an address with nothing behind it reads wrong instead of faulting, and their flags are cleared after.
 * fm1_ram2_mark (.noinit) is set while it runs: a reset in the middle (the bus hung) is seen at the next boot, which
 * then does not try again (fm1_ram2_hung). */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#define FM1_RAM2_BASE 0x04000000u
#define FM1_RAM2_MAX (8u << 20)
#define FM1_RAM2_MARK 0x324D4152u               /* "RAM2" */
struct { uint32_t magic, hung; } fm1_ram2_mark __attribute__((section(".noinit")));

static int fm1_ram2_hung(void) { return fm1_ram2_mark.magic == FM1_RAM2_MARK && fm1_ram2_mark.hung == 1u; }

static uint32_t fm1_ram2_probe(void)
{
    volatile uint32_t *b = (volatile uint32_t *)FM1_RAM2_BASE;
    uint32_t locked = !(*(volatile uint32_t *)0x1EEE240u & 1u), en, bus = 0x003F0030u, k, n = FM1_RAM2_MAX / 4096u, size;
    __asm__("" : "+r"(bus));                    /* (not folded into an address that looks like the chip ROM's) */
    if (fm1_ram2_hung())
        return 0;
    fm1_ram2_mark.magic = FM1_RAM2_MARK;
    fm1_ram2_mark.hung = 1;
    __asm__ volatile("csync" ::: "memory");
    if (locked)
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;          /* DBG_WR_EN: writing 0xE7 toggles the unlock */
    en = *(volatile uint32_t *)0x1EEE340u;                 /* DBG_EN: the bus traps off */
    *(volatile uint32_t *)0x1EEE340u = en & ~bus;
    __asm__ volatile("csync" ::: "memory");
    for (k = 0; k < n; k++)
        b[k * 1024u] = k ^ 0x5AC3A55Au;
    __asm__ volatile("csync" ::: "memory");
    size = 0;
    {
        uint32_t v = b[0] ^ 0x5AC3A55Au;
        if (v == 0u) {                          /* word 0 is its own: the end is the first that is not */
            for (k = 1; k < n && (b[k * 1024u] ^ 0x5AC3A55Au) == k; k++)
                ;
            size = k * 4096u;
        } else if (v < n) {                     /* word 0 holds a later index's: the bank wraps there (the last
                                                   index on it: n - size / 4 KiB) */
            uint32_t s = (n - v) * 4096u;
            if (s >= 65536u && (s & (s - 1u)) == 0u) size = s;
        }
    }
    if (size >= 65536u) {                       /* three patterns through its first 64 KiB */
        static const uint32_t P[3] = {0x00000000u, 0xFFFFFFFFu, 0xA5A55A5Au};
        uint32_t p;
        for (p = 0; p < 3u && size; p++) {
            for (k = 0; k < 16384u; k++) b[k] = P[p] ^ (k * 0x9E3779B1u);
            __asm__ volatile("csync" ::: "memory");
            for (k = 0; k < 16384u; k++)
                if (b[k] != (P[p] ^ (k * 0x9E3779B1u))) { size = 0; break; }
        }
    } else
        size = 0;
    *(volatile uint32_t *)0x1EEE248u = bus;                /* DBG_MSG_CLR: the flags it may have raised */
    __asm__ volatile("csync" ::: "memory");
    *(volatile uint32_t *)0x1EEE340u = en;
    if (locked && (*(volatile uint32_t *)0x1EEE240u & 1u))
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;
    fm1_ram2_mark.hung = 0;
    __asm__ volatile("csync" ::: "memory");
    return size;
}

static void fm1_ram2_clear(uint32_t n)
{
    volatile uint32_t *w = (volatile uint32_t *)FM1_RAM2_BASE;
    uint32_t i;
    for (i = 0; i < n / 4u; i++) w[i] = 0;
}
