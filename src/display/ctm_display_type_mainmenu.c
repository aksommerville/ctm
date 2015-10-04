#include "ctm.h"
#include "ctm_display.h"
#include "video/ctm_video_internal.h"

// One axis will match, the other may run larger.
#define CTM_MENUW_MIN CTM_RESIZE(512)
#define CTM_MENUH_MIN CTM_RESIZE(384)

#define CTM_MAINMENU_OPTIONS_COLOR 0xa07040ff

/* Object type.
 */

struct ctm_display_mainmenu {
  struct ctm_display hdr;
  
  int phase; // see constants
  
  int playerc,bluec,redc; // straight counts, (playerc==bluec+redc)
  int timelimit; // minutes
  int population; // straight count
  int difficulty; // 1,2,3

  int fingeropacity;
  int fingerdopacity;
};

#define DISPLAY ((struct ctm_display_mainmenu*)display)

/* Delete.
 */

static void _ctm_display_mainmenu_del(struct ctm_display *display) {
}

/* Initialize.
 */

static int _ctm_display_mainmenu_init(struct ctm_display *display) {

  DISPLAY->phase=CTM_MAINMENU_PHASE_PLAYERC;
  
  DISPLAY->playerc=1;
  DISPLAY->bluec=0;
  DISPLAY->redc=1;
  DISPLAY->timelimit=10;
  DISPLAY->population=1000;
  DISPLAY->difficulty=2;

  DISPLAY->fingeropacity=0xff;
  DISPLAY->fingerdopacity=-5;
  
  return 0;
}

/* Resized.
 */

static int _ctm_display_mainmenu_resized(struct ctm_display *display) {
  int wforh=(display->w*CTM_MENUH_MIN)/display->h;
  int hforw=(display->h*CTM_MENUW_MIN)/display->w;
  if (wforh>=CTM_MENUW_MIN) { // Likeliest case (4:3 or wider aspect).
    if (ctm_display_resize_fb(display,wforh,CTM_MENUH_MIN)<0) return -1;
  } else if (hforw>=CTM_MENUH_MIN) { // Really should be true...
    if (ctm_display_resize_fb(display,CTM_MENUW_MIN,hforw)<0) return -1;
  } else { // Whatever, let it scale wildly.
    if (ctm_display_resize_fb(display,CTM_MENUW_MIN,CTM_MENUH_MIN)<0) return -1;
  }
  return 0;
}

/* Title bar.
 */

static int ctm_mainmenu_draw_title(struct ctm_display *display) {
  int midx=display->fbw>>1;
  int midy=CTM_RESIZE(128);
  int quartx=((display->fbw>>1)-CTM_RESIZE(128))>>1; // actually a little less than a quarter width
  if (ctm_draw_texture(midx-CTM_RESIZE(128),midy-CTM_RESIZE(128),CTM_RESIZE(256),CTM_RESIZE(256),ctm_video.texid_title,0)<0) return -1;
  if (ctm_draw_texture(quartx-CTM_RESIZE(64),midy,CTM_RESIZE(128),CTM_RESIZE(128),ctm_video.texid_logo_frostbite,0)<0) return -1;
  if (ctm_draw_texture(display->fbw-quartx-CTM_RESIZE(64),midy,CTM_RESIZE(128),CTM_RESIZE(128),ctm_video.texid_logo_blood,0)<0) return -1;
  return 0;
}

/* Draw sprites for specific phases.
 */

