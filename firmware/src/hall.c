/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * GR!MM changes: Copyright (C) 2026 GR!MM
 * DAKHMEH changes (the plate family, after Clipper's fx.c): Copyright (C) 2026 DAKHMEH */
/* GR!MM's SPACE: Felucca's HALL reverb (hugelton/Felucca firmware/src/fx.c: rev_hall, hall_ap, hall_end(_lp),
 * sh_tz, HL_RUNS), taken over without Felucca's buses, made longer and darker. As Felucca's:
 *   at half the rate (22.05 kHz: the same memory holds twice the time, half the work; the tail stops near 10 kHz):
 *   the send, every other sample low-passed 1 2 1 -> a low cut (~110 Hz: no boom in a long tail) -> allpass
 *   diffusers -> 8 lines, mutually prime lengths, that all feed each other through a conference matrix (Paley,
 *   order 8: every line feeds the 7 others, +-1 each, none itself), lines 1 2 4 7 read through slow sines (0.37 /
 *   0.74 Hz: the resonances drift instead of ringing); each line's gain from its length (all decay alike); outputs
 *   left 0 3 4 -7, right 1 2 5 6; rounding that lets a quiet tail end in exact silence; back to 44.1 kHz between
 *   samples.
 * GR!MM's: lines of 39 .. 74 ms (Felucca's 21 .. 34) and 4 diffusers (Felucca's 2): bigger; DAMP's low-pass on all
 * 8 lines (Felucca's 4), down to ~650 Hz: darker; RT60 0.4 .. ~32 s (Felucca's 0.5 .. 9 s), and SIZE 127 = INF:
 * the matrix lossless but for 1e-4 a pass, the loop's damping off and the send faded out (a loop that keeps its
 * energy and still takes more can only grow: INF holds what is in it, as a freeze); a soft knee on everything that
 * goes back into the lines, so nothing feeding back can clip or run away; PRE-DELAY 0 .. 250 ms before the
 * diffusers; MIX: the return (the wet's level: the tracks' REV sends set how much goes in); nothing run at MIX 0 (the tail waits)
 * or once the tank is empty and nothing comes in.
 *
 * DAKHMEH's TYPE (SPACE 2): HALL is the above. PLATE, CAVERN and SHIMMER are Clipper's plate (isod89/clipper-fm1
 * firmware/src/fx.c plate_step, GPL-3.0; after J. Dattorro, "Effect Design, Part 1", JAES 45/9, 1997: the lengths
 * at 29.76 kHz x 0.741 for 22.05): the send through a bandwidth low-pass and four allpass diffusers into a figure
 * of eight of two modulated diffusers (+-12 samples at ~1 Hz), two delays, a damping low-pass and two more
 * diffusers each; left and right each a sum of seven taps. PLATE: decay 0.2 .. 0.8 a pass; CAVERN: a longer decay
 * (0.5 .. 0.9), a darker input, 18 ms more pre-delay; SHIMMER: CAVERN with an eighth of the tail an octave up fed
 * back in (two taps sweeping a 46 ms window at twice the speed, triangle-windowed). The plate family shares HALL's
 * memory (a union: the FM-1's pool is full), so its PRE-DELAY stops at 54 ms, and SIZE 127 is its longest decay, not
 * a freeze. A TYPE change fades the return out (~35 ms), clears the memory, switches and fades back in: no two
 * reverbs run at once, no cut.
 *
 * The audio ISR calls hall_render with the REV bus (the tracks' REV sends and DELAY's echoes); the main loop sets the
 * five parameters. Its buffers are in the pool (zero at boot): ~39 KB. */

#define RV_NL 8u                                /* lines */
#define RV_NAP 4u                               /* diffusers */
#define RV_NC 128u                              /* a block at half the rate: app_audio's 256 frames */
#define RV_MOD 10                               /* the drifting taps' swing, samples (read 2 .. 22 short of the end) */
#define RV_DRIFT 0x96u                          /* the lines that drift: 1 2 4 7 (Felucca's) */
#define RV_LOUD 32                              /* a line this loud (two samples' sum) rounds its gain to the nearest */
#define RV_KD(d) (32767 - (d) * 210)            /* DAMP d's low-pass (Q15): none at 0 .. ~650 Hz at 127 */
#define RV_IN 10320                             /* the send's level (Q15): 4 x Felucca's, the lines 12 dB hotter: the tail
                                                 * holds 12 dB longer before its last LSBs round away */
#define RV_OUT 5                                /* the outputs' gain, / 4 (RV_IN's 4 x back out) */
#define RV_PRE_MAX 5513u                        /* PRE-DELAY at most 250 ms (22.05 kHz) */
#define RV_PRE_LEN 5760u                        /* its line: that and a block (and a little) */
#define RV_LINES (853u + 941u + 1031u + 1123u + 1237u + 1361u + 1493u + 1637u + RV_NL)   /* (+1 each: the mirror) */
#define RV_APS (149u + 211u + 337u + 499u)
#define RV_EMPTY 14u                            /* blocks of silence in and out of the lines: all of them silent (1637 / 128) */
static const uint16_t RV_LEN[RV_NL] = {853, 941, 1031, 1123, 1237, 1361, 1493, 1637};
static const uint16_t RV_AP[RV_NAP] = {149, 211, 337, 499};
static const int16_t RV_APG[RV_NAP] = {11469, 11469, 10240, 10240};   /* Q14: 0.7, 0.7, 0.625, 0.625 (Felucca's two) */
_Static_assert(RV_NC + 2u * RV_MOD + 2u < 853u && RV_NC <= 149u, "SPACE: one wrap a block at most, in place");
_Static_assert(RV_PRE_MAX + RV_NC <= RV_PRE_LEN, "SPACE: the pre-delay's line");

/* the plate family's lines (Clipper's PL_LEN, but PRE: HALL's memory bounds it) */
enum { PL_AP1, PL_AP2, PL_AP3, PL_AP4, PL_MA, PL_DA1, PL_AA, PL_DA2, PL_MB, PL_DB1, PL_AB, PL_DB2, PL_PRE, PL_N };
#define PL_EXC 12                               /* the modulated diffusers' excursion (+- samples) */
#define PL_PRE_LEN 1200u                        /* PRE-DELAY at most 54 ms (22.05 kHz) */
#define PL_CAVE_PRE 400u                        /* CAVERN: 18 ms more */
static const uint16_t PL_LEN[PL_N] = {105, 79, 281, 205, 498 + PL_EXC + 2, 3299, 1334, 2756, 673 + PL_EXC + 2, 3124,
                                      1968, 2343, PL_PRE_LEN};
#define PL_TOTAL (105u + 79u + 281u + 205u + 512u + 3299u + 1334u + 2756u + 687u + 3124u + 1968u + 2343u + PL_PRE_LEN)
#define PS_LEN 1024u                            /* the octave's line: 46 ms */
#define PS_WIN 896u                             /* the window its taps sweep, samples */
#define SH_DECAY 27000                          /* SHIMMER: the decay and the damping capped (the octaves pile up) */
#define SH_DAMP 28000
#define SH_GAIN 8192                            /* .. and how much of the tail comes back an octave up (Q15: 1/4) */
#define PL_OUT 9830                             /* the taps' sum x 0.6 (Q14): as loud as HALL at the same SIZE */
enum { RT_HALL, RT_PLATE, RT_CAVERN, RT_SHIMMER, RV_NTYPE };
static const char *const RV_TYPE_NAME[RV_NTYPE] = {"HALL", "PLATE", "CAVERN", "SHIMMER"};
#define RV_FADE_STEP 5461                       /* a TYPE change: the return's gain a sixth a block down, then up (~35 ms) */

typedef struct {                                /* HALL's state */
    uint16_t i[RV_NL], d[RV_NAP], pw;           /* the lines' write indices, the diffusers', the pre-delay's */
    int32_t lp[RV_NL], le[RV_NL];               /* the low-passes and their steps' remainders */
    int32_t rp[RV_NL];                          /* the drifting taps at the end of the last block (Q8 samples) */
    uint32_t ph;                                /* the taps' drift */
    int16_t y[RV_NL][RV_NC];                    /* the lines' ends, then what goes back into them */
} rv_hall_t;
typedef struct {                                /* the plate family's */
    uint16_t base[PL_N], w[PL_N];               /* each line's start in the buffer and its write index */
    int32_t bw, dmp[2], end[2];                 /* the bandwidth low-pass, the two damping low-passes, the two ends */
    uint32_t lph, pre;                          /* the diffusers' modulation; the pre-delay now (gliding a sample a step) */
    struct { uint32_t w, ph; int32_t lp; } ps;  /* the octave's write index, its taps' phase, its low-pass */
} rv_plate_t;
static union {                                  /* the type in use has the memory (rv.type) */
    struct { int16_t line[RV_LINES], apb[RV_APS], pbuf[RV_PRE_LEN]; rv_hall_t st; } h;
    struct { int16_t buf[PL_TOTAL], ps[PS_LEN]; rv_plate_t st; } p;
} rv_mem __attribute__((section(".pool")));
#define rv_line rv_mem.h.line
#define rv_apb rv_mem.h.apb
#define rv_pbuf rv_mem.h.pbuf
#define pl_buf rv_mem.p.buf
#define ps_buf rv_mem.p.ps
static struct {                                 /* every type's: the send in, the return out */
    int32_t x, hp, he, ml, mr, send;            /* the send's last sample, its low cut (and remainder); the last wet
                                                 * out, left / right; the send's level (Q15: 0 at INF) */
    uint32_t quiet, empty;                      /* half-rate samples with nothing coming in; blocks with nothing in
                                                 * the lines */
    int32_t bx[RV_NC], l[RV_NC], r[RV_NC];      /* the block at half the rate: the diffused send, left, right */
    int32_t cut;                                /* what the return's gain lacks (Q15; 0 at power-on: full): up to
                                                 * 32767 on a TYPE change, then back down */
    uint8_t type, ready;                        /* the type the memory holds; its state begun */
} rv __attribute__((section(".pool")));
#define RVH rv_mem.h.st
#define RVP rv_mem.p.st

/* the parameters, 0..127, written by the main loop */
static volatile int32_t rv_size = 80, rv_damp = 48, rv_mix = 100, rv_pre = 16;  /* 6.4 s, ~2.5 kHz, return 79 %, 31 ms */
static volatile int32_t rv_type = RT_HALL;      /* SPACE 2: TYPE */
#define RV_INF 127

/* Felucca's helpers: m / 2^sh rounded towards zero (a product that feeds back must not floor: a loop of floors holds
 * an offset and a little noise for ever) */
static inline int32_t sh_tz(int32_t m, uint32_t sh) { return (m + ((m >> 31) & ((1 << sh) - 1))) >> sh; }
/* an allpass diffuser over x[0 .. n) from q (its next samples, no wrap): its state rounded towards zero */
static inline void hall_ap(int16_t *q, int32_t g, int32_t *x, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t y = q[i], v = x[i] + sh_tz(y * g, 14);
        q[i] = (int16_t)clamp(v, -32768, 32767);
        x[i] = y - ((v * g + 8192) >> 14);
    }
}
/* a line's end over y[0 .. n) from b (no wrap), through the low-pass (kd, Q15; its state *lp and its step's
 * remainder *le), times its gain g (Q14): rounded to the nearest while the line is loud, towards zero once it is
 * quiet (lo: the tail ends in exact silence). Felucca's hall_end_lp */
