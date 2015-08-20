#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "video/ctm_video.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "audio/ctm_audio.h"
#include <math.h>

#define CTM_SANTA_TREASURE CTM_ITEM_HOOKSHOT,1

struct ctm_sprite_santa {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int bounce_phase;
  int basey;
  int dx;
  int xlo,xhi;
  int giftx,gifty; // Gift position, sample, relative to sprite.
  double giftt,giftr; // Gift position, polar.
  double giftdt,giftdr; // Gift delta.
};

#define SPR ((struct ctm_sprite_santa*)spr)

// Half of a parabolic curve, precalculated for efficiency:
static const int8_t ctm_bounce_offsets[30]={0,0,1,1,1,2,2,3,3,3,3,4,4,4,4,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6};

/* Delete.
 */

static void _ctm_santa_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_santa_draw(struct ctm_sprite *spr,int addx,int addy) {
  int linkc=6;
  int vtxc=linkc+3;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;

  int i; for (i=0;i<vtxc;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=0xff;
    vtxv[i].tr=vtxv[i].tg=vtxv[i].tb=0x00;
    vtxv[i].ta=0x00;
    vtxv[i].flop=0;
  }

  int giftp=linkc;
  int bodyp=linkc+1;
  int headp=linkc+2;

  vtxv[bodyp].x=spr->x+addx;
  vtxv[bodyp].y=spr->y+addy;
  vtxv[bodyp].tile=0x19;

  vtxv[headp].x=spr->x+addx;
  vtxv[headp].y=spr->y+addy-10;
  vtxv[headp].tile=0x09;

  // Head bounces up and down for no reason at all.
  if (SPR->bounce_phase<30) {
    vtxv[headp].y-=ctm_bounce_offsets[SPR->bounce_phase];
  } else {
    vtxv[headp].y-=ctm_bounce_offsets[59-SPR->bounce_phase];
  }
  if ((SPR->bounce_phase<3)||(SPR->bounce_phase>=57)) vtxv[bodyp].tile=0x1a;

  vtxv[giftp].x=spr->x+addx+SPR->giftx;
  vtxv[giftp].y=spr->y+addy+SPR->gifty;
  vtxv[giftp].tile=0x0a;

  for (i=0;i<linkc;i++) {
    vtxv[i].x=spr->x+addx+(SPR->giftx*i)/linkc;
    vtxv[i].y=spr->y+addy+(SPR->gifty*i)/linkc;
    vtxv[i].tile=0x0b;
  }
  
  if (SPR->invincible) {
    vtxv[headp].ta=vtxv[bodyp].ta=(SPR->invincible*255)/60;
  }
  
  return 0;
}

/* Update.
 */

static int _ctm_santa_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

  // First time we update, we must take a few measurements.
  // Can't do this during init, because they depend on position.
  if (!SPR->basey) {
    SPR->basey=spr->y;
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

  // Vertical position is based on bounce counter.
  // This bouncing is doubled for the head, but that is purely cosmetic.
  if (++(SPR->bounce_phase)>=60) {
    SPR->bounce_phase=0;
  }
  if (SPR->bounce_phase<30) {
    spr->y=SPR->basey-ctm_bounce_offsets[SPR->bounce_phase];
  } else {
    spr->y=SPR->basey-ctm_bounce_offsets[59-SPR->bounce_phase];
  }

  // Move gift and sample its new position.
  SPR->giftt+=SPR->giftdt; if (SPR->giftt>M_PI*2.0) SPR->giftt-=M_PI*2.0;
  SPR->giftr+=SPR->giftdr;
  if ((SPR->giftr>60.0)&&(SPR->giftdr>0.0)) SPR->giftdr=-SPR->giftdr;
  else if ((SPR->giftr<20.0)&&(SPR->giftdr<0.0)) SPR->giftdr=-SPR->giftdr;
  SPR->giftx=cos(SPR->giftt)*SPR->giftr;
  SPR->gifty=sin(SPR->giftt)*SPR->giftr;

  // Horizontal position just runs back and forth. Limits are premeasured.
  spr->x+=SPR->dx;
  if (spr->x<SPR->xlo) { spr->x=SPR->xlo; if (SPR->dx<0) SPR->dx=-SPR->dx; }
  else if (spr->x>SPR->xhi) { spr->x=SPR->xhi; if (SPR->dx>0) SPR->dx=-SPR->dx; }

  // Check for collisions with player against gift -- our central body is managed externally.
  int i; for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *player=ctm_group_player.sprv[i];
    if (player->interior!=spr->interior) continue;
    int dx=player->x-(spr->x+SPR->giftx);
    if (dx<-8) continue;
    if (dx>8) continue;
    int dy=player->y-(spr->y+SPR->gifty);
    if (dy<-8) continue;
    if (dy>8) continue;
    if (ctm_sprite_hurt(player,spr)<0) return -1;
  }
  
  return 0;
}

/* Hurt.
 */

static int _ctm_santa_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
  SPR->invincible=60;
  if (SPR->hp>1) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_HURT,0xff);
    SPR->hp--;
    SPR->giftdt+=0.02;
  } else {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_KILLED,0xff);

    // If we don't have a player assailant, eg killed by projectile, credit any nearby player.
    // We don't do this for minor monsters, but for minibosses I imagine players would feel ripped off if a projectile kill doesn't count.
    // And anyway, the logic for not crediting on projectile kills was that the voters can't see it happen.
    // They can't see this happen either way!
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
    if (!ctm_treasure_new(assailant,CTM_SANTA_TREASURE)) return -1;

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

static int _ctm_santa_init(struct ctm_sprite *spr) {
  SPR->hp=5;
  SPR->dx=1;
  SPR->giftt=0.0;
  SPR->giftr=40.0;
  SPR->giftdt=0.07;
  SPR->giftdr=0.20;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_santa={
  .name="santa",
  .objlen=sizeof(struct ctm_sprite_santa),
  .soul=1,
  .guts=1,
  .del=_ctm_santa_del,
  .init=_ctm_santa_init,
  .draw=_ctm_santa_draw,
  .update=_ctm_santa_update,
  .hurt=_ctm_santa_hurt,
};
