#include "ctm.h"
#include "ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"

/* Globals.
 */

struct ctm_sprgrp ctm_group_keepalive={0};
struct ctm_sprgrp ctm_group_interior={0};
struct ctm_sprgrp ctm_group_exterior={0};
struct ctm_sprgrp ctm_group_update={0};
struct ctm_sprgrp ctm_group_player={0};
struct ctm_sprgrp ctm_group_voter={0};
struct ctm_sprgrp ctm_group_hazard={0};
struct ctm_sprgrp ctm_group_solid={0};
struct ctm_sprgrp ctm_group_prize={0};
struct ctm_sprgrp ctm_group_fragile={0};
struct ctm_sprgrp ctm_group_deathrow={0};
struct ctm_sprgrp ctm_group_beast={0};
struct ctm_sprgrp ctm_group_hookable={0};
struct ctm_sprgrp ctm_group_boomerangable={0};
struct ctm_sprgrp ctm_group_bonkable={0};
struct ctm_sprgrp ctm_group_cop={0};
struct ctm_sprgrp ctm_group_state_voter[9]={0};

int ctm_sprgrp_init() {
  ctm_group_interior.order=CTM_SPRGRP_ORDER_VISIBLE;
  ctm_group_exterior.order=CTM_SPRGRP_ORDER_VISIBLE;
  return 0;
}
 
