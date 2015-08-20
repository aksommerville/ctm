#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include <math.h>

struct ctm_sprite_flames {
  struct ctm_sprite hdr;
  struct ctm_sprite *owner;
  double t;
  int animclock,animframe;
};

#define SPR ((struct ctm_sprite_flames*)spr)

#define CTM_FLAMES_DISTANCE (CTM_TILESIZE*1.5) // pixels
#define CTM_FLAMES_SPEED   0.10 // radians

/* Delete.
 */

static void _ctm_flames_del(struct ctm_sprite *spr) {
  ctm_sprite_del(SPR->owner);
}

/* Update.
 */

static void ctm_flames_reposition(struct ctm_sprite *spr) {
  spr->x=SPR->owner->x+sin(SPR->t)*CTM_FLAMES_DISTANCE;
  spr->y=SPR->owner->y+cos(SPR->t)*CTM_FLAMES_DISTANCE;
}

static int _ctm_flames_update(struct ctm_sprite *spr) {
  if (!SPR->owner) return ctm_sprite_kill_later(spr);
  if (SPR->owner->interior!=spr->interior) return ctm_sprite_kill_later(spr);

  if (++(SPR->animclock)>=5) {
    if (++(SPR->animframe)>=4) SPR->animframe=0;
    switch (SPR->animframe) {
      case 0: spr->tile=0x93; break;
      case 1: spr->tile=0x94; break;
      case 2: spr->tile=0x95; break;
      case 3: spr->tile=0x94; break;
    }
  }

  SPR->t+=CTM_FLAMES_SPEED;
  if (SPR->t>M_PI*2.0) SPR->t-=M_PI*2.0;
  ctm_flames_reposition(spr);

  int l=spr->x-6,r=spr->x+6,t=spr->y-6,b=spr->y+6;
  int i; for (i=0;i<ctm_group_fragile.sprc;i++) {
    struct ctm_sprite *qspr=ctm_group_fragile.sprv[i];
    if (qspr==SPR->owner) continue;
    if (qspr->interior!=spr->interior) continue;
    if (qspr->x<l) continue;
    if (qspr->x>r) continue;
    if (qspr->y<t) continue;
    if (qspr->y>b) continue;
    if (ctm_sprite_hurt(qspr,SPR->owner)<0) return -1;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_flames={
  .name="flames",
  .objlen=sizeof(struct ctm_sprite_flames),
  .del=_ctm_flames_del,
  .update=_ctm_flames_update,
};

/* Public access.
 */
 
struct ctm_sprite *ctm_flames_new(struct ctm_sprite *owner,double offset) {
  if (!owner) return 0;
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_flames);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) { ctm_sprite_kill(spr); return 0; }
  spr->x=owner->x;
  spr->y=owner->y;
  spr->layer=10;
  spr->tile=0x93;
  if (spr->interior=owner->interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) { ctm_sprite_kill(spr); return 0; }
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) { ctm_sprite_kill(spr); return 0; }
  }
  if (ctm_sprite_ref(owner)<0) { ctm_sprite_kill(spr); return 0; }
  SPR->owner=owner;
  SPR->t=offset;
  ctm_flames_reposition(spr);
  return spr;
}

int ctm_flames_drop(struct ctm_sprite *owner) {
  int i; for (i=0;i<ctm_group_update.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_update.sprv[i];
    if (spr->type!=&ctm_sprtype_flames) continue;
    if (SPR->owner!=owner) continue;
    if (ctm_sprite_kill_later(spr)<0) return -1;
  }
  return 0;
}
