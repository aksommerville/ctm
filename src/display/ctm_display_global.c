#include "ctm.h"
#include "ctm_display.h"
#include "video/ctm_video_internal.h"
#include <math.h>

/* Globals.
 */

#define CTM_NEWS_SPIN_TIME 40 // frames
 
GLuint ctm_generate_newspaper(int stateix,int party);

static struct {
  struct ctm_display **v;
  int c,a;
  struct ctm_line { int ax,ay,bx,by; uint8_t r,g,b,a; } *linev;
  int linec,linea;

  GLuint news_texid;
  int news_counter;
  
} ctm_displays={0};

/* Init.
 */

int ctm_display_init() {
  memset(&ctm_displays,0,sizeof(ctm_displays));
  return 0;
}

/* Quit.
 */

void ctm_display_quit() {
  if (ctm_displays.news_texid) glDeleteTextures(1,&ctm_displays.news_texid);
  if (ctm_displays.v) {
    while (ctm_displays.c-->0) ctm_display_del(ctm_displays.v[ctm_displays.c]);
    free(ctm_displays.v);
  }
  if (ctm_displays.linev) free(ctm_displays.linev);
  memset(&ctm_displays,0,sizeof(ctm_displays));
}

/* Draw the newspaper, if we're doing that.
 * It draws directly to GL's output and ignores the installed displays.
 * ie, on top of everything.
 */

static int ctm_display_draw_newspaper() {

  if (!ctm_displays.news_texid) return 0;
  if (ctm_displays.news_counter<INT_MAX) ctm_displays.news_counter++;

  // Black out the background.
  uint8_t alpha;
  if (ctm_displays.news_counter>=CTM_NEWS_SPIN_TIME) alpha=0xc0; 
  else alpha=(ctm_displays.news_counter*0xc0)/CTM_NEWS_SPIN_TIME;
  if (ctm_draw_rect(0,0,ctm_screenw,ctm_screenh,0x00000000|alpha)<0) return -1;

  // Paper reaches 5/6 of screen height ultimately.
  int papersize=(ctm_screenh*5)/6;
  if (ctm_displays.news_counter<CTM_NEWS_SPIN_TIME) papersize=(papersize*ctm_displays.news_counter)/CTM_NEWS_SPIN_TIME;

  struct ctm_vertex_tex *vtxv=ctm_video_vtxv_append(&ctm_shader_tex,4);
  if (!vtxv) return -1;
  if (ctm_displays.news_counter>=CTM_NEWS_SPIN_TIME) {
    int x=(ctm_screenw>>1)-(papersize>>1);
    int y=(ctm_screenh>>1)-(papersize>>1);
    vtxv[0].x=x;
    vtxv[0].y=y;   
    vtxv[1].x=x+papersize;
    vtxv[1].y=y;
    vtxv[2].x=x;
    vtxv[2].y=y+papersize;
    vtxv[3].x=x+papersize;
    vtxv[3].y=y+papersize;
  } else {
    double t=(ctm_displays.news_counter*M_PI*2.0)/CTM_NEWS_SPIN_TIME-(M_PI*0.25);
    double cost=cos(t);
    double sint=sin(t);
    double radius=(papersize>>1)*M_SQRT2; // 'papersize' is the diameter; we need distance to corners, ie multiply by sqrt(2).
    int midx=ctm_screenw>>1;
    int midy=ctm_screenh>>1;
    vtxv[0].x=midx-cos(t)*radius;
    vtxv[0].y=midy+sin(t)*radius;
    vtxv[1].x=midx-sin(t)*radius;
    vtxv[1].y=midy-cos(t)*radius;
    vtxv[2].x=midx+sin(t)*radius;
    vtxv[2].y=midy+cos(t)*radius;
    vtxv[3].x=midx+cos(t)*radius;
    vtxv[3].y=midy-sin(t)*radius;
  }
  vtxv[0].tx=0.0f; vtxv[0].ty=1.0f;
  vtxv[1].tx=1.0f; vtxv[1].ty=1.0f;
  vtxv[2].tx=0.0f; vtxv[2].ty=0.0f;
  vtxv[3].tx=1.0f; vtxv[3].ty=0.0f;
  glUseProgram(ctm_shader_tex.program);
  glBindTexture(GL_TEXTURE_2D,ctm_displays.news_texid);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_tex),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct ctm_vertex_tex),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  ctm_video.vtxc=0;

  return 0;
}

/* Draw.
 */

