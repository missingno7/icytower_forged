/* handle_player_input.c -- handle_player_input (0x40b3e4, 728 bytes,
 * F:\projects\icytower\trunk\source\main.c decl_line 2389).
 * PROMOTIONS.md batch 11.
 *
 * THE INPUT SEAM.  This is the single place where the outside world enters
 * the simulation: play() calls it exactly once per consumed 20 ms tick
 * (0x411f2a), and everything downstream -- jump_player, update_player, the
 * five handle_player_collision_* variants, the whole tower -- is a pure
 * function of what this one call decides.  notes/replay_format.md SS1 is
 * about this function; the notes below CORRECT that document in two places,
 * both flagged inline.
 *
 * SHAPE (0x40b3e4-0x40b6bb, hand-traced instruction by instruction):
 *
 *   if (!ctrl) return;                                     (0x40b3f0)
 *   if (!recording)  <decode one tick out of demo->data>   (0x40b406)
 *   else             <encode one tick into demo->data>     (0x40b59c)
 *   <steer sx left/right/decay, then the jump>             (0x40b423 on)
 *
 * The two halves BOTH end at the same 0x40b427 tail, and that tail is the
 * actual physics input: whichever half ran, `ctrl->flags` now holds this
 * tick's held-key mask, and the tail reads it back through the ordinary
 * is_left()/is_right()/is_fire() predicates -- which is exactly why replay
 * playback is bit-exact by construction (notes/replay_format.md SS3): live
 * play and playback run the SAME steering code over the same byte, they
 * only disagree about where the byte came from.
 *
 * ---------------------------------------------------------------- decode
 * PLAYBACK (`recording == 0`), 0x40b406-0x40b4d9 + 0x40b608.  The record
 * stream is run-length encoded: `Trecord {unsigned char key_flags; int
 * cycle_count;}` means "hold key_flags for cycle_count more ticks".  The
 * cursor `rec_pos` (VA 0x4fec58) points ONE PAST the record being consumed
 * -- every read is `demo->data[rec_pos - 1]`, and the local the DWARF names
 * `rp` (decl_line 2394, the lexical block at 0x40b414..) is that biased
 * index.  Three cases, in the original's own order:
 *
 *   rp < 0            rec_pos++ and DO NOT touch ctrl->flags.  This is the
 *                     first tick of a playback: rec_pos starts at 0, so the
 *                     first call only advances the cursor, and this tick's
 *                     flags are whatever the caller left in the struct.
 *   rp >= demo->size  ctrl->flags = 0 and DO NOT advance.  Past the end of
 *                     a finished replay the player simply stops steering --
 *                     it is not an error path and nothing is clamped.
 *   otherwise         ctrl->flags = r->key_flags; then if r->cycle_count is
 *                     strictly positive, decrement it IN PLACE (the replay
 *                     buffer is consumed destructively -- 0x40b609 writes
 *                     back into demo->data), else advance rec_pos to the
 *                     next record.  So a record with cycle_count == 0 is
 *                     played for exactly one tick.
 *
 * ---------------------------------------------------------------- encode
 * RECORDING (`recording != 0`, VA 0x4f8e28), 0x40b59c-0x40b661.
 *
 *   poll_control(ctrl, 0)                                  (0x40b5a7)
 *   if (ply[player_id]->dead                               (0x40b5b8, +0x4c)
 *       || (demo->data[rec_pos].key_flags & 0x80))         (0x40b5d5/0x40b5dc)
 *       <write the 0x80 sentinel pair>                     (0x40b645)
 *   else if (demo->data[rec_pos].key_flags == (ctrl->flags & 0x93))
 *       demo->data[rec_pos].cycle_count++;                 (0x40b6b4)
 *   else {
 *       rec_pos++;
 *       demo->data[rec_pos].key_flags   = ctrl->flags & 0x93;
 *       demo->data[rec_pos].cycle_count = 0;               (0x40b5ec-0x40b5fa)
 *   }
 *
 * Here the cursor is UNBIASED -- the encoder extends `data[rec_pos]` and
 * writes a new record at `data[rec_pos + 1]` via the pre-increment, which
 * is the same physical record the decoder will later read as
 * `data[rp] = data[rec_pos - 1]`.  The two halves use the same cursor
 * global with a one-record offset between them; that is not a bug, it is
 * why a recording can be replayed by the same code without rewinding.
 *
 * THE 0x93 MASK (0x40b5e1) keeps bits 0 (left), 1 (right), 4 (fire) and 7.
 * Bits 2/3 (up/down, unused by this game) and 5/6 (enter/pause, menu-only)
 * are dropped, so they can never enter a saved replay -- confirming
 * notes/replay_format.md SS1.  Bit 7 is kept only because it IS the
 * sentinel bit below; poll_control() can never set it (its widest possible
 * output is 0x7f), so in practice the mask is "left | right | fire".
 *
 * THE 0x80 SENTINEL -- notes/replay_format.md's open question, answered.
 * That document records the location (0x40b634/0x40b645) but guesses the
 * trigger is "a difficulty-table field is nonzero" and the meaning is
 * "practice-mode/segment-boundary marker".  Both were wrong.  The trigger
 * is `ply[player_id]->dead` (Tplayer+0x4c -- the very field play() polls at
 * 0x41246a/0x41249f to decide the game is over), OR a record whose
 * key_flags ALREADY has bit 7 set, i.e. a re-entry after the sentinel was
 * written.  The effect is a two-record trailer written at data[rec_pos + 1]
 * and data[rec_pos + 2] -- {0x80, 0} then {0x00, 0} -- and, notably,
 * rec_pos is NOT advanced, so every subsequent dead tick rewrites the same
 * two records rather than growing the stream.  It is an END-OF-INPUT
 * TERMINATOR, idempotent by design: the recording stops growing the instant
 * the player dies, and the terminator sits one past the last real record
 * exactly where a decoder walking `rp = rec_pos - 1` would meet it.
 *
 * ------------------------------------------------------------- the steer
 * 0x40b427-0x40b4b7 + 0x40b53c/0x40b614/0x40b668.  Three-way on the decoded
 * flags, then the jump.  Constants read from .rdata: 0x4d6d38 = 0.7,
 * 0x4d6d40 = 0.3, 0x4d6d48 = 0.9 (all `double`).
 *
 *   left  held : if (sx > 0) sx *= 0.7;  sx -= 0.3;     (0x40b437)
 *   right held : if (sx < 0) sx *= 0.7;  sx += 0.3;     (0x40b4ff)
 *   neither    : sx *= 0.9;                             (0x40b614)
 *
 * The `*= 0.7` on a reversal is a turn-around brake -- pressing the other
 * way cuts 30% of the existing speed on the same tick the 0.3 impulse is
 * applied.  x87 note (win32_pilot.md SS6a): the original keeps sx on the
 * register stack across the store (`fstl`, non-popping) and subtracts from
 * the 80-bit register, not from the freshly-rounded memory double.  Written
 * below through a local `double sx` so the same residency is expressed in
 * plain C -- writing it as two statements on `p->sx` would ask the compiler
 * to reload, and only GCC's excess-precision default would save it.  Left
 * and right test STRICTLY (`fucom`/`test $0x45,%ah`/`jne` and its `fucomp`
 * mirror), so sx == 0.0 takes neither brake; NaN takes neither either,
 * which plain C `>`/`<` reproduces (the same unordered-compare rule
 * notes/living_record.md divergence 006 established for line_intersect).
 *
 * THE JUMP, gated on the global `rejump` (VA 0x4fdcd8):
 *
 *   rejump != 0 (0x40b53c): fire held -> jump every tick it succeeds.
 *   rejump == 0 (0x40b475): fire held AND ply[player_id]->jump_key == 0 ->
 *       jump, and on success latch jump_key = -1 so the SAME hold cannot
 *       jump twice; then, unconditionally, releasing fire clears jump_key
 *       back to 0 (0x40b4a4).  `jump_key` (Tplayer+0x38) is therefore an
 *       edge detector, not a key code.
 *
 * On a successful jump both arms call play_jump_sound(ply[player_id]) and
 * bump profile->total_jumps (Tprofile+0xd8) if a profile is loaded.  Only
 * the rejump == 0 arm writes jump_key.
 *
 * PARAMETER NAME.  DWARF names this parameter `ctrl` (decl_line 2389), and
 * every comment above uses that name -- but `ctrl` is ALSO a game global
 * (Tcontrol ctrl @0x5000c8, the player-1 control state), which
 * carrier/gen/pf_bindings_src.h turns into a blunt textual `#define ctrl
 * (*(Tcontrol *)0x5000c8)` in the carrier world.  A parameter spelled
 * `ctrl` therefore macro-expands into a syntax error there -- the
 * MEMBER_ACCESS_COLLISIONS class PROMOTIONS.md batch 10 hit for
 * `data`/`cycle_count`, this time on a parameter.  The project already has
 * the right mechanism for exactly this (carrier/win32_policy.json's
 * `param_renames`, which was already renaming `key` -> `key_arg` for
 * check_control_key), so the fix is one policy entry, `ctrl` -> `ctrl_arg`,
 * plus this definition matching the regenerated game_funcs.h prototype.
 * Nothing else about the recovery changes; `ctrl_arg` is the ONLY
 * identifier here that is not the original's own.
 *
 * Verified offline (PROMOTIONS.md batch 11), GCC x87 -mfpmath=387
 * -mno-sse2 -O2: memory domain = Tcontrol + the whole Tplayer + rec_pos +
 * the Trecord window around the cursor + Treplay.size/data +
 * profile->total_jumps, plus the play_sound() and poll_joystick()
 * call-trace slots.  is_left/is_right/is_fire/jump_player/play_jump_sound/
 * poll_control are all already promoted, so the compiled side calls the
 * clean forms while the ORIGINAL side runs their original bytes -- a real
 * end-to-end check of the whole seam, not a stub.
 */
