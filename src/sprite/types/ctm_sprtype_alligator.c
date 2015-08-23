#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

#define CTM_ALLIGATOR_TREASURE CTM_ITEM_PIZZA_FART,1

struct ctm_sprite_alligator {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int phase;
  int phasettl;
  int animclock;
  int animframe;
  int xlo,xhi,ylo,yhi;
  int dx,dy;
};

#define SPR ((struct ctm_sprite_alligator*)spr)

#define CTM_ALLIGATOR_PHASE_GNASH        1 // Standing at the wall, walking up and down.
#define CTM_ALLIGATOR_PHASE_CHARGE       2 // Running horizontally across the room.
#define CTM_ALLIGATOR_PHASE_CONDENSE     3 // Showing our expensive special effects, transforming into ball.
#define CTM_ALLIGATOR_PHASE_BOUNCE       4 // Bouncing diagonally in ball form.
#define CTM_ALLIGATOR_PHASE_EXPAND       5 // Return to true form from ball.

#define CTM_ALLIGATOR_GNASH_MIN    40
#define CTM_ALLIGATOR_GNASH_MAX   120
#define CTM_ALLIGATOR_BOUNCE_MIN  120
#define CTM_ALLIGATOR_BOUNCE_MAX  240
#define CTM_ALLIGATOR_GNASH_SPEED   ((CTM_TILESIZE*1)/16)
#define CTM_ALLIGATOR_CHARGE_SPEED  ((CTM_TILESIZE*2)/16)
#define CTM_ALLIGATOR_BOUNCE_SPEED  ((CTM_TILESIZE*3)/16)

/* Delete.
 */

static void _ctm_alligator_del(struct ctm_sprite *spr) {
}

/* Draw.
 */

static int _ctm_alligator_draw(struct ctm_sprite *spr,int addx,int addy) {

  int vtxc;
  switch (SPR->phase) {
    case CTM_ALLIGATOR_PHASE_GNASH: vtxc=2; break;
    case CTM_ALLIGATOR_PHASE_CHARGE: vtxc=2; break;
    case CTM_ALLIGATOR_PHASE_CONDENSE: vtxc=2; break;
    case CTM_ALLIGATOR_PHASE_BOUNCE: vtxc=1; break;
    case CTM_ALLIGATOR_PHASE_EXPAND: vtxc=2; break;
    default: return -1;
  }
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  if (SPR->invincible) vtxv[0].ta=(SPR->invincible*255)/60;
  else vtxv[0].ta=0;
  int i; for (i=0;i<vtxc;i++) {
    vtxv[i].x=spr->x+addx;
    vtxv[i].y=spr->y+addy;
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=0xff;
    vtxv[i].tr=vtxv[i].tg=vtxv[i].tb=0x00;
    vtxv[i].ta=vtxv[0].ta;
    vtxv[i].flop=spr->flop;
  }

  switch (SPR->phase) {
    case CTM_ALLIGATOR_PHASE_GNASH:
    case CTM_ALLIGATOR_PHASE_CHARGE: {
        if (spr->flop) vtxv[0].x-=CTM_TILESIZE; else vtxv[0].x+=CTM_TILESIZE;
        vtxv[0].tile=0x6a;
        vtxv[1].tile=0x69;
        if (SPR->animframe) { vtxv[0].tile+=0x10; vtxv[1].tile+=0x10; }
      } break;
    case CTM_ALLIGATOR_PHASE_CONDENSE: {
        if (spr->flop) vtxv[0].x-=CTM_TILESIZE; else vtxv[0].x+=CTM_TILESIZE;
        switch (SPR->animframe) {
          case 0: vtxv[0].tile=0x6c; vtxv[1].tile=0x6b; break;
          case 1: vtxv[0].tile=0x7c; vtxv[1].tile=0x7b; break;
          case 2: vtxv[0].tile=0x6e; vtxv[1].tile=0x6d; break;
          case 3: vtxv[0].tile=0x7e; vtxv[1].tile=0x7d; break;
        }
      } break;
    case CTM_ALLIGATOR_PHASE_BOUNCE: {
        vtxv[0].tile=0x5c+SPR->animframe;
      } break;
    case CTM_ALLIGATOR_PHASE_EXPAND: {
        if (spr->flop) vtxv[0].x-=CTM_TILESIZE; else vtxv[0].x+=CTM_TILESIZE;
        switch (SPR->animframe) {
          case 3: vtxv[0].tile=0x6c; vtxv[1].tile=0x6b; break;
          case 2: vtxv[0].tile=0x7c; vtxv[1].tile=0x7b; break;
          case 1: vtxv[0].tile=0x6e; vtxv[1].tile=0x6d; break;
          case 0: vtxv[0].tile=0x7e; vtxv[1].tile=0x7d; break;
        }
      } break;
  }
  
  return 0;
}

