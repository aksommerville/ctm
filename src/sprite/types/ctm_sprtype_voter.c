#include "ctm.h"
#include "ctm_sprtype_voter.h"
#include "ctm_sprtype_player.h"
#include "video/ctm_video.h"
#include "game/ctm_geography.h"

// Voters of the red and blue parties tend to vote that way.
#define CTM_INITIAL_VOTER_BIAS 20
#define CTM_INITIAL_VOTER_RANDOM 10
#define CTM_INITIAL_VOTER_BELL 30

// When a candidate is nearby, his glorious presence inspires devotion in all voters present.
// But not very much.
#define CTM_VOTER_HEY_LOOK_THATS_A_CANDIDATE_DISTANCE (CTM_TILESIZE*5)

#define CTM_VOTER_NUMB_TIME 60

#define SPR ((struct ctm_sprite_voter*)spr)

#define CTM_VOTER_HEAD_Y_ADJUST ((CTM_TILESIZE*12)/16)

/* Delete and initialize.
 */

static void _ctm_voter_del(struct ctm_sprite *spr) {
}

static int _ctm_voter_init(struct ctm_sprite *spr) {
  SPR->bodytile=0x20;
  SPR->trouserstile=0x40;
  SPR->shirttile=0x22;
  SPR->headtile=0x00;
  SPR->hairtile=0x10;
  SPR->skincolor=(struct ctm_rgb){0x80,0x80,0x80};
  SPR->haircolor=(struct ctm_rgb){0x80,0x80,0x80};
  SPR->shirtcolor=(struct ctm_rgb){0x80,0x80,0x80};
  SPR->trouserscolor=(struct ctm_rgb){0x80,0x80,0x80};
  return 0;
}

/* Draw.
 */

static void _ctm_voter_vtx(struct ctm_vertex_sprite *vtx,int x,int y,uint8_t tile,struct ctm_rgb rgb) {
  vtx->x=x;
  vtx->y=y;
  vtx->tile=tile;
  vtx->r=rgb.r;
  vtx->g=rgb.g;
  vtx->b=rgb.b;
  vtx->a=0xff;
  vtx->tr=vtx->tg=vtx->tb=vtx->ta=0x00;
  vtx->flop=0;
}

static int _ctm_voter_draw(struct ctm_sprite *spr,int addx,int addy) {
  struct ctm_vertex_sprite *vtxv=ctm_add_sprites(5);
  if (!vtxv) return -1;
  
  int x=spr->x+addx,y=spr->y+addy;
  _ctm_voter_vtx(vtxv+0,x,y,SPR->bodytile,SPR->skincolor);
  _ctm_voter_vtx(vtxv+1,x,y,SPR->trouserstile,SPR->trouserscolor);
  _ctm_voter_vtx(vtxv+2,x,y,SPR->shirttile,SPR->shirtcolor);
  _ctm_voter_vtx(vtxv+3,x,y-CTM_VOTER_HEAD_Y_ADJUST,SPR->headtile,SPR->skincolor);
  _ctm_voter_vtx(vtxv+4,x,y-CTM_VOTER_HEAD_Y_ADJUST,SPR->hairtile,SPR->haircolor);

  uint8_t a=0x00,r,g,b;
  if (SPR->numb<0) {
    a=0x40+(-SPR->numb*0x80)/CTM_VOTER_NUMB_TIME;
    r=0x00; g=0x00; b=0xff;
  } else if (SPR->numb>0) {
    a=0x40+(SPR->numb*0x80)/CTM_VOTER_NUMB_TIME;
    r=0xff; g=0x00; b=0x00;
  }
  if (a) { // Apply tint to body and head.
    vtxv[0].tr=vtxv[3].tr=r;
    vtxv[0].tg=vtxv[3].tg=g;
    vtxv[0].tb=vtxv[3].tb=b;
    vtxv[0].ta=vtxv[3].ta=a;
  }
  
  return 0;
}

/* Update.
 */
 
