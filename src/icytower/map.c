/* map.c -- Tmap producer/consumer functions from
 * F:\projects\icytower\trunk\source\map.c, grouped in one file per that
 * shared CU (is_solid.c, the other map.c consumer, was recovered earlier
 * and keeps its own file -- see src/icytower/is_solid.c and
 * notes/promotion_candidates.md #2/#3/#6). Batch 3 adds get_level(), the
 * third consumer sharing the same row-lookup idiom; add_floor() (the
 * producer's write side, 608 bytes) is not attempted this pass.
 */
#include "game_types.h"

/* getFloorData -- like is_solid(), row-index the fixed 32-row Tmap.room[]
 * array from a pixel Y, but instead of a single solid/not-solid answer,
 * hand back the floor's left/right pixel edges and its on-screen Y through
 * three int* outputs. Writes nothing if the row is out of range or empty
 * (matching is_solid's bounds/empty checks exactly -- both functions do
 * the same `y = 29 - ((cy+1)>>4)` row lookup).
 *
 * Original source: F:\projects\icytower\trunk\source\map.c, decl_line 148
 * (artifacts/dwarf_info.txt), which names the parameters m, cy, fy, fx1,
 * fx2 and the local y -- kept as named there.
 *
 * Recovered from artifacts/disasm.txt (0x416770..0x4167db) instruction by
 * instruction; carrier/lift/lifted/lifted_getFloorData.c (generated
 * mechanically from the same bytes) was used to cross-check the derivation.
 * No FPU involved -- pure integer arithmetic, so no x87 fidelity question.
 */
void getFloorData(Tmap *m, int cy, int *fy, int *fx1, int *fx2)
{
    int y = 29 - ((cy + 1) >> 4);
    if (y < 0 || y > 31)
        return;                                /* off the top or bottom: leave outputs untouched */

    if (m->room[y].empty != 0)
        return;                                /* no floor drawn in this row */

    *fx1 = m->room[y].start_tile * 16 - 2;
    *fx2 = m->room[y].end_tile * 16 + 17;
    /* on-screen Y: the row's pixel top (cy+1 rounded down to a 16px tile
     * boundary), adjusted by the map's current vertical scroll offset. The
     * original computes `m->offset % 16` via a sign-preserving AND/OR bit
     * trick; is_solid.c already established (and the offline equivalence
     * check confirms again here) that plain C truncating `%` by a
     * compile-time power of two reproduces that idiom exactly on this
     * target. */
    *fy = (((cy + 1) >> 4) << 4) + (m->offset % 16);
}

/* reset_map -- (re)initialize a Tmap to "no floors anywhere": marks every
 * one of the 32 rows empty (empty = -1, matching is_solid's/getFloorData's
 * `!= 0` empty test -- any nonzero works, and the original chose -1, not
 * 0), zeroes `level` and `sign`, and resets the scroll offset. Deliberately
 * leaves `start_tile`/`end_tile`/`tiles` stale: every consumer gates on
 * `empty` first and never reads them for an empty row.
 *
 * Original source: F:\projects\icytower\trunk\source\map.c, decl_line 11,
 * which names the parameter m and the loop local i.
 *
 * Recovered from artifacts/disasm.txt (0x4166a4..0x4166d9); cross-checked
 * against carrier/lift/lifted/lifted_reset_map.c. No FPU, no return value.
 */
void reset_map(Tmap *m)
{
    int i;

    for (i = 0; i < 32; i++) {
        m->room[i].empty = -1;
        m->room[i].level = 0;
        m->room[i].sign = 0;
    }
    m->offset = 0;
}

/* get_level -- report the `level` tag of the floor at pixel row `cy`,
 * using the same `y = 29 - ((cy+1)>>4)` row lookup as is_solid()/
 * getFloorData() (out-of-range rows -- the same `(unsigned)y > 31` bounds
 * test -- return 0, not the row's stale/empty data).
 *
 * Unlike is_solid()/getFloorData(), this one does not gate on
 * `room[y].empty`: `level` is read even for an empty row (empty rows are
 * left with stale `level` by reset_map() only at map-reset time, so a
 * caller checking `level` on a still-unpopulated row already knows to
 * ignore it by other means -- recovered faithfully, not "fixed").
 *
 * Original source: F:\projects\icytower\trunk\source\map.c, decl_line 139
 * (artifacts/dwarf_info.txt), which names the parameters m and cy.
 * Recovered from artifacts/disasm.txt (0x416748..0x41676f). No FPU.
 */
int get_level(Tmap *m, int cy)
{
    int y = 29 - ((cy + 1) >> 4);
    if (y < 0 || y > 31)
        return 0;

    return m->room[y].level;
}
