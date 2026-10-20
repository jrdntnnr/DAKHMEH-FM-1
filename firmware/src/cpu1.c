/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab)
 * DAKHMEH changes (the host double in this file, no menu switch): Copyright (C) 2026 DAKHMEH */
/* The second core of the AC7911, as the render sees it: one job at a time.
 *   cpu1_run(fn, arg)  hands a job over: 1 = core 1 took it, 0 = run it yourself (no second core, or held)
 *   cpu1_wait()        0 = the job is done; -1 = core 1 did not finish in time: it is held from now on and
 *                      the caller runs the job itself, once
 *   cpu1_on()          1 while core 1 runs and answers
 * Between run and wait the caller must not touch what the job reads or writes. On the FM-1 (hal/fm1_cpu1.h,
 * X0X's): started at power-on (platform.c fm1_cstart, before the audio and the timers), proved with a self-test
 * job, shown on the SYSTEM pages' header ("2C" / "1C"). The waiting loop lives in RAM (.c1_text), so an idle core 1
 * never fetches from the flash and flash writes need no parking. voices.c hands it a share of the tracks each
 * control step (voices_render).
 *
 * On a computer (DINGHY_HOST) and in the browser (GRIMM_WEB) a double stands in, so the same render code is proven
 * there (tests/split_test.c):
 *   CORES=1 (default)   the job runs on this thread, but only at cpu1_wait: what the caller does between run and
 *                       wait must not depend on the job, as on the device (one thread: deterministic)
 *   CORES=2             the job runs on a POSIX thread, started at the first cpu1_run; cpu1_wait blocks for it.
 *                       Built with -fsanitize=thread, every shared access shows up
 *   CHAOS=N             with CORES=2: the thread sleeps 0..10 N us before and after each job (seeded by N), so
 *                       nothing may depend on how fast core 1 is
 *   CPU1_TIMEOUT_AT=K   the K-th job is lost: the double never runs it, cpu1_wait returns -1 and core 1 is held
 *                       from then on: the caller's fallback path, once
 *   CPU1_SPLIT=0        nothing is handed over (as cpu1_split 0): the one-core render to compare against
 * The browser has one thread and no environment: CORES=1 as it is. */
typedef void (*cpu1_job_t)(uint32_t);
static volatile int32_t cpu1_split = 1;         /* 1: voices.c hands tracks to core 1 (0: all on this core; the output
                                                 * is the same bit for bit either way) */
enum { CPU1_OFF, CPU1_NOSTART, CPU1_FAILED, CPU1_OK };   /* not built / did not report in / self-test wrong / running */
static uint8_t cpu1_state = CPU1_OFF;
static uint32_t cpu1_timeouts;                  /* jobs core 1 did not finish in time */
static uint32_t cpu1_jobs;                      /* jobs handed over */

#if defined DINGHY_HOST || defined GRIMM_WEB
/* ---- the double */
#include <stdlib.h>
#ifndef GRIMM_WEB
#include <pthread.h>
#include <unistd.h>
#endif
static struct {
    int init, mode, chaos, on, pending;         /* pending: a job handed over and not waited yet */
    uint32_t timeout_at, seed;
    cpu1_job_t fn;
    uint32_t arg;
#ifndef GRIMM_WEB
    pthread_t th;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int th_started, go, done;
#endif
} c1h;

