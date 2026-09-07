/* add_jump_sequence.c -- append one jump-sequence record to a Tgame_data,
 * the game_data.c sibling of add_combo.c (same CU, same shape: a bounded
 * ring-less append into a fixed-size array gated by its own running count).
 *
 * Original source: F:\projects\icytower\trunk\source\game_data.c,
 * decl_line 182 (artifacts/dwarf_info.txt), which names the parameters gd
 * and js.
 *
 * Recovered from artifacts/disasm.txt (0x4040f4..0x40414a) instruction by
 * instruction: `jumpPosts` is read three times (once per field, never
 * cached across the three stores -- like add_combo's three re-reads of
 * comboPosts) to compute the same `jumps[jumpPosts]` element address for
 * each of Tgd_jump_sequence's three fields (start, dist, num), in the
 * order num, dist, start; then jumpPosts is incremented. Tgd_jump_sequence
 * has no padding (three plain `int`s, per add_combo.c's already-verified
 * Tgd_combo precedent), so a single struct assignment reproduces the same
 * bytes as the three separate field stores. Guarded by
 * `jumpPosts <= 4999` (jumps[5000]); no-op past that, exactly like
 * add_combo's `comboPosts > 4999` guard on combos[5000]. No FPU, no
 * return value.
 */
#include "game_types.h"

void add_jump_sequence(Tgame_data *gd, Tgd_jump_sequence *js)
{
    if (gd->jumpPosts > 4999)
        return;                            /* jumps[5000] is full: drop it */

    gd->jumps[gd->jumpPosts] = *js;
    gd->jumpPosts++;
}
