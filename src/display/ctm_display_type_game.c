#include "ctm.h"
#include "ctm_display.h"
#include "ctm_scatter_graph.h"
#include "video/ctm_video_internal.h"
#include "game/ctm_grid.h"
#include "game/ctm_game.h"
#include "game/ctm_geography.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "misc/ctm_time.h"
#include <math.h>

// Preferred framebuffer size. We may go longer on one axis if necessary.
#define CTM_FBW_MIN (CTM_TILESIZE*12)
#define CTM_FBH_MIN (CTM_TILESIZE*10)
#define CTM_FBW_MAX (CTM_TILESIZE*25)
#define CTM_FBH_MAX (CTM_TILESIZE*20)

#define CTM_REPORTW_LO (CTM_TILESIZE<<3)
#define CTM_REPORTH_LO (CTM_TILESIZE*3+((CTM_TILESIZE*4)/16))

/* Object type.
 */

struct ctm_display_game {
  struct ctm_display hdr;
  int playerid;
  int scrollx,scrolly;
  int interior;
  int selx,sely; // Cursor in pause menu.
  int itemstore_colc;
  int itemstore_rowc;
  GLuint texid_report;
  GLuint fb_report;
  int reportw,reporth;
  int have_report;
};

#define DISPLAY ((struct ctm_display_game*)display)

/* Delete.
 */

static void _ctm_display_game_del(struct ctm_display *display) {
  if (DISPLAY->texid_report) glDeleteTextures(1,&DISPLAY->texid_report);
  if (DISPLAY->fb_report) glDeleteFramebuffers(1,&DISPLAY->fb_report);
}

/* Initialize.
 */

static int _ctm_display_game_init(struct ctm_display *display) {

  #if !CTM_TEST_DISABLE_VIDEO
  /* Allocate texture and framebuffer for report. */
  glGenTextures(1,&DISPLAY->texid_report);
  if (!DISPLAY->texid_report) return -1;
  glBindTexture(GL_TEXTURE_2D,DISPLAY->texid_report);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1,&DISPLAY->fb_report);
  if (!DISPLAY->fb_report) return -1;
  glBindFramebuffer(GL_FRAMEBUFFER,DISPLAY->fb_report);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,DISPLAY->texid_report,0);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  #endif

  return 0;
}

/* Resized.
 */

static int _ctm_display_game_resized(struct ctm_display *display) {

  // Item store is drawn 8x2 for wide displays, or 4x4 for tall.
  if (display->w>=display->h) {
    DISPLAY->itemstore_colc=8;
    DISPLAY->itemstore_rowc=2;
  } else {
    DISPLAY->itemstore_colc=4;
    DISPLAY->itemstore_rowc=4;
  }

  // In the absence of limits, we prefer 1:1 scale.
  int fbw=display->w;
  int fbh=display->h;

  // Must display at least (CTM_FBW_MIN,CTM_FBH_MIN); extend framebuffer as necessary.
  if (fbw<CTM_FBW_MIN) {
    fbh=(fbh*CTM_FBW_MIN)/fbw;
    fbw=CTM_FBW_MIN;
  }
  if (fbh<CTM_FBH_MIN) {
    fbw=(fbw*CTM_FBH_MIN)/fbh;
    fbh=CTM_FBH_MIN;
  }

  // If we're too large, consider matching one of the maximum limits.
  if ((fbw>CTM_FBW_MAX)||(fbh>CTM_FBH_MAX)) {
    int wforh=(fbw*CTM_FBH_MAX)/fbh;
    int hforw=(fbh*CTM_FBW_MAX)/fbw;
    int wforhok=((wforh>=CTM_FBW_MIN)&&(wforh<=CTM_FBW_MAX));
    int hforwok=((hforw>=CTM_FBH_MIN)&&(hforw<=CTM_FBH_MAX)); // A "forwok" is like an Ewok, but in base 4 instead of the natural base.
    if (wforhok&&hforwok) { // Doesn't make sense, but go with wforh. Whatever.
      fbw=wforh;
      fbh=CTM_FBH_MAX;
    } else if (wforhok) { // Maxmimum height.
      fbw=wforh;
      fbh=CTM_FBH_MAX;
    } else if (hforwok) { // Maximum width.
      fbw=CTM_FBW_MAX;
      fbh=hforw;
    } else if (wforh>CTM_FBW_MAX) { // Too wide but go with it. (wide output aspect).
      fbw=wforh;
      fbh=CTM_FBH_MAX;
    } else if (hforw>CTM_FBH_MAX) { // Too tall but go with it. (narrow output aspect).
      fbw=CTM_FBW_MAX;
      fbh=hforw;
    } else { // Should never land here.
    }
  }

  if (ctm_display_resize_fb(display,fbw,fbh)<0) return -1;

  DISPLAY->reportw=(display->fbw*5)/6;
  DISPLAY->reporth=((display->fbh-(CTM_TILESIZE<<2)-(CTM_TILESIZE*DISPLAY->itemstore_rowc*2)-(CTM_TILESIZE<<1))*7)/8;
  if ((DISPLAY->reportw>0)&&(DISPLAY->reporth>0)) {
    if (DISPLAY->reportw<CTM_REPORTW_LO) DISPLAY->reportw=CTM_REPORTW_LO;
    if (DISPLAY->reporth<CTM_REPORTH_LO) DISPLAY->reporth=CTM_REPORTH_LO;
    glBindTexture(GL_TEXTURE_2D,DISPLAY->texid_report);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,DISPLAY->reportw,DISPLAY->reporth,0,GL_RGB,GL_UNSIGNED_BYTE,0);
    DISPLAY->have_report=0;
  } else {
    DISPLAY->have_report=1;
  }
  
  return 0;
}

