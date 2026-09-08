#!/usr/bin/env python3
"""batch14_check.py -- PURE / MEMORY-domain oracle for PROMOTIONS.md
batch 14's eight non-tracing functions:

    qualify_hisc_table        0x404994    39 B   src/icytower/hisc.c
    sort_hisc_table           0x4049bc   147 B   src/icytower/hisc.c
    enter_hisc_table          0x405790   136 B   src/icytower/hisc.c
    get_rank_id               0x418a84    76 B   src/icytower/profile.c
    get_rank                  0x418ad0    82 B   src/icytower/profile.c
    hash                      0x41b9c8    71 B   src/icytower/replay.c
    calc_replay_checksum_131  0x41ba10   177 B   src/icytower/replay.c
    calc_replay_checksum      0x41bac4   676 B   src/icytower/replay.c

Why this is not a lift_check.py SPECS entry
-------------------------------------------
The same two reasons batches 10/12/13 gave, plus one specific to this
batch: three of the eight take a whole 2220-byte Treplay -- including a
variable-length record array reached through a POINTER field and 300
floats -- as their single input, which the SPECS wire protocol's fixed
per-function vector layout cannot express.  icytower_specs.py is
untouched again, the fifth batch running that way.

How it works
------------
1. The ORIGINAL bytes run under unicorn on the shared
   port_forge/tools/pf_win32_offline_oracle engine (build_guest, the
   FNINIT/FLDCW convention), exactly as batch13b_check.py does.
2. Only ONE callee needs hooking: enter_hisc_table TAIL-CALLS strcpy
   (0x405808 `jmp 4bad48 <_strcpy>`), whose thunk would land in an
   unmapped msvcrt.  It is emulated in Python at the thunk VA and the
   `ret` simulated, the same mechanism batch 5 used for rand() and
   batch 13 for the pthreads IAT slots.  Every other function here is a
   leaf.
3. The compiled candidate is batch14_check.exe (build_batch14.sh),
   driven by a BINARY vector file -- one process for the whole run.
   Binary because the payloads are raw structure images; see
   batch14_check.c's header for why that is the safer choice.
4. Domains, per function:
     qualify/sort/enter   the whole 180-byte `Thisc posts[5]` array,
                          plus the return value for qualify.  So "did
                          not touch anything else" is proven, not
                          assumed.
     get_rank_id          return value.
     get_rank             the returned STRING'S CONTENT (its address is
                          necessarily different on the two sides --
                          batch 13's get_version_str convention).
     hash / both checksums  return value (all three are pure: every
                          instruction in their ranges was read and none
                          of them stores to memory outside its own
                          frame).

Usage
-----
  python batch14_check.py --vectors 20000 --seed 20260908
  python batch14_check.py --fault
"""
import argparse
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(PROJ, "port_forge", "tools"))
import pf_win32_offline_oracle as pf                      # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE   # noqa: E402
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EAX,  # noqa: E402
                               UC_X86_REG_EIP)

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")
CHECK_EXE = os.path.join(HERE, "batch14_check.exe")
VECFILE = os.path.join(HERE, "batch14_vectors.bin")

FN = {
    "qualify_hisc_table":       0x404994,
    "sort_hisc_table":          0x4049bc,
    "enter_hisc_table":         0x405790,
    "get_rank_id":              0x418a84,
    "get_rank":                 0x418ad0,
    "hash":                     0x41b9c8,
    "calc_replay_checksum_131": 0x41ba10,
    "calc_replay_checksum":     0x41bac4,
}

STRCPY_THUNK = 0x4bad48

# ------------------------------------------------------------ globals
G_RANK_FLOORS = 0x4bdc20        # int[12]
G_RANK_COMBOS = 0x4bdc60        # int[12]
G_RANK_NMLS   = 0x4bdce0        # int[12]
G_RANK_CCCS   = 0x4bdca0        # int[12]
G_RANK_LABELS = 0x4bdbe0        # char *[12]

# --------------------------------------------------------- scratch VAs
S = 0x7d0000
S_POSTS   = S + 0x0000          # Thisc[5], 180 bytes
S_TABLE   = S + 0x0100          # Thisc_table (name[32] + posts)
S_NAME    = S + 0x0140          # char[33]
S_PROFILE = S + 0x0200          # Tprofile, 0x550 bytes
S_REPLAY  = S + 0x0800          # Treplay, 0x8ac bytes
S_RECS    = S + 0x1200          # Trecord[], 8 bytes each
S_LABELS  = S + 0x9200          # 12 x 16-byte label strings

REPLAY_BYTES = 0x8ac
POSTS_BYTES = 5 * 36
PROFILE_BYTES = 0x550

K_QUALIFY, K_SORT, K_ENTER, K_RANK, K_HASH, K_CS131, K_CS = range(7)


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


