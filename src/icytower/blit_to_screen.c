/* blit_to_screen.c -- blit_to_screen (0x40b6bc, 1415 bytes,
 * F:\projects\icytower\trunk\source\main.c decl_line 2267).
 * PROMOTIONS.md batch 11.
 *
 * THE PRESENTATION SEAM.  draw_frame() renders one frame into an off-screen
 * BITMAP; this is the only function that puts it on the display, and it is
 * the point the carrier's --frame-digest oracle samples.  play() calls it
 * from six sites (0x412e8b, 0x413287, 0x413326, 0x413453 and two more in
 * the game-over path); the per-tick one is 0x413326.
 *
 * It is ONE `switch` on a function-static mode selector plus a debug
 * keyboard override, wrapped in acquire_screen()/release_screen():
 *
 *   if (debug) { blit_mode = <which of F2..F8 is down>; }   (0x40b6c7)
 *   acquire_screen();                                       (0x40b74b)
 *   switch (blit_mode) { ... seven presentation modes ... }
 *   release_screen();                                       (0x40b79f)
 *
 * `debug` (VA 0x4dd160) has no store anywhere in the image (established in
 * PROMOTIONS.md batch 9 for collision.c's own debug overlay), so in vivo
 * the mode is always 0 and this function is exactly one `blit`.  The other six
 * modes are recovered anyway -- they are real code, and the offline oracle
 * drives all seven.
 *
 * ------------------------------------------------------------ blit_mode
 * DWARF puts `blit_mode` (decl_line 2268) at DW_OP_addr 0x4dd324: a
 * FUNCTION-STATIC, i.e. state that must persist across calls AND stay at
 * that one address for as long as state is address-backed (src/README.md,
 * win32_pilot.md SS7a).  Writing `static int blit_mode;` here would give
 * this port private storage that does NOT alias 0x4dd324, so the carrier
 * would silently keep two copies.  The generated headers already solve
 * this: gen_src_headers.py mangles a function-static to
 * `<name>__<function>` and emits it as an ordinary extern
 * (game_state.h:72) with storage in state.c and a binding at 0x4dd324 in
 * every bindings header.  So the spelling below is the generator's, not an
 * invention, and it is the only identifier in src/icytower/ that is not the
 * original's own -- flagged here because it is a deliberate exception.
 *
 * ------------------------------------------------------- the F-key probe
 * 0x40b6d0-0x40b749 is seven independent `if`s (not an else-chain): every
 * pressed key assigns, so with several held the LAST one wins.  The
 * scancodes are `key[]` indices 48..54 = KEY_F2..KEY_F8, read as seven
 * absolute addresses 0x5069b8..0x5069be off Allegro's `key` (0x506988).
 *
 * --------------------------------------------------------- the modes
 * All geometry constants below are the original's own literals; the four
 * float thresholds come from .rdata (0x4d6d50 = 160.0f, 0x4d6d54 = 320.0f,
 * 0x4d6d58 = 240.0f, 0x4d6d5c = 80.0f, 0x4d6d60 = 520.0f, 0x4d6d64 =
 * 360.0f).  The screen is 640x480 and `bmp` is the game's own back buffer.
 *
 *   0  straight blit(bmp, screen, 0,0, 0,0, bmp->w, bmp->h)   (0x40b7c8)
 *   1  draw_sprite_h_flip(screen, bmp, 0, 0)  -- mirrored     (0x40b970)
 *   2  draw_sprite_v_flip(screen, bmp, 0, 0)  -- upside down  (0x40ba59)
 *   3  480 one-scanline blits, each shifted horizontally by
 *        fixtoi(fixsin(itofix(y + logic_count*5)) * p->level / 2)
 *      -- a travelling sine wobble whose amplitude is half the player's
 *      current level, in pixels                                (0x40b820)
 *   4  two black `line`s across the top and bottom rows of BMP itself,
 *      then the same frame blitted twice at d_y = level%480 and
 *      d_y = level%480 - 480 -- a vertical wrap-scroll            (0x40ba80)
 *   5  stretch_blit of a 320x240 window at (clamp(p->x-160, 0,320),
 *      clamp(p->y-160, 0,240)) up to the full 640x480 -- a 2x
 *      player-following zoom                                     (0x40b997)
 *   6  the same at 160x120 from (clamp(p->x-80, 0,520),
 *      clamp(p->y-80, 0,360)) -- a 4x zoom                       (0x40b8c7)
 *   default  nothing is presented at all (the switch just falls through to
 *      release_screen) -- recovered faithfully; blit_mode can only ever be
 *      0..6, so this arm is unreachable.
 *
 * Mode 4's two `line` calls target BMP, not `screen`: the debug separators
 * are drawn INTO the back buffer, so they persist into whatever the next
 * frame draws over.  Recovered as-is (the same "the original really does
 * target the other bitmap" finding PROMOTIONS.md batch 10 recorded for
 * draw_frame's reward stars going to `swap_screen`).
 *
 * The two zoom modes' clamps compile from three x87 compares each against
 * literal FLOATS, with the middle arm an ordinary `(int)` truncation under
 * the local round-to-zero control word (`fnstcw`/`fldcw 0x0c..`/`fistpl`).
 * Written below as the plain `if (v < 0) .. else if (v > C) .. else (int)v`
 * chain: on a NaN both compares are false and the truncation runs, which is
 * exactly what the original's unordered `fucom`/`test $0x45,%ah` pair does
 * (notes/living_record.md divergence 006).
 *
 * ---------------------------------------------------------- verification
 * Offline, GCC x87 (-mfpmath=387 -mno-sse2 -O2), ORDERED CALL-TRACE domain
 * -- carrier/lift/harness/blit_to_screen_xcheck.py, modelled on batch 10's
 * draw_frame_xcheck.py for the same reason: mode 3 issues 480 blits whose
 * arguments all differ, so the shared first-call-capture mechanism
 * (PROMOTIONS.md batch 8 "mechanism B") would compare 1/480th of it.  The
 * traced callees are blit, stretch_blit and the four GFX_VTABLE slots the
 * Allegro AL_INLINEs reach (acquire +0x10, release +0x14,
 * draw_sprite_v_flip +0x4c, draw_sprite_h_flip +0x50) plus BITMAP.vtable's
 * `line` (+0x34); the memory domain is `blit_mode` itself.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c.
 */
