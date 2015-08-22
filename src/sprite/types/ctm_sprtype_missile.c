#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "ctm_sprtype_player.h"
#include "ctm_sprtype_voter.h"
#include "game/ctm_grid.h"

struct ctm_sprite_missile {
  struct ctm_sprite hdr;
  int dx,dy;
  struct ctm_sprite *owner;
  int defunct;
  int action;
  int ttl;
};

#define SPR ((struct ctm_sprite_missile*)spr)

#define CTM_MISSILE_ACTION_DAMAGE    0
#define CTM_MISSILE_ACTION_COIN      1
#define CTM_MISSILE_RADIUS ((CTM_TILESIZE*8)/16)

static void _ctm_missile_del(struct ctm_sprite *spr) {
  ctm_sprite_del(SPR->owner);
}

/* Update.
 */

static int _ctm_missile_update(struct ctm_sprite *spr) {

  if (SPR->defunct) return ctm_sprite_kill_later(spr);
  if (SPR->ttl) {
    if (!--(SPR->ttl)) return ctm_sprite_kill_later(spr);
    if (SPR->ttl<20) spr->opacity=(SPR->ttl*255)/20;
  }

  if (!SPR->dx&&!SPR->dy) return 0;
  spr->x+=SPR->dx;
  spr->y+=SPR->dy;

  uint32_t prop=ctm_grid_tileprop_for_pixel(spr->x,spr->y,spr->interior);
  if (prop&(CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS)) return ctm_sprite_kill_later(spr);

  struct ctm_bounds bounds={spr->x-CTM_MISSILE_RADIUS,spr->x+CTM_MISSILE_RADIUS,spr->y-CTM_MISSILE_RADIUS,spr->y+CTM_MISSILE_RADIUS};
  int i; for (i=0;i<ctm_group_fragile.sprc;i++) {
    struct ctm_sprite *qspr=ctm_group_fragile.sprv[i];
    if (qspr==SPR->owner) continue;
    if (qspr->interior!=spr->interior) continue;
    if (qspr->x<bounds.l) continue;
    if (qspr->x>bounds.r) continue;
    if (qspr->y<bounds.t) continue;
    if (qspr->y>bounds.b) continue;
    switch (SPR->action) {
      case CTM_MISSILE_ACTION_COIN: {
          if (qspr->type==&ctm_sprtype_player) {
            if (((struct ctm_sprite_player*)qspr)->hp<1) continue;
            if (ctm_player_add_inventory(qspr,CTM_ITEM_COIN,1)<0) return -1;
          } else if (qspr->type==&ctm_sprtype_voter) {
            if (SPR->owner&&(SPR->owner->type==&ctm_sprtype_player)) {
              if (ctm_voter_favor(qspr,((struct ctm_sprite_player*)SPR->owner)->party,10)<0) return -1;
            } // ...but even if it doesn't buy a vote, dude will keep it.
          }
        } break;
      case CTM_MISSILE_ACTION_DAMAGE:
      default: if (ctm_sprite_hurt(qspr,SPR->owner)<0) return -1; break;
    }
    if (spr->tile==0x0d) {
      // metal blade cuts through any amount of flesh
    } else {
      SPR->defunct=1;
      return ctm_sprite_kill_later(spr);
    }
  }
  
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_missile={
  .name="missile",
  .objlen=sizeof(struct ctm_sprite_missile),
  .update=_ctm_missile_update,
};

/* Public constructor.
 */

struct ctm_sprite *ctm_sprite_missile_new(struct ctm_sprite *owner,uint8_t tile,int dx,int dy) {
  if (!owner) return 0;
  if (!dx&&!dy) return 0;
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_missile);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  if (ctm_sprite_ref(owner)<0) { ctm_sprite_kill(spr); return 0; }
  SPR->owner=owner;
  SPR->dx=dx;
  SPR->dy=dy;
  spr->x=owner->x;
  spr->y=owner->y;
  spr->tile=tile;
  SPR->action=CTM_MISSILE_ACTION_DAMAGE;
  if (spr->interior=owner->interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) { ctm_sprite_kill(spr); return 0; }
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) { ctm_sprite_kill(spr); return 0; }
  }
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) { ctm_sprite_kill(spr); return 0; }
  return spr;
}

int ctm_missile_setup_coin(struct ctm_sprite *spr) {
  if (!spr||(spr->type!=&ctm_sprtype_missile)) return -1;
  SPR->action=CTM_MISSILE_ACTION_COIN;
  return 0;
}

int ctm_missile_add_ttl(struct ctm_sprite *spr,int framec) {
  if (!spr||(spr->type!=&ctm_sprtype_missile)) return -1;
  SPR->ttl=framec;
  return 0;
}
