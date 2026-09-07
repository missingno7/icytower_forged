/* update_player.c -- the per-tick player physics step: velocity clamp,
 * position integration, screen-edge wall bounce, and gravity. Called once
 * per player per tick from play() (the same caller as
 * handle_player_collision_*), after collision handling has had its chance
 * to change `sx`/`sy`/`status`.
 *
 * Recovered by hand-tracing the x87 register-stack traffic in
 * artifacts/disasm.txt (0x418740..0x4189cb) instruction by instruction --
 * dense, heavily-branched unordered-compare code (every comparison is an
 * `fucom(p(p))?`/`fnstsw`/`test $0x45,%ah` or `test $0x5,%ah` pair, per the
 * divergence-006 lesson in notes/living_record.md and the GCC x87 rule in
 * carrier/lift/harness/GCC_X87.md). Two things resisted quick reading and
 * were only nailed down by cross-checking a Python model against the
 * ORIGINAL bytes executed directly in unicorn over 20000 vectors (the same
 * technique new_rand.c/create_particle.c used):
 *
 *   1. `test $0x45,%ah` after an `fucom`/`fucomp` pair, read *without*
 *      C3 folded into "unordered", decodes to STRICT greater-than when
 *      paired with `je` (C3=1 on the EQUAL case is inside the 0x45 mask,
 *      so `je` only fires on the all-flags-clear GT case) and to
 *      "not(>), i.e. <= or unordered" when paired with `jne`. `test
 *      $0x5,%ah` (no C3 bit) is the non-strict >= pair instead. An early
 *      reading treated every `0x45` mask as a non-strict >=/<= NaN-guard
 *      like line_intersect's, which is right for the sy/sx *clamp*
 *      thresholds (-100.0/max_speed[collision_type], where the clamp
 *      target equals the compared constant so strict-vs-non-strict is
 *      unobservable) but WRONG for the screen-edge x thresholds
 *      (555.0/85.0) and the |bounce speed| >= 4.0 threshold, where the
 *      wall-clamp's *side effect* (the `sx *= -0.9` bounce) only fires on
 *      the strict side -- caught by the unicorn cross-check the moment a
 *      generated vector landed exactly on x==555.0.
 *   2. `x_new`/`y_new` (the position after `x += sx`/`y += sy`) are
 *      compared against 555.0/1000.0/85.0 *before* being rounded to a
 *      64-bit double a second time -- the `fstl` that stores them to
 *      `p->x`/`p->y` does not pop, so the comparison reads the same
 *      80-bit register the store rounded from memory's point of view but
 *      not the FPU's. Three vectors in the cross-check (engineered by the
 *      random generator, not sought out) had `x + sx` round to exactly
 *      555.0/85.0 as a 64-bit double while the original still took the
 *      wall-bounce branch -- the double-vs-80-bit gap already established
 *      for line_intersect/new_rand/add_floor, reproduced here in a
 *      comparison rather than a final truncation. Not fixable in a
 *      `double`-only Python model; GCC `-mfpmath=387` keeps `p->x + sx` in
 *      an x87 register across the comparison the same way the original
 *      does, so the C below (unchanged, ordinary `if (p->x > 555.0)`)
 *      reproduces it once compiled with real x87 arithmetic. Every other
 *      field (`sy`'s final gravity sum aside, see below) was bit-exact
 *      against the ORIGINAL over all 20000 cross-check vectors once the
 *      strict-vs-non-strict fix above landed.
 *
 * The `p->sy = 0.8 + gravity_modifier[...] + p->sy` chain at the end is
 * the third x87-sensitive spot: three values summed in FPU registers with
 * a single rounding at the final `fstl`, matching the operand order
 * (constant, then array lookup, then the old `sy`) the disassembly's
 * `fld`/`faddl`/`faddl` sequence pushes them in, left-associative in C the
 * same way. 529/20000 cross-check vectors differed here, always by
 * exactly one ULP -- the expected double-vs-80-bit gap (GCC_X87.md), not a
 * logic error (the unicorn cross-check flags a logic error as a
 * *different* field going wrong; only `sy` ever did after the fix above).
 *
 * Semantics: `sy` is clamped to [-100.0, max_speed[collision_type]] (a
 * hard vertical-speed cap, both up and down) and stored; `sx` is clamped
 * to [-max_speed[collision_type], max_speed[collision_type]] and stored;
 * both are then added into `x`/`y` (this function's own dead-simple
 * "move"). `y` is capped at 1000.0 (falling off the bottom of the tower).
 * `x` is bounced off the two screen edges (85.0 left, 555.0 right): the
 * position is clamped to the edge, `sx` is reversed and damped by 0.9, and
 * if the *rebounded* speed is at least 4.0 in magnitude the bounce is
 * "hard enough" to record in `p->bounce` (+-20, consumed elsewhere for
 * animation/sound -- this function never reads it back) -- below that
 * threshold `p->bounce` is left exactly as the caller set it. A hard
 * bounce with `status == 0` (not jumping/falling) returns immediately,
 * skipping gravity for this tick. Otherwise, unless `status == 0`, gravity
 * (a per-difficulty `gravity_modifier[get_demo()->gravity]` plus a fixed
 * 0.8) is added into `sy`; if the player was still in the launch phase
 * (`status == 1`) and the new `sy` has gone strictly positive (falling,
 * past the apex), `status` advances to 2.
 *
 * Original source: F:\projects\icytower\trunk\source\player.c, decl_line
 * 48 (artifacts/dwarf_info.txt), which names the parameter `p`.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

void update_player(Tplayer *p)
{
    double ms = max_speed[collision_type];

    double sy = p->sy;
    if (sy < -100.0)
        sy = -100.0;
    if (sy > ms)
        sy = ms;
    p->sy = sy;

    double sx = p->sx;
    if (sx < -ms)
        sx = -ms;
    if (sx > ms)
        sx = ms;
    p->sx = sx;

    p->x = p->x + sx;
    p->y = p->y + sy;

    if (p->y > 1000.0)
        p->y = 1000.0;

    if (p->x > 555.0) {
        p->x = 555.0;
        p->sx = p->sx * -0.9;
        if (p->sx < -4.0 || p->sx > 4.0) {
            p->bounce = -20;
            if (p->status == 0)
                return;
        }
    } else if (p->x < 85.0) {
        p->x = 85.0;
        p->sx = p->sx * -0.9;
        if (p->sx < -4.0 || p->sx > 4.0) {
            p->bounce = 20;
            if (p->status == 0)
                return;
        }
    }

    if (p->status == 0)
        return;

    p->sy = 0.8 + gravity_modifier[get_demo()->gravity] + p->sy;

    if (p->status == 1) {
        if (p->sy > 0.0)
            p->status = 2;
    }
}
