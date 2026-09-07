/* handle_player_collision_original.c -- one of five collision-response
 * variants play() dispatches to through a jump table on `collision_type`
 * (VA 0x4dd140, values 0..4 -- confirmed by reading the dispatch site
 * itself, artifacts/disasm.txt 0x4125a3: `jmp *0x4d60c4(,%eax,4)` guarded
 * by `cmpl $0x4,collision_type; ja <default>`). All FIVE variants
 * (_original, _old, _combo, _vector, _vector_2) are live, each called from
 * exactly one case of that same jump table -- batch 6's "may be dead code"
 * hedge does not hold for any of them; this pass promotes _original only
 * (the smallest, and the one batch 6 had already fully hand-traced) and
 * leaves the other four for a future pass (headroom, not dead code).
 *
 * Skipped in batch 6 for one reason: a call to play_sound() with a
 * sound-handle global at VA 0x4dd300 "that has no DWARF-recovered name
 * anywhere". Also wrong, the same way play_jump_sound.c's header comment
 * documents: 0x4dd300 is sounds[8] -- DWARF names `sounds` as
 * `SAMPLE *sounds[9]` at VA 0x4dd2e0 (already in game_state.h), and
 * 0x4dd300 - 0x4dd2e0 == 0x20 == 8 * sizeof(SAMPLE*). The real blocker,
 * once that is seen, is the same one play_jump_sound.c had: play_sound()
 * is not yet promoted, so the offline harness needs the call-trace domain
 * (mechanism B) to verify it without executing it. See
 * src/icytower/names.json's _meta note and PROMOTIONS.md batch 7.
 *
 * Ignores both of its own parameters (confirmed: neither 8(%ebp) nor
 * 0xc(%ebp) is referenced anywhere in 0x407e10..0x407fd7) -- recovered
 * faithfully as unused, not removed; the other collision variants likely
 * DO use their candidate-position arguments (unverified, future pass).
 *
 * Body, in order (artifacts/disasm.txt 0x407e10..0x407fd7, hand-traced):
 *
 *  1. Two `is_solid(&map, (int)p->x -+ 11, (int)p->y)` foot probes (the
 *     truncations use the same round-to-zero FPU control word every other
 *     recovered physics function already sets up locally) -- left foot
 *     result into `any11`, right foot into `any12`; `any21`/`any22`/`any23`
 *     are unconditionally zeroed (their old values are never read again by
 *     this function -- likely consumed by one of the other four variants,
 *     matching the shared `any1*`/`any2*` naming scheme).
 *
 *  2. If BOTH feet are in the air (any11 + any12 == 0): if status is 2, or
 *     0, set status to 3 (start falling); any other status is left alone.
 *     Return -- this branch never plays a sound or moves the player.
 *
 *  3. Otherwise (at least one foot touching): if status is 1 or 2, do
 *     nothing (mid-jump or already falling-through-a-tile) and return.
 *
 *  4. If status is 0 (was already idle/grounded), OR anything else
 *     (falling, negative, ...) -- landing, or already-landed re-check:
 *     status other than 0 additionally plays the LANDING SOUND
 *     (`play_sound(sounds[8], 1, 1)`) first, then both cases fall into the
 *     SAME landing logic below (status was 0: silent re-settle; status was
 *     anything else: audible landing -- `sound_landing` gating, exactly as
 *     batch 6's skip note named it):
 *       - status = 0, sy = 0.0 (kill vertical velocity)
 *       - if the left foot touched something (any11 != 0): snap
 *         p->y -= (any11 - 0x270f); rotate = 0; edge = (any11 == any12) ?
 *         0 : 1 (the two feet agreeing on the same tile means a flat
 *         landing, edge 0; disagreeing means the left foot is the one on
 *         solid ground and the right is hanging off an edge, edge 1)
 *       - else if the right foot touched something (any12 != 0, any11 was
 *         0): snap p->y -= (any12 - 0x270f); rotate = 0; edge = 2 (only the
 *         right foot is on solid ground)
 *       - else (both zero -- unreachable given step 3's own any11+any12!=0
 *         invariant, but the original still has the code path, so it is
 *         reproduced faithfully): rotate = 0; edge = 0
 *
 * 0x270f (9999) is a fixed offset the original subtracts from the raw
 * is_solid() tile-Y result before applying it to p->y -- read directly off
 * the disassembly's `sub $0x270f,%eax` / `lea -0x270f(%ebx),%ecx`, not
 * derived from is_solid.c (is_solid's own promoted form returns this same
 * raw encoding; PROMOTIONS.md's is_solid.c does not decode it further,
 * since is_solid's own callers are what interpret it, exactly as here).
 *
 * Original source: F:\projects\icytower\trunk\source\main.c (per
 * artifacts/dwarf_info.txt), which does not name this function's two
 * parameters distinctly from its four other collision-variant siblings'
 * (all five share the same two-int signature at their call site).
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

void handle_player_collision_original(int a1, int a2)
{
    Tplayer *p;
    int left, right;

    (void)a1;
    (void)a2;

    p = ply[player_id];
    left = is_solid(&map, (int)p->x - 11, (int)p->y);
    right = is_solid(&map, (int)p->x + 11, (int)p->y);
    any11 = left;
    any12 = right;
    any23 = 0;
    any22 = 0;
    any21 = 0;

    if (left + right == 0) {
        if (p->status == 2 || p->status == 0)
            p->status = 3;
        return;
    }

    if (p->status == 1 || p->status == 2)
        return;

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
