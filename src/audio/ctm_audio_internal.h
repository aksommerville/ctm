#ifndef CTM_AUDIO_INTERNAL_H
#define CTM_AUDIO_INTERNAL_H

#include "ctm.h"
#include "ctm_audio.h"
#include "ctm_song.h"
#include <pthread.h>

#if CTM_AUDIO_DISABLE
  // Stub!
#elif CTM_USE_alsa
  #include "opt/alsa/ctm_alsa.h"
#elif CTM_USE_sdl
  #include "opt/sdl/ctm_sdl.h"
#else
  // Stub by default.
#endif

/* Sound effect.
 */

struct ctm_audio_effect {
  int effectid;
  int16_t *samplev;
  int samplec;
};

/* Globals.
 */

extern struct ctm_audio {
  int enabled;
  pthread_mutex_t mtx;
  int mtxinit;

  // Mixer.
  struct ctm_audio_channel_verbatim *fxchv;
  int fxchc,fxcha;
  struct ctm_song *song; // weak; also contained in songv.
  struct ctm_song *song_out;
  int song_out_counter;

  // Effects store. Populated at init and constant until quit.
  struct ctm_audio_effect *fxv;
  int fxc,fxa;

  // Song store. Ditto.
  struct ctm_song **songv;
  int songc,songa;

  double *refwave;
  double *synthscratch;
  
} ctm_audio;

/* Private API.
 */

int ctm_audio_lock();
int ctm_audio_unlock();

int ctm_audio_effect_search(int effectid);
int ctm_audio_song_search(int songid);

// Call these only during init.
int ctm_audio_load_effects();
int ctm_audio_load_songs();
int ctm_audio_effect_insert(int p,int effectid,int16_t *samplev,int samplec); // ONLY legal before initializing backend! handoff samplev
int ctm_audio_insert_song(struct ctm_song *song);

#endif
