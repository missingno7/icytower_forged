/* pf_harness_msvc_types.h -- harness-only portability shim, GCC build only.
 *
 * batch 8 (2026-09-07): draw_scroller.c is the first src/icytower/ file
 * the GCC harness build (build_src_gcc.sh) compiles that itself
 * #includes "allegro_api.h" (draw_buffer.c, the only earlier file to do
 * so, was never part of this build's file list -- ASSETS.md's draw_buffer
 * verification is compile-only, MSVC `cl`, never GCC). allegro_api.h
 * (generated, carrier/gen/gen_lib_bindings.py) has exactly one line that
 * needs it: `typedef unsigned __int64 uint64_t;` for file_size_ex()'s
 * return type -- `__int64` is an MSVC/Windows-SDK spelling this GCC
 * (plain `gcc -m32`, no Windows SDK headers pulled in) does not define on
 * its own, so the typedef fails to parse (`error: expected '=', ',', ';',
 * 'asm' or '__attribute__' before 'uint64_t'`, confirmed reproducible in
 * isolation).
 *
 * Fix scope, deliberately narrow: this is a GCC-toolchain-only portability
 * gap in a GENERATED file (allegro_api.h), not a src/ bug and not
 * something this batch's task (draw_scroller.c) should fix by hand-
 * editing generated output -- the real fix belongs in
 * carrier/gen/gen_lib_bindings.py (e.g. spell the typedef with
 * `unsigned long long` when targeting GCC, or emit it from <stdint.h>
 * directly), out of scope here. This one-line shim -- force-included
 * ahead of every file build_src_gcc.sh compiles, the same way
 * pf_harness_rand.h/pf_harness_calltrace.h already are -- unblocks this
 * batch's GCC verification without touching the generated header or
 * src/icytower/ at all.
 */
#ifndef PF_HARNESS_MSVC_TYPES_H
#define PF_HARNESS_MSVC_TYPES_H

#define __int64 long long

#endif
