/* new_rand.c -- the game's own pseudo-random number generator: a self-
 * contained x87 "multiply, fold into [0, MODULUS], take the fractional
 * part" generator over the global `seed` (a `double`, main.c). This is the
 * per-frame gameplay RNG every other RNG consumer in this pass
 * (update_particle.c, create_particle.c) calls -- distinct from msvcrt's
 * rand()/srand(), which win32_pilot.md SS2's "RNG" row already establishes
 * is only used to seed the tower layout at `new_game`/`init_game`, not for
 * per-tick gameplay randomness.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c, decl_line 648
 * (artifacts/dwarf_info.txt); its one local variable is named `x`
 * (decl_line 649), kept here as this file's variable name, same convention
 * as line_intersect.c keeping `ua`/`ub`.
 *
 * Recovered by hand-simulating the x87 stack traffic in
 * artifacts/disasm.txt (0x406984..0x406a03) instruction by instruction,
 * then CROSS-CHECKED by direct execution: the original bytes were run in
 * unicorn (the same oracle carrier/lift/harness/lift_check.py uses) over
 * ~3200 seeds spanning small/negative/boundary/multi-fold values, and the
 * return value matched this file's algorithm on every single one (0
 * mismatches); the ~1200 cases where the ORACLE's returned `seed` disagreed
 * with a plain-`double`-arithmetic Python re-implementation in its last
 * bit or two are exactly win32_pilot.md SS6a's expected 80-bit-vs-64-bit
 * gap (see PROMOTIONS.md for the GCC/MSVC offline numbers this file's
 * source recovery was cross-checked against, not guessed).
 *
 * Two literal constants, read verbatim from the image (`assets/
 * icytower15.exe`, file bytes at the VAs the disassembly's `fldl`/`flds`
 * reference):
 *
 *   MULTIPLIER (0x4d6ca8, a `double` -- `fldl`): 1.4294484665 exactly (its
 *     8 bytes round-trip losslessly through this exact decimal literal).
 *   MODULUS (0x4d6cb0, a `float` -- `flds`): 65535.0 (2**16 - 1), exact in
 *     any FP format, so writing it as a plain (double) literal below changes
 *     nothing -- the same choice line_intersect.c already made for its
 *     `0.5` (also loaded there via `flds`).
 *
 * Algorithm, in the disassembly's own operation order:
 *
 *   x = MULTIPLIER * seed;
 *   seed = x;                          // fldl/fmull/fstl -- stored BEFORE
 *                                       // the fold check even runs
 *   if (x > MODULUS) {
 *       do {
 *           x = x - MODULUS;           // fsubr %st,%st(1): dest -= src,
 *                                       // i.e. x -= MODULUS (verified
 *                                       // against real hardware -- see
 *                                       // above; NOT the reversed
 *                                       // "MODULUS - x" the mnemonic's
 *                                       // name alone would suggest)
 *       } while (x > MODULUS);         // fucom/fnstsw/test/je pair at
 *                                       // 0x4069ba-0x4069c1: a genuine
 *                                       // loop, not dead code -- an `x`
 *                                       // more than 2*MODULUS above zero
 *                                       // (reachable for large |seed|)
 *                                       // folds more than once
 *       seed = x;                      // re-stored only on this path
 *   }
 *   {
 *       int ix = (int)x;               // fistl under an explicit
 *                                       // fnstcw/fldcw round-toward-zero
 *                                       // pair, same idiom line_intersect.c
 *                                       // already documents
 *       return (int)((x - (double)ix) * MODULUS);   // fractional part,
 *                                       // rescaled into [0, MODULUS) and
 *                                       // truncated the same way
 *   }
 *
 * Every intermediate is kept in an ordinary automatic `double` and the
 * compiler's own x87 register allocation is left to carry any extra-than-
 * double precision across the branch and into the two final truncating
 * conversions (win32_pilot.md SS6a's rule: `-m32 -mfpmath=387 -mno-sse2
 * -O1`/`-O2`, no `-ffloat-store`, no `-O0` -- see PROMOTIONS.md for this
 * file's measured result under that build).
 */
#include "game_types.h"
#include "game_state.h"

int new_rand(void)
{
    double x = 1.4294484665 * seed;
    seed = x;
    if (x > 65535.0) {
        do {
            x = x - 65535.0;
        } while (x > 65535.0);
        seed = x;
    }
    {
        int ix = (int)x;
        return (int)((x - (double)ix) * 65535.0);
    }
}
