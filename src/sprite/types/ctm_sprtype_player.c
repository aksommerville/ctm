#include "ctm.h"
#include "ctm_sprtype_player.h"
#include "ctm_sprtype_treasure.h"
#include "video/ctm_video.h"
#include "input/ctm_input.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"
#include "ctm_sprtype_voter.h"
#include "display/ctm_display.h"
#include "audio/ctm_audio.h"

#define SPR ((struct ctm_sprite_player*)spr)

// Semi-private functions in ctm_sprtype_player_actions.c (not declared in our header).
void ctm_player_set_initial_items(struct ctm_sprite *spr);
int ctm_player_check_prize_pickup(struct ctm_sprite *spr);
int ctm_player_unswing_sword(struct ctm_sprite *spr);
int ctm_player_end_speech(struct ctm_sprite *spr);
int ctm_player_deal_damage(struct ctm_sprite *spr,int x,int y,int w,int h);

#define CTM_PLAYER_WALK_SPEED (CTM_TILESIZE>>3)

#define CTM_PLAYER_INVINCIBLE_TIME 60

#define CTM_KISS_OF_DEATH_RADIUS (CTM_TILESIZE)
#define CTM_KISS_OF_DEATH_FAVOR 10
#define CTM_WANTED_FAVOR 20

/* Some magic numbers from before resolution flexibility. */
#define CTM_PLAYER_HEAD_OFFSET ((CTM_TILESIZE*13)/16)
#define CTM_PLAYER_HEAD_SWING_DOWN ((CTM_TILESIZE*3)/16)
#define CTM_PLAYER_HEAD_SWING_HORZ ((CTM_TILESIZE*2)/16)
#define CTM_PLAYER_SPEECH_FROM_HEAD ((CTM_TILESIZE*18)/16)
#define CTM_PLAYER_HAZARD_RADIUS ((CTM_TILESIZE*8)/16)
#define CTM_SWORD_WIDTH ((CTM_TILESIZE*8)/16)
#define CTM_SWORD_LENGTH ((CTM_TILESIZE*24)/16)

/* Delete and initialize.
 */

static void _ctm_player_del(struct ctm_sprite *spr) {
  ctm_sprgrp_kill(SPR->holdgrp);
  ctm_sprgrp_del(SPR->holdgrp);
  ctm_sprgrp_kill(SPR->boomerang);
  ctm_sprgrp_del(SPR->boomerang);
}

static int _ctm_player_init(struct ctm_sprite *spr) {

  if (!(SPR->holdgrp=ctm_sprgrp_new(0))) return -1;
  if (!(SPR->boomerang=ctm_sprgrp_new(0))) return -1;

  SPR->col=0x00;
  SPR->row=0x60;
  SPR->poscol=-1;
  SPR->posrow=-1;
  SPR->pvinput=0xffff; // Wait for each button to be released before we ackowledge any presses.

  SPR->hp=SPR->hpmax=5;

  ctm_player_set_initial_items(spr);

  return 0;
}

/* Draw.
 */

