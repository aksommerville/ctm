#include "ctm_audio_internal.h"

#define CTM_AUDIO_SONG_OUT_TIME 10000

struct ctm_audio ctm_audio={0};

/* Main callback.
 */

int phase=0;

static void ctm_audio_callback(int16_t *dst,int dstc) {
  if (pthread_mutex_lock(&ctm_audio.mtx)) return;
  while (dstc-->0) {
    int sample=0,i;

    for (i=0;i<ctm_audio.fxchc;i++) {
      struct ctm_audio_channel_verbatim *ch=ctm_audio.fxchv+i;
      if ((ch->samplep<0)||(ch->samplep>=ch->samplec)) continue;
      if (ch->level==0xff) {
        sample+=ch->samplev[ch->samplep++];
      } else {
        sample+=(ch->samplev[ch->samplep++]*ch->level)>>8;
      }
    }

    if (ctm_audio.song) {
      sample+=ctm_song_update(ctm_audio.song);
    }
    if (ctm_audio.song_out_counter>0) {
      sample+=(ctm_song_update(ctm_audio.song_out)*ctm_audio.song_out_counter)/CTM_AUDIO_SONG_OUT_TIME;
      ctm_audio.song_out_counter--;
    }
    
    if (sample<-32768) *dst=-32768;
    else if (sample>32767) *dst=32767;
    else *dst=sample;
    dst++;
  }
  pthread_mutex_unlock(&ctm_audio.mtx);
}

/* Init.
 */

int ctm_audio_init(const char *device) {
  memset(&ctm_audio,0,sizeof(struct ctm_audio));

  #if CTM_AUDIO_DISABLE
    return 0;
  #endif
  
  if (pthread_mutex_init(&ctm_audio.mtx,0)) return -1;
  ctm_audio.mtxinit=1;

  #if CTM_USE_mmal
    ctm_audio.enabled=1;
    if (ctm_audio_load_effects()<0) return -1;
    if (ctm_audio_load_songs()<0) return -1;
    if (ctm_mmal_init(ctm_audio_callback)<0) return -1;
  #elif CTM_USE_alsa
    ctm_audio.enabled=1;
    if (ctm_audio_load_effects()<0) return -1;
    if (ctm_audio_load_songs()<0) return -1;
    if (ctm_alsa_init(ctm_audio_callback,device)<0) return -1;
  #elif CTM_USE_sdl
    ctm_audio.enabled=1;
    if (ctm_audio_load_effects()<0) return -1;
    if (ctm_audio_load_songs()<0) return -1;
    if (ctm_sdl_init_audio(ctm_audio_callback)<0) return -1;
  #endif
  
  return 0;
}

/* Quit.
 */

void ctm_audio_quit() {
  #if CTM_AUDIO_DISABLE
    return;
  #elif CTM_USE_mmal
    ctm_mmal_quit();
  #elif CTM_USE_alsa
    ctm_alsa_quit();
  #elif CTM_USE_sdl
    ctm_sdl_quit_audio();
  #endif
  if (ctm_audio.mtxinit) pthread_mutex_destroy(&ctm_audio.mtx);
  if (ctm_audio.fxchv) free(ctm_audio.fxchv);
  if (ctm_audio.fxv) {
    while (ctm_audio.fxc-->0) if (ctm_audio.fxv[ctm_audio.fxc].samplev) free(ctm_audio.fxv[ctm_audio.fxc].samplev);
    free(ctm_audio.fxv);
  }
  if (ctm_audio.songv) {
    while (ctm_audio.songc-->0) ctm_song_del(ctm_audio.songv[ctm_audio.songc]);
    free(ctm_audio.songv);
  }
  if (ctm_audio.refwave) free(ctm_audio.refwave);
  if (ctm_audio.synthscratch) free(ctm_audio.synthscratch);
  memset(&ctm_audio,0,sizeof(struct ctm_audio));
}

/* Update.
 */

int ctm_audio_update() {
  // No backend needs maintenance.
  return 0;
}

/* Test compilation.
 */

int ctm_audio_available() {
  return ctm_audio.enabled;
}

/* Mutex.
 */
 
int ctm_audio_lock() {
  if (pthread_mutex_lock(&ctm_audio.mtx)) return -1;
  return 0;
}

int ctm_audio_unlock() {
  pthread_mutex_unlock(&ctm_audio.mtx);
  return 0;
}

/* Sound effects store.
 */
 
