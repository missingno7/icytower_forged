#!/usr/bin/env python3
"""x87_cw_probe.py -- why the x87 hypothesis has an answer at all.

Standalone (it shares nothing with lift_check.py but the image): runs the
ORIGINAL bytes of line_intersect in unicorn on a handful of hand-constructed
vectors, once for each x87 PRECISION-CONTROL setting, and reports which of the
two lifted models the hardware agrees with.

    CW = 0x037F   PC = 11  extended, 64-bit significand   <- FNINIT default
    CW = 0x027F   PC = 10  double,   53-bit significand   <- MSVC/CRT default
    CW = 0x007F   PC = 00  single,   24-bit significand   <- unicorn power-on

KNOWN: Icy Tower's ___mingw_CRTStartup (0x401020) calls __fpreset (0x4b2850),
which is a bare FNINIT, so the game runs at 0x037F and nothing in the game
changes the PC field (every FLDCW in the image is one half of GCC's
save / set-RC-to-truncate / FISTP / restore idiom).  0x037F is therefore the
control word `lift_check.py` gives the oracle and `PF_CW_INIT` gives the
lifted code.

The vectors are built so that ua*d + 0.5 is EXACTLY an integer, which puts the
FISTP truncation exactly on a boundary; the two models then land on opposite
sides of it.  See artifacts/lift_x87_finding.md.
"""
import os
import struct
import sys

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_FPCW

HERE = os.path.dirname(os.path.abspath(__file__))
BASE, SIZE = 0x400000, 0x400000
STACK, STACK_SZ = 0x00100000, 0x00100000
RET = STACK + 0x10
CW_SLOT, CW_STUB = STACK + 0x20, STACK + 0x30
OUT_X, OUT_Y = 0x794000, 0x794004
VA = 0x406b80

# (D, dx1, N1) with dy1 = -D, segment 3-4 = (0,0)-(1,0); px chosen so ub is in
# range.  x_80 / x_64 are the *px results the 80-bit and the double models give.
CASES = [
    (1484783616, 986972160, 390913232, 136, 259849632, 259849633),
    (907542528, 406585344, 35554163, 54, 15928511, 15928512),
    (633902080, 442958080, 184970646, 879423, 129253783, 129253784),
    (729808896, 1019215872, 381243889, 377, 532426810, 532426811),
    (1659322368, 30873600, 888671916, 253040, 16534763, 16534762),
    (1285029888, 1013710848, 671775799, 1854, 529938192, 529938191),
]


def w32(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v >= (1 << 31) else v


def main():
    img = os.path.join(HERE, "..", "..", "..", "assets", "icytower15.exe")
    pe = pefile.PE(img, fast_load=True)
    mm = pe.get_memory_mapped_image(ImageBase=BASE)
    buf = bytearray(SIZE)
    buf[0:len(mm)] = mm[:SIZE]

    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(BASE, SIZE)
    mu.mem_write(BASE, bytes(buf))
    mu.mem_map(STACK, STACK_SZ)
    mu.mem_write(RET, b"\xF4")
    stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + struct.pack("<I", CW_SLOT)
    mu.mem_write(CW_STUB, stub)
    print("unicorn power-on FPCW = 0x%04x" % mu.reg_read(UC_X86_REG_FPCW))

    for cw in (0x037F, 0x027F, 0x007F):
        agree = {"80-bit": 0, "double": 0, "neither": 0}
        for (D, dx1, n1, px, x80, x64) in CASES:
            mu.mem_write(CW_SLOT, struct.pack("<H", cw))
            mu.emu_start(CW_STUB, CW_STUB + len(stub))
            x1, y1 = px, n1
            args = [x1, y1, w32(x1 + dx1), w32(y1 - D), 0, 0, 1, 0, OUT_X, OUT_Y]
            esp = STACK + STACK_SZ - 0x1000
            for a in reversed(args):
                esp -= 4
                mu.mem_write(esp, struct.pack("<I", a & 0xFFFFFFFF))
            esp -= 4
            mu.mem_write(esp, struct.pack("<I", RET))
            mu.reg_write(UC_X86_REG_ESP, esp)
            mu.emu_start(VA, RET, count=1000000)
            got = struct.unpack("<i", bytes(mu.mem_read(OUT_X, 4)))[0]
            if got == w32(x1 + x80):
                agree["80-bit"] += 1
            elif got == w32(x1 + x64):
                agree["double"] += 1
            else:
                agree["neither"] += 1
        print("CW = 0x%04X  (PC = %2d bits) -> matches 80-bit model %d/%d, "
              "double model %d/%d, neither %d/%d"
              % (cw, {0: 24, 2: 53, 3: 64}[(cw >> 8) & 3],
                 agree["80-bit"], len(CASES), agree["double"], len(CASES),
                 agree["neither"], len(CASES)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
