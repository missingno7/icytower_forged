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
 *
 * Batch 3 additions (set_control, init_control, check_control_key):
 *
 * `set_control` is DWARF-recovered as a genuine named, `DW_AT_inline: 1`
 * function (decl_line 31, params c/up/down/left/right/fire,
 * artifacts/dwarf_info.txt offset 0x45f1) -- carrier/gen/gen_interop.py's
 * function-name pass only walks concrete (non-abstract) DW_TAG_subprogram
 * entries with their own DW_AT_name, so the *out-of-line copy* at 0x4017d4
 * (the one this file recovers -- GCC still emits one because the address
 * is taken/called normally, inline hint or not) came back nameless from
 * that pass (artifacts/functions.json/dwarf_subprograms.json both show
 * `"name": null` for VA 0x4017d4); resolving its `DW_AT_abstract_origin:
 * <0x45f1>` back to the inline declaration recovers the real name and
 * parameter names directly from DWARF, not from the COFF export table
 * (which does carry the same spelling, `_set_control`, corroborating it).
 * `init_control` (decl_line 16) itself contains a
 * `DW_TAG_inlined_subroutine` with the same `abstract_origin` at
 * 0x401796-0x4017b9 -- i.e. the original source's `init_control` body
 * literally reads `set_control(c, 0x54, 0x55, 0x52, 0x53, 0x4b);` followed
 * by three more field stores of its own; recovered as exactly that call
 * plus those three stores, not re-inlined by hand. The five key/scan
 * values passed (0x54/0x55/0x52/0x53/0x4b) and the two set directly
 * (key_enter=0x43, key_pause=0x10) are preserved as literal defaults;
 * no independent source in this project's evidence (artifacts/,
 * notes/library_boundary.md's KEY_* constants) corroborates a specific
 * keyboard mapping for them, so they are recorded as-is rather than
 * guessed at.
 *
 * `check_control_key` (decl_line 75, params c/key) reports whether `key`
 * is currently bound to any of the five remappable actions plus
 * enter/pause (i.e. every field set_control/init_control can set, in the
 * same field order the disassembly compares them: key_left, key_right,
 * key_up, key_down, key_fire, key_enter, key_pause) -- `use_joy` and
 * `flags` are not part of the comparison. Same -1/0 idiom as the is_*
 * predicates above (`mov $0xffffffff,%eax` on a hit; falls through to
 * `xor %eax,%eax` otherwise).
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

/* set_control -- bind the five remappable movement/action keys at once.
 * DWARF-named (see file header); leaves use_joy, key_enter, key_pause and
 * flags untouched. */
void set_control(Tcontrol *c, int up, int down, int left, int right, int fire)
{
    c->key_up = up;
    c->key_down = down;
    c->key_left = left;
    c->key_right = right;
    c->key_fire = fire;
}

/* init_control -- (re)initialize a Tcontrol to its hard-coded defaults.
 * Original source inlines a call to set_control() for the five remappable
 * keys, then sets the two fixed ones and clears flags/use_joy (see file
 * header for the inlined-subroutine evidence). */
void init_control(Tcontrol *c)
{
    set_control(c, 0x54, 0x55, 0x52, 0x53, 0x4b);
    c->key_enter = 0x43;
    c->key_pause = 0x10;
    c->flags = 0;
    c->use_joy = 0;
}

/* check_control_key -- true if `key` is currently bound to any of the
 * five remappable actions, or to enter/pause. */
/* `key_arg` rather than DWARF's own `key`: Allegro's global key[] array is
 * a blunt textual `#define key ...` in carrier/gen/pf_lib_bindings.h, so a
 * parameter spelled `key` macro-expands into a syntax error the moment this
 * file is compiled in the carrier world with that header force-included
 * (found while doing the same for handle_player_input's `ctrl` in
 * PROMOTIONS.md batch 11).  carrier/win32_policy.json's `param_renames`
 * already renames it to `key_arg` in the generated game_funcs.h prototype;
 * this definition now matches. */
int check_control_key(Tcontrol *c, int key_arg)
{
    if (key_arg == c->key_left || key_arg == c->key_right ||
        key_arg == c->key_up || key_arg == c->key_down ||
        key_arg == c->key_fire || key_arg == c->key_enter ||
        key_arg == c->key_pause)
        return -1;
    return 0;
}
