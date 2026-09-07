/* clean_example.c -- fixture: must PASS scripts/check_native_layer.py.
 * Address-free clean C in the style src/icytower/*.c will use
 * (win32_pilot.md SS7a): ordinary externs, no carrier include, no guest
 * address, no reserved identifier prefix, no inline asm.
 */
#include "icytower_state.h"

extern int reward_scale;
extern int player_id;

void bump_reward(void)
{
    reward_scale += 1;
}

int is_player_alive(void)
{
    return player_id >= 0;
}
