/* ctm_spawner.h
 * API for managing the beasts' lifecycle.
 * Basically, we create a new beast just outside a player's view every so often.
 * Then, when a beast is offscreen for a certain amount of time, we drop it.
 * This logic was formerly scattered between ctm_game and ctm_sprtype_beast.
 * It is subtle and tricky enough that I feel it warrants a distinct interface.
 */

#ifndef CTM_SPAWNER_H
#define CTM_SPAWNER_H

/* Forget everything we know; prepare to start fresh.
 * This must be called before the first update().
 * Call again any time you need to flush the state (eg restart game).
 * All spawner state is private.
 */
int ctm_spawner_reset();

/* Clean up all private state.
 */
void ctm_spawner_quit();

/* Call every frame.
 */
int ctm_spawner_update();

#endif
