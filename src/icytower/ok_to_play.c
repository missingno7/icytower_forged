/* ok_to_play.c -- trivial gate checked before starting a game.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c, decl_line 1013
 * (artifacts/dwarf_info.txt). Recovered from artifacts/disasm.txt
 * (0x406a50..0x406a59): `mov eax,1; leave; ret` -- unconditional,
 * no arguments, no globals read or written. Left out of batch 3 only for
 * headroom (PROMOTIONS.md's "Skipped this pass" table), not for any
 * recovery difficulty.
 */

int ok_to_play(void)
{
    return 1;
}
