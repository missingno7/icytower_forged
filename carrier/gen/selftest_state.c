/* selftest_state.c -- stub storage for the externs selftest_state.h
 * declares, standing in for the port's own state.c in the standalone
 * (non-carrier) world (win32_pilot.md SS7a: "standalone, a state.c defines
 * the globals and the bindings header is absent"). Only compiled/linked
 * when bindings_selftest.c is built WITHOUT /FIpf_bindings.h.
 */
#include "selftest_state.h"

fixed reward_scale;
int player_id;
Tplayer *ply[1000];
