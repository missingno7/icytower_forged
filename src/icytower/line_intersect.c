/* line_intersect.c -- 2D line-segment intersection test used by the
 * player/floor collision code (handle_player_collision_combo/vector/vector_2).
 *
 * Original source: F:\projects\icytower\trunk\source\main.c. This is the
 * win32_pilot.md SS3/SS6a x87 escalation case: 37 x87 instructions, an
 * explicit `fnstcw`/`fldcw`-guarded round-toward-zero `fistp` pair, and
 * KNOWN (carrier/lift/README.md SS6b, artifacts/lift_x87_finding.md) to
 * need genuine 80-bit extended precision -- a `double`-only model of this
 * function's arithmetic DIFFERS from the original on 107-128 of every
 * 50000 vectors (whole-pixel errors), because every intermediate here
 * stays in the x87 register stack and nothing is spilled to memory as a
 * float/double before the final truncating conversion. That precision gap
 * is the ONLY remaining difference once this file is built with genuine
 * 80-bit x87 codegen (`-mfpmath=387 -mno-sse2`, carrier/lift/harness/
 * GCC_X87.md); see PROMOTIONS.md.
 *
 * Segments are (x1,y1)-(x2,y2) and (x3,y3)-(x4,y4). Letting
 * dx1=x2-x1, dy1=y2-y1, dx3=x4-x3, dy3=y4-y3, px=x1-x3, py=y1-y3:
 *
 *   D  = dx1*dy3 - dx3*dy1
 *   ua = (dx3*py - dy3*px) / D
 *   ub = (dx1*py - dy1*px) / D
 *
 * If 0 <= ua <= 1 and 0 <= ub <= 1, the segments intersect at parameter ua
 * along the first segment; *px_out/*py_out receive that point, rounded
 * with the game's own round-half-away-from-zero-toward-the-segment-start
 * convention (`(int)(ua*dx1 + 0.5)`, truncated toward zero under an
 * explicit `FLDCW`), and the function returns 1. Otherwise it returns 0
 * and touches neither output (the function's only two stores outside its
 * own frame are exactly those two, per carrier/lift/README.md SS7 -- the
 * comparison domain is provably complete).
 *
 * D == 0 (parallel/collinear segments): every degenerate case except one
 * is caught by the range checks below exactly like any other
 * out-of-[0,1] value, because dividing a nonzero numerator by D == 0
 * yields +-infinity, and +-infinity is an ORDERED IEEE-754 value that
 * still fails one of the four `< 0.0` / `> 1.0` tests. The one case that
 * is NOT caught is the fully-degenerate one where BOTH numerators are
 * also zero (e.g. the segments share their start point, px == py == 0):
 * then ua = ub = 0.0/0.0 = NaN, and every comparison against a NaN is
 * UNORDERED, i.e. false, in both directions. The original x87 code
 * expresses each guard as "fail (return 0) only on the ordered
 * true case" (`fucom`/`fnstsw`/`test $0x45,%ah`/`je`, per
 * artifacts/disasm.txt 0x406be2-0x406c27): a NaN operand leaves every
 * `je` untaken, so execution falls through all four guards uncaught and
 * reaches the store/return-1 path with ua == ub == NaN still on the FPU
 * stack. `(int)(NaN + 0.5)` under the function's masked, round-toward-
 * zero `fistp` then yields the x87 "integer indefinite" result
 * (0x80000000, since #IA is masked -- carrier/lift/README.md SS6a), so
 * *px_out and *py_out end up as `x1`/`y1` plus that indefinite value, and the
 * function returns 1 -- garbage coordinates, but not a crash, and it IS
 * what the original binary does (confirmed against ORIGINAL execution in
 * unicorn, carrier/lift/harness/GCC_X87.md SS4).
 *
 * The guards below are written as direct `<`/`>` comparisons rather than
 * negated `!(... >= ...)` forms specifically so that C's own IEEE-754
 * comparison semantics reproduce that "NaN never fails a guard" fall-
 * through: `ua < 0.0` and `ua > 1.0` are both false when `ua` is NaN,
 * exactly like the unordered `fucom` above, whereas the previous
 * `!(ua >= 0.0)` form is true for NaN (`ua >= 0.0` itself is false for
 * NaN, so its negation is true) and returned 0 where the original does
 * not -- a toolchain-independent EAX mismatch on ~5% of vectors
 * (948-1011/20000 per seed, identical across every MSVC and GCC build
 * tried), unrelated to the x87-precision gap described above and fixed
 * here. `ub`'s two guards were already written this way and needed no
 * change.
 *
 * Recovered by hand-simulating the x87 stack traffic in
 * artifacts/disasm.txt (0x406b80..0x406cad) instruction by instruction and
 * cross-checked against carrier/lift/lifted/lifted_line_intersect.c
 * (generated mechanically from the same bytes, already offline-verified
 * bit-exact against the original with a software 80-bit x87 backend,
 * carrier/lift/README.md SS6b/SS7). This file keeps `ua`/`ub` as ordinary
 * automatic `double`s and lets the compiler's own x87 register allocation
 * carry their extra precision across the range checks and into the final
 * `+ 0.5` / truncation, exactly like the original GCC output did; see
 * PROMOTIONS.md for this file's offline equivalence result under the MSVC
 * build this harness uses, since that is a different compiler from the
 * one the game shipped with and win32_pilot.md SS6a is explicit that only
 * a real 80-bit x87 build (not an MSVC/SSE double) is trusted by
 * construction -- the oracle comparison, not this comment, is the actual
 * verdict.
 */
#include "game_types.h"

int line_intersect(int x1, int y1, int x2, int y2,
                    int x3, int y3, int x4, int y4,
                    int *px_out, int *py_out)
{
    int dx1 = x2 - x1;
    int dy1 = y2 - y1;
    int dx3 = x4 - x3;
    int dy3 = y4 - y3;
    int px = x1 - x3;
    int py = y1 - y3;

    double D = (double)(dx1 * dy3 - dx3 * dy1);
    double ua = (double)(dx3 * py - dy3 * px) / D;

    if (ua < 0.0)
        return 0;                      /* ua < 0 (ordered); NaN falls through */

    {
        double ub = (double)(dx1 * py - dy1 * px) / D;

        if (ub < 0.0)
            return 0;
        if (ua > 1.0)
            return 0;
        if (ub > 1.0)
            return 0;

        *px_out = x1 + (int)(ua * dx1 + 0.5);
        *py_out = y1 + (int)(ua * dy1 + 0.5);
        return 1;
    }
}
