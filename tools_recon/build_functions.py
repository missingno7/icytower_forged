import json, re, sys

ART = r"D:\Games\DOS\dos_recosystem\icytower_forged\artifacts"

dwarf_funcs = json.load(open(ART + r"\dwarf_subprograms.json"))
cus = json.load(open(ART + r"\dwarf_cus.json"))
coff = json.load(open(ART + r"\coff_symbols.json"))

def to_int(v):
    if v is None:
        return None
    v = v.strip()
    m = re.match(r'0x([0-9a-fA-F]+)', v)
    if m:
        return int(m.group(1), 16)
    try:
        return int(v)
    except ValueError:
        return None

MINGW_CRT_BASENAMES = {
    "strtodg.c", "smisc.c", "mbrtowc.c", "dmisc.c", "gccmain.c", "wcrtomb.c",
    "pseudo-reloc.c", "crt1.c", "cygming-crtbegin.c", "cygming-crtend.c",
    "natstart.c", "gs_support.c", "tlssup.c", "crtdll.c", "tlsthrd.c",
    "tlsmthread.c", "tlsmcrt.c", "cinitexe.c", "merr.c", "mingw_helpers.c",
    "CRT_fp10.c", "snprintf.c", "strtof.c", "vsnprintf.c", "gmisc.c",
    "cpu_features.c", "strtodnrp.c", "sum.c", "gethex.c", "hexnan.c",
    "gdtoa.c", "fpclassify.c", "hd_init.c", "main.c", "fake",
}
VORBIS_OGG_BASENAMES = {
    "vorbisfile.c", "bitwise.c", "framing.c", "psy.c", "block.c", "info.c",
    "sharedbook.c", "res0.c", "pformat.c", "misc.c", "floor1.c", "smallft.c",
    "codebook.c", "lsp.c", "envelope.c", "floor0.c", "synthesis.c", "mdct.c",
    "bitrate.c", "mapping0.c", "lpc.c", "window.c", "analysis.c", "vorbisenc.c",
    "registry.c", "lookup.c",
}

def classify(path, producer=None):
    if path is None:
        return "other"
    p = path.replace("/", "\\")
    base = p.split("\\")[-1]
    if p.startswith("F:\\projects\\icytower"):
        return "game"
    if "allegro4" in p.lower():
        return "allegro"
    if base in VORBIS_OGG_BASENAMES:
        return "other"  # libvorbis/libogg (statically linked via Allegro logg addon)
    if "gcc-4.4.1" in p or "libgcc2.c" in p or "cygwin.asm" in p or "cygming" in p:
        return "libgcc/crt"
    if base in MINGW_CRT_BASENAMES:
        return "libgcc/crt"
    return "other"

# ---- 1. DWARF-derived functions ----
functions = []
covered_ranges = []  # (low, high) for overlap-skip when adding COFF-only entries

for fn in dwarf_funcs:
    low = to_int(fn.get("low_pc"))
    high = to_int(fn.get("high_pc"))
    if low is None or high is None or high <= low:
        continue
    cu_name = fn.get("cu_name")
    origin = classify(cu_name, fn.get("cu_producer"))
    functions.append({
        "name": fn.get("name"),
        "va": hex(low),
        "size": high - low,
        "compile_unit": cu_name,
        "origin": origin,
        "source": "dwarf",
    })
    covered_ranges.append((low, high))

covered_ranges.sort()

def in_covered(addr):
    # binary search
    import bisect
    idx = bisect.bisect_right(covered_ranges, (addr, float('inf'))) - 1
    if idx >= 0:
        lo, hi = covered_ranges[idx]
        if lo <= addr < hi:
            return True
    return False

# ---- 2. COFF-only .text functions (no DWARF coverage) ----
text_syms = [s for s in coff if s.get("sec") == 1 and s.get("ty") == 20 and not s.get("is_section_symbol")]
text_syms.sort(key=lambda s: s["value"])

# also gather ALL .text symbol values (any type) to bound sizes for coff-only funcs
all_text_vals = sorted(set(s["va"] for s in coff if s.get("sec") == 1 and s.get("va") is not None))

import bisect
def next_boundary(addr):
    i = bisect.bisect_right(all_text_vals, addr)
    if i < len(all_text_vals):
        return all_text_vals[i]
    return None

TEXT_END = 0x00401000 + 0x000ba048  # VMA + Size from section header

coff_only_count = 0
for s in text_syms:
    va = s["va"]
    if va is None:
        continue
    if in_covered(va):
        continue
    nb = next_boundary(va)
    if nb is None:
        nb = TEXT_END
    size = nb - va
    if size <= 0 or size > 0x20000:
        # unreliable, still record but flag
        pass
    origin = classify(s.get("file"))
    functions.append({
        "name": s.get("name"),
        "va": hex(va),
        "size": size,
        "compile_unit": s.get("file"),
        "origin": origin,
        "source": "coff",
    })
    coff_only_count += 1

functions.sort(key=lambda f: int(f["va"], 16))

print(f"DWARF-derived: {len(dwarf_funcs)} raw -> {len(covered_ranges)} with valid ranges", file=sys.stderr)
print(f"COFF-only additions: {coff_only_count}", file=sys.stderr)
print(f"Total functions: {len(functions)}", file=sys.stderr)

from collections import Counter
c = Counter(f["origin"] for f in functions)
print("Origin counts:", dict(c), file=sys.stderr)

json.dump(functions, open(ART + r"\functions.json", "w"), indent=1)

# ---- compile_units.txt ----
with open(ART + r"\compile_units.txt", "w", encoding="utf-8") as f:
    f.write(f"{'CU source path':70s} {'origin':12s} producer\n")
    seen = set()
    for cu in cus:
        name = cu.get("name") or "(anonymous)"
        origin = classify(name, cu.get("producer"))
        key = (name, cu.get("low_pc"))
        f.write(f"{name:70s} {origin:12s} {cu.get('producer')}\n")
    # also note CRT files present only as COFF File symbols, no DWARF CU
    f.write("\n-- COFF 'File' entries with no corresponding DWARF compile_unit (no -g debug info) --\n")
    coff_files = sorted(set(s.get("file") for s in coff if s.get("file")))
    dwarf_cu_basenames = set()
    for cu in cus:
        n = cu.get("name")
        if n:
            dwarf_cu_basenames.add(n.replace("\\", "/").split("/")[-1])
    for cf in coff_files:
        base = cf.replace("\\", "/").split("/")[-1]
        if base not in dwarf_cu_basenames:
            f.write(f"{cf:40s} {classify(cf)}\n")

print("wrote functions.json and compile_units.txt", file=sys.stderr)
