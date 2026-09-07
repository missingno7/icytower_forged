#!/usr/bin/env python3
"""asset_oracle_digest.py -- the PYTHON half of the cross-world asset oracle
(src/icytower/ASSETS.md "Extraction and the asset oracle", src/build/
asset_oracle.c's own header comment for the full derivation). Reads
assets_extracted/manifest.json (scripts/extract_assets.py's output -- a
LOCAL CACHE, never committed) and, for every id, reconstructs the SAME
canonical serialization asset_oracle.c hashes from a REAL Allegro
load_datafile() call -- working only from the extracted files, never
reopening the original datafile. Prints one line per id in the same
`<index> <object_name> <sha256hex>` shape asset_oracle.c's stdout uses, so
the two outputs diff directly:

    python scripts/extract_assets.py
    ( cd assets && /path/to/asset_oracle.exe ) > oracle.txt
    python scripts/asset_oracle_digest.py > python_digest.txt
    diff oracle.txt python_digest.txt

BITMAP/PALETTE/info reconstruct exactly (see the per-type notes below and
asset_oracle.c's header comment for the byte-level derivation from
third_party/allegro-4.4.3.1/src/datafile.c). SAMPLE (this project's 53
OGG-typed sound objects) is print with a `UNPROVEN` marker instead of a
hash: the real oracle decodes actual Ogg Vorbis audio via libvorbis, and
reproducing that bit-for-bit from the .ogg file would need a bit-exact
Vorbis decoder. A quick empirical check (ffmpeg's own Vorbis decoder
against one sample) produced a DIFFERENT PCM stream than the real
libvorbis-backed oracle -- expected (independent Vorbis implementations are
not bit-deterministic with each other), and exactly why this script does
not claim OGG/SAMPLE equality rather than silently comparing a mismatching
decode.
"""
import argparse
import json
import os
import struct
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
TOOLS_DIR = os.path.join(ROOT, "port_forge", "tools")
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_allegro4_datafile as datafile_tool  # noqa: E402

DEFAULT_MANIFEST = os.path.join(ROOT, "assets_extracted", "manifest.json")

# in-memory Allegro bytes-per-pixel -- NOTE 32bpp differs from the on-disk 3
# (see asset_oracle.c: the loader always builds a real alpha byte, value 0
# since these datafiles store no alpha).
MEM_BYTES_PER_PIXEL = {8: 1, 15: 2, 16: 2, 24: 3, 32: 4}


def _u16be(v):
    return struct.pack(">H", v & 0xFFFF)


def _u32be(v):
    return struct.pack(">I", v & 0xFFFFFFFF)


def bitmap_mem_bytes_from_png(png_path, bpp):
    """The exact bytes asset_oracle.c's serialize_bitmap() hashes for a BMP
    object, reconstructed from its exported PNG.

    EMPIRICALLY CALIBRATED, not purely derived from reading datafile.c: a
    from-source derivation predicted 24/32bpp memory bytes would be the
    on-disk (B,G,R) triplet BYTE-REVERSED (Allegro's makecol24/WRITE3BYTES
    repacking through the _rgb_r/g/b_shift_24/32 globals a real graphics
    driver sets), which is what a GDI driver's own shift assignment
    (win/gdi.c: R=16,G=8,B=0) would produce. Actually building and running
    src/build/asset_oracle.c against this project's real assets/ (see
    src/icytower/ASSETS.md) showed the DISK bytes reproduced UNCHANGED
    instead -- meaning whichever real driver GFX_AUTODETECT_WINDOWED
    actually installed on the build host set the OPPOSITE shift convention
    for 24/32bpp (R=0,G=8,B=16) from what win/gdi.c hardcodes, while still
    matching the on-disk R-high convention for 16bpp (there the two
    predictions agree, and both matched immediately). This is the kind of
    driver/host-dependent fact only an actual run can settle -- see
    asset_oracle.c's header comment for the pointer to the empirical
    result. 8bpp is disk-verbatim (a raw index copy, no makecolNN call at
    all -- never in question). 16bpp is re-quantized from the PNG's
    expanded RGB888 back to 565, which reproduces the disk/memory value
    exactly regardless of shift convention (expand-then-requantize at
    matching bit depths is its own inverse). 24bpp reverses each pixel
    triplet from the PNG's own (already once-reversed, see
    port_forge/tools/pf_allegro4_datafile.py's bmp_payload_to_png) row back
    to on-disk order. 32bpp is the same reversal with a 0x00 alpha byte
    appended per pixel (these datafiles store no alpha)."""
    w, h, color_type, rows, _palette = datafile_tool.png_read(png_path)
    out = bytearray(struct.pack(">HHH", bpp, w, h))
    if bpp == 8:
        for row in rows:
            out += row
        return bytes(out)
    for row in rows:
        if bpp in (15, 16):
            for x in range(w):
                r, g, b = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]
                out += struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
        elif bpp == 24:
            for x in range(w):
                out += bytes((row[x * 3 + 2], row[x * 3 + 1], row[x * 3]))
        else:  # 32bpp
            for x in range(w):
                out += bytes((row[x * 3 + 2], row[x * 3 + 1], row[x * 3], 0))
    return bytes(out)


