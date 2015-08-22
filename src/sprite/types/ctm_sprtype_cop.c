#include "ctm.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "display/ctm_display.h"
#include "audio/ctm_audio.h"

#define CTM_COP_GUN_MIN 120
#define CTM_COP_GUN_MAX 240
#define CTM_COP_RECOIL_TIME 40
#define CTM_COP_BULLET_TTL 100
#define CTM_COP_WALK_SPEED ((CTM_TILESIZE*1)/16)
#define CTM_COP_BULLET_SPEED ((CTM_TILESIZE*3)/16)

int ctm_missile_add_ttl(struct ctm_sprite *spr,int ttl);

/* Object definition.
 */

struct ctm_sprite_cop {
  struct ctm_sprite hdr;
  int stateix;
  int playerid;
  int animcounter;
  int dx,dy;
  int reassess;
  int guncounter;
  int recoil;
};

#define SPR ((struct ctm_sprite_cop*)spr)

/* Check position.
 */

static int ctm_cop_bad_position(struct ctm_sprite *spr) {
  uint32_t prop=ctm_grid_tileprop_for_rect(spr->x-(CTM_TILESIZE>>1),spr->y-(CTM_TILESIZE>>1),CTM_TILESIZE,CTM_TILESIZE,0);
  if (prop) return 1;
  return 0;
}

/* Update.
 */

