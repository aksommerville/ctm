#include "ctm.h"
#include "misc/ctm_time.h"
#include "misc/akpng.h"
#include "io/ctm_fs.h"
#include "io/ctm_poll.h"
#include "input/ctm_input.h"
#include "video/ctm_video.h"
#include "display/ctm_display.h"
#include "game/ctm_grid.h"
#include <unistd.h>
#include <time.h>

#include <signal.h>
#ifndef SIGQUIT
  #define SIGQUIT -1
#endif

// If we're using SDL, <SDL.h> must be included wherever main() is defined.
#if CTM_USE_sdl
  #include <SDL.h>
#endif

/* Globals.
 */

static struct {

  uint16_t pvbtns;
  struct ctm_editor_input pvinput;
  int ptrcol,ptrrow;
  int selcol,selrow;
  int dx,dy;
  int dirty;
  int autojoin;

} ctm_editor={0};

/* Signals.
 */

static volatile int ctm_sigc=0;

static void ctm_rcvsig(int sigid) {
  switch (sigid) {
    case SIGTERM: case SIGQUIT: case SIGINT: {
        if (++ctm_sigc>=3) {
          fprintf(stderr,"ctm: Too many pending signals.\n");
          exit(1);
        }
      } break;
  }
}

/* Init.
 */

static int ctm_init() {

  signal(SIGINT,ctm_rcvsig);
  signal(SIGTERM,ctm_rcvsig);
  signal(SIGQUIT,ctm_rcvsig);

  /* Bring subsystems online. */
  if (ctm_video_init(1)<0) return -1;
  if (ctm_poll_init()<0) return -1;
  if (ctm_input_init(1)<0) return -1;
  if (ctm_display_init()<0) return -1;

  if (ctm_grid_init()<0) return -1;
  if (ctm_display_rebuild_editor()<0) return -1;

  return 0;
}

/* Quit.
 */

static void ctm_quit() {
  ctm_grid_quit();
  ctm_input_quit();
  ctm_poll_quit();
  ctm_display_quit();
  ctm_video_quit();
}

/* Save changes if necessary.
 */

static int ctm_editor_save() {
  if (!ctm_editor.dirty) return 0;
  if (ctm_grid_save()<0) return -1;
  ctm_editor.dirty=0;
  return 0;
}

/* Tile groups.
 */

static int ctm_editor_tile_group(uint8_t tile) {
  int trow=tile>>4,tcol=tile&15;
  struct ctm_display *display=ctm_display_get_editor();
  if (!display) return 0;
  if (ctm_display_editor_get_interior(display)) { // *** Interior ***
    if ((trow>=1)&&(trow<=3)&&(tcol>=8)&&(tcol<=12)) return 13;
  
  } else { // *** Exterior *** 12 (5x3) groups laid out in a grid covering (0,0)..(14,12). Groups 1..12.
    if ((trow>=1)&&(trow<=12)&&(tcol<15)) {
      int gcol=tcol/5,grow=(trow-1)/3;
      return 1+grow*3+gcol;
    }
  }
  return 0;
}

static int ctm_editor_tile_group_at(int col,int row) {
  if (col<0) return 0;
  if (row<0) return 0;
  if (col>=ctm_grid.colc) return 0;
  if (row>=ctm_grid.rowc) return 0;
  struct ctm_display *display=ctm_display_get_editor();
  if (!display) return 0;
  if (ctm_display_editor_get_interior(display)) return ctm_editor_tile_group(ctm_grid.cellv[row*ctm_grid.colc+col].itile);
  return ctm_editor_tile_group(ctm_grid.cellv[row*ctm_grid.colc+col].tile);
}

/* Check neighbors for modified cell.
 */

