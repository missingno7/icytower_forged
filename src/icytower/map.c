/* map.c -- Tmap producer/consumer functions from
 * F:\projects\icytower\trunk\source\map.c, grouped in one file per that
 * shared CU (is_solid.c, the other map.c consumer, was recovered earlier
 * and keeps its own file -- see src/icytower/is_solid.c and
 * notes/promotion_candidates.md #2/#3/#6). Batch 3 added get_level(), the
 * third consumer sharing the same row-lookup idiom. This pass adds
 * add_floor() (the producer's write side, 608 bytes, 0x4167dc..0x416a3c) --
 * see its own header comment below for the generation rules, and
 * notes/layout_rules_1.5.1.md for the prose writeup and the SAME/CHANGED/
 * UNKNOWN comparison against 1.3's new_floor().
 */
#include <stdlib.h>
#include "game_types.h"
#include "game_funcs.h"

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

/* floor_size_modifiers -- static per-difficulty width offset, indexed by
 * get_demo()->floor_size (VA 0x4bdb60, DWARF-confirmed `int[5]`). The
 * original does NOT range-check floor_size against this table before
 * indexing it -- notes/layout_rules_1.5.1.md documents why the offline
 * check restricts floor_size to [0,4] rather than reproducing that
 * out-of-bounds read.
 */
static const int floor_size_modifiers[5] = { 2, 0, -2, -4, -6 };

/* add_floor -- the tower layout generator. Called 30 times in a row from
 * new_game() (after reset_map()) to build the initial floors, then on
 * demand from play(), at most once per game tick, as the player's climbed
 * height crosses into not-yet-generated territory. Every call shifts the
 * 32-row ring buffer m->room[] up by one (room[i] = room[i+1] for i<31),
 * discarding the oldest (lowest, now off-screen) row and freeing room[31]
 * as the slot this call fills in.
 *
 * GENERATION RULES (1.5.1, recovered from artifacts/disasm.txt
 * 0x4167dc..0x416a3c; see notes/layout_determinism.md SS2 for the KNOWN
 * rand()-count table this restates as code, and notes/layout_rules_1.5.1.md
 * for the full prose writeup and the point-by-point comparison against
 * 1.3's new_floor(), assets/replay_checker/Icy Tower.cpp):
 *
 * - m->room[31].level IS the generator's own floor-index counter: read
 *   before the shift (as this call's floor index, k), and written back as
 *   k+1 on every reachable return -- no wall-clock or tick input anywhere
 *   in this function.
 * - m->room[31].tiles is set unconditionally, before anything else below:
 *   a purely cosmetic value (no consumer in this file gates on it) --
 *   k/500, capped at 10 once k>4999 (so it reaches its cap exactly where
 *   it would have hit it anyway: 4999/500 == 9).
 * - Row cadence: 1 floor in 5 is an actual platform; the other 4 are empty
 *   filler rows (empty=-1, level=k+1, no tiles drawn, no rand()).
 * - Full-width CHECKPOINT floors (start_tile=0, end_tile=40, no rand())
 *   pre-empt the row cadence: every 250th floor up to k<=5004, then every
 *   2500th floor beyond that -- (k%250==0 && k<=5004) || k%2500==0. Every
 *   50th checkpoint additionally gets a visible "sign" (the new level/5);
 *   every other checkpoint, and every non-checkpoint floor, has sign=0.
 * - A real floor (k%5==0, not a checkpoint) draws a WIDTH in tiles, in one
 *   of three ways depending on get_demo()->floor_shrink:
 *     floor_shrink==0            width = 6 + rand()%10                (1 rand)
 *     floor_shrink!=0, new k<=2999
 *                                 rand() is drawn unconditionally first;
 *                                 width = 6 unless a float ratio derived
 *                                 from k is >=1.0, in which case
 *                                 width = 6 + (that rand()) % (int)ratio
 *                                 -- see the source below for the exact
 *                                 arithmetic (x87-sensitive: verify with
 *                                 the GCC toolchain, notes/
 *                                 layout_rules_1.5.1.md)                (1 rand, always drawn)
 *     floor_shrink!=0, new k>2999 a fixed schedule by height: 6 (k<=5004),
 *                                 5 (k<=7504), 4 (k<=10004), 3 (k<50005),
 *                                 2 (else)                             (0 rand)
 *   width is then adjusted by floor_size_modifiers[get_demo()->floor_size];
 *   if the adjusted width is <=0 it is clamped to 1 with a fixed 29-wide
 *   placement range, otherwise the placement range is 30-width. Either
 *   way, exactly one more rand() places it: start_tile = 5 + rand()%range,
 *   end_tile = start_tile+width. So a real floor draws 2 rand() calls when
 *   floor_shrink==0 or (floor_shrink!=0 && new k<=2999), 1 otherwise.
 *
 * Original source: F:\projects\icytower\trunk\source\map.c, decl_line 27
 * (artifacts/dwarf_info.txt), which names the parameter m and two locals
 * that survive the whole function: i (the shift-loop index, decl_line 28)
 * and width (the floor's tile length, decl_line 29) -- kept as named
 * there. A third local, max_w (decl_line 83, scoped to the lexical block
 * around the placement rand()), is the rand()%max_w modulus for
 * start_tile. No other local survived DWARF: every struct field and every
 * get_demo() result is read straight off memory at each use, matching the
 * disassembly's repeated reloads exactly -- including THREE separate
 * get_demo() calls (0x4168ff, 0x416933, 0x4169a1), not one cached
 * pointer, and the width+floor_size_modifiers sum being computed twice
 * (once at 0x416945 just to test its sign, again at 0x4169ac to actually
 * add it) -- both recovered faithfully, not "fixed" into a cached value,
 * matching this project's established practice (see add_combo.c's own
 * note on the same kind of redundancy).
 *
 * The rand() calls below are ordinary, unqualified C -- this file knows
 * nothing about where they land, exactly like every other name in src/.
 * Worth knowing anyway, because it cost a real in-vivo divergence
 * (notes/living_record.md 008): add_floor is the game's ONLY live gameplay
 * consumer of libc rand(), so this stream IS the tower layout, and it must
 * be the SAME stream the rest of the game draws from -- the one
 * new_game()'s srand(Treplay.random_seed) seeds. Whoever compiles this
 * file is responsible for making `rand` mean the game's rand: the offline
 * harness does it with pf_harness_rand.h's per-vector LCG shim, the
 * carrier does it by binding the name to the guest's own msvcrt IAT slot
 * (carrier/gen/gen_bindings.py's GUEST_CRT_IMPORTS). A build that resolves
 * it to its own C library instead compiles, links, runs, and generates a
 * perfectly plausible -- but different -- tower.
 *
 * tiles' and sign's divisors (500 and 5) are each a magic-multiply
 * reciprocal in the disassembly (0x10624dd3>>5 and 0x66666667>>1), NOT a
 * plain `idiv` with a literal constant -- an early hand-read guessed 20
 * and 10 by eyeballing the magic constants against a standard-magic-
 * number table, and both guesses were WRONG. The offline check's first
 * 20000-vector run (this pass), after a separate harness bug was fixed
 * (get_demo()'s result reaching add_floor as an untranslated guest
 * pointer -- notes/layout_rules_1.5.1.md's "verification" section),
 * reported a DIFFER at Tmap+0x2fc (tiles): both divisors were then
 * re-derived correctly by brute-force testing the magic-multiply
 * arithmetic against every plausible small divisor over a 0..20000 sweep,
 * the same technique particle.c's create_particle() note describes for
 * its own two magic constants.
 */
