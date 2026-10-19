/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GR!MM */
/* GR!MM's SEL: the scale the keys snap to (per track: ROOT, SCALE; physical layout is separate) and a track's one-key chord (CHORD TYPE, VOICING).
 *   sc_snap:     a note to the nearest one of the scale (a tie: the lower); SCALE OFF: as it is
 *   chord_notes: the notes a chord on a note is (its CHORD TYPE, then its VOICING). SCALE: the triad of the scale on the
 *                note (the scale's 1st, 3rd and 5th from it: in key); SCALE OFF: MIN
 *   chord_name:  "Cm7", "D#dim", a scale's odd triad by its notes ("C-F-A#")
 * The main loop and the audio ISR (note_on) both call these: no state of their own. */

enum { CH_SCALE, CH_MIN, CH_MAJ, CH_SUS2, CH_SUS4, CH_FIFTH, CH_MIN7, CH_MAJ7, CH_DIM, CH_ADD9, CH_CLUSTER, CH_SCALE7, NCHORD };
enum { VC_CLOSE, VC_OPEN, VC_SPREAD, VC_OCT, VC_DROP, NVOICING };
#define CH_MAX 5u
#define SC_NAME N_SCALE
#define SC_SHORT N_SCALE
static const char *const SC_NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const CH_SHORT[NCHORD] = {"DIA3", "MIN", "MAJ", "SUS2", "SUS4", "FIFTH", "MIN7", "MAJ7", "DIM",
                                             "ADD9", "CLUSTR", "DIA7"};
static const char *const CH_SUF[NCHORD] = {"m", "m", "", "sus2", "sus4", "5", "m7", "maj7", "dim", "add9", "clus", "m7"};
static const uint8_t CH_IV[NCHORD][5] = {       /* its notes' count, then their semitones over the note */
    {3, 0, 3, 7}, {3, 0, 3, 7}, {3, 0, 4, 7}, {3, 0, 2, 7}, {3, 0, 5, 7}, {2, 0, 7}, {4, 0, 3, 7, 10}, {4, 0, 4, 7, 11},
    {3, 0, 3, 6}, {4, 0, 4, 7, 14}, {3, 0, 2, 3}, {4, 0, 3, 7, 10}};
static const char *const VC_NAME[NVOICING] = {"CLOSE", "OPEN", "SPREAD", "+OCT", "DROP"};

