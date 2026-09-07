#!/usr/bin/env python3
"""x87_soft_selftest.py -- check lifted/pf_x87_soft.h against exact arithmetic.

Runs harness/x87_soft_selftest.exe (built by build_soft.cmd), recomputes every
add / sub / mul / div it printed with Python Fractions, rounds the exact result
to nearest-even at 64 significand bits, and requires the packed (sign, biased
exponent, significand) triple to match bit for bit.

This is deliberately independent of the unicorn oracle: it tests the backend
against the *definition* of x87 extended precision, so a failure here is a
softfloat bug and not an oracle disagreement.
"""
import os
import struct
import subprocess
import sys
from fractions import Fraction as F

HERE = os.path.dirname(os.path.abspath(__file__))


def d2frac(u):
    """exact value of a finite, normal IEEE double bit pattern"""
    s = -1 if u >> 63 else 1
    e = (u >> 52) & 0x7FF
    m = u & ((1 << 52) - 1)
    if e == 0:
        return s * F(m, 1) * F(2) ** -1074
    return s * F((1 << 52) | m, 1) * F(2) ** (e - 1075)


def round64(x):
    """-> (sign, biased exponent, 64-bit significand), nearest-even"""
    s = 1 if x > 0 else -1
    x = abs(x)
    e = x.numerator.bit_length() - x.denominator.bit_length() - 64
    two = F(2)
    while x / two ** e >= 1 << 64:
        e += 1
    while x / two ** e < 1 << 63:
        e -= 1
    m = x / two ** e
    fl = m.numerator // m.denominator
    rem = m - fl
    if rem > F(1, 2) or (rem == F(1, 2) and fl % 2 == 1):
        fl += 1
    if fl == 1 << 64:
        fl >>= 1
        e += 1
    return (0 if s > 0 else 1, e + 63 + 16383, int(fl))


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "x87_soft_selftest.exe")
    n = sys.argv[2] if len(sys.argv) > 2 else "40000"
    out = subprocess.run([exe, n], capture_output=True, text=True)
    if out.returncode != 0:
        print("selftest binary failed: %s%s" % (out.stdout, out.stderr))
        return 2
    checked = bad = 0
    for line in out.stdout.splitlines():
        op, au, bu, se, m = line.split()
        op, au, bu = int(op), int(au, 16), int(bu, 16)
        se, m = int(se, 16), int(m, 16)
        a, b = d2frac(au), d2frac(bu)
        ex = (a + b, a - b, a * b, (a / b if b != 0 else None))[op]
        if ex is None:
            continue
        checked += 1
        if ex == 0:
            ok = (se & 0x7FFF) == 0 and m == 0
        else:
            s, be, mm = round64(ex)
            ok = ((se >> 15) & 1) == s and (se & 0x7FFF) == be and m == mm
        if not ok:
            bad += 1
            if bad <= 3:
                print("MISMATCH op=%d a=%016x b=%016x got se=%04x m=%016x want %r"
                      % (op, au, bu, se, m, round64(ex)))
    print("%s: %d operations checked, %d mismatches"
          % ("PASS" if bad == 0 else "FAIL", checked, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
