#include "ctm.h"
#include "ctm_geography.h"
#include "ctm_grid.h"
#include "ctm_game.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_player.h"
#include <math.h>

/* Party data.
 */

const uint32_t ctm_party_color[7]={
  0x808080ff,
  0xe0e0e0ff,
  0x202020ff,
  0xffff00ff,
  0x008000ff,
  0x4040ffff,
  0xff0000ff,
};

const char *ctm_party_name[7]={
  "Everyone",
  "White",
  "Black",
  "Yellow",
  "Green",
  "Blue",
  "Red",
};

const char *CTM_PARTY_NAME[7]={
  "EVERYONE",
  "WHITE",
  "BLACK",
  "YELLOW",
  "GREEN",
  "BLUE",
  "RED",
};

/* State data.
 */

struct ctm_state_data ctm_state_data[9]={

  {"Alaska","ALASKA","AK",CTM_PARTY_WHITE,5},
  {"Wisconsin","WISCONSIN","WI",CTM_PARTY_YELLOW,8},
  {"Vermont","VERMONT","VT",CTM_PARTY_GREEN,5},
  {"California","CALIFORNIA","CA",0,10},
  {"Kansas","KANSAS","KS",0,8},
  {"Virginia","VIRGINIA","VA",CTM_PARTY_RED,9},
  {"Hawaii","HAWAII","HI",CTM_PARTY_BLUE,6},
  {"Texas","TEXAS","TX",CTM_PARTY_BLACK,10},
  {"Florida","FLORIDA","FL",0,9},

};

// 2010 census:
// CA 37253956   10
// TX 25145561   10
// FL 18801310    9
// VA  8001024    9
// WI  5686986    8
// KS  2853118    8
// HI  1360301    6
// AK   710231    5
// VT   625741    5
//...it didn't occur to me before, that these are really in different ballparks.
// So, the populations will not be proportionate to real life.
// They will at least preserve the correct order.

/* Apportion population.
 */
 
int ctm_apportion_population(int *dstv,int total) {
  if (!dstv||(total<100)||(total>100000)) return -1;
  int i;

  // First pass simply multiplies states' relative population by a floor unit size.
  int unitc=0;
  for (i=0;i<9;i++) unitc+=ctm_state_data[i].population;
  int unit=total/unitc;
  for (i=0;i<9;i++) {
    dstv[i]=unit*ctm_state_data[i].population;
    total-=dstv[i];
  }

  // Let the dogs fight over whatever is left in (total)...
  int slice=total/9;
  for (i=0;i<9;i++) dstv[i]+=slice;
  total%=9;
  while (total-->0) dstv[total]++;

  return 0;
}

/* What state is this?
 * aka Low-Resolution GPS.
 */

int ctm_state_for_cell(int col,int row) {
  col=(col*3)/ctm_grid.colc; if (col<0) col=0; else if (col>=3) col=2;
  row=(row*3)/ctm_grid.rowc; if (row<0) row=0; else if (row>=3) row=2;
  return row*3+col;
}
 
int ctm_state_for_pixel(int x,int y) {
  return ctm_state_for_cell(x/CTM_TILESIZE,y/CTM_TILESIZE);
}

/* Choose a vacant location.
 */

static int ctm_location_occupied(struct ctm_sprgrp *grp,int x,int y) {
  if (!grp) return 0;
  int i; for (i=0;i<grp->sprc;i++) {
    struct ctm_sprite *spr=grp->sprv[i];
    if (spr->x!=x) continue;
    if (spr->y!=y) continue;
    return 1;
  }
  return 0;
}
 
