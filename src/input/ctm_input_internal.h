#ifndef CTM_INPUT_INTERNAL_H
#define CTM_INPUT_INTERNAL_H

#include "ctm.h"
#include "ctm_input.h"
#if CTM_USE_evdev
  #include "opt/evdev/ctm_evdev.h"
#endif
#if CTM_USE_sdl
  #include "opt/sdl/ctm_sdl.h"
#endif
#if CTM_USE_glx
  #include "opt/glx/ctm_glx.h"
#endif

/* Device.
 */

struct ctm_input_device {
  int devid;      // >0, unique among all registered devices.
  int playerid;   // 0=unset, 1..4=playerid, no other value permitted.
  uint16_t state; // Mask of buttons pressed right now.
  int priority;   // Joystick = 10, keyboard or uncertain = 0. When more devices than players, we spread out the hi-prio devs.
};

/* Globals.
 */

extern struct ctm_input {

  struct ctm_input_device *devv;
  int devc,deva;
  int playerc;

  // List of definitions is constructed during init and never changes after that.
  struct ctm_input_definition *defv;
  int defc,defa;

} ctm_input;

/* Private API.
 */

int ctm_input_devv_search(int devid);
int ctm_input_devv_insert(int p,int devid);
int ctm_input_devv_remove(int p);

int ctm_input_configure(const char *src,int srcc,const char *refname);

struct ctm_input_definition *ctm_input_add_definition(const char *name,int namec);
int ctm_input_definition_add_field(struct ctm_input_definition *def,const struct ctm_input_field *fld);

#endif
