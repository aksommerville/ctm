#ifndef CTM_VIDEO_INTERNAL_H
#define CTM_VIDEO_INTERNAL_H

#include "ctm.h"
#include "ctm_video.h"

#if CTM_USE_bcm
  #include "opt/bcm/ctm_bcm.h"
#elif CTM_USE_glx
  #include "opt/glx/ctm_glx.h"
#elif CTM_USE_sdl
  #include "opt/sdl/ctm_sdl.h"
#else
  #error "No suitable video backend. Please enable one of: bcm, glx, sdl"
#endif

/* Shaders.
 */

#include "ctm_shader.h"

/* When adding a new shader, add its name to this macro and define its vertex type.
 */
#define CTM_FOR_EACH_SHADER(MACRO) \
  MACRO(raw) \
  MACRO(tex) \
  MACRO(tile) \
  MACRO(sprite) \
  MACRO(text)
  
#define ONCE(tag) extern struct ctm_shader ctm_shader_##tag;
CTM_FOR_EACH_SHADER(ONCE)
#undef ONCE

struct ctm_vertex_raw {
  GLshort x,y;
  GLubyte r,g,b,a;
};

struct ctm_vertex_tex {
  GLshort x,y;
  GLfloat tx,ty;
};

struct ctm_vertex_tile {
  GLshort x,y;
  GLubyte tile;
};

struct ctm_vertex_text {
  GLshort x,y;
  GLubyte tile;
  GLubyte r,g,b,a;
};

/* Globals.
 */

extern struct ctm_video {

  //struct ctm_display *displayv[4];
  //int displayc;

  struct ctm_shader *vtxuser; // Shader associated with current vertex buffer.
  void *vtxv; // Shared vertex buffer.
  int vtxc;   // Count of vertices in buffer.
  int vtxa;   // Size of vertex buffer in *bytes*.

  GLuint texid_exterior;
  GLuint texid_interior;
  GLuint texid_sprites;
  GLuint texid_font;
  GLuint texid_logo_frostbite;
  GLuint texid_logo_blood;
  GLuint texid_logo_tie;
  GLuint texid_uisprites;
  GLuint texid_title;
  GLuint texid_tinyfont;
  GLuint texid_bunting;
  GLuint location_tex_alpha; // 'tex' shader has a uniform alpha multiplier -- please reset to 1.0 when you're done!
  GLuint location_text_size;

  int statsp; // Which stats display will we knock out next? Rotates.

  int is_editor;

  // Somehow, point sprite coordinates get flipped when drawing to an external framebuffer on the Mac.
  // This doesn't happen when drawing to screen, and never happens on the Pi.
  // Have not yet tested under Linux; maybe it's a Mac GLX bug.
  // (or more likely, I fucked something up. Whatever).
  // With this flag nonzero, offscreen sprite shaders should reverse their Y coordinates.
  int sprites_upside_down_in_framebuffer;

  int textsize;

} ctm_video;

/* Private API.
 */

void *ctm_video_vtxv_append(struct ctm_shader *shader,int c);
int ctm_video_set_uniform_screen_size(int w,int h); // Call me before entering offscreen framebuffer.
int ctm_video_reset_uniform_screen_size(); // ...and call me when you're done.
GLuint ctm_video_load_png(const char *path_under_data,int filter);

int ctm_video_begin_tiles();
int ctm_video_end_tiles(GLuint texid);
int ctm_video_begin_sprites();
int ctm_video_end_sprites(GLuint texid);

int ctm_video_begin_text(int size);
int ctm_video_add_text(const char *src,int srcc,int x,int y,uint32_t rgba);
int ctm_video_add_textf(int x,int y,uint32_t rgba,const char *fmt,...);
int ctm_video_add_textf_centered(int x,int y,int w,int h,uint32_t rgba,const char *fmt,...);
int ctm_video_end_text(GLuint texid);

#endif
