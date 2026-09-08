/* replay.c -- the .itr replay file's checksum, teardown and writer.
 *
 * Original source: F:\projects\icytower\trunk\source\replay.c.
 *
 * PROMOTIONS.md batch 14.  Four of these five are on play()'s COASTLINE
 * -- the game-over / replay-menu half batch 13's closing table listed as
 * still ORIGINAL (`calc_replay_checksum`, `destroy_replay`,
 * `save_replay`); `hash` and `calc_replay_checksum_131` come with them
 * because calc_replay_checksum ends by INLINING the first and the .itr
 * format needs the second.
 *
 * notes/replay_format.md SS4 describes this file's subject from the
 * outside ("trace stopped here; INFERRED the rest follows struct order
 * ... a near-verbatim Treplay serialization").  save_replay() below is
 * that trace, finished: the on-disk order is NOT struct order -- see its
 * own header.
 *
 * Domains, per function:
 *   hash, calc_replay_checksum_131, calc_replay_checksum   pure; return
 *                     value only (no writes at all -- verified by
 *                     reading every store in the three ranges).
 *   destroy_replay    call trace of free() (batch 13's destroy_game_data
 *                     row established that "which pointer reached free"
 *                     is a call-trace fact, not an unrepresentable one).
 *   save_replay       ordered trace of every library call with its
 *                     arguments AND, for the pack_fwrite calls, the
 *                     BYTES written -- the call sequence IS the file.
 *
 * PROMOTIONS.md batch 15 adds the two readers at the bottom of the file:
 *
 *   create_replay     the malloc'd Treplay's DEFINED bytes (the domain
 *                     stops short of the fields this function provably
 *                     leaves uninitialised -- see its own header).
 *   load_replay       ordered trace of pack_fopen / 30 x pack_fread /
 *                     pack_fclose / create_replay / calc_replay_checksum
 *                     / log2file / destroy_replay, plus the whole
 *                     Treplay it fills in.
 *
 * Batch 15 also closes notes/replay_format.md SS4's two open literals:
 * the .itr magic at 0x4d7dd0 is "ITR140" (six bytes, no terminator) and
 * the 7-byte name tag at 0x4d7a7e is "Harold".  Both were read out of
 * the image with pefile.  With them, the write order save_replay()
 * documents parses all thirteen real .itr files under
 * assets/profiles/MissingNO/replays exactly, and calc_replay_checksum()
 * reproduces every one of their stored checksums -- the format is closed
 * against real data, not only against the emulated original.
 */
/* ------------------------------------------------------------------ */
/* MEMBER-ACCESS COLLISIONS -- the same class handle_player_input.c and */
/* draw_frame.c already document, and the reason                        */
/* carrier/win32_policy.json's `scan_exclude` temporarily listed this    */
/* file.  Three `Treplay`/`Trecord` MEMBER names are also top-level      */
/* game globals that carrier/gen/pf_bindings_src.h rewrites with a blunt */
/* textual #define, which turns `r->data` / `r->rejump` /                */
/* `r->data[i].cycle_count` into syntax errors in the carrier world:     */
/*                                                                      */
/*   data          `Treplay.data`, the Trecord array                     */
/*                 vs `DATAFILE *data`         @0x4dd23c                 */
/*   rejump        `Treplay.rejump`, the difficulty field                */
/*                 vs `int rejump`             @0x4fdcd8                 */
/*   cycle_count   `Trecord.cycle_count`, the RLE run length             */
/*                 vs `volatile int cycle_count` @0x506938               */
/*                                                                      */
/* Dropping all three bindings for the rest of THIS translation unit is  */
/* right rather than merely expedient: nothing in replay.c may touch the */
/* datafile (that belongs behind ASSETS.md's asset_*() seam), the live   */
/* difficulty global (the replay carries its OWN copy, which is the      */
/* whole point of the field) or the tick counter (play()'s pacing).      */
/* The real fix is the context-sensitive rewrite gen_bindings.py's own   */
/* MEMBER_ACCESS_COLLISIONS comment describes; reported, not attempted.  */
/* ------------------------------------------------------------------ */
#ifdef data
#undef data
#endif
#ifdef rejump
#undef rejump
#endif
#ifdef cycle_count
#undef cycle_count
#endif

