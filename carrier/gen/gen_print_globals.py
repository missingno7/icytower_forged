#!/usr/bin/env python3
"""gen_print_globals.py - generates gen/it_print_globals.inc: a flat, typed
C table of every DWARF-named game global (carrier/gen/interop_index.json)
plus every struct member (carrier/gen/it_types.h + it_types_check.c), for
carrier/src/print_globals.cpp's `--print-globals` at shutdown.

Why generated rather than hand-written (see win32_pilot.md milestone 9a /
carrier/NOTES.md "Headless, frame oracle, named globals, .itr workload"):
`--print-globals name1,name2,...` must work for ANY named global the DWARF
knows about, not just Icy Tower's own score/floor/combo - project-specific
names are supplied on the command line (by scripts/play.py), never baked
into carrier source. This is the exact same source pair
carrier/scripts/pf_inspect.py already parses OFFLINE in Python (see its own
Types/Globals classes, which this generator's parsing mirrors); the
difference is this table has to be usable at carrier RUNTIME (shutdown time,
inside carrier.exe), so the type resolution pf_inspect.py does lazily in
Python is done once here, ahead of time, and baked into a flat C array the
runtime just walks - no string parsing, no typedef-chasing, at runtime.

Every global's declared type is resolved down to either a primitive kind
(int/unsigned/long/float/double/char, with real byte size) or a known
struct name, with pointer-count and array-dims split out - the same
`split_decl`/`Types.resolve` logic pf_inspect.py already uses, ported here
to emit data instead of interpreting it live. Struct members are flattened
into ONE table across all 139 structs in it_types.h (small enough that a
linear scan at runtime, filtered by struct name, is cheap and needs no
per-struct indexing scheme).

Inputs (read-only):
    carrier/gen/interop_index.json  - the `globals` list (name, va, type, cu)
    carrier/gen/it_types.h          - every struct's members, in DWARF order
    carrier/gen/it_types_check.c    - authoritative sizeof/offsetof values

Output: carrier/gen/it_print_globals.inc

Usage: python gen_print_globals.py [repo_root]
"""
import json
import re
import sys
from pathlib import Path

PRIM = {
    "char": (1, "C"), "signed char": (1, "I"), "unsigned char": (1, "U"),
    "short": (2, "I"), "short int": (2, "I"), "unsigned short": (2, "U"),
    "short unsigned int": (2, "U"), "unsigned short int": (2, "U"),
    "int": (4, "I"), "signed int": (4, "I"), "unsigned": (4, "U"),
    "unsigned int": (4, "U"), "long": (4, "I"), "long int": (4, "I"),
    "unsigned long": (4, "U"), "long unsigned int": (4, "U"),
    "long long": (8, "I"), "unsigned long long": (8, "U"),
    "float": (4, "F"), "double": (8, "D"), "long double": (12, "D"),
    "_Bool": (1, "U"),
}
MEMBER_RE = re.compile(
    r"^\s*(?P<type>[A-Za-z_][\w \t]*?)\s*\**\s*(?P<name>[A-Za-z_]\w*)\s*"
    r"(?P<arr>(?:\[\d*\])*)\s*;\s*$")


