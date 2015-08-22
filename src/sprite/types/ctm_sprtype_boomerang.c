#include "ctm.h"
#include "game/ctm_grid.h"
#include "sprite/ctm_sprite.h"

struct ctm_sprite_boomerang {
  struct ctm_sprite hdr;
  int dx,dy;
  struct ctm_sprite *owner;
  int animclock,animframe;
  int counter;
  int returning;
  int grabpoison;
  struct ctm_sprite *content;
};

#define SPR ((struct ctm_sprite_boomerang*)spr)

#define CTM_BOOMERANG_FRAME_TIME 5
#define CTM_BOOMERANG_SPEED ((CTM_TILESIZE*3)/16)
#define CTM_BOOMERANG_COUNTER_LIMIT 40
#define CTM_BOOMERANG_RADIUS ((CTM_TILESIZE*6)/16)
#define CTM_BOOMERANG_GRABPOISON 8

/* Delete.
 */

static void _ctm_boomerang_del(struct ctm_sprite *spr) {
  ctm_sprite_del(SPR->owner);
  if (SPR->content) {
    if (SPR->content->type->grab) SPR->content->type->grab(SPR->content,0);
    ctm_sprite_del(SPR->content);
  }
}

/* Update.
 */

static int _ctm_boomerang_update(struct ctm_sprite *spr) {

  if (++(SPR->animclock)>=CTM_BOOMERANG_FRAME_TIME) {
    SPR->animclock=0;
    if (SPR->returning) {
      if (--(SPR->animframe)<0) SPR->animframe=7;
    } else {
      if (++(SPR->animframe)>=8) SPR->animframe=0;
    }
    switch (SPR->animframe) {
      case 0: spr->tile=0x89; break;
      case 1: spr->tile=0x8a; break;
      case 2: spr->tile=0x8b; break;
      case 3: spr->tile=0x8c; break;
      case 4: spr->tile=0x99; break;
      case 5: spr->tile=0x9a; break;
      case 6: spr->tile=0x9b; break;
      case 7: spr->tile=0x9c; break;
    }
  }

  if (SPR->returning) {
    if (!SPR->owner) return ctm_sprite_kill_later(spr);
    int dx=SPR->owner->x-spr->x,dy=SPR->owner->y-spr->y;
    if (dx<-CTM_BOOMERANG_SPEED) dx=-CTM_BOOMERANG_SPEED; else if (dx>CTM_BOOMERANG_SPEED) dx=CTM_BOOMERANG_SPEED;
    if (dy<-CTM_BOOMERANG_SPEED) dy=-CTM_BOOMERANG_SPEED; else if (dy>CTM_BOOMERANG_SPEED) dy=CTM_BOOMERANG_SPEED;
    spr->x+=dx;
    spr->y+=dy;
    if ((spr->x==SPR->owner->x)&&(spr->y==SPR->owner->y)) return ctm_sprite_kill_later(spr);
  } else {
    spr->x+=CTM_BOOMERANG_SPEED*SPR->dx;
    spr->y+=CTM_BOOMERANG_SPEED*SPR->dy;
    if (++(SPR->counter)>=CTM_BOOMERANG_COUNTER_LIMIT) SPR->returning=1;
  }

  if (!SPR->content) {
    int l=spr->x-CTM_BOOMERANG_RADIUS;
    int r=spr->x+CTM_BOOMERANG_RADIUS;
    int t=spr->y-CTM_BOOMERANG_RADIUS;
    int b=spr->y+CTM_BOOMERANG_RADIUS;
    int i; 

    if (SPR->grabpoison>0) {
      SPR->grabpoison--;
    } else for (i=0;i<ctm_group_boomerangable.sprc;i++) {
      struct ctm_sprite *qspr=ctm_group_boomerangable.sprv[i];
      if (qspr->interior!=spr->interior) continue;
      if (qspr==SPR->owner) continue;
      if (qspr->x<l) continue;
      if (qspr->x>r) continue;
      if (qspr->y<t) continue;
      if (qspr->y>b) continue;
      if (qspr->type->grab&&(qspr->type->grab(qspr,1)<0)) return -1;
      if (ctm_sprite_ref(qspr)<0) return -1;
      SPR->content=qspr;
      SPR->returning=1;
      break;
    }

    // If we didn't pick anything up, we act as a weapon.
    if (!SPR->content) {
      for (i=0;i<ctm_group_fragile.sprc;i++) {
        struct ctm_sprite *qspr=ctm_group_fragile.sprv[i];
        if (qspr->interior!=spr->interior) continue;
        if (qspr==SPR->owner) continue;
        if (qspr->x<l) continue;
        if (qspr->x>r) continue;
        if (qspr->y<t) continue;
        if (qspr->y>b) continue;
        if (ctm_sprite_hurt(qspr,SPR->owner)<0) return -1;
        SPR->returning=1;
        SPR->grabpoison=CTM_BOOMERANG_GRABPOISON;
      }
    }
  } else {
    SPR->content->x=spr->x;
    SPR->content->y=spr->y;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_boomerang={
  .name="boomerang",
  .objlen=sizeof(struct ctm_sprite_boomerang),
  .del=_ctm_boomerang_del,
  .update=_ctm_boomerang_update,
};

/* Constructor.
 */

struct ctm_sprite *ctm_boomerang_new(struct ctm_sprite *owner,int dx,int dy) {
  if (!owner||(!dx&&!dy)||(dx<-1)||(dx>1)||(dy<-1)||(dy>1)) return 0;
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_boomerang);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);

  spr->tile=0x89;
  spr->layer=owner->layer+1;
  
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_ref(owner)<0) { ctm_sprite_kill(spr); return 0; }
  SPR->owner=owner;
  spr->x=owner->x;
  spr->y=owner->y;
  if (spr->interior=owner->interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) { ctm_sprite_kill(spr); return 0; }
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) { ctm_sprite_kill(spr); return 0; }
  }

  SPR->dx=dx;
  SPR->dy=dy;
  
  return spr;
}