static int ctm_mainmenu_draw_sprites_PLAYERC(struct ctm_display *display,int optx,int opty,int optw,int opth) {
  int i;
  int vtxc=4+2*DISPLAY->playerc;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  for (i=0;i<vtxc;i++) {
    vtxv[i].a=0xff;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }
  for (i=0;i<4;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].a=DISPLAY->fingeropacity;
  }
  for (i=0;i<DISPLAY->bluec;i++) {
    int p=4+i*2;
    vtxv[p].r=vtxv[p+1].r=0x00;
    vtxv[p].g=vtxv[p+1].g=0x00;
    vtxv[p].b=vtxv[p+1].b=0xff;
  }
  for (i=0;i<DISPLAY->redc;i++) {
    int p=4+(DISPLAY->bluec+i)*2;
    vtxv[p].r=vtxv[p+1].r=0x80;
    vtxv[p].g=vtxv[p+1].g=0x00;
    vtxv[p].b=vtxv[p+1].b=0x00;
  }
  for (i=0;i<DISPLAY->playerc;i++) {
    int p=4+i*2;
    vtxv[p].x=vtxv[p+1].x=optx+((i+1)*optw)/(DISPLAY->playerc+1);
    vtxv[p].y=opty+(opth>>1)+7;
    vtxv[p+1].y=vtxv[p].y-CTM_RESIZE(13);
    vtxv[p].tile=0x50;
    vtxv[p+1].tile=0x40;
  }
  vtxv[0].x=optx-CTM_TILESIZE;
  vtxv[0].y=opty+(opth>>1);
  vtxv[0].tile=0x42;
  vtxv[1].x=optx+optw+CTM_TILESIZE;
  vtxv[1].y=opty+(opth>>1);
  vtxv[1].tile=0x41;
  vtxv[2].x=optx+(optw>>1);
  vtxv[2].y=opty-CTM_TILESIZE;
  vtxv[2].tile=0x51;
  vtxv[3].x=optx+(optw>>1);
  vtxv[3].y=opty+opth+CTM_TILESIZE;
  vtxv[3].tile=0x52;
  return 0;
}

static int ctm_mainmenu_draw_sprites_TIMELIMIT(struct ctm_display *display,int optx,int opty,int optw,int opth) {
  int i;
  int vtxc=4;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  for (i=0;i<vtxc;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }
  vtxv[0].x=optx-CTM_TILESIZE;
  vtxv[0].y=opty+(opth>>1);
  vtxv[0].tile=0x42;
  vtxv[0].a=DISPLAY->fingeropacity;
  vtxv[1].x=optx+optw+CTM_TILESIZE;
  vtxv[1].y=opty+(opth>>1);
  vtxv[1].tile=0x41;
  vtxv[1].a=DISPLAY->fingeropacity;
  vtxv[2].x=optx+(optw>>1);
  vtxv[2].y=opty+(opth>>1);
  switch (DISPLAY->timelimit) {
    case 5: vtxv[2].tile=0x09; break;
    case 10: vtxv[2].tile=0x0a; break;
    case 20: vtxv[2].tile=0x0b; break;
    case 30: vtxv[2].tile=0x0c; break;
  }
  vtxv[2].a=0xff;
  vtxv[3].x=vtxv[2].x;
  vtxv[3].y=opty-CTM_TILESIZE;
  vtxv[3].tile=0x08;
  vtxv[3].a=0xff;
  return 0;
}

static int ctm_mainmenu_draw_sprites_POPULATION(struct ctm_display *display,int optx,int opty,int optw,int opth) {
  int i;
  int vtxc=4;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  for (i=0;i<vtxc;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }
  vtxv[0].x=optx-CTM_TILESIZE;
  vtxv[0].y=opty+(opth>>1);
  vtxv[0].tile=0x42;
  vtxv[0].a=DISPLAY->fingeropacity;
  vtxv[1].x=optx+optw+CTM_TILESIZE;
  vtxv[1].y=opty+(opth>>1);
  vtxv[1].tile=0x41;
  vtxv[1].a=DISPLAY->fingeropacity;
  vtxv[2].x=optx+(optw>>1);
  vtxv[2].y=opty+(opth>>1);
  switch (DISPLAY->population) {
    case 500: vtxv[2].tile=0x19; break;
    case 1000: vtxv[2].tile=0x1a; break;
    case 2000: vtxv[2].tile=0x1b; break;
  }
  vtxv[2].a=0xff;
  vtxv[3].x=vtxv[2].x;
  vtxv[3].y=opty-CTM_TILESIZE;
  vtxv[3].tile=0x18;
  vtxv[3].a=0xff;
  return 0;
}

