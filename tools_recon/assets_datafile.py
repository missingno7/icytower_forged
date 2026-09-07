"""assets_datafile.py - read-only Allegro 4 datafile reader for Icy Tower.

Handles:
  * packfile header  (F_PACK_MAGIC 'slh!' / F_NOPACK_MAGIC 'slh.'), optionally
    obfuscated with Allegro's encrypt_id() mask derived from a password
  * Allegro LZSS decompression (N=4096, F=18, THRESHOLD=2)
  * the datafile object tree: 'ALL.' + count + [prop*] type size data
  * extraction of objects to plain files (PNG/OGG/DATA verbatim; BMP/RLE/
    PAL/FONT/SAMP written both as a raw .albin blob and, where possible, as a
    normal .bmp/.wav)

Never writes into assets/.  Usage:
    python tools_recon/assets_datafile.py list  assets/data/data.dat
    python tools_recon/assets_datafile.py json  assets/data/data.dat out.json
    python tools_recon/assets_datafile.py extract assets/data/data.dat outdir [--limit N] [--only NAME]
"""
import sys, os, json, struct, hashlib, io

F_PACK_MAGIC = 0x736C6821      # 'slh!'
F_NOPACK_MAGIC = 0x736C682E    # 'slh.'
DAT_MAGIC = 0x414C4C2E         # 'ALL.'
DAT_PROPERTY = 0x70726F70      # 'prop'
DAT_END = -1

# password used by the game (pwd_garble_string(0x4bdb3c, 0x32) -> "CHEESE")
PW_GAME = "CHEESE"
PW_LOADING = "(c) Free Lunch Design"

PASSWORDS = {
    "data.dat": PW_GAME,
    "sfx15.dat": PW_GAME,
    "loading.dat": PW_LOADING,
}


def fourcc(v):
    v &= 0xFFFFFFFF
    return "".join(chr((v >> s) & 0xFF) for s in (24, 16, 8, 0))


def encrypt_mask(password, new_format=True):
    """Allegro encrypt_id() mask."""
    if not password:
        return 0
    mask = 0
    for i, ch in enumerate(password.encode("latin-1")):
        mask ^= ch << ((i & 3) * 8)
    if new_format:
        mask ^= 42
    return mask & 0xFFFFFFFF


def decrypt_stream(data, password, phase):
    """XOR the packfile body with the repeating password."""
    if not password:
        return data
    pw = password.encode("latin-1")
    out = bytearray(data)
    n = len(pw)
    for i in range(len(out)):
        out[i] ^= pw[(i + phase) % n]
    return bytes(out)


def lzss_decompress(src):
    """Allegro lzss.c decompressor (Okumura LZSS, N=4096 F=18 THRESHOLD=2)."""
    N, F, THRESHOLD = 4096, 18, 2
    text = bytearray(N)
    r = N - F
    out = bytearray()
    pos = 0
    flags = 0
    n = len(src)
    while True:
        if not (flags & 0x100):
            if pos >= n:
                break
            flags = src[pos] | 0xFF00
            pos += 1
        if flags & 1:
            if pos >= n:
                break
            c = src[pos]; pos += 1
            out.append(c)
            text[r] = c
            r = (r + 1) & (N - 1)
        else:
            if pos + 1 >= n:
                break
            i = src[pos]; j = src[pos + 1]; pos += 2
            i |= (j & 0xF0) << 4
            j = (j & 0x0F) + THRESHOLD
            for k in range(j + 1):
                c = text[(i + k) & (N - 1)]
                out.append(c)
                text[r] = c
                r = (r + 1) & (N - 1)
        flags >>= 1
    return bytes(out)


def open_packfile(path, password=None, verbose=False):
    """Return (plain_bytes, info) - the decrypted, decompressed packfile body."""
    raw = open(path, "rb").read()
    hdr = struct.unpack(">I", raw[:4])[0]
    info = {"file": path, "size": len(raw), "raw_header": "0x%08x" % hdr}
    cands = []
    for pw in ([password] if password is not None else [None, PW_GAME, PW_LOADING]):
        for nf in (True, False):
            for magic, packed in ((F_PACK_MAGIC, True), (F_NOPACK_MAGIC, False)):
                if hdr == (magic ^ encrypt_mask(pw, nf)):
                    cands.append((pw, nf, packed))
    if not cands:
        raise ValueError("no packfile magic match for %s (hdr 0x%08x)" % (path, hdr))
    pw, nf, packed = cands[0]
    info.update(password=pw, new_format=nf, packed=packed,
                mask="0x%08x" % encrypt_mask(pw, nf))
    body = raw[4:]
    if pw:
        # phase determined empirically: the 4 header bytes advance the key
        body = decrypt_stream(body, pw, 4)
    if packed:
        body = lzss_decompress(body)
    info["plain_size"] = len(body)
    return body, info


class R:
    def __init__(self, b):
        self.b = b
        self.p = 0

    def mgetl(self):
        v = struct.unpack(">i", self.b[self.p:self.p + 4])[0]
        self.p += 4
        return v

    def read(self, n):
        d = self.b[self.p:self.p + n]
        self.p += n
        return d


def parse_datafile(body):
    r = R(body)
    magic = r.mgetl()
    if (magic & 0xFFFFFFFF) != DAT_MAGIC:
        raise ValueError("bad DAT_MAGIC 0x%08x" % (magic & 0xFFFFFFFF))
    return read_objects(r, "")