static inline void hall_end_lp(const int16_t *b, int16_t *y, uint32_t n, int32_t g, int lo, int32_t *lp, int32_t *le,
                               int32_t kd)
{
    uint32_t i;
    int32_t l = *lp, r = *le, e;
    for (i = 0; i < n; i++) {
        e = (b[i] - l) * kd + r, r = e & 32767, l += e >> 15;
        y[i] = (int16_t)(lo ? sh_tz(l * g, 14) : (l * g + 8192) >> 14);
    }
    *lp = l;
    *le = r;
}
/* a drifting line's end over the block: r0 -> r1 (Q8 samples ahead of the write index w, gliding over the block:
 * GR!MM's block is 8 of Felucca's, too long to hold a tap still), between samples (b[len] is b[0]: the mirror), then
 * as hall_end_lp */
static inline void hall_end_drift(const int16_t *b, uint32_t len, uint32_t w, int32_t r0, int32_t r1, int16_t *y,
                                  int32_t g, int lo, int32_t *lp, int32_t *le, int32_t kd)
{
    uint32_t i, p;
    int32_t l = *lp, re = *le, e, dr = r1 - r0;
    for (i = 0; i < RV_NC; i++) {
        int32_t r = r0 + ((dr * (int32_t)i) >> 7), f = r & 255, x;
        p = w + i + (uint32_t)(r >> 8);
        p = p >= len ? p - len : p;
        x = b[p] + (((b[p + 1u] - b[p]) * f + 128) >> 8);
        e = (x - l) * kd + re, re = e & 32767, l += e >> 15;
        y[i] = (int16_t)(lo ? sh_tz(l * g, 14) : (l * g + 8192) >> 14);
    }
    *lp = l;
    *le = re;
}
/* the block's RV_NC samples of a circular buffer of len from index d in at most two runs, no wrap inside them:
 * body with s (the run's start in the block) and k (its length); d moves on (Felucca's HL_RUNS) */
