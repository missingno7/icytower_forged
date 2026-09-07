#!/usr/bin/env python3
"""pf_inspect.py - milestone 9 (win32_pilot.md sec 8 row 9): read a carrier
snapshot OFFLINE and print the game's own state with names and real types.

A snapshot directory (carrier/src/snapshot.cpp, format
`portforge-win32-carrier-snapshot-v1`) is a flat address space plus a
manifest. This tool re-assembles that address space and decodes it with the
GENERATED interop metadata - never with hand-written offsets:

    carrier/gen/interop_index.json  every game global: name, VA, C type, CU
    carrier/gen/it_types.h          every struct's members, in order, with
                                    their declared types (generated from DWARF)
    carrier/gen/it_types_check.c    the authoritative sizeof/offsetof values
                                    for those structs (the same numbers the
                                    carrier's own build verifies under MSVC)
    artifacts/coff_symbols.json     each global's size (gap to the next symbol),
                                    the same rule gen_game_globals.py uses

Subcommands
-----------
  show DIR [--globals a,b,c] [--all]
      Named globals with typed values. With no --globals, prints the
      gameplay summary the milestone asks for: reward_time, reward_scale,
      player_id, ply[player_id]->{x,y,sx,sy,dead,frame,...}, the tick
      counters, and the carrier's own externalized state (virtual clock,
      script cursor, RNG).

  player DIR [--index N]
      Full Tplayer struct dump for ply[N] (default: ply[player_id]).

  diff DIR_A DIR_B
      Compares two snapshots BY NAMED GLOBAL and reports the first
      difference as (global, member, byte offset) - never as a percentage.
      Falls back to raw section bytes for anything outside a named global.

Usage: python carrier/scripts/pf_inspect.py <subcommand> ... [--repo ROOT]
Exit code: 0 on success (diff: 0 when the snapshots are equal), 1 on a
difference, 2 on a usage/parse error.
"""
import argparse
import bisect
import json
import re
import struct
import sys
from pathlib import Path

# ---------------------------------------------------------------------
# The snapshot's address space
# ---------------------------------------------------------------------
class Address_space:
    """Byte ranges from a snapshot dir, plus (optionally) the read-only
    sections of the guest EXE so string literals in .rdata resolve too."""

    def __init__(self, snapdir, image=None):
        self.dir = Path(snapdir)
        self.manifest = json.loads((self.dir / "manifest.json").read_text())
        if self.manifest.get("format") != "portforge-win32-carrier-snapshot-v1":
            raise ValueError(f"{snapdir}: not a portforge-win32-carrier-snapshot-v1")
        self.ranges = []  # (va, bytes)
        for key in ("data", "bss", "arena", "stack"):
            comp = self.manifest.get(key)
            if not comp:
                continue
            blob = (self.dir / comp["file"]).read_bytes()
            if len(blob) != comp["size"]:
                raise ValueError(f"{comp['file']}: size {len(blob)} != manifest {comp['size']}")
            self.ranges.append((int(comp["va"], 16), blob))
        if image and Path(image).exists():
            self._add_image(Path(image))
        self.ranges.sort()
        self._starts = [r[0] for r in self.ranges]
        self.carrier = self._read_carrier_state()

    def _add_image(self, path):
        """Map the guest PE's read-only sections (.text/.rdata) at their VAs.
        Only needed so `char *` globals can be dereferenced into strings."""
        raw = path.read_bytes()
        pe = struct.unpack_from("<I", raw, 0x3C)[0]
        nsec = struct.unpack_from("<H", raw, pe + 6)[0]
        opt = struct.unpack_from("<H", raw, pe + 20)[0]
        base = struct.unpack_from("<I", raw, pe + 24 + 28)[0]
        for i in range(nsec):
            off = pe + 24 + opt + i * 40
            name = raw[off:off + 8].rstrip(b"\0").decode("latin1")
            vsize, va, rsize, roff = struct.unpack_from("<IIII", raw, off + 8)
            if name in (".text", ".rdata", ".idata"):
                self.ranges.append((base + va, raw[roff:roff + min(rsize, vsize)]))

    def _read_carrier_state(self):
        c = dict(self.manifest.get("carrier", {}))
        c["tick"] = self.manifest.get("tick")
        return c

    def read(self, va, n):
        i = bisect.bisect_right(self._starts, va) - 1
        if i < 0:
            return None
        start, blob = self.ranges[i]
        off = va - start
        if off < 0 or off + n > len(blob):
            return None
        return blob[off:off + n]


