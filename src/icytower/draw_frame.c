/* draw_frame.c -- the per-frame renderer: everything the game paints into
 * one back-buffer bitmap for one displayed frame, in the exact order the
 * original paints it.
 *
 * Recovered from artifacts/disasm.txt 0x40929c..0x40b3e1 (8518 bytes,
 * F:\projects\icytower\trunk\source\main.c per DWARF, source lines
 * 2490..~2800).  Prototype from src/icytower/game_funcs.h:
 * `void draw_frame(BITMAP *bmp)`.
 *
 * ============================================================ STRUCTURE
 *
 * PROMOTIONS.md batch 8 recorded this function as "one monolithic
 * function, NOT a set of small pre-existing pieces to extract", and that
 * is still true of the BINARY: 0x40929c..0x40b3e1 is a single `.text`
 * range with exactly one call to a game-scope helper (`draw_reward`),
 * ~2100 disassembly lines, and no internal call boundaries.  What batch 8
 * could not see without reading the DWARF locals is that the ORIGINAL C
 * still had visible seams -- DW_AT_decl_line on this subprogram's own
 * locals brackets the source into contiguous, non-overlapping regions
 * (2491 x/y, 2492 p_im, 2493 flip, 2495 cx/cy, 2496 ls, 2498 fo, 2499 so,
 * 2504 max_bg_id, 2549 f, 2563 s, 2565 sy, 2566 sw, 2569 c1, 2570 c2,
 * 2605 customFrame, 2606 oy, 2607 ox, 2743 myBuf, 2744 myPos, 2773 vcr,
 * 2774 len, 2786 scrollerText), and DW_AT_call_line on its 30 inlined
 * Allegro AL_INLINEs pins each drawing call to its own source line
 * (2542 hurry sign, 2552/2555/2558 floor tiles, 2568 sign, 2582 stars,
 * 2621/2624/2638/2639/2651 player, 2699/2700 side rails, 2705 combo
 * meter, 2712 combo count, 2718 clock, 2720 clock hand,
 * 2778/2780/2781/2782 VCR buttons).
 *
 * This file therefore splits the recovery along the ORIGINAL's own
 * boundaries -- one `static` helper per region, in paint order -- and
 * `draw_frame()` at the bottom composes them.  The decomposition is a
 * READABILITY choice, exactly as the task brief frames it; the
 * composition reproduces the original's ordered sequence of Allegro calls
 * and their arguments unchanged, and the helpers are `static` so the
 * compiler is free to inline them straight back into one body.
 *
 *   draw_background()      0x40930c-0x40942a   bg stripe scroll + 5 blits
 *   draw_hurry_sign()      0x40942b-0x409484   HURRYUP banner
 *   draw_floors()          0x409485-0x4098d7   32 rows: tiles + sign digits
 *   draw_stars()           0x4098d8-0x409999   512 reward particles
 *   draw_player()          0x40999a-0x409d9e   frame pick + one sprite
 *   draw_side_rails()      0x409d9f-0x40a09f   5 x 2 SIDEBLOCK sprites
 *   draw_combo_meter()     0x40a0a0-0x40a17e   meter + liquid + count
 *   draw_clock()           0x40a17f-0x40a290   CLOCK + rotated CLOCK_HAND
 *   draw_score()           0x40a291-0x40a2f7   draw_reward + "score: %d"
 *   draw_replay_hud()      0x40a2f8-0x40a5ba   REPLAY tag, custom-game
 *                                              settings, VCR panel,
 *                                              scrolling title, progress bar
 *   draw_debug_overlay()   0x40a5bb-0x40a887   8 lines, debug && F2 only
 *
 * ================================================== ASSETS (ASSETS.md)
 *
 * Every `data[N].dat` in the original is spelled here as
 * `asset_bitmap(ASSET_...)` / `asset_font(ASSET_...)`.  Five of the sites
 * are COMPUTED indices, and this pass resolves four of the five that
 * src/icytower/ASSETS.md's "What remains hand-mapped" table still lists
 * as open (its rows for VA 0x409383 and VA 0x4095ff are both inside this
 * function).  Each is resolved the same way start_reward.c's own
 * `ASSET_DATA_REWARD_000 + tier` was (PROMOTIONS.md batch 7): the base
 * object opens a run of consecutively-named objects that
 * assets_table.inc GENERATES contiguously from the manifest's own
 * consecutive object names, so `<base id> + k` is a mechanical offset
 * inside one generator-guaranteed family -- never "an asset_id used as a
 * global datafile index", which ASSETS.md rightly forbids.
 *
 *   0x409383  data[bg_stripe_ids[i] + 1]  -> ASSET_DATA_BGTILE + id
 *             (BGTILE, BGTILE2..BGTILE5 = data 1..6; ids are drawn from
 *             `new_rand() % max_bg_id`, max_bg_id <= 5, so the reachable
 *             range is exactly the 6-wide family)
 *   0x409508  data[f]      -> ASSET_DATA_FLOOR_01 + (f - 17)
 *   0x4095a5  data[f + 1]  ->   "     (middle tile)
 *   0x409605  data[f + 2]  ->   "     (right tile)
 *             (FLOOR01..FLOOR27 = data 17..49, 33 objects = 11 triples;
 *             `f` is clamped to 44 and then +3, so f+2 <= 49 -- the
 *             family's last member, exactly)
 *   0x409690  data[s]      -> ASSET_DATA_SIGN_01 + (s - 101)
 *             (SIGN01..SIGN09 = data 101..111, 11 objects; `s` is clamped
 *             to 110 and then +1, so s <= 111)
 *   0x409959  data[stars[i].color + 117] -> ASSET_DATA_STAR_01 + color
 *             (STAR01..STAR08 = data 117..124; create_particle() draws
 *             `color` as `new_rand() % 8`, so the range is exact)
 *
 * The remaining two computed-index sites the census counts are in
 * `play()` (0x4146e4 / 0x4149e6), not here.
 *
 * =============================================== ALLEGRO AL_INLINE GAP
 *
 * Five of the Allegro primitives this function uses are AL_INLINE in real
 * Allegro 4.4 -- they have no callee address of their own, they compile
 * into the CALLER's .text:
 *
 *   draw_sprite         -> a colour-depth BRANCH, then bmp->vtable->
 *                          draw_256_sprite (8bpp) or ->draw_sprite
 *                          (0x44 / 0x48 in the disassembly; every one of
 *                          this function's 0x44/0x48 pairs is one
 *                          draw_sprite() call site)
 *   draw_sprite_h_flip  -> bmp->vtable->draw_sprite_h_flip   (0x50)
 *   rotate_sprite       -> bmp->vtable->pivot_scaled_sprite_flip (0xa4)
 *   rect                -> bmp->vtable->rect                 (0xbc)
 *   fixtoi / ftofix     -> pure fixed-point arithmetic, no call at all
 *
 * carrier/gen/pf_lib_bindings.h emits the 23 straight one-call
 * passthroughs (so `draw_sprite_h_flip` and `rect` resolve in the carrier
 * world), but by construction it emits NEITHER the ones with a branch or
 * arithmetic of their own (`draw_sprite`, `rotate_sprite`) NOR the
 * fixed-point family (`fixtoi`, `ftofix`) -- see that file's own
 * AL_INLINE_VTABLE_DISPATCH comment and carrier/gen/LIB_BINDINGS_NOTES.md
 * ("inline function" gap, already documented there for itofix/etc).  The
 * generated, no-bindings src/icytower/allegro_api.h emits none of the
 * five, the same gap PROMOTIONS.md batch 8 found for rectfill/putpixel
 * and batch 9 found for line.
 *
 * Handled here the way batch 9 handled `line`: an `#ifndef`-guarded,
 * upstream-faithful definition of exactly the missing names, so the
 * source under test stays byte-identical in all three worlds and the real
 * <allegro.h> always wins when it is present.  A real fix belongs in
 * port_forge/tools/pf_win32_gen_lib_bindings.py (the shared framework
 * submodule another agent owns), not here -- reported, not fixed.
 *
 * ======================================================== x87 SEMANTICS
 *
 * Every floating-point comparison below is written in the form that
 * reproduces the ORIGINAL's `fucom`/`fucomp`/`fucompp` + `test $0x45,%ah`
 * (or `sahf`) flag reading EXACTLY, including the unordered (NaN) case --
 * the divergence-006 lesson (notes/living_record.md; PROMOTIONS.md
 * batch 2's `line_intersect` EAX bug) applied pre-emptively.  Concretely:
 *
 *   `test $0x45,%ah` + `je`/`sete`  == "ST0 > ST1, ORDERED"  ==  `a > b`
 *   `test $0x45,%ah` + `jne`        == "NOT (a > b)"         ==  `!(a > b)`
 *   `test $0x05,%ah` + `je`         == "a >= b, ORDERED"     ==  `a >= b`
 *   `test $0x05,%ah` + `jne`        == "NOT (a >= b)"        ==  `!(a >= b)`
 *   `sahf` + `jbe`                  == "NOT (a > b)"         ==  `!(a > b)`
 *
 * so a guard whose original spelling falls THROUGH on NaN is written here
 * as a negated ordered comparison (`if (!(p->sx >= 0.0))`), never as the
 * arithmetically-equal-looking `if (p->sx < 0.0)`, which would reject NaN
 * and diverge on exactly the degenerate inputs divergence 006 was about.
 * The three float constants the original loads with `flds` (3.0f, -3.0f,
 * 400.0f) are spelled `f`-suffixed to mirror the load width; all three
 * are exactly representable, so no rounding question arises.
 *
 * The only arithmetic x87 chains are (a) `1.476 * (map.offset % 84)`
 * truncated to int, twice per side-rail row, (b) `(clock_angle % 1500) *
 * 0.1706666` fed through ftofix, and (c) the `(int)p->x` / `(int)p->y`
 * truncations, which use the same local round-to-zero control word every
 * other recovered function in this directory already establishes.
 *
 * ================================================ MEMORY-DOMAIN EFFECTS
 *
 * draw_frame is not a pure renderer.  It writes, in paint order:
 *   frame_count      ++, unconditionally, first thing
 *   last_stripe_y    ++ once per generated background stripe
 *   bg_stripe_ids[5] shifted + refilled once per generated stripe
 *   seed             (indirectly, via new_rand() -- 2 calls per stripe)
 *   ply[player_id]->frame   reset to 0 on most player-animation paths
 *   scroll_count     ++ / snapped to -250 while a replay title scrolls
 *   scroll_delay     -- while a replay title is still paused
 *   *allegro_errno   ERANGE, from ftofix's own out-of-range guards
 *
 * ==================================================== VERIFICATION
 *
 * See PROMOTIONS.md batch 10 and artifacts/src_equivalence.json.  Short
 * version: COMPILE-ONLY offline, IN-VIVO-PENDING for behaviour.  The
 * offline harness cannot express this function's comparison domain as it
 * stands -- not for want of a hook, but because the domain is an ORDERED
 * sequence of ~120 library calls per invocation (blit / draw_sprite /
 * draw_256_sprite / draw_sprite_h_flip / pivot_scaled_sprite_flip / rect /
 * makecol / textout_ex / textprintf_ex / textprintf_centre_ex /
 * text_length / set_clip_rect / sprintf / strcpy / draw_reward /
 * is_left / is_fire / is_right), and carrier/lift/harness/lift_check.py's
 * call-trace mechanism records per callee only a COUNT plus the arguments
 * of its FIRST call (PROMOTIONS.md batch 8's own documented limitation).
 * With 15 draw_sprite() sites and 12 textprintf_ex() sites in one
 * invocation, first-call-capture compares roughly 15% of what this
 * function actually does.  The in-vivo frame oracle at blit_to_screen is
 * the authority instead; the exact command is in PROMOTIONS.md batch 10.
 */