static int _ctm_player_draw(struct ctm_sprite *spr,int addx,int addy) {

  if (SPR->hp<1) {
    struct ctm_vertex_sprite *vtx=ctm_add_sprites(1);
    if (!vtx) return -1;
    vtx->x=spr->x+addx;
    vtx->y=spr->y+addy;
    vtx->tile=0x15;
    vtx->a=0xff;
    vtx->ta=0;
    vtx->flop=0;
    return 0;
  }

  int vtxc=2;
  if (SPR->swinging||SPR->speaking||SPR->boxing) vtxc=3;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;

  int headix=1,bodyix=0,swordix=2;
  if (SPR->col==0x01) { 
    headix=0; 
    bodyix=1;
    if (SPR->swinging||SPR->boxing) { headix++; bodyix++; swordix=0; }
  }

  uint32_t rgba=ctm_party_color[SPR->party];
  int i; for (i=0;i<vtxc;i++) {
    vtxv[i].r=rgba>>24;
    vtxv[i].g=rgba>>16;
    vtxv[i].b=rgba>>8;
    vtxv[i].a=0xff;
    vtxv[i].ta=0x00;
  }

  vtxv[bodyix].x=spr->x+addx;
  vtxv[bodyix].y=spr->y+addy;
  vtxv[bodyix].tile=SPR->col|SPR->row;
  vtxv[bodyix].flop=spr->flop;

  vtxv[headix].x=spr->x+addx;
  vtxv[headix].y=spr->y+addy-CTM_PLAYER_HEAD_OFFSET;
  vtxv[headix].tile=0x50|SPR->col;
  vtxv[headix].flop=spr->flop;

  if (SPR->swinging||SPR->boxing) {
    vtxv[swordix].x=spr->x+addx;
    vtxv[swordix].y=spr->y+addy;
    vtxv[swordix].tile=(SPR->boxing?0x83:0xa0)+SPR->col;
    vtxv[swordix].flop=spr->flop;
    switch (SPR->col) {
      case 0x00: vtxv[swordix].y+=CTM_TILESIZE; vtxv[headix].y+=CTM_PLAYER_HEAD_SWING_DOWN; break;
      case 0x01: vtxv[swordix].y-=CTM_TILESIZE; break;
      case 0x02: if (spr->flop) {
          vtxv[swordix].x+=CTM_TILESIZE;
          vtxv[headix].x+=CTM_PLAYER_HEAD_SWING_HORZ;
        } else {
          vtxv[swordix].x-=CTM_TILESIZE;
          vtxv[headix].x-=CTM_PLAYER_HEAD_SWING_HORZ;
        } break;
    }
    
  } else if (SPR->speaking) {
    vtxv[bodyix].tile=SPR->col|0xb0;
    vtxv[swordix].x=spr->x+addx;
    vtxv[swordix].y=vtxv[headix].y-CTM_PLAYER_SPEECH_FROM_HEAD;
    vtxv[swordix].tile=0xd0;
    vtxv[swordix].flop=0;
    
  } else if (SPR->holdgrp->sprc) {
    switch (SPR->col) {
      case 0x00: vtxv[swordix].y+=CTM_TILESIZE; vtxv[headix].y+=CTM_PLAYER_HEAD_SWING_DOWN; break;
      case 0x01: vtxv[swordix].y-=CTM_TILESIZE; break;
      case 0x02: if (spr->flop) {
          vtxv[swordix].x+=CTM_TILESIZE;
          vtxv[headix].x+=CTM_PLAYER_HEAD_SWING_HORZ;
        } else {
          vtxv[swordix].x-=CTM_TILESIZE;
          vtxv[headix].x-=CTM_PLAYER_HEAD_SWING_HORZ;
        } break;
    }
  }

  if (SPR->invincible>0) {
    vtxv[0].tr=0x00;
    vtxv[0].tg=0x00;
    vtxv[0].tb=0x00;
    vtxv[0].ta=(SPR->invincible>30)?0xff:((SPR->invincible*255)/30);
    int i; for (i=1;i<vtxc;i++) {
      vtxv[i].tr=vtxv[0].tr;
      vtxv[i].tg=vtxv[0].tg;
      vtxv[i].tb=vtxv[0].tb;
      vtxv[i].ta=vtxv[0].ta;
    }
  }
  
  return 0;
}

/* Add to wanted level.
 */
 
int ctm_player_add_wanted(struct ctm_sprite *spr,int amount) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return -1;
  if (amount<1) return 0;
  int stateix=ctm_state_for_pixel(spr->x,spr->y);
  if (SPR->wanted[stateix]>=CTM_WANTED_MAX) return 0;
  if (SPR->wanted[stateix]>=CTM_WANTED_MAX-amount) { // Cross max threshhold.
    CTM_SFX(WANTED)
    SPR->wanted[stateix]=CTM_WANTED_MAX;
    if (ctm_perform_broad_event(stateix,-1,SPR->party,-CTM_WANTED_FAVOR)<0) return -1;
  } else { // More wanted, but still under the threshhold.
    SPR->wanted[stateix]+=amount;
  }
  return 0;
}

/* Moved to new cell.
 */

static int ctm_player_leaving_cell(struct ctm_sprite *spr) {
  if ((SPR->poscol<0)||(SPR->poscol>=ctm_grid.colc)) return 0;
  if ((SPR->posrow<0)||(SPR->posrow>=ctm_grid.rowc)) return 0;
  return 0;
}