# --------------------------------------------------------------------------
# ORIGINAL side
# --------------------------------------------------------------------------
_GUEST = {}


def guest_bytes():
    if "g" not in _GUEST:
        _GUEST["g"] = pf.build_guest(IMAGE)
    return _GUEST["g"]


class Original(object):
    def __init__(self):
        g = guest_bytes()
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(pf.GUEST_BASE, pf.GUEST_SIZE)
        mu.mem_write(pf.GUEST_BASE, g)
        mu.mem_map(pf.STACK_BASE, pf.STACK_SIZE)
        mu.mem_write(pf.RET_MAGIC, b"\xF4")
        self.stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + u32(pf.CW_SLOT)
        mu.mem_write(pf.CW_SLOT, struct.pack("<H", pf.CW_INIT))
        mu.mem_write(pf.CW_STUB, self.stub)
        mu.hook_add(UC_HOOK_CODE, self._strcpy_hook,
                    begin=STRCPY_THUNK, end=STRCPY_THUNK)
        # the 12 rank labels: fixed scratch strings, written once
        for i in range(12):
            mu.mem_write(S_LABELS + 16 * i, ("rank%d" % i).encode() + b"\0")
            mu.mem_write(G_RANK_LABELS + 4 * i, u32(S_LABELS + 16 * i))

    # ---- guest helpers ----
    def gstr(self, va, cap=512):
        out = b""
        while len(out) < cap:
            c = bytes(self.mu.mem_read(va + len(out), 1))
            if c == b"\0":
                break
            out += c
        return out.decode("latin1")

    def _strcpy_hook(self, uc, address, size, data):
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]
        dst = struct.unpack("<I", bytes(uc.mem_read(esp + 4, 4)))[0]
        src = struct.unpack("<I", bytes(uc.mem_read(esp + 8, 4)))[0]
        s = self.gstr(src)
        uc.mem_write(dst, s.encode("latin1") + b"\0")
        uc.reg_write(UC_X86_REG_EAX, dst)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, ret)
        uc.emu_stop()

    def call(self, va, args):
        mu = self.mu
        mu.emu_start(pf.CW_STUB, pf.CW_STUB + len(self.stub))
        esp = pf.STACK_BASE + pf.STACK_SIZE - 0x2000
        for v in reversed(args):
            esp -= 4
            mu.mem_write(esp, u32(v))
        esp -= 4
        mu.mem_write(esp, u32(pf.RET_MAGIC))
        mu.reg_write(UC_X86_REG_ESP, esp)
        pc, guard = va, 0
        while True:
            mu.emu_start(pc, pf.RET_MAGIC, count=8000000)
            eip = mu.reg_read(UC_X86_REG_EIP)
            if eip == pf.RET_MAGIC:
                break
            pc = eip
            guard += 1
            if guard > 40000:
                raise RuntimeError("call-hook resume runaway")
        return mu.reg_read(UC_X86_REG_EAX)


def hexs(b):
    return "".join("%02x" % c for c in b)


def si32(v):
    return ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000


# --------------------------------------------------------------------------
# vectors
# --------------------------------------------------------------------------
NAMES = ["", "a", "Bob", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "Zzz 9",
         "player_one", "  ", "\x01\xff\x7f"]


def gen_posts(rnd):
    """One 180-byte Thisc[5] image.  Deliberately over-weighted towards the
    shapes enter_hisc_table/sort_hisc_table actually meet: already sorted
    descending, partly filled with trailing zeroes, all-equal, and
    saturated at or above the 10 000 000 threshold."""
    shape = rnd.randrange(0, 6)
    if shape == 0:                                  # sorted descending
        vals = sorted((rnd.randrange(0, 200000) for _ in range(5)), reverse=True)
    elif shape == 1:                                # partly filled
        k = rnd.randrange(0, 6)
        vals = sorted((rnd.randrange(1, 200000) for _ in range(k)), reverse=True)
        vals += [0] * (5 - k)
    elif shape == 2:                                # all equal
        vals = [rnd.randrange(0, 200000)] * 5
    elif shape == 3:                                # saturated
        vals = [rnd.choice([10000000, 10000001, 0xFFFFFFFF]) for _ in range(5)]
    elif shape == 4:                                # boundary values
        vals = [rnd.choice([0, 1, 9999999, 10000000, 10000001, 0x7FFFFFFF,
                            0x80000000, 0xFFFFFFFF]) for _ in range(5)]
    else:                                           # unsorted random
        vals = [rnd.randrange(0, 0x100000000) for _ in range(5)]
    out = b""
    for i, v in enumerate(vals):
        nm = rnd.choice(NAMES).encode("latin1")[:31]
        out += nm + b"\0" * (32 - len(nm)) + u32(v)
    return out, vals


