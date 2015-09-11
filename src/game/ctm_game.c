#include "ctm.h"
#include "ctm_game.h"
#include "ctm_grid.h"
#include "ctm_geography.h"
#include "sprite/ctm_sprite.h"
#include "sprite/types/ctm_sprtype_voter.h"
#include "sprite/types/ctm_sprtype_player.h"
#include "sprite/types/ctm_sprtype_treasure.h"
#include "misc/ctm_time.h"
#include "display/ctm_display.h"
#include "input/ctm_input.h"
#include "audio/ctm_audio.h"
#include <signal.h>

#define CTM_TIMELIMIT_LIMIT 99
#define CTM_STATSTRIGGER_TIME 100 // Frames.
#define CTM_CONSIDER_COUNT 5 // Voters per frame, for idle consideration.
#define CTM_STATE_PREDICTION_TIME 120 // Frames, how often to update state predictions, when a menu is open.
#define CTM_NEWS_MODAL_TIME 40 // Frames
#define CTM_NEWS_AUTODISMISS_TIME 600 // Frames

// If we don't have so many beasts afield, create one. (each frame).
#define CTM_BEAST_PREFERRED_LEVEL 4
#define CTM_BEASTS_PER_PLAYER 4

// How long after game-over before users allowed to return to main menu?
// A decent interval is helpful here, becuase the game could end unexpectedly while a player is pressing buttons.
#define CTM_RESULTS_MANDATORY_TIME 2*1000000

struct ctm_game ctm_game={0};

// A dirty hack that prevents additions to the 'update' group from mucking up our list while iterating over it.
static struct ctm_sprgrp ctm_group_altupdate={0};

/* Init.
 */

int ctm_game_init() {
  memset(&ctm_game,0,sizeof(struct ctm_game));
  ctm_game.phase=-1;
  return 0;
}

/* Quit.
 */

void ctm_game_quit() {

  ctm_sprgrp_kill(&ctm_group_altupdate); 
  if (ctm_group_altupdate.sprv) free(ctm_group_altupdate.sprv);
  memset(&ctm_group_altupdate,0,sizeof(struct ctm_sprgrp));

  if (ctm_game.result) free(ctm_game.result);
  if (ctm_game.werewolf_eventv) free(ctm_game.werewolf_eventv);
  memset(&ctm_game,0,sizeof(struct ctm_game));
}

/* Finish game -- conduct election, choose a winner, yadda yadda.
 */

static int ctm_game_vote() {

  // Conduct election.
  if (!ctm_game.result&&!(ctm_game.result=malloc(sizeof(struct ctm_election_result)))) return -1;
  if (ctm_conduct_election(ctm_game.result)<0) return -1;
  struct ctm_election_result *result=ctm_game.result;

  ctm_audio_play_song(CTM_SONGID_GAMEOVER);
  ctm_audio_effect(CTM_AUDIO_EFFECTID_GAMEOVER,0xff);

  ctm_game.clock_running=0;
  ctm_game.endtime=ctm_get_time(); // Was expiration date, now is timestamp.
  ctm_game.phase=CTM_GAME_PHASE_PREGAMEOVER;
  ctm_game.exitok=0;

  // Assign player awards.
  if (ctm_decide_player_awards()<0) return -1;

  // If a newspaper is visible, dismiss it.
  ctm_display_end_news();

  //if (ctm_display_rebuild_gameover()<0) return -1;
  
  return 0;
}

/* Check how long the newspaper has been visible, and dismiss it automatically after a given duration.
 */

static void ctm_game_dismiss_news_if_stale() {
  ctm_game.news_autodismiss_counter++;
  if (ctm_game.news_autodismiss_counter<CTM_NEWS_AUTODISMISS_TIME) return;
      ctm_audio_effect(CTM_AUDIO_EFFECTID_DISMISS_NEWS,0xff);
  ctm_display_end_news();
  ctm_game.news_counter=0;
}

/* Update, while game is running.
 */

