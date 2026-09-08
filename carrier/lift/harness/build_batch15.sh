#!/usr/bin/env bash
# build_batch15.sh -- builds the COMPILED-CANDIDATE side of PROMOTIONS.md
# batch 15's two standalone oracles.
#
#   batch15_check.exe    key_to_str
#                        (batch15_check.py -- pure, canaried buffer)
#   batch15b_check.exe   create_replay / load_replay / getGameDataXML /
#                        draw_results / my_alert
#                        (batch15b_check.py -- ordered call trace +
#                         memory domain)
#
# Wholly-GCC (32-bit MinGW), the toolchain of record for every
# x87-sensitive function since batch 2 -- harness/GCC_X87.md.  -mno-sse is
# kept for the reason build_batch13.sh's own note gives; this batch needs
# it for getGameDataXML, whose `flr="%d"` column is a truncating x87
# fistpl (an explicit RC=11 control-word switch at 0x4045ff) that
# -mno-sse2 alone would still let GCC emit as cvttss2si.
#
# Usage (Git Bash or MSYS2):
#   ./build_batch15.sh
set -euo pipefail
cd "$(dirname "$0")"

GCC=/mingw32/bin/gcc.exe
if [ ! -x "$GCC" ]; then GCC=/c/msys64/mingw32/bin/gcc.exe; fi
if [ ! -x "$GCC" ]; then GCC=gcc; fi

# cc1.exe lives in lib/gcc/... and loads its DLLs from mingw32/bin, so that
# directory has to be on PATH even when gcc is invoked by absolute path --
# without it cc1 dies with STATUS_INVALID_IMAGE_FORMAT (0xC000007B) and gcc
# exits 1 printing nothing at all.  (batch14's environment note.)
export PATH="$(dirname "$GCC"):$PATH"

SRC=../../../src/icytower
ORACLE=../../../port_forge/tools/win32_oracle
COMMON_FLAGS="-m32 -mfpmath=387 -mno-sse -mno-sse2 -O2 -Wall -Wno-unused-variable -I. -I$SRC -I$ORACLE -include $ORACLE/pf_harness_msvc_types.h"

# ---- batch15_check.exe -----------------------------------------------------
"$GCC" $COMMON_FLAGS \
    batch15_check.c \
    "$SRC/menu_keys.c" \
    "$SRC/state.c" \
    -o batch15_check.exe
echo "OK: harness/batch15_check.exe"

# ---- batch15b_check.exe ----------------------------------------------------
# pf_harness_batch15.h is what makes the CRT calls inside replay.c and
# game_data.c reach the tracing stubs in batch15b_check.c; the Allegro,
# asset and game-function stubs are plain definitions there.
#
# assets_standalone.c is NOT linked: batch15b_check.c defines
# asset_bitmap()/asset_font() itself, so that the datafile objects
# results.c and alert.c ask for become STABLE SYMBOLS in the trace rather
# than pointers into a datafile this harness has no copy of.
#
# state.c is compiled in a SEPARATE invocation, deliberately WITHOUT the
# force-include: pf_harness_batch15.h renames REPLAY_HEADER (note 4
# there), and if state.c saw that rename its own zero-filled definition
# would collide with batch15b_check.c's correctly-initialised one.
# Compiled without the header it still defines the real (now unreferenced)
# REPLAY_HEADER and every other global the standalone world needs.
mkdir -p obj_batch15
"$GCC" $COMMON_FLAGS -c "$SRC/state.c" -o obj_batch15/state.o

"$GCC" $COMMON_FLAGS -Wno-unused-function -Wno-stringop-truncation \
    -include pf_harness_batch15.h \
    batch15b_check.c \
    "$SRC/replay.c" \
    "$SRC/game_data.c" \
    "$SRC/results.c" \
    "$SRC/alert.c" \
    obj_batch15/state.o \
    -o batch15b_check.exe
echo "OK: harness/batch15b_check.exe"
