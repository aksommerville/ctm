#include "ctm.h"
#include "ctm_display.h"
#include "video/ctm_video_internal.h"
#include "game/ctm_grid.h"
#include "game/ctm_game.h"
#include "game/ctm_geography.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "misc/ctm_time.h"
#include <math.h>

#define CTM_SCATTER_COLOR_BLACKOUT 0x000000ff
#define CTM_SCATTER_COLOR_BORDER   0x000000ff
#define CTM_SCATTER_COLOR_MIDLINE  0x00000080

static const uint32_t ctm_scatter_colors_base[7]={
  0x606060ff, // gray
  0x808080ff, // white
  0x505050ff, // black
  0x808040ff, // yellow
  0x408040ff, // green
  0x303060ff, // blue
  0x804040ff, // red
};

static const uint32_t ctm_scatter_colors_lit[7]={
  0xa0a0a0a0, // gray
  0xffffffa0, // white
  0x000000a0, // black
  0xffff00a0, // yellow
  0x00ff00a0, // green
  0x4080ffc0, // blue
  0xff0000a0, // red
};

/* Draw scatter graph with pure GL, main entry point.
 */
 
int ctm_scatter_graph_draw(
  int x,int y,int w,int h,
  struct ctm_sprgrp *grp
) {
  int i;

  /* Ensure sane arguments. */
  if (!grp) grp=&ctm_group_voter;
  if ((w<1)||(h<1)) return 0;

  /* If too small, black it out and return. 
   * We want a 1-pixel margin on all sides, 7 internal rows (each color), and at least 3 internal columns.
   */
  if ((w<5)||(h<9)) {
    if (ctm_draw_rect(x,y,w,h,CTM_SCATTER_COLOR_BLACKOUT)<0) return -1;
    return 0;
  }

  /* Fill background to produce the border, then trim border off given boundaries. */
  if (ctm_draw_rect(x,y,w,h,CTM_SCATTER_COLOR_BORDER)<0) return -1;
  x++; y++; w-=2; h-=2;

  /* Draw background bars for each color. */
  int top=y;
  for (i=0;i<7;i++) {
    int bottom=y+((i+1)*h)/7;
    if (ctm_draw_rect(x,top,w,bottom-top,ctm_scatter_colors_base[i])<0) return -1;
    top=bottom;
  }

  /* Draw a rectangle for each voter. */
  int lightw=1+w/256;
  for (i=0;i<grp->sprc;i++) {
    struct ctm_sprite *spr=grp->sprv[i];
    if (spr->type!=&ctm_sprtype_voter) continue;
    struct ctm_sprite_voter *SPR=(struct ctm_sprite_voter*)spr;
    if ((SPR->party<0)||(SPR->party>6)) continue;
    int lightx=((SPR->decision+128)*(w-lightw))/256;
    if (lightx<0) lightx=0;
    else if (lightx+lightw>w) lightx=w-lightw;
    int lighty=(SPR->party*h)/7;
    int lighth=((SPR->party+1)*h)/7-lighty;
    if (ctm_draw_rect(x+lightx,y+lighty,lightw,lighth,ctm_scatter_colors_lit[SPR->party])<0) return -1;
  }

  /* Put a skinny vertical line right in the middle. */
  if (ctm_draw_rect(x+(w>>1),y,1,h,CTM_SCATTER_COLOR_MIDLINE)<0) return -1;

  return 0;
}

/* Choose graph height.
 * The ideal height is two greater than a multiple of seven; the closest to (prefh) and within (limith) wins.
 */
 
int ctm_scatter_graph_choose_height(int prefh,int limith) {

  if (limith<0) return 0;
  if (limith<9) return limith;
  if (prefh<0) prefh=0;
  if (prefh>limith) prefh=limith;

  prefh-=2;
  int mod=prefh&7;
  if (mod>=3) { // try higher first
    int h=2+prefh+(7-mod);
    if (h<=limith) return h;
  }
  int h=2+prefh-mod;
  if (h>2) return h;
  h=2+prefh+(7-mod);
  if (h<=limith) return h;
  return prefh;
}
