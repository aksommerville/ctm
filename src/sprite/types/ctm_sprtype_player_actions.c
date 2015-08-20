#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "input/ctm_input.h"
#include "video/ctm_video.h"
#include "audio/ctm_audio.h"

extern int ctm_sprite_prize_set_prizeid(struct ctm_sprite *spr,int prizeid);
extern int ctm_sprite_prize_get_prizeid(const struct ctm_sprite *spr);
int ctm_player_set_motion(struct ctm_sprite *spr,int dx,int dy);
int ctm_player_unset_motion(struct ctm_sprite *spr,int dx,int dy);

int ctm_missile_setup_coin(struct ctm_sprite *spr);
int ctm_missile_add_ttl(struct ctm_sprite *spr,int framec);
struct ctm_sprite *ctm_hookshot_new(struct ctm_sprite *owner,int dx,int dy);
struct ctm_sprite *ctm_boomerang_new(struct ctm_sprite *owner,int dx,int dy);
struct ctm_sprite *ctm_flames_new(struct ctm_sprite *owner,double offset);
int ctm_flames_drop(struct ctm_sprite *owner);

#define SPR ((struct ctm_sprite_player*)spr)

#define CTM_PLAYER_SWORD_TIME 60
#define CTM_PLAYER_SPEECH_TIME 60
#define CTM_PLAYER_AUTOBOX_TIME 30

#define CTM_PLAYER_SPEECH_DISTANCE (CTM_TILESIZE*7)
#define CTM_PLAYER_SPEECH_FAVOR       5
#define CTM_PLAYER_SPEECH_FAVOR_RADIO 3

// Duration before eliminating metal blade, in frames. (4 pixels/frame)
#define CTM_METAL_BLADE_TTL 75
#define CTM_ARROW_TTL 75

/* Setup item store and inventory.
 */

void ctm_player_set_initial_items(struct ctm_sprite *spr) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return;

  // Basically, all inventory begins at 0 or UNLIMITED.
  // The only countable thing you start with is SPEECH.
  SPR->inventory[CTM_ITEM_NONE]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_SWORD]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_SPEECH]=10;
  SPR->inventory[CTM_ITEM_METAL_BLADE]=0;
  SPR->inventory[CTM_ITEM_MAGIC_WAND]=0;
  SPR->inventory[CTM_ITEM_HOOKSHOT]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_BOOMERANG]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_AUTOBOX]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_FLAMES]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_BOW]=0;
  SPR->inventory[CTM_ITEM_LENS_OF_TRUTH]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_COIN]=0;
  SPR->inventory[CTM_ITEM_12]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_13]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_14]=CTM_INVENTORY_UNLIMITED;
  SPR->inventory[CTM_ITEM_15]=CTM_INVENTORY_UNLIMITED;

  // Set the three selected items and populate any slots in item store.
  SPR->item_primary=CTM_ITEM_COIN;
  SPR->item_secondary=CTM_ITEM_SWORD;
  SPR->item_tertiary=CTM_ITEM_SPEECH;

}

/* Get prize.
 */

int ctm_player_get_prize(struct ctm_sprite *spr,int prizeid) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return -1;
  switch (prizeid) {
    case CTM_PRIZE_SPEECH: CTM_SFX(PRIZE) return ctm_player_add_inventory(spr,CTM_ITEM_SPEECH,1);
    case CTM_PRIZE_ARROW: CTM_SFX(PRIZE) return ctm_player_add_inventory(spr,CTM_ITEM_BOW,4);
    case CTM_PRIZE_COIN: CTM_SFX(PRIZE) return ctm_player_add_inventory(spr,CTM_ITEM_COIN,3);
    case CTM_PRIZE_HEART: {
        CTM_SFX(HEAL) 
        if (SPR->hp<1) { // Coming back to life.
          if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
          if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) return -1;
          if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) return -1;
          if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) return -1;
        }
        if (SPR->hp<SPR->hpmax) SPR->hp++; 
      } break;
    case CTM_PRIZE_WAND_FLUID: CTM_SFX(PRIZE) return ctm_player_add_inventory(spr,CTM_ITEM_MAGIC_WAND,3);
    case CTM_PRIZE_BLADES: CTM_SFX(PRIZE) return ctm_player_add_inventory(spr,CTM_ITEM_METAL_BLADE,3);
  }
  return 0;
}

