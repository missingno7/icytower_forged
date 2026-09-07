#!/usr/bin/env python3
"""lift_check.py -- offline equivalence check for the LIFTED form.

Both sides run against the SAME flat guest address space (0x400000..0x800000),
built once from the PE image:

  ORIGINAL side : the original bytes executed in unicorn (QEMU x87 = real
                  80-bit extended precision -- this is what makes the
                  double-vs-80-bit question answerable at all).
  LIFTED side   : the generated C compiled by 32-bit MSVC into
                  harness/lift_check.exe, with PF_MEM() redirected at an
                  in-process copy of that same image (harness/pf_harness_mem.h).
                  No VirtualAlloc at 0x400000 is needed; the generated .c is
                  byte-identical in both configurations.

Neither side needs the game running.  Per vector we compare the return value
and the raw bytes of the function's comparison domain
(notes/promotion_candidates.md SS4) and report EQUAL or the first differing byte.
"""

import argparse
import json
import os
import random
import struct
import subprocess
import sys

import pefile
from unicorn import (Uc, UC_ARCH_X86, UC_MODE_32, UcError, UC_HOOK_CODE)
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP)

HERE = os.path.dirname(os.path.abspath(__file__))
GUEST_BASE = 0x400000
GUEST_SIZE = 0x400000                  # 0x400000 .. 0x800000
STACK_BASE = 0x00100000
STACK_SIZE = 0x00100000
RET_MAGIC = STACK_BASE + 0x10

PLAYER_VA = 0x790000                   # past every PE section (last ends 0x78b6e0)
MAP_VA = 0x792000
OUT_X = 0x794000                       # int *px / int *py for line_intersect
OUT_Y = 0x794004
OUT_FY = 0x794010                      # int *fy/*fx1/*fx2 for getFloorData
OUT_FX1 = 0x794014
OUT_FX2 = 0x794018
CTRL_VA = 0x796000                     # Tcontrol for is_up/is_down/.../is_any
GD_VA = 0x7a0000                       # Tgame_data for add_combo (comboPosts + combos[5000])
GD_COMBO_VA = 0x7a0040                 #   == GD_VA + 0x40 (comboPosts field)
GD_COMBOS_VA = 0x7a0044                #   == GD_VA + 0x44 (combos[] array base)
C_VA = 0x7bf000                        # source Tgd_combo for add_combo

# -- batch 3 (2026-09-07) additions --
JS_VA = 0x7bf100                       # source Tgd_jump_sequence for add_jump_sequence
PART_VA = 0x7a4000                     # Tparticle[512] for reset_particles
SCROLLER_VA = 0x7a8000                 # Tscroller for scroll_scroller/restart_scroller
SZ_JUMPSEQ = 12                        # Tgd_jump_sequence: start, dist, num (3 ints)
SZ_PARTICLE = 24                       # Tparticle: intensity, x, y, sx, sy, color (6 ints)
# DWARF-confirmed (artifacts/dwarf_info.txt DW_AT_data_member_location):
# Tgame_data.jumpPosts @ +60068 (0xeaa4), jumps[] array base @ +60072 (0xeaa8) --
# same derivation as GD_COMBO_VA/GD_COMBOS_VA above, one field later (jumpPosts
# follows combos[5000] the way comboPosts precedes it).
GD_JS_VA = GD_VA + 0xeaa4              # jumpPosts field
GD_JUMPS_VA = GD_VA + 0xeaa8           # jumps[] array base

G_SEED = 0x4ff108                      # double seed -- new_rand()'s LCG state
G_DEMO = 0x4dd250                      # Treplay *demo
G_CTRL = 0x5000c8                      # Tcontrol ctrl (player-1 control state)
G_HASFOCUS = 0x4bc020                  # int hasFocus
G_CLOSEBTN = 0x4dd264                  # int closeButtonClicked
G_CYCLE_COUNT = 0x506938               # volatile int cycle_count
G_FPS = 0x506948                       # volatile int fps
G_FRAME_COUNT = 0x506978               # volatile int frame_count
G_LPS = 0x506968                       # volatile int lps

# The ORIGINAL side must enter the function with the SAME x87 control word the
# game enters it with.  KNOWN: ___mingw_CRTStartup (0x401020) calls __fpreset
# (0x4b2850) = a bare FNINIT, which leaves CW = 0x037F: PC = 11 (64-bit
# significand, full extended precision) and RC = 00 (nearest-even).  unicorn
# powers up with CW = 0x0000 (PC = 00 = SINGLE precision), which is not what
# the game runs with, so the oracle executes FNINIT + FLDCW before every call.
CW_INIT = 0x037F
CW_SLOT = STACK_BASE + 0x20
CW_STUB = STACK_BASE + 0x30

G_REWARD_TIME = 0x4fec68
G_REWARD_SCALE = 0x4fac28
G_PLAYER_ID = 0x4fe518
G_PLY = 0x4ff128
G_LOGIC_COUNT = 0x506958
G_COLLISION_TYPE = 0x4dd140
G_MAX_SPEED = 0x4bdb80

# -- batch 7 (2026-09-07) -- call-trace domain additions (win32_pilot.md task
# brief "mechanism B") plus the globals play_jump_sound/handle_player_
# collision_original/start_reward read that PROMOTIONS.md batches 2/6
# mis-diagnosed as "unnamed": they are DWARF-named aggregate members
# (sounds[8], custom.jump_sound[0..2]), already declared in game_state.h --
# see PROMOTIONS.md batch 7 and src/icytower/names.json's _meta note.
G_ITRCHECK = 0x4dd168                  # int itrcheck (headless replay-checker mode)
G_OPTIONS_FLASH = 0x4fe528             # Toptions.flash (first field of `options`)
G_MAP = 0x4f8b18                       # Tmap `map` -- the REAL global handle_player_
                                        #   collision_original reads (distinct from
                                        #   is_solid's own scratch MAP_VA parameter slot)
G_ANY11 = 0x4dd170
G_ANY12 = 0x4dd174
G_ANY21 = 0x4dd17c
G_ANY22 = 0x4dd180
G_ANY23 = 0x4dd184
G_SOUNDS = 0x4dd2e0                    # SAMPLE *sounds[9]; sounds[8] == 0x4dd300
G_COMBO_SOUND = 0x4dd280               # SAMPLE *combo_sound[10]
G_CUSTOM_JUMP_SOUND = 0x4fabf4         # Tcustom.jump_sound[3] (custom @0x4fa738 + 1212)
G_REWARD_BMP = 0x4f8af8                # BITMAP *reward_bmp
G_DATA = 0x4dd23c                      # DATAFILE *data
G_STARS = 0x4facc8                     # Tparticle stars[512] (start_reward's confetti array)
DATA_TABLE_VA = 0x7c2000               # scratch DATAFILE[100] table `data` points at for
                                        #   start_reward's vectors (indices 90..99 populated)
SZ_DATAFILE_ENTRY = 16                 # {void *dat; int type; long size; int flags;}

# Mechanism B: for a function under test whose own effect includes a call
# into a still-ORIGINAL-only game function (play_sound here), the ORIGINAL
# side hooks the callee's entry VA (like the existing _rand_hook), captures
# argc argument dwords off the stack into a scratch "trace slot"
# (call_count + up to 3 args, reusing the ordinary memory-domain diff --
# no new comparator machinery needed), and stubs a `ret`; the compiled side
# links a harness-only stub (harness/call_trace_stubs.c, redirected in by
# harness/pf_harness_calltrace.h, force-included the same way
# pf_harness_rand.h redirects rand()) that logs the same shape into the same
# VA. See PROMOTIONS.md batch 7 "Mechanism B" for the design writeup.
CALLTRACE_PLAY_SOUND_VA = 0x7c1000     # {call_count, arg0, arg1, arg2} (16 bytes)


def load_call_targets(interop_index_path, names):
    """Table of callee name -> (va, argc), derived from interop_index.json's
    DWARF-recovered prototypes (not hand-counted) -- win32_pilot.md task
    brief: 'a table of callee name -> argument count, from the DWARF
    prototypes in interop_index.json, so any library-calling function can be
    verified' generically, not just play_sound."""
    with open(interop_index_path, encoding='utf-8') as f:
        idx = json.load(f)
    by_name = {fn['name']: fn for fn in idx['functions']}
    out = {}
    for nm in names:
        fn = by_name[nm]
        proto = fn['prototype']
        inside = proto[proto.index('(') + 1:proto.rindex(')')].strip()
        argc = 0 if inside in ('', 'void') else len(inside.split(','))
        out[nm] = {'va': int(fn['va'], 16), 'argc': argc}
    return out


CALL_TARGETS = load_call_targets(
    os.path.join(HERE, '..', '..', 'gen', 'interop_index.json'), ['play_sound'])

SZ_PLAYER = 184
SZ_MAP = 772
SZ_CONTROL = 36                        # Tcontrol: 8 ints + 1 byte + 3 pad
SZ_COMBO = 12                          # Tgd_combo: start, end, length (3 ints)
GD_LOW_WINDOW = 26                     # combos[0..25] -- covers every "small" comboPosts vector
GD_HIGH_BASE = 4990                    # combos[4990..4999] -- covers the boundary vectors

# -- add_floor pass (2026-09-07) --
DEMO_VA = 0x7c0000                     # scratch Treplay-shaped struct: only
SZ_DEMO = 0x100                        #   floor_shrink@0x8c/floor_size@0x90 matter
RAND_SEED_VA = 0x794020                # harness-only: initial msvcrt-LCG state word,
                                        # read by BOTH the unicorn rand() hook below
                                        # AND harness_rand_state on the compiled side
                                        # (src_check.c/gcc_check.c) -- see
                                        # harness/pf_harness_rand.h's header comment.
RAND_THUNK_VA = 0x4bad18               # _rand: `jmp *[0x514944]` (msvcrt IAT slot,
                                        # unmapped in unicorn -- see Oracle._rand_hook)


def i32(v):
    return struct.pack("<i", v & 0xFFFFFFFF if v >= 0 else v)


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def si32(v):
    return struct.pack("<i", ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000)


# --------------------------------------------------------------------------
# guest image
# --------------------------------------------------------------------------

def build_guest(image_path):
    pe = pefile.PE(image_path, fast_load=True)
    mm = pe.get_memory_mapped_image(ImageBase=GUEST_BASE)
    buf = bytearray(GUEST_SIZE)
    buf[0:len(mm)] = mm[:GUEST_SIZE]
    return bytes(buf)


