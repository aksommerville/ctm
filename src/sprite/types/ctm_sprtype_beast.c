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

struct ctm_sprite_beast {
  struct ctm_sprite hdr;
  int clock;
  int frame;
  int dx,dy;
  int delay;
  int linger;
  int grabbed;
  int ttl; // Counts down while candidate for deletion (due to nobody nearby).
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

  /* If too far off screen, terminate.
   * Also, note the nearest player. Move towards him most of the time.
   */
  int closeenough=0,i;
  int inclinex=0,incliney=0;
  for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *player=ctm_group_player.sprv[i];
    if (player->interior!=spr->interior) continue;
    int dx=player->x-spr->x;
    int dy=player->y-spr->y;
    int distance=((dx<0)?-dx:dx)+((dy<0)?-dy:dy);
    if (distance<CTM_BEAST_TERMINAL_DISTANCE) { 
      closeenough=1;
      if (distance<CTM_BEAST_NO_INCLINATION_DISTANCE) inclinex=incliney=0;
      else {
        int adx=(dx<0)?-dx:dx,ady=(dy<0)?-dy:dy;
        if (adx>ady) { inclinex=(dx>0)?1:-1; incliney=0; }
        else { incliney=(dy>0)?1:-1; inclinex=0; }
      }
    }
  }
  if (!closeenough) {
    if (--(SPR->ttl)<=0) return ctm_sprite_kill_later(spr);
  } else {
    SPR->ttl=CTM_BEAST_TTL;
  }

  /* Walk around. */
  if (SPR->delay>0) {
    SPR->delay--;
    spr->tile=(spr->tile&0xf0)|6;
  } else if (SPR->dx||SPR->dy) {
    if (SPR->linger++>=1) {
      SPR->linger=0;
      spr->x+=SPR->dx;
      spr->y+=SPR->dy;
      int stop=0;
      if (SPR->dx&&(spr->x%CTM_TILESIZE==CTM_TILESIZE>>1)) stop=1;
      if (SPR->dy&&(spr->y%CTM_TILESIZE==CTM_TILESIZE>>1)) stop=1;
      if (stop) {
        SPR->dx=SPR->dy=0;
        SPR->delay=CTM_BEAST_DELAY_MIN+rand()%(CTM_BEAST_DELAY_MAX-CTM_BEAST_DELAY_MIN+1);
      }
    }
  } else {
    if ((inclinex||incliney)&&(rand()%3)) { // usually follow our inclination (towards player)
      SPR->dx=inclinex;
      SPR->dy=incliney;
    } else switch (rand()&3) {
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

/* Spawn.
 */

int ctm_sprite_spawn_beast() {

  // Pick a player at random. No players? No beast.
  if (ctm_group_player.sprc<1) return 0;
  int playerix=rand()%ctm_group_player.sprc;
  struct ctm_sprite *player=ctm_group_player.sprv[playerix];
  if (player->type!=&ctm_sprtype_player) return 0;
  if (player->interior) return 0;
  struct ctm_sprite_player *PLAYER=(struct ctm_sprite_player*)player;

  // Pick a random location within about half a screen's width of the player.
  int screenw=0,screenh=0;
  if (ctm_display_player_get_size(&screenw,&screenh,PLAYER->playerid)<1) return 0;
  if ((screenw<1)||(screenh<1)) return 0;
  int screencolc=screenw/CTM_TILESIZE; if (screencolc<2) screencolc=2; int screencolc2=screencolc>>1;
  int screenrowc=screenh/CTM_TILESIZE; if (screenrowc<2) screenrowc=2; int screenrowc2=screenrowc>>1;
  int col=player->x/CTM_TILESIZE,row=player->y/CTM_TILESIZE;
  int xp,yp,xc,yc;
  switch (rand()&7) {
    case 0: xp=col-screencolc; yp=row-screenrowc; xc=screencolc2; yc=screenrowc2; break;
    case 1: xp=col-screencolc2; yp=row-screenrowc; xc=screencolc; yc=screenrowc2; break;
    case 2: xp=col+screencolc2; yp=row-screenrowc; xc=screencolc2; yc=screenrowc2; break;
    case 3: xp=col-screencolc; yp=row-screenrowc2; xc=screencolc2; yc=screenrowc; break;
    case 4: xp=col+screencolc2; yp=row-screenrowc2; xc=screencolc2; yc=screenrowc; break;
    case 5: xp=col-screencolc; yp=row+screenrowc2; xc=screencolc2; yc=screenrowc2; break;
    case 6: xp=col-screencolc2; yp=row+screenrowc2; xc=screencolc; yc=screenrowc2; break;
    case 7: xp=col+screencolc2; yp=row+screenrowc2; xc=screencolc2; yc=screenrowc2; break;
  }
  col=xp+rand()%xc;
  row=yp+rand()%yc;
  int x=col*CTM_TILESIZE+(CTM_TILESIZE>>1);
  int y=row*CTM_TILESIZE+(CTM_TILESIZE>>1);

  // If this location is out of bounds, blocked, or within sight of any player, forget about it.
  if ((x<0)||(y<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return 0;
  uint8_t tile=ctm_grid.cellv[row*ctm_grid.colc+col].tile;
  uint32_t prop=ctm_tileprop_exterior[tile];
  if (prop) return 0;
  int i; for (i=0;i<ctm_group_player.sprc;i++) {
    // This test assumes that all player displays are the same size, which at least for now is true enough.
    struct ctm_sprite *qspr=ctm_group_player.sprv[i];
    int dx=x-qspr->x;
    if (dx<-(screenw>>1)-CTM_TILESIZE) continue;
    if (dx>(screenw>>1)+CTM_TILESIZE) continue;
    int dy=y-qspr->y;
    if (dy<-(screenh>>1)-CTM_TILESIZE) continue;
    if (dy>(screenh>>1)+CTM_TILESIZE) continue;
    return 0;
  }

  // OK, let's summon the beast.
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_beast);
  if (!spr) return -1;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return -1; }
  ctm_sprite_del(spr);
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) return -1;
  if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) return -1;
  if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) return -1;
  if (ctm_sprite_add_group(spr,&ctm_group_beast)<0) return -1;
  if (ctm_sprite_add_group(spr,&ctm_group_hazard)<0) return -1;
  if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) return -1;
  if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) return -1;
  spr->x=x;
  spr->y=y;

  // There are three universal species (kitten, raccoon, owl), and one local variety for each state.
  // Beast's tile is 0x16 + 16*state.
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

  return 1;
}