#include "allegro_api.h"   /* screen, key[], KEY_F2.., blit, stretch_blit */
#include "game_types.h"
#include "game_state.h"

/* ------------------------------------------------------------------ */
/* Allegro AL_INLINE primitives this build's headers do not supply.    */
/* Bodies transcribed from allegro-4.4.3.1/include/allegro/inline/     */
/* gfx.inl (acquire_bitmap:64, release_bitmap:70), draw.inl            */
/* (draw_sprite_v_flip/h_flip:280-ish) and fmaths.inl (itofix:26,      */
/* fixtoi:187, fixsin:199) -- the same decl_file/decl_line DWARF        */
/* records this function's own inlined instances carry (call_file 4     */
/* lines 221/227 for acquire/release_bitmap).  Guarded per name: real   */
/* <allegro.h> and the carrier's pf_lib_bindings.h always win where     */
/* they define one.  Same disposition as draw_frame.c's own block.      */
/* ------------------------------------------------------------------ */
#ifndef ICYTOWER_UPSTREAM_ALLEGRO

#ifndef acquire_screen
#define acquire_screen() \
    do { if (screen->vtable->acquire) screen->vtable->acquire(screen); } while (0)
#endif

#ifndef release_screen
#define release_screen() \
    do { if (screen->vtable->release) screen->vtable->release(screen); } while (0)
#endif

#ifndef draw_sprite_h_flip
#define draw_sprite_h_flip(b, s, x, y) \
    ((b)->vtable->draw_sprite_h_flip((b), (s), (x), (y)))
#endif

#ifndef draw_sprite_v_flip
#define draw_sprite_v_flip(b, s, x, y) \
    ((b)->vtable->draw_sprite_v_flip((b), (s), (x), (y)))
#endif

#ifndef line
#define line(b, x1, y1, x2, y2, c) \
    ((b)->vtable->line((b), (x1), (y1), (x2), (y2), (c)))
#endif

#ifndef itofix
#define itofix(x) ((fixed)((x) << 16))
#endif

#ifndef fixtoi
static int it_al_fixfloor2(fixed x)
{
    if (x >= 0)
        return (x >> 16);
    return ~((~x) >> 16);
}
static int it_al_fixtoi2(fixed x)
{
    return it_al_fixfloor2(x) + ((x & 0x8000) >> 15);
}
#define fixtoi(x) it_al_fixtoi2(x)
#endif

