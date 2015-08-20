#include "ctm_glx_internal.h"
#include "io/ctm_fs.h"

struct ctm_glx ctm_glx={0};

/* Init.
 */
 
int ctm_glx_init(int fullscreen,int w,int h) {
  if (ctm_glx.dpy) return -1;
  memset(&ctm_glx,0,sizeof(ctm_glx));
  
  /* Open display and measure screen. */
  if (!(ctm_glx.dpy=XOpenDisplay(0))) return -1;
  ctm_glx.screenid=DefaultScreen(ctm_glx.dpy);
  ctm_glx.screenw=WidthOfScreen(ScreenOfDisplay(ctm_glx.dpy,ctm_glx.screenid));
  ctm_glx.screenh=HeightOfScreen(ScreenOfDisplay(ctm_glx.dpy,ctm_glx.screenid));
  if ((ctm_glx.screenw<1)||(ctm_glx.screenh<1)) return -1;
  
  /* Acquire a few atoms. */
  #define DEFATOM(tag) ctm_glx.atom_##tag=XInternAtom(ctm_glx.dpy,#tag,0);
  DEFATOM(WM_PROTOCOLS)
  DEFATOM(WM_DELETE_WINDOW)
  DEFATOM(UTF8_STRING)
  DEFATOM(_NET_WM_ICON)
  DEFATOM(_NET_WM_STATE)
  DEFATOM(_NET_WM_STATE_FULLSCREEN)
  #undef DEFATOM
  
  /* If requested size is unreasonable, we are going fullscreen. Also, if they asked us to explicitly. */
  if (fullscreen||(w<1)||(h<1)||(w>ctm_glx.screenw)||(h>ctm_glx.screenh)) {
    ctm_glx.fullscreen=1;
    ctm_glx.winw=ctm_glx.screenw;
    ctm_glx.winh=ctm_glx.screenh;
    ctm_glx.restorew=ctm_glx.screenw>>1;
    ctm_glx.restoreh=ctm_glx.screenh>>1;
  } else {
    ctm_glx.winw=ctm_glx.restorew=w;
    ctm_glx.winh=ctm_glx.restoreh=h;
  }
  
  /* Create window, with default event mask. */
  XSetWindowAttributes winattr={.event_mask=StructureNotifyMask|FocusChangeMask|KeyPressMask|KeyReleaseMask};
  if (!(ctm_glx.win=XCreateWindow(
    ctm_glx.dpy,RootWindow(ctm_glx.dpy,ctm_glx.screenid),
    0,0,ctm_glx.winw,ctm_glx.winh,0,
    CopyFromParent,InputOutput,CopyFromParent,
    CWEventMask,&winattr
  ))) return -1;
  
  /* Map window. */
  XSetWMProtocols(ctm_glx.dpy,ctm_glx.win,&ctm_glx.atom_WM_DELETE_WINDOW,1);
  XMapWindow(ctm_glx.dpy,ctm_glx.win);
  
  /* Go fullscreen if requested. Window must be mapped first. */
  if (ctm_glx.fullscreen) {
    XEvent evt={0};
    evt.xclient.type=ClientMessage;
    evt.xclient.send_event=1;
    evt.xclient.display=ctm_glx.dpy;
    evt.xclient.window=ctm_glx.win;
    evt.xclient.message_type=ctm_glx.atom__NET_WM_STATE;
    evt.xclient.format=32;
    evt.xclient.data.l[0]=1;
    evt.xclient.data.l[1]=ctm_glx.atom__NET_WM_STATE_FULLSCREEN;
    evt.xclient.data.l[3]=1;
    XSendEvent(ctm_glx.dpy,RootWindow(ctm_glx.dpy,ctm_glx.screenid),0,StructureNotifyMask,&evt);
  }

  /* Not resizable. */
  XSizeHints sizehints={
    .flags=PMinSize|PMaxSize,
    .min_width=ctm_glx.winw,
    .max_width=ctm_glx.winw,
    .min_height=ctm_glx.winh,
    .max_height=ctm_glx.winh,
  };
  XSetWMNormalHints(ctm_glx.dpy,ctm_glx.win,&sizehints);

  ctm_screenw=ctm_glx.winw;
  ctm_screenh=ctm_glx.winh;
  
  /* Create OpenGL context and activate it. */
  int glxattr[]={
    GLX_RGBA,
    GLX_DOUBLEBUFFER,
    GLX_RED_SIZE,8,
    GLX_GREEN_SIZE,8,
    GLX_BLUE_SIZE,8,
    GLX_DEPTH_SIZE,0,
  0};
  XVisualInfo *info=glXChooseVisual(ctm_glx.dpy,ctm_glx.screenid,glxattr);
  if (!info) return -1;
  ctm_glx.ctx=glXCreateContext(ctm_glx.dpy,info,0,1);
  XFree(info);
  if (!ctm_glx.ctx) return -1;
  glXMakeCurrent(ctm_glx.dpy,ctm_glx.win,ctm_glx.ctx);
  
  // Some implementations do not block for vsync by default. This should catch them.
  int (*fn)(Display*,Window,int)=(void*)glXGetProcAddress("glXSwapIntervalEXT");
  if (fn) fn(ctm_glx.dpy,ctm_glx.win,1);
  
  /* Setup icon, title, cursor. */
  if (ctm_glx_set_icon(ctm_program_icon,ctm_program_icon_w,ctm_program_icon_h)<0) return -1;
  if (ctm_glx_set_title("The Campaign Trail of the Mummy",-1)<0) return -1;
  if (ctm_glx_hide_cursor()<0) return -1;
  
  ctm_glx.focus=1;

  XFlush(ctm_glx.dpy);
  XSetInputFocus(ctm_glx.dpy,ctm_glx.win,RevertToNone,CurrentTime);

  return 0;
}