/* Create a prize for having killed (victim).
 */

int ctm_player_create_prize(struct ctm_sprite *spr,struct ctm_sprite *victim) {
  if (!spr||!victim) return -1;
  if (spr->type!=&ctm_sprtype_player) return -1;

  struct ctm_sprite *prize=ctm_sprite_alloc(&ctm_sprtype_prize);
  if (!prize) return -1;
  if (ctm_sprite_add_group(prize,&ctm_group_keepalive)<0) { ctm_sprite_del(prize); return -1; }
  ctm_sprite_del(prize);
  prize->x=victim->x;
  prize->y=victim->y;
  if (prize->interior=victim->interior) {
    if (ctm_sprite_add_group(prize,&ctm_group_interior)<0) { ctm_sprite_kill(prize); return -1; }
  } else {
    if (ctm_sprite_add_group(prize,&ctm_group_exterior)<0) { ctm_sprite_kill(prize); return -1; }
  }
  if (ctm_sprite_add_group(prize,&ctm_group_update)<0) { ctm_sprite_kill(prize); return -1; }
  if (ctm_sprite_add_group(prize,&ctm_group_prize)<0) { ctm_sprite_kill(prize); return -1; }
  if (ctm_sprite_add_group(prize,&ctm_group_hookable)<0) { ctm_sprite_kill(prize); return -1; }
  if (ctm_sprite_add_group(prize,&ctm_group_boomerangable)<0) { ctm_sprite_kill(prize); return -1; }

  // SPEECH, COIN, and HEART are always possible.
  // Others appear only if relevant.
  // NB: You don't need to be injured to create HEART; that's intentional.
  int can_make_arrow=ctm_player_has_item(spr,CTM_ITEM_BOW)?1:0;
  int can_make_fluid=ctm_player_has_item(spr,CTM_ITEM_MAGIC_WAND)?1:0;
  int can_make_blade=ctm_player_has_item(spr,CTM_ITEM_METAL_BLADE)?1:0;
  int prizec=3+can_make_arrow+can_make_fluid+can_make_blade;
  int prizeid=rand()%prizec;
  if (!prizeid--) prizeid=CTM_PRIZE_SPEECH;
  else if (!prizeid--) prizeid=CTM_PRIZE_COIN;
  else if (!prizeid--) prizeid=CTM_PRIZE_HEART;
  else if (can_make_arrow&&!prizeid--) prizeid=CTM_PRIZE_ARROW;
  else if (can_make_fluid&&!prizeid--) prizeid=CTM_PRIZE_WAND_FLUID;
  else if (can_make_blade&&!prizeid--) prizeid=CTM_PRIZE_BLADES;
  ctm_sprite_prize_set_prizeid(prize,prizeid);
  
  return 0;
}

/* Check prize pickup.
 */

