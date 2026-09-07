#!/usr/bin/env python3
"""blit_to_screen_xcheck.py -- ordered-call-trace oracle for blit_to_screen
(0x40b6bc, 1415 bytes, main.c).  PROMOTIONS.md batch 11.

Why this is not a lift_check.py SPECS entry
-------------------------------------------
Exactly batch 10's draw_frame reason, one mode down: blit_mode 3 issues 480
`blit` calls per invocation, every one with a different d_x, and blit_mode 4
issues two `line`s with different arguments followed by two `blit`s.
lift_check.py's shared call-trace mechanism records per callee a COUNT plus
the arguments of its FIRST call only (PROMOTIONS.md batch 8's documented
limitation, deliberate there), so it would compare one of those 480 rows.
This file is therefore an additive, standalone oracle built on the SAME
engine (port_forge/tools/pf_win32_offline_oracle.py's build_guest and its
unicorn FNINIT/FLDCW convention), modelled directly on
draw_frame_xcheck.py -- not a new SPECS row, and lift_check.py/
icytower_specs.py's existing mechanism is untouched.

What is traced
--------------
Two named Allegro imports, `blit` (0x456264) and `stretch_blit` (0x4632f8),
plus five GFX_VTABLE slots the function reaches through Allegro AL_INLINEs,
hooked with batch 9's synthetic-vtable-VA trick (point the slot at an
otherwise-unused guest VA the engine can hook):

    screen->vtable +0x10  acquire              (acquire_screen())
    screen->vtable +0x14  release              (release_screen())
    screen->vtable +0x4c  draw_sprite_v_flip   (blit_mode 2)
    screen->vtable +0x50  draw_sprite_h_flip   (blit_mode 1)
    bmp->vtable    +0x34  line                 (blit_mode 4)

acquire/release are the interesting pair: the AL_INLINE tests the slot for
NULL before calling, so every vector randomises whether each is present --
and a NULL slot is a synthetic VA of 0, which the oracle simply never sees
called.  The memory domain is the one global this function writes,
`blit_mode` (0x4dd324, a FUNCTION-STATIC -- see blit_to_screen.c's own
header for why src/ spells it blit_mode__blit_to_screen).

Usage
-----
  python blit_to_screen_xcheck.py --random   --seed 20260908 --vectors 2000
  python blit_to_screen_xcheck.py --directed --seed 777
"""
import argparse
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(PROJ, "port_forge", "tools"))
import pf_win32_offline_oracle as pf                      # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE   # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP  # noqa: E402

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")
BLIT_TO_SCREEN_VA = 0x40b6bc

# ---------------------------------------------------------------- globals
G_DEBUG = 0x4dd160
G_KEY = 0x506988
G_BLIT_MODE = 0x4dd324
G_LOGIC_COUNT = 0x506958
G_PLAYER_ID = 0x4fe518
G_PLY = 0x4ff128
G_SCREEN = 0x4dda8c
G_COS_TBL = 0x4ce100                   # Allegro's own fixed[512] quarter wave
KEY_F2 = 0x30

# scratch guest layout (inside the mapped 0x400000..0x800000 window, past
# every real section -- the last one ends at ~0x78b7a0)
S_BASE = 0x7d8000
S_BMP = S_BASE + 0x0000                # BITMAP (the `bmp` argument)
S_BMP_VT = S_BASE + 0x0100             # its GFX_VTABLE (only +0x34 `line`)
S_SCREEN = S_BASE + 0x0400             # BITMAP (`screen`)
S_SCREEN_VT = S_BASE + 0x0500          # its GFX_VTABLE
S_PLAYER = S_BASE + 0x0800             # Tplayer

VT_SLOT_BASE = 0x7c5000                # synthetic "callee" VAs for vtable slots
VT_ACQUIRE = VT_SLOT_BASE + 0x00
VT_RELEASE = VT_SLOT_BASE + 0x10
VT_V_FLIP = VT_SLOT_BASE + 0x20
VT_H_FLIP = VT_SLOT_BASE + 0x30
VT_LINE = VT_SLOT_BASE + 0x40

