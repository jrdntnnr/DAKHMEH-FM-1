/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 SARADC: PB6 = MASTER pot (ch4),
 * PB1 = battery divider (ch3). Polled, 10 bit, from the main loop.
 *
 *   fm1_adc_init()        both pins analog inputs, no pulls
 *   fm1_adc_read(ch)      0..1023, -1 on timeout
 */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"

#ifndef FM1_ADC_CON
#define FM1_ADC_CON (*(volatile uint32_t *)0x13100u)
#define FM1_ADC_RES (*(volatile uint32_t *)0x13104u)
#define FM1_WLA_CON0 (*(volatile uint32_t *)0x11900u)
#endif
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };

/* Both are only ever analog inputs: PB1 held low for 8 s would reset the
 * chip (isd_config). */
static void fm1_adc_init(void)
{
    uint32_t m = (1u << 6) | (1u << 1);
    FM1_PR(FM1_PB, FM1_DIE) &= ~m;
    FM1_PR(FM1_PB, FM1_PU) &= ~m;
    FM1_PR(FM1_PB, FM1_PD) &= ~m;
    FM1_PR(FM1_PB, FM1_DIR) |= m;
}

/* Deferred scan conversions: divider 7 / delay 2, measured by fm1-velocity. */
static void fm1_adc_start(uint32_t ch)
{
    FM1_ADC_CON = 0;
    FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = 0x204Fu | ((ch & 15u) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
}
static int fm1_adc_done(void) { return (FM1_ADC_CON & 0x80u) != 0; }
static void fm1_adc_abort(void) { FM1_ADC_CON = 0x40u; FM1_ADC_CON = 0; }
static uint32_t fm1_adc_take(void)
{
    uint32_t v = FM1_ADC_RES & 1023u;
    fm1_adc_abort();
    return v;
}
/* Main-loop readers use the cache while the scan owns the single converter. */
static volatile uint8_t fm1_adc_owned;
static volatile int32_t fm1_adc_master = -1, fm1_adc_batt = -1;
static int32_t fm1_adc_read(uint32_t ch)
{
    uint32_t t, v;
    if (fm1_adc_owned) return ch == FM1_ADC_MASTER ? fm1_adc_master : ch == FM1_ADC_BATT ? fm1_adc_batt : -1;
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = 0xF04Eu | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
    for (t = 0; t < 20000u; t++)
        if (FM1_ADC_CON & 0x80u)
            break;
    v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return t == 20000u ? -1 : (int32_t)v;
}
