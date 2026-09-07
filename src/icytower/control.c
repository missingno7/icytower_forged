/* control.c -- Tcontrol accessors from F:\projects\icytower\trunk\source\control.c,
 * grouped in one file per that shared, small CU.
 *
 * `Tcontrol.flags` (an `unsigned char`, DWARF-confirmed, no bitfield info
 * survived -- game_types.h just has `unsigned char flags`) packs one
 * direction/action per bit; the bit assignment below was not recovered
 * from DWARF (no field-level names exist for it) but read directly off
 * each function's `and eax, <mask>` in artifacts/disasm.txt, one mask per
 * function, all against the same byte at Tcontrol+0x20:
 *
 *   is_left  0x01   is_right 0x02   is_up  0x04   is_down  0x08
 *   is_fire  0x10   is_enter 0x20   is_pause 0x40
 *   is_any   tests every bit except CTRL_PAUSE (mask 0xffffffbf, i.e.
 *            ~CTRL_PAUSE) -- "any key" that should not count a paused game
 *            as active input.
 *
 * Every one of the eight predicates below compiles (via slightly different
 * code sequences depending on whether the mask is exactly bit 0, per
 * artifacts/disasm.txt) to the same C idiom: return -1 (all bits set) if
 * the flag is set, 0 otherwise -- the classic "boolean as -1/0" x86 idiom,
 * not a plain 0/1 boolean. Cross-checked against
 * carrier/lift/lifted/lifted_is_{up,down,left,right,fire,pause,enter,any}.c
 * (generated mechanically from the same bytes). No FPU anywhere in this
 * file.
 *
 * Original source/decl_line for all nine functions below: control.c,
 * decl_line 39 (get_gamepad) and 87-94 (is_up..is_any), each naming its
 * Tcontrol* parameter `c` (artifacts/dwarf_info.txt).
 */
#include "game_types.h"
#include "game_state.h"

#define CTRL_LEFT  0x01
#define CTRL_RIGHT 0x02
#define CTRL_UP    0x04
#define CTRL_DOWN  0x08
#define CTRL_FIRE  0x10
#define CTRL_ENTER 0x20
#define CTRL_PAUSE 0x40

/* get_gamepad -- accessor for the single global gamepad state. */
Tgamepad *get_gamepad(void)
{
    return &gamepad;
}

int is_up(Tcontrol *c)
{
    return (c->flags & CTRL_UP) ? -1 : 0;
}

int is_down(Tcontrol *c)
{
    return (c->flags & CTRL_DOWN) ? -1 : 0;
}

int is_left(Tcontrol *c)
{
    return (c->flags & CTRL_LEFT) ? -1 : 0;
}

int is_right(Tcontrol *c)
{
    return (c->flags & CTRL_RIGHT) ? -1 : 0;
}

int is_fire(Tcontrol *c)
{
    return (c->flags & CTRL_FIRE) ? -1 : 0;
}

int is_pause(Tcontrol *c)
{
    return (c->flags & CTRL_PAUSE) ? -1 : 0;
}

int is_enter(Tcontrol *c)
{
    return (c->flags & CTRL_ENTER) ? -1 : 0;
}

/* is_any -- true if any control other than pause is active. */
int is_any(Tcontrol *c)
{
    return (c->flags & (unsigned char)~CTRL_PAUSE) ? -1 : 0;
}