# ---------------------------------------------------------------------
# Types, from the generated headers
# ---------------------------------------------------------------------
PRIM = {
    "char": (1, "i"), "signed char": (1, "i"), "unsigned char": (1, "u"),
    "short": (2, "i"), "short int": (2, "i"), "unsigned short": (2, "u"),
    "short unsigned int": (2, "u"), "unsigned short int": (2, "u"),
    "int": (4, "i"), "signed int": (4, "i"), "unsigned": (4, "u"),
    "unsigned int": (4, "u"), "long": (4, "i"), "long int": (4, "i"),
    "unsigned long": (4, "u"), "long unsigned int": (4, "u"),
    "long long": (8, "i"), "unsigned long long": (8, "u"),
    "float": (4, "f"), "double": (8, "f"), "long double": (12, "ld"),
    "_Bool": (1, "u"), "void": (0, "v"),
}
MEMBER_RE = re.compile(
    r"^\s*(?P<type>[A-Za-z_][\w \t*]*?)\s*(?P<name>[A-Za-z_]\w*)\s*"
    r"(?P<arr>(?:\[\d*\])*)\s*;\s*$")


class Types:
    def __init__(self, gen_dir):
        self.typedefs = {}   # name -> underlying type string
        self.structs = {}    # name -> {"size": n|None, "members": [ {...} ]}
        self._parse_types_h(Path(gen_dir) / "it_types.h")
        self._parse_check_c(Path(gen_dir) / "it_types_check.c")

    def _parse_types_h(self, path):
        cur = None
        for line in path.read_text(errors="replace").splitlines():
            m = re.match(r"^\s*(?:typedef\s+)?struct\s+(\w+)\s*\{\s*$", line)
            if m:
                cur = {"name": m.group(1), "members": [], "size": None}
                self.structs[m.group(1)] = cur
                continue
            if cur is not None:
                if line.startswith("}"):
                    cur = None
                    continue
                mm = MEMBER_RE.match(line)
                if mm:
                    t = mm.group("type").strip()
                    nm = mm.group("name")
                    dims = [int(d) for d in re.findall(r"\[(\d+)\]", mm.group("arr"))]
                    stars = t.count("*")
                    cur["members"].append({"name": nm, "type": t.replace("*", "").strip(),
                                           "ptr": stars, "dims": dims})
                continue
            m = re.match(r"^\s*typedef\s+([\w \t]+?)\s+(\w+)\s*;\s*$", line)
            if m and m.group(2) not in self.structs:
                self.typedefs[m.group(2)] = m.group(1).strip()

    def _parse_check_c(self, path):
        if not path.exists():
            return
        for line in path.read_text(errors="replace").splitlines():
            m = re.search(r"sizeof\(struct (\w+)\) != (\d+)", line)
            if m and m.group(1) in self.structs:
                self.structs[m.group(1)]["size"] = int(m.group(2))
                continue
            m = re.search(r"offsetof\(struct (\w+), (\w+)\) != (\d+)", line)
            if m and m.group(1) in self.structs:
                for mem in self.structs[m.group(1)]["members"]:
                    if mem["name"] == m.group(2):
                        mem["offset"] = int(m.group(3))

    def resolve(self, t):
        """Follow typedefs down to a primitive or a struct name."""
        seen = 0
        while t in self.typedefs and t not in self.structs and seen < 16:
            t = self.typedefs[t]
            seen += 1
        return t

    def sizeof(self, t, ptr=0, dims=()):
        n = 4 if ptr else self._base_size(t)
        for d in dims:
            n *= d
        return n

    def _base_size(self, t):
        t = self.resolve(t)
        if t in PRIM:
            return PRIM[t][0]
        st = self.structs.get(t)
        if st and st["size"]:
            return st["size"]
        m = re.match(r"^(\w+)\[(\d+)\]$", t)   # e.g. PALETTE = RGB[256]
        if m:
            return self._base_size(m.group(1)) * int(m.group(2))
        return 0  # unknown/opaque


# ---------------------------------------------------------------------
# Value formatting
# ---------------------------------------------------------------------
def _scalar(types, t, raw):
    t = types.resolve(t)
    size, kind = PRIM.get(t, (len(raw), "u"))
    if not raw or len(raw) < size or size == 0:
        return "<unreadable>"
    if kind == "f":
        return repr(struct.unpack_from("<f" if size == 4 else "<d", raw)[0])
    fmt = {1: "b", 2: "h", 4: "i", 8: "q"}[size]
    if kind == "u":
        fmt = fmt.upper()
    return str(struct.unpack_from("<" + fmt, raw)[0])


