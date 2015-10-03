#include "ctm_video_internal.h"
#include <stdarg.h>
#include <math.h>

#if CTM_TEST_DISABLE_VIDEO
  int ctm_draw_rect(int x,int y,int w,int h,uint32_t rgba) { return 0; }
  int ctm_draw_frame(int x,int y,int w,int h,uint32_t rgba) { return 0; }
  int ctm_draw_pointer(int x,int y) { return 0; }
  int ctm_draw_texture(int x,int y,int w,int h,int texid,int upsidedown) { return 0; }
  int ctm_draw_texture_flop(int x,int y,int w,int h,int texid,int upsidedown) { return 0; }
  int ctm_video_begin_tiles() { return 0; }
  int ctm_video_end_tiles(GLuint texid) { ctm_video.vtxc=0; return 0; }
  int ctm_video_begin_sprites() { return 0; }
  int ctm_video_end_sprites(GLuint texid) { ctm_video.vtxc=0; return 0; }
  struct ctm_vertex_sprite *ctm_add_sprites(int c) { return ctm_video_vtxv_append(&ctm_shader_sprite,c); }
  int ctm_video_begin_text(int size) { return 0; }
  int ctm_video_end_text(GLuint texid) { return 0; }
  int ctm_video_add_text(const char *src,int srcc,int x,int y,uint32_t rgba) { return 0; }
  int ctm_video_add_textf(int x,int y,uint32_t rgba,const char *fmt,...) { return 0; }
  int ctm_video_add_textf_centered(int x,int y,int w,int h,uint32_t rgba,const char *fmt,...) { return 0; }
  int ctm_draw_pie(int x,int y,int radius,double startt,double stopt,uint32_t rgba) { return 0; }
#else

/* Draw simple rectangle.
 */

int ctm_draw_rect(int x,int y,int w,int h,uint32_t rgba) {
  struct ctm_vertex_raw *vtxv=ctm_video_vtxv_append(&ctm_shader_raw,4);
  if (!vtxv) return -1;
  vtxv[0].r=vtxv[1].r=vtxv[2].r=vtxv[3].r=rgba>>24;
  vtxv[0].g=vtxv[1].g=vtxv[2].g=vtxv[3].g=rgba>>16;
  vtxv[0].b=vtxv[1].b=vtxv[2].b=vtxv[3].b=rgba>>8;
  vtxv[0].a=vtxv[1].a=vtxv[2].a=vtxv[3].a=rgba;
  vtxv[0].x=x; vtxv[0].y=y;
  vtxv[1].x=x+w; vtxv[1].y=y;
  vtxv[2].x=x; vtxv[2].y=y+h;
  vtxv[3].x=x+w; vtxv[3].y=y+h;
  glUseProgram(ctm_shader_raw.program);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_raw),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_raw),&vtxv[0].r);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  ctm_video.vtxc=0;
  return 0;
}

/* Frame rectangle.
 */

int ctm_draw_frame(int x,int y,int w,int h,uint32_t rgba) {
  struct ctm_vertex_raw *vtxv=ctm_video_vtxv_append(&ctm_shader_raw,5);
  if (!vtxv) return -1;
  vtxv[0].r=vtxv[1].r=vtxv[2].r=vtxv[3].r=rgba>>24;
  vtxv[0].g=vtxv[1].g=vtxv[2].g=vtxv[3].g=rgba>>16;
  vtxv[0].b=vtxv[1].b=vtxv[2].b=vtxv[3].b=rgba>>8;
  vtxv[0].a=vtxv[1].a=vtxv[2].a=vtxv[3].a=rgba;
  vtxv[0].x=x; vtxv[0].y=y;
  vtxv[1].x=x+w; vtxv[1].y=y;
  vtxv[2].x=x+w; vtxv[2].y=y+h;
  vtxv[3].x=x; vtxv[3].y=y+h;
  vtxv[4]=vtxv[0];
  glUseProgram(ctm_shader_raw.program);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_raw),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_raw),&vtxv[0].r);
  glDrawArrays(GL_LINE_STRIP,0,5);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  ctm_video.vtxc=0;
  return 0;
}

/* Draw pointer.
 */

static int ctm_pointer_phase=0; // Just for fun...