int ctm_display_draw() {
  int i;

  /* Clear framebuffer if necessary. Clearing is a bit expensive and is usually not necessary. */
  int must_clear=1;
  if (ctm_displays.c>0) {
    must_clear=0;
    for (i=0;i<ctm_displays.c;i++) {
      if (ctm_displays.v[i]->type->draw) {
        if (!ctm_displays.v[i]->opaque) { must_clear=1; break; }
      } else if (ctm_displays.v[i]->fb) {
        if (ctm_displays.v[i]->fbalpha<0xff) { must_clear=1; break; }
      } else {
        must_clear=1; break;
      }
    }
  }
  if (must_clear) {
    glClearColor(0.0,0.0,0.0,1.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  /* Every display that has a framebuffer and a 'draw_fb' function, enter its framebuffer and draw it. */
  int left_context=0;
  for (i=0;i<ctm_displays.c;i++) {
    struct ctm_display *display=ctm_displays.v[i];
    if (display->fb&&display->type->draw_fb) {
      glBindFramebuffer(GL_FRAMEBUFFER,display->fb);
      ctm_video_set_uniform_screen_size(display->fbw,display->fbh);
      if (ctm_displays.v[i]->type->draw_fb(display)<0) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        ctm_video_reset_uniform_screen_size();
        return -1;
      }
      left_context=1;
    }
  }
  if (left_context) {
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    ctm_video_reset_uniform_screen_size();
  }

  /* Copy displays to screen, either by their 'draw' function, or by drawing their fb texture. 
   * Ugly hack to prevent lines on top of modals: Skip modals in this pass.
   */
  for (i=0;i<ctm_displays.c;i++) {
    struct ctm_display *display=ctm_displays.v[i];
    if (display->type==&ctm_display_type_modal) continue;
    if (display->type->draw) {
      if (display->type->draw(display)<0) return -1;
    } else if (display->fbtexid) {
      if (ctm_draw_texture(display->x,display->y,display->w,display->h,display->fbtexid,1)<0) return -1;
    }
  }

  /* Draw lines separating displays. */
  if (ctm_displays.linec>0) {
    struct ctm_vertex_raw *vtxv=ctm_video_vtxv_append(&ctm_shader_raw,ctm_displays.linec<<1);
    if (!vtxv) return -1;
    struct ctm_vertex_raw *vtx=vtxv;
    struct ctm_line *line=ctm_displays.linev;
    for (i=0;i<ctm_displays.linec;i++,line++,vtx+=2) {
      vtx[0].x=line->ax;
      vtx[0].y=line->ay;
      vtx[1].x=line->bx;
      vtx[1].y=line->by;
      vtx[0].r=vtx[1].r=line->r;
      vtx[0].g=vtx[1].g=line->g;
      vtx[0].b=vtx[1].b=line->b;
      vtx[0].a=vtx[1].a=line->a;
    }
    glUseProgram(ctm_shader_raw.program);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_raw),&vtxv[0].x);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_raw),&vtxv[0].r);
    glDrawArrays(GL_LINES,0,ctm_displays.linec<<1);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    ctm_video.vtxc=0;
  }

  if (ctm_display_draw_newspaper()<0) return -1;

  /* Ugly hack: modal displays get drawn above lines and newspaper.
   */
  for (i=0;i<ctm_displays.c;i++) {
    struct ctm_display *display=ctm_displays.v[i];
    if (display->type!=&ctm_display_type_modal) continue;
    if (display->type->draw) {
      if (display->type->draw(display)<0) return -1;
    } else if (display->fbtexid) {
      if (ctm_draw_texture(display->x,display->y,display->w,display->h,display->fbtexid,1)<0) return -1;
    }
  }

  /* Let 'video' unit figure out how to commit the frame. (by triggering one of the optional units...) */
  if (ctm_video_swap()<0) return -1;
  
  return 0;
}

/* Add a separator line.
 */

static int ctm_display_add_line(int ax,int ay,int bx,int by,uint32_t rgba) {
  if (ctm_displays.linec>=ctm_displays.linea) {
    int na=ctm_displays.linea+2;
    if (na>INT_MAX/sizeof(struct ctm_line)) return -1;
    void *nv=realloc(ctm_displays.linev,sizeof(struct ctm_line)*na);
    if (!nv) return -1;
    ctm_displays.linev=nv;
    ctm_displays.linea=na;
  }
  struct ctm_line *line=ctm_displays.linev+ctm_displays.linec++;
  line->ax=ax;
  line->ay=ay;
  line->bx=bx;
  line->by=by;
  line->r=rgba>>24;
  line->g=rgba>>16;
  line->b=rgba>>8;
  line->a=rgba;
  return 0;
}

/* Wipe or grow display set.
 */

