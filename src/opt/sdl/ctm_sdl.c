#include "ctm.h"
#include "ctm_sdl.h"
#include "video/ctm_video.h"
#include "input/ctm_input.h"
#include "io/ctm_fs.h"
#include "ctm_dump_audio.h"

/* Globals.
 */

// Set this NULL to disable audio dump. No other changes necessary.
#define CTM_DUMP_AUDIO_PATH 0
 
extern const unsigned char ctm_program_icon[]; // ctm_program_icon.c
extern const int ctm_program_icon_w;
extern const int ctm_program_icon_h;

struct ctm_joystick {
  int sdljoyid; // As reported in events.
  SDL_Joystick *sdljoy;
  char *name;
  int namec;
  int btnc;
  int axisc;
  int hatc;
  int devid;
  struct ctm_input_definition *def;
  uint16_t *btnmapv; // One slot for each button, up to (btnc).
  uint16_t *axismapv; // Two slots for each button, up to (axisc<<1).
  uint8_t *btnv; // Current value for each button.
  int16_t *axisv; // " axis.
  uint8_t *hatv; // " hat.
  // Hats map implicitly.
  // Balls don't map at all.
};

struct ctm_keymap {
  SDLKey keysym;
  uint16_t btnid;
};

static struct {

  int videoinit;
  SDL_Surface *icon;
  SDL_Surface *screen;
  int videoflags;
  int winw,winh; // Constant; size when windowed.
  int screenw,screenh; // Constants; expected size of screen. (0,0) if undetermined (window only)
  int fullscreen;
  
  int audioinit;
  
  int inputinit;
  struct ctm_joystick **joyv;
  int joyc,joya;
  int joyccontig;
  int devid_keyboard;
  struct ctm_keymap *keymapv;
  int keymapc,keymapa;

  int shift;
  
} ctm_sdl={0};

/* Init/quit video.
 */
 
int ctm_sdl_init(int fullscreen) {

  if (ctm_sdl.videoinit) return -1;
  if (SDL_InitSubSystem(SDL_INIT_VIDEO)) return -1;

  SDL_WM_SetCaption("The Campaign Trail of the Mummy","The Campaign Trail of the Mummy");
  if (ctm_sdl.icon=SDL_CreateRGBSurfaceFrom(
    (void*)ctm_program_icon,ctm_program_icon_w,ctm_program_icon_h,32,ctm_program_icon_w<<2,
    #if BYTE_ORDER==BIG_ENDIAN
      0xff000000,0x00ff0000,0x0000ff00,0x000000ff
    #else
      0x000000ff,0x0000ff00,0x00ff0000,0xff000000
    #endif
  )) {
    SDL_WM_SetIcon(ctm_sdl.icon,0);
  }
  #if CTM_ARCH==CTM_ARCH_mswin //XXX For the time being, Windows fullscreen never works. (but I'm only testing it through WINE).
    fullscreen=0;
  #endif

  ctm_sdl.videoinit=1;
  ctm_sdl.videoflags=SDL_OPENGL|SDL_DOUBLEBUF;
  ctm_sdl.winw=960;//640;
  ctm_sdl.winh=540;//480;
  ctm_sdl.fullscreen=0;
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
  const SDL_VideoInfo *vinfo=SDL_GetVideoInfo();
  if (vinfo&&(vinfo->current_w>0)&&(vinfo->current_h>0)) {
    ctm_sdl.screenw=vinfo->current_w;
    ctm_sdl.screenh=vinfo->current_h;
    if (fullscreen) {
      ctm_sdl.fullscreen=1;
      ctm_sdl.videoflags|=SDL_FULLSCREEN;
    }
  } else {
    ctm_sdl.screenw=0;
    ctm_sdl.screenh=0;
  }

  if (ctm_sdl.fullscreen) {
    if (!(ctm_sdl.screen=SDL_SetVideoMode(ctm_sdl.screenw,ctm_sdl.screenh,32,ctm_sdl.videoflags))) return -1;
  } else {
    if (!(ctm_sdl.screen=SDL_SetVideoMode(ctm_sdl.winw,ctm_sdl.winh,32,ctm_sdl.videoflags))) return -1;
  }
  ctm_screenw=ctm_sdl.screen->w;
  ctm_screenh=ctm_sdl.screen->h;
  
  SDL_ShowCursor(0);
  
  #if CTM_ARCH==CTM_ARCH_mswin
    glewInit();
    if (!glewIsSupported("GL_VERSION_2_0")) {
      fprintf(stderr,"ctm: OpenGL 2.x or greater required.\n");
      return -1;
    }
  #endif
  
  return 0;
}

