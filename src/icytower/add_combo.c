/* add_combo.c -- append one combo record to a Tgame_data, called from
 * play() whenever a jump combo finishes.
 *
 * Original source: F:\projects\icytower\trunk\source\game_data.c,
 * decl_line 193 (artifacts/dwarf_info.txt), which names the parameters
 * gd and c.
 *
 * Recovered from artifacts/disasm.txt (0x40414c..0x40418a); cross-checked
 * against carrier/lift/lifted/lifted_add_combo.c (generated mechanically
 * from the same bytes). The original writes the three Tgd_combo fields
 * (end, then start, then length -- an arbitrary field order the compiler
 * chose) through three separate `gd + comboPosts*12 + <field offset>`
 * stores, re-reading `comboPosts` before each one even though nothing
 * changes it in between; Tgd_combo has no padding (three plain `int`s), so
 * a single struct assignment reproduces the same bytes and is what is
 * written below. No FPU, no return value.
 */
#include "game_types.h"

void add_combo(Tgame_data *gd, Tgd_combo *c)
{
    if (gd->comboPosts > 4999)
        return;                            /* combos[5000] is full: drop it */

    gd->combos[gd->comboPosts] = *c;
    gd->comboPosts++;
}