static int ctm_game_update_play() {
  int i;

  /* Update clock, check for completion. */
  int64_t now=ctm_get_time();
  int64_t remaining=ctm_game.endtime-now;
  if (remaining<=0) {
    ctm_game.time_remaining.min=ctm_game.time_remaining.s=ctm_game.time_remaining.cs=0;
    return ctm_game_vote();
  }
  remaining/=10000; // centiseconds
  ctm_game.time_remaining.cs=remaining%100; remaining/=100;
  ctm_game.time_remaining.s=remaining%60; remaining/=60;
  if ((ctm_game.time_remaining.min>0)&&!remaining) ctm_audio_play_song(CTM_SONGID_FINALE); // one minute left!
  ctm_game.time_remaining.min=(remaining>99)?99:remaining;
  ctm_game.play_framec++;

  /* If every player "wants_termination", pretend the clock expired. */
  int terminate=1; for (i=0;i<ctm_group_player.sprc;i++) {
    struct ctm_sprite_player *player=(struct ctm_sprite_player*)(ctm_group_player.sprv[i]);
    if (!player->wants_termination) { terminate=0; break; }
  }
  if (terminate) return ctm_game_vote();

  /* Update stats periodically. */
  if (--(ctm_game.statstrigger)<=0) {
    ctm_game.statstrigger=CTM_STATSTRIGGER_TIME;
    if (ctm_display_game_rebuild_all()<0) return -1;
  }

  /* Update state-by-state predictions periodically, if at least one player is paused. */
  if (--(ctm_game.state_prediction_time)<=0) {
    ctm_game.state_prediction_time=CTM_STATE_PREDICTION_TIME;
    int i; for (i=0;i<ctm_group_player.sprc;i++) {
      if (ctm_group_player.sprv[i]->type!=&ctm_sprtype_player) continue;
      if (((struct ctm_sprite_player*)(ctm_group_player.sprv[i]))->pause) {
        if (ctm_update_state_prediction()<0) return -1;
        break;
      }
    }
  }

  /* Run any scheduled werewolf events. */
  if (ctm_game.news_counter>0) {
    if (!--(ctm_game.news_counter)) ctm_game.news_counter=-1;
    return 0;
  } else if (ctm_game.news_counter==-1) {
    ctm_game_dismiss_news_if_stale();
    for (i=1;i<=4;i++) if (ctm_input_by_playerid[i]&CTM_BTNID_ANYKEY) {
      ctm_game.news_counter=-2;
    }
    return 0;
  } else if (ctm_game.news_counter==-2) {
    ctm_game_dismiss_news_if_stale();
    int zero=1;
    for (i=1;i<4;i++) if (ctm_input_by_playerid[i]&CTM_BTNID_ANYKEY) zero=0;
    if (zero) {
      ctm_audio_effect(CTM_AUDIO_EFFECTID_DISMISS_NEWS,0xff);
      ctm_game.news_counter=0;
      return ctm_display_end_news();
    }
    return 0;
  } else if ((ctm_game.werewolf_eventp<ctm_game.werewolf_eventc)&&(now>=ctm_game.werewolf_eventv[ctm_game.werewolf_eventp].when)) {
    ctm_audio_effect(CTM_AUDIO_EFFECTID_WEREWOLF_EVENT,0xff);
    struct ctm_werewolf_event *event=ctm_game.werewolf_eventv+ctm_game.werewolf_eventp++;
    if (ctm_perform_broad_event(event->stateix,event->party,ctm_game.werewolf_party,event->favor)<0) return -1;
    if (ctm_display_begin_news(event->stateix,event->party)<0) return -1;
    ctm_game.news_counter=CTM_NEWS_MODAL_TIME;
    ctm_game.news_autodismiss_counter=0;
  }

  /* Run idle consideration for voters. */
  if (ctm_group_voter.sprc>0) {
    int considerc=CTM_CONSIDER_COUNT;
    while (considerc-->0) {
      if (ctm_game.considerp>=ctm_group_voter.sprc) ctm_game.considerp=0;
      if (ctm_voter_consider(ctm_group_voter.sprv[ctm_game.considerp])<0) return -1;
      ctm_game.considerp++;
    }
  }

  /* Replenish our beast supply, if we're low. */
  if (ctm_group_beast.sprc<ctm_game.beastc) {
    if (ctm_sprite_spawn_beast()<0) return -1;
  }

  /* Update any sprites that need it. */
  if (ctm_sprgrp_copy(&ctm_group_altupdate,&ctm_group_update)<0) return -1;
  for (i=0;i<ctm_group_altupdate.sprc;i++) {
    struct ctm_sprite *spr=ctm_group_altupdate.sprv[i];
    if (!spr->type->update) continue;
    if (spr->type->update(spr)<0) return -1;
  }
  ctm_sprgrp_clear(&ctm_group_altupdate);

  /* Kill any sprites queued with 'delete_later'. */
  if (ctm_sprgrp_kill(&ctm_group_deathrow)<0) return -1;

  /* Do a single-pass sort of the draw groups (beware, these groups can have thousands of members). */
  if (ctm_sprgrp_semisort(&ctm_group_exterior)<0) return -1;
  if (ctm_sprgrp_semisort(&ctm_group_interior)<0) return -1;
  
  return 0;
}