static void ctm_display_clear() {
  ctm_display_end_news();
  while (ctm_displays.c>0) ctm_display_del(ctm_displays.v[--(ctm_displays.c)]);
  ctm_displays.linec=0;
}

static int ctm_display_require(int c) {
  if (c<1) return 0;
  if (ctm_displays.c>INT_MAX-c) return -1;
  int na=ctm_displays.c+c;
  if (na<=ctm_displays.a) return 0;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(ctm_displays.v,sizeof(void*)*na);
  if (!nv) return -1;
  ctm_displays.v=nv;
  ctm_displays.a=na;
  return 0;
}

/* Rebuild displays.
 */
 
int ctm_display_rebuild_mainmenu() {
  ctm_display_clear();
  if (ctm_display_require(1)<0) return -1;
  if (!(ctm_displays.v[0]=ctm_display_alloc(&ctm_display_type_mainmenu))) return -1;
  ctm_displays.c=1;
  if (ctm_display_set_bounds(ctm_displays.v[0],0,0,ctm_screenw,ctm_screenh)<0) return -1;
  return 0;
}

int ctm_display_rebuild_gameover() {
  ctm_display_clear();
  if (ctm_display_require(1)<0) return -1;
  if (!(ctm_displays.v[0]=ctm_display_alloc(&ctm_display_type_gameover))) return -1;
  ctm_displays.c=1;
  if (ctm_display_set_bounds(ctm_displays.v[0],0,0,ctm_screenw,ctm_screenh)<0) return -1;
  return 0;
}

int ctm_display_rebuild_game(int playerc) {
  if ((playerc<1)||(playerc>4)) return -1;
  ctm_display_clear();
  if (ctm_display_require(playerc)<0) return -1;
  int i;
  for (i=0;i<playerc;i++) {
    if (!(ctm_displays.v[i]=ctm_display_alloc(&ctm_display_type_game))) {
      while (i-->0) ctm_display_del(ctm_displays.v[i]);
      return -1;
    }
    if (ctm_display_game_set_playerid(ctm_displays.v[i],i+1)<0) {
      while (i-->0) ctm_display_del(ctm_displays.v[i]);
      return -1;
    }
  }
  switch (ctm_displays.c=playerc) {
  
    case 1: {
        if (ctm_display_set_bounds(ctm_displays.v[0],0,0,ctm_screenw,ctm_screenh)<0) return -1;
      } break;
      
    case 2: if (ctm_screenw>ctm_screenh) { // landscape screen (extremely likely)
        int midx=ctm_screenw>>1;
        if (ctm_display_set_bounds(ctm_displays.v[0],0,0,midx,ctm_screenh)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[1],midx,0,ctm_screenw-midx,ctm_screenh)<0) return -1;
        if (ctm_display_add_line(midx,0,midx,ctm_screenh,0x000000ff)<0) return -1;
      } else { // portrait screen (do they still make those?)
        int midy=ctm_screenh>>1;
        if (ctm_display_set_bounds(ctm_displays.v[0],0,0,ctm_screenw,midy)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[1],0,midy,ctm_screenw,ctm_screenh-midy)<0) return -1;
        if (ctm_display_add_line(0,midy,ctm_screenw,midy,0x000000ff)<0) return -1;
      } break;
      
    case 3: if (ctm_screenw>=ctm_screenh*3) { // Extremely wide screen, go 3x1. I've never seen such a wide monitor.
        int w=ctm_screenw/3;
        if (ctm_display_set_bounds(ctm_displays.v[0],0,0,w,ctm_screenh)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[1],w,0,w,ctm_screenh)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[2],w<<1,0,ctm_screenw-(w<<1),ctm_screenh)<0) return -1;
        if (ctm_display_add_line(w,0,w,ctm_screenh,0x000000ff)<0) return -1;
        if (ctm_display_add_line(w<<1,0,w<<1,ctm_screenh,0x000000ff)<0) return -1;
      } else if (ctm_screenh>=ctm_screenw*3) { // Extremely tall screen, for the sake of completeness. Go 1x3.
        int h=ctm_screenh/3;
        if (ctm_display_set_bounds(ctm_displays.v[0],0,0,ctm_screenw,h)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[1],0,h,ctm_screenw,h)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[2],0,h<<1,ctm_screenw,ctm_screenh-(h<<1))<0) return -1;
        if (ctm_display_add_line(0,h,ctm_screenw,h,0x000000ff)<0) return -1;
        if (ctm_display_add_line(0,h<<1,ctm_screenw,h<<1,0x000000ff)<0) return -1;
      } else { // Monitors that actually exist on Earth. Go 2x2 and put a 'filler' display in the lower quadrant.
        if (ctm_display_require(1)<0) return -1;
        int w=ctm_screenw>>1,h=ctm_screenh>>1;
        if (ctm_display_set_bounds(ctm_displays.v[0],0,0,w,h)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[1],w,0,ctm_screenw-w,h)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[2],0,h,w,ctm_screenh-h)<0) return -1;
        if (!(ctm_displays.v[3]=ctm_display_alloc(&ctm_display_type_filler))) return -1;
        ctm_displays.c=4;
        if (ctm_display_set_bounds(ctm_displays.v[3],w,h,ctm_screenw-w,ctm_screenh-h)<0) return -1;
        if (ctm_display_add_line(w,0,w,ctm_screenh,0x000000ff)<0) return -1;
        if (ctm_display_add_line(0,h,ctm_screenw,h,0x000000ff)<0) return -1;
      } break;
      
    case 4: { // Same as the realistic 3-player case, but use all four quadrants.
        int w=ctm_screenw>>1,h=ctm_screenh>>1;
        if (ctm_display_set_bounds(ctm_displays.v[0],0,0,w,h)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[1],w,0,ctm_screenw-w,h)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[2],0,h,w,ctm_screenh-h)<0) return -1;
        if (ctm_display_set_bounds(ctm_displays.v[3],w,h,ctm_screenw-w,ctm_screenh-h)<0) return -1;
        if (ctm_display_add_line(w,0,w,ctm_screenh,0x000000ff)<0) return -1;
        if (ctm_display_add_line(0,h,ctm_screenw,h,0x000000ff)<0) return -1;
      } break;
    
  }
  return 0;
}