/* Draw Fountain of Youth indicator.
 */

static int ctm_display_game_draw_foy_indicator(struct ctm_display *display) {

  // Where is it?
  int x,y;
  if (ctm_grid_nearest_fountain(&x,&y,DISPLAY->scrollx+(display->fbw>>1),DISPLAY->scrolly+(display->fbh>>1))<1) return 0; // No fountains. Whatever.
  int dx=(x<DISPLAY->scrollx)?-1:(x>DISPLAY->scrollx+display->fbw)?1:0;
  int dy=(y<DISPLAY->scrolly)?-1:(y>DISPLAY->scrolly+display->fbh)?1:0;
  if (!dx&&!dy) return 0; // On screen.
  x-=DISPLAY->scrollx;
  if (x<CTM_TILESIZE) x=CTM_TILESIZE;
  else if (x>display->fbw-CTM_TILESIZE) x=display->fbw-CTM_TILESIZE;
  y-=DISPLAY->scrolly;
  if (y<CTM_TILESIZE*3) y=CTM_TILESIZE*3;
  else if (y>display->fbh-CTM_TILESIZE) y=display->fbh-CTM_TILESIZE;

  // What does it look like?
  uint8_t tile=0x70+((dy+1)*16)+dx+1;
  uint8_t alpha=(ctm_game.play_framec&0x3f)<<2;
  if (ctm_game.play_framec&0x40) {
    alpha=0xff-alpha;
  }

  // Make two sprites: arrow and icon. These are in the 'uisprites' sheet.
  if (ctm_video_begin_sprites()<0) return -1;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(2);
  if (!vtxv) return -1;
  vtxv[0].x=x;
  vtxv[0].y=y;
  vtxv[0].tile=tile;
  vtxv[0].r=0x80;
  vtxv[0].g=0x80;
  vtxv[0].b=0x80;
  vtxv[0].a=alpha;
  vtxv[0].ta=0x00;
  vtxv[0].flop=0;
  vtxv[1]=vtxv[0];
  vtxv[1].x-=dx*CTM_TILESIZE;
  vtxv[1].y-=dy*CTM_TILESIZE;
  vtxv[1].tile=0x81;
  if (ctm_video_end_sprites(ctm_video.texid_uisprites)<0) return -1;
  
  return 0;
}

/* Draw overlay elements in framebuffer. (sprites: equipped items, HP, and coins label)
 */

static void ctm_display_game_draw_equipped_item(struct ctm_vertex_sprite *vtxv,int itemid,int x,int y) {
  int suby; for (suby=0;suby<3;suby++) {
    int subx; for (subx=0;subx<3;subx++,vtxv++) {
      vtxv->x=x+subx*CTM_TILESIZE;
      vtxv->y=y+suby*CTM_TILESIZE;
      vtxv->tile=0xbd+(suby*16)+subx;
    }
  }
  vtxv->x=x+CTM_TILESIZE;
  vtxv->y=y+CTM_TILESIZE;
  vtxv->tile=0xf0;
  if ((itemid>0)&&(itemid<16)) vtxv->tile+=itemid;
}

static void ctm_display_game_draw_wanted_level(int x,int y,int w,int h,int wanted) {
  if (wanted>=CTM_WANTED_MAX) {
    if (ctm_draw_rect(x,y,w,h,0xff8000ff)<0) return;
  } else if (wanted>0) {
    uint32_t barcolor;
    if (wanted>=CTM_WANTED_MAX-CTM_WANTED_INCREMENT) barcolor=0xff0000ff;
    else barcolor=0xffff00ff;
    int sep=(wanted*w)/CTM_WANTED_MAX;
    if (ctm_draw_rect(x,y,sep,h,barcolor)<0) return;
    if (ctm_draw_rect(x+sep,y,w-sep,h,0x80808080)<0) return;
  } else {
    // Draw an empty bar? No, just don't draw anything at zero.
    //if (ctm_draw_rect(wantedl,wantedy,wantedw,wantedh,0x808080ff)<0) return;
  }
}

#define CTM_DISPK_WANTEDL ((CTM_TILESIZE*88)/16)
#define CTM_DISPK_WANTEDR ((CTM_TILESIZE*40)/16)
#define CTM_DISPK_WANTEDW ((CTM_TILESIZE*50)/16)
#define CTM_DISPK_ITEM ((CTM_TILESIZE*24)/16)