/* Test damage or hazard collision.
 */

static int _ctm_alligator_test_damage_collision(struct ctm_sprite *spr,int x,int y,int w,int h,struct ctm_sprite *assailant) {

  // Start with our basic square.
  // In the double-wide configurations, spr->(x,y) is always the head.
  int sprx=spr->x-(CTM_TILESIZE>>1);
  int spry=spr->y-(CTM_TILESIZE>>1);
  int sprw=CTM_TILESIZE;
  int sprh=CTM_TILESIZE;

  // Expand one tile horizontally, depending on state.
  switch (SPR->phase) {
    case CTM_ALLIGATOR_PHASE_GNASH:
    case CTM_ALLIGATOR_PHASE_CHARGE: {
        if (spr->flop) { // Facing right
          sprx-=CTM_TILESIZE;
        }
        sprw+=CTM_TILESIZE;
      } break;
  }

  // Compare to weapon.
  if (sprx+sprw<=x) return 0;
  if (spry+sprh<=y) return 0;
  if (sprx>=x+w) return 0;
  if (spry>=y+h) return 0;
  return 1;
}

/* Update.
 */

static int _ctm_alligator_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;

  if (!SPR->ylo) {
    spr->x+=CTM_TILESIZE;
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

  switch (SPR->phase) {

    case CTM_ALLIGATOR_PHASE_GNASH: {
        if (++(SPR->animclock)>=8) {
          SPR->animclock=0;
          if (++(SPR->animframe)>=2) SPR->animframe=0;
        }
        if (--(SPR->phasettl)<=0) {
          SPR->phase=CTM_ALLIGATOR_PHASE_CHARGE;
        } else {
          spr->y+=SPR->dy*CTM_ALLIGATOR_GNASH_SPEED;
          if (spr->y<SPR->ylo) { spr->y=SPR->ylo; if (SPR->dy<0) SPR->dy=-SPR->dy; }
          else if (spr->y>SPR->yhi) { spr->y=SPR->yhi; if (SPR->dy>0) SPR->dy=-SPR->dy; }
        }
      } break;

    case CTM_ALLIGATOR_PHASE_CHARGE: {
        if (++(SPR->animclock)>=8) {
          SPR->animclock=0;
          if (++(SPR->animframe)>=2) SPR->animframe=0;
        }
        if (spr->flop) {
          spr->x+=CTM_ALLIGATOR_CHARGE_SPEED;
          if (spr->x>=SPR->xhi) {
            spr->x=SPR->xhi;
            SPR->phase=CTM_ALLIGATOR_PHASE_CONDENSE;
            SPR->animclock=SPR->animframe=0;
          }
        } else {
          spr->x-=CTM_ALLIGATOR_CHARGE_SPEED;
          if (spr->x<=SPR->xlo) {
            spr->x=SPR->xlo;
            SPR->phase=CTM_ALLIGATOR_PHASE_CONDENSE;
            SPR->animclock=SPR->animframe=0;
          }
        }
      } break;

    case CTM_ALLIGATOR_PHASE_CONDENSE: {
        if (++(SPR->animclock)>=10) {
          SPR->animclock=0;
          if (++(SPR->animframe)>=4) {
            SPR->animframe=0;
            SPR->phase=CTM_ALLIGATOR_PHASE_BOUNCE;
            if (spr->flop) spr->x-=CTM_TILESIZE; else spr->x+=CTM_TILESIZE; // (x,y) is the jaws, but this animation looks like it condenses into body.
            if (!SPR->dx) SPR->dx=(rand()&1)?1:-1;
            if (!SPR->dy) SPR->dy=(rand()&1)?1:-1;
            SPR->phasettl=CTM_ALLIGATOR_BOUNCE_MIN+rand()%(CTM_ALLIGATOR_BOUNCE_MAX-CTM_ALLIGATOR_BOUNCE_MIN+1);
          }
        }
      } break;

    case CTM_ALLIGATOR_PHASE_BOUNCE: {
        if (++(SPR->animclock)>=5) {
          SPR->animclock=0;
          if (++(SPR->animframe)>=4) SPR->animframe=0;
        }
        if (SPR->phasettl>0) SPR->phasettl--;
        spr->x+=SPR->dx*CTM_ALLIGATOR_BOUNCE_SPEED; 
        if (spr->x<SPR->xlo) { spr->x=SPR->xlo; if (SPR->dx<0) SPR->dx=-SPR->dx; }
        else if (spr->x>SPR->xhi) { spr->x=SPR->xhi; if (SPR->dx>0) SPR->dx=-SPR->dx; }
        spr->y+=SPR->dy*CTM_ALLIGATOR_BOUNCE_SPEED;
        if (spr->y<SPR->ylo) { spr->y=SPR->ylo; if (SPR->dy<0) SPR->dy=-SPR->dy; }
        else if (spr->y>SPR->yhi) { spr->y=SPR->yhi; if (SPR->dy>0) SPR->dy=-SPR->dy; }
        if (!SPR->phasettl&&((spr->x==SPR->xlo)||(spr->x==SPR->xhi))) {
          SPR->phase=CTM_ALLIGATOR_PHASE_EXPAND;
          SPR->animframe=SPR->animclock=0;
          if (spr->x==SPR->xlo) {
            spr->flop=1; 
            spr->x+=CTM_TILESIZE;
          } else {
            spr->flop=0;
            spr->x-=CTM_TILESIZE;
          }
        }
      } break;

    case CTM_ALLIGATOR_PHASE_EXPAND: {
        if (++(SPR->animclock)>=10) {
          SPR->animclock=0;
          if (++(SPR->animframe)>=4) {
            SPR->animframe=0;
            SPR->phase=CTM_ALLIGATOR_PHASE_GNASH;
            SPR->phasettl=CTM_ALLIGATOR_GNASH_MIN+rand()%(CTM_ALLIGATOR_GNASH_MAX-CTM_ALLIGATOR_GNASH_MIN+1);
          }
        }
      } break;

    default: {
        SPR->phase=CTM_ALLIGATOR_PHASE_GNASH;
        SPR->animframe=0;
      } break;

  }
  return 0;
}