LIBS = {
    0x456264: ("blit", 8),
    0x4632f8: ("stretch_blit", 10),
}
VT = {
    VT_ACQUIRE: ("acquire", 1),
    VT_RELEASE: ("release", 1),
    VT_V_FLIP: ("draw_sprite_v_flip", 4),
    VT_H_FLIP: ("draw_sprite_h_flip", 4),
    VT_LINE: ("line", 6),
}

# argument kinds per traced call: 'p' pointer (rendered as a symbol), 'i' int
KINDS = {
    "blit": "ppiiiiii",
    "stretch_blit": "ppiiiiiiii",
    "acquire": "p",
    "release": "p",
    "draw_sprite_v_flip": "ppii",
    "draw_sprite_h_flip": "ppii",
    "line": "piiiii",
}

SYMS = {S_BMP: "bmp", S_SCREEN: "screen"}


def sym(va):
    return SYMS.get(va & 0xFFFFFFFF, "?%08x" % (va & 0xFFFFFFFF))


def render_call(name, args):
    kinds = KINDS[name]
    out = [name]
    for i, a in enumerate(args):
        k = kinds[i] if i < len(kinds) else "i"
        out.append(sym(a) if k == "p" else str(a))
    return "|".join(out)


class World(object):
    """One vector's whole seeded state."""

    def __init__(self, rnd, forced=None):
        f = forced or {}
        self.debug = f.get("debug", 1 if rnd.random() < 0.85 else 0)
        # which F-keys are down.  Weighted so a SINGLE key is the common
        # case (that is what picks one mode) but multi-key vectors happen
        # too -- the original's seven independent `if`s mean the LAST one
        # down wins, and only a multi-key vector can catch that.
        self.keys = [0] * 7
        if "keys" in f:
            self.keys = list(f["keys"])
        else:
            r = rnd.random()
            if r < 0.70:
                self.keys[rnd.randrange(7)] = 1
            elif r < 0.90:
                for i in range(7):
                    self.keys[i] = 1 if rnd.random() < 0.3 else 0
            # else: no key down -- blit_mode keeps its incoming value
        self.blit_mode = f.get("blit_mode", rnd.choice([0, 1, 2, 3, 4, 5, 6]))
        self.logic_count = f.get("logic_count", rnd.randint(-100000, 100000))
        self.level = f.get("level", rnd.choice(
            [0, 1, 2, 479, 480, 481, 959, 960, -1, -480, rnd.randint(-2000, 5000)]))
        self.bw = f.get("bw", rnd.choice([640, 320, 1, 0, -1, 800]))
        self.bh = f.get("bh", rnd.choice([480, 240, 1, 0, -1, 600]))
        self.has_acquire = f.get("has_acquire", 1 if rnd.random() < 0.7 else 0)
        self.has_release = f.get("has_release", 1 if rnd.random() < 0.7 else 0)
        self.x = f.get("x", rnd.choice(
            [0.0, 80.0, 160.0, 159.9999, 160.0001, 480.0, 479.9999, 480.0001,
             600.0, 640.0, -50.0, 1e9, float("nan"), rnd.uniform(-200.0, 900.0)]))
        self.y = f.get("y", rnd.choice(
            [0.0, 80.0, 160.0, 159.9999, 400.0, 399.9999, 400.0001, 440.0,
             480.0, -50.0, 1e9, float("nan"), rnd.uniform(-200.0, 700.0)]))


_GUEST_CACHE = {}


def build_guest_cached():
    if "g" not in _GUEST_CACHE:
        _GUEST_CACHE["g"] = pf.build_guest(IMAGE)
    return _GUEST_CACHE["g"]


def cos_tbl(guest):
    off = G_COS_TBL - pf.GUEST_BASE
    return list(struct.unpack_from("<512i", guest, off))


