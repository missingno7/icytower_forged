/* results.c -- draw_results(), the score panel the game-over screen and
 * the replay browser both draw under their logo.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c.
 *
 * PROMOTIONS.md batch 15.  One of the five functions batch 14's closing
 * table left ORIGINAL on play()'s coastline, and the smallest: 839
 * bytes, four real calls (textprintf_ex x2, textprintf_right_ex,
 * stricmp, makecol) plus three sprite draws through the BITMAP vtable.
 * Its domain is the ORDERED TRACE of those calls with their arguments --
 * the draw_frame.c family's convention, reused.
 *
 * Recovered from artifacts/disasm.txt 0x4076c0..0x407a06.
 *
 * -------------------------------------------------------------------
 * Findings
 * -------------------------------------------------------------------
 *
 * 1. THREE CATEGORIES ARE DRAWN, AND NOT IN CATEGORY ORDER.  The frame
 *    holds a 20-byte (five-int) local cleared by `rep stos` at 0x4076e2
 *    with only two non-zero elements written afterwards (0x4076e4 sets
 *    [1] = 2, 0x4076eb sets [2] = 1), and the loop walks it three times
 *    -- the row offset starts at 0, steps by 30, and the loop exits when
 *    it reaches 60 (0x407890).  So the rows are category_names[0],
 *    [2], [1]: "Score", "Floor", "Best Combo".  The last two elements of
 *    the array are never read; they are in the object code because the
 *    initialiser clears all five, and they are kept here for that reason.
 *
 * 2. THE PERSONAL-BEST BADGE IS SUPPRESSED FOR THE GUEST PROFILE, by
 *    name.  0x40781a compares `profile->handle` (the Tprofile field at
 *    +6) against the literal "guest" at 0x4d4b86 with stricmp -- a
 *    case-insensitive compare, so "Guest" and "GUEST" are suppressed
 *    too.  A profile literally called "guest" therefore never gets a PB
 *    icon no matter what it scores.  That is a name check, not a flag
 *    check, and nothing else in this function looks at the profile.
 *
 * 3. THE BADGE SHIFTS THE HIGH-SCORE ICON, and the shift is the only
 *    thing the personal-best branch leaves behind: both arms of the test
 *    set a horizontal offset (+18 when the PB icon was drawn, -4 when it
 *    was not, 0x407877 / 0x4078a4) which then displaces BOTH the
 *    high-score icon and the rank number beside it.  The two icons are
 *    laid out relative to each other, not to the panel.
 *
 * 4. The three sprite draws all go through the vtable pair
 *    draw_sprite/draw_256_sprite selected on the SOURCE bitmap's colour
 *    depth (0x407708, 0x40784f, 0x4078e0) -- Allegro's draw_sprite()
 *    AL_INLINE expanding in place, the same shape batch 14 identified in
 *    fade.c and batch 9 in draw_frame.c.  The logo is drawn CENTRED by
 *    arithmetic, not by a centring helper: x = 320 - logo->w / 2, with
 *    the divide compiled as the signed `shr $0x1f`/`add`/`sar` sequence
 *    at 0x4076f6, i.e. a plain `/ 2` on a signed int.
 *
 * 5. The two per-row texts share one font (data[52], ASSET_DATA_FONT_MED_WHITE)
 *    and one baseline; the rank number under the icons uses data[53]
 *    (ASSET_DATA_FONT_MONO) and is the only text in the function with an
 *    explicit colour -- makecol(0, 0, 0), evaluated per row inside the
 *    qualified branch (0x407926).  Everything else passes -1 for both
 *    colour and background, i.e. "the font's own".
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"
#include "assets.h"

#include <string.h>

/* code reaches datafile objects through asset_bitmap()/asset_font(),
 * never data[N] -- ASSETS.md's seam.  The four this function needs:
 *   data[52] 0x340  ASSET_DATA_FONT_MED_WHITE   the row label + value
 *   data[53] 0x350  ASSET_DATA_FONT_MONO        the rank number
 *   data[68] 0x440  ASSET_DATA_ICON_HS          the high-score icon
 *   data[69] 0x450  ASSET_DATA_ICON_PB          the personal-best icon
 */

/* MSVCRT.stricmp, reached through the import table -- declared the way
 * play.c already declares it (artifacts/imports.json). */
#ifndef stricmp
int stricmp(const char *a, const char *b);
#endif

#ifndef ICYTOWER_UPSTREAM_ALLEGRO

#ifndef draw_sprite
static void it_res_draw_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    if (sprite->vtable->color_depth == 8)
        bmp->vtable->draw_256_sprite(bmp, sprite, x, y);
    else
        bmp->vtable->draw_sprite(bmp, sprite, x, y);
}
#define draw_sprite(b, s, x, y) it_res_draw_sprite((b), (s), (x), (y))
#endif

#endif /* !ICYTOWER_UPSTREAM_ALLEGRO */

void draw_results(BITMAP *bmp, BITMAP *logo, int y, int *qualified,
                  int *qValues, int showQ)
{
    int rows[5] = {0, 2, 1};
    int i;
    int idx;
    int dy;
    int xoff;

    draw_sprite(bmp, logo, 320 - logo->w / 2, y);

    dy = 0;
    for (i = 0; i < 3; i++) {
        idx = rows[i];

        textprintf_ex(bmp, asset_font(ASSET_DATA_FONT_MED_WHITE), 200,
                      y + logo->h + 3 + dy, -1, -1, "%s:",
                      category_names[idx]);
        textprintf_right_ex(bmp, asset_font(ASSET_DATA_FONT_MED_WHITE), 440,
                            y + logo->h + 3 + dy, -1, -1, "%d",
                            qValues[idx]);

        if (showQ) {
            if (new_personal_best[idx] > 0
                && stricmp(profile->handle, "guest") != 0) {
                draw_sprite(bmp, asset_bitmap(ASSET_DATA_ICON_PB), 476,
                            y + logo->h + 13 + dy);
                xoff = 18;
            } else {
                xoff = -4;
            }

            if (qualified[idx] > 0) {
                draw_sprite(bmp, asset_bitmap(ASSET_DATA_ICON_HS), xoff + 480,
                            y + logo->h + 13 + dy);
                textprintf_ex(bmp, asset_font(ASSET_DATA_FONT_MONO),
                              xoff + 487, y + logo->h + 18 + dy,
                              makecol(0, 0, 0), -1, "%d", qualified[idx]);
            }
        }

        dy += 30;
    }
}
