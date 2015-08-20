#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

#define CTM_CTHULHU_TREASURE CTM_ITEM_MAGIC_WAND,30

struct ctm_sprite_cthulhu {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int mouthopen;
  int tentanim;
  int tentframe;
  int elevation;
  int delevation;
  int dy;
  int ylo,yhi;
  int firetime;
};

#define SPR ((struct ctm_sprite_cthulhu*)spr)

#define CTM_CTHULHU_FIRE_MIN 60
#define CTM_CTHULHU_FIRE_MAX 180
#define CTM_CTHULHU_FIRE_SPEED 4

/* Delete.
 */

static void _ctm_cthulhu_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_cthulhu_draw(struct ctm_sprite *spr,int addx,int addy) {

  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(4);
  if (!vtxv) return -1;
  if (SPR->invincible) vtxv[0].ta=(SPR->invincible*255)/60;
  else vtxv[0].ta=0x00;
  int i; for (i=0;i<4;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=0xff;
    vtxv[i].tr=vtxv[i].tg=vtxv[i].tb=0x00;
    vtxv[i].ta=vtxv[0].ta;
    vtxv[i].flop=0;
  }

  vtxv[0].x=spr->x+addx;
  vtxv[0].y=spr->y+addy;
  vtxv[0].tile=SPR->mouthopen?0x2f:0x1f;

  vtxv[1].x=spr->x+addx+1;
  vtxv[1].y=spr->y+addy-8;
  vtxv[1].tile=SPR->tentframe?0x1e:0x0e;

  vtxv[2].x=spr->x+addx+1;
  vtxv[2].y=spr->y+addy-12-SPR->elevation/10;
  vtxv[2].tile=SPR->tentframe?0x0e:0x1e;

  vtxv[3].x=spr->x+addx+1;
  vtxv[3].y=spr->y+addy-16-SPR->elevation/5;
  vtxv[3].tile=0x0f;

  return 0;
}

/* Update.
 */

static int _ctm_cthulhu_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

  // First time we update, we must take a few measurements.
  // Can't do this during init, because they depend on position.
  if (!SPR->ylo) {
    int col=spr->x/CTM_TILESIZE;
    int row=spr->y/CTM_TILESIZE;
    uint32_t stopprop=CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS|CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE;
    if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return -1;
    while ((row>0)&&!(ctm_tileprop_interior[ctm_grid.cellv[(row-1)*ctm_grid.colc+col].itile]&stopprop)) row--;
    SPR->ylo=row*CTM_TILESIZE+(CTM_TILESIZE>>1);
    row=spr->y/CTM_TILESIZE;
    while ((row<ctm_grid.rowc)&&!(ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile]&stopprop)) row++;
    SPR->yhi=row*CTM_TILESIZE-(CTM_TILESIZE>>1);
    SPR->firetime=CTM_CTHULHU_FIRE_MIN+rand()%(CTM_CTHULHU_FIRE_MAX-CTM_CTHULHU_FIRE_MIN+1);
  }

  if (SPR->invincible>0) SPR->invincible--;

  // Animate tentacles.
  if (++(SPR->tentanim)>=8) {
    SPR->tentanim=0;
    if (++(SPR->tentframe)>=2) SPR->tentframe=0;
  }

  // Animate spinal compression/expansion.
  SPR->elevation+=SPR->delevation;
  if (SPR->elevation<0) { SPR->elevation=0; SPR->delevation=1; }
  else if (SPR->elevation>29) { SPR->elevation=29; SPR->delevation=-1; }

  // Walk up and down.
  spr->y+=SPR->dy;
  if (spr->y<SPR->ylo) { spr->y=SPR->ylo; if (SPR->dy<0) SPR->dy=-SPR->dy; }
  else if (spr->y>SPR->yhi) { spr->y=SPR->yhi; if (SPR->dy>0) SPR->dy=-SPR->dy; }

  // Time fireballs.
  if (--(SPR->firetime)<=0) {
    if (SPR->mouthopen) {
      SPR->mouthopen=0;
      SPR->firetime=CTM_CTHULHU_FIRE_MIN+rand()%(CTM_CTHULHU_FIRE_MAX-CTM_CTHULHU_FIRE_MIN+1);
    } else {
      CTM_SFX(FIREBALL)
      SPR->mouthopen=1;
      SPR->firetime=30;
      struct ctm_sprite *missile=ctm_sprite_missile_new(spr,0x2e,CTM_CTHULHU_FIRE_SPEED,0);
      if (!missile) return -1;
    }
  }

  return 0;
}

/* Hurt.
 */

static int _ctm_cthulhu_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
  if (SPR->hp-->1) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_HURT,0xff);
    SPR->invincible=60;
  } else if (SPR->hp==0) {
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
    if (!ctm_treasure_new(assailant,CTM_CTHULHU_TREASURE)) return -1;

    // The usual kill-a-monster stuff...
    if (ctm_sprite_create_ghost(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_gore(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
    return ctm_sprite_kill_later(spr);
  } else return ctm_sprite_kill_later(spr);
  return 0;
}

/* Init.
 */

static int _ctm_cthulhu_init(struct ctm_sprite *spr) {
  SPR->hp=5;
  SPR->delevation=1;
  SPR->dy=1;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_cthulhu={
  .name="cthulhu",
  .objlen=sizeof(struct ctm_sprite_cthulhu),
  .soul=1,
  .guts=1,
  .del=_ctm_cthulhu_del,
  .init=_ctm_cthulhu_init,
  .update=_ctm_cthulhu_update,
  .draw=_ctm_cthulhu_draw,
  .hurt=_ctm_cthulhu_hurt,
};
