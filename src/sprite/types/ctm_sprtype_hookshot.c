#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "game/ctm_grid.h"
#include "audio/ctm_audio.h"

int ctm_player_move(struct ctm_sprite *spr,int dx,int dy,int leap);

struct ctm_sprite_hookshot {
  struct ctm_sprite hdr;
  struct ctm_sprgrp *links;
  struct ctm_sprite *owner;
  struct ctm_sprite *hookee;
  int dx,dy;
  int ttl;
  int phase;
};

#define CTM_HOOKSHOT_TTL 60
#define CTM_HOOKSHOT_LINK_SPACING (CTM_TILESIZE>>1)
#define CTM_HOOKSHOT_RADIUS ((CTM_TILESIZE*4)/16)

#define CTM_HOOKSHOT_EXTEND_SPEED ((CTM_TILESIZE*3)/16)
#define CTM_HOOKSHOT_RETURN_SPEED ((CTM_TILESIZE*4)/16)
#define CTM_HOOKSHOT_TRANSPORT_SPEED ((CTM_TILESIZE*3)/16)

#define CTM_HOOKSHOT_PHASE_EXTEND    0 // Going out.
#define CTM_HOOKSHOT_PHASE_RETURN    1 // Coming back with a sprite hooked.
#define CTM_HOOKSHOT_PHASE_TRANSPORT 2 // Retracting with jaws locked, moving owner.

#define SPR ((struct ctm_sprite_hookshot*)spr)

/* Delete.
 */

static void _ctm_hookshot_del(struct ctm_sprite *spr) {
  ctm_sprgrp_kill(SPR->links);
  ctm_sprgrp_del(SPR->links);
  ctm_sprite_del(SPR->owner);
  if (SPR->hookee) {
    if (SPR->hookee->type->grab) SPR->hookee->type->grab(SPR->hookee,0);
    ctm_sprite_del(SPR->hookee);
  }
}

/* Initialize.
 */

static int _ctm_hookshot_init(struct ctm_sprite *spr) {
  if (!(SPR->links=ctm_sprgrp_new(0))) return -1;
  SPR->phase=CTM_HOOKSHOT_PHASE_EXTEND;
  return 0;
}

/* Rebuild chain.
 */

static int ctm_hookshot_rebuild(struct ctm_sprite *spr) {

  // Owner required.
  if (!SPR->owner) return ctm_sprite_kill_later(spr);

  // Decide how many links in the chain.
  int linkc;
  if (SPR->dx<0) linkc=SPR->owner->x-spr->x;
  else if (SPR->dx>0) linkc=spr->x-SPR->owner->x;
  else if (SPR->dy<0) linkc=SPR->owner->y-spr->y;
  else linkc=spr->y-SPR->owner->y;
  linkc/=CTM_HOOKSHOT_LINK_SPACING;
  if (linkc<0) linkc=0;

  // Get the correct count of links in link group. Don't worry about setting them up just yet.
  while (SPR->links->sprc>linkc) {
    struct ctm_sprite *link=SPR->links->sprv[SPR->links->sprc-1];
    ctm_sprgrp_remove_sprite(SPR->links,link);
    ctm_sprite_kill_later(link);
  }
  while (SPR->links->sprc<linkc) {
    struct ctm_sprite *link=ctm_sprite_alloc(&ctm_sprtype_dummy);
    if (!link) return -1;
    if (ctm_sprite_add_group(link,&ctm_group_keepalive)<0) { ctm_sprite_del(link); return -1; }
    ctm_sprite_del(link);
    link->layer=spr->layer-1;
    link->x=spr->x;
    link->y=spr->y;
    if (link->interior=spr->interior) {
      if (ctm_sprite_add_group(link,&ctm_group_interior)<0) { ctm_sprite_kill(link); return -1; }
    } else {
      if (ctm_sprite_add_group(link,&ctm_group_exterior)<0) { ctm_sprite_kill(link); return -1; }
    }
    if (ctm_sprite_add_group(link,SPR->links)<0) { ctm_sprite_kill(link); return -1; }
  }

  // Set up link sprites. Everything is mutable except interior/exterior.
  int x=spr->x,y=spr->y;
  int i; for (i=0;i<SPR->links->sprc;i++) {
    struct ctm_sprite *link=SPR->links->sprv[i];
    x-=SPR->dx*CTM_HOOKSHOT_LINK_SPACING;
    y-=SPR->dy*CTM_HOOKSHOT_LINK_SPACING;
    link->layer=spr->layer-1;
    link->x=x;
    link->y=y;
    link->tile=0x0b;
  }

  return 0;
}

/* Look for something to latch onto.
 */

