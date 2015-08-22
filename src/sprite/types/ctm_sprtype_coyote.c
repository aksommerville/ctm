#include "ctm.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "game/ctm_geography.h"
#include "game/ctm_grid.h"
#include "audio/ctm_audio.h"

#define CTM_COYOTE_TREASURE CTM_ITEM_FLAMES,1

struct ctm_sprite_coyote {
  struct ctm_sprite hdr;
  int hp;
  int invincible;
  int dx,dy;
  int animclock;
  int animframe;
  int waitclock;
};

#define SPR ((struct ctm_sprite_coyote*)spr)

#define CTM_COYOTE_COLOR_1 0x108030
#define CTM_COYOTE_COLOR_2 0xffc000
#define CTM_COYOTE_COLOR_3 0xff2040

#define CTM_COYOTE_COLLISION_RADIUS ((CTM_TILESIZE*6)/16)
#define CTM_COYOTE_WALK_SPEED ((CTM_TILESIZE*1)/16)

/* Delete.
 */

static void _ctm_coyote_del(struct ctm_sprite *spr) {
}

/* Collision test.
 */

static int ctm_coyote_collision(struct ctm_sprite *spr) {
  int cola=(spr->x-CTM_COYOTE_COLLISION_RADIUS)/CTM_TILESIZE;
  int colz=(spr->x+CTM_COYOTE_COLLISION_RADIUS)/CTM_TILESIZE;
  int rowa=(spr->y-CTM_COYOTE_COLLISION_RADIUS)/CTM_TILESIZE;
  int rowz=(spr->y+CTM_COYOTE_COLLISION_RADIUS)/CTM_TILESIZE;
  int row; for (row=rowa;row<=rowz;row++) {
    int col; for (col=cola;col<=colz;col++) {
      uint32_t prop=ctm_grid_tileprop_for_cell(col,row,spr->interior);
      if (prop&(CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS|CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE)) return 1;
    }
  }
  return 0;
}

/* Update.
 */

static int _ctm_coyote_update(struct ctm_sprite *spr) {
  if (!ctm_sprite_near_player(spr)) return 0;
  
  if (SPR->invincible>0) {
    if (--(SPR->invincible)) spr->tintrgba=(SPR->invincible*255)/60;
    else spr->tintrgba=0;
  }

  if (++(SPR->animclock)>=8) {
    SPR->animclock=0;
    if (++(SPR->animframe)>=4) SPR->animframe=0;
    switch (SPR->animframe) {
      case 0: spr->tile=0x59; break;
      case 1: spr->tile=0x5a; break;
      case 2: spr->tile=0x59; break;
      case 3: spr->tile=0x5b; break;
    }
  }

  if (SPR->dx||SPR->dy) {
    spr->x+=SPR->dx*CTM_COYOTE_WALK_SPEED;
    spr->y+=SPR->dy*CTM_COYOTE_WALK_SPEED;
    if (ctm_coyote_collision(spr)) {
      spr->x-=SPR->dx*CTM_COYOTE_WALK_SPEED;
      spr->y-=SPR->dy*CTM_COYOTE_WALK_SPEED;
      SPR->dx=SPR->dy=0;
    } else if (SPR->dx&&!((spr->x-(CTM_TILESIZE>>1))%CTM_TILESIZE)) {
      SPR->dx=SPR->dy=0;
    } else if (SPR->dy&&!((spr->y-(CTM_TILESIZE>>1))%CTM_TILESIZE)) {
      SPR->dx=SPR->dy=0;
    }
  } else switch (rand()&3) {
    case 0: SPR->dx=-1; spr->flop=0; break;
    case 1: SPR->dx=1; spr->flop=1; break;
    case 2: SPR->dy=-1; break;
    case 3: SPR->dy=1; break;
  }

  return 0;
}

/* Split.
 */