#define RV_RUNS(d, len, k, s, body)                                                         \
    for (s = 0; s < RV_NC; s += k) {                                                        \
        k = (len) - (d) < RV_NC - s ? (len) - (d) : RV_NC - s;                              \
        body;                                                                               \
        d = (d) + k >= (len) ? 0u : (d) + k;                                                \
    }
/* the drift's sine: a full cycle over 2^32 -> -32767..32767 (a refined parabola, ~0.1 %: Dinghy's) */
static int32_t rv_sine(uint32_t ph)
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}
#define RV_LFO 9224927u                         /* the drift: 0.37 Hz (even lines), 0.74 Hz (odd): 2^32 0.37 256 / 44100 a block */

/* main loop: the reverb gives nothing out (MIX 0, or the tank empty and nothing coming in) */
static int hall_idle(void) { return rv_mix <= 0 || (rv.quiet >= RV_PRE_LEN && rv.empty >= RV_EMPTY); }

/* HALL: the block's diffused send (rv.bx) through the lines and the matrix -> rv.l, rv.r (half rate); 1 if anything
 * is left in the lines */
static uint32_t hall_block(int32_t size, int32_t damp)
{
    int16_t *b, *a;
    uint32_t i, k, s, m, d, full = 0;
    int32_t inf = size >= RV_INF, kd = inf ? 32767 : RV_KD(damp);
    const int16_t *g = RV_G[size < 0 ? 0 : size > RV_INF ? RV_INF : size];

    a = rv_apb;                                 /* the diffusers */
    for (k = 0; k < RV_NAP; k++) {
        d = RVH.d[k];
        RV_RUNS(d, RV_AP[k], m, s, hall_ap(a + d, RV_APG[k], rv.bx + s, m));
        RVH.d[k] = (uint16_t)d;
        a += RV_AP[k];
    }

    RVH.ph += RV_LFO;                             /* the lines' ends */
    b = rv_line;
    for (k = 0; k < RV_NL; k++) {
        int32_t lo;
        d = RVH.i[k];
        s = d + RV_LEN[k] / 2u;                 /* quiet? (two samples half a line apart) */
        s = s >= RV_LEN[k] ? s - RV_LEN[k] : s;
        lo = (b[d] < 0 ? -b[d] : b[d]) + (b[s] < 0 ? -b[s] : b[s]) < RV_LOUD;
        if ((RV_DRIFT >> k) & 1u) {             /* RV_MOD + 2 +- RV_MOD samples short of the end, between samples */
            int32_t r = ((RV_MOD + 2) << 8) + ((rv_sine(RVH.ph * (1u + (k & 1u)) + k * 0x2C000000u) * RV_MOD) >> 7);
            hall_end_drift(b, RV_LEN[k], d, RVH.rp[k], r, RVH.y[k], g[k], lo, &RVH.lp[k], &RVH.le[k], kd);
            RVH.rp[k] = r;
        } else {                                /* (the line's end) */
            RV_RUNS(d, RV_LEN[k], m, s, hall_end_lp(b + d, RVH.y[k] + s, m, g[k], lo, &RVH.lp[k], &RVH.le[k], kd));
        }
        b += RV_LEN[k] + 1u;
    }

    for (i = 0; i < RV_NC; i++) {               /* the outputs and the matrix */
        int32_t y0 = RVH.y[0][i], y1 = RVH.y[1][i], y2 = RVH.y[2][i], y3 = RVH.y[3][i];
        int32_t y4 = RVH.y[4][i], y5 = RVH.y[5][i], y6 = RVH.y[6][i], y7 = RVH.y[7][i], x = rv.bx[i];
        int32_t t, p0, p1, p2, p3, p4, p5, p6;
        rv.l[i] = ((y0 + y3 + y4 - y7) * RV_OUT) >> 2;
        rv.r[i] = ((y1 + y2 + y5 + y6) * RV_OUT) >> 2;
        /* C = [0 1; -1 Q], Q (7 x 7) = chi(j - i), the quadratic character mod 7 (+1 at distances 1 2 4, -1 at 3 5
         * 6): row 1 + i of Q y = 2 (the three at +1) - (the sum of the 7) + y_1+i (Felucca's) */
        t = y1 + y2 + y3 + y4 + y5 + y6 + y7;
        p0 = y2 + y3 + y5, p1 = y3 + y4 + y6, p2 = y4 + y5 + y7, p3 = y5 + y6 + y1;
        p4 = y6 + y7 + y2, p5 = y7 + y1 + y3, p6 = y1 + y2 + y4;
        y0 += t;
        /* back into the lines through a soft knee (linear to half scale; GR!MM's: nothing feeding back clips) */
        RVH.y[0][i] = (int16_t)clamp(soft_knee(t + x, 16384), -32767, 32767);
        RVH.y[1][i] = (int16_t)clamp(soft_knee(2 * p0 + y1 - y0 + x, 16384), -32767, 32767);
        RVH.y[2][i] = (int16_t)clamp(soft_knee(2 * p1 + y2 - y0 + x, 16384), -32767, 32767);
        RVH.y[3][i] = (int16_t)clamp(soft_knee(2 * p2 + y3 - y0 + x, 16384), -32767, 32767);
        RVH.y[4][i] = (int16_t)clamp(soft_knee(2 * p3 + y4 - y0 + x, 16384), -32767, 32767);
        RVH.y[5][i] = (int16_t)clamp(soft_knee(2 * p4 + y5 - y0 + x, 16384), -32767, 32767);
        RVH.y[6][i] = (int16_t)clamp(soft_knee(2 * p5 + y6 - y0 + x, 16384), -32767, 32767);
        RVH.y[7][i] = (int16_t)clamp(soft_knee(2 * p6 + y7 - y0 - x, 16384), -32767, 32767);
        full |= (uint32_t)(rv.l[i] | rv.r[i] | x);
        for (k = 0; k < RV_NL; k++)
            full |= (uint32_t)RVH.y[k][i];
    }

    b = rv_line;                                /* back into the lines (and the mirror after a wrap) */
    for (k = 0; k < RV_NL; k++) {
        d = RVH.i[k];
        RV_RUNS(d, RV_LEN[k], m, s, for (i = 0; i < m; i++) b[d + i] = RVH.y[k][s + i]; if (!d) b[RV_LEN[k]] = b[0]);
        RVH.i[k] = (uint16_t)d;
        b += RV_LEN[k] + 1u;
    }
    return full != 0;
}

