#include "ctm.h"
#include "ctm_spawner.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "game/ctm_grid.h"
#include "game/ctm_game.h"
#include "display/ctm_display.h"

struct ctm_sprite *ctm_sprite_beast_summon(int x,int y,int interior);

/* Globals.
 */

// Spawn ops are a little expensive, so right off the bat, we skip so many frames.
#define CTM_SPAWNER_UPDATE_INTERVAL 10

// Screen edges, so we can indicate a preference for the one the player is looking at.
#define CTM_SPAWNER_EDGE_NONE     0
#define CTM_SPAWNER_EDGE_LEFT     1
#define CTM_SPAWNER_EDGE_RIGHT    2
#define CTM_SPAWNER_EDGE_TOP      3
#define CTM_SPAWNER_EDGE_BOTTOM   4

static struct {
  int init;
  int target_population; // Constant after reset. How many beasts do we want per player?
  int update_timer;
} ctm_spawner={0};

/* Spawner's perspective of a display, for private calculations.
 */

struct ctm_spawner_display {
  int x,y,w,h; // Boundaries in game space.
  int interior; // Displaying interior?
  int beastc; // Current count of visible beasts.
  int playerid; // ID of owning player.
  int playerx,playery; // Owning player's position.
  int playeralive; // Nonzero if player is alive, ie we should spawn and attract.
  int prefer_edge; // If nonzero, player is moving towards this edge and we should prefer to spawn there.
};

/* Count beast sprites visible on one display.
 */

static int ctm_spawner_count_beasts_in_display(struct ctm_spawner_display *display) {
  int left=display->x-CTM_TILESIZE;
  int top=display->y-CTM_TILESIZE;
  int right=left+display->w+(CTM_TILESIZE<<1);
  int bottom=top+display->h+(CTM_TILESIZE<<1);
  int beastc=0;
  int i; for (i=0;i<ctm_group_beast.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_beast.sprv[i];
    if (!spr) continue;
    if (spr->interior!=display->interior) continue;
    if (spr->x<left) continue;
    if (spr->y<top) continue;
    if (spr->x>right) continue;
    if (spr->y>bottom) continue;
    beastc++;
  }
  return beastc;
}

/* Acquire displays.
 */

static int ctm_spawner_acquire_displays(struct ctm_spawner_display *displayv) {
  int displayc=0;
  int playerid;
  for (playerid=1;playerid<=ctm_game.playerc;playerid++) {
  
    struct ctm_display *display=ctm_display_for_playerid(playerid);
    if (!display) continue;
    struct ctm_spawner_display *dst=displayv+displayc;
    if (ctm_display_game_get_position(
      &dst->x,&dst->y,&dst->w,&dst->h,&dst->interior,display
    )<0) continue;
    
    dst->playerid=playerid;
    displayc++;
    
    struct ctm_sprite *spr=ctm_sprite_for_player(playerid);
    if (spr) {
      struct ctm_sprite_player *SPR=(struct ctm_sprite_player*)spr;
      dst->playerx=spr->x;
      dst->playery=spr->y;
      dst->playeralive=(SPR->hp>0)?1:0;
      if (SPR->dx<0) dst->prefer_edge=CTM_SPAWNER_EDGE_LEFT;
      else if (SPR->dx>0) dst->prefer_edge=CTM_SPAWNER_EDGE_RIGHT;
      else if (SPR->dy<0) dst->prefer_edge=CTM_SPAWNER_EDGE_TOP;
      else if (SPR->dy>0) dst->prefer_edge=CTM_SPAWNER_EDGE_BOTTOM;
      else dst->prefer_edge=CTM_SPAWNER_EDGE_NONE;
    } else {
      dst->playerx=dst->x+(dst->w>>1);
      dst->playery=dst->y+(dst->h>>1);
      dst->playeralive=0;
      dst->prefer_edge=CTM_SPAWNER_EDGE_NONE;
    }

    dst->beastc=ctm_spawner_count_beasts_in_display(dst);
  }
  return displayc;
}

/* Nonzero if a display would overlap a CTM_TILESIZE square sprite.
 * We allow half a tile of grace on each edge, too.
 */

static int ctm_spawner_display_contains_sprite(struct ctm_spawner_display *display,int x,int y) {
  if (x<display->x-CTM_TILESIZE) return 0;
  if (y<display->y-CTM_TILESIZE) return 0;
  if (x>display->x+display->w+CTM_TILESIZE) return 0;
  if (y>display->y+display->h+CTM_TILESIZE) return 0;
  return 1;
}

