import json, os, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
f = json.load(open(os.path.join(ROOT, "artifacts", "dwarf_subprograms.json")))
pat = sys.argv[1].lower() if len(sys.argv) > 1 else ""
by = collections.defaultdict(list)
for x in f:
    cu = (x.get("cu_name") or "").replace("\\", "/")
    if "icytower" not in cu.lower():
        continue
    by[os.path.basename(cu)].append((x["low_pc"], x["name"]))
for cu in sorted(by):
    rows = sorted(by[cu])
    sel = [r for r in rows if not pat or pat in (r[1] or "").lower() or pat in cu.lower()]
    if not sel:
        continue
    print("%s  (%d funcs)" % (cu, len(rows)))
    for a, n in sel:
        print("   ", a, n)
