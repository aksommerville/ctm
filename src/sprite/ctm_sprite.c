#include "ctm.h"
#include "ctm_sprite.h"
#include "game/ctm_geography.h"
#include "types/ctm_sprtype_player.h"
#include "audio/ctm_audio.h"
#include "display/ctm_display.h"

/* Allocate sprite object.
 */
 
struct ctm_sprite *ctm_sprite_alloc(const struct ctm_sprtype *type) {
  if (!type) return 0;
  if (type->objlen<(int)sizeof(struct ctm_sprite)) return 0;
  struct ctm_sprite *spr=calloc(1,type->objlen);
  if (!spr) return 0;
  spr->refc=1;
  spr->type=type;
  spr->primaryrgb=0x808080;
  spr->opacity=0xff;
  if (type->init&&(type->init(spr)<0)) { ctm_sprite_killdel(spr); return 0; }
  return spr;
}

/* Delete sprite.
 */
  
void ctm_sprite_del(struct ctm_sprite *spr) {
  if (!spr) return;
  if (spr->refc>1) { spr->refc--; return; }
  if (spr->grpc) fprintf(stderr,"ctm:PANIC: Deleting sprite %p, grpc==%d.\n",spr,spr->grpc);
  if (spr->type->del) spr->type->del(spr);
  if (spr->grpv) free(spr->grpv);
  free(spr);
}

void ctm_sprite_killdel(struct ctm_sprite *spr) {
  if (!spr) return;
  if (spr->refc>spr->grpc) {
    ctm_sprite_kill(spr);
    ctm_sprite_del(spr);
  } else {
    ctm_sprite_kill(spr);
  }
}

/* Retain.
 */

int ctm_sprite_ref(struct ctm_sprite *spr) {
  if (!spr) return -1;
  if (spr->refc<1) return -1;
  if (spr->refc==INT_MAX) return -1;
  spr->refc++;
  return 0;
}

/* Deal damage to a sprite.
 */
 
int ctm_sprite_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant) {
  if (!spr) return -1;

  /* Does type handle this specially?
   */
  if (spr->type->hurt) return spr->type->hurt(spr,assailant);

  /* Sprite is being destroyed anyway, ignore it.
   */
  if (ctm_sprite_has_group(spr,&ctm_group_deathrow)) return 0;

  /* Play sound, create gore, ghost, and prize as necessary.
   */
  ctm_audio_effect(CTM_AUDIO_EFFECTID_MONSTER_KILLED,0xff);
  if (spr->type->soul) {
    if (ctm_sprite_create_ghost(spr->x,spr->y,spr->interior)<0) return -1;
  }
  if (spr->type->guts) {
    if (ctm_sprite_create_gore(spr->x,spr->y,spr->interior)<0) return -1;
  }
  if (assailant) {
    if (assailant->type==&ctm_sprtype_player) {
      struct ctm_sprite_player *ASSAILANT=(struct ctm_sprite_player*)assailant;
      if (ctm_player_create_prize(assailant,spr)<0) return -1;
      int party=((struct ctm_sprite_player*)assailant)->party;
      int favor=spr->type->soul?-CTM_MURDER_WITNESS_FAVOR:CTM_HEROISM_WITNESS_FAVOR;
      int err=ctm_perform_local_event(spr->x,spr->y,spr->interior,CTM_MURDER_WITNESS_RADIUS,party,favor);
      if (err<0) return -1;
      if ((favor<0)&&(ctm_player_add_wanted(assailant,CTM_WANTED_INCREMENT)<0)) return -1;
      if (spr->type==&ctm_sprtype_beast) {
        ASSAILANT->beastkills++;
      } else if (spr->type==&ctm_sprtype_cop) {
        ASSAILANT->copkills++;
      } else if (spr->type==&ctm_sprtype_voter) {
        ASSAILANT->voterkills++;
      }
    }
  }

  /* Kill sprite.
   */
  if (ctm_sprite_kill_later(spr)<0) return -1;
  
  return 0;
}

/* Super things that happen when you kill a man.
 */

int ctm_sprite_create_ghost(int x,int y,int interior) {
  struct ctm_sprite *ghost=ctm_sprite_alloc(&ctm_sprtype_ghost);
  if (!ghost) return -1;
  if (ctm_sprite_add_group(ghost,&ctm_group_keepalive)<0) { ctm_sprite_del(ghost); return -1; }
  ctm_sprite_del(ghost);
  ghost->x=x;
  ghost->y=y;
  if (ghost->interior=interior) {
    if (ctm_sprite_add_group(ghost,&ctm_group_interior)<0) { ctm_sprite_kill(ghost); return -1; }
  } else {
    if (ctm_sprite_add_group(ghost,&ctm_group_exterior)<0) { ctm_sprite_kill(ghost); return -1; }
  }
  if (ctm_sprite_add_group(ghost,&ctm_group_update)<0) { ctm_sprite_kill(ghost); return -1; }
  return 0;
}

int ctm_sprite_create_gore(int x,int y,int interior) {
  struct ctm_sprite *gore=ctm_sprite_alloc(&ctm_sprtype_gore);
  if (!gore) return -1;
  if (ctm_sprite_add_group(gore,&ctm_group_keepalive)<0) { ctm_sprite_del(gore); return -1; }
  ctm_sprite_del(gore);
  gore->x=x;
  gore->y=y;
  if (gore->interior=interior) {
    if (ctm_sprite_add_group(gore,&ctm_group_interior)<0) { ctm_sprite_kill(gore); return -1; }
  } else {
    if (ctm_sprite_add_group(gore,&ctm_group_exterior)<0) { ctm_sprite_kill(gore); return -1; }
  }
  if (ctm_sprite_add_group(gore,&ctm_group_update)<0) { ctm_sprite_kill(gore); return -1; }
  return 0;
}

/* Are we on screen, or close to it?
 */
 
int ctm_sprite_near_player(struct ctm_sprite *spr) {
  if (!spr) return 0;
  int distance=ctm_display_distance_to_player_screen(0,spr->x,spr->y,spr->interior);
  if (distance<0) return 0;
  if (distance<CTM_TILESIZE*4) return 1;
  return 0;
}
