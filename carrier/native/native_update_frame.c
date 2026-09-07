/* native_update_frame.c -- NATIVE form of update_frame (VA 0x00406ac4).
 *
 * Hand-written, not generated. Read directly from artifacts/disasm.txt
 * (0x00406ac4-0x00406b3b) and cross-checked instruction-for-instruction
 * against carrier/lift/lifted/lifted_update_frame.c (the LIFTED form,
 * win32_pilot.md SS3), which is the faithful-but-unreadable reference this
 * file was written to match. See carrier/native/README.md for the full
 * local/global name mapping and the argument for every place where writing
 * this readably required a construct that isn't a literal byte-for-byte
 * transcription of the assembly.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c, decl_line 2826
 * (artifacts/dwarf_info.txt). Same prototype as PFN_update_frame in
 * carrier/gen/it_funcs.h.
 *
 * Memory model (win32_pilot.md SS3, carrier/lift/README.md SS7): PF_MEM() is
 * the single seam shared with the LIFTED form and defined in
 * carrier/lift/lifted/pf_rt.h. It is the identity inside the real carrier,
 * where the original image is mapped at its real base so a guest VA already
 * *is* a valid host pointer; the offline harness redefines it
 * (carrier/lift/harness/pf_harness_mem.h) to reach an in-process copy of the
 * image instead, so this file compiles and runs unchanged in both places.
 * it_globals.h/it_types.h supply the real DWARF names, addresses and struct
 * layouts; their IT_G_* macros are never dereferenced directly here (that
 * would bypass PF_MEM and read/write the wrong copy in the harness) -- only
 * their address is taken and fed through PF_MEM, once per global or pointer.
 * PF_MEM is therefore this file's only non-obvious dependency, and it is
 * used nowhere else.
 */
#include "pf_rt.h"      /* the one memory seam: PF_MEM() */
#include "it_types.h"
#include "it_funcs.h"
#include "it_globals.h" /* names/addresses only -- IT_G_* is never dereferenced here */

void __cdecl native_update_frame(void)
{
    int      *reward_time_p  = (int      *)PF_MEM((unsigned int)&IT_G_reward_time);
    fixed    *reward_scale_p = (fixed    *)PF_MEM((unsigned int)&IT_G_reward_scale);
    int       player_id      = *(int     *)PF_MEM((unsigned int)&IT_G_player_id);
    Tplayer *(*ply)[1000]    = (Tplayer *(*)[1000])PF_MEM((unsigned int)&IT_G_ply);
    int       logic_count    = *(volatile int *)PF_MEM((unsigned int)&IT_G_logic_count);
    int       reward_time    = *reward_time_p;

    /* This whole block was the inlined call update_reward(reward_time) at
     * main.c:2828 (artifacts/dwarf_info.txt: DW_TAG_inlined_subroutine,
     * abstract_origin "update_reward", entry_pc 0x00406ad1). It decays/grows
     * the fixed-point HUD reward-bar value and counts reward_time down to 0,
     * one tick at a time. reward_scale is `fixed` (int32_t, Allegro
     * fixed-point), so += / -= here are plain 32-bit integer arithmetic,
     * matching the original's add/sub bit-for-bit including wraparound. */
    if (reward_time != 0) {
        if (reward_time > 60)
            *reward_scale_p += 0xccd;          /* growing: +0xccd (3277) */
        else if (reward_time <= 9)
            *reward_scale_p -= 0x199a;         /* draining: -0x199a (6554) */
        /* else (9 < reward_time <= 60): reward_scale is left unchanged --
         * the plateau band; only the countdown below still runs. */
        *reward_time_p = reward_time - 1;
    }

    /* the active player: ply[player_id] (main.c's global player table). The
     * value read out of ply[] is itself a guest VA, so it is threaded
     * through PF_MEM a second time before any field of *p is touched. */
    Tplayer *p = (Tplayer *)PF_MEM((unsigned int)(size_t)(*ply)[player_id]);

    /* death-animation counter: while in progress (dead != 0) and still
     * inside its animation range (dead <= 0x12b = 299), advance it 8
     * ticks' worth per frame. */
    if (p->dead != 0 && p->dead <= 0x12b)
        p->dead += 8;

    /* edge-of-floor indicator: count how many ticks it has been shown. */
    if (p->edge != 0)
        p->edge_drawn++;

    /* walk-cycle frame: advances once every 10 logic ticks. logic_count is
     * `volatile int` (shared with the 20ms timer thread, win32_pilot.md
     * SS2/SS5), so it is read exactly once above -- matching the original's
     * single `mov eax, [0x506958]` -- and every use below reads that same
     * captured value, never the live global again. */
    if (logic_count % 10 == 0)
        p->frame++;
}
