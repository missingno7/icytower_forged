/* lib_bindings_selftest.c -- proof that carrier/gen/pf_lib_bindings.h (the
 * library-scope binding table) and src/icytower/allegro_api.h (the
 * standalone declaration world) both let address-free code call Allegro
 * functions and read Allegro globals by their plain upstream names, per
 * win32_pilot.md SS7a/SS7b and notes/library_boundary.md item 13.
 *
 * NOT part of src/ (kept in carrier/gen/, as the task instructing this file
 * requires): it deliberately mixes carrier concerns (checking which world
 * is active via PF_LIB_BINDINGS_H) that scripts/check_native_layer.py would
 * refuse inside src/. The function body itself, lib_bindings_selftest_touch,
 * is written in the same address-free style as a real src/icytower/*.c file
 * would use -- no VA, no PF_/IT_G_/IT_F_/PFN_ token appears inside it.
 *
 * Two builds of this exact file (BINDINGS_NOTES.md's own two-worlds proof,
 * extended to the library scope):
 *
 *   carrier world (both binding tables force-included, matching
 *   src/README.md's own build line for update_frame.c/is_solid.c):
 *     cl /nologo /c /W3 /TC /Icarrier\gen ^
 *        /FIcarrier\gen\pf_lib_bindings.h /FIcarrier\gen\pf_bindings_src.h ^
 *        carrier\gen\lib_bindings_selftest.c
 *
 *   standalone declaration world (no /FI at all; allegro_api.h is reached
 *   through the #include below, guarded by PF_LIB_BINDINGS_H so it is a
 *   no-op in the carrier world instead of re-declaring already-macro-
 *   expanded names):
 *     cl /nologo /c /W3 /TC /Icarrier\gen /Isrc\icytower ^
 *        carrier\gen\lib_bindings_selftest.c
 *
 * Both must produce 0 errors, 0 warnings.
 */
#ifndef PF_LIB_BINDINGS_H
#include "allegro_api.h"
#endif

void lib_bindings_selftest_touch(void)
{
    volatile char k = key[KEY_SPACE];
    int w = screen->w;

    blit(screen, screen, 0, 0, 0, 0, 1, 1);
    textout_ex(screen, font, "x", 0, 0, 1, -1);

    (void)k;
    (void)w;
}