/* Quit.
 */
 
void ctm_glx_quit() {
  if (ctm_glx.dpy) {
    if (ctm_glx.ctx) {
      glXMakeCurrent(ctm_glx.dpy,0,0);
      glXDestroyContext(ctm_glx.dpy,ctm_glx.ctx);
    }
    if (ctm_glx.win) XDestroyWindow(ctm_glx.dpy,ctm_glx.win);
    XCloseDisplay(ctm_glx.dpy);
  }
  memset(&ctm_glx,0,sizeof(ctm_glx));
}

/* Keyboard map.
 */

int ctm_glx_keymap_search(int keysym) {
  int lo=0,hi=ctm_glx.keymapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (keysym<ctm_glx.keymapv[ck].keysym) hi=ck;
    else if (keysym>ctm_glx.keymapv[ck].keysym) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ctm_glx_add_keymap(int keysym,uint16_t btnid) {
  int p=ctm_glx_keymap_search(keysym);
  if (p>=0) return 0; p=-p-1;
  if (ctm_glx.keymapc>=ctm_glx.keymapa) {
    int na=ctm_glx.keymapa+8;
    if (na>INT_MAX/sizeof(struct ctm_keymap)) return -1;
    void *nv=realloc(ctm_glx.keymapv,sizeof(struct ctm_keymap)*na);
    if (!nv) return -1;
    ctm_glx.keymapv=nv;
    ctm_glx.keymapa=na;
  }
  memmove(ctm_glx.keymapv+p+1,ctm_glx.keymapv+p,sizeof(struct ctm_keymap)*(ctm_glx.keymapc-p));
  ctm_glx.keymapc++;
  ctm_glx.keymapv[p].keysym=keysym;
  ctm_glx.keymapv[p].btnid=btnid;
  return 0;
}

static inline uint16_t ctm_glx_lookup_key(int keysym) {
  int p=ctm_glx_keymap_search(keysym);
  if (p<0) return 0;
  return ctm_glx.keymapv[p].btnid;
}

/* Enable and disable input.
 */

int ctm_glx_init_input() {

  if ((ctm_glx.devid_keyboard=ctm_input_register_device())<1) return -1;

  struct ctm_input_definition *def=ctm_input_get_definition("X11 Keyboard",12);
  if (def) {
    int i; for (i=0;i<def->fldc;i++) {
      if (def->fldv[i].srcscope==11) {
        if (ctm_glx_add_keymap(def->fldv[i].srcbtnid,def->fldv[i].dstbtnidhi)<0) return -1;
      }
    }
  } else {
    printf("No definition found for keyboard. Copy the following into your config file (%s)...\nX11 Keyboard\n",ctm_data_path("input.cfg"));
    printf("  11.%d QUIT\n",XK_Escape);
    printf("  11.%d FULLSCREEN\n",XK_f);
    printf("  11.%d UP\n",XK_Up);
    printf("  11.%d DOWN\n",XK_Down);
    printf("  11.%d LEFT\n",XK_Left);
    printf("  11.%d RIGHT\n",XK_Right);
    printf("  11.%d PRIMARY\n",XK_z);
    printf("  11.%d SECONDARY\n",XK_x);
    printf("  11.%d TERTIARY\n",XK_c);
    printf("  11.%d PAUSE\n",XK_Return);
    printf("end\n");
    if (ctm_glx_add_keymap(XK_Escape,CTM_BTNID_QUIT)<0) return -1;
    if (ctm_glx_add_keymap(XK_f,CTM_BTNID_FULLSCREEN)<0) return -1;
    if (ctm_glx_add_keymap(XK_Print,CTM_BTNID_SCREENSHOT)<0) return -1;
    if (ctm_glx_add_keymap(XK_Up,CTM_BTNID_UP)<0) return -1;
    if (ctm_glx_add_keymap(XK_Down,CTM_BTNID_DOWN)<0) return -1;
    if (ctm_glx_add_keymap(XK_Left,CTM_BTNID_LEFT)<0) return -1;
    if (ctm_glx_add_keymap(XK_Right,CTM_BTNID_RIGHT)<0) return -1;
    if (ctm_glx_add_keymap(XK_z,CTM_BTNID_PRIMARY)<0) return -1;
    if (ctm_glx_add_keymap(XK_x,CTM_BTNID_SECONDARY)<0) return -1;
    if (ctm_glx_add_keymap(XK_c,CTM_BTNID_TERTIARY)<0) return -1;
    if (ctm_glx_add_keymap(XK_Return,CTM_BTNID_PAUSE)<0) return -1;
  }
  
  return 0;
}

void ctm_glx_quit_input() {
  ctm_input_unregister_device(ctm_glx.devid_keyboard);
  ctm_glx.devid_keyboard=0;
  if (ctm_glx.keymapv) free(ctm_glx.keymapv);
  ctm_glx.keymapv=0;
  ctm_glx.keymapc=0;
  ctm_glx.keymapa=0;
}

/* Trivial accessors.
 */
 
int ctm_glx_screen_size(int *w,int *h) {
  if (!ctm_glx.dpy) return -1;
  if (w) *w=ctm_glx.screenw;
  if (h) *h=ctm_glx.screenh;
  return 0;
}

int ctm_glx_window_size(int *w,int *h) {
  if (!ctm_glx.dpy) return -1;
  if (w) *w=ctm_glx.winw;
  if (h) *h=ctm_glx.winh;
  return 0;
}

int ctm_glx_screen_id() {
  if (!ctm_glx.dpy) return -1;
  return ctm_glx.screenid;
}

void *ctm_glx_display() {
  return ctm_glx.dpy;
}

void *ctm_glx_window() {
  if (!ctm_glx.dpy) return 0;
  return &ctm_glx.win;
}

void *ctm_glx_context() {
  if (!ctm_glx.dpy) return 0;
  return &ctm_glx.ctx;
}

int ctm_glx_has_focus() {
  return ctm_glx.focus;
}

int ctm_glx_is_fullscreen() {
  return ctm_glx.fullscreen;
}

/* Swap buffers.
 */
 
int ctm_glx_swap() {
  if (!ctm_glx.dpy) return -1;
  glXSwapBuffers(ctm_glx.dpy,ctm_glx.win);
  return 0;
}

/* Set title.
 */
 
int ctm_glx_set_title(const char *src,int srcc) {
  if (!ctm_glx.dpy) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  XTextProperty prop={
    .value=(char*)src,
    .format=8,
    .encoding=ctm_glx.atom_UTF8_STRING,
    .nitems=srcc,
  };
  XSetWMName(ctm_glx.dpy,ctm_glx.win,&prop);
  XSetWMIconName(ctm_glx.dpy,ctm_glx.win,&prop);
  return 0;
}

/* Set icon.
 */

int ctm_glx_set_icon(const void *rgba,int w,int h) {
  if (!ctm_glx.dpy) return -1;
  if (!rgba||(w<1)||(h<1)) return -1;
  if (w>(INT_MAX-2)/h) return -1;
  int pixelc=w*h;
  int wordc=2+pixelc;
  if (wordc>INT_MAX>>2) return -1;
  int bufsize=wordc<<2;
  uint32_t *dst=malloc(bufsize);
  if (!dst) return -1;
  dst[0]=w;
  dst[1]=h;
  ctm_glx_pixels_from_rgba(dst+2,rgba,w,h);
  // No icon appears under MacOS. Does it work under Linux?
  // ...yes. So it must be a Mac thing, who cares.
  XChangeProperty(ctm_glx.dpy,ctm_glx.win,ctm_glx.atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(char*)dst,wordc);
  free(dst);
  return 0;
}

/* Hide cursor.
 */
 
int ctm_glx_hide_cursor() {
  if (!ctm_glx.dpy) return -1;
  Pixmap blank=XCreateBitmapFromData(ctm_glx.dpy,ctm_glx.win,"\0",1,1);
  if (blank==None) return -1;
  XColor dummy;
  Cursor cursor=XCreatePixmapCursor(ctm_glx.dpy,blank,blank,&dummy,&dummy,0,0);
  XFreePixmap(ctm_glx.dpy,blank);
  if (!cursor) return -1;
  XDefineCursor(ctm_glx.dpy,ctm_glx.win,cursor);
  XFreeCursor(ctm_glx.dpy,cursor);
  return 0;
}

/* Toggle fullscreen.
 */

// On the Mac, when we toggle fullscreen we sometimes lose graphics. It snaps back if you click in the window.
// I've tried setting input focus manually and it doesn't help.
// The problem does not seem to happen on my Linux box, so I guess it's a Mac thing.
// Not bothering to fix because (a) it's only a minor nuisance and (b) Macs should use SDL anyway.
// And also (c) it might not be fixable.
 
int ctm_glx_set_fullscreen(int fullscreen) {
  if (!ctm_glx.dpy) return -1;
  if (fullscreen<0) fullscreen=ctm_glx.fullscreen^1;
  if (fullscreen) {
    fullscreen=1;
    if (ctm_glx.fullscreen) return 1;
    ctm_glx.restorew=ctm_glx.winw;
    ctm_glx.restoreh=ctm_glx.winh;
    XSizeHints sizehints={
      .flags=PMinSize|PMaxSize,
      .min_width=ctm_glx.screenw,
      .max_width=ctm_glx.screenw,
      .min_height=ctm_glx.screenh,
      .max_height=ctm_glx.screenh,
    };
    XSetWMNormalHints(ctm_glx.dpy,ctm_glx.win,&sizehints);
    XResizeWindow(ctm_glx.dpy,ctm_glx.win,ctm_glx.screenw,ctm_glx.screenh);
    XEvent evt={0};
    evt.xclient.type=ClientMessage;
    evt.xclient.send_event=1;
    evt.xclient.display=ctm_glx.dpy;
    evt.xclient.window=ctm_glx.win;
    evt.xclient.message_type=ctm_glx.atom__NET_WM_STATE;
    evt.xclient.format=32;
    evt.xclient.data.l[0]=1;
    evt.xclient.data.l[1]=ctm_glx.atom__NET_WM_STATE_FULLSCREEN;
    evt.xclient.data.l[3]=1;
    XSendEvent(ctm_glx.dpy,RootWindow(ctm_glx.dpy,ctm_glx.screenid),0,StructureNotifyMask,&evt);
  } else {
    if (!ctm_glx.fullscreen) return 0;
    XSizeHints sizehints={
      .flags=PMinSize|PMaxSize,
      .min_width=ctm_glx.restorew,
      .max_width=ctm_glx.restorew,
      .min_height=ctm_glx.restoreh,
      .max_height=ctm_glx.restoreh,
    };
    XSetWMNormalHints(ctm_glx.dpy,ctm_glx.win,&sizehints);
    XEvent evt={0};
    evt.xclient.type=ClientMessage;
    evt.xclient.send_event=1;
    evt.xclient.display=ctm_glx.dpy;
    evt.xclient.window=ctm_glx.win;
    evt.xclient.message_type=ctm_glx.atom__NET_WM_STATE;
    evt.xclient.format=32;
    evt.xclient.data.l[0]=0;
    evt.xclient.data.l[1]=ctm_glx.atom__NET_WM_STATE_FULLSCREEN;
    evt.xclient.data.l[3]=1;
    XSendEvent(ctm_glx.dpy,RootWindow(ctm_glx.dpy,ctm_glx.screenid),0,StructureNotifyMask,&evt);
    XResizeWindow(ctm_glx.dpy,ctm_glx.win,ctm_glx.restorew,ctm_glx.restoreh);
  }
  return ctm_glx.fullscreen=fullscreen;
}

/* Convert portable RGBA to device pixels.
 */
 
static int ctm_glx_pixel_index_from_mask(uint32_t mask) {
  switch (mask) {
    #if BYTE_ORDER==BIG_ENDIAN
      case 0xff000000: return 0;
      case 0x00ff0000: return 1;
      case 0x0000ff00: return 2;
      case 0x000000ff: return 3;
    #else
      case 0xff000000: return 3;
      case 0x00ff0000: return 2;
      case 0x0000ff00: return 1;
      case 0x000000ff: return 0;
    #endif
  }
  return -1;
}
 
int ctm_glx_pixels_from_rgba(void *dst,const void *src,int w,int h) {
  if (!w&&!h) return 0;
  if (!dst||!src||(w<1)||(h<1)) return -1;
  if (!ctm_glx.dpy) return -1;
  if (w>INT_MAX/h) return -1;
  int c=w*h;
  uint8_t *DST=dst;
  const uint8_t *SRC=src;
  int rp,gp,bp,ap;
  Screen *screen=ScreenOfDisplay(ctm_glx.dpy,ctm_glx.screenid);
  if (!screen) return -1;
  Visual *visual=screen->root_visual;
  if (!visual) return -1;
  rp=ctm_glx_pixel_index_from_mask(visual->red_mask);
  gp=ctm_glx_pixel_index_from_mask(visual->green_mask);
  bp=ctm_glx_pixel_index_from_mask(visual->blue_mask);
  ap=ctm_glx_pixel_index_from_mask(~(visual->red_mask|visual->green_mask|visual->blue_mask));
  if ((rp<0)||(gp<0)||(bp<0)||(ap<0)) return -1;
  if ((rp==0)&&(gp==1)&&(bp==2)&&(ap==3)) { memcpy(dst,src,c<<2); return 0; }
  while (c-->0) {
    DST[rp]=SRC[0];
    DST[gp]=SRC[1];
    DST[bp]=SRC[2];
    DST[ap]=SRC[3];
    DST+=4;
    SRC+=4;
  }
  return 0;
}
