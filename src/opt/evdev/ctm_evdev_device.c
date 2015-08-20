#include "ctm_evdev_internal.h"
#include "io/ctm_fs.h"

extern int ctm_screenw,ctm_screenh;

/* Delete device.
 */

void ctm_evdev_device_del(struct ctm_evdev_device *dev) {
  if (!dev) return;
  if (ctm_poll_close(dev->fd,dev)>=0) return;
  ctm_input_unregister_device(dev->devid);
  int i; for (i=0;i<ctm_evdev.devc;i++) if (ctm_evdev.devv[i]==dev) {
    ctm_evdev.devc--;
    memmove(ctm_evdev.devv+i,ctm_evdev.devv+i+1,sizeof(void*)*(ctm_evdev.devc-i));
    break;
  }
  if (dev->fd>=0) {
    if (dev->grabbed) ioctl(dev->fd,EVIOCGRAB,0);
    close(dev->fd);
  }
  if (dev->name) free(dev->name);
  if (dev->absv) free(dev->absv);
  if (dev->keyv) free(dev->keyv);
  free(dev);
}

/* Set name.
 */
 
int ctm_evdev_device_set_name(struct ctm_evdev_device *dev,const char *src,int srcc) {
  if (!dev) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  if (srcc==INT_MAX) return -1;
  char *nname=0;
  if (srcc) {
    if (!(nname=malloc(srcc+1))) return -1;
    int i; for (i=0;i<srcc;i++) if ((src[i]<0x20)||(src[i]>0x7e)) nname[i]='?'; else nname[i]=src[i];
    nname[srcc]=0;
  }
  if (dev->name) free(dev->name);
  dev->name=nname;
  return 0;
}

/* Search device registry.
 */
 
struct ctm_evdev_device *ctm_evdev_device_for_evid(int evid) {
  int i; for (i=0;i<ctm_evdev.devc;i++) if (ctm_evdev.devv[i]->evid==evid) return ctm_evdev.devv[i];
  return 0;
}

/* Search for key or axis record.
 */

