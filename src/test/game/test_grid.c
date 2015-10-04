#include "test/ctm_test.h"
#include "game/ctm_grid.h"

/* Validate inline "round to cell" functions.
 */

CTM_UNIT_TEST(test_round_to_cell) {
  const int halftile=CTM_TILESIZE>>1;

  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(0),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(halftile-1),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(halftile),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(halftile+1),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(CTM_TILESIZE-1),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(CTM_TILESIZE),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(CTM_TILESIZE+halftile-1),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(CTM_TILESIZE+halftile),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(CTM_TILESIZE+halftile+1),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_to_cell_midpoint(CTM_TILESIZE+CTM_TILESIZE-1),CTM_TILESIZE+halftile,"")

  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(halftile),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(halftile+1),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(CTM_TILESIZE-1),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(CTM_TILESIZE),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(CTM_TILESIZE+halftile-1),halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(CTM_TILESIZE+halftile),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(CTM_TILESIZE+halftile+1),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_down_to_cell_midpoint(CTM_TILESIZE+CTM_TILESIZE-1),CTM_TILESIZE+halftile,"")
  
  CTM_ASSERT_INTS(ctm_grid_round_up_to_cell_midpoint(halftile+1),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_up_to_cell_midpoint(CTM_TILESIZE-1),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_up_to_cell_midpoint(CTM_TILESIZE),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_up_to_cell_midpoint(CTM_TILESIZE+halftile-1),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_up_to_cell_midpoint(CTM_TILESIZE+halftile),CTM_TILESIZE+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_up_to_cell_midpoint(CTM_TILESIZE+halftile+1),CTM_TILESIZE*2+halftile,"")
  CTM_ASSERT_INTS(ctm_grid_round_up_to_cell_midpoint(CTM_TILESIZE+CTM_TILESIZE-1),CTM_TILESIZE*2+halftile,"")

  return 0;
}