int ctm_draw_pointer(int x,int y) {
  if ((ctm_pointer_phase+=10)>511) ctm_pointer_phase-=512;
  struct ctm_vertex_raw *vtxv=ctm_video_vtxv_append(&ctm_shader_raw,4);
  if (!vtxv) return -1;
  if (ctm_pointer_phase<256) {
    vtxv[0].r=vtxv[1].r=vtxv[2].r=255-ctm_pointer_phase;
    vtxv[0].g=vtxv[1].g=vtxv[2].g=ctm_pointer_phase;
  } else {
    vtxv[0].r=vtxv[1].r=vtxv[2].r=ctm_pointer_phase-256;
    vtxv[0].g=vtxv[1].g=vtxv[2].g=511-ctm_pointer_phase;
  }
  vtxv[0].b=vtxv[1].b=vtxv[2].b=0x00;
  vtxv[0].a=vtxv[1].a=vtxv[2].a=0xc0;
  vtxv[0].x=x; vtxv[0].y=y;
  vtxv[1].x=x+8; vtxv[1].y=y+16;
  vtxv[2].x=x+16; vtxv[2].y=y+8;
  glUseProgram(ctm_shader_raw.program);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_raw),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_raw),&vtxv[0].r);
  glDrawArrays(GL_TRIANGLES,0,3);
  vtxv[0].r=vtxv[1].r=vtxv[2].r=0x00;
  vtxv[0].g=vtxv[1].g=vtxv[2].g=0x00;
  vtxv[0].a=vtxv[1].a=vtxv[2].a=0xff;
  vtxv[3]=vtxv[0];
  glDrawArrays(GL_LINE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  ctm_video.vtxc=0;
  return 0;
}

/* Draw textured rectangle.
 */
 
int ctm_draw_texture(int x,int y,int w,int h,int texid,int upsidedown) {
  struct ctm_vertex_tex *vtxv=ctm_video_vtxv_append(&ctm_shader_tex,4);
  if (!vtxv) return -1;
  GLfloat ya,yz; if (upsidedown) { ya=1.0f; yz=0.0f; } else { ya=0.0f; yz=1.0f; }
  vtxv[0].x=x;   vtxv[0].y=y;   vtxv[0].tx=0.0f; vtxv[0].ty=ya;
  vtxv[1].x=x+w; vtxv[1].y=y;   vtxv[1].tx=1.0f; vtxv[1].ty=ya;
  vtxv[2].x=x;   vtxv[2].y=y+h; vtxv[2].tx=0.0f; vtxv[2].ty=yz;
  vtxv[3].x=x+w; vtxv[3].y=y+h; vtxv[3].tx=1.0f; vtxv[3].ty=yz;
  glUseProgram(ctm_shader_tex.program);
  glBindTexture(GL_TEXTURE_2D,texid);
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
 
int ctm_draw_texture_flop(int x,int y,int w,int h,int texid,int upsidedown) {
  struct ctm_vertex_tex *vtxv=ctm_video_vtxv_append(&ctm_shader_tex,4);
  if (!vtxv) return -1;
  GLfloat ya,yz; if (upsidedown) { ya=1.0f; yz=0.0f; } else { ya=0.0f; yz=1.0f; }
  vtxv[0].x=x;   vtxv[0].y=y;   vtxv[0].tx=1.0f; vtxv[0].ty=ya;
  vtxv[1].x=x+w; vtxv[1].y=y;   vtxv[1].tx=0.0f; vtxv[1].ty=ya;
  vtxv[2].x=x;   vtxv[2].y=y+h; vtxv[2].tx=1.0f; vtxv[2].ty=yz;
  vtxv[3].x=x+w; vtxv[3].y=y+h; vtxv[3].tx=0.0f; vtxv[3].ty=yz;
  glUseProgram(ctm_shader_tex.program);
  glBindTexture(GL_TEXTURE_2D,texid);
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

/* Tiles, piecemeal interface.
 */
 
int ctm_video_begin_tiles() {
  ctm_video.vtxc=0;
  return 0;
}

int ctm_video_end_tiles(GLuint texid) {
  if (ctm_video.vtxc<1) return 0;
  if (ctm_video.vtxuser!=&ctm_shader_tile) return -1;
  glUseProgram(ctm_shader_tile.program);
  glBindTexture(GL_TEXTURE_2D,texid);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  struct ctm_vertex_tile *vtxv=ctm_video.vtxv;
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_tile),&vtxv[0].x);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct ctm_vertex_tile),&vtxv[0].tile);
  glDrawArrays(GL_POINTS,0,ctm_video.vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  ctm_video.vtxc=0;
  return 0;
}

/* Sprites, piecemeal interface.
 */
 
int ctm_video_begin_sprites() {
  ctm_video.vtxc=0;
  return 0;
}

int ctm_video_end_sprites(GLuint texid) {
  if (ctm_video.vtxc<1) return 0;
  if (ctm_video.vtxuser!=&ctm_shader_sprite) return -1;
  glUseProgram(ctm_shader_sprite.program);
  glBindTexture(GL_TEXTURE_2D,texid);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glEnableVertexAttribArray(4);
  struct ctm_vertex_sprite *vtxv=ctm_video.vtxv;
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_sprite),&vtxv[0].x);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct ctm_vertex_sprite),&vtxv[0].tile);
  glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_sprite),&vtxv[0].r);
  glVertexAttribPointer(3,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_sprite),&vtxv[0].tr);
  glVertexAttribPointer(4,1,GL_UNSIGNED_BYTE,0,sizeof(struct ctm_vertex_sprite),&vtxv[0].flop);
  glDrawArrays(GL_POINTS,0,ctm_video.vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(3);
  glDisableVertexAttribArray(4);
  ctm_video.vtxc=0;
  return 0;
}