int ctm_display_rebuild_editor() {
  ctm_display_clear();
  if (ctm_display_require(1)<0) return -1;
  if (!(ctm_displays.v[0]=ctm_display_alloc(&ctm_display_type_editor))) return -1;
  ctm_displays.c=1;
  if (ctm_display_set_bounds(ctm_displays.v[0],0,0,ctm_screenw,ctm_screenh)<0) return -1;
  return 0;
}

/* Resize screen.
 */

int ctm_display_resized() {
  if (ctm_displays.c<1) return 0;
  // If we are running the game, it's easier to rebuild the display set from scratch.
  if (ctm_displays.v[0]->type==&ctm_display_type_game) {
    int playerc=ctm_displays.c;
    if ((playerc==4)&&(ctm_displays.v[3]->type==&ctm_display_type_filler)) playerc=3;
    return ctm_display_rebuild_game(playerc);
  } else {
    if (ctm_displays.c!=1) return -1; // Multiple displays only happens if the first one is 'game' type.
    struct ctm_display *display=ctm_displays.v[0];
    display->x=0;
    display->y=0;
    display->w=ctm_screenw;
    display->h=ctm_screenh;
    if (display->type->resized&&(display->type->resized(display)<0)) return -1;
  }
  return 0;
}

/* Public access to display list.
 */

struct ctm_display *ctm_display_for_playerid(int playerid) {
  if ((playerid<0)||(playerid>4)) return 0;
  int i; for (i=0;i<ctm_displays.c;i++) {
    if (ctm_displays.v[i]->type!=&ctm_display_type_game) continue;
    if (ctm_display_game_get_playerid(ctm_displays.v[i])==playerid) return ctm_displays.v[i];
  }
  return 0;
}

int ctm_display_player_get_size(int *w,int *h,int playerid) {
  struct ctm_display *display=ctm_display_for_playerid(playerid);
  if (!display) return 0;
  if (w) *w=display->fbw;
  if (h) *h=display->fbh;
  return 1;
}

int ctm_display_game_rebuild_all() {
  int i; for (i=0;i<ctm_displays.c;i++) {
    if (ctm_displays.v[i]->type!=&ctm_display_type_game) continue;
    if (ctm_display_game_rebuild_report(ctm_displays.v[i])<0) return -1;
  }
  return 0;
}

struct ctm_display *ctm_display_get_editor() {
  int i; for (i=0;i<ctm_displays.c;i++) if (ctm_displays.v[i]->type==&ctm_display_type_editor) return ctm_displays.v[i];
  return 0;
}

struct ctm_display *ctm_display_get_mainmenu() {
  int i; for (i=0;i<ctm_displays.c;i++) if (ctm_displays.v[i]->type==&ctm_display_type_mainmenu) return ctm_displays.v[i];
  return 0;
}

struct ctm_display *ctm_display_get_gameover() {
  int i; for (i=0;i<ctm_displays.c;i++) if (ctm_displays.v[i]->type==&ctm_display_type_gameover) return ctm_displays.v[i];
  return 0;
}