void ctm_choose_vacant_location(int *x,int *y,int *interior,int stateix,int interior_ok) {
  int _x; if (!x) x=&_x;
  int _y; if (!y) y=&_y;
  int _interior; if (!interior) interior=&_interior;
  if ((stateix<0)||(stateix>8)) stateix=rand()%9;

  // Choose state boundaries, in cells.
  int xp=((stateix%3)*ctm_grid.colc)/3;
  int xc=((stateix%3+1)*ctm_grid.colc)/3-xp;
  int yp=((stateix/3)*ctm_grid.rowc)/3;
  int yc=((stateix/3+1)*ctm_grid.rowc)/3-yp;
  if (xc<1) xc=1;
  if (yc<1) yc=1;
  int cellc=xc*yc;

  // Try 100 times to pick a cell entirely at random.
  int panic=100; while (panic-->0) {
    int col=xp+rand()%xc;
    int row=yp+rand()%yc;
    *x=col*CTM_TILESIZE+(CTM_TILESIZE>>1);
    *y=row*CTM_TILESIZE+(CTM_TILESIZE>>1);
    if (ctm_location_is_vacant(col,row,0)) {
      *interior=0;
      return;
    }
    if (interior_ok&&ctm_location_is_vacant(col,row,1)) {
      *interior=1;
      return;
    }
  }

  // Search top to bottom and return the first vacant cell.
  int yi; for (yi=0;yi<yc;yi++) {
    *y=(yp+yi)*CTM_TILESIZE+(CTM_TILESIZE>>1);
    int xi; for (xi=0;xi<xc;xi++) {
      int cellp=(yp+yi)*ctm_grid.colc+xp+xi;
      *x=(xp+xi)*CTM_TILESIZE+(CTM_TILESIZE>>1);
      if (!ctm_tileprop_exterior[ctm_grid.cellv[cellp].tile]&&!ctm_location_occupied(&ctm_group_exterior,*x,*y)) { *interior=0; return; }
      if (interior_ok) {
        if (!ctm_tileprop_interior[ctm_grid.cellv[cellp].itile]&&!ctm_location_occupied(&ctm_group_interior,*x,*y)) { *interior=1; return; }
      }
    }
  }

  // Seriously? OK, return the exterior cell in the middle, occupied or not.
  *x=xp+(xc>>1); *x*=CTM_TILESIZE; *x+=CTM_TILESIZE>>1;
  *y=yp+(yc>>1); *y*=CTM_TILESIZE; *y+=CTM_TILESIZE>>1;
  *interior=0;
}

int ctm_location_is_vacant(int x,int y,int interior) {
  if ((x<0)||(x>=ctm_grid.colc)) return 0;
  if ((y<0)||(y>=ctm_grid.rowc)) return 0;
  int cellp=y*ctm_grid.colc+x;
  if (interior) {
    if (ctm_tileprop_interior[ctm_grid.cellv[cellp].itile]) return 0;
    if (ctm_location_occupied(&ctm_group_interior,x*CTM_TILESIZE+(CTM_TILESIZE>>1),y*CTM_TILESIZE+(CTM_TILESIZE>>1))) return 0;
  } else {
    if (ctm_tileprop_exterior[ctm_grid.cellv[cellp].tile]) return 0;
    if (ctm_location_occupied(&ctm_group_exterior,x*CTM_TILESIZE+(CTM_TILESIZE>>1),y*CTM_TILESIZE+(CTM_TILESIZE>>1))) return 0;
  }
  return 1;
}

/* Conduct poll.
 */
 
