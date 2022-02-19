#include "ctm.h"
#include "ctm_alsa.h"
#include <pthread.h>
#include <alsa/asoundlib.h>

// Size of hardware buffer in samples (ie 1/44100 of second).
// Lower values increase likelihood of underrun.
// Higher values uniformly increase latency.
// Must be a power of two.
// I find 2048 works, but haven't tested it under heavy load yet.
#define CTM_ALSA_BUFFER_SIZE 2048

#ifndef CTM_ALSA_DEVICE
  #define CTM_ALSA_DEVICE "default"
#endif

/* Globals.
 */

static struct {
  void (*cb)(int16_t *dst,int dstc);

  snd_pcm_t *alsa;
  snd_pcm_hw_params_t *hwparams;

  int rate;
  int chanc;
  int hwbuffersize;
  int bufc; // frames
  int16_t *buf;

  pthread_t iothd;
  pthread_mutex_t iomtx;
  
} ctm_alsa={0};

/* I/O thread.
 */

static void *ctm_alsa_iothd(void *dummy) {
  while (1) {
    pthread_testcancel();
    
    // fill buffer
    if (pthread_mutex_lock(&ctm_alsa.iomtx)) return 0;
    ctm_alsa.cb(ctm_alsa.buf,ctm_alsa.bufc);
    pthread_mutex_unlock(&ctm_alsa.iomtx);
    
    // expand to stereo
    if (ctm_alsa.chanc==2) {
      const int16_t *srcp=ctm_alsa.buf+ctm_alsa.bufc-1;
      int16_t *dstp=ctm_alsa.buf+(ctm_alsa.bufc<<1)-2;
      int i=ctm_alsa.bufc;
      for (;i-->0;srcp-=1,dstp-=2) {
        dstp[1]=dstp[0]=*srcp;
      }
    }

    // dump buffer to alsa
    int16_t *samplev=ctm_alsa.buf;
    int samplep=0,samplec=ctm_alsa.bufc;
    while (samplep<samplec) {
      pthread_testcancel();
      int err=snd_pcm_writei(ctm_alsa.alsa,samplev+samplep,samplec-samplep);
      if (err<=0) {
        if ((err=snd_pcm_recover(ctm_alsa.alsa,err,0))<0) {
          fprintf(stderr,"ctm: snd_pcm_writei: %d (%s)\n",err,snd_strerror(err));
          return 0;
        }
        break;
      }
      samplep+=err*ctm_alsa.chanc;
    }
  }
  return 0;
}

/* Init.
 */

int ctm_alsa_init(void (*cb)(int16_t *dst,int dstac)) {
  if (!cb) return -1;
  memset(&ctm_alsa,0,sizeof(ctm_alsa));
  ctm_alsa.cb=cb;

  int rate=44100;
  int chanc=2;

  if (snd_pcm_open(&ctm_alsa.alsa,CTM_ALSA_DEVICE,SND_PCM_STREAM_PLAYBACK,0)<0) return -1;
  if (snd_pcm_hw_params_malloc(&ctm_alsa.hwparams)<0) return -1;
  if (snd_pcm_hw_params_any(ctm_alsa.alsa,ctm_alsa.hwparams)<0) return -1;
  if (snd_pcm_hw_params_set_access(ctm_alsa.alsa,ctm_alsa.hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0) return -1;
  if (snd_pcm_hw_params_set_format(ctm_alsa.alsa,ctm_alsa.hwparams,SND_PCM_FORMAT_S16)<0) return -1;
  if (snd_pcm_hw_params_set_rate_near(ctm_alsa.alsa,ctm_alsa.hwparams,&rate,0)<0) return -1;
  if (snd_pcm_hw_params_set_channels_near(ctm_alsa.alsa,ctm_alsa.hwparams,&chanc)<0) return -1;
  if (snd_pcm_hw_params_set_buffer_size(ctm_alsa.alsa,ctm_alsa.hwparams,CTM_ALSA_BUFFER_SIZE)<0) return -1;
  if (snd_pcm_hw_params(ctm_alsa.alsa,ctm_alsa.hwparams)<0) return -1;

  if (snd_pcm_nonblock(ctm_alsa.alsa,0)<0) return -1;
  if (snd_pcm_prepare(ctm_alsa.alsa)<0) return -1;
  ctm_alsa.rate=rate;
  ctm_alsa.chanc=chanc;
  ctm_alsa.bufc=CTM_ALSA_BUFFER_SIZE/chanc;
  ctm_alsa.cb=cb;
  if (!(ctm_alsa.buf=malloc(sizeof(int16_t)*ctm_alsa.chanc*ctm_alsa.bufc))) return -1;

  { pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&ctm_alsa.iomtx,&mattr)) return -1;
    pthread_mutexattr_destroy(&mattr);
    if (pthread_create(&ctm_alsa.iothd,0,ctm_alsa_iothd,0)) return -1;
  }
  
  return 0;
}

/* Quit.
 */

void ctm_alsa_quit() {

  if (ctm_alsa.iothd) {
    pthread_cancel(ctm_alsa.iothd);
    pthread_join(ctm_alsa.iothd,0);
  }
  pthread_mutex_destroy(&ctm_alsa.iomtx);

  if (ctm_alsa.hwparams) snd_pcm_hw_params_free(ctm_alsa.hwparams);
  if (ctm_alsa.alsa) snd_pcm_close(ctm_alsa.alsa);
  if (ctm_alsa.buf) free(ctm_alsa.buf);

  memset(&ctm_alsa,0,sizeof(ctm_alsa));
}
