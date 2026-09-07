#!/usr/bin/env bash
# build_src_gcc.sh -- builds the GCC side of the win32_pilot.md SS6a x87
# experiment (see harness/GCC_X87.md) with the 32-bit MinGW GCC now
# installed at C:\msys64\mingw32\bin\gcc.exe.
#
# Compiles gcc_check.c (this dir) together with the SAME, UNCHANGED
# src/icytower/line_intersect.c and .../jump_player.c the MSVC src_check.exe
# links (build_src.cmd) into one wholly-GCC executable -- no MSVC object
# file is ever linked against a GCC one (see gcc_check.c's header comment
# for why that is the cheap, safe route here).
#
# Usage (run from an MSYS2 MINGW32 shell, or via
#   powershell: $env:MSYSTEM='MINGW32'; & C:\msys64\usr\bin\bash.exe -lc
#     '/d/.../carrier/lift/harness/build_src_gcc.sh OUT.exe [extra gcc flags...]'
# ):
#   build_src_gcc.sh gcc_check_x87_O2.exe -mfpmath=387 -O2
#   build_src_gcc.sh gcc_check_x87_O0.exe -mfpmath=387 -O0
#   build_src_gcc.sh gcc_check_x87_O2_fs.exe -mfpmath=387 -O2 -ffloat-store
#
# src/ itself needs no change and no seam (win32_pilot.md SS7a); the only
# additive plumbing is this script, gcc_check.c and pf_bindings_gcc_min.h.
set -euo pipefail
cd "$(dirname "$0")"

if [ $# -lt 1 ]; then
    echo "usage: build_src_gcc.sh <out.exe> [gcc flags...]" >&2
    exit 2
fi
OUT="$1"; shift

GCC=/mingw32/bin/gcc.exe
if [ ! -x "$GCC" ]; then GCC=gcc; fi

mkdir -p obj_gcc
"$GCC" -m32 "$@" -Wall -Wno-unused-variable -Wno-unused-but-set-variable \
    -I. -I../../gen -I../../../src/icytower \
    -include pf_harness_rand.h -include pf_harness_calltrace.h \
    -include pf_harness_msvc_types.h \
    gcc_check.c harness_rand.c call_trace_stubs.c \
    ../../../src/icytower/line_intersect.c \
    ../../../src/icytower/jump_player.c \
    ../../../src/icytower/new_rand.c \
    ../../../src/icytower/particle.c \
    ../../../src/icytower/ok_to_play.c \
    ../../../src/icytower/map.c \
    ../../../src/icytower/main_state.c \
    ../../../src/icytower/reset_player.c \
    ../../../src/icytower/update_player.c \
    ../../../src/icytower/play_jump_sound.c \
    ../../../src/icytower/start_reward.c \
    ../../../src/icytower/handle_player_collision_original.c \
    ../../../src/icytower/is_solid.c \
    ../../../src/icytower/draw_scroller.c \
    -o "$OUT"
echo "OK: harness/$OUT  (flags: $*)"