/* Update at "game over" display.
 */

static int ctm_game_update_pregameover() {
  uint16_t allinput=0;
  int i; for (i=0;i<5;i++) allinput|=ctm_input_by_playerid[i];
  if (ctm_game.exitok) {
    if (allinput&(CTM_BTNID_PRIMARY|CTM_BTNID_SECONDARY|CTM_BTNID_TERTIARY|CTM_BTNID_PAUSE)) {
      ctm_game.exitok=0;
      ctm_game.phase=CTM_GAME_PHASE_GAMEOVER;
      ctm_game.endtime=ctm_get_time();
      if (ctm_display_rebuild_gameover()<0) return -1;
    }
  } else {
    if (allinput&(CTM_BTNID_PRIMARY|CTM_BTNID_SECONDARY|CTM_BTNID_TERTIARY|CTM_BTNID_PAUSE)) return 0; // Input must go dark.
    int64_t now=ctm_get_time();
    if (now<ctm_game.endtime+CTM_RESULTS_MANDATORY_TIME) return 0; // Must wait a decent interval.
    ctm_game.exitok=1; // OK, any input after this point will boink us back to the main menu.
  }
  return 0;
}

static int ctm_game_update_gameover() {
  uint16_t allinput=0;
  int i; for (i=0;i<5;i++) allinput|=ctm_input_by_playerid[i];
  if (ctm_game.exitok) {
    if (allinput&(CTM_BTNID_PRIMARY|CTM_BTNID_SECONDARY|CTM_BTNID_TERTIARY|CTM_BTNID_PAUSE)) return ctm_game_main_menu();
  } else {
    if (allinput&(CTM_BTNID_PRIMARY|CTM_BTNID_SECONDARY|CTM_BTNID_TERTIARY|CTM_BTNID_PAUSE)) return 0; // Input must go dark.
    int64_t now=ctm_get_time();
    if (now<ctm_game.endtime+CTM_RESULTS_MANDATORY_TIME) return 0; // Must wait a decent interval.
    ctm_game.exitok=1; // OK, any input after this point will boink us back to the main menu.
  }
  return 0;
}

/* Activate main menu selection.
 */

static int ctm_game_activate_main_menu() {
  struct ctm_display *display=ctm_display_get_mainmenu();
  if (!display) return -1;
  int err=ctm_display_mainmenu_advance(display);
  if (err<0) return -1;
  ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_COMMIT,0xff);
  if (err) {
    return 0;
  }
  int timelimit,bluec,redc,population,difficulty;
  timelimit=ctm_display_mainmenu_get_timelimit(display);
  ctm_display_mainmenu_get_playerc(&bluec,&redc,display);
  population=ctm_display_mainmenu_get_population(display);
  difficulty=ctm_display_mainmenu_get_difficulty(display);
  if (ctm_game_reset(timelimit,bluec,redc,population,difficulty)<0) return -1;
  return 0;
}

