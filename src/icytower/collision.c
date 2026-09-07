/* collision.c -- the four remaining `handle_player_collision_*` variants
 * (main.c), completing the five-way family whose first member
 * (`handle_player_collision_original`, 0x407e10) is already promoted in
 * src/icytower/handle_player_collision_original.c.
 *
 * ==========================================================================
 * WHICH VARIANT THE GAME ACTUALLY RUNS
 * ==========================================================================
 * `play()` dispatches through one jump table on `collision_type`
 * (VA 0x4dd140): artifacts/disasm.txt 0x412591 guards `cmpl $0x4,
 * collision_type; ja <allegro_message>` and 0x4125a3 is `jmp
 * *0x4d60c4(,%eax,4)`. The table itself (read out of the image at 0x4d60c4,
 * 5 dwords) resolves to:
 *
 *     collision_type == 0  ->  handle_player_collision_original  (0x407e10)
 *     collision_type == 1  ->  handle_player_collision_old       (0x407fd8)
 *     collision_type == 2  ->  handle_player_collision_vector    (0x408d08)
 *     collision_type == 3  ->  handle_player_collision_vector_2  (0x4088c8)
 *     collision_type == 4  ->  handle_player_collision_combo     (0x408358)
 *
 * `collision_type` has exactly ONE store anywhere in the image:
 * `new_game()` (0x40dc9c) opens with `movl $0x2,0x4dd140` at 0x40dcb0,
 * unconditionally, before anything else it does (verified by searching
 * artifacts/disasm.txt for every reference to 0x4dd140 -- the other five
 * hits are the two `play()` dispatch guards and three reads in
 * jump_player/update_player, all loads). There is no options field, no
 * .ini key, no Treplay field and no command-line switch that reaches it.
 *
 * So **the operator's recording -- and every other run of this build --
 * uses `handle_player_collision_vector`**. The other four are live in the
 * sense batch 7 established (each is the target of a distinct, reachable
 * jump-table case, none is dead code the linker could have dropped), but
 * the selector that would pick them is a compile-time-fixed 2. They are
 * recovered here for completeness of the CU, and because a variant that is
 * one `collision_type = N` away from being live is worth having correct.
 *
 * ==========================================================================
 * THE THREE ALGORITHMS
 * ==========================================================================
 * The five variants are three different collision algorithms, not five
 * spellings of one:
 *
 *  (a) TWO STATIC FOOT PROBES -- `_original`: `is_solid(&map, (int)p->x-11,
 *      (int)p->y)` and `... +11 ...`, nothing else. See
 *      handle_player_collision_original.c.
 *
 *  (b) FOOT PROBES + ONE MIDPOINT RE-PROBE -- `_old`. Batch 8's skip note
 *      called this "a genuine iterative bisection loop"; **that reading was
 *      wrong** and this pass corrects it. The three backward branches batch
 *      8 cited (0x408052 from 0x408131, 0x408066 from 0x40813e, and the
 *      0x4081b0/0x4081b8 `neg` blocks) are GCC's tail-duplicated arms of two
 *      plain if/else chains plus two inlined `abs()`es -- there is no
 *      loop-carried variable and no convergence: the function probes ONCE at
 *      the current position and, only if the player moved DOWN by at least
 *      two pixels, ONCE more at the midpoint between the previous and the
 *      current position. It is a two-sample swept check, cruder than (c).
 *
 *  (c) LINE SWEEP -- `_combo`, `_vector`, `_vector_2`. Ask `getFloorData()`
 *      for the floor segment nearest the player's row, then intersect that
 *      horizontal segment with the two vertical-ish segments the player's
 *      left and right foot travelled along since the previous frame
 *      (`line_intersect()`, already promoted). The intersection X is what
 *      the player is snapped to, so a fast fall cannot tunnel through a
 *      floor the way (a) can. `_combo` = (a) first, then (c) if (a) did not
 *      land; `_vector` = (c) alone plus a +-10000 sanity guard;
 *      `_vector_2` = (c) alone, re-swept once at `fy + 4` if the first
 *      sweep missed, and without the sanity guard.
 *
 * ==========================================================================
 * THE DEBUG OVERLAY (all three sweep variants)
 * ==========================================================================
 * Each sweep variant draws its own floor segment and its two probe segments
 * on `screen` when `debug && key[KEY_F2]`. The original does this with a
 * direct `call *0x34(bmp->vtable)` -- offset 0x34 in `GFX_VTABLE` is
 * `line` (allegro_types.h; batch 8 already identified 0x3c as `rectfill`
 * from the same table), and `bmp->vtable` is `BITMAP` offset 0x1c. That is
 * exactly what Allegro 4's `line()` expands to: it is an `AL_INLINE` whose
 * whole body is `bmp->vtable->line(bmp, x1, y_1, x2, y2, color)`
 * (third_party/allegro-4.4.3.1/include/allegro/inline/draw.inl:72), and
 * carrier/gen/pf_lib_bindings.h already models exactly that -- `line` is
 * one of its 23 generated "AL_INLINE vtable-dispatch macros", spelled
 * `#define line(a0,...) ((a0)->vtable->line((a0), ...))`. So this file
 * writes the ordinary `line(screen, ...)` the original source wrote; the
 * carrier world's macro and the upstream world's real AL_INLINE both
 * expand it back to the one indirect call the bytes make. (The one world
 * that does NOT supply it is the generated, no-bindings
 * src/icytower/allegro_api.h -- a real gap in that generator's non-binding
 * half, reported in PROMOTIONS.md batch 9 and worked around, harness-only,
 * by the same macro in carrier/lift/harness/pf_harness_calltrace.h.)
 *
 * `debug` (VA 0x4dd160, `extern int debug` in game_state.h) has NO store
 * anywhere in the image: all fourteen references to 0x4dd160 in
 * artifacts/disasm.txt are loads or `cmpl $0x0,...`. It lives in .bss, so
 * it is 0 for the entire life of the process and the overlay is
 * unreachable in vivo. It is recovered anyway, and the offline harness
 * verifies it directly: a fifth of every function's vectors set `debug`
 * and `key[KEY_F2]` non-zero and compare the resulting `makecol` and
 * vtable-`line` call traces (PROMOTIONS.md batch 9 "the debug overlay").
 *
 * ==========================================================================
 * SHARED READING NOTES
 * ==========================================================================
 * - `0x270f` (9999) is the same fixed offset `_original` subtracts from a
 *   raw `is_solid()` result before applying it to `p->y`; see that file's
 *   header comment. Only the two foot-probe algorithms use it -- the sweep
 *   snaps to `fy - 1` instead, straight from `getFloorData()`.
 * - `-12345678` (0xff439eb2) is the source's own "no floor here" sentinel:
 *   every sweep variant pre-seeds its `fy` local with it and tests for it
 *   after the call, because `getFloorData()` writes NOTHING when the row is
 *   out of range or empty (src/icytower/map.c).
 * - Every `(int)` cast below is the original's `fistpl` under a locally
 *   installed round-to-zero control word -- plain C truncation, the same
 *   idiom every other recovered physics function in this directory uses.
 * - `p->edge` is written as 0 when both probes agree (both hit or both
 *   miss), 1 when only the LEFT one hit, 2 when only the RIGHT one hit.
 *   The original computes 1-vs-2 with the `cmp $1 / sbb / not / add $2`
 *   flag trick, which for `line_intersect`'s 0-or-1 return is exactly
 *   `i1 ? 1 : 2`.
 * - All four functions read `ply[player_id]` internally; neither of their
 *   two parameters is a player pointer. DWARF names the parameters `lastX`
 *   and `lastY` (carrier/gen/interop_index.json) -- the player's position
 *   on the PREVIOUS frame, which is what makes the sweep a sweep.
 *
 * Recovered from artifacts/disasm.txt instruction by instruction
 * (0x407fd8..0x408356, 0x408358..0x4088c6, 0x4088c8..0x408d06,
 * 0x408d08..0x409136), then cross-checked by executing the ORIGINAL bytes
 * in unicorn against a Python model of each algorithm -- the methodology
 * new_rand.c/create_particle.c/update_player.c already established for a
 * novel algorithm. 0 mismatches for all four over 200 random + 500
 * directed boundary vectors each before any C was written; see
 * PROMOTIONS.md batch 9.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c.
 */