int ctm_conduct_poll(struct ctm_poll_result *resultv,int stateix,int8_t threshhold) {
  if (!resultv) return -1;
  if ((threshhold==-128)||(threshhold==0)) return -1;
  memset(resultv,0,sizeof(struct ctm_poll_result)*7);
  struct ctm_sprgrp *grp;
  if ((stateix>=0)&&(stateix<=8)) grp=ctm_group_state_voter+stateix;
  else grp=&ctm_group_voter;
  int8_t lo,hi;
  if (threshhold<0) { lo=threshhold; hi=-threshhold; }
  else { lo=-threshhold; hi=threshhold; }
  int i; for (i=0;i<grp->sprc;i++) {
    struct ctm_sprite *spr=grp->sprv[i];
    if (spr->type!=&ctm_sprtype_voter) continue; // Should not be in a 'voter' group, but ok...
    struct ctm_sprite_voter *SPR=(struct ctm_sprite_voter*)spr;
    
    if (SPR->decision<=lo) resultv[0].bluec++;
    else if (SPR->decision>=hi) resultv[0].redc++;
    else if (SPR->decision<0) resultv[0].subbluec++;
    else if (SPR->decision>0) resultv[0].subredc++;
    else resultv[0].zeroc++;
    resultv[0].total++;
    
    if ((SPR->party<1)||(SPR->party>6)) continue;
    if (SPR->decision<=lo) resultv[SPR->party].bluec++;
    else if (SPR->decision>=hi) resultv[SPR->party].redc++;
    else if (SPR->decision<0) resultv[SPR->party].subbluec++;
    else if (SPR->decision>0) resultv[SPR->party].subredc++;
    else resultv[SPR->party].zeroc++;
    resultv[SPR->party].total++;
    
  }
  return 0;
}

/* Make state-by-state predictions.
 */
 
int ctm_state_prediction[9]={0};

int ctm_update_state_prediction() {
  int stateix; for (stateix=0;stateix<9;stateix++) {
    int bluec=0,redc=0,subbluec=0,subredc=0,count=0;
    int spri; for (spri=0;spri<ctm_group_state_voter[stateix].sprc;spri++) {
      struct ctm_sprite *spr=ctm_group_state_voter[stateix].sprv[spri];
      if (spr->type!=&ctm_sprtype_voter) continue;
      struct ctm_sprite_voter *SPR=(struct ctm_sprite_voter*)spr;
      if (SPR->decision<=-10) bluec++;
      else if (SPR->decision>=10) redc++;
      else if (SPR->decision<0) subbluec++;
      else if (SPR->decision>0) subredc++;
      count++;
    }
    if (count<1) ctm_state_prediction[stateix]=0; // No quorum == undecided.
    else if (bluec+redc+subbluec+subredc<=count>>1) ctm_state_prediction[stateix]=0; // At least half of voters undecided == state undecided.
    else if (bluec+subbluec>=(redc+subredc)<<1) ctm_state_prediction[stateix]=-2; // Strong+lean of one color doubles the other == strong.
    else if (redc+subredc>=(bluec+subbluec)<<1) ctm_state_prediction[stateix]=2; // "
    else if (bluec+subbluec>count>>1) ctm_state_prediction[stateix]=-1; // Strong+lean of one color is a majority == lean.
    else if (redc+subredc>count>>1) ctm_state_prediction[stateix]=1; // "
    else ctm_state_prediction[stateix]=0; // Too close to call.
  }
  return 0;
}

/* Conduct election.
 */
 
int ctm_conduct_election(struct ctm_election_result *result) {
  if (!result) return -1;
  memset(result,0,sizeof(struct ctm_election_result));
  int *statevotes=result->brokendown;
  int stateix; for (stateix=0;stateix<9;stateix++,statevotes+=14) {
    int spri; for (spri=0;spri<ctm_group_state_voter[stateix].sprc;spri++) {
      struct ctm_sprite *spr=ctm_group_state_voter[stateix].sprv[spri];
      if (spr->type!=&ctm_sprtype_voter) continue;
      int party=((struct ctm_sprite_voter*)spr)->party;
      int decision=((struct ctm_sprite_voter*)spr)->decision;
      result->population++;
      result->favor+=decision;
      if (!decision) { // Everyone must vote. If we rewrite this, commit it also to the sprite, so we get the same result next time.
        decision=(rand()&1)?1:-1;
        ((struct ctm_sprite_voter*)spr)->decision=decision;
      }
      if (decision<0) {
        result->totalblue++;
        result->favorblue+=decision;
        statevotes[0]++;
        if ((party>=1)&&(party<=6)) statevotes[(party<<1)+0]++;
      } else {
        result->totalred++;
        result->favorred+=decision;
        statevotes[1]++;
        if ((party>=1)&&(party<=6)) statevotes[(party<<1)+1]++;
      }
    }
    if (statevotes[0]>statevotes[1]) result->elecblue++;
    else if (statevotes[0]<statevotes[1]) result->elecred++;
  }
  return 0;
}