static int ctm_coyote_split(struct ctm_sprite *spr) {
  struct ctm_sprite *spr2=ctm_sprite_alloc(&ctm_sprtype_coyote);
  if (!spr2) return -1;
  if (ctm_sprite_add_group(spr2,&ctm_group_keepalive)<0) { ctm_sprite_del(spr2); return -1; }
  ctm_sprite_del(spr2);
  struct ctm_sprite_coyote *SPR2=(struct ctm_sprite_coyote*)spr2;
  if (ctm_sprite_add_group(spr2,&ctm_group_update)<0) return -1;
  if (ctm_sprite_add_group(spr2,&ctm_group_hazard)<0) return -1;
  if (ctm_sprite_add_group(spr2,&ctm_group_fragile)<0) return -1;
  spr2->x=spr->x;
  spr2->y=spr->y;
  if (spr2->interior=spr->interior) {
    if (ctm_sprite_add_group(spr2,&ctm_group_interior)<0) return -1;
  } else {
    if (ctm_sprite_add_group(spr2,&ctm_group_exterior)<0) return -1;
  }
  spr2->primaryrgb=spr->primaryrgb;
  SPR2->dx=-SPR->dx;
  SPR2->dy=-SPR->dy;
  if (SPR2->dx<0) spr2->flop=0;
  else if (SPR2->dx>0) spr2->flop=1;
  SPR2->invincible=SPR->invincible;
  SPR2->hp=SPR->hp;
  return 0;
}

/* Hurt.
 */

static int _ctm_coyote_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (SPR->invincible) return 0;
  if (SPR->hp-->1) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_HURT,0xff);
    SPR->invincible=60;
  } else if (SPR->hp==0) {

    // Are we actually dying this time, or splitting atwain?
    if (spr->primaryrgb==CTM_COYOTE_COLOR_1) {
      ctm_audio_effect(CTM_AUDIO_EFFECTID_MONSTER_KILLED,0xff);
      spr->primaryrgb=CTM_COYOTE_COLOR_2;
      SPR->hp=2;
      SPR->invincible=60;
      if (ctm_coyote_split(spr)<0) return -1;
      if (ctm_coyote_split(spr)<0) return -1;
      return 0;
    } else if (spr->primaryrgb==CTM_COYOTE_COLOR_2) {
      ctm_audio_effect(CTM_AUDIO_EFFECTID_MONSTER_KILLED,0xff);
      spr->primaryrgb=CTM_COYOTE_COLOR_3;
      SPR->hp=1;
      SPR->invincible=60;
      if (ctm_coyote_split(spr)<0) return -1;
      if (ctm_coyote_split(spr)<0) return -1;
      return 0;
    }

    if (!assailant||(assailant->type!=&ctm_sprtype_player)) {
      int havedistance=INT_MAX;
      int i; for (i=0;i<ctm_group_player.sprc;i++) {
        struct ctm_sprite *player=ctm_group_player.sprv[i];
        if (player->interior!=spr->interior) continue;
        int dx=player->x-spr->x;
        int dy=player->y-spr->y;
        int distance=((dx<0)?-dx:dx)+((dy<0)?-dy:dy);
        if (distance<havedistance) { havedistance=distance; assailant=player; }
      }
    }

    // Look for other coyotes. If we're the last one, do the fortune-and-glory stuff.
    int fortune_and_glory=1,i;
    for (i=0;i<ctm_group_hazard.sprc;i++) {
      struct ctm_sprite *qspr=ctm_group_hazard.sprv[i];
      if (qspr==spr) continue;
      if (qspr->type!=&ctm_sprtype_coyote) continue;
      fortune_and_glory=0;
      break;
    }

    if (fortune_and_glory) {
      // Create treasure and record the kill.
      ctm_audio_effect(CTM_AUDIO_EFFECTID_BOSS_KILLED,0xff);
      if (!ctm_treasure_new(assailant,CTM_COYOTE_TREASURE)) return -1;
      if (assailant&&(assailant->type==&ctm_sprtype_player)) {
        ((struct ctm_sprite_player*)assailant)->bosskills++;
      }
    } else {
      ctm_audio_effect(CTM_AUDIO_EFFECTID_MONSTER_KILLED,0xff);
    }

    // The usual kill-a-monster stuff...
    if (ctm_sprite_create_ghost(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_gore(spr->x,spr->y,spr->interior)<0) return -1;
    if (ctm_sprite_create_fireworks(spr->x,spr->y,spr->interior)<0) return -1;
    return ctm_sprite_kill_later(spr);
  } else return ctm_sprite_kill_later(spr);
  return 0;
}

/* Initialize.
 */

static int _ctm_coyote_init(struct ctm_sprite *spr) {
  SPR->hp=3;
  spr->primaryrgb=CTM_COYOTE_COLOR_1;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_coyote={
  .name="coyote",
  .objlen=sizeof(struct ctm_sprite_coyote),
  .soul=1,
  .guts=0, // eww gross
  .del=_ctm_coyote_del,
  .init=_ctm_coyote_init,
  .update=_ctm_coyote_update,
  .hurt=_ctm_coyote_hurt,
};
