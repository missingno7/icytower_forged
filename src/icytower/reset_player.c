/* reset_player.c -- zero a Tplayer back to its "just spawned" state, called
 * from new_game() (both a fresh game and a replay start reset every player
 * this way before play() begins).
 *
 * Recovered by hand-tracing artifacts/disasm.txt (0x418550..0x418677): the
 * whole function is a straight-line sequence of `movl $imm,off(%eax)`
 * stores plus one `fldz`/`fstl`/`fstl`/`fstpl` group (x, sy and max_s share
 * the single pushed 0.0 -- `fstl` stores without popping, so the same
 * register feeds sx, sy, and finally max_s via the pop-and-store `fstpl`),
 * so there is no control flow and no ambiguity to resolve.
 *
 * `x`/`y` (offsets 0x0/0x8) are the two fields this function deliberately
 * does NOT touch -- new_game() sets the player's spawn position separately
 * (notes/player_start_randomness.md: x=200.0/y=431.0, written right after
 * this call), and `angle` (fixed, offset 0x54) is also left alone (only
 * `rotate` is cleared; the two are set together only by jump_player()).
 * `ccc[5]`/`jcTop[5]`/`jc[5]` (offsets 0x78/0x8c/0xa0, each 5 ints) are
 * zeroed in reverse index order (element 4 down to element 0) -- an
 * artifact of the compiler's store scheduling, not meaningful; this file
 * writes them in the natural forward order (the DWARF-recovered field
 * order in game_types.h), which the purity-safe assembly-independent
 * scheduling of an ordinary struct-field-store list has to reproduce
 * bit-for-bit regardless of the order the source lists them in.
 *
 * Original source: F:\projects\icytower\trunk\source\player.c, decl_line 18
 * (artifacts/dwarf_info.txt), which names the parameter `p`.
 */
#include "game_types.h"
#include "game_state.h"

void reset_player(Tplayer *p)
{
    p->sx = 0.0;
    p->sy = 0.0;
    p->status = 0;
    p->jump_key = 1;
    p->frame = 0;
    p->level = 0;
    p->in_combo = 0;
    p->acc_level = 0;
    p->acc_jumps = 0;
    p->score = 0;
    p->dead = 0;
    p->max_s = 0.0;
    p->rotate = 0;
    p->edge = 0;
    p->edge_drawn = 0;
    p->bounce = 0;
    p->shake = 0;
    p->latest_combo = 0;
    p->show_combo = 0;
    p->best_combo = 0;
    p->no_combo_top_floor = 0;
    p->biggest_lost_combo = 0;

    for (int i = 0; i < 5; i++) {
        p->ccc[i] = 0;
        p->jcTop[i] = 0;
        p->jc[i] = 0;
    }
}