/* Update at main menu.
 */

static int ctm_game_update_mainmenu() {
  struct ctm_display *display=ctm_display_get_mainmenu();
  if (!display) return -1;
  uint16_t allinput=0;
  int i; for (i=0;i<5;i++) allinput|=ctm_input_by_playerid[i];
  if (ctm_game.exitok) {
    if (allinput!=ctm_game.pvinput) { // Input state changed.
      uint16_t n=allinput;
      uint16_t o=ctm_game.pvinput;
      ctm_game.pvinput=n;
      #define PRESS(tag) ((n&CTM_BTNID_##tag)&&!(o&CTM_BTNID_##tag))
      if (PRESS(UP)) {
        ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_CHANGE,0xff);
        if (ctm_display_mainmenu_adjust(display,0,-1)<0) return -1; 
      }
      if (PRESS(DOWN)) {
        ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_CHANGE,0xff);
        if (ctm_display_mainmenu_adjust(display,0,1)<0) return -1;
      }
      if (PRESS(LEFT)) {
        ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_CHANGE,0xff);
        if (ctm_display_mainmenu_adjust(display,-1,0)<0) return -1;
      }
      if (PRESS(RIGHT)) {
        ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_CHANGE,0xff);
        if (ctm_display_mainmenu_adjust(display,1,0)<0) return -1;
      }
      if (PRESS(PRIMARY)||PRESS(SECONDARY)||PRESS(PAUSE)) {
        if (ctm_game_activate_main_menu()<0) return -1;
      } else if (PRESS(TERTIARY)) {
        ctm_audio_effect(CTM_AUDIO_EFFECTID_MENU_RETREAT,0xff);
        if (ctm_display_mainmenu_retreat(display)<0) return -1;
      }
      #undef PRESS
    }
  } else {
    if (allinput) return 0; // Input must go dark.
    ctm_game.exitok=1;
    ctm_game.pvinput=0;
  }
  return 0;
}

/* Update modal dialog box.
 */

static int ctm_game_update_modal(struct ctm_display *modal) {
  uint16_t allinput=0;
  int i; for (i=0;i<5;i++) allinput|=ctm_input_by_playerid[i];
  if (allinput!=ctm_game.pvinput) { // Input state changed.
    uint16_t n=allinput;
    uint16_t o=ctm_game.pvinput;
    ctm_game.pvinput=n;
    #define PRESS(tag) ((n&CTM_BTNID_##tag)&&!(o&CTM_BTNID_##tag))
    if (PRESS(UP)) {
      if (ctm_display_modal_adjust(modal,-1)<0) return -1; 
    }
    if (PRESS(DOWN)) {
      if (ctm_display_modal_adjust(modal,1)<0) return -1;
    }
    if (PRESS(PRIMARY)||PRESS(SECONDARY)||PRESS(TERTIARY)||PRESS(PAUSE)) {
      if (ctm_display_modal_activate(modal)<0) return -1;
    }
    #undef PRESS
  }
  return 0;
}

/* Update.
 */

int ctm_game_update() {

  struct ctm_display *modal=ctm_display_get_modal();
  if (modal) return ctm_game_update_modal(modal);

  switch (ctm_game.phase) {
    case CTM_GAME_PHASE_MAINMENU: return ctm_game_update_mainmenu();
    case CTM_GAME_PHASE_PLAY: return ctm_game_update_play();
    case CTM_GAME_PHASE_PREGAMEOVER: return ctm_game_update_pregameover();
    case CTM_GAME_PHASE_GAMEOVER: return ctm_game_update_gameover();
    default: return ctm_game_main_menu();
  }
}

/* Enter main menu.
 */

