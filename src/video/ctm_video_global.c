#include "ctm_video_internal.h"
#include "input/ctm_input.h"
#include "game/ctm_grid.h"
#include "misc/akpng.h"
#include "display/ctm_display.h"
#include "io/ctm_fs.h"

#define CTM_DISPLAY_FADE_IN_SPEED   4
#define CTM_DISPLAY_FADE_OUT_SPEED  2

/* Globals.
 */

struct ctm_video ctm_video={0};
int ctm_screenw=0,ctm_screenh=0;

/* Init.
 */

int ctm_video_init(int fullscreen,const char *device) {
  memset(&ctm_video,0,sizeof(struct ctm_video));
  ctm_screenw=ctm_screenh=0;

  #if CTM_USE_bcm
    if (ctm_bcm_init()<0) return -1;
  #elif CTM_USE_glx || CTM_USE_drm
    // Try GLX first, them DRM.
    if (ctm_glx_init(fullscreen,640,480)>=0) ctm_video.driver='g';
    else if (ctm_drm_init(device)>=0) ctm_video.driver='d';
    else return -1;
  #elif CTM_USE_sdl
    if (ctm_sdl_init(fullscreen)<0) return -1;
  #endif
  if ((ctm_screenw<1)||(ctm_screenh<1)) return -1;

  glClearColor(0.0,0.0,0.0,1.0);
  glEnable(GL_BLEND);
  #ifdef __gl_h_ // Desktop OpenGL.
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    //glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    #if CTM_ARCH==CTM_ARCH_macos // MacOS/GLX requires this; Linux/GLX does not.
      ctm_video.sprites_upside_down_in_framebuffer=1;
    #endif
  #else // OpenGL ES.
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  #endif

  #if !CTM_TEST_DISABLE_VIDEO
  #define ONCE(tag) \
    if (ctm_shader_compile(&ctm_shader_##tag)<0) { \
      printf("ctm_shader_compile(%s) failed\n",#tag); \
      return -1; \
    } \
    glUseProgram(ctm_shader_##tag.program); \
    glUniform2f(ctm_shader_##tag.location_screensize,ctm_screenw,ctm_screenh); \
    glUniform2f(ctm_shader_##tag.location_tilesize,CTM_TILESIZE,CTM_TILESIZE);
  CTM_FOR_EACH_SHADER(ONCE)
  #undef ONCE

  glUseProgram(ctm_shader_tex.program);
  ctm_video.location_tex_alpha=glGetUniformLocation(ctm_shader_tex.program,"alpha");
  glUniform1f(ctm_video.location_tex_alpha,1.0f);
  glUseProgram(ctm_shader_text.program);
  ctm_video.location_text_size=glGetUniformLocation(ctm_shader_text.program,"size");
  ctm_video.textsize=16;
  #endif

  if (!(ctm_video.texid_exterior=ctm_video_load_png("exterior.png",0))) return -1;
  if (!(ctm_video.texid_interior=ctm_video_load_png("interior.png",0))) return -1;
  if (!(ctm_video.texid_sprites=ctm_video_load_png("sprites.png",0))) return -1;
  if (!(ctm_video.texid_font=ctm_video_load_png("font.png",0))) return -1;
  if (!(ctm_video.texid_logo_frostbite=ctm_video_load_png("logo-frostbite.png",1))) return -1;
  if (!(ctm_video.texid_logo_blood=ctm_video_load_png("logo-blood.png",1))) return -1;
  if (!(ctm_video.texid_logo_tie=ctm_video_load_png("logo-tie.png",1))) return -1;
  if (!(ctm_video.texid_uisprites=ctm_video_load_png("uisprites.png",0))) return -1;
  if (!(ctm_video.texid_title=ctm_video_load_png("title.png",1))) return -1;
  if (!(ctm_video.texid_introbg=ctm_video_load_png("introbg.png",1))) return -1;
  if (!(ctm_video.texid_tinyfont=ctm_video_load_png("tinyfont.png",0))) return -1;
  if (!(ctm_video.texid_bunting=ctm_video_load_png("bunting.png",1))) return -1;

  return 0;
}

/* Quit.
 */

void ctm_video_quit() {

  if (ctm_video.texid_exterior) glDeleteTextures(1,&ctm_video.texid_exterior);
  if (ctm_video.texid_interior) glDeleteTextures(1,&ctm_video.texid_interior);
  if (ctm_video.texid_sprites) glDeleteTextures(1,&ctm_video.texid_sprites);
  if (ctm_video.texid_font) glDeleteTextures(1,&ctm_video.texid_font);
  if (ctm_video.texid_logo_frostbite) glDeleteTextures(1,&ctm_video.texid_logo_frostbite);
  if (ctm_video.texid_logo_blood) glDeleteTextures(1,&ctm_video.texid_logo_blood);
  if (ctm_video.texid_logo_tie) glDeleteTextures(1,&ctm_video.texid_logo_tie);
  if (ctm_video.texid_uisprites) glDeleteTextures(1,&ctm_video.texid_uisprites);
  if (ctm_video.texid_title) glDeleteTextures(1,&ctm_video.texid_title);
  if (ctm_video.texid_introbg) glDeleteTextures(1,&ctm_video.texid_introbg);
  if (ctm_video.texid_tinyfont) glDeleteTextures(1,&ctm_video.texid_tinyfont);
  if (ctm_video.texid_bunting) glDeleteTextures(1,&ctm_video.texid_bunting);

  #if !CTM_TEST_DISABLE_VIDEO
  #define ONCE(tag) ctm_shader_cleanup(&ctm_shader_##tag);
  CTM_FOR_EACH_SHADER(ONCE)
  #undef ONCE
  #endif

  #if CTM_USE_bcm
    ctm_bcm_quit();
  #elif CTM_USE_glx || CTM_USE_drm
    ctm_glx_quit();
    ctm_drm_quit();
  #elif CTM_USE_sdl
    ctm_sdl_quit();
  #endif

  if (ctm_video.vtxv) free(ctm_video.vtxv);

  ctm_screenw=ctm_screenh=0;
  memset(&ctm_video,0,sizeof(struct ctm_video));
}

/* Swap buffers.
 */

int ctm_video_swap() {
  #if CTM_USE_bcm
    return ctm_bcm_swap();
  #elif CTM_USE_glx || CTM_USE_drm
    if (ctm_video.driver=='g') return ctm_glx_swap();
    if (ctm_video.driver=='d') return ctm_drm_swap();
    return -1;
  #elif CTM_USE_sdl
    return ctm_sdl_swap();
  #endif
}

/* Fullscreen.
 */
 
int ctm_video_get_fullscreen() {
  #if CTM_USE_bcm
    return 1;
  #elif CTM_USE_glx || CTM_USE_drm
    if (ctm_video.driver=='g') return ctm_glx_is_fullscreen();
    return 1;
  #elif CTM_USE_sdl
    return ctm_sdl_is_fullscreen();
  #endif
}

int ctm_video_set_fullscreen(int fs) {
  #if CTM_USE_bcm
    return 1;
  #elif CTM_USE_glx || CTM_USE_drm
    if (ctm_video.driver=='g') return ctm_glx_set_fullscreen(fs);
    return 1;
  #elif CTM_USE_sdl
    return ctm_sdl_set_fullscreen(fs);
  #endif
}

/* Respond to resized screen.
 */
 
int ctm_video_set_uniform_screen_size(int w,int h) {
  #if CTM_TEST_DISABLE_VIDEO
    return 0;
  #else
  #define ONCE(tag) \
    glUseProgram(ctm_shader_##tag.program); \
    glUniform2f(ctm_shader_##tag.location_screensize,w,h); \
    if (ctm_video.sprites_upside_down_in_framebuffer) glUniform1f(ctm_shader_##tag.location_upsidedown,0.0f);
  CTM_FOR_EACH_SHADER(ONCE)
  #undef ONCE
  glViewport(0,0,w,h);
  return 0;
  #endif
}

int ctm_video_reset_uniform_screen_size() {
  #if CTM_TEST_DISABLE_VIDEO
    return 0;
  #else
  #define ONCE(tag) \
    glUseProgram(ctm_shader_##tag.program); \
    glUniform2f(ctm_shader_##tag.location_screensize,ctm_screenw,ctm_screenh); \
    if (ctm_video.sprites_upside_down_in_framebuffer) glUniform1f(ctm_shader_##tag.location_upsidedown,1.0f);
  CTM_FOR_EACH_SHADER(ONCE)
  #undef ONCE
  glViewport(0,0,ctm_screenw,ctm_screenh);
  return 0;
  #endif
}

int ctm_video_screen_size_changed() {
  if (ctm_display_resized()<0) return -1;
  return ctm_video_reset_uniform_screen_size();
}

/* Vertex buffer.
 */

void *ctm_video_vtxv_append(struct ctm_shader *shader,int c) {
  if (!shader||(c<1)) return 0;
  if (ctm_video.vtxc) {
    if (shader!=ctm_video.vtxuser) return 0;
  } else {
    ctm_video.vtxuser=shader;
  }
  if (ctm_video.vtxc>INT_MAX-c) return 0;
  int na=ctm_video.vtxc+c;
  na*=shader->vtxsize;
  if (na<1) return 0;
  if (na>ctm_video.vtxa) {
    if (na<INT_MAX-1024) na=(na+1024)&~1023;
    void *nv=realloc(ctm_video.vtxv,na);
    if (!nv) return 0;
    ctm_video.vtxv=nv;
    ctm_video.vtxa=na;
  }
  void *rtn=(char*)ctm_video.vtxv+ctm_video.vtxc*shader->vtxsize;
  ctm_video.vtxc+=c;
  return rtn;
}

/* Load PNG file to texture.
 */

GLuint ctm_video_load_png(const char *path,int filter) {
  struct akpng png={0};
  if (!(path=ctm_data_path(path))) return 0;
  if (akpng_decode_file(&png,path)<0) { akpng_cleanup(&png); return 0; }
  int format;
  if ((png.colortype==2)&&(png.depth==8)) format=GL_RGB;
  else if ((png.colortype==6)&&(png.depth==8)) format=GL_RGBA;
  else if ((png.colortype==0)&&(png.depth==8)) format=GL_ALPHA;
  else {
    if (akpng_force_rgba(&png)<0) { akpng_cleanup(&png); return 0; }
    format=GL_RGBA;
  }
  GLuint texid=0;
  glGenTextures(1,&texid);
  if (!texid) glGenTextures(1,&texid);
  if (!texid) { akpng_cleanup(&png); return 0; }
  glBindTexture(GL_TEXTURE_2D,texid);
  filter=filter?GL_LINEAR:GL_NEAREST;
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,format,png.w,png.h,0,format,GL_UNSIGNED_BYTE,png.pixels);
  akpng_cleanup(&png);
  return texid;
}
