#ifndef CTM_EVDEV_INTERNAL_H
#define CTM_EVDEV_INTERNAL_H

#include "ctm.h"
#include "input/ctm_input.h"
#include "io/ctm_poll.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <linux/input.h>

#define CTM_EVDEV_PATH "/dev/input"

#define CTM_EVDEV_USAGE_NONE        0
#define CTM_EVDEV_USAGE_PLAYER      1
#define CTM_EVDEV_USAGE_POINTER     2

struct ctm_evdev_abs {
  uint16_t code;
  int lo,hi; // Threshholds to report event. (half of nominal range).
  int value; // Recent adjusted value (-1,0,1).
  uint16_t btnidlo,btnidhi;
};

struct ctm_evdev_key {
  uint16_t code;
  int value;
  uint16_t btnid;
};

struct ctm_evdev_device {
  int devid;
  int evid;
  int fd;
  int grabbed;
  char *name;
  struct input_id id;
  struct ctm_evdev_abs *absv; int absc,absa;
  struct ctm_evdev_key *keyv; int keyc,keya;
  struct ctm_input_definition *def; // weak; may be NULL
  int usage;
};

extern struct ctm_evdev {
  int infd,inwd;
  struct ctm_evdev_device **devv; int devc,deva;
} ctm_evdev;

int ctm_evdev_scan();
int ctm_evdev_inotify();

int ctm_evdev_try_file(const char *basename);
void ctm_evdev_device_del(struct ctm_evdev_device *dev);
int ctm_evdev_device_set_name(struct ctm_evdev_device *dev,const char *src,int srcc);
struct ctm_evdev_device *ctm_evdev_device_for_evid(int evid);

#endif
