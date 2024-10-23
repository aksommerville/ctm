/* ctm_audio.h
 * Generic audio wrapper.
 * We manage the loading and triggering of sound effects and background music.
 * We also implement a software mixer.
 * We depend on a backend -- MMAL, ALSA, or SDL -- to provide us a callback-based output thread.
 * Audio is not mission-critical, so we can compile without it.
 * This unit manages the stubbery in that case.
 *
 * Backend must provide a single-channel 44.1 kHz stream, signed 16-bit, native byte order.
 * If the hardware can't manage that, the backend must fake it somehow.
 *
 */

#ifndef CTM_AUDIO_H
#define CTM_AUDIO_H

int ctm_audio_init(const char *device);
void ctm_audio_quit();
int ctm_audio_update();

// Nonzero if an audio backend was compiled into CTM.
// If zero, the entire audio unit is a dummy.
// Always returns the same thing.
int ctm_audio_available();

// Set background music. Song zero is always silence.
// Setting to the current song does nothing (in particular, it does not restart the song).
int ctm_audio_play_song(int songid);

#define CTM_SONGID_MAINMENU     1
#define CTM_SONGID_PLAY         2
#define CTM_SONGID_GAMEOVER     3
#define CTM_SONGID_FINALE       4

int ctm_audio_effect(int effectid,uint8_t level);

#define CTM_AUDIO_EFFECTID_LENS_BEGIN       1
#define CTM_AUDIO_EFFECTID_TREASURE         2
#define CTM_AUDIO_EFFECTID_REFUSE           3
#define CTM_AUDIO_EFFECTID_REMOVE           4
#define CTM_AUDIO_EFFECTID_DOOR             5
#define CTM_AUDIO_EFFECTID_PLAYER_HURT      6
#define CTM_AUDIO_EFFECTID_PRIZE            7
#define CTM_AUDIO_EFFECTID_FLAME_BEGIN      8
#define CTM_AUDIO_EFFECTID_HOOKSHOT_BEGIN   9
#define CTM_AUDIO_EFFECTID_BONK            10
#define CTM_AUDIO_EFFECTID_ARROW           11
#define CTM_AUDIO_EFFECTID_WEREWOLF_EVENT  12
#define CTM_AUDIO_EFFECTID_MONSTER_HURT    13
#define CTM_AUDIO_EFFECTID_MONSTER_KILLED  14
// 15 unused
#define CTM_AUDIO_EFFECTID_HOOKSHOT_GRAB   16
#define CTM_AUDIO_EFFECTID_PLAYER_KILLED   17
#define CTM_AUDIO_EFFECTID_RADIO_SPEECH    18
#define CTM_AUDIO_EFFECTID_WAND            19
#define CTM_AUDIO_EFFECTID_MENU_CHANGE     20
#define CTM_AUDIO_EFFECTID_BOOMERANG_BEGIN 21
// 22 unused
// 23 unused
#define CTM_AUDIO_EFFECTID_BLADE           24
#define CTM_AUDIO_EFFECTID_GUN             25
#define CTM_AUDIO_EFFECTID_SWORD           26
// 27 unused
// 28 unused
#define CTM_AUDIO_EFFECTID_MENU_RETREAT    29
#define CTM_AUDIO_EFFECTID_SPEECH          30
// 31 unused
// 32 unused
// 33 unused
#define CTM_AUDIO_EFFECTID_KISSOFDEATH     34
#define CTM_AUDIO_EFFECTID_MENU_COMMIT     35

#define CTM_AUDIO_EFFECTID_WANTED          CTM_AUDIO_EFFECTID_WEREWOLF_EVENT
#define CTM_AUDIO_EFFECTID_FIREBALL        CTM_AUDIO_EFFECTID_WAND
#define CTM_AUDIO_EFFECTID_GAMEOVER        0
#define CTM_AUDIO_EFFECTID_BIGACTIVATE     CTM_AUDIO_EFFECTID_TREASURE // unused?
#define CTM_AUDIO_EFFECTID_HEAL            CTM_AUDIO_EFFECTID_PRIZE
#define CTM_AUDIO_EFFECTID_ACTIVATE        0
#define CTM_AUDIO_EFFECTID_DISMISS_NEWS    CTM_AUDIO_EFFECTID_REMOVE
#define CTM_AUDIO_EFFECTID_BOSS_HURT       CTM_AUDIO_EFFECTID_MONSTER_HURT
#define CTM_AUDIO_EFFECTID_BOSS_KILLED     CTM_AUDIO_EFFECTID_MONSTER_KILLED
#define CTM_AUDIO_EFFECTID_THROW_COIN      CTM_AUDIO_EFFECTID_ARROW
#define CTM_AUDIO_EFFECTID_HOOKSHOT_CANCEL 0
#define CTM_AUDIO_EFFECTID_HOOKSHOT_SMACK  0
#define CTM_AUDIO_EFFECTID_NO_BONK         CTM_AUDIO_EFFECTID_SWORD
#define CTM_AUDIO_EFFECTID_LENS_END        0
#define CTM_AUDIO_EFFECTID_SPEND           0
#define CTM_AUDIO_EFFECTID_SWAPITEM        CTM_AUDIO_EFFECTID_MENU_COMMIT
#define CTM_AUDIO_EFFECTID_PAUSE           CTM_AUDIO_EFFECTID_MENU_RETREAT
#define CTM_AUDIO_EFFECTID_UNPAUSE         CTM_AUDIO_EFFECTID_MENU_RETREAT
#define CTM_AUDIO_EFFECTID_TRYVOTE         CTM_AUDIO_EFFECTID_MENU_COMMIT
#define CTM_AUDIO_EFFECTID_UNTRYVOTE       CTM_AUDIO_EFFECTID_MENU_RETREAT

#define CTM_SFX(tag) ctm_audio_effect(CTM_AUDIO_EFFECTID_##tag,0xff);

#endif