int ctm_game_main_menu() {
  if (ctm_game.phase==CTM_GAME_PHASE_MAINMENU) return 0;
  ctm_audio_play_song(CTM_SONGID_MAINMENU);
  ctm_game.phase=CTM_GAME_PHASE_MAINMENU;
  ctm_game.exitok=0;

  if (ctm_display_rebuild_mainmenu()<0) return -1;
  if (ctm_input_set_player_count(4)<0) return -1;
  ctm_game.pvinput=0;

  struct ctm_display *display=ctm_display_get_mainmenu();
  if (display) {
    if (ctm_game.playerc) ctm_display_mainmenu_set_playerc(display,ctm_game.playerc,ctm_game.bluec,ctm_game.redc);
    if (ctm_game.timelimit_minutes) ctm_display_mainmenu_set_timelimit(display,ctm_game.timelimit_minutes);
    if (ctm_game.population) ctm_display_mainmenu_set_population(display,ctm_game.population);
    if (ctm_game.difficulty) ctm_display_mainmenu_set_difficulty(display,ctm_game.difficulty);
  }
  ctm_game.playerc=ctm_game.bluec=ctm_game.redc=0;
  ctm_game.timelimit_minutes=0;
  ctm_game.population=0;
  ctm_game.difficulty=0;

  return 0;
}

/* User requests quit, eg by pressing ESC.
 */

static int ctm_game_abortcb_resume(void *userdata) {
  return 0;
}

static int ctm_game_abortcb_menu(void *userdata) {
  if (ctm_game_main_menu()<0) return -1;
  return 0;
}

static int ctm_game_abortcb_quit(void *userdata) {
  raise(SIGTERM);
  return 0;
}
 
int ctm_game_user_quit() {
  switch (ctm_game.phase) {

    // Play modes: Show or dismiss the Abort modal.
    case CTM_GAME_PHASE_PLAY:
    case CTM_GAME_PHASE_PREGAMEOVER: {
        struct ctm_display *modal=ctm_display_get_modal();
        if (modal) {
          ctm_display_end_modal();
        } else {
          if (!(modal=ctm_display_begin_modal())) return -1;
          if (ctm_display_modal_add_option(modal,"Resume",6,ctm_game_abortcb_resume,0)<0) return -1;
          if (ctm_display_modal_add_option(modal,"Main Menu",9,ctm_game_abortcb_menu,0)<0) return -1;
          if (ctm_display_modal_add_option(modal,"Quit",4,ctm_game_abortcb_quit,0)<0) return -1;
        }
      } break;

    // MAINMENU, GAMEOVER, anything else: Quit immediately.
    default: { 
        raise(SIGTERM);
      }
  }
  return 0;
}

/* Create voter.
 * Returns weak reference on success or NULL on error.
 */

struct ctm_sprite *ctm_create_voter(int x,int y,int interior,int party,int stateix) {
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_voter);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  spr->x=x;
  spr->y=y;
  if (interior) {
    spr->interior=1;
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) { ctm_sprite_kill(spr); return 0; }
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) { ctm_sprite_kill(spr); return 0; }
  }
  if (ctm_sprite_add_group(spr,&ctm_group_voter)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,ctm_group_state_voter+stateix)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_voter_setup(spr,0,party)<0) { ctm_sprite_kill(spr); return 0; }
  return spr;
}

/* Create player.
 */

struct ctm_sprite *ctm_create_player(int stateix,int playerid) {
  int x,y,interior;
  ctm_choose_vacant_location(&x,&y,&interior,stateix,0);
  struct ctm_sprite *spr=ctm_sprite_alloc(&ctm_sprtype_player);
  if (!spr) return 0;
  if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return 0; }
  ctm_sprite_del(spr);
  ((struct ctm_sprite_player*)spr)->playerid=playerid;
  spr->x=x;
  spr->y=y;
  if (interior) {
    spr->interior=1;
    if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) { ctm_sprite_kill(spr); return 0; }
  } else {
    if (ctm_sprite_add_group(spr,&ctm_group_exterior)<0) { ctm_sprite_kill(spr); return 0; }
  }
  if (ctm_sprite_add_group(spr,&ctm_group_player)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_update)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_hookable)<0) { ctm_sprite_kill(spr); return 0; }
  if (ctm_sprite_add_group(spr,&ctm_group_bonkable)<0) { ctm_sprite_kill(spr); return 0; }
  return spr;
}

