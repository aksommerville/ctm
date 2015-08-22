#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_geography.h"
#include "game/ctm_grid.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

#define CTM_PINEAPPLE_TREASURE CTM_ITEM_AUTOBOX,1

struct ctm_sprite_pineapple {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int dstx,dsty;
  int xweight,yweight;
  int weight;
  int dx,dy;
  int xlo,xhi,ylo,yhi;
  int animclock;
  int animframe;
  int waitclock; // Counts down while in shell.
  int prewait; // Out of shell, haven't moved yet.
  int postwait; // Out of shell, done moving.
};

#define SPR ((struct ctm_sprite_pineapple*)spr)

#define CTM_PINEAPPLE_WAIT_MIN   120
#define CTM_PINEAPPLE_WAIT_MAX   300
#define CTM_PINEAPPLE_PREWAIT     20
#define CTM_PINEAPPLE_POSTWAIT    20

#define CTM_PINEAPPLE_SPEED ((CTM_TILESIZE*1)/16)

#define CTM_PINEAPPLE_EXTRUSIONS_OFFSET_X ((CTM_TILESIZE*0)/16)
#define CTM_PINEAPPLE_EXTRUSIONS_OFFSET_Y ((CTM_TILESIZE*5)/16)

/* Delete.
 */

static void _ctm_pineapple_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_pineapple_draw(struct ctm_sprite *spr,int addx,int addy) {

  int vtxc=SPR->waitclock?1:2;
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

  vtxv[0].tile=0x49;
  vtxv[0].x=spr->x+addx;
  vtxv[0].y=spr->y+addy;

  if (!SPR->waitclock) {
    vtxv[1].tile=SPR->animframe?0x4a:0x4b;
    vtxv[1].x=spr->x+addx+CTM_PINEAPPLE_EXTRUSIONS_OFFSET_X;
    vtxv[1].y=spr->y+addy+CTM_PINEAPPLE_EXTRUSIONS_OFFSET_Y;
  }

  return 0;
}

/* Choose new target.
 */

static int ctm_pineapple_target(struct ctm_sprite *spr) {
  SPR->dstx=SPR->xlo+rand()%(SPR->xhi-SPR->xlo);
  SPR->dsty=SPR->ylo+rand()%(SPR->yhi-SPR->ylo);
  if (SPR->dstx<spr->x) {
    SPR->xweight=spr->x-SPR->dstx;
    SPR->dx=-1;
  } else if (SPR->dstx>spr->x) {
    SPR->xweight=SPR->dstx-spr->x;
    SPR->dx=1;
  } else {
    SPR->xweight=0;
    SPR->dx=0;
  }
  if (SPR->dsty<spr->y) {
    SPR->yweight=SPR->dsty-spr->y;
    SPR->dy=-1;
  } else if (SPR->dsty>spr->y) {
    SPR->yweight=spr->y-SPR->dsty;
    SPR->dy=1;
  } else {
    SPR->yweight=0;
    SPR->dy=0;
  }
  SPR->weight=SPR->xweight+SPR->yweight;
  return 0;
}

/* Update.
 */

static int _ctm_pineapple_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

  if (!SPR->xlo) {
    int col=spr->x/CTM_TILESIZE;
    int row=spr->y/CTM_TILESIZE;
    uint32_t stopprop=CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS|CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE;
    if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return -1;
    while ((row>0)&&!(ctm_tileprop_interior[ctm_grid.cellv[(row-1)*ctm_grid.colc+col].itile]&stopprop)) row--;
    SPR->ylo=row*CTM_TILESIZE+(CTM_TILESIZE>>1);
    row=spr->y/CTM_TILESIZE;
    while ((row<ctm_grid.rowc)&&!(ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile]&stopprop)) row++;
    SPR->yhi=row*CTM_TILESIZE-(CTM_TILESIZE>>1);
    row=spr->y/CTM_TILESIZE;
    while ((col>0)&&!(ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col-1].itile]&stopprop)) col--;
    SPR->xlo=col*CTM_TILESIZE+(CTM_TILESIZE>>1);
    col=spr->x/CTM_TILESIZE;
    while ((col<ctm_grid.colc)&&!(ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile]&stopprop)) col++;
    SPR->xhi=col*CTM_TILESIZE-(CTM_TILESIZE>>1);
    if ((SPR->xlo>=SPR->xhi)||(SPR->ylo>=SPR->yhi)) return -1;
  }
  
  if (SPR->invincible>0) SPR->invincible--;

  if (SPR->prewait>0) {
    SPR->prewait--;
  } else if (SPR->postwait>0) {
    if (!--(SPR->postwait)) {
      SPR->waitclock=CTM_PINEAPPLE_WAIT_MIN+rand()%(CTM_PINEAPPLE_WAIT_MAX-CTM_PINEAPPLE_WAIT_MIN+1);
      if (ctm_sprite_remove_group(spr,&ctm_group_fragile)<0) return -1;
    }
  } else if (SPR->waitclock>1) {
    SPR->waitclock--;
  } else if (SPR->waitclock==1) {
    SPR->waitclock=0;
    SPR->prewait=CTM_PINEAPPLE_PREWAIT;
    if (ctm_pineapple_target(spr)<0) return -1;
    if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) return -1;
  } else {
    if (++(SPR->animclock)>=7) {
      SPR->animclock=0;
      if (++(SPR->animframe)>=2) SPR->animframe=0;
    }
    int i; for (i=0;i<CTM_PINEAPPLE_SPEED;i++) {
      if ((SPR->weight>=SPR->xweight)&&(SPR->dstx!=spr->x)) { SPR->weight+=SPR->yweight; spr->x+=SPR->dx; }
      else if ((SPR->weight<=SPR->yweight)&&(SPR->dsty!=spr->y)) { SPR->weight+=SPR->xweight; spr->y+=SPR->dy; }
      else {
        if (SPR->dstx!=spr->x) spr->x+=SPR->dx;
        if (SPR->dsty!=spr->y) spr->y+=SPR->dy;
        SPR->weight+=SPR->xweight+SPR->yweight;
      }
    }
    if ((SPR->dstx==spr->x)&&(SPR->dsty==spr->y)) {
      SPR->postwait=CTM_PINEAPPLE_POSTWAIT;
    }
  }

  return 0;
}

/* Hurt.
 */

static int _ctm_pineapple_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
  SPR->invincible=60;
  if (SPR->hp>1) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_HURT,0xff);
    SPR->hp--;
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
    if (!ctm_treasure_new(assailant,CTM_PINEAPPLE_TREASURE)) return -1;

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

static int _ctm_pineapple_init(struct ctm_sprite *spr) {
  SPR->hp=5;
  SPR->dx=1;
  SPR->waitclock=1;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_pineapple={
  .name="pineapple",
  .objlen=sizeof(struct ctm_sprite_pineapple),
  .soul=1,
  .guts=0, // eww gross
  .del=_ctm_pineapple_del,
  .init=_ctm_pineapple_init,
  .draw=_ctm_pineapple_draw,
  .update=_ctm_pineapple_update,
  .hurt=_ctm_pineapple_hurt,
};
