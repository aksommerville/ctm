/* ctm_fs.h
 */

#ifndef CTM_FS_H
#define CTM_FS_H

#if CTM_ARCH==CTM_ARCH_mswin
  #define CTM_PATHSEP_CH '\\'
  #define CTM_PATHSEP_STR "\\"
#else
  #define CTM_PATHSEP_CH '/'
  #define CTM_PATHSEP_STR "/"
#endif

int ctm_file_read(void *dstpp,const char *path);
int ctm_file_write(const char *path,const void *src,int srcc);

/* Return the root directory, eg "/usr/local/share/ctm/".
 * If it hasn't been determined yet, determine it. That only happens once per run.
 * This must begin with '/', must exist, and must be a directory.
 * Use the first match:
 *   - $CTM_ROOT
 *   - {EXECUTABLE}/../ctm-data (ie in same directory as executable, if we can determine its absolute path)
 *   - {EXECUTABLE}/../../Resources (MacOS bundles)
 *   - /usr/local/share/ctm
 *   - /usr/share/ctm
 *   - $HOME/.ctm
 *   - ~/.ctm (Discovered with passwd rather than $HOME).
 *   - ./ctm-data
 *   - ./.ctm-data
 * If it fails, we log a friendly message and return NULL. Caller should abort.
 */
const char *ctm_get_root();

/* Return a shared static string with (sfx) appended to the root path.
 * Returns NULL if either is invalid or doesn't fit in our buffer.
 * Next call invalidates the result.
 * eg "foos/foo.bar" => "/usr/local/share/ctm/foos/foo.bar"
 */
const char *ctm_data_path(const char *sfx);

void ctm_set_argv0(const char *src);

/* Returns a sensible path in the user's home directory.
 * Base name is formed like "ctm-screenshot-YYYMMDDHHMM.WxH.rgba".
 */
int ctm_get_screenshot_path(char *dst,int dsta,int w,int h);

#endif