static int _ctm_voter_update(struct ctm_sprite *spr) {
  if (SPR->numb<0) SPR->numb++;
  else if (SPR->numb>0) SPR->numb--;
  else return ctm_sprite_remove_group(spr,&ctm_group_update);
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_voter={

  .name="voter",
  .objlen=sizeof(struct ctm_sprite_voter),

  .soul=1,
  .guts=1,

  .del=_ctm_voter_del,
  .init=_ctm_voter_init,

  .draw=_ctm_voter_draw,

  .update=_ctm_voter_update,

};

/* Setup.
 */

static const struct ctm_rgb ctm_skin_euro_min={0xa9,0x89,0x5f};
static const struct ctm_rgb ctm_skin_euro_max={0xe3,0xca,0xa9};

static const struct ctm_rgb ctm_skin_afro_min={};
static const struct ctm_rgb ctm_skin_afro_max={};
static const struct ctm_rgb ctm_skin_asio_min={};
static const struct ctm_rgb ctm_skin_asio_max={};
static const struct ctm_rgb ctm_skin_tonto_min={};
static const struct ctm_rgb ctm_skin_tonto_max={};

static inline uint8_t ctm_randomcolor(uint8_t lo,uint8_t hi1,uint8_t hi2) {
  if (hi2<hi1) hi1=hi2;
  if (lo==hi1) return lo;
  if (lo>hi1) { uint8_t tmp=lo; lo=hi1; hi1=tmp; }
  return lo+rand()%(hi1-lo+1);
}

static inline struct ctm_rgb ctm_randombrown(uint8_t lo,uint8_t hi,uint8_t minsaturation) {
  int range=hi-lo+1;
  uint8_t a,b,c,tmp;
  while (1) {
    a=lo+rand()%range;
    b=lo+rand()%range;
    c=lo+rand()%range;
    if (a<b) { tmp=a; a=b; b=tmp; }
    if (a<c) { tmp=a; a=c; c=tmp; }
    if (b<c) { tmp=b; b=c; c=tmp; }
    if (minsaturation<=a-c) break;
  }
  return (struct ctm_rgb){a,b,c};
}

int ctm_voter_setup(struct ctm_sprite *spr,int race,int party) {
  if (!spr||(spr->type!=&ctm_sprtype_voter)) return -1;

  /* Set skin and hair color based on race. Choose random race if unset. */
  _race_again_: switch (race) {
    case CTM_RACE_EURO: {
        SPR->skincolor.r=ctm_randomcolor(ctm_skin_euro_min.r,ctm_skin_euro_max.r,0xff);
        SPR->skincolor.g=ctm_randomcolor(ctm_skin_euro_min.g,ctm_skin_euro_max.g,(ctm_skin_euro_min.r+SPR->skincolor.r)>>1);
        SPR->skincolor.b=ctm_randomcolor(ctm_skin_euro_min.b,ctm_skin_euro_max.b,(ctm_skin_euro_min.g+SPR->skincolor.g)>>1);
        SPR->haircolor=ctm_randombrown(0x40,0x60,0x00);
      } break;
    case CTM_RACE_AFRO: {
        SPR->skincolor=ctm_randombrown(0x40,0x80,0x20);
        SPR->haircolor=ctm_randombrown(0x00,0x40,0x00);
      } break;
    case CTM_RACE_ASIO: {
        SPR->skincolor=ctm_randombrown(0x60,0xa0,0x30);
        SPR->haircolor=ctm_randombrown(0x00,0x30,0x00);
      } break;
    default: switch (rand()%10) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4: race=CTM_RACE_EURO; break;
      case 5:
      case 6:
      case 7: race=CTM_RACE_AFRO; break;
      case 8:
      case 9: race=CTM_RACE_ASIO; break;
    } goto _race_again_;
  }
  SPR->race=race;

  /* Set shirt color based on party. Random party if unset.
   * Trousers same color, slightly darker.
   */
  _party_again_: switch (party) {
    case 0:               SPR->shirtcolor=(struct ctm_rgb){0x70,0x70,0x70}; SPR->trouserscolor=(struct ctm_rgb){0x40,0x80,0xa0}; break;
    case CTM_PARTY_BLACK: SPR->shirtcolor=(struct ctm_rgb){0x40,0x40,0x40}; break;
    case CTM_PARTY_WHITE: SPR->shirtcolor=(struct ctm_rgb){0xd0,0xd0,0xd0}; break;
    case CTM_PARTY_RED:   SPR->shirtcolor=(struct ctm_rgb){0xff,0x00,0x00}; break;
    case CTM_PARTY_GREEN: SPR->shirtcolor=(struct ctm_rgb){0x00,0x80,0x00}; break;
    case CTM_PARTY_BLUE:  SPR->shirtcolor=(struct ctm_rgb){0x40,0x40,0xff}; break;
    case CTM_PARTY_YELLOW:SPR->shirtcolor=(struct ctm_rgb){0xff,0xff,0x00}; break;
    default: party=1+rand()%6; goto _party_again_;
  }
  if (SPR->party=party) {
    SPR->trouserscolor.r=(SPR->shirtcolor.r*80)/100;
    SPR->trouserscolor.g=(SPR->shirtcolor.g*80)/100;
    SPR->trouserscolor.b=(SPR->shirtcolor.b*80)/100;
  }
  
  /* Choose tiles. 4 heads, 4 hairs, 2 bodies, 6 shirts, 4 trousers. */
  SPR->bodytile=0x20+rand()%2;
  SPR->trouserstile=0x40+rand()%4;
  switch (rand()%6) {
    case 0: SPR->shirttile=0x22; break;
    case 1: SPR->shirttile=0x23; break;
    case 2: SPR->shirttile=0x30; break;
    case 3: SPR->shirttile=0x31; break;
    case 4: SPR->shirttile=0x32; break;
    case 5: SPR->shirttile=0x33; break;
  }
  SPR->headtile=rand()%4;
  SPR->hairtile=0x10+rand()%4;

  /* Start at zero or one of the bias points, then bell out randomly. */
  if (SPR->party==CTM_PARTY_BLUE) SPR->decision=-CTM_INITIAL_VOTER_BIAS;
  else if (SPR->party==CTM_PARTY_RED) SPR->decision=CTM_INITIAL_VOTER_BIAS;
  else SPR->decision=0;
  int offset=rand()%CTM_INITIAL_VOTER_BELL;
  offset*=offset*offset; // Parabolate!
  const int limit=CTM_INITIAL_VOTER_BELL*CTM_INITIAL_VOTER_BELL*CTM_INITIAL_VOTER_BELL;
  offset=(offset*CTM_INITIAL_VOTER_BELL)/limit; // Curved scale up + linear scale down == bell distribution.
  if (rand()&1) SPR->decision+=offset; else SPR->decision-=offset;

  return 0;
}

