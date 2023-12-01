#ifndef CTM_DRM_INTERNAL_H
#define CTM_DRM_INTERNAL_H

#include "ctm_drm.h"
#include "ctm.h"
#include "video/ctm_video.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

struct drm_fb {
  uint32_t fbid;
  int handle;
  int size;
};

extern struct ctm_drm_driver {
  int fd;
  
  int mmw,mmh; // monitor's physical size
  int w,h; // monitor's logical size in pixels
  int rate; // monitor's refresh rate in hertz
  drmModeModeInfo mode; // ...and more in that line
  int connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  
  int flip_pending;
  struct drm_fb fbv[2];
  int fbp;
  struct gbm_bo *bo_pending;
  int crtcunset;
  
  struct gbm_device *gbmdevice;
  struct gbm_surface *gbmsurface;
  EGLDisplay egldisplay;
  EGLContext eglcontext;
  EGLSurface eglsurface;
  
} ctm_drm;

int drm_open_file(const char *device);
int drm_configure();
int drm_init_gx();

#endif