static int ctm_display_game_draw_overlay_sprites(struct ctm_display *display,struct ctm_sprite_player *SPR) {
  int i;

  /* Draw wanted level (not a "sprite" but it had to go somewhere). */
  int wantedl=CTM_DISPK_WANTEDL+CTM_TILESIZE*SPR->hpmax;
  int wantedr=display->fbw-CTM_DISPK_WANTEDR-CTM_TILESIZE;
  int wantedw=wantedr-wantedl;
  if (wantedw>=CTM_DISPK_WANTEDW) {
    int wantedy=CTM_TILESIZE+CTM_RESIZE(4);
    int wantedh=CTM_TILESIZE-CTM_RESIZE(8);
    int wanted=SPR->wanted[ctm_state_for_pixel(SPR->hdr.x,SPR->hdr.y)];
    ctm_display_game_draw_wanted_level(wantedl,wantedy,wantedw,wantedh,wanted);
  } else {
    int wantedy=CTM_TILESIZE<<1;
    int wantedh=CTM_TILESIZE>>1;
    int wanted=SPR->wanted[ctm_state_for_pixel(SPR->hdr.x,SPR->hdr.y)];
    ctm_display_game_draw_wanted_level(0,wantedy,display->fbw,wantedh,wanted);
  }

  /* Begin proper sprites. */
  if (ctm_video_begin_sprites()<0) return -1;
  int vtxc=10*3+1+SPR->hpmax; // 10 for each item slot, 1 coin label, 1 each for HP.
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  for (i=0;i<vtxc;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=0xff;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }

  ctm_display_game_draw_equipped_item(vtxv+ 0,SPR->item_primary,0,0);
  ctm_display_game_draw_equipped_item(vtxv+10,SPR->item_secondary,CTM_DISPK_ITEM,0);
  ctm_display_game_draw_equipped_item(vtxv+20,SPR->item_tertiary,CTM_DISPK_ITEM*2,0);

  vtxv[30].x=CTM_DISPK_WANTEDL;
  vtxv[30].y=CTM_TILESIZE>>1;
  vtxv[30].tile=0xe4;

  for (i=0;i<SPR->hpmax;i++) {
    struct ctm_vertex_sprite *vtx=vtxv+31+i;
    vtx->x=CTM_DISPK_WANTEDL+CTM_TILESIZE*i;
    vtx->y=CTM_TILESIZE+(CTM_TILESIZE>>1);
    vtx->tile=(i<SPR->hp)?0x24:0x25;
  }
  
  if (ctm_video_end_sprites(ctm_video.texid_sprites)<0) return -1;
  return 0;
}

/* Draw overlay text. Coin count, location name, item counts, clock.
 */

#define CTM_DISPK_COINHORZ ((CTM_TILESIZE*104)/16)
#define CTM_DISPK_TIMEHORZ ((CTM_TILESIZE*40)/16)

static int ctm_display_game_draw_overlay_text(struct ctm_display *display,struct ctm_sprite_player *SPR) {
  const struct ctm_state_data *state=ctm_state_data+ctm_state_for_pixel(SPR->hdr.x,SPR->hdr.y);
  
  if (ctm_video_begin_text(CTM_TILESIZE)<0) return -1;

  if (ctm_video_add_textf(CTM_DISPK_COINHORZ,CTM_TILESIZE>>1,0xffffffff,"%d",SPR->inventory[CTM_ITEM_COIN])<0) return -1;
  if (state->name) {
    int namec=0; while (state->name[namec]) namec++;
    if (ctm_video_add_text(state->name,namec,display->fbw-namec*(CTM_TILESIZE>>1),CTM_TILESIZE>>1,0xffffffff)<0) return -1;
  }
  if (ctm_video_add_textf(display->fbw-CTM_DISPK_TIMEHORZ,CTM_TILESIZE+(CTM_TILESIZE>>1),0xffffffff,
    "%02d%c%02d",ctm_game.time_remaining.min,(ctm_game.time_remaining.cs<80)?':':' ',ctm_game.time_remaining.s
  )<0) return -1;
  
  if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;

  if (ctm_video_begin_text(CTM_RESIZE(12))<0) return -1;
  if (SPR->inventory[SPR->item_primary]!=CTM_INVENTORY_UNLIMITED) {
    if (ctm_video_add_textf_centered(CTM_RESIZE(3),CTM_RESIZE(28),CTM_RESIZE(24),0,0xffff00ff,"%d",SPR->inventory[SPR->item_primary])<0) return -1;
  }
  if (SPR->inventory[SPR->item_secondary]!=CTM_INVENTORY_UNLIMITED) {
    if (ctm_video_add_textf_centered(CTM_RESIZE(27),CTM_RESIZE(28),CTM_RESIZE(24),0,0xffff00ff,"%d",SPR->inventory[SPR->item_secondary])<0) return -1;
  }
  if (SPR->inventory[SPR->item_tertiary]!=CTM_INVENTORY_UNLIMITED) {
    if (ctm_video_add_textf_centered(CTM_RESIZE(51),CTM_RESIZE(28),CTM_RESIZE(24),0,0xffff00ff,"%d",SPR->inventory[SPR->item_tertiary])<0) return -1;
  }
  if (ctm_video_end_text(ctm_video.texid_tinyfont)<0) return -1;
  
  return 0;
}

/* Draw item store in pause menu.
 */