# --------------------------------------------------------------------------
# vector generators
# --------------------------------------------------------------------------

DBL_SPECIALS = [
    0.0, -0.0, 1.0, -1.0, 0.5, -0.5,
    6.0, -6.0, 6.1, -6.1, 5.9999999999999991, -5.9999999999999991,
    11.0, -11.0, 10.999999999999998, -10.999999999999998,
    12.0, -12.0, 6.1000000000000005, -6.1000000000000005,
    1e-300, -1e-300, 5e-324, -5e-324,            # denormals
    2.2250738585072014e-308, -2.2250738585072014e-308,
    1.7976931348623157e308, -1.7976931348623157e308,   # DBL_MAX: *2 overflows
    8.988465674311579e307, -8.988465674311579e307,     # DBL_MAX/2
    float("inf"), float("-inf"), float("nan"),
    3.141592653589793, -2.718281828459045,
]


def rnd_double(rng, k):
    if k < len(DBL_SPECIALS):
        return DBL_SPECIALS[k]
    m = k % 5
    if m == 0:
        return rng.uniform(-30.0, 30.0)
    if m == 1:
        return rng.uniform(-12.2, 12.2)
    if m == 2:                       # random bit pattern: NaNs, infs, denormals
        return struct.unpack("<d", struct.pack("<Q", rng.getrandbits(64)))[0]
    if m == 3:                       # near the +-6.1 / +-11 decision boundaries
        c = rng.choice([6.0, 6.1, 11.0, 12.0, 12.2])
        return rng.choice([1, -1]) * (c + rng.choice([0, 1, -1, 2, -2]) * 2 ** -50)
    return struct.unpack("<d", struct.pack("<Q",
                                           rng.getrandbits(52) |
                                           (rng.randrange(0x7FE) << 52) |
                                           (rng.getrandbits(1) << 63)))[0]


def gen_update_frame(rng, k):
    rt_pool = [0, 1, 2, 9, 10, 11, 59, 60, 61, 100, 255, -1, -5, -60]
    rt = rt_pool[k] if k < len(rt_pool) else rng.randint(-200, 200)
    scale = rng.randint(-(1 << 24), 1 << 24)
    pid = rng.randrange(4)
    logic = rng.randint(-5000, 5000)
    ply = bytearray(rng.getrandbits(8) for _ in range(SZ_PLAYER))
    dead_pool = [0, 1, 8, 100, 291, 292, 299, 300, 301, -8, -100, -299]
    dead = dead_pool[k % len(dead_pool)] if k < 40 else rng.randint(-400, 400)
    struct.pack_into("<i", ply, 0x4c, dead)                    # dead
    struct.pack_into("<i", ply, 0x58, rng.choice([0, 1, -1]))  # edge
    struct.pack_into("<i", ply, 0x5c, rng.randint(-1000, 1000))  # edge_drawn
    struct.pack_into("<i", ply, 0x3c, rng.randint(-1000, 1000))  # frame
    writes = [(G_REWARD_TIME, si32(rt)), (G_REWARD_SCALE, si32(scale)),
              (G_PLAYER_ID, si32(pid)), (G_PLY + 4 * pid, u32(PLAYER_VA)),
              (G_LOGIC_COUNT, si32(logic)), (PLAYER_VA, bytes(ply))]
    return [], writes


def gen_is_solid(rng, k):
    m = bytearray(SZ_MAP)
    for r in range(32):
        b = r * 24
        struct.pack_into("<i", m, b + 0, rng.choice([0, 0, 0, 1, rng.randint(-3, 3)]))
        struct.pack_into("<i", m, b + 4, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 8, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 12, rng.getrandbits(31))
        struct.pack_into("<i", m, b + 16, rng.getrandbits(31))
        struct.pack_into("<i", m, b + 20, rng.getrandbits(31))
    off_pool = [0, 1, 7, 15, 16, 17, -1, -9, -16, -17, 0x40000000, -0x40000000]
    off = off_pool[k % len(off_pool)] if k < 48 else rng.randint(-100000, 100000)
    struct.pack_into("<i", m, 768, off)
    x = rng.randint(-600, 600)
    if k % 4 == 0:
        y = rng.randint(-40, 500)
    elif k % 4 == 1:
        y = rng.choice([-34, -33, -32, -17, -16, -1, 0, 1, 15, 16, 479, 480, 481])
    else:
        y = rng.randint(-20000, 20000)
    return [MAP_VA, x, y], [(MAP_VA, bytes(m))]


def gen_jump_player(rng, k):
    p = bytearray(rng.getrandbits(8) for _ in range(SZ_PLAYER))
    sx = rnd_double(rng, k)
    struct.pack_into("<d", p, 0x10, sx)
    status = 0 if k % 3 else rng.choice([1, -1, 7])
    struct.pack_into("<i", p, 0x34, status)
    struct.pack_into("<i", p, 0x50, rng.choice([0, 1]))     # rotate
    struct.pack_into("<i", p, 0x54, rng.randint(-1000, 1000))  # angle
    ct = rng.randrange(5)
    # arg2 == 0 is the physics path (the x87 one) -- weight it heavily
    a2_pool = [1, 2, 3, -1, -5, 100, -100]
    a2 = 0 if (k % 10) < 7 else (a2_pool[k % len(a2_pool)] if k % 3
                                 else rng.randint(-1000, 1000))
    writes = [(PLAYER_VA, bytes(p)), (G_COLLISION_TYPE, si32(ct))]
    return [PLAYER_VA, a2], writes


def gen_getFloorData(rng, k):
    """Tmap.room[32] (Tfloor: empty,start_tile,end_tile,level,sign,tiles,
    each int) + Tmap.offset -- same row layout is_solid()/gen_is_solid use."""
    m = bytearray(SZ_MAP)
    for r in range(32):
        b = r * 24
        struct.pack_into("<i", m, b + 0, rng.choice([0, 0, 0, 1, rng.randint(-3, 3)]))
        struct.pack_into("<i", m, b + 4, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 8, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 12, rng.getrandbits(31))
        struct.pack_into("<i", m, b + 16, rng.getrandbits(31))
        struct.pack_into("<i", m, b + 20, rng.getrandbits(31))
    off_pool = [0, 1, 7, 15, 16, 17, -1, -9, -16, -17, 0x40000000, -0x40000000]
    off = off_pool[k % len(off_pool)] if k < 48 else rng.randint(-100000, 100000)
    struct.pack_into("<i", m, 768, off)
    if k % 4 == 0:
        cy = rng.randint(-40, 500)
    elif k % 4 == 1:
        cy = rng.choice([-34, -33, -32, -17, -16, -1, 0, 1, 15, 16, 479, 480, 481])
    else:
        cy = rng.randint(-20000, 20000)
    writes = [(MAP_VA, bytes(m)),
              (OUT_FY, u32(0xdeadbeef)), (OUT_FX1, u32(0xdeadbeef)), (OUT_FX2, u32(0xdeadbeef))]
    return [MAP_VA, cy, OUT_FY, OUT_FX1, OUT_FX2], writes


def gen_reset_map(rng, k):
    m = bytearray(rng.getrandbits(8) for _ in range(SZ_MAP))
    return [MAP_VA], [(MAP_VA, bytes(m))]


def gen_add_combo(rng, k):
    """comboPosts pooled at small indices (covers the ordinary insert path)
    and near the 5000-entry bound (covers the reject-if->4999 branch);
    combos[] is only compared in the two windows those indices can land
    in (GD_LOW_WINDOW / GD_HIGH_BASE.. -- see the 'add_combo' SPECS entry),
    so comboPosts never strays outside either window."""
    low_pool = [0, 1, 2, 5, 10, 19, 25]
    high_pool = [4990, 4995, 4998, 4999, 5000, 5001, 5005, 5100]
    if k % 3 == 0:
        cp = low_pool[k % len(low_pool)] if k < 400 else rng.randint(0, GD_LOW_WINDOW - 1)
    else:
        cp = high_pool[k % len(high_pool)]
    c = bytearray(SZ_COMBO)
    struct.pack_into("<i", c, 0, rng.randint(-100000, 100000))   # start
    struct.pack_into("<i", c, 4, rng.randint(-100000, 100000))   # end
    struct.pack_into("<i", c, 8, rng.randint(-100000, 100000))   # length
    writes = [(GD_COMBO_VA, si32(cp)), (C_VA, bytes(c)),
              (GD_COMBOS_VA, b"\x00" * (GD_LOW_WINDOW * SZ_COMBO)),
              (GD_COMBOS_VA + GD_HIGH_BASE * SZ_COMBO, b"\x00" * (10 * SZ_COMBO))]
    return [GD_VA, C_VA], writes


def gen_control(mask, invert=False):
    def gen(rng, k):
        c = bytearray(rng.getrandbits(8) for _ in range(SZ_CONTROL))
        flags_pool = list(range(256)) if k < 256 else [rng.getrandbits(8)]
        c[0x20] = flags_pool[k % len(flags_pool)]
        return [CTRL_VA], [(CTRL_VA, bytes(c))]
    return gen


def gen_get_gamepad(rng, k):
    return [], []


# --------------------------------------------------------------------------
# batch 3 (2026-09-07) -- 15 leaf/near-leaf functions, all pure integer (no
# x87 in any of them), reusing the same struct-layout knowledge (SZ_MAP,
# SZ_CONTROL, SZ_COMBO's sibling SZ_JUMPSEQ) already validated by the
# is_solid/getFloorData/add_combo generators above.
# --------------------------------------------------------------------------

def gen_set_control(rng, k):
    c = bytearray(rng.getrandbits(8) for _ in range(SZ_CONTROL))
    up, down, left, right, fire = (rng.randint(-(1 << 31), (1 << 31) - 1) for _ in range(5))
    return [CTRL_VA, up, down, left, right, fire], [(CTRL_VA, bytes(c))]


def gen_init_control(rng, k):
    c = bytearray(rng.getrandbits(8) for _ in range(SZ_CONTROL))
    return [CTRL_VA], [(CTRL_VA, bytes(c))]


def gen_check_control_key(rng, k):
    """Populate the 7 fields check_control_key compares (key_left..key_pause,
    everything but use_joy/flags) with small values, then pool `key` so it
    matches one of them about half the time and misses entirely the rest."""
    c = bytearray(SZ_CONTROL)
    vals = [rng.randint(-1000, 1000) for _ in range(7)]
    offs = [4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c]
    for off, v in zip(offs, vals):
        struct.pack_into("<i", c, off, v)
    key = rng.choice(vals) if k % 2 == 0 else rng.randint(-1000, 1000)
    return [CTRL_VA, key], [(CTRL_VA, bytes(c))]


