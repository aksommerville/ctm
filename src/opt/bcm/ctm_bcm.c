#include "ctm.h"
#include "ctm_bcm.h"
#include "video/ctm_video.h"
#include <bcm_host.h>
#include <EGL/egl.h>

/* Globals.
 */

static struct {
  DISPMANX_DISPLAY_HANDLE_T vcdisplay;
  DISPMANX_ELEMENT_HANDLE_T vcelement;
  DISPMANX_UPDATE_HANDLE_T vcupdate;
  EGL_DISPMANX_WINDOW_T eglwindow;
  EGLDisplay egldisplay;
  EGLSurface eglsurface;
  EGLContext eglcontext;
  EGLConfig eglconfig;
  int initstate;
} ctm_bcm={0};

/* Init.
 */

int ctm_bcm_init() {
  if (ctm_bcm.initstate) return -1;
  memset(&ctm_bcm,0,sizeof(ctm_bcm));

  bcm_host_init();
  ctm_bcm.initstate=1;

  // We enforce a screen size sanity limit of 4096. Could be as high as 32767 if we felt like it.
  graphics_get_display_size(0,&ctm_screenw,&ctm_screenh);
  if ((ctm_screenw<1)||(ctm_screenh<1)) { ctm_bcm_quit(); return -1; }
  if ((ctm_screenw>4096)||(ctm_screenh>4096)) { ctm_bcm_quit(); return -1; }

  if (!(ctm_bcm.vcdisplay=vc_dispmanx_display_open(0))) { ctm_bcm_quit(); return -1; }
  if (!(ctm_bcm.vcupdate=vc_dispmanx_update_start(0))) { ctm_bcm_quit(); return -1; }

  VC_RECT_T srcr={0,0,ctm_screenw<<16,ctm_screenh<<16};
  VC_RECT_T dstr={0,0,ctm_screenw,ctm_screenh};
  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xffffffff};
  if (!(ctm_bcm.vcelement=vc_dispmanx_element_add(
    ctm_bcm.vcupdate,ctm_bcm.vcdisplay,1,&dstr,0,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  ))) { ctm_bcm_quit(); return -1; }

  ctm_bcm.eglwindow.element=ctm_bcm.vcelement;
  ctm_bcm.eglwindow.width=ctm_screenw;
  ctm_bcm.eglwindow.height=ctm_screenh;

  if (vc_dispmanx_update_submit_sync(ctm_bcm.vcupdate)<0) { ctm_bcm_quit(); return -1; }

  static const EGLint eglattr[]={
    EGL_RED_SIZE,8,
    EGL_GREEN_SIZE,8,
    EGL_BLUE_SIZE,8,
    EGL_ALPHA_SIZE,0,
    EGL_DEPTH_SIZE,0,
    EGL_LUMINANCE_SIZE,EGL_DONT_CARE,
    EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
    EGL_SAMPLES,1,
  EGL_NONE};
  static EGLint ctxattr[]={
    EGL_CONTEXT_CLIENT_VERSION,2,
  EGL_NONE};

  ctm_bcm.egldisplay=eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (eglGetError()!=EGL_SUCCESS) { ctm_bcm_quit(); return -1; }

  eglInitialize(ctm_bcm.egldisplay,0,0);
  if (eglGetError()!=EGL_SUCCESS) { ctm_bcm_quit(); return -1; }
  ctm_bcm.initstate=2;

  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint configc=0;
  eglChooseConfig(ctm_bcm.egldisplay,eglattr,&ctm_bcm.eglconfig,1,&configc);
  if (eglGetError()!=EGL_SUCCESS) { ctm_bcm_quit(); return -1; }
  if (configc<1) { ctm_bcm_quit(); return -1; }

  ctm_bcm.eglsurface=eglCreateWindowSurface(ctm_bcm.egldisplay,ctm_bcm.eglconfig,&ctm_bcm.eglwindow,0);
  if (eglGetError()!=EGL_SUCCESS) { ctm_bcm_quit(); return -1; }
  ctm_bcm.initstate=3;

  ctm_bcm.eglcontext=eglCreateContext(ctm_bcm.egldisplay,ctm_bcm.eglconfig,0,ctxattr);
  if (eglGetError()!=EGL_SUCCESS) { ctm_bcm_quit(); return -1; }

  eglMakeCurrent(ctm_bcm.egldisplay,ctm_bcm.eglsurface,ctm_bcm.eglsurface,ctm_bcm.eglcontext);
  if (eglGetError()!=EGL_SUCCESS) { ctm_bcm_quit(); return -1; }

  eglSwapInterval(ctm_bcm.egldisplay,1);

  return 0;
}

/* Quit.
 */

void ctm_bcm_quit() {
  if (ctm_bcm.initstate>=3) {
    eglMakeCurrent(ctm_bcm.egldisplay,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroySurface(ctm_bcm.egldisplay,ctm_bcm.eglsurface);
  }
  if (ctm_bcm.initstate>=2) {
    eglTerminate(ctm_bcm.egldisplay);
    eglReleaseThread();
  }
  if (ctm_bcm.initstate>=1) bcm_host_deinit();
  memset(&ctm_bcm,0,sizeof(ctm_bcm));
}

/* Swap buffers.
 */

int ctm_bcm_swap() {
  if (!ctm_bcm.initstate) return -1;
  eglSwapBuffers(ctm_bcm.egldisplay,ctm_bcm.eglsurface);
  return 0;
}
