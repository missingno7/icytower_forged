/* particle.c -- Tparticle producer from F:\projects\icytower\trunk\source\particle.c.
 *
 * reset_particles() was recovered in an earlier pass (see below).
 * update_particle()/create_particle() were deferred then because both call
 * new_rand() (0x406984, main.c), which was not yet promoted; now that
 * new_rand.c exists in this directory as an ordinary compiled function,
 * both call it directly -- no guest-bytes execution trick needed, exactly
 * as PROMOTIONS.md's batch-3 "Skipped this pass" entry anticipated.
 *
 * Original source: F:\projects\icytower\trunk\source\particle.c,
 * decl_line 34 (reset_particles), 24 (update_particle), 7
 * (create_particle) (artifacts/dwarf_info.txt), all naming the particle
 * pointer parameter `p`.
 *
 * reset_particles(): recovered from artifacts/disasm.txt
 * (0x418420..0x41843b): `p` here is not one particle but the base of a
 * fixed 512-element array (the loop's byte stride is 0x18 = sizeof(Tparticle),
 * and it runs for 0x3000 bytes = 512 * 24), and only each element's first
 * field (`intensity`, offset 0) is cleared -- x/y/sx/sy/color are left
 * stale. A zeroed `intensity` is exactly the "free slot" marker
 * create_particle() below scans for. No FPU, no return value.
 *
 * update_particle(): recovered from artifacts/disasm.txt
 * (0x41843c..0x41848f), integer-only (no FPU at all -- x/y/sx/sy are the
 * `fixed` 16.16 Allegro type, plain int32 arithmetic, not float). Advances
 * one particle by its own velocity, applies a constant downward
 * acceleration to `sy` (0x4ccd, i.e. ~0.29981 in 16.16 fixed), ages it by
 * one tick (`intensity--`), and -- 1 time in 5 (`new_rand() % 5 == 1`) --
 * rerolls its `color` to `new_rand() % 8`. Cross-checked by direct
 * execution (not just disassembly reading): the original bytes were run
 * in unicorn over 2000 random (particle, seed) pairs and every resulting
 * particle AND every resulting `seed` matched this file's algorithm
 * exactly (0 mismatches) -- including catching, during that check, that a
 * first hand-transcription of the `% 5`/`% 8` idiom used Python's
 * floor-style `%` instead of C's truncating one, which the negative-input
 * vectors immediately exposed.
 *
 * create_particle(): recovered from artifacts/disasm.txt
 * (0x418490..0x41854e). Scans `p[0..511]` for the first `intensity == 0`
 * slot (same marker reset_particles() writes); if none is found, does
 * nothing and returns 0 (NOT -1 -- confirmed against the disassembly's
 * `xor si,si` "not found" path, not assumed). On a hit, it seeds that
 * slot from `x`/`y` (converted to 16.16 fixed via `<< 16`) and two more
 * new_rand() draws for `sx`/`sy`, sets `intensity = 0xff` (full) and
 * rerolls `color` from a third new_rand() draw, then returns the slot
 * index. The two magic-number integer divisions in the disassembly
 * (0x66666667/shift 2 and 0x51eb851f/shift 4) were NOT assumed to be
 * `/5` and `/10` by pattern-matching against update_particle.c's visually
 * similar `%5`/`%8` -- both were independently decoded by testing the
 * magic-multiply arithmetic against every plausible divisor and, because
 * that first guess (5, 10) was WRONG, re-derived from the actual
 * unicorn-executed field values: `sx`'s divisor is 10, `sy`'s is 50, not
 * the initially-assumed 5/10 (confirmed both analytically -- the magic
 * constants reproduce plain truncating `/10` and `/50` bit-for-bit over a
 * dense sweep -- and empirically, 0 mismatches over 2000 unicorn-executed
 * (array, x, y, seed) vectors covering both the found-a-slot and
 * no-free-slot paths).
 */
#include "game_types.h"
#include "game_funcs.h"

void reset_particles(Tparticle *p)
{
    int i;

    for (i = 0; i < 512; i++)
        p[i].intensity = 0;
}

void update_particle(Tparticle *p)
{
    p->x += p->sx;
    p->y += p->sy;
    p->sy += 0x4ccd;
    p->intensity--;

    if (new_rand() % 5 == 1)
        p->color = new_rand() % 8;
}

int create_particle(Tparticle *p, int x, int y)
{
    int i;

    for (i = 0; i < 512; i++) {
        if (p[i].intensity == 0) {
            p[i].x = x << 16;
            p[i].y = y << 16;
            p[i].sx = (((new_rand() % 50) - 25) << 16) / 10;
            p[i].sy = (((new_rand() % 50) - 25) << 16) / 50;
            p[i].intensity = 0xff;
            p[i].color = new_rand() % 8;
            return i;
        }
    }
    return 0;
}
