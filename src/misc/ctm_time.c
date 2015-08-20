#include "ctm.h"
#include <sys/time.h>

/* Time.
 */

int64_t ctm_get_time() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000+tv.tv_usec;
}
