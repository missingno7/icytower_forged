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
from unicorn import (Uc, UC_ARCH_X86, UC_MODE_32, UcError)
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EAX)

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

SZ_PLAYER = 184
SZ_MAP = 772
SZ_CONTROL = 36                        # Tcontrol: 8 ints + 1 byte + 3 pad
SZ_COMBO = 12                          # Tgd_combo: start, end, length (3 ints)
GD_LOW_WINDOW = 26                     # combos[0..25] -- covers every "small" comboPosts vector
GD_HIGH_BASE = 4990                    # combos[4990..4999] -- covers the boundary vectors


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
}


# --------------------------------------------------------------------------
# ORIGINAL side (unicorn)
# --------------------------------------------------------------------------

class Oracle(object):
    def __init__(self, guest):
        self.guest = guest
        self.mu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.mu.mem_map(GUEST_BASE, GUEST_SIZE)
        self.mu.mem_write(GUEST_BASE, guest)
        self.mu.mem_map(STACK_BASE, STACK_SIZE)
        self.mu.mem_write(RET_MAGIC, b"\xF4")           # never executed
        self.stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + u32(CW_SLOT)  # fninit; fldcw
        self.mu.mem_write(CW_SLOT, struct.pack("<H", CW_INIT))
        self.mu.mem_write(CW_STUB, self.stub)

    def fpu_reset(self):
        """FNINIT + FLDCW 0x037F -- the x87 state ___mingw_CRTStartup leaves."""
        self.mu.emu_start(CW_STUB, CW_STUB + len(self.stub))

    def restore(self, regions):
        for va, n in regions:
            off = va - GUEST_BASE
            self.mu.mem_write(va, self.guest[off:off + n])

    def call(self, va, args, writes, domain):
        self.fpu_reset()
        for wva, wb in writes:
            self.mu.mem_write(wva, wb)
        esp = STACK_BASE + STACK_SIZE - 0x1000
        for a in reversed(args):
            esp -= 4
            self.mu.mem_write(esp, u32(a))
        esp -= 4
        self.mu.mem_write(esp, u32(RET_MAGIC))
        self.mu.reg_write(UC_X86_REG_ESP, esp)
        self.mu.emu_start(va, RET_MAGIC, count=4000000)
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
            args.funcs = "line_intersect,jump_player"
        elif args.form == "native":
            args.funcs = "update_frame,is_solid"
        elif args.form == "src":
            args.funcs = ("update_frame,is_solid,jump_player,getFloorData,reset_map,"
                         "add_combo,line_intersect,get_gamepad,is_up,is_down,is_left,"
                         "is_right,is_fire,is_pause,is_enter,is_any")
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

        oracle = Oracle(guest)
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
