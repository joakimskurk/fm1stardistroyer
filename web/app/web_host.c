/* SPDX-License-Identifier: GPL-3.0-only */
/* Star Distroyer in a browser: an FM-1 for the page (web/app/), as tests/host.c is one for a computer. platform.c
 * includes this file instead of tests/host.c when DINGHY_WEB is set (with DINGHY_HOST: the app's host branches). The
 * firmware's own code runs on it, compiled to WebAssembly (web/app/build.sh), no imports, no C library:
 *
 *   web_init()              the app's start (the splash runs from here, a pass a millisecond)
 *   web_run(n)              n stereo frames of its sound into web_out (L R L R, +-32767): the app's main loop runs a
 *                           pass for each millisecond those frames are worth, the sound in its blocks of 256 frames,
 *                           as the audio interrupt renders it
 *   web_lcd(), web_lcd_rev()  the screen, 240 x 240 RGB565 (CPU order), and a count that moves when it changes
 *   web_leds(i), web_glow(i)  the LEDs, 32 ids each (i 0: ids 0..31, 1: 32..63): 0..13 the buttons, 14..40 the keys;
 *                           lit, and the dim layer
 *   web_key(k, down)        note key k (0 = F3 .. 26 = G5) down or up
 *   web_btn(id, down)       a button (BTN_*) down or up (SCL held: the second layer)
 *   web_enc(id, n)          an encoder turned n detents (ENC_*, + = clockwise)
 *   web_midi(st, d1, d2, src)  a MIDI message in, from input src (1..255; 0 as 1): each of the page's MIDI inputs
 *                           is a source of its own, as USB and the jack are on the FM-1, so a clock is taken from
 *                           one at a time (the same clock on two ports leads once, not at twice its tempo)
 *   web_store(), web_store_size(), web_store_rev()   the flash (settings, blobs, slots: store.h's sizes): the page
 *                           keeps a copy in the browser when the count moves, and puts it back before web_init
 *
 * A press and its release within one pass both count: the release waits a pass, so the app sees the press. */
#include <stdint.h>
#include <stddef.h>

#define WEB_EXPORT(name) __attribute__((export_name(#name)))

void *memcpy(void *d, const void *s, size_t n)
{
    volatile uint8_t *a = d;
    const uint8_t *b = s;
    while (n--) *a++ = *b++;
    return d;
}
void *memmove(void *d, const void *s, size_t n)
{
    volatile uint8_t *a = d;
    const uint8_t *b = s;
    if (a < b) while (n--) *a++ = *b++;
    else while (n--) a[n] = b[n];
    return d;
}
void *memset(void *d, int c, size_t n)
{
    volatile uint8_t *a = d;
    while (n--) *a++ = (uint8_t)c;
    return d;
}
int memcmp(const void *x, const void *y, size_t n)
{
    const uint8_t *a = x, *b = y;
    for (; n; n--, a++, b++)
        if (*a != *b) return *a - *b;
    return 0;
}

#ifndef APP_NAME
#define APP_NAME "DINGHY"
#endif
#ifndef APP_VER
#define APP_VER "0.0"
#endif
#define FS 44100u
#define HALF_FRAMES 256u

static volatile uint32_t fm1_ms;
#define HOST_NENC 7
static struct { uint32_t notes, buttons; } fm1_in;
static uint32_t host_edges, host_rel;           /* buttons pressed, released, since the app last asked */
static uint32_t btn_fresh, btn_up_next, btn_up_due;   /* pressed this pass; released this pass (due the next) */
static uint32_t key_fresh, key_up_next, key_up_due;
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t b = host_edges;
    host_edges = 0;
    *released = host_rel;
    host_rel = 0;
    return b;
}
static uint32_t fm1_input_note_edges(void) { return 0; }
static int32_t host_enc[HOST_NENC];
static int32_t fm1_enc_take(uint32_t e) { int32_t v = host_enc[e % HOST_NENC]; host_enc[e % HOST_NENC] = 0; return v; }
static uint64_t host_leds, host_glow;
static void fm1_irq_off(void) {}                 /* (the sound is rendered between the main loop's passes) */
static void fm1_irq_on(void) {}
static void fm1_led_key(uint32_t id, int on) { if (on) host_leds |= (uint64_t)1 << id; else host_leds &= ~((uint64_t)1 << id); }
static void fm1_led_key_glow(uint32_t id, int on) { if (on) host_glow |= (uint64_t)1 << id; else host_glow &= ~((uint64_t)1 << id); }

