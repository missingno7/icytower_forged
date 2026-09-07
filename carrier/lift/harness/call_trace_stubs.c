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

/* pf_untranslate() -- the REVERSE of PF_MEM(): host pointer -> guest VA.
 *
 * batch 8 (2026-09-07), found while verifying draw_scroller: unlike
 * play_sound's SAMPLE* (never dereferenced -- rounds-trips as an opaque
 * bit pattern with no translation needed anywhere, PROMOTIONS.md batch 7)
 * or a plain struct FIELD holding a sentinel dword (sc->fnt/sc->text/
 * sc->lines[i] -- never a pointer the DRIVER itself translates, so its
 * stored bytes already round-trip unchanged), a BITMAP* ARGUMENT is
 * translated host<-guest at the call boundary by every driver's tr()
 * (src_check.c/gcc_check.c), because draw_scroller.c genuinely
 * dereferences it (bmp->w, bmp->h for the final "restore full clip rect"
 * call) -- draw_scroller.c needs a real host pointer to do that. But that
 * SAME pointer is then relayed, unchanged, as an ARGUMENT to
 * set_clip_rect()/textout_ex()/textout_centre_ex() -- and the ORIGINAL
 * side (unicorn, executing the real bytes directly in guest address
 * space) never translates anything, so whatever guest VA the vector
 * generator chose (e.g. BMP_VA = 0x7a9000) is exactly what reaches the
 * traced call there. Logging the COMPILED side's raw host pointer instead
 * would compare two different numbers for the same logical bitmap
 * (confirmed: this was the very first draw_scroller run's own DIFFER,
 * `set_clip_rect_trace+0x5`, one byte of the host malloc address instead
 * of BMP_VA's own 0x90 byte) -- so any BITMAP or FONT pointer argument a
 * call-trace stub logs must be reverse-translated back to the guest VA it
 * came from
 * first, symmetric with every "second translation, driver-side" fixup
 * this project's dispatch code already needed for a pointer VALUE read
 * out of guest memory (ply[player_id]/demo/data). A pointer outside the
 * pf_guest range (should not happen for anything src/icytower/ passes
 * here) is left as-is rather than producing a garbage VA. */
static unsigned int pf_untranslate(const void *hostptr)
{
    const unsigned char *h = (const unsigned char *)hostptr;
    if (h >= pf_guest && h < pf_guest + PF_GUEST_SIZE)
        return PF_GUEST_BASE + (unsigned int)(h - pf_guest);
    return (unsigned int)(size_t)hostptr;
}

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

/* batch 8 (2026-09-07): the compiled-candidate half of the three Allegro-
 * family call traces draw_scroller.c needs (pf_harness_calltrace.h's own
 * header comment has the full design). All three share the SAME
 * first-call-capture rule lift_check.py's Oracle._make_call_trace_hook now
 * implements on the ORIGINAL side: call_count always increments; the
 * argument words are written only the FIRST time (count == 0 on entry),
 * so a later, deterministic call to the same callee cannot overwrite an
 * earlier, argument-dependent one's logged arguments. */
void harness_trace_set_clip_rect(BITMAP *bmp, int x1, int y1, int x2, int y2)
{
    unsigned int *slot = (unsigned int *)PF_MEM(CALLTRACE_SET_CLIP_RECT_VA);
    if (slot[0] == 0) {
        slot[1] = pf_untranslate(bmp);
        slot[2] = (unsigned int)x1;
        slot[3] = (unsigned int)y1;
        slot[4] = (unsigned int)x2;
        slot[5] = (unsigned int)y2;
    }
    slot[0] += 1;
}

void harness_trace_textout_ex(BITMAP *bmp, const FONT *f, const char *s,
                               int x, int y, int color, int bg)
{
    unsigned int *slot = (unsigned int *)PF_MEM(CALLTRACE_TEXTOUT_EX_VA);
    if (slot[0] == 0) {
        slot[1] = pf_untranslate(bmp);
        slot[2] = (unsigned int)(size_t)f;
        slot[3] = (unsigned int)(size_t)s;
        slot[4] = (unsigned int)x;
        slot[5] = (unsigned int)y;
        slot[6] = (unsigned int)color;
        slot[7] = (unsigned int)bg;
    }
    slot[0] += 1;
}

void harness_trace_textout_centre_ex(BITMAP *bmp, const FONT *f, const char *s,
                                      int x, int y, int color, int bg)
{
    unsigned int *slot = (unsigned int *)PF_MEM(CALLTRACE_TEXTOUT_CENTRE_EX_VA);
    if (slot[0] == 0) {
        slot[1] = pf_untranslate(bmp);
        slot[2] = (unsigned int)(size_t)f;
        slot[3] = (unsigned int)(size_t)s;
        slot[4] = (unsigned int)x;
        slot[5] = (unsigned int)y;
        slot[6] = (unsigned int)color;
        slot[7] = (unsigned int)bg;
    }
    slot[0] += 1;
}
