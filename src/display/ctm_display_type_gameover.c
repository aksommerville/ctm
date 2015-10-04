#include "ctm.h"
#include "ctm_display.h"
#include "ctm_scatter_graph.h"
#include "video/ctm_video_internal.h"
#include "game/ctm_geography.h"
#include "game/ctm_game.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_player.h"
#include <math.h>

// Minimal framebuffer size.
// One axis will match this; the other will be larger.
#define CTM_GAMEOVER_MINW  CTM_RESIZE(768) // logo==256, allow at least the same amount left and right for statistics.
#define CTM_GAMEOVER_MINH  CTM_RESIZE(300) // (32*9==288), plus some margin

/* Object type.
 */

struct ctm_display_gameover {
  struct ctm_display hdr;
  int counter;
};

#define DISPLAY ((struct ctm_display_gameover*)display)

/* Delete.
 */

static void _ctm_display_gameover_del(struct ctm_display *display) {
}

/* Initialize.
 */

static int _ctm_display_gameover_init(struct ctm_display *display) {
  return 0;
}

/* Draw national map.
 */

static int ctm_display_gameover_draw_national_map(struct ctm_display *display,int x,int y,int w,int h,const struct ctm_election_result *result) {
  if (ctm_draw_rect(x,y,w,h,0xffffffff)<0) return -1;
  if (ctm_video_begin_sprites()<0) return -1;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(9);
  if (!vtxv) return -1;
  int i;
  for (i=0;i<9;i++) {
    int col=i%3,row=i/3;
    vtxv[i].x=x+(w>>1)+(col-1)*CTM_TILESIZE;
    vtxv[i].y=y+(h>>1)+(row-1)*CTM_TILESIZE;
    vtxv[i].tile=0xba+(row*16)+col;
    const int *votes=result->brokendown+(i*14);
         if (votes[0]>votes[1]) { vtxv[i].r=0x00; vtxv[i].g=0x00; vtxv[i].b=0xff; }
    else if (votes[1]>votes[0]) { vtxv[i].r=0xff; vtxv[i].g=0x00; vtxv[i].b=0x00; }
    else                        { vtxv[i].r=0x80; vtxv[i].g=0x80; vtxv[i].b=0x80; }
    vtxv[i].a=0xff;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }
  if (ctm_video_end_sprites(ctm_video.texid_sprites)<0) return -1;
  return 0;
}

/* Draw pie chart of demographic distribution.
 */

static int ctm_display_gameover_draw_demographics(struct ctm_display *display,int x,int y,int w,int h,struct ctm_sprgrp *grp) {
  if (!grp) return 0;
  int pop[7]={0},total=0;
  int i; for (i=0;i<grp->sprc;i++) {
    struct ctm_sprite *spr=grp->sprv[i];
    if (!spr||(spr->type!=&ctm_sprtype_voter)) continue;
    struct ctm_sprite_voter *SPR=(struct ctm_sprite_voter*)spr;
    if ((SPR->party<0)||(SPR->party>6)) continue;
    pop[SPR->party]++;
    total++;
  }
  if (total<1) return 0; // Zero population? Draw nothing.
  int radius=((w<h)?w:h)>>1;
  int midx=x+(w>>1);
  int midy=y+(h>>1);
  double t=0.0;
  int lastp=6;
  while ((lastp>0)&&!pop[lastp]) lastp--;
  for (i=0;i<=lastp;i++) {
    if (pop[i]<1) continue;
    double nextt;
    if (i==lastp) nextt=M_PI*2.0; // Make sure we actually hit the end, don't let rounding fuck us.
    else nextt=t+(pop[i]*M_PI*2.0)/total;
    if (ctm_draw_pie(midx,midy,radius,t,nextt,ctm_party_color[i])<0) return -1;
    t=nextt;
  }
  return 0;
}

/* Draw summary chart (blue on the left, red on the right, and percentages written out).
 */

