import re, json, sys

DWARF = r"D:\Games\DOS\dos_recosystem\icytower_forged\artifacts\dwarf_info.txt"
OUT_FUNCS = r"D:\Games\DOS\dos_recosystem\icytower_forged\artifacts\dwarf_subprograms.json"
OUT_CUS = r"D:\Games\DOS\dos_recosystem\icytower_forged\artifacts\dwarf_cus.json"

cu_re = re.compile(r'^\s*Compilation Unit @ offset (0x[0-9a-fA-F]+|\d+):')
die_re = re.compile(r'^ <(\d+)><([0-9a-fA-F]+)>: Abbrev Number: (\d+)(?: \((\S+)\))?')
attr_re = re.compile(r'^\s+<[0-9a-fA-F]+>\s+(DW_AT_\w+)\s*:\s*(.*)$')

cus = []
funcs = []

cur_cu = None
cur_die_tag = None
cur_die_depth = None
cur_attrs = None

def flush_die():
    global cur_die_tag, cur_attrs, cur_die_depth
    if cur_die_tag == 'DW_TAG_compile_unit' and cur_attrs is not None:
        cu = {
            'producer': cur_attrs.get('DW_AT_producer'),
            'name': cur_attrs.get('DW_AT_name'),
            'low_pc': cur_attrs.get('DW_AT_low_pc'),
            'high_pc': cur_attrs.get('DW_AT_high_pc'),
            'comp_dir': cur_attrs.get('DW_AT_comp_dir'),
        }
        cus.append(cu)
        cur_cu_holder['cu'] = cu
    elif cur_die_tag == 'DW_TAG_subprogram' and cur_attrs is not None:
        if cur_die_depth == 1:  # top-level function in the CU
            low = cur_attrs.get('DW_AT_low_pc')
            high = cur_attrs.get('DW_AT_high_pc')
            if low and high:
                funcs.append({
                    'name': cur_attrs.get('DW_AT_name'),
                    'low_pc': low,
                    'high_pc': high,
                    'external': cur_attrs.get('DW_AT_external'),
                    'decl_file': cur_attrs.get('DW_AT_decl_file'),
                    'decl_line': cur_attrs.get('DW_AT_decl_line'),
                    'cu_name': cur_cu_holder['cu']['name'] if cur_cu_holder['cu'] else None,
                    'cu_producer': cur_cu_holder['cu']['producer'] if cur_cu_holder['cu'] else None,
                })
    cur_die_tag = None
    cur_attrs = None

cur_cu_holder = {'cu': None}

with open(DWARF, encoding='utf-8', errors='replace') as f:
    for line in f:
        m = die_re.match(line)
        if m:
            flush_die()
            depth = int(m.group(1))
            tag = m.group(4)
            cur_die_depth = depth
            cur_die_tag = tag
            cur_attrs = {} if tag else None
            continue
        if cur_attrs is not None:
            am = attr_re.match(line)
            if am:
                key, val = am.groups()
                cur_attrs[key] = val.strip()
    flush_die()

print(f"CUs: {len(cus)}  top-level subprograms with low/high pc: {len(funcs)}", file=sys.stderr)

json.dump(cus, open(OUT_CUS, 'w'), indent=0)
json.dump(funcs, open(OUT_FUNCS, 'w'), indent=0)
print("wrote", OUT_CUS, OUT_FUNCS, file=sys.stderr)