int ctm_audio_effect_search(int effectid) {
  int lo=0,hi=ctm_audio.fxc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (effectid<ctm_audio.fxv[ck].effectid) hi=ck;
    else if (effectid>ctm_audio.fxv[ck].effectid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ctm_audio_effect_insert(int p,int effectid,int16_t *samplev,int samplec) {
  if ((p<0)||(p>ctm_audio.fxc)) return -1;
  if (p&&(effectid<=ctm_audio.fxv[p-1].effectid)) return -1;
  if ((p<ctm_audio.fxc)&&(effectid>=ctm_audio.fxv[p].effectid)) return -1;
  if (!samplev||(samplec<1)) return -1;
  if (ctm_audio.fxc>=ctm_audio.fxa) {
    int na=ctm_audio.fxa+16;
    if (na>INT_MAX/sizeof(struct ctm_audio_effect)) return -1;
    void *nv=realloc(ctm_audio.fxv,sizeof(struct ctm_audio_effect)*na);
    if (!nv) return -1;
    ctm_audio.fxv=nv;
    ctm_audio.fxa=na;
  }
  memmove(ctm_audio.fxv+p+1,ctm_audio.fxv+p,sizeof(struct ctm_audio_effect)*(ctm_audio.fxc-p));
  ctm_audio.fxc++;
  struct ctm_audio_effect *fx=ctm_audio.fxv+p;
  memset(fx,0,sizeof(struct ctm_audio_effect));
  fx->effectid=effectid;
  fx->samplev=samplev;
  fx->samplec=samplec;
  return 0;
}

/* Acquire verbatim channel. Call locked.
 */

static struct ctm_audio_channel_verbatim *ctm_audio_channel_verbatim_new() {
  int i; for (i=0;i<ctm_audio.fxchc;i++) {
    if (ctm_audio.fxchv[i].samplep<0) return ctm_audio.fxchv+i;
    if (ctm_audio.fxchv[i].samplep>=ctm_audio.fxchv[i].samplec) return ctm_audio.fxchv+i;
  }
  if (ctm_audio.fxchc>=ctm_audio.fxcha) {
    int na=ctm_audio.fxcha+8;
    if (na>INT_MAX/sizeof(struct ctm_audio_channel_verbatim)) return 0;
    void *nv=realloc(ctm_audio.fxchv,sizeof(struct ctm_audio_channel_verbatim)*na);
    if (!nv) return 0;
    ctm_audio.fxchv=nv;
    ctm_audio.fxcha=na;
  }
  return ctm_audio.fxchv+ctm_audio.fxchc++;
}

/* Play effect.
 */

int ctm_audio_effect(int effectid,uint8_t level) {
  if (!ctm_audio.enabled) return 0;
  int fxp=ctm_audio_effect_search(effectid);
  if (fxp<0) return -1;
  struct ctm_audio_effect *fx=ctm_audio.fxv+fxp;

  // Don't play this effect if another instance of it has just started.
  int i; for (i=0;i<ctm_audio.fxchc;i++) {
    if (ctm_audio.fxchv[i].samplev!=fx->samplev) continue;
    if (ctm_audio.fxchv[i].samplep<0) continue;
    if (ctm_audio.fxchv[i].samplep>3000) continue; // Arbitrary.
    return 0;
  }
  
  if (pthread_mutex_lock(&ctm_audio.mtx)) return -1;
  struct ctm_audio_channel_verbatim *ch=ctm_audio_channel_verbatim_new();
  if (!ch) { pthread_mutex_unlock(&ctm_audio.mtx); return -1; }
  ch->samplev=fx->samplev;
  ch->samplec=fx->samplec;
  ch->samplep=0;
  ch->level=level;
  pthread_mutex_unlock(&ctm_audio.mtx);
  return 0;
}

/* Set background music.
 */

int ctm_audio_song_search(int songid) {
  int lo=0,hi=ctm_audio.songc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (songid<ctm_audio.songv[ck]->songid) hi=ck;
    else if (songid>ctm_audio.songv[ck]->songid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ctm_audio_insert_song(struct ctm_song *song) {
  if (!song) return -1;
  int p=ctm_audio_song_search(song->songid);
  if (p>=0) return -1;
  p=-p-1;
  if (ctm_audio.songc>=ctm_audio.songa) {
    int na=ctm_audio.songa=4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctm_audio.songv,sizeof(void*)*na);
    if (!nv) return -1;
    ctm_audio.songv=nv;
    ctm_audio.songa=na;
  }
  memmove(ctm_audio.songv+p+1,ctm_audio.songv+p,sizeof(void*)*(ctm_audio.songc-p));
  ctm_audio.songc++;
  ctm_audio.songv[p]=song;
  return p;
}

static int ctm_audio_play_song_1(int songid) {
  int p=ctm_audio_song_search(songid);
  struct ctm_song *song=(p>=0)?ctm_audio.songv[p]:0;
  if (song==ctm_audio.song) return 0;
  if (ctm_audio.song) {
    ctm_audio.song_out=ctm_audio.song;
    ctm_audio.song_out_counter=CTM_AUDIO_SONG_OUT_TIME;
  }
  if (ctm_audio.song=song) {
    ctm_song_reset(song);
  }
  return 0;
}

int ctm_audio_play_song(int songid) {
  if (!ctm_audio.enabled) return 0;
  if (ctm_audio_lock()<0) return -1;
  int err=ctm_audio_play_song_1(songid);
  ctm_audio_unlock();
  return err;
}
