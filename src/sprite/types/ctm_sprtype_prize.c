#include "ctm.h"
#include "sprite/ctm_sprite.h"

#define CTM_PRIZE_TTL       300
#define CTM_PRIZE_FADE_TIME 100
#define CTM_PRIZE_COUNT 9

struct ctm_sprite_prize {
  struct ctm_sprite hdr;
  int ttl;
  int prizeid;
};

#define SPR ((struct ctm_sprite_prize*)spr)

/* Init.
 */

static int _ctm_prize_init(struct ctm_sprite *spr) {

  SPR->prizeid=rand()%CTM_PRIZE_COUNT;
  SPR->ttl=CTM_PRIZE_TTL;

  spr->layer=-1;
  spr->tile=0xe0+SPR->prizeid;
  spr->primaryrgb=0x808080;
  spr->opacity=0xff;
  spr->tintrgba=0x00000000;
  
  return 0;
}

/* Update.
 */

static int _ctm_prize_update(struct ctm_sprite *spr) {
  if (--(SPR->ttl)<=0) return ctm_sprite_kill(spr);
  if (SPR->ttl<CTM_PRIZE_FADE_TIME) spr->opacity=(SPR->ttl*255)/CTM_PRIZE_FADE_TIME;
  return 0;
}

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_prize={
  .name="prize",
  .objlen=sizeof(struct ctm_sprite_prize),
  .soul=0,
  .guts=0,
  .init=_ctm_prize_init,
  .update=_ctm_prize_update,
};

/* Set prize id.
 */
 
int ctm_sprite_prize_set_prizeid(struct ctm_sprite *spr,int prizeid) {
  if (!spr||(spr->type!=&ctm_sprtype_prize)) return -1;
  if ((prizeid<0)||(prizeid>16)) return 0;
  SPR->prizeid=prizeid;
  spr->tile=0xe0+SPR->prizeid;
  return 0;
}

int ctm_sprite_prize_get_prizeid(struct ctm_sprite *spr) {
  if (!spr||(spr->type!=&ctm_sprtype_prize)) return 0;
  return SPR->prizeid;
}
