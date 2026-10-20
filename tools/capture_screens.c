/* SPDX-License-Identifier: GPL-3.0-only */
/* Deterministic documentation captures from the actual firmware renderer. */
#define DINGHY_HOST
#define main firmware_host_main
#include "../firmware/src/grimm.c"
#undef main
static void shot(const char *name, uint32_t pg)
{
    char path[160];
    page=pg; fm1_ms+=200; ui_update(0);
    snprintf(path,sizeof path,"build/screens/%s.ppm",name); write_ppm_as(path);
}
int main(void)
{
    uint32_t k; int32_t audio[2*VB];
    memset(fake_flash,0xff,sizeof fake_flash);
    ima_init();seq_init();
    set.col=UI_DESERT;sy_col=UI_DESERT;ui_theme_set(UI_DESERT);   /* the desert's colours (a fresh device's) */
    ui_boot();write_ppm_as("build/screens/splash.ppm");
    fm1_ms=4000;
    snd_apply(SND_FACTORY[40],0);brw_at(0,SND_FACTORY[40]);
    trk[0].scale=16;trk[0].map=MAP_CHR;
    note_on(0,7,60,SRC_KEY,0,-1,127);
    for(k=0;k<100;k++)app_audio(audio,VB);
    shot("desert-oud",PG_SOUND);
    trk[1].level=90;trk[2].level=60;trk[3].level=110;shot("home",PG_HOME);
    for(k=0;k<NT;k++)an_e[k]=256;cn_s[1]=1;cn_s[3]=2;shot("home",PG_HOME);   /* (the lanterns burning) */
    shot("mixer",PG_MIX1);                       /* (its meters: the peaks since the page opened) */
    note_on(0,7,60,SRC_KEY,0,-1,127);note_on(1,9,62,SRC_KEY,0,-1,100);
    for(k=0;k<12;k++)app_audio(audio,VB);
    shot("mixer",PG_MIX1);
    note_off(0,7,SRC_KEY,0);note_off(1,9,SRC_KEY,0);
    trk[0].scale=23;shot("scroll-a",PG_SCALE);   /* HIJAZKAR: the SCALE slab scrolls (build/screens only) */
    fm1_ms+=UI_SCROLL_MS*4;shot("scroll-b",PG_SCALE);
    trk[0].scale=16;
    shot("colour",PG_SYSTEM);
    rv_type=RT_CAVERN;shot("space",PG_SPACE);  /* the reverb's TYPE in the caption (SPACE 2 picks it) */
    rv_type=RT_HALL;
    note_off(0,7,SRC_KEY,0);
    note_on(0,11,64,SRC_KEY,0,-1,127);
    ch_last=64;shot("maqam-rast",PG_SCALE);
    ch_last=60;trk[0].chord=1;trk[0].ch_type=CH_SCALE7;
    shot("chords",PG_CHORD);
    ch_last=64;trk[0].chord=0;trk[0].tuning_mode=1;trk[0].tash_on[4]=trk[0].tash_on[11]=1;
    shot("tashriq",PG_TASH2);
    trk[0].retune=1;shot("tuning",PG_TUNING);
    for(k=0;k<16;k++) {
        static const int notes[8]={60,62,64,67,69,67,64,62};
        seq[0].pat[0].st[k].note=notes[k%8];
    }
    shot("sequencer",PG_GRID);
    for(k=0;k<16;k++) seq[0].pat[0].st[k].vel=40+(int32_t)(k*5);
    seq[0].cursor=5;shot("velocity",PG_STEP);
    trk[0].canon=9;shot("canon",PG_CANON);
    trk[0].canon=0;sel=1;trk[1].scale=18;
    trk[0].eng=ENG_WHEEL;trk[1].eng=ENG_PHYS;trk[2].env[0]=0;trk[3].eng=ENG_FM;trk[3].env[0]=0;
    dr_mood=DM_MAQAM;dream_compose(2);sel=0;shot("dream",PG_DREAM);
    return 0;
}