#include "allegro_api.h"     /* screen, key[], KEY_F2, makecol, GFX_VTABLE */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

/* getFloorData()'s "nothing was written" sentinel -- see the header
 * comment. Spelled as the original spells it: a decimal magic number
 * pre-stored into the fy local before every call. */
#define FLOOR_NONE (-12345678)

/* The player's half-width: both feet are probed at x-11 and x+11. */
#define FOOT 11


/* ==========================================================================
 * handle_player_collision_old -- collision_type 1 (0x407fd8, 894 bytes)
 *
 * Foot probes at the CURRENT position, exactly as `_original`; then, if
 * the player moved down by at least two pixels since (lastX, lastY), one
 * more pair of probes at the midpoint of that move.
 *
 * The midpoint rounds TOWARDS (lastX, lastY): `mx = lastX -+ |lastX-ix|/2`,
 * with the sign chosen so the step goes from the previous position towards
 * the current one. (The disassembly writes the two arms asymmetrically --
 * a sign-correcting `shr $0x1f`/`add`/`sar` in one, a bare `sar` in the
 * other -- but the value being halved is the output of an inlined `abs()`
 * and so is never negative, where the two idioms agree bit for bit; and
 * even at INT_MIN, which is even, they still agree.)
 *
 * The re-probe gate is `my > lastY`, which -- since `my` always lies on
 * the (lastX,lastY)->current side -- is true exactly when the player's
 * integer Y grew by 2 or more, i.e. when it fell far enough that a single
 * sample at the destination could have missed a floor on the way.
 * ========================================================================== */
