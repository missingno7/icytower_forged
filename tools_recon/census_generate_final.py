import json, importlib.util, sys
from collections import defaultdict, Counter

ROOT = r"D:\Games\DOS\dos_recosystem\icytower_forged"

spec = importlib.util.spec_from_file_location("ct", ROOT + r"\tools_recon\census_classification_table.py")
ct = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ct)
T = ct.T

imports = json.load(open(ROOT + r"\imports.json", encoding="utf-8"))
callers = json.load(open(ROOT + r"\tools_recon\census_import_callers.json", encoding="utf-8"))
callers_by_key = {(c["dll"], c["name"], c["iat"]): c for c in callers}

records = []
for dll, name, iat_hex in imports:
    cls, cb, rationale = T[name]
    c = callers_by_key[(dll, name, iat_hex)]
    all_sites = c["callsites"] + c["data_refs"]
    # dedupe callers preserving order, prefer variety of origins
    seen = []
    seen_names = set()
    for site in all_sites:
        fn = site["func_name"]
        if fn not in seen_names:
            seen_names.add(fn)
            seen.append({"va": site["site_va"], "func": fn, "origin": site["origin"]})
    records.append({
        "dll": dll,
        "name": name,
        "iat": iat_hex,
        "class": cls,
        "callback": cb,
        "num_callsites": c["num_callsites"],
        "num_data_refs": c["num_data_refs"],
        "origins": c["origins"],
        "callers": seen,
        "rationale": rationale,
    })

json.dump(records, open(ROOT + r"\artifacts\import_census.json", "w"), indent=1)
print("wrote artifacts/import_census.json with", len(records), "records")

# ---- summary stats ----
class_counts = Counter(r["class"] for r in records)
dll_counts = Counter(r["dll"] for r in records)
class_by_dll = defaultdict(Counter)
for r in records:
    class_by_dll[r["dll"]][r["class"]] += 1

direct_wrap = class_counts["DIRECT"] + class_counts["WRAP"]
pct_direct_wrap = 100.0 * direct_wrap / len(records)

game_direct_calls = []  # imports with a GAME-origin caller
for r in records:
    if "GAME" in r["origins"]:
        game_direct_calls.append(r)

callback_bearing = [r for r in records if r["callback"]]

print("class_counts", class_counts)
print("pct DIRECT+WRAP: %.1f%%" % pct_direct_wrap)
print("game-code OS surface size:", len(game_direct_calls))
print("callback-bearing imports:", len(callback_bearing))

json.dump({
    "class_counts": dict(class_counts),
    "dll_counts": dict(dll_counts),
    "class_by_dll": {k: dict(v) for k, v in class_by_dll.items()},
    "pct_direct_wrap": pct_direct_wrap,
    "game_surface_count": len(game_direct_calls),
    "callback_bearing_count": len(callback_bearing),
}, open(ROOT + r"\tools_recon\census_summary_stats.json", "w"), indent=1)
