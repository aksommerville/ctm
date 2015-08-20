#ifndef CTM_GLX_INTERNAL_H
#define CTM_GLX_INTERNAL_H

#include "ctm.h"
#include "ctm_glx.h"
#include "video/ctm_video.h"
#include "input/ctm_input.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <GL/glx.h>

extern const unsigned char ctm_program_icon[]; // ctm_program_icon.c
extern const int ctm_program_icon_w;
extern const int ctm_program_icon_h;

#define KeyRepeat (LASTEvent+2)
#define CTM_GLX_KEY_REPEAT_TIME 10 // ms

struct ctm_keymap {
  int keysym;
  uint16_t btnid;
};
 
extern struct ctm_glx {

  Display *dpy;
  Window win;
  GLXContext ctx;
  
  int screenid;
  int screenw,screenh;
  int winw,winh;
  int restorew,restoreh; // Window bounds to return to, if we are fullscreen.
  int fullscreen;
  int focus;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom_UTF8_STRING;
  Atom atom__NET_WM_ICON;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;

  int devid_keyboard;
  struct ctm_keymap *keymapv;
  int keymapc,keymapa;

} ctm_glx;

// Remnants from akglx, these are used only during init...
int ctm_glx_set_title(const char *src,int srcc);
int ctm_glx_set_icon(const void *rgba,int w,int h);
int ctm_glx_hide_cursor();

#endif
