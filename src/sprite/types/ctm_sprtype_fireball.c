#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "game/ctm_grid.h"

struct ctm_sprite_fireball {
  struct ctm_sprite hdr;
  int xweight,yweight;
  int weight;
  int dx,dy;
  struct ctm_sprite *owner;
  int speed;
  int ttl;
};

#define SPR ((struct ctm_sprite_fireball*)spr)

/* Delete.
 */

static void _ctm_fireball_del(struct ctm_sprite *spr) {
  ctm_sprite_del(SPR->owner);
}

/* Update.
 */

static int _ctm_fireball_update(struct ctm_sprite *spr) {

  // Setting up the path delays until the first update, in case the caller moves us right after construction.
  if (!SPR->dx&&!SPR->dy) {
    struct ctm_sprite *player=0;
    int distance=INT_MAX;
    int i; for (i=0;i<ctm_group_player.sprc;i++) {
      struct ctm_sprite *qspr=ctm_group_player.sprv[i];
      int dx=qspr->x-spr->x; if (dx<0) dx=-dx;
      int dy=qspr->y-spr->y; if (dy<0) dy=-dy;
      if (dx+dy<=distance) { distance=dx+dy; player=qspr; }
    }
    if (!player) return ctm_sprite_kill_later(spr);
    if (player->x>spr->x) {
      SPR->xweight=player->x-spr->x;
      SPR->dx=1;
    } else {
      SPR->xweight=spr->x-player->x;
      SPR->dx=-1;
    }
    if (player->y>spr->y) {
      SPR->yweight=spr->y-player->y;
      SPR->dy=1;
    } else {
      SPR->yweight=player->y-spr->y;
      SPR->dy=-1;
    }
    SPR->weight=SPR->xweight+SPR->yweight;
  }
  
  if (--(SPR->ttl)<=0) return ctm_sprite_kill_later(spr);
  if (SPR->ttl<20) spr->opacity=(SPR->ttl*255)/20;

  int i; for (i=0;i<SPR->speed;i++) {
    if (SPR->weight>=SPR->xweight) { SPR->weight+=SPR->yweight; spr->x+=SPR->dx; }
    else if (SPR->weight<=SPR->yweight) { SPR->weight+=SPR->xweight; spr->y+=SPR->dy; }
    else {
      SPR->weight+=SPR->xweight+SPR->yweight;
      spr->x+=SPR->dx;
      spr->y+=SPR->dy;
    }
  }

  uint32_t prop=ctm_grid_tileprop_for_pixel(spr->x,spr->y,spr->interior);
  if (prop&(CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS)) return ctm_sprite_kill_later(spr);
  
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_fireball={
  .name="fireball",
  .objlen=sizeof(struct ctm_sprite_fireball),
  .del=_ctm_fireball_del,
  .update=_ctm_fireball_update,
};

/* Public ctor.
 */

struct ctm_sprite *ctm_sprite_fireball_new(struct ctm_sprite *owner,uint8_t tile) {
  if (!owner) return 0;
  
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_fireball);
  if (!spr) return 0;
  SPR->ttl=240;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_hazard)<0) return 0;
  if (ctm_sprite_ref(owner)<0) return 0;
  SPR->owner=owner;
  spr->x=owner->x;
  spr->y=owner->y;
  spr->flop=owner->flop;
  spr->layer=owner->layer+1;
  if (spr->interior=owner->interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return 0;
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return 0;
  }
  spr->tile=tile;
  SPR->speed=1;
  
  return spr;
}
