#include "ctm_input_internal.h"
#include "video/ctm_video.h"
#include "display/ctm_display.h"
#include "game/ctm_geography.h"
#include "io/ctm_fs.h"
#include "game/ctm_game.h"
#include <signal.h>

/* Globals.
 */

struct ctm_input ctm_input={0};
uint16_t ctm_input_by_playerid[5]={0};
struct ctm_editor_input ctm_editor_input={0};

/* Init.
 */

int ctm_input_init(int is_editor) {
  memset(&ctm_input,0,sizeof(struct ctm_input));
  memset(&ctm_editor_input,0,sizeof(struct ctm_editor_input));

  ctm_input.playerc=0;

  if (is_editor) {
    ctm_editor_input.enabled=1;
  }

  const char *cfgpath=ctm_data_path("input.cfg");
  char *src=0;
  int srcc=ctm_file_read(&src,cfgpath);
  if ((srcc>=0)&&src) {
    if (ctm_input_configure(src,srcc,cfgpath)<0) { free(src); return -1; }
    free(src);
  } else {
    fprintf(stderr,"ctm: Failed to read input config from '%s'. Using defaults.\n",cfgpath);
  }

  #if CTM_USE_evdev
    if (ctm_evdev_init()<0) return -1;
  #endif
  #if CTM_USE_glx
    if (ctm_glx_init_input()<0) return -1;
  #endif
  #if CTM_USE_sdl
    if (ctm_sdl_init_input()<0) return -1;
  #endif
  
  return 0;
}

/* Quit.
 */

void ctm_input_quit() {
  #if CTM_USE_evdev
    ctm_evdev_quit();
  #endif
  #if CTM_USE_glx
    ctm_glx_quit_input();
  #endif
  #if CTM_USE_sdl
    ctm_sdl_quit_input();
  #endif
  if (ctm_input.devv) free(ctm_input.devv);
  if (ctm_input.defv) {
    while (ctm_input.defc-->0) if (ctm_input.defv[ctm_input.defc].fldv) free(ctm_input.defv[ctm_input.defc].fldv);
    free(ctm_input.defv);
  }
  memset(ctm_input_by_playerid,0,sizeof(ctm_input_by_playerid));
  memset(&ctm_input,0,sizeof(struct ctm_input));
}

/* Update.
 */

int ctm_input_update() {

  /* Update optional units. */
  #if CTM_USE_evdev
    if (ctm_evdev_update()<0) return -1;
  #endif
  #if CTM_USE_glx
    if (ctm_glx_update()<0) return -1;
  #endif
  #if CTM_USE_sdl
    if (ctm_sdl_update()<0) return -1;
  #endif

  /* If we are the editor, clamp pointer to screen. */
  if (ctm_editor_input.enabled) {
    if (ctm_editor_input.ptrx<0) ctm_editor_input.ptrx=0;
    else if (ctm_editor_input.ptrx>=ctm_screenw) ctm_editor_input.ptrx=ctm_screenw-1;
    if (ctm_editor_input.ptry<0) ctm_editor_input.ptry=0;
    else if (ctm_editor_input.ptry>=ctm_screenh) ctm_editor_input.ptry=ctm_screenh-1;
  }
  
  return 0;
}

/* Public accessors.
 */
 
int ctm_input_set_player_count(int playerc) {
  if ((playerc<1)||(playerc>4)) return -1;
  if (ctm_input.playerc==playerc) return 0;
  memset(ctm_input_by_playerid,0,sizeof(ctm_input_by_playerid));
  ctm_input.playerc=playerc;
  if (ctm_input_automap_players(ctm_input.playerc)<0) return -1;
  return 0;
}

/* Device list primitives.
 */
 