void ctm_sdl_quit() {
  if (!ctm_sdl.videoinit) return;
  if (ctm_sdl.icon) { ctm_sdl.icon->pixels=0; SDL_FreeSurface(ctm_sdl.icon); }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  ctm_sdl.screen=0;
  ctm_sdl.videoinit=0;
}

/* Delete joystick.
 */

static void ctm_joystick_del(struct ctm_joystick *joy) {
  if (!joy) return;
  if (joy->devid>0) ctm_input_unregister_device(joy->devid);
  if (joy->sdljoy) SDL_JoystickClose(joy->sdljoy);
  if (joy->name) free(joy->name);
  if (joy->btnmapv) free(joy->btnmapv);
  if (joy->axismapv) free(joy->axismapv);
  if (joy->btnv) free(joy->btnv);
  if (joy->axisv) free(joy->axisv);
  if (joy->hatv) free(joy->hatv);
  free(joy);
}

/* Setup joystick map from its 'device definition'. (entry in config file)
 */

static int ctm_joystick_setup_map(struct ctm_joystick *joy) {
  if (!joy||!joy->def) return -1;
  int fldi; for (fldi=0;fldi<joy->def->fldc;fldi++) {
    struct ctm_input_field *fld=joy->def->fldv+fldi;
    switch (fld->srcscope) {
      case SDL_JOYAXISMOTION: {
          if ((fld->srcbtnid>=0)&&(fld->srcbtnid<joy->axisc)) {
            joy->axismapv[(fld->srcbtnid<<1)+0]=fld->dstbtnidlo;
            joy->axismapv[(fld->srcbtnid<<1)+1]=fld->dstbtnidhi;
          }
        } break;
      case SDL_JOYBUTTONDOWN: {
          if ((fld->srcbtnid>=0)&&(fld->srcbtnid<joy->btnc)) {
            joy->btnmapv[fld->srcbtnid]=fld->dstbtnidhi;
          }
        } break;
    }
  }
  return 0;
}

/* Setup joystick map by default. Optionally print text for config file.
 */

static int ctm_joystick_setup_default_map(struct ctm_joystick *joy,int print) {
  if (!joy) return -1;
  int i,p;
  for (i=p=0;i<joy->axisc;i++,p+=2) {
    if (i&1) {
      joy->axismapv[p+0]=CTM_BTNID_UP;
      joy->axismapv[p+1]=CTM_BTNID_DOWN;
      if (print) printf("  %d.%d UP DOWN\n",SDL_JOYAXISMOTION,i);
    } else {
      joy->axismapv[p+0]=CTM_BTNID_LEFT;
      joy->axismapv[p+1]=CTM_BTNID_DOWN;
      if (print) printf("  %d.%d LEFT RIGHT\n",SDL_JOYAXISMOTION,i);
    }
  }
  for (i=0;i<joy->btnc;i++) {
    const char *name;
    switch (i&3) {
      case 0: joy->btnmapv[i]=CTM_BTNID_PRIMARY; name="PRIMARY"; break;
      case 1: joy->btnmapv[i]=CTM_BTNID_SECONDARY; name="SECONDARY"; break;
      case 2: joy->btnmapv[i]=CTM_BTNID_TERTIARY; name="TERTIARY"; break;
      case 3: joy->btnmapv[i]=CTM_BTNID_PAUSE; name="PAUSE"; break;
    }
    if (print) printf("  %d.%d %s\n",SDL_JOYBUTTONDOWN,i,name);
  }
  return 0;
}