/* Hurt.
 */

static int _ctm_alligator_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
  SPR->invincible=60;
  if (SPR->hp-->1) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_HURT,0xff);
  } else if (!SPR->hp) {
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
    if (!ctm_treasure_new(assailant,CTM_ALLIGATOR_TREASURE)) return -1;

    // The usual kill-a-monster stuff...
    if (ctm_sprite_create_ghost(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_gore(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
    return ctm_sprite_kill_later(spr);
  } else return ctm_sprite_kill_later(spr);
  return 0;
}

/* Initialize.
 */

static int _ctm_alligator_init(struct ctm_sprite *spr) {
  spr->flop=1;
  SPR->hp=6;
  SPR->phase=CTM_ALLIGATOR_PHASE_GNASH;
  SPR->phasettl=CTM_ALLIGATOR_GNASH_MIN+rand()%(CTM_ALLIGATOR_GNASH_MAX-CTM_ALLIGATOR_GNASH_MIN+1);
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_alligator={
  .name="alligator",
  .objlen=sizeof(struct ctm_sprite_alligator),
  .soul=1,
  .guts=1,
  .del=_ctm_alligator_del,
  .init=_ctm_alligator_init,
  .draw=_ctm_alligator_draw,
  .update=_ctm_alligator_update,
  .hurt=_ctm_alligator_hurt,
  .test_damage_collision=_ctm_alligator_test_damage_collision,
  .test_hazard_collision=_ctm_alligator_test_damage_collision,
};
