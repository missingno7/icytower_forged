#!/usr/bin/env python3
"""batch15_check.py -- PURE oracle for PROMOTIONS.md batch 15's one
genuinely pure function:

    key_to_str   0x416a9c   2543 B   src/icytower/menu_keys.c

Why this is not a lift_check.py SPECS entry
-------------------------------------------
Mechanism B compares a fixed-size scalar result; this function's result
is a STRING WRITTEN THROUGH A POINTER, and the interesting half of the
claim is what it did NOT write (see the canary below).  Same reason
batches 10/12/13/14 gave.  icytower_specs.py is untouched again -- the
sixth batch running that way.

How it works
------------
1. The ORIGINAL bytes run under unicorn on the shared
   port_forge/tools/pf_win32_offline_oracle engine.  NO hooks at all:
   the function makes no calls (a `call` census over its whole
   0x416a9c..0x41748a range finds none), so there is nothing to emulate.
2. The compiled candidate is batch15_check.exe, driven by a text vector
   file -- one decimal scancode per line, one process for the whole run.
3. Domain: the destination buffer's CONTENT and length, PLUS a 64-byte
   0xA5 canary on each side of it, all three compared.  A 108-branch
   dispatch that writes string literals is exactly the shape where the
   wrong branch still produces plausible output, and where an off-by-one
   literal length is invisible unless the bytes after the NUL are
   checked too.

Vector plan
-----------
The input is a scancode, and there are only 0x7f meaningful ones, so
this oracle does NOT sample: it enumerates EVERY value in
[-256, 512] on every run (769 vectors, covering all 108 branches, all
nineteen unhandled scancodes inside KEY_MAX, both signs and both
boundaries), and spends the rest of --vectors on random 32-bit ints to
prove the default arm is really the default.  Branch coverage here is
therefore measured by construction, not argued.

Usage
-----
  python batch15_check.py --vectors 20000 --seed 20260908
  python batch15_check.py --fault
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
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32           # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP  # noqa: E402

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")
CHECK_EXE = os.path.join(HERE, "batch15_check.exe")
VECFILE = os.path.join(HERE, "batch15_vectors.txt")

FN_KEY_TO_STR = 0x416a9c

S_BLOCK = 0x600000          # 192-byte canary block; dest is +64
GUARD = 64
BLOCK = GUARD + 64 + GUARD


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


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
        mu.emu_start(va, pf.RET_MAGIC, count=4000000)
        if mu.reg_read(UC_X86_REG_EIP) != pf.RET_MAGIC:
            raise RuntimeError("did not return")

    def run(self, k):
        mu = self.mu
        mu.mem_write(S_BLOCK, b"\xA5" * BLOCK)
        self.call(FN_KEY_TO_STR, [k & 0xFFFFFFFF, S_BLOCK + GUARD])
        blk = bytes(mu.mem_read(S_BLOCK, BLOCK))
        s = blk[GUARD:]
        z = s.find(b"\0")
        txt = s[:z] if z >= 0 else s
        return ['str "%s"' % txt.decode("latin1"),
                "len %d" % len(txt),
                "block " + "".join("%02x" % c for c in blk)]


def gen_vectors(rnd, n):
    vals = list(range(-256, 513))
    while len(vals) < n:
        vals.append(rnd.randrange(-(1 << 31), 1 << 31))
    return vals


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vectors", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--fault", action="store_true",
                    help="negative control: corrupt one ORIGINAL-side fact "
                         "at vector 5 and require a DIFFER")
    args = ap.parse_args()

    n = 40 if args.fault else max(769, args.vectors)
    rnd = random.Random(args.seed)
    vals = gen_vectors(rnd, n)

    with open(VECFILE, "w", newline="\n") as f:
        f.write("\n".join(str(v) for v in vals) + "\n")

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
    if len(blocks) != len(vals):
        print("PROTOCOL: %d candidate blocks for %d vectors"
              % (len(blocks), len(vals)))
        sys.exit(1)

    orig = Original()
    fails = 0
    shown = 0
    for i, k in enumerate(vals):
        got = orig.run(k)
        cand = list(blocks[i])
        if args.fault and i == 5:
            got = list(got)
            got[0] = got[0] + "FAULT"
        if got != cand:
            fails += 1
            if shown < 8:
                shown += 1
                print("  DIFFER key %d" % k)
                for a, b in zip(got + [""] * 4, cand + [""] * 4):
                    if a != b:
                        print("    original:  %s" % a[:200])
                        print("    candidate: %s" % b[:200])
                        break
    print("%-16s %d of %d vectors differ" % ("key_to_str", fails, len(vals)))
    if args.fault:
        if fails == 0:
            print("NEGATIVE CONTROL FAILED: key_to_str fault not detected")
            sys.exit(1)
        sys.exit(0)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