static int ctm_player_entering_cell(struct ctm_sprite *spr) {

  /* What are we looking at exactly? */
  if ((SPR->poscol<0)||(SPR->poscol>=ctm_grid.colc)) return 0;
  if ((SPR->posrow<0)||(SPR->posrow>=ctm_grid.rowc)) return 0;
  uint8_t tile;
  uint32_t prop;
  if (spr->interior) {
    tile=ctm_grid.cellv[SPR->posrow*ctm_grid.colc+SPR->poscol].itile;
    prop=ctm_tileprop_interior[tile];
  } else {
    tile=ctm_grid.cellv[SPR->posrow*ctm_grid.colc+SPR->poscol].tile;
    prop=ctm_tileprop_exterior[tile];
  }

  /* Fountain of youth? */
  if (prop&CTM_TILEPROP_YOUTH) {
    if (SPR->hp<SPR->hpmax) {
      CTM_SFX(HEAL)
      if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
      SPR->hp=SPR->hpmax;
      if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) return -1;
      if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) return -1;
      if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) return -1;
    }
  }

  /* Toggle interior and exterior? */
  if (prop&CTM_TILEPROP_DOOR) {
    CTM_SFX(DOOR)
    if (spr->interior) {
      if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) return -1;
      if (ctm_sprite_remove_group(spr,&ctm_group_interior)<0) return -1;
      spr->interior=0;
    } else {
      if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return -1;
      if (ctm_sprite_remove_group(spr,&ctm_group_exterior)<0) return -1;
      spr->interior=1;
    }
  }
  
  return 0;
}

/* Collision test.
 */

static int ctm_player_test_collision(struct ctm_sprite *spr,const struct ctm_bounds *bounds,int leap) {

  /* Check world edges. */
  if (bounds->l<0) return 1;
  if (bounds->t<0) return 1;
  if (bounds->r>ctm_grid.colc*CTM_TILESIZE) return 1;
  if (bounds->b>ctm_grid.rowc*CTM_TILESIZE) return 1;

  /* Check grid cells. */
  int cola=bounds->l; if (cola<0) cola=0; cola/=CTM_TILESIZE;
  int colz=bounds->r-1; colz/=CTM_TILESIZE; if (colz>=ctm_grid.colc) colz=ctm_grid.colc-1;
  int rowa=bounds->t; if (rowa<0) rowa=0; rowa/=CTM_TILESIZE;
  int rowz=bounds->b-1; rowz/=CTM_TILESIZE; if (rowz>=ctm_grid.rowc) rowz=ctm_grid.rowc-1;
  uint32_t collprop=CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS;
  if (SPR->hp&&!leap) collprop|=CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE;
  if ((cola<=colz)&&(rowa<=rowz)) {
    int col; for (col=cola;col<=colz;col++) {
      int row; for (row=rowa;row<=rowz;row++) {
        uint8_t tile;
        uint32_t prop;
        if (spr->interior) {
          tile=ctm_grid.cellv[row*ctm_grid.colc+col].itile;
          prop=ctm_tileprop_interior[tile];
        } else {
          tile=ctm_grid.cellv[row*ctm_grid.colc+col].tile;
          prop=ctm_tileprop_exterior[tile];
        }
        if (prop&collprop) return 1;
      }
    }
  }

  return 0;
}

/* Move.
 */

