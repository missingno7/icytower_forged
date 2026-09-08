/* pf_harness_batch14.h -- force-included ahead of the whole
 * batch14b_check.exe build (see build_batch14.sh), the same way
 * pf_harness_batch13.h is force-included ahead of batch13b_check.exe.
 *
 * PROMOTIONS.md batch 14.  Purpose: redirect the CRT calls that
 * src/icytower/replay.c, profile.c and config.c make, so the compiled
 * candidate TRACES them instead of touching the real filesystem or the
 * real clock -- exactly mirroring what the Python side's unicorn hooks
 * do to the ORIGINAL bytes at the matching PE thunk VAs (sprintf
 * 0x4bad60, fopen 0x4bad28, fwrite 0x4bad00, fprintf 0x4bad70, fputs
 * 0x4badf0, fputc 0x4badc0, fclose 0x4bad30, free 0x4bad08, time
 * 0x4bad78, localtime 0x4badf8, mkdir 0x4b2de0).
 *
 * THREE deliberate asymmetries, each for a reason:
 *
 *  1. h_sprintf() traces AND performs the real formatting -- its output
 *     is the filename every later call in the trace is compared on.
 *  2. `strcpy` is NOT redirected.  save_replay() contains two of them:
 *     one with a string LITERAL, which both GCC and the original inline
 *     (`rep movsb`, no call at all), and one with a runtime source
 *     (0x41e229), which both sides really call.  Tracing the second
 *     would be fine; tracing the FIRST is impossible on the original
 *     side and would appear only on the candidate's, so neither is
 *     traced.  The copies are checked through the memory domain
 *     (`Treplay.date`) instead, which sees both.
 *  3. h_free() traces WITHOUT freeing.  Three of the four pointers
 *     save_profile() frees come from stubbed page builders and point at
 *     static storage, so a real free() would abort the run.
 *
 * A translation unit that needs the REAL function (the stubs
 * themselves, and the driver's own output) undefines the macro it needs
 * at the top of the file -- there is no way to un-force-include a
 * header.
 */
#ifndef PF_HARNESS_BATCH14_H
#define PF_HARNESS_BATCH14_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int   h_sprintf(char *dst, const char *fmt, ...);
FILE *h_fopen(const char *path, const char *mode);
size_t h_fwrite(const void *buf, size_t sz, size_t n, FILE *f);
int   h_fprintf(FILE *f, const char *fmt, ...);
int   h_fputs(const char *s, FILE *f);
int   h_fputc(int c, FILE *f);
int   h_fclose(FILE *f);
void  h_free(void *p);
time_t h_time(time_t *t);
struct tm *h_localtime(const time_t *t);
int   h_mkdir(const char *path);

/* <stdio.h>/<stdlib.h>/<time.h> are pulled in ABOVE these macros on
 * purpose: their own declarations (and any __mingw_ovr inline
 * definitions) must be seen with the real names before the names are
 * hijacked.  Each name is #undef'd first because several of them are
 * macros in some CRT headers. */
#undef sprintf
#undef fopen
#undef fwrite
#undef fprintf
#undef fputs
#undef fputc
#undef fclose
#undef free
#undef time
#undef localtime
#undef mkdir
#define sprintf   h_sprintf
#define fopen     h_fopen
#define fwrite    h_fwrite
#define fprintf   h_fprintf
#define fputs     h_fputs
#define fputc     h_fputc
#define fclose    h_fclose
#define free      h_free
#define time      h_time
#define localtime h_localtime
#define mkdir     h_mkdir

#endif /* PF_HARNESS_BATCH14_H */