/* Idle decision adjustment.
 */
 
int ctm_voter_consider(struct ctm_sprite *spr) {
  if (!spr||(spr->type!=&ctm_sprtype_voter)) return -1;
  int i;

  // If a player is nearby, earn one point.
  int proximity_bonus=0;
  for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *player=ctm_group_player.sprv[i];
    if (player->type!=&ctm_sprtype_player) continue;
    if (player->interior!=spr->interior) continue;
    int dx=spr->x-player->x; if (dx<0) dx=-dx;
    int dy=spr->y-player->y; if (dy<0) dy=-dy;
    if (dx+dy<=CTM_VOTER_HEY_LOOK_THATS_A_CANDIDATE_DISTANCE) {
      switch (((struct ctm_sprite_player*)player)->party) {
        case CTM_PARTY_BLUE: proximity_bonus--; break;
        case CTM_PARTY_RED: proximity_bonus++; break;
      }
    }
  }
  if (proximity_bonus) {
    int ndecision=SPR->decision+proximity_bonus;
    if (ndecision<-128) SPR->decision=-128;
    else if (ndecision>127) SPR->decision=127;
    else SPR->decision=ndecision;
    return 0;
  }
  
  return 0;
}

/* Modify decision.
 */

int ctm_voter_favor(struct ctm_sprite *spr,int party,int delta) {
  if (!spr||(spr->type!=&ctm_sprtype_voter)) return -1;
  if (SPR->numb) return 0;
  switch (party) {
    case CTM_PARTY_BLUE: delta=-delta; break;
    case CTM_PARTY_RED: break;
    default: if (SPR->decision>0) {
        delta=-delta;
        if (delta<-SPR->decision) delta=-SPR->decision;
      } else {
        if (delta>-SPR->decision) delta=-SPR->decision;
      } break;
  }
  if (delta>0) {
    if (SPR->decision>=127-delta) SPR->decision=127;
    else SPR->decision+=delta;
    SPR->numb=CTM_VOTER_NUMB_TIME;
  } else {
    if (SPR->decision<=-128-delta) SPR->decision=-128;
    else SPR->decision+=delta;
    SPR->numb=-CTM_VOTER_NUMB_TIME;
  }
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) return -1;
  return 1;
}