int ctm_input_devv_search(int devid) {
  int lo=0,hi=ctm_input.devc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (devid<ctm_input.devv[ck].devid) hi=ck;
    else if (devid>ctm_input.devv[ck].devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ctm_input_devv_insert(int p,int devid) {
  if ((p<0)||(p>ctm_input.devc)||(devid<1)) return -1;
  if (p&&(devid<=ctm_input.devv[p-1].devid)) return -1;
  if ((p<ctm_input.devc)&&(devid>=ctm_input.devv[p].devid)) return -1;
  if (ctm_input.devc>=ctm_input.deva) {
    int na=ctm_input.deva+8;
    if (na>INT_MAX/sizeof(struct ctm_input_device)) return -1;
    void *nv=realloc(ctm_input.devv,sizeof(struct ctm_input_device)*na);
    if (!nv) return -1;
    ctm_input.devv=nv;
    ctm_input.deva=na;
  }
  memmove(ctm_input.devv+p+1,ctm_input.devv+p,sizeof(struct ctm_input_device)*(ctm_input.devc-p));
  ctm_input.devc++;
  struct ctm_input_device *dev=ctm_input.devv+p;
  memset(dev,0,sizeof(struct ctm_input_device));
  dev->devid=devid;
  return 0;
}

int ctm_input_devv_remove(int p) {
  if ((p<0)||(p>=ctm_input.devc)) return -1;
  ctm_input.devc--;
  memmove(ctm_input.devv+p,ctm_input.devv+p+1,sizeof(struct ctm_input_device)*(ctm_input.devc-p));
  return 0;
}

/* Register or unregister device, public.
 */
 
int ctm_input_register_device() {
  int p,devid;
  if (ctm_input.devc<1) { p=ctm_input.devc; devid=1; }
  else if (ctm_input.devv[ctm_input.devc-1].devid<INT_MAX) { p=ctm_input.devc; devid=ctm_input.devv[ctm_input.devc-1].devid+1; }
  else {
    p=0; devid=1;
    while (p<ctm_input.devc) {
      if (devid!=ctm_input.devv[p].devid) break;
      p++; devid++;
    }
  }
  if (ctm_input_devv_insert(p,devid)<0) return -1;
  ctm_input_automap_device(devid,ctm_input.playerc);
  return devid;
}

int ctm_input_unregister_device(int devid) {
  return ctm_input_devv_remove(ctm_input_devv_search(devid));
}

/* Public device-map entry points for gross action.
 */
 
int ctm_input_unmap_all_players() {
  int i; for (i=0;i<ctm_input.devc;i++) ctm_input.devv[i].playerid=0;
  return 0;
}

int ctm_input_automap_players(int playerc) {
  if (playerc<1) {
    int i; for (i=0;i<ctm_input.devc;i++) ctm_input.devv[i].playerid=0;
  } else {
    // Sanitize request, then start at the highest priority.
    if (playerc>4) playerc=4;
    int priority=INT_MAX,i,donec=0;
    while (donec<ctm_input.devc) {
      int nextpriority=INT_MIN;
      for (i=0;i<ctm_input.devc;i++) {
        if (ctm_input.devv[i].priority>priority) {
          // Already touched this one.
        } else if (ctm_input.devv[i].priority==priority) {
          // OK, map it.
          ctm_input.devv[i].playerid=donec%playerc+1;
          donec++;
        } else if (ctm_input.devv[i].priority>nextpriority) {
          // Remember this one for the next loop.
          nextpriority=ctm_input.devv[i].priority;
        }
      }
      priority=nextpriority;
    }
  }
  return 0;
}

int ctm_input_automap_device(int devid,int playerc) {
  int devp=ctm_input_devv_search(devid);
  if (devp<0) return -1;
  ctm_input.devv[devp].playerid=0;
  if (playerc<1) return 0;
  if (playerc==1) { // It's easy in this case.
    ctm_input.devv[devp].playerid=1;
    return 1;
  }
  if (playerc>4) playerc=4;

  // Add the priority of each device, minimum 1, into playerid buckets.
  // Whichever, in (1..playerc), is the lowest, that's our new playerid.
  int weight[5]={0};
  int i; for (i=0;i<ctm_input.devc;i++) {
    int playerid=ctm_input.devv[i].playerid;
    if ((playerid<1)||(playerid>playerc)) continue;
    int priority=ctm_input.devv[i].priority;
    if (priority<1) priority=1;
    weight[playerid]+=priority;
  }
  int playerid=1,score=weight[1];
  for (i=2;i<=playerc;i++) if (weight[i]<score) { score=weight[i]; playerid=i; }
  
  ctm_input.devv[devp].playerid=playerid;
  return playerid;
}

/* Public device-map accessors.
 */

int ctm_input_set_playerid(int devid,int playerid) {
  int devp=ctm_input_devv_search(devid);
  if (devp<0) return -1;
  if (playerid<1) ctm_input.devv[devp].playerid=0;
  else if (playerid>=4) ctm_input.devv[devp].playerid=4;
  else ctm_input.devv[devp].playerid=playerid;
  return 0;
}

int ctm_input_get_playerid(int devid) {
  int devp=ctm_input_devv_search(devid);
  if (devp<0) return 0;
  return ctm_input.devv[devp].playerid;
}

int ctm_input_playerid_active(int playerid) {
  if ((playerid<1)||(playerid>4)) return 0;
  int i; for (i=0;i<ctm_input.devc;i++) if (ctm_input.devv[i].playerid==playerid) return 1;
  return 0;
}

int ctm_input_count_players() {
  return ctm_input.playerc;
}

int ctm_input_count_attached_players() {
  int i,usage[5]={0};
  for (i=0;i<ctm_input.devc;i++) usage[ctm_input.devv[i].playerid]=1;
  return usage[1]+usage[2]+usage[3]+usage[4];
}

int ctm_input_count_devices() {
  return ctm_input.devc;
}

int ctm_input_devid_for_index(int index) {
  if ((index<0)||(index>=ctm_input.devc)) return -1;
  return ctm_input.devv[index].devid;
}

int ctm_input_devid_for_playerid(int playerid,int devindex) {
  if (devindex<0) return -1;
  int i; for (i=0;i<ctm_input.devc;i++) {
    if (playerid!=ctm_input.devv[i].playerid) continue;
    if (!devindex--) return ctm_input.devv[i].devid;
  }
  return -1;
}

uint16_t ctm_input_device_state(int devid) {
  int devp=ctm_input_devv_search(devid);
  if (devp<0) return 0;
  return ctm_input.devv[devp].state;
}

int ctm_input_device_set_priority(int devid,int priority) {
  int devp=ctm_input_devv_search(devid);
  if (devp<0) return -1;
  ctm_input.devv[devp].priority=priority;
  return 0;
}

/* Take screenshot.
 */

static int ctm_input_screenshot() {
  char path[1024];
  void *pixels=0;
  int w=0,h=0;
  int size=ctm_display_take_screenshot(&pixels,&w,&h);
  if ((size<0)||!pixels) return -1;
  int pathc=ctm_get_screenshot_path(path,sizeof(path),w,h);
  if ((pathc<0)||(pathc>=sizeof(path))) { free(pixels); return -1; }
  int err=ctm_file_write(path,pixels,size);
  free(pixels);
  if (err<0) return -1;

  // It would be nice of us to encode to PNG or something similar.
  // But I'm ever so lazy...
  printf("Saved screenshot to %s\n",path);
  
  return 0;
}

/* Receive event.
 */
 
int ctm_input_event(int devid,uint16_t btnid,int value) {
  if (!btnid) return 0;
  if (btnid&CTM_BTNID_SIGNAL) {
    if (value) switch (btnid) {

      case CTM_BTNID_QUIT: return ctm_game_user_quit();
      case CTM_BTNID_RESET: return ctm_game_main_menu();
      case CTM_BTNID_SCREENSHOT: return ctm_input_screenshot();
      case CTM_BTNID_RESIZE: return ctm_video_screen_size_changed();
      case CTM_BTNID_FULLSCREEN: return ctm_video_set_fullscreen(-1);
      case CTM_BTNID_FULLSCREEN_1: return ctm_video_set_fullscreen(1);
      case CTM_BTNID_FULLSCREEN_0: return ctm_video_set_fullscreen(0);

    }
  } else {
    int p=ctm_input_devv_search(devid);
    if (p<0) return 0;
    struct ctm_input_device *dev=ctm_input.devv+p;
    if (value) {
      if (dev->state&btnid) return 0;
      dev->state|=btnid;
      ctm_input_by_playerid[dev->playerid]|=btnid;
    } else {
      if (!(dev->state&btnid)) return 0;
      dev->state&=~btnid;
      ctm_input_by_playerid[dev->playerid]&=~btnid;
    }
  }
  return 0;
}

/* Create device definition.
 */
 
struct ctm_input_definition *ctm_input_add_definition(const char *name,int namec) {
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (namec>255) namec=255;
  if (ctm_input.defc>=ctm_input.defa) {
    int na=ctm_input.defa+8;
    if (na>INT_MAX/sizeof(struct ctm_input_definition)) return 0;
    void *nv=realloc(ctm_input.defv,sizeof(struct ctm_input_definition)*na);
    if (!nv) return 0;
    ctm_input.defv=nv;
    ctm_input.defa=na;
  }
  char *nname=0;
  if (namec) {
    if (!(nname=malloc(namec+1))) return 0;
    memcpy(nname,name,namec);
    nname[namec]=0;
    int i; for (i=0;i<namec;i++) if ((nname[i]<0x20)||(nname[i]>0x7e)) nname[i]='?';
  }
  struct ctm_input_definition *def=ctm_input.defv+ctm_input.defc++;
  memset(def,0,sizeof(struct ctm_input_definition));
  def->name=nname;
  def->namec=namec;
  return def;
}

/* Add field to device definition.
 */
 
int ctm_input_definition_add_field(struct ctm_input_definition *def,const struct ctm_input_field *fld) {
  if (!def||!fld) return -1;
  if (def->fldc>=def->flda) {
    int na=def->flda+8;
    if (na>INT_MAX/sizeof(struct ctm_input_field)) return -1;
    void *nv=realloc(def->fldv,sizeof(struct ctm_input_field)*na);
    if (!nv) return -1;
    def->fldv=nv;
    def->flda=na;
  }
  def->fldv[def->fldc++]=*fld;
  return 0;
}

/* Compare device name to pattern.
 * Returns:
 *   <0 for no match.
 *   INT_MAX for perfect match.
 *   0..(INT_MAX-1) for matches with wildcard, value is the count of literal characters matched.
 */

static int ctm_input_compare_name_inner(const char *pat,int patc,const char *src,int srcc) {
  int patp=0,srcp=0,matchc=0,wild=0;
  while (1) {

    // Termination and wildcard.
    if (patp>=patc) {
      if (srcp>=srcc) return wild?matchc:INT_MAX;
      return -1;
    }
    if (pat[patp]=='*') {
      wild=1;
      while ((patp<patc)&&(pat[patp]=='*')) patp++; // contiguous '*' are redundant but legal.
      if (patp>=patc) return matchc; // '*' at end of pattern is a match for sure.
      while (srcp<srcc) {
        int sub=ctm_input_compare_name_inner(pat+patp,patc-patp,src+srcp,srcc-srcp);
        if (sub==INT_MAX) return matchc+(srcc-srcp);
        if (sub>=0) return matchc+sub;
        srcp++;
      }
      return -1;
    }
    if (srcp>=srcc) return -1;

    // Whitespace.
    if ((unsigned char)pat[patp]<=0x20) {
      if ((unsigned char)src[srcp]>0x20) return -1;
      patp++; while ((patp<patc)&&((unsigned char)pat[patp]<=0x20)) patp++;
      srcp++; while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; 
      matchc++; // It only counts as one character, no matter how many it really was.
      continue;
    }

    // Literal digits. Case-insensitive.
    char cha=pat[patp++]; if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    char chb=src[srcp++]; if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha!=chb) return -1;
    matchc++;
  
  }
}

// Strip leading and trailing space first; the inner algorithm would choke on it.
static int ctm_input_compare_name(const char *pat,int patc,const char *src,int srcc) {
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (patc&&((unsigned char)pat[patc-1]<=0x20)) patc--;
  while (patc&&((unsigned char)pat[0]<=0x20)) { patc--; pat++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  return ctm_input_compare_name_inner(pat,patc,src,srcc);
}

/* Find definition for device name.
 */
 
struct ctm_input_definition *ctm_input_get_definition(const char *devname,int devnamec) {
  if (!devname) devnamec=0; else if (devnamec<0) { devnamec=0; while (devname[devnamec]) devnamec++; }
  struct ctm_input_definition *fallback=0;
  int fallbackquality=-1,i;
  for (i=0;i<ctm_input.defc;i++) {
    struct ctm_input_definition *def=ctm_input.defv+i;
    int quality=ctm_input_compare_name(def->name,def->namec,devname,devnamec);
    if (quality<0) continue;
    if (quality==INT_MAX) return def;
    if (quality>fallbackquality) { fallback=def; fallbackquality=quality; }
  }
  return fallback;
}

/* Names for buttons.
 */

static char ctm_input_btnid_name_store[256];
static int ctm_input_btnid_name_store_p=0;

const char *ctm_input_btnid_name(uint16_t btnid) {
  switch (btnid) {

    case CTM_BTNID_UP: return "UP";
    case CTM_BTNID_DOWN: return "DOWN";
    case CTM_BTNID_LEFT: return "LEFT";
    case CTM_BTNID_RIGHT: return "RIGHT";
    case CTM_BTNID_PRIMARY: return "PRIMARY";
    case CTM_BTNID_SECONDARY: return "SECONDARY";
    case CTM_BTNID_TERTIARY: return "TERTIARY";
    case CTM_BTNID_PAUSE: return "PAUSE";

    case CTM_BTNID_QUIT: return "QUIT";
    case CTM_BTNID_RESET: return "RESET";
    case CTM_BTNID_SCREENSHOT: return "SCREENSHOT";
    case CTM_BTNID_FULLSCREEN: return "FULLSCREEN";

  }
  if (ctm_input_btnid_name_store_p>=sizeof(ctm_input_btnid_name_store)-6) ctm_input_btnid_name_store_p=0;
  const char *rtn=ctm_input_btnid_name_store+ctm_input_btnid_name_store_p;
  ctm_input_btnid_name_store[ctm_input_btnid_name_store_p++]='0';
  ctm_input_btnid_name_store[ctm_input_btnid_name_store_p++]='x';
  ctm_input_btnid_name_store[ctm_input_btnid_name_store_p++]="0123456789abcdef"[(btnid>>12)&15];
  ctm_input_btnid_name_store[ctm_input_btnid_name_store_p++]="0123456789abcdef"[(btnid>> 8)&15];
  ctm_input_btnid_name_store[ctm_input_btnid_name_store_p++]="0123456789abcdef"[(btnid>> 4)&15];
  ctm_input_btnid_name_store[ctm_input_btnid_name_store_p++]="0123456789abcdef"[(btnid    )&15];
  ctm_input_btnid_name_store[ctm_input_btnid_name_store_p++]=0;
  return rtn;
}