def format_value(mem_space, types, t, ptr, dims, va, depth=0, orig_type=None):
    """One value at `va`, given its declared type. Returns a string."""
    label = orig_type or t
    if ptr:
        raw = mem_space.read(va, 4)
        if raw is None:
            return "<not in snapshot>"
        p = struct.unpack_from("<I", raw)[0]
        if types.resolve(t) == "char" and ptr == 1 and p:
            s = read_cstr(mem_space, p)
            if s is not None:
                return f"0x{p:08x} -> {s!r}"
        return f"0x{p:08x}"
    base = types.resolve(t)
    if dims:
        n = dims[0]
        rest = dims[1:]
        elem = types.sizeof(t, 0, rest)
        if base == "char" and not rest:
            raw = mem_space.read(va, n)
            return repr(raw.split(b"\0")[0].decode("latin1")) if raw else "<not in snapshot>"
        shown = min(n, 8)
        parts = [format_value(mem_space, types, t, 0, rest, va + i * elem, depth + 1)
                 for i in range(shown)]
        return "[" + ", ".join(parts) + (", ...]" if n > shown else "]")
    if base in PRIM:
        v = _scalar(types, base, mem_space.read(va, PRIM[base][0]) or b"")
        # Allegro's `fixed` is 16.16; showing only the raw int hides the value.
        if (orig_type or t) == "fixed" and v != "<unreadable>":
            return f"{v} (fixed 16.16 = {int(v) / 65536.0:g})"
        return v
    st = types.structs.get(base)
    if st and depth < 2:
        return "{" + ", ".join(
            f"{m['name']}=" + format_value(mem_space, types, m['type'], m['ptr'], m['dims'],
                                           va + m.get('offset', 0), depth + 1, m['type'])
            for m in st["members"] if "offset" in m and not m["name"].startswith("_pad")) + "}"
    raw = mem_space.read(va, min(16, types.sizeof(t, 0, dims) or 4))
    return raw.hex() if raw else "<not in snapshot>"


def read_cstr(mem_space, va, limit=120):
    out = bytearray()
    for i in range(limit):
        b = mem_space.read(va + i, 1)
        if not b or b == b"\0":
            break
        out += b
    return out.decode("latin1") if out else None


# ---------------------------------------------------------------------
# Globals
# ---------------------------------------------------------------------
class Globals:
    def __init__(self, repo):
        idx = json.loads((repo / "carrier/gen/interop_index.json").read_text())
        self.by_name = {}
        for g in idx["globals"]:
            g = dict(g)
            g["va"] = int(g["va"], 16)
            self.by_name[g["name"]] = g
        # Size = gap to the next COFF symbol at any VA, the same rule
        # gen_game_globals.py uses (a global's own symbol carries no size).
        syms = json.loads((repo / "artifacts/coff_symbols.json").read_text())
        vas = sorted({s["va"] for s in syms if s.get("va") is not None})
        for g in self.by_name.values():
            i = bisect.bisect_right(vas, g["va"])
            g["size"] = (vas[i] - g["va"]) if i < len(vas) else 4
        self.by_va = sorted(self.by_name.values(), key=lambda g: g["va"])
        self._starts = [g["va"] for g in self.by_va]

    def at(self, va):
        i = bisect.bisect_right(self._starts, va) - 1
        if i < 0:
            return None
        g = self.by_va[i]
        return g if va < g["va"] + g["size"] else None


def split_decl(types, decl):
    """'Tplayer *[1000]' -> ('Tplayer', ptr=1, dims=[1000])."""
    decl = decl.replace("volatile", "").replace("const", "").strip()
    dims = [int(d) for d in re.findall(r"\[(\d+)\]", decl)]
    decl = re.sub(r"\[\d*\]", "", decl).strip()
    ptr = decl.count("*")
    return decl.replace("*", "").strip(), ptr, dims


def print_global(space, types, g, indent=""):
    t, ptr, dims = split_decl(types, g["type"])
    val = format_value(space, types, t, ptr, dims, g["va"], orig_type=t)
    print(f"{indent}{g['name']:<28} @0x{g['va']:08x}  {g['type']:<20} = {val}")