int ctm_player_move(struct ctm_sprite *spr,int dx,int dy,int leap) {

  /* Move one axis at a time. */
  if (dx&&dy) {
    int rdx=ctm_player_move(spr,dx,0,leap); if (rdx<0) return -1;
    int rdy=ctm_player_move(spr,0,dy,leap); if (rdy<0) return -1;
    return rdx+rdy;
  }
  if (!dx&&!dy) return 0;

  /* Get old and new physical boundaries.
   * Test the difference, and reenter if it collides with something.
   */
  struct ctm_bounds obounds=(struct ctm_bounds){spr->x+CTM_PLAYER_PHLEFT,spr->x+CTM_PLAYER_PHRIGHT,spr->y+CTM_PLAYER_PHTOP,spr->y+CTM_PLAYER_PHBOTTOM};
  struct ctm_bounds nbounds=(struct ctm_bounds){obounds.l+dx,obounds.r+dx,obounds.t+dy,obounds.b+dy};
  struct ctm_bounds qbounds;
  if (dx<0) {
    qbounds=(struct ctm_bounds){nbounds.l,obounds.l,nbounds.t,nbounds.b};
    if (ctm_player_test_collision(spr,&qbounds,leap)) return ctm_player_move(spr,dx+1,0,leap);
  } else if (dx>0) {
    qbounds=(struct ctm_bounds){obounds.r,nbounds.r,nbounds.t,nbounds.b};
    if (ctm_player_test_collision(spr,&qbounds,leap)) return ctm_player_move(spr,dx-1,0,leap);
  } else if (dy<0) {
    qbounds=(struct ctm_bounds){nbounds.l,nbounds.r,nbounds.t,obounds.t};
    if (ctm_player_test_collision(spr,&qbounds,leap)) return ctm_player_move(spr,0,dy+1,leap);
  } else {
    qbounds=(struct ctm_bounds){nbounds.l,nbounds.r,obounds.b,nbounds.b};
    if (ctm_player_test_collision(spr,&qbounds,leap)) return ctm_player_move(spr,0,dy-1,leap);
  }

  /* Apply change and examine new position. */
  spr->x+=dx;
  spr->y+=dy;
  
  int col=spr->x/CTM_TILESIZE,row=spr->y/CTM_TILESIZE;
  if ((col!=SPR->poscol)||(row!=SPR->posrow)) {
    if (ctm_player_leaving_cell(spr)<0) return -1;
    SPR->poscol=col;
    SPR->posrow=row;
    if (ctm_player_entering_cell(spr)<0) return -1;
  }
  
  return (dx<0)?-dx:(dx>0)?dx:(dy<0)?-dy:dy;
}

/* Move failed; tweak towards cell alignment.
 */

static int ctm_player_correct_position(struct ctm_sprite *spr) {
  int mid=CTM_TILESIZE>>1;
  if (SPR->dx) {
    int off=spr->y%CTM_TILESIZE;
    if (off<mid) return ctm_player_move(spr,0,1,0);
    if (off>mid) return ctm_player_move(spr,0,-1,0);
  } else if (SPR->dy) {
    int off=spr->x%CTM_TILESIZE;
    if (off<mid) return ctm_player_move(spr,1,0,0);
    if (off>mid) return ctm_player_move(spr,-1,0,0);
  }
  return 0;
}

/* Change movement.
 */

int ctm_player_set_motion(struct ctm_sprite *spr,int dx,int dy) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return -1;

  // Is walking permitted in current state?
  if (SPR->swinging) return 0;
  if (SPR->speaking) return 0;
  if (SPR->holdgrp->sprc) return 0;
  if (SPR->grabbed) return 0;
  if (SPR->boxing) return 0;
  
  SPR->animcounter=SPR->animphase=0;
  if (dx) {
    SPR->dx=dx;
    SPR->col=0x02;
    if (dx<0) spr->flop=0;
    else spr->flop=1;
  } else if (dy) {
    SPR->dy=dy;
    spr->flop=0;
    if (dy<0) SPR->col=0x01;
    else SPR->col=0x00;
  }
  return 0;
}

int ctm_player_unset_motion(struct ctm_sprite *spr,int dx,int dy) {
  if (!spr||(spr->type!=&ctm_sprtype_player)) return -1;
  if (dx&&(dx==SPR->dx)) {
    SPR->dx=0;
    if (SPR->dy<0) { spr->flop=0; SPR->col=0x01; }
    else if (SPR->dy>0) { spr->flop=0; SPR->col=0x00; }
  } else if (dy&&(dy==SPR->dy)) {
    SPR->dy=0;
    if (SPR->dx<0) { spr->flop=0; SPR->col=0x02; }
    else if (SPR->dx>0) { spr->flop=1; SPR->col=0x02; }
  }
  return 0;
}

/* Input for pause menu.
 */

static int ctm_player_move_pause(struct ctm_sprite *spr,int dx,int dy) {
  struct ctm_display *display=ctm_display_for_playerid(SPR->playerid);
  if (!display) return 0;
  CTM_SFX(MENU_CHANGE)
  if (ctm_display_game_move_cursor(display,dx,dy)<0) return -1;
  return 0;
}

static int ctm_player_swap_pause(struct ctm_sprite *spr,int *addr) {
  struct ctm_display *display=ctm_display_for_playerid(SPR->playerid);
  if (!display) return 0;
  int ix=ctm_display_game_get_selected_index(display);
  if (ix<0) return 0;
  if (ix<16) {
    CTM_SFX(SWAPITEM)
    int tmp=SPR->item_store[ix];
    SPR->item_store[ix]=*addr;
    *addr=tmp;
  } else switch (ix) {
    case CTM_DISPLAY_GAME_INDEX_VOTE: if (SPR->wants_termination=!SPR->wants_termination) {
        CTM_SFX(TRYVOTE)
      } else {
        CTM_SFX(UNTRYVOTE)
      } break;
  }
  return 0;
}

