#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

#define CTM_VAMPIRE_TREASURE CTM_ITEM_LENS_OF_TRUTH,1

struct ctm_sprite_vampire {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int xlo,xhi,ylo,yhi;
  int alpha;
  int dalpha;
  int teletime;
};

#define SPR ((struct ctm_sprite_vampire*)spr)

// These times are a little misleading; they are the timeframe when you can hurt the vampire.
// He is also visible for about 5/6 s fading in.
#define CTM_VAMPIRE_TELETIME_MIN  10
#define CTM_VAMPIRE_TELETIME_MAX  40

#define CTM_VAMPIRE_HEAD_Y_ADJUST ((CTM_TILESIZE*13)/16)

/* Delete.
 */

static void _ctm_vampire_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_vampire_draw(struct ctm_sprite *spr,int addx,int addy) {

  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(2);
  if (!vtxv) return -1;
  if (SPR->invincible) vtxv[0].ta=vtxv[1].ta=(SPR->invincible*255)/60;
  else vtxv[0].ta=vtxv[1].ta=0x00;
  int i; for (i=0;i<2;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=SPR->alpha;
    vtxv[i].tr=vtxv[i].tg=vtxv[i].tb=0x00;
    vtxv[i].flop=spr->flop;
  }

  vtxv[0].x=spr->x+addx;
  vtxv[0].y=spr->y+addy;
  vtxv[0].tile=0x39;

  vtxv[1].x=spr->x+addx;
  vtxv[1].y=spr->y+addy-CTM_VAMPIRE_HEAD_Y_ADJUST;
  vtxv[1].tile=0x29;
  if (SPR->invincible) vtxv[1].tile++;

  return 0;
}

/* Update.
 */

static int _ctm_vampire_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

  // First time we update, we must take a few measurements.
  // Can't do this during init, because they depend on position.
  if (!SPR->ylo) {
    int col=spr->x/CTM_TILESIZE;
    int row=spr->y/CTM_TILESIZE;
    uint32_t stopprop=CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS|CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE;
    if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return -1;
    while ((row>0)&&!(ctm_tileprop_interior[ctm_grid.cellv[(row-1)*ctm_grid.colc+col].itile]&stopprop)) row--;
    row++; // Our projectile appears about 1 tile above the sprite's center, which could be in a wall if we use the full range.
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

  // Teleport.
  if (SPR->dalpha) {
    SPR->alpha+=SPR->dalpha;
    if (SPR->alpha<=0) { // disappeared...
      SPR->alpha=0;
      SPR->dalpha=5;
      spr->x=SPR->xlo+rand()%(SPR->xhi-SPR->xlo);
      spr->y=SPR->ylo+rand()%(SPR->yhi-SPR->ylo);
      if (spr->x>=(SPR->xlo+SPR->xhi)>>1) spr->flop=1; else spr->flop=0;
      
    } else if (SPR->alpha>=255) { // reappeared...
      if (ctm_sprite_add_group(spr,&ctm_group_hazard)<0) return -1;
      if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) return -1;
      SPR->alpha=255;
      SPR->dalpha=0;
      SPR->teletime=CTM_VAMPIRE_TELETIME_MIN+rand()%(CTM_VAMPIRE_TELETIME_MAX-CTM_VAMPIRE_TELETIME_MIN+1);
      struct ctm_sprite *fireball=ctm_sprite_fireball_new(spr,0x3a);
      if (!fireball) return -1;
      fireball->y-=CTM_VAMPIRE_HEAD_Y_ADJUST;
    }
    
  } else if (--(SPR->teletime)<=0) { // ready to disappear...
    if (ctm_sprite_remove_group(spr,&ctm_group_hazard)<0) return -1;
    if (ctm_sprite_remove_group(spr,&ctm_group_fragile)<0) return -1;
    SPR->dalpha=-5;
  }

  return 0;
}

/* Hurt.
 */

static int _ctm_vampire_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
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
    if (!ctm_treasure_new(assailant,CTM_VAMPIRE_TREASURE)) return -1;

    // The usual kill-a-monster stuff...
    if (ctm_sprite_create_ghost(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_gore(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
    return ctm_sprite_kill_later(spr);
  }
  return 0;
}

/* Init.
 */

static int _ctm_vampire_init(struct ctm_sprite *spr) {
  SPR->hp=5;
  SPR->dalpha=-1;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_vampire={
  .name="vampire",
  .objlen=sizeof(struct ctm_sprite_vampire),
  .soul=1,
  .guts=1,
  .del=_ctm_vampire_del,
  .init=_ctm_vampire_init,
  .update=_ctm_vampire_update,
  .draw=_ctm_vampire_draw,
  .hurt=_ctm_vampire_hurt,
};
