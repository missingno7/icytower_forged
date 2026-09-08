/* sound.c -- the three audio entry points main.c exposes to the rest of
 * the game: play_sound() (every sample the gameplay tick plays goes
 * through it) and the startGameMusic()/stopGameMusic() pair.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c (the same CU
 * as update_frame.c / line_intersect.c / main_state.c / new_rand.c).
 *
 * Why these three, and why now (PROMOTIONS.md batch 13): batch 11's
 * "What is still ORIGINAL in the tick body" table named five functions,
 * and play_sound was the one it explicitly could not promote --
 *
 *     "play_sound is the callee EIGHT existing SPECS entries trace through
 *      CALLTRACE_PLAY_SOUND_VA, and pf_harness_calltrace.h redirects the
 *      plain name to the stub for the whole harness build -- promoting it
 *      means that redirect would rename its own DEFINITION."
 *
 * That is still true of the SPECS-table harness build, and this batch does
 * NOT touch it: play_jump_sound / handle_player_collision_original /
 * start_reward keep tracing a stubbed play_sound exactly as before (their
 * comparison domain is "which value reached play_sound's arguments", which
 * is unchanged by play_sound acquiring a real definition elsewhere).  What
 * this file adds is a SECOND, independent oracle -- carrier/lift/harness/
 * batch13b_check.py -- that verifies play_sound itself, one translation
 * unit at a time, in its own executable that never links the SPECS harness.
 * The two builds are disjoint, so neither redirect renames the other's
 * definition.  batch 11's blocker was a property of ONE build, not of the
 * function.
 *
 * ---------------------------------------------------------------------
 * play_sound  (0x406da4, 215 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x406da4..0x406e7a.  Prototype and
 * parameter names from the generated game_funcs.h:
 *     void play_sound(SAMPLE *s, int pitch, int please_pan)
 *
 *   406dae  itrcheck != 0            -> return, nothing played at all
 *           (the .itr integrity-check run mutes the game entirely; the
 *           same global gates log2file() in logfile.c)
 *   406db8  pitch != 0               -> freq = new_rand() % 300 + 925
 *           else                        freq = 1000
 *           (0x3e8 = 1000 is Allegro's "play at the sample's own rate";
 *           the random spread is +-37.5 semitone-cents-ish, 925..1224)
 *   406dc4  s == NULL                -> return  (checked AFTER the pitch
 *           roll, so a NULL sample still CONSUMES a new_rand() draw --
 *           load-bearing, since new_rand() is the game's own deterministic
 *           LCG and a skipped draw would desynchronise a replay)
 *   406dc8  options.snd_volume == 0  -> return, and the volume is passed
 *           through verbatim as play_sample's `vol`
 *   406dd1  please_pan != 0          -> pan follows the local player's x:
 *               pan = (int)((float)(ply[player_id]->x / 640.0f)
 *                           * 192.0f + 32.0f)
 *             and is ALSO published in the global any11 (0x4dd170).
 *           else pan = 128 (0x80, dead centre) and any11 is left alone.
 *   406ddd  fast_forward       != 0  -> freq <<= 1
 *   406de8  fast_fast_forward  != 0  -> freq <<= 1   (so 4x when both)
 *   406e0a  play_sample(s, options.snd_volume, pan, freq, 0)  -- loop = 0
 *
 * The pan arithmetic is the one x87 site in this file and is written to
 * reproduce the original's instruction shape exactly:
 *   fldl (ply[player_id])      -- Tplayer.x is a double at offset 0
 *   fdivs  640.0f              -- single-precision divisor
 *   fstps/flds -0xc(%ebp)      -- ROUNDED THROUGH A 32-BIT FLOAT, then
 *                                 reloaded: the `float pan_x` local below
 *                                 is that store, not decoration.  Dropping
 *                                 it would leave the quotient in an 80-bit
 *                                 register and change the truncation on
 *                                 roughly a third of inputs.
 *   fmuls  192.0f / fadds 32.0f -- kept in the x87 register stack (no
 *                                 store between them), so they are written
 *                                 as one expression here.
 *   fnstcw/fldcw 0x0c00/fistpl -- the ordinary C (int) truncation.
 * 640/192/32 read out of .rdata at 0x4d6cb8/0x4d6cbc/0x4d6cc0 with pefile
 * (all three exactly representable, so the float/double question does not
 * arise for the constants themselves).  The result maps x in [0,640] onto
 * Allegro's pan range as 32..224 -- not 0..255: the game never hard-pans.
 *
 * ---------------------------------------------------------------------
 * startGameMusic  (0x40cb30, 144 bytes)  /  stopGameMusic (0x40caf4, 58)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x40cb30..0x40cbbf and
 * 0x40caf4..0x40cb2d.  Three globals, all already declared in
 * game_state.h: gameMusicVoiceID (0x4bc178), custom (0x4fa738 -- bg_music
 * at +0x4d8, bg_midi at +0x4dc, both confirmed against game_types.h's own
 * Tcustom layout by a compiled offsetof() probe, not by eyeballing), and
 * bg_beat (0x4dd2a8).
 *
 * startGameMusic picks exactly one of three sources, in priority order,
 * and always resets gameMusicVoiceID to -1 first:
 *   1. custom.bg_music  -- a per-character SAMPLE, looped
 *   2. custom.bg_midi   -- a per-character MIDI (set_volume THEN play_midi;
 *                          note gameMusicVoiceID stays -1 on this path,
 *                          which is what makes stopGameMusic's own
 *                          `>= 0` guard meaningful)
 *   3. bg_beat          -- the built-in loop, same play_sample call as (1)
 *                          (the disassembly literally jumps back into
 *                          case 1's instruction block at 0x40cb53 with
 *                          bg_beat in the register, which is why the two
 *                          calls are written identically here)
 * and does nothing at all when options.msc_volume is 0 -- checked before
 * any source is looked at, so a muted game does not even latch a voice ID.
 *
 * stopGameMusic is the mirror, but it is NOT a simple "undo whatever
 * startGameMusic did": it unconditionally tries all three teardowns,
 * guarded only on the state each one needs (`gameMusicVoiceID >= 0`,
 * `custom.bg_music != NULL`, `custom.bg_midi != NULL`), so it also cleans
 * up after a source that a *previous* startGameMusic left running.  The
 * stop_midi() call is a tail jump (`leave; jmp _stop_midi`) with no
 * arguments.
 *
 * play_sample/set_volume/play_midi/voice_stop/stop_sample/stop_midi are
 * ordinary named Allegro imports, declared address-free in the generated
 * allegro_api.h; nothing here needs an AL_INLINE shim.
 */