/* Initialize joystick.
 */

static int ctm_sdl_welcome_joystick(SDL_Joystick *sdljoy,const char *name,int sdljoyid) {
  if (!sdljoy) return -1;
  if (!name) name="(Joystick)";
  if (ctm_sdl.joyc>=ctm_sdl.joya) {
    int na=ctm_sdl.joya+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctm_sdl.joyv,sizeof(void*)*na);
    if (!nv) return -1;
    ctm_sdl.joyv=nv;
    ctm_sdl.joya=na;
  }
  struct ctm_joystick *joy=calloc(1,sizeof(struct ctm_joystick));
  if (!joy) return -1;

  if ((joy->devid=ctm_input_register_device())<1) { ctm_joystick_del(joy); return -1; }
  if (ctm_input_device_set_priority(joy->devid,10)<0) { ctm_joystick_del(joy); return -1; }

  int namec=0; while (name[namec]) namec++;
  while (namec&&((unsigned char)name[namec-1]<=0x20)) namec--;
  while (namec&&((unsigned char)name[0]<=0x20)) { namec--; name++; }
  if (!namec) { name="(Joystick)"; namec=10; }
  if (namec>32) namec=32;
  if (!(joy->name=malloc(namec+1))) { ctm_joystick_del(joy); return -1; }
  int i; for (i=0;i<namec;i++) if ((name[i]<0x20)||(name[i]>0x7e)) joy->name[i]='?'; else joy->name[i]=name[i];
  joy->name[joy->namec=namec]=0;

  joy->sdljoyid=sdljoyid;
  joy->sdljoy=sdljoy;
  if ((joy->btnc=SDL_JoystickNumButtons(joy->sdljoy))<0) joy->btnc=0; else if (joy->btnc>256) joy->btnc=256;
  if (joy->btnc&&!(joy->btnmapv=calloc(sizeof(uint16_t),joy->btnc))) { ctm_joystick_del(joy); return -1; }
  if ((joy->axisc=SDL_JoystickNumAxes(joy->sdljoy))<0) joy->axisc=0; else if (joy->axisc>256) joy->axisc=256;
  if (joy->axisc&&!(joy->axismapv=calloc(sizeof(uint16_t),joy->axisc<<1))) { ctm_joystick_del(joy); return -1; }
  if ((joy->hatc=SDL_JoystickNumHats(joy->sdljoy))<0) joy->hatc=0; // No upper limit and no explicit maps.
  if (joy->btnc&&!(joy->btnv=calloc(1,joy->btnc))) { ctm_joystick_del(joy); return -1; }
  if (joy->axisc&&!(joy->axisv=calloc(2,joy->axisc))) { ctm_joystick_del(joy); return -1; }
  if (joy->hatc&&!(joy->hatv=calloc(1,joy->hatc))) { ctm_joystick_del(joy); return -1; }

  if (joy->def=ctm_input_get_definition(joy->name,joy->namec)) {
    if (ctm_joystick_setup_map(joy)<0) return -1;
  } else {
    printf("No definition found for device. Copy the following into your config file (%s)...\n",ctm_data_path("input.cfg"));
    printf("%s\n",(joy->name&&joy->name[0])?joy->name:"*");
    if (ctm_joystick_setup_default_map(joy,1)<0) return -1;
    if (!joy->def) printf("end\n");
  }

  ctm_sdl.joyv[ctm_sdl.joyc++]=joy;
  return 0;
}

/* Keyboard map.
 */

