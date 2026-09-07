/* play_jump_sound.c -- pick one of a character's 3 jump sound-effect
 * samples by how hard the player just jumped, and play it.
 *
 * Skipped in batch 2 (PROMOTIONS.md) for two reasons that turned out to
 * both be wrong, or at least incomplete, on closer inspection this pass
 * (batch 7):
 *
 * (1) "its 3 sound-handle globals (0x4fabf4/f8/fc) have no DWARF-recovered
 *     name" -- FALSE. They are not top-level globals at all: DWARF (
 *     artifacts/dwarf_info.txt, type offset 0x1bfae) names them
 *     Tcustom.jump_sound[3], a member of the already-named `custom` global
 *     (VA 0x4fa738; 0x4fa738 + 1212 == 0x4fabf4, matching the struct's
 *     own DW_AT_data_member_location). `custom`/`Tcustom` (with
 *     `jump_sound[3]`) are already in game_state.h/game_types.h -- the
 *     earlier pass's search only looked for a literal top-level DW_OP_addr
 *     match at the exact computed address and never checked whether that
 *     address fell inside an already-named aggregate's member/element
 *     range. src/icytower/names.json documents this investigation and the
 *     general rule for the next such claim.
 *
 * (2) "its real comparison domain ... is a call-trace, not a memory-domain
 *     one ... extending [the harness] to trace calls is out of scope" --
 *     true as far as it went, but the scope restriction it deferred to
 *     no longer applies: carrier/lift/harness/lift_check.py's Oracle now
 *     supports exactly this (win32_pilot.md task-brief "mechanism B" --
 *     see lift_check.py's CALL_TARGETS/_make_call_trace_hook and
 *     harness/pf_harness_calltrace.h/call_trace_stubs.c).
 *
 * Recovered from artifacts/disasm.txt (0x406ecc..0x406f5b): loads
 * p->sy (the vertical launch speed jump_player() just set, Tplayer offset
 * 0x18) and compares it (x87, `fucomp`/`fnstsw`/`test $0x45,%ah`/`je` --
 * the same "strict greater-than, ordered" idiom PROMOTIONS.md's
 * update_player entry already established for this exact instruction
 * shape) against two unnamed single-precision .rdata constants at
 * 0x4d6cc4/0x4d6cc8, read directly out of the original image with pefile
 * (not guessed): -22.0 and -15.0. sy is negative going up (jump_player.c),
 * so more negative == a harder jump: sy < -22.0 selects
 * custom.jump_sound[2] ("hi"), -22.0 <= sy < -15.0 selects
 * custom.jump_sound[1] ("med"), sy >= -15.0 (or unordered) selects
 * custom.jump_sound[0] ("lo") -- matching the array's own declaration
 * order (index 0 first tested last, i.e. the fallthrough/"softest jump"
 * case). play_sound()'s own two extra arguments are always the literal
 * constants 1, 1 in every one of the three call sites.
 *
 * No writes of its own (verified: PLAYER_VA is in this function's offline
 * "must_be_unchanged" set) -- its only observable effect is the argument
 * it passes to play_sound(), which is what the call-trace domain compares.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c, decl_line
 * unresolved by name (artifacts/dwarf_info.txt records the function itself
 * under DW_AT_name "play_jump_sound"), which names the parameter `p`.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

void play_jump_sound(Tplayer *p)
{
    if (p->sy < -22.0)
        play_sound(custom.jump_sound[2], 1, 1);
    else if (p->sy < -15.0)
        play_sound(custom.jump_sound[1], 1, 1);
    else
        play_sound(custom.jump_sound[0], 1, 1);
}
