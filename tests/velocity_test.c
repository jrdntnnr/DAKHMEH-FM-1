/* SPDX-License-Identifier: GPL-3.0-only */
/* DAKHMEH: velocity (keys, MIDI, steps, arp, sensing), CANON and DREAM against the real firmware translation unit. */
#define DINGHY_HOST
#define main firmware_host_main
#include "../firmware/src/grimm.c"
#undef main
#include <assert.h>

static const track_t defaults = TRK_DEF;
static void clean(void)
{
    unsigned t;
    memset(vc, 0, sizeof vc); nlat = 0; nq_r = nq_w = 0; ec_r = ec_w = 0;
    trk_quiet = trk_mute = trk_solo = 0; tune_f = 0; playing = 0; song_mode = SM_OFF; song_rec = 0;
    for (t = 0; t < NT; t++) { trk[t] = defaults; lfo_p16[t] = 0; fm_builtin(t, 0); }
    seq_init();
    memset(arp, 0, sizeof arp); for (t = 0; t < NT; t++) arp[t].last = -1;
    kv_config = 0, kv_hw_on = 0;
}
static void render(void) { int32_t out[2*VB] = {0}; app_audio(out, VB); }
static int lat_of(uint32_t src) { unsigned i; for (i = 0; i < nlat; i++) if (lat_src[i] == src) return (int)i; return -1; }
static seq_step_t *step(uint32_t t, uint32_t i) { return &seq[t].pat[0].st[i]; }

/* the editor's frames, as a page sends them */
static uint8_t rx[4096];
static uint32_t frame(uint8_t *f, uint32_t cmd, const uint8_t *a, uint32_t n, const uint8_t *d, uint32_t dn)
{
    uint32_t w = 0, i, j;
    f[w++] = 0x7D, f[w++] = 0x47, f[w++] = 0x52, f[w++] = (uint8_t)cmd;
    for (i = 0; i < n; i++) f[w++] = a[i];
    for (i = 0; i < dn; i += 7u) {
        uint32_t g = dn - i < 7u ? dn - i : 7u, top = 0;
        for (j = 0; j < g; j++) top |= (uint32_t)(d[i + j] >> 7) << j;
        f[w++] = (uint8_t)top;
        for (j = 0; j < g; j++) f[w++] = d[i + j] & 0x7Fu;
    }
    return w;
}
static int put_obj(uint32_t o, uint32_t fmt, const uint8_t *d, uint32_t len)   /* BEGIN, DATA, COMMIT: 0 ok */
{
    uint8_t f[600], a[16];
    uint32_t off, c = st_crc(d, len), n;
    a[0] = 0, a[1] = (uint8_t)o, a[2] = (uint8_t)fmt;
    ed_wr35(a + 3, len), ed_wr35(a + 8, c);
    n = frame(f, ED_PUT, a, 13, 0, 0);
    if (ed_frame(f, n, 1) != 1 || ed_obj != (int32_t)o) return -1;
    for (off = 0; off < len; off += 256u) {
        uint32_t k = len - off < 256u ? len - off : 256u;
        a[0] = 1, a[1] = (uint8_t)o, ed_wr35(a + 2, off);
        n = frame(f, ED_PUT, a, 7, d + off, k);
        ed_frame(f, n, 1);
        if (ed_got != off + k) return -2;
    }
    a[0] = 2, a[1] = (uint8_t)o;
    n = frame(f, ED_PUT, a, 2, 0, 0);
    ed_frame(f, n, 1);
    return ed_obj == -1 ? 0 : -3;
}

