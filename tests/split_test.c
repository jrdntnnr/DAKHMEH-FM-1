/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DAKHMEH */
/* The second core's share of the render (voices.c, cpu1.c): the four tracks on four engines, a chord each, held,
 * changed and released over 400 blocks, rendered with the split off (CPU1_SPLIT=0: one core) and on; the two
 * outputs must match bit for bit, and with the split on core 1 must have been handed work. Run it as
 * tests/run_host.sh does: CORES=1 (the job inline at the wait), CORES=2 (a thread), CORES=2 CHAOS=5 (the thread
 * slow at random), CPU1_TIMEOUT_AT=40 (a job lost: the fallback), and CORES=2 under -fsanitize=thread. */
#define DINGHY_HOST
#define main firmware_host_main
#include "../firmware/src/grimm.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static const track_t defaults = TRK_DEF;
static void clean(void)
{
    unsigned t;
    memset(vc, 0, sizeof vc); nlat = 0; nq_r = nq_w = 0; ec_r = ec_w = 0;
    trk_quiet = trk_mute = trk_solo = 0; tune_f = 0; playing = 0;
    for (t = 0; t < NT; t++) { trk[t] = defaults; lfo_p16[t] = 0; fm_builtin(t, 0); }
    seq_init();
    memset(&vx_g, 0, sizeof vx_g); memset(&ph_slot, 0, sizeof ph_slot); memset(&drw_v, 0, sizeof drw_v);
    memset(&fm_note, 0, sizeof fm_note); memset(&fm_trk, 0, sizeof fm_trk); memset(mix_g, 0, sizeof mix_g);
    for (t = 0; t < NT; t++) formant_nz[t] = 0x2545F491 + (int32_t)t * 0x9E3779B9;
    tf_wow = tf_sat = tf_tone = tf_hiss = 0;
    ge_mix = 0, dl_mix = 0, rv_mix = 0;
}
static uint32_t render_hash(uint32_t blocks)
{
    static const uint8_t ENG[NT] = {ENG_FM, ENG_PHYS, ENG_VA, ENG_WHEEL};
    uint32_t h = 2166136261u, b, i, t;
    clean();
    for (t = 0; t < NT; t++) {
        trk[t].eng = ENG[t];
        note_on(t, 1, 48 + 3 * t, SRC_KEY, 0, 0, 100);
        note_on(t, 2, 55 + 3 * t, SRC_KEY, 0, 0, 90);
        note_on(t, 3, 64 + 3 * t, SRC_KEY, 0, 0, 110);
    }
    for (b = 0; b < blocks; b++) {
        int32_t out[2 * VB] = {0};
        if (b == 100) trk[2].eng = ENG_VOICE, nq_put(NQ_RESTART, 2, 0);   /* a source changed under its notes */
        if (b == 150) trk[0].p[ENG_FM][1] = 100;                          /* a knob */
        if (b == 200) note_off(1, 2, SRC_KEY, 0);                         /* a note let go */
        if (b == 260) trk[3].eng = ENG_PHASE, nq_put(NQ_RESTART, 3, 0);
        if (b == 300) for (t = 0; t < NT; t++) note_off(t, 1, SRC_KEY, 0), note_off(t, 3, SRC_KEY, 0);
        app_audio(out, VB);
        for (i = 0; i < 2 * VB; i++)
            h = (h ^ (uint32_t)out[i]) * 16777619u;
    }
    return h;
}

/* each render in a child forked from here: the same memory to start from, whatever the firmware keeps (Clipper's
 * split_test does the same); back come the hash, the jobs handed over and the jobs lost */
static int run_child(int split, uint32_t *r)
{
    int fd[2], st;
    pid_t pid;
    if (pipe(fd)) return -1;
    pid = fork();
    if (pid < 0) return -1;
    if (!pid) {
        uint32_t out[3];
        close(fd[0]);
        cpu1_split = split;
        cpu1_jobs = 0;
        out[0] = render_hash(400);
        out[1] = cpu1_jobs;
        out[2] = cpu1_timeouts;
        if (write(fd[1], out, sizeof out) != (ssize_t)sizeof out) _exit(2);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], r, 12) != 12) return -1;
    close(fd[0]);
    waitpid(pid, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}

int main(void)
{
    uint32_t one[3], two[3];
    if (getenv("SPLIT_NOFORK")) {               /* under -fsanitize=thread (no fork: TSan and fork fall out on a Mac):
                                                 * the split render alone, for the races it would show */
        cpu1_split = 1;
        printf("split: two cores %08x, %u jobs handed over (no comparison: SPLIT_NOFORK)\n", render_hash(400), cpu1_jobs);
        printf("SPLIT OK\n");
        return 0;
    }
    if (run_child(0, one) || run_child(1, two)) { printf("SPLIT FAIL: a render died\n"); return 1; }
    printf("split: one core %08x, two %08x, %u jobs handed over, %u lost\n", one[0], two[0], two[1], two[2]);
    if (one[0] != two[0]) { printf("SPLIT FAIL: the outputs differ\n"); return 1; }
    if (!two[1]) { printf("SPLIT FAIL: core 1 was handed nothing\n"); return 1; }
    if (getenv("CPU1_TIMEOUT_AT") && !two[2]) { printf("SPLIT FAIL: the lost job was not lost\n"); return 1; }
    printf("SPLIT OK\n");
    return 0;
}
