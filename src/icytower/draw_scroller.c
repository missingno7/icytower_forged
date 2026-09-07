/* draw_scroller.c -- draws a Tscroller: either a single line of
 * horizontally scrolling text (Tscroller.horizontal != 0) or a block of
 * vertically scrolling lines (Tscroller.horizontal == 0), clipped to a
 * (x,y)-(x+width,y+height) rectangle while drawing, then restored to the
 * bitmap's own full extent before returning.
 *
 * scroll_scroller()/restart_scroller() (src/icytower/scroller.c) already
 * cover this struct's step/reset pair; that file's own header comment
 * flagged this function as "not attempted this pass" -- attempted now.
 *
 * Recovered from artifacts/disasm.txt 0x41f0ec..0x41f277 (F:\projects\
 * icytower\trunk\source\scroller.c per DWARF); prototype and parameter
 * names from src/icytower/game_funcs.h: `int draw_scroller(Tscroller *sc,
 * BITMAP *bmp, int x, int y, int color)`.
 *
 * Gating, before anything is drawn or clipped (0x41f0f8-0x41f135):
 *   horizontal != 0: draws only if -sc->length <= sc->offset <= sc->width;
 *     otherwise returns 0 immediately, no clip rect touched, nothing called.
 *   horizontal == 0: draws only if -sc->rows*sc->font_height <= sc->offset
 *     <= sc->height; same immediate-0 return otherwise.
 *
 * Horizontal draw (0x41f1e1-0x41f243): set_clip_rect(bmp, x, y, x+width,
 *   y+height), then ONE textout_ex(bmp, sc->fnt, sc->text,
 *   x + sc->offset, y, color, -1) -- the whole string scrolls as a single
 *   call, transparent background (bg = -1).
 *
 * Vertical draw (0x41f137-0x41f1df): the SAME set_clip_rect(bmp, x, y,
 *   x+width, y+height), then one textout_centre_ex() per row i in
 *   [1, sc->rows] that survives TWO INDEPENDENT, differently-computed
 *   guards (0x41f18d-0x41f19f) -- not the same quantity tested twice:
 *   `top = (i-1)*font_height + offset` (the row's own top edge) must not
 *   exceed sc->height (0x41f193: `jg` skips if top > height), and
 *   `bottom = i*font_height + offset` (one font_height further down, the
 *   row's bottom edge) must not be negative (0x41f19f: `js` skips if
 *   bottom < 0) -- together, "does [top,bottom] intersect [0,height]".
 *   Rows failing either guard are skipped individually, not clipped; their
 *   textout_centre_ex call never happens at all. Each surviving row draws
 *   sc->lines[i-1], centred at x + width/2, at y = y + top, transparent
 *   background.
 *
 * Either drawn path ends the same way (0x41f244-0x41f26d): set_clip_rect
 *   restored to the bitmap's own full extent (0, 0, bmp->w-1, bmp->h-1),
 *   then returns -1. A gated-out call returns 0 and never touches the clip
 *   rect at all or calls anything -- recovered faithfully, not "fixed" to
 *   always restore.
 *
 * Verified offline (carrier/lift/harness/lift_check.py --form src
 * --toolchain gcc --funcs draw_scroller): this function writes no game
 * memory of its own (only reads sc/bmp fields and calls three Allegro
 * functions), so its whole comparison domain is the call-trace domain
 * (PROMOTIONS.md batch 8's "mechanism B" extended to set_clip_rect/
 * textout_ex/textout_centre_ex, none of which were previously in the
 * harness's LIB_CALL_TARGETS table) plus EAX. GCC x87 (win32_pilot.md
 * SS6a, -mfpmath=387 -mno-sse2 -O2): EQUAL, 20000/20000 -- this function
 * has no floating point of its own (all geometry is plain int), so no x87
 * precision question arises here the way it does for line_intersect/
 * new_rand/update_player; the GCC toolchain is used only because this
 * sandbox has no MSVC cl.exe available (see PROMOTIONS.md batch 8's own
 * note on toolchain availability).
 */
#include "allegro_api.h"
#include "game_types.h"

int draw_scroller(Tscroller *sc, BITMAP *bmp, int x, int y, int color)
{
    int i, row_y;

    if (sc->horizontal) {
        if (sc->offset < -sc->length || sc->offset > sc->width)
            return 0;

        set_clip_rect(bmp, x, y, x + sc->width, y + sc->height);
        textout_ex(bmp, sc->fnt, sc->text, x + sc->offset, y, color, -1);
    } else {
        if (sc->offset < -sc->rows * sc->font_height || sc->offset > sc->height)
            return 0;

        set_clip_rect(bmp, x, y, x + sc->width, y + sc->height);
        for (i = 1; i <= sc->rows; i++) {
            row_y = (i - 1) * sc->font_height + sc->offset;         /* top edge */
            if (row_y > sc->height)
                continue;                      /* scrolled off the bottom */
            if (i * sc->font_height + sc->offset < 0)
                continue;                      /* bottom edge scrolled off the top */
            textout_centre_ex(bmp, sc->fnt, sc->lines[i - 1],
                               x + sc->width / 2, y + row_y,
                               color, -1);
        }
    }

    set_clip_rect(bmp, 0, 0, bmp->w - 1, bmp->h - 1);
    return -1;
}
