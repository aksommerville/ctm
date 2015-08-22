#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

#define CTM_ROBOT_TREASURE CTM_ITEM_METAL_BLADE,20

struct ctm_sprite_robot {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int loaded;
  int gunclock;
  int headclock;
  int headx,headdx;
  int bladeclock;
  int bladeframe;
  int dx;
  int dxttl;
  int xlo,xhi;
};

#define SPR ((struct ctm_sprite_robot*)spr)

#define CTM_ROBOT_DXTTL_MIN   10 // Time spent in a horizontal run.
#define CTM_ROBOT_DXTTL_MAX   40
#define CTM_ROBOT_AIM_MIN    100 // Time spent with next missile visible.
#define CTM_ROBOT_AIM_MAX    150
#define CTM_ROBOT_RELOAD_MIN 100 // Time spent after firing, displaying empty chamber.
#define CTM_ROBOT_RELOAD_MAX 150

#define CTM_ROBOT_MISSILE_SPEED ((CTM_TILESIZE*4)/16)
#define CTM_ROBOT_WALK_SPEED_LIMIT ((CTM_TILESIZE*2)/16)

#define CTM_ROBOT_HEADX_LO ((CTM_TILESIZE*-2)/16)
#define CTM_ROBOT_HEADX_HI ((CTM_TILESIZE*3)/16)

/* Delete.
 */

static void _ctm_robot_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_robot_draw(struct ctm_sprite *spr,int addx,int addy) {

  int vtxc=5;
  if (SPR->loaded) vtxc++;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  struct ctm_vertex_sprite
    *vlarm=vtxv+0,
    *vrarm=vtxv+1,
    *vback=vtxv+2,
    *vtits=vtxv+3,
    *vhead=vtxv+4,
    *vammo=0;
  if (SPR->loaded) {
    vammo=vtxv+3;
    vtits=vtxv+4;
    vhead=vtxv+5;
  }

  int i; for (i=0;i<vtxc;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=0xff;
    vtxv[i].tr=vtxv[i].tg=vtxv[i].tb=vtxv[i].ta=0x00;
    vtxv[i].flop=0;
  }
  if (SPR->invincible) {
    vtxv[0].ta=(SPR->invincible*255)/60;
    for (i=1;i<vtxc;i++) vtxv[i].ta=vtxv[0].ta;
  }

  vback->x=vtits->x=spr->x+addx;
  vback->y=vtits->y=spr->y+addy;
  if (vammo) { vammo->x=vback->x; vammo->y=vback->y; }
  vlarm->x=vback->x-(CTM_TILESIZE>>1);
  vrarm->x=vback->x+(CTM_TILESIZE>>1);
  vlarm->y=vrarm->y=vback->y;
  vhead->x=vback->x+SPR->headx;
  vhead->y=vback->y-CTM_TILESIZE;

  vback->tile=0x1b;
  vtits->tile=0x1c;
  vhead->tile=0x0c;
  vlarm->tile=vrarm->tile=SPR->bladeframe?0x1d:0x0d;
  if (vammo) vammo->tile=vlarm->tile;

  return 0;
}

/* Update.
 */

static int _ctm_robot_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

  // First time we update, we must take a few measurements.
  // Can't do this during init, because they depend on position.
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

  // Animate head, sliding back and forth.
  if (++(SPR->headclock)>=3) {
    SPR->headclock=0;
    SPR->headx+=SPR->headdx;
    if (SPR->headx<CTM_ROBOT_HEADX_LO) { SPR->headx=CTM_ROBOT_HEADX_LO; SPR->headdx=1; }
    else if (SPR->headx>CTM_ROBOT_HEADX_HI) { SPR->headx=CTM_ROBOT_HEADX_HI; SPR->headdx=-1; }
  }

  // Animate blades.
  if (++(SPR->bladeclock)>=4) {
    SPR->bladeclock=0;
    if (++(SPR->bladeframe)>=2) SPR->bladeframe=0;
  }

  // Move side to side.
  if (SPR->dxttl>0) {
    SPR->dxttl--;
    spr->x+=SPR->dx;
    if (spr->x<SPR->xlo) { spr->x=SPR->xlo; SPR->dxttl=0; }
    else if (spr->x>SPR->xhi) { spr->x=SPR->xhi; SPR->dxttl=0; }
  } else {
    SPR->dxttl=CTM_ROBOT_DXTTL_MIN+rand()%(CTM_ROBOT_DXTTL_MAX-CTM_ROBOT_DXTTL_MIN+1);
    SPR->dx=rand()%CTM_ROBOT_WALK_SPEED_LIMIT+1;
    if (rand()&1) SPR->dx=-SPR->dx;
  }

  // Missiles.
  if (--(SPR->gunclock)<=0) {
    if (SPR->loaded) {
      CTM_SFX(BLADE)
      SPR->loaded=0;
      SPR->gunclock=CTM_ROBOT_RELOAD_MIN+rand()%(CTM_ROBOT_RELOAD_MAX-CTM_ROBOT_RELOAD_MIN+1);
      struct ctm_sprite *missile=ctm_sprite_missile_new(spr,0x0d,0,CTM_ROBOT_MISSILE_SPEED);
      missile->layer=1;
    } else {
      SPR->loaded=1;
      SPR->gunclock=CTM_ROBOT_AIM_MIN+rand()%(CTM_ROBOT_AIM_MAX-CTM_ROBOT_AIM_MIN+1);
    }
  }

  return 0;
}

/* Hurt.
 */

static int _ctm_robot_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
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
    if (!ctm_treasure_new(assailant,CTM_ROBOT_TREASURE)) return -1;

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

static int _ctm_robot_init(struct ctm_sprite *spr) {
  SPR->hp=5;
  SPR->headdx=1;
  SPR->loaded=1;
  SPR->gunclock=CTM_ROBOT_AIM_MIN+rand()%(CTM_ROBOT_AIM_MAX-CTM_ROBOT_AIM_MIN+1);
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_robot={
  .name="robot",
  .objlen=sizeof(struct ctm_sprite_robot),
  .del=_ctm_robot_del,
  .init=_ctm_robot_init,
  .draw=_ctm_robot_draw,
  .update=_ctm_robot_update,
  .hurt=_ctm_robot_hurt,
};