def gen_replay(rnd, big):
    """A 0x8ac-byte Treplay image plus its record array."""
    def i32():
        return rnd.choice([0, 1, -1, rnd.randrange(-1000, 100000),
                           rnd.randrange(-(1 << 31), 1 << 31)])
    b = bytearray(REPLAY_BYTES)
    b[0:6] = b"ITR15\0"
    size = rnd.choice([0, 1, 2, 3, 17, rnd.randrange(0, 200 if big else 40)])
    struct.pack_into("<i", b, 0x08, size)
    for off in range(0x0c, 0x4c):
        b[off] = rnd.randrange(0, 256)
    for off in (0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60):
        struct.pack_into("<i", b, off, i32())
    for k in range(5):
        struct.pack_into("<i", b, 0x64 + 4 * k, i32())
        struct.pack_into("<i", b, 0x78 + 4 * k, i32())
    for off in (0x8c, 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4):
        struct.pack_into("<i", b, off, i32())
    for off in range(0xa8, 0xd2):
        b[off] = rnd.randrange(0, 256)
    struct.pack_into("<i", b, 0xd4, i32())
    # the five float columns; only c/q/t are hashed, s/f are noise on
    # purpose so that hashing them by mistake would show up
    for col in range(5):
        base = 0xd8 + 400 * col
        for k in range(100):
            f = rnd.choice([0.0, -0.0, 1.0, -1.0, 50.0, 49.94,
                            rnd.uniform(-1e6, 1e6), rnd.uniform(-1.0, 1.0),
                            float(rnd.randrange(-100000, 100000))])
            struct.pack_into("<f", b, base + 4 * k, f)
    recs = b""
    for k in range(size):
        recs += bytes([rnd.randrange(0, 256), 0, 0, 0]) + struct.pack("<i", i32())
    return bytes(b), recs, size


def gen(rnd, kind):
    if kind == "qualify_hisc_table":
        posts, _ = gen_posts(rnd)
        value = rnd.choice([0, 1, -1, rnd.randrange(0, 300000),
                            rnd.randrange(-(1 << 31), 1 << 31)])
        return K_QUALIFY, posts + u32(value), (posts, value)
    if kind == "sort_hisc_table":
        posts, _ = gen_posts(rnd)
        return K_SORT, posts, (posts,)
    if kind == "enter_hisc_table":
        posts, _ = gen_posts(rnd)
        value = rnd.choice([0, 1, rnd.randrange(0, 300000),
                            rnd.randrange(-(1 << 31), 1 << 31)])
        nm = rnd.choice(NAMES).encode("latin1")[:31]
        nm = nm + b"\0" * (32 - len(nm))
        return K_ENTER, posts + u32(value) + nm, (posts, value, nm)
    if kind == "get_rank":
        tabs = b""
        for _ in range(4):
            base = rnd.randrange(0, 100)
            step = rnd.randrange(0, 500)
            vals = [base + step * i for i in range(12)]
            if rnd.random() < 0.25:
                vals = [rnd.randrange(-1000, 100000) for _ in range(12)]
            tabs += struct.pack("<12i", *vals)
        fields = [rnd.choice([0, -1, rnd.randrange(0, 6000),
                              rnd.randrange(-(1 << 31), 1 << 31)])
                  for _ in range(4)]
        return K_RANK, tabs + struct.pack("<4i", *fields), (tabs, fields)
    if kind == "hash":
        v = rnd.choice([0, 1, 0xFFFFFFFF, 61, rnd.randrange(0, 1 << 32)])
        return K_HASH, u32(v), (v,)
    if kind in ("calc_replay_checksum_131", "calc_replay_checksum"):
        rb, recs, size = gen_replay(rnd, kind == "calc_replay_checksum")
        k = K_CS131 if kind.endswith("131") else K_CS
        return k, rb + recs, (rb, recs, size)
    raise RuntimeError(kind)


KINDS = ["qualify_hisc_table", "sort_hisc_table", "enter_hisc_table",
         "get_rank", "hash", "calc_replay_checksum_131",
         "calc_replay_checksum"]