def gen_get_level(rng, k):
    """Same Tmap.room[32] row layout as gen_is_solid/gen_getFloorData; get_level
    reads `level` (offset +12) but does not gate on `empty`, unlike those two."""
    m = bytearray(SZ_MAP)
    for r in range(32):
        b = r * 24
        struct.pack_into("<i", m, b + 0, rng.choice([0, 0, 0, 1, rng.randint(-3, 3)]))
        struct.pack_into("<i", m, b + 4, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 8, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 12, rng.getrandbits(31) * rng.choice([1, -1]))
        struct.pack_into("<i", m, b + 16, rng.getrandbits(31))
        struct.pack_into("<i", m, b + 20, rng.getrandbits(31))
    off_pool = [0, 1, 7, 15, 16, 17, -1, -9, -16, -17, 0x40000000, -0x40000000]
    off = off_pool[k % len(off_pool)] if k < 48 else rng.randint(-100000, 100000)
    struct.pack_into("<i", m, 768, off)
    if k % 4 == 0:
        cy = rng.randint(-40, 500)
    elif k % 4 == 1:
        cy = rng.choice([-34, -33, -32, -17, -16, -1, 0, 1, 15, 16, 479, 480, 481])
    else:
        cy = rng.randint(-20000, 20000)
    return [MAP_VA, cy], [(MAP_VA, bytes(m))]


def gen_add_jump_sequence(rng, k):
    """Same low/high-window bound-testing shape as gen_add_combo: jumpPosts
    pooled near 0 (ordinary insert) and near 4999/5000 (the reject branch)."""
    low_pool = [0, 1, 2, 5, 10, 19, 25]
    high_pool = [4990, 4995, 4998, 4999, 5000, 5001, 5005, 5100]
    if k % 3 == 0:
        jp = low_pool[k % len(low_pool)] if k < 400 else rng.randint(0, GD_LOW_WINDOW - 1)
    else:
        jp = high_pool[k % len(high_pool)]
    js = bytearray(SZ_JUMPSEQ)
    struct.pack_into("<i", js, 0, rng.randint(-100000, 100000))   # start
    struct.pack_into("<i", js, 4, rng.randint(-100000, 100000))   # dist
    struct.pack_into("<i", js, 8, rng.randint(-100000, 100000))   # num
    writes = [(GD_JS_VA, si32(jp)), (JS_VA, bytes(js)),
              (GD_JUMPS_VA, b"\x00" * (GD_LOW_WINDOW * SZ_JUMPSEQ)),
              (GD_JUMPS_VA + GD_HIGH_BASE * SZ_JUMPSEQ, b"\x00" * (10 * SZ_JUMPSEQ))]
    return [GD_VA, JS_VA], writes


def gen_reset_particles(rng, k):
    buf = bytearray(rng.getrandbits(8) for _ in range(512 * SZ_PARTICLE))
    return [PART_VA], [(PART_VA, bytes(buf))]


def gen_scroll_scroller(rng, k):
    off = rng.randint(-(1 << 31), (1 << 31) - 1)
    step = rng.randint(-(1 << 31), (1 << 31) - 1)
    return [SCROLLER_VA, step], [(SCROLLER_VA + 0x18, si32(off))]


def gen_restart_scroller(rng, k):
    horiz = 0 if k % 2 == 0 else rng.choice([1, -1, 2, 7])
    width = rng.randint(-100000, 100000)
    height = rng.randint(-100000, 100000)
    off = rng.randint(-(1 << 31), (1 << 31) - 1)
    writes = [(SCROLLER_VA + 0x0, si32(horiz)), (SCROLLER_VA + 0x10, si32(width)),
              (SCROLLER_VA + 0x14, si32(height)), (SCROLLER_VA + 0x18, si32(off))]
    return [SCROLLER_VA], writes


def gen_cycle_counter(rng, k):
    pool = [0, 1, -1, 0x7fffffff, -0x80000000, 100]
    val = pool[k % len(pool)] if k < 100 else rng.randint(-(1 << 31), (1 << 31) - 1)
    return [], [(G_CYCLE_COUNT, si32(val))]


def gen_fps_counter(rng, k):
    fc = rng.randint(-(1 << 31), (1 << 31) - 1)
    lc = rng.randint(-(1 << 31), (1 << 31) - 1)
    old_fps = rng.randint(-(1 << 31), (1 << 31) - 1)
    old_lps = rng.randint(-(1 << 31), (1 << 31) - 1)
    return [], [(G_FRAME_COUNT, si32(fc)), (G_FPS, si32(old_fps)),
                (G_LOGIC_COUNT, si32(lc)), (G_LPS, si32(old_lps))]


def gen_get_demo(rng, k):
    pool = [0, 0x790000, 0x7fffffff, -1, -0x80000000]
    val = pool[k % len(pool)] if k < 20 else rng.randint(-(1 << 31), (1 << 31) - 1)
    return [], [(G_DEMO, si32(val))]


def gen_get_controls(rng, k):
    payload = bytes(rng.getrandbits(8) for _ in range(4))
    return [], [(G_CTRL, payload)]


def gen_switched_focus(rng, k):
    val = rng.randint(-(1 << 31), (1 << 31) - 1)
    return [], [(G_HASFOCUS, si32(val))]


def gen_clicked_close_button(rng, k):
    val = rng.randint(-(1 << 31), (1 << 31) - 1)
    return [], [(G_CLOSEBTN, si32(val))]


# --------------------------------------------------------------------------
# batch 4 (2026-09-07) -- new_rand (the game's own x87 float LCG, the
# callee that blocked update_particle/create_particle in batch 3) plus its
# two integer callers and the trivial ok_to_play.
# --------------------------------------------------------------------------

NEW_RAND_MULTIPLIER = 1.4294484665
NEW_RAND_MODULUS = 65535.0


def gen_new_rand(rng, k):
    """`seed` values chosen to exercise: the branch not taken (x <= MODULUS),
    taken with exactly one fold, and taken with several folds (the do/while
    loop the disassembly's fucom/je pair actually is -- reachable once
    |seed| is large enough that MULTIPLIER*seed exceeds 2*MODULUS), plus
    the truncation boundary of the final fractional-part conversion."""
    pool = [0.0, 1.0, -1.0, 0.5, -0.5, 65535.0, -65535.0, 131070.0,
            45845.0, -45845.0, 65535.0 / NEW_RAND_MULTIPLIER]
    if k < len(pool):
        s = pool[k]
    else:
        m = k % 5
        if m == 0:
            s = rng.uniform(-65535.0 * 4, 65535.0 * 4)
        elif m == 1:
            s = rng.uniform(-5.0, 5.0)
        elif m == 2:
            s = rng.uniform(-1e7, 1e7)          # several loop folds
        elif m == 3:
            # near a multiple of MODULUS/MULTIPLIER -- pushes x close to a
            # MODULUS boundary, exercising the final truncation the same
            # way line_intersect.c's _boundary() helper targets ua*dx1+0.5
            fold = rng.randint(-6, 6)
            s = (NEW_RAND_MODULUS * fold) / NEW_RAND_MULTIPLIER + rng.uniform(-2.0, 2.0)
        else:
            s = rng.uniform(-200000.0, 200000.0)  # the realistic gameplay range
    return [], [(G_SEED, struct.pack("<d", s))]


SZ_SEED = 8


def gen_update_particle(rng, k):
    p = bytearray(rng.getrandbits(8) for _ in range(SZ_PARTICLE))
    s = gen_new_rand(rng, k)[1][0][1]
    return [PART_VA], [(PART_VA, bytes(p)), (G_SEED, s)]


def gen_create_particle(rng, k):
    n = 512
    buf = bytearray(SZ_PARTICLE * n)
    for i in range(n):
        off = i * SZ_PARTICLE
        intens = rng.choice([0, 0, 0, rng.randint(-1000, 1000)])
        struct.pack_into("<i", buf, off, intens)
        for f in range(4, SZ_PARTICLE, 4):
            struct.pack_into("<i", buf, off + f, rng.randint(-100000, 100000))
    if k % 5 == 0:
        # force NO free slot anywhere -- exercises the "return 0, touch
        # nothing" path (the disassembly's xor si,si branch)
        for i in range(n):
            off = i * SZ_PARTICLE
            if struct.unpack_from("<i", buf, off)[0] == 0:
                struct.pack_into("<i", buf, off, rng.randint(1, 1000))
    x = rng.randint(-2000, 2000)
    y = rng.randint(-2000, 2000)
    s = gen_new_rand(rng, k)[1][0][1]
    return [PART_VA, x, y], [(PART_VA, bytes(buf)), (G_SEED, s)]


def gen_ok_to_play(rng, k):
    return [], []


# --------------------------------------------------------------------------
# add_floor (0x4167dc) -- the tower layout generator pass (2026-09-07).
#
# Domain: the WHOLE Tmap (772 bytes) -- the shift touches room[0..30], the
# generation logic touches room[31]. No return value.
#
# room[31].level is the one field that actually steers the branch structure
# (notes/layout_determinism.md SS2's KNOWN table); level_pool below hits
# every boundary by hand (k%250, k%2500, k%5, and the five floor_shrink!=0
# height breakpoints at 2999/5004/7504/10004/50005) before falling back to
# four random families that keep landing near those same boundaries by
# construction, so the random tail is not just uniform noise either.
#
# floor_size is kept in [0,4] -- floor_size_modifiers[5] is NOT
# range-checked by the original, so an out-of-range index reads whatever
# static data happens to sit next to the table at its real address
# (0x4bdb60), which the recovered source's own array (at a different host
# address) cannot reproduce; notes/layout_rules_1.5.1.md documents this as
# a deliberate domain restriction, not an oversight.
# --------------------------------------------------------------------------

ADD_FLOOR_LEVEL_POOL = [
    0, 1, 2, 3, 4, 5, 6, 10, 15, 20, 25,
    249, 250, 251, 499, 500, 501, 749, 750, 999, 1000, 1001,
    2495, 2499, 2500, 2501, 2504, 2505, 2999, 3000, 3001, 3004, 3005,
    4750, 4995, 4999, 5000, 5001, 5004, 5005, 5006, 5010, 5250, 5500,
    7495, 7499, 7500, 7501, 7504, 7505, 7510,
    9995, 9999, 10000, 10001, 10004, 10005, 10010,
    24995, 25000, 25005, 49995, 49999, 50000, 50004, 50005, 50006, 50010,
    75000, 100000, 249995, 250000, 250001, 250005, 500000,
]