int ctm_player_check_prize_pickup(struct ctm_sprite *spr) {
  int xlo=spr->x+CTM_PLAYER_PHLEFT;
  int xhi=spr->x+CTM_PLAYER_PHRIGHT;
  int ylo=spr->y+CTM_PLAYER_PHTOP;
  int yhi=spr->y+CTM_PLAYER_PHBOTTOM;
  int i; for (i=0;i<ctm_group_prize.sprc;i++) {
    struct ctm_sprite *prize=ctm_group_prize.sprv[i];
    if (prize->interior!=spr->interior) continue;
    if (prize->x<xlo) continue;
    if (prize->x>xhi) continue;
    if (prize->y<ylo) continue;
    if (prize->y>yhi) continue;
    if (prize->type==&ctm_sprtype_prize) {
      int prizeid=ctm_sprite_prize_get_prizeid(prize);
      if (SPR->hp<1) {
        if (prizeid!=CTM_PRIZE_HEART) continue;
      }
      ctm_sprite_kill_later(prize);
      return ctm_player_get_prize(spr,prizeid);
    } else if (prize->type==&ctm_sprtype_treasure) {
      if (SPR->hp<1) continue;
      struct ctm_sprite_treasure *PRIZE=(struct ctm_sprite_treasure*)prize;
      CTM_SFX(TREASURE)
      if ((PRIZE->item>=0)&&(PRIZE->item<16)) {
        if (!ctm_sprite_toast_new(prize->x,prize->y-CTM_TILESIZE,prize->interior,0xf0+PRIZE->item)) return -1;
      } else switch (PRIZE->item) {
        case CTM_ITEM_PIZZA_FART: if (!ctm_sprite_toast_new(prize->x,prize->y-CTM_TILESIZE,prize->interior,0xe6)) return -1; break;
      }
      ctm_sprite_kill_later(prize);
      if (ctm_player_get_treasure(spr,PRIZE->item,PRIZE->quantity)<0) return -1;
      PRIZE->item=PRIZE->quantity=0;
      return 0;
    }
  }
  return 0;
}

/* Search for fragile sprites and crush them.
 */

int ctm_player_deal_damage(struct ctm_sprite *spr,int x,int y,int w,int h) {
  int did_commit_murder=0;
  int did_commit_heroism=0; // It's a fine distinction.
  x-=CTM_TILESIZE>>1;
  y-=CTM_TILESIZE>>1;
  w+=CTM_TILESIZE;
  h+=CTM_TILESIZE;
  int i; 

  for (i=0;i<ctm_group_fragile.sprc;i++) {
    struct ctm_sprite *qspr=ctm_group_fragile.sprv[i];
    if (qspr==spr) continue;
    if (qspr->interior!=spr->interior) continue;
    if (qspr->x<x) continue;
    if (qspr->y<y) continue;
    if (qspr->x>=x+w) continue;
    if (qspr->y>=y+h) continue;
    if (qspr->type->soul) did_commit_murder=1;
    else did_commit_heroism=1;
    if (ctm_sprite_hurt(qspr,spr)<0) return -1;
  }
  
  return 0;
}

/* Sword.
 */

static int ctm_player_swing_sword(struct ctm_sprite *spr) {
  if (SPR->boxing) return 0;

  CTM_SFX(SWORD)

  // Cancel any movement in progress.
  if (ctm_player_unset_motion(spr,SPR->dx,0)<0) return -1;
  if (ctm_player_unset_motion(spr,0,SPR->dy)<0) return -1;

  SPR->speaking=0;
  SPR->swinging=CTM_PLAYER_SWORD_TIME;
  SPR->row=0x90;
  
  return 0;
}

int ctm_player_unswing_sword(struct ctm_sprite *spr) {
  SPR->swinging=0;
  if (!SPR->holdgrp->sprc) {
    SPR->row=0x60;
  }

  // If the dpad is pressed, pretend it's new the next time we see it.
  // ie: Resume walking if relevant.
  SPR->pvinput&=~(CTM_BTNID_UP|CTM_BTNID_DOWN|CTM_BTNID_LEFT|CTM_BTNID_RIGHT);
  
  return 0;
}

// Nonzero if the first affected voter is receptive (not numb). Also nonzero if no such voters.
static int ctm_player_speech_validate_radio(int tostate,int toparty) {
  if ((tostate>=0)&&(tostate<9)) {
    struct ctm_sprgrp *grp=ctm_group_state_voter+tostate;
    if (grp->sprc<1) return 1;
    if (grp->sprv[0]->type!=&ctm_sprtype_voter) return 1;
    struct ctm_sprite_voter *voter=(struct ctm_sprite_voter*)(grp->sprv[0]);
    if (voter->numb) return 0;
  } else if ((toparty>=0)&&(toparty<=7)) {
    int i; for (i=0;i<ctm_group_voter.sprc;i++) {
      struct ctm_sprite *voter=ctm_group_voter.sprv[i];
      if (((struct ctm_sprite_voter*)voter)->party!=toparty) continue;
      if (((struct ctm_sprite_voter*)voter)->numb) return 0;
    }
  }
  return 1;
}

