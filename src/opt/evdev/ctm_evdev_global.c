#include "ctm_evdev_internal.h"

struct ctm_evdev ctm_evdev={0};

/* Init.
 */

int ctm_evdev_init() {
  memset(&ctm_evdev,0,sizeof(struct ctm_evdev));

  if ((ctm_evdev.infd=inotify_init())<0) return -1;
  fcntl(ctm_evdev.infd,F_SETFD,FD_CLOEXEC);
  if ((ctm_evdev.inwd=inotify_add_watch(ctm_evdev.infd,CTM_EVDEV_PATH,IN_CREATE|IN_ATTRIB))<0) return -1;
  if (ctm_poll_open(ctm_evdev.infd,0,ctm_evdev_inotify,0,0,0)<0) return -1;

  if (ctm_evdev_scan()<0) return -1;
  
  return 0;
}

/* Quit.
 */

void ctm_evdev_quit() {
  if (ctm_evdev.devv) {
    while (ctm_evdev.devc-->0) ctm_evdev_device_del(ctm_evdev.devv[ctm_evdev.devc]);
    free(ctm_evdev.devv);
  }
  if (ctm_evdev.infd>=0) {
    ctm_poll_close(ctm_evdev.infd,0);
    if (ctm_evdev.inwd>=0) inotify_rm_watch(ctm_evdev.infd,ctm_evdev.inwd);
    close(ctm_evdev.infd);
  }
  memset(&ctm_evdev,0,sizeof(struct ctm_evdev));
}

/* Update.
 */

int ctm_evdev_update() {
  return 0;
}

/* Scan.
 */

int ctm_evdev_scan() {
  DIR *dir=opendir(CTM_EVDEV_PATH);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    int err=ctm_evdev_try_file(de->d_name);
    if (err<0) { closedir(dir); return -1; }
  }
  closedir(dir);
  return 0;
}

/* Read from inotify.
 */

int ctm_evdev_inotify() {
  uint8_t buf[1024];
  int bufc=read(ctm_evdev.infd,buf,sizeof(buf));
  if (bufc<=0) return -1;
  int bufp=0; while (bufp<=bufc-sizeof(struct inotify_event)) {
    struct inotify_event *evt=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-evt->len) break;
    bufp+=evt->len;
    if (evt->mask&(IN_CREATE|IN_ATTRIB)) {
      evt->name[evt->len-1]=0;
      int err=ctm_evdev_try_file(evt->name);
      if (err<0) return -1;
    }
  }
  return 0;
}