void add_floor(Tmap *m)
{
    int i, width, max_w;

    for (i = 0; i < 31; i++)
        m->room[i] = m->room[i + 1];

    m->room[31].tiles = (m->room[31].level <= 4999) ? m->room[31].level / 500 : 10;

    if ((m->room[31].level % 250 == 0 && m->room[31].level <= 5004) ||
        m->room[31].level % 2500 == 0) {
        m->room[31].empty = 0;
        m->room[31].level++;
        m->room[31].start_tile = 0;
        m->room[31].end_tile = 40;
        m->room[31].sign = ((m->room[31].level - 1) % 50 == 0) ? m->room[31].level / 5 : 0;
        return;
    }

    if (m->room[31].level % 5 != 0) {
        m->room[31].empty = -1;
        m->room[31].level++;
        m->room[31].sign = ((m->room[31].level - 1) % 50 == 0) ? m->room[31].level / 5 : 0;
        return;
    }

    m->room[31].empty = 0;
    m->room[31].level++;

    if (get_demo()->floor_shrink == 0) {
        width = 6 + rand() % 10;
    } else if (m->room[31].level <= 2999) {
        /* rand() is drawn here unconditionally -- the disassembly's
         * 0x4169d3 call always executes, and its result (r1) is used
         * below only if ratio>=1.0; discarding it otherwise still
         * advances the RNG stream exactly as the original does. */
        int r1 = rand();
        int q = m->room[31].level / -5 + 300;             /* fidivrl's memory operand */
        float ratio = (float)q / 300.0f * 10.0f;           /* fidivr then fmuls, kept on
                                                              * the x87 stack in the
                                                              * original -- see the header
                                                              * comment's x87 note */
        width = (ratio < 1.0f) ? 6 : 6 + r1 % (int)ratio;   /* (int) cast truncates toward
                                                              * zero, matching the
                                                              * original's explicit
                                                              * round-to-zero fistp */
    } else if (m->room[31].level <= 5004) {
        width = 6;
    } else if (m->room[31].level <= 7504) {
        width = 5;
    } else if (m->room[31].level <= 10004) {
        width = 4;
    } else if (m->room[31].level < 50005) {
        width = 3;
    } else {
        width = 2;
    }

    if (width + floor_size_modifiers[get_demo()->floor_size] > 0) {
        width += floor_size_modifiers[get_demo()->floor_size];
        max_w = 30 - width;
    } else {
        width = 1;
        max_w = 29;
    }
    m->room[31].start_tile = 5 + rand() % max_w;
    m->room[31].end_tile = m->room[31].start_tile + width;
    m->room[31].sign = ((m->room[31].level - 1) % 50 == 0) ? m->room[31].level / 5 : 0;
}
