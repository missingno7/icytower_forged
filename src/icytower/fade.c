/* fade.c -- fadeIn() / fadeOut(), the screen transitions play() runs
 * around the game-over and menu screens.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c (the same CU
 * as sound.c / logfile.c / screenshot.c / draw_reward.c / play.c).
 *
 * PROMOTIONS.md batch 14.  Both are play()-coastline callees from batch
 * 13's closing list.  Neither touches game state at all: their whole
 * effect is an ORDERED sequence of Allegro calls, one iteration per
 * animation step, so the comparison domain is that trace with its
 * arguments -- the same domain batch 13 used for draw_reward and
 * take_screenshot.  rest() is part of it: a wait is a call, and both
 * sides trace it rather than performing it.
 *
 * They are also the pair that makes `cycle_count` (0x506938, the 20ms
 * tick counter timer.c's cycle_counter() drives) observable OUTSIDE the
 * tick loop: each animation step zeroes it and then spins
 * `while (cycle_count <= 0) rest(2);`, which paces the fade at one step
 * per tick without a frame-rate assumption.  The oracle scripts that
 * counter so the spin terminates deterministically on both sides.
 *
 * ---------------------------------------------------------------------
 * SCREEN_W / SCREEN_H -- a finding, not a defensive check
 * ---------------------------------------------------------------------
 * Both functions read the screen size through a NULL-guarded load:
 *
 *     40bf65:  mov 0x4dda84,%eax ; test %eax,%eax ; je <use 0>
 *     40bf72:  mov 0x70(%eax),%edx ; mov 0x6c(%eax),%eax
 *
 * 0x4dda84 is Allegro's own `gfx_driver` (allegro_api.h already declares
 * it) and +0x6c/+0x70 are GFX_DRIVER's `w`/`h` -- counted field by field
 * down that struct rather than assumed.  The guard is not hand-written:
 * it is Allegro's own
 *     #define SCREEN_W  (gfx_driver ? gfx_driver->w : 0)
 * expanding in place.  The same shape appears four times across the two
 * functions (40bf65, 40bf86, 40c08b, 40c138, 40c1d2, 40c2a6), always in
 * pairs, always with the same fallback -- which is what identifies it.
 * Recovering it as a hand-written `if (gfx_driver)` would have been
 * observationally identical but wrong about the source.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"

/* ------------------------------------------------------------------ */
/* Allegro macros/AL_INLINEs this file needs, transcribed from         */
/* allegro-4.4.3.1/include/allegro/gfx.h (SCREEN_W/SCREEN_H) and       */
/* .../inline/draw.inl (rectfill:88, draw_sprite:238).  Guarded per     */
/* name exactly as draw_frame.c/draw_reward.c already guard theirs:     */
/* real <allegro.h> and the carrier's pf_lib_bindings.h always win.     */
/* ------------------------------------------------------------------ */
#ifndef ICYTOWER_UPSTREAM_ALLEGRO

#ifndef SCREEN_W
#define SCREEN_W (gfx_driver ? gfx_driver->w : 0)
#endif
#ifndef SCREEN_H
#define SCREEN_H (gfx_driver ? gfx_driver->h : 0)
#endif

#ifndef rectfill
#define rectfill(b, x1, y1, x2, y2, c) \
    ((b)->vtable->rectfill((b), (x1), (y1), (x2), (y2), (c)))
#endif

#ifndef draw_sprite
static void it_fade_draw_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    if (sprite->vtable->color_depth == 8)
        bmp->vtable->draw_256_sprite(bmp, sprite, x, y);
    else
        bmp->vtable->draw_sprite(bmp, sprite, x, y);
}
#define draw_sprite(b, s, x, y) it_fade_draw_sprite((b), (s), (x), (y))
#endif

#endif /* !ICYTOWER_UPSTREAM_ALLEGRO */

