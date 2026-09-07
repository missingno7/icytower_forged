"""assets_report_tables.py - filter the DWARF variable dump to game-owned
initialised data (.data/.rdata) and dump the initialised bytes."""
import json, os, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools_recon"))
import assets_exe_read as X

rows = json.load(open(os.path.join(ROOT, "artifacts/assets_extract/dwarf_tables.json")))
cus = json.load(open(os.path.join(ROOT, "artifacts/dwarf_cus.json")))
game = set()
for c in cus:
    n = (c.get("name") or "").replace("\\", "/")
    if "/icytower/" in n.lower() or "projects/icytower" in n.lower():
        game.add(os.path.basename(n))
print("# game CUs:", len(game))
g = [r for r in rows if r["cu"] in game]
print("# game vars with fixed address:", len(g))
print("#", collections.Counter(r["section"] for r in g))
data = sorted([r for r in g if r["section"] in (".data", ".rdata")],
              key=lambda x: x["va_int"])
print("# initialised game tables (.data/.rdata):", len(data))
print()
for r in data:
    print("%s %-6s %-7s %-28s %-36s %s" % (
        r["va"], r["section"], r["size"], r["name"], r["type"], r["cu"]))
    if "--bytes" in sys.argv and r["size"] and r["size"] <= 2048:
        b = X.read(r["va_int"], r["size"])
        pr = "".join(chr(c) if 32 <= c < 127 else "." for c in b)
        print("      | " + pr[:900])