void handle_player_collision_old(int lastX, int lastY)
{
    Tplayer *p;
    int ix, iy, dx, dy, mx, my;
    int left, right;

    p = ply[player_id];

    ix = (int)p->x;
    dx = lastX - ix;
    if (dx < 0)
        dx = -dx;
    iy = (int)p->y;
    dy = lastY - iy;
    if (dy < 0)
        dy = -dy;

    if (ix < lastX)
        mx = lastX - dx / 2;
    else
        mx = lastX + dx / 2;
    if (iy < lastY)
        my = lastY - dy / 2;
    else
        my = lastY + dy / 2;

    /* --- pass 1: the two feet, where the player is right now --- */
    left = is_solid(&map, ix - FOOT, iy);
    right = is_solid(&map, ix + FOOT, iy);
    any11 = left;
    any12 = right;
    any23 = 0;
    any22 = 0;
    any21 = 0;

    if (left + right == 0) {
        if (p->status == 2 || p->status == 0)
            p->status = 3;                       /* start falling */
    } else if (p->status != 1 && p->status != 2) {
        /* landing (or an already-grounded re-settle when status was 0) --
         * this arm RETURNS; the midpoint re-probe below is only for the
         * "still in the air" and "mid-jump" cases. */
        if (p->status != 0)
            play_sound(sounds[8], 1, 1);
        p->status = 0;
        p->sy = 0.0;
        if (left != 0) {
            p->y -= (double)(left - 0x270f);
            p->rotate = 0;
            p->edge = (left == right) ? 0 : 1;
        } else if (right != 0) {
            p->y -= (double)(right - 0x270f);
            p->rotate = 0;
            p->edge = 2;
        } else {
            p->rotate = 0;
            p->edge = 0;
        }
        return;
    }

    /* --- pass 2: the midpoint of this frame's move, only when falling --- */
    if (my <= lastY)
        return;

    left = is_solid(&map, mx - FOOT, my);
    right = is_solid(&map, mx + FOOT, my);
    any21 = left;
    any22 = right;

    if (left + right == 0) {
        if (p->status == 2 || p->status == 0)
            p->status = 3;
        return;
    }
    if (p->status == 1 || p->status == 2)
        return;

    any23 = 1;                                   /* "landed on the re-probe" */
    if (p->status != 0)
        play_sound(sounds[8], 1, 1);
    p->status = 0;
    p->sy = 0.0;
    if (left != 0) {
        p->y -= (double)(left - 0x270f);
        p->rotate = 0;
        p->edge = (left == right) ? 0 : 1;
    } else if (right != 0) {
        p->y -= (double)(right - 0x270f);
        p->rotate = 0;
        p->edge = 2;
    } else {
        p->rotate = 0;
        p->edge = 0;
    }
}