/* Begin or end speech delivery.
 */

static int ctm_player_begin_speech(struct ctm_sprite *spr) {

  // If we're standing on a RADIO cell, it's a whole different ball game.
  // These are only interior, so don't bother looking if we are outside.
  int isradio=0;
  if (spr->interior) {
    int col=spr->x/CTM_TILESIZE,row=spr->y/CTM_TILESIZE;
    if ((col>=0)&&(row>=0)&&(col<ctm_grid.colc)&&(row<ctm_grid.rowc)) {
      int tostate=-1,toparty=-1;
      switch (ctm_tileprop_interior[ctm_grid.cellv[row*ctm_grid.colc+col].itile]&CTM_TILEPROP_RADIO) {
        case CTM_TILEPROP_RADIO_AK: tostate=CTM_STATE_ALASKA; break;
        case CTM_TILEPROP_RADIO_WI: tostate=CTM_STATE_WISCONSIN; break;
        case CTM_TILEPROP_RADIO_VT: tostate=CTM_STATE_VERMONT; break;
        case CTM_TILEPROP_RADIO_CA: tostate=CTM_STATE_CALIFORNIA; break;
        case CTM_TILEPROP_RADIO_KS: tostate=CTM_STATE_KANSAS; break;
        case CTM_TILEPROP_RADIO_VA: tostate=CTM_STATE_VIRGINIA; break;
        case CTM_TILEPROP_RADIO_HI: tostate=CTM_STATE_HAWAII; break;
        case CTM_TILEPROP_RADIO_TX: tostate=CTM_STATE_TEXAS; break;
        case CTM_TILEPROP_RADIO_FL: tostate=CTM_STATE_FLORIDA; break;
        case CTM_TILEPROP_RADIO_WHITE: toparty=CTM_PARTY_WHITE; break;
        case CTM_TILEPROP_RADIO_BLACK: toparty=CTM_PARTY_BLACK; break;
        case CTM_TILEPROP_RADIO_YELLOW: toparty=CTM_PARTY_YELLOW; break;
        case CTM_TILEPROP_RADIO_GREEN: toparty=CTM_PARTY_GREEN; break;
        case CTM_TILEPROP_RADIO_BLUE: toparty=CTM_PARTY_BLUE; break;
        case CTM_TILEPROP_RADIO_RED: toparty=CTM_PARTY_RED; break;
      }
      if ((tostate>=0)||(toparty>=0)) {
        if (!ctm_player_speech_validate_radio(tostate,toparty)) {
          // ABORT! The affected voters are numb right now (but it's likely that the player can't see them.)
          SPR->inventory[CTM_ITEM_SPEECH]++;
          CTM_SFX(REFUSE)
          return 0;
        }
        int err=ctm_player_spend(spr,5);
        if (err<0) return -1;
        if (!err) { // Can't afford air time. Return this speech to the inventory.
          // DO NOT deliver the speech locally, at the radio station. That would be confusing.
          SPR->inventory[CTM_ITEM_SPEECH]++;
          CTM_SFX(REFUSE)
          return 0;
        }
        if (ctm_perform_broad_event(tostate,toparty,SPR->party,CTM_PLAYER_SPEECH_FAVOR_RADIO)<0) return -1;
        isradio=1;
      }
    }
  }

  if (isradio) {
    CTM_SFX(RADIO_SPEECH)
    SPR->radiospeeches++;
  } else {
    CTM_SFX(SPEECH)
    SPR->facespeeches++;
  }

  if (SPR->swinging&&(ctm_player_unswing_sword(spr)<0)) return -1;
  if (ctm_player_unset_motion(spr,SPR->dx,0)<0) return -1;
  if (ctm_player_unset_motion(spr,0,SPR->dy)<0) return -1;

  SPR->speaking=CTM_PLAYER_SPEECH_TIME;

  // Influence nearby voters.
  if (!isradio) {
    if (ctm_perform_local_event(spr->x,spr->y,spr->interior,CTM_PLAYER_SPEECH_DISTANCE,SPR->party,CTM_PLAYER_SPEECH_FAVOR)<0) return -1;
  }
  
  return 0;
}

