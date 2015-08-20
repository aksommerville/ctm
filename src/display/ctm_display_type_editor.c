#include "ctm.h"
#include "ctm_display.h"
#include "video/ctm_video_internal.h"
#include "input/ctm_input.h"
#include "game/ctm_grid.h"

// Higher numbers to zoom in, or 1.0 to display at the monitor's native resolution.
// When playing, display is always zoomed in, though there's no rule as to how far (typically somewhere between 2 and 5...?)
// Editors normally want to see more than players.
// Fractional values are perfectly legal.
// We're not clearing the framebuffer, so please do ensure that the total viewable area is smaller than the world. Not that it really matters.
#define CTM_EDITOR_SCALE 2.00

/* Object type.
 */

struct ctm_display_editor {
  struct ctm_display hdr;
  int selx,sely;
  int scrollx,scrolly;
  int interior;
  int draw_state_lines;
  int draw_cursor;
  int draw_selection;
};

#define DISPLAY ((struct ctm_display_editor*)display)

/* Delete.
 */

static void _ctm_display_editor_del(struct ctm_display *display) {
}

/* Initialize.
 */

static int _ctm_display_editor_init(struct ctm_display *display) {
  DISPLAY->draw_state_lines=1;
  DISPLAY->draw_cursor=1;
  DISPLAY->draw_selection=1;
  return 0;
}

/* Resized.
 */

static int _ctm_display_editor_resized(struct ctm_display *display) {
  if (ctm_display_resize_fb(display,display->w/CTM_EDITOR_SCALE,display->h/CTM_EDITOR_SCALE)<0) return -1;
  return 0;
}

/* Draw framebuffer.
 */

static int _ctm_display_editor_draw_fb(struct ctm_display *display) {
  if (ctm_display_draw_grid(display,DISPLAY->scrollx,DISPLAY->scrolly,DISPLAY->interior)<0) return -1;
  return 0;
}

/* Draw state lines (main).
 */

static int ctm_display_editor_draw_state_lines(struct ctm_display *display) {
  int linex1=(ctm_grid.colc/3)*CTM_TILESIZE-DISPLAY->scrollx;
  int linex2=((ctm_grid.colc*2)/3)*CTM_TILESIZE-DISPLAY->scrollx;
  int liney1=(ctm_grid.rowc/3)*CTM_TILESIZE-DISPLAY->scrolly;
  int liney2=((ctm_grid.rowc*2)/3)*CTM_TILESIZE-DISPLAY->scrolly;
  if ((linex1>=0)&&(linex1<=display->fbw)) {
    if (display->fbw!=display->w) linex1=(linex1*display->w)/display->fbw;
    if (ctm_draw_rect(display->x+linex1,display->y,1,display->h,0xffff00ff)<0) return -1;
  }
  if ((linex2>=0)&&(linex2<=display->fbw)) {
    if (display->fbw!=display->w) linex2=(linex2*display->w)/display->fbw;
    if (ctm_draw_rect(display->x+linex2,display->y,1,display->h,0xffff00ff)<0) return -1;
  }
  if ((liney1>=0)&&(liney1<=display->fbh)) {
    if (display->fbh!=display->h) liney1=(liney1*display->h)/display->fbh;
    if (ctm_draw_rect(display->x,display->y+liney1,display->w,1,0xffff00ff)<0) return -1;
  }
  if ((liney2>=0)&&(liney2<=display->fbh)) {
    if (display->fbh!=display->h) liney2=(liney2*display->h)/display->fbh;
    if (ctm_draw_rect(display->x,display->y+liney2,display->w,1,0xffff00ff)<0) return -1;
  }
  return 0;
}

/* Draw selection (main).
 */

static int ctm_display_editor_draw_selection(struct ctm_display *display) {
  int x=DISPLAY->selx*CTM_TILESIZE-DISPLAY->scrollx;
  if ((x<-CTM_TILESIZE)||(x>display->fbw)) return 0;
  int y=DISPLAY->sely*CTM_TILESIZE-DISPLAY->scrolly;
  if ((y<-CTM_TILESIZE)||(y>display->fbh)) return 0;
  int w=CTM_TILESIZE;
  if (display->fbw!=display->w) {
    x=(x*display->w)/display->fbw;
    y=(y*display->h)/display->fbh;
    w=(w*display->w)/display->fbw;
  }
  x+=display->x;
  y+=display->y;
  if (ctm_draw_frame(x,y,w,w,0xff000080)<0) return -1;
  return 0;
}