static int ctm_mainmenu_draw_sprites_DIFFICULTY(struct ctm_display *display,int optx,int opty,int optw,int opth) {
  int i;
  int vtxc=5;
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(vtxc);
  if (!vtxv) return -1;
  for (i=0;i<vtxc;i++) {
    vtxv[i].r=vtxv[i].g=vtxv[i].b=0x80;
    vtxv[i].ta=0;
    vtxv[i].flop=0;
  }
  vtxv[0].x=optx-CTM_TILESIZE;
  vtxv[0].y=opty+(opth>>1);
  vtxv[0].tile=0x42;
  vtxv[0].a=DISPLAY->fingeropacity;
  vtxv[1].x=optx+optw+CTM_TILESIZE;
  vtxv[1].y=opty+(opth>>1);
  vtxv[1].tile=0x41;
  vtxv[1].a=DISPLAY->fingeropacity;
  vtxv[2].x=optx+(optw>>1)-(CTM_TILESIZE>>1)-2;
  vtxv[2].y=opty+(opth>>1);
  vtxv[3].x=vtxv[2].x+CTM_TILESIZE+4;
  vtxv[3].y=vtxv[2].y;
  switch (DISPLAY->difficulty) {
    case 1: vtxv[2].tile=0x29; vtxv[3].tile=0x39; break;
    case 2: vtxv[2].tile=0x2a; vtxv[3].tile=0x3a; break;
    case 3: vtxv[2].tile=0x2b; vtxv[3].tile=0x3b; break;
    case 4: vtxv[2].tile=0x2c; vtxv[3].tile=0x3c; break;
  }
  vtxv[2].a=vtxv[3].a=0xff;
  vtxv[4].x=optx+(optw>>1);
  vtxv[4].y=opty-CTM_TILESIZE;
  vtxv[4].tile=0x28;
  vtxv[4].a=0xff;
  return 0;
}

/* Clear background.
 */