struct ctm_display *ctm_display_get_modal() {
  int i; for (i=0;i<ctm_displays.c;i++) if (ctm_displays.v[i]->type==&ctm_display_type_modal) return ctm_displays.v[i];
  return 0;
}

/* Public: Begin or end modal dialog. There can only be one.
 */
 
struct ctm_display *ctm_display_begin_modal() {
  struct ctm_display *display=ctm_display_get_modal();
  if (display) {
    ctm_display_modal_reset(display);
    return display;
  }
  if (ctm_display_require(1)<0) return 0;
  if (!(display=ctm_display_alloc(&ctm_display_type_modal))) return 0;
  if (ctm_display_set_bounds(display,ctm_screenw>>2,ctm_screenh>>2,ctm_screenw>>1,ctm_screenh>>1)<0) return 0;
  ctm_displays.v[ctm_displays.c++]=display;
  return display;
}

void ctm_display_end_modal() {
  int i; for (i=0;i<ctm_displays.c;i++) if (ctm_displays.v[i]->type==&ctm_display_type_modal) {
    struct ctm_display *display=ctm_displays.v[i];
    ctm_display_del(display);
    ctm_displays.c--;
    memmove(ctm_displays.v+i,ctm_displays.v+i+1,sizeof(void*)*(ctm_displays.c-i));
    return;
  }
}

/* Public newspaper access.
 */
 
int ctm_display_begin_news(int stateix,int party) {
  GLuint texid=ctm_generate_newspaper(stateix,party);
  if (!texid) return -1;
  if (ctm_displays.news_texid) glDeleteTextures(1,&ctm_displays.news_texid);
  ctm_displays.news_texid=texid;
  ctm_displays.news_counter=0;
  return 0;
}

int ctm_display_end_news() {
  if (!ctm_displays.news_texid) return 0;
  glDeleteTextures(1,&ctm_displays.news_texid);
  ctm_displays.news_texid=0;
  return 0;
}

/* Measure distance to nearest player display.
 */
 
int ctm_display_distance_to_player_screen(int *playerid,int x,int y,int interior) {
  int result=INT_MAX;
  int i; for (i=0;i<ctm_displays.c;i++) {
    struct ctm_display *display=ctm_displays.v[i];
    int dx,dy,dw,dh,di;
    if (ctm_display_game_get_position(&dx,&dy,&dw,&dh,&di,display)<0) continue;
    if (di!=interior) continue;
    if (x<dx) dx=dx-x; else if (x>dx+dw) dx=x-dw-dx; else dx=0;
    if (y<dy) dy=dy-y; else if (y>dy+dh) dy=y-dh-dy; else dy=0;
    int distance=dx+dy;
    if (distance<result) {
      if (playerid) *playerid=ctm_display_game_get_playerid(display);
      if (!distance) return 0;
      result=distance;
    }
  }
  if (result<INT_MAX) return result;
  return -1;
}

/* Screen shot.
 */
 
int ctm_display_take_screenshot(void *rgbapp,int *w,int *h) {

  // Validate request and prepare buffer.
  if (!rgbapp||!w||!h) return -1;
  if (ctm_displays.c<1) return -1;
  struct ctm_display *display=ctm_displays.v[0];
  if (!display->fb||!display->fbtexid||(display->fbw<1)||(display->fbh<1)) return -1;
  int imgsize=(display->fbw*display->fbh)<<2;
  if (imgsize<4) return -1;
  void *pixels=calloc(1,imgsize);
  if (!pixels) return -1;

  /* D'oh. OpenGL ES doesn't have glGetTexImage2D.
  glBindTexture(GL_TEXTURE_2D,display->fbtexid);
  glGetTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  */
  glBindFramebuffer(GL_FRAMEBUFFER,display->fb);
  glReadPixels(0,0,display->fbw,display->fbh,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  //...OK, that wasn't so bad.

  // OpenGL images are bottom-to-top, so reverse it rowwise.
  int rowsize=display->fbw<<2;
  void *tmp=malloc(rowsize);
  if (tmp) {
    char *rowa=pixels;
    char *rowb=(char*)pixels+rowsize*(display->fbh-1);
    int y; for (y=display->fbh>>1;y-->0;) {
      memcpy(tmp,rowa,rowsize);
      memcpy(rowa,rowb,rowsize);
      memcpy(rowb,tmp,rowsize);
      rowa+=rowsize;
      rowb-=rowsize;
    }
    free(tmp);
  }
  
  *(void**)rgbapp=pixels;
  *w=display->fbw;
  *h=display->fbh;
  return imgsize;
}
