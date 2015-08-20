#include "ctm.h"
#include "ctm_sprtype_treasure.h"
#include "game/ctm_grid.h"

#define SPR ((struct ctm_sprite_treasure*)spr)

/* Type definition.
 */

const struct ctm_sprtype ctm_sprtype_treasure={
  .name="treasure",
  .objlen=sizeof(struct ctm_sprite_treasure),
  .soul=0,
  .guts=0,
};

/* Finalize position.
 * Initially, we copy the focus sprite's position.
 * Look for a spot nearby, accessible*, but not right underfoot.
 * Best effort: If all else fails, we can leave everything as is.
 * We don't do anything cool like route poisoning, so it could easily end up back where it started.
 * Whatever.
 * [*] What we call "accessible", means we only traverse cells with identical properties.
 */

static int ctm_treasure_step_position(int *col,int *row,int interior) {
  const struct ctm_grid_cell *cellv=ctm_grid.cellv+(*row)*ctm_grid.colc+(*col);
  #define MATCH(d) ((interior \
    ?(ctm_tileprop_interior[cellv[0].itile]==ctm_tileprop_interior[cellv[d].itile]) \
    :(ctm_tileprop_exterior[cellv[0].tile]==ctm_tileprop_exterior[cellv[d].tile]) \
  )?1:0)
  int paths[4]; // left,right,up,down
  if (*col<=0) paths[0]=0; else paths[0]=MATCH(-1);
  if (*col>=ctm_grid.colc-1) paths[1]=0; else paths[1]=MATCH(1);
  if (*row<=0) paths[2]=0; else paths[2]=MATCH(-ctm_grid.colc);
  if (*row>=ctm_grid.rowc-1) paths[3]=0; else paths[3]=MATCH(ctm_grid.colc);
  #undef MATCH
  int pathc=0,i;
  for (i=0;i<4;i++) pathc+=paths[i];
  if (pathc<1) return 0;
  int choice=rand()%pathc;
  for (i=0;i<4;i++) {
    if (!paths[i]) continue;
    if (!choice--) {
      switch (i) {
        case 0: (*col)--; break;
        case 1: (*col)++; break;
        case 2: (*row)--; break;
        case 3: (*row)++; break;
      }
      return 1;
    }
  }
  return 0;
}

static int ctm_treasure_finalize_position_DRUNKWALK(struct ctm_sprite *spr) {

  int col=spr->x/CTM_TILESIZE,row=spr->y/CTM_TILESIZE;
  if ((col<0)||(row<0)||(col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return 0;

  int stepc=3+rand()%7;
  while (stepc-->0) {
    if (!ctm_treasure_step_position(&col,&row,spr->interior)) return 0;
  }

  spr->x=col*CTM_TILESIZE+(CTM_TILESIZE>>1);
  spr->y=row*CTM_TILESIZE+(CTM_TILESIZE>>1);

  return 0;
}

static int ctm_treasure_dicey_neighborhood(int col,int row,int interior) {
  const int radius=CTM_TILESIZE*3;
  int l=col*CTM_TILESIZE+(CTM_TILESIZE>>1);
  int t=row*CTM_TILESIZE+(CTM_TILESIZE>>1);
  int r=l+radius;
  int b=r+radius;
  int i; for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_player.sprv[i];
    if (spr->interior!=interior) continue;
    if (spr->x<l) continue;
    if (spr->x>r) continue;
    if (spr->y<t) continue;
    if (spr->y>b) continue;
    return 1;
  }
  return 0;
}

static int ctm_treasure_finalize_position(struct ctm_sprite *spr) {

  // If we're out of bounds, give up.
  if ((spr->x<0)||(spr->y<0)) return 0;
  int col=spr->x/CTM_TILESIZE,row=spr->y/CTM_TILESIZE;
  if ((col>=ctm_grid.colc)||(row>=ctm_grid.rowc)) return 0;

  // Treasure is normally spawned in a rectangular room, or something close to it.
  // Try to appear in the center.
  // If that's no good, try the other 8 cardinals and corners.
  uint32_t badmask=(CTM_TILEPROP_SOLID|CTM_TILEPROP_POROUS|CTM_TILEPROP_WATER|CTM_TILEPROP_HOLE|CTM_TILEPROP_DOOR);
  #define BADCELL(c,r) ((spr->interior?ctm_tileprop_interior[ctm_grid.cellv[(r)*ctm_grid.colc+(c)].itile]:ctm_tileprop_exterior[ctm_grid.cellv[(r)*ctm_grid.colc+(c)].tile])&badmask)
  if (BADCELL(col,row)) return 0;
  int colc=1,rowc=1;
  while ((col>0)&&!BADCELL(col-1,row)) { col--; colc++; }
  while ((row>0)&&!BADCELL(col,row-1)) { row--; rowc++; }
  while ((colc+colc<ctm_grid.colc)&&!BADCELL(colc+colc,row)) colc++;
  while ((rowc+rowc<ctm_grid.rowc)&&!BADCELL(col,rowc+row)) rowc++;
  
  int dstcol=col+(colc>>1),dstrow=row+(rowc>>1); // 1: Dead center
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col+(colc>>1); dstrow=row; // 2: North
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col+(colc>>1); dstrow=row+rowc-1; // 3: South
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col; dstrow=row+(rowc>>1); // 4: West
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col+colc-1; dstrow=row+(rowc>>1); // 5: East
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col; dstrow=row; // 6: Northwest
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col+colc-1; dstrow=row; // 7: Northeast
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col; dstrow=row+rowc-1; // 8: Southwest
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) {
  dstcol=col+colc-1; dstrow=row+rowc-1; // 9: Southeast
  if (BADCELL(dstcol,dstrow)||ctm_treasure_dicey_neighborhood(dstcol,dstrow,spr->interior)) return 0; // OK, fuck it.
  }}}}}}}}
  
  #undef BADCELL
  spr->x=dstcol*CTM_TILESIZE+(CTM_TILESIZE>>1);
  spr->y=dstrow*CTM_TILESIZE+(CTM_TILESIZE>>1);
  return 0;
}

