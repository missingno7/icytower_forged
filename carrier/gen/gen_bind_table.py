#!/usr/bin/env python3
"""gen_bind_table.py -- generates carrier/gen/bind_table.inc, the binding
table carrier/src/bind.cpp used to hand-maintain as a 35-row C++ literal
(`kFns[]`, one row per src/icytower function: name, VA, argc, whether the
prototype returns a value, the lifted_/native_/src function pointers, and a
comparison-domain function pointer + a fault-injection-address function
pointer). This script derives every one of those fields mechanically:

  - which functions get a row              -> gen/scan_src_defs.py (scans
                                               src/icytower/*.c for definitions)
  - VA / size / prototype                   -> gen/interop_index.json, cross-
                                               checked against
                                               gen/it_funcs_table.inc
  - whether a lifted_<fn>/native_<fn> form
    exists (is actually linked into
    carrier.exe, not merely present as a
    file under lift/lifted/)               -> parsed out of build.cmd's own
                                               compile/link command line
  - the comparison domain (which bytes of
    guest memory + which argument-relative
    regions a function's forms must agree
    on, and which single byte
    --fault-inject flips)                   -> gen/fn_domains.json (hand-
                                               curated DATA, see that file's
                                               own header for why this is a
                                               carrier-side file rather than
                                               something shared byte-for-byte
                                               with carrier/lift/harness's own
                                               SPECS dict)

Every domain reduces to a short list of small, generic "regions" (global VA,
argument-relative pointer, argument-relative pointer + offset, the
ply[player_id] double-indirection update_frame uses, or the
counter-then-indexed-slot shape add_combo/add_jump_sequence use) - see
fn_domains.json's own "_region_kinds". bind_table.inc therefore emits, per
function that has a domain, a small `static const Region regions_<fn>[] =
{...};` DATA array, never a hand-written dom_<fn>()/fa_<fn>() C++ function -
carrier/src/bind.cpp gained ONE generic engine (hash_one_region() /
fault_addr_generic()) that walks whichever region array a row points at,
instead of one bespoke function per row. A function with no fn_domains.json
entry gets the SAME default previously used in bind.cpp for a leaf function
with nothing interesting to compare: an empty region list (EAX only, when the
function returns one) and no --fault-inject target (see fn_domains.json's own
"_default").

Output is marked DO-NOT-EDIT and included by carrier/src/bind.cpp, which
after this pass contains no per-function literals of its own (see that
file's own header comment for the small, genuinely-generic pieces that DO
still live there: the Region engine, the asm stub template, and the record
writer).

Usage (also run automatically by build.cmd before compiling):
    python gen_bind_table.py [--root ..\..] [--out bind_table.inc]
Exits non-zero with a named message on any inconsistency (VA/size mismatch
between interop_index.json and it_funcs_table.inc, a function present in
fn_domains.json that scan_src_defs.py no longer finds, an unknown region
kind, etc.) - never silently drops a row or guesses.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def scan_src_defs(src_dir):
    """Same DEF_RE scan_src_defs.py itself uses (kept in sync by inspection -
    both are tiny and this avoids an import-path dance); see that file's own
    docstring for why this particular regex matches this codebase's style."""
    def_re = re.compile(
        r'^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\n\{',
        re.MULTILINE)
    names = []
    for fn in sorted(os.listdir(src_dir)):
        if not fn.endswith('.c') or fn == 'state.c':
            continue
        with open(os.path.join(src_dir, fn), 'r', encoding='utf-8') as f:
            text = f.read()
        found = [m.group(1) for m in def_re.finditer(text)]
        if not found:
            print("gen_bind_table: WARNING - no function definitions found in %s" % fn,
                  file=sys.stderr)
        for n in found:
            names.append((n, fn))
    return names


PROTO_RE = re.compile(r'^(?P<ret>.+?)\s+(?P<name>[A-Za-z_]\w*)\s*\((?P<args>.*)\)\s*$')


def split_args(args_text):
    args_text = args_text.strip()
    if args_text == '' or args_text == 'void':
        return []
    return [a.strip() for a in args_text.split(',')]


def parse_prototype(proto, fname):
    m = PROTO_RE.match(proto.strip())
    if not m:
        die("interop_index.json prototype for '%s' does not parse: %r" % (fname, proto))
    ret = m.group('ret').strip()
    args = split_args(m.group('args'))
    argc = len(args)
    returns_value = (ret != 'void')
    ret_c = 'void*' if '*' in ret else ('void' if ret == 'void' else 'int')
    arg_c_types = ['void*' if '*' in a else 'int' for a in args]
    return argc, returns_value, ret_c, arg_c_types


def die(msg):
    print("gen_bind_table: FATAL - %s" % msg, file=sys.stderr)
    sys.exit(1)


