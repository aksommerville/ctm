#include "ctm.h"
#include "sprite/ctm_sprite.h"

struct ctm_sprite_toast {
  struct ctm_sprite hdr;
  int ttl;
  int toggle;
};

#define SPR ((struct ctm_sprite_toast*)spr)

/* Update.
 */

static int _ctm_toast_update(struct ctm_sprite *spr) {
  if (--(SPR->ttl)<=0) return ctm_sprite_kill_later(spr);
  if (SPR->ttl<30) spr->opacity=(SPR->ttl*0xc0)/30;
  else spr->opacity=0xc0;
  if ((SPR->toggle++)&8) spr->opacity>>=1;
  return 0;
}

/* Type deifnition.
 */

const struct ctm_sprtype ctm_sprtype_toast={
  .name="toast",
  .objlen=sizeof(struct ctm_sprite_toast),
  .update=_ctm_toast_update,
};

/* Public ctor.
 */

struct ctm_sprite *ctm_sprite_toast_new(int x,int y,int interior,uint8_t tile) {
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_toast);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) return 0;
  spr->layer=100;
  spr->x=x;
  spr->y=y;
  spr->tile=tile;
  spr->opacity=0xc0;
  SPR->ttl=120;
  if (spr->interior=interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return 0;
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) return 0;
  }
  return spr;
}
