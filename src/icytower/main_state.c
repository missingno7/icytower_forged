/* main_state.c -- a cluster of trivial single-global accessors/mutators
 * from F:\projects\icytower\trunk\source\main.c, kept in their own file
 * rather than folded into update_frame.c/line_intersect.c (main.c's two
 * other recovered functions so far) since none of the three share any
 * logic with this group. get_version_str()/get_demo()/ok_to_play() (the
 * remaining trivial getters) are not attempted this pass: get_version_str
 * returns a literal string address (0x4d4b20, not a named global -- its
 * bytes were not independently re-extracted this pass) and ok_to_play()
 * always returns the constant 1 with no observable domain worth adding to
 * this batch.
 *
 * Original source/decl_line (artifacts/dwarf_info.txt), none of the five
 * taking a parameter or touching the FPU:
 *   get_demo              decl_line 565
 *   get_controls           decl_line 570
 *   switchedFromProgram    decl_line 1342
 *   switchedToProgram      decl_line 1348
 *   clickedCloseButton     decl_line 1354
 */
#include "game_types.h"
#include "game_state.h"

/* get_demo -- accessor for the single global demo-replay pointer.
 * Recovered from artifacts/disasm.txt (0x40696c..0x406976): `return demo;`
 */
Treplay *get_demo(void)
{
    return demo;
}

/* get_controls -- accessor for the single global player-1 control state,
 * the same "return &global" idiom as get_gamepad() (control.c): the
 * address literal in the disassembly (0x5000c8) is just the compiler's
 * computed &ctrl, not a value this source ever writes down.
 * Recovered from artifacts/disasm.txt (0x406978..0x406982).
 */
Tcontrol *get_controls(void)
{
    return &ctrl;
}

/* switchedFromProgram -- Allegro window-focus-lost callback: records that
 * the game no longer has input focus. Recovered from artifacts/disasm.txt
 * (0x406a5c..0x406a6a).
 */
void switchedFromProgram(void)
{
    hasFocus = 0;
}

/* switchedToProgram -- Allegro window-focus-gained callback: the mirror
 * of switchedFromProgram(). Recovered from artifacts/disasm.txt
 * (0x406a6c..0x406a7a).
 */
void switchedToProgram(void)
{
    hasFocus = 1;
}

/* clickedCloseButton -- Allegro window-close-button callback: latches the
 * close request for the main loop to act on. Recovered from
 * artifacts/disasm.txt (0x406a7c..0x406a89).
 */
void clickedCloseButton(void)
{
    closeButtonClicked = 1;
}