struct ctm_vertex_sprite *ctm_add_sprites(int c) {
  return ctm_video_vtxv_append(&ctm_shader_sprite,c);
}

/* Text, piecemeal interface.
 */
 
int ctm_video_begin_text(int size) {
  if (size<2) return -1;
  ctm_video.vtxc=0;
  glUseProgram(ctm_shader_text.program);
  glUniform1f(ctm_video.location_text_size,size);
  ctm_video.textsize=size;
  return 0;
}

int ctm_video_end_text(GLuint texid) {
  if (ctm_video.vtxc<1) return 0;
  if (ctm_video.vtxuser!=&ctm_shader_text) return -1;
  glUseProgram(ctm_shader_text.program);
  glBindTexture(GL_TEXTURE_2D,texid);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  struct ctm_vertex_text *vtxv=ctm_video.vtxv;
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_text),&vtxv[0].x);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct ctm_vertex_text),&vtxv[0].tile);
  glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_text),&vtxv[0].r);
  glDrawArrays(GL_POINTS,0,ctm_video.vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  ctm_video.vtxc=0;
  return 0;
}

int ctm_video_add_text(const char *src,int srcc,int x,int y,uint32_t rgba) {
  if (!src) return 0;
  if (!(rgba&0xff)) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  struct ctm_vertex_text *vtxv=ctm_video_vtxv_append(&ctm_shader_text,srcc);
  if (!vtxv) return -1;
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  int i; for (i=0;i<srcc;i++) {
    vtxv[i].x=x; x+=CTM_RESIZE(8);
    vtxv[i].y=y;
    if (src[i]&0x80) vtxv[i].tile='?'; else vtxv[i].tile=src[i];
    vtxv[i].r=r;
    vtxv[i].g=g;
    vtxv[i].b=b;
    vtxv[i].a=a;
  }
  return 0;
}

int ctm_video_add_textf(int x,int y,uint32_t rgba,const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  char buf[256];
  int bufc=vsnprintf(buf,sizeof(buf),fmt,vargs);
  if ((bufc<1)||(bufc>=sizeof(buf))) return 0;
  return ctm_video_add_text(buf,bufc,x,y,rgba);
}

int ctm_video_add_textf_centered(int x,int y,int w,int h,uint32_t rgba,const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  char buf[256];
  int bufc=vsnprintf(buf,sizeof(buf),fmt,vargs);
  if ((bufc<1)||(bufc>=sizeof(buf))) return 0;
  return ctm_video_add_text(buf,bufc,x+(w>>1)-bufc*(ctm_video.textsize>>2)+(ctm_video.textsize>>2),y+(h>>1),rgba);
}

/* Draw pie slice.
 */
 
int ctm_draw_pie(int x,int y,int radius,double startt,double stopt,uint32_t rgba) {

  if (radius<1) return 0;
  if (!(rgba&0xff)) return 0;
  while (stopt<startt) stopt+=M_PI*2.0;
  double trange=stopt-startt;

  // Multiply by 5 yields about 30 vertices in a full circle.
  // Clamp to a minimum of 5 vertices.
  int vtxc=(int)(trange*5.0);
  if (vtxc<5) vtxc=5;
  struct ctm_vertex_raw *vtxv=ctm_video_vtxv_append(&ctm_shader_raw,vtxc);
  if (!vtxv) return -1;
  
  vtxv[0].x=x;
  vtxv[0].y=y;
  vtxv[0].r=rgba>>24; vtxv[0].g=rgba>>16; vtxv[0].b=rgba>>8; vtxv[0].a=rgba;
  vtxv[1].r=vtxv[0].r>>1; vtxv[1].g=vtxv[0].g>>1; vtxv[1].b=vtxv[0].b>>1; vtxv[1].a=vtxv[0].a;
  int i; for (i=1;i<vtxc;i++) {
    double t=startt+((i-1)*trange)/(vtxc-2);
    vtxv[i].x=x-radius*sin(t);
    vtxv[i].y=y+radius*cos(t);
    vtxv[i].r=vtxv[1].r; vtxv[i].g=vtxv[1].g; vtxv[i].b=vtxv[1].b; vtxv[i].a=vtxv[1].a;
  }
  
  glUseProgram(ctm_shader_raw.program);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct ctm_vertex_raw),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct ctm_vertex_raw),&vtxv[0].r);
  glDrawArrays(GL_TRIANGLE_FAN,0,vtxc);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  ctm_video.vtxc=0;
  return 0;
}

#endif