static int ctm_display_gameover_draw_summary_bar(struct ctm_display *display,int x,int y,int w,int h,int stateix,const struct ctm_election_result *result) {
  int blue,red;
  if ((stateix>=0)&&(stateix<9)) {
    blue=result->brokendown[stateix*14];
    red=result->brokendown[stateix*14+1];
  } else {
    blue=result->totalblue;
    red=result->totalred;
  }
  int total=blue+red;
  if (total>0) {
    int bluew=(blue*w)/total;
    int redw=w-bluew;
    if (ctm_draw_rect(x,y,bluew,h,0x0000ffff)<0) return -1;
    if (ctm_draw_rect(x+bluew,y,redw,h,0xff0000ff)<0) return -1;
  } else {
    if (ctm_draw_rect(x,y,w,h,0x808080ff)<0) return -1;
  }
  if (ctm_draw_rect(x+(w>>1),y,1,h,0x000000ff)<0) return -1;

  // Percentages. Ensure that we only say "100/0" when it is a full sweep, and that it always add up to 100.
  if (total>0) {
    int bluepct;
    if (blue==total) bluepct=100;
    else if (red==total) bluepct=0;
    else {
      bluepct=(blue*100)/total;
      if (bluepct<1) bluepct=1; else if (bluepct>99) bluepct=99;
    }
    int redpct=100-bluepct;
    if (ctm_video_begin_text(CTM_RESIZE(16))<0) return -1;
    if (ctm_video_add_textf(x+CTM_RESIZE(8),y+(h>>1),0xffffffff,"%d%%",bluepct)<0) return -1;
    int redx=x+w-CTM_RESIZE(8);
    if (redpct>=100) redx-=CTM_RESIZE(24);
    else if (redpct>=10) redx-=CTM_RESIZE(16);
    else redx-=CTM_RESIZE(8);
    if (ctm_video_add_textf(redx,y+(h>>1),0xffffffff,"%d%%",redpct)<0) return -1;
    if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;
  }
  
  return 0;
}

/* Draw results for a single player or the werewolf, in a 32x32 square.
 * (playerid) outside (1..4) for the werewolf.
 * This also adds the blinker.
 */

static int ctm_display_gameover_draw_player_result(struct ctm_display *display,int x,int y,int playerid,const struct ctm_election_result *result) {

  uint8_t tilebase;
  int party;
  if ((playerid>=1)&&(playerid<=4)) {
    tilebase=0x73;
    struct ctm_sprite *spr=ctm_sprite_for_player(playerid);
    if (spr&&(spr->type==&ctm_sprtype_player)) party=((struct ctm_sprite_player*)spr)->party;
    else party=0;
  } else {
    tilebase=0x75;
    party=ctm_game.werewolf_party;
  }
  if ((party<0)||(party>6)) party=0;
  int winner;
  if (result->elecblue>result->elecred) winner=(party==CTM_PARTY_BLUE)?1:0;
  else if (result->elecblue<result->elecred) winner=(party==CTM_PARTY_RED)?1:0;
  else winner=0;
  
  if (ctm_video_begin_sprites()<0) return -1;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(5);
  if (!vtxv) return -1;
  int i; for (i=0;i<4;i++) {
    vtxv[i].x=x+(CTM_TILESIZE>>1)+((i&1)?CTM_TILESIZE:0);
    vtxv[i].y=y+(CTM_TILESIZE>>1)+((i&2)?CTM_TILESIZE:0);
    vtxv[i].tile=tilebase+((i&1)?1:0)+((i&2)?16:0);
    vtxv[i].r=ctm_party_color[party]>>24;
    vtxv[i].g=ctm_party_color[party]>>16;
    vtxv[i].b=ctm_party_color[party]>>8;
    vtxv[i].a=0xff;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }
  vtxv[4].x=x+CTM_RESIZE(16);
  vtxv[4].y=y+CTM_RESIZE(30);
  vtxv[4].tile=winner?0x93:0x94;
  vtxv[4].a=0xff;
  vtxv[4].ta=0;
  vtxv[4].flop=0;
  if (ctm_video_end_sprites(ctm_video.texid_uisprites)<0) return -1;

  return 0;
}

/* Draw nationwide statistics.
 */

