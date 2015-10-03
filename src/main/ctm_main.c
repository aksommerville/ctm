#include "ctm.h"
#include "misc/ctm_time.h"
#include "io/ctm_poll.h"
#include "io/ctm_fs.h"
#include "input/ctm_input.h"
#include "video/ctm_video.h"
#include "display/ctm_display.h"
#include "game/ctm_game.h"
#include "game/ctm_grid.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "game/ctm_geography.h"
#include "audio/ctm_audio.h"
#include <unistd.h>
#include <time.h>

#include <signal.h>
#ifndef SIGQUIT
  #define SIGQUIT -1
#endif

// libSDL needs to take control of main().
// We just include <SDL.h>, and the rest happens via black magic.
#if CTM_USE_sdl
  #include <SDL.h>
#endif

/* Signals.
 */

static volatile int ctm_sigc=0;

static void ctm_rcvsig(int sigid) {
  switch (sigid) {
    case SIGTERM: case SIGQUIT: case SIGINT: {
        if (++ctm_sigc>=3) {
          fprintf(stderr,"ctm: Too many pending signals.\n");
          exit(1);
        }
      } break;
  }
}

/* Init.
 */

static int ctm_init(int fullscreen,int audio) {

  signal(SIGINT,ctm_rcvsig);
  signal(SIGTERM,ctm_rcvsig);
  signal(SIGQUIT,ctm_rcvsig);

  /* Bring subsystems online. */
  if (ctm_video_init(fullscreen)<0) return -1;
  if (ctm_poll_init()<0) return -1;
  if (ctm_input_init(0)<0) return -1;
  if (ctm_display_init()<0) return -1;
  if (audio&&(ctm_audio_init())<0) return -1;

  if (ctm_sprgrp_init()<0) return -1;
  if (ctm_grid_init()<0) return -1;
  if (ctm_game_init()<0) return -1;

  /* Now that everything is initialized, begin setup. */
  if (ctm_game_main_menu()<0) return -1;

  return 0;
}

/* Quit.
 */

static void ctm_quit() {
  ctm_game_quit();
  ctm_grid_quit();
  ctm_sprgrp_quit();
  ctm_audio_quit();
  ctm_input_quit();
  ctm_poll_quit();
  ctm_display_quit();
  ctm_video_quit();
}

/* Update.
 */

static int ctm_update() {
  if (ctm_sigc) return 0;
  if (ctm_audio_update()<0) return -1;
  if (ctm_poll_update()<0) return -1;
  if (ctm_input_update()<0) return -1;
  if (ctm_game_update()<0) return -1;
  if (ctm_display_draw()<0) return -1;
  return 1;
}

/* Evaluate argument.
 */

static int ctm_memcasecmp(const void *a,const void *b,int c) {
  if (a==b) return 0;
  if (c<1) return 0;
  if (!a) return -1;
  if (!b) return 1;
  while (c--) {
    uint8_t cha=*(uint8_t*)a; a=(char*)a+1; if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    uint8_t chb=*(uint8_t*)b; b=(char*)b+1; if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

static int ctm_eval_boolean(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  switch (srcc) {
    case 0: return 0;
    case 1: switch (src[0]) {
        case '0': return 0;
        case '1': return 1;
        case 'y': case 'Y': return 1;
        case 'n': case 'N': return 0;
      } break;
    case 2: {
        if (!ctm_memcasecmp(src,"no",2)) return 0;
        if (!ctm_memcasecmp(src,"on",2)) return 1;
      } break;
    case 3: {
        if (!ctm_memcasecmp(src,"yes",3)) return 1;
        if (!ctm_memcasecmp(src,"off",3)) return 0;
      } break;
    case 4: {
        if (!ctm_memcasecmp(src,"true",4)) return 1;
      } break;
    case 5: {
        if (!ctm_memcasecmp(src,"false",5)) return 0;
      } break;
    case 6: {
        if (!ctm_memcasecmp(src,"enable",6)) return 1;
      } break;
    case 7: {
        if (!ctm_memcasecmp(src,"disable",7)) return 0;
      } break;
  }
  return 1;
}

/* --help
 */

static void ctm_print_help(const char *name) {
  if (!name||!name[0]) name="ctm";
  printf("Usage: %s [OPTIONS]\n",name);
  printf(
    "OPTIONS:\n"
    "  --help             Print this message.\n"
    "  --fullscreen=0|1   Start in fullscreen mode, if switchable.\n"
    "  --audio=0|1        Enable or disable audio.\n"
    "ENVIRONMENT:\n"
    "  CTM_ROOT=PATH      Path to directory containing all data files.\n"
  );
}

/* Main.
 */

int main(int argc,char **argv) {
  int err=0,framec=0;
  int64_t starttime=0,endtime=0;

  #if CTM_TEST_DISABLE_VIDEO
    printf("!!! Video is intentionally disabled in this build. DO NOT RELEASE. !!!\n");
  #endif

  // argv
  int fullscreen=1;
  int audio=1;
  if (argc>=1) {
    ctm_set_argv0(argv[0]);
    int i; for (i=1;i<argc;i++) {
      if (!memcmp(argv[i],"--fullscreen=",13)) fullscreen=ctm_eval_boolean(argv[i]+13,-1);
      else if (!memcmp(argv[i],"--audio=",8)) audio=ctm_eval_boolean(argv[i]+8,-1);
      else if (!strcmp(argv[i],"--help")) { ctm_print_help(argv[0]); return 0; }
      else {
        fprintf(stderr,"%s: Unexpected argument '%s'.\n",argv[0],argv[i]);
        return 1;
      }
    }
  }

  srand(time(0));
  if ((err=ctm_init(fullscreen,audio))<0) goto _done_;

  starttime=ctm_get_time();

  while ((err=ctm_update())>0) framec++;

  endtime=ctm_get_time();
  if ((endtime>starttime)&&(framec>0)) {
    double elapsed=(endtime-starttime)/1000000.0;
    printf("ctm: Average frame rate %.2f Hz.\n",framec/elapsed);
  }

 _done_:
  ctm_quit();
  if (err<0) {
    fprintf(stderr,"ctm: Fatal error.\n");
    return 1;
  } else {
    fprintf(stderr,"ctm: Normal exit.\n");
    return 0;
  }
}
