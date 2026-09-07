/* native_is_solid.c -- NATIVE form of is_solid (VA 0x004166dc).
 *
 * Hand-written, not generated. Read directly from artifacts/disasm.txt
 * (0x004166dc-0x00416747) and cross-checked instruction-for-instruction
 * against carrier/lift/lifted/lifted_is_solid.c (the LIFTED form,
 * win32_pilot.md SS3), which is the faithful-but-unreadable reference this
 * file was written to match. See carrier/native/README.md for the full
 * local/global name mapping and the argument for every place where writing
 * this readably required a construct that isn't a literal byte-for-byte
 * transcription of the assembly -- most notably the map->offset % 16
 * derivation below.
 *
 * Original source: F:\projects\icytower\trunk\source\map.c, decl_line 122
 * (artifacts/dwarf_info.txt), which also names the parameters (m, cx, cy)
 * and the two locals (x, y) used below. Same prototype as PFN_is_solid in
 * carrier/gen/it_funcs.h.
 *
 * Memory model: see native_update_frame.c's header comment for the full
 * explanation of PF_MEM(); the short version is that `m` arrives as a guest
 * VA, not a host pointer, so it is threaded through PF_MEM exactly once,
 * at the top, before any field of *m is read. Nothing else in this file
 * depends on that mechanism.
 *
 * is_solid never writes to *m (notes/promotion_candidates.md SS2/SS4: it is
 * a pure predicate and the designated negative-control candidate -- the
 * offline check requires Tmap to come back byte-identical).
 */
#include "pf_rt.h"      /* the one memory seam: PF_MEM() */
#include "it_types.h"
#include "it_funcs.h"

int __cdecl native_is_solid(Tmap *m, int cx, int cy)
{
    Tmap *map = (Tmap *)PF_MEM((unsigned int)(size_t)m);
    int x, y;

    /* y: which of the 32 floor rows (Tmap.room[32]) pixel row cy falls in,
     * counting rows from the bottom of the tower (room[0]) upward. `>> 4`
     * is a 16-pixel-per-tile arithmetic shift; MSVC documents `>>` on a
     * negative int as arithmetic (matching the original's `sar`), the same
     * implementation-defined point carrier/lift/README.md SS9 already
     * relies on and that the offline equivalence check re-verifies here. */
    y = 29 - ((cy + 1) >> 4);
    if (y < 0 || y > 31)
        return 0;                              /* off the top or bottom of the tower */

    if (map->room[y].empty != 0)
        return 0;                              /* no floor drawn in this row */

    /* x: which 16-pixel tile column cx falls in. */
    x = cx >> 4;
    if (x < map->room[y].start_tile)
        return 0;                              /* left of the floor's tile span */
    if (x > map->room[y].end_tile)
        return 0;                              /* right of the floor's tile span */

    /* Solid: return the floor's on-screen pixel Y, adjusted for the map's
     * current vertical scroll offset.
     *
     * The original computes `map->offset & 0x8000000f`, tests its sign bit,
     * and on a negative result runs a `dec/or 0xfffffff0/inc` correction
     * (artifacts/disasm.txt 0x00416719-0x00416731). That sequence is GCC's
     * idiom for `%` on a signed int against a power-of-two divisor without
     * cmov: for offset >= 0 it is exactly offset & 0xf; the correction step
     * for offset < 0 subtracts 16 from that whenever the low 4 bits are
     * nonzero, and returns 0 when they are, which is precisely C's `%`
     * (truncating toward zero, result takes the dividend's sign) applied to
     * a 32-bit int -- verified by hand for both branches and re-verified
     * empirically below across positive, negative and exact-multiple
     * offsets (notes/promotion_candidates.md SS4 vector pool). The divisor
     * is the compile-time constant 16, so this can never divide-by-zero or
     * overflow the way a variable idiv could. */
    return cy - (map->offset % 16) + 10000 - (((cy + 1) >> 4) << 4);
}