void ctm_sprgrp_quit() {
  int i;
  #define QUITGRP(tag) \
    ctm_sprgrp_kill(&ctm_group_##tag); \
    if (ctm_group_##tag.sprv) free(ctm_group_##tag.sprv); \
    memset(&ctm_group_##tag,0,sizeof(struct ctm_sprgrp));
  QUITGRP(keepalive)
  QUITGRP(interior)
  QUITGRP(exterior)
  QUITGRP(update)
  QUITGRP(player)
  QUITGRP(voter)
  QUITGRP(hazard)
  QUITGRP(solid)
  QUITGRP(prize)
  QUITGRP(fragile)
  QUITGRP(deathrow)
  QUITGRP(beast)
  QUITGRP(hookable)
  QUITGRP(boomerangable)
  QUITGRP(bonkable)
  QUITGRP(cop)
  for (i=0;i<9;i++) {
    ctm_sprgrp_kill(ctm_group_state_voter+i);
    if (ctm_group_state_voter[i].sprv) free(ctm_group_state_voter[i].sprv);
  }
  memset(ctm_group_state_voter,0,sizeof(ctm_group_state_voter));
  #undef QUITGRP
}

/* Allocate and delete.
 */

struct ctm_sprgrp *ctm_sprgrp_new(int order) {
  struct ctm_sprgrp *grp=calloc(1,sizeof(struct ctm_sprgrp));
  if (!grp) return 0;
  grp->refc=1;
  grp->order=order;
  return grp;
}

void ctm_sprgrp_del(struct ctm_sprgrp *grp) {
  if (!grp) return;
  if (!grp->refc) return; // Immortal.
  if (grp->refc>1) { grp->refc--; return; }
  if (grp->sprc) fprintf(stderr,"ctm:PANIC: Deleting group %p, sprc==%d.\n",grp,grp->sprc);
  if (grp->sprv) free(grp->sprv);
  free(grp);
}

int ctm_sprgrp_ref(struct ctm_sprgrp *grp) {
  if (!grp) return -1;
  if (!grp->refc) return 0; // Immortal.
  if (grp->refc<1) return -1;
  if (grp->refc==INT_MAX) return -1;
  grp->refc++;
  return 0;
}

/* Primitive search, private.
 */

static int _ctm_sprite_search(const struct ctm_sprite *spr,const struct ctm_sprgrp *grp) {
  int lo=0,hi=spr->grpc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (grp<spr->grpv[ck]) hi=ck;
    else if (grp>spr->grpv[ck]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int _ctm_sprgrp_search(const struct ctm_sprgrp *grp,const struct ctm_sprite *spr) {
  switch (grp->order) {
    case CTM_SPRGRP_ORDER_VISIBLE: {
        // We can't optimize for VISIBLE, since its sorting is only best-effort.
        // However, we can select a reasonable insertion point if it's missing.
        int insp=-1,i;
        for (i=0;i<grp->sprc;i++) {
          if (grp->sprv[i]==spr) return i;
          if (insp<0) {
            if (spr->layer<grp->sprv[i]->layer) insp=i;
            else if ((spr->layer==grp->sprv[i]->layer)&&(spr->y<=grp->sprv[i]->y)) insp=i;
          }
        }
        if (insp>=0) return -insp-1;
        return -grp->sprc-1;
      }
    case CTM_SPRGRP_ORDER_EXPLICIT: {
        int i; for (i=0;i<grp->sprc;i++) {
          if (grp->sprv[i]==spr) return i;
        }
        return -grp->sprc-1;
      }
    case CTM_SPRGRP_ORDER_UNIMPORTANT: default: {
        int lo=0,hi=grp->sprc;
        while (lo<hi) {
          int ck=(lo+hi)>>1;
               if (spr<grp->sprv[ck]) hi=ck;
          else if (spr>grp->sprv[ck]) lo=ck+1;
          else return ck;
        }
        return -lo-1;
      }
  }
}

/* Primitive insert and remove, private.
 */

static int _ctm_sprite_insert(struct ctm_sprite *spr,int p,struct ctm_sprgrp *grp) {
  if ((p<0)||(p>spr->grpc)) return -1;
  if (spr->grpc>=spr->grpa) {
    int na=spr->grpa+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(spr->grpv,sizeof(void*)*na);
    if (!nv) return -1;
    spr->grpv=nv;
    spr->grpa=na;
  }
  if (ctm_sprgrp_ref(grp)<0) return -1;
  memmove(spr->grpv+p+1,spr->grpv+p,sizeof(void*)*(spr->grpc-p));
  spr->grpc++;
  spr->grpv[p]=grp;
  return 0;
}

static int _ctm_sprgrp_insert(struct ctm_sprgrp *grp,int p,struct ctm_sprite *spr) {
  if ((p<0)||(p>grp->sprc)) return -1;
  if (grp->sprc>=grp->spra) {
    int na=grp->spra+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(grp->sprv,sizeof(void*)*na);
    if (!nv) return -1;
    grp->sprv=nv;
    grp->spra=na;
  }
  if (ctm_sprite_ref(spr)<0) return -1;
  memmove(grp->sprv+p+1,grp->sprv+p,sizeof(void*)*(grp->sprc-p));
  grp->sprc++;
  grp->sprv[p]=spr;
  return 0;
}

static void _ctm_sprite_remove(struct ctm_sprite *spr,int p) {
  if ((p<0)||(p>=spr->grpc)) return;
  struct ctm_sprgrp *grp=spr->grpv[p];
  spr->grpc--;
  memmove(spr->grpv+p,spr->grpv+p+1,sizeof(void*)*(spr->grpc-p));
  ctm_sprgrp_del(grp);
}

static void _ctm_sprgrp_remove(struct ctm_sprgrp *grp,int p) {
  if ((p<0)||(p>=grp->sprc)) return;
  struct ctm_sprite *spr=grp->sprv[p];
  grp->sprc--;
  memmove(grp->sprv+p,grp->sprv+p+1,sizeof(void*)*(grp->sprc-p));
  ctm_sprite_del(spr);
}

/* Simple public accessors.
 */

int ctm_sprgrp_has_sprite(const struct ctm_sprgrp *grp,const struct ctm_sprite *spr) {
  if (!grp||!spr) return 0;
  return (_ctm_sprite_search(spr,grp)>=0)?1:0;
}

int ctm_sprgrp_add_sprite(struct ctm_sprgrp *grp,struct ctm_sprite *spr) {
  if (!grp||!spr) return -1;
  int grpp=_ctm_sprite_search(spr,grp);
  if (grpp>=0) return 0;
  grpp=-grpp-1;
  int sprp=_ctm_sprgrp_search(grp,spr);
  if (sprp>=0) return -1;
  sprp=-sprp-1;
  if (_ctm_sprite_insert(spr,grpp,grp)<0) return -1;
  if (_ctm_sprgrp_insert(grp,sprp,spr)<0) { _ctm_sprite_remove(spr,grpp); return -1; }
  return 1;
}

int ctm_sprgrp_remove_sprite(struct ctm_sprgrp *grp,struct ctm_sprite *spr) {
  if (!grp||!spr) return 0;
  int grpp=_ctm_sprite_search(spr,grp);
  if (grpp<0) return 0;
  int sprp=_ctm_sprgrp_search(grp,spr);
  if (sprp<0) return -1;
  if (grp->refc) {
    if (ctm_sprgrp_ref(grp)<0) return -1;
    _ctm_sprite_remove(spr,grpp);
    _ctm_sprgrp_remove(grp,sprp);
    ctm_sprgrp_del(grp);
  } else {
    _ctm_sprite_remove(spr,grpp);
    _ctm_sprgrp_remove(grp,sprp);
  }
  return 1;
}

/* Compound accessors.
 */

int ctm_sprgrp_clear(struct ctm_sprgrp *grp) {
  if (!grp) return -1;
  if (grp->sprc<1) return 0;
  if (ctm_sprgrp_ref(grp)<0) return -1;
  while (grp->sprc>0) {
    struct ctm_sprite *spr=grp->sprv[--(grp->sprc)];
    int grpp=_ctm_sprite_search(spr,grp);
    if (grpp>=0) _ctm_sprite_remove(spr,grpp);
    ctm_sprite_del(spr);
  }
  ctm_sprgrp_del(grp);
  return 0;
}

int ctm_sprite_kill(struct ctm_sprite *spr) {
  if (!spr) return -1;
  if (spr->grpc<1) return 0;
  if (ctm_sprite_ref(spr)<0) return -1;
  while (spr->grpc>0) {
    struct ctm_sprgrp *grp=spr->grpv[--(spr->grpc)];
    int sprp=_ctm_sprgrp_search(grp,spr);
    if (sprp>=0) _ctm_sprgrp_remove(grp,sprp);
    ctm_sprgrp_del(grp);
  }
  ctm_sprite_del(spr);
  return 0;
}

int ctm_sprgrp_kill(struct ctm_sprgrp *grp) {
  if (!grp) return -1;
  if (grp->sprc<1) return 0;
  if (ctm_sprgrp_ref(grp)<0) return -1;
  while (grp->sprc>0) {
    struct ctm_sprite *spr=grp->sprv[--(grp->sprc)];
    ctm_sprite_kill(spr);
    ctm_sprite_del(spr);
  }
  ctm_sprgrp_del(grp);
  return 0;
}

/* Copy group.
 */
 
int ctm_sprgrp_copy(struct ctm_sprgrp *dst,struct ctm_sprgrp *src) {
  if (!dst||!src) return -1;
  if (dst==src) return 0;
  ctm_sprgrp_clear(dst);
  dst->order=src->order;
  if (dst->spra<src->sprc) {
    void *nv=malloc(sizeof(void*)*src->sprc);
    if (!nv) return -1;
    if (dst->sprv) free(dst->sprv);
    dst->sprv=nv;
    dst->spra=src->sprc;
  }
  int i; for (i=0;i<src->sprc;i++) {
    struct ctm_sprite *spr=src->sprv[i];
    int grpp=_ctm_sprite_search(spr,dst);
    if (grpp>=0) return -1;
    grpp=-grpp-1;
    if (ctm_sprgrp_ref(dst)<0) return -1;
    if (ctm_sprite_ref(spr)<0) { ctm_sprgrp_del(dst); return -1; }
    if (_ctm_sprite_insert(spr,grpp,dst)<0) { ctm_sprgrp_del(dst); ctm_sprite_del(spr); return -1; }
    dst->sprv[dst->sprc++]=spr;
  }
  return 0;
}

/* Get player sprite.
 */
 
struct ctm_sprite *ctm_sprite_for_player(int playerid) {
  if ((playerid<1)||(playerid>4)) return 0;
  int i; for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_player.sprv[i];
    if (spr->type!=&ctm_sprtype_player) continue;
    if (((struct ctm_sprite_player*)spr)->playerid!=playerid) continue;
    return spr;
  }
  return 0;
}

/* Single pass of sort.
 */
 
int ctm_sprgrp_semisort(struct ctm_sprgrp *grp) {
  if (!grp) return -1;
  if (grp->sprc<2) return 0;
  int first,last,i;
  if (grp->sortd==1) { first=0; last=grp->sprc-1; }
  else { grp->sortd=-1; first=grp->sprc-1; last=0; }
  for (i=first;i!=last;i+=grp->sortd) {
    int cmp;
         if (grp->sprv[i]->layer<grp->sprv[i+grp->sortd]->layer) cmp=-1;
    else if (grp->sprv[i]->layer>grp->sprv[i+grp->sortd]->layer) cmp=1;
    else if (grp->sprv[i]->y<grp->sprv[i+grp->sortd]->y) cmp=-1;
    else if (grp->sprv[i]->y>grp->sprv[i+grp->sortd]->y) cmp=1;
    else cmp=0;
    if (cmp==grp->sortd) {
      struct ctm_sprite *tmp=grp->sprv[i];
      grp->sprv[i]=grp->sprv[i+grp->sortd];
      grp->sprv[i+grp->sortd]=tmp;
    }
  }
  grp->sortd*=-1;
  return 0;
}

/* Find a random living player sprite.
 */
 
struct ctm_sprite *ctm_get_random_living_player_sprite() {
  int playerc=0;
  int i; for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_player.sprv[i];
    if (!spr||(spr->type!=&ctm_sprtype_player)) continue;
    struct ctm_sprite_player *SPR=(struct ctm_sprite_player*)spr;
    if (!SPR->hp) continue;
    playerc++;
  }
  if (!playerc) return 0;
  int playerix=rand()%playerc;
  for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_player.sprv[i];
    if (!spr||(spr->type!=&ctm_sprtype_player)) continue;
    struct ctm_sprite_player *SPR=(struct ctm_sprite_player*)spr;
    if (!SPR->hp) continue;
    if (!playerix--) return spr;
  }
  return 0;
}
