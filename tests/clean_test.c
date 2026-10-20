/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DAKHMEH (after Clipper's tests/clean_test.c, Copyright (C) 2026 3dSam) */
/* No clicks: every knob of the mix and the effects turned under a held chord, the output rendered and looked at
 * for a jump. The metric is the largest second difference |x[i] - 2 x[i-1] + x[i-2]| over the window after the
 * change, against the steady windows before and after it: a glide leaves it where the sound's own motion has it,
 * a step shows up as a spike. The threshold (2 x the steady value + 400, ~-38 dB of full scale) lets the sound's
 * own transients through and catches a parameter applied as a step. Runs against the real firmware translation
 * unit, as desert_test.c. */
#define DINGHY_HOST
#define main firmware_host_main
#include "../firmware/src/grimm.c"
#undef main
#include <stdio.h>

#define NCAP (VB * 600u)
#define JUMP_FLOOR 400                          /* the spike a step makes that we care about (~-38 dBFS) */
static int32_t cl[NCAP], cr[NCAP];
static uint32_t ncap;
static int fails, cases;
static const track_t defaults = TRK_DEF;
typedef void (*change_fn)(uint32_t j);

static void run(uint32_t nb)                    /* nb blocks rendered and kept (the newest NCAP samples) */
{
    uint32_t b, i;
    ncap = 0;
    for (b = 0; b < nb; b++) {
        int32_t out[2 * VB] = {0};
        app_audio(out, VB);
        for (i = 0; i < VB && ncap < NCAP; i++, ncap++) cl[ncap] = out[2 * i], cr[ncap] = out[2 * i + 1];
    }
}
static int32_t d2max(void)
{
    int32_t mx = 0;
    uint32_t i;
    for (i = 2; i < ncap; i++) {
        int32_t a = cl[i] - 2 * cl[i - 1] + cl[i - 2], b = cr[i] - 2 * cr[i - 1] + cr[i - 2];
        a = a < 0 ? -a : a, b = b < 0 ? -b : b;
        mx = a > mx ? a : mx, mx = b > mx ? b : mx;
    }
    return mx;
}
static void clean(void)
{
    unsigned t;
    memset(vc, 0, sizeof vc); nlat = 0; nq_r = nq_w = 0; ec_r = ec_w = 0;
    trk_quiet = trk_mute = trk_solo = 0; tune_f = 0; playing = 0;
    for (t = 0; t < NT; t++) { trk[t] = defaults; lfo_p16[t] = 0; fm_builtin(t, 0); }
    seq_init();
    tf_wow = tf_sat = tf_tone = tf_hiss = 0;
    ge_mix = 0, dl_mix = 0, rv_mix = 0, rv_type = RT_HALL;
    run(40);
}
/* a chord held on track 0 (the others silent) with the engine and the sends asked for, settled; then each change,
 * its window and the steady window after it */
static void check(const char *what, uint32_t eng, change_fn fn, uint32_t k, uint32_t settle)
{
    static const uint8_t ch[3] = {48, 55, 64};
    int32_t before, worst = 0, at = -1;
    uint32_t j, i;
    clean();
    trk[0].eng = (int32_t)eng;
    for (i = 0; i < 3u; i++) note_on(0, 1u + i, ch[i], SRC_KEY, 0, 0, 100);
    run(300);
    run(200);
    before = d2max();
    for (j = 0; j < k; j++) {
        int32_t s, steady;
        fn(j);
        run(settle);
        s = d2max();
        run(150);
        steady = d2max();
        if (s > 2 * (steady > before ? steady : before) + JUMP_FLOOR && s > worst)
            worst = s, at = (int32_t)j;
        before = steady;
    }
    cases++;
    printf("clean: %-60s %s", what, worst ? "FAIL" : "ok");
    if (worst) {
        fails++;
        printf(" (change %d: %d, steady %d)", at, worst, before);
    }
    printf("\n");
}

/* the changes: j is the step (0, 1, 2, ..): the knob goes back and forth */
static void ch_level(uint32_t j) { trk[0].level = j & 1u ? 127 : 40; }
static void ch_pan(uint32_t j) { trk[0].pan = j & 1u ? 0 : 127; }
static void ch_mute(uint32_t j) { trk_mute = j & 1u ? 1u : 0u; }
static void ch_fx(uint32_t j) { trk[0].fx = j & 1u ? 200 : 0; }
static void ch_cut(uint32_t j) { trk[0].cut = j & 1u ? 127 : 30; }
static void ch_res(uint32_t j) { trk[0].cut = 60; trk[0].res = j & 1u ? 127 : 0; }
static void ch_dly_send(uint32_t j) { dl_mix = 100; trk[0].dly = j & 1u ? 127 : 0; }
static void ch_rev_send(uint32_t j) { rv_mix = 100; trk[0].rev = j & 1u ? 127 : 0; }
static void ch_grn_send(uint32_t j) { ge_mix = 100; trk[0].grn = j & 1u ? 127 : 0; }
static void ch_dly_time(uint32_t j) { dl_mix = 100; trk[0].dly = 100; dl_time = (int32_t)((j * 37u) % 128u); }
static void ch_dly_fb(uint32_t j) { dl_mix = 100; trk[0].dly = 100; dl_fb = j & 1u ? 120 : 10; }
static void ch_dly_wear(uint32_t j) { dl_mix = 100; trk[0].dly = 100; dl_wear = j & 1u ? 127 : 0; }
static void ch_dly_mix(uint32_t j) { trk[0].dly = 100; dl_mix = j & 1u ? 127 : 0; }
static void ch_rv_size(uint32_t j) { rv_mix = 100; trk[0].rev = 100; rv_size = j & 1u ? 120 : 10; }
static void ch_rv_inf(uint32_t j) { rv_mix = 100; trk[0].rev = 100; rv_size = j & 1u ? RV_INF : 80; }
static void ch_rv_damp(uint32_t j) { rv_mix = 100; trk[0].rev = 100; rv_damp = j & 1u ? 127 : 0; }
static void ch_rv_mix(uint32_t j) { trk[0].rev = 100; rv_mix = j & 1u ? 127 : 0; }
static void ch_rv_pre(uint32_t j) { rv_mix = 100; trk[0].rev = 100; rv_pre = j & 1u ? 127 : 0; }
static void ch_rv_type(uint32_t j) { rv_mix = 100; trk[0].rev = 100; rv_type = (int32_t)((j + 1u) % (uint32_t)RV_NTYPE); }
static uint32_t type_now;
static void ch_rv_size_t(uint32_t j) { rv_type = (int32_t)type_now; rv_mix = 100; trk[0].rev = 100; rv_size = j & 1u ? 127 : 10; }
static void ch_rv_pre_t(uint32_t j) { rv_type = (int32_t)type_now; rv_mix = 100; trk[0].rev = 100; rv_pre = j & 1u ? 127 : 0; }
static void ch_rv_damp_t(uint32_t j) { rv_type = (int32_t)type_now; rv_mix = 100; trk[0].rev = 100; rv_damp = j & 1u ? 127 : 0; }
static void ch_ge_size(uint32_t j) { ge_mix = 100; trk[0].grn = 100; ge_size = j & 1u ? 127 : 10; }
static void ch_ge_dens(uint32_t j) { ge_mix = 100; trk[0].grn = 100; ge_dens = j & 1u ? 127 : 10; }
static void ch_ge_scat(uint32_t j) { ge_mix = 100; trk[0].grn = 100; ge_scat = j & 1u ? 127 : 0; }
static void ch_ge_mix(uint32_t j) { trk[0].grn = 100; ge_mix = j & 1u ? 127 : 0; }
static void ch_tf_wow(uint32_t j) { tf_wow = j & 1u ? 127 : 1; }
static void ch_tf_sat(uint32_t j) { tf_sat = j & 1u ? 127 : 1; tapefx_poll(); }
static void ch_tf_tone(uint32_t j) { tf_tone = j & 1u ? 127 : 1; }
static void ch_tf_hiss(uint32_t j) { tf_hiss = j & 1u ? 127 : 1; }
static void ch_tf_on(uint32_t j) { tf_wow = j & 1u ? 40 : 0; tf_tone = j & 1u ? 60 : 0; }
static void ch_tune(uint32_t j) { tune_f = j & 1u ? 120 : -117; }
static void ch_env_sus(uint32_t j) { trk[0].env[2] = j & 1u ? 127 : 30; }
static void ch_lfo(uint32_t j) { trk[0].lfo_dest = LD_CUT; trk[0].lfo_rate = 100; trk[0].lfo_depth = j & 1u ? 100 : 0; }
static void ch_eng(uint32_t j) { trk[0].eng = (int32_t)(j % (uint32_t)NENG); nq_put(NQ_RESTART, 0, 0); }
static uint32_t eng_now;
static void ch_knob(uint32_t j)                  /* the engine's four knobs, each to its ends in turn (PHYS's MODEL
                                                  * strikes the new model and STRUCTURE retunes its resonators, which
                                                  * ring up: transients by design, not checked) */
{
    uint32_t k = (j >> 1) & 3u;
    if (eng_now == ENG_PHYS && k <= 1u) return;
    trk[0].p[eng_now][k] = j & 1u ? ENGINES[eng_now]->max[k] : 0;
}

int main(void)
{
    static const char *const ENAME[NENG] = {"VA", "FM", "SAMPLE", "VOICE", "WHEEL", "PHYS", "PHASE"};
    char what[96];
    uint32_t e;
    check("LEVEL 40 <-> 127", ENG_VA, ch_level, 6, 30);
    check("PAN L <-> R", ENG_VA, ch_pan, 6, 30);
    check("MUTE on <-> off", ENG_VA, ch_mute, 6, 40);
    check("FX 0 <-> 200 %", ENG_VA, ch_fx, 6, 30);
    check("CUTOFF 30 <-> 127", ENG_VA, ch_cut, 6, 30);
    check("RESONANCE 0 <-> 127", ENG_VA, ch_res, 6, 30);
    check("DLY SEND 0 <-> 127", ENG_VA, ch_dly_send, 6, 30);
    check("REV SEND 0 <-> 127", ENG_VA, ch_rev_send, 6, 30);
    check("GRAIN SEND 0 <-> 127", ENG_VA, ch_grn_send, 6, 30);
    check("DELAY TIME", ENG_VA, ch_dly_time, 7, 60);
    check("DELAY FEEDBACK 10 <-> 120", ENG_VA, ch_dly_fb, 6, 60);
    check("DELAY WEAR 0 <-> 127", ENG_VA, ch_dly_wear, 6, 60);
    check("DELAY MIX 0 <-> 127", ENG_VA, ch_dly_mix, 6, 40);
    check("SPACE SIZE 10 <-> 120", ENG_VA, ch_rv_size, 6, 60);
    check("SPACE SIZE 80 <-> INF", ENG_VA, ch_rv_inf, 6, 60);
    check("SPACE DAMP 0 <-> 127", ENG_VA, ch_rv_damp, 6, 60);
    check("SPACE MIX 0 <-> 127", ENG_VA, ch_rv_mix, 6, 40);
    check("SPACE PRE-DELAY 0 <-> 250 ms", ENG_VA, ch_rv_pre, 6, 60);
    check("SPACE TYPE round HALL PLATE CAVERN SHIMMER", ENG_VA, ch_rv_type, 8, 40);
    for (type_now = RT_PLATE; type_now < (uint32_t)RV_NTYPE; type_now++) {
        snprintf(what, sizeof what, "SPACE %s: SIZE 10 <-> 127", RV_TYPE_NAME[type_now]);
        check(what, ENG_VA, ch_rv_size_t, 6, 60);
        snprintf(what, sizeof what, "SPACE %s: DAMP 0 <-> 127", RV_TYPE_NAME[type_now]);
        check(what, ENG_VA, ch_rv_damp_t, 6, 60);
        snprintf(what, sizeof what, "SPACE %s: PRE-DELAY 0 <-> 54 ms", RV_TYPE_NAME[type_now]);
        check(what, ENG_VA, ch_rv_pre_t, 6, 60);
    }
    check("GRAIN SIZE 10 <-> 127", ENG_VA, ch_ge_size, 6, 60);
    check("GRAIN DENSITY 10 <-> 127", ENG_VA, ch_ge_dens, 6, 60);
    check("GRAIN SCATTER 0 <-> 127", ENG_VA, ch_ge_scat, 6, 60);
    check("GRAIN MIX 0 <-> 127", ENG_VA, ch_ge_mix, 6, 40);
    check("TAPE WOW 1 <-> 127", ENG_VA, ch_tf_wow, 6, 40);
    check("TAPE SATURATION 1 <-> 127", ENG_VA, ch_tf_sat, 6, 40);
    check("TAPE TONE 1 <-> 127", ENG_VA, ch_tf_tone, 6, 40);
    check("TAPE HISS 1 <-> 127", ENG_VA, ch_tf_hiss, 6, 40);
    check("TAPE on <-> off", ENG_VA, ch_tf_on, 6, 40);
    check("TUNE -50 <-> +50 cents", ENG_VA, ch_tune, 6, 30);
    check("ENV SUSTAIN 30 <-> 127", ENG_VA, ch_env_sus, 6, 40);
    check("LFO DEPTH 0 <-> 100 on CUTOFF", ENG_VA, ch_lfo, 6, 40);
    check("the engine changed under the chord", ENG_VA, ch_eng, 2u * NENG, 60);
    for (e = 0; e < (uint32_t)NENG; e++) {
        eng_now = e;
        snprintf(what, sizeof what, "%s: its four knobs to their ends", ENAME[e]);
        check(what, e, ch_knob, 8, 40);
    }
    printf("clean: %d cases\n", cases);
    printf("%s\n", fails ? "CLEAN FAIL" : "CLEAN OK");
    return fails ? 1 : 0;
}
