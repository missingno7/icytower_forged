/* pf_harness_rand.h -- harness-only shim redirecting rand() (see
 * add_floor()'s own header comment in src/icytower/map.c for the
 * generation rules that make it call rand(), and notes/
 * layout_rules_1.5.1.md's "verification" section for why this exists).
 *
 * WHY: unicorn (lift_check.py's ORIGINAL side) never maps msvcrt.dll, so
 * the game's own `_rand` thunk (0x4bad18: `jmp *[0x514944]`, an IAT slot)
 * is unreachable there -- add_floor's `call 0x4bad18` would fault. The
 * ORIGINAL side works around this with a code hook at 0x4bad18
 * (lift_check.py's Oracle._rand_hook) that emulates msvcrt's LCG in
 * Python from a per-vector seed (RAND_SEED_VA in the vector's writes)
 * instead of letting the jmp execute. This header is the other half: it
 * makes the COMPILED candidate (src_check.exe / gcc_check.exe) draw from
 * the exact same LCG, seeded the exact same way per vector, instead of
 * whatever the host's actual CRT rand() happens to do.
 *
 * rand()'s algorithm is KNOWN and independently confirmed twice --
 * assets/replay_checker/Icy Tower.cpp's rand_p() (`*seed = *seed*214013 +
 * 2531011; return (*seed>>16)&0x7fff;`) and notes/
 * replay_checker_reference.md's "RNG" bullet -- so pinning our own copy
 * (harness_rand.c) rather than relying on the host toolchain's actual
 * CRT rand()/srand() removes any dependency on which CRT (msvcrt.dll vs.
 * ucrtbase.dll) the harness happens to link against, or on srand()'s
 * internal representation matching this 32-bit state word 1:1 on every
 * toolchain -- a hand-controlled, harness-only state we can set directly
 * per vector, not a hope that srand(seed) reproduces it.
 *
 * MECHANISM: force-included (/FI on cl, -include on gcc) ahead of EVERY
 * file in the add_floor harness build, the same "whole invocation" scope
 * carrier/gen/pf_bindings_harness.h already uses for every other game
 * global (see that file's `#define demo (*(Treplay**)PF_MEM(0x4dd250))`
 * for the precedent). <stdlib.h> is pulled in FIRST, right here, so the
 * real `rand()` declaration (msvcrt's actual prototype, whatever
 * decoration the host's <stdlib.h> gives it) is fully parsed and locked
 * in BEFORE the #define below takes effect -- only src/icytower/map.c's
 * own `rand()` call (and this header's own re-inclusion, inert) is ever
 * touched by the rename; <stdlib.h>'s own declaration text is not
 * reprocessed a second time (its own include guard prevents that), so it
 * is never itself rewritten.
 *
 * NOT part of src/icytower/ -- src/icytower/map.c calls plain, ordinary
 * `rand()` (src/README.md: "#include's of ... the C standard library" is
 * explicitly allowed), and that stays correct and address-free in the
 * STANDALONE and CARRIER worlds (a real CRT rand() stream, seeded via
 * new_game()'s srand(), is exactly what those worlds want). This
 * redirection exists ONLY for the offline equivalence harness's own
 * build, exactly like src_check.c's tr()/untr() memory-model shims.
 */
#ifndef PF_HARNESS_RAND_H
#define PF_HARNESS_RAND_H

#include <stdlib.h>

extern unsigned int harness_rand_state;
int harness_rand(void);

#define rand harness_rand

#endif /* PF_HARNESS_RAND_H */
