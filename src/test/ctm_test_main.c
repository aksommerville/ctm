#include "ctm_test.h"
#include "ctm_test_contents.h"

// If we're using SDL, <SDL.h> must be included wherever main() is defined.
#if CTM_USE_sdl
  #include <SDL.h>
#endif

/* Main entry point.
 *****************************************************************************/

int main(int argc,char **argv) {
  int testc=sizeof(ctm_testv)/sizeof(struct ctm_test);
  int passc=0,failc=0,i;
  const struct ctm_test *test=ctm_testv;
  for (i=0;i<testc;i++,test++) {
    if (test->fn()<0) {
      failc++;
      printf("FAIL: %s [%s:%d]\n",test->name,test->file,test->line);
    } else {
      passc++;
    }
  }
  if (failc) printf("\x1b[41m                            \x1b[0m\n");
  else if (passc) printf("\x1b[42m                            \x1b[0m\n");
  else printf("\x1b[40m                            \x1b[0m\n");
  printf("%d pass, %d fail\n",passc,failc);
  return 0;
}