/* ==========================================================================
 * handle_player_collision_vector -- collision_type 2 (0x408d08, 1071 bytes)
 *
 * THE VARIANT THE GAME RUNS (see the file header). Pure line sweep:
 *
 *   1. `getFloorData(&map, (int)p->y, &fy, &fx1, &fx2)` -- the floor
 *      segment in the player's own row. If that row has no floor, retry
 *      with the PREVIOUS row's Y (`lastY`); if that has none either, the
 *      player is in open air: status 0 or 2 becomes 3, and we are done.
 *   2. Intersect the floor segment (fx1,fy)-(fx2,fy) with each foot's
 *      travel segment ((ix-+11, iy+1) -> (lastX-+11, lastY)).
 *   3. `p->edge` from which feet hit; `p->status = 3` if neither did.
 *   4. If at least one hit AND the player was airborne (status 2 or 3),
 *      land: play the landing sound, kill sy, snap `p->y = fy - 1` and
 *      `p->x` so that the foot that hit sits on the intersection point.
 *
 * Step 4 has one extra guard the other two sweep variants do not have:
 * when BOTH feet hit, the two intersection X values must both lie within
 * +-10000, otherwise the whole landing is abandoned (the `p->edge` write
 * from step 3 still stands). That is this family's own defence against the
 * degenerate `line_intersect()` results notes/living_record.md divergence
 * 006 documents -- a fully-degenerate segment pair makes `ua`/`ub` NaN, the
 * original's `fistp` then yields "integer indefinite" (0x80000000), and
 * `x1 - 2147483648` lands far outside +-10000. The guard is reached in the
 * offline corpus (59 of 1500 directed vectors), not merely inferred.
 * ========================================================================== */
void handle_player_collision_vector(int lastX, int lastY)
{
    Tplayer *p;
    int ix, iy;
    int fy, fx1 = 0, fx2 = 0;                    /* pre-zeroed by the original */
    int i1, i2, hx1, hy1, hx2, hy2;

    p = ply[player_id];
    iy = (int)p->y;

    fy = FLOOR_NONE;
    getFloorData(&map, iy, &fy, &fx1, &fx2);
    if (fy == FLOOR_NONE) {
        getFloorData(&map, lastY, &fy, &fx1, &fx2);
        if (fy == FLOOR_NONE) {
            if (p->status == 2 || p->status == 0)
                p->status = 3;
            return;
        }
    }

    ix = (int)p->x;

    if (debug && key[KEY_F2]) {
        int c_floor = makecol(0xff, 0x00, 0x00);
        int c_probe = makecol(0xff, 0xff, 0x00);
        line(screen, fx1, fy, fx2, fy, c_floor);
        line(screen, ix - FOOT, iy + 1, lastX - FOOT, lastY, c_probe);
        line(screen, ix + FOOT, iy + 1, lastX + FOOT, lastY, c_probe);
    }

    i1 = line_intersect(fx1, fy, fx2, fy,
                        ix - FOOT, iy + 1, lastX - FOOT, lastY, &hx1, &hy1);
    i2 = line_intersect(fx1, fy, fx2, fy,
                        ix + FOOT, iy + 1, lastX + FOOT, lastY, &hx2, &hy2);

    if (i1 + i2 == 0) {
        if (p->status == 2 || p->status == 0)
            p->status = 3;
    }

    if (i1 == i2)
        p->edge = 0;                             /* both hit, or both missed */
    else
        p->edge = i1 ? 1 : 2;

    if (i1 + i2 == 0)
        return;
    if (p->status != 2 && p->status != 3)
        return;                                  /* not airborne: nothing to land */

    if (i1 != 0 && i2 != 0) {
        /* both feet crossed the floor -- reject a degenerate intersection */
        if (hx1 < -10000 || hx2 < -10000 || hx1 > 10000 || hx2 > 10000)
            return;
    }

    play_sound(sounds[8], 1, 1);
    p->status = 0;
    p->sy = 0.0;
    p->y = (double)(fy - 1);
    if (i1 != 0)
        p->x = (double)(hx1 + FOOT);             /* left foot on the crossing */
    else
        p->x = (double)(hx2 - FOOT);             /* right foot on the crossing */
    p->rotate = 0;
}