def parse_linked_forms(build_cmd_text):
    """Which lifted_<fn>/native_<fn> objects are actually compiled into
    carrier.exe, per build.cmd's own compile/link command line - NOT which
    lift/lifted/*.c or native/*.c FILES merely exist on disk (lift/lifted/
    has many more generated candidates than are wired into the link, per
    build.cmd's own comment)."""
    lifted = set(re.findall(r'lift\\lifted\\lifted_([A-Za-z_]\w*)\.c', build_cmd_text))
    native = set(re.findall(r'native\\native_([A-Za-z_]\w*)\.c', build_cmd_text))
    return lifted, native


def region_c_init(name, region, all_names):
    kind = region['kind']
    if kind == 'global':
        return '{ RK_GLOBAL, %s, 0, 0, %du, 0, 0 }' % (region['va'], region['len'])
    if kind == 'arg':
        return '{ RK_ARG, 0, %d, 0, %du, 0, 0 }' % (region['index'], region['len'])
    if kind == 'arg_offset':
        return '{ RK_ARG_OFFSET, 0, %d, %du, %du, 0, 0 }' % (region['index'], region['offset'], region['len'])
    if kind == 'player_indirect':
        return '{ RK_PLAYER_INDIRECT, %s, 0, 0, %du, %s, %du }' % (
            region['table_va'], region['len'], region['index_va'], region['max_index'])
    if kind == 'counter_indexed':
        return '{ RK_COUNTER_INDEXED, 0, %d, %du, %du, %du, %du }' % (
            region['index'], region['counter_offset'], region['elem_size'],
            region['table_offset'], region['max_index'])
    die("fn_domains.json: unknown region kind '%s' (function table: %s)" % (kind, name))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=os.path.normpath(os.path.join(HERE, '..', '..')),
                     help='repo root (default: two levels up from this script)')
    ap.add_argument('--out', default=os.path.join(HERE, 'bind_table.inc'))
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    carrier_dir = os.path.join(root, 'carrier')
    src_dir = os.path.join(root, 'src', 'icytower')
    gen_dir = os.path.join(carrier_dir, 'gen')

    scanned = scan_src_defs(src_dir)  # [(name, file), ...] in scan order
    if not scanned:
        die("scan_src_defs found no function definitions under %s" % src_dir)

    with open(os.path.join(gen_dir, 'interop_index.json'), 'r', encoding='utf-8') as f:
        interop = json.load(f)
    fn_by_name = {f['name']: f for f in interop['functions']}

    # Cross-check against it_funcs_table.inc (VA/size), the same evidence
    # bind.cpp's own header comment used to cite by hand - MEASURED here
    # instead of trusted by inspection.
    table_inc_path = os.path.join(gen_dir, 'it_funcs_table.inc')
    table_row_re = re.compile(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(\d+)\s*,')
    table_rows = {}
    with open(table_inc_path, 'r', encoding='utf-8') as f:
        for line in f:
            m = table_row_re.search(line)
            if m:
                table_rows[m.group(1)] = (int(m.group(2), 16), int(m.group(3)))

    with open(os.path.join(gen_dir, 'fn_domains.json'), 'r', encoding='utf-8') as f:
        domains_doc = json.load(f)
    domains = {k: v for k, v in domains_doc['functions'].items() if not k.startswith('_comment')}

    blockers_path = os.path.join(gen_dir, 'build_blockers.json')
    blocked_files = {}
    if os.path.isfile(blockers_path):
        with open(blockers_path, 'r', encoding='utf-8') as f:
            blocked_files = json.load(f).get('files', {})

    with open(os.path.join(carrier_dir, 'build.cmd'), 'r', encoding='utf-8') as f:
        build_cmd_text = f.read()
    linked_lifted, linked_native = parse_linked_forms(build_cmd_text)

    rows = []
    seen = set()
    skipped_no_va = []
    skipped_blocked = []
    for name, srcfile in scanned:
        if name in seen:
            continue  # a name can legitimately appear once per file only; guard anyway
        seen.add(name)
        if srcfile in blocked_files:
            skipped_blocked.append(name)
            continue
        f = fn_by_name.get(name)
        if f is None:
            skipped_no_va.append(name)
            continue
        va = int(f['va'], 16)
        size = f['size']
        if name in table_rows:
            t_va, t_size = table_rows[name]
            if t_va != va or t_size != size:
                die("interop_index.json and it_funcs_table.inc disagree for '%s': "
                    "(va=0x%x,size=%d) vs (va=0x%x,size=%d)" % (name, va, size, t_va, t_size))
        argc, returns_value, ret_c, arg_c_types = parse_prototype(f['prototype'], name)
        if argc > 10:
            die("'%s' has %d cdecl arguments, more than bind.cpp's kMaxArgs(10) re-push "
                "width supports - raise kMaxArgs the same way Milestone 12 at scale did"
                % (name, argc))
        rows.append({
            'name': name, 'va': va, 'argc': argc, 'returns_value': returns_value,
            'ret_c': ret_c, 'arg_c_types': arg_c_types,
            'has_lifted': name in linked_lifted, 'has_native': name in linked_native,
            'srcfile': srcfile,
        })

    if skipped_no_va:
        print("gen_bind_table: %d scanned name(s) have no interop_index.json entry "
              "(not real game functions - harness/test helpers), excluded from the "
              "binding table: %s" % (len(skipped_no_va), ', '.join(skipped_no_va)),
              file=sys.stderr)
    if skipped_blocked:
        print("gen_bind_table: %d function(s) excluded - their source file is not "
              "compiled into the carrier (see gen/build_blockers.json): %s"
              % (len(skipped_blocked), ', '.join(skipped_blocked)), file=sys.stderr)

    row_names = {r['name'] for r in rows}
    stray = [k for k in domains if k not in row_names]
    if stray:
        die("fn_domains.json names function(s) scan_src_defs.py no longer finds "
            "(stale entries, fix or remove): %s" % ', '.join(stray))

    n_default = sum(1 for r in rows if r['name'] not in domains)
    print("gen_bind_table: %d function row(s) (%d with a curated domain, %d default/empty)"
          % (len(rows), len(rows) - n_default, n_default), file=sys.stderr)

    # ---- emit bind_table.inc -------------------------------------------
    out = []
    out.append('/* GENERATED FILE -- DO NOT EDIT.')
    out.append(' * Produced by carrier/gen/gen_bind_table.py from:')
    out.append(' *   - carrier/gen/scan_src_defs.py\'s scan of src/icytower/*.c (which functions)')
    out.append(' *   - carrier/gen/interop_index.json + it_funcs_table.inc (VA/size/prototype)')
    out.append(' *   - carrier/gen/fn_domains.json (comparison-domain regions, hand-curated data)')
    out.append(' *   - carrier/build.cmd\'s own link line (which lifted_/native_ forms are linked in)')
    out.append(' * Included by carrier/src/bind.cpp, which defines RegionKind, Region and FnDesc')
    out.append(' * above the #include and consumes kFns[]/kNumFns below it. Re-run by build.cmd')
    out.append(' * before every compile; never hand-edit -- edit fn_domains.json or the sources')
    out.append(' * above instead.')
    out.append(' */')
    out.append('')
    out.append('extern "C" {')
    for r in rows:
        args_c = ', '.join(r['arg_c_types']) if r['arg_c_types'] else 'void'
        out.append('    %s __cdecl %s(%s);' % (r['ret_c'], r['name'], args_c))
        if r['has_lifted']:
            out.append('    %s __cdecl lifted_%s(%s);' % (r['ret_c'], r['name'], args_c))
        if r['has_native']:
            out.append('    %s __cdecl native_%s(%s);' % (r['ret_c'], r['name'], args_c))
    out.append('}')
    out.append('')

    for r in rows:
        name = r['name']
        d = domains.get(name)
        regions = d['regions'] if d else []
        if regions:
            out.append('static const Region regions_%s[] = {' % name)
            for reg in regions:
                comment = ' // %s' % reg['name'] if 'name' in reg else ''
                out.append('    %s,%s' % (region_c_init(name, reg, row_names), comment))
            out.append('};')
    out.append('')

    out.append('static const FnDesc kFns[] = {')
    out.append('    // name  va  argc  returns_value  lifted  native  src  regions  num_regions  fault_region')
    for r in rows:
        name = r['name']
        d = domains.get(name)
        regions = d['regions'] if d else []
        fault_region = d['fault_region'] if d else -1
        lifted_c = '(void*)lifted_%s' % name if r['has_lifted'] else 'nullptr'
        native_c = '(void*)native_%s' % name if r['has_native'] else 'nullptr'
        regions_c = 'regions_%s' % name if regions else 'nullptr'
        out.append('    { "%s", 0x%08xu, %d, %s, %s, %s, (void*)%s, %s, %d, %d },' % (
            name, r['va'], r['argc'], 'true' if r['returns_value'] else 'false',
            lifted_c, native_c, name, regions_c, len(regions), fault_region))
    out.append('};')
    out.append('static const int kNumFns = (int)(sizeof(kFns) / sizeof(kFns[0]));')
    out.append('')

    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out) + '\n')
    print("gen_bind_table: wrote %s (%d rows)" % (args.out, len(rows)), file=sys.stderr)


if __name__ == '__main__':
    main()