static int ctm_evdev_device_keyv_search(const struct ctm_evdev_device *dev,uint16_t code) {
  int lo=0,hi=dev->keyc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (code<dev->keyv[ck].code) hi=ck;
    else if (code>dev->keyv[ck].code) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ctm_evdev_device_absv_search(const struct ctm_evdev_device *dev,uint16_t code) {
  int lo=0,hi=dev->absc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (code<dev->absv[ck].code) hi=ck;
    else if (code>dev->absv[ck].code) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Read from device.
 */

static int ctm_evdev_device_read(int fd,void *userdata) {
  struct ctm_evdev_device *dev=userdata;
  struct input_event evtv[16];
  int evtc=read(dev->fd,evtv,sizeof(evtv));
  if (evtc<=0) return ctm_poll_close(fd,userdata);
  evtc/=sizeof(struct input_event);
  int evtp; for (evtp=0;evtp<evtc;evtp++) switch (evtv[evtp].type) {
  
    case EV_KEY: if (dev->usage==CTM_EVDEV_USAGE_POINTER) switch (evtv[evtp].code) {
        case BTN_LEFT: ctm_editor_input.ptrleft=evtv[evtp].value?1:0; break;
        case BTN_RIGHT: ctm_editor_input.ptrright=evtv[evtp].value?1:0; break;
      } else {
        int keyp=ctm_evdev_device_keyv_search(dev,evtv[evtp].code);
        if (keyp<0) break;
        struct ctm_evdev_key *key=dev->keyv+keyp;
        if (evtv[evtp].value) {
          if (key->value) break;
          key->value=1;
          if (key->btnid&&(ctm_input_event(dev->devid,key->btnid,1))<0) return -1;
        } else {
          if (!key->value) break;
          key->value=0;
          if (key->btnid&&(ctm_input_event(dev->devid,key->btnid,0))<0) return -1;
        }
      } break;

    case EV_ABS: {
        int absp=ctm_evdev_device_absv_search(dev,evtv[evtp].code);
        if (absp<0) break;
        struct ctm_evdev_abs *abs=dev->absv+absp;
        int value;
        if (evtv[evtp].value<=abs->lo) value=-1;
        else if (evtv[evtp].value>=abs->hi) value=1;
        else value=0;
        if (value==abs->value) break;
        if ((abs->value<0)&&abs->btnidlo) {
          if (ctm_input_event(dev->devid,abs->btnidlo,0)<0) return -1;
        } else if ((abs->value>0)&&abs->btnidhi) {
          if (ctm_input_event(dev->devid,abs->btnidhi,0)<0) return -1;
        }
        abs->value=value;
        if ((abs->value<0)&&abs->btnidlo) {
          if (ctm_input_event(dev->devid,abs->btnidlo,1)<0) return -1;
        } else if ((abs->value>0)&&abs->btnidhi) {
          if (ctm_input_event(dev->devid,abs->btnidhi,1)<0) return -1;
        }
      } break;

    case EV_REL: switch (evtv[evtp].code) {
        case REL_X: ctm_editor_input.ptrx+=evtv[evtp].value; break;
        case REL_Y: ctm_editor_input.ptry+=evtv[evtp].value; break;
        case REL_WHEEL: ctm_editor_input.ptrwheely=(evtv[evtp].value<0)?1:(evtv[evtp].value>0)?-1:0; break;
        case REL_HWHEEL: ctm_editor_input.ptrwheelx=(evtv[evtp].value<0)?-1:(evtv[evtp].value>0)?1:0; break;
      } break;
      
  }
  return 0;
}

/* Map source buttons to outputs.
 */

static uint16_t ctm_evdev_map_key(struct ctm_evdev_device *dev,uint16_t code) {
  if (dev->def) {
    int i; for (i=0;i<dev->def->fldc;i++) {
      struct ctm_input_field *fld=dev->def->fldv+i;
      if (fld->srcbtnid!=code) continue;
      if (fld->srcscope==EV_KEY) ;
      else if (fld->srcscope) continue;
      else if (fld->dstbtnidlo||!fld->dstbtnidhi) continue;
      return fld->dstbtnidhi;
    }
  }
  switch (code) {

    /* Keyboard directions. */
    case KEY_UP: return CTM_BTNID_UP;
    case KEY_DOWN: return CTM_BTNID_DOWN;
    case KEY_LEFT: return CTM_BTNID_LEFT;
    case KEY_RIGHT: return CTM_BTNID_RIGHT;
    case KEY_H: return CTM_BTNID_LEFT;
    case KEY_J: return CTM_BTNID_DOWN;
    case KEY_K: return CTM_BTNID_UP;
    case KEY_L: return CTM_BTNID_RIGHT;
    case KEY_KP8: return CTM_BTNID_UP;
    case KEY_KP4: return CTM_BTNID_LEFT;
    case KEY_KP5: return CTM_BTNID_DOWN;
    case KEY_KP6: return CTM_BTNID_RIGHT;
    case KEY_KP2: return CTM_BTNID_DOWN;

    /* Keyboard action buttons. */
    case KEY_Z: return CTM_BTNID_PRIMARY;
    case KEY_X: return CTM_BTNID_SECONDARY;
    case KEY_C: return CTM_BTNID_TERTIARY;
    case KEY_KP0: return CTM_BTNID_PRIMARY;
    case KEY_KPDOT: return CTM_BTNID_SECONDARY;
    case KEY_KPENTER: return CTM_BTNID_TERTIARY;

    /* Keyboard signal triggers. */
    case KEY_ESC: return CTM_BTNID_QUIT;
    case KEY_SPACE: return CTM_BTNID_PAUSE;
    case KEY_ENTER: return CTM_BTNID_PAUSE;
    case KEY_SYSRQ: return CTM_BTNID_SCREENSHOT;
    case KEY_PRINT: return CTM_BTNID_SCREENSHOT;

    /* Joystick buttons. */
    case BTN_A: return CTM_BTNID_PRIMARY;
    case BTN_B: return CTM_BTNID_SECONDARY;
    case BTN_C: return CTM_BTNID_TERTIARY;
    case BTN_X: return CTM_BTNID_TERTIARY;
    case BTN_Y: return CTM_BTNID_SECONDARY;
    case BTN_Z: return CTM_BTNID_PRIMARY;
    case BTN_TL: return CTM_BTNID_PRIMARY;
    case BTN_TR: return CTM_BTNID_SECONDARY;
    case BTN_TL2: return CTM_BTNID_TERTIARY;
    case BTN_TR2: return CTM_BTNID_PRIMARY;
    case BTN_START: return CTM_BTNID_PAUSE;
    case BTN_SELECT: return CTM_BTNID_QUIT;
    case BTN_MODE: return CTM_BTNID_QUIT;

    /* Any other button in the 'MISC' 'JOYSTICK' 'GAMEPAD' 'WHEEL' groups, assign to one of (PRIMARY,SECONDARY,TERTIARY). */
    default: switch (code&~15) {
        case BTN_MISC:
        case BTN_JOYSTICK:
        case BTN_GAMEPAD:
        case BTN_WHEEL: switch (code%3) {
            case 0: return CTM_BTNID_PRIMARY;
            case 1: return CTM_BTNID_SECONDARY;
            case 2: return CTM_BTNID_TERTIARY;
          }
      } break;
    
  }
  return 0;
}

/* Get button IDs for absolute axis.
 */

static void ctm_evdev_map_abs(uint16_t *btnidlo,uint16_t *btnidhi,struct ctm_evdev_device *dev,uint16_t code) {
  if (dev->def) {
    int i; for (i=0;i<dev->def->fldc;i++) {
      struct ctm_input_field *fld=dev->def->fldv+i;
      if (fld->srcbtnid!=code) continue;
      if (fld->srcscope==EV_ABS) ;
      else if (fld->srcscope) continue;
      else if (!fld->dstbtnidlo||!fld->dstbtnidhi) continue;
      *btnidlo=fld->dstbtnidlo;
      *btnidhi=fld->dstbtnidhi;
      return;
    }
  }
  switch (code) {
    case ABS_X: case ABS_RX: case ABS_HAT0X: case ABS_HAT1X: case ABS_HAT2X: case ABS_HAT3X: {
        if (!dev->def) printf("  %d.0x%x LEFT RIGHT\n",EV_ABS,code);
        *btnidlo=CTM_BTNID_LEFT;
        *btnidhi=CTM_BTNID_RIGHT;
      } return;
    case ABS_Y: case ABS_RY: case ABS_HAT0Y: case ABS_HAT1Y: case ABS_HAT2Y: case ABS_HAT3Y: {
        if (!dev->def) printf("  %d.0x%x UP DOWN\n",EV_ABS,code);
        *btnidlo=CTM_BTNID_UP;
        *btnidhi=CTM_BTNID_DOWN;
      } return;
    default: if (!dev->def) printf("# %d.0x%x unused\n",EV_ABS,code); break;
  }
}

/* Define capabilities. (at connection)
 */

static int ctm_evdev_device_define_keys(struct ctm_evdev_device *dev,const uint8_t *src,int srcc) {
  dev->keyc=0; // Should already be zero, but let's be sure, since we're appending blindly.
  int major; for (major=0;major<srcc;major++) {
    if (!src[major]) continue;
    int minor; for (minor=0;minor<8;minor++) {
      if (!(src[major]&(1<<minor))) continue;
      uint16_t code=(major<<3)|minor;
      if (dev->keyc>=dev->keya) {
        int na=dev->keya+16;
        if (na>INT_MAX/sizeof(struct ctm_evdev_key)) return -1;
        void *nv=realloc(dev->keyv,sizeof(struct ctm_evdev_key)*na);
        if (!nv) return -1;
        dev->keyv=nv;
        dev->keya=na;
      }
      struct ctm_evdev_key *key=dev->keyv+dev->keyc++;
      memset(key,0,sizeof(struct ctm_evdev_key));
      key->code=code;
      key->btnid=ctm_evdev_map_key(dev,code);
      if (!dev->def) {
        if (key->btnid) printf("  %d.0x%x %s\n",EV_KEY,code,ctm_input_btnid_name(key->btnid));
        else if (key->btnid>=BTN_MISC) printf("# %d.0x%x unused\n",EV_KEY,code); // Don't bother printing the 100 or so ordinary keyboard keys.
      }
    }
  }
  return 0;
}

static int ctm_evdev_device_define_abs(struct ctm_evdev_device *dev,const uint8_t *src,int srcc) {
  dev->absc=0; // Should already be zero, but let's be sure, since we're appending blindly.
  int major; for (major=0;major<srcc;major++) {
    if (!src[major]) continue;
    int minor; for (minor=0;minor<8;minor++) {
      if (!(src[major]&(1<<minor))) continue;
      uint16_t code=(major<<3)|minor;
      struct input_absinfo info={0};
      if (ioctl(dev->fd,EVIOCGABS(code),&info)<0) continue;
      if (dev->absc>=dev->absa) {
        int na=dev->absa+8;
        if (na>INT_MAX/sizeof(struct ctm_evdev_abs)) return -1;
        void *nv=realloc(dev->absv,sizeof(struct ctm_evdev_abs)*na);
        if (!nv) return -1;
        dev->absv=nv;
        dev->absa=na;
      }
      struct ctm_evdev_abs *abs=dev->absv+dev->absc++;
      memset(abs,0,sizeof(struct ctm_evdev_abs));
      abs->code=code;
      if (info.minimum<info.maximum) { abs->lo=info.minimum; abs->hi=info.maximum; }
      else { abs->lo=info.maximum; abs->hi=info.minimum; }
      int mid=(abs->lo+abs->hi)>>1;
      int lo=(abs->lo+mid)>>1;
      int hi=(abs->hi+mid)>>1;
      if (lo<hi-1) { abs->lo=lo; abs->hi=hi; }
      if (info.value<=abs->lo) abs->value=-1;
      else if (info.value>=abs->hi) abs->value=1;
      else abs->value=0;
      ctm_evdev_map_abs(&abs->btnidlo,&abs->btnidhi,dev,code);
    }
  }
  return 0;
}

/* Try a file. No error if it is not evdev, or fails to open.
 */

int ctm_evdev_try_file(const char *basename) {

  /* Examine file name. Must be "event[0-9]+".
   * Check whether it is already open.
   */
  if (!basename) return 0;
  if (memcmp(basename,"event",5)) return 0;
  if (!basename[5]) return -1;
  int evid=0,basenamec=5;
  while (basename[basenamec]) {
    int digit=basename[basenamec++]-'0';
    if ((digit<0)||(digit>9)) return 0;
    if (evid>INT_MAX/10) return 0; evid*=10;
    if (evid>INT_MAX-digit) return 0; evid+=digit;
  }
  struct ctm_evdev_device *dev=ctm_evdev_device_for_evid(evid);
  if (dev) return 0;

  /* Open file. No error if it fails.
   * Call EVIOCGVERSION to confirm that it is evdev.
   */
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/%s",CTM_EVDEV_PATH,basename);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  int fd=open(path,O_RDONLY|O_CLOEXEC);
  if (fd<0) return 0;
  int version=0;
  if (ioctl(fd,EVIOCGVERSION,&version)<0) { close(fd); return 0; }

  /* Create the device object.
   * Hand off file descriptor.
   */
  if (ctm_evdev.devc>=ctm_evdev.deva) {
    int na=ctm_evdev.deva+8;
    if (na>INT_MAX/sizeof(void*)) { close(fd); return -1; }
    void *nv=realloc(ctm_evdev.devv,sizeof(void*)*na);
    if (!nv) { close(fd); return -1; }
    ctm_evdev.devv=nv;
    ctm_evdev.deva=na;
  }
  dev=calloc(1,sizeof(struct ctm_evdev_device));
  if (!dev) { close(fd); return -1; }
  ctm_evdev.devv[ctm_evdev.devc++]=dev;
  dev->fd=fd;
  dev->evid=evid;

  /* Device name and ID. */
  ioctl(dev->fd,EVIOCGID,&dev->id);
  char name[256]={0};
  ioctl(dev->fd,EVIOCGNAME(sizeof(name)),name);
  if (name[0]) {
    name[sizeof(name)-1]=0;
    ctm_evdev_device_set_name(dev,name,-1);
  }

  /* Now that we have the name, look for a definition (user config info). */
  if (dev->def=ctm_input_get_definition(dev->name,-1)) {
    // super
  } else {
    printf("%s: No definition found for device. Copy the following into your config file (%s)...\n",path,ctm_data_path("input.cfg"));
    printf("%s\n",(dev->name&&dev->name[0])?dev->name:"*");
  }

  /* Device capabilities. */
  uint8_t keybit[(KEY_MAX+7)>>3]={0};
  uint8_t absbit[(ABS_MAX+7)>>3]={0};
  ioctl(dev->fd,EVIOCGBIT(EV_KEY,sizeof(keybit)),keybit);
  ctm_evdev_device_define_keys(dev,keybit,sizeof(keybit));
  ioctl(dev->fd,EVIOCGBIT(EV_ABS,sizeof(absbit)),absbit);
  ctm_evdev_device_define_abs(dev,absbit,sizeof(absbit));

  if (!dev->def) printf("end\n");

  /* Set (playerok) if we have (UP,DOWN,LEFT,RIGHT) and at least one of (PRIMARY,SECONDARY,TERTIARY). */
  uint16_t havebtns=0;
  int i; 
  for (i=0;i<dev->keyc;i++) if (!(dev->keyv[i].btnid&CTM_BTNID_SIGNAL)) havebtns|=dev->keyv[i].btnid;
  for (i=0;i<dev->absc;i++) {
    if (!(dev->absv[i].btnidlo&CTM_BTNID_SIGNAL)) havebtns|=dev->absv[i].btnidlo;
    if (!(dev->absv[i].btnidhi&CTM_BTNID_SIGNAL)) havebtns|=dev->absv[i].btnidhi;
  }
  if ((havebtns&(CTM_BTNID_LEFT|CTM_BTNID_RIGHT|CTM_BTNID_UP|CTM_BTNID_DOWN))==(CTM_BTNID_LEFT|CTM_BTNID_RIGHT|CTM_BTNID_UP|CTM_BTNID_DOWN)) {
    if (havebtns&(CTM_BTNID_PRIMARY|CTM_BTNID_SECONDARY|CTM_BTNID_TERTIARY)) {
      dev->usage=CTM_EVDEV_USAGE_PLAYER;
    }
  }

  /* If we're not a player, are we a pointer? */
  if (dev->usage==CTM_EVDEV_USAGE_NONE) {
    if (keybit[BTN_LEFT>>3]&(1<<(BTN_LEFT&7))) {
      uint8_t relbit[(REL_MAX+7)>>3]={0};
      ioctl(dev->fd,EVIOCGBIT(EV_REL,sizeof(relbit)),relbit);
      if ((relbit[REL_X>>3]&(1<<(REL_X&7)))&&(relbit[REL_Y>>3]&(1<<(REL_Y&7)))) {
        dev->usage=CTM_EVDEV_USAGE_POINTER;
      }
    }
  }

  /* Try to grab the device. */
  if (ioctl(fd,EVIOCGRAB,1)>=0) dev->grabbed=1;

  /* Register with our other systems. */
  if (ctm_poll_open(dev->fd,dev,ctm_evdev_device_read,0,0,(void(*)(void*))ctm_evdev_device_del)<0) return -1;
  if (dev->usage==CTM_EVDEV_USAGE_PLAYER) {
    if ((dev->devid=ctm_input_register_device())<1) return -1;
    #define HAVEABS(code) (absbit[ABS_##code>>3]&(1<<(ABS_##code&7)))
    #define HAVEPAIR(code) (HAVEABS(code##X)&&HAVEABS(code##Y))
    if (HAVEPAIR()||HAVEPAIR(R)||HAVEPAIR(HAT0)||HAVEPAIR(HAT1)||HAVEPAIR(HAT2)||HAVEPAIR(HAT3)) {
      if (ctm_input_device_set_priority(dev->devid,10)<0) return -1;
    }
    #undef HAVEPAIR
    #undef HAVEABS
  }

  return 1;
}
