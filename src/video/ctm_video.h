/* ctm_video.h
 * Global video manager.
 *
 * There must be a separate video backend unit: GLX, BCM, or SDL.
 * That backend sets up an OpenGL or OpenGL ES context and leaves it running.
 * Backend's buffer-swap function must enforce timing.
 *
 */

#ifndef CTM_VIDEO_H
#define CTM_VIDEO_H

// Set by video backend (GLX, BCM, SDL). To others, is it read-only.
extern int ctm_screenw,ctm_screenh;

int ctm_video_init(int fullscreen);
void ctm_video_quit();
int ctm_video_swap();

int ctm_video_get_fullscreen();
int ctm_video_set_fullscreen(int fs); // <0 to toggle; returns new state

// Input dispatcher calls this:
int ctm_video_screen_size_changed();

/* Drawing.
 */

int ctm_draw_rect(int x,int y,int w,int h,uint32_t rgba);
int ctm_draw_frame(int x,int y,int w,int h,uint32_t rgba);
int ctm_draw_texture(int x,int y,int w,int h,int texid,int upsidedown);
int ctm_draw_texture_flop(int x,int y,int w,int h,int texid,int upsidedown);
int ctm_draw_pointer(int x,int y);
int ctm_draw_pie(int x,int y,int radius,double startt,double stopt,uint32_t rgba);

/* Add sprites to draw list.
 * Call this *only* from within a sprite's "draw" callback.
 */

struct ctm_vertex_sprite {
  int16_t x,y;
  uint8_t tile;
  uint8_t r,g,b,a;
  uint8_t tr,tg,tb,ta;
  uint8_t flop;
};

struct ctm_vertex_sprite *ctm_add_sprites(int c);

#endif
