#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

#define CTM_BUFFALO_TREASURE CTM_ITEM_BOW,30

struct ctm_sprite_buffalo {
  struct ctm_sprite spr;
  int invincible;
  int hp;
  int extensionclock;
  int extension;
  int dextension;
  int animclock;
  int animframe;
  int dy;
  int ylo,yhi;
};

#define SPR ((struct ctm_sprite_buffalo*)spr)

#define CTM_BUFFALO_EXTENSION_LIMIT ((CTM_TILESIZE*80)/16)
#define CTM_BUFFALO_WAIT_MIN  60
#define CTM_BUFFALO_WAIT_MAX 180
#define CTM_BUFFALO_LINKC_ADD ((CTM_TILESIZE*15)/16)
#define CTM_BUFFALO_LINKC_DIVIDE ((CTM_TILESIZE*8)/16)
#define CTM_BUFFALO_EXTENSION_SPEED_PROJECT ((CTM_TILESIZE*2)/16)
#define CTM_BUFFALO_EXTENSION_SPEED_RETURN ((CTM_TILESIZE*-1)/16)
#define CTM_BUFFALO_HURT_RADIUS ((CTM_TILESIZE*8)/16)
#define CTM_BUFFALO_VERTICAL_SPEED ((CTM_TILESIZE*1)/16)

/* Delete.
 */

static void _ctm_buffalo_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_buffalo_draw(struct ctm_sprite *spr,int addx,int addy) {

  int linkc=0;
  if (SPR->extension) {
    linkc=(CTM_BUFFALO_EXTENSION_LIMIT+CTM_TILESIZE)/(CTM_TILESIZE>>1);
  }
  int vtxc=linkc+2;
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

  int ax=spr->x+addx-(CTM_TILESIZE>>1);
  for (i=0;i<linkc;i++) {
    vtxv[i].x=ax-(i*SPR->extension)/(linkc-1);
    vtxv[i].tile=0x2d;
  }

  vtxv[linkc].x=spr->x+addx;
  switch (SPR->animframe) {
    case 1: vtxv[linkc].tile=0x3c; break;
    case 3: vtxv[linkc].tile=0x3d; break;
    default: vtxv[linkc].tile=0x2c; break;
  }

  vtxv[linkc+1].x=spr->x+addx-CTM_TILESIZE-SPR->extension+CTM_RESIZE(1);
  vtxv[linkc+1].tile=0x2b;
  if (SPR->extension) vtxv[linkc+1].tile+=0x10;

  return 0;
}

/* Test damage collision.
 */

static int _ctm_buffalo_test_damage_collision_1(int sprx,int spry,int x,int y,int w,int h) {
  const int sprw=CTM_TILESIZE;
  const int sprh=CTM_TILESIZE;
  sprx-=sprw>>1;
  spry-=sprh>>1;
  if (sprx+sprw<=x) return 0;
  if (spry+sprh<=y) return 0;
  if (sprx>=x+w) return 0;
  if (spry>=y+h) return 0;
  return 1;
}

static int _ctm_buffalo_test_damage_collision(struct ctm_sprite *spr,int x,int y,int w,int h,struct ctm_sprite *assailant) {
  if (_ctm_buffalo_test_damage_collision_1(spr->x,spr->y,x,y,w,h)) return 1;
  if (_ctm_buffalo_test_damage_collision_1(spr->x-CTM_TILESIZE-SPR->extension+CTM_RESIZE(1),spr->y,x,y,w,h)) return 1;
  return 0;
}

/* Update.
 */

static int _ctm_buffalo_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

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
    // Go to far right edge (normally starts at far left).
    row=spr->y/CTM_TILESIZE;
    while ((col<ctm_grid.colc)&&!(ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile]&stopprop)) col++;
    spr->x=col*CTM_TILESIZE-(CTM_TILESIZE>>1);
  }

  if (SPR->invincible) SPR->invincible--;

  if (++(SPR->animclock)>=8) {
    SPR->animclock=0;
    if (++(SPR->animframe)>=4) SPR->animframe=0;
  }

  if (SPR->dextension) {
    SPR->extension+=SPR->dextension;
    if (SPR->extension>=CTM_BUFFALO_EXTENSION_LIMIT) {
      SPR->extension=CTM_BUFFALO_EXTENSION_LIMIT;
      SPR->dextension=CTM_BUFFALO_EXTENSION_SPEED_RETURN;
    } else if (SPR->extension<0) {
      SPR->extension=0;
      SPR->dextension=0;
    }
  } else if (--(SPR->extensionclock)<=0) {
    CTM_SFX(HOOKSHOT_BEGIN)
    SPR->dextension=CTM_BUFFALO_EXTENSION_SPEED_PROJECT;
    SPR->extensionclock=CTM_BUFFALO_WAIT_MIN+rand()%(CTM_BUFFALO_WAIT_MAX-CTM_BUFFALO_WAIT_MIN+1);
  }

  spr->y+=SPR->dy*CTM_BUFFALO_VERTICAL_SPEED;
  if (spr->y<SPR->ylo) { spr->y=SPR->ylo; if (SPR->dy<0) SPR->dy=-SPR->dy; }
  else if (spr->y>SPR->yhi) { spr->y=SPR->yhi; if (SPR->dy>0) SPR->dy=-SPR->dy; }

  int exx=spr->x-CTM_TILESIZE-SPR->extension;
  int i; for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *player=ctm_group_player.sprv[i];
    if (player->interior!=spr->interior) continue;
    int dx=player->x-exx;
    if (dx<-CTM_BUFFALO_HURT_RADIUS) continue;
    if (dx>CTM_BUFFALO_HURT_RADIUS) continue;
    int dy=player->y-spr->y;
    if (dy<-CTM_BUFFALO_HURT_RADIUS) continue;
    if (dy>CTM_BUFFALO_HURT_RADIUS) continue;
    if (ctm_sprite_hurt(player,spr)<0) return -1;
  }

  return 0;
}

/* Hurt.
 */

static int _ctm_buffalo_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
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
    if (!ctm_treasure_new(assailant,CTM_BUFFALO_TREASURE)) return -1;

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

static int _ctm_buffalo_init(struct ctm_sprite *spr) {
  SPR->hp=5;
  SPR->dy=1;
  SPR->extensionclock=CTM_BUFFALO_WAIT_MIN+rand()%(CTM_BUFFALO_WAIT_MAX-CTM_BUFFALO_WAIT_MIN+1);
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_buffalo={
  .name="buffalo",
  .objlen=sizeof(struct ctm_sprite_buffalo),
  .soul=1,
  .guts=1,
  .del=_ctm_buffalo_del,
  .init=_ctm_buffalo_init,
  .draw=_ctm_buffalo_draw,
  .update=_ctm_buffalo_update,
  .hurt=_ctm_buffalo_hurt,
  .test_damage_collision=_ctm_buffalo_test_damage_collision,
};
