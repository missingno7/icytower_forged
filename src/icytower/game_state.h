/* game_state.h -- ordinary extern declarations of the game globals used by
 * update_frame.c and is_solid.c (win32_pilot.md SS7a).
 *
 * No address appears here or anywhere else in src/: each name is declared
 * exactly as an application would declare a global it does not own the
 * storage for. Two things give it real storage and a real address,
 * depending on which world this file is compiled into:
 *
 *   - INTO THE CARRIER: the generated bindings header
 *     (carrier/gen/pf_bindings_src.h, or its PF_MEM-wrapped harness twin) is
 *     force-included (`/FI`) ahead of every other token in the translation
 *     unit. It #defines each of these names to `(*(T*)original_address)`,
 *     so by the time the `extern` lines below would otherwise be seen, the
 *     names are already macros -- and an `extern` re-declaration of an
 *     already-macro-expanded name does not parse (the macro body would
 *     expand inside the declarator). The ICYTOWER_BINDINGS_ACTIVE guard,
 *     defined by that same generated header, is what lets this file detect
 *     that and skip its own declarations.
 *   - STANDALONE: no bindings header is force-included, so
 *     ICYTOWER_BINDINGS_ACTIVE is undefined, the extern declarations below
 *     are the only declaration of these names, and state.c supplies their
 *     storage (win32_pilot.md SS7a: "a state.c defines the globals and the
 *     bindings header is absent").
 */
#ifndef ICYTOWER_GAME_STATE_H
#define ICYTOWER_GAME_STATE_H

#include "game_types.h"

#ifndef ICYTOWER_BINDINGS_ACTIVE

extern int reward_time;          /* counts down to 0; drives the reward-bar ramp */
extern fixed reward_scale;       /* HUD reward-bar value */
extern int player_id;            /* index of the active player into ply[] */
extern Tplayer *ply[1000];       /* the player table */
extern volatile int logic_count; /* 20ms-tick counter, shared with the timer thread */

#endif /* !ICYTOWER_BINDINGS_ACTIVE */

#endif /* ICYTOWER_GAME_STATE_H */
