/* particle.c -- Tparticle producer from F:\projects\icytower\trunk\source\particle.c.
 * Only reset_particles() is recovered this pass: update_particle() and
 * create_particle() both call new_rand() (0x406984, main.c) -- an x87
 * float LCG with its own control-word manipulation, not yet promoted --
 * so neither can be verified by the offline memory-domain harness yet
 * (calling into a not-yet-promoted game function would mean executing a
 * copy of its bytes from the harness's plain, non-executable `malloc`
 * buffer, per carrier/lift/harness/src_check.c's own header comment on
 * why that buffer is deliberately not VirtualAlloc'd executable). Left
 * for a future pass once new_rand() itself is promoted.
 *
 * Original source: F:\projects\icytower\trunk\source\particle.c,
 * decl_line 34 (artifacts/dwarf_info.txt), which names the parameter p.
 *
 * Recovered from artifacts/disasm.txt (0x418420..0x41843b): `p` here is
 * not one particle but the base of a fixed 512-element array (the loop's
 * byte stride is 0x18 = sizeof(Tparticle), and it runs for 0x3000 bytes =
 * 512 * 24), and only each element's first field (`intensity`, offset 0)
 * is cleared -- x/y/sx/sy/color are left stale. (create_particle(),
 * per artifacts/disasm.txt 0x418490 area, scans for the first particle
 * with `intensity == 0` before reusing a slot, so a zeroed `intensity` is
 * exactly the "free slot" marker this function resets every entry to.)
 * No FPU, no return value.
 */
#include "game_types.h"

void reset_particles(Tparticle *p)
{
    int i;

    for (i = 0; i < 512; i++)
        p[i].intensity = 0;
}