#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------------
 * hash  (0x41b9c8, 71 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x41b9c8..0x41ba0e.  This is
 * Thomas Wang's well-known 32-bit integer mix, verbatim:
 *
 *   41b9d0/41b9d3  key = (key ^ 61) ^ (key >> 16)     (0x3d == 61)
 *   41b9d8         key = key + (key << 3)             (`lea (%eax,%eax,8)`
 *                                                      == key * 9)
 *   41b9dd         key = key ^ (key >> 4)
 *   41b9e2..41ba03 key = key * 0x27d4eb2d             -- expanded by GCC
 *                  into an 11-instruction lea/shl/add chain, no `imul`;
 *                  the chain multiplies by 5, 25, 125, 251, 1255,
 *                  321280, 321281, 1285125, 2570251, 167066315 and
 *                  finally 668265261 == 0x27d4eb2d.  Each step was
 *                  evaluated by hand rather than assumed, because a
 *                  single misread `lea` silently changes the constant.
 *   41ba08         key = key ^ (key >> 15)
 *
 * Every shift is `shr` (logical), which is what pins the parameter as
 * UNSIGNED -- and matches game_funcs.h's DWARF prototype
 * `unsigned int hash(unsigned int)`.
 *
 * DWARF gave this function no direct name until batch 4's
 * gen_interop.py `follow_origin()` fix (PROMOTIONS.md "Generator gap fix
 * (2026-09-07)"); it is one of the eleven names that pass recovered.
 */
unsigned int hash(unsigned int a)
{
    a = (a ^ 61u) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2du;
    a = a ^ (a >> 15);
    return a;
}

/* ---------------------------------------------------------------------
 * calc_replay_checksum_131  (0x41ba10, 177 bytes)
 * ---------------------------------------------------------------------
 * The LEGACY checksum -- the "131" is Icy Tower 1.3.1, i.e. the format
 * version this function validates.  Recovered from artifacts/disasm.txt
 * 0x41ba10..0x41bac0.  notes/replay_format.md SS4 already described this
 * one correctly from a partial trace; this is the same algorithm with
 * the multipliers read off the `lea` chains rather than paraphrased.
 *
 *   41ba19  random_seed * 17          (`shl $4` + add)
 *   41ba26  rejump      * 26          (x*13, then the *2 folded in at
 *                                      41ba32's `lea (%edx,%eax,2)`)
 *   41ba35  (score + 1) * 7
 *   41ba45  (floor + 1) * 13
 *   41ba52  (combo + 1) * 23
 *   41ba61  a rolling multiplier starting at 0x75 (117), stepping by
 *           0x75 each character, over the 32-byte `name` and `date`
 *           fields TOGETHER: sum += (name[i]+i) * (date[i]+i) * mult.
 *           `movsbl` on both, so plain `char` read as SIGNED -- which is
 *           what GCC's x86 default gives, and what this file relies on.
 *   41ba89  finally every one of the `size` records:
 *           sum += (key_flags*5 + cycle_count*3) * i.
 *           `movzbl` for key_flags (its `unsigned char` type) and a
 *           plain 32-bit load for cycle_count.
 *
 * The record loop's first iteration is PEELED by GCC (41ba96 loads
 * before the 41ba9d `jmp` into the middle of the loop); with i == 0 the
 * `imul %edx` makes that iteration contribute exactly 0, so the peel is
 * invisible and the plain `for` below is equivalent.
 *
 * The return type is `int` (game_funcs.h/DWARF) and every operation is a
 * 32-bit wrapping add/multiply, so signedness is unobservable here --
 * unlike calc_replay_checksum below, which really does need `unsigned`.
 */
int calc_replay_checksum_131(Treplay *r)
{
    int sum;
    int mult;
    int i;

    sum = r->random_seed * 17
        + r->rejump * 26
        + (r->score + 1) * 7
        + (r->floor + 1) * 13
        + (r->combo + 1) * 23;

    mult = 0x75;
    for (i = 0; i < 32; i++) {
        sum += ((int)r->name[i] + i) * ((int)r->date[i] + i) * mult;
        mult += 0x75;
    }

    for (i = 0; i < r->size; i++)
        sum += (r->data[i].key_flags * 5 + r->data[i].cycle_count * 3) * i;

    return sum;
}

/* ---------------------------------------------------------------------
 * calc_replay_checksum  (0x41bac4, 676 bytes)
 * ---------------------------------------------------------------------
 * The CURRENT checksum -- the one save_replay() stores and the one
 * `icytower15.exe -check file.itr` verifies.  notes/replay_format.md SS4
 * left this one explicitly untraced ("The larger calc_replay_checksum
 * (0x41bac4, 676B ...) wasn't step-traced"); this is that trace.
 * Recovered from artifacts/disasm.txt 0x41bac4..0x41bd67.
 *
 * Part 1 -- the header terms (41bad0..41bb9d).  GCC folded a large sum
 * of `field * <small constant>` products into one lea/shl/add tree with
 * no `imul` at all, so every multiplier below was reconstructed by
 * evaluating that tree step by step:
 *
 *     3702
 *   + (biggest_lost_combo + 1) *  34      (a *17 term, doubled at 41bb3d)
 *   +  no_combo_top_floor      * 254      (a *127 term, same doubling)
 *   +  floor_size              *  17
 *   +  floor_shrink            * 102
 *   +  start_speed             * 163
 *   +  speed_increase          *  23
 *   +  gravity                 *  88
 *   +  random_seed             * 329
 *   +  tc_posts                * 127
 *   +  rejump                  *  13
 *   + (score + 1)              *  17
 *   + (combo + 1)              *  73
 *   + (floor + 1)              * 113
 *
 * The bare 3702 (0xe76, appearing at 41bb05) is the folded remainder of
 * whatever "+1"s the original source spelled on the other fields; the
 * decomposition is NOT recoverable from the object code and is not
 * guessed at here.  The two explicit `lea 0x11(...)` folds (41bad8 for
 * biggest_lost_combo, 41bb74 for score) and the two `inc`s (41bb7d for
 * combo, 41bb89 for floor) ARE visible, so those four keep their +1.
 *
 * Part 2 -- ccc/jc (41bba0):  sum += ccc[k]*(39+3k) + jc[k]*(27+3k),
 * k = 0..4.  One shared induction variable (%eax starting at 0x27,
 * stepping 3, stopping at 0x36) drives both multipliers, the second as
 * `%eax - 12`.
 *
 * Part 3 -- the per-column statistics (41bbc4..41bc70).  This is the one
 * x87 section, and the one place the accumulator's SIGNEDNESS is
 * observable:
 *
 *   * `fildll` is fed a 64-bit slot whose HIGH dword is explicitly
 *     zeroed (41bbe0/41bc07/41bc37 `xor %edx,%edx`), so the accumulator
 *     is widened as an UNSIGNED 32-bit value.
 *   * `fistpll` + taking only the low dword (41bc04/41bc35/41bc68) is
 *     GCC's x86 idiom for `(unsigned int)<floating expression>`.
 *   Together those pin the accumulator's C type to `unsigned int` and
 *   the three statements to the exact `sum = (unsigned)(sum + ...)`
 *   shape below.  Written any other way -- `int`, or one fused
 *   expression -- the truncation points move and the result changes.
 *
 *   The three terms, with the index arithmetic read off the `idiv`s at
 *   41bbf0 (divisor 13), 41bc1b (17) and 41bc4e (23).  %ecx is already
 *   INCREMENTED when those run (41bbdf) but NOT when the array is
 *   addressed (41bbd8 `flds 0xd8(%ebx,%ecx,4)`), which is why the
 *   modulus operand is one larger than the index:
 *
 *       sum = (unsigned)(sum + tc_c_data[i] * ((i + 1) % 13));
 *       sum = (unsigned)(sum + tc_q_data[i] * ((i + 7) % 17));
 *       sum = (unsigned)(sum + tc_t_data[i] * ((i + 9) % 23));
 *
 *   tc_s_data and tc_f_data are NOT hashed -- 100 * 2 floats of the
 *   replay tail are outside this checksum entirely, even though
 *   save_replay() writes them.  (The two array bases the disassembly
 *   uses, 0x264 and 0x3f4, are 0x268 - 4 and 0x3f8 - 4: the -4 pairs
 *   with the pre-incremented %ecx.  Getting that backwards would silently
 *   shift both arrays by one element.)
 *
 * Part 4 -- name/date (41bc76): the same product form as the "131"
 * version but with the rolling multiplier starting at 0x11 (17) and
 * stepping by 17.
 *
 * Part 5 -- comment (41bc9d): sum += (comment[i]+i)^2 * mult, with mult
 * starting at -3 and stepping by 3, over all 42 bytes.  The square is a
 * literal `imul %eax,%eax` at 41bcaf, not two different loads.
 *
 * Part 6 -- the records (41bcc0):
 *     sum += key_flags*3*((i % 193) + 1) + cycle_count*7*((i % 167) + 1)
 * (0xc1 == 193 and 0xa7 == 167, both from the `idiv` divisors, and both
 * PRIME -- so the per-record weight cycles with a long period instead of
 * the "131" version's plain `* i`.)
 *
 * Part 7 -- the tail (41bd21..41bd5e) is hash() inlined, instruction for
 * instruction: the same 0x3d, the same *9, the same >>4, the same
 * 0x27d4eb2d lea-chain, the same >>15.  Calling the promoted hash()
 * above is what this file spells; GCC re-inlines it at -O2 and emits the
 * identical chain.
 */
int calc_replay_checksum(Treplay *r)
{
    unsigned int sum;
    int mult;
    int t;
    int i;

    sum = (unsigned int)(3702
        + (r->biggest_lost_combo + 1) * 34
        + r->no_combo_top_floor * 254
        + r->floor_size * 17
        + r->floor_shrink * 102
        + r->start_speed * 163
        + r->speed_increase * 23
        + r->gravity * 88
        + r->random_seed * 329
        + r->tc_posts * 127
        + r->rejump * 13
        + (r->score + 1) * 17
        + (r->combo + 1) * 73
        + (r->floor + 1) * 113);

    for (i = 0; i < 5; i++)
        sum += (unsigned int)(r->ccc[i] * (39 + 3 * i) + r->jc[i] * (27 + 3 * i));

    for (i = 0; i < 100; i++) {
        sum = (unsigned int)(sum + r->tc_c_data[i] * ((i + 1) % 13));
        sum = (unsigned int)(sum + r->tc_q_data[i] * ((i + 7) % 17));
        sum = (unsigned int)(sum + r->tc_t_data[i] * ((i + 9) % 23));
    }

    mult = 0x11;
    for (i = 0; i < 32; i++) {
        sum += (unsigned int)(((int)r->name[i] + i) * ((int)r->date[i] + i) * mult);
        mult += 0x11;
    }

    mult = -3;
    for (i = 0; i < 42; i++) {
        t = (int)r->comment[i] + i;
        sum += (unsigned int)(t * t * mult);
        mult += 3;
    }

    for (i = 0; i < r->size; i++)
        sum += (unsigned int)(r->data[i].key_flags * 3 * ((i % 193) + 1)
                            + r->data[i].cycle_count * 7 * ((i % 167) + 1));

    return (int)hash(sum);
}

/* ---------------------------------------------------------------------
 * destroy_replay  (0x41bd68, 54 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x41bd68..0x41bd9d.  Two guarded
 * frees, the second a TAIL CALL (`jmp 4bad08 <_free>` at 41bd90) exactly
 * like destroy_game_data's, so free()'s own `ret` returns to this
 * function's caller.
 *
 *   41bd72  r == NULL   -> nothing at all (not even the inner free)
 *   41bd7c  r->data == NULL -> skip the inner free, still free(r)
 *
 * Both guards are real branches in the object code, not free()'s own
 * NULL tolerance being relied on.
 */
void destroy_replay(Treplay *r)
{
    if (r == 0)
        return;
    if (r->data != 0)
        free(r->data);
    free(r);
}

/* ---------------------------------------------------------------------
 * create_replay  (0x41cce8, 254 bytes)
 * ---------------------------------------------------------------------
 * PROMOTIONS.md batch 15.  Recovered from artifacts/disasm.txt
 * 0x41cce8..0x41cde5.  load_replay() below is its only caller in this
 * file, and the reason it comes with it.
 *
 * Three literals, all READ OUT OF THE IMAGE with pefile rather than
 * inferred -- notes/replay_format.md SS4 explicitly left two of them
 * open ("Exact magic bytes not extracted ... would need a raw hex dump
 * at VA 0x4d7dd0"):
 *
 *   0x4d7dd0  REPLAY_HEADER   "ITR140"   -- SIX bytes, NO terminator.
 *                             The .itr magic is "ITR140", not "ITR15":
 *                             the file-format version is 1.4.0 even in a
 *                             1.5.1 binary, exactly as save_profile()'s
 *                             stats header still says "ICY TOWER 1.4".
 *                             Copied as `mov`+`mov %ax` (4+2), i.e. a
 *                             6-byte memcpy, not a strcpy.
 *   0x4d7a7e  "Harold"        -- the default player name (7 bytes with
 *                             its NUL, a `rep movsb`), the same default
 *                             character the game boots with.  This is
 *                             notes/replay_format.md's "7-byte tag from
 *                             VA 0x4d7a7e", now named.
 *   "no date"                 -- immediate stores (0x64206f6e /
 *                             0x00657461), overwritten by save_replay().
 *
 * TWO facts here are defects in the original, reproduced rather than
 * repaired, and both are visible only in the object code:
 *
 *  1. The 32-byte clearing loop is emitted TWICE (0x41cd48 and 0x41cd58)
 *     and BOTH write `0xc(%ebx,%eax,1)` -- that is `name`, at +0x0c,
 *     both times.  `date` (+0x2c) is never cleared; it only ever
 *     receives the 8 bytes of "no date".  So date[8..31] of a
 *     freshly-created replay is whatever malloc() handed back --
 *     and calc_replay_checksum() hashes all 32 bytes of `date`.
 *     save_replay() then rewrites date[0..30] from a 31-byte literal,
 *     leaving date[31] as the one byte of the checksum's input that
 *     nothing in the program ever defines.  (In all thirteen real .itr
 *     files under assets/profiles/MissingNO/replays that byte is 0, so
 *     the arena happens to be zeroed in practice; nothing guarantees it.)
 *     Written below as the two loops the object code contains, because
 *     collapsing them to one would hide the bug while being
 *     observationally identical.
 *  2. `malloc(0x20 + size * 8)` over-allocates the record array by 32
 *     bytes and the zero-init loop only covers `size` records -- the
 *     slack is never touched.  notes/replay_format.md SS4 already called
 *     this "INFERRED padding"; the call map confirms it, and it is kept.
 *
 * The two malloc failure paths differ: a failed HEADER malloc returns
 * NULL immediately (0x41cd0d), a failed RECORD malloc free()s the header
 * first (0x41cdd5) and then returns NULL.  Both are real branches.
 */
Treplay *create_replay(int size)
{
    Treplay *r;
    int i;

    r = (Treplay *)malloc(sizeof(Treplay));
    if (r == 0)
        return 0;

    memcpy(r->header, REPLAY_HEADER, 6);
    r->comment[0] = 0;
    r->size = size;
    r->score = 0;
    r->floor = 0;
    r->combo = 0;

    for (i = 0; i < 32; i++)
        r->name[i] = 0;
    /* the original's second clearing loop -- see note 1 above; it clears
     * `name` again instead of `date`. */
    for (i = 0; i < 32; i++)
        r->name[i] = 0;

    strcpy(r->name, "Harold");
    strcpy(r->date, "no date");

    r->data = (Trecord *)malloc(0x20 + r->size * 8);
    if (r->data == 0) {
        free(r);
        return 0;
    }

    for (i = 0; i < size; i++) {
        r->data[i].key_flags = 0;
        r->data[i].cycle_count = 0;
    }

    return r;
}

/* ---------------------------------------------------------------------
 * load_replay  (0x41cde8, 1136 bytes)
 * ---------------------------------------------------------------------
 * PROMOTIONS.md batch 15.  Recovered from artifacts/disasm.txt
 * 0x41cde8..0x41d257.  save_replay()'s exact inverse, and the reason
 * batch 14's closing paragraph named it as the obvious next one: the
 * on-disk order that function's trace established is read back here in
 * the SAME order, which is what makes the two mutually checkable.
 *
 * The file is opened TWICE, and that is not redundant:
 *
 *   pass 1 (0x41ce02..0x41ce53)  read the 6-byte magic and the 4-byte
 *       record count into STACK locals, close.  Compare the magic with
 *       REPLAY_HEADER ("ITR140") using a 6-byte `repz cmpsb` -- a
 *       memcmp, not a strcmp, so a file whose magic differs anywhere in
 *       those six bytes is rejected.  The count is needed before
 *       create_replay() can size the record array, and Allegro's
 *       PACKFILE has no seek, which is why the file is reopened rather
 *       than rewound.
 *   pass 2 (0x41ce88..0x41d20f)  create_replay(size), then read every
 *       field into it.
 *
 * The read order below is the object code's, field for field, and it is
 * the same NOT-struct-order save_replay() writes: `checksum` (+0x4c) is
 * read near the end, after `comment`; the five statistics columns are
 * read INTERLEAVED BY INDEX in one 100-iteration loop of five 4-byte
 * reads (0x41d108); each record is read REVERSED and SHORT --
 * `cycle_count` (4 bytes) first, `key_flags` (1 byte) second, five bytes
 * on disk against the struct's eight (0x41d1bb / 0x41d1e0).  The `ccc[]`
 * and `jc[]` arrays are ten separate 4-byte reads (0x41cf70, 0x41cfa0).
 *
 * Three things worth naming, all of which a reader would otherwise get
 * wrong:
 *
 *  1. NO pack_fread RETURN VALUE IS EVER CHECKED.  A truncated file is
 *     not detected here; it is detected by the checksum, which is the
 *     only integrity gate this function has.
 *  2. THE VERIFIED REPLAY IS RETURNED WITH `checksum` LEFT AT 0.
 *     0x41d214 saves the field, 0x41d217 zeroes it, and
 *     calc_replay_checksum() is called on the zeroed struct (the field
 *     has to be excluded from its own hash).  On the MATCH path the
 *     saved value is never written back -- the object code jumps
 *     straight to the return.  So every replay this function hands out
 *     has checksum == 0, and save_replay()'s later
 *     `r->checksum = calc_replay_checksum(r)` is what makes it right
 *     again.  Restoring it here would be "tidier" and wrong.
 *  3. The MISMATCH path logs and then destroy_replay()s, and the
 *     create_replay-returned-NULL path (0x41ce78) does NOT: it returns
 *     NULL without a log line.  The pack_fopen-failed-on-pass-2 path
 *     (0x41ce91) jumps INTO the mismatch tail at 0x41d249, so it
 *     destroys the replay but skips the log line -- three different
 *     failure shapes, all in the object code.
 *
 * The log format at 0x4d7a88 is
 *     "Checksum failed for %s: got %d, expected %d"
 * with `got` = the freshly computed value and `expected` = the value
 * that was in the file (0x41d22e pushes the FILE's value last).
 */
Treplay *load_replay(const char *filename)
{
    PACKFILE *f;
    Treplay *r;
    char magic[6];
    int size;
    int stored;
    int got;
    int i;

    f = pack_fopen(filename, "rb");
    if (f == 0)
        return 0;
    pack_fread(magic, 6, f);
    pack_fread(&size, 4, f);
    pack_fclose(f);

    if (memcmp(magic, REPLAY_HEADER, 6) != 0)
        return 0;

    r = create_replay(size);
    if (r == 0)
        return 0;

    f = pack_fopen(filename, "rb");
    if (f == 0) {
        destroy_replay(r);
        return 0;
    }

    pack_fread(r->header, 6, f);
    pack_fread(&r->size, 4, f);
    pack_fread(r->name, 32, f);
    pack_fread(r->date, 32, f);
    pack_fread(&r->score, 4, f);
    pack_fread(&r->floor, 4, f);
    pack_fread(&r->combo, 4, f);
    pack_fread(&r->no_combo_top_floor, 4, f);
    pack_fread(&r->biggest_lost_combo, 4, f);
    for (i = 0; i < 5; i++)
        pack_fread(&r->ccc[i], 4, f);
    for (i = 0; i < 5; i++)
        pack_fread(&r->jc[i], 4, f);
    pack_fread(&r->floor_shrink, 4, f);
    pack_fread(&r->floor_size, 4, f);
    pack_fread(&r->start_speed, 4, f);
    pack_fread(&r->speed_increase, 4, f);
    pack_fread(&r->gravity, 4, f);
    pack_fread(&r->rejump, 4, f);
    pack_fread(&r->random_seed, 4, f);
    pack_fread(r->comment, 42, f);
    pack_fread(&r->checksum, 4, f);
    pack_fread(&r->tc_posts, 4, f);
    for (i = 0; i < 100; i++) {
        pack_fread(&r->tc_c_data[i], 4, f);
        pack_fread(&r->tc_q_data[i], 4, f);
        pack_fread(&r->tc_t_data[i], 4, f);
        pack_fread(&r->tc_s_data[i], 4, f);
        pack_fread(&r->tc_f_data[i], 4, f);
    }
    for (i = 0; i < r->size; i++) {
        pack_fread(&r->data[i].cycle_count, 4, f);
        pack_fread(&r->data[i].key_flags, 1, f);
    }
    pack_fclose(f);

    stored = r->checksum;
    r->checksum = 0;
    got = calc_replay_checksum(r);
    if (got == stored)
        return r;

    /* ONE call, its result used twice -- 0x41d221 calls once and pushes
     * %eax into the log at 0x41d232.  Calling it twice would be
     * observationally different in the ordered-call-trace oracle and is
     * not what the object code does. */
    log2file("Checksum failed for %s: got %d, expected %d", filename, got,
             stored);
    destroy_replay(r);
    return 0;
}

/* month names, 0x4d7da0: twelve `char *` read out of the image with
 * pefile and dereferenced -- "Jan".."Dec" at 0x4d7d5b..0x4d7d87.  The
 * original builds this array ON THE STACK every call (41dd84's
 * `rep movsl` of 12 dwords from .rdata into -0x4c(%ebp)), which is what
 * a local `char *months[12] = {...}` initializer compiles to.  Kept
 * local for that reason rather than hoisted to file scope. */

/* ---------------------------------------------------------------------
 * save_replay  (0x41dd78, 1227 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x41dd78..0x41e242.  Every string
 * constant below was read out of the image with pefile, not guessed.
 *
 *   41ddb2  sprintf(name, "%s%s", path, file)      -- fmt 0x4d7c64.  No
 *           separator: the caller's `path` already ends in one.
 *   41ddc2  log2file("  saving replay: %s", name)  -- fmt 0x4d7c69
 *
 *   make_new_date != 0 (41ddc7):
 *     41ddd9  time(NULL) / localtime()
 *     41ddfe  a 31-byte `rep movsb` of the literal at 0x4d7c80 into
 *             r->date -- which is
 *                 "              ICYTOWERISGREAT "
 *             (14 spaces, then the tag, then one space; 30 chars + NUL).
 *             It looks dead, because the sprintf on the very next line
 *             writes the SAME buffer -- but it is not.  The sprintf's
 *             output is "%2d %3s %4d" == exactly 11 characters plus a
 *             NUL, so bytes 12..30 of the 32-byte `date` field KEEP
 *             "  ICYTOWERISGREAT ", and calc_replay_checksum() hashes
 *             all 32 bytes of `date`.  The tag is therefore a watermark
 *             folded into every replay's checksum: a .itr whose date
 *             field was rebuilt by anything that does not know about it
 *             fails the check.  This is the one place in this batch
 *             where an apparently-dead store had to be kept EXACTLY, and
 *             it is why the recovered form writes the literal rather
 *             than zero-filling.
 *     41de2a  sprintf(r->date, "%2d %3s %4d",
 *                     tm->tm_mday, months[tm->tm_mon], tm->tm_year+1900)
 *             -- fmt 0x4d7c9f; 0x76c == 1900.
 *   make_new_date == 0 (41e209):
 *     41e20c  load_replay(name) and, if it returned non-NULL, copy the
 *             EXISTING file's date across (41e229 strcpy).  The old
 *             replay object is deliberately NOT destroyed -- there is no
 *             destroy_replay call on this path in the object code.  That
 *             is a leak in the original, recovered rather than fixed.
 *
 *   41de35  r->size = size            (BEFORE the checksum, so `size` is
 *                                      covered by it)
 *   41de3b  r->checksum = calc_replay_checksum(r)
 *   41de51  pack_fopen(name, "wb")    -- fmt 0x4d7cab.  Allegro's own
 *                                      packfile, not fopen; 'b' is inert
 *                                      to pack_fopen, 'p' (compression)
 *                                      is absent, so the .itr body is
 *                                      stored uncompressed.
 *   41de5a  NULL -> return -1 with no log line at all.
 *
 * The write sequence (41de60..41e1ec) is the answer to
 * notes/replay_format.md SS4's open question, and it is NOT struct order:
 *
 *     header[6]  size  name[32]  date[32]
 *     score  floor  combo  no_combo_top_floor  biggest_lost_combo
 *     ccc[0..4]  jc[0..4]                       (ten SEPARATE 4-byte
 *                                                writes, not two 20-byte
 *                                                ones -- 41df4f/41df76)
 *     floor_shrink  floor_size  start_speed  speed_increase
 *     gravity  rejump  random_seed
 *     comment[42]
 *     checksum                                  <- OUT of struct order:
 *                                                  offset 0x4c, written
 *                                                  here, after comment
 *     tc_posts
 *     100 x { tc_c  tc_q  tc_t  tc_s  tc_f }    <- INTERLEAVED by index
 *                                                  (41e0f0), one 4-byte
 *                                                  write each; the struct
 *                                                  stores them as five
 *                                                  separate arrays
 *     size x { cycle_count(4)  key_flags(1) }   <- REVERSED within the
 *                                                  record (41e1b5 writes
 *                                                  data+i*8+4 first,
 *                                                  41e1da writes
 *                                                  data+i*8 second) and
 *                                                  5 bytes on disk, not
 *                                                  the struct's 8
 *
 * The last two rows are the ones an "it is just a struct dump" reading
 * would get wrong, and both are unambiguous in the object code.
 *
 *   41e1f7  pack_fclose(f); return 0.
 */
int save_replay(const char *path, const char *file, Treplay *r, int size,
                int make_new_date)
{
    char *months[12];
    char name[2048];
    PACKFILE *f;
    time_t now;
    struct tm *lt;
    int i;

    months[0] = "Jan"; months[1] = "Feb"; months[2] = "Mar";
    months[3] = "Apr"; months[4] = "May"; months[5] = "Jun";
    months[6] = "Jul"; months[7] = "Aug"; months[8] = "Sep";
    months[9] = "Oct"; months[10] = "Nov"; months[11] = "Dec";

    sprintf(name, "%s%s", path, file);
    log2file("  saving replay: %s", name);

    if (make_new_date) {
        now = time(0);
        lt = localtime(&now);
        strcpy(r->date, "              ICYTOWERISGREAT ");
        sprintf(r->date, "%2d %3s %4d", lt->tm_mday, months[lt->tm_mon],
                lt->tm_year + 1900);
    } else {
        Treplay *old = load_replay(name);
        if (old != 0)
            strcpy(r->date, old->date);
    }

    r->size = size;
    r->checksum = calc_replay_checksum(r);

    f = pack_fopen(name, "wb");
    if (f == 0)
        return -1;

    pack_fwrite(r->header, 6, f);
    pack_fwrite(&r->size, 4, f);
    pack_fwrite(r->name, 32, f);
    pack_fwrite(r->date, 32, f);
    pack_fwrite(&r->score, 4, f);
    pack_fwrite(&r->floor, 4, f);
    pack_fwrite(&r->combo, 4, f);
    pack_fwrite(&r->no_combo_top_floor, 4, f);
    pack_fwrite(&r->biggest_lost_combo, 4, f);
    for (i = 0; i < 5; i++)
        pack_fwrite(&r->ccc[i], 4, f);
    for (i = 0; i < 5; i++)
        pack_fwrite(&r->jc[i], 4, f);
    pack_fwrite(&r->floor_shrink, 4, f);
    pack_fwrite(&r->floor_size, 4, f);
    pack_fwrite(&r->start_speed, 4, f);
    pack_fwrite(&r->speed_increase, 4, f);
    pack_fwrite(&r->gravity, 4, f);
    pack_fwrite(&r->rejump, 4, f);
    pack_fwrite(&r->random_seed, 4, f);
    pack_fwrite(r->comment, 42, f);
    pack_fwrite(&r->checksum, 4, f);
    pack_fwrite(&r->tc_posts, 4, f);
    for (i = 0; i < 100; i++) {
        pack_fwrite(&r->tc_c_data[i], 4, f);
        pack_fwrite(&r->tc_q_data[i], 4, f);
        pack_fwrite(&r->tc_t_data[i], 4, f);
        pack_fwrite(&r->tc_s_data[i], 4, f);
        pack_fwrite(&r->tc_f_data[i], 4, f);
    }
    for (i = 0; i < r->size; i++) {
        pack_fwrite(&r->data[i].cycle_count, 4, f);
        pack_fwrite(&r->data[i].key_flags, 1, f);
    }

    pack_fclose(f);
    return 0;
}
