/* ctm_grid.h
 * Defines the world map, a single giant image.
 */

#ifndef CTM_GRID_H
#define CTM_GRID_H

// This only limits the fountains that will be reported by ctm_grid_nearest_fountain().
// You could have thousands of them if you want and they will still work.
// But they won't advertise.
#define CTM_FOUNTAINS_LIMIT 8

struct ctm_grid_cell {
  uint8_t tile;   // Exterior tile. (main)
  uint8_t itile;  // Interior tile.
};

struct ctm_grid {
  int colc,rowc; // Size of grid in cells.
  struct ctm_grid_cell *cellv; // LRTB packed.

  // Cached location of fountain of youths.
  struct ctm_location { int x,y; } fountains[CTM_FOUNTAINS_LIMIT];
};

/* The single master grid.
 * All outside code should treat this 'const'.
 * Mutate only via the functions provided here.
 */
extern struct ctm_grid ctm_grid;

/* One word of bits for each tile ID.
 */
extern uint32_t ctm_tileprop_interior[256];
extern uint32_t ctm_tileprop_exterior[256];
#define CTM_TILEPROP_SOLID      0x00000001 // Impassable, eg rocks.
#define CTM_TILEPROP_WATER      0x00000002 // Same as HOLE, may have extra meaning later.
#define CTM_TILEPROP_POROUS     0x00000004 // Same as SOLID, but if we do a hookshot, it can latch these.
#define CTM_TILEPROP_HOLE       0x00000008 // Impassable but projectiles may cross.
#define CTM_TILEPROP_DOOR       0x00000010 // Impassable to robots. When player touches, it toggles interior/exterior.
#define CTM_TILEPROP_YOUTH      0x00000020 // Fountain of youth.
#define CTM_TILEPROP_RADIO      0xf0000000 // Mask for radio stations.
#define CTM_TILEPROP_MINIBOSS   0x0f000000 // Mask for miniboss zones.

#define CTM_TILEPROP_RADIO_AK      0x10000000
#define CTM_TILEPROP_RADIO_WI      0x20000000
#define CTM_TILEPROP_RADIO_VT      0x30000000
#define CTM_TILEPROP_RADIO_CA      0x40000000
#define CTM_TILEPROP_RADIO_KS      0x50000000
#define CTM_TILEPROP_RADIO_VA      0x60000000
#define CTM_TILEPROP_RADIO_HI      0x70000000
#define CTM_TILEPROP_RADIO_TX      0x80000000
#define CTM_TILEPROP_RADIO_FL      0x90000000
#define CTM_TILEPROP_RADIO_WHITE   0xa0000000
#define CTM_TILEPROP_RADIO_BLACK   0xb0000000
#define CTM_TILEPROP_RADIO_YELLOW  0xc0000000
#define CTM_TILEPROP_RADIO_GREEN   0xd0000000
#define CTM_TILEPROP_RADIO_BLUE    0xe0000000
#define CTM_TILEPROP_RADIO_RED     0xf0000000

#define CTM_TILEPROP_MINIBOSS_AK   0x01000000
#define CTM_TILEPROP_MINIBOSS_WI   0x02000000
#define CTM_TILEPROP_MINIBOSS_VT   0x03000000
#define CTM_TILEPROP_MINIBOSS_CA   0x04000000
#define CTM_TILEPROP_MINIBOSS_KS   0x05000000
#define CTM_TILEPROP_MINIBOSS_VA   0x06000000
#define CTM_TILEPROP_MINIBOSS_HI   0x07000000
#define CTM_TILEPROP_MINIBOSS_TX   0x08000000
#define CTM_TILEPROP_MINIBOSS_FL   0x09000000

int ctm_grid_init();
void ctm_grid_quit();

int ctm_grid_save();

uint32_t ctm_grid_tileprop_for_cell(int col,int row,int interior);
uint32_t ctm_grid_tileprop_for_pixel(int x,int y,int interior);
uint32_t ctm_grid_tileprop_for_rect(int x,int y,int w,int h,int interior); // All bits OR'd.

int ctm_grid_nearest_fountain(int *fountainx,int *fountainy,int qx,int qy);

/* Return a number halfway between multiples of CTM_TILESIZE.
 * "up" and "down" always move in that direction; the regular function goes to the nearest.
 * We will break in the presence of negative inputs.
 */
static inline int ctm_grid_round_to_cell_midpoint(int n) {
  int offset=(n-(CTM_TILESIZE>>1))%CTM_TILESIZE;
  if (offset<(CTM_TILESIZE>>1)) {
    return n-offset;
  } else {
    return n-offset+CTM_TILESIZE;
  }
}
static inline int ctm_grid_round_down_to_cell_midpoint(int n) {
  return ((n-(CTM_TILESIZE>>1))/CTM_TILESIZE)*CTM_TILESIZE+(CTM_TILESIZE>>1);
}
static inline int ctm_grid_round_up_to_cell_midpoint(int n) {
  return ((n-(CTM_TILESIZE>>1)-1)/CTM_TILESIZE+1)*CTM_TILESIZE+(CTM_TILESIZE>>1);
}

#endif
