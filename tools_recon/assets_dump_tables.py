"""assets_dump_tables.py - dump the contents of the game's compiled-in tables.

Reads the EXE read-only and prints the initialised value of each named table,
resolving char*[] into the pointed-to strings.
"""
import json, os, sys, struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools_recon"))
import assets_exe_read as X

WANT = {
    "hisc_names": "cstrs", "category_names": "cstrs", "hints": "cstrs",
    "start_speeds": "ints", "version_str": "cstr1",
    "scroller_greetings": "chars", "init_string": "chars",
    "floor_size_modifiers": "ints", "max_speed": "doubles",
    "gravity_modifier": "doubles", "jcLabels": "cstrs", "rankLables": "cstrs",
    "rankFloors": "ints", "rankCombos": "ints", "rankCCCs": "ints",
    "rankNMLs": "ints", "comboNames": "cstrs", "REPLAY_HEADER": "chars",
    "full_weekdays": "cstrs", "abb_weekdays": "cstrs", "full_month": "cstrs",
    "abb_month": "cstrs", "ampm": "cstrs",
}


def main():
    rows = json.load(open(os.path.join(ROOT, "artifacts/assets_extract/dwarf_tables.json")))
    by = {r["name"]: r for r in rows}
    out = {}
    for name, kind in WANT.items():
        r = by.get(name)
        if not r:
            print("!! missing", name)
            continue
        va, size = r["va_int"], r["size"]
        b = X.read(va, size)
        if kind == "cstrs":
            vals = []
            for i in range(0, size - 3, 4):
                p = struct.unpack("<I", b[i:i + 4])[0]
                vals.append(X.cstr(p).decode("latin-1") if p else None)
        elif kind == "ints":
            vals = list(struct.unpack("<%di" % (size // 4), b[:size // 4 * 4]))
        elif kind == "doubles":
            vals = list(struct.unpack("<%dd" % (size // 8), b[:size // 8 * 8]))
        elif kind == "cstr1":
            p = struct.unpack("<I", b[:4])[0]
            vals = X.cstr(p).decode("latin-1")
        else:
            vals = b.decode("latin-1")
        out[name] = {"va": r["va"], "size": size, "type": r["type"],
                     "cu": r["cu"], "values": vals}
        print("%-20s %s %-5s %s" % (name, r["va"], size, json.dumps(vals)[:800]))
    p = os.path.join(ROOT, "artifacts/assets_extract/embedded_tables.json")
    open(p, "w").write(json.dumps(out, indent=1))
    print("\nwrote", p)


if __name__ == "__main__":
    main()