/* the screen: 240 x 240 RGB565, in the CPU's order (gfx.c sends the bytes swapped, as the ST7789 wants them) */
static uint16_t fb[240 * 240];
static uint32_t fb_rev;
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
    fb_rev++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)(c >> 8 | c << 8);
            }
    fb_rev++;
}
#ifndef CV_MAX
#define CV_MAX (240u * 64u)
#endif
#include "gfx.c"

/* the flash, in RAM: each place a byte "used", three of length, then its bytes (store.h's sizes); the page keeps it */
#include "store.h"
static struct {
    uint8_t set[SECTOR_MAX + 4], blob[NBLOB][SECTOR_MAX + 4], slot[NSLOT][SLOT_MAX + 4];
} web_flash;
static uint32_t flash_rev;
static int host_load(uint8_t *a, uint32_t max, void *p, uint32_t n)
{
    if (!a[0] || n > max || (uint32_t)(a[1] | a[2] << 8 | a[3] << 16) != n) return -1;
    memcpy(p, a + 4, n);
    return 0;
}
static int host_save(uint8_t *a, uint32_t max, const void *p, uint32_t n)
{
    if (n > max) return -1;
    a[0] = 1; a[1] = (uint8_t)n; a[2] = (uint8_t)(n >> 8); a[3] = (uint8_t)(n >> 16);
    memcpy(a + 4, p, n);
    flash_rev++;
    return 0;
}
static int settings_load(void *p, uint32_t n) { return host_load(web_flash.set, SECTOR_MAX, p, n); }
static int settings_save(const void *p, uint32_t n) { return host_save(web_flash.set, SECTOR_MAX, p, n); }
static int blob_load(uint32_t i, void *p, uint32_t n) { return i < NBLOB ? host_load(web_flash.blob[i], SECTOR_MAX, p, n) : -1; }
static int blob_save(uint32_t i, const void *p, uint32_t n) { return i < NBLOB ? host_save(web_flash.blob[i], SECTOR_MAX, p, n) : -1; }
static int slot_load(uint32_t i, void *p, uint32_t n) { return i < NSLOT ? host_load(web_flash.slot[i], SLOT_MAX, p, n) : -1; }
static int slot_save(uint32_t i, const void *p, uint32_t n) { return i < NSLOT ? host_save(web_flash.slot[i], SLOT_MAX, p, n) : -1; }

static uint8_t mq[256][4];
static uint32_t mq_w, mq_r;
static int midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)   /* its source, or 0: none */
{
    int src;
    if (mq_r == mq_w) return 0;
    *st = mq[mq_r % 256u][0]; *d1 = mq[mq_r % 256u][1]; *d2 = mq[mq_r % 256u][2];
    src = mq[mq_r % 256u][3];
    mq_r++;
    return src;
}
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static void app_audio(int32_t *out, uint32_t n);
static void app_main(void);

/* the sound: a ring the passes fill, a block at a time; web_run hands it out */
#define RING 8192u                              /* frames */
static int32_t ring[2u * RING];
static uint32_t ring_w, ring_r;                 /* frames written, read */
static int32_t web_out[2u * 1024u];