/* Create miniboss.
 */

static int ctm_create_miniboss(int stateix) {
  if ((stateix<0)||(stateix>=9)) return -1;
  const struct ctm_state_data *state=ctm_state_data+stateix;
  uint32_t prop=(stateix+1)<<24; // There must be a contiguous region of interior tiles with this property; that's where we put the miniboss.
  const struct ctm_grid_cell *cell=ctm_grid.cellv;
  int row; for (row=0;row<ctm_grid.rowc;row++) {
    int col; for (col=0;col<ctm_grid.colc;col++,cell++) {
      if (ctm_tileprop_interior[cell->itile]!=prop) continue;
      const struct ctm_sprtype *type=0; // It would be nice to dump these in ctm_state_data, but it might violate compiler constraints. Whatever.
      switch (stateix) {
        case CTM_STATE_ALASKA: type=&ctm_sprtype_santa; break;
        case CTM_STATE_WISCONSIN: type=&ctm_sprtype_robot; break;
        case CTM_STATE_VERMONT: type=&ctm_sprtype_cthulhu; break;
        case CTM_STATE_CALIFORNIA: type=&ctm_sprtype_vampire; break;
        case CTM_STATE_KANSAS: type=&ctm_sprtype_buffalo; break;
        case CTM_STATE_VIRGINIA: type=&ctm_sprtype_crab; break;
        case CTM_STATE_HAWAII: type=&ctm_sprtype_pineapple; break;
        case CTM_STATE_TEXAS: type=&ctm_sprtype_coyote; break;
        case CTM_STATE_FLORIDA: type=&ctm_sprtype_alligator; break;
      }
      if (!type) return 0;

      struct ctm_sprite *spr=ctm_sprite_alloc(type);
      if (!spr) return -1;
      if (ctm_sprite_add_group(spr,&ctm_group_keepalive)<0) { ctm_sprite_del(spr); return -1; }
      ctm_sprite_del(spr);
      if (ctm_sprite_add_group(spr,&ctm_group_interior)<0) return -1;
      if (ctm_sprite_add_group(spr,&ctm_group_fragile)<0) return -1;
      if (ctm_sprite_add_group(spr,&ctm_group_hazard)<0) return -1;
      if (ctm_sprite_add_group(spr,&ctm_group_update)<0) return -1;
      spr->x=col*CTM_TILESIZE+(CTM_TILESIZE>>1);
      spr->y=row*CTM_TILESIZE+(CTM_TILESIZE>>1);
      spr->interior=1;
      
      return 0;
    }
  }
  return 0;
}

/* Generate a random schedule for the Werewolf.
 * ----XXX---- This is where we adjust the difficulty levels. ----XXX----
 */

