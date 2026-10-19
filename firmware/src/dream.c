/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (SARDAB's DREAM, firmware/src/dream.c); DAKHMEH port */
/* DREAM (GLO's second page, after SONG): ambient music from the four tracks' own sounds, SARDAB's DREAM on GR!MM's
 * sequencer. A key on the page composes a piece in its key (over steps that are there: the same key again within
 * 3 s; EDIT held undoes it) and MOOD's scale: MIN DOR PHRY HARM MAJ MIX, or MAQAM, the selected track's maqam with its
 * whole tuning (TUNING, TASHRIQ, TUNE trims, custom table) on all four. Four chords, one per 16 steps of 64 at 1/8
 * (DR_PROG), and each track a part that suits its sound (dr_role: by its sound's name, else its source): a drone holds
 * the root, a bass the chords' roots, a pad their scale triads (chord steps: tuned as the maqam is), a plucked sound
 * breaks them, a melodic one sings a motif and, two chords on, the same rhythm on the new chord, a second melodic one
 * a few high notes. DENSITY: how much each part plays; the steps' CHANCE varies them on every pass, their VELOCITY
 * shapes them. EVOLVE ON: every 9 - DRIFT passes of track 1 one thing changes (dream_evolve: a chord, a phrase played
 * anew, a part in or out). What DREAM writes is ordinary steps: edit them, save them.
 * Also sets each track's STEPS 64, DIVISION 1/8, ROOT and SCALE (the keys play along in key), chord mode and ARP off
 * (they would stack on the parts). Main loop only; the audio ISR plays the steps as it finds them. */

enum { DR_DRONE, DR_BASS, DR_PAD, DR_ARP, DR_MELODY, DR_HIGH, NDROLE };
#define DR_LEN 64u
#define DR_SPAN 16u
static const uint8_t DR_MOOD_SCL[NDMOOD - 1] = {2, 3, 8, 7, 1, 4};   /* MOOD -> SCALE: MIN DOR PHRY HARM MAJ MIX */
/* the progressions, in scale degrees (0: the root's chord), a chord per 16 steps */
static const uint8_t DR_PROG[8][4] = {{0, 5, 6, 0}, {0, 3, 0, 4}, {0, 6, 5, 4}, {0, 5, 3, 4},
                                      {0, 2, 5, 6}, {0, 3, 6, 0}, {0, 5, 0, 6}, {0, 3, 5, 4}};
#define DR_TRIAD (0x80u | CH_SCALE)            /* a chord step: the scale's triad, CLOSE */
#define DR_FIFTH (0x80u | CH_FIFTH)            /* .. the root and its fifth */

static struct {
    uint8_t valid;                              /* composed (or rebuilt from the tracks) this session */
    uint8_t asked;                              /* over steps: said yes once (later keys dream at once) */
    uint8_t root, deg[4], role[NT], dup[NT];    /* dup: the second track of its role */
    uint8_t passes, last, mlen, motif[8];       /* EVOLVE's count; the motif's onsets (steps into its span) */
    uint16_t mask;                              /* the scale's tones over the root */
    int32_t ask_key;                            /* a key asked to dream over steps (the same within 3 s: yes), -1 */
    uint32_t ask_until;
    char said[24];                              /* "D MAQAM" (the picture) */
} dream = {.ask_key = -1};

static uint32_t dr_rnd(uint32_t n) { return tl_rnd(n); }

static int dr_has(const char *s, const char *w)  /* the word w (up to a space) in s */
{
    uint32_t i, j;
    for (i = 0; s[i]; i++) {
        for (j = 0; w[j] > ' ' && s[i + j] == w[j]; j++)
            ;
        if (w[j] <= ' ')
            return 1;
    }
    return 0;
}

/* the part a track's sound suits: by a word in its sound's name (each word after its DR_ role; the first that is
 * there), else by its source (VA, PHASE, FM: a slow ATTACK a pad, else a melody) */
static uint32_t dr_role(uint32_t t)
{
    static const char KW[] = "0DRONE 0HUM 0WELL 0ABYSS 1BASS 1TUBA 2PAD 2VEIL 2HALL 2VELVET 2MOSS 2STR 2CHOIR 2MONK "
                             "2ORGAN 2CHAPEL 2HYMN 2BOWED 2RABAB 4NEY 4DUDUK 4MIJ 4ARGH 4LEAD 4FLUTE 4REED 3OUD 3QAN "
                             "3SANT 3TAR 3HARP 3BELL 3CHIME 3GONG 3PLUCK 3KEYS 3PIANO 3BOX";
    char nm[24];
    const char *w;
    uint32_t e = (uint32_t)clamp(trk[t].eng, 0, NENG - 1);
    trk_sname(t, nm);
    if (nm[0])
        for (w = KW; *w; w++)
            if ((w == KW || w[-1] == ' ') && dr_has(nm, w + 1))
                return (uint32_t)(*w - '0');
    if (e == ENG_WHEEL || e == ENG_VOICE) return DR_PAD;
    if (e == ENG_PHYS || e == ENG_SAMPLE) return DR_ARP;
    return trk[t].env[0] >= 50 ? DR_PAD : DR_MELODY;
}

static uint32_t dr_tones(uint8_t *pc)           /* the scale's tones above the root (semitones), how many */
{
    uint32_t m = dream.mask ? dream.mask : 1u, i, n = 0;
    for (i = 0; i < 12u; i++)
        if ((m >> i) & 1u) pc[n++] = (uint8_t)i;
    return n;
}
/* scale degree d (0: the root; any, below too) as a note, its octave from C of octave oct (4: C4 60) */
static uint32_t dr_note(int32_t d, int32_t oct)
{
    uint8_t pc[12];
    int32_t n = (int32_t)dr_tones(pc), o = d >= 0 ? d / n : -((n - 1 - d) / n);
    return (uint32_t)clamp(12 * (oct + 1 + o) + dream.root + pc[d - o * n], 12, 115);
}
static int32_t dr_degree(uint32_t x)            /* the degree of note x (rounded down onto the scale) */
{
    uint8_t pc[12];
    int32_t n = (int32_t)dr_tones(pc), r = ((int32_t)x - dream.root) % 12, o = ((int32_t)x - dream.root) / 12, d = n - 1;
    if (r < 0) r += 12, o--;
    while (d > 0 && pc[d] > r) d--;
    return o * n + d;
}

/* step i: note (a chord step: chord), at vel, CHANCE chance, LENGTH len steps (cut at the piece's end) */
static void dr_put(uint32_t t, uint32_t i, uint32_t note, uint32_t chord, uint32_t vel, uint32_t chance, uint32_t len)
{
    seq_step_t *s;
    if (i >= DR_LEN) return;
    s = &seq[t].pat[0].st[i];
    step_off(s);
    s->chance = (int32_t)(chance > 100u ? 100u : chance);
    s->len = (int32_t)clamp((int32_t)len, 1, (int32_t)(DR_LEN - i));
    s->vel = (int32_t)clamp((int32_t)vel, 1, 127);
    s->chord = (int32_t)chord;
    s->note = (int32_t)note;                    /* (last: the ISR sees a whole step or OFF) */
}
static uint32_t dr_vel(void) { return 78u + dr_rnd(24); }

/* ------------------------------------------------------------------ the parts, a chord (a span of 16 steps) at once */
static void dr_drone(uint32_t t, uint32_t s, uint32_t dens)
{
    if (!s) dr_put(t, 0, dr_note(0, 3), dens < 5u ? 0u : DR_FIFTH, 90, 100, DR_LEN);   /* (one for the whole piece) */
}
static void dr_bass(uint32_t t, uint32_t s, uint32_t dens)
{
    uint32_t a = s * DR_SPAN;
    dr_put(t, a, dr_note(dream.deg[s], 2), 0, dr_vel(), 100, dens >= 6u ? 12u : DR_SPAN);
    if (dens >= 6u)                             /* busier: the fifth before the next chord */
        dr_put(t, a + 12u, dr_note(dream.deg[s] + 4, 2), 0, dr_vel(), 70, 3);
}
static uint32_t dr_pad_root(uint32_t s, int32_t lo)   /* span s's chord's root in the octave from lo */
{
    int32_t x = (int32_t)dr_note(dream.deg[s], 3);
    while (x < lo) x += 12;
    while (x >= lo + 12) x -= 12;
    return (uint32_t)x;
}
static void dr_pad(uint32_t t, uint32_t s, uint32_t dens, uint32_t dup)
{
    uint32_t a = s * DR_SPAN + (dup ? 4u : 0u), end = s * DR_SPAN + DR_SPAN + (dup && s < 3u ? 4u : 0u);
    uint32_t x = dr_pad_root(s, dup ? 67 : 55);
    if (dup && !s)                              /* (the late entrance: the piece's last chord over the top) */
        dr_put(t, 0, dr_pad_root(3, 67), DR_TRIAD, dr_vel() - 24u, 100, 4);
    dr_put(t, a, x, DR_TRIAD, dr_vel() - (dup ? 24u : 16u), 100, (dens >= 6u && !dup ? a + 8u : end) - a);
    if (dens >= 6u && !dup)                     /* busier: the chord again halfway, half the time */
        dr_put(t, a + 8u, x, DR_TRIAD, dr_vel() - 26u, 50, DR_SPAN - 8u);
}
static void dr_arp(uint32_t t, uint32_t s, uint32_t dens, uint32_t dup)
{
    uint32_t a = s * DR_SPAN, j, step = dens >= 6u ? 1u : 2u, shape = dr_rnd(3), ch;
    for (j = 0; j * step < DR_SPAN; j++) {
        uint32_t k = shape == 0u ? j % 6u : shape == 1u ? (j % 10u < 6u ? j % 10u : 10u - j % 10u) : dr_rnd(6);
        ch = j ? 25u + dens * 9u : 100u;
        dr_put(t, a + j * step, dr_note(dream.deg[s] + 2 * (int32_t)(k % 3u), (dup ? 3 : 4) + (int32_t)(k / 3u)), 0,
               dr_vel() - 8u, ch, step);
    }
}
/* the melody of span s: the motif's rhythm (spans 0 and 2) or a new one, on the span's chord */
static void dr_melody(uint32_t t, uint32_t s, uint32_t dens, int new_motif)
{
    uint32_t a = s * DR_SPAN, j, n, at[8];
    uint8_t pc[12];
    int32_t d, n7 = (int32_t)dr_tones(pc), lo = dr_degree(62), cd = dream.deg[s], k;
    if (s == 0u || s == 2u) {
        if (new_motif || !dream.mlen) {         /* 2..4 notes, the first on a strong step */
            uint32_t want = 2u + dens / 3u, p = dr_rnd(2) * 2u;
            for (dream.mlen = 0; dream.mlen < want && p < 13u; p += 2u + 2u * dr_rnd(dens >= 5u ? 2u : 3u))
                dream.motif[dream.mlen++] = (uint8_t)p;
        }
        for (n = 0; n < dream.mlen; n++) at[n] = dream.motif[n];
    } else {                                    /* the answer: sparser, later */
        uint32_t want = 1u + dens / 3u, p = 2u + dr_rnd(3) * 2u;
        for (n = 0; n < want && p < 13u; p += 3u + 2u * dr_rnd(3)) at[n++] = p;
    }
    d = cd + 2 * (int32_t)dr_rnd(3);            /* a chord tone to begin */
    d = lo + ((d - lo) % n7 + n7) % n7;         /* into the octave from 62 */
    for (j = 0; j < n; j++) {
        uint32_t end = j + 1u < n ? at[j + 1u] : (s == 3u ? 15u : 13u), len;
        k = ((d - cd) % n7 + n7) % n7;          /* (the degree above the chord's root) */
        if (j == n - 1u && s == 3u) d -= k;     /* the piece ends on the root's chord tone */
        else if (j == n - 1u) d += k == n7 - 1 ? 1 : k & 1 ? -1 : 0;   /* the phrase rests on a chord tone */
        len = end > at[j] + 1u ? end - at[j] - (dr_rnd(3) == 0u) : 1u;
        dr_put(t, a + at[j], dr_note(d, -1), 0, dr_vel(), j && dens < 4u && dr_rnd(3) == 0u ? 75u : 100u, len);
        d += dr_rnd(5) == 0u ? (int32_t)dr_rnd(5) - 2 : dr_rnd(3) ? -1 : 1;   /* mostly down a step: a lament */
        if (d < lo) d += 2;
        if (d > lo + n7 + 2) d -= 2;
    }
}
static void dr_high(uint32_t t, uint32_t s, uint32_t dens)
{
    uint32_t a = s * DR_SPAN, k = (dens + 2u) / 4u, j, p = dr_rnd(4) * 2u;
    for (j = 0; j < k && p < 14u; j++, p += 6u + dr_rnd(3) * 2u)
        dr_put(t, a + p, dr_note(dream.deg[s] + 2 * (int32_t)dr_rnd(3), 5), 0, dr_vel() - 15u, 50u + dr_rnd(30),
               5u + dr_rnd(4));
}

static void dr_span(uint32_t t, uint32_t s, int new_motif)   /* track t's part over span s, its steps there replaced */
{
    uint32_t dens = (uint32_t)clamp(dr_dens, 1, 8), j, r = dream.role[t];
    if (r == DR_DRONE && s) return;
    if (r != DR_PAD || !dream.dup[t])           /* (a late pad's span reaches into the next) */
        for (j = 0; j < DR_SPAN; j++) step_off(&seq[t].pat[0].st[s * DR_SPAN + j]);
    switch (r) {
    case DR_DRONE: dr_drone(t, s, dens); break;
    case DR_BASS: dr_bass(t, s, dens); break;
    case DR_PAD: dr_pad(t, s, dens, dream.dup[t]); break;
    case DR_ARP: dr_arp(t, s, dens, dream.dup[t]); break;
    case DR_MELODY: dr_melody(t, s, dens, new_motif); break;
    default: dr_high(t, s, dens); break;
    }
}
static void dr_roles(void)
{
    uint32_t i, seen = 0;
    for (i = 0; i < NT; i++) {
        uint32_t r = dr_role(i);
        dream.dup[i] = (uint8_t)((seen >> r) & 1u);
        if (r == DR_MELODY && dream.dup[i]) {   /* a second melodic voice: high and sparse */
            r = DR_HIGH;
            dream.dup[i] = (uint8_t)((seen >> r) & 1u);
        }
        seen |= 1u << r;
        dream.role[i] = (uint8_t)r;
    }
}
static void dr_say(void)                        /* "D MAQAM", "D BAYATI" (MAQAM: its scale's name) */
{
    char *e = put_s(put_s(dream.said, SC_NOTE[dream.root % 12u]), " ");
    put_s(e, dr_mood == DM_MAQAM ? SC_SHORT[clamp(trk[0].scale, 0, NSCALE - 1)] : DMOOD_SHORT[clamp(dr_mood, 0, NDMOOD - 1)]);
}

/* compose: a piece in key root (0..11 from C) */
static void dream_compose(uint32_t root)
{
    uint32_t i, s, k, maqam = dr_mood == DM_MAQAM;
    const uint8_t *pg = DR_PROG[dr_rnd(8)];
    const track_t *m = &trk[sel];               /* MAQAM: the selected track's maqam and tuning */
    dream.root = (uint8_t)(root % 12u);
    dream.mask = (uint16_t)(maqam ? sc_mask(sel) : SCALE_MASK[DR_MOOD_SCL[clamp(dr_mood, 0, NDMOOD - 2)]]);
    for (s = 0; s < 4u; s++) dream.deg[s] = pg[s];
    dr_roles();
    dream.mlen = 0;
    for (i = 0; i < NT; i++) {
        track_t *t = &trk[i];
        if (maqam && i != sel) {
            t->scale = m->scale, t->custom = m->custom, t->temper = m->temper;
            t->tuning_mode = m->tuning_mode, t->tuning_system = m->tuning_system, t->retune = m->retune;
            for (k = 0; k < 12u; k++)
                t->trim[k] = m->trim[k], t->table[k] = m->table[k], t->tash_on[k] = m->tash_on[k],
                t->tash_cents[k] = m->tash_cents[k];
        } else if (!maqam)
            t->scale = DR_MOOD_SCL[clamp(dr_mood, 0, NDMOOD - 2)];
        t->root = dream.root;
        t->chord = 0, t->arp = 0;
        seq[i].steps = DR_LEN, seq[i].div = 2, seq[i].mode = SEQ_FWD;   /* (64 steps of 1/8) */
        seq[i].cur = 0;
        for (s = 0; s < 4u; s++) dr_span(i, s, s == 0u);
    }
    dream.valid = 1, dream.passes = 0;
    dr_say();
    playing = 1;                                /* (every part from the top, together; stopped: it plays) */
    nq_put(NQ_PLAY, 0, (uint32_t)slot_cur & 3u);
}

/* after power-on or a load: the key and the chords back from the tracks (track 1's ROOT and SCALE; each span's chord:
 * its bass note's degree, else the pad's chord's root, else the root's) */
static void dr_rebuild(void)
{
    uint8_t pc[12];
    uint32_t s, i, n, d;
    dream.root = (uint8_t)clamp(trk[0].root, 0, 11);
    dream.mask = (uint16_t)sc_mask(0);
    n = dr_tones(pc);
    dr_roles();
    for (s = 0; s < 4u; s++) {
        for (d = n, i = 0; i < NT; i++) {
            const seq_step_t *st = &seq[i].pat[0].st[s * DR_SPAN];
            if (!st->note || (dream.role[i] != DR_BASS && (dream.role[i] != DR_PAD || dream.dup[i]))) continue;
            if (dream.role[i] == DR_BASS || d == n) d = (uint32_t)dr_degree((uint32_t)st->note) % n;
        }
        dream.deg[s] = (uint8_t)(d < n ? d : 0u);
    }
    dream.valid = 1;
}

/* EVOLVE: one change, at the end of a pass, in span s (2..4: the first, the motif's, stays): a new chord there (every
 * part but the drone again on it), or one part there played anew, or a part (not the drone, the bass or the first
 * pad) out or back in (its MUTE) */
static void dream_evolve(void)
{
    uint32_t r = dr_rnd(4), s = 1u + dr_rnd(3), i = dr_rnd(NT), n, d;
    if (!dream.valid) dr_rebuild();
    if (!r) {
        d = 2u + dr_rnd(5);
        if (d == dream.deg[s]) d = d == 6u ? 2u : d + 1u;
        dream.deg[s] = (uint8_t)d;
        for (i = 0; i < NT; i++) dr_span(i, s, 0);
        return;
    }
    for (n = 0; n < NT; n++, i = (i + 1u) % NT)   /* from a track picked at random, the first that may */
        if (dream.role[i] != DR_DRONE && (r < 3u || (dream.role[i] != DR_BASS && (dream.role[i] != DR_PAD || dream.dup[i]))))
            break;
    if (n == NT) return;
    if (r < 3u) dr_span(i, s, 0);
    else trk_mute ^= 1u << i;
}

/* DREAM's page: key k (its note: base + k) pressed. Over steps that are there, the first time: the same key again within
 * 3 s */
static void dream_key(uint32_t k, uint32_t note)
{
    uint32_t i, j, any = 0;
    char m[24];
    if (song_mode != SM_OFF || song_rec) { hd_say("SONG MODE: SET IT OFF"); return; }
    for (i = 0; i < NT && !any; i++)
        for (j = 0; j < SEQ_N && !any; j++) any = seq[i].pat[0].st[j].note != 0;
    if (any && !dream.asked && !(dream.ask_key == (int32_t)k && fm1_ms < dream.ask_until)) {
        dream.ask_key = (int32_t)k, dream.ask_until = fm1_ms + 3000u;
        put_s(hd_msg, "AGAIN: DREAM OVER STEPS"), hd_until = dream.ask_until;
        return;
    }
    dream.asked = 1, dream.ask_key = -1;
    undo_take(UK_ONE | ++uk_n, -1);             /* (EDIT held: the steps back) */
    dream_compose(note % 12u);
    put_s(put_s(m, "DREAMING "), dream.said);
    hd_say(m);
}
static void dream_forget(void) { dream.valid = 0; }   /* (a load, an undo: what the tracks hold is someone else's) */

/* every pass of the main loop: EVOLVE's clock (track 1 back at its first step: a pass) */
static void dream_tick(void)
{
    int32_t p = seq[0].pos;
    uint32_t every = 9u - (uint32_t)clamp(dr_drft, 1, 8);
    if (!dr_evol || !playing || song_mode != SM_OFF || song_rec || p < 0) {
        dream.last = 0, dream.passes = 0;
        return;
    }
    if ((uint32_t)p < dream.last && ++dream.passes >= every) {
        dream.passes = 0;
        dream_evolve();
    }
    dream.last = (uint8_t)p;
}
static const char *dream_said(void) { return dream.said; }   /* (ui.c: DREAM's picture) */
static uint32_t dream_role(uint32_t t)          /* the part track t plays (the piece's; none yet: what it would) */
{
    if (!dream.said[0]) dr_roles();
    return dream.role[t % NT];
}
