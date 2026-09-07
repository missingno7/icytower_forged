/* pf_harness_calltrace.h -- harness-only shim implementing the COMPILED
 * side of the call-trace comparison domain (win32_pilot.md task brief
 * "mechanism B", PROMOTIONS.md batch 7).
 *
 * WHY: play_jump_sound() / handle_player_collision_original() / start_reward()
 * each call play_sound() (0x406da4, a GAME function, not yet promoted to
 * src/) as part of their own recovered behaviour. Their real comparison
 * domain (per notes/promotion_candidates.md SS4.3, extended this pass to
 * handle_player_collision_original/start_reward too) is "which value
 * reached play_sound()'s arguments", not a register/memory digest of
 * play_sound() itself -- and even if it were, the offline harness's guest
 * buffer (harness/src_check.c's own header comment) is a plain malloc
 * region, deliberately not VirtualAlloc'd executable, so it cannot safely
 * jump into and execute play_sound()'s ORIGINAL bytes at their real address
 * the way the real carrier does.
 *
 * MECHANISM: on the ORIGINAL side, carrier/lift/harness/lift_check.py's
 * Oracle installs a UC_HOOK_CODE hook at play_sound's own entry VA
 * (0x406da4) -- exactly the same "hook the callee's entry instead of
 * executing it" trick already established for the msvcrt `_rand` thunk
 * (harness/pf_harness_rand.h) -- that captures play_sound's 3 cdecl
 * arguments off the stack into a fixed scratch slot (CALLTRACE_PLAY_SOUND_VA
 * = 0x7c1000: {call_count, arg0, arg1, arg2}, 16 bytes) and stubs a `ret`.
 * This header is the COMPILED half: force-included ahead of every file in
 * this build (the same "whole invocation" scope pf_harness_rand.h already
 * uses), it redirects the plain name `play_sound` to
 * harness_trace_play_sound() (harness/call_trace_stubs.c), which writes the
 * exact same {call_count, arg0, arg1, arg2} shape into the exact same
 * PF_MEM-redirected scratch VA, so lift_check.py's ordinary memory-domain
 * diff (no new comparator machinery) compares the two logs directly, plus
 * whatever other memory domain the function under test also writes.
 *
 * `play_sound` is NOT part of pf_bindings_harness.h's own macro table for
 * this build: harness/BUILD_NOTES (see build_src.cmd's own header comment)
 * regenerates pf_bindings_harness.h with `play_sound` added to
 * scan_src_defs.py's auto-scanned --exclude list (a harness-only addition,
 * NOT applied to carrier/gen/pf_bindings_src.h -- play_sound is not
 * promoted to src/, so the real carrier build must still redirect it to its
 * original address) -- so there is no macro-redefinition conflict between
 * this header and pf_bindings_harness.h; the plain name is free for this
 * header's own #define below to claim.
 *
 * NOT part of src/icytower/ -- an src/icytower/ .c file calls plain, ordinary
 * `play_sound(...)` (declared address-free in game_funcs.h, src/README.md's
 * usual contract), and that stays correct in the STANDALONE and CARRIER
 * worlds (a real play_sound(), whether still-ORIGINAL via pf_bindings_src.h
 * or, once promoted, a real src/ definition). This redirection exists ONLY
 * for the offline equivalence harness's own build, exactly like
 * pf_harness_rand.h's rand() redirection.
 */
#ifndef PF_HARNESS_CALLTRACE_H
#define PF_HARNESS_CALLTRACE_H

#include "game_types.h"   /* SAMPLE, forward-declared by allegro_types.h */
#include "assets.h"       /* asset_id -- start_reward.c's own #include */

#define CALLTRACE_PLAY_SOUND_VA 0x7c1000u

void harness_trace_play_sound(SAMPLE *s, int pitch, int please_pan);

#define play_sound harness_trace_play_sound

/* batch 8 (2026-09-07): the same mechanism extended to three Allegro-family
 * callees draw_scroller.c calls (set_clip_rect/textout_ex/textout_centre_ex)
 * -- see lift_check.py's LIB_CALL_TARGETS / _make_call_trace_hook header
 * comment for the FIRST-CALL-CAPTURE rule both sides now share (call_count
 * increments on every call; the argument slot is only written the first
 * time, so a deterministic later call -- e.g. draw_scroller's own second,
 * "restore to the whole bitmap" set_clip_rect -- cannot mask an earlier,
 * argument-dependent one). Needs allegro_types.h's BITMAP/FONT (already
 * pulled in transitively by draw_scroller.c's own "allegro_api.h" include,
 * which this header is force-included ahead of). */
#define CALLTRACE_SET_CLIP_RECT_VA 0x7c1100u
#define CALLTRACE_TEXTOUT_EX_VA 0x7c1200u
#define CALLTRACE_TEXTOUT_CENTRE_EX_VA 0x7c1300u

void harness_trace_set_clip_rect(BITMAP *bmp, int x1, int y1, int x2, int y2);
void harness_trace_textout_ex(BITMAP *bmp, const FONT *f, const char *s,
                               int x, int y, int color, int bg);
void harness_trace_textout_centre_ex(BITMAP *bmp, const FONT *f, const char *s,
                                      int x, int y, int color, int bg);

#define set_clip_rect harness_trace_set_clip_rect
#define textout_ex harness_trace_textout_ex
#define textout_centre_ex harness_trace_textout_centre_ex

/* start_reward.c's own asset_bitmap(ASSET_DATA_REWARD_000 + tier) call
 * (src/icytower/ASSETS.md's "5-function API" seam, used exactly as
 * documented there): assets.h only declares the 5-function API when
 * ICYTOWER_BINDINGS_ACTIVE is NOT defined (its own #ifndef guard -- the
 * carrier-world branch assumes carrier/gen/pf_asset_bindings.h, force-
 * included ahead of src/, supplies the real definition instead). This
 * harness build defines ICYTOWER_BINDINGS_ACTIVE (pf_bindings_harness.h's
 * own --guard-define) but does NOT force-include pf_asset_bindings.h --
 * that header's fallback path for the "loading"/"char:*" families needs
 * load_datafile()/packfile_password() (real Allegro imports via
 * pf_lib_bindings.h, which this offline harness has no need to model at
 * all: start_reward only ever reads the "data" family, a pure memory read
 * through the already PF_MEM-redirected `data` global, per ASSETS.md's own
 * carrier-world description). harness/call_trace_stubs.c provides a
 * harness-only asset_bitmap() that does exactly that one memory read
 * (nothing else -- see its own header comment); this prototype is what
 * lets start_reward.c's own translation unit see it declared, matching
 * the "declared exactly once, ahead of every file" shape
 * pf_bindings_harness.h/pf_harness_rand.h already established. */
BITMAP *asset_bitmap(asset_id id);

#endif /* PF_HARNESS_CALLTRACE_H */
