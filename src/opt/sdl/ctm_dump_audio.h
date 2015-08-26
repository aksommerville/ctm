/* ctm_dump_audio.h
 * Quick hack to dump all audio output to a file.
 * This only works with the SDL driver, since we do not have a public interface for locking the audio callback.
 */

#ifndef CTM_DUMP_AUDIO_H
#define CTM_DUMP_AUDIO_H

/* Initialize with a valid path to enable audio dump.
 * Initialize with NULL to quietly disable dumping.
 * If path is not NULL but can't be opened for writing, we fail.
 */
int ctm_dump_audio_init(const char *path);
void ctm_dump_audio_quit();

/* "provide" during the io callback.
 * This simply copies to a private buffer.
 * "update" in the main thread whenever it's convenient.
 */
void ctm_dump_audio_provide(const void *src,int srcc);
int ctm_dump_audio_update();

#endif