/* ---- the plate family (Clipper's plate_step; its lines in pl_buf, each from RVP.base[k], written at RVP.w[k]) */
static inline int32_t pl_tap(uint32_t k, uint32_t d)   /* line k, d samples back (1 the newest, PL_LEN[k] the oldest) */
{
    uint32_t i = RVP.w[k] + PL_LEN[k] - d;
    return pl_buf[RVP.base[k] + (i >= PL_LEN[k] ? i - PL_LEN[k] : i)];
}
static inline void pl_put(uint32_t k, int32_t x)
{
    pl_buf[RVP.base[k] + RVP.w[k]] = (int16_t)clamp(x, -32768, 32767);
    if (++RVP.w[k] >= PL_LEN[k])
        RVP.w[k] = 0;
}
static inline int32_t pl_ap(uint32_t k, int32_t x, int32_t c)   /* a diffuser: an allpass over line k, c Q15 */
{
    int32_t d = pl_tap(k, PL_LEN[k]), v = x - mulq15(d, c);
    pl_put(k, v);
    return d + mulq15(v, c);
}
static inline int32_t pl_mod_ap(uint32_t k, int32_t x, int32_t c, int32_t pos)   /* pos: the delay, Q8 samples */
{
    uint32_t di = (uint32_t)pos >> 8;
    int32_t a = pl_tap(k, di), b = pl_tap(k, di + 1u), d = a + (((b - a) * (pos & 255)) >> 8), v = x - mulq15(d, c);
    pl_put(k, v);
    return d + mulq15(v, c);
}
/* SHIMMER's octave: in through a 46 ms line, two taps half a window apart read at twice the speed (the delay shrinks
 * a sample a step), each under a triangle (the two sum to 1), a little low-passed (a bright tail's octave is harsh) */