static int ctm_sdl_keymap_search(SDLKey keysym) {
  int lo=0,hi=ctm_sdl.keymapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (keysym<ctm_sdl.keymapv[ck].keysym) hi=ck;
    else if (keysym>ctm_sdl.keymapv[ck].keysym) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ctm_sdl_add_keymap(SDLKey keysym,uint16_t btnid) {
  int p=ctm_sdl_keymap_search(keysym);
  if (p>=0) return 0; p=-p-1;
  if (ctm_sdl.keymapc>=ctm_sdl.keymapa) {
    int na=ctm_sdl.keymapa+8;
    if (na>INT_MAX/sizeof(struct ctm_keymap)) return -1;
    void *nv=realloc(ctm_sdl.keymapv,sizeof(struct ctm_keymap)*na);
    if (!nv) return -1;
    ctm_sdl.keymapv=nv;
    ctm_sdl.keymapa=na;
  }
  memmove(ctm_sdl.keymapv+p+1,ctm_sdl.keymapv+p,sizeof(struct ctm_keymap)*(ctm_sdl.keymapc-p));
  ctm_sdl.keymapc++;
  ctm_sdl.keymapv[p].keysym=keysym;
  ctm_sdl.keymapv[p].btnid=btnid;
  return 0;
}

static inline uint16_t ctm_sdl_lookup_key(SDLKey keysym) {
  int p=ctm_sdl_keymap_search(keysym);
  if (p<0) return 0;
  return ctm_sdl.keymapv[p].btnid;
}

/* Init/quit input.
 */

int ctm_sdl_init_input() {
  if (ctm_sdl.inputinit) return -1;
  
  if ((ctm_sdl.devid_keyboard=ctm_input_register_device())<1) return -1;
  struct ctm_input_definition *def=ctm_input_get_definition("SDL Keyboard",12);
  if (def) {
    int i; for (i=0;i<def->fldc;i++) {
      if (def->fldv[i].srcscope==SDL_KEYDOWN) {
        if (ctm_sdl_add_keymap(def->fldv[i].srcbtnid,def->fldv[i].dstbtnidhi)<0) return -1;
      }
    }
  } else {
    printf("No definition found for keyboard. Copy the following into your config file (%s)...\nSDL Keyboard\n",ctm_data_path("input.cfg"));
    printf("  %d.%d QUIT\n",SDL_KEYDOWN,SDLK_ESCAPE);
    printf("  %d.%d FULLSCREEN\n",SDL_KEYDOWN,SDLK_f);
    printf("  %d.%d SCREENSHOT\n",SDL_KEYDOWN,SDLK_PRINT);
    printf("  %d.%d UP\n",SDL_KEYDOWN,SDLK_UP);
    printf("  %d.%d DOWN\n",SDL_KEYDOWN,SDLK_DOWN);
    printf("  %d.%d LEFT\n",SDL_KEYDOWN,SDLK_LEFT);
    printf("  %d.%d RIGHT\n",SDL_KEYDOWN,SDLK_RIGHT);
    printf("  %d.%d PRIMARY\n",SDL_KEYDOWN,SDLK_z);
    printf("  %d.%d SECONDARY\n",SDL_KEYDOWN,SDLK_x);
    printf("  %d.%d TERTIARY\n",SDL_KEYDOWN,SDLK_c);
    printf("  %d.%d PAUSE\n",SDL_KEYDOWN,SDLK_RETURN);
    printf("end\n");
    if (ctm_sdl_add_keymap(SDLK_ESCAPE,CTM_BTNID_QUIT)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_f,CTM_BTNID_FULLSCREEN)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_PRINT,CTM_BTNID_SCREENSHOT)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_UP,CTM_BTNID_UP)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_DOWN,CTM_BTNID_DOWN)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_LEFT,CTM_BTNID_LEFT)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_RIGHT,CTM_BTNID_RIGHT)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_z,CTM_BTNID_PRIMARY)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_x,CTM_BTNID_SECONDARY)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_c,CTM_BTNID_TERTIARY)<0) return -1;
    if (ctm_sdl_add_keymap(SDLK_RETURN,CTM_BTNID_PAUSE)<0) return -1;
  }
  
  if (SDL_InitSubSystem(SDL_INIT_JOYSTICK)) return -1;
  ctm_sdl.inputinit=1;
  int joyc=SDL_NumJoysticks();
  if (joyc>0) {
    int i; for (i=0;i<joyc;i++) {
      const char *joyname=SDL_JoystickName(i);
      if (!joyname) joyname="(Joystick)";
      SDL_Joystick *sdljoy=SDL_JoystickOpen(i);
      if (sdljoy) {
        if (ctm_sdl_welcome_joystick(sdljoy,joyname,i)<0) return -1;
      } else {
        // No error if we fail to open a joystick.
        fprintf(stderr,"ctm: Failed to open joystick '%s', ignoring.\n",joyname);
      }
    }
  }
  ctm_sdl.joyccontig=0;
  while ((ctm_sdl.joyccontig<ctm_sdl.joyc)&&(ctm_sdl.joyv[ctm_sdl.joyccontig]->sdljoyid==ctm_sdl.joyccontig)) ctm_sdl.joyccontig++;

  return 0;
}