static uint32_t sc_mask(uint32_t t)
{
    uint32_t s = (uint32_t)clamp(trk[t].scale, 0, NSCALE - 1);
    uint32_t m = s == SCALE_CUSTOM_ID ? (uint32_t)trk[t].custom & 4095u : SCALE_MASK[s];
    return m ? m : 1u;
}
static int sc_in(uint32_t t, int32_t n)                     /* note n (from 0) in the scale */
{
    return sc_mask(t) >> ((uint32_t)(n + 132 - clamp(trk[t].root, 0, 11)) % 12u) & 1u;
}
static uint32_t sc_snap(uint32_t t, uint32_t n)
{
    int32_t d, m = (int32_t)n;
    if (trk[t].scale <= 0) return n;
    for (d = 0; d < 12; d++) {
        if (m - d >= 0 && sc_in(t, m - d)) return (uint32_t)(m - d);
        if (m + d <= 127 && sc_in(t, m + d)) return (uint32_t)(m + d);
    }
    return n;
}
static int32_t sc_up(uint32_t t, int32_t n, uint32_t k)     /* the k-th note of the scale over n */
{
    for (; k; k--)
        do n++; while (!sc_in(t, n));
    return n;
}
static int32_t sc_walk(uint32_t t, int32_t n, int32_t d)    /* GRID's NOTE d notes of the scale on (0 OFF; under the lowest: OFF) */
{
    int32_t s = d > 0 ? 1 : -1, m;
    if (!n) return d > 0 ? (int32_t)sc_snap(t, 60u) : 0;
    for (; d; d -= s) {
        for (m = n + s; m >= 1 && m <= 127 && !sc_in(t, m); m += s)
            ;
        if (m < 1) return 0;
        if (m > 127) break;
        n = m;
    }
    return n;
}
static uint32_t chord_notes_tv(uint32_t t, int32_t ty, int32_t vo, uint32_t note, uint8_t *out)   /* CHORD TYPE ty, VOICING vo on
                                                 * note: its notes (0..127, at most CH_MAX) */
{
    int32_t n[CH_MAX], c, i, r = (int32_t)(note & 127u);
    ty = clamp(ty, 0, NCHORD - 1);
    uint32_t k = 0;
    if ((ty == CH_SCALE || ty == CH_SCALE7) && trk[t].scale > 0) {       /* the scale's triad on it (a note off the scale: as its nearest's) */
        int32_t s = (int32_t)sc_snap(t, note);
        r = s;
        n[0] = r, n[1] = sc_up(t, s, 2), n[2] = sc_up(t, s, 4), c = 3;
        if (ty == CH_SCALE7) n[c++] = sc_up(t, s, 6);
    } else
        for (c = 0; c < CH_IV[ty][0]; c++) n[c] = r + CH_IV[ty][1 + c];
    switch (clamp(vo, 0, NVOICING - 1)) {
    case VC_OPEN: if (c >= 3) n[1] += 12; break;   /* the middle note up an octave */
    case VC_SPREAD: n[0] -= 12; for (i = 1; i < c; i++) n[i] += 12; break;   /* the root down, the rest up */
    case VC_OCT: n[c++] = r + 12; break;        /* close, and the root an octave up */
    case VC_DROP: n[0] -= 12; break;            /* the root down an octave */
    default: break;
    }
    for (i = 0; i < c; i++) {                   /* out of 0..127 (DROP, SPREAD at the ends): octaves in */
        while (n[i] < 0) n[i] += 12;
        while (n[i] > 127) n[i] -= 12;
        out[k++] = (uint8_t)n[i];
    }
    return k;
}
static uint32_t chord_notes(uint32_t t, uint32_t note, uint8_t *out)   /* track t's chord on note */
{
    return chord_notes_tv(t, trk[t].ch_type, trk[t].ch_voice, note, out);
}
static uint32_t chord_step(uint32_t t)          /* a chord step's byte: track t's chord now (0x80, VOICING << 4, TYPE) */
{
    return 0x80u | (uint32_t)clamp(trk[t].ch_voice, 0, NVOICING - 1) << 4 | (uint32_t)clamp(trk[t].ch_type, 0, NCHORD - 1);
}
static char *chord_name_tv(uint32_t t, int32_t ty, uint32_t note, char *s)   /* CHORD TYPE ty's name on note: "Cm7" */
{
    static const uint8_t TRI[6][2] = {{3, 7}, {4, 7}, {3, 6}, {4, 8}, {2, 7}, {5, 7}};
    static const char *const TRI_SUF[6] = {"m", "", "dim", "aug", "sus2", "sus4"};
    int32_t i;
    ty = clamp(ty, 0, NCHORD - 1);
    s = put_s(s, SC_NOTE[note % 12u]);
    {
        uint8_t ns[CH_MAX];
        uint32_t k, n = chord_notes_tv(t, ty, trk[t].ch_voice, note, ns);
        for (k = 0; k < n; k++) if (tuning_dc(t, ns[k])) return put_s(s, " TUNED");
    }
    if (ty == CH_SCALE7) return put_s(s, " DIA7");
    if ((ty == CH_SCALE || ty == CH_SCALE7) && trk[t].scale > 0) {
        int32_t r = (int32_t)sc_snap(t, note), a = sc_up(t, r, 2) - r, b = sc_up(t, r, 4) - r;
        for (i = 0; i < 6; i++)
            if (TRI[i][0] == a && TRI[i][1] == b) return put_s(s, TRI_SUF[i]);
        s = put_s(put_s(s, "-"), SC_NOTE[(note + (uint32_t)a) % 12u]);   /* (a triad without a name: its notes) */
        return put_s(put_s(s, "-"), SC_NOTE[(note + (uint32_t)b) % 12u]);
    }
    return put_s(s, CH_SUF[ty]);
}
static char *chord_name(uint32_t t, uint32_t note, char *s) { return chord_name_tv(t, trk[t].ch_type, note, s); }

/* Physical C4 anchors the selected tonic in octave four. A negative return is
 * a silent key (WHITE black keys or a degree beyond the MIDI range). */
static int32_t sc_key(uint32_t t, int32_t key)
{
    static const int8_t white[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t mode = clamp(trk[t].map, 0, NMAP - 1), d, oct, index, count = 0, k, note;
    uint32_t mask = sc_mask(t);
    if (key < 0 || key > 127) return -1;
    if (mode == MAP_CHR) return key;
    if (mode == MAP_SNAP || mode == MAP_SEQ) return (int32_t)sc_snap(t, (uint32_t)key);
    if (mode == MAP_WHITE && white[key % 12] < 0) return -1;
    d = mode == MAP_WHITE ? (key / 12 - 5) * 7 + white[key % 12] : key - 60;
    for (k = 0; k < 12; k++) count += (mask >> k) & 1u;
    oct = d / count; index = d % count;
    if (index < 0) index += count, oct--;
    for (k = 0; k < 12; k++) if ((mask >> k & 1u) && index-- == 0) break;
    note = 60 + clamp(trk[t].root, 0, 11) + oct * 12 + k;
    return note >= 0 && note <= 127 ? note : -1;
}
