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
 */
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