static int ctm_mainmenu_clear_background(struct ctm_display *display) {
  if (ctm_video.texid_introbg) {

    // Bind to the larger axis and let it overflow the other.
    // Source texture is square.
    int x,y,w,h;
    if (display->fbw>=display->fbh) {
      w=h=display->fbw;
      x=0;
      y=(display->fbh>>1)-(display->fbw>>1);
    } else {
      w=h=display->fbh;
      y=0;
      x=(display->fbw>>1)-(display->fbh>>1);
    }
      
    if (ctm_draw_texture(x,y,w,h,ctm_video.texid_introbg,0)<0) return -1;
    
  } else {
    glClearColor(0.3,0.1,0.0,1.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  return 0;
}

/* Draw framebuffer, main.
 */

static int _ctm_display_mainmenu_draw_fb(struct ctm_display *display) {

  /* Update animation. */
  DISPLAY->fingeropacity+=DISPLAY->fingerdopacity;
  if (DISPLAY->fingeropacity<0x40) {
    DISPLAY->fingeropacity=0x40;
    if (DISPLAY->fingerdopacity<0) DISPLAY->fingerdopacity*=-1;
  } else if (DISPLAY->fingeropacity>0xff) {
    DISPLAY->fingeropacity=0xff;
    if (DISPLAY->fingerdopacity>0) DISPLAY->fingerdopacity*=-1;
  }

  /* Clear screen and draw common constant elements. */
  if (ctm_mainmenu_clear_background(display)<0) return -1;
  if (ctm_mainmenu_draw_title(display)<0) return -1;
  int optw=CTM_RESIZE(400),opth=CTM_RESIZE(50);
  int optx=(display->fbw>>1)-(optw>>1);
  int opty=CTM_RESIZE(256)+((display->fbh-CTM_RESIZE(256))>>1)-(opth>>1);
  if (ctm_draw_rect(optx,opty,optw,opth,CTM_MAINMENU_OPTIONS_COLOR)<0) return -1;

  /* Place sprites for each phase. */
  if (ctm_video_begin_sprites()<0) return -1;
  switch (DISPLAY->phase) {
    case CTM_MAINMENU_PHASE_PLAYERC: if (ctm_mainmenu_draw_sprites_PLAYERC(display,optx,opty,optw,opth)<0) return -1; break;
    case CTM_MAINMENU_PHASE_TIMELIMIT: if (ctm_mainmenu_draw_sprites_TIMELIMIT(display,optx,opty,optw,opth)<0) return -1; break;
    case CTM_MAINMENU_PHASE_POPULATION: if (ctm_mainmenu_draw_sprites_POPULATION(display,optx,opty,optw,opth)<0) return -1; break;
    case CTM_MAINMENU_PHASE_DIFFICULTY: if (ctm_mainmenu_draw_sprites_DIFFICULTY(display,optx,opty,optw,opth)<0) return -1; break;
  }
  if (ctm_video_end_sprites(ctm_video.texid_uisprites)<0) return -1;
  
  return 0;
}

/* Type definition.
 */

const struct ctm_display_type ctm_display_type_mainmenu={
  .name="mainmenu",
  .objlen=sizeof(struct ctm_display_mainmenu),
  .del=_ctm_display_mainmenu_del,
  .init=_ctm_display_mainmenu_init,
  .resized=_ctm_display_mainmenu_resized,
  .draw_fb=_ctm_display_mainmenu_draw_fb,
};

/* Adjust phase.
 */
 
int ctm_display_mainmenu_get_phase(const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  return DISPLAY->phase;
}

int ctm_display_mainmenu_advance(struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  switch (DISPLAY->phase) {
    case CTM_MAINMENU_PHASE_PLAYERC: DISPLAY->phase=CTM_MAINMENU_PHASE_TIMELIMIT; return 1;
    case CTM_MAINMENU_PHASE_TIMELIMIT:
    case CTM_MAINMENU_PHASE_POPULATION: if (DISPLAY->bluec&&DISPLAY->redc) { // competitive
        return 0;
      } else { // single-party
        DISPLAY->phase=CTM_MAINMENU_PHASE_DIFFICULTY;
        return 1;
      }
    case CTM_MAINMENU_PHASE_DIFFICULTY: return 0;
  }
  return -1;
}

int ctm_display_mainmenu_retreat(struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  switch (DISPLAY->phase) {
    case CTM_MAINMENU_PHASE_PLAYERC: return 0;
    case CTM_MAINMENU_PHASE_TIMELIMIT: DISPLAY->phase=CTM_MAINMENU_PHASE_PLAYERC; return 1;
    case CTM_MAINMENU_PHASE_POPULATION: DISPLAY->phase=CTM_MAINMENU_PHASE_TIMELIMIT; return 1;
    case CTM_MAINMENU_PHASE_DIFFICULTY: DISPLAY->phase=CTM_MAINMENU_PHASE_TIMELIMIT; return 1;
  }
  return -1;
}

int ctm_display_mainmenu_reset(struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  DISPLAY->phase=CTM_MAINMENU_PHASE_PLAYERC;
  DISPLAY->playerc=1;
  DISPLAY->bluec=0;
  DISPLAY->redc=1;
  DISPLAY->timelimit=10;
  DISPLAY->population=1000;
  DISPLAY->difficulty=2;
  return 0;
}

/* User configuration fields.
 */

int ctm_display_mainmenu_get_playerc(int *bluec,int *redc,const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) { if (bluec) *bluec=0; if (redc) *redc=1; return 1; }
  if (bluec) *bluec=DISPLAY->bluec;
  if (redc) *redc=DISPLAY->redc;
  return DISPLAY->playerc;
}

int ctm_display_mainmenu_get_timelimit(const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return 10;
  return DISPLAY->timelimit;
}

int ctm_display_mainmenu_get_population(const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return 1000;
  return DISPLAY->population;
}

int ctm_display_mainmenu_get_difficulty(const struct ctm_display *display) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return 2;
  return DISPLAY->difficulty;
}

int ctm_display_mainmenu_set_playerc(struct ctm_display *display,int playerc,int bluec,int redc) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  if (playerc<1) playerc=1;
  else if (playerc>4) playerc=4;
  if (bluec+redc!=playerc) {
    if ((bluec&&redc)&&(playerc>1)) {
      bluec=playerc>>1;
      redc=playerc-bluec;
    } else if (bluec) bluec=playerc;
    else if (redc) redc=playerc;
  }
  DISPLAY->playerc=playerc;
  DISPLAY->bluec=bluec;
  DISPLAY->redc=redc;
  return 0;
}