void ctm_sdl_quit_input() {

  ctm_input_unregister_device(ctm_sdl.devid_keyboard);
  ctm_sdl.devid_keyboard=0;

  if (ctm_sdl.keymapv) free(ctm_sdl.keymapv);
  ctm_sdl.keymapv=0;
  ctm_sdl.keymapc=0;
  ctm_sdl.keymapa=0;
  
  if (!ctm_sdl.inputinit) return;
  if (ctm_sdl.joyv) {
    while (ctm_sdl.joyc-->0) ctm_joystick_del(ctm_sdl.joyv[ctm_sdl.joyc]);
    free(ctm_sdl.joyv);
    ctm_sdl.joyv=0;
  }
  ctm_sdl.joyc=0;
  ctm_sdl.inputinit=0;
  
}

/* Init/quit audio.
 */

static void ctm_sdl_audio_cb(void *userdata,Uint8 *dst,int dstc) {
  void (*cb)(int16_t*,int)=userdata;
  cb((int16_t*)dst,dstc>>1);
  ctm_dump_audio_provide(dst,dstc);
}

int ctm_sdl_init_audio(void (*cb)(int16_t *dst,int dstc)) {
  if (!cb||ctm_sdl.audioinit) return -1;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO)) return -1;

  if (ctm_dump_audio_init(CTM_DUMP_AUDIO_PATH)<0) return -1;

  SDL_AudioSpec spec={
    .freq=44100,
    .format=AUDIO_S16SYS,
    .channels=1,
    .samples=1024,
    .callback=ctm_sdl_audio_cb,
    .userdata=cb,
  };
  if (SDL_OpenAudio(&spec,0)<0) return -1;
  SDL_PauseAudio(0);
  
  ctm_sdl.audioinit=1;
  return 0;
}

void ctm_sdl_quit_audio() {
  if (!ctm_sdl.audioinit) return;
  ctm_dump_audio_quit();
  SDL_PauseAudio(1);
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  ctm_sdl.audioinit=0;
}

/* Swap framebuffer.
 */

int ctm_sdl_swap() {
  SDL_GL_SwapBuffers();
  return 0;
}

/* Joystick events.
 */

static struct ctm_joystick *ctm_sdl_get_joystick(int sdljoyid) {
  if (sdljoyid<0) return 0;
  if (sdljoyid<ctm_sdl.joyccontig) return ctm_sdl.joyv[sdljoyid];
  int lo=ctm_sdl.joyccontig,hi=ctm_sdl.joyc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (sdljoyid<ctm_sdl.joyv[ck]->sdljoyid) hi=ck;
    else if (sdljoyid>ctm_sdl.joyv[ck]->sdljoyid) lo=ck+1;
    else return ctm_sdl.joyv[ck];
  }
  return 0;
}

static int ctm_sdl_event_joybutton(SDL_Event *evt) {
  struct ctm_joystick *joy=ctm_sdl_get_joystick(evt->jbutton.which);
  if (!joy) return 0;
  if (evt->jbutton.button>=joy->btnc) return 0;
  if (evt->jbutton.state) {
    if (joy->btnv[evt->jbutton.button]) return 0;
    joy->btnv[evt->jbutton.button]=1;
  } else {
    if (!joy->btnv[evt->jbutton.button]) return 0;
    joy->btnv[evt->jbutton.button]=0;
  }
  if (!joy->btnmapv[evt->jbutton.button]) return 0;
  return ctm_input_event(joy->devid,joy->btnmapv[evt->jbutton.button],joy->btnv[evt->jbutton.button]);
}