/* ---------------------------------------------------------------------
 * fadeOut  (0x40bf5c, 609 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x40bf5c..0x40c1bc.
 *
 *   40bf7f  tmp = create_bitmap(SCREEN_W, SCREEN_H)
 *   40bfcd  blit(screen, tmp, 0, 0, 0, 0, SCREEN_W, SCREEN_H)
 *           -- a snapshot of what is currently ON SCREEN, so the fade
 *              works even though play()'s own back buffer has moved on.
 *           `screen` is 0x4dda8c (Allegro's global, allegro_api.h
 *           declares it); `swap_screen` is 0x4dd194 (the game's own,
 *           game_state.h declares it).  Both appear in this function and
 *           they are NOT interchangeable -- see the last step.
 *
 *   the loop (40bff0..40c10e), one iteration per step:
 *       cycle_count = 0
 *       draw_sprite(swap_screen, tmp, 0, 0)
 *       set_trans_blender(0, 0, 0, i)
 *       drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0)
 *       rectfill(swap_screen, 0, 0, SCREEN_W, SCREEN_H, makecol(0,0,0))
 *       solid_mode()
 *       blit_to_screen(swap_screen)
 *       while (cycle_count <= 0) rest(2)
 *
 *   loop shape: GCC rotated `for (i = 0; i < 255; i += speed)` into a
 *   bottom-tested loop and strength-reduced the test, keeping `255-i-
 *   speed` in -0x1c(%ebp) and testing `(255-i-speed) + speed > 0` at
 *   40c109.  That is exactly `i + speed < 255` after the increment, i.e.
 *   the rotated form of `i < 255` -- reconstructed algebraically rather
 *   than transcribed, because -0x1c has no direct source counterpart.
 *   The alpha argument is the OTHER counter, -0x20, which starts at 0
 *   and steps by `speed`: alpha == i.
 *
 *   40c117  destroy_bitmap(tmp)
 *   40c170  rectfill(screen, 0, 0, SCREEN_W, SCREEN_H, makecol(0,0,0))
 *           -- the final black fill goes to `screen` DIRECTLY (0x4dda8c
 *           reloaded at 40c148), not to swap_screen and not through
 *           blit_to_screen.  Everything inside the loop targets
 *           swap_screen instead.  The asymmetry is in the object code,
 *           not a transcription slip: it leaves the visible framebuffer
 *           black while the game's own buffer is left holding the last
 *           blended frame.
 */
void fadeOut(int speed)
{
    BITMAP *tmp;
    int i;

    tmp = create_bitmap(SCREEN_W, SCREEN_H);
    blit(screen, tmp, 0, 0, 0, 0, SCREEN_W, SCREEN_H);

    for (i = 0; i < 255; i += speed) {
        cycle_count = 0;
        draw_sprite(swap_screen, tmp, 0, 0);
        set_trans_blender(0, 0, 0, i);
        drawing_mode(DRAW_MODE_TRANS, 0, 0, 0);
        rectfill(swap_screen, 0, 0, SCREEN_W, SCREEN_H, makecol(0, 0, 0));
        solid_mode();
        blit_to_screen(swap_screen);
        while (cycle_count <= 0)
            rest(2);
    }

    destroy_bitmap(tmp);
    rectfill(screen, 0, 0, SCREEN_W, SCREEN_H, makecol(0, 0, 0));
}

/* ---------------------------------------------------------------------
 * fadeIn  (0x40c1c0, 424 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x40c1c0..0x40c365.  The mirror of
 * fadeOut, with three structural differences, all of them real:
 *
 *   1. There is no blit() at the top.  The image to fade IN is the
 *      caller's `bmp`, not a snapshot of the screen -- so the scratch
 *      bitmap is created but never initialised before the first
 *      draw_sprite fills it.
 *   2. swap_screen is never touched.  Every draw in the loop targets the
 *      scratch bitmap, and it is the scratch bitmap that reaches
 *      blit_to_screen (40c2e7).
 *   3. The alpha counter runs DOWN from 255 (-0x20 initialised at
 *      40c208, decremented at 40c310) while the same rotated
 *      `for (i = 0; i < 255; i += speed)` drives the trip count, so the
 *      blend argument is `255 - i`.
 *
 *   40c22a  the colour-depth branch reads `bmp->vtable->color_depth`
 *           (40c21a, the SPRITE's vtable) but dispatches through
 *           `tmp->vtable` (40c226/40c334, the DESTINATION's) -- which is
 *           precisely draw_sprite()'s own AL_INLINE, not two different
 *           bitmaps being confused.  draw_frame.c's stand-in for the
 *           same inline has the identical shape.
 *
 *   40c32e  destroy_bitmap(tmp) is a TAIL CALL, so fadeIn's frame is
 *           already gone when it runs.  No behavioural consequence; noted
 *           because the `jmp` can read as a missing call.
 */
void fadeIn(BITMAP *bmp, int speed)
{
    BITMAP *tmp;
    int i;

    tmp = create_bitmap(SCREEN_W, SCREEN_H);

    for (i = 0; i < 255; i += speed) {
        cycle_count = 0;
        draw_sprite(tmp, bmp, 0, 0);
        set_trans_blender(0, 0, 0, 255 - i);
        drawing_mode(DRAW_MODE_TRANS, 0, 0, 0);
        rectfill(tmp, 0, 0, SCREEN_W, SCREEN_H, makecol(0, 0, 0));
        solid_mode();
        blit_to_screen(tmp);
        while (cycle_count <= 0)
            rest(2);
    }

    destroy_bitmap(tmp);
}