/* Global sentiment poll.
 */
 
void ctm_poll_total_sentiment(int *sentiment,int *count) {
  if (count) *count=ctm_group_voter.sprc;
  if (sentiment) {
    *sentiment=0;
    int i; for (i=0;i<ctm_group_voter.sprc;i++) {
      (*sentiment)+=((struct ctm_sprite_voter*)ctm_group_voter.sprv[i])->decision;
    }
  }
}

/* Apply favor to multiple voters.
 */

int ctm_perform_broad_event(int target_stateix,int target_party,int party,int favor) {
  struct ctm_sprgrp *grp;
  if ((target_stateix>=0)&&(target_stateix<9)) grp=ctm_group_state_voter+target_stateix;
  else grp=&ctm_group_voter;
  int affectc=0;
  int i; for (i=0;i<grp->sprc;i++) {
    struct ctm_sprite *spr=grp->sprv[i];
    if (!spr||(spr->type!=&ctm_sprtype_voter)) continue;
    struct ctm_sprite_voter *SPR=(struct ctm_sprite_voter*)spr;
    if ((target_party<0)||(target_party>6)||(SPR->party==target_party)) {
      int err=ctm_voter_favor(spr,party,favor);
      if (err<0) return -1;
      if (err) affectc++;
    }
  }
  return affectc;
}

int ctm_perform_local_event(int x,int y,int interior,int radius,int party,int favor) {
  if (radius<1) return 0;
  struct ctm_sprgrp *grp;
  int l=x-radius,t=y-radius,r=x+radius,b=y+radius;
  int stateix=ctm_state_for_pixel(l,t);
  if (stateix!=ctm_state_for_pixel(r,b)) { // Far corners of affected range are in different states -- must use full group.
    grp=&ctm_group_voter;
  } else {
    grp=ctm_group_state_voter+stateix;
  }
  interior=interior?1:0;
  int affectc=0;
  int outerradius=(int)(radius*M_SQRT2);
  int i; for (i=0;i<grp->sprc;i++) {
    struct ctm_sprite *spr=grp->sprv[i];
    if (!spr||(spr->type!=&ctm_sprtype_voter)) continue;
    if (spr->interior!=interior) continue;
    struct ctm_sprite_voter *SPR=(struct ctm_sprite_voter*)spr;
    if (SPR->numb) continue; // Distance can be a little expensive, so check numbness early.
    int dx=spr->x-x; if (dx<0) dx=-dx;
    if (dx>=radius) continue; // Too far, easy.
    int dy=spr->y-y; if (dy<0) dy=-dy;
    if (dy>=radius) continue; // Too far, easy.
    int distance=dx+dy;
    if (distance>=outerradius) continue; // Manhattan further than root 2, definitely outside.
    if (distance>radius) { // In diagonal slice, must check geometric distance.
      // FWIW, we could just not do this test, and probably no one would notice the difference.
      distance=(int)sqrt((double)dx*(double)dx+(double)dy*(double)dy);
      if (distance>radius) continue;
    }
    int err=ctm_voter_favor(spr,party,favor);
    if (err<0) return -1;
    if (err) affectc++;
  }
  return affectc;
}

/* Award names.
 */

const char *ctm_award_name[16]={
  "Participation Award",
  "Most Deadly",
  "Most Dishonorable",
  "Big Game Hunter",
  "Dead Meat",
  "Most Talkative",
  "Man of the People",
  "Media Savvy",
  "Punching Bag",
  "Unkillable",
  "Coin Hoarder",
  "Most Informed",
  "Most Active",
  "Most Wanted",
  "AWARD FOURTEEN",
  "AWARD FIFTEEN",
};

