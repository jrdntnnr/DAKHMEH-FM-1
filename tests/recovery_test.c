/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DAKHMEH (after SLOOP's tests/recovery_test.c, isod89) */
/* The USB rescue's key test (recovery.c recovery_key) with the hardware mocked, and the one fact SLOOP 2.7.2 measured
 * on an FM-1: the key matrix reads only while PA2, the backlight (active low), is driven low. The ROM and the loaders
 * leave it undriven and the LCD init comes after this scan, so OCT- held at power-on was never seen. The mock gives
 * keys only once RECOVERY_KEYS_ON() has run; the test holds OCT- and expects the rescue to see it. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/bootguard.h"
static bootguard_t bootguard;
static uint32_t ticks, fm1_ms, scans, feeds;
static uint8_t recovery_active, flash_ok;
static struct { uint8_t ota_req, uboot_req; } usb;
static struct { uint32_t buttons; } fm1_in;
static uint32_t input_buttons;
static unsigned keys_on, scans_before_on;
#define FM1_TICKS_PER_US 24u
#define FL_FAR(x) x
#define APP_NAME "DAKHMEH"
#define C_BLACK 0
#define C_WHITE 65535
static int FONT_S;
static uint32_t fm1_ticks(void) { return ticks; }
static void fm1_wdt_feed(void) { feeds++; }
static void usb_poll(void) {}
static void usb_retry(uint32_t t) { (void)t; }
static void usb_detach(void) {}
static void fm1_delay_ms(uint32_t n) { ticks += n * 24000u; }
static void fm1_enter_uboot(void) {}
static void ota_service(void) {}
static int ota_session(void) { return -13; }
static void fm1_input_init(void) { fm1_in.buttons = 0; }
#define RECOVERY_KEYS_ON() (keys_on++, scans_before_on += scans)   /* the matrix powered: PA2 driven low */
static void fm1_input_scan(void) { scans++; ticks += 24000u; fm1_in.buttons = keys_on ? input_buttons : 0u; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void lcd_init(void) {}
static void lcd_fill(int x, int y, int w, int h, int c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void draw_text_box(int x, int y, int w, const void *f, const char *s, int c, int a)
{ (void)x; (void)y; (void)w; (void)f; (void)s; (void)c; (void)a; }
static void lcd_sync(void) {}
static void usb_start(void) {}
#include "../firmware/src/recovery.c"

static int fails;
static void check(const char *what, int ok) { printf("rescue: %-62s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

int main(void)
{
    input_buttons = 1;                       /* OCT- held through power-on */
    check("OCT- held at power-on opens the rescue", recovery_key());
    check("the matrix powered (PA2 low) before the first scan", keys_on == 1 && scans_before_on == 0);
    check("the scan ran its 50 ms", scans >= 40 && feeds >= 40);
    input_buttons = 3;
    check("OCT- with another key: no rescue", !recovery_key());
    input_buttons = 0;
    check("nothing held: no rescue", !recovery_key());
    printf("%s\n", fails ? "RESCUE FAIL" : "RESCUE OK");
    return fails ? 1 : 0;
}
