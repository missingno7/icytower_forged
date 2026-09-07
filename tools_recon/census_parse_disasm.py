import json, re, sys, bisect

ROOT = r"D:\Games\DOS\dos_recosystem\icytower_forged"
DISASM = ROOT + r"\artifacts\icytower_disasm.txt"
IMPORTS = ROOT + r"\imports.json"
COFF = ROOT + r"\artifacts\coff_symbols.json"
CU_RANGES = ROOT + r"\tools_recon\census_cu_ranges.json"

imports = json.load(open(IMPORTS, encoding="utf-8"))
iat_map = {}
for dll, name, iat_hex in imports:
    iat_map[int(iat_hex, 16)] = (dll, name)

print("Loaded", len(iat_map), "unique IAT slots (", len(imports), "import entries)")

label_re = re.compile(r'^([0-9a-f]{8}) <(.+)>:$')
insn_re = re.compile(r'^\s*([0-9a-f]+):\t')
call_indirect_re = re.compile(r'\t(call|jmp)\s+\*0x([0-9a-f]+)')
call_direct_re = re.compile(r'\t(call|jmp)\s+([0-9a-f]+) <(.+)>')

# Pass 1: collect all labels and thunk map (label whose body is a single "jmp *0x514xxx")
labels = []  # (va, name)
thunk_map = {}  # thunk_va -> iat_addr

with open(DISASM, encoding="utf-8", errors="replace") as f:
    lines = f.readlines()

print("disasm lines:", len(lines))

mov_load_re = re.compile(r'\tmov\s+0x([0-9a-f]+),%(e[a-z]{2})\b')
reg_indirect_re = re.compile(r'\t(call|jmp)\s+\*%(e[a-z]{2})\b')

cur_label_va = None
pending_reg = None  # (reg, iat_addr) seen since start of this function body
for i, line in enumerate(lines):
    m = label_re.match(line)
    if m:
        va = int(m.group(1), 16)
        name = m.group(2)
        labels.append((va, name))
        cur_label_va = va
        pending_reg = None
        continue
    m = insn_re.match(line)
    if m and cur_label_va is not None:
        insn_va = int(m.group(1), 16)
        mi = call_indirect_re.search(line)
        if mi and mi.group(1) == 'jmp':
            target = int(mi.group(2), 16)
            if target in iat_map:
                # this label is (or contains at this point) a thunk to an import
                thunk_map[cur_label_va] = target
        # two-instruction thunk pattern: mov 0x514xxx,%reg ; ... ; jmp/call *%reg
        mm = mov_load_re.search(line)
        if mm:
            addr = int(mm.group(1), 16)
            if addr in iat_map:
                pending_reg = (mm.group(2), addr)
        mr = reg_indirect_re.search(line)
        if mr and pending_reg and mr.group(2) == pending_reg[0]:
            thunk_map.setdefault(cur_label_va, pending_reg[1])

labels.sort(key=lambda x: x[0])
label_vas = [x[0] for x in labels]
label_names = [x[1] for x in labels]

print("labels:", len(labels), "thunks:", len(thunk_map))

def containing_func(va):
    idx = bisect.bisect_right(label_vas, va) - 1
    if idx < 0:
        return None, None
    return label_vas[idx], label_names[idx]

# Pass 2: collect call sites
callsites = []  # (site_va, iat_addr, func_va, func_name, mode)
cur_label_va = None
cur_label_name = None
for line in lines:
    m = label_re.match(line)
    if m:
        cur_label_va = int(m.group(1), 16)
        cur_label_name = m.group(2)
        continue
    m = insn_re.match(line)
    if not m or cur_label_va is None:
        continue
    insn_va = int(m.group(1), 16)
    mi = call_indirect_re.search(line)
    if mi:
        op = mi.group(1)
        target = int(mi.group(2), 16)
        # skip the thunk's own defining instruction (jmp *0x514xxx that IS the thunk body,
        # not a real call site from a distinct caller)
        if target in iat_map and not (cur_label_va in thunk_map and thunk_map[cur_label_va] == target and insn_va == cur_label_va):
            callsites.append((insn_va, target, cur_label_va, cur_label_name, "indirect_" + op))
        continue
    md = call_direct_re.search(line)
    if md:
        op = md.group(1)
        target = int(md.group(2), 16)
        if target in thunk_map:
            callsites.append((insn_va, thunk_map[target], cur_label_va, cur_label_name, "thunk_" + op))
        continue

print("callsites found:", len(callsites))

# Which imports have zero call sites found
called_iats = set(c[1] for c in callsites)
uncalled = [ (dll,name,iat_hex) for dll,name,iat_hex in imports if int(iat_hex,16) not in called_iats ]
print("imports with NO call site found:", len(uncalled))
for dll,name,iat_hex in uncalled:
    print("  ", dll, name, iat_hex)

# For imports that still have zero callsites (likely pure DATA imports, e.g. msvcrt __iob/_pctype/_tzname/__mb_cur_max/__lc_codepage),
# scan for any instruction referencing the IAT slot address directly (not via *0x / not via mov-then-indirect-jmp already handled).
called_iats2 = set(c[1] for c in callsites)
remaining = { int(h,16): (d,n) for d,n,h in imports if int(h,16) not in called_iats2 }
data_refs = []  # (site_va, iat_addr, func_va, func_name)
if remaining:
    generic_ref_re = re.compile(r'0x([0-9a-f]+)')
    cur_label_va = None
    cur_label_name = None
    for line in lines:
        m = label_re.match(line)
        if m:
            cur_label_va = int(m.group(1), 16)
            cur_label_name = m.group(2)
            continue
        m = insn_re.match(line)
        if not m or cur_label_va is None:
            continue
        if '\t' not in line:
            continue
        # only look at the operand part (after the mnemonic column), roughly after second tab
        parts = line.split('\t')
        if len(parts) < 3:
            continue
        operand_part = parts[-1]
        insn_va = int(m.group(1), 16)
        for hm in generic_ref_re.finditer(operand_part):
            addr = int(hm.group(1), 16)
            if addr in remaining:
                # skip the thunk's own defining instruction referencing its own IAT slot
                if cur_label_va in thunk_map and thunk_map[cur_label_va] == addr and insn_va == cur_label_va:
                    continue
                data_refs.append((insn_va, addr, cur_label_va, cur_label_name))

print("data_refs found:", len(data_refs))
still_unresolved = remaining.keys() - set(r[1] for r in data_refs)
print("still fully unresolved imports:", len(still_unresolved))
for a in still_unresolved:
    print("  ", remaining[a], hex(a))

out = {
    "callsites": [
        {"site_va": hex(s), "iat": hex(i), "func_va": hex(fv), "func_name": fn, "mode": mode}
        for (s,i,fv,fn,mode) in callsites
    ],
    "labels": [{"va": hex(v), "name": n} for v,n in labels],
    "thunk_map": {hex(k): hex(v) for k,v in thunk_map.items()},
    "uncalled_imports": [{"dll":d,"name":n,"iat":h} for d,n,h in uncalled],
    "data_refs": [
        {"site_va": hex(s), "iat": hex(i), "func_va": hex(fv), "func_name": fn}
        for (s,i,fv,fn) in data_refs
    ],
}
json.dump(out, open(ROOT + r"\tools_recon\census_callsites.json", "w"), indent=1)
print("wrote census_callsites.json")
