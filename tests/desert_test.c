/* SPDX-License-Identifier: GPL-3.0-only */
/* SARDAB integration checks against the real firmware translation unit. */
#define DINGHY_HOST
#define main firmware_host_main
#include "../firmware/src/grimm.c"
#undef main
#include <assert.h>
#include <math.h>

static const track_t defaults = TRK_DEF;
static void clean(void)
{
    unsigned t;
    memset(vc, 0, sizeof vc); nlat = 0; nq_r = nq_w = 0; ec_r = ec_w = 0;
    trk_quiet = trk_mute = trk_solo = 0; tune_f = 0;
    for (t = 0; t < NT; t++) { trk[t] = defaults; lfo_p16[t] = 0; fm_builtin(t, 0); }
    seq_init();
}
static double hz(unsigned note, double cents) { return 440 * pow(2, ((double)note - 69) / 12 + cents / 1200); }
static double error(double actual, double expected) { return fabs(1200 * log2(actual / expected)); }
static void owners(void)
{
    unsigned i, j;
    assert(nlat <= NV);
    for (i = 0; i < nlat; i++) {
        assert(lat_voice[i] < voice_limit(trk[lat_trk[i]].eng));
        for (j = i + 1; j < nlat; j++) assert(lat_voice[i] != lat_voice[j]);
    }
}
static void render(void) { int32_t out[2*VB] = {0}; app_audio(out, VB); }
int main(void)
{
    unsigned s, root, note, i, n, t; int trim;
    double worst = 0, worst_fm = 0;
    uint8_t notes[CH_MAX], work[WORK_PK], slot[SLOT_ALL], sound[SND_SIZE];
    clean();
    /* Independent mathematical oracle: catalog offsets + temperament + trims. */
    for (s = 0; s < (unsigned)NSCALE; s++) for (root = 0; root < 12; root++)
    for (note = 12; note < 116; note++) for (trim = -63; trim <= 63; trim++) {
        unsigned pc = (note + 12 - root) % 12;
        double want, got, e;
        trk[0].scale = s; trk[0].root = root; trk[0].trim[pc] = trim;
        vc[0].note = note; vc[0].trk = 0;
        want = hz(note, SCALE_CENTS[s][pc] + trim);
        got = voice_inc(&vc[0], VP16(&vc[0])) * (44100.0 / 4294967296.0);
        e = error(got, want); if (e > worst) worst = e;
        assert(e < 0.5);
        got = fm6_freq(voice_logfreq(&vc[0],0)) * (44100.0 / 16777216.0);
        e = error(got, want); if (e > worst_fm) worst_fm = e;
        if (e >= 0.6) { fprintf(stderr,"FM s=%u root=%u note=%u trim=%d p16=%d fine=%d got=%f want=%f err=%f\n",s,root,note,trim,VP16(&vc[0]),VFINE(&vc[0]),got,want,e); return 1; }
        trk[0].trim[pc] = 0;
    }
    clean(); trk[0].scale = 16; /* Rast on C */
    assert(tuning_dc(0,64) == -500 && tuning_dc(0,71) == -500 && tuning_dc(0,60) == 0);
    trk[1].scale = 18; trk[1].root = 2; /* Bayati on D */
    assert(tuning_dc(1,64) == -500 && tuning_dc(1,71) == 0);
    for (note = 0; note < 128; note++) assert(sc_key(0,note) == (int)note);
    trk[0].map = MAP_ALL;
    { const int want[] = {60,62,64,65,67,69,71,72};
      for(i=0;i<8;i++) assert(sc_key(0,60+i)==want[i]); }
    for (s=0;s<(unsigned)NSCALE;s++) {
        int last=-1; trk[0].scale=s;
        for(note=0;note<128;note++) { int got=sc_key(0,note); if(got>=0){assert(got>last);last=got;} }
    }
    trk[0].scale=16; trk[0].map=MAP_WHITE; assert(sc_key(0,61)==-1 && sc_key(0,62)==62);
    trk[0].map=MAP_CHR; trk[0].ch_type=CH_SCALE7; trk[0].chord=1;
    n=chord_notes(0,60,notes); assert(n==4 && notes[1]==64 && notes[3]==71);
    note_on(0,7,60,SRC_KEY,0,-1,127); render(); assert(nlat==4); owners();
    for(i=0;i<nlat;i++) {
        voice_t *v=&vc[lat_voice[i]]; int32_t out[CTL]={0}; uint32_t before=v->ph[0];
        va_block(v,trk[0].p[ENG_VA],out,20000,20000);
        assert(v->ph[0]-before == voice_inc(v,VP16(v))*CTL);
    }
    /* Release by physical identity after a layout/root change. */
    trk[0].root=7; trk[0].map=MAP_ALL; note_off(0,7,SRC_KEY,0); assert(!nlat);
    /* Recorded chord steps and arpeggiated notes resolve tuning at playback. */
    clean(); trk[0].scale=16; trk[0].ch_type=CH_SCALE7;
    seq_note(0,60,24,0x80|CH_SCALE7,127); render(); assert(nlat==4);
    for(i=0;i<nlat;i++) if(lat_note[i]==64 || lat_note[i]==71) assert(tuning_dc(0,lat_note[i])==-500);
    note_off(0,60,SRC_SEQ,0); assert(!nlat);
    arp_play(0,64); render(); assert(nlat==1 && vc[lat_voice[0]].note==64);
    assert(tuning_dc(0,64)==-500); arp_release(0); assert(!nlat);
    trk[0].temper=3; assert(tuning_dc(0,64)==-637); trk[0].trim[4]=13; assert(tuning_dc(0,64)==-507);
    /* Every pitched source executes the fine-pitch path. */
    clean(); trk[0].scale=16; trk[0].trim[4]=13;
    ima_init(); assert(!smp_factory(0));
    for(i=0;i<1000 && !smp[0].ready;i++) samples_poll(1);
    assert(smp[0].ready);
    for(s=0;s<NENG;s++) {
        int32_t out[CTL]={0}; voice_t *v=&vc[0];
        trk[0].eng=s; v->want_note=64; v->want_trk=0; v->want_on=1;
        if(s==ENG_FM) { trk[0].fm_alg=31;trk[0].fm_fb=0;trk[0].p[s][3]=0;
            for(i=0;i<6;i++){trk[0].fmo[i][FO_RATIO]=1;trk[0].fmo[i][FO_FINE]=0;} }
        voice_begin(v); vblock++;
        uint32_t before=v->ph[0];
        ENGINES[s]->block(v,trk[0].p[s],out,20000,20000);
        if(s==ENG_VA || s==ENG_VOICE || s==ENG_PHASE)
            assert(v->ph[0]-before == voice_inc(v,VP16(v))*CTL);
        if(s==ENG_FM) assert(error(fm_note[0].op[0].freq * (44100.0/16777216.0),hz(64,-37))<0.6);
        if(s==ENG_SAMPLE) {
            double ratio=((double)sp[0].ui*65536+sp[0].uf)/CTL/65536;
            int32_t sample_root=smp[0].zones ? (int16_t)rd16(smp[0].zones+24*sp[0].zone+16) : smp[0].root16;
            double want=0.5*pow(2,((64.0-sample_root/16.0)*100-37)/1200);
            assert(error(ratio,want)<1);
        }
    }
    /* Tashriq: absolute pitch classes, independent of maqam/root/trim, every octave. */
    clean(); trk[0].scale=16;trk[0].root=7;trk[0].trim[4]=30;trk[0].temper=3;trk[0].tuning_mode=1;
    for(i=0;i<128;i++)assert(tuning_dc(0,i)==0);
    trk[0].tash_on[4]=1;
    assert(tuning_dc(0,64)==-500 && tuning_dc(0,76)==-500 && tuning_dc(0,65)==0 && tuning_dc(1,64)==0);
    assert(fabs(hz(64,-50)-320.2437)<0.001);
    trk[0].tash_cents[4]=-37;assert(tuning_dc(0,64)==-370);
    /* Retuning held notes does not retrigger envelopes; glide converges, hold latches at attack. */
    vc[0].want_note=64;vc[0].want_trk=0;voice_begin(&vc[0]);
    trk[0].retune=2;trk[0].tash_on[4]=0;voice_tuning_tick(&vc[0]);assert(voice_tuning_dc(&vc[0])==-370);
    trk[0].retune=1;vc[0].tuning_now=-370;
    voice_tuning_tick(&vc[0]);assert(voice_tuning_dc(&vc[0])>-370 && voice_tuning_dc(&vc[0])<0);
    for(i=0;i<1000;i++)voice_tuning_tick(&vc[0]);assert(voice_tuning_dc(&vc[0])==0);
    trk[0].retune=0;trk[0].tash_on[4]=1;assert(voice_tuning_dc(&vc[0])==-370);
    trk[0].tuning_mode=0;trk[0].tuning_system=1;trk[0].root=0;trk[0].trim[4]=0;trk[0].table[4]=-373;
    assert(tuning_dc(0,64)==-373); /* no inherited factory -50 or temperament */
    pk_work(work); clean();uk_work(work,UW_GLOBALS|UW_CHAIN,OBJ_VERS[0]);assert(trk[0].table[4]==-373 && trk[0].tash_cents[4]==-37 && trk[0].tash_on[4]);
    snd_apply(SND_FACTORY[0],0);assert(trk[0].table[4]==-373);
    /* New factory banks: every zone at its root; one-shot exhaustion remains silent. */
    clean();
    for(t=0;t<SMP_SLOTS;t++) {
        assert(!smp_factory(t));for(i=0;i<1000&&!smp[t].ready;i++)samples_poll(1);
        assert(smp[t].ready && smp[t].zones && smp_header_ok(SMP_FACT_HDR[t]));
        for(s=0;s<smp[t].nz;s++) {
            int32_t out[CTL];uint32_t energy=0,j;voice_t *v=&vc[0];
            trk[0].eng=ENG_SAMPLE;trk[0].p[ENG_SAMPLE][0]=t;
            v->want_trk=0;v->want_note=rd16(smp[t].zones+24*s+16)/16;voice_begin(v);
            for(j=0;j<12000 && !sp[0].done;j++) {
                memset(out,0,sizeof out);sampler_block(v,trk[0].p[ENG_SAMPLE],out,20000,20000);
                for(i=0;i<CTL;i++)energy|=(uint32_t)out[i];
            }
            assert(sp[0].zone==s && sp[0].done && energy);
            memset(out,0,sizeof out);sampler_block(v,trk[0].p[ENG_SAMPLE],out,20000,20000);
            for(i=0;i<CTL;i++)assert(out[i]==0);
        }
        {uint8_t bad[SMP_HDR_LEN];memcpy(bad,SMP_FACT_HDR[t],sizeof bad);bad[64]^=1;assert(!smp_header_ok(bad));}
    }
    /* A repeated one-shot at the same pitch starts again, even if its voice is still held. */
    clean();trk[0].eng=ENG_SAMPLE;voice_start(0,0,60,127);render();
    sp[0].done=1;voice_start(0,0,60,127);render();assert(!sp[0].done && sp[0].ui>0);
    /* Every desert preset really renders; no percussion preset in this category. */
    clean();
    for(s=0;s<SND_NFACT;s++)if(SND_FACTORY[s][1]==CAT_DESERT) {
        uint32_t energy=0,j;int32_t out[CTL];voice_t *v=&vc[0];
        snd_apply(SND_FACTORY[s],0);v->want_note=60;v->want_trk=0;voice_begin(v);
        for(j=0;j<128;j++){memset(out,0,sizeof out);vblock++;ENGINES[v->eng]->block(v,trk[0].p[v->eng],out,20000,20000);for(i=0;i<CTL;i++)energy|=(uint32_t)out[i];}
        assert(energy);
    }
    /* Twelve simultaneous voices, then switching a held track to constrained engines. */
    clean();
    for(t=0;t<NT;t++) for(i=0;i<3;i++) note_on(t,i,48+t*12+i*4,SRC_KEY,0,0,127);
    render(); assert(nlat==12); owners();
    trk[0].eng=ENG_PHYS; restart_track(0); owners();
    for(i=0;i<20;i++) render();
    for(i=0;i<8;i++) { note_on(0,20+i,60+i,SRC_KEY,0,0,127); owners(); render(); }
    n=0;for(i=0;i<nlat;i++) n+=lat_trk[i]==0; assert(n==PHYS_NV);
    trk[0].eng=ENG_SAMPLE; restart_track(0); owners();
    for(i=0;i<20;i++) render();
    trk[0].eng=ENG_FM; restart_track(0); owners();
    for(i=0;i<20;i++) render();
    for(t=0;t<NT;t++) release_keys(t); assert(!nlat);
    /* Project, song slot and undo state carry independent per-track tuning. */
    clean();
    for(t=0;t<NT;t++) { trk[t].root=t*2;trk[t].scale=16+t;trk[t].map=t;trk[t].temper=t;
        trk[t].custom=0x5ad; for(i=0;i<12;i++)trk[t].trim[i]=(int)i-6; }
    pk_work(work); pk_slot(slot); clean(); uk_work(work,UW_GLOBALS|UW_CHAIN,OBJ_VERS[0]);
    for(t=0;t<NT;t++){assert(trk[t].root==(int)t*2 && trk[t].scale==(int)16+t);assert(trk[t].trim[0]==-6 && trk[t].temper==(int)t);}
    clean();uk_slot(slot,1);assert(trk[3].scale==19 && trk[3].trim[11]==5);
    /* Sound changes keep performance tuning. */
    snd_pack(sound,0,0,0); snd_apply(sound,3); assert(trk[3].scale==19);
    sel=0;page=PG_TUNE1;trk[0].trim[0]=-13;
    { slab_t slab; ui_slab_make(0,&slab); assert(!strcmp(slab.val,"-13")); }
    page=PG_SCALE;knob_step(page,2,99);assert(trk[0].map==MAP_ALL);
    /* Flash boot and editor buffer separation. */
    slot_save(0); assert(!autosave()); clean(); store_boot(); assert(trk[3].scale==19);
    slot_recall(0);assert(trk[3].trim[11]==5);
    assert(SLOT_ALL <= (unsigned)(ED_FX-ED_RX));
    printf("DESERT OK: 7,924,800 pitch cases; worst oscillator %.4f ct, FM %.4f ct; mapping/chords/12 voices/engine switches/storage/UI\n",worst,worst_fm);
    return 0;
}
