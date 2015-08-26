#include "ctm.h"
#include "ctm_dump_audio.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <SDL/SDL.h>

/* Globals.
 */

#define CTM_DUMP_AUDIO_BUFFER_LIMIT (1024*1024)

static struct {
  int init;
  int fd;
  char *buf;
  int bufp,bufc,bufa;
} ctm_dump_audio={
  0,
  -1,
  0,
  0,0,0,
};

/* Init.
 */
 
int ctm_dump_audio_init(const char *path) {
  if (ctm_dump_audio.init) return -1;
  if (!path) return 0;
  if ((ctm_dump_audio.fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_APPEND,0666))<0) {
    fprintf(stderr,"ctm_dump_audio:ERROR: Failed to open '%s' for writing: %s\n",path,strerror(errno));
    return -1;
  }
  ctm_dump_audio.init=1;
  return 0;
}

/* Quit.
 */
 
void ctm_dump_audio_quit() {

  /* Flush whatever we still have. */
  while (ctm_dump_audio.bufc>0) {
    if (ctm_dump_audio_update()<0) {
      fprintf(stderr,"ctm_dump_audio:WARNING: Final %d bytes of output were not written.\n",ctm_dump_audio.bufc);
      break;
    }
  }

  /* Clean up. */
  if (ctm_dump_audio.fd>=0) close(ctm_dump_audio.fd);
  if (ctm_dump_audio.buf) free(ctm_dump_audio.buf);

  /* Reset. */
  memset(&ctm_dump_audio,0,sizeof(ctm_dump_audio));
  ctm_dump_audio.fd=-1;
}

/* Receive data.
 * This is called from the io callback, so keep it short and sweet!
 */

static int ctm_dump_audio_require(int addc) {
  if (ctm_dump_audio.bufc>INT_MAX-addc) return -1;
  int na=ctm_dump_audio.bufc+addc;
  if (na>CTM_DUMP_AUDIO_BUFFER_LIMIT) return -1;
  if (na>ctm_dump_audio.bufa) {
    int nax=(na+32768)&~32767;
    void *nv=realloc(ctm_dump_audio.buf,nax);
    if (!nv) return -1;
    ctm_dump_audio.buf=nv;
    ctm_dump_audio.bufa=nax;
  }
  if (ctm_dump_audio.bufp+ctm_dump_audio.bufc>ctm_dump_audio.bufa-addc) {
    memmove(ctm_dump_audio.buf,ctm_dump_audio.buf+ctm_dump_audio.bufp,ctm_dump_audio.bufc);
    ctm_dump_audio.bufp=0;
  }
  return 0;
}

void ctm_dump_audio_provide(const void *src,int srcc) {
  if (!ctm_dump_audio.init) return;
  if (srcc<1) return;
  if (ctm_dump_audio_require(srcc)<0) return;
  memcpy(ctm_dump_audio.buf+ctm_dump_audio.bufp+ctm_dump_audio.bufc,src,srcc);
  ctm_dump_audio.bufc+=srcc;
}

/* Update: deliver buffer to disk.
 */
 
int ctm_dump_audio_update() {
  if (!ctm_dump_audio.init) return 0;
  if (ctm_dump_audio.bufc<1) return 0;
  SDL_LockAudio();
  int err=write(ctm_dump_audio.fd,ctm_dump_audio.buf+ctm_dump_audio.bufp,ctm_dump_audio.bufc);
  if (err<0) {
    fprintf(stderr,"ctm_dump_audio:ERROR: Write failed: %s\n",strerror(errno));
  } else if (!err) {
    fprintf(stderr,"ctm_dump_audio:ERROR: Write stalled.\n");
    err=-1;
  } else if (err>ctm_dump_audio.bufc) {
    fprintf(stderr,"ctm_dump_audio:ERROR: Unexpected result from write: %d > %d\n",err,ctm_dump_audio.bufc);
    err=-1;
  } else {
    if (ctm_dump_audio.bufc-=err) ctm_dump_audio.bufp+=err;
    else ctm_dump_audio.bufp=0;
    err=0;
  }
  SDL_UnlockAudio();
  return err;
}
