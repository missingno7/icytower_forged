"""assets_dwarf_tables.py - list compiled-in DATA tables of the game CUs.

Parses artifacts/dwarf_info.txt, resolves every top-level DW_TAG_variable that
has a fixed DW_OP_addr location, resolves its type chain (arrays, structs,
pointers, base types) and reports name, VA, byte size, element type and the
owning compile unit.  Also dumps the initialised bytes for string/array tables
from the EXE.

Usage:
    python tools_recon/assets_dwarf_tables.py > artifacts/assets_extract/dwarf_tables.txt
    python tools_recon/assets_dwarf_tables.py --json artifacts/assets_extract/dwarf_tables.json
"""
import re, json, sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DWARF = os.path.join(ROOT, "artifacts", "dwarf_info.txt")

die_re = re.compile(r"^ <(\d+)><([0-9a-fA-F]+)>: Abbrev Number: (\d+)(?: \((\S+)\))?")
attr_re = re.compile(r"^\s+<[0-9a-fA-F]+>\s+(DW_AT_\w+)\s*:\s*(.*)$")
ref_re = re.compile(r"<0x([0-9a-fA-F]+)>")

# section bounds (from pefile)
SECTIONS = [(".text", 0x401000, 0x4BC000), (".data", 0x4BC000, 0x4D4000),
            (".rdata", 0x4D4000, 0x4DD000), (".bss", 0x4DD000, 0x514000)]


def section_of(va):
    for n, lo, hi in SECTIONS:
        if lo <= va < hi:
            return n
    return "?"


def parse():
    dies = {}       # offset -> die
    order = []
    cur = None
    cu_name = None
    with open(DWARF, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = die_re.match(line)
            if m:
                depth = int(m.group(1))
                off = int(m.group(2), 16)
                tag = m.group(4)
                cur = {"off": off, "depth": depth, "tag": tag, "at": {},
                       "cu": cu_name}
                dies[off] = cur
                order.append(cur)
                continue
            m = attr_re.match(line)
            if m and cur is not None:
                k, v = m.group(1), m.group(2).strip()
                cur["at"][k] = v
                if k == "DW_AT_name" and cur["tag"] == "DW_TAG_compile_unit":
                    cu_name = v
                    cur["cu"] = v
    return dies, order


def ref(die, attr, dies):
    v = die["at"].get(attr)
    if not v:
        return None
    m = ref_re.search(v)
    if not m:
        return None
    return dies.get(int(m.group(1), 16))


def type_name(t, dies, depth=0):
    if t is None or depth > 12:
        return "?"
    tag = t["tag"]
    nm = t["at"].get("DW_AT_name")
    if tag in ("DW_TAG_base_type", "DW_TAG_typedef"):
        return nm or "?"
    if tag == "DW_TAG_structure_type":
        return "struct %s" % (nm or "<anon>")
    if tag == "DW_TAG_union_type":
        return "union %s" % (nm or "<anon>")
    if tag == "DW_TAG_enumeration_type":
        return "enum %s" % (nm or "<anon>")
    if tag == "DW_TAG_pointer_type":
        return type_name(ref(t, "DW_AT_type", dies), dies, depth + 1) + "*"
    if tag == "DW_TAG_const_type":
        return "const " + type_name(ref(t, "DW_AT_type", dies), dies, depth + 1)
    if tag == "DW_TAG_volatile_type":
        return "volatile " + type_name(ref(t, "DW_AT_type", dies), dies, depth + 1)
    if tag == "DW_TAG_array_type":
        return type_name(ref(t, "DW_AT_type", dies), dies, depth + 1) + "[]"
    return tag


def type_size(t, dies, order, depth=0):
    """byte size of a type DIE."""
    if t is None or depth > 12:
        return None
    bs = t["at"].get("DW_AT_byte_size")
    if bs is not None:
        try:
            return int(bs.split()[0], 0)
        except ValueError:
            pass
    if t["tag"] == "DW_TAG_pointer_type":
        return 4
    if t["tag"] in ("DW_TAG_const_type", "DW_TAG_volatile_type", "DW_TAG_typedef"):
        return type_size(ref(t, "DW_AT_type", dies), dies, order, depth + 1)
    if t["tag"] == "DW_TAG_array_type":
        el = type_size(ref(t, "DW_AT_type", dies), dies, order, depth + 1)
        if el is None:
            return None
        n = 1
        # subrange children follow the array DIE in document order
        i = order.index(t) if t in order else None
        if i is None:
            return None
        d = t["depth"]
        for k in range(i + 1, min(i + 16, len(order))):
            c = order[k]
            if c["depth"] <= d:
                break
            if c["tag"] == "DW_TAG_subrange_type":
                ub = c["at"].get("DW_AT_upper_bound")
                if ub is None:
                    return None
                n *= int(ub.split()[0], 0) + 1
        return el * n
    return None


def main():
    dies, order = parse()
    idx = {id(d): i for i, d in enumerate(order)}
    order_index = {}
    for i, d in enumerate(order):
        order_index[d["off"]] = i

    class OL(list):
        def index(self, x, *a):
            return order_index[x["off"]]
    ol = OL(order)

    rows = []
    for d in order:
        if d["tag"] != "DW_TAG_variable" or d["depth"] != 1:
            continue
        loc = d["at"].get("DW_AT_location", "")
        m = re.search(r"DW_OP_addr:?\s*([0-9a-fA-F]+)", loc)
        if not m:
            continue
        va = int(m.group(1), 16)
        t = ref(d, "DW_AT_type", dies)
        sz = type_size(t, dies, ol)
        cu = d["cu"] or ""
        rows.append({
            "name": d["at"].get("DW_AT_name"),
            "va": "0x%08x" % va, "va_int": va,
            "size": sz,
            "type": type_name(t, dies),
            "elem": type_name(ref(t, "DW_AT_type", dies), dies)
                    if t is not None and t["tag"] == "DW_TAG_array_type" else None,
            "section": section_of(va),
            "cu": os.path.basename(cu.replace("\\", "/")) if cu else None,
            "cu_full": cu,
            "decl_line": d["at"].get("DW_AT_decl_line"),
            "external": bool(d["at"].get("DW_AT_external")),
        })
    rows.sort(key=lambda r: r["va_int"])
    if "--json" in sys.argv:
        p = sys.argv[sys.argv.index("--json") + 1]
        open(p, "w").write(json.dumps(rows, indent=1))
        print("wrote", p, len(rows), "variables")
    else:
        for r in rows:
            print("%s %-8s %-7s %-28s %-30s %s" % (
                r["va"], r["section"], str(r["size"]), r["name"] or "?",
                r["type"], r["cu"]))


if __name__ == "__main__":
    main()
