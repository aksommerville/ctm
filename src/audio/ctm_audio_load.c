#include "ctm_audio_internal.h"
#include "io/ctm_fs.h"
#include <math.h>
#include <dirent.h>

/* Load single effect from file.
 */

static int ctm_audio_load_effect(const char *path,int effectid) {
  void *src=0;
  int srcc=ctm_file_read(&src,path);
  if (srcc<0) return -1;
  if (!src) return -1;
  int fxp=ctm_audio_effect_search(effectid);
  if (fxp>=0) { free(src); return -1; }
  fxp=-fxp-1;
  if (ctm_audio_effect_insert(fxp,effectid,src,srcc>>1)<0) { free(src); return -1; }
  return 0;
}

/* Load effects store.
 */

int ctm_audio_load_effects() {
  const char *dirpath=ctm_data_path("sound");
  if (!dirpath) return 0;
  DIR *dir=opendir(dirpath);
  if (!dir) return 0;
  int pathc=0; while (dirpath[pathc]) pathc++;
  char subpath[1024];
  if (pathc>=sizeof(subpath)) { closedir(dir); return -1; }
  memcpy(subpath,dirpath,pathc);
  subpath[pathc++]='/';
  struct dirent *de;
  while (de=readdir(dir)) {
    #if CTM_ARCH!=CTM_ARCH_mswin // MinGW doesn't provide d_type. MacOS does, but doesn't set _DIRENT_HAVE_D_TYPE.
      if (de->d_type!=DT_REG) continue;
    #endif
    int effectid=0,basec=0;
    while (de->d_name[basec]) {
      int digit=de->d_name[basec++]-'0';
      if ((digit<0)||(digit>9)) { effectid=-1; break; }
      if (effectid>INT_MAX/10) { effectid=-1; break; } effectid*=10;
      if (effectid>INT_MAX-digit) { effectid=-1; break; } effectid+=digit;
    }
    if (effectid<1) continue;
    if (pathc>=sizeof(subpath)-basec) continue;
    memcpy(subpath+pathc,de->d_name,basec+1);
    if (ctm_audio_load_effect(subpath,effectid)<0) { closedir(dir); return -1; }
  }
  closedir(dir);
  return 0;
}

/* Load song store.
 */

int ctm_audio_load_songs() {
  const char *dirpath=ctm_data_path("song");
  if (!dirpath) return 0;
  DIR *dir=opendir(dirpath);
  if (!dir) return 0;
  int pathc=0; while (dirpath[pathc]) pathc++;
  char subpath[1024];
  if (pathc>=sizeof(subpath)) { closedir(dir); return -1; }
  memcpy(subpath,dirpath,pathc);
  subpath[pathc++]='/';
  struct dirent *de;
  while (de=readdir(dir)) {
    #if CTM_ARCH!=CTM_ARCH_mswin // MinGW doesn't provide d_type. MacOS does, but doesn't set _DIRENT_HAVE_D_TYPE.
      if (de->d_type!=DT_REG) continue;
    #endif
    int songid=0,basec=0;
    while (de->d_name[basec]) {
      int digit=de->d_name[basec++]-'0';
      if ((digit<0)||(digit>9)) { songid=-1; break; }
      if (songid>INT_MAX/10) { songid=-1; break; } songid*=10;
      if (songid>INT_MAX-digit) { songid=-1; break; } songid+=digit;
    }
    if (songid<1) continue;
    if (pathc>=sizeof(subpath)-basec) continue;
    memcpy(subpath+pathc,de->d_name,basec+1);
    
    struct ctm_song *song=ctm_song_new();
    if (!song) { closedir(dir); return -1; }
    song->songid=songid;
    void *src=0;
    int srcc=ctm_file_read(&src,subpath);
    if (srcc<0) { ctm_song_del(song); closedir(dir); return -1; }
    if (ctm_song_decode(song,src,srcc,subpath)<0) { ctm_song_del(song); closedir(dir); free(src); return -1; }
    free(src);
    if (ctm_audio_insert_song(song)<0) { ctm_song_del(song); closedir(dir); return -1; }
    
  }
  closedir(dir);
  return 0;
}
