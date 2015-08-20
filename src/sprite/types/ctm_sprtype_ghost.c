#include "ctm.h"
#include "sprite/ctm_sprite.h"

struct ctm_sprite_ghost {
  struct ctm_sprite hdr;
};

#define SPR ((struct ctm_sprite_ghost*)spr)

/* Init.
 */

static int _ctm_ghost_init(struct ctm_sprite *spr) {

  spr->layer=10;
  spr->tile=0x15;
  spr->primaryrgb=0x808080;
  spr->opacity=0xff;
  spr->tintrgba=0x00000000;
  
  return 0;
}

/* Update.
 */

static int _ctm_ghost_update(struct ctm_sprite *spr) {
  if (spr->opacity<=3) return ctm_sprite_kill(spr);
  spr->opacity-=3;
  spr->y--;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_ghost={
  .name="ghost",
  .objlen=sizeof(struct ctm_sprite_ghost),

  .soul=0,
  .guts=0,
  
  .init=_ctm_ghost_init,
  .update=_ctm_ghost_update,
};
