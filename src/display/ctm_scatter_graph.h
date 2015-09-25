/* ctm_scatter_graph.h
 * Helper to produce graphs that show every voter's opinion.
 */

#ifndef CTM_SCATTER_GRAPH_H
#define CTM_SCATTER_GRAPH_H

struct ctm_sprgrp;

/* (grp) should be one of ctm_group_state_voter, or ctm_group_voter.
 * If NULL, it defaults to ctm_group_voter.
 * Ideal heights are 2 more than a multiple of 7.
 * Any width above 5 is fine, the more the merrier.
 */
int ctm_scatter_graph_draw(
  int x,int y,int w,int h,
  struct ctm_sprgrp *grp
);

/* Returns a sensible height close to (prefh) and guaranteed no more than (limit).
 */
int ctm_scatter_graph_choose_height(int prefh,int limith);

#endif
