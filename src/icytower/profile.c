/* profile.c -- the player profile's rank lookup and its writer.
 *
 * Original source: F:\projects\icytower\trunk\source\profile.c.
 *
 * PROMOTIONS.md batch 14.  `get_rank_id` and `save_profile` are on batch
 * 13's list of the 19 play()-coastline functions still ORIGINAL;
 * `get_rank` comes with them because it is get_rank_id's one-line
 * sibling AND because save_profile INLINES it verbatim.
 *
 * Domains:
 *   get_rank_id / get_rank   pure; return value only.
 *   save_profile             ordered call trace with arguments (the file
 *                            it produces IS the sequence of fwrite /
 *                            fprintf / fputs calls), plus the memory
 *                            domain of the two profile fields it writes
 *                            before writing anything out (`checksum` and
 *                            `saveDate`).
 *
 * ---------------------------------------------------------------------
 * The four rank tables
 * ---------------------------------------------------------------------
 * `rankFloors` / `rankCombos` / `rankNMLs` / `rankCCCs` (0x4bdc20 /
 * 0x4bdc60 / 0x4bdce0 / 0x4bdca0, each `int[12]`) and `rankLables`
 * (0x4bdbe0, `char *[12]`) are all named in
 * carrier/gen/interop_index.json and already declared in game_state.h.
 * The four comparison sites in get_rank_id map onto them one for one,
 * and the profile fields they are compared against confirm the naming
 * independently:
 *
 *     Tprofile+0x4c  best_floor          vs rankFloors[i]
 *     Tprofile+0x50  best_combo          vs rankCombos[i]
 *     Tprofile+0x58  no_combo_top_floor  vs rankNMLs[i]
 *     Tprofile+0x88  ccc[0]              vs rankCCCs[i]
 *
 * (offsets computed from game_types.h's Tprofile, which is DWARF-derived;
 * 0x88 is `ccc[0]`, not the whole array -- only the first element is
 * tested.)  So a rank requires ALL FOUR thresholds at once, and the
 * search runs from the TOP down (i = 11) and returns the first level
 * whose four thresholds are all met.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* mkdir(): the original calls `_mkdir` (0x4b2de0) with ONE argument,
 * i.e. MinGW's <direct.h> single-argument form, not POSIX's two-argument
 * one.  Declared here rather than pulled in through a platform header so
 * that this file stays inside src/'s no-OS-headers convention. */
#ifndef mkdir
int mkdir(const char *path);
#endif

#define RANK_LEVELS 12

/* ---------------------------------------------------------------------
 * get_rank_id  (0x418a84, 76 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x418a84..0x418ace.
 *
 *   418a8e  the counter starts at 11 (`mov $0xb,%eax`) and counts DOWN;
 *           418ac4/418ac8 `dec %eax` / `cmp $0xffffffff,%eax` is the
 *           loop's only exit besides a hit.
 *   418a94..418ac2  four `cmp <table>(,%eax,4),<field>` guards.  Note
 *           the AT&T operand order: `cmp mem,reg` computes reg - mem, so
 *           `jl` leaves the loop when the FIELD is below the threshold
 *           and the fourth guard's `jge` is the success exit.  Reading
 *           these the other way round inverts the whole function.
 *   418aca  falling out of the loop returns 0 -- the SAME value a
 *           successful match at i == 0 returns.  The two are
 *           indistinguishable by design: rank 0 is the default rank, so
 *           "no rank qualified" and "the lowest rank qualified" are the
 *           same answer.
 *
 * All four comparisons are SIGNED (`jl`/`jge`), matching the `int` type
 * of both the tables and the profile fields.
 */