#include <stdio.h>              /* sprintf */
#include <string.h>             /* strcpy */
#include <errno.h>              /* ERANGE, for the ftofix shim below */

/* ------------------------------------------------------------------ */
/* Carrier-world name collision, worked around HERE rather than in the  */
/* generator (which another agent owns): `Treplay.data` (the recorded   */
/* input stream, read by the debug overlay as `demo->data[rec_pos]`)    */
/* collides with the unrelated top-level global `DATAFILE *data`        */
/* @0x4dd23c, which carrier/gen/pf_bindings_src.h rewrites with a blunt */
/* textual `#define data (*(DATAFILE **)0x4dd23c)`.  That turns any     */
/* `->data` / `.data` member access into a syntax error -- the exact    */
/* bug class PROMOTIONS.md batch 7 fixed for `jump_sound` (via          */
/* gen_bindings.py's MEMBER_ACCESS_COLLISIONS) and batch 8 could NOT    */
/* fix for `stars` (because another promoted file needs the bare        */
/* global).  `data` is the batch-8 case again: several unpromoted       */
/* functions still read the bare global.                                */
/*                                                                      */
/* This file, though, is precisely the one that never wants it: the     */
/* whole point of src/icytower/ASSETS.md's seam is that clean drawing   */
/* code reaches datafile objects through asset_bitmap()/asset_font(),   */
/* never through `data[N]`.  So dropping the binding for the rest of    */
/* this translation unit is not a workaround around a missing feature,  */
/* it is the seam's own rule stated locally.  The generator-side fix    */
/* (a context-sensitive rewrite that skips `.name`/`->name` and keeps   */
/* rewriting bare occurrences) is still the real one; reported, not     */
/* attempted here.                                                      */
/* ------------------------------------------------------------------ */
/* `Trecord.cycle_count` (the recorded tick stamp, same debug-overlay      */
/* line) collides the same way with the global `volatile int cycle_count`  */
/* @0x506938 that timer.c's cycle_counter() increments.  Same disposition. */
#ifdef data
#undef data
#endif
#ifdef cycle_count
#undef cycle_count
#endif

