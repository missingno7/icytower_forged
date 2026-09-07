/* state.c -- standalone storage for the globals declared in game_state.h.
 *
 * Only used when src/ is built OUTSIDE the carrier (win32_pilot.md SS7a:
 * "standalone, a state.c defines the globals and the bindings header is
 * absent"). When src/ is compiled INTO the carrier, the generated bindings
 * header supplies these names as address-backed macros instead and this
 * file is not part of that build at all.
 *
 * This is a placeholder home for state ownership, not a claim that it has
 * moved: today the values here are only exercised by standalone unit/compile
 * checks (scripts/check_native_layer.py's gate, offline compile tests), and
 * gameplay still runs address-backed inside the carrier.
 */
#include "game_state.h"

int reward_time;
fixed reward_scale;
int player_id;
Tplayer *ply[1000];
volatile int logic_count;
