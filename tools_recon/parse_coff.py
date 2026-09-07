import re, json, sys

OBJDUMP_T = r"D:\Games\DOS\dos_recosystem\icytower_forged\artifacts\objdump_t.txt"
OUT = r"D:\Games\DOS\dos_recosystem\icytower_forged\artifacts\coff_symbols.json"

# section number (1-based, as used in symbol table "sec N") -> (name, VMA)
SECTIONS = {
    1: (".text", 0x00401000),
    2: (".data", 0x004bc000),
    3: (".rdata", 0x004d4000),
    4: (".bss", 0x004dd000),
    5: (".idata", 0x00514000),
    6: (".rsrc", 0x00517000),
    7: (".debug_aranges", 0x00518000),
    8: (".debug_pubnames", 0x0051a000),
    9: (".debug_info", 0x00525000),
    10: (".debug_abbrev", 0x006bf000),
    11: (".debug_line", 0x006d7000),
    12: (".debug_frame", 0x006fd000),
    13: (".debug_str", 0x0070d000),
    14: (".debug_loc", 0x00717000),
    15: (".debug_ranges", 0x00782000),
}
SECTION_NAMES = {v[0] for v in SECTIONS.values()}

line_re = re.compile(
    r'^\[\s*(\d+)\]\(sec\s*(-?\d+)\)\(fl 0x([0-9a-fA-F]+)\)\(ty\s*(\d+)\)\(scl\s*(\d+)\)\s*\(nx (\d+)\)\s*0x([0-9a-fA-F]+)\s+(.*)$'
)

symbols = []  # dict entries
cur_file = None

with open(OBJDUMP_T, encoding="utf-8", errors="replace") as f:
    lines = f.readlines()

i = 0
n = len(lines)
while i < n:
    line = lines[i].rstrip("\n")
    m = line_re.match(line)
    if not m:
        i += 1
        continue
    idx, sec, fl, ty, scl, nx, val, name = m.groups()
    sec = int(sec)
    ty = int(ty)
    scl = int(scl)
    nx = int(nx)
    val = int(val, 16)
    name = name.strip()

    # File symbol: scl 103 (C_FILE), sec -2, next line is "File ..." with actual filename possibly
    if scl == 103:
        # the printed "name" here is usually the source filename directly for pei-i386 objdump output
        cur_file = name
        # skip the following "File" aux line(s)
        i += 1
        # consume aux lines starting with "File" or "AUX"
        while i < n and (lines[i].startswith("File") or lines[i].startswith("AUX")):
            i += 1
        continue

    is_section_symbol = (sec > 0 and name in SECTION_NAMES)

    symbols.append({
        "table_idx": int(idx),
        "sec": sec,
        "ty": ty,
        "scl": scl,
        "value": val,
        "name": name,
        "file": cur_file,
        "is_section_symbol": is_section_symbol,
    })

    i += 1
    # skip AUX continuation lines for this symbol (not File)
    while i < n and lines[i].startswith("AUX"):
        i += 1

print(f"Parsed {len(symbols)} symbol records", file=sys.stderr)

# compute VA
for s in symbols:
    sec = s["sec"]
    if sec in SECTIONS:
        s["va"] = SECTIONS[sec][1] + s["value"]
        s["section_name"] = SECTIONS[sec][0]
    else:
        s["va"] = None
        s["section_name"] = None

json.dump(symbols, open(OUT, "w"), indent=0)
print(f"Wrote {OUT}", file=sys.stderr)

# quick stats
text_funcs = [s for s in symbols if s["sec"] == 1 and s["ty"] == 20 and not s["is_section_symbol"]]
print(f".text function-typed (ty=20) symbols: {len(text_funcs)}", file=sys.stderr)
