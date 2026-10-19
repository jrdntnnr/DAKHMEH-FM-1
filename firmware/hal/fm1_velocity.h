/* SPDX-License-Identifier: GPL-3.0-only */
/* ADC/weak-drive technique adapted from charlesvestal/fm1-velocity, 8f62ce0,
 * firmware/hal/fm1_input.h (Felucca platform, Leo Kuroshita / Hügelton Instruments).
 * Integrated with SARDAB's existing scan/LED timing. No blocking conversions.
 * Finish/restore precedes every digital row read; start follows the LED sweep. */
#pragma once
#include "fm1_adc.h"
#include "../src/key_velocity.h"
static struct {
    uint8_t pending, phase, row, col, driven, probe, channel;
    uint32_t config, timeouts;
} fm1_kv;
static void fm1_kv_drive(uint32_t row, uint32_t strength)
{
    uint32_t m = 1u << (row == 3u ? 7 : row == 4u ? 8 : 6);
    if (strength & 1u) FM1_PR(FM1_PA, FM1_HD0) |= m;
    else FM1_PR(FM1_PA, FM1_HD0) &= ~m;
    FM1_PR(FM1_PA, FM1_HD) &= ~m;
    FM1_PR(FM1_PA, FM1_OUT) |= m;
    FM1_PR(FM1_PA, FM1_DIR) &= ~m;
}
static void fm1_kv_restore(void)
{
    if (fm1_kv.driven) {
        uint32_t m = 1u << (fm1_kv.row == 3u ? 7 : fm1_kv.row == 4u ? 8 : 6);
        FM1_PR(FM1_PA, FM1_DIR) |= m;
        FM1_PR(FM1_PA, FM1_OUT) &= ~m;
        FM1_PR(FM1_PA, FM1_HD0) &= ~m;
        FM1_PR(FM1_PA, FM1_HD) &= ~m;
    }
    if (fm1_kv.probe) {
        FM1_PR(FM1_PA, FM1_PU) &= ~(1u << 10);
        FM1_PR(FM1_PA, FM1_DIR) &= ~(1u << 10);
    }
}
static __attribute__((noinline)) void fm1_kv_finish(void)
{
    if (!fm1_kv.pending) return;
    /* Conversion has had a full 100 us; sampled result survives pin restoration. */
    fm1_kv_restore();
    if (fm1_adc_done()) {
        uint32_t v = fm1_adc_take();
        if (fm1_kv.row) {
            int id = FM1_KEYMAP[fm1_kv.row][fm1_kv.col];
            if (id >= 14 && (fm1_kv.driven || fm1_kv.probe)) kv_sample((uint32_t)id - 14u, v);
        } else if (fm1_kv.channel == FM1_ADC_MASTER) fm1_adc_master = (int32_t)v;
        else fm1_adc_batt = (int32_t)v;
    } else { fm1_adc_abort(); fm1_kv.timeouts++; }
    fm1_kv.pending = fm1_kv.driven = fm1_kv.probe = 0;
}
static __attribute__((noinline)) void fm1_kv_contacts(uint32_t col)
{
    uint32_t r;
    if (!kv_hw_on) return;
    for (r = 2; r <= 4u; r++) {
        int id = FM1_KEYMAP[r][col];
        if (id >= 14) kv_contact((uint32_t)id - 14u, (fm1_in.notes >> (id - 14)) & 1u);
    }
}
static __attribute__((noinline)) void fm1_kv_start(uint32_t col)
{
    uint32_t cfg = kv_config, row = 0, ch;
    if (!fm1_adc_owned) return;
    kv_hw_on = (uint8_t)(cfg & 1u);
    if (cfg != fm1_kv.config) {
        uint32_t k;
        for (k = 0; k < KV_NKEYS; k++) { kv_capture[k].landing = 0; kv_capture[k].count = 0; kv_capture[k].best = k < 20u ? 1023u : 0u; }
        fm1_kv.config = cfg;
    }
    if (!kv_hw_on && col > 1u) return;
    if (col == 0u) fm1_kv.phase = (uint8_t)((fm1_kv.phase + 1u) % ((cfg & 32u) ? 3u : 2u));
    /* Two non-note PA8 columns also provide MASTER / battery slots. In fixed
     * mode all columns alternate those channels; key GPIO stays untouched. */
    ch = col & 1u ? FM1_ADC_BATT : FM1_ADC_MASTER;
    if (kv_hw_on) {
        if (fm1_kv.phase == 0u) { row = 3; ch = 0; }
        else if (fm1_kv.phase == 1u && col >= 2u) { row = 4; ch = 1; }
        else if (fm1_kv.phase == 2u && col < 7u) { row = 2; ch = 2; }
    }
    fm1_kv.col = (uint8_t)col;
    fm1_kv.row = (uint8_t)row;
    fm1_kv.channel = (uint8_t)ch;
    if (row == 2u) {
        /* This column's other LEDs are already set. PA10 alone becomes analog. */
        FM1_PR(FM1_PA, FM1_OUT) &= ~(1u << 10);
        FM1_PR(FM1_PA, FM1_PU) |= 1u << 10;
        FM1_PR(FM1_PA, FM1_DIR) |= 1u << 10;
        fm1_kv_drive(row, 0); /* weakest, measured by Velo */
        fm1_kv.probe = fm1_kv.driven = 1;
    } else if (row && kv_capture[(uint32_t)FM1_KEYMAP[row][col] - 14u].contact) {
        fm1_kv_drive(row, 1); /* Velo DRIVE 2 */
        fm1_kv.driven = 1;
    }
    fm1_adc_start(ch);
    fm1_kv.pending = 1;
}
