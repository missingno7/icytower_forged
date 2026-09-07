/* is_solid.c -- pure predicate, no writes at all: "is there floor here, and
 * if so at what on-screen Y".
 *
 * Recovered from the verified NATIVE form (carrier/native/native_is_solid.c,
 * carrier/native/README.md; equivalence result: artifacts/native_equivalence.json,
 * 20000 vectors, EQUAL, plus a negative-control run since this function's
 * only writes would be a bug), rewritten as ordinary game source: no
 * PF_MEM, no generated interop headers. See src/README.md for how the same
 * file below still resolves `Tmap`/`Tfloor` to the original memory layout
 * when built into the carrier.
 *
 * Original source: F:\projects\icytower\trunk\source\map.c, decl_line 122
 * (artifacts/dwarf_info.txt), which also names the parameters (m, cx, cy)
 * and the two locals (x, y) used below -- kept as named there.
 *
 * Given a pixel coordinate (cx, cy), answers "is there floor here": convert
 * cy to a tower row y (16 px/tile, counted from the top of the fixed
 * 32-row m->room[] array downward), bounds-check y, bail out if that row is
 * empty, convert cx to a tile column x, bail out if x falls outside the
 * row's [start_tile, end_tile] span, and otherwise return the floor's solid
 * pixel Y adjusted for m->offset (the map's current vertical scroll
 * position). Never writes to *m.
 */
#include "game_types.h"

int is_solid(Tmap *m, int cx, int cy)
{
    int x, y;

    /* y: which of the 32 floor rows pixel row cy falls in, counting rows
     * from the bottom of the tower (room[0]) upward. `>> 4` is a
     * 16-pixel-per-tile arithmetic shift; right shift of a negative int is
     * implementation-defined in C, and MSVC documents it as arithmetic
     * (matching the original compiler's shift), which is exactly what the
     * offline equivalence check verifies with negative and boundary values
     * in its vector pool. */
    y = 29 - ((cy + 1) >> 4);
    if (y < 0 || y > 31)
        return 0;                              /* off the top or bottom of the tower */

    if (m->room[y].empty != 0)
        return 0;                              /* no floor drawn in this row */

    /* x: which 16-pixel tile column cx falls in. */
    x = cx >> 4;
    if (x < m->room[y].start_tile)
        return 0;                              /* left of the floor's tile span */
    if (x > m->room[y].end_tile)
        return 0;                              /* right of the floor's tile span */

    /* Solid: return the floor's on-screen pixel Y, adjusted for the map's
     * current vertical scroll offset. `m->offset % 16` is C's truncating
     * `%` (result takes the dividend's sign) against the compile-time
     * constant divisor 16 -- verified against the original's bit-level
     * idiom for signed modulo by a power of two, and re-verified
     * empirically by the offline equivalence check across positive,
     * negative and exact-multiple offsets. */
    return cy - (m->offset % 16) + 10000 - (((cy + 1) >> 4) << 4);
}
