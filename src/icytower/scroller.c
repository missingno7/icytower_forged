/* scroller.c -- Tscroller step/reset pair from
 * F:\projects\icytower\trunk\source\scroller.c. draw_scroller() and
 * init_scroller() (the drawing/setup side, both Allegro-calling) are not
 * attempted this pass.
 *
 * Original source/decl_line: scroller.c, decl_line 70 (scroll_scroller,
 * params sc/step) and decl_line 75 (restart_scroller, param sc)
 * (artifacts/dwarf_info.txt). No FPU in either function.
 */
#include "game_types.h"

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
