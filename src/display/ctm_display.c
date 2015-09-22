#include "ctm.h"
#include "ctm_display.h"
#include "video/ctm_video_internal.h"
#include "game/ctm_grid.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_voter.h"

/* Allocation.
 */
 
struct ctm_display *ctm_display_alloc(const struct ctm_display_type *type) {
  if (!type) return 0;
  if (type->objlen<(int)sizeof(struct ctm_display)) return 0;
  struct ctm_display *display=calloc(1,type->objlen);
  if (!display) return 0;
  display->type=type;
  display->fbalpha=0xff;
  if (type->init&&(type->init(display)<0)) { ctm_display_del(display); return 0; }
  return display;
}

void ctm_display_del(struct ctm_display *display) {
  if (!display) return;
  if (display->type->del) display->type->del(display);
  if (display->fbtexid) glDeleteTextures(1,&display->fbtexid);
  if (display->fb) glDeleteFramebuffers(1,&display->fb);
  free(display);
}

/* Resize or allocate framebuffer.
 */

int ctm_display_resize_fb(struct ctm_display *display,int fbw,int fbh) {
  if (!display) return -1;
  if ((fbw<1)||(fbh<1)) return -1;
  if ((fbw==display->fbw)&&(fbh==display->fbh)) return 0;

  // Allocate texture.
  if (!display->fbtexid) {
    glGenTextures(1,&display->fbtexid);
    if (!display->fbtexid) glGenTextures(1,&display->fbtexid);
    if (!display->fbtexid) return -1;
    glBindTexture(GL_TEXTURE_2D,display->fbtexid);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  } else {
    glBindTexture(GL_TEXTURE_2D,display->fbtexid);
  }
  int fmt=display->use_master_alpha?GL_RGBA:GL_RGB;
  glTexImage2D(GL_TEXTURE_2D,0,fmt,fbw,fbh,0,fmt,GL_UNSIGNED_BYTE,0);

  // Allocate framebuffer.
  if (!display->fb) {
    GLuint fb=0;
    glGenFramebuffers(1,&fb);
    if (!fb) glGenFramebuffers(1,&fb);
    if (!fb) return -1;
    display->fb=fb;
    glBindFramebuffer(GL_FRAMEBUFFER,display->fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,display->fbtexid,0);
    int status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if (status!=GL_FRAMEBUFFER_COMPLETE) return -1;
  }

  display->fbw=fbw;
  display->fbh=fbh;

  return 0;
}

/* Delete framebuffer.
 */

int ctm_display_drop_fb(struct ctm_display *display) {
  if (!display) return -1;
  if (display->fbtexid) { glDeleteTextures(1,&display->fbtexid); display->fbtexid=0; }
  if (display->fb) { glDeleteFramebuffers(1,&display->fb); display->fb=0; }
  display->fbw=0;
  display->fbh=0;
  display->fbalpha=0xff;
  return 0;
}

/* Set output boundary.
 */
 
int ctm_display_set_bounds(struct ctm_display *display,int x,int y,int w,int h) {
  if (!display||(w<1)||(h<1)) return -1;
  int ox=display->x; display->x=x;
  int oy=display->y; display->y=y;
  int ow=display->w; display->w=w;
  int oh=display->h; display->h=h;
  if (display->type->resized) {
    if (display->type->resized(display)<0) { 
      display->x=ox; display->y=oy; display->w=ow; display->h=oh;
      return -1;
    }
  }
  return 0;
}

/* Draw grid.
 */
 
