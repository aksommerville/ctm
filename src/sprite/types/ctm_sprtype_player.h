/* ctm_sprtype_player.h
 */

#ifndef CTM_SPRTYPE_PLAYER_H
#define CTM_SPRTYPE_PLAYER_H

#include "sprite/ctm_sprite.h"

#define CTM_ITEM_NONE            0
#define CTM_ITEM_SWORD           1
#define CTM_ITEM_SPEECH          2
#define CTM_ITEM_METAL_BLADE     3
#define CTM_ITEM_MAGIC_WAND      4
#define CTM_ITEM_HOOKSHOT        5
#define CTM_ITEM_BOOMERANG       6
#define CTM_ITEM_AUTOBOX         7
#define CTM_ITEM_FLAMES          8
#define CTM_ITEM_BOW             9
#define CTM_ITEM_LENS_OF_TRUTH  10
#define CTM_ITEM_COIN           11
#define CTM_ITEM_12             12
#define CTM_ITEM_13             13
#define CTM_ITEM_14             14
#define CTM_ITEM_15             15

// Special items.
#define CTM_ITEM_PIZZA_FART     16

#define CTM_INVENTORY_UNLIMITED -1 // Infinite use, eg sword.

#define CTM_PRIZE_SPEECH      0
#define CTM_PRIZE_WAND_FLUID  1
#define CTM_PRIZE_BLADES      2
#define CTM_PRIZE_ARROW       3
#define CTM_PRIZE_COIN        4
#define CTM_PRIZE_HEART       5
#define CTM_PRIZE_PIZZA_FART  6

// Wanted level constants.
#define CTM_WANTED_MAX       1600 // Above this point, police take action and it never drops again.
#define CTM_WANTED_INCREMENT  400 // Add so much for each kill.
#define CTM_WANTED_DECAY        1 // Remove so much each frame.

struct ctm_sprite_player {
  struct ctm_sprite hdr;
  int playerid;
  uint16_t pvinput;
  int dx,dy;
  uint8_t col,row;
  int animcounter,animphase;
  int poscol,posrow; // Where on the grid are we standing?
  int party; // CTM_PARTY_RED or CTM_PARTY_BLUE
  int swinging;
  int speaking;
  int boxing;
  int pause;
  int item_primary;
  int item_secondary;
  int item_tertiary;
  int inventory[16]; // count or special value above; index is item ID.
  int item_store[16]; // Available but unequipped items, for display while paused.
  int hp,hpmax;
  int invincible;
  int wants_termination;
  struct ctm_sprgrp *holdgrp; // If populated, no other action is possible. (eg hookshot)
  int grabbed;
  struct ctm_sprgrp *boomerang;
  int lens_of_truth;
  int wanted[9]; // One for each state. 
  int copdelay;
  int64_t pause_framec;

  // Statistics and award:
  int award; // One of CTM_AWARD_*; set when game ends.
  int beastkills;
  int bosskills;
  int voterkills;
  int mummykills;
  int copkills;
  int strikes; // How many times was I hurt?
  int deaths; // How many times was I killed?
  int facespeeches; // How many speeches delivered in person?
  int radiospeeches; // How many speeches delivered over radio?
  
};

int ctm_player_has_item(struct ctm_sprite *spr,int itemid);
int ctm_player_add_inventory(struct ctm_sprite *spr,int itemid,int addc);
int ctm_player_spend(struct ctm_sprite *spr,int price); // >0 if spent, 0 if can't afford, <0 for real errors
int ctm_player_create_prize(struct ctm_sprite *spr,struct ctm_sprite *victim);
int ctm_player_get_treasure(struct ctm_sprite *spr,int item,int quantity);
int ctm_player_get_prize(struct ctm_sprite *spr,int prizeid);
int ctm_player_begin_action(struct ctm_sprite *spr,int item);
int ctm_player_end_action(struct ctm_sprite *spr,int item);
int ctm_player_abort_actions(struct ctm_sprite *spr);
int ctm_player_add_wanted(struct ctm_sprite *spr,int amount);

#define CTM_PLAYER_PHLEFT (-6)
#define CTM_PLAYER_PHRIGHT 6
#define CTM_PLAYER_PHTOP (-6)
#define CTM_PLAYER_PHBOTTOM 8

#endif