static int ctm_editor_check_neighbors(int col,int row,int group) {
  struct ctm_display *display=ctm_display_get_editor();
  if (!display) return 0;
  if (ctm_editor_tile_group_at(col,row)!=group) return 0;
  uint8_t neighbors=0,mask=0x80;
  int dy; for (dy=-1;dy<=1;dy++) {
    int dx; for (dx=-1;dx<=1;dx++) {
      if (!dx&&!dy) continue;
      if (ctm_editor_tile_group_at(col+dx,row+dy)==group) neighbors|=mask;
      mask>>=1;
    }
  }
  uint8_t *dst;
  if (ctm_display_editor_get_interior(display)) dst=&ctm_grid.cellv[row*ctm_grid.colc+col].itile;
  else dst=&ctm_grid.cellv[row*ctm_grid.colc+col].tile;
  uint8_t gbase;
  int style=0;
  
  if ((group>=1)&&(group<=12)) { // 5x3 groups in exterior.
    int grow=(group-1)/3;
    int gcol=(group-1)%3;
    gbase=0x10+grow*48+gcol*5;
    style=53;
  } else if (group==13) { // Special carpet, interior.
    gbase=0x18;
    style=53;
  }

  switch (style) {
  
    case 53: {
             if ((neighbors&0xff)==0xff) *dst=gbase+0x11;
        else if ((neighbors&0xfe)==0xfe) *dst=gbase+0x03;
        else if ((neighbors&0xfb)==0xfb) *dst=gbase+0x04;
        else if ((neighbors&0xdf)==0xdf) *dst=gbase+0x13;
        else if ((neighbors&0x7f)==0x7f) *dst=gbase+0x14;
        else if ((neighbors&0x1f)==0x1f) *dst=gbase+0x01;
        else if ((neighbors&0x6b)==0x6b) *dst=gbase+0x10;
        else if ((neighbors&0xd6)==0xd6) *dst=gbase+0x12;
        else if ((neighbors&0xf8)==0xf8) *dst=gbase+0x21;
        else if ((neighbors&0x0b)==0x0b) *dst=gbase+0x00;
        else if ((neighbors&0x16)==0x16) *dst=gbase+0x02;
        else if ((neighbors&0x68)==0x68) *dst=gbase+0x20;
        else if ((neighbors&0xd0)==0xd0) *dst=gbase+0x22;
        else *dst=gbase+0x23;
      } break;
      
  }
  return 0;
}

/* Edit grid.
 */

static int ctm_editor_copy_selection(int col,int row) {
  struct ctm_display *display=ctm_display_get_editor();
  if (!display) return -1;
  if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return 0;
  if ((col==ctm_editor.selcol)&&(row==ctm_editor.selrow)) return 0;
  int srcp=ctm_editor.selrow*ctm_grid.colc+ctm_editor.selcol;
  uint8_t srctile;
  if (ctm_display_editor_get_interior(display)) {
    srctile=ctm_grid.cellv[srcp].itile;
    ctm_grid.cellv[row*ctm_grid.colc+col].itile=srctile;
  } else {
    srctile=ctm_grid.cellv[srcp].tile;
    ctm_grid.cellv[row*ctm_grid.colc+col].tile=srctile;
  }
  ctm_editor.dirty=1;

  /* Auto-join neighbors. */
  if (ctm_editor.autojoin) {
    int group=ctm_editor_tile_group(srctile);
    if (group>0) {
      int dx; for (dx=-1;dx<=1;dx++) {
        int dy; for (dy=-1;dy<=1;dy++) {
          if (ctm_editor_check_neighbors(col+dx,row+dy,group)<0) return -1;
        }
      }
    }
  }
  
  return 0;
}

static int ctm_editor_modify_selection(int dx,int dy) {
  int cellp=ctm_editor.selrow*ctm_grid.colc+ctm_editor.selcol;
  uint8_t *tile;
  if (ctm_display_editor_get_interior(ctm_display_get_editor())) tile=&(ctm_grid.cellv[cellp].itile);
  else tile=&(ctm_grid.cellv[cellp].tile);
  int x=(*tile)&15;
  int y=(*tile)>>4;
  x=(x+dx)&15;
  y=(y+dy)&15;
  uint8_t ntile=(y<<4)|x;
  if (ntile==*tile) return 0;
  *tile=ntile;
  ctm_editor.dirty=1;
  return 0;
}

/* Poll input.
 */