static int32_t shimmer_step(int32_t in)
{
    uint32_t k;
    int32_t y = 0;
    ps_buf[RVP.ps.w & (PS_LEN - 1u)] = (int16_t)clamp(in, -32768, 32767);
    for (k = 0; k < 2u; k++) {
        uint32_t ph = (RVP.ps.ph + k * (PS_WIN / 2u)) % PS_WIN, d = PS_WIN - ph + 2u;
        int32_t g = ph < PS_WIN / 2u ? (int32_t)(ph * 65536u / PS_WIN) : (int32_t)((PS_WIN - ph) * 65536u / PS_WIN);
        y += (ps_buf[(RVP.ps.w - d) & (PS_LEN - 1u)] * g) >> 15;
    }
    RVP.ps.w++;
    RVP.ps.ph = (RVP.ps.ph + 1u) % PS_WIN;
    RVP.ps.lp += mulq15(y - RVP.ps.lp, 16000);
    return RVP.ps.lp;
}
/* one half-rate sample x through the tank: decay, damp (Q15), the pre-delay (samples), the bandwidth (Q15), the
 * octave's share (Q15, 0 none) -> left, right (the taps' sums) */
static void plate_step(int32_t x, int32_t decay, int32_t damp, uint32_t pre, int32_t bw, int32_t shim, int32_t *yl,
                       int32_t *yr)
{
    int32_t a, b, ma, mb;
    pl_put(PL_PRE, x);
    x = pre ? pl_tap(PL_PRE, pre) : x;
    if (shim)
        x += mulq15(shimmer_step((RVP.end[0] + RVP.end[1]) >> 1), shim);
    RVP.bw += mulq15(x - RVP.bw, bw);               /* bandwidth (~6 kHz at 22.05; CAVERN ~3.5 kHz) */
    x = pl_ap(PL_AP1, RVP.bw, 24576);             /* 0.75 */
    x = pl_ap(PL_AP2, x, 24576);
    x = pl_ap(PL_AP3, x, 20480);                /* 0.625 */
    x = pl_ap(PL_AP4, x, 20480);
    RVP.lph += 194783u;                           /* ~1 Hz at 22.05 kHz */
    ma = (498 << 8) + ((rv_sine(RVP.lph) * PL_EXC) >> 7);
    mb = (673 << 8) + ((rv_sine(RVP.lph + 0x40000000u) * PL_EXC) >> 7);
    a = pl_mod_ap(PL_MA, x + mulq15(RVP.end[1], decay), -22938, ma);   /* -0.7: the decay diffusion */
    b = pl_mod_ap(PL_MB, x + mulq15(RVP.end[0], decay), -22938, mb);
    pl_put(PL_DA1, a);
    pl_put(PL_DB1, b);
    a = pl_tap(PL_DA1, PL_LEN[PL_DA1]);
    b = pl_tap(PL_DB1, PL_LEN[PL_DB1]);
    RVP.dmp[0] += mulq15(a - RVP.dmp[0], damp);
    RVP.dmp[1] += mulq15(b - RVP.dmp[1], damp);
    a = pl_ap(PL_AA, mulq15(RVP.dmp[0], decay), 16384);   /* 0.5 */
    b = pl_ap(PL_AB, mulq15(RVP.dmp[1], decay), 16384);
    pl_put(PL_DA2, a);
    pl_put(PL_DB2, b);
    RVP.end[0] = pl_tap(PL_DA2, PL_LEN[PL_DA2]);
    RVP.end[1] = pl_tap(PL_DB2, PL_LEN[PL_DB2]);
    *yl = pl_tap(PL_DA1, 197) + pl_tap(PL_DA1, 2203) - pl_tap(PL_AA, 1417) + pl_tap(PL_DA2, 1479) -
          pl_tap(PL_DB1, 1474) - pl_tap(PL_AB, 139) - pl_tap(PL_DB2, 790);
    *yr = pl_tap(PL_DB1, 262) + pl_tap(PL_DB1, 2687) - pl_tap(PL_AB, 910) + pl_tap(PL_DB2, 1980) -
          pl_tap(PL_DA1, 1564) - pl_tap(PL_AA, 248) - pl_tap(PL_DA2, 90);
}
/* PLATE, CAVERN, SHIMMER: the block's send (rv.bx) through the tank -> rv.l, rv.r; 1 if anything is left in it */
static uint32_t plate_block(uint32_t type, int32_t size, int32_t damp, int32_t pre)
{
    int cave = type >= RT_CAVERN;
    int32_t decay = cave ? 16384 + size * 104 : 6554 + size * 155;   /* 0.5 .. 0.9 a pass; 0.2 .. 0.8 */
    int32_t dk = 32767 - damp * 193, bw = cave ? 18000 : 26000, shim = type == RT_SHIMMER ? SH_GAIN : 0;
    uint32_t pt = (uint32_t)pre * (PL_PRE_LEN - 1u) / 127u, i, full = 0;
    if (cave)
        pt = pt + PL_CAVE_PRE > PL_PRE_LEN - 1u ? PL_PRE_LEN - 1u : pt + PL_CAVE_PRE;
    if (type == RT_SHIMMER) {
        decay = decay > SH_DECAY ? SH_DECAY : decay;
        dk = dk > SH_DAMP ? SH_DAMP : dk;
    }
    if (!rv.ready) {                            /* (the memory is zero: the lines' places) */
        uint32_t k, b = 0;
        for (k = 0; k < PL_N; k++) {
            RVP.base[k] = (uint16_t)b;
            b += PL_LEN[k];
        }
        RVP.pre = pt;
        rv.ready = 1;
    }
    for (i = 0; i < RV_NC; i++) {
        int32_t yl, yr;
        RVP.pre += RVP.pre < pt ? 1u : RVP.pre > pt ? (uint32_t)-1 : 0u;   /* (a jump in PRE-DELAY: a glide, no step) */
        plate_step(rv.bx[i], decay, dk, RVP.pre, bw, shim, &yl, &yr);
        rv.l[i] = (yl * PL_OUT) >> 14;
        rv.r[i] = (yr * PL_OUT) >> 14;
        full |= (uint32_t)(rv.l[i] | rv.r[i] | rv.bx[i] | RVP.end[0] | RVP.end[1]);
    }
    return full != 0;
}

