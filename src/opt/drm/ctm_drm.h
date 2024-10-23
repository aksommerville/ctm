/* ctm_drm.h
 * Linux Direct Rendering Manager.
 */
 
#ifndef CTM_DRM_H
#define CTM_DRM_H

void ctm_drm_quit();
int ctm_drm_init(const char *device);
int ctm_drm_swap();

#endif
