#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_geography.h"
#include "game/ctm_grid.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

#define CTM_CRAB_TREASURE CTM_ITEM_BOOMERANG,1

struct ctm_sprite_crab {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int dx;
  int xlo,xhi;
  int animclock;
  int animframe;
  int fireclock;
};

#define SPR ((struct ctm_sprite_crab*)spr)

#define CTM_CRAB_FIRE_MIN    60
#define CTM_CRAB_FIRE_MAX   180
#define CTM_CRAB_FIRE_SPEED   ((CTM_TILESIZE*3)/16)
#define CTM_CRAB_WALK_SPEED   ((CTM_TILESIZE*1)/16)

/* Delete.
 */

static void _ctm_crab_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_crab_draw(struct ctm_sprite *spr,int addx,int addy) {

  int vtxc=3;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  if (SPR->invincible) vtxv[0].ta=(SPR->invincible*255)/60; else vtxv[0].ta=0;
  int i; for (i=0;i<vtxc;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=0xff;
    vtxv[i].tr=vtxv[i].tg=vtxv[i].tb=0x00;
    vtxv[i].ta=vtxv[0].ta;
    vtxv[i].flop=0;
    vtxv[i].y=spr->y+addy;
  }

  vtxv[0].tile=0x3f;
  vtxv[0].x=spr->x+addx;
  vtxv[0].y=spr->y+addy;

  vtxv[1].tile=0x3e;
  vtxv[1].x=spr->x+addx-CTM_TILESIZE;
  vtxv[1].y=spr->y+addy;

  vtxv[2].tile=0x3e;
  vtxv[2].x=spr->x+addx+CTM_TILESIZE;
  vtxv[2].y=spr->y+addy;
  vtxv[2].flop=1;

  if (SPR->animframe) { vtxv[1].tile+=0x10; vtxv[2].tile+=0x10; }

  return 0;
}

/* Update.
 */

static int _ctm_crab_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

  if (!SPR->xlo) {
    int col=spr->x/CTM_TILESIZE;
    int row=spr->y/CTM_TILESIZE;
    uint32_t stopprop=CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS|CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE;
    if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return -1;
    while ((col>0)&&!(ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col-1].itile]&stopprop)) col--;
    SPR->xlo=col*CTM_TILESIZE+(CTM_TILESIZE>>1);
    col=spr->x/CTM_TILESIZE;
    while ((col<ctm_grid.colc)&&!(ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile]&stopprop)) col++;
    SPR->xhi=col*CTM_TILESIZE-(CTM_TILESIZE>>1);
  }
  
  if (SPR->invincible>0) SPR->invincible--;
  
  spr->x+=SPR->dx*CTM_CRAB_WALK_SPEED;
  if (spr->x<SPR->xlo) { spr->x=SPR->xlo; if (SPR->dx<0) SPR->dx=-SPR->dx; }
  else if (spr->x>SPR->xhi) { spr->x=SPR->xhi; if (SPR->dx>0) SPR->dx=-SPR->dx; }

  if (++(SPR->animclock)>=7) {
    SPR->animclock=0;
    if (++(SPR->animframe)>=2) SPR->animframe=0;
  }

  if (--(SPR->fireclock)<=0) {
    CTM_SFX(FIREBALL)
    SPR->fireclock=CTM_CRAB_FIRE_MIN+rand()%(CTM_CRAB_FIRE_MAX-CTM_CRAB_FIRE_MIN+1);
    struct ctm_sprite *missile=ctm_sprite_missile_new(spr,0x4f,0,CTM_CRAB_FIRE_SPEED);
    if (!missile) return -1;
  }

  return 0;
}

/* Hurt.
 */

static int _ctm_crab_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
  SPR->invincible=60;
  if (SPR->hp>1) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_HURT,0xff);
    SPR->hp--;
    SPR->invincible=60;
  } else {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_KILLED,0xff);

    if (!assailant||(assailant->type!=&ctm_sprtype_player)) {
      int havedistance=INT_MAX;
      int i; for (i=0;i<ctm_group_player.sprc;i++) {
        struct ctm_sprite *player=ctm_group_player.sprv[i];
        if (player->interior!=spr->interior) continue;
        int dx=player->x-spr->x;
        int dy=player->y-spr->y;
        int distance=((dx<0)?-dx:dx)+((dy<0)?-dy:dy);
        if (distance<havedistance) { havedistance=distance; assailant=player; }
      }
    }

    if (assailant&&(assailant->type==&ctm_sprtype_player)) {
      ((struct ctm_sprite_player*)assailant)->bosskills++;
    }

    // Create treasure.
    if (!ctm_treasure_new(assailant,CTM_CRAB_TREASURE)) return -1;

    // The usual kill-a-monster stuff...
    if (ctm_sprite_create_ghost(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_gore(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
    return ctm_sprite_kill_later(spr);
  }
  return 0;
}

/* Initialize.
 */

static int _ctm_crab_init(struct ctm_sprite *spr) {
  SPR->hp=5;
  SPR->dx=1;
  SPR->fireclock=CTM_CRAB_FIRE_MIN+rand()%(CTM_CRAB_FIRE_MAX-CTM_CRAB_FIRE_MIN+1);
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_crab={
  .name="crab",
  .objlen=sizeof(struct ctm_sprite_crab),
  .soul=1,
  .guts=1,
  .del=_ctm_crab_del,
  .init=_ctm_crab_init,
  .draw=_ctm_crab_draw,
  .update=_ctm_crab_update,
  .hurt=_ctm_crab_hurt,
};
