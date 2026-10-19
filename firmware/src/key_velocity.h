/* SPDX-License-Identifier: GPL-3.0-only */
/* DAKHMEH contact velocity (ported from SARDAB, ghoulbox-fm1-dungeon-synth firmware/src/key_velocity.h). Sensing method from Charles Vestal's fm1-velocity
 * (8f62ce0): weak-drive PA7/PA8, PA10 probe for PA6. Bounded manual curves here;
 * no adaptive calibration, pressure, capture buffers or event queue.
 * The scan owns capture[]. It publishes each landing as one aligned word.
 * The main loop owns wait/played/velocity (grimm.c kv_keys). Digital debounce owns release. */
#pragma once
#define DAKHMEH_KEY_VELOCITY 1
#include <stdint.h>
#define KV_NKEYS 27u
#define KV_READY (1u << 16)
#define KV_TIMEOUT_MS 14u
static volatile uint32_t kv_config; /* mode bit0, curve bits1..2, touch bits3..4, top-probe bit5 */
static volatile uint8_t kv_hw_on;
static struct {
    volatile uint32_t landing;
    uint16_t best, baseline;
    uint8_t contact, count, baseline_valid;
} kv_capture[KV_NKEYS];
static uint32_t kv_seen, kv_wait[KV_NKEYS];
static uint8_t kv_played[KV_NKEYS], kv_velocity[KV_NKEYS];

/* Scan only. Release invalidates the landing before the next attack. */
static void kv_contact(uint32_t k, int down)
{
    if (k >= KV_NKEYS || kv_capture[k].contact == !!down) return;
    kv_capture[k].contact = (uint8_t)!!down;
    kv_capture[k].count = 0;
    kv_capture[k].best = k < 20u ? 1023u : 0u;
    kv_capture[k].landing = 0;
}
static void kv_sample(uint32_t k, uint32_t value)
{
    if (k >= KV_NKEYS || value > 1023u) return;
    if (!kv_capture[k].contact) {
        if (k >= 20u) {
            kv_capture[k].baseline = (uint16_t)(kv_capture[k].baseline_valid ?
                (7u * kv_capture[k].baseline + value + 4u) / 8u : value);
            kv_capture[k].baseline_valid = 1;
        }
        return;
    }
    if (kv_capture[k].landing & KV_READY) return;
    if (k < 20u) {
        /* Open/rail readings are not a quiet note: wait, then use FIXED fallback. */
        if (value >= 1018u || value < 200u) return;
        if (value < kv_capture[k].best) kv_capture[k].best = (uint16_t)value;
    } else {
        if (!kv_capture[k].baseline_valid || value <= kv_capture[k].baseline || value >= 1018u) return;
        value -= kv_capture[k].baseline;
        if (value > kv_capture[k].best) kv_capture[k].best = (uint16_t)value;
    }
    if (++kv_capture[k].count >= 2u)
        kv_capture[k].landing = KV_READY | kv_capture[k].best;
}
/* Audio only; curves are monotonic, bounded, and do not touch pitch. */
static uint8_t kv_map(uint32_t k, uint32_t raw, uint32_t cfg)
{
    static const uint16_t floor_[3] = {550, 620, 480}, ceiling[3] = {850, 950, 780};
    uint32_t touch = (cfg >> 3) & 3u, curve = (cfg >> 1) & 3u, x;
    int32_t n;
    if (touch > 2u) touch = 0;
    if (k < 20u) n = ((int32_t)ceiling[touch] - (int32_t)raw) * 126 / (ceiling[touch] - floor_[touch]);
    else { /* PA10 probe has its own scale; strongest readings saturate upstream too. */
        static const uint8_t lo[3] = {20, 16, 24}, hi[3] = {48, 42, 54};
        n = ((int32_t)raw - lo[touch]) * 126 / (hi[touch] - lo[touch]);
    }
    x = n < 0 ? 0u : n > 126 ? 126u : (uint32_t)n;
    if (curve == 1u) x = x * (252u - x) / 126u;
    else if (curve == 2u) x = x * x / 126u;
    return (uint8_t)(1u + x);
}
/* A key is delayed only at onset. Already sounding notes never change velocity
 * or disappear when settings change. Timeout uses the performance FIXED value.
 * Bypass is for UI key layers / boot: their presses are never delayed. */
static __attribute__((noinline)) uint32_t kv_keys(uint32_t raw, uint32_t now, uint32_t fixed, int bypass, uint32_t held)
{
    uint32_t cfg = kv_config, out = raw, k;
    if (!(cfg & 1u) || !kv_hw_on) { kv_seen = 0; return raw; }
    for (k = 0; k < KV_NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((raw & held) & bit) continue; /* mode changes never release/retrigger a held note */
        if (!(raw & bit)) { kv_played[k] = 0; kv_velocity[k] = 0; continue; }
        if (!(kv_seen & bit)) { kv_wait[k] = now; kv_played[k] = 0; }
        if (!kv_played[k]) {
            uint32_t landing = kv_capture[k].landing;
            int sense = (cfg & 1u) && kv_hw_on && !bypass && (k < 20u || (cfg & 32u));
            if (sense && !(landing & KV_READY) && (uint32_t)(now - kv_wait[k]) < KV_TIMEOUT_MS) {
                out &= ~bit;
                continue;
            }
            kv_velocity[k] = sense && (landing & KV_READY) ? kv_map(k, landing & 1023u, cfg) :
                (uint8_t)(fixed < 1u ? 1u : fixed > 127u ? 127u : fixed);
            kv_played[k] = 1;
        }
    }
    kv_seen = raw;
    return out;
}