static int ctm_editor_poll_input() {

  /* Look at joysticks. */
  if (ctm_input_by_playerid[0]!=ctm_editor.pvbtns) {
    uint16_t n=ctm_input_by_playerid[0],o=ctm_editor.pvbtns;
    
    if ((n&CTM_BTNID_UP)&&!(o&CTM_BTNID_UP)) ctm_editor.dy=-1;
    else if (!(n&CTM_BTNID_UP)&&(o&CTM_BTNID_UP)&&(ctm_editor.dy<0)) ctm_editor.dy=0;
    if ((n&CTM_BTNID_DOWN)&&!(o&CTM_BTNID_DOWN)) ctm_editor.dy=1;
    else if (!(n&CTM_BTNID_DOWN)&&(o&CTM_BTNID_DOWN)&&(ctm_editor.dy>0)) ctm_editor.dy=0;
    if ((n&CTM_BTNID_LEFT)&&!(o&CTM_BTNID_LEFT)) ctm_editor.dx=-1;
    else if (!(n&CTM_BTNID_LEFT)&&(o&CTM_BTNID_LEFT)&&(ctm_editor.dx<0)) ctm_editor.dx=0;
    if ((n&CTM_BTNID_RIGHT)&&!(o&CTM_BTNID_RIGHT)) ctm_editor.dx=1;
    else if (!(n&CTM_BTNID_RIGHT)&&(o&CTM_BTNID_RIGHT)&&(ctm_editor.dx>0)) ctm_editor.dx=0;

    if ((n&CTM_BTNID_PRIMARY)&&!(o&CTM_BTNID_PRIMARY)) ctm_editor.autojoin^=1;
    if ((n&CTM_BTNID_SECONDARY)&&!(o&CTM_BTNID_SECONDARY)) { if (ctm_display_editor_toggle_interior()<0) return -1; }
    
    ctm_editor.pvbtns=ctm_input_by_playerid[0];
  }

  /* Mouse moved? */
  if ((ctm_editor_input.ptrx!=ctm_editor.pvinput.ptrx)||(ctm_editor_input.ptry!=ctm_editor.pvinput.ptry)) {
    int col=0,row=0;
    if (ctm_display_editor_cell_from_coords(&col,&row,ctm_display_get_editor(),ctm_editor_input.ptrx,ctm_editor_input.ptry)>=0) {
      if ((col!=ctm_editor.ptrcol)||(row!=ctm_editor.ptrrow)) {
        ctm_editor.ptrcol=col;
        ctm_editor.ptrrow=row;
        if (ctm_editor_input.ptrleft) {
          if (ctm_editor_copy_selection(ctm_editor.ptrcol,ctm_editor.ptrrow)<0) return -1;
        }
      }
    }
  }

  /* Left mouse button: copy selection. (Also happens when mouse moves). */
  if (ctm_editor_input.ptrleft&&!ctm_editor.pvinput.ptrleft) {
    if (ctm_editor_copy_selection(ctm_editor.ptrcol,ctm_editor.ptrrow)<0) return -1;
  }

  /* Right mouse button: move selection. */
  if (ctm_editor_input.ptrright&&!ctm_editor.pvinput.ptrright) {
    if ((ctm_editor.ptrcol>=0)&&(ctm_editor.ptrcol<ctm_grid.colc)) {
      if ((ctm_editor.ptrrow>=0)&&(ctm_editor.ptrrow<ctm_grid.rowc)) {
        ctm_editor.selcol=ctm_editor.ptrcol;
        ctm_editor.selrow=ctm_editor.ptrrow;
        ctm_display_editor_set_selection(ctm_display_get_editor(),ctm_editor.selcol,ctm_editor.selrow);
      }
    }
  }

  /* Mouse wheels: modify value under selection. */
  if (ctm_editor_input.ptrwheelx||ctm_editor_input.ptrwheely) {
    if (ctm_editor_modify_selection(ctm_editor_input.ptrwheelx,ctm_editor_input.ptrwheely)<0) return -1;
    ctm_editor_input.ptrwheelx=0;
    ctm_editor_input.ptrwheely=0;
  }
  
  ctm_editor.pvinput=ctm_editor_input;
  return 0;
}

/* Scroll screen in response to joystick axes.
 * This also counts as mouse motion. (It's moving the paper, not the pencil -- effectively the same thing).
 */

