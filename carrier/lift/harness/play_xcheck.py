#!/usr/bin/env python3
"""play_xcheck.py -- offline oracle for the three isolatable regions of
play() (0x411a00, 17420 bytes, main.c 3405-5021).  PROMOTIONS.md batch 12.

Why only three regions
----------------------
play() is one function with no return until the game is over.  It reaches
~45 game/Allegro/CRT callees, holds ~47 ebp-relative locals shared between
its tick half and its game-over half, and its outer loop is paced by a
50 Hz interrupt and by readkey()/keypressed().  There is no offline
comparison domain for the whole of it -- the in-vivo per-tick digest and
frame oracle are the authority (PROMOTIONS.md batch 12, "In vivo").

What IS offline-checkable is the part of play() that src/icytower/play.c
recovers as self-contained `static` helpers AND that the original emits as
straight-line code with no game calls:

  R1  clear_replay_telemetry()  main.c 3473-3480   0x411a61 .. 0x411ab0
      100 x 5 telemetry channels + tc_posts zeroed.  Memory domain: the
      2004 bytes of Treplay the loop writes.  No calls.
  R2  draw_pause_curtain()      main.c 4122-4124   0x412d55 .. 0x412db6
      640 ORDERED vtable calls (vline/hline alternating).  This is
      exactly the case lift_check.py's shared call-trace mechanism cannot
      express (count + first call only), so it is traced here the way
      draw_frame_xcheck.py / blit_to_screen_xcheck.py trace theirs, using
      batch 9's synthetic-vtable-VA trick.
  R3  collect_game_data()       main.c 4389-4418   0x4137ab .. 0x4138ee
      The end-of-game census: the final scores, plus a per-control press
      count walked out of the recorded input stream.  Memory domain:
      Tgame_data's header fields and its left/right/jump counters.  No
      calls.

Entry into a region is a synthetic frame: the guest is seeded, ebp/esp are
pointed at a scratch stack, the locals the region reads are written at
their ebp offsets, and emulation starts at the region's first instruction
and stops at the first instruction past it.  That is legitimate here
because each region is entered with the same register/memory state on
every path in the original: R1 loads `demo` itself, R2 starts at the
`xor %ebx,%ebx` that initialises its own induction variable, and R3 reads
only globals plus the zero the compiler parked at ebp-0x934.

Usage
-----
  python play_xcheck.py --region 1 --vectors 500  --seed 20260908
  python play_xcheck.py --region 2 --vectors 8    --seed 20260908
  python play_xcheck.py --region 3 --vectors 500  --seed 20260908
  python play_xcheck.py --all --vectors 500 --seed 20260908
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
import pf_win32_offline_oracle as pf                              # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE      # noqa: E402
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EBP,     # noqa: E402
                               UC_X86_REG_EAX, UC_X86_REG_EBX,
                               UC_X86_REG_EIP)

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")
EXE = os.path.join(HERE, "play_check.exe")
VEC = os.path.join(HERE, "play_vectors.bin")

# ------------------------------------------------------------- addresses
G_DEMO = 0x4dd250
G_GAMEDATA = 0x4dd260
G_PLAYER_ID = 0x4fe518
G_PLY = 0x4ff128
G_SWAP_SCREEN = 0x4dd194

# region entry / exit, read straight off artifacts/disasm.txt
R1_LO, R1_HI = 0x411a61, 0x411ab1
R2_LO, R2_HI = 0x412d55, 0x412db7
R3_LO, R3_HI = 0x4137ab, 0x4138ef

# scratch guest layout (inside 0x400000..0x800000, past the last real
# section, and clear of the VAs icytower_specs.py / the other xchecks use)
S_BASE = 0x790000
S_REPLAY = S_BASE + 0x000000        # Treplay  (0x8ac bytes)
S_RECORDS = S_BASE + 0x001000       # Trecord[4096] (0x8000 bytes)
S_PLAYER = S_BASE + 0x00a000        # Tplayer  (0xb8 bytes)
S_BMP = S_BASE + 0x00a200           # BITMAP
S_BMP_VT = S_BASE + 0x00a300        # its GFX_VTABLE
S_GAMEDATA = S_BASE + 0x00b000      # Tgame_data (0x1d554 bytes)

VT_SLOT_BASE = 0x7c6000             # synthetic "callee" VAs for vtable slots
VT_VLINE = VT_SLOT_BASE + 0x00
VT_HLINE = VT_SLOT_BASE + 0x10

RECORD_SLOTS = 320                  # record slots seeded per R3 vector

VT = {
    VT_VLINE: ("vline", 5),
    VT_HLINE: ("hline", 5),
}

# Treplay field offsets (src/icytower/game_types.h)
T_SIZE = 0x08
T_TC_POSTS = 0xd4
T_TC_C = 0x0d8
T_TC_Q = 0x268
T_TC_T = 0x3f8
T_TC_S = 0x588
T_TC_F = 0x718
T_DATA = 0x8a8

# Tgame_data field offsets
GD_SCORE = 0x04
GD_LEFT = 0x1d508
GD_RIGHT = 0x1d50c
GD_JUMP = 0x1d510

# Tplayer field offsets
P_LEVEL = 0x28
P_SCORE = 0x2c
P_BEST_COMBO = 0x30
P_NCTF = 0x70
P_BLC = 0x74
P_CCC = 0x78
P_JCTOP = 0x8c


def fnv(b):
    h = 14695981039346656037
    for x in b:
        h ^= x
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


_GUEST = {}


def guest():
    if "g" not in _GUEST:
        _GUEST["g"] = pf.build_guest(IMAGE)
    return _GUEST["g"]


def new_uc():
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(pf.GUEST_BASE, pf.GUEST_SIZE)
    mu.mem_write(pf.GUEST_BASE, guest())
    mu.mem_map(pf.STACK_BASE, pf.STACK_SIZE)
    mu.mem_write(pf.RET_MAGIC, b"\xF4")
    stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + struct.pack("<I", pf.CW_SLOT)
    mu.mem_write(pf.CW_SLOT, struct.pack("<H", pf.CW_INIT))
    mu.mem_write(pf.CW_STUB, stub)
    mu.emu_start(pf.CW_STUB, pf.CW_STUB + len(stub))
    return mu


def wi(mu, va, v):
    mu.mem_write(va, struct.pack("<i", ((int(v) + 0x80000000) & 0xFFFFFFFF) - 0x80000000))


def wu(mu, va, v):
    mu.mem_write(va, struct.pack("<I", v & 0xFFFFFFFF))


# =========================================================== R1: telemetry
class W1(object):
    def __init__(self, rnd, forced=None):
        f = forced or {}
        self.tc_posts = f.get("tc_posts", rnd.choice(
            [0, 1, 50, 98, 99, 100, -1, rnd.randint(-1000, 1000)]))
        self.chan = f.get("chan", [rnd.randint(-1000, 1000) for _ in range(500)])


def run1_orig(w):
    mu = new_uc()
    mu.mem_write(S_REPLAY, b"\0" * 0x900)
    wu(mu, G_DEMO, S_REPLAY)
    wi(mu, S_REPLAY + T_TC_POSTS, w.tc_posts)
    for j in range(100):
        for k, off in enumerate((T_TC_C, T_TC_Q, T_TC_T, T_TC_S, T_TC_F)):
            mu.mem_write(S_REPLAY + off + 4 * j,
                         struct.pack("<f", float(w.chan[j * 5 + k])))
    esp = pf.STACK_BASE + pf.STACK_SIZE - 0x4000
    mu.reg_write(UC_X86_REG_ESP, esp)
    mu.reg_write(UC_X86_REG_EBP, esp + 0x1000)
    mu.emu_start(R1_LO, R1_HI, count=2000000)
    posts = struct.unpack("<i", bytes(mu.mem_read(S_REPLAY + T_TC_POSTS, 4)))[0]
    digs = [fnv(bytes(mu.mem_read(S_REPLAY + off, 400)))
            for off in (T_TC_C, T_TC_Q, T_TC_T, T_TC_S, T_TC_F)]
    return "%d|%s" % (posts, "|".join("%016x" % d for d in digs))


def export1(ws, path):
    with open(path, "wb") as f:
        f.write(struct.pack("<iii", 0x31594C50, 1, len(ws)))
        for w in ws:
            f.write(struct.pack("<i", w.tc_posts))
            for v in w.chan:
                f.write(struct.pack("<i", v))


# ============================================================ R2: curtain
class W2(object):
    def __init__(self, rnd, forced=None):
        self.id = (forced or {}).get("id", rnd.randrange(1 << 30))


def run2_orig(w):
    mu = new_uc()
    mu.mem_write(S_BASE, b"\0" * 0x400)
    wu(mu, G_SWAP_SCREEN, S_BMP)
    wu(mu, S_BMP + 0x1c, S_BMP_VT)
    wu(mu, S_BMP_VT + 0x28, VT_VLINE)
    wu(mu, S_BMP_VT + 0x2c, VT_HLINE)

    trace = []

    def mk(name, argc):
        def hook(uc, address, size, data):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]
            args = [struct.unpack("<i", bytes(uc.mem_read(esp + 4 + 4 * i, 4)))[0]
                    for i in range(argc)]
            trace.append("%s|bmp|%d|%d|%d|%d" % (name, args[1], args[2],
                                                 args[3], args[4]))
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EIP, ret)
            uc.emu_stop()
        return hook

    for va, (name, argc) in VT.items():
        mu.hook_add(UC_HOOK_CODE, mk(name, argc), begin=va, end=va)

    esp = pf.STACK_BASE + pf.STACK_SIZE - 0x4000
    mu.reg_write(UC_X86_REG_ESP, esp)
    mu.reg_write(UC_X86_REG_EBP, esp + 0x1000)

    pc = R2_LO
    guard = 0
    while True:
        mu.emu_start(pc, R2_HI, count=20000000)
        eip = mu.reg_read(UC_X86_REG_EIP)
        if eip == R2_HI:
            break
        pc = eip
        guard += 1
        if guard > 4000:
            raise RuntimeError("call-hook resume runaway")
    return "%d;%s" % (len(trace), ";".join(trace)) if trace else "0"


def export2(ws, path):
    with open(path, "wb") as f:
        f.write(struct.pack("<iii", 0x31594C50, 2, len(ws)))
        for w in ws:
            f.write(struct.pack("<i", w.id))


# ========================================================== R3: game data
class W3(object):
    def __init__(self, rnd, forced=None):
        f = forced or {}
        self.level = f.get("level", rnd.choice([0, 1, 100, 999, rnd.randint(-5000, 5000)]))
        self.score = f.get("score", rnd.randint(-100000, 100000))
        self.best_combo = f.get("best_combo", rnd.randint(-100, 5000))
        self.nctf = f.get("nctf", rnd.randint(-100, 5000))
        self.blc = f.get("blc", rnd.randint(-100, 5000))
        self.ccc = f.get("ccc", [rnd.randint(-10, 500) for _ in range(5)])
        self.jctop = f.get("jctop", [rnd.randint(-10, 500) for _ in range(5)])
        self.size = f.get("size", rnd.choice([0, 1, 2, 3, 17, 64, 300, -1, -5]))
        # ALWAYS 320 record slots, whatever `size` says.  Seeding the tail
        # past `size` with live data is what makes an off-by-one in the
        # census loop bound observable -- with a zeroed tail the mutant
        # `i <= demo->size` is a provable no-op (batch 11 hit the same
        # thing with poll_control's button bound).
        self.flags = list(f["flags"]) if "flags" in f else             [rnd.randrange(256) for _ in range(RECORD_SLOTS)]
        while len(self.flags) < RECORD_SLOTS:
            self.flags.append(rnd.randrange(256))


def run3_orig(w):
    mu = new_uc()
    mu.mem_write(S_REPLAY, b"\0" * 0x900)
    mu.mem_write(S_PLAYER, b"\0" * 0x100)
    mu.mem_write(S_GAMEDATA, b"\0" * 0x80)
    mu.mem_write(S_GAMEDATA + 0x1d500, b"\0" * 0x20)
    mu.mem_write(S_RECORDS, b"\0" * (8 * 4096))

    wu(mu, G_DEMO, S_REPLAY)
    wu(mu, G_GAMEDATA, S_GAMEDATA)
    wi(mu, G_PLAYER_ID, 0)
    wu(mu, G_PLY, S_PLAYER)
    wu(mu, S_REPLAY + T_DATA, S_RECORDS)
    wi(mu, S_REPLAY + T_SIZE, w.size)
    for j, fl in enumerate(w.flags):
        mu.mem_write(S_RECORDS + 8 * j, bytes([fl & 0xFF]))

    wi(mu, S_PLAYER + P_LEVEL, w.level)
    wi(mu, S_PLAYER + P_SCORE, w.score)
    wi(mu, S_PLAYER + P_BEST_COMBO, w.best_combo)
    wi(mu, S_PLAYER + P_NCTF, w.nctf)
    wi(mu, S_PLAYER + P_BLC, w.blc)
    for j in range(5):
        wi(mu, S_PLAYER + P_CCC + 4 * j, w.ccc[j])
        wi(mu, S_PLAYER + P_JCTOP + 4 * j, w.jctop[j])

    esp = pf.STACK_BASE + pf.STACK_SIZE - 0x4000
    ebp = esp + 0x1000
    mu.reg_write(UC_X86_REG_ESP, esp)
    mu.reg_write(UC_X86_REG_EBP, ebp)
    # the compiler parks a materialised 0 at ebp-0x934 and fills
    # keys_pressed[]/last_keys[] from it (0x413797 / 0x413847 / 0x413873)
    wi(mu, ebp - 0x934, 0)
    mu.emu_start(R3_LO, R3_HI, count=200000000)

    def gi(off):
        return struct.unpack("<i", bytes(mu.mem_read(S_GAMEDATA + off, 4)))[0]

    head = [gi(GD_SCORE + 4 * k) for k in range(5)]
    ccc = [gi(0x18 + 4 * k) for k in range(5)]
    jc = [gi(0x2c + 4 * k) for k in range(5)]
    return "%s|%s|%d|%d|%d" % (
        "|".join(str(v) for v in head),
        "".join("%d," % v for v in ccc) + "".join("%d," % v for v in jc),
        gi(GD_LEFT), gi(GD_RIGHT), gi(GD_JUMP))


def export3(ws, path):
    with open(path, "wb") as f:
        f.write(struct.pack("<iii", 0x31594C50, 3, len(ws)))
        for w in ws:
            for v in (w.level, w.score, w.best_combo, w.nctf, w.blc):
                f.write(struct.pack("<i", v))
            for v in w.ccc:
                f.write(struct.pack("<i", v))
            for v in w.jctop:
                f.write(struct.pack("<i", v))
            f.write(struct.pack("<i", w.size))
            for v in w.flags:
                f.write(struct.pack("<i", v))


REGIONS = {
    1: ("clear_replay_telemetry", W1, run1_orig, export1),
    2: ("draw_pause_curtain", W2, run2_orig, export2),
    3: ("collect_game_data", W3, run3_orig, export3),
}

DIRECTED = {
    1: [{"tc_posts": 0}, {"tc_posts": 99}, {"tc_posts": -1},
        {"tc_posts": 100}, {"chan": [0] * 500}, {"chan": [1] * 500}],
    2: [{"id": 0}],
    3: [{"size": 0}, {"size": 1, "flags": [0]}, {"size": 1, "flags": [0xFF]},
        {"size": 2, "flags": [0x10, 0x10]}, {"size": 2, "flags": [0x10, 0x00]},
        {"size": 3, "flags": [0x01, 0x00, 0x01]},
        {"size": 3, "flags": [0x02, 0x02, 0x00]},
        {"size": 4, "flags": [0x80, 0x00, 0x80, 0x80]},
        {"size": -1}, {"size": 5, "flags": [0x13, 0x13, 0x00, 0x13, 0x2f]}],
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--region", type=int, default=0)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--vectors", type=int, default=500)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--directed", action="store_true")
    ap.add_argument("--exe", default=EXE)
    args = ap.parse_args()

    regions = [1, 2, 3] if args.all or not args.region else [args.region]
    bad_total = 0
    for r in regions:
        name, W, run_orig, export = REGIONS[r]
        rnd = random.Random(args.seed + r)
        if args.directed:
            ws = [W(rnd, f) for f in DIRECTED[r]]
        else:
            ws = [W(rnd) for _ in range(args.vectors)]
        export(ws, VEC)
        out = subprocess.run([args.exe, VEC], stdout=subprocess.PIPE,
                             check=True).stdout.decode().splitlines()
        if len(out) != len(ws):
            print("region %d (%s): compiled side produced %d/%d lines"
                  % (r, name, len(out), len(ws)))
            return 1
        bad = 0
        for i, w in enumerate(ws):
            o = run_orig(w)
            if o != out[i].strip():
                bad += 1
                if bad <= 3:
                    print("region %d vector %d DIFFER" % (r, i))
                    print("  orig: %s" % o[:220])
                    print("  src : %s" % out[i].strip()[:220])
        print("region %d (%s): %d vectors, differ %d"
              % (r, name, len(ws), bad))
        bad_total += bad
    return 1 if bad_total else 0


if __name__ == "__main__":
    sys.exit(main())