int ctm_display_mainmenu_set_timelimit(struct ctm_display *display,int timelimit) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  if (timelimit<=5) DISPLAY->timelimit=5;
  else if (timelimit<=10) DISPLAY->timelimit=10;
  else if (timelimit<=20) DISPLAY->timelimit=20;
  else DISPLAY->timelimit=30;
  return 0;
}

int ctm_display_mainmenu_set_population(struct ctm_display *display,int population) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  if (population<=500) DISPLAY->population=500;
  else if (population<=1000) DISPLAY->population=1000;
  else DISPLAY->population=2000;
  return 0;
}

int ctm_display_mainmenu_set_difficulty(struct ctm_display *display,int difficulty) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  if (difficulty<1) DISPLAY->difficulty=1;
  else if (difficulty>4) DISPLAY->difficulty=4;
  else DISPLAY->difficulty=difficulty;
  return 0;
}

/* Adjust current value.
 */

int ctm_display_mainmenu_adjust(struct ctm_display *display,int dx,int dy) {
  if (!display||(display->type!=&ctm_display_type_mainmenu)) return -1;
  switch (DISPLAY->phase) {
  
    case CTM_MAINMENU_PHASE_PLAYERC: if (dy) {
        DISPLAY->playerc+=dy;
        if (DISPLAY->playerc<1) DISPLAY->playerc=4;
        else if (DISPLAY->playerc>4) DISPLAY->playerc=1;
        if (DISPLAY->bluec>DISPLAY->playerc) { DISPLAY->bluec=DISPLAY->playerc; DISPLAY->redc=0; }
        else DISPLAY->redc=DISPLAY->playerc-DISPLAY->bluec;
      } else {
        DISPLAY->bluec+=dx;
        if (DISPLAY->bluec<0) { DISPLAY->bluec=DISPLAY->playerc; DISPLAY->redc=0; }
        else if (DISPLAY->bluec>DISPLAY->playerc) { DISPLAY->bluec=0; DISPLAY->redc=DISPLAY->playerc; }
        else DISPLAY->redc=DISPLAY->playerc-DISPLAY->bluec;
      } break;
      
    case CTM_MAINMENU_PHASE_TIMELIMIT: if (dx<0) switch (DISPLAY->timelimit) {
        case 5: DISPLAY->timelimit=30; break;
        case 10: DISPLAY->timelimit=5; break;
        case 20: DISPLAY->timelimit=10; break;
        case 30: DISPLAY->timelimit=20; break;
      } else if (dx>0) switch (DISPLAY->timelimit) {
        case 5: DISPLAY->timelimit=10; break;
        case 10: DISPLAY->timelimit=20; break;
        case 20: DISPLAY->timelimit=30; break;
        case 30: DISPLAY->timelimit=5; break;
      } break;
      
    case CTM_MAINMENU_PHASE_POPULATION: if (dx<0) switch (DISPLAY->population) {
        case 500: DISPLAY->population=2000; break;
        case 1000: DISPLAY->population=500; break;
        case 2000: DISPLAY->population=1000; break;
      } else if (dx>0) switch (DISPLAY->population) {
        case 500: DISPLAY->population=1000; break;
        case 1000: DISPLAY->population=2000; break;
        case 2000: DISPLAY->population=500; break;
      } break;
      
    case CTM_MAINMENU_PHASE_DIFFICULTY: if (dx<0) switch (DISPLAY->difficulty) {
        case 1: DISPLAY->difficulty=4; break;
        case 2: DISPLAY->difficulty=1; break;
        case 3: DISPLAY->difficulty=2; break;
        case 4: DISPLAY->difficulty=3; break;
      } else if (dx>0) switch (DISPLAY->difficulty) {
        case 1: DISPLAY->difficulty=2; break;
        case 2: DISPLAY->difficulty=3; break;
        case 3: DISPLAY->difficulty=4; break;
        case 4: DISPLAY->difficulty=1; break;
      } break;
      
  }
  return 0;
}