/* the audio ISR: n (= 2 RV_NC) frames of the REV bus in (mono) -> the reverb added to out (L R) */
static void hall_render(const int32_t *in, int32_t *out, uint32_t n)
{
    uint32_t i, w, rd, pd, any = 0, full, type = (uint32_t)clamp(rv_type, 0, RV_NTYPE - 1);
    int32_t mix = rv_mix, size = clamp(rv_size, 0, 127), damp = clamp(rv_damp, 0, 127), pre = clamp(rv_pre, 0, 127);
    int32_t inf = type == RT_HALL && size >= RV_INF, st = inf ? 0 : 32767;
    int32_t xp = rv.x, hp = rv.hp, he = rv.he, sd = rv.send, ml = rv.ml, mr = rv.mr, f0, f1;
    if (n != 2u * RV_NC)
        return;
    if (type != rv.type) {
        int idle = rv.quiet >= RV_PRE_LEN && rv.empty >= RV_EMPTY;
        if (rv.cut >= 32767 || idle) {          /* TYPE changed, the return faded out (or nothing sounding): the new
                                                 * type from silence */
            memset(&rv_mem, 0, sizeof rv_mem);
            rv.type = (uint8_t)type;
            rv.ready = 0;
            rv.ml = rv.mr = ml = mr = 0;
            rv.quiet = RV_PRE_LEN, rv.empty = RV_EMPTY;
            rv.cut = idle ? 0 : 32767;
        }
    }
    f0 = 32767 - rv.cut;
    rv.cut += type != rv.type ? RV_FADE_STEP : -RV_FADE_STEP;   /* (the return down over ~6 blocks, then back up) */
    rv.cut = clamp(rv.cut, 0, 32767);
    f1 = 32767 - rv.cut;
    if (mix <= 0)
        return;                                 /* MIX 0: the dry alone; the tank waits as it is */
    for (i = 0; i < 2u * RV_NC && !any; i++)
        any = in[i] != 0;
    if (!any && rv.quiet >= RV_PRE_LEN && rv.empty >= RV_EMPTY)
        return;                                 /* nothing comes in, nothing is left: nothing to do */

    /* the send: every other sample, 1 2 1, the low cut, its level (fading to 0 at INF); HALL: into the pre-delay */
    pd = (uint32_t)pre * RV_PRE_MAX / 127u;
    w = RVH.pw;
    rd = w >= pd ? w - pd : w + RV_PRE_LEN - pd;
    for (i = 0; i < RV_NC; i++) {
        int32_t x = (xp + 2 * in[2u * i] + in[2u * i + 1u]) >> 2, v;
        xp = in[2u * i + 1u];
        x = mulq15(clamp(x, -262143, 262143), RV_IN);
        v = x - hp + he;                        /* the low cut, its step's remainder kept */
        he = v & 31;
        hp += v >> 5;
        sd += sd < st ? 256 : sd > st ? -256 : 0;   /* (~6 ms from full to 0) */
        sd = clamp(sd, 0, 32767);
        x = clamp(mulq15(x - hp, sd), -32768, 32767);
        if (rv.type == RT_HALL) {
            rv_pbuf[w] = (int16_t)x;
            rv.bx[i] = rv_pbuf[rd];
            if (++w == RV_PRE_LEN) w = 0;
            if (++rd == RV_PRE_LEN) rd = 0;
        } else {
            rv.bx[i] = x;
        }
    }
    rv.x = xp, rv.hp = hp, rv.he = he, rv.send = sd;
    if (rv.type == RT_HALL)
        RVH.pw = (uint16_t)w;
    rv.quiet = any ? 0u : rv.quiet + RV_NC;

    full = rv.type == RT_HALL ? hall_block(size, damp) : plate_block(rv.type, size, damp, pre);
    rv.empty = full ? 0u : rv.empty + 1u;

    /* back to 44.1 kHz, between samples, at the return's level (MIX; and the TYPE change's fade) */
    {
        int32_t wg = mix * 258, df = f1 - f0;
        for (i = 0; i < RV_NC; i++) {
            int32_t wl = rv.l[i], wr = rv.r[i];
            int32_t *o = out + 4u * i;
            if (f0 != 32767 || df) {
                int32_t f = f0 + ((df * (int32_t)i) >> 7);
                wl = mulq15(wl, f), wr = mulq15(wr, f);
            }
            o[0] += mulq15((ml + wl) >> 1, wg);
            o[1] += mulq15((mr + wr) >> 1, wg);
            o[2] += mulq15(wl, wg);
            o[3] += mulq15(wr, wg);
            ml = wl;
            mr = wr;
        }
    }
    rv.ml = ml, rv.mr = mr;
}
