/* pf_harness_batch13.h -- force-included ahead of the whole
 * batch13b_check.exe build (see build_batch13.sh), the same way
 * pf_harness_calltrace.h is force-included ahead of the SPECS-table
 * harnesses.
 *
 * PROMOTIONS.md batch 13.  Purpose: redirect the CRT and pthreads calls
 * that src/icytower/logfile.c and src/icytower/screenshot.c make, so the
 * compiled candidate TRACES them instead of touching the real filesystem
 * or a real mutex -- exactly mirroring what the Python side's unicorn
 * hooks do to the ORIGINAL bytes at the matching PE thunk VAs
 * (fopen 0x4bad28, vfprintf 0x4badb0, vsprintf 0x4badb8, fputc 0x4badc0,
 * fclose 0x4bad30, sprintf 0x4bad60, pthread_mutex_lock/unlock through
 * the IAT slots 0x514a5c/0x514a60).
 *
 * NOTE the one deliberate asymmetry: h_vsprintf() traces AND performs the
 * real formatting into the caller's buffer.  `last_log` (0x4f89e8) is the
 * memory domain that carries log2file's FORMATTED text, so the candidate
 * has to actually format; the Python side reproduces the same bytes from
 * the format string and the va_list it reads out of guest memory.  Every
 * other redirect here is inert.
 *
 * A translation unit that needs the REAL function (the stubs themselves,
 * and the driver's own output) undefines the macro it needs at the top of
 * the file -- there is no way to un-force-include a header.
 */
#ifndef PF_HARNESS_BATCH13_H
#define PF_HARNESS_BATCH13_H

#include <stdarg.h>
#include <stdio.h>

typedef struct { int dummy; } h_mutex_t;

FILE *h_fopen(const char *path, const char *mode);
int   h_vfprintf(FILE *f, const char *fmt, va_list ap);
int   h_vsprintf(char *dst, const char *fmt, va_list ap);
int   h_fputc(int c, FILE *f);
int   h_fclose(FILE *f);
int   h_sprintf(char *dst, const char *fmt, ...);
int   h_mutex_lock(void *m);
int   h_mutex_unlock(void *m);

/* <stdio.h> is pulled in ABOVE these macros on purpose: its own
 * declarations (and any __mingw_ovr inline definitions) must be seen
 * with the real names before the names are hijacked.  Each name is
 * #undef'd first because several of them are macros in some CRT
 * headers. */
#undef fopen
#undef vfprintf
#undef vsprintf
#undef fputc
#undef fclose
#undef sprintf
#define fopen               h_fopen
#define vfprintf            h_vfprintf
#define vsprintf            h_vsprintf
#define fputc               h_fputc
#define fclose              h_fclose
#define sprintf             h_sprintf
#define pthread_mutex_lock  h_mutex_lock
#define pthread_mutex_unlock h_mutex_unlock

#endif /* PF_HARNESS_BATCH13_H */