# ---------------------------------------------------------------------
# Subcommands
# ---------------------------------------------------------------------
# The gameplay summary win32_pilot.md's milestone-9 brief names, plus the
# tick counters the carrier's own clock is defined against (cycle_count is
# the 20 ms Allegro timer global at 0x506938; logic_count is timer.c's).
DEFAULT_GLOBALS = ["reward_time", "reward_scale", "player_id", "logic_count",
                   "cycle_count", "cycle_loops", "frame_count", "fall_count",
                   "scroll_count", "seed", "rec_seed", "rec_pos", "recording",
                   "max_speed", "gravity_modifier", "curr_char", "version_str"]


def cmd_show(args, repo):
    space = Address_space(args.dir, args.image)
    types = Types(repo / "carrier/gen")
    gl = Globals(repo)
    print(f"snapshot {args.dir}")
    print(f"  format={space.manifest['format']} tick={space.manifest['tick']} "
          f"safepoint={space.manifest['safepoint_va']}")
    c = space.manifest["context"]
    print(f"  eip={c['eip']} esp={c['esp']} ebp={c['ebp']} eflags={c['eflags']} "
          f"x87cw={c['x87_control_word_live']}")
    cs = space.manifest["carrier"]
    print(f"  carrier state: virtual_ms={cs['virtual_ms']} (tick {space.manifest['tick']}) "
          f"rng_state={cs['rng_state']} (0x{cs['rng_state']:08x}) rng_calls={cs['rng_calls']} "
          f"input_script_cursor={cs['input_script_cursor']} arena_bump={cs['arena_bump']}")
    print()
    names = args.globals.split(",") if args.globals else (
        sorted(gl.by_name) if args.all else DEFAULT_GLOBALS)
    for name in names:
        g = gl.by_name.get(name.strip())
        if g is None:
            print(f"  {name}: <no such game global in interop_index.json>")
            continue
        print_global(space, types, g, "  ")
    if not args.globals and not args.all:
        print()
        dump_player(space, types, gl, None)
    return 0


def dump_player(space, types, gl, index):
    pid_g = gl.by_name.get("player_id")
    ply_g = gl.by_name.get("ply")
    if not pid_g or not ply_g:
        print("  (no player_id/ply global in interop_index.json)")
        return 2
    if index is None:
        raw = space.read(pid_g["va"], 4)
        if raw is None:
            print("  player_id not in snapshot")
            return 2
        index = struct.unpack_from("<i", raw)[0]
    raw = space.read(ply_g["va"] + index * 4, 4)
    if raw is None:
        print(f"  ply[{index}] not in snapshot")
        return 2
    p = struct.unpack_from("<I", raw)[0]
    print(f"  ply[{index}] = 0x{p:08x}  (Tplayer, {types.sizeof('Tplayer')} bytes)")
    if not p:
        return 0
    st = types.structs.get("Tplayer")
    for m in st["members"]:
        if "offset" not in m or m["name"].startswith("_pad"):
            continue
        v = format_value(space, types, m["type"], m["ptr"], m["dims"],
                         p + m["offset"], 1, m["type"])
        print(f"    +{m['offset']:<4} {m['name']:<20} {m['type']:<10} = {v}")
    return 0


def cmd_player(args, repo):
    space = Address_space(args.dir, args.image)
    return dump_player(space, Types(repo / "carrier/gen"), Globals(repo), args.index)


DIGEST_SCOPE_RE = re.compile(r"\{0x([0-9a-f]+)u,\s*(\d+)u\}")


def digest_scope(repo):
    """The exact {VA,size} ranges carrier/src/det.cpp's per-tick digest hashes
    (carrier/gen/game_globals.inc, generated). A difference INSIDE this scope
    is a certification failure; one outside it is not - that distinction is
    the whole point of carrier/NOTES.md's 'the digest region' finding."""
    path = repo / "carrier/gen/game_globals.inc"
    out = []
    if path.exists():
        for line in path.read_text().splitlines():
            m = DIGEST_SCOPE_RE.search(line)
            if m:
                out.append((int(m.group(1), 16), int(m.group(2))))
    return out


