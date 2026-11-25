/* SPDX-License-Identifier: GPL-3.0-only */
/* Application-level recovery, not a second firmware bank. The intact app's
 * startup, RAM flash driver and USB stack are still required. TIMER5, audio,
 * settings, samples, projects, editor and normal UI are never started here. */
static uint32_t recovery_last, recovery_fraction, recovery_usb_last;

static void recovery_poll(void)
{
    uint32_t now = fm1_ticks(), elapsed = now - recovery_last;
    recovery_last = now;
    /* Split before adding: also safe across the 32-bit TIMER4 wrap. */
    fm1_ms += elapsed / (1000u * FM1_TICKS_PER_US);
    recovery_fraction += elapsed % (1000u * FM1_TICKS_PER_US);
    fm1_ms += recovery_fraction / (1000u * FM1_TICKS_PER_US);
    recovery_fraction %= 1000u * FM1_TICKS_PER_US;
    fm1_wdt_feed();
    if (now - recovery_usb_last >= 500u * FM1_TICKS_PER_US) {
        recovery_usb_last = now;
        usb_poll();
    }
}

static void recovery_step(void)
{
    recovery_poll();
    usb_retry(fm1_ms);
    if (usb.uboot_req) {
        usb_detach();
        fm1_delay_ms(30);
        bootguard_clear(&bootguard);
        fm1_enter_uboot();
        return;
    }
    ota_service();
    if (usb.ota_req) {
        usb.ota_req = 0;
        if (flash_ok) ota_session();         /* same validated updater as normal mode */
    }
}

static uint32_t recovery_seen[8];              /* what the two looks for OCT- read, for the diagnosis over USB
                                                   (2026-11-25, a user's FM-1 started as ever with OCT- held): each the
                                                   buttons, the rows seen in the OCT keys' columns (0 | 1 << 8) and the
                                                   key rows of any (<< 16), the frames, TIMER4 at its start */
static int recovery_key(uint32_t *rec)
{
    uint32_t start = fm1_ticks(), c0 = 0, c1 = 0, any = 0, n = 0, p;
    fm1_input_init();
    do {
        fm1_wdt_feed();
        fm1_input_scan();
        c0 |= fm1_in.raw[0];
        c1 |= fm1_in.raw[1];
        for (p = 0; p < FM1_NCOL; p++) any |= fm1_in.raw[p] & 0x1Eu;
        n++;
    } while ((uint32_t)(fm1_ticks() - start) < 50u * 1000u * FM1_TICKS_PER_US);
    rec[0] = fm1_in.buttons;
    rec[1] = c0 | c1 << 8 | any << 16;
    rec[2] = n;
    rec[3] = start;
    return bootguard_manual(fm1_in.buttons);
}

static void recovery_main(void)
{
    recovery_active = 1;
    bootguard.pending = 2;                   /* WDT/exception here falls back to ROM */
    recovery_last = recovery_usb_last = fm1_ticks();
    recovery_fraction = fm1_ms = 0;
    /* No persist_boot or OTA cleanup: no flash write just for entering rescue. */
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
    lcd_sync();                              /* the second look comes after the boot's clear: its last strip is done
                                                but still pending, and lcd_init's SPI reset clears that flag, so the
                                                panel's first command waited for it for ever (on an FM-1 only: the
                                                emulator reads an idle SPI as done) and the watchdog sent it to ROM */
    lcd_init();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 64, 240, &FONT_S, APP_NAME " USB RESCUE", C_WHITE, 1);
    draw_text_box(0, 96, 240, &FONT_S, "CONNECT USB", C_WHITE, 1);
    draw_text_box(0, 124, 240, &FONT_S,
                  flash_ok ? "OPEN THE INSTALLER" : "UNKNOWN FLASH", C_WHITE, 1);
    draw_text_box(0, 164, 240, &FONT_S, "AUDIO OFF", C_WHITE, 1);
    lcd_sync();
    usb_start();
    for (;;) recovery_step();
}
