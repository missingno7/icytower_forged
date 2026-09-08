/* alert.c -- my_alert(), the game's own modal yes/no dialog.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c.
 *
 * PROMOTIONS.md batch 15.  One of the five functions batch 14's closing
 * table left ORIGINAL on play()'s coastline, and the first INTERACTIVE
 * one the project has recovered: it draws once, then spins on the
 * control layer until the player commits.  Its domain is the ORDERED
 * TRACE of every library and game call it makes, with arguments, plus
 * its return value -- there is no other observable (it writes two
 * Allegro globals, which the trace covers as data).
 *
 * Recovered from artifacts/disasm.txt 0x40cd68..0x40d452.
 *
 * The name is not decoration: Allegro HAS an alert() (0x44befc, on
 * gui.c's alert3()), and this function deliberately does not use it.
 * It reproduces alert()'s two colour globals -- gui_fg_color @0x4cc3a0
 * and gui_bg_color @0x4ddaa8, both set from makecol() at 0x40ce10 and
 * 0x40ce31 -- and then draws its own dialog out of the datafile instead,
 * so the modal looks like the rest of the game rather than like Allegro.
 * Setting those two globals is a leftover with no reader inside this
 * function; it is kept because it is a real side effect on library
 * state that a later real alert() would see.
 *
 * -------------------------------------------------------------------
 * Findings
 * -------------------------------------------------------------------
 *
 * 1. A THIRD text_length() CALL WHOSE RESULT IS DISCARDED.  0x40cd97 and
 *    0x40cdba measure `func` and `txt`; 0x40cdef measures WHICHEVER OF
 *    THE TWO WAS LONGER -- and %eax is overwritten by the next makecol()
 *    without ever being read.  A dead width computation, and it must be
 *    reproduced exactly: text_length() is not a pure function to the
 *    compiler, the call is in the object code, and an ordered-call-trace
 *    oracle sees it.  Written below as the `if/else` that selects the
 *    operand, with the result assigned to a variable nothing reads.
 *
 * 2. A NULL ARGUMENT BECOMES THE STRING " ".  Three separate branches
 *    (0x40cd7c, 0x40cda0, 0x40cdc8/0x40cdd5) substitute the one-space
 *    literal at 0x4d4daf when `func` or `txt` is NULL, so the dialog
 *    still measures and prints something.  The substitution reaches
 *    text_length() only -- the later textprintf_centre_ex() at 0x40cfbe
 *    passes the RAW `func`, NULL and all.  Two different treatments of
 *    the same argument in one function, both in the object code.
 *
 * 3. THE DIALOG IS DRAWN ON `screen`, NOT ON THE BACK BUFFER, AND THE
 *    BACK BUFFER IS USED AS THE UNDO.  0x40cf25 blits screen ->
 *    swap_screen before anything is drawn, and 0x40d350 blits
 *    swap_screen -> screen on the way out: the game's own double buffer
 *    is borrowed as a saved rectangle.  Both blits are 639 x 479, not
 *    640 x 480 -- one pixel short in each direction, in the original, on
 *    both calls.  Kept.
 *
 * 4. THE DIM IS A FULL-SCREEN TRANSLUCENT RECTFILL AT ALPHA 158
 *    (0x40ce36's 0x9e), drawn in DRAW_MODE_TRANS, with SCREEN_W/SCREEN_H
 *    expanding to the gfx_driver null-guard batch 14 identified in
 *    fade.c (0x40cea2 tests gfx_driver and substitutes 0 for both).
 *
 * 5. THE BUTTON SPRITES ARE PICKED BY AN UNSIGNED COMPARISON.  0x40d1bd
 *    and 0x40d201 are `cmp $1,%esi; sbb %eax,%eax`, i.e. CF = ((unsigned)
 *    sel < 1), which is `sel == 0` -- because the only two values `sel`
 *    ever holds are 0 and -1, and -1 is a huge unsigned.  sel == 0 draws
 *    data[10]/data[8], sel == -1 draws data[11]/data[7]; the `_2`
 *    variants are the highlighted ones, so sel == 0 is "No" selected and
 *    sel == -1 is "Yes".  Recovering the test as a signed `sel < 1`
 *    would be observationally identical on those two values and wrong
 *    about the object code, so it is written as the equality it is.
 *
 * 6. THE TWO DRAIN LOOPS ARE NOT THE SAME.  The one before the dialog
 *    (0x40d0a0) waits for is_any() on both control sets plus
 *    key[KEY_ESC] to go quiet; the one after (0x40d2d8) waits for those
 *    three PLUS key[KEY_ENTER].  The extra condition on the way out is
 *    what stops the Enter that dismissed the dialog from immediately
 *    activating whatever is behind it.
 *
 * 7. ESC RETURNS 0, i.e. THE SAME AS "No".  0x40d143 sets both the
 *    done flag and sel = 0, so a cancelled dialog is indistinguishable
 *    from a deliberate "No" in the return value.
 *
 * 8. The main loop's pacing is play()'s: `cycle_count = 0` at the top
 *    (0x40d0ea) and `while (!cycle_count) rest(2)` at the bottom
 *    (0x40d170), so one iteration per 20 ms timer tick.  vsync() is
 *    called ONLY when `choice` is set (0x40d1b8) -- a dialog with no
 *    buttons never syncs and never draws inside the loop at all.
 *
 * The two control sets are `ctrl` (0x5000c8, the player's) and
 * `menu_params.ctrl` (0x4f8e40 == menu_params + 8, the menu's); every
 * poll and every test is done on both, in that order.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"
#include "assets.h"

#include <string.h>

/* code reaches datafile objects through asset_bitmap()/asset_font(),
 * never data[N] -- ASSETS.md's seam.  The five this function needs:
 *   data[51] 0x330  ASSET_DATA_FONT_MED_BLACK   measure + the title
 *   data[54] 0x360  ASSET_DATA_FONT_SMALL       the body + the hint
 *   data[88] 0x580  ASSET_DATA_REPLAY_BG_SML    the dialog panel
 *   data[10] 0x0a0  ASSET_DATA_BT_YES     data[11] ASSET_DATA_BT_YES_2
 *   data[ 7] 0x070  ASSET_DATA_BT_NO      data[ 8] ASSET_DATA_BT_NO_2
 */