# Directed cross-product (divergence 008, notes/living_record.md): the
# pooled levels above used to be paired with `floor_shrink = 0 if k % 2 == 0`
# and a random floor_size, which silently made the DIRECTED half of this
# generator one-sided -- an even-indexed pooled level was NEVER seen with
# floor_shrink != 0 (the float-ratio branch) and an odd-indexed one never
# with floor_shrink == 0 (the `6 + rand()%10` branch), so half of every
# hand-picked boundary went untested in the branch it was picked for.
#
# The first len(pool)*2*5 vectors now enumerate every
# (pooled level, floor_shrink in {0, 1}, floor_size in 0..4) triple
# deterministically instead. That includes, by construction, the exact
# in-vivo precondition divergence 008 first showed up at:
# level=5 (-> new level 6, the first REAL floor a game ever generates),
# floor_shrink=1, floor_size=1 -- the `human_test.txt` Treplay's own
# settings (MEASURED from the tick-220 snapshot: Treplay+0x8c..0x90 =
# {1, 1}), which reaches the k<=2999 float-ratio branch: ratio =
# (6/-5+300)/300.0f*10.0f = 9.9667f, width = 6 + rand()%9.
# floor_shrink is pinned to exactly 1 here (not a random nonzero) because
# the original only ever tests it against 0; the random tail below still
# sweeps arbitrary nonzero values.
ADD_FLOOR_DIRECTED = [(level, shrink, size)
                      for level in ADD_FLOOR_LEVEL_POOL
                      for shrink in (0, 1)
                      for size in range(5)]


def gen_add_floor(rng, k):
    m = bytearray(SZ_MAP)
    for r in range(31):
        b = r * 24
        struct.pack_into("<i", m, b + 0, rng.choice([0, -1, 0, rng.randint(-3, 3)]))
        struct.pack_into("<i", m, b + 4, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 8, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 12, rng.getrandbits(20))
        struct.pack_into("<i", m, b + 16, rng.getrandbits(20))
        struct.pack_into("<i", m, b + 20, rng.getrandbits(20))
    struct.pack_into("<i", m, 768, rng.randint(-100000, 100000))     # m->offset (unread)

    # room[31]: empty/start_tile/end_tile/sign/tiles are stale-on-entry
    # bytes any EMPTY-branch vector must carry through unchanged, so they
    # get arbitrary values too; level is the pooled/boundary-hunting one.
    b31 = 31 * 24
    struct.pack_into("<i", m, b31 + 0, rng.choice([0, -1, rng.randint(-3, 3)]))
    struct.pack_into("<i", m, b31 + 4, rng.randint(-40, 40))
    struct.pack_into("<i", m, b31 + 8, rng.randint(-40, 40))
    struct.pack_into("<i", m, b31 + 16, rng.getrandbits(20))
    struct.pack_into("<i", m, b31 + 20, rng.getrandbits(20))

    directed = ADD_FLOOR_DIRECTED[k] if k < len(ADD_FLOOR_DIRECTED) else None
    if directed is not None:
        level = directed[0]
    else:
        fam = k % 4
        if fam == 0:
            level = rng.randrange(0, 300000)
        elif fam == 1:
            level = rng.choice([250, 2500]) * rng.randint(0, 400) + rng.randint(-2, 2)
        elif fam == 2:
            level = 5 * rng.randint(0, 60000) + rng.choice([0, 1, 2, 3, 4])
        else:
            level = rng.randrange(0, 1 << 30)
    level = max(0, level) & 0x7FFFFFFF
    struct.pack_into("<i", m, b31 + 12, level)

    demo = bytearray(SZ_DEMO)
    if directed is not None:
        _, floor_shrink, floor_size = directed
    else:
        floor_shrink = 0 if k % 2 == 0 else (rng.randint(-1000, 1000) or 7)
        floor_size = rng.randrange(5)
    struct.pack_into("<i", demo, 0x8c, floor_shrink)
    struct.pack_into("<i", demo, 0x90, floor_size)

    rand_seed = rng.getrandbits(32)

    writes = [(MAP_VA, bytes(m)), (DEMO_VA, bytes(demo)),
              (G_DEMO, u32(DEMO_VA)), (RAND_SEED_VA, u32(rand_seed))]
    return [MAP_VA], writes


# --------------------------------------------------------------------------
# batch 6 (2026-09-07) -- player physics core: reset_player, update_player
# --------------------------------------------------------------------------

GRAVITY_MODIFIER_VA = 0x4bdba8         # double[3], Treplay.gravity selects it


def gen_reset_player(rng, k):
    """Every field random -- reset_player() zeroes almost all of them and
    the offline check's domain is the whole 184-byte struct, so any stale
    byte pattern that survives (x, y, angle -- see reset_player.c's header
    comment) has to be reproduced byte-for-byte too."""
    p = bytearray(rng.getrandbits(8) for _ in range(SZ_PLAYER))
    return [PLAYER_VA], [(PLAYER_VA, bytes(p))]


