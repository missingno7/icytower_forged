/* pf_harness_batch15.h -- force-included ahead of batch15b_check.exe's
 * TRANSLATION UNITS (see build_batch15.sh), the same way
 * pf_harness_batch14.h is force-included ahead of batch14b_check.exe.
 *
 * PROMOTIONS.md batch 15.  Purpose: redirect the CRT calls that
 * src/icytower/replay.c and game_data.c make, so the compiled candidate
 * TRACES them instead of touching the real heap -- exactly mirroring
 * what the Python side's unicorn hooks do to the ORIGINAL bytes at the
 * matching PE thunk VAs (malloc 0x4bad10, free 0x4bad08, sprintf
 * 0x4bad60, stricmp 0x4b2dd8).
 *
 * FOUR deliberate asymmetries, each for a reason:
 *
 *  1. `strcat` is NOT redirected.  getGameDataXML() makes fourteen
 *     appends, and the original inlines the six with a string LITERAL
 *     (repnz scas + rep movsb, no call at all) while really calling
 *     0x4bad58 for the eight with a buffer argument.  Redirecting the
 *     name would turn all fourteen into calls on the candidate side and
 *     none of them would line up.  Left alone, GCC's own builtin makes
 *     the same split, and neither side traces strcat: what the appends
 *     did is compared through the RETURNED BUFFER instead, which sees
 *     all fourteen.
 *  2. `strcpy` is NOT redirected either, and for the same reason:
 *     create_replay's "Harold"/"no date" and getGameDataXML's four
 *     literal seeds are inlined on both sides.
 *  3. h_malloc() does NOT call the real malloc.  It bump-allocates from
 *     a static arena PRE-FILLED WITH 0xA5, which is the whole point:
 *     create_replay provably leaves part of the block it returns
 *     uninitialised (its `date[8..31]`), and an arena with a known fill
 *     is what turns "uninitialised" from an incomparable fact into a
 *     visible one -- the same bytes appear on both sides and the domain
 *     can cover the WHOLE structure instead of stopping short of it.
 *     h_free() traces without freeing, for the same reason.
 *  4. `REPLAY_HEADER` is renamed.  src/icytower/state.c -- a GENERATED
 *     file -- defines it as `const char REPLAY_HEADER[6] = {0}`, because
 *     the generator zero-fills every global; but this one is real
 *     .rodata in the original ("ITR140"), and a standalone build that
 *     believes the zero writes .itr files nothing can read back.  The
 *     macro below points replay.c at an array batch15b_check.c
 *     initialises correctly, and state.c is compiled WITHOUT this header
 *     so its own (now unused) definition still links.  Reported as a
 *     generator gap; not worked around anywhere but here.
 *
 * A translation unit that needs the REAL function (the stubs themselves,
 * and the driver's own output) undefines the macro it needs at the top
 * of the file -- there is no way to un-force-include a header.
 */
#ifndef PF_HARNESS_BATCH15_H
#define PF_HARNESS_BATCH15_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *h_malloc(size_t n);
void  h_free(void *p);
int   h_sprintf(char *dst, const char *fmt, ...);
int   h_stricmp(const char *a, const char *b);

extern const char pf_b15_header[6];

/* <stdio.h>/<stdlib.h>/<string.h> are pulled in ABOVE these macros on
 * purpose: their own declarations (and any __mingw_ovr inline
 * definitions) must be seen with the real names before the names are
 * hijacked.  Each name is #undef'd first because several of them are
 * macros in some CRT headers. */
#undef malloc
#undef free
#undef sprintf
#undef stricmp
#define malloc   h_malloc
#define free     h_free
#define sprintf  h_sprintf
#define stricmp  h_stricmp

#define REPLAY_HEADER pf_b15_header

#endif /* PF_HARNESS_BATCH15_H */