static int ctm_sdl_event_joyaxis(SDL_Event *evt) {
  struct ctm_joystick *joy=ctm_sdl_get_joystick(evt->jaxis.which);
  if (!joy) return 0;
  int axisp=evt->jaxis.axis,mapp=evt->jaxis.axis<<1;
  if ((axisp<0)||(axisp>=joy->axisc)) return 0;
  int value=evt->jaxis.value;
  int prev=joy->axisv[axisp];
  if ((value<=-16384)&&(prev>-16384)) {
    if (ctm_input_event(joy->devid,joy->axismapv[mapp+0],1)<0) return -1;
    if (prev>=16384) {
      if (ctm_input_event(joy->devid,joy->axismapv[mapp+1],0)<0) return -1;
    }
  } else if ((value>=16384)&&(prev<16384)) {
    if (prev<=-16384) {
      if (ctm_input_event(joy->devid,joy->axismapv[mapp+0],0)<0) return -1;
    }
    if (ctm_input_event(joy->devid,joy->axismapv[mapp+1],1)<0) return -1;
  } else if ((value>-16384)&&(value<16384)) {
    if (prev<=-16384) {
      if (ctm_input_event(joy->devid,joy->axismapv[mapp+0],0)<0) return -1;
    } else if (prev>=16384) {
      if (ctm_input_event(joy->devid,joy->axismapv[mapp+1],0)<0) return -1;
    }
  }
  joy->axisv[axisp]=value;
  return 0;
}

static int ctm_sdl_event_joyhat(SDL_Event *evt) {
  struct ctm_joystick *joy=ctm_sdl_get_joystick(evt->jhat.which);
  if (!joy) return 0;
  int hatp=evt->jhat.hat;
  if ((hatp<0)||(hatp>=joy->hatc)) return 0;
  int nx,ox,ny,oy;
  switch (evt->jhat.value&(SDL_HAT_UP|SDL_HAT_DOWN)) {
    case SDL_HAT_UP: ny=-1; break;
    case SDL_HAT_DOWN: ny=1; break;
    default: ny=0; break;
  }
  switch (evt->jhat.value&(SDL_HAT_LEFT|SDL_HAT_RIGHT)) {
    case SDL_HAT_LEFT: nx=-1; break;
    case SDL_HAT_RIGHT: nx=1; break;
    default: nx=0; break;
  }
  switch (joy->hatv[hatp]&(SDL_HAT_UP|SDL_HAT_DOWN)) {
    case SDL_HAT_UP: oy=-1; break;
    case SDL_HAT_DOWN: oy=1; break;
    default: oy=0; break;
  }
  switch (joy->hatv[hatp]&(SDL_HAT_LEFT|SDL_HAT_RIGHT)) {
    case SDL_HAT_LEFT: ox=-1; break;
    case SDL_HAT_RIGHT: ox=1; break;
    default: ox=0; break;
  }
  if (nx!=ox) {
    if (ox<0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_LEFT,0)<0) return -1;
    } else if (ox>0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_RIGHT,0)<0) return -1;
    }
    if (nx<0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_LEFT,1)<0) return -1;
    } else if (nx>0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_RIGHT,1)<0) return -1;
    }
  }
  if (ny!=oy) {
    if (oy<0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_UP,0)<0) return -1;
    } else if (oy>0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_DOWN,0)<0) return -1;
    }
    if (ny<0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_UP,1)<0) return -1;
    } else if (ny>0) {
      if (ctm_input_event(joy->devid,CTM_BTNID_DOWN,1)<0) return -1;
    }
  }
  joy->hatv[hatp]=evt->jhat.value;
  return 0;
}

/* Keyboard events.
 */