static int ctm_editor_update_scroll() {
  if (!ctm_editor.dx&&!ctm_editor.dy) return 0;
  struct ctm_display *display=ctm_display_get_editor();
  if (!display) return -1;
  if (ctm_display_editor_scroll(display,ctm_editor.dx*8,ctm_editor.dy*8)<0) return -1;
  int col=0,row=0;
  if (ctm_display_editor_cell_from_coords(&col,&row,display,ctm_editor_input.ptrx,ctm_editor_input.ptry)>=0) {
    if ((col!=ctm_editor.ptrcol)||(row!=ctm_editor.ptrrow)) {
      ctm_editor.ptrcol=col;
      ctm_editor.ptrrow=row;
      if (ctm_editor_input.ptrleft) {
        if (ctm_editor_copy_selection(ctm_editor.ptrcol,ctm_editor.ptrrow)<0) return -1;
      }
    }
  }
  return 0;
}

/* Update.
 */

static int ctm_update() {
  if (ctm_sigc) return 0;
  if (ctm_poll_update()<0) return -1;
  if (ctm_input_update()<0) return -1;
  if (ctm_editor_poll_input()<0) return -1;
  if (ctm_editor_update_scroll()<0) return -1;
  if (ctm_display_draw()<0) return -1;
  return 1;
}

/* Draw map.
 */

static void ctm_editor_blit_tile(uint8_t *dst,int dststride,const uint8_t *src,int srcstride,int scale) {
  srcstride*=scale;
  int y; for (y=0;y<16;y+=scale) {
    int srcx,x; for (srcx=x=0;srcx<48;srcx+=scale*3,x+=3) {
      memcpy(dst+x,src+srcx,3);
    }
    dst+=dststride;
    src+=srcstride;
  }
}

// Scale must be in (1,2,4,8,16).
static int ctm_editor_draw_map_1(const uint8_t *srcpixels,int scale) {
  if ((scale<1)||(scale>16)||(16%scale)) return -1;
  int dsttilesize=16/scale;
  int dstw=ctm_grid.colc*dsttilesize;
  int dsth=ctm_grid.rowc*dsttilesize;
  int dststride=dstw*3;
  int srcstride=768;
  uint8_t *dst=malloc(dststride*dsth);
  if (!dst) return -1;
  int dsty=0;
  int row; for (row=0;row<ctm_grid.rowc;row++,dsty+=dsttilesize) {
    int dstx=0;
    int col; for (col=0;col<ctm_grid.colc;col++,dstx+=dsttilesize) {
      uint8_t tile=ctm_grid.cellv[row*ctm_grid.colc+col].tile;
      int srcx=(tile&15)*16;
      int srcy=(tile>>4)*16;
      ctm_editor_blit_tile(dst+dsty*dststride+dstx*3,dststride,srcpixels+srcy*srcstride+srcx*3,srcstride,scale);
    }
  }
  char dstpath[256];
  snprintf(dstpath,sizeof(dstpath),"map.%dx%d.rgb",dstw,dsth);
  int err=ctm_file_write(dstpath,dst,dststride*dsth);
  free(dst);
  if (err<0) return err;
  printf("Saved map image to '%s'.\n",dstpath);
  return 0;
}

static int ctm_editor_draw_map() {
  struct akpng png={0};
  if ((akpng_decode_file(&png,"data/exterior.png")<0)||(akpng_force_rgb(&png)<0)) {
    printf("Failed to acquire graphics.\n");
    akpng_cleanup(&png);
    return -1;
  }
  if ((png.w!=256)||(png.h!=256)) {
    printf("Expected 256x256 tilesheet, have %dx%d\n",png.w,png.h);
    akpng_cleanup(&png);
    return -1;
  }
  int err=ctm_editor_draw_map_1(png.pixels,4);
  akpng_cleanup(&png);
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {
  int err=0;

  if (argc>=1) ctm_set_argv0(argv[0]);

  srand(time(0));
  if ((err=ctm_init())<0) goto _done_;

  if (argc==2) {
    if (!strcmp(argv[1],"map")) { err=ctm_editor_draw_map(); goto _done_; }
  }

  while ((err=ctm_update())>0) ;
  if (err>=0) err=ctm_editor_save();

 _done_:
  ctm_quit();
  if (err<0) {
    fprintf(stderr,"ctm: Fatal error.\n");
    return 1;
  } else {
    fprintf(stderr,"ctm: Normal exit.\n");
    return 0;
  }
}