static int ctm_display_gameover_draw_nationwide(struct ctm_display *display,int limitx,int limity,int limitw,int limith,const struct ctm_election_result *result) {

  // Decide what we will draw.
  int had_werewolf=(ctm_game.bluec&&ctm_game.redc)?0:1;
  int contenth=CTM_RESIZE(124);
  if (ctm_game.playerc>=3) contenth+=CTM_RESIZE(64); // Two rows of player results.
  else contenth+=CTM_RESIZE(32); // One row of player results.
  contenth+=CTM_RESIZE(16)*4; // spacer, timelimit, population
  if (had_werewolf) contenth+=CTM_RESIZE(16); // difficulty
  if (ctm_game.time_remaining.min||ctm_game.time_remaining.s||ctm_game.time_remaining.cs) contenth+=CTM_RESIZE(16); // cancellation message
  if ((result->elecblue>result->elecred)&&(result->totalblue<result->totalred)) contenth+=CTM_RESIZE(16); // popular vs electoral message
  else if ((result->elecblue<result->elecred)&&(result->totalblue>result->totalred)) contenth+=CTM_RESIZE(16); // "
  else if (result->totalblue&&(result->totalblue==result->totalred)) contenth+=CTM_RESIZE(16); // popular vote tie
  else if (!result->totalblue||!result->totalred) contenth+=CTM_RESIZE(16); // popular sweep
  
  int midx=limitx+(limitw>>1);
  int midy=limity+(limith>>1);
  int topy=midy-(contenth>>1);

  // Election results.
  if (ctm_display_gameover_draw_national_map(display,midx-CTM_RESIZE(56),topy,CTM_RESIZE(52),CTM_RESIZE(52),result)<0) return -1;
  if (ctm_display_gameover_draw_demographics(display,midx+CTM_RESIZE(4),topy,CTM_RESIZE(52),CTM_RESIZE(52),&ctm_group_voter)<0) return -1;
  if (ctm_scatter_graph_draw(limitx+CTM_RESIZE(32),topy+CTM_RESIZE(68),limitw-CTM_RESIZE(64),CTM_RESIZE(15),&ctm_group_voter)<0) return -1;
  if (ctm_display_gameover_draw_summary_bar(display,limitx+CTM_RESIZE(32),topy+CTM_RESIZE(92),limitw-CTM_RESIZE(64),CTM_RESIZE(16),-1,result)<0) return -1;

  // Draw results for each player and the werewolf.
  int y=topy+CTM_RESIZE(124);
  if (had_werewolf) switch (ctm_game.playerc) {
    case 1: {
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(36),y,1,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx+CTM_RESIZE(4),y,0,result)<0) return -1;
        y+=CTM_RESIZE(48);
      } break;
    case 2: {
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(52),y,1,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(20),y,2,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx+CTM_RESIZE(20),y,0,result)<0) return -1;
        y+=CTM_RESIZE(48);
      } break;
    case 3: {
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(52),y,1,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(20),y,2,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(52),y+CTM_RESIZE(32),3,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx+CTM_RESIZE(20),y+CTM_RESIZE(16),0,result)<0) return -1;
        y+=CTM_RESIZE(80);
      } break;
    case 4: {
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(52),y,1,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(20),y,2,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(52),y+CTM_RESIZE(32),3,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(20),y+CTM_RESIZE(32),4,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx+CTM_RESIZE(20),y+CTM_RESIZE(16),0,result)<0) return -1;
        y+=CTM_RESIZE(80);
      } break;
  } else switch (ctm_game.playerc) {
    case 1: { // This mode doesn't exist, but whatever.
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(16),y,1,result)<0) return -1;
        y+=CTM_RESIZE(48);
      } break;
    case 2: {
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(32),y,1,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx,y,2,result)<0) return -1;
        y+=CTM_RESIZE(48);
      } break;
    case 3: {
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(32),y,1,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx,y,2,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(32),y+CTM_RESIZE(32),3,result)<0) return -1;
        y+=CTM_RESIZE(80);
      } break;
    case 4: {
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(32),y,1,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx,y,2,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx-CTM_RESIZE(32),y+CTM_RESIZE(32),3,result)<0) return -1;
        if (ctm_display_gameover_draw_player_result(display,midx,y+CTM_RESIZE(32),4,result)<0) return -1;
        y+=CTM_RESIZE(80);
      } break;
  }

  // Text fields.
  int textx=limitx+CTM_RESIZE(16);
  if (ctm_video_begin_text(CTM_RESIZE(16))<0) return -1;
  if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffffffff,"Duration: %d:00",ctm_game.timelimit_minutes)<0) return -1; 
  y+=CTM_RESIZE(16);
  if (ctm_game.time_remaining.min||ctm_game.time_remaining.s||ctm_game.time_remaining.cs) {
    if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffff00ff,"Aborted at %d:%02d.",ctm_game.time_remaining.min,ctm_game.time_remaining.s)<0) return -1;
    y+=CTM_RESIZE(16);
  }
  if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffffffff,"Population: %d",ctm_game.population)<0) return -1; 
  y+=CTM_RESIZE(16);
  if (had_werewolf) {
    const char *diffname="?";
    switch (ctm_game.difficulty) {
      case 1: diffname="Easy"; break;
      case 2: diffname="Medium"; break;
      case 3: diffname="Hard"; break;
      case 4: diffname="Crazy"; break;
    }
    if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffffffff,"Difficulty: %s",diffname)<0) return -1;
    y+=CTM_RESIZE(16);
  }
  if ((result->elecblue>result->elecred)&&(result->totalblue<result->totalred)) {
    if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffff00ff,"Red won the popular vote.")<0) return -1; y+=CTM_RESIZE(16);
  } else if ((result->elecblue<result->elecred)&&(result->totalblue>result->totalred)) {
    if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffff00ff,"Blue won the popular vote.")<0) return -1; y+=CTM_RESIZE(16);
  } else if (result->totalblue&&(result->totalblue==result->totalred)) {
    if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffff00ff,"Popular vote tied at %d.",result->totalblue)<0) return -1; y+=CTM_RESIZE(16);
  } else if (!result->totalblue&&!result->totalred) {
    if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffff00ff,"No living voters!")<0) return -1; y+=CTM_RESIZE(16);
  } else if (!result->totalblue||!result->totalred) {
    if (ctm_video_add_textf(textx,y+CTM_RESIZE(8),0xffff00ff,"Popular vote unanimous!")<0) return -1; y+=CTM_RESIZE(16);
  }
  if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;
  
  return 0;
}