/* Choose award for single-player game.
 * One of: DEADLY WANTED BIG_GAME DEAD_MEAT FACE_TO_FACE MEDIA_SAVVY TALKATIVE HOARDER PARTICIPANT.
 * Never: DISHONORABLE PUNCHING_BAG UNKILLABLE STATS_READER ACTIVE.
 */

static int ctm_decide_player_awards_single(struct ctm_sprite_player *SPR) {

  // Total elapsed time in seconds. Clamp low to 1.
  int secondc=(ctm_game.endtime-ctm_game.starttime)/1000000;
  if (secondc<1) secondc=1;

  int killc=SPR->bosskills+SPR->beastkills+SPR->copkills+SPR->voterkills+SPR->mummykills;
  double killrate=(double)killc/(double)secondc; // 0.3 is pretty high. That's somebody playing straight, but killing everywhere he goes.
  if (killrate>1.0) { SPR->award=CTM_AWARD_DEADLY; return 0; } // More than 1 k/s is an impressive amount of killing.

  // If we are wanted in more states than bosses we've killed, win WANTED.
  int wantedc=0,i;
  for (i=0;i<9;i++) if (SPR->wanted[i]>=CTM_WANTED_MAX) wantedc++;
  if (wantedc>SPR->bosskills) { SPR->award=CTM_AWARD_WANTED; return 0; }

  // If we killed any bosses, win BIG_GAME.
  if (SPR->bosskills>0) { SPR->award=CTM_AWARD_BIG_GAME; return 0; }

  // If we died more than twice per five minutes, win DEAD_MEAT.
  double deathrate=((double)SPR->deaths*60.0)/(double)secondc;
  if (deathrate>0.1) { SPR->award=CTM_AWARD_DEAD_MEAT; return 0; }

  // If our kill rate is over 0.30, win DEADLY. A little over 3 seconds per kill, that's a lot of killing. But not Olympic quality.
  if (killrate>0.3) { SPR->award=CTM_AWARD_DEADLY; return 0; }

  // If we delivered at least ten speeches, consider speech awards:
  //   FACE_TO_FACE if our speeches were >90% in person.
  //   MEDIA_SAVVY if our speeches were >90% over the radio.
  //   TALKATIVE if our speech rate is over 0.05 sp/s.
  // Keep this block towards the end of consideration: Speech awards are a fucking bore.
  int speechc=SPR->facespeeches+SPR->radiospeeches;
  if (speechc>10) {
    int lopside=speechc-speechc/10;
    if (SPR->facespeeches>lopside) { SPR->award=CTM_AWARD_FACE_TO_FACE; return 0; }
    if (SPR->radiospeeches>lopside) { SPR->award=CTM_AWARD_MEDIA_SAVVY; return 0; }
    double speechrate=(double)speechc/(double)secondc;
    if (speechrate>0.05) { SPR->award=CTM_AWARD_TALKATIVE; return 0; }
  }

  // If we made no speeches but have at least 5 coins, call it "hoarding".
  if (!speechc&&(SPR->inventory[CTM_ITEM_COIN]>=5)) { SPR->award=CTM_AWARD_HOARDER; return 0; }

  // Every mummy is special!
  SPR->award=CTM_AWARD_PARTICIPANT;
  return 0;
}

/* Choose awards for multiplayer game.
 */

static int ctm_count_wanted(struct ctm_sprite_player *SPR) {
  int c=0,i; for (i=0;i<9;i++) if (SPR->wanted[i]>=CTM_WANTED_MAX) c++;
  return c;
}

