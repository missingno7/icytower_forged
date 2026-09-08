/* scroller.c -- the Tscroller setup/step/reset trio from
 * F:\projects\icytower\trunk\source\scroller.c.  draw_scroller() lives
 * in its own file (batch 8); init_scroller() was added by batch 14.
 *
 * Original source/decl_line: scroller.c, decl_line 70 (scroll_scroller,
 * params sc/step) and decl_line 75 (restart_scroller, param sc)
 * (artifacts/dwarf_info.txt). No FPU in any of the three.
 */
#include "game_types.h"
#include "game_funcs.h"
#include "allegro_api.h"

#include <string.h>

/* scroll_scroller -- advance the scroller's current position by `step`
 * pixels. Recovered from artifacts/disasm.txt (0x41f0c0..0x41f0ce): a
 * single field add, nothing else.
 */
void scroll_scroller(Tscroller *sc, int step)
{
    sc->offset += step;
}

/* restart_scroller -- snap the scroller back to one of its two end
 * positions, depending on scroll direction. Recovered from
 * artifacts/disasm.txt (0x41f0d0..0x41f0eb): `horizontal == 0` (vertical
 * scroll) restarts at `height`; otherwise (horizontal scroll) it restarts
 * at `width`.
 */
void restart_scroller(Tscroller *sc)
{
    if (sc->horizontal == 0)
        sc->offset = sc->height;
    else
        sc->offset = sc->width;
}

/* ---------------------------------------------------------------------
 * init_scroller  (0x41f278, 200 bytes)
 * ---------------------------------------------------------------------
 * PROMOTIONS.md batch 14 -- one of the 19 play()-coastline functions
 * batch 13's closing table left ORIGINAL.  Recovered from
 * artifacts/disasm.txt 0x41f278..0x41f33f.
 *
 * Domain: the whole 2072-byte Tscroller (memory), plus the ordered trace
 * of its two Allegro calls -- text_height() is called on EVERY path and
 * text_length() only on the horizontal one, so which of the two ran is
 * itself part of what the oracle compares.
 *
 * Field offsets used by the object code map onto game_types.h's
 * Tscroller exactly: horizontal +0x0, text +0x4, fnt +0x8, font_height
 * +0xc, width +0x10, height +0x14, offset +0x18, rows +0x1c, length
 * +0x20, lines[512] +0x24.
 *
 *   41f290..41f2b1  the six unconditional field stores.  GCC emitted
 *           them out of declaration order (fnt, font_height, height,
 *           horizontal, text, width) purely to keep text_height()'s
 *           argument live across the call; no store depends on another,
 *           so the source order below is the readable one.
 *   41f299  font_height = text_height(fnt)
 *
 *   HORIZONTAL (41f320, `horiz != 0`):
 *   41f32a  length = text_length(sc->fnt, t)   -- note it re-reads the
 *           field it just wrote rather than reusing the argument.
 *   41f335  offset = sc->width                 -- so a horizontal
 *           scroller starts one screen-width off the right edge, which
 *           is what restart_scroller() above independently snaps back
 *           to.
 *
 *   VERTICAL (41f2b6):
 *   41f2bf  len = strlen(t)                    (inline `repnz scasb`)
 *   41f2c4  lines[0] = t; rows = 1             -- set BEFORE the scan,
 *           so a text with no newline at all still has one row.
 *   41f2e4  scan for '\n'.  On each hit, and only while `rows <= 511`
 *           (41f2ed's `cmp $0x1ff` / `jg`, i.e. the 512-entry array's
 *           last usable index), it records lines[rows] = text + i + 1,
 *           increments rows, and OVERWRITES the newline with '\0'
 *           (41f304).  The input string is destroyed in place and split
 *           into `rows` NUL-terminated lines -- the caller's buffer is
 *           an out-parameter, not just an input.
 *           Past 511 rows the newline is left ALONE (not even
 *           NUL-terminated), because the guard skips the whole body, not
 *           just the array store.
 *   41f313  offset = sc->height
 *
 *   The scan re-loads `sc->text` from the struct on every iteration
 *   (41f2e1/41f301) instead of caching it -- the compiler cannot prove
 *   the `sc->text[i] = 0` store does not alias `sc->text` itself.  Same
 *   value every time; noted only because it makes the loop look like it
 *   is doing more than it is.
 */
void init_scroller(Tscroller *sc, FONT *f, char *t, int w, int h, int horiz)
{
    int len;
    int i;

    sc->horizontal = horiz;
    sc->text = t;
    sc->fnt = f;
    sc->font_height = text_height(f);
    sc->width = w;
    sc->height = h;

    if (horiz) {
        sc->length = text_length(sc->fnt, t);
        sc->offset = sc->width;
        return;
    }

    len = (int)strlen(t);
    sc->lines[0] = t;
    sc->rows = 1;
    for (i = 0; i < len; i++) {
        if (sc->text[i] == '\n' && sc->rows <= 511) {
            sc->lines[sc->rows] = sc->text + i + 1;
            sc->rows++;
            sc->text[i] = '\0';
        }
    }
    sc->offset = sc->height;
}
