"""itr_real_files_check -- parse every real .itr in the tree with the on-disk
order batch 14's save_replay trace established and the checksum batch 14's
calc_replay_checksum recovered, and compare against the value stored in the
file.  Evidence for PROMOTIONS.md batch 15: the format is confirmed against
files the real game wrote, not only against the emulated original.

Reads only; writes artifacts/itr_real_files_check.txt.
"""
import io
import os
import struct

PROJ = r"D:\Games\DOS\dos_recosystem\icytower_forged"
SRCDIR = os.path.join(PROJ, "assets", "profiles", "MissingNO", "replays")
OUT = os.path.join(PROJ, "artifacts", "itr_real_files_check.txt")

M = 0xFFFFFFFF


def hash32(a):                                   # replay.c hash(), verbatim
    a &= M
    a = ((a ^ 61) ^ (a >> 16)) & M
    a = (a + (a << 3)) & M
    a = (a ^ (a >> 4)) & M
    a = (a * 0x27D4EB2D) & M
    a = (a ^ (a >> 15)) & M
    return a


def s32(v):
    return ((v & M) ^ 0x80000000) - 0x80000000


def sbyte(b):
    return b - 256 if b > 127 else b


def parse(b):
    """save_replay()'s write order, read back -- PROMOTIONS.md batch 14
    finding 2.  NOT struct order: checksum after comment, the five float
    columns interleaved by index, each record reversed and five bytes."""
    r = {}
    off = 0
    r["header"] = b[0:6]; off = 6
    r["size"] = struct.unpack_from("<i", b, off)[0]; off += 4
    r["name"] = b[off:off + 32]; off += 32
    r["date"] = b[off:off + 32]; off += 32
    (r["score"], r["floor"], r["combo"], r["nctf"],
     r["blc"]) = struct.unpack_from("<5i", b, off); off += 20
    r["ccc"] = list(struct.unpack_from("<5i", b, off)); off += 20
    r["jc"] = list(struct.unpack_from("<5i", b, off)); off += 20
    (r["floor_shrink"], r["floor_size"], r["start_speed"], r["speed_increase"],
     r["gravity"], r["rejump"], r["seed"]) = struct.unpack_from("<7i", b, off)
    off += 28
    r["comment"] = b[off:off + 42]; off += 42
    r["checksum"] = struct.unpack_from("<i", b, off)[0]; off += 4
    r["tc_posts"] = struct.unpack_from("<i", b, off)[0]; off += 4
    cols = [[], [], [], [], []]
    for _ in range(100):
        for k in range(5):
            cols[k].append(struct.unpack_from("<f", b, off)[0]); off += 4
    r["cols"] = cols
    recs = []
    for _ in range(r["size"]):
        cc = struct.unpack_from("<i", b, off)[0]; off += 4
        kf = b[off]; off += 1
        recs.append((kf, cc))
    r["records"] = recs
    r["trailing"] = len(b) - off
    return r


def checksum(r):
    """calc_replay_checksum(), PROMOTIONS.md batch 14 -- unsigned
    accumulator, only the c/q/t float columns hashed, hash() tail."""
    s = (3702
         + (r["blc"] + 1) * 34
         + r["nctf"] * 254
         + r["floor_size"] * 17
         + r["floor_shrink"] * 102
         + r["start_speed"] * 163
         + r["speed_increase"] * 23
         + r["gravity"] * 88
         + r["seed"] * 329
         + r["tc_posts"] * 127
         + r["rejump"] * 13
         + (r["score"] + 1) * 17
         + (r["combo"] + 1) * 73
         + (r["floor"] + 1) * 113) & M
    for i in range(5):
        s = (s + r["ccc"][i] * (39 + 3 * i) + r["jc"][i] * (27 + 3 * i)) & M
    c, q, t = r["cols"][0], r["cols"][1], r["cols"][2]
    for i in range(100):
        s = int(s + c[i] * ((i + 1) % 13)) & M
        s = int(s + q[i] * ((i + 7) % 17)) & M
        s = int(s + t[i] * ((i + 9) % 23)) & M
    mult = 0x11
    for i in range(32):
        s = (s + (sbyte(r["name"][i]) + i) * (sbyte(r["date"][i]) + i) * mult) & M
        mult += 0x11
    mult = -3
    for i in range(42):
        v = sbyte(r["comment"][i]) + i
        s = (s + v * v * mult) & M
        mult += 3
    for i, (kf, cc) in enumerate(r["records"]):
        s = (s + kf * 3 * ((i % 193) + 1) + cc * 7 * ((i % 167) + 1)) & M
    return s32(hash32(s))


def main():
    lines = []
    lines.append("itr_real_files_check -- PROMOTIONS.md batch 15")
    lines.append("")
    lines.append("Every .itr file in the tree, parsed with save_replay()'s recovered")
    lines.append("on-disk order (batch 14 finding 2) and re-checksummed with the")
    lines.append("recovered calc_replay_checksum (batch 14 finding 3), using the")
    lines.append('magic "ITR140" read out of the image at 0x4d7dd0 (batch 15).')
    lines.append("")
    lines.append("%-38s %5s %6s %13s %13s %5s %4s %s"
                 % ("file", "size", "trail", "stored", "computed", "magic",
                    "tag", "verdict"))
    bad = 0
    n = 0
    for fn in sorted(os.listdir(SRCDIR)):
        if not fn.lower().endswith(".itr"):
            continue
        b = open(os.path.join(SRCDIR, fn), "rb").read()
        r = parse(b)
        got = checksum(r)
        ok = (got == r["checksum"] and r["trailing"] == 0
              and r["header"] == b"ITR140")
        tag = b"ICYTOWERISGREAT" in r["date"]
        n += 1
        if not ok:
            bad += 1
        lines.append("%-38s %5d %6d %13d %13d %5s %4s %s"
                     % (fn, r["size"], r["trailing"], r["checksum"], got,
                        r["header"].decode("latin1"),
                        "yes" if tag else "no", "OK" if ok else "MISMATCH"))
    lines.append("")
    lines.append("%d file(s), %d mismatch(es)." % (n, bad))
    lines.append("")
    lines.append("`magic` is the six bytes at file offset 0; `tag` is whether the")
    lines.append("32-byte date field still carries save_replay()'s")
    lines.append('"  ICYTOWERISGREAT " watermark in bytes 12..29 -- batch 14')
    lines.append("finding 1, confirmed here from the outside.")
    io.open(OUT, "w", encoding="utf-8", newline="\r\n").write("\n".join(lines) + "\n")
    print("\n".join(lines))


main()