static int ctm_sdl_event_key(SDLKey keysym,int value) {

  // ALT keys are special; they modify the mouse wheel.
  if ((keysym==SDLK_LALT)||(keysym==SDLK_RALT)) {
    if (value) ctm_sdl.shift++; else ctm_sdl.shift--;
    return 0;
  }

  // Anything else can be mapped.
  uint16_t btnid=ctm_sdl_lookup_key(keysym);
  if (!btnid) return 0;
  return ctm_input_event(ctm_sdl.devid_keyboard,btnid,value);
}

/* Poll input.
 */
 
int ctm_sdl_update() {
  SDL_Event evt;
  if (ctm_dump_audio_update()<0) return -1;
  while (SDL_PollEvent(&evt)) switch (evt.type) {
  
    case SDL_QUIT: if (ctm_input_event(0,CTM_BTNID_QUIT,1)<0) return -1; break;

    case SDL_KEYDOWN:
    case SDL_KEYUP: if (ctm_sdl_event_key(evt.key.keysym.sym,evt.key.state)<0) return -1; break;

    // Joysticks.
    case SDL_JOYBUTTONDOWN: if (ctm_sdl_event_joybutton(&evt)<0) return -1; break;
    case SDL_JOYBUTTONUP: if (ctm_sdl_event_joybutton(&evt)<0) return -1; break;
    case SDL_JOYAXISMOTION: if (ctm_sdl_event_joyaxis(&evt)<0) return -1; break;
    case SDL_JOYHATMOTION: if (ctm_sdl_event_joyhat(&evt)<0) return -1; break;

    // Mouse (only used by editor).
    case SDL_MOUSEMOTION: {
        ctm_editor_input.ptrx=evt.motion.x;
        ctm_editor_input.ptry=evt.motion.y;
      } break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: switch (evt.button.button) {
        case SDL_BUTTON_LEFT: ctm_editor_input.ptrleft=evt.button.state?1:0; break;
        case SDL_BUTTON_RIGHT: ctm_editor_input.ptrright=evt.button.state?1:0; break;
        case SDL_BUTTON_WHEELUP: if (evt.button.state) { 
            if (ctm_sdl.shift) ctm_editor_input.ptrwheelx=-1; 
            else ctm_editor_input.ptrwheely=-1;
          } break;
        case SDL_BUTTON_WHEELDOWN: if (evt.button.state) {
            if (ctm_sdl.shift) ctm_editor_input.ptrwheelx=1;
            else ctm_editor_input.ptrwheely=1;
          } break;
      } break;

    default: break;
  }
  return 0;
}

/* Fullscreen.
 */

int ctm_sdl_is_fullscreen() {
  return ctm_sdl.fullscreen;
}

int ctm_sdl_set_fullscreen(int fullscreen) {
  if (!ctm_sdl.videoinit) return -1;
  if (fullscreen<0) fullscreen=!ctm_sdl.fullscreen;
  if (fullscreen>0) fullscreen=1;
  if (fullscreen==ctm_sdl.fullscreen) return 0;

  // When we do this under MacOS, it seems like the GL context is lost or disconnected or something.
  // So, Mac users have to launch fullscreen or windowed, and stick with it.
  // Under Linux, this fullscreen-swapping works beautifully.
  // No idea what to expect for Windows.
  #if CTM_ARCH!=CTM_ARCH_macos
    SDL_Surface *nscreen;
    if (fullscreen) {
      nscreen=SDL_SetVideoMode(ctm_sdl.screenw,ctm_sdl.screenh,32,ctm_sdl.videoflags|SDL_FULLSCREEN);
    } else {
      nscreen=SDL_SetVideoMode(ctm_sdl.winw,ctm_sdl.winh,32,ctm_sdl.videoflags&~SDL_FULLSCREEN);
    }
    if (!nscreen) return -1;
    ctm_sdl.videoflags^=SDL_FULLSCREEN;
    ctm_sdl.fullscreen=fullscreen;
    ctm_screenw=ctm_sdl.screen->w;
    ctm_screenh=ctm_sdl.screen->h;
    if (ctm_input_event(0,CTM_BTNID_RESIZE,1)<0) return -1;
  #endif
  
  return 0;
}