int ctm_player_end_speech(struct ctm_sprite *spr) {
  SPR->speaking=0;
  
  // If the dpad is pressed, pretend it's new the next time we see it.
  // ie: Resume walking if relevant.
  SPR->pvinput&=~(CTM_BTNID_UP|CTM_BTNID_DOWN|CTM_BTNID_LEFT|CTM_BTNID_RIGHT);
  
  return 0;
}

/* Fire missiles.
 */

static int ctm_player_fire_arrow(struct ctm_sprite *spr) {
  CTM_SFX(ARROW)
  struct ctm_sprite *missile;
  switch (SPR->col) {
    case 0: missile=ctm_sprite_missile_new(spr,0xc0,0,4); break;
    case 1: missile=ctm_sprite_missile_new(spr,0xc1,0,-4); break;
    case 2: if (spr->flop) {
        missile=ctm_sprite_missile_new(spr,0xc2,4,0);
      } else {
        missile=ctm_sprite_missile_new(spr,0xc2,-4,0);
      } break;
  }
  if (!missile) return -1;
  ctm_missile_add_ttl(spr,CTM_ARROW_TTL);
  missile->flop=spr->flop;
  return 0;
}

static int ctm_player_throw_coin(struct ctm_sprite *spr) {
  CTM_SFX(THROW_COIN)
  struct ctm_sprite *missile;
  switch (SPR->col) {
    case 0: missile=ctm_sprite_missile_new(spr,0xfb,0,3); break;
    case 1: missile=ctm_sprite_missile_new(spr,0xfb,0,-3); break;
    case 2: if (spr->flop) {
        missile=ctm_sprite_missile_new(spr,0xfb,3,0);
      } else {
        missile=ctm_sprite_missile_new(spr,0xfb,-3,0);
      } break;
  }
  if (!missile) return -1;
  if (ctm_missile_setup_coin(missile)<0) return -1;
  return 0;
}

static int ctm_player_fire_magic_wand(struct ctm_sprite *spr) {
  CTM_SFX(WAND)
  struct ctm_sprite *missile;
  switch (SPR->col) {
    case 0: missile=ctm_sprite_missile_new(spr,0x53,0,4); break;
    case 1: missile=ctm_sprite_missile_new(spr,0x54,0,-4); break;
    case 2: if (spr->flop) {
        missile=ctm_sprite_missile_new(spr,0x55,4,0);
      } else {
        missile=ctm_sprite_missile_new(spr,0x55,-4,0);
      } break;
  }
  if (!missile) return -1;
  ctm_missile_add_ttl(spr,CTM_ARROW_TTL);
  missile->flop=spr->flop;
  return 0;
}

static int ctm_player_fire_metal_blade(struct ctm_sprite *spr) {
  CTM_SFX(BLADE)
  int dx=0,dy=0;
  switch (SPR->col) {
    case 0: dy=1; if (SPR->pvinput&CTM_BTNID_LEFT) dx=-1; else if (SPR->pvinput&CTM_BTNID_RIGHT) dx=1; break;
    case 1: dy=-1; if (SPR->pvinput&CTM_BTNID_LEFT) dx=-1; else if (SPR->pvinput&CTM_BTNID_RIGHT) dx=1; break;
    case 2: if (spr->flop) {
        dx=1; if (SPR->pvinput&CTM_BTNID_UP) dy=-1; else if (SPR->pvinput&CTM_BTNID_DOWN) dy=1; break;
      } else {
        dx=-1; if (SPR->pvinput&CTM_BTNID_UP) dy=-1; else if (SPR->pvinput&CTM_BTNID_DOWN) dy=1; break;
      } break;
  }
  if (dx&&dy) { dx*=3; dy*=3; } else { dx*=4; dy*=4; }
  struct ctm_sprite *missile=ctm_sprite_missile_new(spr,0x0d,dx,dy);
  if (!missile) return -1;
  if (ctm_missile_add_ttl(missile,CTM_METAL_BLADE_TTL)<0) return -1;
  return 0;
}

