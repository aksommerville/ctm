#include "ctm.h"
#include "ctm_fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#if CTM_ARCH==CTM_ARCH_mswin
  #define CTM_USE_pwd_h 0
  #define OPENMODE O_RDONLY|O_BINARY
  #define OPENMODEW O_WRONLY|O_CREAT|O_TRUNC|O_BINARY
#else
  #define CTM_USE_pwd_h 1
  #include <pwd.h>
  #define OPENMODE O_RDONLY
  #define OPENMODEW O_WRONLY|O_CREAT|O_TRUNC
#endif

/* Read file.
 */

int ctm_file_read(void *dstpp,const char *path) {
  if (!dstpp||!path) return -1;
  int fd;
  while (1) {
    if ((fd=open(path,OPENMODE))>=0) break;
    if (errno!=EINTR) return -1;
  }
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) { close(fd); return -1; }
  char *dst=malloc(flen);
  if (!dst) { close(fd); return -1; }
  int dstc=0,err;
  while (dstc<flen) {
    if ((err=read(fd,dst+dstc,flen-dstc))<=0) {
      if (err&&(errno==EINTR)) continue;
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Write file.
 */

int ctm_file_write(const char *path,const void *src,int srcc) {
  if (!path) return -1;
  if (!src) srcc=0;
  int fd;
  while (1) {
    if ((fd=open(path,OPENMODEW,0666))>=0) break;
    if (errno!=EINTR) return -1;
  }
  int srcp=0,err;
  while (srcp<srcc) {
    if ((err=write(fd,(char*)src+srcp,srcc-srcp))<=0) {
      if (err&&(errno==EINTR)) continue;
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}

/* Absolute path?
 */
 
static int ctm_path_is_absolute(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  #if CTM_ARCH==CTM_ARCH_mswin
    if ((srcc>=3)&&(src[1]==':')&&(src[2]=='\\')) {
      if (((src[0]>='A')&&(src[0]<='Z'))||((src[0]>='a')&&(src[0]<='z'))) return 1;
    }
    return 0;
  #else
    return (src[0]=='/')?1:0;
  #endif
}

/* Root directory internals.
 */

static char ctm_root_path[1024]="";

static const char *ctm_root_path_keep(const char *src) {
  if (!src) return 0;
  int srcc=0; while (src[srcc]) srcc++;
  if ((srcc<1)||!ctm_path_is_absolute(src,srcc)||(srcc>=sizeof(ctm_root_path))) return 0;
  memcpy(ctm_root_path,src,srcc);
  ctm_root_path[srcc]=0;
  return ctm_root_path;
}

static int ctm_root_valid(const char *path) {
  if (!path) return 0;
  int pathc=0; while (path[pathc]) pathc++;
  if (!ctm_path_is_absolute(path,pathc)) return 0;
  struct stat st;
  if (stat(path,&st)<0) return 0;
  if (!S_ISDIR(st.st_mode)) return 0;
  return 1;
}

/* Path to executable.
 */

static char ctm_executable_path[1024]="";

static const char *ctm_executable_path_keep(const char *src) {
  if (!src) return 0;
  int srcc=0; while (src[srcc]) srcc++;
  if ((srcc<1)||!ctm_path_is_absolute(src,srcc)||(srcc>=sizeof(ctm_executable_path))) return 0;
  memcpy(ctm_executable_path,src,srcc);
  ctm_executable_path[srcc]=0;
  return ctm_executable_path;
}

void ctm_set_argv0(const char *src) {
  if (!src) return;
  int srcc=0; while (src[srcc]) srcc++;

  // If it begins with slash, it is already the absolute path.
  if (ctm_path_is_absolute(src,srcc)) { ctm_executable_path_keep(src); return; }

  // If it contains a slash, it is relative to the working directory.
  int relative=0,i;
  for (i=0;i<srcc;i++) if (src[i]==CTM_PATHSEP_CH) { relative=1; break; }
  if (relative) {
    char path[1024];
    if (getcwd(path,sizeof(path))!=path) return;
    if (path[0]!=CTM_PATHSEP_CH) return;
    int pathc=0; while (path[pathc]) pathc++;
    if (path[pathc-1]!=CTM_PATHSEP_CH) path[pathc++]=CTM_PATHSEP_CH;
    if (pathc>=sizeof(path)-srcc) return;
    memcpy(path+pathc,src,srcc+1);
    struct stat st;
    if (stat(path,&st)<0) return;
    if (!S_ISREG(st.st_mode)) return;
    ctm_executable_path_keep(path);
    return;
  }

  // If no slash, we must search $PATH.
  const char *PATH=getenv("PATH");
  if (PATH) {
    int PATHp=0;
    while (PATH[PATHp]) {
      if (PATH[PATHp]==':') { PATHp++; continue; }
      const char *pfx=PATH+PATHp;
      int pfxc=0;
      while (PATH[PATHp]&&(PATH[PATHp]!=':')) { PATHp++; pfxc++; }
      if ((pfxc>0)&&(pfx[0]==CTM_PATHSEP_CH)) {
        char path[1024];
        if (pfxc<sizeof(path)-srcc-1) {
          memcpy(path,pfx,pfxc);
          if (pfx[pfxc-1]!=CTM_PATHSEP_CH) path[pfxc++]=CTM_PATHSEP_CH;
          memcpy(path+pfxc,src,srcc+1);
          struct stat st;
          if (stat(path,&st)>0) {
            if (S_ISREG(st.st_mode)) { ctm_executable_path_keep(path); return; }
          }
        }
      }
    }
  }
  
}

static const char *ctm_get_root_for_executable() {
  if (!ctm_executable_path[0]) return 0;
  
  char path[1024];
  const char *src=ctm_executable_path;
  int srcc=0; while (src[srcc]) srcc++;

  // Remove final path component, ie executable's base name, and append "ctm-data".
  while (srcc&&(src[srcc-1]!=CTM_PATHSEP_CH)) srcc--;
  if (srcc&&(srcc<sizeof(path)-8)) {
    memcpy(path,src,srcc);
    memcpy(path+srcc,"ctm-data",9);
    if (ctm_root_valid(path)) return ctm_root_path_keep(path);
  }

  // Remove another path component and append "Resources".
  while (srcc&&(src[srcc-1]==CTM_PATHSEP_CH)) srcc--;
  while (srcc&&(src[srcc-1]!=CTM_PATHSEP_CH)) srcc--;
  if (srcc&&(srcc<sizeof(path)-9)) {
    memcpy(path,src,srcc);
    memcpy(path+srcc,"Resources",10);
    if (ctm_root_valid(path)) return ctm_root_path_keep(path);
  }
  
  return 0;
}

/* Get our root directory.
 */
 
const char *ctm_get_root() {
  if (ctm_root_path[0]) return ctm_root_path;
  const char *kstr;
  char path[1024];

  kstr=getenv("CTM_ROOT"); if (ctm_root_valid(kstr)) return ctm_root_path_keep(kstr);

  if (ctm_get_root_for_executable()) return ctm_root_path;

  kstr="/usr/local/share/ctm"; if (ctm_root_valid(kstr)) return ctm_root_path_keep(kstr);
  kstr="/usr/share/ctm"; if (ctm_root_valid(kstr)) return ctm_root_path_keep(kstr);
  
  if (kstr=getenv("HOME")) {
    int kc=1; while (kstr[kc]) kc++;
    if (ctm_path_is_absolute(kstr,kc)) {
      if (kc<sizeof(path)-5) {
        memcpy(path,kstr,kc);
        if (kstr[kc-1]==CTM_PATHSEP_CH) memcpy(path+kc,".ctm",5);
        else memcpy(path+kc,CTM_PATHSEP_STR ".ctm",6);
        if (ctm_root_valid(path)) return ctm_root_path_keep(path);
      }
    }
  }

  #if CTM_USE_pwd_h
    struct passwd *pwd=getpwuid(geteuid());
    if (pwd&&pwd->pw_dir) {
      int kc=1; while (pwd->pw_dir[kc]) kc++;
      if (ctm_path_is_absolute(pwd->pw_dir,kc)) {
        if (kc<sizeof(path)-5) {
          memcpy(path,pwd->pw_dir,kc);
          if (kstr[kc-1]==CTM_PATHSEP_CH) memcpy(path+kc,".ctm",5);
          else memcpy(path+kc,CTM_PATHSEP_STR ".ctm",6);
          if (ctm_root_valid(path)) return ctm_root_path_keep(path);
        }
      }
    }
  #endif

  if (getcwd(path,sizeof(path))==path) {
    int pfxc=0; while (path[pfxc]) pfxc++;
    if ((pfxc>0)&&ctm_path_is_absolute(path,pfxc)) {
      if (pfxc<sizeof(path)-10) {
        if (path[pfxc-1]==CTM_PATHSEP_CH) memcpy(path+pfxc,"ctm-data",9); else memcpy(path+pfxc,CTM_PATHSEP_STR "ctm-data",10);
        if (ctm_root_valid(path)) return ctm_root_path_keep(path);
        if (path[pfxc-1]==CTM_PATHSEP_CH) memcpy(path+pfxc,".ctm-data",10); else memcpy(path+pfxc,CTM_PATHSEP_STR ".ctm-data",11);
        if (ctm_root_valid(path)) return ctm_root_path_keep(path);
      }
    }
  }
      
  return 0;
}

/* Compose data path.
 */

static char ctm_data_path_storage[1024];
 
const char *ctm_data_path(const char *sfx) {
  const char *root=ctm_get_root();
  if (!root||!root[0]) return 0;
  int rootc=0; while (root[rootc]) rootc++;
  int sfxc=0; if (sfx) while (sfx[sfxc]) sfxc++;
  if (rootc>=sizeof(ctm_data_path_storage)-sfxc-1) return 0;
  memcpy(ctm_data_path_storage,root,rootc);
  if ((root[rootc-1]!=CTM_PATHSEP_CH)&&(sfx[0]!=CTM_PATHSEP_CH)) ctm_data_path_storage[rootc++]=CTM_PATHSEP_CH;
  memcpy(ctm_data_path_storage+rootc,sfx,sfxc+1);
  return ctm_data_path_storage;
}

/* Get screenshot path.
 */
 
int ctm_get_screenshot_path(char *dst,int dsta,int w,int h) {
  if (!dst||(dsta<0)) return -1;
  int dstc=0;
  const char *HOME=getenv("HOME");
  if (HOME&&HOME[0]) {
    int HOMEc=0; while (HOME[HOMEc]) HOMEc++;
    if (HOMEc>=dsta) return -1;
    memcpy(dst,HOME,HOMEc);
    dstc=HOMEc;
    if (dstc&&(dst[dstc-1]!=CTM_PATHSEP_CH)) dst[dstc++]=CTM_PATHSEP_CH;
  } else {
    // If HOME not defined, take our chances in the working directory.
  }
  time_t now=time(0);
  struct tm *tm=localtime(&now);
  int basec;
  if (tm) {
    basec=snprintf(dst+dstc,dsta-dstc,
      "ctm-screenshot-%04d%02d%02d%02d%02d%02d.%dx%d.rgba",
      tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,w,h
    );
  } else {
    basec=snprintf(dst+dstc,dsta-dstc,
      "ctm-screenshot-%d.%dx%d.rgba",
      (int)now,w,h
    );
  }
  if ((basec<1)||(dstc>=dsta-basec)) return -1;
  dstc+=basec;
  return dstc;
}