def seed_guest(mu, w):
    def wi(va, v):
        mu.mem_write(va, struct.pack("<i", ((int(v) + 0x80000000) & 0xFFFFFFFF)
                                     - 0x80000000))

    def wu(va, v):
        mu.mem_write(va, struct.pack("<I", v & 0xFFFFFFFF))

    def wd(va, v):
        mu.mem_write(va, struct.pack("<d", v))

    mu.mem_write(S_BASE, b"\0" * 0x1000)
    mu.mem_write(G_KEY, b"\0" * 127)
    for i in range(7):
        mu.mem_write(G_KEY + KEY_F2 + i, bytes([w.keys[i]]))

    wi(G_DEBUG, w.debug)
    wi(G_BLIT_MODE, w.blit_mode)
    wi(G_LOGIC_COUNT, w.logic_count)
    wi(G_PLAYER_ID, 0)
    wu(G_PLY, S_PLAYER)
    wd(S_PLAYER + 0x00, w.x)
    wd(S_PLAYER + 0x08, w.y)
    wi(S_PLAYER + 0x28, w.level)

    wi(S_BMP + 0x00, w.bw)
    wi(S_BMP + 0x04, w.bh)
    wu(S_BMP + 0x1c, S_BMP_VT)
    wu(S_BMP_VT + 0x34, VT_LINE)

    wi(S_SCREEN + 0x00, 640)
    wi(S_SCREEN + 0x04, 480)
    wu(S_SCREEN + 0x1c, S_SCREEN_VT)
    wu(S_SCREEN_VT + 0x10, VT_ACQUIRE if w.has_acquire else 0)
    wu(S_SCREEN_VT + 0x14, VT_RELEASE if w.has_release else 0)
    wu(S_SCREEN_VT + 0x4c, VT_V_FLIP)
    wu(S_SCREEN_VT + 0x50, VT_H_FLIP)
    wu(G_SCREEN, S_SCREEN)


def run_original(guest, w):
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(pf.GUEST_BASE, pf.GUEST_SIZE)
    mu.mem_write(pf.GUEST_BASE, guest)
    mu.mem_map(pf.STACK_BASE, pf.STACK_SIZE)
    mu.mem_write(pf.RET_MAGIC, b"\xF4")
    stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + struct.pack("<I", pf.CW_SLOT)
    mu.mem_write(pf.CW_SLOT, struct.pack("<H", pf.CW_INIT))
    mu.mem_write(pf.CW_STUB, stub)

    seed_guest(mu, w)
    trace = []

    def mk(name, argc):
        def hook(uc, address, size, data):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]
            args = [struct.unpack("<i", bytes(uc.mem_read(esp + 4 + 4 * i, 4)))[0]
                    for i in range(argc)]
            trace.append(render_call(name, args))
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EIP, ret)
            uc.emu_stop()
        return hook

    for va, (name, argc) in list(LIBS.items()) + list(VT.items()):
        mu.hook_add(UC_HOOK_CODE, mk(name, argc), begin=va, end=va)

    mu.emu_start(pf.CW_STUB, pf.CW_STUB + len(stub))
    esp = pf.STACK_BASE + pf.STACK_SIZE - 0x2000
    esp -= 4
    mu.mem_write(esp, struct.pack("<I", S_BMP))
    esp -= 4
    mu.mem_write(esp, struct.pack("<I", pf.RET_MAGIC))
    mu.reg_write(UC_X86_REG_ESP, esp)

    pc = BLIT_TO_SCREEN_VA
    guard = 0
    while True:
        mu.emu_start(pc, pf.RET_MAGIC, count=20000000)
        eip = mu.reg_read(UC_X86_REG_EIP)
        if eip == pf.RET_MAGIC:
            break
        pc = eip
        guard += 1
        if guard > 4000:
            raise RuntimeError("call-hook resume runaway")

    dom = struct.unpack("<i", bytes(mu.mem_read(G_BLIT_MODE, 4)))[0]
    return trace, dom


# --------------------------------------------------------------- the C side
EXE = os.path.join(HERE, "blit_to_screen_check.exe")
VEC = os.path.join(HERE, "blit_to_screen_vectors.bin")
TRC = os.path.join(HERE, "blit_to_screen_ctrace.txt")