#include "allegro_api.h"
#include "assets.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

/* ------------------------------------------------------------------ */
/* Allegro AL_INLINE primitives this build's headers do not supply.    */
/* Bodies transcribed from allegro-4.4.3.1/include/allegro/inline/     */
/* draw.inl (draw_sprite:238, draw_sprite_h_flip:280, rotate_sprite:   */
/* 345 -- the same decl_file/decl_line DWARF records for this          */
/* function's own inlined instances) and .../fmaths.inl (fixfloor:158, */
/* fixtoi:187, ftofix).  Guarded per name: real <allegro.h> and the    */
/* carrier's pf_lib_bindings.h always win where they define one.       */
/* ------------------------------------------------------------------ */
#ifndef ICYTOWER_UPSTREAM_ALLEGRO

#ifndef draw_sprite
static void it_al_draw_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    if (sprite->vtable->color_depth == 8)
        bmp->vtable->draw_256_sprite(bmp, sprite, x, y);
    else
        bmp->vtable->draw_sprite(bmp, sprite, x, y);
}
#define draw_sprite(b, s, x, y) it_al_draw_sprite((b), (s), (x), (y))
#endif

#ifndef draw_sprite_h_flip
#define draw_sprite_h_flip(b, s, x, y) \
    ((b)->vtable->draw_sprite_h_flip((b), (s), (x), (y)))
#endif

#ifndef rect
#define rect(b, x1, y1, x2, y2, c) \
    ((b)->vtable->rect((b), (x1), (y1), (x2), (y2), (c)))
#endif

#ifndef rotate_sprite
static void it_al_rotate_sprite(BITMAP *bmp, BITMAP *sprite,
                                int x, int y, fixed angle)
{
    bmp->vtable->pivot_scaled_sprite_flip(bmp, sprite,
        (x << 16) + (sprite->w << 15), (y << 16) + (sprite->h << 15),
        sprite->w << 15, sprite->h << 15, angle, 0x10000, 0);
}
#define rotate_sprite(b, s, x, y, a) it_al_rotate_sprite((b), (s), (x), (y), (a))
#endif

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

#ifndef ftofix
static fixed it_al_ftofix(double x)
{
    if (x > 32767.0) { *allegro_errno = ERANGE; return 0x7FFFFFFF; }
    if (x < -32767.0) { *allegro_errno = ERANGE; return -0x7FFFFFFF; }
    return (fixed)(x * 65536.0 + (x < 0 ? -0.5 : 0.5));
}
#define ftofix(x) it_al_ftofix(x)
#endif

#endif /* !ICYTOWER_UPSTREAM_ALLEGRO */

