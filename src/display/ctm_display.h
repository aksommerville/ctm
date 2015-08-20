/* ctm_display.h
 * Generic display object, global store of active displays, and public API for individual display classes.
 * A "display" is a region of the screen with its own rendering logic.
 */

#ifndef CTM_DISPLAY_H
#define CTM_DISPLAY_H

struct ctm_sprgrp;

/* --- Generic display object. ---
 */

struct ctm_display;

struct ctm_display_type {
  const char *name;
  int objlen;
  void (*del)(struct ctm_display *display);
  int (*init)(struct ctm_display *display);
  int (*resized)(struct ctm_display *display); // (x,y) can change without notification.
  int (*draw_fb)(struct ctm_display *display); // Called first every frame, with GL targetting framebuffer.
  int (*draw)(struct ctm_display *display); // Called every frame, with GL targetting main output.
};

struct ctm_display {

  // Required. Set at construction and never changes.
  const struct ctm_display_type *type;

  // Position on screen.
  // Global manager may change, to others it is read only.
  // Object is notified of changes to (w,h) but not to (x,y).
  int x,y,w,h;

  // If type implements 'draw', this flag tells the manager that the screen behind this display need not be cleared.
  // If you don't implement 'draw', this is ignored.
  // Leave zero if unsure.
  int opaque;

  // For convenience, we provide managed framebuffer per display.
  // Set this up with ctm_display_resize_fb().
  // Manager draws this automatically if you don't implement 'draw'.
  uint32_t fbtexid;
  uint32_t fb;
  int fbw,fbh;
  uint8_t fbalpha;
  
};

struct ctm_display *ctm_display_alloc(const struct ctm_display_type *type);
void ctm_display_del(struct ctm_display *display);

int ctm_display_resize_fb(struct ctm_display *display,int fbw,int fbh);
int ctm_display_drop_fb(struct ctm_display *display); // Don't know why you'd need this, but let's be complete.

// Only the global display manager should do this:
int ctm_display_set_bounds(struct ctm_display *display,int x,int y,int w,int h);

/* --- Global display store. ---
 */

/* First and last calls you must make.
 * 'video' must be initialized at all times when display is alive.
 * We do not create an initial display set at init, so other init sequencing is not important.
 */
int ctm_display_init();
void ctm_display_quit();

/* Update framebuffers as necessary, draw everything, commit one frame of video, and block until VSync.
 * This is kind of a big deal.
 */
int ctm_display_draw();

// Alert displays of new screen size.
int ctm_display_resized();

/* Destroy all existing displays and build a new set from scratch.
 * Only the 'game' or 'editor' units should touch these.
 */
int ctm_display_rebuild_mainmenu();
int ctm_display_rebuild_gameover();
int ctm_display_rebuild_game(int playerc);
int ctm_display_rebuild_editor();

/* We do a special effect of a spinning newspaper with a message about the Werewolf's latest wacky adventure.
 * All you have to do is call the one to begin it, and the other to dismiss it.
 * You tell us the relevant state and party (out of range for "all"), and we make up the story.
 */
int ctm_display_begin_news(int stateix,int party);
int ctm_display_end_news();

/* Yoink the most recent framebuffer of the first installed display.
 * Returns image size in bytes, or <0 on error.
 * On success, (rgbapp) points to a new image which the caller must free.
 * In mainmenu or gameover states, this is the whole screen.
 * In play state, this is player one's screen. You can't screenshot the players' composite.
 */
int ctm_display_take_screenshot(void *rgbapp,int *w,int *h);

/* --- All display types. ---
 */

extern const struct ctm_display_type ctm_display_type_game;
extern const struct ctm_display_type ctm_display_type_mainmenu;
extern const struct ctm_display_type ctm_display_type_gameover;
extern const struct ctm_display_type ctm_display_type_editor;
extern const struct ctm_display_type ctm_display_type_filler;

/* --- Player views. ---
 */

int ctm_display_game_set_playerid(struct ctm_display *display,int playerid);
int ctm_display_game_get_playerid(const struct ctm_display *display);
int ctm_display_game_get_position(int *x,int *y,int *w,int *h,int *interior,const struct ctm_display *display);

// Returns NULL or a display of type 'game'.
struct ctm_display *ctm_display_for_playerid(int playerid);

// Get the size in world pixels of a player's dedicated display.
int ctm_display_player_get_size(int *w,int *h,int playerid);

