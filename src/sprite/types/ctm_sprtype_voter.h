/* ctm_sprtype_voter.h
 */

#ifndef CTM_SPRTYPE_VOTER_H
#define CTM_SPRTYPE_VOTER_H

#include "sprite/ctm_sprite.h"

struct ctm_sprite_voter {
  struct ctm_sprite hdr;
  struct ctm_rgb skincolor;
  struct ctm_rgb haircolor;
  struct ctm_rgb shirtcolor;
  struct ctm_rgb trouserscolor;
  uint8_t bodytile;
  uint8_t trouserstile;
  uint8_t shirttile;
  uint8_t headtile;
  uint8_t hairtile;
  int race;
  int party;
  int8_t decision; // <0:blue, >0:red
  int numb; // Counts toward zero after modification of decision.
};

// Initialize tiles and colors for this sprite.
// Use zero to pick randomly (purely random; equal weights).
int ctm_voter_setup(struct ctm_sprite *spr,int race,int party);

int ctm_voter_consider(struct ctm_sprite *spr);

// Award so many points to blue or red party.
// Toggling (party) has the same effect as swapping (delta)'s sign.
// Favoring any other party, eg zero, drives decision towards (positive) or away from (negative) zero.
int ctm_voter_favor(struct ctm_sprite *spr,int party,int delta);

#endif
