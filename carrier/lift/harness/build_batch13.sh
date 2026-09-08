#!/usr/bin/env bash
# build_batch13.sh -- builds the COMPILED-CANDIDATE side of PROMOTIONS.md
# batch 13's standalone oracles (batch13_check.py and batch13b_check.py).
#
# Two executables, both wholly-GCC (32-bit MinGW, the toolchain of record
# for every x87-sensitive function since batch 2 -- harness/GCC_X87.md):
#
#   batch13_check.exe   get_version_str / syncProfileFromOptions /
#                       destroy_game_data   (batch13_check.py)
#   batch13b_check.exe  stopGameMusic / startGameMusic / play_sound /
#                       log2file / take_screenshot / draw_reward
#                       (batch13b_check.py)
#
# Both are one-shot CLIs run fresh per vector, NOT the persistent
# lift_check.exe wire-protocol harness -- see batch13_check.c's own header
# comment for why.
#
# destroy_game_data.c is compiled with -Dfree=harness_capture_free so its
# one call is captured rather than actually freeing an unallocated pointer;
# every other call-trace redirect for batch 13 lives in
# pf_harness_batch13.h, force-included ahead of the whole build the same way
# pf_harness_calltrace.h already is for the SPECS-table harnesses.
#
# Usage (Git Bash or MSYS2):
#   ./build_batch13.sh
set -euo pipefail
cd "$(dirname "$0")"

GCC=/mingw32/bin/gcc.exe
if [ ! -x "$GCC" ]; then GCC=/c/msys64/mingw32/bin/gcc.exe; fi
if [ ! -x "$GCC" ]; then GCC=gcc; fi

SRC=../../../src/icytower
# -mno-sse is NOT redundant next to -mno-sse2, and batch 13 is where that
# stopped being theoretical.  With -mno-sse2 alone, GCC still reaches for
# SSE1's `cvttss2si` to convert a FLOAT expression to int -- and to use it
# it must first spill the 80-bit x87 value to a 32-bit float slot, adding a
# rounding step the original does not have.  play_sound's
# `(int)(pan_x * 192.0f + 32.0f)` is exactly that shape, and the extra
# rounding showed up as 1 differing vector in 20000 (seed 20260908, vector
# 18904: pan -563 against the original's -562).  With -mno-sse, GCC emits
# the original's own sequence byte for byte -- fstps/flds, fmuls, fadds,
# fnstcw/fldcw/fistpl -- and the vector matches.
#
# Earlier batches are not silently affected: cvttss2si only applies to
# FLOAT sources, and every float-to-int conversion promoted before this
# batch (line_intersect, draw_frame, draw_reward, ...) converts from
# DOUBLE, which needs SSE2's cvttsd2si and was already forbidden.
COMMON_FLAGS="-m32 -mfpmath=387 -mno-sse -mno-sse2 -O2 -Wall -Wno-unused-variable -I. -I$SRC"

# ---- batch13_check.exe -----------------------------------------------------
mkdir -p obj_batch13
"$GCC" $COMMON_FLAGS -c -o obj_batch13/destroy_game_data.o \
    -Dfree=harness_capture_free "$SRC/destroy_game_data.c"
"$GCC" $COMMON_FLAGS \
    batch13_check.c \
    "$SRC/main_state.c" \
    "$SRC/state.c" \
    obj_batch13/destroy_game_data.o \
    -o batch13_check.exe
echo "OK: harness/batch13_check.exe"

# ---- batch13b_check.exe ----------------------------------------------------
# The traced-callee stubs live inside batch13b_check.c itself (there are
# only ~15 of them, all one-liners); pf_harness_batch13.h is what makes the
# CRT/pthreads calls inside logfile.c and screenshot.c reach them.
"$GCC" $COMMON_FLAGS -Wno-unused-function -include pf_harness_batch13.h \
    batch13b_check.c \
    "$SRC/sound.c" \
    "$SRC/logfile.c" \
    "$SRC/screenshot.c" \
    "$SRC/draw_reward.c" \
    "$SRC/state.c" \
    -o batch13b_check.exe
echo "OK: harness/batch13b_check.exe"