int get_rank_id(Tprofile *profile_arg)
{
    int i;

    for (i = RANK_LEVELS - 1; i >= 0; i--) {
        if (profile_arg->best_floor >= rankFloors[i]
         && profile_arg->best_combo >= rankCombos[i]
         && profile_arg->no_combo_top_floor >= rankNMLs[i]
         && profile_arg->ccc[0] >= rankCCCs[i])
            return i;
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * get_rank  (0x418ad0, 82 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x418ad0..0x418b21.  Byte for byte
 * get_rank_id's body -- GCC inlined get_rank_id into it rather than
 * calling it -- followed by one extra instruction, 418b18
 * `mov 0x4bdbe0(,%eax,4),%eax`, i.e. `return rankLables[id];`.
 *
 * The recovered form CALLS get_rank_id instead of repeating the loop;
 * -O2 re-inlines it and emits the same code.  Writing the loop twice
 * would match the object code more literally and the source less.
 */
char *get_rank(Tprofile *profile_arg)
{
    return rankLables[get_rank_id(profile_arg)];
}

/* ---------------------------------------------------------------------
 * save_profile  (0x41a3b8, 1073 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x41a3b8..0x41a7e3.  Every string
 * constant below was read out of the image with pefile.  The function
 * writes TWO files: the binary .itp profile and a human-readable
 * _stats.txt beside it.
 *
 * Directory preparation (41a3eb..41a45c) -- note both file_exists()
 * calls pass attrib 0x10 (FA_DIREC), i.e. they ask "is there a
 * DIRECTORY here", and mkdir() is called only when there is not:
 *
 *   41a3eb  get_profile_dir_for_profile(path, 1024, p->handle)
 *           -- p->handle is `Tprofile + 6` (41a3c7's `lea 0x6(%ebx)`),
 *              which is exactly where game_types.h puts it after the
 *              6-byte header.
 *   41a409  file_exists(path, 0x10, NULL) == 0 -> mkdir(path)   (41a795)
 *   41a423  strcat(path, "replays/")  (0x4d756b) -- GCC's inline
 *           expansion: `repnz scasb` for the strlen, `rep movsb` for the
 *           9-byte literal.
 *   41a455  file_exists(path, 0x10, NULL) == 0 -> mkdir(path)   (41a7a9)
 *
 * The binary profile (41a462..41a57c):
 *   41a47d  get_profile_dir_for_profile(path, 1024, p->handle) again --
 *           the buffer was overwritten by the "replays/" append, so it
 *           has to be rebuilt, and the original rebuilds it rather than
 *           keeping a copy.
 *   41a4a1  sprintf(path, "%s%s.itp", path, p->handle)  (0x4d7574)
 *           -- sprintf reading and writing the SAME buffer.  Undefined
 *           behaviour by the letter of C99 7.19.6.6, and it is what the
 *           original does; kept, because "fixing" it would change the
 *           trace on any implementation where it matters.
 *   41a4ad  time()/localtime(), then
 *   41a51c  sprintf(p->saveDate, "%d-%s%d-%s%d", ...)   (0x4d757f)
 *           with the two "%s" slots filled by "0" (0x4d757d) or ""
 *           (0x4d71f4) -- zero padding done with a STRING argument, not
 *           a "%02d" width.  The two comparisons are `<= 9` (41a4d3 for
 *           tm_mday, 41a4e1 for tm_mon + 1), and the argument order in
 *           the frame (41a4f2..41a511) is year, MONTH pad, month, DAY
 *           pad, day -- so the output is ISO-ish "2026-09-08".
 *           p->saveDate is Tprofile + 0x540 (41a4ec).
 *   41a524  p->checksum = generate_profile_checksum(p)   (field 0x28)
 *   41a53d  fopen(path, "wb")  (0x4d758c); NULL ->
 *           log2file("Failed to open \"%s\" for writing", path)
 *           (0x4d7590) and return -1.
 *   41a563  fwrite(p, 0x550, 1, f) -- ONE 1360-byte record, and 0x550 is
 *           exactly sizeof(Tprofile) as game_types.h reconstructs it,
 *           which is an independent confirmation of that struct.
 *   41a574  save_control(get_controls(), f) -- the key bindings ride
 *           along in the same file, after the struct.
 *   41a57c  fclose(f)
 *
 * The stats text (41a581..41a762):
 *   41a59c  get_profile_dir_for_profile(path, 1024, p->handle) a THIRD
 *           time, then
 *   41a5c0  sprintf(path, "%s%s_stats.txt", path, p->handle) (0x4d75b0)
 *   41a5d6  fopen(path, "wt"); NULL ->
 *           log2file("failed to open profile stats \"%s\" for writing",
 *                    path)  (0x4d75c4)  and return -1 -- note this
 *           second failure path leaves the .itp already written.
 *   41a600  fwrite("ICY TOWER 1.4 PROFILE\n", 1, 0x16, f)   (0x4d75f2)
 *           -- the literal really says 1.4 in a 1.5.1 binary.
 *   41a620  fwrite("****...***\n", 1, 0x2f, f)               (0x4d760c)
 *           Both are fwrite with an explicit length, not fputs.
 *   41a63a  fprintf(f, "Profile name:          %s\n", p->handle)
 *   41a64e  fprintf(f, "Last updated:          %s\n", p->saveDate)
 *   41a6aa  fprintf(f, "Rank:                  %s\n", get_rank(p))
 *           -- get_rank INLINED (41a65c..41a694 is its loop plus the
 *           rankLables load, identical to 0x418ad0's).
 *   41a6ba  the four page builders, in this order: general(p, "       ")
 *           -- seven spaces, 0x4d768d -- basic(p), advanced(p),
 *           extra(p).  All four are called BEFORE any of them is
 *           written, so all four buffers are live at once.
 *   41a6f4  fputs each, in the same order, then fputc('\n', f) (0xa),
 *           fclose, and free() of all four in the same order again.
 *   41a767  return 0.
 */
int save_profile(Tprofile *p)
{
    char path[1024];
    FILE *f;
    time_t now;
    struct tm *lt;
    char *general, *basic, *advanced, *extra;

    get_profile_dir_for_profile(path, 1024, p->handle);
    if (!file_exists(path, 0x10, 0))
        mkdir(path);
    strcat(path, "replays/");
    if (!file_exists(path, 0x10, 0))
        mkdir(path);

    get_profile_dir_for_profile(path, 1024, p->handle);
    sprintf(path, "%s%s.itp", path, p->handle);

    now = time(0);
    lt = localtime(&now);
    sprintf(p->saveDate, "%d-%s%d-%s%d",
            lt->tm_year + 1900,
            (lt->tm_mon + 1 <= 9) ? "0" : "", lt->tm_mon + 1,
            (lt->tm_mday <= 9) ? "0" : "", lt->tm_mday);

    p->checksum = generate_profile_checksum(p);

    f = fopen(path, "wb");
    if (f == 0) {
        log2file("Failed to open \"%s\" for writing", path);
        return -1;
    }
    fwrite(p, 0x550, 1, f);
    /* The generated game_funcs.h types save_control's second parameter as
     * `it_orig_FILE *` -- the MSVC-shaped FILE the ORIGINAL binary's CRT
     * used, which allegro_types.h reconstructs from DWARF.  In a build
     * whose own <stdio.h> is not that CRT's, the two structs are distinct
     * types for the same pointer value, so the cast is a type-system
     * formality and not a representation change. */
    save_control(get_controls(), (it_orig_FILE *)f);
    fclose(f);

    get_profile_dir_for_profile(path, 1024, p->handle);
    sprintf(path, "%s%s_stats.txt", path, p->handle);

    f = fopen(path, "wt");
    if (f == 0) {
        log2file("failed to open profile stats \"%s\" for writing", path);
        return -1;
    }

    fwrite("ICY TOWER 1.4 PROFILE\n", 1, 0x16, f);
    fwrite("**********************************************\n", 1, 0x2f, f);
    fprintf(f, "Profile name:          %s\n", p->handle);
    fprintf(f, "Last updated:          %s\n", p->saveDate);
    fprintf(f, "Rank:                  %s\n", get_rank(p));

    general  = profile_data_page_general(p, "       ");
    basic    = profile_data_page_basic(p);
    advanced = profile_data_page_advanced(p);
    extra    = profile_data_page_extra(p);

    fputs(general, f);
    fputs(basic, f);
    fputs(advanced, f);
    fputs(extra, f);
    fputc('\n', f);
    fclose(f);

    free(general);
    free(basic);
    free(advanced);
    free(extra);
    return 0;
}
