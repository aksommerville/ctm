#include "ctm_test.h"
#include "input/ctm_input.h"
#include "video/ctm_video.h"
#include "display/ctm_display.h"
#include "io/ctm_poll.h"
#include "game/ctm_grid.h"

#include <signal.h>
#ifndef SIGQUIT
  #define SIGQUIT -1
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

int ctm_init() {

  ctm_quit();

  signal(SIGINT,ctm_rcvsig);
  signal(SIGTERM,ctm_rcvsig);
  signal(SIGQUIT,ctm_rcvsig);

  /* Bring subsystems online. */
  if (ctm_video_init(0)<0) return -1;
  if (ctm_poll_init()<0) return -1;
  if (ctm_input_init(1)<0) return -1;
  if (ctm_display_init()<0) return -1;

  if (ctm_grid_init()<0) return -1;
  //if (ctm_display_rebuild_editor()<0) return -1;

  return 0;
}

/* Quit.
 */

void ctm_quit() {
  ctm_grid_quit();
  ctm_input_quit();
  ctm_poll_quit();
  ctm_display_quit();
  ctm_video_quit();
}