/* Draw cursor (main).
 */

static int ctm_display_editor_draw_cursor(struct ctm_display *display) {
  if (ctm_draw_pointer(ctm_editor_input.ptrx,ctm_editor_input.ptry)<0) return -1;
  return 0;
}

/* Draw main output.
 */

static int _ctm_display_editor_draw(struct ctm_display *display) {
  if (ctm_draw_texture(display->x,display->y,display->w,display->h,display->fbtexid,1)<0) return -1;
  if (DISPLAY->draw_state_lines&&(ctm_display_editor_draw_state_lines(display)<0)) return -1;
  if (DISPLAY->draw_selection&&(ctm_display_editor_draw_selection(display)<0)) return -1;
  if (DISPLAY->draw_cursor&&(ctm_display_editor_draw_cursor(display)<0)) return -1;
  return 0;
}

/* Type definition.
 */

const struct ctm_display_type ctm_display_type_editor={
  .name="editor",
  .objlen=sizeof(struct ctm_display_editor),
  .del=_ctm_display_editor_del,
  .init=_ctm_display_editor_init,
  .resized=_ctm_display_editor_resized,
  .draw_fb=_ctm_display_editor_draw_fb,
  .draw=_ctm_display_editor_draw,
};

/* Public property accessors.
 */

int ctm_display_editor_get_interior(const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_editor)) return 0;
  return DISPLAY->interior;
}

int ctm_display_editor_set_interior(struct ctm_display *display,int interior) {
  if (!display||(display->type!=&ctm_display_type_editor)) return -1;
  if (interior<0) DISPLAY->interior^=1;
  else if (interior) DISPLAY->interior=1;
  else DISPLAY->interior=0;
  return 0;
}

int ctm_display_editor_set_selection(struct ctm_display *display,int col,int row) {
  if (!display||(display->type!=&ctm_display_type_editor)) return -1;
  if (col<0) col=0; else if (col>=ctm_grid.colc) col=ctm_grid.colc-1;
  if (row<0) row=0; else if (row>=ctm_grid.rowc) row=ctm_grid.rowc-1;
  DISPLAY->selx=col;
  DISPLAY->sely=row;
  return 0;
}

int ctm_display_editor_cell_from_coords(int *col,int *row,const struct ctm_display *display,int x,int y) {
  if (!display||(display->type!=&ctm_display_type_editor)) return -1;
  if (col) {
    x-=display->x;
    if (display->w!=display->fbw) x=(x*display->fbw)/display->w;
    *col=(x+DISPLAY->scrollx)/CTM_TILESIZE;
    if (*col<0) *col=0; else if (*col>=ctm_grid.colc) *col=ctm_grid.colc-1;
  }
  if (row) {
    y-=display->y;
    if (display->h!=display->fbh) y=(y*display->fbh)/display->h;
    *row=(y+DISPLAY->scrolly)/CTM_TILESIZE;
    if (*row<0) *row=0; else if (*row>=ctm_grid.rowc) *row=ctm_grid.rowc-1;
  }
  return 0;
}

int ctm_display_editor_scroll(struct ctm_display *display,int dx,int dy) {
  if (!display||(display->type!=&ctm_display_type_editor)) return -1;
  DISPLAY->scrollx+=dx;
  DISPLAY->scrolly+=dy;
  if (DISPLAY->scrollx<0) DISPLAY->scrollx=0;
  else {
    int limit=ctm_grid.colc*CTM_TILESIZE-display->fbw;
    if (limit<=0) DISPLAY->scrollx=0; else if (DISPLAY->scrollx>limit) DISPLAY->scrollx=limit;
  }
  if (DISPLAY->scrolly<0) DISPLAY->scrolly=0;
  else {
    int limit=ctm_grid.rowc*CTM_TILESIZE-display->fbh;
    if (limit<=0) DISPLAY->scrolly=0; else if (DISPLAY->scrolly>limit) DISPLAY->scrolly=limit;
  }
  return 0;
}
