/* hisc.c -- the high-score table's three pure list operations.
 *
 * Original source: F:\projects\icytower\trunk\source\hisc.c.
 *
 * PROMOTIONS.md batch 14.  All three are on play()'s COASTLINE (the
 * game-over half that runs after the tick loop exits), which batch 13's
 * closing table named as the remaining ORIGINAL surface:
 * `qualify_hisc_table` and `enter_hisc_table` are called from play()
 * directly; `sort_hisc_table` is enter_hisc_table's partner (play()
 * calls it at 0x413f0b's neighbourhood, and view_scores/draw_table read
 * the result).
 *
 * None of the three calls Allegro, the CRT (beyond one strcpy) or any
 * other game function, so all three are plain MEMORY-domain functions:
 * the comparison domain is the whole 5-entry `Thisc posts[5]` array
 * (180 bytes) plus the return value.  No FPU anywhere -- checked
 * instruction by instruction over 0x404994..0x404a4f and
 * 0x405790..0x405817 in artifacts/disasm.txt.
 *
 * Types (game_types.h, DWARF-derived):
 *     Thisc        char name[32]; unsigned int value;      -- 36 bytes
 *     Thisc_table  char name[32]; Thisc *posts;            -- posts @ +0x20
 * Both match the disassembly exactly: stride 0x24 and `value` at +0x20
 * in every one of the three functions, and `mov 0x20(%eax),%edx` for
 * `table->posts`.
 *
 * The table is FIVE entries everywhere: 0x4049af `cmp $0x5,%eax`,
 * 0x4057db `cmp $0x5,%eax`, 0x404a34 `cmpl $0x5,-0x48(%ebp)`.  There is
 * no length field; 5 is compiled in.
 */
#include "game_types.h"
#include "game_funcs.h"

#include <string.h>

#define HISC_POSTS 5

/* ---------------------------------------------------------------------
 * qualify_hisc_table  (0x404994, 39 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x404994..0x4049ba.
 *
 *   404997  value == 0 -> return 0 immediately (`test %ecx,%ecx; je`).
 *           A zero score never qualifies, even against an empty table.
 *   4049a6  `cmp %ecx,0x20(%edx)` / `jb` -- an UNSIGNED compare, matching
 *           `Thisc.value`'s `unsigned int` type.  The first slot whose
 *           stored value is strictly LESS than `value` wins.
 *   4049b8  the hit path is `inc %eax; leave; ret`, so the return is the
 *           1-BASED position (1..5), not the array index.  The miss path
 *           at 4049b4 returns 0 -- the same value the `value == 0`
 *           early-out returns, which is why callers can treat 0 as "did
 *           not qualify" without distinguishing the two.
 *
 * The parameter is declared `int value` by the generated game_funcs.h
 * (DWARF), but the comparison against the `unsigned` field is unsigned,
 * so the cast is written out here rather than left implicit.
 */
int qualify_hisc_table(Thisc_table *table, int value)
{
    Thisc *posts;
    int i;

    if (value == 0)
        return 0;

    posts = table->posts;
    for (i = 0; i < HISC_POSTS; i++) {
        if (posts[i].value < (unsigned int)value)
            return i + 1;
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * sort_hisc_table  (0x4049bc, 147 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x4049bc..0x404a4e.  A textbook
 * insertion sort, DESCENDING by `value`, over the same fixed 5 entries.
 *
 *   4049e4  `rep movsl` with %ecx = 9 -- the 36-byte `Thisc` moved as one
 *           whole-struct assignment, three times (into the temporary, to
 *           shift an element right, and out of the temporary).  Written
 *           here as three struct assignments for the same reason
 *           add_combo.c's own note gives: `Thisc` is 32 + 4 with no
 *           padding, so a struct copy is bit-identical, not merely
 *           equivalent.
 *   404a04  `cmp %ebx,(%eax)` / `jae` -- again UNSIGNED, and the loop
 *           STOPS on `>=`, i.e. the shift continues only while the
 *           element to the left is strictly smaller.  That makes the sort
 *           STABLE: equal scores keep their existing order, so a newly
 *           entered score that ties an old one stays below it.
 *   404a1f  the "ran off the front" exit writes the temporary to
 *           posts[0]; 404a44 computes j*36 for the ordinary exit.  Both
 *           reach the same store through 404a21.
 *
 * The compiler keeps `tmp.value` alive in %ebx across the inner loop and
 * writes it back at 404a21 (`mov %ebx,-0x10(%ebp)`) -- a register
 * allocation artifact of the `rep movsl` clobbering the temporary's
 * home, not an observable step.
 */
void sort_hisc_table(Thisc_table *table)
{
    Thisc *posts;
    Thisc tmp;
    int i, j;

    posts = table->posts;
    for (i = 1; i < HISC_POSTS; i++) {
        tmp = posts[i];
        for (j = i; j > 0 && posts[j - 1].value < tmp.value; j--)
            posts[j] = posts[j - 1];
        posts[j] = tmp;
    }
}

/* ---------------------------------------------------------------------
 * enter_hisc_table  (0x405790, 136 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x405790..0x405817.
 *
 * This function does NOT insert in sorted position -- it OVERWRITES one
 * slot and leaves re-ordering to sort_hisc_table().  Which slot is the
 * whole content of the function, and the search is more subtle than it
 * looks:
 *
 *   4057b5  a running threshold starts at 0x989680 == 10 000 000 -- the
 *           game's hard score ceiling, larger than any real entry.
 *   4057ba/4057ce  `cmp %ebx,%edx` / `jb`: an entry is a CANDIDATE only
 *           when its value is strictly less than the running threshold,
 *           and taking a candidate lowers the threshold to that entry's
 *           own value (4057cc `mov %edx,%ebx`).  A NON-candidate leaves
 *           the threshold untouched (4057d5 `mov %ebx,%edx` immediately
 *           before 4057cc reloads it -- a no-op round trip).
 *   4057c1  the LAST candidate wins (%esi is overwritten, not kept).
 *
 * On a table sorted descending -- which is the only state
 * sort_hisc_table() leaves it in -- the threshold decreases at every
 * step and the winner is slot 4, the lowest score.  On a partly-filled
 * table (trailing zeroes) the first zero is a candidate and the ones
 * after it are not, so the winner is the FIRST FREE slot.  Both are the
 * intended behaviour, and both fall out of the same three lines.
 *
 *   4057e0  `cmp $0xffffffff,%esi` -- if nothing was ever a candidate
 *           (every entry already at or above 10 000 000, i.e. all five
 *           saturated) the function writes NOTHING and returns.
 *   405808  the store is `posts[pos].value = value` and then a TAIL CALL
 *           into strcpy for the name; the destination address is
 *           recomputed from `table->posts` (4057fb) rather than reusing
 *           the cached pointer -- same address, so no behaviour rides on
 *           it.
 *
 * `value` is stored raw into the `unsigned int` field; the threshold
 * comparisons are unsigned throughout (jb/jae), so the search itself is
 * written against the unsigned field type.
 */
void enter_hisc_table(Thisc_table *table, int value, char *name)
{
    Thisc *posts;
    unsigned int threshold;
    int i, pos;

    posts = table->posts;
    pos = -1;
    threshold = 10000000u;
    for (i = 0; i < HISC_POSTS; i++) {
        if (posts[i].value < threshold) {
            pos = i;
            threshold = posts[i].value;
        }
    }
    if (pos == -1)
        return;

    posts[pos].value = (unsigned int)value;
    strcpy(table->posts[pos].name, name);
}