def cmd_diff(args, repo):
    a = Address_space(args.dir_a, args.image)
    b = Address_space(args.dir_b, args.image)
    types = Types(repo / "carrier/gen")
    gl = Globals(repo)
    scope = digest_scope(repo)
    print(f"A = {args.dir_a} (tick {a.manifest['tick']})")
    print(f"B = {args.dir_b} (tick {b.manifest['tick']})")

    def in_digest(va, size):
        return any(s <= va and va + size <= s + n for s, n in scope)

    rows = []
    for g in gl.by_va:
        ra = a.read(g["va"], g["size"])
        rb = b.read(g["va"], g["size"])
        if ra is None or rb is None or ra == rb:
            continue
        off = next(i for i in range(len(ra)) if ra[i] != rb[i])
        rows.append((g, off, member_at(types, g, off), ra, rb, in_digest(g["va"], g["size"])))

    def report(g, off, member, ra, rb, heading):
        print(f"\n{heading}: {g['name']} @0x{g['va']:08x} "
              f"({g['type']}, {g['size']} bytes, {Path(g['cu']).name})")
        print(f"  first differing byte: +{off} (VA 0x{g['va'] + off:08x})  "
              f"A={ra[off]:02x} B={rb[off]:02x}")
        if member:
            print(f"  member: .{member}")
        t, ptr, dims = split_decl(types, g["type"])
        print(f"  A = {format_value(a, types, t, ptr, dims, g['va'], orig_type=t)}")
        print(f"  B = {format_value(b, types, t, ptr, dims, g['va'], orig_type=t)}")

    if rows:
        report(*rows[0][:5], heading="FIRST DIFFERING GLOBAL")
        scoped = [r for r in rows if r[5]]
        if scoped and scoped[0] is not rows[0]:
            report(*scoped[0][:5], heading="FIRST DIFFERING GLOBAL INSIDE THE PER-TICK DIGEST SCOPE")
        elif not scoped:
            print("\nNo differing global is inside the per-tick digest scope "
                  "(carrier/gen/game_globals.inc) - the per-tick digests of these two "
                  "states are EQUAL; every difference is host-object identity or a "
                  "field the generator deliberately excludes.")
        print(f"\n{len(rows)} named game global(s) differ "
              f"({len(scoped)} of them inside the digest scope):")
        for g, off, member, _, _, sc in rows:
            print(f"  {'[digest] ' if sc else '          '}{g['name']:<28} "
                  f"@0x{g['va']:08x} +{off}" + (f"  .{member}" if member else ""))
        return 1
    # 2. Nothing named differs - say where the raw sections differ anyway,
    #    so a difference in Allegro/CRT/DirectX state is still visible (that
    #    is exactly the host-identity category carrier/NOTES.md documents).
    print("\nno named game global differs.")
    raw_diffs = []
    for key in ("data", "bss", "arena", "stack"):
        ca, cb = a.manifest.get(key), b.manifest.get(key)
        if not ca or not cb:
            continue
        if ca["sha256"] != cb["sha256"]:
            raw_diffs.append(key)
    if raw_diffs:
        print(f"raw components that DO differ: {', '.join(raw_diffs)} "
              f"(not game-owned state - see carrier/NOTES.md 'Milestones 8-9')")
        return 1
    print("the two snapshots are byte-identical in every component.")
    return 0


def member_at(types, g, off):
    t, ptr, dims = split_decl(types, g["type"])
    st = types.structs.get(types.resolve(t))
    if not st or ptr:
        return None
    size = st["size"] or 0
    if dims and size:
        idx, off = divmod(off, size)
        prefix = f"[{idx}]"
    else:
        prefix = ""
    best = None
    for m in st["members"]:
        if "offset" in m and m["offset"] <= off:
            if best is None or m["offset"] > best["offset"]:
                best = m
    return f"{prefix}{best['name']}+{off - best['offset']}" if best else None


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--repo", default=None, help="repository root (default: inferred)")
    p.add_argument("--image", default=None,
                   help="guest EXE, mapped read-only so char* globals resolve "
                        "(default: <repo>/assets/icytower15.exe)")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("show")
    s.add_argument("dir")
    s.add_argument("--globals", default=None, help="comma-separated global names")
    s.add_argument("--all", action="store_true", help="every game global")
    s.set_defaults(fn=cmd_show)

    s = sub.add_parser("player")
    s.add_argument("dir")
    s.add_argument("--index", type=int, default=None)
    s.set_defaults(fn=cmd_player)

    s = sub.add_parser("diff")
    s.add_argument("dir_a")
    s.add_argument("dir_b")
    s.set_defaults(fn=cmd_diff)

    args = p.parse_args(argv[1:])
    repo = Path(args.repo) if args.repo else Path(__file__).resolve().parents[2]
    if args.image is None:
        cand = repo / "assets/icytower15.exe"
        args.image = str(cand) if cand.exists() else None
    try:
        return args.fn(args, repo)
    except (OSError, ValueError, KeyError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