/* ------------------------------------------------------------------ */
/* MEMBER_ACCESS_COLLISIONS -- the same pair PROMOTIONS.md batch 10      */
/* worked around inside draw_frame.c, hit again here for the same two    */
/* names and for the same structural reason:                            */
/*                                                                      */
/*   demo->data        `Treplay.data`, the Trecord array -- collides     */
/*                     with the top-level global `DATAFILE *data`        */
/*                     @0x4dd23c;                                        */
/*   r->cycle_count    `Trecord.cycle_count`, the RLE run length --      */
/*                     collides with `volatile int cycle_count`          */
/*                     @0x506938, the 20 ms tick counter timer.c's       */
/*                     cycle_counter() increments.                       */
/*                                                                      */
/* carrier/gen/pf_bindings_src.h's blunt textual `#define`s rewrite both  */
/* MEMBER accesses into syntax errors in the carrier world.  Dropping    */
/* the two bindings for the rest of THIS translation unit is exactly     */
/* right rather than merely expedient: this file must never touch either */
/* global -- the datafile belongs behind ASSETS.md's asset_*() seam, and */
/* the tick counter is play()'s pacing, not the input seam's.  The real  */
/* fix is still the context-sensitive rewrite gen_bindings.py's own      */
/* MEMBER_ACCESS_COLLISIONS comment describes; reported, not attempted.  */
/* ------------------------------------------------------------------ */
#ifdef data
#undef data
#endif
#ifdef cycle_count
#undef cycle_count
#endif

