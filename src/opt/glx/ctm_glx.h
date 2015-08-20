/* ctm_glx.h
 * Manages video and input via Xlib and GLX.
 */

#ifndef CTM_GLX_H
#define CTM_GLX_H

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

int ctm_glx_init(int fullscreen,int winw,int winh);
void ctm_glx_quit();
int ctm_glx_init_input();
void ctm_glx_quit_input();
int ctm_glx_update();
int ctm_glx_swap();

int ctm_glx_is_fullscreen(); // => 0,1
int ctm_glx_set_fullscreen(int fullscreen);

int ctm_glx_screen_size(int *w,int *h); // Full screen size. Typically what you want is 'window_size'.
int ctm_glx_window_size(int *w,int *h); // Interior size of our window. (same as ctm_screenw,ctm_screenh).
int ctm_glx_screen_id(); // => Screen number as delivered by X.
void *ctm_glx_display(); // => Display*
void *ctm_glx_window(); // => Window*
void *ctm_glx_context(); // => GLXContext*
int ctm_glx_has_focus(); // => 0,1; Do we have input focus right now?
int ctm_glx_pixels_from_rgba(void *dst,const void *src,int w,int h);
int ctm_glx_key_repr(char *dst,int dsta,int keysym);
int ctm_glx_key_eval(int *keysym,const char *src,int srcc);
int ctm_glx_key_to_unicode(int keysym);

#endif