/* Public ctor.
 */
 
struct ctm_sprite *ctm_treasure_new(struct ctm_sprite *focus,int item,int quantity) {

  if (!focus) {
    if (ctm_group_player.sprc>0) focus=ctm_group_player.sprv[0];
    else if (ctm_group_keepalive.sprc>0) focus=ctm_group_keepalive.sprv[0];
    else return 0;
  }
  if (quantity<0) quantity=0;
  //if ((item<0)||(item>=16)) item=0;

  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_treasure);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);

  if (ctm_sprite_add_group(spr,&ctm_group_prize)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_boomerangable)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) return 0;
  SPR->item=item;
  SPR->quantity=quantity;

  spr->x=focus->x;
  spr->y=focus->y;
  spr->tile=0x4c;
  if (spr->interior=focus->interior) {
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return 0;
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) return 0;
  }

  if (ctm_treasure_finalize_position(spr)<0) return 0;

  return spr;
}

/* Choose random position for new treasure.
 */
 
static int ctm_treasure_locc=0;
 
static int ctm_treasure_locate(int *x,int *y) {
  if (!x||!y) return -1;

  // Count possible locations and cache it.
  if (ctm_treasure_locc<1) {
    int p; for (p=ctm_grid.colc*ctm_grid.rowc;p-->0;) {
      if (!ctm_tileprop_interior[ctm_grid.cellv[p].itile]) ctm_treasure_locc++;
    }
    if (ctm_treasure_locc<1) return 0;
  }
  
  // Pick a random index up to ctm_treasure_locc, confirm that no sprite occupies that cell.
  int panic=ctm_treasure_locc; while (panic-->0) {
    int index=rand()%ctm_treasure_locc;
    int collision=0;
    int p; for (p=ctm_grid.colc*ctm_grid.rowc;p-->0;) {
      if (ctm_tileprop_interior[ctm_grid.cellv[p].itile]) continue;
      if (--index<=0) {
        *x=(p%ctm_grid.colc)*CTM_TILESIZE+(CTM_TILESIZE>>1);
        *y=(p/ctm_grid.colc)*CTM_TILESIZE+(CTM_TILESIZE>>1);
        int i;
        for (i=0;i<ctm_group_interior.sprc;i++) {
          struct ctm_sprite *spr=ctm_group_interior.sprv[i];
          int dx=spr->x-*x;
          int dy=spr->y-*y;
          if ((dx<=-CTM_TILESIZE)||(dx>=-CTM_TILESIZE)||(dy<=-CTM_TILESIZE)||(dy>=CTM_TILESIZE)) continue;
          collision=1;
          break;
        }
        if (collision) break;
        return 1;
      }
    }
  }

  return 0;
}

/* Public ctor, random position.
 */
 
struct ctm_sprite *ctm_treasure_new_random(int item,int quantity) {
  int x,y;
  if (ctm_treasure_locate(&x,&y)<1) return 0;
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_treasure);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  
  spr->x=x;
  spr->y=y;
  spr->interior=1;
  SPR->item=item;
  SPR->quantity=quantity;
  spr->tile=0x4c;

  if (ctm_sprite_add_group(spr,&ctm_group_prize)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_boomerangable)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return 0;
  
  return spr;
}