#ifndef GRIMM_WEB
static uint32_t c1h_rand(void) { c1h.seed = c1h.seed * 1664525u + 1013904223u; return c1h.seed >> 8; }
static void *cpu1_host_thread(void *p)
{
    (void)p;
    for (;;) {
        cpu1_job_t fn;
        uint32_t arg;
        pthread_mutex_lock(&c1h.mu);
        while (!c1h.go)
            pthread_cond_wait(&c1h.cv, &c1h.mu);
        c1h.go = 0;
        fn = c1h.fn;
        arg = c1h.arg;
        pthread_mutex_unlock(&c1h.mu);
        if (c1h.chaos)
            usleep(c1h_rand() % (10u * (uint32_t)c1h.chaos + 1u));
        fn(arg);
        if (c1h.chaos)
            usleep(c1h_rand() % (10u * (uint32_t)c1h.chaos + 1u));
        pthread_mutex_lock(&c1h.mu);
        c1h.done = 1;
        pthread_cond_signal(&c1h.cv);
        pthread_mutex_unlock(&c1h.mu);
    }
    return 0;
}
#endif
static void cpu1_host_init(void)
{
    if (c1h.init)
        return;
    c1h.init = 1;
    c1h.mode = 1;
    c1h.on = 1;
#ifndef GRIMM_WEB
    {
        const char *e = getenv("CORES");
        c1h.mode = e && atoi(e) >= 2 ? 2 : 1;
        e = getenv("CHAOS");
        c1h.chaos = e ? atoi(e) : 0;
        c1h.seed = (uint32_t)c1h.chaos * 7919u + 1u;
        e = getenv("CPU1_TIMEOUT_AT");
        c1h.timeout_at = e ? (uint32_t)atoi(e) : 0;
        e = getenv("CPU1_SPLIT");
        if (e)
            cpu1_split = atoi(e);
        pthread_mutex_init(&c1h.mu, 0);
        pthread_cond_init(&c1h.cv, 0);
    }
#endif
    cpu1_state = CPU1_OK;
}
static void cpu1_boot(void) { cpu1_host_init(); }
static int cpu1_on(void) { cpu1_host_init(); return c1h.on; }
static int cpu1_run(cpu1_job_t fn, uint32_t arg)
{
    cpu1_host_init();
    if (!c1h.on)
        return 0;
    if (c1h.pending)
        abort();                                /* two jobs at once: the protocol is one at a time */
    cpu1_jobs++;
    c1h.pending = 1;
    c1h.fn = fn;
    c1h.arg = arg;
    if (c1h.timeout_at && cpu1_jobs == c1h.timeout_at)
        return 1;                               /* lost: cpu1_wait says so */
#ifndef GRIMM_WEB
    if (c1h.mode == 2) {
        pthread_mutex_lock(&c1h.mu);
        if (!c1h.th_started) {
            pthread_create(&c1h.th, 0, cpu1_host_thread, 0);
            c1h.th_started = 1;
        }
        c1h.go = 1;
        c1h.done = 0;
        pthread_cond_signal(&c1h.cv);
        pthread_mutex_unlock(&c1h.mu);
    }
#endif
    return 1;
}
static int cpu1_wait(void)
{
    if (!c1h.pending)
        return 0;                               /* nothing handed over, or already waited */
    c1h.pending = 0;
    if (c1h.timeout_at && cpu1_jobs == c1h.timeout_at) {
        cpu1_timeouts++;
        c1h.on = 0;                             /* held: one core from here on */
        cpu1_state = CPU1_FAILED;
        return -1;
    }
#ifndef GRIMM_WEB
    if (c1h.mode == 2) {
        pthread_mutex_lock(&c1h.mu);
        while (!c1h.done)
            pthread_cond_wait(&c1h.cv, &c1h.mu);
        pthread_mutex_unlock(&c1h.mu);
        return 0;
    }
#endif
    c1h.fn(c1h.arg);                            /* inline, deferred to here: as late as the device's */
    return 0;
}
#else
/* ---- the FM-1 (hal/fm1_cpu1.h) */
static volatile uint32_t cpu1_test_out;
static int cpu1_on(void) { return cpu1_state == CPU1_OK && fm1_c1_on; }
static int cpu1_run(cpu1_job_t fn, uint32_t arg)
{
    if (cpu1_state != CPU1_OK || !fm1_cpu1_run(fn, arg))
        return 0;
    cpu1_jobs++;
    return 1;
}
static int cpu1_wait(void)
{
    if (fm1_cpu1_wait() == 0)
        return 0;
    cpu1_timeouts++;                            /* fm1_cpu1_wait held the core: one core from here on */
    cpu1_state = CPU1_FAILED;
    return -1;
}
static uint32_t cpu1_lcg(uint32_t x)            /* 64 steps of an LCG: the self-test's work */
{
    uint32_t i;
    for (i = 0; i < 64u; i++)
        x = x * 1664525u + 1013904223u;
    return x;
}
static void cpu1_selftest(uint32_t arg)         /* runs on core 1, from the flash, as the render will */
{
    cpu1_test_out = cpu1_lcg(arg);
}
/* at power-on, before the audio and the timers (X0X: started later, core 1 never reached its entry). The reserved
 * top of RAM is not locked yet (fm1_guard_lock_top comes after): its entry word can be written. The PC limits are
 * opened for the start (it boots through the chip's ROM) and armed again after. */
static void cpu1_boot(void)
{
    fm1_guard_pc_open();
    if (fm1_cpu1_start() != 0) {
        fm1_guard_enable(FM1_GUARD_PC);
        cpu1_state = CPU1_NOSTART;
        return;
    }
    fm1_guard_enable(FM1_GUARD_PC);
    cpu1_test_out = 0;
    if (!fm1_cpu1_run(cpu1_selftest, 20261010u) || fm1_cpu1_wait() != 0 || cpu1_test_out != cpu1_lcg(20261010u)) {
        fm1_cpu1_hold();                        /* answered, but wrong or late: one core from here on */
        cpu1_state = CPU1_FAILED;
        return;
    }
    cpu1_state = CPU1_OK;
}
#endif
/* the SYSTEM pages' header, after the version: both cores at work, or one */
static const char *cpu1_label(void) { return cpu1_state == CPU1_OK && cpu1_split ? "2C" : "1C"; }
