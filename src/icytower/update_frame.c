/* update_frame.c -- per-tick maintenance, called once per game tick from
 * play() (win32_pilot.md SS2).
 *
 * Recovered from the verified NATIVE form (carrier/native/native_update_frame.c,
 * carrier/native/README.md; equivalence result: artifacts/native_equivalence.json,
 * 20000 vectors, EQUAL), rewritten as ordinary game source: no PF_MEM, no
 * generated interop headers, globals used as plain identifiers. See
 * src/README.md for how the same file below still resolves those
 * identifiers to the original game memory when built into the carrier.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c, decl_line 2826
 * (artifacts/dwarf_info.txt).
 *
 * Two independent pieces of state:
 *
 * 1. Reward-bar decay/growth. `reward_time` (counts down to 0) and
 *    `reward_scale` (a HUD value) implement a three-band ramp: while
 *    reward_time is still counting (!= 0), reward_scale grows by 0xccd
 *    (3277) once reward_time > 60, drains by 0x199a (6554) once
 *    reward_time <= 9, and is left untouched in the 10-60 plateau in
 *    between; either way reward_time decrements by 1. This block is, per
 *    DWARF, the inlined call update_reward(reward_time) at main.c:2828
 *    (DW_TAG_inlined_subroutine, abstract_origin update_reward) -- the
 *    compiler inlined it into update_frame, so there is no separate
 *    update_reward function to call here; its body is reproduced in place.
 *
 * 2. The active player's per-tick counters, via ply[player_id]: the death
 *    animation counter `dead` advances by 8 while it is in progress and
 *    still inside its animation range (0 < dead <= 0x12b = 299); the
 *    edge-of-floor indicator counter `edge_drawn` advances while `edge` is
 *    set; the walk-cycle `frame` counter advances once every 10 ticks of
 *    the (volatile, timer-thread-shared) `logic_count` global.
 *
 * No return value.
 */
#include "game_types.h"
#include "game_state.h"

void update_frame(void)
{
    if (reward_time != 0) {
        if (reward_time > 60)
            reward_scale += 0xccd;         /* growing: +0xccd (3277) */
        else if (reward_time <= 9)
            reward_scale -= 0x199a;        /* draining: -0x199a (6554) */
        /* else (9 < reward_time <= 60): reward_scale is left unchanged --
         * the plateau band; only the countdown below still runs. */
        reward_time -= 1;
    }

    {
        Tplayer *p = ply[player_id];

        /* death-animation counter: while in progress (dead != 0) and still
         * inside its animation range (dead <= 0x12b = 299), advance it 8
         * ticks' worth per frame. */
        if (p->dead != 0 && p->dead <= 0x12b)
            p->dead += 8;

        /* edge-of-floor indicator: count how many ticks it has been shown. */
        if (p->edge != 0)
            p->edge_drawn++;

        /* walk-cycle frame: advances once every 10 logic ticks. */
        if (logic_count % 10 == 0)
            p->frame++;
    }
}
