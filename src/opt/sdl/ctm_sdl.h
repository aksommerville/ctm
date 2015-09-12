/* ctm_sdl.h
 */

#ifndef CTM_SDL_H
#define CTM_SDL_H

#if CTM_ARCH==CTM_ARCH_mswin && 0 // As usual, Windows requires that everything be done differently.
  #define GLEW_STATIC 1
  #include <SDL_opengl.h>
  #include <SDL.h>
#else

  #define GL_GLEXT_PROTOTYPES 1
  #include <SDL.h>
  #include <SDL_opengl.h>

  // The OpenGL headers that ship with SDL/MacOS seem to be pre-2.0. Or something...
  #ifndef GL_FRAMEBUFFER
    #define GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
    #define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
    #define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
    #define GL_PROGRAM_POINT_SIZE 0x8642
    #define glDeleteFramebuffers glDeleteFramebuffersEXT
    #define glGenFramebuffers glGenFramebuffersEXT
    #define glBindFramebuffer glBindFramebufferEXT
    #define glFramebufferTexture2D glFramebufferTexture2DEXT
    #define glCheckFramebufferStatus glCheckFramebufferStatusEXT
  #endif
  
#endif

int ctm_sdl_init(int fullscreen);
void ctm_sdl_quit();

int ctm_sdl_init_input();
void ctm_sdl_quit_input();

int ctm_sdl_init_audio(void (*cb)(int16_t *dst,int dstc));
void ctm_sdl_quit_audio();

int ctm_sdl_swap();
int ctm_sdl_update();

int ctm_sdl_is_fullscreen();
int ctm_sdl_set_fullscreen(int fullscreen);

#endif