static int _ctm_cop_update(struct ctm_sprite *spr) {

  // During recoil, do nothing else.
  if (SPR->recoil>0) { 
    if (--(SPR->recoil)) return 0;
    spr->tile=0xa0|(spr->tile&0x0f);
  }

  // Die if we are far offscreen.
  int distance=ctm_display_distance_to_player_screen(0,spr->x,spr->y,0);
  if (distance>CTM_TILESIZE*8) return ctm_sprite_kill_later(spr);

  // Animate.
  if (--(SPR->animcounter)<=0) {
    SPR->animcounter=7;
    spr->tile^=0x10;
  }

  // Assess direction.
  if (--(SPR->reassess)<=0) {
    SPR->reassess=20;
    SPR->dx=SPR->dy=0;
  }
  if (!SPR->dx&&!SPR->dy) {
    switch (rand()%20) { // Don't always make a logical move. Shake it up sometimes.
      case 0: SPR->dy=1; break;
      case 1: SPR->dy=-1; break;
      case 2: SPR->dx=-1; break;
      case 3: SPR->dx=1; break;
      default: { // Move toward player on furthest axis.
          struct ctm_sprite *player=ctm_sprite_for_player(SPR->playerid);
          if (!player) return ctm_sprite_hurt(spr,0);
          int dx=player->x-spr->x; int adx=(dx<0)?-dx:dx;
          int dy=player->y-spr->y; int ady=(dy<0)?-dy:dy;
          switch (rand()%4) { // Sometimes move correct direction on the less-likely axis.
            case 0: SPR->dx=(dx<0)?-1:1; break;
            case 1: SPR->dy=(dy<0)?-1:1; break;
            default: {
                if (adx>ady) SPR->dx=(dx<0)?-1:1;
                else SPR->dy=(dy<0)?-1:1;
              }
          }
        }
    }
    spr->tile&=0xf0;
    if (SPR->dx<0) { spr->flop=0; spr->tile|=0x05; }
    else if (SPR->dx>0) { spr->flop=1; spr->tile|=0x05; }
    else if (SPR->dy<0) { spr->flop=0; spr->tile|=0x04; }
    else { spr->flop=0; spr->tile|=0x03; }
  }

  // Move.
  spr->x+=SPR->dx*CTM_COP_WALK_SPEED;
  spr->y+=SPR->dy*CTM_COP_WALK_SPEED;
  if (ctm_cop_bad_position(spr)) { 
    spr->x-=SPR->dx*CTM_COP_WALK_SPEED; 
    spr->y-=SPR->dy*CTM_COP_WALK_SPEED; 
    SPR->reassess=0; 
  }

  // Shoot.
  if (--(SPR->guncounter)<=0) {
    CTM_SFX(GUN)
    SPR->guncounter=CTM_COP_GUN_MIN+rand()%(CTM_COP_GUN_MAX-CTM_COP_GUN_MIN+1);
    spr->tile=0xc0|(spr->tile&0x0f);
    SPR->recoil=CTM_COP_RECOIL_TIME;
    struct ctm_sprite *bullet=ctm_sprite_missile_new(spr,0xd0|(spr->tile&0x0f),SPR->dx*CTM_COP_BULLET_SPEED,SPR->dy*CTM_COP_BULLET_SPEED);
    if (!bullet) return -1;
    bullet->flop=spr->flop;
    ctm_missile_add_ttl(bullet,CTM_COP_BULLET_TTL);
  }

  // If we accidentally step outside our jurisdiction, we explode in a blaze of guts.
  if (ctm_state_for_pixel(spr->x,spr->y)!=SPR->stateix) return ctm_sprite_hurt(spr,0);
  
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_cop={
  .name="cop",
  .objlen=sizeof(struct ctm_sprite_cop),
  .update=_ctm_cop_update,
  .soul=1,
  .guts=1,
};

/* Choose starting position.
 */

static int ctm_cop_locate(int *x,int *y,int worldx,int worldy,int worldw,int worldh,int stateix) {
  int limitx=CTM_TILESIZE*ctm_grid.colc-1;
  int limity=CTM_TILESIZE*ctm_grid.rowc-1;
  int x1=worldx+(worldw>>2),x2=worldx+(worldw>>1),x3=worldx+worldw-(worldw>>2);
  int y1=worldy+(worldh>>2),y2=worldy+(worldh>>1),y3=worldy+worldh-(worldh>>2);
  int panic=100; while (panic-->0) {

    // Chose a random position within the given boundaries.
    *x=worldx+rand()%worldw;
    *y=worldy+rand()%worldh;

    // If it's inside the middle half of the boundaries, clamp it out (that's the visible part).
    if ((*x>x1)&&(*x<x3)&&(*y>y1)&&(*y<y3)) {
      int l=*x-x1,t=*y-y1,r=x3-*x,b=y3-*y;
      if ((l<=t)&&(l<=r)&&(l<=b)) *x=x1;
      else if ((t<=r)&&(t<=b)) *y=y1;
      else if (r<=b) *x=x3;
      else *y=y3;
    }

    // Clamp to world boundaries and confirm that it is in-state.
    if (*x<0) *x=0;
    else if (*x>limitx) *x=limitx;
    if (*y<0) *y=0;
    else if (*y>limity) *y=limity;
    if (ctm_state_for_pixel(*x,*y)!=stateix) continue;

    // Examine tile.
    uint32_t prop=ctm_grid_tileprop_for_rect(*x-(CTM_TILESIZE>>1),*y-(CTM_TILESIZE>>1),CTM_TILESIZE,CTM_TILESIZE,0);
    if (prop) continue;
    return 0;
  }
  return -1;
}

/* Public ctor.
 */

struct ctm_sprite *ctm_sprite_cop_new(struct ctm_sprite *player) {

  // Acquire visible range.
  if (!player||(player->type!=&ctm_sprtype_player)) return 0;
  struct ctm_sprite_player *PLAYER=(struct ctm_sprite_player*)player;
  struct ctm_display *display=ctm_display_for_playerid(PLAYER->playerid);
  if (!display) return 0;
  int worldx,worldy,worldw,worldh;
  if (ctm_display_game_get_position(&worldx,&worldy,&worldw,&worldh,0,display)<0) return 0;
  int stateix=ctm_state_for_pixel(player->x,player->y);

  // Find a good starting position -- No more than half a screen off any edge, and physically passable.
  // We are exterior regardless of the player.
  worldx-=worldw>>1;
  worldy-=worldh>>1;
  worldw<<=1;
  worldh<<=1;
  int x,y;
  if (ctm_cop_locate(&x,&y,worldx,worldy,worldw,worldh,stateix)<0) return 0;

  // Allocate sprite.
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_cop);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);

  // Basic setup.
  spr->x=x;
  spr->y=y;
  spr->tile=0xa3;
  SPR->stateix=stateix;
  SPR->playerid=PLAYER->playerid;
  SPR->guncounter=CTM_COP_GUN_MIN+rand()%(CTM_COP_GUN_MAX-CTM_COP_GUN_MIN+1);

  // Add to groups.
  if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_cop)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_hazard)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) { ctm_sprite_kill(spr); return 0; }
  
  return 0;
}
