#ifndef CTM_SPRTYPE_TREASURE_H
#define CTM_SPRTYPE_TREASURE_H

#include "sprite/ctm_sprite.h"

struct ctm_sprite_treasure {
  struct ctm_sprite hdr;
  int item;
  int quantity;
};

struct ctm_sprite *ctm_treasure_new(struct ctm_sprite *focus,int item,int quantity);
struct ctm_sprite *ctm_treasure_new_random(int item,int quantity);

#endif
