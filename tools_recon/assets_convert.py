"""assets_convert.py - convert Allegro datafile objects to normal formats and
prove the conversion is lossless by re-encoding and comparing bytes.

DAT_BITMAP payload: uint16 bpp, uint16 w, uint16 h, then h rows of w pixels.
For bpp==16 each pixel is a little-endian RGB565 word.  RGB565 -> RGB888 with
the usual (v<<3)|(v>>2) / (v<<2)|(v>>4) expansion is injective, so a 24-bit PNG
round-trips to the identical payload.

DAT_PALETTE payload: 256 * (r,g,b,filler) with 6-bit components (Allegro RGB).

Usage:
  python tools_recon/assets_convert.py bmp <dat> <objname> <out.png>
  python tools_recon/assets_convert.py pal <dat> <objname> <out.pal.txt>
  python tools_recon/assets_convert.py font <dat> <objname>
"""
import sys, os, struct, zlib, hashlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from assets_datafile import open_packfile, parse_datafile, flatten, PASSWORDS


def load(path, name):
    pw = PASSWORDS.get(os.path.basename(path).lower())
    body, info = open_packfile(path, pw)
    for o in flatten(parse_datafile(body)):
        if o["path"] == name or o["name"] == name:
            return o
    raise KeyError(name)


def png_write(path, w, h, rgb_rows):
    raw = b"".join(b"\x00" + r for r in rgb_rows)
    def chunk(t, d):
        c = t + d
        return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c))
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    out += chunk(b"IEND", b"")
    open(path, "wb").write(out)


SBYTES = {8: 1, 15: 2, 16: 2, 24: 3, 32: 3}


def bmp_to_png(data, out, pal=None):
    """Allegro DAT_BITMAP -> 24-bit PNG, verified by re-encoding.

    Storage is 1 byte/px at 8bpp (paletted, 6-bit RGB palette), 2 bytes at
    15/16bpp (little-endian 565), 3 bytes at 24 and 32bpp (B,G,R, no alpha).
    """
    bpp, w, h = struct.unpack(">HHH", data[:6])
    sb = SBYTES[bpp]
    px = data[6:]
    assert len(px) == w * h * sb, "payload %d != %d" % (len(px), w * h * sb)
    rows = []
    for y in range(h):
        row = bytearray()
        base = y * w * sb
        for x in range(w):
            o = base + x * sb
            if bpp == 8:
                i = px[o]
                if pal:
                    r, g, b = pal[i * 4] * 255 // 63, pal[i * 4 + 1] * 255 // 63, pal[i * 4 + 2] * 255 // 63
                else:
                    r = g = b = i
                row += bytes((r, g, b))
            elif bpp in (15, 16):
                v = px[o] | (px[o + 1] << 8)
                r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
                row += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
            else:
                row += bytes((px[o + 2], px[o + 1], px[o]))
        rows.append(bytes(row))
    png_write(out, w, h, rows)
    back = bytearray(struct.pack(">HHH", bpp, w, h))
    for row in rows:
        for x in range(w):
            r, g, b = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]
            if bpp in (15, 16):
                back += struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
            elif bpp == 8:
                back += bytes((0,))  # index not recoverable from RGB
            else:
                back += bytes((b, g, r))
    ok = bytes(back) == data if bpp != 8 else None
    return ok, w, h, bpp


bmp16_to_png = bmp_to_png


def main():
    mode, path, name = sys.argv[1], sys.argv[2], sys.argv[3]
    o = load(path, name)
    d = o["data"]
    if mode == "bmp":
        pal = None
        if len(sys.argv) > 5:
            pal = load(path, sys.argv[5])["data"]
        ok, w, h, bpp = bmp_to_png(d, sys.argv[4], pal)
        print("%s %dx%d %dbpp -> %s  round-trip identical: %s" % (
            name, w, h, bpp, sys.argv[4],
            ok if ok is not None else "n/a (8bpp indices need the palette)"))
    elif mode == "pal":
        lines = []
        for i in range(256):
            r, g, b, f = d[i * 4:i * 4 + 4]
            lines.append("%3d %2d %2d %2d %2d   #%02x%02x%02x" % (
                i, r, g, b, f, r * 255 // 63, g * 255 // 63, b * 255 // 63))
        open(sys.argv[4], "w").write("\n".join(lines) + "\n")
        print("palette ->", sys.argv[4], "entries 256, 6-bit components,",
              "non-zero filler bytes:", sum(1 for i in range(256) if d[i * 4 + 3]))
    elif mode == "font":
        print(name, "size", len(d), "head", d[:32].hex(" "))
        h = struct.unpack(">h", d[:2])[0]
        print("  first short (height/marker) =", h)
        print("  next dwords:", [struct.unpack(">i", d[2 + i * 4:6 + i * 4])[0] for i in range(4)])


if __name__ == "__main__":
    main()