static int ctm_decide_player_awards_multiple(struct ctm_sprite_player **SPRV,int sprc) {

  // Give each of the 16 awards to one player, or to nobody.
  int awards[16]={0};
  int i,highscore;

  #define GIVEAWARD(tag,formula) \
    for (i=highscore=0;i<sprc;i++) { \
      struct ctm_sprite_player *SPR=SPRV[i]; \
      int score=(formula); \
      if (score>highscore) { highscore=score; awards[CTM_AWARD_##tag]=i+1; } \
      else if (score==highscore) awards[CTM_AWARD_##tag]=0; \
    }
  GIVEAWARD(DEADLY,SPR->beastkills+SPR->voterkills+SPR->bosskills+SPR->copkills+SPR->mummykills)
  GIVEAWARD(DISHONORABLE,SPR->mummykills)
  GIVEAWARD(BIG_GAME,SPR->bosskills)
  GIVEAWARD(DEAD_MEAT,SPR->deaths)
  GIVEAWARD(TALKATIVE,SPR->facespeeches+SPR->radiospeeches)
  GIVEAWARD(FACE_TO_FACE,(SPR->facespeeches>10)?(((SPR->facespeeches*100)/(SPR->facespeeches+SPR->radiospeeches))-90):0)
  GIVEAWARD(MEDIA_SAVVY,(SPR->radiospeeches>10)?(((SPR->radiospeeches*100)/(SPR->facespeeches+SPR->radiospeeches))-90):0)
  GIVEAWARD(PUNCHING_BAG,SPR->strikes)
  GIVEAWARD(UNKILLABLE,(SPR->deaths?0:1))
  GIVEAWARD(HOARDER,SPR->inventory[CTM_ITEM_COIN])
  GIVEAWARD(STATS_READER,SPR->pause_framec)
  GIVEAWARD(ACTIVE,ctm_game.play_framec-SPR->pause_framec)
  GIVEAWARD(WANTED,ctm_count_wanted(SPR))
  #undef GIVEAWARD

  #define KEEPAWARD(tag) \
    if (awards[CTM_AWARD_##tag]) { \
      SPRV[awards[CTM_AWARD_##tag]-1]->award=CTM_AWARD_##tag; \
      for (i=0;i<16;i++) if ((i!=CTM_AWARD_##tag)&&(awards[i]==awards[CTM_AWARD_##tag])) awards[i]=0; \
    }
  KEEPAWARD(BIG_GAME)
  KEEPAWARD(DISHONORABLE)
  KEEPAWARD(WANTED)
  KEEPAWARD(DEADLY)
  KEEPAWARD(UNKILLABLE)
  KEEPAWARD(DEAD_MEAT)
  KEEPAWARD(PUNCHING_BAG)
  KEEPAWARD(FACE_TO_FACE)
  KEEPAWARD(MEDIA_SAVVY)
  KEEPAWARD(HOARDER)
  KEEPAWARD(STATS_READER)
  KEEPAWARD(ACTIVE)
  KEEPAWARD(TALKATIVE)
  #undef KEEPAWARD
  
  return 0;
}

/* Choose awards, entry point and dispatch.
 */

int ctm_decide_player_awards() {

  // Organize player sprites by ID, ensure that they are all valid, etc.
  struct ctm_sprite_player *SPRV[4]={0};
  int i; for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_player.sprv[i];
    if (!spr||(spr->type!=&ctm_sprtype_player)) continue;
    struct ctm_sprite_player *SPR=(struct ctm_sprite_player*)spr;
    if ((SPR->playerid<1)||(SPR->playerid>4)) continue;
    SPRV[SPR->playerid-1]=SPR;
    SPR->award=CTM_AWARD_PARTICIPANT;
  }
  int playerc=0;
  while ((playerc<4)&&SPRV[playerc]) playerc++;

  // Dispatch based on player count -- Single-player games score differently from multiplayer.
  if (playerc>1) return ctm_decide_player_awards_multiple(SPRV,playerc);
  if (playerc==1) return ctm_decide_player_awards_single(SPRV[0]);
  return 0;
}