/* ==========================================================================
 * handle_player_collision_vector_2 -- collision_type 3 (0x4088c8, 1086 B)
 *
 * `_vector` with three differences, all read straight off the bytes:
 *
 *  1. The two `makecol()` calls are UNCONDITIONAL, at the very top of the
 *     function, outside the `debug` gate (0x408944/0x408963) -- so this
 *     variant calls into Allegro twice per frame even with the overlay off.
 *     `_vector` puts them inside the gate; `_combo`, like this one, does
 *     not.
 *  2. When NEITHER `getFloorData()` probe finds a floor it does not return:
 *     it zeroes fx1/fx2 and carries on with `fy` still holding the
 *     FLOOR_NONE sentinel, so the sweep runs against a degenerate segment
 *     at y = -12345678 that (barring a crafted map) never intersects.
 *  3. If the first sweep misses with BOTH feet, it sweeps a second time
 *     against a floor line four pixels lower (`fy + 4`) -- a second chance
 *     for a foot that passed just under the floor's own line. Note the
 *     landing still snaps to `fy - 1`, the FIRST line's Y, not `fy + 3`:
 *     the original re-reads its untouched `fy` local (0x408be2), while
 *     only the register copy was incremented.
 *
 * There is no +-10000 guard here.
 * ========================================================================== */
void handle_player_collision_vector_2(int lastX, int lastY)
{
    Tplayer *p;
    int ix, iy;
    int fy, fx1 = 0, fx2 = 0;
    int c_floor, c_probe;
    int i1, i2, hx1, hy1, hx2, hy2;

    p = ply[player_id];
    ix = (int)p->x;
    iy = (int)p->y;

    c_floor = makecol(0xff, 0x00, 0x00);
    c_probe = makecol(0xff, 0xff, 0x00);

    fy = FLOOR_NONE;
    getFloorData(&map, (int)p->y, &fy, &fx1, &fx2);
    if (fy == FLOOR_NONE) {
        getFloorData(&map, lastY, &fy, &fx1, &fx2);
        if (fy == FLOOR_NONE) {
            fx1 = 0;
            fx2 = 0;
        }
    }

    if (debug && key[KEY_F2]) {
        line(screen, fx1, fy, fx2, fy, c_floor);
        line(screen, ix - FOOT, iy + 1, lastX - FOOT, lastY, c_probe);
        line(screen, ix + FOOT, iy + 1, lastX + FOOT, lastY, c_probe);
    }

    i1 = line_intersect(fx1, fy, fx2, fy,
                        ix - FOOT, iy + 1, lastX - FOOT, lastY, &hx1, &hy1);
    i2 = line_intersect(fx1, fy, fx2, fy,
                        ix + FOOT, iy + 1, lastX + FOOT, lastY, &hx2, &hy2);
    if (i1 == 0 && i2 == 0) {
        i1 = line_intersect(fx1, fy + 4, fx2, fy + 4,
                            ix - FOOT, iy + 1, lastX - FOOT, lastY, &hx1, &hy1);
        i2 = line_intersect(fx1, fy + 4, fx2, fy + 4,
                            ix + FOOT, iy + 1, lastX + FOOT, lastY, &hx2, &hy2);
    }

    if (i1 + i2 == 0) {
        if (p->status == 2 || p->status == 0)
            p->status = 3;
    }

    if (i1 == i2)
        p->edge = 0;
    else
        p->edge = i1 ? 1 : 2;

    if (i1 + i2 == 0)
        return;
    if (p->status != 2 && p->status != 3)
        return;

    play_sound(sounds[8], 1, 1);
    p->status = 0;
    p->sy = 0.0;
    p->y = (double)(fy - 1);
    if (i1 != 0)
        p->x = (double)(hx1 + FOOT);
    else
        p->x = (double)(hx2 - FOOT);
    p->rotate = 0;
}


