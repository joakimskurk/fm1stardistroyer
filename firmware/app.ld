/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * 0x01C00000..0x01C07F00 holds .ram_text (code that must run while the flash
 * is busy: the flash driver; the second core's idle loop, .c1_text; then the app's hot audio code, .ram_audio,
 * which may call into XIP: the audio never runs while the flash is off); 0x01C07F00..0x01C08000 is crt0's STATUS.
 * RAM  : .data/.bss (96 KiB)
 * POOL : big zero-initialised buffers (FX lines, canvases), section .pool
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x7F00
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x54000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(.text.memcpy) *(.text.memset) . = ALIGN(64); _sd_hot = .; *(.text.sd_hot)
              _sd_hot_end = .; . = _sd_hot + 0x8800; *(.rodata) *(.rodata.cst*) . = ALIGN(4); *(.rodata*) . = ALIGN(4); *(.text*)
              . = ALIGN(4); _etext = .; } > XIP
                                                /* (memcpy and memset first: the RAM code calls them directly, and a
                                                   call reaches 4 MB, the flash's first few KB from the RAM; then the
                                                   audio's flash code at a fixed place, sd_engine.h's SD_HOT, in 34 KB
                                                   of its own: what follows starts at one address whatever it holds, so
                                                   a change inside it moves nothing else. 34 KB measured best (the gate
                                                   53.5 %, everything on 63.6; 36 and 38 KB put the tables on worse
                                                   sets: 55.7 / 66.2, 56.2 / 66.7). Past 34 KB the link fails (the
                                                   location counter would go back): then add 8 KB, the cache's period
                                                   (128 sets of 64 B), 0xA800 keeps the same sets) */
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); _c1_start = .; KEEP(*(.c1_text*)) . = ALIGN(4);
                 _c1_end = .; _ra_start = .;
                 *(.ram_audio*) . = ALIGN(4); _ra_end = .; _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