def read_objects(r, prefix):
    count = r.mgetl()
    out = []
    for idx in range(count):
        props = {}
        while True:
            t = r.mgetl()
            if (t & 0xFFFFFFFF) != DAT_PROPERTY:
                break
            ptype = fourcc(r.mgetl())
            psize = r.mgetl()
            pdata = r.read(psize)
            props[ptype] = pdata.decode("latin-1")
        typ = fourcc(t)
        # every object body is an Allegro sub-chunk:
        #   filesize (bytes on disk) + datasize (bytes after unpacking)
        filesize = r.mgetl()
        datasize = r.mgetl()
        obj = {"index": idx, "path": prefix + (props.get("NAME") or "#%d" % idx),
               "name": props.get("NAME"), "type": typ,
               "size": datasize, "filesize": filesize, "props": props,
               "offset": r.p}
        if filesize != datasize:
            obj["packed_chunk"] = True
        if typ == "FILE":
            end = r.p + datasize
            obj["children"] = read_objects(r, obj["path"] + "/")
            obj["data"] = None
            r.p = end if end >= r.p else r.p
        else:
            obj["data"] = r.read(datasize)
        out.append(obj)
    return out


def flatten(objs, acc=None):
    if acc is None:
        acc = []
    for o in objs:
        acc.append(o)
        if o.get("children"):
            flatten(o["children"], acc)
    return acc


# ---------------------------------------------------------------- extraction
def bmp_from_allegro(data):
    """DAT_BITMAP payload -> (info, .bmp bytes or None).

    Layout: uint16 bpp, uint16 w, uint16 h, then pixel rows.
    """
    if len(data) < 6:
        return None, None
    bpp, w, h = struct.unpack(">HHH", data[:6])
    px = data[6:]
    return {"bpp": bpp, "w": w, "h": h, "pixel_bytes": len(px)}, None


def sample_info(data):
    """DAT_SAMPLE payload -> header info + WAV bytes."""
    if len(data) < 12:
        return None, None
    bits, stereo, freq, length = struct.unpack(">HHHI", data[:10])[0], None, None, None
    bits, stereo, freq, priority, length = struct.unpack(">HHHHI", data[:12])
    px = data[12:]
    nch = 2 if stereo else 1
    sw = 2 if bits == 16 else 1
    hdr = b"RIFF" + struct.pack("<I", 36 + len(px)) + b"WAVEfmt " + \
        struct.pack("<IHHIIHH", 16, 1, nch, freq, freq * nch * sw, nch * sw, bits) + \
        b"data" + struct.pack("<I", len(px))
    return {"bits": bits, "stereo": stereo, "freq": freq, "priority": priority,
            "len": length, "data_bytes": len(px)}, hdr + px


EXT = {"PNG ": ".png", "DATA": ".bin", "PAL ": ".pal", "FONT": ".alfont",
       "BMP ": ".albmp", "RLE ": ".alrle", "SAMP": ".alsamp", "MIDI": ".mid",
       "FILE": "", "CMP ": ".alcmp", "XCMP": ".alxcmp", "FLIC": ".fli"}


def sniff(data):
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return "png"
    if data[:4] == b"OggS":
        return "ogg"
    if data[:4] == b"RIFF":
        return "riff"
    if data[:2] == b"BM":
        return "bmp"
    if data[:4] == b"MThd":
        return "midi"
    return None


def main():
    cmd = sys.argv[1]
    path = sys.argv[2]
    pw = PASSWORDS.get(os.path.basename(path).lower())
    body, info = open_packfile(path, pw)
    objs = parse_datafile(body)
    flat = flatten(objs)
    if cmd == "list":
        print("# %s  %s" % (path, json.dumps(info)))
        for o in flat:
            s = sniff(o["data"]) if o["data"] else None
            print("%-4d %-5s %-40s %9d %-6s %s" % (
                o["index"], o["type"], o["path"], o["size"], s or "",
                ",".join(k for k in o["props"] if k != "NAME")))
    elif cmd == "json":
        rec = {"info": info, "objects": []}
        for o in flat:
            rec["objects"].append({
                "index": o["index"], "path": o["path"], "name": o["name"],
                "type": o["type"], "size": o["size"], "offset": o["offset"],
                "props": {k: v for k, v in o["props"].items()},
                "sniff": sniff(o["data"]) if o["data"] else None,
                "sha256": hashlib.sha256(o["data"]).hexdigest() if o["data"] else None,
            })
        out = sys.argv[3] if len(sys.argv) > 3 else None
        j = json.dumps(rec, indent=1)
        if out:
            open(out, "w").write(j)
            print("wrote", out, len(rec["objects"]), "objects")
        else:
            print(j)
    elif cmd == "extract":
        outdir = sys.argv[3]
        limit = int(sys.argv[sys.argv.index("--limit") + 1]) if "--limit" in sys.argv else None
        only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
        n = 0
        for o in flat:
            if o["data"] is None:
                continue
            if only and only not in o["path"]:
                continue
            if limit is not None and n >= limit:
                break
            rel = o["path"].replace("/", os.sep)
            ext = EXT.get(o["type"], ".bin")
            s = sniff(o["data"])
            if s == "ogg":
                ext = ".ogg"
            elif s == "png":
                ext = ".png"
            dest = os.path.join(outdir, rel + ext)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            open(dest, "wb").write(o["data"])
            print("%-5s %-40s -> %s (%d)" % (o["type"], o["path"], dest, len(o["data"])))
            if o["type"] == "SAMP":
                inf, wav = sample_info(o["data"])
                if wav:
                    open(dest + ".wav", "wb").write(wav)
                    print("      SAMP", inf)
            if o["type"] == "BMP ":
                inf, _ = bmp_from_allegro(o["data"])
                print("      BMP ", inf)
            n += 1
        print("extracted", n)


if __name__ == "__main__":
    main()
