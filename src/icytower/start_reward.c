/* start_reward.c -- pick a reward tier (0..9) from a combo/points value,
 * optionally spawn a confetti particle shower and load that tier's reward
 * bitmap, and always play that tier's reward sound.
 *
 * Skipped in batch 6 (PROMOTIONS.md) for the asset-seam computed-index gap
 * src/icytower/ASSETS.md's "What remains hand-mapped" table already
 * flagged: `data[0x5a + i]` at VA 0x407c7d, base object 90 (ASSET_DATA_
 * REWARD_000), "base only, not the 10-wide range". That range turns out to
 * already be fully, contiguously generated -- src/icytower/assets_table.inc
 * rows 114-123 are ASSET_DATA_REWARD_000..ASSET_DATA_REWARD_009 at object
 * indices 90..99, generated in one block by carrier/gen/gen_assets.py from
 * the manifest's own consecutive REWARD000..REWARD009 object names, and
 * carrier/gen/check_assets.py's "real-file order" check already confirms
 * table index N really is object N in the real datafile for every row.
 * `ASSET_DATA_REWARD_000 + tier` (tier in [0,9]) is therefore a safe
 * mechanical offset INTO that one generator-guaranteed contiguous family --
 * not the general "asset_id == raw table index" equivalence
 * src/icytower/ASSETS.md's own "nothing here or in clean code is allowed
 * to rely on that" warns against (this file never assumes the enum's
 * ABSOLUTE numeric value, only that ten ids generated together, in one
 * pass, from one manifest object range, stay adjacent to each other).
 *
 * Recovered from artifacts/disasm.txt (0x407c38..0x407e0b), hand-traced
 * instruction by instruction:
 *
 *  1. reward_time = 0x50 (80); reward_scale = 0 (Q16.16 `fixed`, so this is
 *     0.0) -- unconditional, first thing the function does.
 *
 *  2. tier (`esi` in the disassembly) is derived from the argument
 *     (`points`) by a cascade of `cmp`/`jle` thresholds read directly off
 *     0x407c58..0x407d03: <=6 -> 0, <=14 -> 1, <=24 -> 2, <=34 -> 3,
 *     <=49 -> 4, <=69 -> 5, <=99 -> 6, <=139 -> 7, <=199 -> 8, >199 -> 9.
 *
 *  3. If `itrcheck` (the headless replay-checker flag) is 0:
 *       reward_bmp = asset_bitmap(ASSET_DATA_REWARD_000 + tier) (data[N] by
 *         the original, decoded through the ASSET seam per
 *         src/icytower/ASSETS.md instead) -- runs UNCONDITIONALLY whenever
 *         itrcheck is 0, independent of `options.flash`/tier (found the
 *         hard way this pass: a first reading had this nested INSIDE the
 *         flash!=0 branch, which the 20000-vector offline check's very
 *         first DIFFER -- reward_bmp wrong on a flash==0 vector --
 *         immediately caught; re-tracing 0x407c69..0x407d0b byte by byte
 *         shows `test options.flash,options.flash; je 407d08` skips
 *         STRAIGHT PAST the reward_bmp code (0x407c77) only when flash is
 *         NONZERO, landing at 0x407d08's own `cmp $2,tier; jle 407c77` --
 *         which itself falls through to reward_bmp when tier<=2 -- so
 *         EVERY path from itrcheck==0 reaches 0x407c77 exactly once,
 *         regardless of flash or tier).
 *       if `options.flash` is 0 AND tier > 2: spawn (tier - 2) * 16
 *         confetti particles into `stars[512]` via
 *         create_particle(stars, 0x140, 0x168) (320,360 -- a fixed
 *         screen-space spawn point), each one immediately given its own
 *         `sy`/`sx` (Q16.16 fixed) from two more new_rand() draws:
 *           sy = -((((new_rand() % 500) + 500) << 16) / 100)
 *           sx = ((((new_rand() % 1000) - 500) << 16) * (tier - 2)) / 100
 *         (both divisions are plain C `/` -- the disassembly's two
 *         magic-multiply sequences both use particle.c's create_particle()
 *         `/50` constant, 0x51eb851f, but with `sar $0x5` (shift 5), not
 *         create_particle's own shift 4 -- one more correction-shift bit
 *         halves the quotient, i.e. these two are `/100`, not `/50`.
 *         `sy`'s correction step is also NOT the same shape as `sx`'s or
 *         create_particle.c's own two magic divisions: it computes
 *         `sign_of_dividend - quotient_guess` (register/memory operand
 *         order reversed from the usual `quotient_guess - sign_of_dividend`
 *         -- `sub -0x20(%ebp),%edx` vs `sx`'s `sub %edx,-0x20(%ebp)`,
 *         opposite AT&T operand order, opposite destination), which negates
 *         the whole result relative to plain truncating division -- found
 *         and confirmed by directly executing JUST this instruction block
 *         (0x407d49-0x407d70) in unicorn with controlled register input
 *         over a dozen positive/negative new_rand() values and comparing
 *         against a Python model of both the negated and non-negated
 *         reading, not by re-reading the mnemonics a second time (an
 *         AT&T-operand-order misread is easy to make twice in a row).
 *         `sx`'s own block (0x407d76-0x407dab) was independently re-checked
 *         the same way and confirmed NOT negated, matching this file's
 *         first reading. A first version of this file had `sy` unnegated,
 *         `/50` for both, and options.flash/reward_bmp nested wrong (see
 *         above); the 20000-vector offline check caught all three,
 *         one DIFFER at a time.
 *         create_particle()'s return value (a slot index, or 0 on a miss --
 *         particle.c's own PROMOTIONS.md entry) indexes `stars[]` directly,
 *         exactly as create_particle.c's own header comment already
 *         documents its return value is meant to be used. `options.flash`
 *         gating the confetti loop OFF when nonzero (not on) reads
 *         backwards for a field named "flash", but the disassembly is
 *         unambiguous and this file follows it, not the name's connotation
 *         -- plausibly a "reduce flashing effects" accessibility toggle,
 *         unconfirmed and not needed to recover the behaviour correctly.
 *     play_sound(combo_sound[tier], 0, 0) -- ALWAYS runs, whether or not
 *     the itrcheck gate above ran (the disassembly's skip path also falls
 *     through into this one shared call site, 0x407c8b).
 *
 *  4. return tier.
 *
 * `itrcheck` (0x4dd168, notes/asset_census.md's headless replay-checker
 * mode) and `options.flash` (Toptions's first field, VA 0x4fe528) are
 * both already-DWARF-named globals/fields already in game_state.h/
 * game_types.h -- neither needed the names.json mechanism either.
 *
 * play_sound() is not yet promoted, so -- exactly like play_jump_sound.c
 * and handle_player_collision_original.c -- this function's own
 * play_sound() call is verified offline through the call-trace domain
 * (mechanism B), not by executing play_sound() itself.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c (per
 * artifacts/dwarf_info.txt), which names the one parameter `points`.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "assets.h"

int start_reward(int points)
{
    int tier;
    int i, count, slot;

    reward_time = 0x50;
    reward_scale = 0;

    if (points <= 6)
        tier = 0;
    else if (points <= 14)
        tier = 1;
    else if (points <= 24)
        tier = 2;
    else if (points <= 34)
        tier = 3;
    else if (points <= 49)
        tier = 4;
    else if (points <= 69)
        tier = 5;
    else if (points <= 99)
        tier = 6;
    else if (points <= 139)
        tier = 7;
    else if (points <= 199)
        tier = 8;
    else
        tier = 9;

    if (!itrcheck) {
        if (!options.flash && tier > 2) {
            count = tier - 2;
            for (i = 0; i < count * 16; i++) {
                slot = create_particle(stars, 0x140, 0x168);
                stars[slot].sy = -((((new_rand() % 500) + 500) << 16) / 100);
                stars[slot].sx = ((((new_rand() % 1000) - 500) << 16) * count) / 100;
            }
        }
        reward_bmp = asset_bitmap((asset_id)(ASSET_DATA_REWARD_000 + tier));
    }
    play_sound(combo_sound[tier], 0, 0);

    return tier;
}
