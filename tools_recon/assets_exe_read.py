"""assets_exe_read.py - read bytes/strings from icytower15.exe by virtual address.

Usage:
  python tools_recon/assets_exe_read.py str 0x4d55b5 [0x4d579b ...]
  python tools_recon/assets_exe_read.py hex 0x4bdb3c 80
"""
import sys, os
import pefile

EXE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "assets", "icytower15.exe")
_pe = None


def pe():
    global _pe
    if _pe is None:
        _pe = pefile.PE(EXE, fast_load=True)
    return _pe


def read(va, n):
    return pe().get_data(va - pe().OPTIONAL_HEADER.ImageBase, n)


def cstr(va, maxn=512):
    b = read(va, maxn)
    i = b.find(b"\x00")
    return b[:i] if i >= 0 else b


def main():
    mode = sys.argv[1]
    if mode == "str":
        for a in sys.argv[2:]:
            va = int(a, 0)
            print("%08x %r" % (va, cstr(va)))
    elif mode == "hex":
        va = int(sys.argv[2], 0)
        n = int(sys.argv[3], 0)
        b = read(va, n)
        for off in range(0, len(b), 16):
            ch = b[off:off + 16]
            print("%08x  %-47s  %s" % (va + off, ch.hex(" "),
                  "".join(chr(c) if 32 <= c < 127 else "." for c in ch)))
    elif mode == "raw":
        va = int(sys.argv[2], 0)
        n = int(sys.argv[3], 0)
        sys.stdout.buffer.write(read(va, n))


if __name__ == "__main__":
    main()
