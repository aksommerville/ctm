#include "ctm.h"
#include "ctm_display.h"
#include "video/ctm_video_internal.h"
#include "audio/ctm_audio.h"

/* Object type.
 */

struct ctm_modal_button {
  int x,y; // Position of top-left sprite, relative to framebuffer.
  int tx,ty; // Position of first glyph, relative to framebuffer.
  int w; // In sprites. Height is always 2 sprites.
  char *label;
  int labelc;
  int (*cb)(void *userdata);
  void *userdata;
};

struct ctm_display_modal {
  struct ctm_display hdr;
  uint32_t bgcolor;
  uint32_t textcolor;
  struct ctm_modal_button *buttonv;
  int buttonc,buttona;
  int selection;
  int intro_delay;
};

#define DISPLAY ((struct ctm_display_modal*)display)

/* Delete or initialize.
 */

static void ctm_modal_button_cleanup(struct ctm_modal_button *button) {
  if (button->label) free(button->label);
}

static void _ctm_display_modal_del(struct ctm_display *display) {
  if (DISPLAY->buttonv) {
    while (DISPLAY->buttonc-->0) ctm_modal_button_cleanup(DISPLAY->buttonv+DISPLAY->buttonc);
    free(DISPLAY->buttonv);
  }
}

static int _ctm_display_modal_init(struct ctm_display *display) {
  display->use_master_alpha=1;
  DISPLAY->bgcolor=0x000000a0;
  DISPLAY->textcolor=0x000000ff;
  DISPLAY->intro_delay=30;
  return 0;
}

/* Repack buttons.
 * Sets (x,y,tx,ty,w) of each button.
 * Buttons are laid out in a single row, with uniform widths, centered on both axes.
 */

static int ctm_display_modal_pack(struct ctm_display *display) {
  if (DISPLAY->buttonc<1) return 0;

  /* Width of all buttons is half of the longest label, rounding up, plus one. */
  int labelc_max=0;
  int i; for (i=0;i<DISPLAY->buttonc;i++) {
    if (DISPLAY->buttonv[i].labelc>labelc_max) labelc_max=DISPLAY->buttonv[i].labelc;
  }
  int buttonw=((labelc_max+1)>>1)+1;
  int buttonw_px=buttonw*CTM_TILESIZE;
  for (i=0;i<DISPLAY->buttonc;i++) DISPLAY->buttonv[i].w=buttonw;

  /* Each button is two rows tall, with one row between them. */
  int totalh=(DISPLAY->buttonc*2)+(DISPLAY->buttonc-1);
  int top=(display->fbh>>1)-((totalh*CTM_TILESIZE)>>1);
  int left=(display->fbw>>1)-((buttonw*CTM_TILESIZE)>>1);
  int spacing=CTM_TILESIZE*3;
  // (x,y) of a button is the CENTER of its top-left sprite:
  int texty=top+CTM_TILESIZE;
  top+=CTM_TILESIZE>>1;
  left+=CTM_TILESIZE>>1;

  /* Apply position to each button. */
  struct ctm_modal_button *button=DISPLAY->buttonv;
  for (i=0;i<DISPLAY->buttonc;i++,button++) {
    button->x=left;
    button->y=top;
    button->tx=(display->fbw>>1)-((button->labelc*(CTM_TILESIZE>>1))>>1)+(CTM_TILESIZE>>2);
    button->ty=texty;
    top+=spacing;
    texty+=spacing;
  }

  /* Force selection into range. */
  if (DISPLAY->selection<0) DISPLAY->selection=0;
  else if (DISPLAY->selection>=DISPLAY->buttonc) DISPLAY->selection=DISPLAY->buttonc-1;

  return 0;
}

/* Resize.
 * We need to reserve about 10 rows and 10 columns, roughly.
 * This is probably only going to be used for the Abort menu.
 * Flying by the seat of our pants here...
 * If we want to do this right, we would need to communicate with the display manager 
 * to have the display's output resized as elements are added.
 */

static int _ctm_display_modal_resized(struct ctm_display *display) {

  /* Preserve aspect, ensure that we have at least 160x160, and try to keep it under 320x320.
   * Don't think too hard about it.
   */
  int fbw=display->w;
  int fbh=display->h;
  int excessx=fbw/160;
  int excessy=fbh/160;
  int excess=(excessx>excessy)?excessx:excessy;
  if (excess>1) {
    excess--;
    fbw/=excess;
    fbh/=excess;
  }
  
  if (ctm_display_resize_fb(display,fbw,fbh)<0) return -1;
  if (ctm_display_modal_pack(display)<0) return -1;

  return 0;
}

/* Draw solid background.
 */

static int ctm_display_modal_draw_background(struct ctm_display *display) {
  if (ctm_draw_rect(0,0,display->fbw,display->fbh,DISPLAY->bgcolor)<0) return -1;
  return 0;
}

/* Draw tiles.
 */

static int ctm_modal_button_add_tiles(struct ctm_modal_button *button,int highlight) {
  if (button->w>0) {
    int tilec=button->w<<1;
    struct ctm_vertex_tile *vtxv=ctm_video_vtxv_append(&ctm_shader_tile,tilec);
    if (!vtxv) return -1;
    uint8_t tilebase=highlight?0x77:0x7a;
    struct ctm_vertex_tile *vtx=vtxv;
    int i,x;
    for (i=0,x=button->x;i<button->w;i++,vtx++,x+=CTM_TILESIZE) {
      vtx->x=x;
      vtx->y=button->y;
      vtx->tile=tilebase+0x01;
    }
    int y=button->y+CTM_TILESIZE;
    for (i=0,x=button->x;i<button->w;i++,vtx++,x+=CTM_TILESIZE) {
      vtx->x=x;
      vtx->y=y;
      vtx->tile=tilebase+0x21;
    }
    vtxv[0].tile=tilebase;
    vtxv[button->w-1].tile=tilebase+0x02;
    vtxv[button->w].tile=tilebase+0x20;
    vtxv[tilec-1].tile=tilebase+0x22;
  }
  return 0;
}