/* ------------------------------------------------------------------ */
/* MEMBER-ACCESS COLLISION, and the one that CANNOT be fixed by simply  */
/* dropping the binding.                                                */
/*                                                                      */
/* This function polls TWO control sets: the player's global `ctrl`     */
/* (@0x5000c8) and the menu's `menu_params.ctrl`.  `Tmenu_params` has a */
/* MEMBER called `ctrl`, so in the carrier world -- where the generated */
/* bindings turn `ctrl` into a blunt textual #define -- the member       */
/* access `menu_params.ctrl` is a syntax error.  replay.c's and         */
/* game_data.c's answer (drop the binding for the whole file with       */
/* #undef) is not available here, because this file also NEEDS the      */
/* global: undefining it would leave `&ctrl` pointing at a declaration  */
/* with no storage.                                                     */
/*                                                                      */
/* So the global's ADDRESS is captured into a file-static pointer while */
/* the macro is still live, and only then is the macro dropped.  The    */
/* two arms below are textually identical on purpose: in the carrier    */
/* world the initialiser expands the binding, in the standalone world   */
/* it takes the address of game_state.h's extern.  Everything after     */
/* this point spells the player's set `player_ctrl` and the menu's      */
/* `&menu_params.ctrl`, and both worlds see the same two addresses.     */
/*                                                                      */
/* This is the first case in the project where MEMBER_ACCESS_COLLISIONS */
/* needs more than a #undef, and it is the sharpest argument yet for    */
/* the context-sensitive rewrite gen_bindings.py's own comment          */
/* describes: a generator that knew `x.ctrl` is a member reference      */
/* would need none of this.                                             */
/* ------------------------------------------------------------------ */
#ifdef ctrl
static Tcontrol *const player_ctrl = &ctrl;
#undef ctrl
#else
static Tcontrol *const player_ctrl = &ctrl;
#endif

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

#ifndef acquire_bitmap
#define acquire_bitmap(b) \
    do { if ((b)->vtable->acquire) (b)->vtable->acquire(b); } while (0)
#endif
#ifndef release_bitmap
#define release_bitmap(b) \
    do { if ((b)->vtable->release) (b)->vtable->release(b); } while (0)
#endif