def export(worlds, tbl, path):
    with open(path, "wb") as f:
        f.write(struct.pack("<I", 0x31535442))          # "BTS1"
        f.write(struct.pack("<i", len(worlds)))
        for v in tbl:
            f.write(struct.pack("<i", v))
        for w in worlds:
            for v in (w.debug, w.blit_mode, w.logic_count, w.level, w.bw, w.bh):
                f.write(struct.pack("<i", v))
            for v in w.keys:
                f.write(struct.pack("<i", v))
            f.write(struct.pack("<i", w.has_acquire))
            f.write(struct.pack("<i", w.has_release))
            f.write(struct.pack("<d", w.x))
            f.write(struct.pack("<d", w.y))


def run_campaign(worlds, label):
    guest = build_guest_cached()
    orig = [run_original(guest, w) for w in worlds]
    export(worlds, cos_tbl(guest), VEC)
    subprocess.check_call([EXE, VEC, TRC])

    cvec, cdom, cur = [], [], None
    for ln in open(TRC).read().splitlines():
        if ln.startswith("#vec"):
            cur = []
            cvec.append(cur)
        elif ln.startswith("#dom|"):
            cdom.append(int(ln.split("|")[1]))
        elif ln:
            cur.append(ln)

    diffs, first = 0, None
    for v, (a, od) in enumerate(orig):
        b, cd = cvec[v], cdom[v]
        if a != b or od != cd:
            diffs += 1
            if first is None:
                k = 0
                while k < min(len(a), len(b)) and a[k] == b[k]:
                    k += 1
                first = dict(vector=v, orig_lines=len(a), c_lines=len(b),
                             first_diff_index=k,
                             orig=a[max(0, k - 2):k + 3],
                             c=b[max(0, k - 2):k + 3],
                             blit_mode=(od, cd))
    print("%s: vectors=%d  differ=%d" % (label, len(worlds), diffs))
    if first:
        print("FIRST DIFFERENCE:")
        for key, val in first.items():
            print("   %s: %s" % (key, val))
    return 1 if diffs else 0


def directed_worlds(seed):
    """The cross-product the random draw would never assemble: every
    blit_mode against every acquire/release presence, the F-key override in
    both the single-key and every-key-down forms, and -- for the two zoom
    modes -- x/y parked exactly on 0 / the clamp constant / one ulp either
    side of it, plus NaN."""
    rnd = random.Random(seed)
    ws = []
    xs = [0.0, 160.0, 159.99999, 160.00001, 480.0, 479.99999, 480.00001,
          80.0, 79.99999, 600.0, 600.00001, -0.0, -1e-9, float("nan"), 1e300]
    ys = [0.0, 160.0, 400.0, 399.99999, 400.00001, 80.0, 440.0, 440.00001,
          -1e-9, float("nan"), -1e300]
    for mode in range(7):
        for acq in (0, 1):
            for rel in (0, 1):
                ws.append(World(rnd, dict(debug=0, keys=[0] * 7,
                                          blit_mode=mode, has_acquire=acq,
                                          has_release=rel)))
    for i in range(7):
        k = [0] * 7
        k[i] = 1
        ws.append(World(rnd, dict(debug=1, keys=k, blit_mode=6)))
        ws.append(World(rnd, dict(debug=0, keys=k, blit_mode=6)))
    ws.append(World(rnd, dict(debug=1, keys=[1] * 7, blit_mode=0)))
    for x in xs:
        for y in ys:
            for mode in (5, 6):
                ws.append(World(rnd, dict(debug=0, keys=[0] * 7,
                                          blit_mode=mode, x=x, y=y)))
    for lv in (-961, -960, -481, -480, -1, 0, 1, 479, 480, 481, 959, 960, 961):
        for lc in (-1, 0, 1, 7, 96, 4096):
            ws.append(World(rnd, dict(debug=0, keys=[0] * 7, blit_mode=3,
                                      level=lv, logic_count=lc)))
            ws.append(World(rnd, dict(debug=0, keys=[0] * 7, blit_mode=4,
                                      level=lv, logic_count=lc)))
    return ws


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--random", action="store_true")
    ap.add_argument("--directed", action="store_true")
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--vectors", type=int, default=2000)
    args = ap.parse_args()

    if args.directed:
        return run_campaign(directed_worlds(args.seed), "directed")
    rnd = random.Random(args.seed)
    return run_campaign([World(rnd) for _ in range(args.vectors)], "random")


if __name__ == "__main__":
    sys.exit(main())