static int ctm_display_modal_draw_tiles(struct ctm_display *display) {
  if (ctm_video_begin_tiles()<0) return -1;
  int i; for (i=0;i<DISPLAY->buttonc;i++) {
    if (ctm_modal_button_add_tiles(DISPLAY->buttonv+i,(i==DISPLAY->selection)&&!DISPLAY->intro_delay)<0) return -1;
  }
  if (ctm_video_end_tiles(ctm_video.texid_uisprites)<0) return -1;
  return 0;
}

/* Draw text.
 */

static int ctm_display_modal_draw_text(struct ctm_display *display) {
  int i;
  struct ctm_modal_button *button=DISPLAY->buttonv;
  if (ctm_video_begin_text(CTM_TILESIZE)<0) return -1;
  for (i=0;i<DISPLAY->buttonc;i++,button++) {
    if (ctm_video_add_text(button->label,button->labelc,button->tx,button->ty,DISPLAY->textcolor)<0) return -1;
  }
  if (ctm_video_end_text(ctm_video.texid_font)<0) return -1;
  return 0;
}

/* Draw framebuffer, dispatch to specific phases.
 */

static int _ctm_display_modal_draw_fb(struct ctm_display *display) {

  if (DISPLAY->intro_delay>0) DISPLAY->intro_delay--;

  if (ctm_display_modal_draw_background(display)<0) return -1;
  if (ctm_display_modal_draw_tiles(display)<0) return -1;
  if (ctm_display_modal_draw_text(display)<0) return -1;
  return 0;
}

/* Type definition.
 */

const struct ctm_display_type ctm_display_type_modal={
  .name="modal",
  .objlen=sizeof(struct ctm_display_modal),
  .del=_ctm_display_modal_del,
  .init=_ctm_display_modal_init,
  .resized=_ctm_display_modal_resized,
  .draw_fb=_ctm_display_modal_draw_fb,
};

/* Add button.
 */
 
int ctm_display_modal_add_option(struct ctm_display *display,const char *label,int labelc,int (*cb)(void *userdata),void *userdata) {
  if (!display) return -1;
  if (display->type!=&ctm_display_type_modal) return -1;
  if (!label) labelc=0; else if (labelc<0) { labelc=0; while (label[labelc]) labelc++; }
  if (labelc>32) labelc=32;

  if (DISPLAY->buttonc>=DISPLAY->buttona) {
    int na=DISPLAY->buttona+4;
    if (na>INT_MAX/sizeof(struct ctm_modal_button)) return -1;
    void *nv=realloc(DISPLAY->buttonv,sizeof(struct ctm_modal_button)*na);
    if (!nv) return -1;
    DISPLAY->buttonv=nv;
    DISPLAY->buttona=na;
  }
  struct ctm_modal_button *button=DISPLAY->buttonv+DISPLAY->buttonc;
  memset(button,0,sizeof(struct ctm_modal_button));
  
  char *nv=0;
  if (labelc) {
    if (!(nv=malloc(labelc+1))) return -1;
    int i; for (i=0;i<labelc;i++) if ((label[i]<0x20)||(label[i]>0x7e)) nv[i]=' '; else nv[i]=label[i];
    nv[labelc]=0;
  }
  button->label=nv;
  button->labelc=labelc;
  button->cb=cb;
  button->userdata=userdata;

  DISPLAY->buttonc++;
  if (ctm_display_modal_pack(display)<0) return -1;
  return 0;
}

/* Reset display.
 */

int ctm_display_modal_reset(struct ctm_display *display) {
  if (!display) return -1;
  if (display->type!=&ctm_display_type_modal) return -1;
  while (DISPLAY->buttonc>0) {
    DISPLAY->buttonc--;
    ctm_modal_button_cleanup(DISPLAY->buttonv+DISPLAY->buttonc);
  }
  DISPLAY->selection=0;
  return 0;
}

/* Receive input.
 */
 
int ctm_display_modal_adjust(struct ctm_display *display,int d) {
  if (!display) return -1;
  if (display->type!=&ctm_display_type_modal) return -1;
  if (DISPLAY->intro_delay) return 0;
  if (DISPLAY->buttonc>1) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_CHANGE,0xff);
    if (d<0) {
      if (--(DISPLAY->selection)<0) DISPLAY->selection=DISPLAY->buttonc-1;
    } else {
      if (++(DISPLAY->selection)>=DISPLAY->buttonc) DISPLAY->selection=0;
    }
  } else {
    DISPLAY->selection=0;
  }
  return 0;
}
 
int ctm_display_modal_activate(struct ctm_display *display) {
  if (!display) return -1;
  if (display->type!=&ctm_display_type_modal) return -1;
  if (DISPLAY->intro_delay) return 0;
  ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_COMMIT,0xff);
  if ((DISPLAY->selection>=0)&&(DISPLAY->selection<DISPLAY->buttonc)) {
    struct ctm_modal_button *button=DISPLAY->buttonv+DISPLAY->selection;
    if (button->cb) {
      if (button->cb(button->userdata)<0) return -1;
    }
  }
  ctm_display_end_modal();
  return 0;
}
