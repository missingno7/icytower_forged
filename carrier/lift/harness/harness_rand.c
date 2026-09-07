/* harness_rand.c -- the compiled-candidate half of the rand() shim (see
 * pf_harness_rand.h's header comment for the full rationale). Defines the
 * function pf_harness_rand.h's `#define rand harness_rand` redirects
 * src/icytower/map.c's `rand()` calls to, and the state word src_check.c /
 * gcc_check.c set directly (from each vector's RAND_SEED_VA) before
 * calling add_floor().
 *
 * The algorithm is msvcrt's classic LCG, KNOWN from
 * assets/replay_checker/Icy Tower.cpp's rand_p() and confirmed by
 * notes/replay_checker_reference.md's "RNG" bullet:
 *   state = state*214013 + 2531011
 *   return (state >> 16) & 0x7fff
 * srand(seed) sets state=seed directly (no transform) -- this file's
 * harness_rand_state IS that state word, set directly by the driver
 * instead of going through a real srand() call.
 *
 * Harness-only: not part of src/icytower/, not compiled into the
 * STANDALONE or CARRIER worlds, only into harness/src_check.exe and the
 * harness/gcc_check_*.exe variants that exercise add_floor.
 */
unsigned int harness_rand_state = 1;

int harness_rand(void)
{
    harness_rand_state = harness_rand_state * 214013u + 2531011u;
    return (int)((harness_rand_state >> 16) & 0x7fffu);
}