/* Hookshot.
 */

static int ctm_player_begin_hookshot(struct ctm_sprite *spr) {
  if (SPR->holdgrp->sprc) return 0;
  if (SPR->grabbed) return 0;
  CTM_SFX(HOOKSHOT_BEGIN)
  if (ctm_player_unset_motion(spr,SPR->dx,SPR->dy)<0) return -1;
  int dx=0,dy=0;
  if (SPR->col==0) dy=1;
  else if (SPR->col==1) dy=-1;
  else if (spr->flop) dx=1;
  else dx=-1;
  struct ctm_sprite *hookshot=ctm_hookshot_new(spr,dx,dy);
  if (!hookshot) return -1;
  if (ctm_sprgrp_add_sprite(SPR->holdgrp,hookshot)<0) return -1;
  SPR->row=0x90;
  return 0;
}

static int ctm_player_cancel_hookshot(struct ctm_sprite *spr) {
  if (SPR->holdgrp->sprc<1) return 0;
  if (SPR->holdgrp->sprv[0]->type!=&ctm_sprtype_hookshot) return 0;
  CTM_SFX(HOOKSHOT_CANCEL)
  if (ctm_sprgrp_kill(SPR->holdgrp)<0) return -1;
  SPR->pvinput&=~(CTM_BTNID_UP|CTM_BTNID_DOWN|CTM_BTNID_LEFT|CTM_BTNID_RIGHT);
  return 0;
}

/* Boomerang.
 */

static int ctm_player_throw_boomerang(struct ctm_sprite *spr) {
  if (SPR->boomerang->sprc) return 0;
  CTM_SFX(BOOMERANG_BEGIN)
  int dx=0,dy=0;
  switch (SPR->col) {
    case 0: dy=1; if (SPR->pvinput&CTM_BTNID_LEFT) dx=-1; else if (SPR->pvinput&CTM_BTNID_RIGHT) dx=1; break;
    case 1: dy=-1; if (SPR->pvinput&CTM_BTNID_LEFT) dx=-1; else if (SPR->pvinput&CTM_BTNID_RIGHT) dx=1; break;
    case 2: if (spr->flop) {
        dx=1; if (SPR->pvinput&CTM_BTNID_UP) dy=-1; else if (SPR->pvinput&CTM_BTNID_DOWN) dy=1; break;
      } else {
        dx=-1; if (SPR->pvinput&CTM_BTNID_UP) dy=-1; else if (SPR->pvinput&CTM_BTNID_DOWN) dy=1; break;
      } break;
  }
  struct ctm_sprite *boomerang=ctm_boomerang_new(spr,dx,dy);
  if (!boomerang) return -1;
  if (ctm_sprgrp_add_sprite(SPR->boomerang,boomerang)<0) { ctm_sprite_kill(boomerang); return -1; }
  return 0;
}

/* Autobox.
 */