/* ------------------------------------------------------------------ */
/* 1. Background (0x40930c-0x40942a, DWARF local `max_bg_id` @2504)    */
/*                                                                     */
/* The tower background is five 128-pixel stripes scrolling with the   */
/* map.  `last_stripe_y` counts how many stripes have been decided so  */
/* far; whenever the map has scrolled past another 256 units, one more */
/* stripe id is pushed onto the front of the 5-deep `bg_stripe_ids`    */
/* ring and the oldest falls off the back.  A pushed id is 0 ("plain") */
/* 59 times in 100, and a decorated id otherwise -- but a decorated id */
/* that would repeat either of the two stripes immediately behind it   */
/* is downgraded back to 0, so no motif appears twice within three     */
/* stripes.  `max_bg_id` (the caller's, from the player's floor) caps  */
/* which motifs are unlocked at all.                                   */
/*                                                                     */
/* NOTE this is a `while`, not an `if`: after a level load or a long   */
/* stall map.offset can be several stripes ahead, and each pass        */
/* advances last_stripe_y by exactly one and consumes 1 or 2 new_rand()*/
/* draws.  Getting that wrong would desynchronise the shared RNG, not  */
/* merely mis-draw the wallpaper.                                      */
/* ------------------------------------------------------------------ */
static void draw_background(BITMAP *bmp, int max_bg_id)
{
    int i;
    BITMAP *tile;

    while (map.offset / 256 > last_stripe_y) {
        last_stripe_y++;
        bg_stripe_ids[4] = bg_stripe_ids[3];
        bg_stripe_ids[3] = bg_stripe_ids[2];
        bg_stripe_ids[2] = bg_stripe_ids[1];
        bg_stripe_ids[1] = bg_stripe_ids[0];
        if (new_rand() % 100 > 40) {
            bg_stripe_ids[0] = 0;
        } else {
            bg_stripe_ids[0] = new_rand() % max_bg_id;
            if (bg_stripe_ids[0] == bg_stripe_ids[1] ||
                bg_stripe_ids[0] == bg_stripe_ids[2])
                bg_stripe_ids[0] = 0;
        }
    }

    /* bg_stripe_ids[0] is the newest and is drawn ONE stripe above the
     * top of the screen (i - 1 == -1), so a stripe is already in place
     * by the time the map scrolls it into view. */
    for (i = 0; i < 5; i++) {
        tile = asset_bitmap((asset_id)(ASSET_DATA_BGTILE + bg_stripe_ids[i]));
        blit(tile, bmp, 0, 0, 37, (i - 1) * 128 + map.offset % 256 / 2,
             tile->w, tile->h);
    }
}

/* ------------------------------------------------------------------ */
/* 2. "Hurry up!" banner (0x40942b-0x409484, call_line 2542)           */
/*                                                                     */
/* `hurry_y` is driven elsewhere; -100 < hurry_y < 480 is "on screen   */
/* or one banner-height above it".  options.flash == 2 suppresses it   */
/* entirely (the flashing-effects option's off setting).               */
/* ------------------------------------------------------------------ */
static void draw_hurry_sign(BITMAP *bmp)
{
    BITMAP *b;

    if (hurry_y > -100 && hurry_y < 480 && options.flash != 2) {
        b = asset_bitmap(ASSET_DATA_HURRYUP);
        draw_sprite(bmp, b, 320 - b->w / 2, hurry_y);
    }
}

