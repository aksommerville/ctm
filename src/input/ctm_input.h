/* ctm_input.h
 */

#ifndef CTM_INPUT_H
#define CTM_INPUT_H

// One bitfield for each possible player, and a dummy at [0].
extern uint16_t ctm_input_by_playerid[5];

// Populated if you request "editor" input at startup.
extern struct ctm_editor_input {
  int enabled;
  int ptrx,ptry;
  int ptrleft,ptrright;
  int ptrwheelx,ptrwheely; // Receiver should clear.
} ctm_editor_input;

/* Must initialize ctm_poll and video before initializing input.
 * We are responsible for initializing any input-specific subsystems (eg evdev).
 * Update doesn't perform any I/O.
 */
int ctm_input_init(int is_editor);
void ctm_input_quit();
int ctm_input_update();

int ctm_input_set_player_count(int playerc);

/* Fire a signal or toggle a bit.
 * For bit events:
 *   - Exactly one bit in (btnid) must be set.
 *   - Value is boolean.
 *   - (devid) must name a registered device.
 * For signal events:
 *   - (devid) may be used but is not required.
 *   - (value) must be nonzero, otherwise we ignore.
 */
int ctm_input_event(int devid,uint16_t btnid,int value);

/* Registration creates a record in our store and allocates a new Device ID.
 * We also automap it, assigning the least-used playerid up to the previously-asserted player count.
 */
int ctm_input_register_device();
int ctm_input_unregister_device(int devid);

/* If there are more devices than players, we try to apportion high-priority devices equally.
 * This is just a hint. It never screws things up bad, and often doesn't mean anything at all.
 * Use zero (default) for keyboards, or ten for joysticks, or anything else.
 */
int ctm_input_device_set_priority(int devid,int priority);

/* Manage mappings between device and player.
 * There can be any number of devices, each of which can point to one of four players, or to the special "player zero".
 * A single player may have multiple devices feeding it.
 * During normal operation, you don't need to worry about all this mapping junk.
 * Just read the player's state from ctm_input_by_playerid.
 */
int ctm_input_unmap_all_players();
int ctm_input_automap_players(int playerc); // Reassign all playerid based on the given total count.
int ctm_input_automap_device(int devid,int playerc);
int ctm_input_automap_unassigned_devices(int playerc); // Like automap_players, but only change unassigned devices.
int ctm_input_set_playerid(int devid,int playerid);
int ctm_input_get_playerid(int devid);
int ctm_input_playerid_active(int playerid);
int ctm_input_count_players(); // How many real playerid are in use? (playerid zero doesn't count).
int ctm_input_count_attached_players(); // <=count_players(); how many have an associated device?
int ctm_input_count_devices();
int ctm_input_devid_for_index(int index);
int ctm_input_devid_for_playerid(int playerid,int devindex); // Player can have more than one device, hence the index.
uint16_t ctm_input_device_state(int devid); // Devices track their own state independant of players.

/* Button IDs are 15 independant bits, and 32767 stateless signals.
 * When CTM_BTNID_SIGNAL (0x8000) is set, the other bits are an enum, not bitfields.
 * We can process signal events without an associated device, but bit events require a registered device.
 */
#define CTM_BTNID_UP          0x0001
#define CTM_BTNID_DOWN        0x0002
#define CTM_BTNID_LEFT        0x0004
#define CTM_BTNID_RIGHT       0x0008
#define CTM_BTNID_PRIMARY     0x0010
#define CTM_BTNID_SECONDARY   0x0020
#define CTM_BTNID_TERTIARY    0x0040
#define CTM_BTNID_PAUSE       0x0080
#define CTM_BTNID_RSVDBITS    0x7f00

#define CTM_BTNID_ANYKEY      0x00f0

#define CTM_BTNID_SIGNAL       0x8000
#define CTM_BTNID_QUIT         0x8001
#define CTM_BTNID_RESET        0x8002
#define CTM_BTNID_SCREENSHOT   0x8005
#define CTM_BTNID_RESIZE       0x8006 // Notify everyone of video resize -- only the video backend should send this.
#define CTM_BTNID_FULLSCREEN   0x8007 // Toggle fullscreen.
#define CTM_BTNID_FULLSCREEN_1 0x8008 // One-way fullscreen.
#define CTM_BTNID_FULLSCREEN_0 0x8009 // One-way exit fullscreen.
#define CTM_BTNID_UNUSED       0x800a // Placeholder to nix a mapping

const char *ctm_input_btnid_name(uint16_t btnid);

/* Definitions, from config file.
 */

struct ctm_input_field {
  int srcscope; // Qualifier for driver-side button.
  int srcbtnid; // Driver-side button ID.
  uint16_t dstbtnidlo; // CTM_BTNID_* or zero, use this for the negative part of 2-way axes.
  uint16_t dstbtnidhi; // CTM_BTNID_* or zero, the button to output on positive input.
  int threshhold,bias; // No universal meaning; backend may use however it likes.
};

struct ctm_input_definition {
  char *name;
  int namec;
  struct ctm_input_field *fldv;
  int fldc,flda;
};

/* Find a device definition matching the given name.
 * Returns a weak reference to the data stored globally.
 * This is guaranteed to remain usable until ctm_input_quit().
 */
struct ctm_input_definition *ctm_input_get_definition(const char *devname,int devnamec);

#endif