static int ctm_player_bonk(struct ctm_sprite *spr) {
  int dx=0,dy=0;
  int l=spr->x-8,t=spr->y-8;
  if (SPR->col==0) { t+=CTM_TILESIZE; dy=1; }
  else if (SPR->col==1) { t-=CTM_TILESIZE; dy=-1; }
  else if (spr->flop) { l+=CTM_TILESIZE; dx=1; }
  else { l-=CTM_TILESIZE; dx=-1; }
  int r=l+16,b=t+16;
  dx*=CTM_TILESIZE;
  dy*=CTM_TILESIZE;
  uint32_t prop=CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS|CTM_TILEPROP_DOOR|CTM_TILEPROP_HOLE|CTM_TILEPROP_WATER;
  int spoke=0;
  int i; for (i=0;i<ctm_group_bonkable.sprc;i++) {
    struct ctm_sprite *qspr=ctm_group_bonkable.sprv[i];
    if (qspr==spr) continue;
    if (qspr->interior!=spr->interior) continue;
    if (qspr->x<l) continue;
    if (qspr->x>r) continue;
    if (qspr->y<t) continue;
    if (qspr->y>b) continue;
    if (!(ctm_grid_tileprop_for_pixel(qspr->x+dx,qspr->y+dy,qspr->interior)&prop)) {
      if (!spoke) { spoke=1; CTM_SFX(BONK) }
      qspr->x+=dx;
      qspr->y+=dy;
    }
  }
  if (!spoke) CTM_SFX(NO_BONK)
  return 0;
}

static int ctm_player_begin_autobox(struct ctm_sprite *spr) {
  if (SPR->swinging) return 0;

  // Cancel any movement in progress.
  if (ctm_player_unset_motion(spr,SPR->dx,0)<0) return -1;
  if (ctm_player_unset_motion(spr,0,SPR->dy)<0) return -1;

  SPR->speaking=0;
  SPR->boxing=CTM_PLAYER_AUTOBOX_TIME;
  SPR->row=0x90;

  if (ctm_player_bonk(spr)<0) return -1;
  
  return 0;
}

static int ctm_player_cancel_autobox(struct ctm_sprite *spr) {
  SPR->boxing=0;
  if (!SPR->holdgrp->sprc) SPR->row=0x60;
  SPR->pvinput&=~(CTM_BTNID_UP|CTM_BTNID_DOWN|CTM_BTNID_LEFT|CTM_BTNID_RIGHT);
  return 0;
}

/* Flames.
 */

static int ctm_player_begin_flames(struct ctm_sprite *spr) {
  struct ctm_sprite *flame;
  CTM_SFX(FLAME_BEGIN)
  if (!(flame=ctm_flames_new(spr,0.00))) return -1;
  return 0;
}

static int ctm_player_end_flames(struct ctm_sprite *spr) {
  return ctm_flames_drop(spr);
}

/* Lens of truth.
 */

static int ctm_player_engage_lens_of_truth(struct ctm_sprite *spr) {
  CTM_SFX(LENS_BEGIN)
  SPR->lens_of_truth=1;
  return 0;
}

static int ctm_player_disengage_lens_of_truth(struct ctm_sprite *spr) {
  CTM_SFX(LENS_END)
  SPR->lens_of_truth=0;
  return 0;
}

/* Start or stop action.
 */

int ctm_player_begin_action(struct ctm_sprite *spr,int item) {
  if ((item<0)||(item>16)) return 0;
  if ((SPR->hp<1)&&(item!=CTM_ITEM_LENS_OF_TRUTH)) return 0; // Only LOT usable while dead.
  //if (SPR->holdgrp->sprc) return 0;
  switch (SPR->inventory[item]) {
    case CTM_INVENTORY_UNLIMITED: break;
    default: {
        if (SPR->inventory[item]<1) {
          CTM_SFX(REFUSE)
          return 0;
        }
        SPR->inventory[item]--;
      }
  }
  switch (item) {
    case CTM_ITEM_NONE: return 0;
    case CTM_ITEM_SWORD: return ctm_player_swing_sword(spr);
    case CTM_ITEM_SPEECH: return ctm_player_begin_speech(spr);
    case CTM_ITEM_METAL_BLADE: return ctm_player_fire_metal_blade(spr);
    case CTM_ITEM_MAGIC_WAND: return ctm_player_fire_magic_wand(spr);
    case CTM_ITEM_HOOKSHOT: return ctm_player_begin_hookshot(spr);
    case CTM_ITEM_BOOMERANG: return ctm_player_throw_boomerang(spr);
    case CTM_ITEM_AUTOBOX: return ctm_player_begin_autobox(spr);
    case CTM_ITEM_FLAMES: return ctm_player_begin_flames(spr);
    case CTM_ITEM_BOW: return ctm_player_fire_arrow(spr);
    case CTM_ITEM_LENS_OF_TRUTH: return ctm_player_engage_lens_of_truth(spr);
    case CTM_ITEM_COIN: return ctm_player_throw_coin(spr);
  }
  return 0;
}