#ifndef draw_sprite
static void it_alert_draw_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    if (sprite->vtable->color_depth == 8)
        bmp->vtable->draw_256_sprite(bmp, sprite, x, y);
    else
        bmp->vtable->draw_sprite(bmp, sprite, x, y);
}
#define draw_sprite(b, s, x, y) it_alert_draw_sprite((b), (s), (x), (y))
#endif

#endif /* !ICYTOWER_UPSTREAM_ALLEGRO */

int my_alert(char *func, char *txt, int choice, int enter_hint)
{
    int len_func;
    int len_txt;
    int width;                  /* finding 1: written, never read */
    int sel;
    int done;

    len_func = text_length(asset_font(ASSET_DATA_FONT_MED_BLACK),
                           func ? func : " ");
    len_txt = text_length(asset_font(ASSET_DATA_FONT_MED_BLACK),
                          txt ? txt : " ");
    if (len_func > len_txt)
        width = text_length(asset_font(ASSET_DATA_FONT_MED_BLACK),
                            func ? func : " ");
    else
        width = text_length(asset_font(ASSET_DATA_FONT_MED_BLACK),
                            txt ? txt : " ");
    (void)width;

    gui_fg_color = makecol(0, 0, 0);
    gui_bg_color = makecol(255, 255, 255);

    set_trans_blender(0, 0, 0, 158);
    drawing_mode(DRAW_MODE_TRANS, 0, 0, 0);
    rectfill(screen, 0, 0, SCREEN_W, SCREEN_H, makecol(0, 0, 0));
    solid_mode();

    blit(screen, swap_screen, 0, 0, 0, 0, 639, 479);

    acquire_bitmap(screen);
    draw_sprite(screen, asset_bitmap(ASSET_DATA_REPLAY_BG_SML), 103, 130);
    textprintf_centre_ex(screen, asset_font(ASSET_DATA_FONT_MED_BLACK),
                         320, 135, -1, -1, "%s", func);
    if (txt)
        textout_centre_ex(screen, asset_font(ASSET_DATA_FONT_SMALL), txt,
                          320, 180, makecol(0, 0, 0), -1);
    if (enter_hint)
        textout_right_ex(screen, asset_font(ASSET_DATA_FONT_SMALL),
                         "(enter to continue)", 520, 200,
                         makecol(80, 80, 80), -1);
    release_bitmap(screen);

    /* drain whatever is still held down before the dialog listens */
    poll_control(player_ctrl, 0);
    poll_control(&menu_params.ctrl, 0);
    while (is_any(player_ctrl) || is_any(&menu_params.ctrl) || key[KEY_ESC]) {
        poll_control(player_ctrl, 0);
        poll_control(&menu_params.ctrl, 0);
        rest(2);
    }
    clear_keybuf();

    done = 0;
    sel = 0;
    while (!closeButtonClicked && !done) {
        cycle_count = 0;

        poll_control(player_ctrl, 0);
        poll_control(&menu_params.ctrl, 0);

        if (is_left(player_ctrl) || is_left(&menu_params.ctrl))
            sel = -1;
        if (is_right(player_ctrl) || is_right(&menu_params.ctrl))
            sel = 0;
        if (key[KEY_ESC]) {
            done = -1;
            sel = 0;
        }
        if (is_fire(player_ctrl) || is_fire(&menu_params.ctrl)
            || is_enter(&menu_params.ctrl))
            done = -1;

        if (choice) {
            vsync();
            draw_sprite(screen,
                        asset_bitmap(sel == 0 ? ASSET_DATA_BT_YES
                                              : ASSET_DATA_BT_YES_2),
                        240, 220);
            draw_sprite(screen,
                        asset_bitmap(sel == 0 ? ASSET_DATA_BT_NO_2
                                              : ASSET_DATA_BT_NO),
                        365, 220);
        }

        while (!cycle_count)
            rest(2);
    }

    /* and drain again on the way out -- finding 6 */
    poll_control(player_ctrl, 0);
    poll_control(&menu_params.ctrl, 0);
    while (is_any(player_ctrl) || is_any(&menu_params.ctrl)
           || key[KEY_ESC] || key[KEY_ENTER]) {
        poll_control(player_ctrl, 0);
        poll_control(&menu_params.ctrl, 0);
        rest(2);
    }
    clear_keybuf();

    blit(swap_screen, screen, 0, 0, 0, 0, 639, 479);
    return sel;
}
