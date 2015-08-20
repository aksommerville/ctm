/* ctm_geography.h
 * Data about the states and their population.
 */

#ifndef CTM_GEOGRAPHY_H
#define CTM_GEOGRAPHY_H

// Race affects the choice of skin and hair color, nothing more.
// This is not exposed to the user, as such.
#define CTM_RACE_EURO    1
#define CTM_RACE_AFRO    2
#define CTM_RACE_ASIO    3

// Party is the clothing color, and is the only demographic metric.
#define CTM_PARTY_WHITE  1
#define CTM_PARTY_BLACK  2
#define CTM_PARTY_YELLOW 3
#define CTM_PARTY_GREEN  4
#define CTM_PARTY_BLUE   5
#define CTM_PARTY_RED    6
extern const uint32_t ctm_party_color[7]; // [0] is the "no party" color.
extern const char *ctm_party_name[7];
extern const char *CTM_PARTY_NAME[7]; // All caps, for newpaper headlines.

// I can't imagine needing a specific state programmatically, but what the hell:
#define CTM_STATE_ALASKA     0
#define CTM_STATE_WISCONSIN  1
#define CTM_STATE_VERMONT    2
#define CTM_STATE_CALIFORNIA 3
#define CTM_STATE_KANSAS     4
#define CTM_STATE_VIRGINIA   5
#define CTM_STATE_HAWAII     6
#define CTM_STATE_TEXAS      7
#define CTM_STATE_FLORIDA    8

extern struct ctm_state_data {
  const char *name;     // eg "Alaska"
  const char *NAME;     // eg "ALASKA"
  char abbreviation[2]; // eg "AK"
  int main_party;       // CTM_PARTY_* or zero.
  int population;       // Relative population, 5..10.
} ctm_state_data[9];

/* (dstv) points to nine slots, one for each state.
 * (total) is the total count of voters, ie the sum of dstv.
 * Populations outside (100..100K) are rejected.
 */
int ctm_apportion_population(int *dstv,int total);

/* Return state index 0..8 for a pixel or cell in world space.
 * Always returns a valid index.
 */
int ctm_state_for_pixel(int x,int y);
int ctm_state_for_cell(int col,int row);

/* Choose a random position (x,y) and 'interior' flag within the named state.
 * (x,y) will always be (CTM_TILESIZE>>1) beyond a cell boundary. (ie center of cell).
 * We reject locations already occupied exactly (to the pixel) by any other sprite.
 * We make a serious attempt to ensure that the chosen cell is passable and not already occupied.
 * If that can't be done, or we fuck it up somehow, we do at least provide sensible coordinates within the state.
 */
void ctm_choose_vacant_location(int *x,int *y,int *interior,int stateix,int interior_ok);

/* Poll all voters in the named state, or nationwide if out of range.
 * Return seven poll results: One counting everybody, then one for each party.
 * (threshhold) is the degree of certainty a voter must have to count firmly.
 */
struct ctm_poll_result {
  int bluec,redc; // How many are beyond the confidence threshold?
  int subbluec,subredc; // How many within confidence threshhold?
  int zeroc; // How many literal zeroes?
  int total; // (bluec+redc+subbluec+subredc+zeroc)
};
int ctm_conduct_poll(struct ctm_poll_result *resultv,int stateix,int8_t threshhold);

/* For each state, contains:
 *   -2  Solid blue.
 *   -1  Leaning blue.
 *    0  Too close to call.
 *    1  Leaning red.
 *    2  Solid red.
 */
extern int ctm_state_prediction[9];
int ctm_update_state_prediction();

/* Conduct the full election.
 * This doesn't modify anything, so you can keep on playing if you want to.
 * Ties are possible but very unlikely.
 */
struct ctm_election_result {
  int elecblue,elecred; // Count of electoral votes (one state, zero or one vote). The rest is academic.
  int totalblue,totalred; // Popular vote. Not used in the final calculation, but fun to observe.
  int favor; // Sum of voter sentiment. <0 for blue or >0 for red, up to (population*128).
  int favorblue,favorred; // Maybe useful for asking whose voters were more enthusastic? (NB favorblue is always negative).
  int population; // Total count of voters considered. (Always == totalblue+totalred).
  int brokendown[2*7*9]; // Votes broken down by party and state. (blue,red,...).
};
int ctm_conduct_election(struct ctm_election_result *result);

/* Returns the sum of all voters' decision, and the count of voters.
 * Handy during debugging, since it yields a fairly deterministic, single number.
 */
void ctm_poll_total_sentiment(int *sentiment,int *count);

/* Search for relevant voters and cause favor in them.
 */
int ctm_perform_broad_event(int target_stateix,int target_party,int party,int favor);
int ctm_perform_local_event(int x,int y,int interior,int radius,int party,int favor);

/* Player awards.
 */
 
#define CTM_AWARD_PARTICIPANT      0 // If nothing else...
#define CTM_AWARD_DEADLY           1 // More kills (of all kinds) than other players.
#define CTM_AWARD_DISHONORABLE     2 // More mummy kills than other players.
#define CTM_AWARD_BIG_GAME         3 // More boss kills -- always awarded if eligible.
#define CTM_AWARD_DEAD_MEAT        4 // More deaths.
#define CTM_AWARD_TALKATIVE        5 // More speeches (both).
#define CTM_AWARD_FACE_TO_FACE     6 // Highest ratio of in-person speeches.
#define CTM_AWARD_MEDIA_SAVVY      7 // Highest ratio of radio speeches.
#define CTM_AWARD_PUNCHING_BAG     8 // More strikes.
#define CTM_AWARD_UNKILLABLE       9 // No deaths, if every other player did die.
#define CTM_AWARD_HOARDER         10 // More coins than other players.
#define CTM_AWARD_STATS_READER    11 // More pause time than other players.
#define CTM_AWARD_ACTIVE          12 // Less pause time than other players.
#define CTM_AWARD_WANTED          13 // Wanted in more states.
 
extern const char *ctm_award_name[16];

int ctm_decide_player_awards();

#endif