#ifndef fixsin
/* Allegro's own 512-entry quarter-wave table (VA 0x4ce100, `fixed[512]`,
 * C:\Lib\allegro4\src\math.c).  It has NO binding in any generated header
 * -- carrier/gen/pf_lib_bindings.h emits a library global only when the
 * referencing GAME CU's own DWARF DIE carries the address, and every
 * game-CU reference to `_cos_tbl` is declaration-only (DW_AT_declaration,
 * no DW_AT_location); the defining DIE, in Allegro's math.c CU, does have
 * DW_OP_addr 0x4ce100.  Reported as an out-of-scope generator gap in
 * PROMOTIONS.md batch 11, in the same class as batch 8's rectfill/putpixel,
 * batch 9's line and batch 10's draw_sprite/rotate_sprite/fixtoi/ftofix --
 * except that this one is a DATA symbol, so an #ifndef stand-in here cannot
 * substitute for the missing binding the way a function-shaped one can.
 * An ordinary address-free extern is all this layer may write. */
extern fixed _cos_tbl[];
static fixed it_al_fixsin(fixed x)
{
    /* fmaths.inl:199 verbatim, except that Allegro spells its quarter-turn
     * offset `0x400000`.  That is 64.0 in 16.16 fixed point -- Allegro's
     * angles run 0..256 for a full turn -- but it is also the guest image
     * base, and scripts/check_native_layer.py's rule is a blunt "does this
     * literal land in the image/heap/stack range".  Written as (64 << 16)
     * it is the same constant and unmistakably an angle. */
    return _cos_tbl[((x - (64 << 16) + 0x4000) >> 15) & 0x1FF];
}
#define fixsin(x) it_al_fixsin(x)
#endif

#endif /* !ICYTOWER_UPSTREAM_ALLEGRO */


void blit_to_screen(BITMAP *bmp)
{
    Tplayer *p;
    double vx, vy;
    int x, y;

    if (debug) {
        if (key[KEY_F2]) blit_mode__blit_to_screen = 0;
        if (key[KEY_F3]) blit_mode__blit_to_screen = 1;
        if (key[KEY_F4]) blit_mode__blit_to_screen = 2;
        if (key[KEY_F5]) blit_mode__blit_to_screen = 3;
        if (key[KEY_F6]) blit_mode__blit_to_screen = 4;
        if (key[KEY_F7]) blit_mode__blit_to_screen = 5;
        if (key[KEY_F8]) blit_mode__blit_to_screen = 6;
    }

    acquire_screen();

    switch (blit_mode__blit_to_screen) {

    case 0:
        blit(bmp, screen, 0, 0, 0, 0, bmp->w, bmp->h);
        break;

    case 1:
        draw_sprite_h_flip(screen, bmp, 0, 0);
        break;

    case 2:
        draw_sprite_v_flip(screen, bmp, 0, 0);
        break;

    case 3:
        for (y = 0; y < 480; y++)
            blit(bmp, screen, 0, y,
                 fixtoi(fixsin(itofix(y + logic_count * 5))
                        * ply[player_id]->level / 2),
                 y, 640, 1);
        break;

    case 4:
        y = ply[player_id]->level % 480;
        line(bmp, 0, 479, 639, 479, 0);
        line(bmp, 0, 0, 639, 0, 0);
        blit(bmp, screen, 0, 0, 0, y, bmp->w, bmp->h);
        blit(bmp, screen, 0, 0, 0, y - 480, bmp->w, bmp->h);
        break;

    case 5:
        p = ply[player_id];
        vx = p->x - 160.0f;
        vy = p->y - 160.0f;
        if (vx < 0) x = 0; else if (vx > 320.0f) x = 320; else x = (int)vx;
        if (vy < 0) y = 0; else if (vy > 240.0f) y = 240; else y = (int)vy;
        stretch_blit(bmp, screen, x, y, 320, 240, 0, 0, 640, 480);
        break;

    case 6:
        p = ply[player_id];
        vx = p->x - 80.0f;
        vy = p->y - 80.0f;
        if (vx < 0) x = 0; else if (vx > 520.0f) x = 520; else x = (int)vx;
        if (vy < 0) y = 0; else if (vy > 360.0f) y = 360; else y = (int)vy;
        stretch_blit(bmp, screen, x, y, 160, 120, 0, 0, 640, 480);
        break;
    }

    release_screen();
}