def original_result(orig, kind, parts):
    mu = orig.mu
    if kind == "qualify_hisc_table":
        posts, value = parts
        mu.mem_write(S_POSTS, posts)
        mu.mem_write(S_TABLE + 0x20, u32(S_POSTS))
        eax = orig.call(FN[kind], [S_TABLE, value])
        return ["ret %d" % si32(eax),
                "posts " + hexs(bytes(mu.mem_read(S_POSTS, POSTS_BYTES)))]
    if kind == "sort_hisc_table":
        (posts,) = parts
        mu.mem_write(S_POSTS, posts)
        mu.mem_write(S_TABLE + 0x20, u32(S_POSTS))
        orig.call(FN[kind], [S_TABLE])
        return ["posts " + hexs(bytes(mu.mem_read(S_POSTS, POSTS_BYTES)))]
    if kind == "enter_hisc_table":
        posts, value, nm = parts
        mu.mem_write(S_POSTS, posts)
        mu.mem_write(S_TABLE + 0x20, u32(S_POSTS))
        mu.mem_write(S_NAME, nm[:31] + b"\0")
        orig.call(FN[kind], [S_TABLE, value, S_NAME])
        return ["posts " + hexs(bytes(mu.mem_read(S_POSTS, POSTS_BYTES)))]
    if kind == "get_rank":
        tabs, fields = parts
        mu.mem_write(G_RANK_FLOORS, tabs[0:48])
        mu.mem_write(G_RANK_COMBOS, tabs[48:96])
        mu.mem_write(G_RANK_NMLS, tabs[96:144])
        mu.mem_write(G_RANK_CCCS, tabs[144:192])
        mu.mem_write(S_PROFILE, b"\0" * PROFILE_BYTES)
        struct.pack_into  # (no-op: writes below are explicit)
        mu.mem_write(S_PROFILE + 0x4c, struct.pack("<i", fields[0]))
        mu.mem_write(S_PROFILE + 0x50, struct.pack("<i", fields[1]))
        mu.mem_write(S_PROFILE + 0x58, struct.pack("<i", fields[2]))
        mu.mem_write(S_PROFILE + 0x88, struct.pack("<i", fields[3]))
        rid = si32(orig.call(FN["get_rank_id"], [S_PROFILE]))
        lbl = orig.call(FN["get_rank"], [S_PROFILE])
        return ["rank_id %d" % rid, 'rank "%s"' % orig.gstr(lbl, 64)]
    if kind == "hash":
        (v,) = parts
        return ["ret %u" % (orig.call(FN[kind], [v]) & 0xFFFFFFFF)]
    if kind in ("calc_replay_checksum_131", "calc_replay_checksum"):
        rb, recs, size = parts
        mu.mem_write(S_REPLAY, rb)
        mu.mem_write(S_REPLAY + 0x8a8, u32(S_RECS))
        if recs:
            mu.mem_write(S_RECS, recs)
        return ["ret %d" % si32(orig.call(FN[kind], [S_REPLAY]))]
    raise RuntimeError(kind)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vectors", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--only", default=None)
    ap.add_argument("--fault", action="store_true",
                    help="negative control: corrupt one ORIGINAL-side fact "
                         "at vector 5 of each kind and require a DIFFER")
    args = ap.parse_args()

    kinds = [args.only] if args.only else KINDS
    per = 40 if args.fault else max(1, args.vectors // len(kinds))

    rnd = random.Random(args.seed)
    orig = Original()

    recs, expected = [], []
    for kind in kinds:
        for k in range(per):
            knum, payload, parts = gen(rnd, kind)
            recs.append(struct.pack("<II", knum, len(payload)) + payload)
            expected.append((kind, k, parts))

    with open(VECFILE, "wb") as f:
        f.write(b"".join(recs))

    out = subprocess.check_output([CHECK_EXE, VECFILE]).decode("latin1")
    blocks, cur = [], None
    for ln in out.replace("\r\n", "\n").split("\n"):
        if ln.startswith("V "):
            cur = []
        elif ln == "E":
            blocks.append(cur)
            cur = None
        elif cur is not None and ln:
            cur.append(ln)
    if len(blocks) != len(expected):
        print("PROTOCOL: %d candidate blocks for %d vectors"
              % (len(blocks), len(expected)))
        sys.exit(1)

    fails = {k: 0 for k in kinds}
    shown = 0
    for i, (kind, k, parts) in enumerate(expected):
        got = original_result(orig, kind, parts)
        cand = list(blocks[i])
        if args.fault and k == 5:
            got = list(got)
            got[0] = got[0] + "FAULT"
        if got != cand:
            fails[kind] += 1
            if shown < 8:
                shown += 1
                print("  DIFFER %s vector %d" % (kind, k))
                for a, b in zip(got + [""] * 8, cand + [""] * 8):
                    if a != b:
                        print("    original:  %s" % a)
                        print("    candidate: %s" % b)
                        break
    bad = 0
    for kind in kinds:
        print("%-26s %d of %d vectors differ" % (kind, fails[kind], per))
        if args.fault:
            if fails[kind] == 0:
                print("NEGATIVE CONTROL FAILED: %s fault not detected" % kind)
                bad = 1
        else:
            bad |= (1 if fails[kind] else 0)
    sys.exit(bad)


if __name__ == "__main__":
    main()
