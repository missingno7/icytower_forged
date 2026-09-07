/* selftest_state.h -- fixture header for carrier/gen/bindings_selftest.c.
 *
 * Declares (as ordinary externs) the small slice of game state the selftest
 * touches, using real names/types from interop_index.json / it_globals.h /
 * it_types.h, in the style src/icytower/*.h will use (win32_pilot.md SS7a:
 * "globals declared as ordinary externs").
 *
 * This header is deliberately SKIPPED when pf_bindings.h has already been
 * forced-included (`PF_BINDINGS_H` already defined by the time this file's
 * own tokens are preprocessed): pf_bindings.h's `#define reward_scale
 * (*(fixed*)0x...)` (etc) already makes the plain name usable as an lvalue
 * at its original address, and re-declaring it with `extern fixed
 * reward_scale;` in the same translation unit is not valid C -- the object
 * macro expands INSIDE the declarator (`extern fixed (*(fixed*)0x...);`),
 * which does not parse. Standalone (no /FI), `PF_BINDINGS_H` is undefined,
 * so these three lines are the only declaration of the names, and
 * selftest_state.c supplies their storage -- this is the "state.c defines
 * the globals and the bindings header is absent" half of SS7a.
 */
#ifndef SELFTEST_STATE_H
#define SELFTEST_STATE_H

#ifndef PF_BINDINGS_H
#include "it_types.h"   /* Tplayer, fixed: carrier-recovered layouts.
                            src/ will own its own copy of these later
                            (SS7a); this fixture reuses it_types.h so the
                            standalone build stays layout-identical to the
                            carrier build without a second, driftable copy. */

extern fixed reward_scale;
extern int player_id;
extern Tplayer *ply[1000];

#endif /* !PF_BINDINGS_H */

#endif /* SELFTEST_STATE_H */