class Types:
    """Ported from carrier/scripts/pf_inspect.py's Types class (same inputs,
    same parsing) - see that file for the original, exercised implementation."""

    def __init__(self, gen_dir):
        self.typedefs = {}
        self.structs = {}  # name -> {"size": n|None, "members": [...]}
        self._parse_types_h(gen_dir / "it_types.h")
        self._parse_check_c(gen_dir / "it_types_check.c")

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
                    stars = line[:line.index(nm)].count("*") if nm in line else 0
                    # Re-derive the star count from the raw declaration text
                    # (MEMBER_RE strips '*' before the name into no group).
                    decl = line.split(";")[0]
                    stars = decl.count("*")
                    cur["members"].append({"name": nm, "type": t,
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
        """Follow typedefs to a primitive name or a known struct name."""
        seen = 0
        while t in self.typedefs and t not in self.structs and t not in PRIM and seen < 16:
            t = self.typedefs[t]
            seen += 1
        return t

    def is_fixed(self, t):
        """True if `t` (before resolution) is Allegro's 16.16 `fixed` typedef
        (pf_inspect.py's own special-cased pretty-printer for this type)."""
        seen, cur = 0, t
        while seen < 16:
            if cur == "fixed":
                return True
            if cur in self.typedefs and cur not in self.structs:
                cur = self.typedefs[cur]
                seen += 1
                continue
            break
        return False


def split_decl(decl):
    """'Tplayer *[1000]' -> ('Tplayer', ptr=1, dims=[1000])."""
    decl = decl.replace("volatile", "").replace("const", "").strip()
    dims = [int(d) for d in re.findall(r"\[(\d*)\]", decl) if d]
    decl = re.sub(r"\[\d*\]", "", decl).strip()
    ptr = decl.count("*")
    return decl.replace("*", "").strip(), ptr, dims


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def classify(types, base):
    """Resolve `base` down to either ('P', prim_size, prim_kind) or
    ('S', struct_name, struct_size) or ('?', base, 0) for anything opaque
    (unknown/incomplete type - printed as raw bytes at runtime)."""
    r = types.resolve(base)
    if r in PRIM:
        size, kind = PRIM[r]
        return "P", size, kind
    if r in types.structs:
        sz = types.structs[r]["size"] or 0
        return "S", r, sz
    return "?", base, 0


def main(argv):
    repo = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parents[2]
    gen_dir = repo / "carrier" / "gen"
    idx = json.loads((gen_dir / "interop_index.json").read_text())
    types = Types(gen_dir)

    globals_out = []
    for g in idx["globals"]:
        name = g["name"]
        decl = g["type"]
        base, ptr, dims = split_decl(decl)
        kind, a, b = classify(types, base)
        fixed = types.is_fixed(base) if not ptr and not dims else False
        dims2 = (dims + [0, 0])[:2]
        globals_out.append({
            "name": name, "va": g["va"], "orig_type": decl,
            "kind": kind, "base": a if kind != "P" else base, "size": b if kind == "S" else (a if kind == "P" else 0),
            "prim_size": a if kind == "P" else 0, "prim_kind": b if kind == "P" else "",
            "ptr": ptr, "dims": dims2, "ndims": len(dims), "fixed": fixed,
        })

    members_out = []
    for sname, st in types.structs.items():
        for m in st["members"]:
            if "offset" not in m:
                continue  # DWARF gave no offsetof check for this member; skip rather than guess
            base, ptr0, dims0 = m["type"], m["ptr"], m["dims"]
            # m["type"] here is already the bare type token (stars stripped by
            # the regex into `ptr`), so no further split_decl needed.
            kind, a, b = classify(types, base)
            fixed = types.is_fixed(base) if not ptr0 and not dims0 else False
            dims2 = (dims0 + [0, 0])[:2]
            members_out.append({
                "struct": sname, "name": m["name"], "offset": m["offset"],
                "orig_type": m["type"] + ("*" * ptr0),
                "kind": kind, "base": a if kind != "P" else base, "size": b if kind == "S" else (a if kind == "P" else 0),
                "prim_size": a if kind == "P" else 0, "prim_kind": b if kind == "P" else "",
                "ptr": ptr0, "dims": dims2, "ndims": len(dims0), "fixed": fixed,
            })

    struct_sizes = [{"name": n, "size": st["size"] or 0} for n, st in sorted(types.structs.items())]

    out = []
    out.append("/* GENERATED FILE -- DO NOT EDIT.")
    out.append(" * Produced by carrier/gen/gen_print_globals.py from interop_index.json,")
    out.append(" * it_types.h and it_types_check.c. See that script's docstring and")
    out.append(" * carrier/src/print_globals.cpp for how this table is walked at runtime.")
    out.append(" * kind: 'P' primitive (size/prim_kind valid), 'S' struct (base names it,")
    out.append(" * size is sizeof), '?' opaque/unresolved (printed as raw bytes).")
    out.append(" * prim_kind: 'I' signed, 'U' unsigned, 'F' float, 'D' double, 'C' char. */")
    out.append("#ifndef IT_PRINT_GLOBALS_INC")
    out.append("#define IT_PRINT_GLOBALS_INC")
    out.append("")
    out.append("struct PfPGVal { char kind; unsigned base_size; char base_name[24]; "
                "char prim_kind; int ptr; int ndims; int dims[2]; int is_fixed; };")
    out.append("struct PfPGGlobal { const char* name; unsigned va; const char* orig_type; struct PfPGVal v; };")
    out.append("struct PfPGMember { const char* struct_name; const char* name; unsigned offset; "
                "const char* orig_type; struct PfPGVal v; };")
    out.append("struct PfPGStructSize { const char* name; unsigned size; };")
    out.append("")

    def val_init(e):
        base_c = (e["base"] if e["kind"] != "P" else "")[:23]
        pk = e["prim_kind"] if e["kind"] == "P" else " "
        return ("{{'{k}', {sz}u, \"{bn}\", '{pk}', {ptr}, {nd}, {{{d0},{d1}}}, {fx}}}"
                .format(k=e["kind"], sz=e["size"], bn=base_c, pk=pk, ptr=e["ptr"],
                        nd=e["ndims"], d0=e["dims"][0], d1=e["dims"][1],
                        fx=1 if e["fixed"] else 0))

    out.append("static const struct PfPGGlobal kPGGlobals[] = {")
    for e in globals_out:
        out.append("  {{ {nm}, {va}u, {ot}, {val} }},".format(
            nm=c_str(e["name"]), va=e["va"], ot=c_str(e["orig_type"]), val=val_init(e)))
    out.append("};")
    out.append("static const int kPGNumGlobals = {};".format(len(globals_out)))
    out.append("")

    out.append("static const struct PfPGMember kPGMembers[] = {")
    for e in members_out:
        out.append("  {{ {sn}, {nm}, {off}u, {ot}, {val} }},".format(
            sn=c_str(e["struct"]), nm=c_str(e["name"]), off=e["offset"],
            ot=c_str(e["orig_type"]), val=val_init(e)))
    out.append("};")
    out.append("static const int kPGNumMembers = {};".format(len(members_out)))
    out.append("")

    out.append("static const struct PfPGStructSize kPGStructSizes[] = {")
    for s in struct_sizes:
        out.append("  {{ {nm}, {sz}u }},".format(nm=c_str(s["name"]), sz=s["size"]))
    out.append("};")
    out.append("static const int kPGNumStructSizes = {};".format(len(struct_sizes)))
    out.append("")
    out.append("#endif /* IT_PRINT_GLOBALS_INC */")

    out_path = gen_dir / "it_print_globals.inc"
    out_path.write_text("\n".join(out) + "\n")
    print(f"gen_print_globals.py: wrote {out_path} "
          f"({len(globals_out)} globals, {len(members_out)} struct members, "
          f"{len(struct_sizes)} structs)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
