/* call_trace_stubs.c -- COMPILED side of the offline harness's call-trace
 * comparison domain (win32_pilot.md task brief "mechanism B",
 * PROMOTIONS.md batch 7). See pf_harness_calltrace.h's own header comment
 * for the full rationale; this file provides the two definitions that
 * header declares.
 *
 * Hand-written harness plumbing (not generated, not part of src/ -- same
 * class of file as src_check.c/harness_rand.c): it is fine for it to know
 * about the carrier's memory model (PF_MEM) the way src/ must not.
 */
#include "pf_harness_mem.h"
#include "pf_harness_calltrace.h"
#include "game_state.h"     /* `data` -- under ICYTOWER_BINDINGS_ACTIVE (the
                             * MSVC harness world) this include is a no-op
                             * (game_state.h's own guard skips its externs,
                             * pf_bindings_harness.h's macro supplies `data`
                             * instead); in a plain-standalone GCC build
                             * (gcc_check.c, no bindings header force-
                             * included) it is what actually declares it. */
#include "assets_table.inc"

/* harness_trace_play_sound(): the compiled-candidate half of the call-trace
 * domain. Writes the SAME {call_count, arg0, arg1, arg2} shape into the
 * SAME scratch VA (CALLTRACE_PLAY_SOUND_VA) that lift_check.py's Oracle
 * hook writes on the ORIGINAL side, so the ordinary memory-domain diff
 * (no new comparator code) compares the two logs directly. `s` arrives as
 * whatever raw bit pattern the calling function read out of guest memory
 * (custom.jump_sound[i], sounds[8], combo_sound[tier]) -- never dereferenced
 * by play_jump_sound()/handle_player_collision_original()/start_reward()
 * themselves, so it round-trips as an opaque value exactly like
 * get_demo()'s raw pointer VALUE relay (PROMOTIONS.md batch 3) -- no
 * host/guest translation needed here either. */
void harness_trace_play_sound(SAMPLE *s, int pitch, int please_pan)
{
    unsigned int *slot = (unsigned int *)PF_MEM(CALLTRACE_PLAY_SOUND_VA);
    slot[0] += 1;
    slot[1] = (unsigned int)(size_t)s;
    slot[2] = (unsigned int)pitch;
    slot[3] = (unsigned int)please_pan;
}

/* asset_bitmap(): harness-only implementation of the ONE case
 * start_reward.c's own call actually reaches -- the "data" family, a pure
 * zero-copy memory read through the already PF_MEM-redirected `data`
 * global (identical to carrier/gen/pf_asset_bindings.h's real
 * implementation for this same family: `data[row->index].dat`). Does not
 * implement the "loading"/"char:*" families' load_datafile() fallback
 * (harness/pf_harness_calltrace.h's own header comment explains why: no
 * promoted src/ function reaches them, so modelling
 * load_datafile()/packfile_password() in this offline harness would be
 * unused machinery). asset_table[]/ASSET_DATAFILE_FAMILY_COUNT come from
 * src/icytower/assets_table.inc, the SAME generated, address-free data
 * table both real implementations already read -- not re-transcribed.
 *
 * `data` (the global) is bound through PF_MEM by pf_bindings_harness.h,
 * so reading its ADDRESS is already correct -- but the VALUE it holds
 * (the DATAFILE* the vector generator wrote, gen_start_reward's
 * DATA_TABLE_VA - 90*16) is a plain guest VA too, exactly like
 * ply[player_id]'s own pointer VALUE (src_check.c's header comment) --
 * PF_MEM only ever translates the address of a bound global, never a
 * pointer VALUE read back out of it. This function is the second
 * translation, symmetric with src_check.c's driver-side ply[player_id]/
 * demo fixups, just living here since it is asset_bitmap()'s own body
 * doing the read, not a driver dispatch branch. */
BITMAP *asset_bitmap(asset_id id)
{
    const struct asset_table_row *row = &asset_table[id];
    DATAFILE *base = (DATAFILE *)PF_MEM((unsigned int)(size_t)data);
    return (BITMAP *)base[row->index].dat;
}
