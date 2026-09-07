/* bindings_selftest.c -- compile fixture proving pf_bindings.h's contract
 * from win32_pilot.md SS7a: the SAME address-free C compiles two ways.
 *
 *   1. carrier world:
 *        cl /nologo /c /W3 /TC /Icarrier\gen /FIpf_bindings.h bindings_selftest.c
 *      pf_bindings.h is forced-included before this file's own tokens are
 *      seen, so `PF_BINDINGS_H` is already defined; selftest_state.h's
 *      extern declarations are skipped (see there for why they must be);
 *      reward_scale/player_id/ply resolve through pf_bindings.h's
 *      `#define <name> ...` to the ORIGINAL game addresses; Tplayer/fixed
 *      come from pf_bindings_types.h -> it_types.h.
 *
 *   2. standalone world:
 *        cl /nologo /c /W3 /TC /Icarrier\gen bindings_selftest.c selftest_state.c
 *      No forced include, so `PF_BINDINGS_H` is undefined; selftest_state.h
 *      makes reward_scale/player_id/ply ordinary externs and
 *      selftest_state.c supplies their storage. No carrier address, no
 *      carrier macro, anywhere in this translation.
 *
 * This file itself contains NO address literal and NO carrier-specific
 * identifier -- it is written exactly the way src/icytower/*.c will be
 * (recovered semantics only, see win32_pilot.md SS7a and
 * scripts/check_native_layer.py).
 */
#include "selftest_state.h"

void selftest_bump(void)
{
    reward_scale += 1;
    ply[player_id]->x += 1;
}