def _bitmap_body_mem_bytes(buf, pos, bpp, w, h):
    """Parse a raw Allegro BMP pixel body (bpp already known, no leading
    bpp/w/h header -- this is what a FONT color-glyph range stores, per
    read_font_color()/read_bitmap() in datafile.c) starting at `pos` inside
    a raw .alfont payload, returning (mem_bytes, new_pos) in the SAME
    canonical form as bitmap_mem_bytes_from_png -- reading directly from the
    on-disk stream this time, memory bytes equal the disk bytes VERBATIM at
    every bpp this project ships (8/16/24bpp identically; 32bpp with a 0x00
    alpha byte appended per pixel), per the empirical result described in
    bitmap_mem_bytes_from_png's own docstring."""
    disk_bpp = {8: 1, 15: 2, 16: 2, 24: 3, 32: 3}[bpp]
    npix = w * h
    body = buf[pos:pos + npix * disk_bpp]
    pos += npix * disk_bpp
    if bpp != 32:
        return bytes(body), pos
    out = bytearray()
    for i in range(npix):
        o = i * disk_bpp
        out += bytes((body[o], body[o + 1], body[o + 2], 0))
    return bytes(out), pos


def font_mem_bytes(alfont_path):
    """The exact bytes asset_oracle.c's serialize_font() hashes, parsed
    directly from the raw .alfont payload -- a faithful, lossless
    transcription of read_font()/read_font_mono()/read_font_color() in
    third_party/allegro-4.4.3.1/src/datafile.c (not a guess: every field
    width/order below was read off that source)."""
    buf = Path(alfont_path).read_bytes()
    pos = 0
    height = struct.unpack(">h", buf[pos:pos + 2])[0]
    pos += 2
    if height != 0:
        raise NotImplementedError("legacy fixed/proportional font format not supported")

    out = bytearray()
    nranges = struct.unpack(">H", buf[pos:pos + 2])[0]
    pos += 2
    out += _u16be(nranges)
    for _r in range(nranges):
        depth = buf[pos]
        pos += 1
        begin = struct.unpack(">i", buf[pos:pos + 4])[0]
        pos += 4
        last = struct.unpack(">i", buf[pos:pos + 4])[0]
        pos += 4
        mono = depth in (1, 255)
        out += b"M" if mono else b"C"
        out += _u32be(begin)
        out += _u32be(last)
        bpp = 8 if depth == 0 else depth
        for _ch in range(begin, last + 1):
            gw = struct.unpack(">H", buf[pos:pos + 2])[0]
            pos += 2
            gh = struct.unpack(">H", buf[pos:pos + 2])[0]
            pos += 2
            if mono:
                sz = ((gw + 7) // 8) * gh
                glyph = buf[pos:pos + sz]
                pos += sz
                out += _u16be(gw) + _u16be(gh) + glyph
            else:
                mem, pos = _bitmap_body_mem_bytes(buf, pos, bpp, gw, gh)
                out += _u16be(bpp) + _u16be(gw) + _u16be(gh) + mem
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST)
    args = ap.parse_args()

    manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
    base = os.path.dirname(os.path.abspath(args.manifest))

    ok = unproven = skipped = 0
    # asset_oracle.c prints the GLOBAL asset_table[]/assets.h enum index
    # (0..ASSET_COUNT-1); rec["index"] is only the per-FAMILY object index
    # (e.g. every character's frame01 is family index 1), so it cannot be
    # used for the leading column here -- reproduce assets_table.inc's row
    # order instead (extract_assets.py already walked it in that order, and
    # JSON object insertion order is preserved by json.loads), so the two
    # outputs' lines line up 1:1 for a plain `diff`.
    rows = list(manifest["assets"].items())

    for global_index, (asset_id, rec) in enumerate(rows):
        typ = rec["type_fourcc"]
        path = os.path.join(base, rec["file"].replace("/", os.sep))
        try:
            if typ == "BMP ":
                data = bitmap_mem_bytes_from_png(path, rec["geometry"]["bpp"])
            elif typ == "PAL ":
                data = Path(path).read_bytes()
            elif typ == "FONT":
                data = font_mem_bytes(path)
            elif typ == "info":
                data = Path(path).read_bytes()[:32]
            elif typ == "OGG ":
                print("%d %s UNPROVEN(vorbis-decode-not-reproduced)" % (global_index, rec["object_name"]))
                unproven += 1
                continue
            else:
                print("%d %s SKIP unrecognized-type=%s" % (global_index, rec["object_name"], typ))
                skipped += 1
                continue
        except Exception as exc:  # noqa: BLE001 -- report, keep going
            print("%d %s ERROR %s" % (global_index, rec["object_name"], exc))
            skipped += 1
            continue
        print("%d %s %s" % (global_index, rec["object_name"], datafile_tool.sha256_bytes(data)))
        ok += 1

    print("-- %d hashed, %d unproven (OGG/vorbis), %d skipped/error --" % (ok, unproven, skipped),
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