static int ctm_display_game_draw_item_store(struct ctm_display *display,struct ctm_sprite_player *SPR) {

  // Establish full output geometry.
  int storew=CTM_TILESIZE*2*DISPLAY->itemstore_colc;
  int storeh=CTM_TILESIZE*2*DISPLAY->itemstore_rowc;
  int storex=(display->fbw>>1)-(storew>>1);
  int storey=CTM_TILESIZE*3;
  if (ctm_draw_rect(storex,storey,storew,storeh,0xffffff40)<0) return -1;
  storex+=CTM_TILESIZE; // center of top left tile
  storey+=CTM_TILESIZE;

  // Draw the items and the cursor.
  if (ctm_video_begin_sprites()<0) return -1;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(9+16+2);
  if (!vtxv) return -1;
  struct ctm_vertex_sprite *itemvtx=vtxv+9;
  int i; for (i=0;i<27;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=0xff;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }

  int storep=0;
  int selx=0,sely=0;
  int row; for (row=0;row<DISPLAY->itemstore_rowc;row++) {
    int col; for (col=0;col<DISPLAY->itemstore_colc;col++,itemvtx++,storep++) {
      itemvtx->x=storex+col*CTM_TILESIZE*2;
      itemvtx->y=storey+row*CTM_TILESIZE*2;
      itemvtx->tile=0xf0+SPR->item_store[storep];
      if ((col==DISPLAY->selx)&&(row==DISPLAY->sely)) { selx=itemvtx->x; sely=itemvtx->y; }
    }
  }

  vtxv[25].x=(display->fbw>>1)-(CTM_TILESIZE>>1);
  vtxv[25].y=storey+storeh;
  vtxv[25].tile=0xd7;
  vtxv[26].x=vtxv[25].x+CTM_TILESIZE;
  vtxv[26].y=vtxv[25].y;
  vtxv[26].tile=0xd8;
  if (SPR->wants_termination) {
    vtxv[25].r=vtxv[26].r=0xff;
    vtxv[25].g=vtxv[26].g=0xff;
    vtxv[25].b=vtxv[26].b=0x00;
  } else {
    vtxv[25].r=vtxv[26].r=0x40;
    vtxv[25].g=vtxv[26].g=0x20;
    vtxv[25].b=vtxv[26].b=0x00;
  }
  if (DISPLAY->sely==DISPLAY->itemstore_rowc) { selx=vtxv[25].x+(CTM_TILESIZE>>1); sely=vtxv[25].y; }

  int vtxp=0;
  int subrow; for (subrow=-1;subrow<=1;subrow++) {
    int subcol; for (subcol=-1; subcol<=1;subcol++,vtxp++) {
      vtxv[vtxp].x=selx+subcol*CTM_TILESIZE;
      vtxv[vtxp].y=sely+subrow*CTM_TILESIZE;
      vtxv[vtxp].tile=(0x9e)+subrow*16+subcol;
    }
  }
  if (ctm_video_end_sprites(ctm_video.texid_sprites)<0) return -1;

  // Draw item quantities.
  if (ctm_video_begin_text(CTM_RESIZE(12))<0) return -1;
  for (i=0;i<16;i++) {
    int itemid=SPR->item_store[i];
    int inventory=SPR->inventory[itemid];
    if (inventory<0) continue;
    int col=i%DISPLAY->itemstore_colc;
    int row=i/DISPLAY->itemstore_colc;
    int x=storex+(col*CTM_TILESIZE*2);
    int y=storey+(row*CTM_TILESIZE*2)+CTM_RESIZE(10);
    if (ctm_video_add_textf_centered(x,y,0,0,0xffff00ff,"%d",inventory)<0) return -1;
  }
  if (ctm_video_end_text(ctm_video.texid_tinyfont)<0) return -1;
  
  return 0;
}

/* Draw framebuffer while paused.
 */

