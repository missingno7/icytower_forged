#!/usr/bin/env bash
# build_batch14.sh -- builds the COMPILED-CANDIDATE side of PROMOTIONS.md
# batch 14's two standalone oracles.
#
#   batch14_check.exe    qualify_hisc_table / sort_hisc_table /
#                        enter_hisc_table / get_rank_id / get_rank / hash /
#                        calc_replay_checksum_131 / calc_replay_checksum
#                        (batch14_check.py -- pure / memory domain)
#   batch14b_check.exe   destroy_replay / myDeleteFile / save_config /
#                        init_scroller / fadeOut / fadeIn / save_replay /
#                        save_profile
#                        (batch14b_check.py -- ordered call trace)
#
# Wholly-GCC (32-bit MinGW), the toolchain of record for every
# x87-sensitive function since batch 2 -- harness/GCC_X87.md.  -mno-sse is
# kept for the reason build_batch13.sh's own note gives, and this batch
# needs it for a second, independent reason: calc_replay_checksum's three
# statistics accumulators multiply a FLOAT column by an int and truncate
# the result to unsigned, which is exactly the `cvttss2si` shape that
# -mno-sse2 alone does not forbid.
#
# Usage (Git Bash or MSYS2):
#   ./build_batch14.sh
set -euo pipefail
cd "$(dirname "$0")"

GCC=/mingw32/bin/gcc.exe
if [ ! -x "$GCC" ]; then GCC=/c/msys64/mingw32/bin/gcc.exe; fi
if [ ! -x "$GCC" ]; then GCC=gcc; fi

# cc1.exe lives in lib/gcc/... and loads its DLLs from mingw32/bin, so that
# directory has to be on PATH even when gcc is invoked by absolute path --
# without it cc1 dies with STATUS_INVALID_IMAGE_FORMAT (0xC000007B) and gcc
# exits 1 printing nothing at all.
export PATH="$(dirname "$GCC"):$PATH"

SRC=../../../src/icytower
ORACLE=../../../port_forge/tools/win32_oracle
# pf_harness_msvc_types.h is the one-line `#define __int64 long long`
# shim the generated allegro_api.h needs under GCC (it spells
# `typedef unsigned __int64 uint64_t;` the MSVC way).  It used to sit in
# this directory; it now lives in the port_forge submodule.
COMMON_FLAGS="-m32 -mfpmath=387 -mno-sse -mno-sse2 -O2 -Wall -Wno-unused-variable -I. -I$SRC -I$ORACLE -include $ORACLE/pf_harness_msvc_types.h"

# ---- batch14_check.exe -----------------------------------------------------
# This executable links replay.c and profile.c WHOLE, so save_replay() and
# save_profile() come along even though the pure oracle never calls either;
# batch14_check.c defines their callees as abort()ing stubs so the link
# resolves and any accidental call is loud rather than silent.
"$GCC" $COMMON_FLAGS \
    batch14_check.c \
    "$SRC/hisc.c" \
    "$SRC/replay.c" \
    "$SRC/profile.c" \
    "$SRC/state.c" \
    -o batch14_check.exe
echo "OK: harness/batch14_check.exe"

# ---- batch14b_check.exe ----------------------------------------------------
# pf_harness_batch14.h is what makes the CRT calls inside replay.c /
# profile.c / config.c reach the tracing stubs in batch14b_check.c; the
# Allegro and game-function stubs are plain definitions there.
"$GCC" $COMMON_FLAGS -Wno-unused-function -include pf_harness_batch14.h \
    batch14b_check.c \
    "$SRC/hisc.c" \
    "$SRC/replay.c" \
    "$SRC/profile.c" \
    "$SRC/config.c" \
    "$SRC/fade.c" \
    "$SRC/scroller.c" \
    "$SRC/state.c" \
    -o batch14b_check.exe
echo "OK: harness/batch14b_check.exe"
