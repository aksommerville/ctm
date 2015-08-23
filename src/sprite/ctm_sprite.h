/* ctm_sprite.h
 * Defines generic sprite and a few related types.
 * Sprites are reference-counted, but normally each reference is a group.
 * Sprites and groups are attached mutually.
 * "Killing" a sprite means removing it from all groups, which usually deletes the object too.
 * Groups can be allocated dynamically or statically.
 */

#ifndef CTM_SPRITE_H
#define CTM_SPRITE_H

struct ctm_sprite;
struct ctm_sprgrp;
struct ctm_sprtype;

#define CTM_MURDER_WITNESS_RADIUS (CTM_TILESIZE*6)
#define CTM_MURDER_WITNESS_FAVOR  10
#define CTM_HEROISM_WITNESS_FAVOR  7

/* Generic sprite header.
 */

struct ctm_sprite {
  const struct ctm_sprtype *type;
  struct ctm_sprgrp **grpv;
  int grpc,grpa;
  int refc;
  int layer;
  int x,y;
  int interior;

  // If the sprite's type does not implement "draw", wrapper draws a single tile with these inputs:
  uint8_t tile;
  uint32_t primaryrgb;
  uint8_t opacity;
  uint32_t tintrgba;
  int flop;
  
};

struct ctm_sprite *ctm_sprite_alloc(const struct ctm_sprtype *type);
void ctm_sprite_del(struct ctm_sprite *spr);
int ctm_sprite_ref(struct ctm_sprite *spr);
void ctm_sprite_killdel(struct ctm_sprite *spr);

#define ctm_sprite_kill_later(spr) ctm_sprite_add_group(spr,&ctm_group_deathrow)

int ctm_sprite_hurt(struct ctm_sprite *spr,struct ctm_sprite *assailant);

int ctm_sprite_create_ghost(int x,int y,int interior);
int ctm_sprite_create_gore(int x,int y,int interior);
int ctm_sprite_create_fireworks(int x,int y,int interior);

struct ctm_sprite *ctm_sprite_missile_new(struct ctm_sprite *owner,uint8_t tile,int dx,int dy);
struct ctm_sprite *ctm_sprite_fireball_new(struct ctm_sprite *owner,uint8_t tile);
struct ctm_sprite *ctm_sprite_toast_new(int x,int y,int interior,uint8_t tile);
struct ctm_sprite *ctm_sprite_cop_new(struct ctm_sprite *player);

/* Returns zero if no beast was created.
 * We only try one random location, and then give up.
 * So this will probably fail more than succeed.
 * Returns <0 for real errors.
 */
int ctm_sprite_spawn_beast();

int ctm_sprite_near_player(struct ctm_sprite *spr);

/* Group.
 */

#define CTM_SPRGRP_ORDER_UNIMPORTANT     0 // Lets us sort by address, speeds things up a bit.
#define CTM_SPRGRP_ORDER_VISIBLE         1 // Try to keep list z-sorted.
#define CTM_SPRGRP_ORDER_EXPLICIT        2 // Preserve order of insertion.

struct ctm_sprgrp {
  struct ctm_sprite **sprv;
  int sprc,spra;
  int refc;
  int order; // Do not change after construction!
  int sortd;
};

extern struct ctm_sprgrp ctm_group_keepalive;
extern struct ctm_sprgrp ctm_group_deathrow;
extern struct ctm_sprgrp ctm_group_interior;
extern struct ctm_sprgrp ctm_group_exterior;
extern struct ctm_sprgrp ctm_group_update;
extern struct ctm_sprgrp ctm_group_player;
extern struct ctm_sprgrp ctm_group_voter;
extern struct ctm_sprgrp ctm_group_hazard;
extern struct ctm_sprgrp ctm_group_solid;
extern struct ctm_sprgrp ctm_group_prize;
extern struct ctm_sprgrp ctm_group_fragile;
extern struct ctm_sprgrp ctm_group_beast;
extern struct ctm_sprgrp ctm_group_hookable;
extern struct ctm_sprgrp ctm_group_boomerangable;
extern struct ctm_sprgrp ctm_group_bonkable;
extern struct ctm_sprgrp ctm_group_cop;
extern struct ctm_sprgrp ctm_group_state_voter[9];

int ctm_sprgrp_init();
void ctm_sprgrp_quit();

struct ctm_sprgrp *ctm_sprgrp_new(int order);
void ctm_sprgrp_del(struct ctm_sprgrp *grp);
int ctm_sprgrp_ref(struct ctm_sprgrp *grp);

