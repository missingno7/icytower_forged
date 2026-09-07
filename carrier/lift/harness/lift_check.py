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

G_REWARD_TIME = 0x4fec68
G_REWARD_SCALE = 0x4fac28
G_PLAYER_ID = 0x4fe518
G_PLY = 0x4ff128
G_LOGIC_COUNT = 0x506958
G_COLLISION_TYPE = 0x4dd140
G_MAX_SPEED = 0x4bdb80

SZ_PLAYER = 184
SZ_MAP = 772


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

    def restore(self, regions):
        for va, n in regions:
            off = va - GUEST_BASE
            self.mu.mem_write(va, self.guest[off:off + n])

    def call(self, va, args, writes, domain):
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
    ap.add_argument("--exe", default=os.path.join(HERE, "lift_check.exe"))
    ap.add_argument("--funcs", default="update_frame,is_solid,jump_player")
    ap.add_argument("--vectors", type=int, default=0,
                    help="vectors per function (0 = per-function default)")
    ap.add_argument("--seed", type=int, default=20260907)
    ap.add_argument("--fault", default=None,
                    help="negative control: FUNC:VECTOR:BYTE -- flip one bit of "
                         "the LIFTED result and require the comparator to name it")
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

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
        nvec = args.vectors or (4000 if name == "jump_player" else 1500)
        rng = random.Random(args.seed + sum(ord(c) for c in name))
        vectors = [spec["gen"](rng, k) for k in range(nvec)]
        vpath = os.path.join(HERE, "vectors_%s.bin" % name)
        rpath = os.path.join(HERE, "results_%s.bin" % name)
        write_vectors(vpath, spec["domain"], vectors)

        r = subprocess.run([args.exe, gpath, vpath, rpath, name],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("[%s] LIFTED side failed: %s%s" % (name, r.stdout, r.stderr))
            report[name] = {"result": "LIFTED_SIDE_FAILED", "detail": r.stderr.strip()}
            rc = 1
            continue
        domlen = sum(n for _, n in spec["domain"])
        lifted = read_results(rpath, nvec, domlen)

        oracle = Oracle(guest)
        restore = list(spec["domain"]) + [(va, len(b)) for _, w in vectors for va, b in w]
        restore = sorted(set(restore))
        first_diff = None
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
                first_diff = {"vector": k, "kind": "return value",
                              "original": "0x%08x" % oeax, "lifted": "0x%08x" % leax,
                              "args": ["0x%08x" % x for x in a]}
                break
            if odom != ldom:
                for bidx in range(len(odom)):
                    if odom[bidx] != ldom[bidx]:
                        first_diff = {"vector": k, "kind": "comparison domain",
                                      "at": dom_locate(spec, bidx),
                                      "byte_index": bidx,
                                      "original": "0x%02x" % odom[bidx],
                                      "lifted": "0x%02x" % ldom[bidx],
                                      "args": ["0x%08x" % x for x in a]}
                        break
                break

        if first_diff is None:
            print("[%s] EQUAL over %d vectors (%d domain bytes + %s)"
                  % (name, nvec, domlen, "EAX" if spec["cmp_eax"] else "no return value"))
            report[name] = {"result": "EQUAL", "vectors": nvec,
                            "domain_bytes": domlen,
                            "compared_return_value": spec["cmp_eax"]}
        else:
            print("[%s] DIFFER at vector %d: %s" % (name, first_diff["vector"], first_diff))
            report[name] = {"result": "DIFFER", "vectors": nvec, "first": first_diff}
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