static int ctm_display_game_draw_pause(struct ctm_display *display,struct ctm_sprite_player *SPR) {

  /* Clear to a darker version of the party's color. */
  uint32_t partycolor=ctm_party_color[SPR->party];
  glClearColor((partycolor>>24)/512.0,((partycolor>>16)&0xff)/512.0,((partycolor>>8)&0xff)/512.0,1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  /* If we have a report, draw it in centered in whatever vertical space remains. */
  if (DISPLAY->have_report) {
    int rptx=(display->fbw>>1)-(DISPLAY->reportw>>1);
    int availy=(CTM_TILESIZE<<1)+(DISPLAY->itemstore_rowc*CTM_TILESIZE*2)+(CTM_TILESIZE<<1);
    int availh=display->fbh-availy;
    int rpty=availy+(availh>>1)-(DISPLAY->reporth>>1);
    if (ctm_draw_texture(rptx,rpty,DISPLAY->reportw,DISPLAY->reporth,DISPLAY->texid_report,1)<0) return -1;
  }

  /* Draw interactive item store. */
  if (ctm_display_game_draw_item_store(display,SPR)<0) return -1;
  
  return 0;
}

/* Draw "WIN!" or "LOSE!".
 */

static int ctm_display_game_draw_WINLOSE(struct ctm_display *display,struct ctm_sprite_player *SPR,uint8_t fgalpha,int y0) {
  const struct ctm_election_result *result=ctm_game.result;
  if (ctm_video_begin_sprites()<0) return -1;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(10);
  if (!vtxv) return -1;
  int tilebase;
  if (result->elecblue>result->elecred) tilebase=(SPR->party==CTM_PARTY_BLUE)?0x00:0x20;
  else if (result->elecblue<result->elecred) tilebase=(SPR->party==CTM_PARTY_RED)?0x00:0x20;
  else tilebase=0xb0;
  uint8_t r=ctm_party_color[SPR->party]>>24;
  uint8_t g=ctm_party_color[SPR->party]>>16;
  uint8_t b=ctm_party_color[SPR->party]>>8;
  int x0=(display->fbw>>1)-((CTM_TILESIZE*5)>>1)+(CTM_TILESIZE>>1);
  int row; for (row=0;row<2;row++) {
    int col; for (col=0;col<5;col++) {
      vtxv->x=x0+col*CTM_TILESIZE;
      vtxv->y=y0+row*CTM_TILESIZE;
      vtxv->tile=tilebase+row*16+col;
      vtxv->r=r; vtxv->g=g; vtxv->b=b;
      vtxv->a=fgalpha;
      vtxv->ta=0;
      vtxv->flop=0;
      vtxv++;
    }
  }
  if (ctm_video_end_sprites(ctm_video.texid_uisprites)<0) return -1;
  return 0;
}

/* Draw final statistics. Award, etc.
 */

static int ctm_display_game_draw_final_stats(struct ctm_display *display,struct ctm_sprite_player *SPR,uint8_t fgalpha,int y) {

  if (ctm_video_begin_text(CTM_RESIZE(16))<0) return -1;
  if (ctm_video_add_textf_centered(0,y,display->fbw,CTM_RESIZE(16),0xffffff00|fgalpha,"        Deaths: %-4d",SPR->deaths)<0) return -1; 
  y+=CTM_RESIZE(16);
  if (ctm_game.playerc>1) {
    if (ctm_video_add_textf_centered(0,y,display->fbw,CTM_RESIZE(16),0xffffff00|fgalpha,"Mummies killed: %-4d",SPR->mummykills)<0) return -1; 
    y+=CTM_RESIZE(16);
  }
  if (ctm_video_add_textf_centered(0,y,display->fbw,CTM_RESIZE(16),0xffffff00|fgalpha," Bosses killed: %-4d",SPR->bosskills)<0) return -1;
  y+=CTM_RESIZE(16);
  if (ctm_video_add_textf_centered(0,y,display->fbw,CTM_RESIZE(16),0xffffff00|fgalpha," Voters killed: %-4d",SPR->voterkills)<0) return -1; 
  y+=CTM_RESIZE(16);
  if (ctm_video_add_textf_centered(0,y,display->fbw,CTM_RESIZE(16),0xffffff00|fgalpha,"   Other kills: %-4d",SPR->beastkills+SPR->copkills)<0) return -1; 
  y+=CTM_RESIZE(16);
  
  const char *awardname=ctm_award_name[SPR->award&15];
  int awardnamec=0; while (awardname[awardnamec]) awardnamec++;
  int contentw=CTM_RESIZE(16+4)+awardnamec*CTM_RESIZE(8);
  int x=(display->fbw>>1)-(contentw>>1);
  if (ctm_video_add_text(awardname,awardnamec,x+CTM_RESIZE(16),y+CTM_RESIZE(8),0xffff0000|fgalpha)<0) return -1;

  if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;

  if (ctm_video_begin_sprites()<0) return -1;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(1);
  if (!vtxv) return -1;
  vtxv->x=x;
  vtxv->y=y+CTM_RESIZE(8);
  vtxv->tile=0xa0+SPR->award;
  vtxv->r=vtxv->g=vtxv->b=0x80;
  vtxv->a=fgalpha;
  vtxv->ta=0;
  vtxv->flop=0;
  if (ctm_video_end_sprites(ctm_video.texid_uisprites)<0) return -1;

  return 0;
}

/* Draw overlay when game ends, before we switch to the election results.
 */

static int ctm_display_game_draw_pregameover(struct ctm_display *display,struct ctm_sprite_player *SPR) {

  int age=(ctm_get_time()-ctm_game.endtime)/10000; // centiseconds
  uint8_t bgalpha=(age<100)?((age*0xc0)/100):0xe0;
  uint8_t fgalpha=(age<100)?((age*0xff)/100):0xff;
  if (ctm_draw_rect(0,0,display->fbw,display->fbh,0x00000000|bgalpha)<0) return -1;

  int contenth=(CTM_TILESIZE*2)+CTM_RESIZE(10)+(CTM_RESIZE(16)*6);
  int y0=(display->fbh>>1)-(contenth>>1);

  if (SPR&&ctm_game.result) {
    if (ctm_display_game_draw_WINLOSE(display,SPR,fgalpha,y0)<0) return -1;
  }
  if (SPR) {
    if (ctm_display_game_draw_final_stats(display,SPR,fgalpha,y0+(CTM_TILESIZE*2)+CTM_RESIZE(10))<0) return -1;
  }
  
  return 0;
}

/* Draw framebuffer.
 */

static int _ctm_display_game_draw_fb(struct ctm_display *display) {

  /* Find player sprite. If paused, follow a completely different track. */
  struct ctm_sprite *spr=ctm_sprite_for_player(DISPLAY->playerid);
  struct ctm_sprite_player *SPR=0;
  if (spr&&(spr->type==&ctm_sprtype_player)) SPR=(struct ctm_sprite_player*)spr;
  
  if (SPR&&SPR->pause) {
    if (ctm_display_game_draw_pause(display,SPR)<0) return -1;
    
  } else {

    /* Update scroll and interior. */
    if (spr) {
      DISPLAY->interior=spr->interior;
      DISPLAY->scrollx=spr->x-(display->fbw>>1);
      DISPLAY->scrolly=spr->y-(display->fbh>>1)-CTM_TILESIZE; // Our top bar is two tiles high, so center with that in mind.
      int limit=ctm_grid.colc*CTM_TILESIZE-display->fbw;
      if (DISPLAY->scrollx<0) DISPLAY->scrollx=0;
      else if (DISPLAY->scrollx>limit) DISPLAY->scrollx=limit;
      limit=ctm_grid.rowc*CTM_TILESIZE-display->fbh;
      if (DISPLAY->scrolly<0) DISPLAY->scrolly=0;
      else if (DISPLAY->scrolly>limit) DISPLAY->scrolly=limit;
    }

    /* Draw grid, then sprites. */
    if (ctm_display_draw_grid(display,DISPLAY->scrollx,DISPLAY->scrolly,DISPLAY->interior)<0) return -1;
    struct ctm_sprgrp *group=DISPLAY->interior?&ctm_group_interior:&ctm_group_exterior;
    if (ctm_display_draw_sprites(display,DISPLAY->scrollx,DISPLAY->scrolly,group,SPR?SPR->lens_of_truth:0)<0) return -1;

    /* If player exists and is dead, highlight the nearest Fountain of Youth. */
    if (SPR&&!SPR->hp) {
      if (ctm_display_game_draw_foy_indicator(display)<0) return -1;
    }

  } // End of not-paused.

  /* Draw overlay elements, like equipped items and clock. Only if player exists. */
  if (SPR) {
    if (ctm_draw_rect(0,0,display->fbw,CTM_TILESIZE<<1,0x00000040)<0) return -1;
    if (ctm_display_game_draw_overlay_sprites(display,SPR)<0) return -1;
    if (ctm_display_game_draw_overlay_text(display,SPR)<0) return -1;
  }

  /* On top of everything else, draw "WIN!" or "LOSE!" if we are in pre-game-over. */
  if (ctm_game.phase==CTM_GAME_PHASE_PREGAMEOVER) {
    if (ctm_display_game_draw_pregameover(display,SPR)<0) return -1;
  }
  
  return 0;
}

/* Type definition.
 */

const struct ctm_display_type ctm_display_type_game={
  .name="game",
  .objlen=sizeof(struct ctm_display_game),
  .del=_ctm_display_game_del,
  .init=_ctm_display_game_init,
  .resized=_ctm_display_game_resized,
  .draw_fb=_ctm_display_game_draw_fb,
};

/* Public property accessors.
 */
 
int ctm_display_game_set_playerid(struct ctm_display *display,int playerid) {
  if (!display||(display->type!=&ctm_display_type_game)||(playerid<0)||(playerid>4)) return -1;
  if (DISPLAY->playerid==playerid) return 0;
  DISPLAY->playerid=playerid;
  return 0;
}

int ctm_display_game_get_playerid(const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_game)) return -1;
  return DISPLAY->playerid;
}