#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

/* The recorded-key mask at 0x40b5e1 -- left | right | fire | sentinel. */
#define REC_KEY_MASK 0x93

/* The end-of-input terminator byte written into key_flags (0x40b649). */
#define REC_SENTINEL 0x80

void handle_player_input(Tcontrol *ctrl_arg)
{
    if (!ctrl_arg)
        return;

    if (!recording) {
        /* --- decode one tick out of the RLE record stream --- */
        int rp = rec_pos - 1;

        if (rp < 0) {
            rec_pos++;
        } else if (rp >= demo->size) {
            ctrl_arg->flags = 0;
        } else {
            Trecord *r = &demo->data[rp];

            ctrl_arg->flags = r->key_flags;
            if (r->cycle_count > 0)
                r->cycle_count--;
            else
                rec_pos++;
        }
    } else {
        /* --- encode this tick into the RLE record stream --- */
        Trecord *rec = demo->data;

        poll_control(ctrl_arg, 0);

        if (ply[player_id]->dead || (rec[rec_pos].key_flags & REC_SENTINEL)) {
            /* end-of-input terminator, written one and two records past the
             * cursor and NOT advancing it (see file header). */
            rec[rec_pos + 1].key_flags = REC_SENTINEL;
            rec[rec_pos + 1].cycle_count = 0;
            rec[rec_pos + 2].key_flags = 0;
            rec[rec_pos + 2].cycle_count = 0;
        } else {
            unsigned char flags = (unsigned char)(ctrl_arg->flags & REC_KEY_MASK);

            if (rec[rec_pos].key_flags == flags) {
                rec[rec_pos].cycle_count++;
            } else {
                rec_pos++;
                rec[rec_pos].key_flags = flags;
                rec[rec_pos].cycle_count = 0;
            }
        }
    }

    /* --- steer: the same code for live play and for playback --- */
    if (is_left(ctrl_arg)) {
        Tplayer *p = ply[player_id];
        double sx = p->sx;

        if (sx > 0.0) {
            sx = sx * 0.7;
            p->sx = sx;
        }
        sx = sx - 0.3;
        p->sx = sx;
    } else if (is_right(ctrl_arg)) {
        Tplayer *p = ply[player_id];
        double sx = p->sx;

        if (sx < 0.0) {
            sx = sx * 0.7;
            p->sx = sx;
        }
        sx = sx + 0.3;
        p->sx = sx;
    } else {
        Tplayer *p = ply[player_id];

        p->sx = p->sx * 0.9;
    }

    /* --- jump --- */
    if (rejump) {
        if (is_fire(ctrl_arg)) {
            if (jump_player(ply[player_id], 0)) {
                play_jump_sound(ply[player_id]);
                if (profile)
                    profile->total_jumps++;
            }
        }
    } else {
        if (is_fire(ctrl_arg) && ply[player_id]->jump_key == 0) {
            if (jump_player(ply[player_id], 0)) {
                ply[player_id]->jump_key = -1;
                play_jump_sound(ply[player_id]);
                if (profile)
                    profile->total_jumps++;
            }
        }
        if (!is_fire(ctrl_arg))
            ply[player_id]->jump_key = 0;
    }
}