static void platform_poll(void)                 /* one call = one millisecond, as on the host */
{
    static uint32_t owed;
    static int32_t blk[2u * HALF_FRAMES];
    fm1_ms++;
    host_rel |= btn_up_due;                     /* the releases a pass late */
    fm1_in.buttons &= ~btn_up_due;
    fm1_in.notes &= ~key_up_due;
    btn_up_due = btn_up_next; btn_up_next = 0; btn_fresh = 0;
    key_up_due = key_up_next; key_up_next = 0; key_fresh = 0;
    owed += FS;
    while (owed >= 1000u * HALF_FRAMES) {
        uint32_t i;
        owed -= 1000u * HALF_FRAMES;
        for (i = 0; i < 2u * HALF_FRAMES; i++) blk[i] = 0;
        app_audio(blk, HALF_FRAMES);
        for (i = 0; i < HALF_FRAMES; i++) {
            uint32_t k = (ring_w + i) % RING;
            ring[2u * k] = blk[2u * i];
            ring[2u * k + 1u] = blk[2u * i + 1u];
        }
        ring_w += HALF_FRAMES;
    }
}

WEB_EXPORT(web_init) void web_init(void)
{
    static int started;
    if (started) return;
    started = 1;
    app_main();                                 /* its start and the splash's first pass (app_main: a pass a call) */
}
WEB_EXPORT(web_run) uint32_t web_run(uint32_t n)
{
    uint32_t i, guard = 0;
    if (n > 1024u) n = 1024u;
    while (ring_w - ring_r < n && guard++ < 1000u)
        app_main();
    for (i = 0; i < n; i++) {
        uint32_t k = (ring_r + i) % RING;
        web_out[2u * i] = ring[2u * k];
        web_out[2u * i + 1u] = ring[2u * k + 1u];
    }
    ring_r += n;
    return n;
}
WEB_EXPORT(web_out_ptr) int32_t *web_out_ptr(void) { return web_out; }
WEB_EXPORT(web_lcd) uint16_t *web_lcd(void) { return fb; }
WEB_EXPORT(web_lcd_rev) uint32_t web_lcd_rev(void) { return fb_rev; }
WEB_EXPORT(web_ms) uint32_t web_ms(void) { return fm1_ms; }
WEB_EXPORT(web_leds) uint32_t web_leds(uint32_t i) { return (uint32_t)(host_leds >> (i ? 32 : 0)); }
WEB_EXPORT(web_glow) uint32_t web_glow(uint32_t i) { return (uint32_t)(host_glow >> (i ? 32 : 0)); }
WEB_EXPORT(web_key) void web_key(uint32_t k, uint32_t down)
{
    uint32_t b;
    if (k >= 27u) return;
    b = 1u << k;
    if (down) { fm1_in.notes |= b; key_fresh |= b; key_up_next &= ~b; key_up_due &= ~b; }
    else if (key_fresh & b) key_up_next |= b;
    else fm1_in.notes &= ~b;
}
WEB_EXPORT(web_btn) void web_btn(uint32_t id, uint32_t down)
{
    uint32_t b;
    if (id >= 14u) return;
    b = 1u << id;
    if (down) { host_edges |= b; fm1_in.buttons |= b; btn_fresh |= b; btn_up_next &= ~b; btn_up_due &= ~b; }
    else if (btn_fresh & b) btn_up_next |= b;
    else if (fm1_in.buttons & b) { host_rel |= b; fm1_in.buttons &= ~b; }
}
WEB_EXPORT(web_enc) void web_enc(uint32_t id, int32_t n) { host_enc[id % HOST_NENC] += n; }
WEB_EXPORT(web_midi) void web_midi(uint32_t st, uint32_t d1, uint32_t d2, uint32_t src)
{
    if (mq_w - mq_r >= 256u) return;            /* (full: dropped, as a full ring on the FM-1 would) */
    mq[mq_w % 256u][0] = (uint8_t)st; mq[mq_w % 256u][1] = (uint8_t)d1; mq[mq_w % 256u][2] = (uint8_t)d2;
    mq[mq_w % 256u][3] = (uint8_t)(src - 1u < 255u ? src : 1u);
    mq_w++;
}
WEB_EXPORT(web_store) uint8_t *web_store(void) { return (uint8_t *)&web_flash; }
WEB_EXPORT(web_store_size) uint32_t web_store_size(void) { return (uint32_t)sizeof web_flash; }
WEB_EXPORT(web_store_rev) uint32_t web_store_rev(void) { return flash_rev; }
