/* draw_star_field.c -- clears a Tstar_field's rectangle (unless disabled)
 * and plots every star in it as a single pixel, depth-shaded.
 *
 * Recovered from artifacts/disasm.txt 0x41f340..0x41f406 (F:\projects\
 * icytower\trunk\source\stars.c per DWARF); prototype and parameter names
 * from src/icytower/game_funcs.h: `void draw_star_field(Tstar_field *sf,
 * BITMAP *bmp, int x, int y)`. Tstar_field/Tstar layouts from
 * game_types.h: `Tstar_field { clear_color, stars, width, height, depth,
 * col1, col_step, ..., Tstar star[1024] }`, `Tstar { double x; double y;
 * int z; }` (stride 24 bytes, matching the disassembly's `ebx += 0x18`
 * per-star advance).
 *
 * Body (0x41f34f-0x41f3fd):
 *   - if sf->clear_color != -1, one rectfill(bmp, x, y, x+width-1,
 *     y+height-1, sf->clear_color) clears the field's own rectangle first
 *     (clear_color == -1 disables the clear -- stars drawn over whatever
 *     was already on the bitmap);
 *   - then one putpixel(bmp, sx, sy, color) per star in sf->star[0..stars-1]:
 *     sx = (int)star.x + x, sy = (int)star.y + y (truncation toward zero,
 *     the same idiom every other recovered physics function already uses
 *     for a double -> int position cast), color = sf->col1 +
 *     sf->col_step * ((sf->depth - 1) - star.z) -- a per-star brightness/
 *     colour ramp keyed on the star's own depth field, sf->depth and
 *     sf->col1/sf->col_step read fresh from *sf every iteration (not
 *     hoisted out of the loop in the original either).
 *
 * Comparison domain the offline harness (carrier/lift/harness/lift_check.py)
 * CANNOT express, and precisely why: `rectfill`/`putpixel` are NOT calls to
 * a fixed, named library entry point the way blit/textout_ex/set_clip_rect
 * are (carrier/gen/pf_lib_bindings.h has no binding for either name --
 * confirmed by grep, 2026-09-07). In real Allegro 4.4 they are `AL_INLINE`
 * macros that expand directly to `bmp->vtable-><method>(bmp, ...)`; the
 * disassembly confirms this exactly (0x41f38a: `call *0x3c(%ecx)` where
 * ecx = bmp->vtable, offset 0x3c = rectfill's own GFX_VTABLE slot per
 * allegro_types.h's struct GFX_VTABLE layout; 0x41f3f0: `call *0x24(%ecx)`
 * = putpixel's slot) -- there is no separate callee address to hook the
 * way mechanism B (PROMOTIONS.md batch 7/8) hooks play_sound/set_clip_rect/
 * textout_ex: the "call" IS the caller's own inlined vtable dispatch, and
 * which concrete routine actually sits at vtable+0x3c/vtable+0x24 is a
 * property of whichever GFX_DRIVER is active at runtime, not a fixed
 * address this source, this build, or even this process's whole lifetime
 * can pin down once -- hooking "whatever the current driver's rectfill
 * happens to be" would be fragile in a way hooking a stable, named,
 * statically-linked function is not. Nor is this a memory-domain function
 * (draw_star_field writes only pixels, never a game global or *sf field).
 * This is the SAME class of gap draw_buffer.c already established
 * (src/icytower/ASSETS.md "Show the seam"), for the same underlying
 * reason (a pixel-output/vtable-dispatch domain, not a scalar/struct
 * memory domain or a stable call target).
 *
 * Compile verification, and its real status in EACH world (not both
 * green -- recorded precisely, per win32_pilot.md's "never a percentage,
 * always the exact byte/reason" rule):
 *
 *   standalone (real upstream Allegro headers, matching src/build/
 *   Makefile.standalone's own recipe -- draw_star_field.c is left OUT of
 *   that Makefile's SOURCES for the same reason draw_buffer.c already is,
 *   an Allegro-calling file needs the allegro_api.h -> <allegro.h> swap
 *   this basic smoke target does not perform; verified with a one-line
 *   scratch copy doing exactly that swap, per carrier/gen/
 *   LIB_BINDINGS_NOTES.md's "Standalone build swap" section):
 *     gcc -m32 -mfpmath=387 -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK
 *         -Ithird_party/allegro-4.4.3.1/include
 *         -Ithird_party/build-allegro-4.4.3.1/include
 *         -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower
 *         -c <scratch copy with '#include <allegro.h>'> -- 0 errors, 0 warnings.
 *   Real Allegro's own `rectfill`/`putpixel` AL_INLINE macros resolve fine
 *   here because a real, linked GFX_VTABLE is what this world's whole
 *   point is.
 *
 *   carrier (scratch pf_bindings_src.h/pf_lib_bindings.h, GCC -- this
 *   sandbox has no MSVC cl.exe available, see PROMOTIONS.md batch 8):
 *   DOES NOT COMPILE, for two separate, real, out-of-scope-for-this-batch
 *   reasons found while trying:
 *     (1) `rectfill`/`putpixel` have NO declaration anywhere in the
 *         carrier's generated headers -- confirmed by grep, 2026-09-07:
 *         they are absent from carrier/gen/pf_lib_bindings.h (the DWARF
 *         callee-scan that built it never saw a `call rectfill`/`call
 *         putpixel` in ANY game CU, for the exact vtable-inlining reason
 *         above) and allegro_api.h has no AL_INLINE-equivalent macro for
 *         either -- upstream Allegro's own header defines them as inline
 *         macros, but the carrier's stand-in header was generated only
 *         from the 100-function allow-list of things the game calls BY
 *         NAME (carrier/gen/LIB_BINDINGS_NOTES.md), which structurally
 *         excludes anything the game only ever reaches by inlined vtable
 *         dispatch. A real fix belongs in carrier/gen/gen_lib_bindings.py
 *         (emit the handful of GFX_VTABLE-dispatch macros upstream's own
 *         gfx.h defines), not attempted this pass.
 *     (2) even past (1), `sf->stars` (Tstar_field's int star-COUNT member)
 *         is rewritten into a syntax error by the SAME blunt textual
 *         `#define stars <address-cast>` bug class PROMOTIONS.md batch 7
 *         already found and fixed for `jump_sound` -- except this
 *         instance CANNOT reuse that fix (adding `stars` to
 *         MEMBER_ACCESS_COLLISIONS): start_reward.c (already promoted)
 *         reads the top-level `Tparticle stars[512]` global BARE and
 *         needs its #define to keep resolving, so blanket-skipping it
 *         would fix this file and silently break that one in the same
 *         build. See carrier/gen/gen_bindings.py's own
 *         MEMBER_ACCESS_COLLISIONS comment (batch 8 addendum) for the
 *         full writeup; a real fix needs a context-sensitive rewrite this
 *         generator's current blunt skip-list mechanism cannot express.
 *   Offline harness / memory-domain / call-trace verification: N/A for
 *   the same reason as draw_buffer.c (see above) -- not attempted.
 */
#include "allegro_api.h"
#include "game_types.h"

void draw_star_field(Tstar_field *sf, BITMAP *bmp, int x, int y)
{
    int i;

    if (sf->clear_color != -1)
        rectfill(bmp, x, y, x + sf->width - 1, y + sf->height - 1, sf->clear_color);

    for (i = 0; i < sf->stars; i++) {
        int color = sf->col1 + sf->col_step * ((sf->depth - 1) - sf->star[i].z);
        int sx = (int)sf->star[i].x + x;
        int sy = (int)sf->star[i].y + y;
        putpixel(bmp, sx, sy, color);
    }
}