int ctm_sprgrp_has_sprite(const struct ctm_sprgrp *grp,const struct ctm_sprite *spr);
int ctm_sprgrp_add_sprite(struct ctm_sprgrp *grp,struct ctm_sprite *spr);
int ctm_sprgrp_remove_sprite(struct ctm_sprgrp *grp,struct ctm_sprite *spr);
#define ctm_sprite_has_group(spr,grp) ctm_sprgrp_has_sprite(grp,spr)
#define ctm_sprite_add_group(spr,grp) ctm_sprgrp_add_sprite(grp,spr)
#define ctm_sprite_remove_group(spr,grp) ctm_sprgrp_remove_sprite(grp,spr)

int ctm_sprgrp_clear(struct ctm_sprgrp *grp);
int ctm_sprite_kill(struct ctm_sprite *spr);
int ctm_sprgrp_kill(struct ctm_sprgrp *grp);

// Clear (dst) and add all sprites from (src) into it.
// This is much faster than transferring sprites individually.
int ctm_sprgrp_copy(struct ctm_sprgrp *dst,struct ctm_sprgrp *src);

// If not NULL, always a sprite of type 'player'.
struct ctm_sprite *ctm_sprite_for_player(int playerid);

int ctm_sprgrp_semisort(struct ctm_sprgrp *grp);

/* Type.
 */

struct ctm_sprtype {

  const char *name;
  int objlen;

  int soul; // Sprites have a soul; ie a ghost appears when you kill it.
  int guts; // Sprites are filled with meat which spills out when opened.

  int (*init)(struct ctm_sprite *spr);
  void (*del)(struct ctm_sprite *spr);

  int (*draw)(struct ctm_sprite *spr,int addx,int addy);
  int (*update)(struct ctm_sprite *spr);
  int (*hurt)(struct ctm_sprite *spr,struct ctm_sprite *assailant);
  int (*grab)(struct ctm_sprite *spr,int grab);

  /* If you join ctm_sprgrp_fragile but do not implement this, sprite is a square at CTM_TILESIZE.
   * If you implement, this must return >0 if a collision occurred.
   * Called only if you are in ctm_sprgrp_fragile, and on the same terior as the weapon.
   * (x,y,w,h) is the effective bounds of the weapon.
   * (assailant) is NULL, or the sprite responsible, typically a hero.
   */
  int (*test_damage_collision)(struct ctm_sprite *spr,int x,int y,int w,int h,struct ctm_sprite *assailant);

  /* Same as test_damage_collision, for checking when we should hurt the heroes.
   * It is perfectly sensible to point this to the same function as test_damage_collision.
   */
  int (*test_hazard_collision)(struct ctm_sprite *spr,int x,int y,int w,int h,struct ctm_sprite *victim);

};

extern const struct ctm_sprtype ctm_sprtype_dummy;
extern const struct ctm_sprtype ctm_sprtype_player;
extern const struct ctm_sprtype ctm_sprtype_voter;
extern const struct ctm_sprtype ctm_sprtype_ghost;
extern const struct ctm_sprtype ctm_sprtype_gore;
extern const struct ctm_sprtype ctm_sprtype_fireworks;
extern const struct ctm_sprtype ctm_sprtype_prize;
extern const struct ctm_sprtype ctm_sprtype_missile;
extern const struct ctm_sprtype ctm_sprtype_beast;
extern const struct ctm_sprtype ctm_sprtype_fireball;
extern const struct ctm_sprtype ctm_sprtype_treasure;
extern const struct ctm_sprtype ctm_sprtype_toast;
extern const struct ctm_sprtype ctm_sprtype_hookshot;
extern const struct ctm_sprtype ctm_sprtype_boomerang;
extern const struct ctm_sprtype ctm_sprtype_autobox;
extern const struct ctm_sprtype ctm_sprtype_flames;
extern const struct ctm_sprtype ctm_sprtype_cop;

// The minibosses:
extern const struct ctm_sprtype ctm_sprtype_santa;
extern const struct ctm_sprtype ctm_sprtype_robot;
extern const struct ctm_sprtype ctm_sprtype_cthulhu;
extern const struct ctm_sprtype ctm_sprtype_vampire;
extern const struct ctm_sprtype ctm_sprtype_buffalo;
extern const struct ctm_sprtype ctm_sprtype_crab;
extern const struct ctm_sprtype ctm_sprtype_pineapple;
extern const struct ctm_sprtype ctm_sprtype_coyote;
extern const struct ctm_sprtype ctm_sprtype_alligator;

#endif