static int ctm_player_pause(struct ctm_sprite *spr) {

  // Cancel anything in progress.
  if (SPR->dx&&(ctm_player_unset_motion(spr,SPR->dx,0)<0)) return -1;
  if (SPR->dy&&(ctm_player_unset_motion(spr,0,SPR->dy)<0)) return -1;
  if (ctm_player_abort_actions(spr)<0) return -1;

  if (SPR->pause=!SPR->pause) { // Enter menu...
    CTM_SFX(PAUSE)
    if (ctm_update_state_prediction()<0) return -1;
    ctm_display_game_rebuild_report(ctm_display_for_playerid(SPR->playerid));
  } else { // Exit menu...
    CTM_SFX(UNPAUSE)
    SPR->wants_termination=0;
  }

  return 0;
}

/* Kiss of death.
 */

static int ctm_player_check_kiss_of_death(struct ctm_sprite *spr) {
  int voterc=ctm_perform_local_event(spr->x,spr->y,spr->interior,CTM_KISS_OF_DEATH_RADIUS,SPR->party,CTM_KISS_OF_DEATH_FAVOR);
  if (voterc<0) return -1;
  if (voterc>0) {
    CTM_SFX(KISSOFDEATH);
  }
  return 0;
}

/* We are wanted by the police -- decide whether to add a new cop sprite.
 */

static int ctm_player_update_fugitive(struct ctm_sprite *spr) {
  if (SPR->copdelay>0) SPR->copdelay--; else {
    SPR->copdelay=60;
    int copc=0,i; for (i=0;i<ctm_group_cop.sprc;i++) {
      struct ctm_sprite *cop=ctm_group_cop.sprv[i];
      int dx=spr->x-cop->x,dy=spr->y-cop->y;
      int distance=((dx<0)?-dx:dx)+((dy<0)?-dy:dy);
      if (distance<CTM_TILESIZE*20) copc++;
    }
    if (copc>=20) return 0; // Plenty of cops, don't create more.
    struct ctm_sprite *cop=ctm_sprite_cop_new(spr);
    if (!cop) return 0; // Making a cop can fail randomly, ignore it.
  }
  return 0;
}

/* Update.
 */