static int ctm_game_schedule_werewolf() {
  ctm_game.werewolf_eventc=0;
  ctm_game.werewolf_eventp=0;
  if (ctm_game.werewolf_eventv) { free(ctm_game.werewolf_eventv); ctm_game.werewolf_eventv=0; }

  // How many events?
  int secondc=ctm_game.timelimit_minutes*60;
  switch (ctm_game.difficulty) {
    case 1: ctm_game.werewolf_eventc=secondc/90; break;
    case 2: ctm_game.werewolf_eventc=secondc/70; break;
    case 3: ctm_game.werewolf_eventc=secondc/50; break;
    case 4: ctm_game.werewolf_eventc=secondc/30; break;
    default: return 0;
  }
  if (!(ctm_game.werewolf_eventv=calloc(sizeof(struct ctm_werewolf_event),ctm_game.werewolf_eventc))) return -1;

  // Establish trigger times, and default each event to one-state-one-party.
  int64_t duration=ctm_game.endtime-ctm_game.starttime;
  int i; for (i=0;i<ctm_game.werewolf_eventc;i++) {
    ctm_game.werewolf_eventv[i].when=ctm_game.starttime+((i+1)*duration)/(ctm_game.werewolf_eventc+1);
    int fudge=rand()%10000000-5000000; // Don't schedule them precisely; fudge it by up to 5 seconds.
    ctm_game.werewolf_eventv[i].when+=fudge;
    ctm_game.werewolf_eventv[i].stateix=rand()%9;
    ctm_game.werewolf_eventv[i].party=1+rand()%6;
    ctm_game.werewolf_eventv[i].favor=10; // 6 or 8 if it focusses.
  }

  // Convert some events to 'nationwide' and 'everybody', based on difficulty.
  // We don't check whether those two phenomena converge, even though it does make a difference.
  int nationwidec=0,everybodyc=0;
  switch (ctm_game.difficulty) {
    case 1: {
        nationwidec=(ctm_game.werewolf_eventc*20)/100;
        everybodyc= (ctm_game.werewolf_eventc*20)/100;
      } break;
    case 2: {
        nationwidec=(ctm_game.werewolf_eventc*30)/100;
        everybodyc= (ctm_game.werewolf_eventc*40)/100;
      } break;
    case 3: {
        nationwidec=(ctm_game.werewolf_eventc*40)/100;
        everybodyc= (ctm_game.werewolf_eventc*50)/100;
      } break;
    case 4: {
        nationwidec=(ctm_game.werewolf_eventc*60)/100;
        everybodyc= (ctm_game.werewolf_eventc*60)/100;
      } break;
  }

  /* Data scrub: Do not generate events for White or Black parties.
   * "Werewolf rescues white baby" and "Free hot dogs for blacks" sound kind of dicey.
   * These drop the 'everybody' counter, possibly below zero.
   * That may impact the precision of difficulty settings, but fuck it, it's not brain surgery.
   */
  for (i=0;i<ctm_game.werewolf_eventc;i++) {
    switch (ctm_game.werewolf_eventv[i].party) {
      case CTM_PARTY_WHITE:
      case CTM_PARTY_BLACK: {
        ctm_game.werewolf_eventv[i].party=-1;
        everybodyc--;
      }
    }
  }

  /* Proceed with expanding event ranges due to difficulty level. */
  while (nationwidec-->0) {
    while (1) {
      int p=rand()%ctm_game.werewolf_eventc;
      if (ctm_game.werewolf_eventv[p].stateix<0) continue;
      ctm_game.werewolf_eventv[p].stateix=-1;
      ctm_game.werewolf_eventv[p].favor-=2;
      break;
    }
  }
  while (everybodyc-->0) {
    while (1) {
      int p=rand()%ctm_game.werewolf_eventc;
      if (ctm_game.werewolf_eventv[p].party<1) continue;
      ctm_game.werewolf_eventv[p].party=-1;
      ctm_game.werewolf_eventv[p].favor-=2;
      break;
    }
  }
  
  return 0;
}

/* Create treasure.
 */
 
static int ctm_game_create_treasure(int count,int value) {
  while (count-->0) {
    if (rand()%100<70) ctm_treasure_new_random(CTM_ITEM_COIN,value);
    else ctm_treasure_new_random(CTM_ITEM_SPEECH,5);
  }
  return 0;
}

/* Reset.
 */

