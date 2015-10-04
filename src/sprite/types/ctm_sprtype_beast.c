#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "game/ctm_grid.h"
#include "video/ctm_video.h"
#include "display/ctm_display.h"

#define CTM_BEAST_TERMINAL_DISTANCE (CTM_TILESIZE*25)
#define CTM_BEAST_NO_INCLINATION_DISTANCE (CTM_TILESIZE*4)
#define CTM_BEAST_DELAY_MIN 10
#define CTM_BEAST_DELAY_MAX 50
#define CTM_BEAST_TTL 180 // must be away from players for so long before we delete
#define CTM_BEAST_WALK_SPEED ((CTM_TILESIZE*1)/16)

struct ctm_sprite_beast {
  struct ctm_sprite hdr;
  int clock;
  int frame;
  int dx,dy;
  int delay;
  int linger;
  int grabbed;
};

#define SPR ((struct ctm_sprite_beast*)spr)

/* Update.
 */

static int _ctm_beast_update(struct ctm_sprite *spr) {

  /* Animate. */
  if (++(SPR->clock)>=8) {
    SPR->clock=0;
    if (++(SPR->frame)>=4) SPR->frame=0;
    switch (SPR->frame) {
      case 0: spr->tile=(spr->tile&0xf0)|6; break;
      case 1: spr->tile=(spr->tile&0xf0)|7; break;
      case 2: spr->tile=(spr->tile&0xf0)|6; break;
      case 3: spr->tile=(spr->tile&0xf0)|8; break;
    }
  }

  if (SPR->grabbed) return 0;

  /* Walk around. */
  if (SPR->delay>0) {
    SPR->delay--;
    spr->tile=(spr->tile&0xf0)|6;
  } else if (SPR->dx||SPR->dy) {
    if (SPR->linger++>=1) {
      SPR->linger=0;
      spr->x+=SPR->dx*CTM_BEAST_WALK_SPEED;
      spr->y+=SPR->dy*CTM_BEAST_WALK_SPEED;
      int stop=0;
      if (SPR->dx&&(spr->x%CTM_TILESIZE==CTM_TILESIZE>>1)) stop=1;
      if (SPR->dy&&(spr->y%CTM_TILESIZE==CTM_TILESIZE>>1)) stop=1;
      if (stop) {
        SPR->dx=SPR->dy=0;
        SPR->delay=CTM_BEAST_DELAY_MIN+rand()%(CTM_BEAST_DELAY_MAX-CTM_BEAST_DELAY_MIN+1);
      }
    }
  } else {
    switch (rand()&3) {
      case 0: SPR->dx=-1; break;
      case 1: SPR->dx=1; break;
      case 2: SPR->dy=-1; break;
      case 3: SPR->dy=1; break;
    }
    int col=spr->x/CTM_TILESIZE+SPR->dx;
    int row=spr->y/CTM_TILESIZE+SPR->dy;
    if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) SPR->dx=SPR->dy=0;
    else {
      if (ctm_tileprop_exterior[ctm_grid.cellv[row*ctm_grid.colc+col].tile]) SPR->dx=SPR->dy=0;
      else {
        if (SPR->dx<0) spr->flop=0;
        else if (SPR->dx>0) spr->flop=1;
      }
    }
  }

  return 0;
}

/* Grab.
 */

static int _ctm_beast_grab(struct ctm_sprite *spr,int grab) {
  if (SPR->grabbed=grab) SPR->dx=SPR->dy=0;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_beast={
  .name="beast",
  .objlen=sizeof(struct ctm_sprite_beast),
  .soul=0,
  .guts=1,
  .update=_ctm_beast_update,
  .grab=_ctm_beast_grab,
};

/* Summon a new beast.
 */
 
struct ctm_sprite *ctm_sprite_beast_summon(int x,int y,int interior) {

  // Create sprite object.
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_beast);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);

  // Add to groups.
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) return 0;
  if (interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return 0;
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) return 0;
  }
  if (ctm_sprite_add_group(spr,&ctm_group_beast)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_hazard)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) return 0;
  spr->x=x;
  spr->y=y;

  // There are three universal species (kitten, raccoon, owl), and one local variety for each state.
  // Beast's tile is 0x16 + 16*state.
  int col=x/CTM_TILESIZE,row=y/CTM_TILESIZE;
  _try_again_: switch (rand()&3) {
    case 0: {
        int stateix=((row*3)/ctm_grid.rowc)*3+((col*3)/ctm_grid.colc);
        if ((stateix<0)||(stateix>8)) goto _try_again_;
        spr->tile=0x16+(stateix<<4);
      } break;
    case 1: spr->tile=0xa6; break;
    case 2: spr->tile=0xb6; break;
    case 3: spr->tile=0xc6; break;
  }
  
  return spr;
}