/* Spawn sprite.
 */

static int ctm_spawner_spawn_in_display(struct ctm_spawner_display *display,struct ctm_spawner_display *displayv,int displayc) {

  /* Choose an edge, off of which the new beast will appear.
   * If the display's player is in motion, we use the edge he's walking towards 50% of the time.
   */
  int edge;
  if (display->prefer_edge&&(rand()&1)) {
    edge=display->prefer_edge;
  } else {
    edge=1+(rand()&3);
  }

  /* Choose a random position along the selected edge, snapping to the grid.
   * If we don't randomly choose an available spot, give up.
   */
  int x,y;
  switch (edge) {
    case CTM_SPAWNER_EDGE_LEFT: {
        x=ctm_grid_round_down_to_cell_midpoint(display->x-(CTM_TILESIZE>>1)); 
        y=ctm_grid_round_to_cell_midpoint(display->y+(rand()%display->h)); 
      } break;
    case CTM_SPAWNER_EDGE_RIGHT: {
        x=ctm_grid_round_up_to_cell_midpoint(display->x+display->w+(CTM_TILESIZE>>1));
        y=ctm_grid_round_to_cell_midpoint(display->y+(rand()%display->h));
      } break;
    case CTM_SPAWNER_EDGE_TOP: {
        x=ctm_grid_round_to_cell_midpoint(display->x+(rand()%display->w)); 
        y=ctm_grid_round_down_to_cell_midpoint(display->y-(CTM_TILESIZE>>1)); 
      } break;
    case CTM_SPAWNER_EDGE_BOTTOM: {
        x=ctm_grid_round_to_cell_midpoint(display->x+(rand()%display->w)); 
        y=ctm_grid_round_up_to_cell_midpoint(display->y+display->h+(CTM_TILESIZE>>1));
      } break;
    default: return -1;
  }
  uint32_t tileprop=ctm_grid_tileprop_for_pixel(x,y,display->interior);
  if (tileprop) {
    return 0;
  }

  /* If the spot we chose is visible to any display, abort.
   */
  int i; for (i=0;i<displayc;i++) {
    if (display==displayv+i) continue;
    if (ctm_spawner_display_contains_sprite(displayv+i,x,y)) {
      return 0;
    }
  }

  /* OK, spawn it. */
  struct ctm_sprite *spr=ctm_sprite_beast_summon(x,y,display->interior);
  if (!spr) {
    return 0;
  }

  return 0;
}

/* Update.
 */

int ctm_spawner_update() {
  if (!ctm_spawner.init) return -1;

  /* Skip frame? */
  if (ctm_spawner.update_timer++<CTM_SPAWNER_UPDATE_INTERVAL) return 0;
  ctm_spawner.update_timer=0;

  /* Generate a list of displays. */
  struct ctm_spawner_display displayv[4]={0};
  int displayc=ctm_spawner_acquire_displays(displayv);
  if (displayc<1) return 0;

  /* Any display we've found with a living player and too few beasts, spawn a new beast. */
  int i; for (i=0;i<displayc;i++) {
    if (!displayv[i].playeralive) continue;
    if (displayv[i].interior) continue;
    if (displayv[i].beastc>=ctm_spawner.target_population) continue;
    if (ctm_spawner_spawn_in_display(displayv+i,displayv,displayc)<0) return -1;
  }

  /* Check each beast for distance from displays. */
  for (i=0;i<ctm_group_beast.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_beast.sprv[i];
    if (!spr) continue;
    int indisplay=0;
    int j; for (j=0;j<displayc;j++) {
      if (ctm_spawner_display_contains_sprite(displayv+j,spr->x,spr->y)) {
        indisplay=1;
        break;
      }
    }
    if (indisplay) continue;
    if (ctm_sprite_kill_later(spr)<0) return -1;
  }

  return 0;
}

/* Reset/Quit.
 */

int ctm_spawner_reset() {
  ctm_spawner.init=1;
  ctm_spawner.update_timer=0;

  /* Establish target_population based on difficulty. */
  if (ctm_game.difficulty<=1) { // Easy.
    ctm_spawner.target_population=2;
  } else if (ctm_game.difficulty==2) { // Medium.
    ctm_spawner.target_population=3;
  } else if (ctm_game.difficulty==3) { // Hard.
    ctm_spawner.target_population=4;
  } else { // Crazy.
    ctm_spawner.target_population=6;
  }
  
  return 0;
}

void ctm_spawner_quit() {
  memset(&ctm_spawner,0,sizeof(ctm_spawner));
}
