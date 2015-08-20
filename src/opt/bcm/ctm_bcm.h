/* ctm_bcm.h
 * Interface to Broadcom video, for Raspberry Pi.
 * This uses EGL to set up an OpenGL ES 2.x context.
 * Only our video unit should use this unit.
 */

#ifndef CTM_BCM_H
#define CTM_BCM_H

#include <GLES2/gl2.h>

int ctm_bcm_init();
void ctm_bcm_quit();
int ctm_bcm_swap();

#endif
