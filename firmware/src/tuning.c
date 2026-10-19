/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDAB: per-track pitch, independent of physical keyboard layout.
 * Adapted from SARDAB tuning.c. Presets are editable starting intonations,
 * not a model of maqam melodic practice. Included after track_t. */
#include "scales.h"
#define NSCALE ((int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]))
enum { MAP_CHR, MAP_SNAP, MAP_SEQ, MAP_WHITE, MAP_ALL, NMAP };
static const char *const MAP_NAME[NMAP] = {"CHR", "SNAP", "SEQ", "WHITE", "ALL"};
static const char *const TEMP_NAME[4] = {"EQUAL", "PYTH", "MEAN", "JUST"};
static const int16_t TEMPER[4][12] = {
    {0},
    {0, -98, 39, -59, 78, -20, 117, 20, -78, 59, -39, 98},
    {0, 171, -68, 103, -137, 34, -205, -34, 137, -103, 68, -171},
    {0, 117, 39, 156, -137, -20, -98, 20, 137, -156, -39, -117},
};
static int32_t tuning_dc(uint32_t t, uint32_t note)
{
    const track_t *T = &trk[t % NT];
    uint32_t pc = (note + 12u - (uint32_t)clamp(T->root, 0, 11)) % 12u;
    if (T->tuning_mode == 1) /* Arabic keyboard: absolute C..B, no maqam/temperament stacking. */
        return T->tash_on[note % 12u] ? 10 * clamp(T->tash_cents[note % 12u], -100, 100) : 0;
    return (T->tuning_system ? clamp(T->table[pc], -2000, 2000) :
        10 * SCALE_CENTS[clamp(T->scale, 0, NSCALE - 1)][pc] + TEMPER[clamp(T->temper, 0, 3)][pc]) +
        10 * clamp(T->trim[pc], -63, 63);
}
static int32_t tuning_p16(uint32_t t, uint32_t note)
{
    return (int32_t)note * 16 + tuning_dc(t, note) * 16 / 1000;
}
static int32_t tuning_fine_dc(int32_t c)
{
    int32_t q = c * 16 / 1000;
    int32_t r = (c * 16 - q * 1000) * 2367;
    return (r + (r < 0 ? -80000 : 80000)) / 160000;
}
static int32_t tuning_fine(uint32_t t, uint32_t note) { return tuning_fine_dc(tuning_dc(t, note)); }
static int32_t voice_tuning_dc(const voice_t *v)
{
    return trk[v->trk].retune == 2 ? v->tuning_start : trk[v->trk].retune == 1 ? v->tuning_now : tuning_dc(v->trk, v->note);
}
static void voice_tuning_tick(voice_t *v)
{
    int32_t target = tuning_dc(v->trk, v->note), d = target - v->tuning_now;
    if (trk[v->trk].retune == 1 && d) v->tuning_now += d / 32 + (d > 0 ? 1 : -1);
    else if (!trk[v->trk].retune) v->tuning_now = target;
}
static int tuning_precise(uint32_t t)
{
    uint32_t i;
    if (trk[t].tuning_mode || trk[t].tuning_system || trk[t].retune || trk[t].scale >= 16 || trk[t].temper) return 1;
    for (i = 0; i < 12u; i++) if (trk[t].trim[i]) return 1;
    return 0;
}
/* Signed multiply, no 64-bit divide; called per control block, never per sample. */
static uint32_t tuning_rate(uint32_t rate, int32_t fine)
{
    return (uint32_t)((int64_t)rate + ((int64_t)rate * fine >> 12));
}
