/* draw_reward.c -- draws the "reward" sprite (the combo / floor-milestone
 * banner) that pops up over the tower.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c.
 *
 * On the gameplay tick path, though NOT as a play() callee: it is
 * draw_frame's ONE call to another game-scope function (draw_frame.c line
 * 717: "draw_reward() is draw_frame's ONLY call to another game-scope
 * helper"), and draw_frame runs every frame of every tick.  Batch 10
 * promoted draw_frame and left this callee ORIGINAL, tracing it as an
 * opaque `("draw_reward", 1)` row in draw_frame_xcheck.py's LIBS table.
 * With this file promoted, the last ORIGINAL byte reachable from the
 * frame renderer is gone.
 *
 * ---------------------------------------------------------------------
 * draw_reward  (0x4070fc, 581 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x4070fc..0x407341.  Prototype
 * from the generated game_funcs.h: `void draw_reward(BITMAP *)`.
 * Three globals, all named in carrier/gen/interop_index.json and already
 * declared in the generated headers:
 *
 *   options       0x4fe528  Toptions   -- only `flash` (offset 0) is read
 *   reward_bmp    0x4f8af8  BITMAP *
 *   reward_scale  0x4fac28  fixed      -- 16.16, animated by the caller
 *
 * and a three-way branch on `options.flash` that the disassembly makes
 * unambiguous (`test %eax,%eax; je <rotate path>` then `dec %eax;
 * je <stretch path>`, otherwise the epilogue):
 *
 *   flash == 0   rotate_scaled_sprite()  -- the default, spinning banner
 *   flash == 1   stretch_sprite()        -- the "no flashing" setting:
 *                                          same sprite, no rotation
 *   flash >= 2   nothing is drawn at all
 *
 * ------------------------------- the fixed-point / x87 reading --------
 * Two Allegro AL_INLINEs account for every non-obvious instruction here,
 * and both were confirmed against the real headers in
 * third_party/allegro-4.4.3.1/, not guessed:
 *
 * 1. `fixtoi()`.  The `js` + `not; sar $0x10; not` pairs at 0x40730c /
 *    0x407320 / 0x407330 are exactly fmaths.inl's fixfloor (its own
 *    comment: "(x >> 16) is not portable"), and the `and $0x8000;
 *    sar $0xf; add` that follows each one is fixtoi's rounding term --
 *    the same expansion draw_frame.c's own `it_al_fixtoi` stand-in
 *    already carries, byte for byte.
 *
 * 2. `rotate_scaled_sprite()`.  The indirect call at 0x4072fd is
 *    `call *0xa4(%edx)` with %edx = bmp->vtable, i.e. GFX_VTABLE's
 *    +0xa4 slot -- `pivot_scaled_sprite_flip`, the same slot batch 10
 *    already traces as VT_PIVOT.  Its nine arguments, read off the
 *    stack stores at 0x40724b..0x4072f7, are
 *        (bmp, sprite, (x<<16)+(w*scale)/2, (y<<16)+(h*scale)/2,
 *         w<<15, h<<15, angle, scale, FALSE)
 *    which is draw.inl:374-377's rotate_scaled_sprite body verbatim,
 *    including the detail that the POSITION offsets are scaled
 *    ((w*scale)/2, a C signed division emitted as shr/add/sar) while the
 *    PIVOT offsets are not (w<<15).  That asymmetry is Allegro's, not a
 *    misreading: it is what pins this call to rotate_scaled_sprite
 *    rather than to a hand-rolled pivot_scaled_sprite call.
 *
 * The float constant at 0x4d6ccc is 1.52587890625e-05 == 2^-16 exactly,
 * loaded with `flds` and applied with `fimull` (x87 integer multiply) --
 * GCC's own strength reduction of a division by 65536.0, i.e. `fixtof`.
 * It is written as `fixtof()` here.  Whether the reciprocal is spelled
 * float or double is immaterial: 2^-16 is exact in both, `flds` widens
 * it losslessly to the 80-bit register, and the value is never stored
 * back to memory.  What DOES matter, and is the one thing this file must
 * not get wrong, is that the original keeps `fixtof(reward_scale/2)`
 * ALIVE IN st(1) across the whole function and never rounds it through a
 * 32- or 64-bit slot -- so no `float`/`double` local may be introduced
 * for it (contrast sound.c's `pan_x`, where the original DOES store
 * through a 32-bit float and the local is load-bearing).  Writing
 * `fixtof(...)` inline, as below, is what keeps the arithmetic in 80-bit
 * registers under `-mfpmath=387`.
 *
 * The two int casts are likewise load-bearing and are placed where the
 * `fistpl` instructions are: the original converts the DOUBLE
 * sub-expression to int first (one fistpl, truncating -- the usual C
 * cast, set up by the `fnstcw`/`mov $0xc,%ah`/`fldcw` dance at
 * 0x4071b1), and only then subtracts it from the integer literal 320 or
 * 360.  Writing `320 - fixtof(...) * w` without the cast would convert
 * `320.0 - x` as one operation and round differently on every input
 * whose fractional part is non-zero.
 *
 * The `fixtof(reward_scale) * 120` term in the rotate path's y is inside
 * that same cast (one `fsubrp` at 0x407287 before the one `fistpl`), and
 * 120 comes from the .rdata float at 0x4d6cd0 (exactly 120.0f).  Its
 * SIGN is the one thing in this file that the disassembly text actively
 * misleads about, and the oracle is what settled it: read naively,
 * `fsubrp %st,%st(1)` looks like "st(1) = st(0) - st(1)", which gives
 *     (int)(fixtof(scale/2)*h - fixtof(scale)*120)
 * and that form DIFFERS -- caught on the very first run, at vector 4 of
 * the draw_reward set (flash 0, scale 24878, w 1, h 32: the pivot call's
 * y argument came out 26546912 against the original's 21435104, i.e.
 * y = 399 where the original computes 321).  GNU as/objdump's AT&T
 * rendering of the two-operand x87 subtract forms is reversed relative
 * to Intel's, so the instruction actually computes
 *     st(1) = st(1) - st(0) = fixtof(scale)*120 - fixtof(scale/2)*h
 * which is the form below and which matches on every vector.  Noted at
 * length because the same trap is waiting in any other x87 function in
 * this image that uses the non-commutative fsub/fsubr/fdiv/fdivr pairs.
 *
 * The stretch path has no equivalent term, and instead subtracts a
 * second, integer-domain half-height (`fixtoi(reward_scale * h / 2)`) so
 * that the sprite's bottom edge lands on y = 360.
 */
