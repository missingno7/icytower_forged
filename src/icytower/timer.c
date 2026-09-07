/* timer.c -- the two timer-tick bookkeeping functions from
 * F:\projects\icytower\trunk\source\timer.c. install_timers() (the third
 * function in this CU) registers fps_counter as an Allegro `install_int`
 * callback and is not attempted this pass -- see src/icytower/ASSETS.md's
 * "compile only, both worlds" precedent for an Allegro-calling function
 * whose real effect (installing an interrupt handler) has no memory-domain
 * comparison to make.
 *
 * `cycle_count` is the same `logic_count`-adjacent global cluster
 * game_state.h already exposes for update_frame.c/is_solid.c: `logic_count`
 * itself (already load-bearing there) is one of the two counters
 * fps_counter samples and resets every call.
 *
 * Original source/decl_line: timer.c, decl_line 30 (cycle_counter, no
 * parameters) and decl_line 21 (fps_counter, no parameters)
 * (artifacts/dwarf_info.txt). Neither has a return value. No FPU in
 * either function.
 */
#include "game_state.h"

/* cycle_counter -- advance the free-running cycle counter by one. Called
 * (per install_timers(), not recovered this pass) once per fixed-rate
 * timer tick; drives the game's own tick pacing.
 * Recovered from artifacts/disasm.txt (0x41fed4..0x41fee3).
 */
void cycle_counter(void)
{
    cycle_count++;
}

/* fps_counter -- once-per-second sample-and-reset: latch the ticks
 * accumulated since the last call into `fps`/`lps` (frames/logic-steps
 * per second) and zero the running counters (`frame_count`, `logic_count`)
 * for the next second. Recovered from artifacts/disasm.txt
 * (0x41fea4..0x41fed1); field order in the original (frame_count/fps
 * pair first, then logic_count/lps) preserved.
 */
void fps_counter(void)
{
    fps = frame_count;
    frame_count = 0;
    lps = logic_count;
    logic_count = 0;
}
