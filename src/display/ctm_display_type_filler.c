#include "ctm.h"
#include "ctm_display.h"
#include "video/ctm_video_internal.h"

/* Object type.
 */

struct ctm_display_filler {
  struct ctm_display hdr;
};

#define DISPLAY ((struct ctm_display_filler*)display)

/* Type definition.
 */

const struct ctm_display_type ctm_display_type_filler={
  .name="filler",
  .objlen=sizeof(struct ctm_display_filler),
};