#include "allegro_api.h"        /* BITMAP, GFX_VTABLE, fixed, stretch_sprite */
#include "game_types.h"
#include "game_state.h"         /* options, reward_bmp, reward_scale */

/* ------------------------------------------------------------------ */
/* Allegro AL_INLINE primitives this build's headers do not supply.    */
/* Bodies transcribed from allegro-4.4.3.1/include/allegro/inline/     */
/* fmaths.inl (fixfloor:158, fixtoi:187, fixtof:48) and draw.inl       */
/* (rotate_scaled_sprite:369).  Guarded per name, exactly as           */
/* draw_frame.c already does: real <allegro.h> and the carrier's       */
/* pf_lib_bindings.h always win where they define one.                 */
/* ------------------------------------------------------------------ */
#ifndef ICYTOWER_UPSTREAM_ALLEGRO

#ifndef fixtoi
static int it_al_fixfloor(fixed x)
{
    if (x >= 0)
        return (x >> 16);
    return ~((~x) >> 16);
}
static int it_al_fixtoi(fixed x)
{
    return it_al_fixfloor(x) + ((x & 0x8000) >> 15);
}
#define fixtoi(x) it_al_fixtoi(x)
#endif

#ifndef fixtof
static double it_al_fixtof(fixed x)
{
    return (double)x / 65536.0;
}
#define fixtof(x) it_al_fixtof(x)
#endif

#ifndef rotate_scaled_sprite
static void it_al_rotate_scaled_sprite(BITMAP *bmp, BITMAP *sprite,
                                       int x, int y, fixed angle, fixed scale)
{
    bmp->vtable->pivot_scaled_sprite_flip(bmp, sprite,
        (x << 16) + (sprite->w * scale) / 2,
        (y << 16) + (sprite->h * scale) / 2,
        sprite->w << 15, sprite->h << 15,
        angle, scale, 0);
}
#define rotate_scaled_sprite(b, s, x, y, a, sc) \
    it_al_rotate_scaled_sprite((b), (s), (x), (y), (a), (sc))
#endif

#endif /* !ICYTOWER_UPSTREAM_ALLEGRO */

/* ------------------------------------------------------------------ */

void draw_reward(BITMAP *bmp)
{
    if (options.flash == 0) {
        rotate_scaled_sprite(bmp, reward_bmp,
            320 - (int)(fixtof(reward_scale / 2) * reward_bmp->w),
            360 - (int)(fixtof(reward_scale) * 120
                        - fixtof(reward_scale / 2) * reward_bmp->h),
            reward_scale << 8, reward_scale);
    } else if (options.flash == 1) {
        stretch_sprite(bmp, reward_bmp,
            320 - (int)(fixtof(reward_scale / 2) * reward_bmp->w),
            360 - (int)(fixtof(reward_scale / 2) * reward_bmp->h)
                - fixtoi(reward_scale * reward_bmp->h / 2),
            fixtoi(reward_scale * reward_bmp->w),
            fixtoi(reward_scale * reward_bmp->h));
    }
}
