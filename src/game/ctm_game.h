/* ctm_game.h
 * High-level coordinator for game logic.
 */

#ifndef CTM_GAME_H
#define CTM_GAME_H

#define CTM_GAME_PHASE_MAINMENU      1
#define CTM_GAME_PHASE_PLAY          2
#define CTM_GAME_PHASE_GAMEOVER      3
#define CTM_GAME_PHASE_PREGAMEOVER   4

// Marks a preordained time when the imaginary werewolf will do something newsworthy.
struct ctm_werewolf_event {
  int64_t when;
  int stateix;
  int party;
  int favor;
};

// Read only.
extern struct ctm_game {

  int playerc;

  int clock_running; // Nonzero until clock expires.
  int64_t starttime,endtime;
  struct { uint8_t min,s,cs; } time_remaining; // Repopulated each update.
  int statstrigger; // Counts down until rebuilding stats.
  int considerp;
  int state_prediction_time;
  int64_t play_framec;

  void *result; // struct ctm_election_result, or NULL
  int exitok; // Nonzero during results display, after some time has passed and all inputs went dark.
  int phase;
  uint16_t pvinput; // Used only during main menu.

  int bluec,redc;
  int timelimit_minutes;
  int population;
  int difficulty;

  int news_counter; // While counting down, input is disabled. If negative, the news is still displayed
  int news_autodismiss_counter; // Counts up while news is dismissable; drops paper after a time.
  struct ctm_werewolf_event *werewolf_eventv;
  int werewolf_eventc;
  int werewolf_eventp; // How many werewolf events completed?
  int werewolf_party;
  
} ctm_game;

/* Public API.
 */

int ctm_game_init();
void ctm_game_quit();
int ctm_game_update();

/* Tear down and rebuild transient data.
 * No sprite survives the transition.
 * Ready to play after completion.
 *   timelimit_minutes: Game duration from this moment.
 *   playerc_(blue,red): Count of players in each party. Sum must be in (1,2,3,4).
 *   population: Total count of voters nationwide.
 */
int ctm_game_reset(int timelimit_minutes,int playerc_blue,int playerc_red,int population,int difficulty);

int ctm_game_main_menu();
int ctm_game_user_quit();

#endif