static int _ctm_player_update(struct ctm_sprite *spr) {
  int i;

  /* Drop invincibility and wanted level. */
  if (SPR->invincible>0) SPR->invincible--;
  for (i=0;i<9;i++) if ((SPR->wanted[i]>0)&&(SPR->wanted[i]<CTM_WANTED_MAX)) {
    if ((SPR->wanted[i]-=CTM_WANTED_DECAY)<0) SPR->wanted[i]=0;
  }
  if (SPR->wanted[ctm_state_for_pixel(spr->x,spr->y)]>=CTM_WANTED_MAX) {
    if (ctm_player_update_fugitive(spr)<0) return -1;
  } else {
    SPR->copdelay=0;
  }
  if (SPR->pause) SPR->pause_framec++;

  /* Check for input changes. */
  uint16_t ninput=0;
  if ((SPR->playerid>=1)&&(SPR->playerid<=4)) ninput=ctm_input_by_playerid[SPR->playerid];
  if (ninput!=SPR->pvinput) {
    uint16_t oinput=SPR->pvinput;
    SPR->pvinput=ninput;
    #define PRESS(tag) ((ninput&CTM_BTNID_##tag)&&!(oinput&CTM_BTNID_##tag))
    #define RELEASE(tag) (!(ninput&CTM_BTNID_##tag)&&(oinput&CTM_BTNID_##tag))

    if (SPR->pause) {
      if (PRESS(UP)) { if (ctm_player_move_pause(spr,0,-1)<0) return -1; }
      if (PRESS(DOWN)) { if (ctm_player_move_pause(spr,0,1)<0) return -1; }
      if (PRESS(LEFT)) { if (ctm_player_move_pause(spr,-1,0)<0) return -1; }
      if (PRESS(RIGHT)) { if (ctm_player_move_pause(spr,1,0)<0) return -1; }
      if (PRESS(PRIMARY)) { if (ctm_player_swap_pause(spr,&SPR->item_primary)<0) return -1; }
      if (PRESS(SECONDARY)) { if (ctm_player_swap_pause(spr,&SPR->item_secondary)<0) return -1; }
      if (PRESS(TERTIARY)) { if (ctm_player_swap_pause(spr,&SPR->item_tertiary)<0) return -1; }

    } else { 
             if (PRESS(UP   )) { if (ctm_player_set_motion(  spr,0,-1)<0) return -1; }
      else if (RELEASE(UP   )) { if (ctm_player_unset_motion(spr,0,-1)<0) return -1; }
             if (PRESS(DOWN )) { if (ctm_player_set_motion(  spr,0, 1)<0) return -1; }
      else if (RELEASE(DOWN )) { if (ctm_player_unset_motion(spr,0, 1)<0) return -1; }
             if (PRESS(LEFT )) { if (ctm_player_set_motion(  spr,-1,0)<0) return -1; }
      else if (RELEASE(LEFT )) { if (ctm_player_unset_motion(spr,-1,0)<0) return -1; }
             if (PRESS(RIGHT)) { if (ctm_player_set_motion(  spr, 1,0)<0) return -1; }
      else if (RELEASE(RIGHT)) { if (ctm_player_unset_motion(spr, 1,0)<0) return -1; }

             if (PRESS(PRIMARY  )) { if (ctm_player_begin_action(spr,SPR->item_primary)<0) return -1; }
      else if (RELEASE(PRIMARY  )) { if (ctm_player_end_action(  spr,SPR->item_primary)<0) return -1; }
             if (PRESS(SECONDARY)) { if (ctm_player_begin_action(spr,SPR->item_secondary)<0) return -1; }
      else if (RELEASE(SECONDARY)) { if (ctm_player_end_action(  spr,SPR->item_secondary)<0) return -1; }
             if (PRESS(TERTIARY )) { if (ctm_player_begin_action(spr,SPR->item_tertiary)<0) return -1; }
      else if (RELEASE(TERTIARY )) { if (ctm_player_end_action(  spr,SPR->item_tertiary)<0) return -1; }
    }

    if (PRESS(PAUSE)) { if (ctm_player_pause(spr)<0) return -1; }

    #undef PRESS
    #undef RELEASE
  }

  /* Apply motion and animate. */
  if (SPR->dx||SPR->dy) {
    if (--(SPR->animcounter)<=0) {
      SPR->animcounter=8;
      if (++(SPR->animphase)>=4) SPR->animphase=0;
      switch (SPR->animphase) {
        case 0: SPR->row=0x60; break;
        case 1: SPR->row=0x70; break;
        case 2: SPR->row=0x60; break;
        case 3: SPR->row=0x80; break;
      }
    }
    int err=ctm_player_move(spr,SPR->dx*CTM_PLAYER_WALK_SPEED,SPR->dy*CTM_PLAYER_WALK_SPEED,0);
    if (err<0) return -1;
    if (!err&&(ctm_player_correct_position(spr)<0)) return -1;
  } else if (SPR->swinging) {
    if (!--(SPR->swinging)) {
      if (ctm_player_unswing_sword(spr)<0) return -1;
    }
  } else if (SPR->boxing) {
    if (!--(SPR->boxing)) {
      if (ctm_player_end_action(spr,CTM_ITEM_AUTOBOX)<0) return -1;
    }
  } else if (SPR->speaking) {
    if (!--(SPR->speaking)) {
      if (ctm_player_end_speech(spr)<0) return -1;
    }
  } else if (SPR->holdgrp->sprc) {
  } else {
    SPR->row=0x60;
    SPR->animphase=0;
  }

  /* Check for damage from hazards. */
  if (!SPR->invincible) {
    int i; for (i=0;i<ctm_group_hazard.sprc;i++) {
      struct ctm_sprite *hazard=ctm_group_hazard.sprv[i];
						if (hazard->interior!=spr->interior) continue;
      int dx=hazard->x-spr->x;
      if ((dx<-CTM_PLAYER_HAZARD_RADIUS)||(dx>CTM_PLAYER_HAZARD_RADIUS)) continue;
      int dy=hazard->y-spr->y;
      if ((dy<-CTM_PLAYER_HAZARD_RADIUS)||(dy>CTM_PLAYER_HAZARD_RADIUS)) continue;
      if (ctm_sprite_hurt(spr,hazard)<0) return -1;
      break;
    }
    if (!SPR->holdgrp->sprc&&!SPR->grabbed) {
      uint32_t prop=ctm_grid_tileprop_for_pixel(spr->x,spr->y,spr->interior);
      if (prop&(CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE)) {
        if (ctm_sprite_hurt(spr,0)<0) return -1;
      }
    }
  }

  /* Deal damage from sword. */
  if (SPR->swinging) {
    switch (SPR->col) {
      case 0x00: if (ctm_player_deal_damage(spr,spr->x-(CTM_SWORD_WIDTH>>1),spr->y,CTM_SWORD_WIDTH,CTM_SWORD_LENGTH)<0) return -1; break;
      case 0x01: if (ctm_player_deal_damage(spr,spr->x-(CTM_SWORD_WIDTH>>1),spr->y-CTM_SWORD_LENGTH,CTM_SWORD_WIDTH,CTM_SWORD_LENGTH)<0) return -1; break;
      case 0x02: if (spr->flop) {
          if (ctm_player_deal_damage(spr,spr->x,spr->y-(CTM_SWORD_WIDTH>>1),CTM_SWORD_LENGTH,CTM_SWORD_WIDTH)<0) return -1;
        } else {
          if (ctm_player_deal_damage(spr,spr->x-CTM_SWORD_LENGTH,spr->y-(CTM_SWORD_WIDTH>>1),CTM_SWORD_LENGTH,CTM_SWORD_WIDTH)<0) return -1;
        } break;
    }
  }

  /* Pick up prizes. */
  if (ctm_player_check_prize_pickup(spr)<0) return -1;

  /* If we are a ghost, deliver the Kiss Of Death. */
  if (!SPR->hp&&(ctm_player_check_kiss_of_death(spr)<0)) return -1;
  
  return 0;
}