int ctm_display_draw_grid(struct ctm_display *display,int scrollx,int scrolly,int interior) {
  if (!display) return -1;

  // Which cells are we drawing?
  int cola=scrollx/CTM_TILESIZE; if (cola<0) cola=0;
  int colz=(scrollx+display->fbw-1)/CTM_TILESIZE; if (colz>=ctm_grid.colc) colz=ctm_grid.colc-1;
  if (cola>colz) return 0;
  int rowa=scrolly/CTM_TILESIZE; if (rowa<0) rowa=0;
  int rowz=(scrolly+display->fbh-1)/CTM_TILESIZE; if (rowz>=ctm_grid.rowc) rowz=ctm_grid.rowc-1;
  if (rowa>rowz) return 0;
  int cellc=(colz-cola+1)*(rowz-rowa+1);

  // Begin drawing, acquire vertex array.
  if (ctm_video_begin_tiles()<0) return -1;
  struct ctm_vertex_tile *vtxv=ctm_video_vtxv_append(&ctm_shader_tile,cellc);
  if (!vtxv) return -1;

  // And now our feature presentation.
  const struct ctm_grid_cell *cellv=ctm_grid.cellv+rowa*ctm_grid.colc;
  int y=rowa*CTM_TILESIZE+(CTM_TILESIZE>>1)-scrolly;
  int x0=cola*CTM_TILESIZE+(CTM_TILESIZE>>1)-scrollx;
  int row; for (row=rowa;row<=rowz;row++,cellv+=ctm_grid.colc,y+=CTM_TILESIZE) {
    int x=x0;
    int col; for (col=cola;col<=colz;col++,vtxv++,x+=CTM_TILESIZE) {
      vtxv->x=x;
      vtxv->y=y;
      vtxv->tile=interior?cellv[col].itile:cellv[col].tile;
    }
  }

  // Commit graphics.
  if (ctm_video_end_tiles(interior?ctm_video.texid_interior:ctm_video.texid_exterior)<0) return -1;

  return 0;
}

/* Draw sprites.
 */
 
int ctm_display_draw_sprites(struct ctm_display *display,int scrollx,int scrolly,const struct ctm_sprgrp *grp,int lens_of_truth) {
  if (!display) return -1;
  if (!grp||(grp->sprc<1)) return 0;

  // Clip to two tiles' width beyond visible boundaries -- some sprites draw further from their midpoint than you'd expect.
  struct ctm_bounds clip=(struct ctm_bounds){scrollx,scrollx+display->fbw,scrolly,scrolly+display->fbh};
  clip.l-=CTM_TILESIZE<<1;
  clip.r+=CTM_TILESIZE<<1;
  clip.t-=CTM_TILESIZE<<1;
  clip.b+=CTM_TILESIZE<<1;
  int addx=-scrollx;
  int addy=-scrolly;

  // Run through all sprites and let them draw themselves, or do our simple fallback.
  // The rendering groups should be sorted in drawing order.
  if (ctm_video_begin_sprites()<0) return -1;
  int i; for (i=0;i<grp->sprc;i++) {
    struct ctm_sprite *spr=grp->sprv[i];
    if (spr->x<clip.l) continue;
    if (spr->x>clip.r) continue;
    if (spr->y<clip.t) continue;
    if (spr->y>clip.b) continue;
    if (spr->type->draw) {
      int vtxc=ctm_video.vtxc;
      if (spr->type->draw(spr,addx,addy)<0) return -1;
      if (lens_of_truth&&(spr->type==&ctm_sprtype_voter)) {
        uint8_t tr,tg,tb;
        int8_t decision=((struct ctm_sprite_voter*)spr)->decision;
        if (decision==-128) decision=-127;
        if (decision<0) { tb=-decision*2; tg=tr=0; }
        else if (decision>0) { tb=tg=0; tr=decision*2; }
        else tr=tg=tb=0;
        while (vtxc<ctm_video.vtxc) {
          struct ctm_vertex_sprite *vtx=((struct ctm_vertex_sprite*)ctm_video.vtxv)+(vtxc++);
          vtx->tr=tr; vtx->tg=tg; vtx->tb=tb; vtx->ta=0xff;
        }
      }
    } else {
      struct ctm_vertex_sprite *vtx=ctm_video_vtxv_append(&ctm_shader_sprite,1);
      if (!vtx) return -1;
      vtx->x=spr->x+addx;
      vtx->y=spr->y+addy;
      vtx->tile=spr->tile;
      vtx->r=spr->primaryrgb>>16;
      vtx->g=spr->primaryrgb>>8;
      vtx->b=spr->primaryrgb;
      vtx->a=spr->opacity;
      vtx->tr=spr->tintrgba>>24;
      vtx->tg=spr->tintrgba>>16;
      vtx->tb=spr->tintrgba>>8;
      vtx->ta=spr->tintrgba;
      vtx->flop=spr->flop;
    }
  }
  if (ctm_video_end_sprites(ctm_video.texid_sprites)<0) return -1;
  
  return 0;
}