int ctm_game_reset(int timelimit_minutes,int playerc_blue,int playerc_red,int population,int difficulty) {

  ctm_audio_play_song(CTM_SONGID_PLAY);

  /* Clamp request. */
  if (timelimit_minutes<1) timelimit_minutes=1;
  else if (timelimit_minutes>CTM_TIMELIMIT_LIMIT) timelimit_minutes=CTM_TIMELIMIT_LIMIT;
  if (playerc_blue<0) playerc_blue=0; else if (playerc_blue>4) playerc_blue=4;
  if (playerc_red<0) playerc_red=0; else if (playerc_red>4) playerc_red=4;
  if (playerc_blue+playerc_red>4) playerc_red=4-playerc_blue;
  int playerc=playerc_blue+playerc_red;
  if (population<100) population=100;
  else if (population>100000) population=100000;
  if (difficulty<1) difficulty=1; else if (difficulty>4) difficulty=4;

  /* Kill all sprites. */
  ctm_sprgrp_kill(&ctm_group_keepalive);

  /* Generate the electorate. */
  int popv[9];
  if (ctm_apportion_population(popv,population)<0) return -1;
  int stateix; for (stateix=0;stateix<9;stateix++) {
    const struct ctm_state_data *state=ctm_state_data+stateix;
    int interior,x,y,i;
    for (i=0;i<popv[stateix];i++) {
      ctm_choose_vacant_location(&x,&y,&interior,stateix,1);
      int party;
      if (/*state->main_party&&*/(rand()&1)) party=state->main_party;
      else party=rand()%7;
      struct ctm_sprite *spr=ctm_create_voter(x,y,interior,party,stateix);
      if (!spr) return -1;
    }
  }

  /* Generate player sprites. */
  int playerid=1,i;
  for (i=0;i<playerc_blue;i++,playerid++) {
    struct ctm_sprite *spr=ctm_create_player(CTM_STATE_HAWAII,playerid);
    if (!spr) return 0;
    ((struct ctm_sprite_player*)spr)->party=CTM_PARTY_BLUE;
  }
  for (i=0;i<playerc_red;i++,playerid++) {
    struct ctm_sprite *spr=ctm_create_player(CTM_STATE_VIRGINIA,playerid);
    if (!spr) return 0;
    ((struct ctm_sprite_player*)spr)->party=CTM_PARTY_RED;
  }

  /* Locate and initialize minibosses -- They live forever, whether you visit them or not. */
  for (stateix=0;stateix<9;stateix++) {
    if (ctm_create_miniboss(stateix)<0) return -1;
  }
  
  /* Create treasure. Easier settings have more and richer treasures. */
  if (playerc_blue&&playerc_red) {
    if (ctm_game_create_treasure(30,10)<0) return -1;
  } else switch (difficulty) {
    case 1: if (ctm_game_create_treasure(40,15)<0) return -1; break;
    case 2: if (ctm_game_create_treasure(30,10)<0) return -1; break;
    case 3: if (ctm_game_create_treasure(20,10)<0) return -1; break;
    case 4: if (ctm_game_create_treasure(10,10)<0) return -1; break;
  }

  /* Copy a few simple things. */
  if (ctm_game.result) free(ctm_game.result);
  if (ctm_game.werewolf_eventv) free(ctm_game.werewolf_eventv);
  memset(&ctm_game,0,sizeof(struct ctm_game));
  ctm_game.timelimit_minutes=timelimit_minutes;
  ctm_game.playerc=playerc;
  ctm_game.bluec=playerc_blue;
  ctm_game.redc=playerc_red;
  ctm_game.starttime=ctm_get_time();
  ctm_game.endtime=ctm_game.starttime+timelimit_minutes*60*1000000ll;
  ctm_game.clock_running=1;
  ctm_game.play_framec=0;
  ctm_game.phase=CTM_GAME_PHASE_PLAY;
  ctm_game.exitok=0;
  ctm_game.beastc=CTM_BEAST_PREFERRED_LEVEL+CTM_BEASTS_PER_PLAYER*playerc;
  ctm_game.difficulty=difficulty;
  ctm_game.population=population;

  /* If all players belong to the same party, prepare the Werewolf's agenda. */
  if (!ctm_game.bluec||!ctm_game.redc) {
    ctm_game.werewolf_party=ctm_game.bluec?CTM_PARTY_RED:CTM_PARTY_BLUE;
    if (ctm_game_schedule_werewolf()<0) return -1;
  }

  /* Reassign input devices and create displays. */
  if (ctm_input_set_player_count(ctm_game.playerc)<0) return -1;
  if (ctm_display_rebuild_game(ctm_game.playerc)<0) return -1;

  return 0;
}