int ctm_player_end_action(struct ctm_sprite *spr,int item) {
  switch (item) {
    case CTM_ITEM_SWORD: return ctm_player_unswing_sword(spr);
    case CTM_ITEM_SPEECH: return ctm_player_end_speech(spr);
    case CTM_ITEM_HOOKSHOT: return ctm_player_cancel_hookshot(spr);
    case CTM_ITEM_AUTOBOX: return ctm_player_cancel_autobox(spr);
    case CTM_ITEM_FLAMES: return ctm_player_end_flames(spr);
    case CTM_ITEM_LENS_OF_TRUTH: return ctm_player_disengage_lens_of_truth(spr);
  }
  return 0;
}

int ctm_player_abort_actions(struct ctm_sprite *spr) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return -1;
  if (ctm_player_end_action(spr,SPR->item_primary)<0) return -1;
  if (ctm_player_end_action(spr,SPR->item_secondary)<0) return -1;
  if (ctm_player_end_action(spr,SPR->item_tertiary)<0) return -1;
  return 0;
}

/* Item set.
 */
 
int ctm_player_has_item(struct ctm_sprite *spr,int itemid) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return 0;
  if ((itemid<0)||(itemid>16)) return 0;
  if (itemid==SPR->item_primary) return 1;
  if (itemid==SPR->item_secondary) return 1;
  if (itemid==SPR->item_tertiary) return 1;
  int i; for (i=0;i<16;i++) if (itemid==SPR->item_store[i]) return 1;
  return 0;
}

int ctm_player_add_inventory(struct ctm_sprite *spr,int itemid,int addc) {
  if (!ctm_player_has_item(spr,itemid)) return 0;
  if (SPR->inventory[itemid]==CTM_INVENTORY_UNLIMITED) return 0;
  SPR->inventory[itemid]+=addc;
  return 0;
}

int ctm_player_get_treasure(struct ctm_sprite *spr,int item,int quantity) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return -1;

  // Special items...
  if ((item<0)||(item>=16)) switch (item) {
    case CTM_ITEM_PIZZA_FART: {
        SPR->hpmax++;
        SPR->hp++;
      } return 0;
    default: return 0;
  }
  
  if (item==CTM_ITEM_NONE) return 0;
  if (quantity<1) return 0;
  if (SPR->inventory[item]>=0) {
    if (SPR->inventory[item]>999-quantity) SPR->inventory[item]=999;
    else SPR->inventory[item]+=quantity;
  }
  if (ctm_player_has_item(spr,item)) return 0;
  if (SPR->item_primary==CTM_ITEM_NONE) SPR->item_primary=item;
  else if (SPR->item_secondary==CTM_ITEM_NONE) SPR->item_secondary=item;
  else if (SPR->item_tertiary==CTM_ITEM_NONE) SPR->item_tertiary=item;
  else {
    int i; for (i=0;i<16;i++) if (SPR->item_store[i]==CTM_ITEM_NONE) {
      SPR->item_store[i]=item;
      break;
    }
  }
  return 0;
}

/* Spend golds.
 */

int ctm_player_spend(struct ctm_sprite *spr,int price) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return -1;
  if (price>0) {
    if (price>SPR->inventory[CTM_ITEM_COIN]) {
      CTM_SFX(REFUSE)
      return 0;
    } else {
      CTM_SFX(SPEND)
      SPR->inventory[CTM_ITEM_COIN]-=price;
    }
  }
  return 1;
}