/* ------------------------------------------------------------------ */
/* 3. Floors and their number signs (0x409485-0x4098d7)                */
/*      DWARF locals: `f` @2549, `s` @2563, `sy` @2565, `sw` @2566,    */
/*      `c1` @2569, `c2` @2570; inlined draw_sprite @2552/2555/2558    */
/*      (left/middle/right tile) and @2568 (the sign board).           */
/*                                                                     */
/* map.room[31] is the BOTTOM row on screen (y = -32 .. its own row)   */
/* and the loop walks up to map.room[0] at y = 464, 16 pixels apart,   */
/* the whole strip riding map.offset % 16.                             */
/*                                                                     */
/* Each non-empty row is three sprites from one FLOOR triple: the left */
/* cap at start_tile*16 - 5, one middle tile per interior column, and  */
/* the right cap.  Which triple is chosen is                           */
/*     f = 17 + 3*profile->start_floor + 3*room.tiles,   capped at 44, */
/*     then + 3 if that floor's level is past 4999,                    */
/* i.e. the tower's visual style advances with the floor's own `tiles` */
/* count and with the player's starting floor, and there is one extra  */
/* style reserved for the 5000+ region.  The cap is applied BEFORE the */
/* +3, so the 5000+ style is reachable only through the cap.           */
/*                                                                     */
/* The sign block runs for every row with a non-zero `sign`, INCLUDING */
/* rows whose `empty` flag skipped the tiles above -- the two are      */
/* independent tests in the original, not an if/else.                  */
/*                                                                     */
/* The sign's number is drawn five times: four dark copies at the four */
/* 4-neighbour offsets around (sx+1, sy+6) and one white copy on top,  */
/* the classic Allegro poor-man's outline.                             */
/* ------------------------------------------------------------------ */
static void draw_floors(BITMAP *bmp, int fo, int so)
{
    int cy, cx, t, f, s, sy, sw, c1, c2, x;
    Tfloor *fl;
    BITMAP *b;

    for (cy = 31, cx = -32; cx != 480; cy--, cx += 16) {
        fl = &map.room[cy];

        if (!fl->empty) {
            f = fo + fl->tiles * 3;
            if (f > 44)
                f = 44;
            if (fl->level > 4999)
                f += 3;

            t = fl->start_tile;
            draw_sprite(bmp, asset_bitmap((asset_id)(ASSET_DATA_FLOOR_01 + f - 17)),
                        t * 16 - 5, cx + map.offset % 16 - 6);
            t++;
            while (t < fl->end_tile) {
                draw_sprite(bmp,
                            asset_bitmap((asset_id)(ASSET_DATA_FLOOR_01 + f + 1 - 17)),
                            t * 16, cx + map.offset % 16 - 6);
                t++;
            }
            draw_sprite(bmp, asset_bitmap((asset_id)(ASSET_DATA_FLOOR_01 + f + 2 - 17)),
                        t * 16, cx + map.offset % 16 - 6);

            /* Statically-dead in a shipped build: `debug` (0x4dd160) has
             * no store anywhere in the image -- PROMOTIONS.md batch 9's
             * finding, unchanged.  Recovered anyway, not elided. */
            if (debug && key[KEY_F2])
                textprintf_ex(bmp, font, 520, cx + map.offset % 16, 15, -1,
                              "%d", (fl->level - 1) / 5);
        }

        if (fl->sign) {
            s = so + fl->tiles;
            if (s > 110)
                s = 110;
            if (fl->level > 4999)
                s++;

            sy = cx + map.offset % 16 + 10;
            b = asset_bitmap((asset_id)(ASSET_DATA_SIGN_01 + s - 101));
            sw = b->w;
            x = (fl->start_tile + (fl->end_tile - fl->start_tile) / 2) * 16;
            draw_sprite(bmp, b, x, sy);

            c1 = makecol(255, 255, 255);
            c2 = makecol(0x37, 0x37, 0x37);
            x += sw / 2;
            textprintf_centre_ex(bmp, asset_font(ASSET_DATA_FONT_SMALL),
                                 x + 1, sy + 7, c2, -1, "%d", fl->sign);
            textprintf_centre_ex(bmp, asset_font(ASSET_DATA_FONT_SMALL),
                                 x + 2, sy + 6, c2, -1, "%d", fl->sign);
            textprintf_centre_ex(bmp, asset_font(ASSET_DATA_FONT_SMALL),
                                 x,     sy + 6, c2, -1, "%d", fl->sign);
            textprintf_centre_ex(bmp, asset_font(ASSET_DATA_FONT_SMALL),
                                 x + 1, sy + 5, c2, -1, "%d", fl->sign);
            textprintf_centre_ex(bmp, asset_font(ASSET_DATA_FONT_SMALL),
                                 x + 1, sy + 6, c1, -1, "%d", fl->sign);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 4. Reward star particles (0x4098d8-0x409999, call_line 2582)        */
/*                                                                     */
/* The one place in draw_frame that does NOT draw into `bmp`: these go */
/* straight onto the global `swap_screen`.  Recovered faithfully, not  */
/* "fixed" -- the disassembly loads swap_screen (0x4dd194) as the      */
/* destination at 0x409965 and passes it as argument 0 on both the     */
/* draw_sprite and draw_256_sprite arms.                               */
/*                                                                     */
/* `stars[]` is the same Tparticle[512] pool start_reward.c fills and  */
/* update_particle.c advances; `intensity == 0` marks a free slot, so  */
/* an inactive slot draws nothing at all.  Positions are 16.16 fixed   */
/* and go through Allegro's own rounding fixtoi(), not a bare >>16.    */
/* ------------------------------------------------------------------ */
static void draw_stars(void)
{
    int i;

    for (i = 0; i < 512; i++) {
        if (stars[i].intensity)
            draw_sprite(swap_screen,
                        asset_bitmap((asset_id)(ASSET_DATA_STAR_01 + stars[i].color)),
                        fixtoi(stars[i].x), fixtoi(stars[i].y));
    }
}

/* ------------------------------------------------------------------ */
/* 5. The player (0x40999a-0x409d9e)                                   */
/*      DWARF locals: `p_im` @2492, `flip` @2493, `customFrame` @2605, */
/*      `oy` @2606, `ox` @2607.                                        */
/*                                                                     */
/* Exactly one sprite is drawn, from custom.frame[0..14] (the loaded   */
/* character's 15-frame sheet).  Everything here is choosing WHICH.    */
/*                                                                     */
/*   custom.frame[0]      standing                                     */
/*   custom.frame[1..4]   running cycle (p_im = 1, + p->frame 0..3)    */
/*   custom.frame[5]      rising fast    (status 1, sy < -3)           */
/*   custom.frame[6]      airborne                                     */
/*   custom.frame[7]      falling fast   (status 2/3, sy > 3)          */
/*   custom.frame[8]      straight-up jump (airborne, |sx| < 0.01)     */
/*   custom.frame[9..11]  idle fidgets (see below)                     */
/*   custom.frame[12]     the spinning frame, used only while rotating */
/*   custom.frame[13/14]  hanging on a ledge, alternating every 8 ticks*/
/*                                                                     */
/* Three speed thresholds separate the ground states, and each is      */
/* written below in the exact ordered/unordered form the original's    */
/* x87 flag test has (see this file's x87 note):                       */
/*   |sx| < 0.02   standing (may fidget, may hang on an edge)          */
/*   |sx| < 0.2    walking  (p_im = 1, animation frame pinned to 0)    */
/*   otherwise     running  (p_im = 1, animation frame cycles 0..3)    */
/*                                                                     */
/* The idle fidget picks a frame purely from the free-running          */
/* `logic_count`, EXCEPT that a player standing still high up the      */
/* tower (map.offset > 200) and low on the screen (p->y > 400) gets    */
/* frame 11 -- the "looking down / vertigo" pose.                      */
/*                                                                     */
/* Facing: `p->sx > 0` draws normally, anything else (including        */
/* exactly 0) draws h-flipped.  A player who has come to a dead stop   */
/* therefore faces left, which is the original's behaviour, not a      */
/* transcription slip.                                                 */
/*                                                                     */
/* The vertical anchor `oy` is ALWAYS computed from custom.frame[0]'s  */
/* height, never from the frame actually drawn -- so a taller or       */
/* shorter animation frame grows downwards from a fixed head position. */
/* The horizontal anchor `ox` does use the drawn frame's own width.    */
/* ------------------------------------------------------------------ */
static void draw_player(BITMAP *bmp)
{
    Tplayer *p = ply[player_id];
    BITMAP *customFrame;
    int p_im, ox, oy, flip;

    if (p->status == 0) {
        /* --- on the ground ------------------------------------- */
        if (p->sx >= 0.0 ? 0.02 > p->sx : p->sx > -0.02) {
            /* standing still */
            p->frame = 0;
            oy = 1 - custom.frame[0]->h;

            if (p->edge) {
                /* hanging off a ledge; edge 1 = right foot on, 2 = left */
                customFrame = custom.frame[(logic_count & 8) ? 13 : 14];
                if (p->edge == 2)
                    draw_sprite_h_flip(bmp, customFrame,
                                       (int)p->x - customFrame->w + 11,
                                       (int)p->y + oy);
                else
                    draw_sprite(bmp, customFrame,
                                (int)p->x - 11, (int)p->y + oy);
                return;
            }

            if (map.offset > 200 && p->y > 400.0f) {
                customFrame = custom.frame[11];
            } else if (logic_count <= 11) {
                customFrame = custom.frame[9];
            } else if (logic_count > 24 && logic_count <= 36) {
                customFrame = custom.frame[10];
            } else {
                customFrame = custom.frame[0];
            }
            ox = -(customFrame->w / 2);
            flip = !(p->sx > 0.0);
            if (flip)
                draw_sprite_h_flip(bmp, customFrame,
                                   (int)p->x + ox, (int)p->y + oy);
            else
                draw_sprite(bmp, customFrame,
                            (int)p->x + ox, (int)p->y + oy);
            return;
        }

        if (!(p->sx >= 0.0) ? !(p->sx > -0.2) : !(0.2 > p->sx)) {
            /* running: cycle custom.frame[1..4] through p->frame */
            if (p->frame > 3)
                p->frame = 0;
        } else {
            /* walking: same base frame, animation pinned */
            p->frame = 0;
        }
        p_im = 1;
        oy = 1 - custom.frame[0]->h;
    } else {
        /* --- airborne ------------------------------------------ */
        if (p->status == 1)
            p_im = (-3.0f > p->sy) ? 5 : 6;
        else if (p->status == 2 || p->status == 3)
            p_im = (p->sy > 3.0f) ? 7 : 6;
        else
            p_im = 6;

        if (!(p->sx >= 0.0) ? p->sx > -0.01 : 0.01 > p->sx)
            p_im = 8;

        p->frame = 0;
        oy = 1 - custom.frame[0]->h;
    }

    if (p->rotate) {
        /* Spinning after a big combo jump: one frame, one angle, and
         * a different vertical anchor (-8 instead of +1) because the
         * rotation pivots about the sprite's own centre. */
        customFrame = custom.frame[12];
        rotate_sprite(bmp, customFrame,
                      (int)p->x - customFrame->w / 2,
                      (int)p->y - 8 - custom.frame[0]->h,
                      p->angle);
        return;
    }

    customFrame = custom.frame[p_im + p->frame];
    ox = -(customFrame->w / 2);
    flip = !(p->sx > 0.0);
    if (flip)
        draw_sprite_h_flip(bmp, customFrame, (int)p->x + ox, (int)p->y + oy);
    else
        draw_sprite(bmp, customFrame, (int)p->x + ox, (int)p->y + oy);
}

/* ------------------------------------------------------------------ */
/* 6. Side rails (0x409d9f-0x40a09f, call_line 2699/2700)              */
/*                                                                     */
/* Five SIDEBLOCK sprites down each edge, 124 pixels apart, the left   */
/* column h-flipped.  Both columns scroll at 1.476 pixels per unit of  */
/* map.offset % 84 -- a DIFFERENT, slower parallax rate from the       */
/* floors, which is why the product is recomputed (not hoisted) for    */
/* every one of the ten draws in the original, and is left recomputed  */
/* here so the x87 multiply/truncate sequence matches call for call.   */
/* ------------------------------------------------------------------ */
static void draw_side_rails(BITMAP *bmp)
{
    int y;

    for (y = -124; y != 496; y += 124) {
        draw_sprite(bmp, asset_bitmap(ASSET_DATA_SIDEBLOCK),
                    565, y + (int)(1.476 * (map.offset % 84)));
        draw_sprite_h_flip(bmp, asset_bitmap(ASSET_DATA_SIDEBLOCK),
                           -57, y + (int)(1.476 * (map.offset % 84)));
    }
}

/* ------------------------------------------------------------------ */
/* 7. Combo meter (0x40a0a0-0x40a17e, call_line 2705/2712)             */
/*                                                                     */
/* The meter frame is always drawn.  Its liquid fill is blitted as a   */
/* bottom-anchored slice `in_combo` pixels tall out of COMBO_LIQUID,   */
/* so the source rectangle and the destination both move up as the     */
/* combo grows.  The number beside it is the combo's accumulated floor */
/* count while a combo is live, and the finished combo's size for as   */
/* long as the reward animation is still playing.                      */
/* ------------------------------------------------------------------ */
static void draw_combo_meter(BITMAP *bmp)
{
    Tplayer *p;

    draw_sprite(bmp, asset_bitmap(ASSET_DATA_COMBO_METER), 22, 100);

    p = ply[player_id];
    if (p->in_combo) {
        blit(asset_bitmap(ASSET_DATA_COMBO_LIQUID), bmp,
             0, 100 - p->in_combo, 33, 219 - p->in_combo, 16, p->in_combo);
        draw_sprite(bmp, asset_bitmap(ASSET_DATA_COMBO_COUNT), -8, 210);
        textprintf_centre_ex(bmp, asset_font(ASSET_DATA_FONT_BIG_WHITE),
                             42, 210, -1, -1, "%d", ply[player_id]->acc_level);
    } else if (reward_time) {
        draw_sprite(bmp, asset_bitmap(ASSET_DATA_COMBO_COUNT), -8, 210);
        textprintf_centre_ex(bmp, asset_font(ASSET_DATA_FONT_BIG_WHITE),
                             42, 210, -1, -1, "%d", ply[player_id]->latest_combo);
    }
}

/* ------------------------------------------------------------------ */
/* 8. Clock (0x40a17f-0x40a290, call_line 2718/2720)                   */
/*      DWARF locals `oy` @2606 / `ox` @2607 are reused here by the    */
/*      register allocator; kept as separate locals for readability.   */
/*                                                                     */
/* Two nested shake windows, both keyed on hurry_y and both a 3-cycle  */
/* -1/0/+1 jitter off logic_count:                                     */
/*   hurry_y in (250, 480): the clock FACE itself jitters (5..7, 9..11)*/
/*   hurry_y in (200, 480): the clock HAND's pivot jitters             */
/* The second window strictly contains the first, so the face-jitter's */
/* own ox/oy assignment is always overwritten before the hand is drawn */
/* -- recovered as written, since only the face's x/y survive it.      */
/*                                                                     */
/* The hand's angle is Allegro's binary-degree fixed point: 1500 units */
/* of clock_angle make one full turn (256 * 1/1500 == 0.1706666).      */
/* clock_angle == 0 short-circuits to a literal 0 angle, skipping      */
/* ftofix entirely -- which matters, because ftofix can set            */
/* *allegro_errno.                                                     */
/* ------------------------------------------------------------------ */
static void draw_clock(BITMAP *bmp)
{
    int x, y, ox = 0, oy = 0;
    fixed angle;

    if (hurry_y > 250 && hurry_y < 480) {
        x = logic_count % 3 + 5;
        y = (logic_count + 1) % 3 + 9;
        ox = logic_count % 3 - 1;
        oy = (logic_count + 1) % 3 - 1;
    } else {
        x = 6;
        y = 10;
    }
    draw_sprite(bmp, asset_bitmap(ASSET_DATA_CLOCK), x, y);

    if (hurry_y > 200 && hurry_y < 480) {
        ox = (logic_count + 2) % 3 - 1;
        oy = (logic_count + 3) % 3 - 1;
    }

    if (clock_angle)
        angle = ftofix(clock_angle % 1500 * 0.1706666);
    else
        angle = 0;
    rotate_sprite(bmp, asset_bitmap(ASSET_DATA_CLOCK_HAND),
                  34 + ox, 28 + oy, angle);
}

/* ------------------------------------------------------------------ */
/* 9. Reward animation + score readout (0x40a291-0x40a2f7)             */
/*                                                                     */
/* draw_reward() is draw_frame's ONLY call to another game-scope       */
/* function, and (like draw_stars) it targets swap_screen, not `bmp`.  */
/* The score line is the player's own floor*10 + score.                */
/* ------------------------------------------------------------------ */
static void draw_score(BITMAP *bmp)
{
    Tplayer *p;

    if (reward_time)
        draw_reward(swap_screen);

    p = ply[player_id];
    textprintf_ex(bmp, asset_font(ASSET_DATA_FONT_MED_WHITE), 8, 440, -1, -1,
                  "score: %d", p->level * 10 + p->score);
}

/* ------------------------------------------------------------------ */
/* 10. Replay / custom-game HUD (0x40a2f8-0x40a5ba)                    */
/*      DWARF locals: `myBuf` @2743, `myPos` @2744, `vcr` @2773,       */
/*      `len` @2774, `scrollerText` @2786; VCR button draw_sprites at  */
/*      call_line 2778/2780/2781/2782.                                 */
/*                                                                     */
/* Skipped entirely while `recording` -- this whole panel is what you  */
/* see when WATCHING a replay, not when setting one.                   */
/*                                                                     */
/* Four independent pieces:                                            */
/*   (a) a "REPLAY" tag that blinks with bit 3 of frame_count;         */
/*   (b) for a custom game, the three replayed settings, each drawn    */
/*       twice (black at +1,+1 then white) for a drop shadow;          */
/*   (c) the VCR panel bottom-right, with the three live input lamps   */
/*       (suppressed once the player is dead);                         */
/*   (d) the replay's own title scrolling right-to-left inside a clip  */
/*       rectangle, plus a green progress bar.                         */
/*                                                                     */
/* The title is drawn in up to three PIECES (name, " - ", comment) at  */
/* independently computed x positions, all sharing one scroll offset,  */
/* while `scrollerText` -- the same three pieces sprintf'd together    */
/* -- exists only so its text_length() can decide when to wrap the     */
/* scroll back to -250.  Also note the name is drawn TWICE, one pixel  */
/* apart horizontally and at the SAME y: a horizontal-only shadow, not */
/* the diagonal one used everywhere else in this function.             */
/* ------------------------------------------------------------------ */
static void draw_replay_hud(BITMAP *bmp)
{
    char myBuf[256];            /* DWARF: char[256] @ ebp-0x15e */
    char scrollerText[70];      /* DWARF: char[70]  @ ebp-0x5e  */
    int myPos, len, x, y, size, pos, c;
    BITMAP *vcr;
    FONT *fnt = asset_font(ASSET_DATA_FONT_MONO);

    if (recording)
        return;

    if (frame_count & 8) {
        sprintf(myBuf, "REPLAY");
        myPos = 630 - text_length(fnt, myBuf);
        textprintf_ex(bmp, fnt, myPos + 1, 5, makecol(0, 0, 0), -1, myBuf);
        textprintf_ex(bmp, fnt, myPos, 4, makecol(255, 255, 255), -1, myBuf);
    }

    if (is_playing_custom_game) {
        sprintf(myBuf, "%s Floors",
                floor_size_selection.caption[demo->floor_size]);
        myPos = 630 - text_length(fnt, myBuf);
        textout_ex(bmp, fnt, myBuf, myPos + 1, 16, makecol(0, 0, 0), -1);
        textout_ex(bmp, fnt, myBuf, myPos, 15, makecol(255, 255, 255), -1);

        sprintf(myBuf, "%s Speed",
                scroll_speed_selection.caption[demo->start_speed]);
        myPos = 630 - text_length(fnt, myBuf);
        textout_ex(bmp, fnt, myBuf, myPos + 1, 26, makecol(0, 0, 0), -1);
        textout_ex(bmp, fnt, myBuf, myPos, 25, makecol(255, 255, 255), -1);

        strcpy(myBuf, gravity_selection.caption[demo->gravity]);
        myPos = 630 - text_length(fnt, myBuf);
        textout_ex(bmp, fnt, myBuf, myPos + 1, 36, makecol(0, 0, 0), -1);
        textout_ex(bmp, fnt, myBuf, myPos, 35, makecol(255, 255, 255), -1);
    }

    pos = rec_pos;
    size = demo->size;

    vcr = asset_bitmap(ASSET_DATA_VCR);
    x = 635 - vcr->w;
    y = 475 - vcr->h;
    draw_sprite(bmp, vcr, x, y);

    if (!ply[player_id]->dead) {
        if (is_left(&ctrl))
            draw_sprite(bmp, asset_bitmap(ASSET_DATA_VCR_LEFT), x + 97, y + 5);
        if (is_fire(&ctrl))
            draw_sprite(bmp, asset_bitmap(ASSET_DATA_VCR_UP), x + 107, y + 5);
        if (is_right(&ctrl))
            draw_sprite(bmp, asset_bitmap(ASSET_DATA_VCR_RIGHT), x + 117, y + 5);
    }

    y += 10;
    x += 10;

    set_clip_rect(bmp, x, 0, 623, 479);
    if (demo->comment[0])
        sprintf(scrollerText, "%s%s%s", demo->name, " - ", demo->comment);
    else
        sprintf(scrollerText, "%s%s%s", demo->name, "", "");

    /* Each colour is computed BEFORE the text_length() that positions
     * the piece it colours -- the original's own call order, kept
     * explicit here rather than left to C's unspecified argument
     * evaluation order, because both are traced library calls. */
    c = makecol(150, 150, 160);
    textout_ex(bmp, fnt, demo->name, x + 2 - scroll_count / 2, y + 4, c, -1);
    c = makecol(200, 200, 210);
    textout_ex(bmp, fnt, demo->name, x + 3 - scroll_count / 2, y + 4, c, -1);
    if (demo->comment[0]) {
        c = makecol(200, 200, 210);
        len = text_length(fnt, demo->name);
        textout_ex(bmp, fnt, " - ", x + 2 - scroll_count / 2 + len, y + 4, c, -1);
        c = makecol(200, 200, 210);
        len = text_length(fnt, demo->name);
        textout_ex(bmp, fnt, demo->comment,
                   x + 20 - scroll_count / 2 + len, y + 4, c, -1);
    }
    set_clip_rect(bmp, 0, 0, 639, 479);

    if (demo->comment[0]) {
        if (scroll_delay > 0) {
            scroll_delay--;
        } else {
            scroll_count++;
            if (scroll_count / 2 > text_length(fnt, scrollerText))
                scroll_count = -250;
        }
    }

    c = makecol(50, 200, 50);
    len = pos * 117 / size;
    if (len > 116)
        len = 116;
    /* y1 == y+20 and y2 == y+19: the original really does pass the two
     * edges the "wrong" way round (0x40a56d / 0x40a5a1), which Allegro's
     * rect() handles by normalising them -- recovered, not corrected. */
    rect(bmp, x, y + 20, x + len, y + 19, c);
}

/* ------------------------------------------------------------------ */
/* 11. Debug overlay (0x40a5bb-0x40a887)                               */
/*                                                                     */
/* Same statically-dead gate as draw_floors' floor numbers and as the  */
/* three collision variants' line overlay: `debug` has no store        */
/* anywhere in the image (PROMOTIONS.md batch 9).  Recovered anyway.   */
/* ------------------------------------------------------------------ */
static void draw_debug_overlay(BITMAP *bmp)
{
    Tplayer *p;

    if (!debug || !key[KEY_F2])
        return;

    p = ply[player_id];
    textprintf_ex(bmp, font, 0, 0, 15, -1, "FPS:%6d / %d", fps, lps);
    textprintf_ex(bmp, font, 0, 10, 15, -1, "REC:%6d / %d", rec_pos, demo->size);
    textprintf_ex(bmp, font, 0, 20, 15, -1, "    %6d  (%d) ",
                  demo->data[rec_pos].key_flags, demo->data[rec_pos].cycle_count);
    textprintf_ex(bmp, font, 200, 0, 15, -1, "POS: %d, %d",
                  (int)p->x, (int)p->y);
    textprintf_ex(bmp, font, 200, 10, 15, -1, " dx: %1.2f", p->sx);
    textprintf_ex(bmp, font, 200, 20, 15, -1, "rjp: %d", options.jump_hold);
    textprintf_ex(bmp, font, 400, 0, 15, -1, "any: %6d %6d %6d",
                  any11, any12, any13);
    textprintf_ex(bmp, font, 400, 10, 15, -1, "any: %6d %6d %6d",
                  any21, any22, any23);
}

/* ------------------------------------------------------------------ */
/* draw_frame -- the composition (0x40929c-0x4092fe entry, then the    */
/* eleven regions above in this exact order).                          */
/*                                                                     */
/* `fo` (@2498) and `so` (@2499) are the two floor/sign asset BASES,   */
/* computed once from the profile's starting floor and then offset     */
/* per row inside draw_floors.  `max_bg_id` (@2504) unlocks background */
/* motifs by the player's own floor: 2 below floor 201, 3 to 350, 4 to */
/* 600, 5 from 601 up.                                                 */
/* ------------------------------------------------------------------ */
void draw_frame(BITMAP *bmp)
{
    int fo, so, max_bg_id;
    int level;

    fo = 17 + profile->start_floor * 3;
    so = 101 + profile->start_floor;

    frame_count++;

    level = ply[player_id]->level;
    if (level <= 200)
        max_bg_id = 2;
    else if (level <= 350)
        max_bg_id = 3;
    else if (level >= 601)
        max_bg_id = 5;
    else
        max_bg_id = 4;

    draw_background(bmp, max_bg_id);
    draw_hurry_sign(bmp);
    draw_floors(bmp, fo, so);
    draw_stars();
    draw_player(bmp);
    draw_side_rails(bmp);
    draw_combo_meter(bmp);
    draw_clock(bmp);
    draw_score(bmp);
    draw_replay_hud(bmp);
    draw_debug_overlay(bmp);
}
