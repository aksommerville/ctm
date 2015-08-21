#include "test/ctm_test.h"
#include "game/ctm_grid.h"
#include "game/ctm_geography.h"

/* There are a few cells on the world map which are unreachable.
 * This test confirms that even though they look normal, they are in fact unpassable.
 * !!! This test is dependant on asset data. !!!
 */

CTM_UNIT_TEST(test_outer_banks_impassable) {
  CTM_ASSERT_CALL(ctm_grid_init(),"")

  /* Outer Banks */
  CTM_ASSERT_NOT(ctm_location_is_vacant(232,158,0),"")
  CTM_ASSERT_NOT(ctm_location_is_vacant(234,155,0),"")
  CTM_ASSERT_NOT(ctm_location_is_vacant(234,154,0),"")
  CTM_ASSERT_NOT(ctm_location_is_vacant(235,154,0),"")
  CTM_ASSERT_NOT(ctm_location_is_vacant(235,153,0),"")
  CTM_ASSERT_NOT(ctm_location_is_vacant(235,152,0),"")

  /* San Jose y=96,97 x=15,16 */
  CTM_ASSERT_NOT(ctm_location_is_vacant(15,97,0),"")
  CTM_ASSERT_NOT(ctm_location_is_vacant(16,96,0),"")
  
  ctm_grid_quit();
  return 0;
}