int main(void)
{
    uint32_t i, t, k, n, notes = 0;
    int j;
    uint8_t work[WORK_PK], slot[SLOT_ALL];

    /* ---- velocity: the gain, 127 exactly as before */
    assert(vel_gain(127) == 32767 && vel_gain(0) == vel_gain(1) && vel_gain(200) == 32767);
    for (i = 2; i <= 127; i++) assert(vel_gain(i) > vel_gain(i - 1));
    assert(vel_gain(64) > 12000 && vel_gain(64) < 13500);   /* (about -8 dB) */
    /* a voice's level follows its velocity; 127 bit for bit the envelope */
    {
        int32_t full, soft;
        clean(); trk[0].env[0] = 0; trk[0].env[2] = 127;
        note_on(0, 1, 60, SRC_KEY, 0, 0, 127); for (i = 0; i < 8; i++) render();
        full = vc[lat_voice[0]].amp; assert(full == vc[lat_voice[0]].env >> 9);
        clean(); trk[0].env[0] = 0; trk[0].env[2] = 127;
        note_on(0, 1, 60, SRC_KEY, 0, 0, 40); for (i = 0; i < 8; i++) render();
        soft = vc[lat_voice[0]].amp;
        assert(soft == mulq15(full, vel_gain(40)));
        /* the same note again harder: its voice goes on, at the new velocity */
        note_on(0, 1, 60, SRC_KEY, 0, 0, 120); render();
        assert(nlat == 1 && vc[lat_voice[0]].vgain == vel_gain(120) && lat_vel[0] == 120);
    }
    /* the main loop's ask carries it; a chord's notes share it; a new source keeps it */
    clean(); trk[0].chord = 1; trk[0].ch_type = CH_MAJ;
    nq_note_v(NQ_ON, 0, 60, KEY_ID(3), 50); render();
    assert(nlat == 3); for (i = 0; i < nlat; i++) assert(lat_vel[i] == 50 && vc[lat_voice[i]].vgain == vel_gain(50));
    trk[0].eng = ENG_FM; restart_track(0); render();
    for (i = 0; i < nlat; i++) assert(lat_vel[i] == 50 && vc[lat_voice[i]].vgain == vel_gain(50));
    nq_note(NQ_ON, 1, 64, KEY_ID(4)); render();   /* (nq_note: 127, as before velocity) */
    assert(lat_vel[nlat - 1] == 127);
    /* ARP: its notes at the velocity of the key into its list; REC writes that */
    clean(); trk[0].arp = 1; arp_watch();
    key_ask(NQ_ON, 0, KEY_ID(2), 62, 0, 33); arp_step(0); render();
    assert(lat_of(SRC_ARP) >= 0 && lat_vel[lat_of(SRC_ARP)] == 33);

    /* ---- ARP TRILL: each note, then its scale neighbour up (Bayati on D: D, E half-flat, F, G ...) */
    clean(); trk[0].scale = 18; trk[0].root = 2; trk[0].arp = 1; trk[0].arp_mode = ARP_TRILL; arp_watch();
    trk[0].arp_rate = 0;                         /* (1/1: its own clock steps once, at the first note, then waits) */
    key_ask(NQ_ON, 0, KEY_ID(1), 62, 0, 90); key_ask(NQ_ON, 0, KEY_ID(5), 65, 0, 90);
    {
        static const uint8_t want[8] = {62, 64, 65, 67, 62, 64, 65, 67};
        for (i = 0; i < 8; i++) {
            if (i) arp_step(0);
            render();
            j = lat_of(SRC_ARP); assert(j >= 0 && lat_note[j] == want[i] && nlat == 1);
            if (want[i] == 64) assert(voice_tuning_dc(&vc[lat_voice[j]]) == -500);
        }
    }
    trk[0].scale = 0; arp_clear(0); key_ask(NQ_ON, 0, KEY_ID(1), 60, 0, 90);   /* (CHR: a semitone) */
    render(); arp_step(0); render(); assert(lat_note[lat_of(SRC_ARP)] == 61);
    /* ARP DRONE: the lowest held sounds on, the others UP over it; it moves with the lowest, ends with the list */
    clean(); trk[0].arp = 1; trk[0].arp_mode = ARP_DRONE; trk[0].arp_rate = 0; arp_watch();
    key_ask(NQ_ON, 0, KEY_ID(1), 64, 0, 70); key_ask(NQ_ON, 0, KEY_ID(2), 50, 0, 70); key_ask(NQ_ON, 0, KEY_ID(3), 67, 0, 70);
    {
        static const uint8_t up[4] = {64, 67, 64, 67};
        for (i = 0; i < 4; i++) {
            uint32_t d = 0, o = 0;
            if (i) arp_step(0);
            render();
            for (k = 0; k < nlat; k++) {
                if (lat_id[k] == ARP_DRONE_ID(50)) d++, assert(lat_note[k] == 50 && lat_vel[k] == 70);
                else if (lat_src[k] == SRC_ARP) o++, assert(lat_note[k] == up[i]);
            }
            assert(d == 1 && o == 1);
            arp_release(0); render();            /* (GATE: the note over it ends, the drone not) */
            for (d = 0, k = 0; k < nlat; k++) d += lat_id[k] == ARP_DRONE_ID(50);
            assert(d == 1 && nlat == 1);
        }
    }
    trk[0].arp_oct = 2; { uint8_t p[ARP_PN]; assert(arp_pattern(0, p, 2) == 4 && p[0] == 64 && p[3] == 79); }
    key_ask(NQ_OFF, 0, KEY_ID(2), 50, 0, 70); arp_step(0); render();   /* the lowest let go: the drone moves to E */
    for (n = 0, k = 0; k < nlat; k++) n += lat_id[k] == ARP_DRONE_ID(64);
    assert(n == 1 && lat_of(SRC_ARP) >= 0);
    for (k = 0; k < nlat; k++) assert(lat_id[k] != ARP_DRONE_ID(50));
    trk_mute = 1; render(); assert(!nlat); trk_mute = 0; render(); arp_step(0); render();   /* (unmuted: on again) */
    for (n = 0, k = 0; k < nlat; k++) n += lat_id[k] == ARP_DRONE_ID(64);
    assert(n == 1);
    trk[0].arp_mode = ARP_UP; arp_step(0); render();   /* (MODE away: the drone ends) */
    for (k = 0; k < nlat; k++) assert(lat_id[k] < 0x80u);
    trk[0].arp_mode = ARP_DRONE; arp_step(0); render();
    key_ask(NQ_OFF, 0, KEY_ID(1), 64, 0, 70); key_ask(NQ_OFF, 0, KEY_ID(3), 67, 0, 70); render();
    assert(!nlat && !arp[0].n && !arp[0].drone);
    trk[0].arp = 0; arp_watch();
    { char s[24]; sel = 0; trk[0].arp_mode = ARP_DRONE; knob_text(PG_ARP, 0, s); assert(!strcmp(s, "DRONE")); }
    { uint8_t q[TRK_PK]; trk[0].arp_mode = ARP_TRILL; pk_track(q, 0); trk[0].arp_mode = 0; uk_track(q, 0, 0, TRK_PK);
      assert(trk[0].arp_mode == ARP_TRILL); }
    /* every mode, arp on the clock with the sequencer playing, long enough to wrap: nothing piles up */
    for (t = 0; t < NARPMODE; t++) {
        clean(); trk[0].arp = 1; trk[0].arp_mode = (int32_t)t; trk[0].arp_oct = 3; trk[0].scale = 22; arp_watch();
        for (i = 0; i < 5; i++) key_ask(NQ_ON, 0, KEY_ID(i), 48 + i * 5, 0, 100);
        seq_start(0);
        for (i = 0; i < 300; i++) { render(); assert(nlat <= NV); }
        seq_stop(); arp_clear(0); render(); assert(!nlat);
    }

    /* ---- steps: recorded at the key's velocity, played at it, every ratchet's hit too */
    clean(); seq[0].steps = 8;
    for (i = 0; i < SEQ_N; i++) assert(step(0, i)->vel == 127);
    seq_write(0, 2, 60, 0, 45); assert(step(0, 2)->vel == 45 && step(0, 2)->note == 60);
    seq_write(0, 3, 62, 0, 0); assert(step(0, 3)->vel == 1);
    step(0, 2)->ratchet = 2;
    seq[0].pos = 2, seq[0].t = 1; seq_fire(0); render();
    assert(lat_of(SRC_SEQ) >= 0 && lat_vel[lat_of(SRC_SEQ)] == 45 && seq[0].r_vel == 45);
    note_off(0, 60, SRC_SEQ, 0); assert(lat_of(SRC_SEQ) < 0);
    for (i = 0; i < SEQ_DIV_T[4] / 2u; i++) seq_track_tick(0);   /* (the second hit, half the step on) */
    render(); assert(lat_of(SRC_SEQ) >= 0 && lat_note[lat_of(SRC_SEQ)] == 60 && lat_vel[lat_of(SRC_SEQ)] == 45);
    /* STEP's VELOCITY knob: 1..127, the selected step, undoable */
    sel = 0; seq[0].cursor = 2; page = PG_STEP;
    knob_step(PG_STEP, 0, -200); assert(step(0, 2)->vel == 1);
    knob_step(PG_STEP, 0, 500); assert(step(0, 2)->vel == 127);
    assert(!undo() && step(0, 2)->vel == 45);
    { slab_t slab; ui_slab_make(0, &slab); assert(!strcmp(slab.val, "045")); }
    page = PG_GRID; assert(next_page(BTN_SEQ) == PG_STEP);
    page = PG_ARP; assert(next_page(BTN_ARP) == PG_CANON);
    page = PG_SONG; assert(next_page(BTN_GLO) == PG_DREAM);
    /* the tools keep it: SHIFT, COPY / PASTE */
    seq_tool(2, 60); assert(step(0, 1)->vel == 45);
    seq_tool(4, 60); step(0, 1)->vel = 9; trk[0].canon = 3; seq_tool(5, 60);
    assert(step(0, 1)->vel == 45 && trk[0].canon == 3);

    /* ---- CANON: half the pattern behind, a scale interval on, 3/4 the velocity, the step's LENGTH */
    clean(); trk[0].scale = 1; trk[0].root = 0;   /* (C MAJ) */
    seq[0].steps = 8; seq[0].div = 4;
    step(0, 0)->note = 60, step(0, 0)->vel = 100, step(0, 0)->len = 3;
    step(0, 5)->note = 71, step(0, 5)->vel = 80;
    trk[0].canon = 1; seq[0].pos = 4; canon_fire(0); render();   /* 3RD: step 1 (0) now at step 5 (4) */
    j = lat_of(SRC_CANON); assert(j >= 0 && lat_note[j] == 64 && lat_vel[j] == 100 * 3 / 4 + 1);
    assert(seq[0].cn_left[0] == 3u * SEQ_DIV_T[4] - 1u);
    for (i = 0; i < 3u * SEQ_DIV_T[4]; i++) seq_track_tick(0);
    render(); assert(lat_of(SRC_CANON) < 0);     /* (off at its LENGTH) */
    clean(); trk[0].scale = 1; seq[0].steps = 8; step(0, 0)->note = 60; step(0, 5)->note = 71;
    trk[0].canon = 2; seq[0].pos = 1; canon_fire(0); render();   /* 5TH: step 6 (5) at step 2: B -> F */
    assert(lat_note[lat_of(SRC_CANON)] == 77);
    note_off(0, 77, SRC_CANON, 0);
    trk[0].canon = 9; seq[0].pos = 3; canon_fire(0); render();   /* R 3RD: (7 - 3 - 4) = step 1 */
    assert(lat_note[lat_of(SRC_CANON)] == 64);
    note_off(0, 64, SRC_CANON, 0);
    trk[0].canon = 8; seq[0].pos = 4; canon_fire(0); render();   /* -OCT */
    assert(lat_note[lat_of(SRC_CANON)] == 48);
    note_off(0, 48, SRC_CANON, 0);
    trk[0].canon = 0; seq[0].pos = 4; canon_fire(0); render(); assert(lat_of(SRC_CANON) < 0);
    trk[0].canon = 1; seq[0].pos = 0; canon_fire(0); render(); assert(lat_of(SRC_CANON) < 0);   /* (an OFF step) */
    /* on a maqam: the scale's degrees, tuned at playback (Bayati on D: a 3RD over E half-flat is G) */
    clean(); trk[0].scale = 18; trk[0].root = 2; seq[0].steps = 2;
    step(0, 0)->note = 62;
    trk[0].canon = 1; seq[0].pos = 1; canon_fire(0); render();
    assert(lat_note[lat_of(SRC_CANON)] == 65);
    note_off(0, 65, SRC_CANON, 0);
    step(0, 0)->note = 64; trk[0].canon = 4; canon_fire(0); render();   /* OCT: E half-flat an octave up, still flat */
    assert(lat_note[lat_of(SRC_CANON)] == 76 && voice_tuning_dc(&vc[lat_voice[lat_of(SRC_CANON)]]) == -500);
    /* a chord step: its chord at the interval; STOP lets everything go */
    clean(); trk[0].scale = 1; seq[0].steps = 2; step(0, 0)->note = 60; step(0, 0)->chord = 0x80 | CH_MAJ;
    trk[0].canon = 1; seq[0].pos = 1; canon_fire(0); render();
    for (n = 0, i = 0; i < nlat; i++) n += lat_src[i] == SRC_CANON;
    assert(n == 3);
    seq_on = 1; seq_stop(); assert(!nlat);
    /* the whole sequencer playing, CANON on, for a while: the notes never pile up */
    clean(); trk[0].scale = 18; trk[0].root = 2; seq[0].steps = 16;
    for (i = 0; i < 16; i++) step(0, i)->note = 62 + (int32_t)(i % 5), step(0, i)->len = 1 + (int32_t)(i % 3), step(0, i)->vel = 20 + (int32_t)i * 6;
    for (t = 1; t < NCANON; t++) {
        trk[0].canon = (int32_t)t; seq_start(0);
        for (i = 0; i < 200; i++) { render(); assert(nlat <= NV); }
        seq_stop(); assert(!nlat);
    }
    { char s[24]; sel = 0; trk[0].canon = 12; knob_text(PG_CANON, 0, s); assert(!strcmp(s, "R -3RD")); }

    /* ---- storage: the extras with the project and the slots, older saves at their defaults */
    clean();
    for (t = 0; t < NT; t++) {
        trk[t].canon = (int32_t)(t * 3 + 1);
        for (i = 0; i < SEQ_N; i++) step(t, i)->vel = (int32_t)(1 + (i * 7 + t) % 127);
    }
    dr_mood = DM_DOR, dr_dens = 7, dr_evol = 1, dr_drft = 2;
    pk_work(work); pk_slot(slot);
    clean(); dr_mood = 0, dr_dens = 1, dr_evol = 0, dr_drft = 1;
    uk_work(work, UW_GLOBALS | UW_CHAIN, OBJ_VERS[0]);
    for (t = 0; t < NT; t++) {
        assert(trk[t].canon == (int32_t)(t * 3 + 1));
        for (i = 0; i < SEQ_N; i++) assert(step(t, i)->vel == (int32_t)(1 + (i * 7 + t) % 127));
    }
    assert(dr_mood == DM_DOR && dr_dens == 7 && dr_evol == 1 && dr_drft == 2);
    uk_work(work, UW_GLOBALS | UW_CHAIN, OBJ_VERS[1]);   /* (version 9: the same bytes before the extras) */
    for (t = 0; t < NT; t++) { assert(trk[t].canon == 0); for (i = 0; i < SEQ_N; i++) assert(step(t, i)->vel == 127); }
    assert(dr_mood == DM_MAQAM && dr_dens == 5 && !dr_evol && dr_drft == 4);
    clean(); uk_slot(slot, 0);
    assert(trk[2].canon == 7 && step(3, 10)->vel == (int32_t)(1 + (70 + 3) % 127));
    memset(slot + SLOT_PK + FM_ALL, 0, XT_ALL);  /* (a stream from before: zeros there) */
    uk_slot(slot, 0); assert(trk[2].canon == 0 && step(3, 10)->vel == 127);
    assert(4u * SLOT_ALL <= SLS_N * SLS_P && OBJ_HDR + WORK_PK + 4u <= 4096u);
    /* through the flash: saved, booted, recalled */
    clean(); trk[1].canon = 5; step(1, 7)->note = 60; step(1, 7)->vel = 33; seq[1].steps = 8;
    slot_save(2); assert(!autosave());
    clean(); store_boot(); assert(trk[1].canon == 5 && step(1, 7)->vel == 33);
    clean(); slot_recall(2); assert(trk[1].canon == 5 && step(1, 7)->vel == 33);
    /* the editor: format 4's slot as it is; format 3's (a backup from before) with the defaults after it */
    {
        uint8_t s4[SLOT_ALL];
        clean(); trk[0].canon = 6; step(0, 0)->note = 61; step(0, 0)->vel = 21; pk_slot(s4);
        assert(!put_obj(EO_SLOT + 1, ED_FMT, s4, SLOT_ALL));
        clean(); slot_recall(1); assert(trk[0].canon == 6 && step(0, 0)->vel == 21);
        assert(!put_obj(EO_SLOT + 1, ED_FMT3, s4, SLOT_ALL - XT_ALL));
        clean(); slot_recall(1); assert(trk[0].canon == 0 && step(0, 0)->note == 61 && step(0, 0)->vel == 127);
        assert(put_obj(EO_SLOT + 1, ED_FMT3, s4, SLOT_ALL));   /* (format 3 at format 4's size: refused) */
        ed_obj = -1;
        set.ver = SET_VER; set.vel = 2; memcpy(rx, &set, sizeof set);
        assert(!put_obj(EO_SET, ED_FMT3, rx, sizeof set - 1u)); assert(set.vel == 0);
        rx[sizeof set - 1u] = 3; assert(!put_obj(EO_SET, ED_FMT, rx, sizeof set)); assert(set.vel == 3 && sy_vel == 3);
        rx[sizeof set - 1u] = 4; put_obj(EO_SET, ED_FMT, rx, sizeof set); assert(set.vel == 3);   /* (out of range: refused) */
    }

    /* ---- the keys' velocity: SYSTEM 2's VELOCITY sets the scan's word; FIXED: every key 127 at once */
    clean(); sy_vel = 0; sys_sync(); assert(kv_config == 0 && key_vel(3) == 127);
    assert(kv_keys(0x8u, 100, 127, 0, 0) == 0x8u);
    sy_vel = 2; sys_sync(); assert(kv_config == (1u | 1u << 1 | 32u) && set.vel == 2);
    kv_hw_on = 1;                                /* (the FM-1's scan: it senses) */
    kv_seen = 0; kv_capture[3].landing = 0;
    assert(kv_keys(0x8u, 1000, 127, 0, 0) == 0);   /* a press without its landing yet: it waits */
    assert(kv_keys(0x8u, 1005, 127, 0, 0) == 0);
    assert(kv_keys(0x8u, 1000 + KV_TIMEOUT_MS, 127, 0, 0) == 0x8u && key_vel(3) == 127);   /* (none: FIXED's) */
    kv_seen = 0; kv_keys(0, 1100, 127, 0, 0);
    kv_capture[3].landing = KV_READY | 600u;
    assert(kv_keys(0x8u, 1200, 127, 0, 0) == 0x8u && key_vel(3) == kv_map(3, 600, kv_config) && key_vel(3) < 127);
    kv_keys(0, 1300, 127, 0, 0); kv_capture[3].landing = KV_READY | 550u;   /* (the hardest the curve has) */
    kv_keys(0x8u, 1400, 127, 0, 0); assert(key_vel(3) == 127);
    kv_keys(0, 1500, 127, 0, 0); kv_capture[3].landing = 0;
    assert(kv_keys(0x8u, 1600, 127, 1, 0) == 0x8u && key_vel(3) == 127);   /* (a layer held: at once, FIXED) */
    { char s[24]; knob_text(PG_SYS2, 3, s); assert(!strcmp(s, "SENSE SOFT")); }
    sy_vel = 0; sys_sync(); kv_hw_on = 0;

    /* ---- DREAM: a piece in the key, MAQAM: the selected track's tuning on all four */
    clean(); tl_rng = 0x12345u;
    trk[0].eng = ENG_WHEEL; trk[1].eng = ENG_PHYS; trk[2].eng = ENG_VA; trk[3].eng = ENG_FM;
    trk[2].env[0] = 0; trk[3].env[0] = 0;
    sel = 1; trk[1].scale = 18; trk[1].tuning_mode = 1; trk[1].tash_on[4] = 1; trk[1].tash_cents[4] = -37;
    trk[1].retune = 2; trk[1].trim[3] = 9; trk[2].chord = 1; trk[3].arp = 1;
    dr_mood = DM_MAQAM; dr_dens = 5;
    dream_compose(2);
    assert(playing && nq_w == 1 && (nq[0] & 0xFFu) == NQ_PLAY);
    assert(!strcmp(dream_said(), "D BAYATI"));
    for (t = 0; t < NT; t++) {
        uint32_t c = 0;
        assert(trk[t].scale == 18 && trk[t].root == 2 && trk[t].tuning_mode == 1 && trk[t].tash_on[4] == 1 &&
               trk[t].tash_cents[4] == -37 && trk[t].retune == 2 && trk[t].trim[3] == 9);
        assert(!trk[t].chord && !trk[t].arp && seq[t].steps == 64 && seq[t].div == 2);
        for (i = 0; i < SEQ_N; i++) {
            const seq_step_t *s = step(t, i);
            if (!s->note) continue;
            c++, notes++;
            assert(sc_in(t, s->note) && s->vel >= 1 && s->vel <= 127 && s->len >= 1 && i + (uint32_t)s->len <= SEQ_N);
            assert(s->chance >= 1 && s->chance <= 100);
            assert(!s->chord || s->chord == (int32_t)DR_TRIAD || s->chord == (int32_t)DR_FIFTH);
        }
        assert(c);
    }
    assert(dream.role[0] == DR_PAD && dream.role[1] == DR_ARP && dream.role[2] == DR_MELODY && dream.role[3] == DR_HIGH);
    for (i = 0; i < 4; i++) assert(step(0, i * 16)->chord == (int32_t)DR_TRIAD);   /* (the pad: a triad a chord) */
    /* a mood: its scale; EVOLVE changes stay in it */
    dr_mood = DM_DOR; dream_compose(9);
    for (t = 0; t < NT; t++) assert(trk[t].scale == 3 && trk[t].root == 9);
    for (k = 0; k < 300; k++) {
        dream_evolve();
        for (t = 0; t < NT; t++) for (i = 0; i < SEQ_N; i++) if (step(t, i)->note) assert(sc_in(t, step(t, i)->note));
    }
    dream_forget(); dream_evolve(); assert(dream.valid);   /* (after a load: rebuilt from the steps) */
    /* every mood and density, played for a while */
    for (k = 0; k < NDMOOD * 8u; k++) {
        clean(); trk[0].eng = (int32_t)(k % NENG); trk[1].eng = ENG_VA; trk[2].eng = ENG_SAMPLE; trk[3].eng = ENG_VOICE;
        sel = 0; trk[0].scale = 22; dr_mood = (int32_t)(k % NDMOOD); dr_dens = (int32_t)(1 + k / NDMOOD);
        dream_compose(k % 12u); seq_start(0);
        for (i = 0; i < 60; i++) { render(); assert(nlat <= NV); }
        seq_stop();
    }
    /* the page: over steps, the same key again within 3 s; undo brings them back */
    clean(); step(2, 5)->note = 70; dream.asked = 0; dream.ask_key = -1; page = PG_DREAM; dr_mood = DM_MAJ;
    dream_key(4, 57); assert(step(2, 5)->note == 70 && !playing);
    fm1_ms += 4000; dream_key(4, 57); assert(step(2, 5)->note == 70);   /* (too late: asked again) */
    dream_key(4, 57); assert(playing && trk[0].root == 9 && trk[0].scale == 1);
    assert(!undo() && step(2, 5)->note == 70 && seq[2].steps == 16);
    song_mode = SM_SONG; playing = 0; dream_key(1, 54); assert(!playing);   /* (SONG mode: not over a song) */
    { slab_t slab; song_mode = SM_OFF; dr_mood = DM_MAQAM; page = PG_DREAM; ui_slab_make(0, &slab); assert(!strcmp(slab.val, "MAQAM")); }

    printf("VELOCITY/CANON/DREAM/ARP OK: gains, keys, MIDI asks, steps, ratchets, arp, sensing; TRILL, DRONE; %u CANON modes played; "
           "storage v10 + v9 + format 3; DREAM: a %u-note MAQAM piece, 7 moods x 8 densities played, 300 EVOLVE "
           "changes in scale\n",
           NCANON - 1, notes);
    return 0;
}