static int ctm_hookshot_try_latch(struct ctm_sprite *spr) {

  // If we hit a POROUS tile, switch to TRANSPORT mode.
  uint32_t prop=ctm_grid_tileprop_for_pixel(spr->x,spr->y,spr->interior);
  if (prop&CTM_TILEPROP_POROUS) {
    CTM_SFX(HOOKSHOT_GRAB)
    spr->tile+=0x10;
    SPR->phase=CTM_HOOKSHOT_PHASE_TRANSPORT;
    return 1;
  }

  // If we hit a SOLID tile, return emptyhanded.
  if (prop&CTM_TILEPROP_SOLID) {
    CTM_SFX(HOOKSHOT_SMACK)
    SPR->phase=CTM_HOOKSHOT_PHASE_RETURN;
    return 1;
  }

  // Certain sprites can be picked up, switch to RETURN mode.
  int l=spr->x-CTM_HOOKSHOT_RADIUS;
  int r=spr->x+CTM_HOOKSHOT_RADIUS;
  int t=spr->y-CTM_HOOKSHOT_RADIUS;
  int b=spr->y+CTM_HOOKSHOT_RADIUS;
  int i; for (i=0;i<ctm_group_hookable.sprc;i++) {
    struct ctm_sprite *qspr=ctm_group_hookable.sprv[i];
    if (qspr==SPR->owner) continue;
    if (qspr->interior!=spr->interior) continue;
    if (qspr->x<l) continue;
    if (qspr->x>r) continue;
    if (qspr->y<t) continue;
    if (qspr->y>b) continue;
    if (qspr->type->grab&&(qspr->type->grab(qspr,1)<0)) return -1;
    if (ctm_sprite_ref(qspr)<0) return -1;
    CTM_SFX(HOOKSHOT_GRAB)
    SPR->hookee=qspr;
    spr->tile+=0x10;
    SPR->phase=CTM_HOOKSHOT_PHASE_RETURN;
    return 1;
  }
  
  return 0;
}

/* Update.
 */

static int _ctm_hookshot_update(struct ctm_sprite *spr) {
  switch (SPR->phase) {

    case CTM_HOOKSHOT_PHASE_EXTEND: {
        // Give up after a certain time.
        if (--(SPR->ttl)<=0) return ctm_sprite_kill_later(spr);
        spr->x+=CTM_HOOKSHOT_EXTEND_SPEED*SPR->dx;
        spr->y+=CTM_HOOKSHOT_EXTEND_SPEED*SPR->dy;
        if (ctm_hookshot_try_latch(spr)<0) return -1;
      } break;

    case CTM_HOOKSHOT_PHASE_RETURN: {
        if (!SPR->owner) return ctm_sprite_kill_later(spr);
        if (SPR->dx<0) { if ((spr->x+=CTM_HOOKSHOT_RETURN_SPEED)>=SPR->owner->x) return ctm_sprite_kill_later(spr); }
        else if (SPR->dx>0) { if ((spr->x-=CTM_HOOKSHOT_RETURN_SPEED)<=SPR->owner->x) return ctm_sprite_kill_later(spr); }
        else if (SPR->dy<0) { if ((spr->y+=CTM_HOOKSHOT_RETURN_SPEED)>=SPR->owner->y) return ctm_sprite_kill_later(spr); }
        else if (SPR->dy>0) { if ((spr->y-=CTM_HOOKSHOT_RETURN_SPEED)<=SPR->owner->y) return ctm_sprite_kill_later(spr); }
        else return ctm_sprite_kill_later(spr);
        if (SPR->hookee) {
          SPR->hookee->x=spr->x;
          SPR->hookee->y=spr->y;
        }
      } break;

    case CTM_HOOKSHOT_PHASE_TRANSPORT: {
        if (!SPR->owner||(SPR->owner->type!=&ctm_sprtype_player)) return ctm_sprite_kill_later(spr);
        if (!SPR->dx&&!SPR->dy) return ctm_sprite_kill_later(spr);
        int err=ctm_player_move(SPR->owner,SPR->dx*CTM_HOOKSHOT_TRANSPORT_SPEED,SPR->dy*CTM_HOOKSHOT_TRANSPORT_SPEED,1);
        if (err<0) return -1;
        if (!err) return ctm_sprite_kill_later(spr);
      } break;

    default: return ctm_sprite_kill_later(spr);
  }
  if (ctm_hookshot_rebuild(spr)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_hookshot={
  .name="hookshot",
  .objlen=sizeof(struct ctm_sprite_hookshot),
  .del=_ctm_hookshot_del,
  .init=_ctm_hookshot_init,
  .update=_ctm_hookshot_update,
};

/* Public ctor.
 */

struct ctm_sprite *ctm_hookshot_new(struct ctm_sprite *owner,int dx,int dy) {
  if (!owner) return 0;
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_hookshot);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_ref(owner)<0) { ctm_sprite_kill(spr); return 0; }
  SPR->owner=owner;
  spr->x=owner->x+CTM_TILESIZE*dx;
  spr->y=owner->y+CTM_TILESIZE*dy;
  if (spr->interior=owner->interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) { ctm_sprite_kill(spr); return 0; }
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) { ctm_sprite_kill(spr); return 0; }
  }

  SPR->ttl=CTM_HOOKSHOT_TTL;
  if (dy>0) {
    SPR->dy=1;
    spr->tile=0x63;
  } else if (dy<0) {
    SPR->dy=-1;
    spr->tile=0x64;
  } else if (dx<0) {
    SPR->dx=-1;
    spr->tile=0x65;
  } else if (dx>0) {
    SPR->dx=1;
    spr->tile=0x65;
    spr->flop=1;
  } else {
    ctm_sprite_kill(spr);
    return 0;
  }
  
  return spr;
}
