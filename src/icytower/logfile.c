/* logfile.c -- log2file(), the game's one logging entry point.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c (the same CU
 * as update_frame.c / sound.c / main_state.c / play.c).
 *
 * log2file is called 15 times from play() alone (batch 12's call-site
 * census row `log2file  original 15  source 15`) and once from
 * take_screenshot, so it sits on the gameplay tick path: batch 11's
 * "What is still ORIGINAL in the tick body" table named it, with this
 * blocker --
 *
 *     "log2file writes through a FILE * behind a mutex
 *      (sLogMutex__log2file, already on carrier/win32_policy.json's
 *      digest-domain exclude list) -- its only observable effect is
 *      outside any domain the offline harness can express."
 *
 * That was wrong in one important particular, and reading the
 * disassembly is what shows it: log2file does NOT only write to the file.
 * At 0x40daca it also runs the SAME format string and the SAME va_list
 * through vsprintf() into the game global `last_log` (0x4f89e8,
 * `char [256]`, named in carrier/gen/interop_index.json) -- a plain
 * game-owned byte array, i.e. an ordinary MEMORY domain, holding the
 * fully FORMATTED text of the most recent log line.  So the formatted
 * output is diffable after all, and this batch diffs it.
 *
 * ---------------------------------------------------------------------
 * log2file  (0x40da58, 189 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x40da58..0x40db14.  Prototype
 * from the generated game_funcs.h: `void log2file(const char *, ...)`.
 *
 *   40da64  itrcheck != 0 -> return WITHOUT taking the mutex (the .itr
 *           integrity-check run logs nothing at all; the same global
 *           gates play_sound() in sound.c)
 *   40da78  pthread_mutex_lock(&sLogMutex__log2file)   -- an INDIRECT
 *           call through the IAT slot at 0x514a5c, which pefile resolves
 *           to pthreadGC2.dll!pthread_mutex_lock (0x514a60 is the
 *           matching pthread_mutex_unlock).  The mutex object itself is
 *           `sLogMutex__log2file` at 0x4bdb44, a function-static that
 *           DWARF names and game_state.h already declares.
 *   40da85  logfilename__log2file[0] == 0
 *              -> get_logfile_path(logfilename__log2file, 1024)
 *           (0x40dafc, then jumps BACK into the fopen block -- so the
 *           path is resolved lazily, exactly once per process, and the
 *           second and later calls skip straight to fopen)
 *   40da9d  f = fopen(logfilename__log2file, "at")
 *           -- the mode string at 0x4d4f13 is "at", not "a": the MSVCRT
 *           't' (text) flag is explicit, so \n is translated to \r\n.
 *           Read out of .rdata with pefile, not assumed.
 *   40daa6  f == NULL -> skip the whole write, but still UNLOCK
 *   40dab6  vfprintf(f, fmt, ap)
 *   40daca  vsprintf(last_log, fmt, ap)     <-- the diffable domain
 *   40dada  fputc('\n', f)
 *   40dae2  fclose(f)
 *   40daee  pthread_mutex_unlock(&sLogMutex__log2file)
 *
 * The va_list is loaded ONCE (`lea 0xc(%ebp),%edi` at 0x40daa8) and the
 * same %edi is passed to BOTH vfprintf and vsprintf.  That is not a bug
 * and it is not a va_copy: on x86 cdecl a `va_list` is a plain pointer
 * into the caller's own argument block, passed BY VALUE, so a callee's
 * traversal cannot advance the caller's copy.  Reusing `ap` after
 * vfprintf is therefore well-defined for this ABI and this toolchain
 * (TDM-GCC 4.4.1, the toolchain of record), and it is what makes
 * `last_log` carry the same text the file received.  Writing it any
 * other way -- a second va_start, or a va_copy -- would compile to a
 * second `lea` and change nothing observable, but this file keeps the
 * original's shape.
 *
 * `printf`-family calls are the CRT, not Allegro: the harness traces
 * them at their PE thunk VAs (fopen 0x4bad28, vfprintf 0x4badb0,
 * vsprintf 0x4badb8, fputc 0x4badc0, fclose 0x4bad30) the same way
 * destroy_game_data.c's oracle traces `free` at 0x4bad08.
 */
#include <stdarg.h>
#include <stdio.h>

#include "game_types.h"         /* pthread_mutex_t */
#include "game_state.h"         /* itrcheck, last_log, logfilename__log2file,
                                   sLogMutex__log2file */
#include "game_funcs.h"         /* get_logfile_path */

/* pthreadGC2's two entry points.  Declared here rather than pulled from
 * <pthread.h>: this directory is address-free but also toolchain-free
 * (src/README.md) -- the carrier binds these by NAME through the IAT the
 * original already imports, and the harness redirects them per name the
 * same way.  Guarded so a real <pthread.h>, or a harness redirect, wins. */
#ifndef pthread_mutex_lock
int __cdecl pthread_mutex_lock(pthread_mutex_t *m);
#endif
#ifndef pthread_mutex_unlock
int __cdecl pthread_mutex_unlock(pthread_mutex_t *m);
#endif

void log2file(const char *fmt, ...)
{
    va_list ap;
    FILE *f;

    if (itrcheck)
        return;

    pthread_mutex_lock(&sLogMutex__log2file);

    if (!logfilename__log2file[0])
        get_logfile_path(logfilename__log2file, 1024);

    f = fopen(logfilename__log2file, "at");
    if (f) {
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        vsprintf(last_log, fmt, ap);
        va_end(ap);
        fputc('\n', f);
        fclose(f);
    }

    pthread_mutex_unlock(&sLogMutex__log2file);
}