#include "allegro_api.h"        /* SAMPLE, MIDI, play_sample, set_volume,
                                   play_midi, voice_stop, stop_sample,
                                   stop_midi */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"         /* new_rand */

void play_sound(SAMPLE *s, int pitch, int please_pan)
{
    int freq;
    int pan;

    if (itrcheck)
        return;

    if (pitch)
        freq = new_rand() % 300 + 925;
    else
        freq = 1000;

    if (!s)
        return;
    if (!options.snd_volume)
        return;

    if (please_pan) {
        float pan_x = (float)(ply[player_id]->x / 640.0f);
        pan = (int)(pan_x * 192.0f + 32.0f);
        any11 = pan;
    } else {
        pan = 128;
    }

    if (fast_forward)
        freq <<= 1;
    if (fast_fast_forward)
        freq <<= 1;

    play_sample(s, options.snd_volume, pan, freq, 0);
}

void startGameMusic(void)
{
    gameMusicVoiceID = -1;

    if (!options.msc_volume)
        return;

    if (custom.bg_music) {
        gameMusicVoiceID = play_sample(custom.bg_music, options.msc_volume,
                                       128, 1000, 1);
        return;
    }

    if (custom.bg_midi) {
        set_volume(-1, options.msc_volume);
        play_midi(custom.bg_midi, 1);
        return;
    }

    if (bg_beat)
        gameMusicVoiceID = play_sample(bg_beat, options.msc_volume,
                                       128, 1000, 1);
}

void stopGameMusic(void)
{
    if (gameMusicVoiceID >= 0)
        voice_stop(gameMusicVoiceID);

    if (custom.bg_music)
        stop_sample(custom.bg_music);

    if (custom.bg_midi)
        stop_midi();
}
