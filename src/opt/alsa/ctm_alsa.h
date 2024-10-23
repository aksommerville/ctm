/* ctm_alsa.h
 * Shallow interface to ALSA.
 * Provides an I/O thread that hits your callback with a buffer ready for delivery.
 * And that's all.
 */

#ifndef CTM_ALSA_H
#define CTM_ALSA_H

int ctm_alsa_init(void (*cb)(int16_t *dst,int dstc),const char *device);
void ctm_alsa_quit();

#endif
