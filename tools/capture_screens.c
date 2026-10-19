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
    ui_boot();write_ppm_as("build/screens/splash.ppm");
    fm1_ms=4000;
    snd_apply(SND_FACTORY[40],0);brw_at(0,SND_FACTORY[40]);
    trk[0].scale=16;trk[0].map=MAP_CHR;
    note_on(0,7,60,SRC_KEY,0,-1);
    for(k=0;k<100;k++)app_audio(audio,VB);
    shot("desert-oud",PG_SOUND);
    note_off(0,7,SRC_KEY,0);
    note_on(0,11,64,SRC_KEY,0,-1);
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
    return 0;
}