/* Grab.
 */

static int _ctm_player_grab(struct ctm_sprite *spr,int grab) {
  if (SPR->grabbed=grab) {
    if (ctm_player_unset_motion(spr,SPR->dx,0)<0) return -1;
    if (ctm_player_unset_motion(spr,0,SPR->dy)<0) return -1;
    if (ctm_player_end_action(spr,CTM_ITEM_HOOKSHOT)<0) return -1;
  } else {
  }
  return 0;
}

/* Hurt.
 */

static int _ctm_player_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
  if (SPR->hp<1) return 0;
  SPR->strikes++;

  // Assailant is also a player.
  if (assailant&&(assailant->type==&ctm_sprtype_player)) {
    struct ctm_sprite_player *ASSAILANT=(struct ctm_sprite_player*)assailant;
    int party=((struct ctm_sprite_player*)assailant)->party;
    int favor=-CTM_MURDER_WITNESS_FAVOR;
    int err=ctm_perform_local_event(spr->x,spr->y,spr->interior,CTM_MURDER_WITNESS_RADIUS,party,favor);
    if (err<0) return -1;
    if (SPR->hp==1) { // final blow
      if (ctm_player_create_prize(assailant,spr)<0) return -1;
      if (ctm_player_add_wanted(assailant,CTM_WANTED_INCREMENT)<0) return -1;
      ASSAILANT->mummykills++;
    }
  }
  
  if (!--(SPR->hp)) {
    // Player killed.
    SPR->deaths++;
    ctm_audio_effect(CTM_AUDIO_EFFECTID_PLAYER_KILLED,0xff);
    if (ctm_sprite_create_gore(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_remove_group(spr,&ctm_group_hookable)<0) return -1;
    if (ctm_sprite_remove_group(spr,&ctm_group_bonkable)<0) return -1;
    if (ctm_sprite_remove_group(spr,&ctm_group_fragile)<0) return -1;
  } else {
    // Player hurt but not killed.
    ctm_audio_effect(CTM_AUDIO_EFFECTID_PLAYER_HURT,0xff);
    SPR->invincible=60;
  }
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_player={
  .name="player",
  .objlen=sizeof(struct ctm_sprite_player),

  .soul=1,
  .guts=1,
  
  .del=_ctm_player_del,
  .init=_ctm_player_init,
  .draw=_ctm_player_draw,
  .update=_ctm_player_update,
  .grab=_ctm_player_grab,
  .hurt=_ctm_player_hurt,
};
