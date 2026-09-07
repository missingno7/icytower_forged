/* icytower_harness_project_native.c -- Icy Tower's own per-function
 * dispatch for the offline harness's macro-bound world, NATIVE form.
 *
 * Implements pf_harness_dispatch_native() (port_forge/tools/win32_oracle/
 * pf_harness_dispatch.h), called by the generic native_check.c, dispatching
 * to carrier/native/'s hand-written native_update_frame()/native_is_solid()
 * instead of the plain recovered src/icytower/ symbols
 * icytower_harness_project.c's pf_harness_dispatch_src() calls -- kept in
 * its own translation unit (not merged with that file) so native_check.exe's
 * build, which never links src/icytower/*.c, never needs to resolve that
 * file's externs (see that file's own header comment for the full reason).
 *
 * Note the ASYMMETRY with pf_harness_dispatch_src()'s own is_solid case: a
 * NATIVE form receives its pointer-shaped argument as a plain integer,
 * cast directly to a pointer type with NO pf_tr() translation here --
 * unlike src/icytower/is_solid.c (address-free, so the driver must
 * translate at the call site), carrier/native/native_is_solid.c does its
 * own PF_MEM() translation of the incoming value internally (that seam
 * used to live in this dispatch, before it moved into src/is_solid.c's
 * caller instead -- see icytower_harness_project.c's header comment), so
 * passing the raw guest VA straight through is correct here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "it_types.h"
#include "pf_harness_dispatch.h"

extern void __cdecl native_update_frame(void);
extern int  __cdecl native_is_solid(Tmap *, int, int);

unsigned int pf_harness_dispatch_native(const char *fn, unsigned int *a, unsigned int nargs)
{
    (void)nargs;
    if (!strcmp(fn, "update_frame")) {
        native_update_frame();
        return 0;
    } else if (!strcmp(fn, "is_solid")) {
        return (unsigned int)native_is_solid((Tmap *)(size_t)a[0], (int)a[1], (int)a[2]);
    }
    fprintf(stderr, "unknown function '%s'\n", fn);
    exit(2);
    return 0;
}
