/* screenshot.c -- take_screenshot(), the F1 key's PNG writer.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c.
 *
 * On the gameplay tick path: play() calls it four times (batch 12's
 * census row `take_screenshot original 4 source 4`), the first of them
 * inside the tick body at play.c line 4064, guarded by
 * `if (!itrcheck && key[KEY_F1])`.  Batch 11 named it ORIGINAL with the
 * blocker "take_screenshot writes a PNG to disk; same class" as
 * log2file's FILE *.
 *
 * The blocker is real for a MEMORY domain and irrelevant for the
 * call-trace domain batches 7/9/10 built: what this function does is a
 * fixed, ORDERED sequence of Allegro and CRT calls whose arguments are
 * entirely determined by two inputs (`number__take_screenshot` and the
 * BITMAP's w/h), plus one write to a game global.  Both halves are
 * diffable:
 *
 *   memory domain     `number__take_screenshot` (0x4dd334, `int`, a
 *                     function-static DWARF names and game_state.h
 *                     already declares) -- how far the filename search
 *                     advanced.
 *   call-trace domain the ordered sprintf / exists / [log2file] /
 *                     get_palette / create_sub_bitmap / save_bitmap /
 *                     destroy_bitmap sequence with its arguments, and
 *                     the FILENAME each one saw, resolved to its content
 *                     at call time (draw_frame_xcheck.py's convention --
 *                     `name` is rewritten every loop iteration, so a
 *                     deferred read would compare only the last one).
 *
 * ---------------------------------------------------------------------
 * take_screenshot  (0x41002c, 203 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x41002c..0x4100f6.  Prototype
 * from the generated game_funcs.h: `void take_screenshot(BITMAP *)`.
 *
 *   410048  n = number__take_screenshot; number__take_screenshot = n + 1
 *   41005f  sprintf(name, "screenshots/icytower_%04d.png", n)
 *           -- the format string at 0x4d5aac, read with pefile.  %04d,
 *              so the first file is icytower_0000.png.
 *   410067  taken = exists(name)              (Allegro's file test)
 *   410072  number__take_screenshot > 9999
 *              -> log2file("*** Too many screenshots in the screenshot
 *                 folder! Delete some and try again.")  and RETURN
 *              (message at 0x4d5acc, read with pefile)
 *   41007c  taken != 0 -> loop back to 410048
 *   410087  get_palette(pal)                  (PALETTE = RGB[256])
 *   4100ac  sub = create_sub_bitmap(bmp, 0, 0, bmp->w, bmp->h)
 *   4100be  save_bitmap(name, sub, pal)       (Allegro picks PNG by
 *                                              extension via libpng3,
 *                                              which the PE imports)
 *   4100c6  destroy_bitmap(sub)
 *   4100cc  while (key[KEY_F1]) ;             -- a bare spin, no rest():
 *           the function does not return until the key is released, so
 *           holding F1 does not fill the folder.  0x5069b7 is
 *           `key + 0x2f`; the Allegro `key[]` array's base is 0x506988
 *           (COFF symbol `_key`, .bss RVA 0x29988 + section base
 *           0x4dd000), and 0x2f == 47 == KEY_F1, which allegro_api.h
 *           already #defines as 0x2f.  Cross-checked against play.c's
 *           own call site, which spins on the SAME key immediately
 *           after calling this function.
 *
 * ORDER, and why the loop is written as a `for (;;)` rather than the
 * `do { } while (exists(name))` it looks like: in the emitted code
 * `exists()` is called BEFORE the >9999 test on every iteration,
 * including the last one -- so on the "too many" path the observable
 * sequence is sprintf, exists, log2file, not sprintf, log2file.  GCC
 * cannot have introduced that call (it would be a call on a path the
 * source did not have one on), so the original really evaluates
 * `exists(name)` first.  Two source shapes produce exactly this and
 * cannot be told apart from the object code -- this `for (;;)`, and a
 * `do { } while (exists(name) && number <= 9999);` followed by a
 * post-loop `if (number > 9999)`.  They are observationally identical,
 * the oracle proves the sequence either way, and this file spells the
 * one that makes the ordering explicit rather than incidental.
 *
 * The two stack buffers: `sub $0x52c` with the PALETTE at -0x518 (768
 * bytes) and the name buffer at -0x118.  The 256 bytes between them are
 * not addressed by any instruction in the function; `name` is sized 256
 * here, which is what the gap implies and is in any case far more than
 * the 30-byte maximum the format string can produce.
 */
#include <stdio.h>              /* sprintf */

#include "allegro_api.h"        /* BITMAP, PALETTE, KEY_F1, key[], exists,
                                   get_palette, create_sub_bitmap,
                                   save_bitmap, destroy_bitmap */
#include "game_types.h"
#include "game_state.h"         /* number__take_screenshot */
#include "game_funcs.h"         /* log2file */

void take_screenshot(BITMAP *bmp)
{
    PALETTE pal;
    char name[256];
    BITMAP *sub;
    int taken;

    for (;;) {
        sprintf(name, "screenshots/icytower_%04d.png",
                number__take_screenshot++);
        taken = exists(name);
        if (number__take_screenshot > 9999) {
            log2file("*** Too many screenshots in the screenshot folder! "
                     "Delete some and try again.");
            return;
        }
        if (!taken)
            break;
    }

    get_palette(pal);
    sub = create_sub_bitmap(bmp, 0, 0, bmp->w, bmp->h);
    save_bitmap(name, sub, pal);
    destroy_bitmap(sub);

    while (key[KEY_F1])
        ;
}
