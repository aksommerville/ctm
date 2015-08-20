#include "ctm.h"
#include "sprite/ctm_sprite.h"

#define CTM_GORE_FRAME_DELAY 5

struct ctm_sprite_gore {
  struct ctm_sprite hdr;
  int clock;
};

#define SPR ((struct ctm_sprite_gore*)spr)

/* Init.
 */

static int _ctm_gore_init(struct ctm_sprite *spr) {

  spr->layer=00;
  spr->tile=0x04;
  spr->primaryrgb=0x808080;
  spr->opacity=0xff;
  spr->tintrgba=0x00000000;
  
  return 0;
}

/* Update.
 */

static int _ctm_gore_update(struct ctm_sprite *spr) {
  if (++(SPR->clock)>=CTM_GORE_FRAME_DELAY) {
    SPR->clock=0;
    if (++(spr->tile)>=0x09) return ctm_sprite_kill(spr);
  }
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_gore={
  .name="gore",
  .objlen=sizeof(struct ctm_sprite_gore),
  .soul=0,
  .guts=0,
  .init=_ctm_gore_init,
  .update=_ctm_gore_update,
};
