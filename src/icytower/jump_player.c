/* jump_player.c -- start (or force) a player's jump, called from
 * handle_player_input() when the jump key/button is pressed, and from the
 * "cheat"/debug bounce path.
 *
 * Recovered by hand-simulating the x87 register-stack traffic in
 * artifacts/disasm.txt (0x418678..0x41873e) instruction by instruction --
 * this function is the win32_pilot.md SS3 x87 HYPOTHESIS case, so every
 * `fld`/`fxch`/`fucom`/`fstp` was traced to keep the arithmetic in its
 * original order. Cross-checked against the already-generated LIFTED form
 * (carrier/lift/lifted/lifted_jump_player.c, which the same disassembly
 * produced mechanically) and, per win32_pilot.md SS3's resolution of the
 * x87 question, verified offline with pf_x87_t = double: EQUAL over 200000
 * vectors (four seeds) in carrier/lift/harness -- this function's only FP
 * ops are `sx+sx`, `sx*-2.0` and comparisons, all exact under any x87
 * precision setting, so double is provably enough here (unlike
 * line_intersect.c, which needs the software 80-bit backend).
 *
 * Original source: F:\projects\icytower\trunk\source\player.c, decl_line 72
 * (artifacts/dwarf_info.txt), which names the parameters `p` and `cheat`.
 *
 * Two entry paths:
 *
 * 1. `cheat != 0` (debug/forced jump, "bounce"): unconditionally marks the
 *    player jumping and sets a launch speed derived only from `cheat`
 *    (sy = -(cheat*12), via the `fildl`/`fstpl` pair at 0x4186a6..0x4186a9),
 *    ignoring horizontal speed and the per-surface floor below entirely.
 *    Returns -1. If the player is already jumping (`status != 0`), this
 *    path re-arms it anyway -- it is a forced override, not a request.
 *
 * 2. `cheat == 0` (the normal jump key): a no-op returning 0 if the player
 *    is already jumping (`status != 0`). Otherwise marks the player jumping
 *    and computes the vertical launch speed from the current horizontal
 *    speed `sx`: `-2*sx` when sx is non-negative, `2*sx` when sx is
 *    negative (both branches are exactly `-2*fabs(sx)`, but the original
 *    keeps the branch structure and this file mirrors it rather than
 *    algebraically simplifying, so the operation order matches the
 *    disassembly exactly). That candidate speed is then floored (in the
 *    "more negative is stronger" sense) against `-max_speed[collision_type]`,
 *    the guaranteed-minimum launch speed for whatever the player jumped off
 *    of: if the candidate is already stronger (more negative) than the
 *    floor it is kept unchanged (momentum is rewarded); otherwise the floor
 *    is used. `max_s` is set to the *unclamped* horizontal speed `sx` in
 *    both cases (0x4186f1/0x4186f6's `fst`/`fstp` pair), and if the chosen
 *    launch speed is below -22.0 the jump-spin animation is armed
 *    (`rotate = 1`); `angle` is always reset to 0. Returns -1.
 */
#include "game_types.h"
#include "game_state.h"

int jump_player(Tplayer *p, int cheat)
{
    if (cheat != 0) {
        /* forced/debug jump: launch straight up at a speed derived only
         * from `cheat`, regardless of current status or horizontal speed. */
        p->status = 1;
        p->sy = (double)(-(cheat * 12));
        return -1;
    }

    if (p->status != 0)
        return 0;                      /* already jumping/falling: no-op */

    p->status = 1;

    {
        double sx = p->sx;
        double candidate = (sx + sx >= 0.0) ? sx * -2.0 : sx + sx;
        double floor_speed = -max_speed[collision_type];

        /* keep the candidate if it is already stronger (more negative)
         * than the guaranteed-minimum floor; otherwise use the floor. */
        p->sy = (floor_speed > candidate) ? candidate : floor_speed;
        p->max_s = sx;

        if (p->sy < -22.0)
            p->rotate = 1;              /* fast enough upward: start the jump-spin animation */
        p->angle = 0;
    }

    return -1;
}