/* Draw assembled statistics bar for one state.
 */

static int ctm_display_gameover_draw_state(struct ctm_display *display,int x,int y,int w,int h,int stateix,const struct ctm_election_result *result) {
  const int *votes=result->brokendown+stateix*14;

  // First a square with the state's abbreviated name. Background color tells which way it voted.
  uint32_t eleccolor=0x808080ff;
  if (votes[0]>votes[1]) eleccolor=ctm_party_color[CTM_PARTY_BLUE];
  else if (votes[1]>votes[0]) eleccolor=ctm_party_color[CTM_PARTY_RED];
  if (ctm_draw_rect(x,y,h,h,eleccolor)<0) return -1;
  if (ctm_video_begin_text(CTM_RESIZE(16))<0) return -1;
  if (ctm_video_add_textf_centered(x,y,h,h,0xffffffff,"%.2s",ctm_state_data[stateix].abbreviation)<0) return -1;
  if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;

  // Next a pie chart with the state's demographics.
  if (ctm_display_gameover_draw_demographics(display,x+h,y,h,h,ctm_group_state_voter+stateix)<0) return -1;

  // Split the remaining horizontal space in two ribbons, and draw scatter and summary.
  x+=h<<1; w-=h<<1;
  int toph=ctm_scatter_graph_choose_height(h>>1,h);
  int bottomh=h-toph;
  if (ctm_scatter_graph_draw(x,y,w,toph,ctm_group_state_voter+stateix)<0) return -1;
  if (ctm_display_gameover_draw_summary_bar(display,x,y+toph,w,bottomh,stateix,result)<0) return -1;
  
  return 0;
}

/* Draw statistics for each state.
 */

static int ctm_display_gameover_draw_perstate(struct ctm_display *display,int limitx,int limity,int limitw,int limith,const struct ctm_election_result *result) {
  int rowh=CTM_RESIZE(33); // includes 1 pixel margin on the bottom
  int contenty=limity+(limith>>1)-((rowh*9)>>1);
  int contentw=limitw-CTM_RESIZE(20); // 20 pixels for horizontal margins
  if (contentw<CTM_RESIZE(74)) return 0; // sanity check, need 64 pixels plus a bit more for the graphs.
  int contentx=limitx+(limitw>>1)-(contentw>>1);
  int stateix; for (stateix=0;stateix<9;stateix++) {
    if (ctm_display_gameover_draw_state(display,contentx,contenty+rowh*stateix,contentw,rowh-1,stateix,result)<0) return -1;
  }
  return 0;
}

/* Redraw framebuffer -- this is done only once. (Or again any time it resizes. Which is just the once, really).
 */