int ctm_display_game_get_position(int *x,int *y,int *w,int *h,int *interior,const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_game)) return -1;
  if (x) *x=DISPLAY->scrollx;
  if (y) *y=DISPLAY->scrolly;
  if (w) *w=display->fbw;
  if (h) *h=display->fbh;
  if (interior) *interior=DISPLAY->interior;
  return 0;
}

/* Pause menu selection.
 */
 
int ctm_display_game_move_cursor(struct ctm_display *display,int dx,int dy) {
  if (!display||(display->type!=&ctm_display_type_game)) return -1;
  int rowc=DISPLAY->itemstore_rowc+1;
  DISPLAY->selx+=dx;
  DISPLAY->sely+=dy;
  if (DISPLAY->sely<0) DISPLAY->sely=rowc-1;
  else if (DISPLAY->sely>=rowc) DISPLAY->sely=0;
  if (DISPLAY->sely<DISPLAY->itemstore_rowc) {
    if (DISPLAY->selx<0) DISPLAY->selx=DISPLAY->itemstore_colc-1;
    else if (DISPLAY->selx>=DISPLAY->itemstore_colc) DISPLAY->selx=0;
  } else switch (DISPLAY->sely-DISPLAY->itemstore_rowc) {
    case 0: break; // "vote now"
  }
  return 0;
}
  
int ctm_display_game_get_selected_index(const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_game)) return -1;
  if (DISPLAY->sely<0) return 0;
  if (DISPLAY->sely<DISPLAY->itemstore_rowc) {
    if ((DISPLAY->selx<0)||(DISPLAY->selx>=DISPLAY->itemstore_colc)) return 0;
    return DISPLAY->sely*DISPLAY->itemstore_colc+DISPLAY->selx;
  } else switch (DISPLAY->sely-DISPLAY->itemstore_rowc) {
    case 0: return CTM_DISPLAY_GAME_INDEX_VOTE;
  }
  return 0;
}