int ctm_display_game_move_cursor(struct ctm_display *display,int dx,int dy);
int ctm_display_game_get_selected_index(const struct ctm_display *display);

// Indices 0..16 are slots in the relevant item store.
#define CTM_DISPLAY_GAME_INDEX_VOTE     16 // "Vote now".

// Game displays carry an optional poll report, displayed in the pause menu.
int ctm_display_game_rebuild_report(struct ctm_display *display);
int ctm_display_game_rebuild_all();

/* Test how far (x,y) is from any player's display.
 * Returns <0 if there is no relevant display (due to interior, or no player displays exist).
 * Returns 0 if this point is within the display, otherwise the manhattan distance to its edge.
 * If (playerid) not NULL, we fill with the owner of the display in question.
 */
int ctm_display_distance_to_player_screen(int *playerid,int x,int y,int interior);

/* --- Editor views. ---
 */

struct ctm_display *ctm_display_get_editor();

int ctm_display_editor_get_interior(const struct ctm_display *display);
int ctm_display_editor_set_interior(struct ctm_display *display,int interior);
static inline int ctm_display_editor_toggle_interior() { return ctm_display_editor_set_interior(ctm_display_get_editor(),-1); }

int ctm_display_editor_set_selection(struct ctm_display *display,int col,int row);

// (x,y) in output space, ie not yet translated to display space.
int ctm_display_editor_cell_from_coords(int *col,int *row,const struct ctm_display *display,int x,int y);

// Adjust scroll, in world pixels.
int ctm_display_editor_scroll(struct ctm_display *display,int dx,int dy);

/* --- Main menu. ---
 */

#define CTM_MAINMENU_PHASE_PLAYERC    1 // Choosing player count, and red/blue split.
#define CTM_MAINMENU_PHASE_TIMELIMIT  2 // Choosing time limit.
#define CTM_MAINMENU_PHASE_POPULATION 3 // Choosing voter count.
#define CTM_MAINMENU_PHASE_DIFFICULTY 4 // Choosing difficulty (for single-party games).

struct ctm_display *ctm_display_get_mainmenu();

/* A mainmenu display is always pointed at one of the phases above, and always has sane values for each config field.
 * You can walk through the fields logically with 'advance' and 'retreat'.
 * These return >0 if a new request is now visible, or 0 at the end of the line.
 * (So, if you 'advance' and get a zero, you should begin the game).
 */
int ctm_display_mainmenu_get_phase(const struct ctm_display *display);
int ctm_display_mainmenu_advance(struct ctm_display *display);
int ctm_display_mainmenu_retreat(struct ctm_display *display);
int ctm_display_mainmenu_reset(struct ctm_display *display);

/* These fields always have sensible values, even if the user hasn't seen them yet.
 * 'playerc' also returns the blue/red split, which is queried at the same time.
 * 'timelimit' is in minutes.
 * 'population' is in voters.
 * 'difficulty' is (1,2,3) == (easy,medium,hard).
 * These return the default if you provide a bad display, so results are always sensible.
 */
int ctm_display_mainmenu_get_playerc(int *bluec,int *redc,const struct ctm_display *display);
int ctm_display_mainmenu_get_timelimit(const struct ctm_display *display);
int ctm_display_mainmenu_get_population(const struct ctm_display *display);
int ctm_display_mainmenu_get_difficulty(const struct ctm_display *display);

// Everything clamps to the nearest legal value. Use these, eg, to set defaults at the second menu instance...?
int ctm_display_mainmenu_set_playerc(struct ctm_display *display,int playerc,int bluec,int redc);
int ctm_display_mainmenu_set_timelimit(struct ctm_display *display,int timelimit);
int ctm_display_mainmenu_set_population(struct ctm_display *display,int population);
int ctm_display_mainmenu_set_difficulty(struct ctm_display *display,int difficulty);

int ctm_display_mainmenu_adjust(struct ctm_display *display,int dx,int dy);

/* --- Game over. ---
 */

struct ctm_display *ctm_display_get_gameover();

/* --- Helpers for display implementation. ---
 */

// Fill display's framebuffer with the grid. (scrollx,scrolly) is the world pixel of top-left corner.
int ctm_display_draw_grid(struct ctm_display *display,int scrollx,int scrolly,int interior);

// Draw all sprites in group into display's framebuffer.
int ctm_display_draw_sprites(struct ctm_display *display,int scrollx,int scrolly,const struct ctm_sprgrp *group,int lens_of_truth);

#endif