static int ctm_display_gameover_redraw_1(struct ctm_display *display) {

  // Conduct the election. If we run multiple times, this should always produce the same result.
  // But we're only going to run once, so don't bother caching it.
  struct ctm_election_result result={0};
  if (ctm_conduct_election(&result)<0) return -1;

  // Clear framebuffer to the winner's color.
  uint32_t winnercolor;
  if (result.elecblue>result.elecred) winnercolor=ctm_party_color[CTM_PARTY_BLUE];
  else if (result.elecred>result.elecblue) winnercolor=ctm_party_color[CTM_PARTY_RED];
  else winnercolor=ctm_party_color[0];
  glClearColor((winnercolor>>24)/1024.0,((winnercolor>>16)&0xff)/1024.0,((winnercolor>>8)&0xff)/1024.0,1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Draw the winner's logo centered.
  int logox=(display->fbw>>1)-CTM_RESIZE(128);
  int logoy=(display->fbh>>1)-CTM_RESIZE(128);
  uint32_t logoid=(result.elecblue>result.elecred)?ctm_video.texid_logo_frostbite:(result.elecred>result.elecblue)?ctm_video.texid_logo_blood:ctm_video.texid_logo_tie;
  if (ctm_draw_texture(logox,logoy,CTM_RESIZE(256),CTM_RESIZE(256),logoid,0)<0) return -1;

  // Draw nationwide statistics in the left third, and per-state statistics in the right.
  if (ctm_display_gameover_draw_nationwide(display,0,0,logox,display->fbh,&result)<0) return -1;
  if (ctm_display_gameover_draw_perstate(display,logox+CTM_RESIZE(256),0,display->fbw-logox-CTM_RESIZE(256),display->fbh,&result)<0) return -1;

  // Draw bunting in both upper corners. The image is 64x64, native to left side.
  if (ctm_draw_texture(0,0,CTM_RESIZE(64),CTM_RESIZE(64),ctm_video.texid_bunting,0)<0) return -1;
  if (ctm_draw_texture_flop(display->fbw-CTM_RESIZE(64),0,CTM_RESIZE(64),CTM_RESIZE(64),ctm_video.texid_bunting,0)<0) return -1;
  
  return 0;
}

static int ctm_display_gameover_redraw(struct ctm_display *display) {
  glBindFramebuffer(GL_FRAMEBUFFER,display->fb);
  ctm_video_set_uniform_screen_size(display->fbw,display->fbh);
  int err=ctm_display_gameover_redraw_1(display);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  ctm_video_reset_uniform_screen_size();
  if (err<0) return -1;
  return 0;
}

/* Resized.
 */

static int _ctm_display_gameover_resized(struct ctm_display *display) {
  int wforh=(display->w*CTM_GAMEOVER_MINH)/display->h;
  int hforw=(display->h*CTM_GAMEOVER_MINW)/display->w;
  if (wforh>=CTM_GAMEOVER_MINW) {
    if (ctm_display_resize_fb(display,wforh,CTM_GAMEOVER_MINH)<0) return -1;
  } else {
    if (ctm_display_resize_fb(display,CTM_GAMEOVER_MINW,hforw)<0) return -1;
  }
  if (ctm_display_gameover_redraw(display)<0) return -1;
  return 0;
}

/* Draw main output.
 */

static int _ctm_display_gameover_draw(struct ctm_display *display) {

  if (ctm_draw_texture(display->x,display->y,display->w,display->h,display->fbtexid,1)<0) return -1;
  
  if (++(DISPLAY->counter)>=120) {
    if ((DISPLAY->counter-120)%60<50) {
      if (ctm_video_begin_text(CTM_RESIZE(16))<0) return -1;
      if (ctm_video_add_textf_centered(0,display->h-CTM_RESIZE(50),display->w,CTM_RESIZE(50),0xffff00ff,"Press any key to restart.")<0) return -1;
      if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;
    }
  }
  
  return 0;
}

/* Type definition.
 */

const struct ctm_display_type ctm_display_type_gameover={
  .name="gameover",
  .objlen=sizeof(struct ctm_display_gameover),
  .del=_ctm_display_gameover_del,
  .init=_ctm_display_gameover_init,
  .resized=_ctm_display_gameover_resized,
  .draw=_ctm_display_gameover_draw,
};