/* ==========================================================================
 * handle_player_collision_combo -- collision_type 4 (0x408358, 1390 bytes)
 *
 * The largest of the five, and the only one that runs TWO algorithms:
 * `_original`'s two static foot probes first (writing any11/any12 and
 * clearing any21/any22/any23 exactly as `_original` does), and -- only if
 * that did not land the player -- `_vector_2`'s line sweep, minus the
 * `fy + 4` retry and minus `_vector`'s +-10000 guard.
 *
 * The two `makecol()` calls are unconditional and come FIRST, before even
 * the foot probes, so this variant calls Allegro twice even on the frames
 * where the foot probes land and it returns early.
 * ========================================================================== */
void handle_player_collision_combo(int lastX, int lastY)
{
    Tplayer *p;
    int ix, iy;
    int fy, fx1 = 0, fx2 = 0;
    int c_floor, c_probe;
    int left, right;
    int i1, i2, hx1, hy1, hx2, hy2;

    p = ply[player_id];

    c_floor = makecol(0xff, 0x00, 0x00);
    c_probe = makecol(0xff, 0xff, 0x00);

    /* --- phase 1: `_original`, verbatim --- */
    left = is_solid(&map, (int)p->x - FOOT, (int)p->y);
    right = is_solid(&map, (int)p->x + FOOT, (int)p->y);
    any11 = left;
    any12 = right;
    any23 = 0;
    any22 = 0;
    any21 = 0;

    if (left + right == 0) {
        if (p->status == 2 || p->status == 0)
            p->status = 3;
    } else if (p->status != 1 && p->status != 2) {
        if (p->status != 0)
            play_sound(sounds[8], 1, 1);
        p->status = 0;
        p->sy = 0.0;
        if (left != 0) {
            p->y -= (double)(left - 0x270f);
            p->rotate = 0;
            p->edge = (left == right) ? 0 : 1;
        } else if (right != 0) {
            p->y -= (double)(right - 0x270f);
            p->rotate = 0;
            p->edge = 2;
        } else {
            p->rotate = 0;
            p->edge = 0;
        }
        return;
    }

    /* --- phase 2: the line sweep --- */
    fy = FLOOR_NONE;
    getFloorData(&map, (int)p->y, &fy, &fx1, &fx2);
    if (fy == FLOOR_NONE) {
        getFloorData(&map, lastY, &fy, &fx1, &fx2);
        if (fy == FLOOR_NONE) {
            fx1 = 0;
            fx2 = 0;
        }
    }

    ix = (int)p->x;
    iy = (int)p->y;

    if (debug && key[KEY_F2]) {
        line(screen, fx1, fy, fx2, fy, c_floor);
        line(screen, ix - FOOT, iy + 1, lastX - FOOT, lastY, c_probe);
        line(screen, ix + FOOT, iy + 1, lastX + FOOT, lastY, c_probe);
    }

    i1 = line_intersect(fx1, fy, fx2, fy,
                        ix - FOOT, iy + 1, lastX - FOOT, lastY, &hx1, &hy1);
    i2 = line_intersect(fx1, fy, fx2, fy,
                        ix + FOOT, iy + 1, lastX + FOOT, lastY, &hx2, &hy2);

    if (i1 + i2 == 0) {
        if (p->status == 2 || p->status == 0)
            p->status = 3;
    }

    if (i1 == i2)
        p->edge = 0;
    else
        p->edge = i1 ? 1 : 2;

    if (i1 + i2 == 0)
        return;
    if (p->status != 2 && p->status != 3)
        return;

    play_sound(sounds[8], 1, 1);
    p->status = 0;
    p->sy = 0.0;
    p->y = (double)(fy - 1);
    if (i1 != 0)
        p->x = (double)(hx1 + FOOT);
    else
        p->x = (double)(hx2 - FOOT);
    p->rotate = 0;
}
