import json, bisect

ROOT = r"D:\Games\DOS\dos_recosystem\icytower_forged"

imports = json.load(open(ROOT + r"\imports.json", encoding="utf-8"))
callsite_data = json.load(open(ROOT + r"\tools_recon\census_callsites.json", encoding="utf-8"))
coff = json.load(open(ROOT + r"\artifacts\coff_symbols.json", encoding="utf-8"))
cus = json.load(open(ROOT + r"\tools_recon\census_cu_ranges.json", encoding="utf-8"))

cus.sort(key=lambda c: c["low"])
cu_los = [c["low"] for c in cus]

name_to_file = {}
for s in coff:
    n = s.get("name")
    if n and n not in name_to_file:
        name_to_file[n] = s.get("file")

CRT_FALLBACK_FILES = {
    "crt1.c", "cygming-crtbegin.c", "cygming-crtend.c", "cygming-shared-data.c",
    "CRT_fp10.c", "CRTfmode.c", "CRTglob.c", "gccmain.c", "pseudo-reloc.c",
    "pseudo-reloc-list.c", "mbrtowc.c", "wcrtomb.c", "snprintf.c", "vsnprintf.c",
    "strtof.c", "strtodg.c", "strtodnrp.c", "hexnan.c", "gdtoa.c", "gethex.c",
    "fpclassify.c", "libgcc2.c", "cpu_features.c", "mbsrtowcs.c", "CRTinit.c",
    "tlssup.c", "tlsthrd.c", "tlsmthread.c", "tlsmcrt.c", "natstart.c",
    # mingw runtime "main.c" (argv/argc + WinMain shim) and gdtoa's misc.c/sum.c,
    # mingw's printf engine pformat.c -- none of these have a DWARF CU (no -g),
    # and are distinct from icytower's own main.c (which IS DWARF-covered separately)
    "main.c", "misc.c", "sum.c", "pformat.c",
}

VORBIS_OGG_FALLBACK_FILES = {
    "analysis.c", "bitrate.c", "bitwise.c", "block.c", "codebook.c", "envelope.c",
    "floor0.c", "floor1.c", "framing.c", "info.c", "lpc.c", "lsp.c", "mapping0.c",
    "mdct.c", "psy.c", "res0.c", "sharedbook.c", "smallft.c", "vorbisfile.c",
    "window.c", "synthesis.c", "registry.c",
}

def classify_path(va, func_name):
    # 1) DWARF CU range (authoritative, full path)
    idx = bisect.bisect_right(cu_los, va) - 1
    if idx >= 0:
        cu = cus[idx]
        if cu["low"] <= va <= cu["high"]:
            path = cu["name"]
            lp = path.lower()
            if "icytower" in lp:
                return "GAME", path
            if "allegro4" in lp:
                return "ALLEGRO", path
            if "gcc-4.4.1" in lp or "libgcc" in lp:
                return "CRT", path
            base = path.replace("/", "\\").rsplit("\\", 1)[-1]
            if base in VORBIS_OGG_FALLBACK_FILES:
                return "VORBIS_OGG", path
            if base in CRT_FALLBACK_FILES:
                return "CRT", path
            return "UNKNOWN", path
    # 2) COFF file fallback (filename only, less precise)
    f = name_to_file.get(func_name)
    if f:
        if f in CRT_FALLBACK_FILES or f == "fake":
            return "CRT", f
        if f in VORBIS_OGG_FALLBACK_FILES:
            return "VORBIS_OGG", f
        return "UNKNOWN", f
    return "UNKNOWN", None

# classify every callsite and data_ref
def enrich(entries, va_key, name_key):
    out = []
    for e in entries:
        fv = int(e[va_key], 16)
        fn = e[name_key]
        origin, path = classify_path(fv, fn)
        e2 = dict(e)
        e2["origin"] = origin
        e2["origin_path"] = path
        out.append(e2)
    return out

callsites = enrich(callsite_data["callsites"], "func_va", "func_name")
data_refs = enrich(callsite_data["data_refs"], "func_va", "func_name")

# aggregate per iat address
from collections import defaultdict
agg = defaultdict(lambda: {"callsites": [], "data_refs": []})
for c in callsites:
    agg[c["iat"]]["callsites"].append(c)
for c in data_refs:
    agg[c["iat"]]["data_refs"].append(c)

result = []
for dll, name, iat_hex in imports:
    key = hex(int(iat_hex, 16))
    a = agg.get(key, {"callsites": [], "data_refs": []})
    origins = sorted(set(c["origin"] for c in a["callsites"]) | set(c["origin"] for c in a["data_refs"]))
    result.append({
        "dll": dll, "name": name, "iat": iat_hex,
        "num_callsites": len(a["callsites"]),
        "num_data_refs": len(a["data_refs"]),
        "origins": origins,
        "callsites": a["callsites"],
        "data_refs": a["data_refs"],
    })

json.dump(result, open(ROOT + r"\tools_recon\census_import_callers.json", "w"), indent=1)

# sanity print
from collections import Counter
orig_counter = Counter()
for r in result:
    for o in r["origins"]:
        orig_counter[o] += 1
print("imports touched by each origin (import may have multiple origins):")
for o, c in orig_counter.most_common():
    print(" ", o, c)

no_callers = [r for r in result if not r["origins"]]
print("imports with zero resolved callers:", len(no_callers))
for r in no_callers:
    print("  ", r["dll"], r["name"])

# spot-check png callers
print("--- png_read_info callers ---")
for r in result:
    if r["name"] == "png_read_info":
        for c in r["callsites"][:6]:
            print(" ", c["func_name"], c["origin"], c["origin_path"])
