#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include <math.h>

struct ctm_sprite_fireworks {
  struct ctm_sprite hdr;
  int xweight,yweight;
  int weight;
  int dx,dy;
  int ttl;
  int speed;
};

#define SPR ((struct ctm_sprite_fireworks*)spr)

/* Update.
 */

static int _ctm_fireworks_update(struct ctm_sprite *spr) {

  if (--(SPR->ttl)<=0) return ctm_sprite_kill_later(spr);
  if (SPR->ttl<20) spr->opacity=(SPR->ttl*255)/20;

  int i; for (i=0;i<SPR->speed;i++) {
    if (SPR->weight>=SPR->xweight) { spr->x+=SPR->dx; SPR->weight+=SPR->yweight<<1; }
    else if (SPR->weight<=SPR->yweight) { spr->y+=SPR->dy; SPR->weight+=SPR->xweight<<1; }
    else {
      spr->x+=SPR->dx;
      spr->y+=SPR->dy;
      SPR->weight+=(SPR->xweight+SPR->yweight)<<1;
    }
  }
  
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_fireworks={
  .name="fireworks",
  .objlen=sizeof(struct ctm_sprite_fireworks),
  .soul=0,
  .guts=0,
  .update=_ctm_fireworks_update,
};

/* Public ctor.
 */

static int ctm_fireworks_setup(struct ctm_sprite *spr,int dx,int dy) {
  SPR->ttl=40;
  SPR->speed=2;
  if (dx<0) { SPR->xweight=-dx; SPR->dx=-1; } else if (dx>0) { SPR->xweight=dx; SPR->dx=1; }
  if (dy<0) { SPR->yweight=dy; SPR->dy=-1; } else if (dy>0) { SPR->yweight=-dy; SPR->dy=1; }
  SPR->weight=(SPR->xweight+SPR->yweight)>>1;
  return 0;
}

int ctm_sprite_create_fireworks(int x,int y,int interior) {
  const int spritec=20;
  const double dt=(M_PI*2.0)/spritec;
  double t=0.0;
  int rgb=0xff0000;
  int i; for (i=0;i<spritec;i++,t+=dt) {
    struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_fireworks);
    if (!spr) return -1;
    if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return -1; }
    ctm_sprite_del(spr);
    if (ctm_sprite_add_group(spr,&ctm_group_update)<0) return -1;
    spr->x=x;
    spr->y=y;
    if (spr->interior=interior) {
      if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return -1;
    } else {
      if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) return -1;
    }
    spr->layer=30;
    spr->tile=0x14;
    switch (spr->primaryrgb=rgb) {
      case 0xff0000: rgb=0xffffff; break;
      case 0xffffff: rgb=0x0000ff; break;
      case 0x0000ff: rgb=0xff0000; break;
    }
    ctm_fireworks_setup(spr,cos(t)*100.0,sin(t)*100.0);
  }
  return 0;
}