/* Draw national map with optional highlight point.
 */

static int ctm_display_game_draw_national_map(struct ctm_display *display,struct ctm_sprite *spr) {
  int h=CTM_TILESIZE*3+CTM_RESIZE(4);
  int y=(DISPLAY->reporth>>1)-(h>>1);
  if (ctm_draw_rect(0,y,h,h,0xffffffff)<0) return -1;

  if (ctm_video_begin_sprites()<0) return -1;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(9+(spr?1:0));
  if (!vtxv) return -1;

  int i;
  for (i=0;i<9;i++) {
    int col=i%3,row=i/3;
    vtxv[i].x=CTM_RESIZE(2)+CTM_TILESIZE*col+(CTM_TILESIZE>>1);
    vtxv[i].y=CTM_RESIZE(2)+y+CTM_TILESIZE*row+(CTM_TILESIZE>>1);
    vtxv[i].tile=0xba+(row*16)+col;
    switch (ctm_state_prediction[i]) {
      case -2: vtxv[i].r=0x00; vtxv[i].g=0x00; vtxv[i].b=0xff; break;
      case -1: vtxv[i].r=0x40; vtxv[i].g=0x40; vtxv[i].b=0x80; break;
      case  1: vtxv[i].r=0x80; vtxv[i].g=0x40; vtxv[i].b=0x40; break;
      case  2: vtxv[i].r=0xff; vtxv[i].g=0x00; vtxv[i].b=0x00; break;
      default: vtxv[i].r=0x80; vtxv[i].g=0x80; vtxv[i].b=0x80; break;
    }
    vtxv[i].a=0xff;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }

  if (spr) {
    vtxv[9].x=(spr->x*CTM_TILESIZE*3)/(ctm_grid.colc*CTM_TILESIZE);
    vtxv[9].y=y+(spr->y*CTM_TILESIZE*3)/(ctm_grid.rowc*CTM_TILESIZE);
    vtxv[9].tile=0xd9;
    vtxv[9].a=0xff;
    vtxv[9].ta=0;
    vtxv[9].flop=0;
  }
  
  if (ctm_video_end_sprites(ctm_video.texid_sprites)<0) return -1;
  return 0;
}

/* Draw voter sentiment graph for one state.
 */

static int ctm_display_game_draw_sentiment(struct ctm_display *display,int x,int y,int w,int h,int stateix) {
  if (ctm_scatter_graph_draw(x,y,w,h,ctm_group_state_voter+stateix)<0) return -1;
  return 0;
}

/* Draw summary graph for one state.
 */

static int ctm_display_game_draw_state_summary(struct ctm_display *display,int x,int y,int w,int h,int stateix) {
  struct ctm_poll_result result[7]={0};
  if (ctm_conduct_poll(result,stateix,15)<0) return -1;
  int dkbluew,ltbluew,grayw,ltredw,dkredw;
  if (result[0].total<1) {
    dkbluew=ltbluew=ltredw=dkredw=0;
  } else {
    dkbluew=(result[0].bluec*w)/result[0].total;
    ltbluew=(result[0].subbluec*w)/result[0].total;
    ltredw=(result[0].subredc*w)/result[0].total;
    dkredw=(result[0].redc*w)/result[0].total;
  }
  grayw=w-dkbluew-ltbluew-ltredw-dkredw;
  if (ctm_draw_rect(x,y,dkbluew,h,0x0000ffff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew,y,ltbluew,h,0x404080ff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew+ltbluew,y,grayw,h,0x808080ff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew+ltbluew+grayw,y,ltredw,h,0x804040ff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew+ltbluew+grayw+ltredw,y,dkredw,h,0xff0000ff)<0) return -1;
  if (ctm_draw_rect(x+(w>>1),y,1,h,0x000000ff)<0) return -1;
  return 0;
}

/* Draw summary graph for entire country (popular vote).
 */

static int ctm_display_game_draw_national_summary(struct ctm_display *display,int x,int y,int w,int h) {
  struct ctm_poll_result result[7]={0};
  if (ctm_conduct_poll(result,-1,15)<0) return -1;
  int dkbluew,ltbluew,grayw,ltredw,dkredw;
  if (result[0].total<1) {
    dkbluew=ltbluew=ltredw=dkredw=0;
  } else {
    dkbluew=(result[0].bluec*w)/result[0].total;
    ltbluew=(result[0].subbluec*w)/result[0].total;
    ltredw=(result[0].subredc*w)/result[0].total;
    dkredw=(result[0].redc*w)/result[0].total;
  }
  grayw=w-dkbluew-ltbluew-ltredw-dkredw;
  if (ctm_draw_rect(x,y,dkbluew,h,0x0000ffff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew,y,ltbluew,h,0x404080ff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew+ltbluew,y,grayw,h,0x808080ff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew+ltbluew+grayw,y,ltredw,h,0x804040ff)<0) return -1;
  if (ctm_draw_rect(x+dkbluew+ltbluew+grayw+ltredw,y,dkredw,h,0xff0000ff)<0) return -1;
  if (ctm_draw_rect(x+(w>>1),y,1,h,0x000000ff)<0) return -1;
  return 0;
}

/* Draw state population pie chart.
 */