def gen_update_player(rng, k):
    """x/y/sx/sy as doubles (rnd_double's special-value pool covers NaN/inf/
    denormals and the exact +-100.0/+-555.0/+-85.0/+-4.0 boundaries the
    unicorn cross-check needed to catch the strict-vs-non-strict compare
    bug -- see update_player.c's header comment), status/bounce pooled
    around the values the function itself branches on (0/1/2/other),
    collision_type over its real 0..4 range, demo->gravity over its real
    0..2 range."""
    p = bytearray(rng.getrandbits(8) for _ in range(SZ_PLAYER))
    sx = rnd_double(rng, k)
    sy = rnd_double(rng, (k * 7 + 1) % len(DBL_SPECIALS) + k // 3)
    boundary_pool = [85.0, 555.0, 1000.0, -100.0,
                     84.99999999999999, 555.0000000000001,
                     999.9999999999999, -99.99999999999999,
                     4.0, -4.0, 3.9999999999999996, 4.000000000000001]
    if k % 5 == 0:
        x = rng.choice(boundary_pool[:8] + [0.0, 640.0])
    else:
        x = rng.uniform(-2000.0, 2000.0)
    if k % 5 == 0:
        y = rng.choice([1000.0, 999.9999999999999, 1000.0000000000002, 0.0])
    else:
        y = rng.uniform(-2000.0, 3000.0)
    struct.pack_into("<d", p, 0x0, x)
    struct.pack_into("<d", p, 0x8, y)
    struct.pack_into("<d", p, 0x10, sx)
    struct.pack_into("<d", p, 0x18, sy)
    status_pool = [0, 1, 2, 3, -1, 5, 7]
    status = status_pool[k % len(status_pool)] if k % 4 else rng.randint(-5, 8)
    struct.pack_into("<i", p, 0x34, status)
    bounce = rng.choice([0, 1, -1, 20, -20]) if k % 3 else rng.randint(-1000, 1000)
    struct.pack_into("<i", p, 0x60, bounce)

    ct = rng.randrange(5)
    grav_idx = rng.randrange(3)
    demo = bytearray(SZ_DEMO)
    struct.pack_into("<i", demo, 0x9c, grav_idx)   # Treplay.gravity

    writes = [(PLAYER_VA, bytes(p)), (G_COLLISION_TYPE, si32(ct)),
              (DEMO_VA, bytes(demo)), (G_DEMO, u32(DEMO_VA))]
    return [PLAYER_VA], writes


# --------------------------------------------------------------------------
# batch 7 (2026-09-07) -- the two recurring "unnamed global" blockers turn
# out to be already-DWARF-named aggregate members (sounds[8],
# custom.jump_sound[0..2] -- PROMOTIONS.md batch 7), so what actually
# unblocks play_jump_sound / handle_player_collision_original / start_reward
# is the call-trace domain (mechanism B) for their play_sound() call(s).
# --------------------------------------------------------------------------

_CT_PLAY_SOUND = {"va": CALL_TARGETS["play_sound"]["va"],
                   "argc": CALL_TARGETS["play_sound"]["argc"],
                   "slot": CALLTRACE_PLAY_SOUND_VA}


def _blank_call_trace():
    return (CALLTRACE_PLAY_SOUND_VA,
            u32(0) + u32(0xdeadbeef) + u32(0xdeadbeef) + u32(0xdeadbeef))


def gen_play_jump_sound(rng, k):
    """Tplayer.sy (offset 0x18, double) picked from rnd_double's pool, plus
    a family of vectors parked right on/near the two threshold constants
    (-22.0, -15.0 -- read from the original .rdata at 0x4d6cc4/0x4d6cc8,
    PROMOTIONS.md batch 7) so all three custom.jump_sound[] branches, and
    the boundary itself, get exercised. custom.jump_sound[0..2] are set to
    three distinct sentinel handle values per vector so the call-trace slot
    unambiguously names which one reached play_sound()."""
    p = bytearray(rng.getrandbits(8) for _ in range(SZ_PLAYER))
    if k % 3 == 0:
        sy = rng.choice([-22.0, -15.0,
                          -21.999999999999996, -22.000000000000004,
                          -14.999999999999998, -15.000000000000002,
                          -100.0, -30.0, -22.5, -18.0, -15.5, -5.0, 0.0])
    else:
        sy = rnd_double(rng, k)
    struct.pack_into("<d", p, 0x18, sy)
    j0 = 0xAAAA0000 | (k & 0xFF)
    j1 = 0xBBBB0000 | (k & 0xFF)
    j2 = 0xCCCC0000 | (k & 0xFF)
    writes = [(PLAYER_VA, bytes(p)),
              (G_CUSTOM_JUMP_SOUND + 0, u32(j0)),
              (G_CUSTOM_JUMP_SOUND + 4, u32(j1)),
              (G_CUSTOM_JUMP_SOUND + 8, u32(j2)),
              _blank_call_trace()]
    return [PLAYER_VA], writes


def gen_handle_player_collision_original(rng, k):
    """Reuses gen_is_solid's own real-map-content generator verbatim (this
    function calls the REAL global is_solid(&map, ...) twice, at the real
    `map` VA -- G_MAP -- not is_solid's own scratch MAP_VA parameter slot),
    so both is_solid() calls see a realistic room[] layout; p->x/p->y are
    integer-valued doubles (this function only ever truncates them with
    `fistpl` under the same round-to-zero control word every other physics
    function already uses, then does plain int arithmetic -- no x87
    precision question of its own to chase) chosen so left/right foot
    probes land in-bounds some of the time and out-of-bounds the rest,
    covering every any11/any12 zero/nonzero/equal/different combination
    over enough vectors. status is pooled over {0,1,2,3,-1} to hit all four
    branches (including the play_sound "just landed" gate)."""
    m = bytearray(SZ_MAP)
    for r in range(32):
        b = r * 24
        struct.pack_into("<i", m, b + 0, rng.choice([0, 0, 0, 1, rng.randint(-3, 3)]))
        struct.pack_into("<i", m, b + 4, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 8, rng.randint(-40, 40))
        struct.pack_into("<i", m, b + 12, rng.getrandbits(31))
        struct.pack_into("<i", m, b + 16, rng.getrandbits(31))
        struct.pack_into("<i", m, b + 20, rng.getrandbits(31))
    off_pool = [0, 1, 7, 15, 16, 17, -1, -9, -16, -17]
    struct.pack_into("<i", m, 768, off_pool[k % len(off_pool)] if k < 200 else rng.randint(-1000, 1000))

    p = bytearray(rng.getrandbits(8) for _ in range(SZ_PLAYER))
    x = float(rng.randint(-600, 600))
    if k % 4 == 1:
        y = float(rng.choice([-34, -33, -32, -17, -16, -1, 0, 1, 15, 16, 479, 480, 481]))
    else:
        y = float(rng.randint(-40, 500))
    struct.pack_into("<d", p, 0x0, x)
    struct.pack_into("<d", p, 0x8, y)
    status_pool = [0, 1, 2, 3, -1, 7]
    status = status_pool[k % len(status_pool)] if k % 3 else rng.randint(-5, 8)
    struct.pack_into("<i", p, 0x34, status)
    struct.pack_into("<d", p, 0x18, rnd_double(rng, k))   # sy: overwritten on some paths

    sounds8 = 0xDDDD0000 | (k & 0xFF)                     # sounds[8] sentinel handle
    writes = [(G_MAP, bytes(m)),                          # real `map` global, not the scratch slot
              (PLAYER_VA, bytes(p)),
              (G_PLAYER_ID, si32(0)), (G_PLY, u32(PLAYER_VA)),
              (G_ANY11, u32(0)), (G_ANY12, u32(0)),
              (G_ANY21, u32(0)), (G_ANY22, u32(0)), (G_ANY23, u32(0)),
              (G_SOUNDS + 8 * 4, u32(sounds8)),
              _blank_call_trace()]
    return [rng.getrandbits(32), rng.getrandbits(32)], writes   # both args ignored by the function


REWARD_TIER_ARG_POOL = [0, 1, 6, 7, 8, 14, 15, 16, 24, 25, 26, 34, 35, 36,
                         49, 50, 51, 69, 70, 71, 99, 100, 101, 139, 140, 141,
                         199, 200, 201]


def gen_start_reward(rng, k):
    """arg1 (the only real parameter) pooled at every tier boundary
    (REWARD_TIER_ARG_POOL, from the disassembly's own cascade of `cmp`
    thresholds -- 6/14/24/34/49/69/99/139/199, PROMOTIONS.md batch 7) so
    all 10 reward tiers (0..9) are reached, then random beyond that. Biases
    `itrcheck`/`options.flash` mostly toward the "normal" path (both taken
    most of the time) since that is the interesting, fully-recovered
    behaviour, with the two short-circuits sampled often enough (k % 5,
    k % 7) to prove they are honoured. `stars[]` pre-state and `seed`
    reuse gen_create_particle's own established technique (an occasional
    "no free slot anywhere" vector, k % 11 == 0) since start_reward's own
    particle spawn is create_particle()/new_rand() called directly, already
    independently verified by their own PROMOTIONS.md rows."""
    arg1 = REWARD_TIER_ARG_POOL[k % len(REWARD_TIER_ARG_POOL)] if k < len(REWARD_TIER_ARG_POOL) * 8 \
        else rng.randint(0, 400)

    itrcheck = 0 if k % 7 else rng.choice([0, 1, -1, 5])
    flash = 1 if k % 5 else 0

    n = 512
    buf = bytearray(SZ_PARTICLE * n)
    for i in range(n):
        off = i * SZ_PARTICLE
        intens = rng.choice([0, 0, 0, rng.randint(-1000, 1000)])
        struct.pack_into("<i", buf, off, intens)
        for f in range(4, SZ_PARTICLE, 4):
            struct.pack_into("<i", buf, off + f, rng.randint(-100000, 100000))
    if k % 11 == 0:
        for i in range(n):
            off = i * SZ_PARTICLE
            if struct.unpack_from("<i", buf, off)[0] == 0:
                struct.pack_into("<i", buf, off, rng.randint(1, 1000))

    # NOT rnd_double()'s general special-value pool: that pool includes
    # DBL_MAX/+-inf/NaN, and start_reward's own particle loop can call
    # new_rand() (transitively, through create_particle()) many times per
    # vector -- new_rand()'s recovered fold loop
    # (`do { x -= 65535.0; } while (x > 65535.0)`, new_rand.c) never
    # terminates for a seed whose first multiply overflows to +inf, which
    # gen_new_rand's OWN generator deliberately never draws for exactly
    # this reason (its pool/ranges are all finite). Reusing rnd_double()
    # here hung src_check.exe outright (found this pass, PROMOTIONS.md
    # batch 7) -- bounded ranges only, matching gen_new_rand's own choice.
    seed = rng.uniform(-1e7, 1e7) if k % 5 else rng.uniform(-200000.0, 200000.0)

    reward_time_pre = rng.getrandbits(32)
    reward_scale_pre = rng.getrandbits(32)
    reward_bmp_pre = 0xE0E00000 | (k & 0xFF)
    combo_sound = [0xF0000000 | (i << 8) | (k & 0xFF) for i in range(10)]

    # data[90+tier].dat must read from guest address DATA_TABLE_VA + tier*16
    # (the table below is tier-indexed, 0..9); since data[90+tier] is
    # computed by the ORIGINAL as `data_ptr + (90+tier)*16`, the `data`
    # global itself must hold data_ptr = DATA_TABLE_VA - 90*16 (never
    # dereferenced at that lower address -- only offsets 90..99 are ever
    # read by start_reward -- so it need not be separately backed).
    data_table = bytearray(160)   # tier-indexed, 16 bytes each (only .dat, offset 0, matters)
    for i in range(10):
        struct.pack_into("<I", data_table, i * SZ_DATAFILE_ENTRY,
                          0x10000000 | (i << 8) | (k & 0xFF))   # .dat sentinel

    writes = [(G_ITRCHECK, si32(itrcheck)), (G_OPTIONS_FLASH, si32(flash)),
              (G_REWARD_TIME, u32(reward_time_pre)), (G_REWARD_SCALE, u32(reward_scale_pre)),
              (G_REWARD_BMP, u32(reward_bmp_pre)),
              (G_COMBO_SOUND, b"".join(u32(v) for v in combo_sound)),
              (G_DATA, u32(DATA_TABLE_VA - 90 * SZ_DATAFILE_ENTRY)),
              (DATA_TABLE_VA, bytes(data_table)),
              (G_STARS, bytes(buf)), (G_SEED, struct.pack("<d", seed)),
              _blank_call_trace()]
    return [arg1], writes


# --------------------------------------------------------------------------
# line_intersect (0x406b80) -- the x87 discriminator
#
#   D  = dx1*dy3 - dx3*dy1        (32-bit IMULs, wrapping)
#   N1 = dx3*py  - dy3*px         ua = N1/D    px = x1-x3, py = y1-y3
#   N2 = dx1*py  - dy1*px         ub = N2/D
#   requires 0 <= ua <= 1 and 0 <= ub <= 1, then
#     *px_out = x1 + (int)(ua*dx1 + 0.5f)      (fistp under RC = "toward zero")
#     *py_out = y1 + (int)(ua*dy1 + 0.5f)
#
# EVERY x87 intermediate stays in the register stack; nothing is spilled to
# memory as a double.  That is what makes this function able to tell an 80-bit
# model apart from a 64-bit one, and it is why the vector families below aim
# at the truncation boundary of ua*dx1 + 0.5.
# --------------------------------------------------------------------------

def _w32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v >= (1 << 31) else v


def _gcd(a, b):
    while b:
        a, b = b, a % b
    return a


def _solve_lin(a, b, M):
    """every x with a*x == b (mod M), as (x0, step); None if unsolvable."""
    g = _gcd(a % M, M)
    if b % g:
        return None
    M2 = M // g
    return ((b // g) * pow(((a % M) // g) % M2, -1, M2) % M2, M2)


def _solve_pow2(D, c):
    """px such that w32(D*px + c) lands in [0, D].

    D*px mod 2**32 covers exactly the multiples of 2**v2(D), so the reachable
    target nearest zero is c mod 2**v2(D), which is below D and therefore a
    legal ub numerator."""
    v = 0
    d = D
    while d % 2 == 0:
        d //= 2
        v += 1
    t = c % (1 << v)                       # the reachable target in [0, 2**v)
    if t > D:
        return None
    mod = 1 << (32 - v)
    delta = ((t - c) % (1 << 32)) >> v
    return _w32((delta * pow(d % mod, -1, mod)) % mod)


def _boundary(rng, want_y):
    """Construct inputs for which ua*d + 0.5 is EXACTLY an integer, where d is
    dx1 (want_y = False) or dy1 (want_y = True).  ua is then a rational with a
    ~2^31 denominator that needs more than 53 significand bits, so the double
    model and the 80-bit model land on opposite sides of the truncation
    boundary about half the time."""
    for _ in range(200):
        k = rng.randrange(4, 22)
        D = ((rng.randrange(1 << 26, 1 << 30) >> k) << k) * 2
        M = ((rng.randrange(1 << 16, 1 << 30) >> k) << k)
        if D <= 0 or D >= (1 << 31) or M == 0:
            continue
        sol = _solve_lin(M, D // 2, D)
        if sol is None:
            continue
        n10, step = sol
        n1 = (n10 + rng.randrange(max(1, D // step)) * step) % D
        if n1 <= 0 or n1 >= D:
            continue
        if not want_y:
            # segment 3-4 horizontal: dx3 = 1, dy3 = 0, x3 = y3 = 0, x4 = 1
            # => D = -dy1, N1 = py, N2 = w32(py*dx1 + D*px)
            dx1, py = M, n1
            px = _solve_pow2(D, _w32(py * dx1))
            if px is None:
                continue
            x1, y1 = px, py
            return [x1, y1, _w32(x1 + dx1), _w32(y1 - D), 0, 0, 1, 0, OUT_X, OUT_Y]
        # segment 3-4 vertical: dx3 = 0, dy3 = 1, x3 = y3 = 0, y4 = 1
        # => D = dx1, N1 = -px, N2 = w32(dx1*py - dy1*px)
        dy1, px = M, _w32(-n1)
        dx1 = D
        py = _solve_pow2(D, _w32(-dy1 * px))
        if py is None:
            continue
        x1, y1 = px, py
        return [x1, y1, _w32(x1 + dx1), _w32(y1 + dy1), 0, 0, 0, 1, OUT_X, OUT_Y]
    return None


def _half_exact(rng):
    """dx1 == D makes ua*dx1 exactly N1, so the stored value is exactly N + 0.5
    -- the +-0.5 truncation boundary itself."""
    D = rng.randrange(1 << 20, 1 << 30) * 2
    if D >= (1 << 31):
        D = D >> 1
    n1 = rng.randrange(1, D)
    dx1 = D
    px = _solve_pow2(D, _w32(n1 * dx1))
    if px is None:
        return None
    return [px, n1, _w32(px + dx1), _w32(n1 - D), 0, 0, 1, 0, OUT_X, OUT_Y]


def gen_line_intersect(rng, k):
    v = None
    fam = k % 10
    if k < 400:                     # a solid block of constructed boundaries
        v = _boundary(rng, (k % 2) == 1)
    elif fam == 0:                  # game-like screen coordinates
        v = [rng.randint(-800, 800) for _ in range(8)] + [OUT_X, OUT_Y]
    elif fam == 1:                  # small, heavily degenerate (parallel, equal)
        a = [rng.randint(-8, 8) for _ in range(8)]
        if rng.random() < 0.5:
            a[4], a[5] = a[0], a[1]
            a[6], a[7] = a[2], a[3]          # identical segments -> D == 0
        v = a + [OUT_X, OUT_Y]
    elif fam == 2:                  # full-range int32: IMULs wrap, D is huge
        v = [rng.randrange(-(1 << 31), 1 << 31) for _ in range(8)] + [OUT_X, OUT_Y]
    elif fam == 3:                  # large but non-wrapping coordinates
        v = [rng.randrange(-(1 << 15), 1 << 15) for _ in range(8)] + [OUT_X, OUT_Y]
    elif fam == 4:                  # the exact +-0.5 truncation boundary
        v = _half_exact(rng)
    elif fam == 5:                  # |ua*dx1 + 0.5| pushed to the 2^31 edge
        D = rng.randrange(1 << 28, 1 << 30) * 2
        n1 = D - rng.randrange(0, 8)
        dx1 = rng.choice([-(1 << 31), (1 << 31) - 1, -(1 << 31) + 1, 1 << 30])
        px = _solve_pow2(D, _w32(n1 * dx1))
        if px is not None:
            v = [px, n1, _w32(px + dx1), _w32(n1 - D), 0, 0, 1, 0, OUT_X, OUT_Y]
    elif fam == 6:                  # constructed boundary, x
        v = _boundary(rng, False)
    elif fam == 7:                  # constructed boundary, y
        v = _boundary(rng, True)
    elif fam == 8:                  # mixed magnitudes
        v = [rng.choice([rng.randint(-50, 50),
                         rng.randrange(-(1 << 20), 1 << 20),
                         rng.randrange(-(1 << 30), 1 << 30)]) for _ in range(8)] \
            + [OUT_X, OUT_Y]
    else:                           # near-collinear: ua/ub close to 0 and 1
        b = rng.randint(-(1 << 20), 1 << 20)
        v = [0, 0, b, rng.randint(-(1 << 20), 1 << 20),
             rng.randint(-3, 3), b, rng.randint(-(1 << 20), 1 << 20), -b,
             OUT_X, OUT_Y]
    if v is None:
        v = [rng.randint(-800, 800) for _ in range(8)] + [OUT_X, OUT_Y]
    return [x & 0xFFFFFFFF for x in v], []

SPECS = {
    "update_frame": {"va": 0x406ac4, "gen": gen_update_frame, "cmp_eax": False,
                     "domain": [(G_REWARD_TIME, 4), (G_REWARD_SCALE, 4),
                                (PLAYER_VA, SZ_PLAYER)],
                     "domain_names": ["reward_time", "reward_scale", "Tplayer"]},
    "is_solid": {"va": 0x4166dc, "gen": gen_is_solid, "cmp_eax": True,
                 "domain": [(MAP_VA, SZ_MAP)], "domain_names": ["Tmap"],
                 "must_be_unchanged": [(MAP_VA, SZ_MAP)]},
    "jump_player": {"va": 0x418678, "gen": gen_jump_player, "cmp_eax": True,
                    "domain": [(PLAYER_VA, SZ_PLAYER)], "domain_names": ["Tplayer"]},
    "line_intersect": {"va": 0x406b80, "gen": gen_line_intersect, "cmp_eax": True,
                       "domain": [(OUT_X, 4), (OUT_Y, 4)],
                       "domain_names": ["*px", "*py"]},
    "getFloorData": {"va": 0x416770, "gen": gen_getFloorData, "cmp_eax": False,
                     "domain": [(OUT_FY, 4), (OUT_FX1, 4), (OUT_FX2, 4)],
                     "domain_names": ["*fy", "*fx1", "*fx2"]},
    "reset_map": {"va": 0x4166a4, "gen": gen_reset_map, "cmp_eax": False,
                 "domain": [(MAP_VA, SZ_MAP)], "domain_names": ["Tmap"]},
    "add_combo": {"va": 0x40414c, "gen": gen_add_combo, "cmp_eax": False,
                 "domain": [(GD_COMBO_VA, 4),
                            (GD_COMBOS_VA, GD_LOW_WINDOW * SZ_COMBO),
                            (GD_COMBOS_VA + GD_HIGH_BASE * SZ_COMBO, 10 * SZ_COMBO)],
                 "domain_names": ["comboPosts", "combos[0..25]", "combos[4990..4999]"]},
    "get_gamepad": {"va": 0x4017fc, "gen": gen_get_gamepad, "cmp_eax": True,
                    "domain": [(0x4f8748, 4)], "domain_names": ["gamepad.up"],
                    "must_be_unchanged": [(0x4f8748, 4)]},
    "is_up": {"va": 0x401844, "gen": gen_control(0x04), "cmp_eax": True,
             "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
             "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "is_down": {"va": 0x40185c, "gen": gen_control(0x08), "cmp_eax": True,
               "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
               "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "is_left": {"va": 0x401874, "gen": gen_control(0x01), "cmp_eax": True,
               "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
               "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "is_right": {"va": 0x401888, "gen": gen_control(0x02), "cmp_eax": True,
                "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
                "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "is_fire": {"va": 0x4018a0, "gen": gen_control(0x10), "cmp_eax": True,
               "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
               "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "is_pause": {"va": 0x4018b8, "gen": gen_control(0x40), "cmp_eax": True,
                "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
                "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "is_enter": {"va": 0x4018d0, "gen": gen_control(0x20), "cmp_eax": True,
                "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
                "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "is_any": {"va": 0x4018e8, "gen": gen_control(0xbf), "cmp_eax": True,
              "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
              "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    # -- batch 3 (2026-09-07) --
    "set_control": {"va": 0x4017d4, "gen": gen_set_control, "cmp_eax": False,
                    "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"]},
    "init_control": {"va": 0x401790, "gen": gen_init_control, "cmp_eax": False,
                     "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"]},
    "check_control_key": {"va": 0x401808, "gen": gen_check_control_key, "cmp_eax": True,
                          "domain": [(CTRL_VA, SZ_CONTROL)], "domain_names": ["Tcontrol"],
                          "must_be_unchanged": [(CTRL_VA, SZ_CONTROL)]},
    "get_level": {"va": 0x416748, "gen": gen_get_level, "cmp_eax": True,
                 "domain": [(MAP_VA, SZ_MAP)], "domain_names": ["Tmap"],
                 "must_be_unchanged": [(MAP_VA, SZ_MAP)]},
    "add_jump_sequence": {"va": 0x4040f4, "gen": gen_add_jump_sequence, "cmp_eax": False,
                          "domain": [(GD_JS_VA, 4),
                                     (GD_JUMPS_VA, GD_LOW_WINDOW * SZ_JUMPSEQ),
                                     (GD_JUMPS_VA + GD_HIGH_BASE * SZ_JUMPSEQ, 10 * SZ_JUMPSEQ)],
                          "domain_names": ["jumpPosts", "jumps[0..25]", "jumps[4990..4999]"]},
    "reset_particles": {"va": 0x418420, "gen": gen_reset_particles, "cmp_eax": False,
                        "domain": [(PART_VA, 512 * SZ_PARTICLE)],
                        "domain_names": ["Tparticle[512]"]},
    "scroll_scroller": {"va": 0x41f0c0, "gen": gen_scroll_scroller, "cmp_eax": False,
                        "domain": [(SCROLLER_VA + 0x18, 4)], "domain_names": ["offset"]},
    "restart_scroller": {"va": 0x41f0d0, "gen": gen_restart_scroller, "cmp_eax": False,
                         "domain": [(SCROLLER_VA + 0x18, 4)], "domain_names": ["offset"]},
    "cycle_counter": {"va": 0x41fed4, "gen": gen_cycle_counter, "cmp_eax": False,
                      "domain": [(G_CYCLE_COUNT, 4)], "domain_names": ["cycle_count"]},
    "fps_counter": {"va": 0x41fea4, "gen": gen_fps_counter, "cmp_eax": False,
                    "domain": [(G_FRAME_COUNT, 4), (G_FPS, 4),
                               (G_LOGIC_COUNT, 4), (G_LPS, 4)],
                    "domain_names": ["frame_count", "fps", "logic_count", "lps"]},
    "get_demo": {"va": 0x40696c, "gen": gen_get_demo, "cmp_eax": True,
                "domain": [(G_DEMO, 4)], "domain_names": ["demo"],
                "must_be_unchanged": [(G_DEMO, 4)]},
    "get_controls": {"va": 0x406978, "gen": gen_get_controls, "cmp_eax": True,
                     "domain": [(G_CTRL, 4)], "domain_names": ["ctrl+0x0"],
                     "must_be_unchanged": [(G_CTRL, 4)]},
    "switchedFromProgram": {"va": 0x406a5c, "gen": gen_switched_focus, "cmp_eax": False,
                            "domain": [(G_HASFOCUS, 4)], "domain_names": ["hasFocus"]},
    "switchedToProgram": {"va": 0x406a6c, "gen": gen_switched_focus, "cmp_eax": False,
                          "domain": [(G_HASFOCUS, 4)], "domain_names": ["hasFocus"]},
    "clickedCloseButton": {"va": 0x406a7c, "gen": gen_clicked_close_button, "cmp_eax": False,
                           "domain": [(G_CLOSEBTN, 4)], "domain_names": ["closeButtonClicked"]},
    # -- batch 4 (2026-09-07) --
    "new_rand": {"va": 0x406984, "gen": gen_new_rand, "cmp_eax": True,
                "domain": [(G_SEED, SZ_SEED)], "domain_names": ["seed"]},
    "update_particle": {"va": 0x41843c, "gen": gen_update_particle, "cmp_eax": False,
                        "domain": [(PART_VA, SZ_PARTICLE), (G_SEED, SZ_SEED)],
                        "domain_names": ["Tparticle", "seed"]},
    "create_particle": {"va": 0x418490, "gen": gen_create_particle, "cmp_eax": True,
                        "domain": [(PART_VA, 512 * SZ_PARTICLE), (G_SEED, SZ_SEED)],
                        "domain_names": ["Tparticle[512]", "seed"]},
    "ok_to_play": {"va": 0x406a50, "gen": gen_ok_to_play, "cmp_eax": True,
                   "domain": [], "domain_names": []},
    # -- add_floor pass (2026-09-07) --
    "add_floor": {"va": 0x4167dc, "gen": gen_add_floor, "cmp_eax": False,
                 "domain": [(MAP_VA, SZ_MAP)], "domain_names": ["Tmap"]},
    # -- batch 6 (2026-09-07) -- player physics core --
    "reset_player": {"va": 0x418550, "gen": gen_reset_player, "cmp_eax": False,
                     "domain": [(PLAYER_VA, SZ_PLAYER)], "domain_names": ["Tplayer"]},
    "update_player": {"va": 0x418740, "gen": gen_update_player, "cmp_eax": False,
                      "domain": [(PLAYER_VA, SZ_PLAYER)], "domain_names": ["Tplayer"]},
    # -- batch 7 (2026-09-07) -- call-trace domain (mechanism B) unblocks these --
    "play_jump_sound": {"va": 0x406ecc, "gen": gen_play_jump_sound, "cmp_eax": False,
                        "call_traces": [_CT_PLAY_SOUND],
                        # PLAYER_VA listed FIRST: dom_locate()/must_be_unchanged both
                        # index from offset 0 of the concatenated domain, so a
                        # must_be_unchanged entry must be domain[0] (every existing
                        # SPECS entry using must_be_unchanged has exactly one domain
                        # entry for the same reason -- is_solid, get_gamepad, ...).
                        "domain": [(PLAYER_VA, SZ_PLAYER), (CALLTRACE_PLAY_SOUND_VA, 16)],
                        "domain_names": ["Tplayer", "play_sound_trace(count,handle,pitch,pan)"],
                        "must_be_unchanged": [(PLAYER_VA, SZ_PLAYER)]},
    "handle_player_collision_original": {
        "va": 0x407e10, "gen": gen_handle_player_collision_original, "cmp_eax": False,
        "call_traces": [_CT_PLAY_SOUND],
        "domain": [(PLAYER_VA, SZ_PLAYER), (G_ANY11, 4), (G_ANY12, 4),
                   (G_ANY21, 4), (G_ANY22, 4), (G_ANY23, 4),
                   (CALLTRACE_PLAY_SOUND_VA, 16)],
        "domain_names": ["Tplayer", "any11", "any12", "any21", "any22", "any23",
                          "play_sound_trace(count,handle,pitch,pan)"]},
    "start_reward": {"va": 0x407c38, "gen": gen_start_reward, "cmp_eax": True,
                     "call_traces": [_CT_PLAY_SOUND],
                     "domain": [(G_REWARD_TIME, 4), (G_REWARD_SCALE, 4), (G_REWARD_BMP, 4),
                                (G_STARS, 512 * SZ_PARTICLE), (G_SEED, SZ_SEED),
                                (CALLTRACE_PLAY_SOUND_VA, 16)],
                     "domain_names": ["reward_time", "reward_scale", "reward_bmp",
                                       "stars[512]", "seed",
                                       "play_sound_trace(count,handle,pitch,pan)"]},
}

SRC_BATCH3_FUNCS = ("set_control,init_control,check_control_key,get_level,"
                    "add_jump_sequence,reset_particles,scroll_scroller,"
                    "restart_scroller,cycle_counter,fps_counter,get_demo,"
                    "get_controls,switchedFromProgram,switchedToProgram,"
                    "clickedCloseButton")

SRC_BATCH4_FUNCS = "new_rand,update_particle,create_particle,ok_to_play"

SRC_ADD_FLOOR_FUNCS = "add_floor"

SRC_BATCH6_FUNCS = "reset_player,update_player"

SRC_BATCH7_FUNCS = "play_jump_sound,handle_player_collision_original,start_reward"


# --------------------------------------------------------------------------
# ORIGINAL side (unicorn)
# --------------------------------------------------------------------------

class Oracle(object):
    def __init__(self, guest, call_traces=None):
        self.guest = guest
        self.mu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.mu.mem_map(GUEST_BASE, GUEST_SIZE)
        self.mu.mem_write(GUEST_BASE, guest)
        self.mu.mem_map(STACK_BASE, STACK_SIZE)
        self.mu.mem_write(RET_MAGIC, b"\xF4")           # never executed
        self.stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + u32(CW_SLOT)  # fninit; fldcw
        self.mu.mem_write(CW_SLOT, struct.pack("<H", CW_INIT))
        self.mu.mem_write(CW_STUB, self.stub)
        self._rand_state = 0
        # Harness-only shim (see harness/pf_harness_rand.h's header comment
        # for the full rationale and the compiled-side half of this same
        # trick): unicorn never maps msvcrt.dll, so add_floor's
        # `call 0x4bad18` (the _rand thunk, `jmp *[0x514944]`, an IAT slot)
        # would fault here. Hook the thunk's entry address instead of
        # letting the jmp execute -- emulate the LCG in Python from
        # self._rand_state (seeded per-vector via RAND_SEED_VA, gen_add_floor),
        # write EAX, and simulate the eventual `ret` ourselves by popping
        # the return address ourselves and redirecting EIP + emu_stop().
        self.mu.hook_add(UC_HOOK_CODE, self._rand_hook,
                          begin=RAND_THUNK_VA, end=RAND_THUNK_VA)
        # Mechanism B (see CALLTRACE_* / load_call_targets above): one hook
        # per traced callee VA, installed only for functions whose SPECS
        # entry declares "call_traces". Each hook captures argc stack dwords
        # into a fixed scratch slot (call_count + args) and stubs a `ret`
        # instead of letting unicorn execute the real callee (which is
        # correct, mapped game-image code -- but the point of this domain is
        # "which value reached the call", not "what the callee itself does",
        # and the callee may not be safely re-enterable/side-effect-free to
        # run twice per vector the way play_sound's own guard globals are).
        self._call_traces = list(call_traces or [])
        for ct in self._call_traces:
            self.mu.hook_add(UC_HOOK_CODE, self._make_call_trace_hook(ct),
                              begin=ct["va"], end=ct["va"])

    def _rand_hook(self, uc, address, size, data):
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret_addr = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]
        self._rand_state = (self._rand_state * 214013 + 2531011) & 0xFFFFFFFF
        result = (self._rand_state >> 16) & 0x7fff
        uc.reg_write(UC_X86_REG_EAX, result)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, ret_addr)
        uc.emu_stop()

    def _make_call_trace_hook(self, ct):
        slot_va, argc = ct["slot"], ct["argc"]

        def hook(uc, address, size, data):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret_addr = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]
            args = [struct.unpack("<I", bytes(uc.mem_read(esp + 4 + 4 * i, 4)))[0]
                    for i in range(argc)]
            count = struct.unpack("<I", bytes(uc.mem_read(slot_va, 4)))[0]
            uc.mem_write(slot_va, u32(count + 1))
            for i, a in enumerate(args):
                uc.mem_write(slot_va + 4 + 4 * i, u32(a))
            uc.reg_write(UC_X86_REG_EAX, 0)              # stub return value
            uc.reg_write(UC_X86_REG_ESP, esp + 4)         # pop the return address
            uc.reg_write(UC_X86_REG_EIP, ret_addr)
            uc.emu_stop()
        return hook

    def fpu_reset(self):
        """FNINIT + FLDCW 0x037F -- the x87 state ___mingw_CRTStartup leaves."""
        self.mu.emu_start(CW_STUB, CW_STUB + len(self.stub))

    def restore(self, regions):
        for va, n in regions:
            off = va - GUEST_BASE
            self.mu.mem_write(va, self.guest[off:off + n])

    def call(self, va, args, writes, domain):
        self.fpu_reset()
        self._rand_state = 0
        for wva, wb in writes:
            self.mu.mem_write(wva, wb)
            if wva == RAND_SEED_VA and len(wb) == 4:
                self._rand_state = struct.unpack("<I", wb)[0]
        esp = STACK_BASE + STACK_SIZE - 0x1000
        for a in reversed(args):
            esp -= 4
            self.mu.mem_write(esp, u32(a))
        esp -= 4
        self.mu.mem_write(esp, u32(RET_MAGIC))
        self.mu.reg_write(UC_X86_REG_ESP, esp)
        pc = va
        while True:
            self.mu.emu_start(pc, RET_MAGIC, count=4000000)
            eip = self.mu.reg_read(UC_X86_REG_EIP)
            if eip == RET_MAGIC:
                break
            pc = eip                # resumed here by _rand_hook's emu_stop()
        eax = self.mu.reg_read(UC_X86_REG_EAX) & 0xFFFFFFFF
        dom = b"".join(bytes(self.mu.mem_read(dva, dn)) for dva, dn in domain)
        return eax, dom


# --------------------------------------------------------------------------
# driver
# --------------------------------------------------------------------------

def write_vectors(path, domain, vectors):
    with open(path, "wb") as f:
        f.write(u32(0x564C4650))
        f.write(u32(GUEST_SIZE))
        f.write(u32(len(domain)))
        for va, n in domain:
            f.write(u32(va)); f.write(u32(n))
        f.write(u32(len(vectors)))
        for args, writes in vectors:
            f.write(u32(len(args)))
            for a in args:
                f.write(u32(a))
            f.write(u32(len(writes)))
            for va, b in writes:
                f.write(u32(va)); f.write(u32(len(b))); f.write(b)


def read_results(path, nvec, domlen):
    with open(path, "rb") as f:
        d = f.read()
    if len(d) < 8 or struct.unpack_from("<I", d, 0)[0] != 0x53455250:
        raise SystemExit("bad result file magic")
    n = struct.unpack_from("<I", d, 4)[0]
    if n != nvec:
        raise SystemExit("result count mismatch")
    out, o = [], 8
    for _ in range(n):
        eax = struct.unpack_from("<I", d, o)[0]; o += 4
        out.append((eax, d[o:o + domlen])); o += domlen
    return out


def dom_locate(spec, k):
    for (va, n), nm in zip(spec["domain"], spec["domain_names"]):
        if k < n:
            return "%s+0x%x (VA 0x%08x)" % (nm, k, va + k)
        k -= n
    return "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default=os.path.join(HERE, "..", "..", "..",
                                                    "assets", "icytower15.exe"))
    ap.add_argument("--form", default="lifted", choices=["lifted", "native", "src"],
                    help="which candidate form to check against ORIGINAL "
                         "(unicorn): 'lifted' runs harness/lift_check.exe "
                         "(generated lifted_<f> symbols), 'native' runs "
                         "harness/native_check.exe (hand-written native_<f> "
                         "symbols from carrier/native), 'src' runs "
                         "harness/src_check.exe (plain update_frame/is_solid "
                         "symbols compiled straight from src/icytower/, "
                         "win32_pilot.md SS7a). Only changes the --exe "
                         "default and the report/print labels below; the "
                         "vector generation, oracle and diff are identical "
                         "for all three forms (carrier/lift/README.md SS7).")
    ap.add_argument("--exe", default=None,
                    help="default: harness/lift_check.exe, "
                         "harness/native_check.exe or harness/src_check.exe, "
                         "per --form; overridden by --toolchain gcc's default "
                         "unless --exe is also given")
    ap.add_argument("--toolchain", default="msvc", choices=["msvc", "gcc"],
                    help="msvc (default): unchanged behaviour, --exe as "
                         "above. gcc: --form must be src; default --exe "
                         "becomes harness/gcc_check_x87_nosse_O2.exe, the "
                         "32-bit MinGW GCC build of the SAME src/icytower/"
                         "line_intersect.c and .../jump_player.c with real "
                         "x87 arithmetic AND SSE2 disabled (-mfpmath=387 "
                         "-mno-sse2 -O2) -- the ONLY flag combination "
                         "harness/GCC_X87.md found bit-equal to the "
                         "original over 80000 vectors; -mfpmath=387 alone "
                         "is NOT enough (GCC still truncates through SSE2's "
                         "cvttsd2sil off a memory-rounded double unless "
                         "SSE2 itself is disabled -- see GCC_X87.md SS2). "
                         "--funcs also narrows to line_intersect,jump_player "
                         "unless given explicitly, since gcc_check.exe only "
                         "wires up those two. Build the other variants with "
                         "build_src_gcc.sh/.cmd and pass one via --exe, e.g. "
                         "--exe harness/gcc_check_x87_O0.exe.")
    ap.add_argument("--funcs", default=None,
                    help="default: update_frame,is_solid,jump_player,"
                         "line_intersect for --form lifted; "
                         "update_frame,is_solid for --form native or src "
                         "(neither check.exe has jump_player/line_intersect)")
    ap.add_argument("--vectors", type=int, default=0,
                    help="vectors per function (0 = per-function default)")
    ap.add_argument("--seed", type=int, default=20260907)
    ap.add_argument("--fault", default=None,
                    help="negative control: FUNC:VECTOR:BYTE -- flip one bit of "
                         "the candidate (LIFTED or NATIVE) result and require "
                         "the comparator to name it")
    ap.add_argument("--census", action="store_true",
                    help="do not stop at the first difference: keep going and "
                         "report how many vectors differ (used to quantify the "
                         "x87 double-vs-80-bit result, README SS6)")
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    if args.toolchain == "gcc" and args.form != "src":
        raise SystemExit("--toolchain gcc only makes sense with --form src "
                         "(gcc_check.exe only wires up src/icytower symbols)")

    if args.exe is None:
        if args.toolchain == "gcc":
            args.exe = os.path.join(HERE, "gcc_check_x87_nosse_O2.exe")
        else:
            args.exe = os.path.join(HERE, {"native": "native_check.exe",
                                            "src": "src_check.exe"}.get(args.form, "lift_check.exe"))
    if args.funcs is None:
        if args.toolchain == "gcc":
            args.funcs = "line_intersect,jump_player,add_floor,update_player"
        elif args.form == "native":
            args.funcs = "update_frame,is_solid"
        elif args.form == "src":
            args.funcs = ("update_frame,is_solid,jump_player,getFloorData,reset_map,"
                         "add_combo,line_intersect,get_gamepad,is_up,is_down,is_left,"
                         "is_right,is_fire,is_pause,is_enter,is_any," + SRC_BATCH3_FUNCS +
                         "," + SRC_BATCH4_FUNCS + "," + SRC_ADD_FLOOR_FUNCS +
                         "," + SRC_BATCH6_FUNCS + "," + SRC_BATCH7_FUNCS)
        else:
            args.funcs = "update_frame,is_solid,jump_player,line_intersect"
    label = args.form.upper() + ("/GCC" if args.toolchain == "gcc" else "")

    guest = build_guest(args.image)
    gpath = os.path.join(HERE, "guest.bin")
    open(gpath, "wb").write(guest)

    fault = None
    if args.fault:
        fn, vi, bi = args.fault.split(":")
        fault = (fn, int(vi, 0), int(bi, 0))

    report = {}
    rc = 0
    for name in args.funcs.split(","):
        spec = SPECS[name]
        nvec = args.vectors or {"jump_player": 4000, "line_intersect": 4000}.get(name, 1500)
        rng = random.Random(args.seed + sum(ord(c) for c in name))
        vectors = [spec["gen"](rng, k) for k in range(nvec)]
        vpath = os.path.join(HERE, "vectors_%s.bin" % name)
        rpath = os.path.join(HERE, "results_%s.bin" % name)
        write_vectors(vpath, spec["domain"], vectors)

        r = subprocess.run([args.exe, gpath, vpath, rpath, name],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("[%s] %s side failed: %s%s" % (name, label, r.stdout, r.stderr))
            report[name] = {"result": "%s_SIDE_FAILED" % label, "form": args.form, "toolchain": args.toolchain,
                            "detail": r.stderr.strip()}
            rc = 1
            continue
        domlen = sum(n for _, n in spec["domain"])
        lifted = read_results(rpath, nvec, domlen)

        oracle = Oracle(guest, call_traces=spec.get("call_traces"))
        restore = list(spec["domain"]) + [(va, len(b)) for _, w in vectors for va, b in w]
        restore = sorted(set(restore))
        first_diff = None
        ndiff = 0
        unchanged_violation = None
        for k, (a, w) in enumerate(vectors):
            oracle.restore(restore)
            try:
                oeax, odom = oracle.call(spec["va"], a, w, spec["domain"])
            except UcError as e:
                first_diff = {"vector": k, "kind": "ORIGINAL faulted in unicorn",
                              "detail": str(e)}
                break
            leax, ldom = lifted[k]
            if fault and fault[0] == name and fault[1] == k:
                ldom = bytearray(ldom); ldom[fault[2]] ^= 0x01; ldom = bytes(ldom)
            if spec.get("must_be_unchanged"):
                for (uva, un) in spec["must_be_unchanged"]:
                    src = dict((wv, wb) for wv, wb in w).get(uva)
                    if src is not None and bytes(odom[:un]) != src[:un]:
                        unchanged_violation = k
            if spec["cmp_eax"] and oeax != leax:
                ndiff += 1
                if first_diff is None:
                    first_diff = {"vector": k, "kind": "return value",
                                  "original": "0x%08x" % oeax,
                                  "lifted": "0x%08x" % leax,
                                  "args": ["0x%08x" % x for x in a]}
                if not args.census:
                    break
                continue
            if odom != ldom:
                ndiff += 1
                if first_diff is None:
                    for bidx in range(len(odom)):
                        if odom[bidx] != ldom[bidx]:
                            first_diff = {"vector": k, "kind": "comparison domain",
                                          "at": dom_locate(spec, bidx),
                                          "byte_index": bidx,
                                          "original": "0x%02x" % odom[bidx],
                                          "lifted": "0x%02x" % ldom[bidx],
                                          "args": ["0x%08x" % x for x in a]}
                            break
                if not args.census:
                    break

        if first_diff is None:
            print("[%s/%s] EQUAL over %d vectors (%d domain bytes + %s)"
                  % (name, label, nvec, domlen, "EAX" if spec["cmp_eax"] else "no return value"))
            report[name] = {"result": "EQUAL", "form": args.form, "toolchain": args.toolchain, "vectors": nvec,
                            "domain_bytes": domlen,
                            "compared_return_value": spec["cmp_eax"]}
        else:
            print("[%s/%s] DIFFER at vector %d%s: %s"
                  % (name, label, first_diff["vector"],
                     (" (%d of %d vectors differ)" % (ndiff, nvec)) if args.census else "",
                     first_diff))
            report[name] = {"result": "DIFFER", "form": args.form, "toolchain": args.toolchain, "vectors": nvec,
                            "differing_vectors": ndiff if args.census else None,
                            "first": first_diff}
            rc = 1
        if unchanged_violation is not None:
            print("[%s] NOTE: the ORIGINAL modified its 'must be unchanged' domain "
                  "at vector %d" % (name, unchanged_violation))
            report[name]["negative_control_note"] = unchanged_violation

    if args.json:
        json.dump(report, open(args.json, "w"), indent=1, sort_keys=True)
    return rc


if __name__ == "__main__":
    sys.exit(main())
