/* ctm_evdev.h
 * Simple interface for Linux input.
 */

#ifndef CTM_EVDEV_H
#define CTM_EVDEV_H

int ctm_evdev_init();
void ctm_evdev_quit();
int ctm_evdev_update();

#endif
