#include "ctm_glx_internal.h"

/* Keystrokes.
 */

int ctm_glx_keymap_search(int keysym);

static inline uint16_t ctm_glx_lookup_key(int keysym) {
  int p=ctm_glx_keymap_search(keysym);
  if (p<0) return 0;
  return ctm_glx.keymapv[p].btnid;
}
 
static int ctm_glx_event_key(XKeyEvent *evt,int value) {
  int keysym=0,unicode=0;
  int index=(evt->state&(ShiftMask|LockMask))?1:0;
  keysym=XLookupKeysym(evt,index);
  uint16_t btnid=ctm_glx_lookup_key(keysym);
  if (!btnid) return 0;
  return ctm_input_event(ctm_glx.devid_keyboard,btnid,value);
}

/* Gain or lose focus.
 * I was initially thinking of pausing gameplay when we lose focus, but decided against it.
 * No need to pull this hook out though, maybe it will have a use in the future.
 */
 
static int ctm_glx_event_focus(XFocusChangeEvent *evt,int value) {
  if (value) {
    if (ctm_glx.focus) return 0;
    ctm_glx.focus=1;
    return 0;
  } else {
    if (!ctm_glx.focus) return 0;
    ctm_glx.focus=0;
    return 0;
  }
}

/* Window frame changed.
 */
 
static int ctm_glx_event_configure(XConfigureEvent *evt) {
  if ((evt->width<1)||(evt->height<1)) return 0;
  if ((evt->width!=ctm_glx.winw)||(evt->height!=ctm_glx.winh)) {
    ctm_screenw=ctm_glx.winw=evt->width;
    ctm_screenh=ctm_glx.winh=evt->height;
    return ctm_input_event(0,CTM_BTNID_RESIZE,1);
  }
  return 0;
}

static int ctm_glx_event_map() {
  //glXMakeCurrent(ctm_glx.dpy,ctm_glx.win,ctm_glx.ctx);
  return 0;
}

static int ctm_glx_event_unmap() {
  //glXMakeCurrent(ctm_glx.dpy,0,0);
  return 0;
}

/* Client message.
 */
 
static int ctm_glx_event_client(XClientMessageEvent *evt) {
  if (evt->message_type==ctm_glx.atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==ctm_glx.atom_WM_DELETE_WINDOW) {
        return ctm_input_event(0,CTM_BTNID_QUIT,1);
      }
    }
  }
  return 0;
}

/* Dispatch single event.
 */
 
static int ctm_glx_event(XEvent *evt) {
  switch (evt->type) {
    case KeyPress: return ctm_glx_event_key(&evt->xkey,1);
    case KeyRelease: return ctm_glx_event_key(&evt->xkey,0);
    case KeyRepeat: return ctm_glx_event_key(&evt->xkey,2);
    case FocusIn: return ctm_glx_event_focus(&evt->xfocus,1);
    case FocusOut: return ctm_glx_event_focus(&evt->xfocus,0);
    case ConfigureNotify: return ctm_glx_event_configure(&evt->xconfigure);
    case UnmapNotify: return ctm_glx_event_unmap();
    case MapNotify: return ctm_glx_event_map();
    case ClientMessage: return ctm_glx_event_client(&evt->xclient);
  }
  return 0;
}

/* Update.
 */
 
int ctm_glx_update() {
  int err;
  if (!ctm_glx.dpy) return 0;
  int evtc=XEventsQueued(ctm_glx.dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(ctm_glx.dpy,&evt);
    if (evtc&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(ctm_glx.dpy,&next); evtc--;
      if ((next.type==KeyPress)&&(next.xkey.keycode==evt.xkey.keycode)&&(next.xkey.time<=evt.xkey.time+CTM_GLX_KEY_REPEAT_TIME)) {
        evt.type=KeyRepeat;
        if ((err=ctm_glx_event(&evt))<0) return err;
      } else {
        if ((err=ctm_glx_event(&evt))<0) return err;
        if ((err=ctm_glx_event(&next))<0) return err;
      }
    } else {
      if ((err=ctm_glx_event(&evt))<0) return err;
    }
  }
  return 0;
}