static int ctm_display_game_draw_population(struct ctm_display *display,int x,int y,int w,int stateix) {
  if (w<5) return 0;
  if (ctm_draw_pie(x+(w>>1),y+(w>>1),w>>1,0.0,M_PI*2.0,0x00000080)<0) return -1;
  x+=1; y+=1; w-=2;
  if ((stateix<0)||(stateix>=9)) return 0;
  int percolor[7]={0},total=0,i;
  for (i=0;i<ctm_group_state_voter[stateix].sprc;i++) {
    struct ctm_sprite *spr=ctm_group_state_voter[stateix].sprv[i];
    if (spr->type!=&ctm_sprtype_voter) continue;
    struct ctm_sprite_voter *SPR=(struct ctm_sprite_voter*)spr;
    if ((SPR->party<0)||(SPR->party>=7)) continue;
    percolor[SPR->party]++;
    total++;
  }
  if (!total) return 0;
  w>>=1;
  x+=w; y+=w;
  int perc=7; while (perc&&!percolor[perc-1]) perc--;
  double t=0.0,nextt=0.0;
  for (i=0;i<perc;i++) {
    if (!percolor[i]) continue;
    if (i==perc-1) nextt=M_PI*2.0;
    else nextt=t+(percolor[i]*M_PI*2.0)/total;
    if (ctm_draw_pie(x,y,w,t,nextt,ctm_party_color[i])<0) return -1;
    t=nextt;
  }
  return 0;
}

/* Rebuild report.
 */

static int ctm_display_game_draw_report(struct ctm_display *display) {

  struct ctm_sprite *spr=ctm_sprite_for_player(DISPLAY->playerid);
  struct ctm_sprite_player *SPR=0;
  if (spr&&(spr->type==&ctm_sprtype_player)) SPR=(struct ctm_sprite_player*)spr;
  int stateix=-1;
  if (spr) stateix=ctm_state_for_pixel(spr->x,spr->y);

  /* The report texture is always very wide, and at least 3 tiles tall.
   * That's not explicitly ensured, but it always plays out so (due to the master fb size requirements).
   * The precise geometry is not known until runtime.
   */

  int mapw=CTM_TILESIZE*3+CTM_RESIZE(4);
  int piew=CTM_TILESIZE*3;
  int lblx=mapw+piew;
  int lblw=(CTM_TILESIZE*3+1)/2;
  int barw=DISPLAY->reportw-lblw-lblx;
  if (barw<3) return 0;
  int sentimenty=DISPLAY->reporth/6; // midpoints of the three text rows
  int statey=DISPLAY->reporth>>1;
  int nationaly=DISPLAY->reporth-sentimenty;
  
  uint32_t partycolor=ctm_party_color[SPR->party];
  glClearColor((partycolor>>24)/512.0,((partycolor>>16)&0xff)/512.0,((partycolor>>8)&0xff)/512.0,1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (ctm_display_game_draw_national_map(display,spr)<0) return -1;
  if (ctm_display_game_draw_population(display,mapw+1,(DISPLAY->reporth>>1)-((CTM_TILESIZE*3)>>1),piew,stateix)<0) return -1;

  if (ctm_video_begin_text(CTM_TILESIZE)<0) return -1;
  if (stateix>=0) {
    if (ctm_video_add_text(ctm_state_data[stateix].abbreviation,2,lblx+(CTM_TILESIZE>>2)+4,sentimenty,0xffffffff)<0) return -1;
    if (ctm_video_add_text(ctm_state_data[stateix].abbreviation,2,lblx+(CTM_TILESIZE>>2)+4,statey,0xffffffff)<0) return -1;
  } else {
    if (ctm_video_add_text("--",2,lblx+(CTM_TILESIZE>>2)+4,sentimenty,0xffffffff)<0) return -1;
    if (ctm_video_add_text("--",2,lblx+(CTM_TILESIZE>>2)+4,statey,0xffffffff)<0) return -1;
  }
  if (ctm_video_add_text("US",2,lblx+(CTM_TILESIZE>>2)+4,nationaly,0xffffffff)<0) return -1;
  if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;

  if (stateix>=0) {
    if (ctm_display_game_draw_sentiment(display,lblx+lblw,sentimenty-(CTM_TILESIZE>>1),barw,CTM_TILESIZE,stateix)<0) return -1;
    if (ctm_display_game_draw_state_summary(display,lblx+lblw,statey-(CTM_TILESIZE>>1),barw,CTM_TILESIZE,stateix)<0) return -1;
  }
  if (ctm_display_game_draw_national_summary(display,lblx+lblw,nationaly-(CTM_TILESIZE>>1),barw,CTM_TILESIZE)<0) return -1;

  
  return 1;
}
 
int ctm_display_game_rebuild_report(struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_game)) return -1;
  if ((DISPLAY->reportw<1)||(DISPLAY->reporth<1)) return 0; // We don't do reports. No worries.
  glBindFramebuffer(GL_FRAMEBUFFER,DISPLAY->fb_report);
  ctm_video_set_uniform_screen_size(DISPLAY->reportw,DISPLAY->reporth);
  int err=ctm_display_game_draw_report(display);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  ctm_video_reset_uniform_screen_size();
  if (err<=0) return err;
  DISPLAY->have_report=1;
  return 0;
}
