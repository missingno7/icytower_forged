/* poll_control.c -- poll_control (0x401958, 294 bytes,
 * F:\projects\icytower\trunk\source\control.c decl_line 44), the ONE
 * function that turns real hardware state into the game's own
 * `Tcontrol.flags` byte.  PROMOTIONS.md batch 11.
 *
 * WHY ITS OWN FILE rather than the bottom of control.c (which holds the
 * rest of this CU):  control.c's `check_control_key(Tcontrol *c, int key)`
 * has a PARAMETER named `key`, and this function needs Allegro's global
 * `key[]` array -- which `carrier/gen/pf_lib_bindings.h` supplies as a
 * blunt textual `#define key (*(volatile char (*)[127])0x506988)`.  Pulling
 * allegro_api.h into control.c would macro-expand that parameter into a
 * syntax error in the carrier world: the same MEMBER_ACCESS_COLLISIONS
 * class PROMOTIONS.md batch 10 hit for `data`/`cycle_count` inside
 * draw_frame.c, here for a parameter name instead of a struct member.
 * Splitting the file is the cost-free fix -- one CU's functions may live in
 * several files here (map.c/add_floor, particle.c/... already do).
 *
 * WHAT IT DOES (0x401958-0x401a7d, read instruction by instruction):
 *
 *   c->flags = 0;                                  (0x401962)
 *   if (c->use_joy) {                              (0x401966, Tcontrol+0x0)
 *       poll_joystick();                           (0x4019f4)
 *       ... four axis half-deflection flags and up to 32 buttons, each
 *       OR-ing in the corresponding REMAPPABLE mask out of the global
 *       `gamepad` (Tgamepad, VA 0x4f8748) ...
 *   }
 *   if (!joystick_only)                            (0x401970/0x401973)
 *       ... seven keyboard scancodes out of c->key_*, each OR-ing in its
 *       FIXED mask ...
 *
 * The asymmetry is real and worth stating plainly, because it is the whole
 * reason `gamepad` exists: the KEYBOARD half hardcodes which bit each
 * action sets (`orb $0x4,0x20(%ebx)` etc. -- seven immediate constants, the
 * same CTRL_* assignment control.c's is_up/is_down/... already recovered),
 * while the JOYSTICK half loads the bit to set out of `gamepad`, so a pad's
 * four directions and 32 buttons are each independently remappable to any
 * control mask at runtime.  `or %al,0x20(%ebx)` ORs only the LOW BYTE of
 * each `int` field of Tgamepad -- writing `c->flags |= gamepad.up` in C is
 * bit-identical (the int is promoted, OR-ed, then truncated back into the
 * unsigned char).
 *
 * The joystick reads decode against Allegro's own `JOYSTICK_INFO joy[8]`
 * (VA 0x506a88), whose layout allegro_api.h carries:
 *   0x506ab0 = joy[0].stick[0].axis[1].d1   ("up",    +0x28)
 *   0x506ab4 = joy[0].stick[0].axis[1].d2   ("down",  +0x2c)
 *   0x506aa0 = joy[0].stick[0].axis[0].d1   ("left",  +0x18)
 *   0x506aa4 = joy[0].stick[0].axis[0].d2   ("right", +0x1c)
 *   0x506a90 = joy[0].num_buttons                     (+0x08)
 *   0x506bc0 = joy[0].button[0].b                     (+0x138, 8-byte stride)
 * -- i.e. stick 0's axis 1 is the vertical one and axis 0 the horizontal
 * one, exactly Allegro's convention.  Only joystick 0 is ever read; a
 * second pad is invisible to this game.
 *
 * The button loop's bound is `b < joy[0].num_buttons` with an additional
 * `b == 32` early exit (0x401a4c, checked at the top of every iteration
 * after the first) -- `gamepad.b[]` is `int[32]`, so the guard is the array
 * bound, not a coincidence.  Written below as the equivalent
 * `b < num_buttons && b < 32`: b starts at 0 and steps by 1, so "== 32" and
 * "< 32" cannot disagree.
 *
 * `joystick_only` (the DWARF parameter name, decl_line 44) gates ONLY the
 * keyboard half -- when set, the pad is still polled.  play() calls it as
 * poll_control(&ctrl, 0) everywhere in the gameplay loop; the menu code is
 * where the 1 comes from.
 *
 * Verified offline (PROMOTIONS.md batch 11): memory domain = the whole
 * Tcontrol plus the poll_joystick() call-trace slot, over vectors that
 * exercise both use_joy arms, every scancode/mask combination, and
 * num_buttons values on both sides of 32.  No floating point anywhere in
 * this function, so no x87 question arises.
 */
#include "allegro_api.h"   /* joy[], key[], poll_joystick */
#include "game_types.h"
#include "game_state.h"

/* Same seven masks control.c's is_up()..is_pause() already recovered from
 * their own `and eax, <mask>`; here they appear as this function's seven
 * `orb $<mask>,0x20(%ebx)` immediates, which is the other half of the same
 * evidence -- the producer and the consumers agree bit for bit. */
#define CTRL_LEFT  0x01
#define CTRL_RIGHT 0x02
#define CTRL_UP    0x04
#define CTRL_DOWN  0x08
#define CTRL_FIRE  0x10
#define CTRL_ENTER 0x20
#define CTRL_PAUSE 0x40

void poll_control(Tcontrol *c, int joystick_only)
{
    int b;

    c->flags = 0;

    if (c->use_joy) {
        poll_joystick();

        /* stick 0, axis 1 = vertical; axis 0 = horizontal. d1/d2 are
         * Allegro's own "deflected towards the low/high end" flags. */
        if (joy[0].stick[0].axis[1].d1)
            c->flags |= gamepad.up;
        if (joy[0].stick[0].axis[1].d2)
            c->flags |= gamepad.down;
        if (joy[0].stick[0].axis[0].d1)
            c->flags |= gamepad.left;
        if (joy[0].stick[0].axis[0].d2)
            c->flags |= gamepad.right;

        for (b = 0; b < joy[0].num_buttons && b < 32; b++)
            if (joy[0].button[b].b)
                c->flags |= gamepad.b[b];
    }

    if (!joystick_only) {
        if (key[c->key_up])
            c->flags |= CTRL_UP;
        if (key[c->key_down])
            c->flags |= CTRL_DOWN;
        if (key[c->key_left])
            c->flags |= CTRL_LEFT;
        if (key[c->key_right])
            c->flags |= CTRL_RIGHT;
        if (key[c->key_fire])
            c->flags |= CTRL_FIRE;
        if (key[c->key_enter])
            c->flags |= CTRL_ENTER;
        if (key[c->key_pause])
            c->flags |= CTRL_PAUSE;
    }
}
