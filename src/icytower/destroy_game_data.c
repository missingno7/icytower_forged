/* destroy_game_data.c -- frees a Tgame_data allocated by create_game_data().
 *
 * Original source: F:\projects\icytower\trunk\source\game_data.c (same CU
 * as add_combo.c/add_jump_sequence.c).
 *
 * Recovered from artifacts/disasm.txt (0x40418c..0x404197):
 *
 *   40418c: push %ebp
 *   40418d: mov  %esp,%ebp
 *   40418f: sub  $0x8,%esp
 *   404192: leave
 *   404193: jmp  4bad08 <_free>          -- tail call, no `call`/`ret` of
 *                                           its own; free()'s own ret pops
 *                                           straight back into THIS
 *                                           function's caller.
 *
 * batches 3/6/7/8/9 deferred this: "its only observable effect is
 * heap-allocator-internal state with no comparison domain the offline
 * harness can express (unlike a memory write, freeing a block leaves no
 * game-owned bytes to diff)". That is still true of a MEMORY domain, but
 * it is exactly the call-trace domain batch 9 built for vtable calls and
 * batch 7 built for play_sound: "which pointer reached free()'s argument"
 * is a call-trace-expressible fact even though freeing itself is not a
 * diffable memory write. carrier/lift/harness/destroy_game_data_check.py
 * (new, this batch) runs the ORIGINAL bytes under unicorn with a
 * UC_HOOK_CODE breakpoint at free()'s own entry VA (4bad08, an IAT thunk
 * to msvcrt free -- never actually executed; the hook fires first and the
 * emulation is stopped there, so the block is never really freed and the
 * malloc bookkeeping this offline harness does not model is never
 * touched) and records EAX/ESP's pointer argument at that point; the
 * compiled candidate below is linked against a stub `free()` doing the
 * same capture. Both sides must report the SAME pointer value (the `gd`
 * argument, untranslated -- destroy_game_data itself never dereferences
 * it, so no host/guest translation happens inside the function, only in
 * the harness's own free() stub, matching play_sound's existing
 * call-trace convention of comparing the pre-translation VALUE).
 *
 * No return value, no other global touched, no FPU.
 */
#include "game_types.h"
#include <stdlib.h>             /* free */

void destroy_game_data(Tgame_data *gd)
{
    free(gd);
}
