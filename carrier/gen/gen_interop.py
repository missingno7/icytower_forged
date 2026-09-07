#!/usr/bin/env python3
"""
gen_interop.py -- generates carrier/gen interop headers from DWARF debug
info of the original icytower15.exe, so hand-written or generated C code
(MSVC, 32-bit) can operate on the original game's memory and call the
original functions by their original virtual addresses.

The image is mapped in-process at its original base 0x400000 (no
relocations, per win32_pilot.md SS2), so a global at VA X is simply
*(T*)X, and a function at VA F is simply ((ret(__cdecl*)(args))F)(...).
This script emits exactly those two idioms, generated instead of
hand-written, for every global/function DWARF describes in scope.

Usage:
    python gen_interop.py --dwarf artifacts/dwarf_info.txt \
        --functions artifacts/functions.json --out carrier/gen --scope game

Inputs:
    --dwarf      objdump --dwarf=info text dump of the original .exe
    --functions  artifacts/functions.json (VA -> {name, size, origin, cu})
                 used only to cross-check function byte size and origin;
                 DWARF is authoritative for names/types/prototypes.
    --scope      "game"  = CUs under F:\\projects\\icytower\\trunk\\source\\
                 "all"   = every CU in the DWARF (Allegro/vorbis/CRT too)

Outputs (into --out):
    it_types.h          every struct/union/enum/typedef reachable from the
                         emitted globals/functions, packed(1) with explicit
                         padding so the layout matches DWARF byte-for-byte
    it_globals.h         IT_G_<name> macros + typed pointer constants
    it_funcs.h            IT_F_<name> macros + PFN_<name> typedefs
    it_funcs_table.inc    { "name", 0xVA, size, "cu" } rows
    interop_index.json    sidecar listing of everything emitted
    it_selftest.c          compiles all three headers + touches a few symbols
    it_types_check.c       standalone offsetof/sizeof PASS/FAIL host program
    INTEROP_NOTES.md        counts, ambiguities, opaque types, collisions

Determinism: every list this script emits is sorted by a stable key
(name, then VA/offset) before being written, so re-running on unchanged
inputs reproduces byte-identical output.
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

GAME_PREFIX = ('f:' + chr(92) + 'projects' + chr(92) + 'icytower' + chr(92)
               + 'trunk' + chr(92) + 'source' + chr(92))

# --------------------------------------------------------------------------
# DWARF text-dump parsing (objdump --dwarf=info format)
# --------------------------------------------------------------------------

DIE_HDR = re.compile(r'^ <(\d+)><([0-9a-fA-F]+)>: Abbrev Number: (\d+)(?: \((\S+)\))?')
ATTR_LINE = re.compile(r'^\s+<[0-9a-fA-F]+>\s+(DW_AT_\w+)\s*:\s*(.*)$')


def parse_dwarf(path):
    """Single pass over the objdump --dwarf=info text dump. DIE offsets in
    this dump are absolute .debug_info byte offsets shared across all CUs
    (verified: CU headers appear at monotonically increasing offsets), so
    a flat offset->die dict is a valid global cross-reference table."""
    dies = {}
    stack = []  # (depth, offset) of currently open ancestors
    cur_cu = None
    cur_attrs = None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = DIE_HDR.match(line)
            if m:
                depth = int(m.group(1))
                offset = int(m.group(2), 16)
                tag = m.group(4)
                if tag is None:
                    # "Abbrev Number: 0" = end of this sibling list
                    while stack and stack[-1][0] >= depth:
                        stack.pop()
                    cur_attrs = None
                    continue
                if depth == 0:
                    cur_cu = offset
                die = {'tag': tag, 'depth': depth, 'attrs': {}, 'children': [],
                       'cu': cur_cu, 'offset': offset}
                dies[offset] = die
                while stack and stack[-1][0] >= depth:
                    stack.pop()
                if stack:
                    dies[stack[-1][1]]['children'].append(offset)
                stack.append((depth, offset))
                cur_attrs = die['attrs']
                continue
            if cur_attrs is not None:
                am = ATTR_LINE.match(line)
                if am:
                    cur_attrs[am.group(1)] = am.group(2).strip()
    return dies


# --------------------------------------------------------------------------
# Attribute value parsing helpers
# --------------------------------------------------------------------------

def parse_ref(raw):
    if raw is None:
        return None
    m = re.match(r'<(0x[0-9a-fA-F]+|[0-9a-fA-F]+)>', raw.strip())
    return int(m.group(1), 16) if m else None


def parse_int(raw):
    # Usually decimal ("120084"), but objdump switches large DW_AT_byte_size
    # values to hex ("0x1d514", seen on 11/17512 byte_size attrs -- e.g.
    # Tgame_data at 0x1d514 = 120084 bytes); accept both.
    if raw is None:
        return None
    m = re.match(r'\s*(0x[0-9a-fA-F]+)', raw)
    if m:
        return int(m.group(1), 16)
    m = re.match(r'\s*(-?\d+)', raw)
    return int(m.group(1)) if m else None


def parse_name(raw):
    if raw is None:
        return None
    m = re.match(r'\(indirect string, offset: 0x[0-9a-fA-F]+\):\s*(.*)$', raw)
    return m.group(1) if m else raw


def parse_member_offset(raw):
    if raw is None:
        return 0
    m = re.search(r'DW_OP_plus_uconst:\s*(\d+)', raw)
    if m:
        return int(m.group(1))
    m = re.match(r'\s*(\d+)\s*$', raw)
    return int(m.group(1)) if m else 0


def parse_op_addr(raw):
    if raw is None:
        return None
    m = re.search(r'DW_OP_addr:\s*([0-9a-fA-F]+)', raw)
    return int(m.group(1), 16) if m else None


def parse_array_dim(cd):
    ub = cd['attrs'].get('DW_AT_upper_bound')
    if ub is not None:
        v = parse_int(ub)
        return v + 1 if v is not None else None
    cnt = cd['attrs'].get('DW_AT_count')
    return parse_int(cnt) if cnt is not None else None


def sanitize_ident(name):
    return re.sub(r'[^A-Za-z0-9_]', '_', name)


def cu_basename(cu_name):
    if not cu_name:
        return 'unknown'
    base = cu_name.replace('\\', '/').rsplit('/', 1)[-1]
    return sanitize_ident(re.sub(r'\.c$', '', base, flags=re.I))


# --------------------------------------------------------------------------
# `typedef struct { ... } Name;` is a common C idiom: the struct DIE itself
# has no DW_AT_name, only the typedef DIE that wraps it carries the name
# the original source actually used. We recover it here so it_types.h uses
# `Name` instead of a synthesized `it_anon_s_<offset>` tag. (Without this,
# ~1/3 of game-scope structs would come out with meaningless offset names.)
# --------------------------------------------------------------------------

def compute_anon_typedef_names(dies):
    names = {}
    for off in sorted(dies):
        d = dies[off]
        if d['tag'] != 'DW_TAG_typedef':
            continue
        tname = parse_name(d['attrs'].get('DW_AT_name'))
        target_off = parse_ref(d['attrs'].get('DW_AT_type'))
        if not tname or target_off is None:
            continue
        td = dies.get(target_off)
        if td is None or td['tag'] not in ('DW_TAG_structure_type', 'DW_TAG_union_type', 'DW_TAG_enumeration_type'):
            continue
        if parse_name(td['attrs'].get('DW_AT_name')) is not None:
            continue
        names.setdefault(target_off, sanitize_ident(tname))
    return names


# --------------------------------------------------------------------------
# Named-type de-duplication: GCC re-emits full struct/union/enum defs in
# every CU that includes the header (verified: BITMAP appears 137 times,
# byte_size 64 identically). We collapse each (kind,name) to one canonical
# definition, preferring a fully-defined one, and flag real conflicts.
# Anonymous structs recovered above (via their typedef name) are grouped
# the same way, so `typedef struct {...} Thisc;` duplicated across CUs
# collapses to one `struct Thisc` instead of colliding redefinitions.
# --------------------------------------------------------------------------

def compute_canonical(dies, anon_names):
    groups = defaultdict(list)
    for off, d in dies.items():
        if d['tag'] in ('DW_TAG_structure_type', 'DW_TAG_union_type', 'DW_TAG_enumeration_type'):
            name = parse_name(d['attrs'].get('DW_AT_name')) or anon_names.get(off)
            if name:
                groups[(d['tag'], name)].append(off)
        elif d['tag'] == 'DW_TAG_typedef':
            # e.g. `time_t` is redeclared once per CU that includes the CRT
            # headers (like every named type here); without this it would
            # come out as N identical `typedef long time_t;` lines.
            name = parse_name(d['attrs'].get('DW_AT_name'))
            if name:
                groups[(d['tag'], name)].append(off)
    canonical = {}
    conflicts = []
    for (tag, name), offs in groups.items():
        offs.sort()
        if tag == 'DW_TAG_typedef':
            # Every CU redeclares its own physical copy of e.g. `time_t`,
            # so the raw DW_AT_type *offset* almost always differs even
            # when the effective type is identical -- comparing offsets
            # directly is pure noise (flagged ~3000 non-conflicts on this
            # binary). Only flag a real mismatch: same immediate target
            # *tag* but a different byte_size (cheap, not a full
            # structural compare, but catches genuine divergence).
            chosen = offs[0]
            canonical.update({o: chosen for o in offs})
            chosen_t = dies.get(parse_ref(dies[chosen]['attrs'].get('DW_AT_type')))
            for o in offs[1:]:
                ot = dies.get(parse_ref(dies[o]['attrs'].get('DW_AT_type')))
                if chosen_t is not None and ot is not None and chosen_t['tag'] == ot['tag'] and \
                        chosen_t['attrs'].get('DW_AT_byte_size') != ot['attrs'].get('DW_AT_byte_size'):
                    conflicts.append({
                        'kind': tag, 'name': name, 'canonical_offset': hex(chosen), 'conflict_offset': hex(o),
                        'note': 'typedef targets are both %s but disagree on byte_size (%s vs %s)' % (
                            chosen_t['tag'], chosen_t['attrs'].get('DW_AT_byte_size'), ot['attrs'].get('DW_AT_byte_size')),
                    })
            continue
        full = [o for o in offs if dies[o]['children']]
        chosen = full[0] if full else offs[0]
        chosen_size = dies[chosen]['attrs'].get('DW_AT_byte_size')
        for o in offs:
            canonical[o] = chosen
        for o in full:
            if o != chosen and dies[o]['attrs'].get('DW_AT_byte_size') != chosen_size:
                conflicts.append({
                    'kind': tag, 'name': name,
                    'canonical_offset': hex(chosen), 'canonical_size': chosen_size,
                    'conflict_offset': hex(o), 'conflict_size': dies[o]['attrs'].get('DW_AT_byte_size'),
                })
    return canonical, conflicts


# --------------------------------------------------------------------------
# Type origin classification (additive: consumed by carrier/gen/
# gen_src_headers.py to split reachable types between src/icytower/
# game_types.h -- the game's own structs -- and src/icytower/allegro_types.h
# -- public library types the game merely uses. Not used by gen_interop.py's
# own it_types.h emission, which does not need the distinction.)
#
# GCC re-emits a full struct/union/enum/typedef definition in every CU that
# includes its declaring header (verified: BITMAP appears 137 times,
# byte_size 64 identically -- see compute_canonical above). A type genuinely
# private to the game (Tplayer, Tmap, Tfloor, ...) is declared only in the
# game's own headers, so EVERY one of its physical redeclarations lives in a
# game-prefixed CU; a type the game shares with the rest of the binary
# (BITMAP, RGB, fixed, time_t, ...) also gets redeclared inside at least one
# non-game CU (Allegro's or the CRT's own source, wherever it is defined),
# because those files include the same header. That is the signal used
# here: "is every occurrence confined to game CUs" is checked across the
# WHOLE dwarf file, not just the scope passed to gen_interop.py's own run,
# so it stays correct even when gen_src_headers.py is invoked with a
# narrower --dwarf/--functions pair than gen_interop.py was.
# --------------------------------------------------------------------------

def compute_type_origin(dies, canonical, anon_names, game_prefix):
    """Returns {canonical_offset: True} for entities where every physical
    redeclaration lives in a game-prefixed CU, {canonical_offset: False}
    otherwise. Anonymous entities (no name, so never grouped by
    compute_canonical) are absent from the result; callers should fall back
    to checking that single DIE's own 'cu' directly."""
    cu_name_cache = {}

    def cu_is_game(cu_off):
        if cu_off not in cu_name_cache:
            cu_die = dies.get(cu_off)
            name = parse_name(cu_die['attrs'].get('DW_AT_name')) if cu_die else None
            cu_name_cache[cu_off] = bool(name) and name.lower().replace('/', '\\').startswith(game_prefix)
        return cu_name_cache[cu_off]

    all_game = {}
    for off, d in dies.items():
        if d['tag'] not in ('DW_TAG_structure_type', 'DW_TAG_union_type',
                             'DW_TAG_enumeration_type', 'DW_TAG_typedef'):
            continue
        name = parse_name(d['attrs'].get('DW_AT_name')) or anon_names.get(off)
        if not name:
            continue
        canon = canonical.get(off, off)
        is_game = cu_is_game(d.get('cu'))
        all_game[canon] = all_game.get(canon, True) and is_game
    return all_game


# --------------------------------------------------------------------------
# Type IR: {'kind': base|ptr|array|struct|union|enum|typedef|func|const|
#           volatile|void, ...}
# --------------------------------------------------------------------------

BASE_TYPE_MAP = {
    'unsigned int': 'unsigned int', 'int': 'int',
    'short unsigned int': 'unsigned short', 'short int': 'short',
    'long unsigned int': 'unsigned long', 'long int': 'long',
    'long long unsigned int': 'unsigned __int64', 'long long int': '__int64',
    'char': 'char', 'signed char': 'signed char', 'unsigned char': 'unsigned char',
    'float': 'float', 'double': 'double', '_Bool': 'unsigned char',
}

LONG_DOUBLE_NOTE = []  # (dwarf name, byte_size) substitutions recorded for notes
ANON_TYPEDEF_NAMES = {}  # offset(anon struct/union/enum die) -> recovered name; set in main()

# Standard C library type names GCC also declared in the game's DWARF
# (because every CU re-declares whatever CRT headers it includes). Renaming
# them for OUR headers only, and only to something that still shows the
# original name, avoids two failure modes verified by compiling
# it_selftest.c / it_types_check.c against real MSVC headers: (1)
# `time_t` is `long` (4 bytes) here but `__int64` (8 bytes) in modern
# MSVC/UCRT -> `error C2371: redefinition; different basic types` the
# moment anything pulls in <time.h>/<corecrt.h>; (2) `struct _iobuf` /
# `FILE` differ in internal layout from UCRT's own -> `error C2011:
# redefinition`. Any hand-written carrier code that also includes real
# CRT headers alongside these generated ones needs the same escape hatch,
# so it is applied unconditionally, not just for our own compile tests.
RESERVED_TYPE_RENAMES = {
    'time_t': 'it_orig_time_t', 'FILE': 'it_orig_FILE', '_iobuf': 'it_orig__iobuf',
    'size_t': 'it_orig_size_t', 'wchar_t': 'it_orig_wchar_t', 'va_list': 'it_orig_va_list',
    'ptrdiff_t': 'it_orig_ptrdiff_t', 'intptr_t': 'it_orig_intptr_t', 'uintptr_t': 'it_orig_uintptr_t',
    'off_t': 'it_orig_off_t', 'clock_t': 'it_orig_clock_t', 'div_t': 'it_orig_div_t',
    'ldiv_t': 'it_orig_ldiv_t', 'jmp_buf': 'it_orig_jmp_buf', 'sig_atomic_t': 'it_orig_sig_atomic_t',
    'wint_t': 'it_orig_wint_t', 'mbstate_t': 'it_orig_mbstate_t', 'tm': 'it_orig_tm',
}


def make_base_node(dwarf_name, size):
    n = (dwarf_name or 'int').strip()
    if n == 'long double':
        # MSVC's `long double` is bit-identical to `double` (8 bytes); GCC's
        # x86 `long double` is 80-bit extended precision stored in `size`
        # bytes (typically 12, 4-byte aligned). Using MSVC `long double`
        # here would silently corrupt any struct layout containing one, so
        # we represent it as an opaque byte blob of the *original* size
        # instead. KNOWN mismatch, deliberately not bridged (no float80
        # support without softfloat, see win32_pilot.md SS3 x87 note).
        LONG_DOUBLE_NOTE.append(size)
        return {'kind': 'array', 'inner': {'kind': 'base', 'name': 'unsigned char', 'size': 1},
                'dims': [size], 'long_double_subst': True}
    cname = BASE_TYPE_MAP.get(n, n if re.match(r'^[A-Za-z_][A-Za-z0-9_ ]*$', n) else 'int')
    return {'kind': 'base', 'name': cname, 'size': size}


def resolve_type(off, dies, canonical, cache):
    if off is None:
        return {'kind': 'void'}
    if off in cache:
        return cache[off]
    d = dies.get(off)
    if d is None:
        return {'kind': 'void'}
    tag = d['tag']
    a = d['attrs']

    if tag == 'DW_TAG_base_type':
        node = make_base_node(parse_name(a.get('DW_AT_name')), parse_int(a.get('DW_AT_byte_size')) or 4)
        cache[off] = node
        return node

    if tag == 'DW_TAG_pointer_type':
        node = {'kind': 'ptr', 'inner': None}
        cache[off] = node
        t = a.get('DW_AT_type')
        node['inner'] = resolve_type(parse_ref(t), dies, canonical, cache) if t else {'kind': 'void'}
        return node

    if tag in ('DW_TAG_const_type', 'DW_TAG_volatile_type'):
        kind = 'const' if tag == 'DW_TAG_const_type' else 'volatile'
        node = {'kind': kind, 'inner': None}
        cache[off] = node
        t = a.get('DW_AT_type')
        node['inner'] = resolve_type(parse_ref(t), dies, canonical, cache) if t else {'kind': 'void'}
        return node

    if tag == 'DW_TAG_array_type':
        node = {'kind': 'array', 'inner': None, 'dims': []}
        cache[off] = node
        t = a.get('DW_AT_type')
        elem = resolve_type(parse_ref(t), dies, canonical, cache) if t else {'kind': 'base', 'name': 'char', 'size': 1}
        dims = [parse_array_dim(dies[c]) for c in d['children'] if dies[c]['tag'] == 'DW_TAG_subrange_type']
        node['inner'] = elem
        node['dims'] = dims or [None]
        return node

    if tag in ('DW_TAG_structure_type', 'DW_TAG_union_type'):
        canon = canonical.get(off, off)
        if canon != off:
            return resolve_type(canon, dies, canonical, cache)
        name = parse_name(a.get('DW_AT_name')) or ANON_TYPEDEF_NAMES.get(off)
        anon = name is None
        if anon:
            name = 'it_anon_%s_%x' % ('u' if tag == 'DW_TAG_union_type' else 's', off)
        name = RESERVED_TYPE_RENAMES.get(name, name)
        node = {'kind': 'union' if tag == 'DW_TAG_union_type' else 'struct', 'name': sanitize_ident(name),
                'anon': anon, 'byte_size': parse_int(a.get('DW_AT_byte_size')),
                'declaration': 'DW_AT_declaration' in a, 'members': [], 'offset': off}
        cache[off] = node
        for c in d['children']:
            cd = dies[c]
            if cd['tag'] != 'DW_TAG_member':
                continue
            mname = parse_name(cd['attrs'].get('DW_AT_name')) or ('_f%d' % len(node['members']))
            mtype = resolve_type(parse_ref(cd['attrs'].get('DW_AT_type')), dies, canonical, cache)
            moff = parse_member_offset(cd['attrs'].get('DW_AT_data_member_location'))
            node['members'].append((sanitize_ident(mname), mtype, moff))
        return node

    if tag == 'DW_TAG_enumeration_type':
        canon = canonical.get(off, off)
        if canon != off:
            return resolve_type(canon, dies, canonical, cache)
        name = parse_name(a.get('DW_AT_name')) or ANON_TYPEDEF_NAMES.get(off)
        anon = name is None
        if anon:
            name = 'it_anon_e_%x' % off
        name = RESERVED_TYPE_RENAMES.get(name, name)
        node = {'kind': 'enum', 'name': sanitize_ident(name), 'anon': anon,
                'byte_size': parse_int(a.get('DW_AT_byte_size')) or 4, 'values': [], 'offset': off}
        cache[off] = node
        for c in d['children']:
            cd = dies[c]
            if cd['tag'] != 'DW_TAG_enumerator':
                continue
            ename = parse_name(cd['attrs'].get('DW_AT_name'))
            eval_ = parse_int(cd['attrs'].get('DW_AT_const_value'))
            if ename is not None:
                node['values'].append((sanitize_ident(ename), eval_ if eval_ is not None else 0))
        return node

    if tag == 'DW_TAG_typedef':
        canon = canonical.get(off, off)
        if canon != off:
            return resolve_type(canon, dies, canonical, cache)
        name = sanitize_ident(parse_name(a.get('DW_AT_name')) or ('it_td_%x' % off))
        name = RESERVED_TYPE_RENAMES.get(name, name)
        node = {'kind': 'typedef', 'name': name, 'inner': None, 'offset': off}
        cache[off] = node
        t = a.get('DW_AT_type')
        node['inner'] = resolve_type(parse_ref(t), dies, canonical, cache) if t else {'kind': 'base', 'name': 'void', 'size': 0}
        return node

    if tag == 'DW_TAG_subroutine_type':
        node = {'kind': 'func', 'ret': None, 'params': [], 'variadic': False,
                'prototyped': 'DW_AT_prototyped' in a}
        cache[off] = node
        t = a.get('DW_AT_type')
        node['ret'] = resolve_type(parse_ref(t), dies, canonical, cache) if t else {'kind': 'void'}
        for c in d['children']:
            cd = dies[c]
            if cd['tag'] == 'DW_TAG_formal_parameter':
                node['params'].append(resolve_type(parse_ref(cd['attrs'].get('DW_AT_type')), dies, canonical, cache))
            elif cd['tag'] == 'DW_TAG_unspecified_parameters':
                node['variadic'] = True
        return node

    node = {'kind': 'void'}
    cache[off] = node
    return node


def sizeof_node(node):
    k = node['kind']
    if k in ('const', 'volatile', 'typedef'):
        return sizeof_node(node['inner'])
    if k == 'base':
        return node['size']
    if k == 'ptr':
        return 4
    if k == 'array':
        n = 1
        for dim in node['dims']:
            if dim is None:
                return 0
            n *= dim
        return n * sizeof_node(node['inner'])
    if k in ('struct', 'union'):
        return node['byte_size'] or 0
    if k == 'enum':
        return node['byte_size'] or 4
    return 0


# --------------------------------------------------------------------------
# C declarator rendering (handles pointers, arrays, function pointers,
# qualifiers -- the classic "declaration follows use" recursion)
# --------------------------------------------------------------------------

def base_type_name(node):
    k = node['kind']
    if k == 'base':
        return node['name']
    if k in ('struct', 'union'):
        return '%s %s' % (k, node['name'])
    if k == 'enum':
        return 'enum %s' % node['name']
    if k == 'typedef':
        return node['name']
    return 'void'


def decl(node, name, quals=()):
    k = node['kind']
    if k in ('const', 'volatile'):
        return decl(node['inner'], name, quals + (k,))
    if k == 'ptr':
        target = node['inner'] or {'kind': 'void'}
        peek = target
        while peek['kind'] in ('const', 'volatile'):
            peek = peek['inner']
        needs_parens = peek['kind'] in ('array', 'func')
        # Calling-convention keyword for a function pointer lives *inside*
        # the parens, right before the `*` (`RET (__cdecl *name)(PARAMS)`);
        # putting it outside (as the func branch used to) produced illegal
        # double-parenthesised declarators like `(__cdecl (*name))(...)`.
        prefix = '__cdecl *' if peek['kind'] == 'func' else '*'
        star = ('%s %s %s' % (prefix, ' '.join(sorted(set(quals))), name)).strip() if quals else ('%s%s' % (prefix, name))
        inner_name = '(%s)' % star if needs_parens else star
        return decl(target, inner_name, ())
    if k == 'array':
        dimstr = ''.join('[%s]' % (d if d is not None else '') for d in node['dims'])
        return decl(node['inner'], '%s%s' % (name, dimstr), quals)
    if k == 'func':
        # Reached only through the ptr branch above in this codebase (the
        # calling convention was already inserted there); a bare function
        # declarator with no pointer just gets its plain name here.
        paramstr = render_params(node)
        ret = decl(node['ret'], '')
        return '%s %s(%s)' % (ret, name, paramstr)
    qual_str = (' '.join(sorted(set(quals))) + ' ') if quals else ''
    return ('%s%s %s' % (qual_str, base_type_name(node), name)).rstrip()


def render_params(fnode):
    parts = [decl(p, '') for p in fnode['params']]
    if fnode['variadic']:
        parts.append('...')
    if not parts:
        return 'void' if fnode['prototyped'] else ''
    return ', '.join(parts)


# --------------------------------------------------------------------------
# Reachability + topological ordering of named types for it_types.h
# --------------------------------------------------------------------------

def entity_key(node):
    if node['kind'] in ('struct', 'union'):
        return ('su', node['offset'])
    if node['kind'] == 'typedef':
        return ('td', node['offset'])
    if node['kind'] == 'enum':
        return ('en', node['offset'])
    return None


def collect_reachable(node, seen_ids, nodes):
    nid = id(node)
    if nid in seen_ids:
        return
    seen_ids.add(nid)
    k = node['kind']
    if k in ('struct', 'union'):
        nodes[entity_key(node)] = node
        for (_, mt, _) in node['members']:
            collect_reachable(mt, seen_ids, nodes)
    elif k == 'enum':
        nodes[entity_key(node)] = node
    elif k == 'typedef':
        nodes[entity_key(node)] = node
        collect_reachable(node['inner'], seen_ids, nodes)
    elif k == 'ptr':
        collect_reachable(node['inner'] or {'kind': 'void'}, seen_ids, nodes)
    elif k == 'array':
        collect_reachable(node['inner'], seen_ids, nodes)
    elif k in ('const', 'volatile'):
        collect_reachable(node['inner'], seen_ids, nodes)
    elif k == 'func':
        collect_reachable(node['ret'] or {'kind': 'void'}, seen_ids, nodes)
        for p in node['params']:
            collect_reachable(p, seen_ids, nodes)


def name_deps(node, seen=None):
    """Entities whose *name* (a typedef line -- a struct/union tag is always
    free, satisfied by the unconditional forward-declare pass) must already
    be declared before `node` can be written out at all, regardless of
    whether it is used by value or by pointer."""
    if seen is None:
        seen = set()
    nid = id(node)
    if nid in seen:
        return set()
    seen.add(nid)
    k = node['kind']
    if k == 'typedef':
        return {entity_key(node)}
    if k in ('const', 'volatile', 'array'):
        return name_deps(node['inner'], seen)
    if k == 'ptr':
        return name_deps(node['inner'] or {'kind': 'void'}, seen)
    if k == 'func':
        s = name_deps(node['ret'] or {'kind': 'void'}, seen)
        for p in node['params']:
            s |= name_deps(p, seen)
        return s
    return set()


def full_deps(node, seen=None):
    """Entities whose *full body* must precede a BY-VALUE use of `node`
    (a pointer anywhere in the chain breaks this -- a pointer's own size
    never depends on what it points to)."""
    if seen is None:
        seen = set()
    nid = id(node)
    if nid in seen:
        return set()
    seen.add(nid)
    k = node['kind']
    if k in ('const', 'volatile', 'array', 'typedef'):
        return full_deps(node['inner'], seen)
    if k in ('struct', 'union'):
        return {entity_key(node)}
    return set()  # ptr/func/base/enum/void: never need completeness here


def entity_deps(key, node):
    if node['kind'] == 'typedef':
        # `typedef struct X Y;` (Y's underlying type is DIRECTLY a struct/
        # union) needs only X's tag, which every struct/union already gets
        # forward-declared unconditionally -- no ordering edge required.
        # This is exactly the classic libpng png_struct/png_structp
        # pattern (struct <-> pointer-typedef <-> pointer-typedef, broken
        # in real C by typedef-ing the still-incomplete struct); treating
        # it as a hard dependency produces a false cycle. Any deeper
        # by-value use (array-of-struct-typedef, nested typedef chains)
        # still needs full completeness, computed below.
        inner = node['inner']
        peek = inner
        while peek['kind'] in ('const', 'volatile'):
            peek = peek['inner']
        if peek['kind'] in ('struct', 'union'):
            return set()
        return (name_deps(inner) | full_deps(inner)) - {key}
    if node['kind'] in ('struct', 'union'):
        deps = set()
        for (_, mt, _) in node['members']:
            deps |= name_deps(mt) | full_deps(mt)
        return deps - {key}
    return set()


def topo_order(nodes):
    """Post-order DFS over struct/union/typedef entities so every by-value
    dependency (and every typedef-name use, which C cannot forward-declare)
    is emitted before the entity that needs it. Cycles cannot occur in
    valid C by-value graphs; if one appears anyway (a parser edge case) we
    break it and record the anomaly rather than recursing forever."""
    order = []
    state = {}
    anomalies = []

    def visit(key, chain):
        st = state.get(key, 0)
        if st == 2:
            return
        if st == 1:
            anomalies.append({'cycle': [str(k) for k in chain + [key]]})
            return
        state[key] = 1
        node = nodes[key]
        for dep in sorted(entity_deps(key, node)):
            if dep in nodes:
                visit(dep, chain + [key])
        state[key] = 2
        order.append(key)

    su_td_keys = sorted([k for k in nodes if k[0] in ('su', 'td')], key=lambda k: (k[0], k[1]))
    for key in su_td_keys:
        visit(key, [])
    return order, anomalies


# --------------------------------------------------------------------------
# Struct/union body emission with pack(1) + explicit padding, computed
# purely from DWARF-reported member offsets and sizes (never from the
# compiler's own alignment rules), so the generated static_assert /
# offsetof checks are true by construction.
# --------------------------------------------------------------------------

def emit_struct_body(node, opaque_reasons):
    lines = []
    tagword = node['kind']
    lines.append('%s %s {' % (tagword, node['name']))
    if node['byte_size'] is None:
        opaque_reasons.append({'name': node['name'], 'kind': tagword,
                                'reason': 'no DW_AT_byte_size (declaration-only, never fully defined in scope)'})
        lines[-1] = '/* OPAQUE: %s %s -- no definition found, size unknown */' % (tagword, node['name'])
        lines.append('typedef struct %s_OPAQUE_UNSIZED { int _unrepresented; } %s;' % (node['name'], node['name']))
        return lines
    cursor = 0
    pad_n = 0
    members = sorted(node['members'], key=lambda m: m[2]) if tagword == 'struct' else node['members']
    body = []
    if tagword == 'struct':
        for (mname, mtype, moff) in members:
            if moff > cursor:
                body.append('    unsigned char _pad_%d[%d];' % (pad_n, moff - cursor))
                pad_n += 1
                cursor = moff
            msize = sizeof_node(mtype)
            if msize == 0 and mtype['kind'] == 'array' and None in mtype['dims']:
                # Flexible-array-style member (DWARF gives no upper bound;
                # e.g. Allegro's `BITMAP.line[]` scanline pointer table,
                # real length == bmp->h, over-allocated after the struct).
                # Emit with the real element type and a minimum bound of 1
                # instead of collapsing to an untyped byte blob, so callers
                # can still index it; it never affects the struct's own
                # size because it sits at the tail (moff == byte_size).
                opaque_reasons.append({'name': node['name'], 'member': mname,
                                        'reason': 'array member with unknown bound (C flexible array, DWARF contributes 0 bytes to the struct)'})
                # GCC's DW_AT_byte_size for the enclosing struct already
                # excludes this member entirely (verified: BITMAP's 16
                # fixed fields alone already sum to its declared 64 bytes),
                # so it must render as a true zero-contribution array, not
                # a [1]-sized guess (which measurably changed sizeof() and
                # broke the offsetof/sizeof self-checks below). [0] is a
                # long-standing MSVC extension (warning C4200) that keeps
                # the real element type without adding bytes.
                fixed = dict(mtype, dims=[0 if dd is None else dd for dd in mtype['dims']])
                body.append('    %s; /* DWARF: unbounded array, kept as zero-length (see it_types.h notes) */' % decl(fixed, mname))
                continue
            body.append('    %s;' % decl(mtype, mname))
            cursor = max(cursor, moff + msize)
        if node['byte_size'] > cursor:
            body.append('    unsigned char _pad_%d[%d];' % (pad_n, node['byte_size'] - cursor))
            cursor = node['byte_size']
    else:  # union: no inter-member padding, just a trailing pad to declared size
        maxsz = 0
        for (mname, mtype, _moff) in members:
            body.append('    %s;' % decl(mtype, mname))
            maxsz = max(maxsz, sizeof_node(mtype))
        if node['byte_size'] > maxsz:
            body.append('    unsigned char _pad_tail[%d];' % (node['byte_size'] - maxsz))
        cursor = node['byte_size']
    if not body:
        body.append('    unsigned char _empty[%d];' % max(node['byte_size'], 1))
    lines.extend(body)
    lines.append('};')
    if cursor != node['byte_size']:
        opaque_reasons.append({'name': node['name'], 'reason':
                                'computed layout size %d != DWARF byte_size %d' % (cursor, node['byte_size'])})
    return lines


# --------------------------------------------------------------------------
# Function / global collection from the scoped CUs
# --------------------------------------------------------------------------

def in_scope_cus(dies, scope):
    out = []
    for off, d in dies.items():
        if d['tag'] != 'DW_TAG_compile_unit':
            continue
        name = parse_name(d['attrs'].get('DW_AT_name')) or ''
        if scope == 'all' or name.lower().replace('/', '\\').startswith(GAME_PREFIX):
            out.append(off)
    return out


def collect_functions(dies, cu_off, canonical, cache, out):
    cu_die = dies[cu_off]
    cu_name = parse_name(cu_die['attrs'].get('DW_AT_name'))
    for child in cu_die['children']:
        cd = dies[child]
        if cd['tag'] != 'DW_TAG_subprogram':
            continue
        a = cd['attrs']
        if 'DW_AT_low_pc' not in a:
            continue
        low = parse_int(a['DW_AT_low_pc']) if not a['DW_AT_low_pc'].startswith('0x') else int(a['DW_AT_low_pc'], 16)
        high_raw = a.get('DW_AT_high_pc')
        high = int(high_raw, 16) if high_raw and high_raw.startswith('0x') else (low + (parse_int(high_raw) or 0))
        name = parse_name(a.get('DW_AT_name'))
        if not name:
            continue
        ret = resolve_type(parse_ref(a.get('DW_AT_type')), dies, canonical, cache) if 'DW_AT_type' in a else {'kind': 'void'}
        params = []
        variadic = False
        for c in cd['children']:
            pd = dies[c]
            if pd['tag'] == 'DW_TAG_formal_parameter':
                params.append(resolve_type(parse_ref(pd['attrs'].get('DW_AT_type')), dies, canonical, cache))
            elif pd['tag'] == 'DW_TAG_unspecified_parameters':
                variadic = True
        out.append({
            'name': name, 'va': low, 'high_pc': high, 'external': 'DW_AT_external' in a,
            'prototyped': 'DW_AT_prototyped' in a, 'ret': ret, 'params': params,
            'variadic': variadic, 'cu': cu_name,
        })


def collect_globals(dies, cu_off, canonical, cache, out):
    cu_die = dies[cu_off]
    cu_name = parse_name(cu_die['attrs'].get('DW_AT_name'))

    def walk(off, func_name):
        d = dies[off]
        if d['tag'] == 'DW_TAG_variable':
            a = d['attrs']
            addr = parse_op_addr(a.get('DW_AT_location'))
            name = parse_name(a.get('DW_AT_name'))
            if addr is not None and name:
                t = resolve_type(parse_ref(a.get('DW_AT_type')), dies, canonical, cache) if 'DW_AT_type' in a else {'kind': 'void'}
                out.append({'name': name, 'va': addr, 'external': 'DW_AT_external' in a,
                             'type': t, 'cu': cu_name, 'local_static_of': func_name})
        if d['tag'] == 'DW_TAG_subprogram':
            fname = parse_name(d['attrs'].get('DW_AT_name')) or func_name
            for c in d['children']:
                walk(c, fname)
        elif d['depth'] >= 1:
            for c in d['children']:
                walk(c, func_name)

    for child in cu_die['children']:
        walk(child, None)


# --------------------------------------------------------------------------
# Name collision resolution (static/local symbols can repeat across CUs)
# --------------------------------------------------------------------------

def resolve_collisions(items, get_name, get_cu):
    by_name = defaultdict(list)
    for it in items:
        by_name[get_name(it)].append(it)
    collisions = []
    for name, group in by_name.items():
        if len(group) <= 1:
            group[0]['emit_name'] = name
            continue
        group.sort(key=lambda it: it.get('va', 0))
        collisions.append({'name': name, 'count': len(group),
                            'cus': [get_cu(it) for it in group]})
        for it in group:
            it['emit_name'] = '%s__%s' % (name, cu_basename(get_cu(it)))
    return collisions


# --------------------------------------------------------------------------
# Calling convention detection from COFF symbol decoration
# --------------------------------------------------------------------------

def load_coff_va_map(functions_json_path):
    va_map = {}
    d = os.path.join(os.path.dirname(functions_json_path))
    coff_path = os.path.join(d, 'coff_symbols.json')
    if os.path.exists(coff_path):
        try:
            syms = json.load(open(coff_path, encoding='utf-8'))
            for s in syms:
                if s.get('section_name') == '.text':
                    va_map.setdefault(s['va'], s['name'])
        except Exception:
            pass
    return va_map


def calling_convention_for(va, coff_va_map):
    """KNOWN: MinGW COFF decorates __stdcall symbols as `_name@N`; a plain
    leading underscore with no @N is __cdecl (verified: create_post ->
    "_create_post", _WinMain@16 -> stdcall callback). Functions with no
    matching COFF text symbol default to cdecl, marked INFERRED."""
    name = coff_va_map.get(va)
    if name is None:
        return 'cdecl', 'INFERRED (no COFF symbol at this VA)'
    m = re.search(r'@(\d+)$', name)
    if m:
        return 'stdcall', 'KNOWN (COFF symbol %s)' % name
    return 'cdecl', 'KNOWN (COFF symbol %s)' % name


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

HEADER_TMPL = """/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_interop.py from:
 *   %(dwarf)s
 *   %(functions)s
 * scope=%(scope)s. Re-run gen_interop.py to regenerate; do not hand-edit.
 * See win32_pilot.md SS3: the original image is mapped in-process at its
 * original base 0x400000 with no relocations, so a global at VA X is
 * *(T*)X and a function at VA F is ((ret(__cdecl*)(args))F).
 */
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dwarf', required=True)
    ap.add_argument('--functions', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--scope', choices=['game', 'all'], default='game')
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    print('parsing DWARF...', file=sys.stderr)
    dies = parse_dwarf(args.dwarf)
    print('DIEs: %d' % len(dies), file=sys.stderr)
    global ANON_TYPEDEF_NAMES
    ANON_TYPEDEF_NAMES = compute_anon_typedef_names(dies)
    canonical, conflicts = compute_canonical(dies, ANON_TYPEDEF_NAMES)
    cache = {}

    scope_cus = in_scope_cus(dies, args.scope)
    print('in-scope CUs: %d' % len(scope_cus), file=sys.stderr)

    functions = []
    globals_ = []
    for cu_off in scope_cus:
        collect_functions(dies, cu_off, canonical, cache, functions)
        collect_globals(dies, cu_off, canonical, cache, globals_)

    coff_va_map = load_coff_va_map(args.functions)
    for fn in functions:
        conv, evidence = calling_convention_for(fn['va'], coff_va_map)
        fn['calling_convention'] = conv
        fn['convention_evidence'] = evidence

    func_collisions = resolve_collisions(functions, lambda f: sanitize_ident(f['name']), lambda f: f['cu'])
    for g in globals_:
        base = sanitize_ident(g['name']) + ('__%s' % sanitize_ident(g['local_static_of']) if g['local_static_of'] else '')
        g['_basename'] = base
    global_collisions = resolve_collisions(globals_, lambda g: g['_basename'], lambda g: g['cu'])

    functions.sort(key=lambda f: (f['emit_name'], f['va']))
    globals_.sort(key=lambda g: (g['emit_name'], g['va']))

    # cross-check byte sizes against functions.json (best-effort, non-fatal)
    fn_sizes = {}
    try:
        fj = json.load(open(args.functions, encoding='utf-8'))
        for e in fj:
            try:
                fn_sizes[int(e['va'], 16)] = e.get('size')
            except Exception:
                pass
    except Exception:
        pass
    for fn in functions:
        fn['size'] = fn_sizes.get(fn['va']) or (fn['high_pc'] - fn['va'])

    # ---- reachable named types ----
    seen_ids = set()
    nodes = {}
    for fn in functions:
        collect_reachable(fn['ret'], seen_ids, nodes)
        for p in fn['params']:
            collect_reachable(p, seen_ids, nodes)
    for g in globals_:
        collect_reachable(g['type'], seen_ids, nodes)

    order, cycle_anomalies = topo_order(nodes)
    opaque_reasons = []

    # ---- emit it_types.h ----
    lines = [HEADER_TMPL % {'dwarf': args.dwarf, 'functions': args.functions, 'scope': args.scope}]
    lines.append('#ifndef IT_TYPES_H')
    lines.append('#define IT_TYPES_H')
    lines.append('')
    lines.append('#pragma pack(push, 1)')
    lines.append('')
    su_keys = sorted([k for k in nodes if k[0] == 'su'], key=lambda k: nodes[k]['name'])
    lines.append('/* forward declarations */')
    for k in su_keys:
        n = nodes[k]
        lines.append('%s %s;' % (n['kind'], n['name']))
    lines.append('')
    en_keys = sorted([k for k in nodes if k[0] == 'en'], key=lambda k: nodes[k]['name'])
    lines.append('/* enumerations */')
    for k in en_keys:
        n = nodes[k]
        lines.append('enum %s {' % n['name'])
        for (vname, vval) in n['values']:
            lines.append('    %s = %d,' % (vname, vval))
        if not n['values']:
            lines.append('    %s_UNUSED = 0,' % n['name'])
        lines.append('};')
        lines.append('typedef enum %s %s_e;' % (n['name'], n['name']))
    lines.append('')
    lines.append('/* struct / union bodies and typedefs, in dependency order */')
    emitted_su_names = set()
    for key in order:
        n = nodes[key]
        if key[0] == 'su':
            if n['name'] in emitted_su_names:
                continue
            emitted_su_names.add(n['name'])
            lines.extend(emit_struct_body(n, opaque_reasons))
        elif key[0] == 'td':
            lines.append('typedef %s;' % decl(n['inner'], n['name']))
    lines.append('')
    lines.append('#pragma pack(pop)')
    lines.append('')
    lines.append('#endif /* IT_TYPES_H */')
    types_h = '\n'.join(lines) + '\n'
    with open(os.path.join(args.out, 'it_types.h'), 'w', encoding='utf-8') as f:
        f.write(types_h)

    # ---- emit it_globals.h ----
    lines = [HEADER_TMPL % {'dwarf': args.dwarf, 'functions': args.functions, 'scope': args.scope}]
    lines.append('#ifndef IT_GLOBALS_H')
    lines.append('#define IT_GLOBALS_H')
    lines.append('#include "it_types.h"')
    lines.append('')
    for g in globals_:
        ptr_node = {'kind': 'ptr', 'inner': g['type']}
        cast_type = decl(ptr_node, '')
        ptr_decl = decl(ptr_node, 'g_%s_p' % g['emit_name'])
        lines.append('/* %s  VA=0x%x  cu=%s%s */' % (
            g['name'], g['va'], g['cu'], '' if g['external'] else ' (static)'))
        lines.append('#define IT_G_%s (*(%s)0x%x)' % (g['emit_name'], cast_type, g['va']))
        lines.append('static %s = (%s)0x%x;' % (ptr_decl, cast_type, g['va']))
        lines.append('')
    lines.append('#endif /* IT_GLOBALS_H */')
    with open(os.path.join(args.out, 'it_globals.h'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')

    # ---- emit it_funcs.h + it_funcs_table.inc ----
    lines = [HEADER_TMPL % {'dwarf': args.dwarf, 'functions': args.functions, 'scope': args.scope}]
    lines.append('#ifndef IT_FUNCS_H')
    lines.append('#define IT_FUNCS_H')
    lines.append('#include "it_types.h"')
    lines.append('')
    table_lines = ['/* GENERATED FILE -- DO NOT EDIT. See it_funcs.h header comment. */']
    for fn in functions:
        conv = '__stdcall' if fn['calling_convention'] == 'stdcall' else '__cdecl'
        params = [decl(p, '') for p in fn['params']]
        if fn['variadic']:
            params.append('...')
        paramstr = ', '.join(params) if params else ('void' if fn['prototyped'] else '')
        ret_str = decl(fn['ret'], '')
        lines.append('/* %s  VA=0x%x  size=%d  cu=%s  conv=%s [%s] */' % (
            fn['name'], fn['va'], fn['size'], fn['cu'], fn['calling_convention'], fn['convention_evidence']))
        lines.append('typedef %s (%s *PFN_%s)(%s);' % (ret_str, conv, fn['emit_name'], paramstr))
        lines.append('#define IT_F_%s ((PFN_%s)0x%x)' % (fn['emit_name'], fn['emit_name'], fn['va']))
        lines.append('')
        table_lines.append('{ "%s", 0x%x, %d, "%s" },' % (fn['name'], fn['va'], fn['size'], fn['cu'].replace('\\', '\\\\')))
    lines.append('#endif /* IT_FUNCS_H */')
    with open(os.path.join(args.out, 'it_funcs.h'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    with open(os.path.join(args.out, 'it_funcs_table.inc'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(table_lines) + '\n')

    # ---- interop_index.json ----
    index = {
        'scope': args.scope,
        'dwarf_source': args.dwarf,
        'functions_source': args.functions,
        'globals': [
            {'name': g['emit_name'], 'dwarf_name': g['name'], 'va': '0x%x' % g['va'],
             'type': decl(g['type'], ''), 'cu': g['cu'], 'external': g['external']}
            for g in globals_
        ],
        'functions': [
            {'name': fn['emit_name'], 'dwarf_name': fn['name'], 'va': '0x%x' % fn['va'], 'size': fn['size'],
             'prototype': '%s %s(%s)' % (decl(fn['ret'], ''), fn['name'],
                                          ', '.join([decl(p, '') for p in fn['params']] + (['...'] if fn['variadic'] else []))),
             'cu': fn['cu'], 'calling_convention': fn['calling_convention'], 'evidence': fn['convention_evidence']}
            for fn in functions
        ],
        'opaque_types': opaque_reasons,
        'type_conflicts': conflicts,
        'topo_cycle_anomalies': cycle_anomalies,
        'name_collisions': {'functions': func_collisions, 'globals': global_collisions},
        'long_double_substitutions': sorted(set(LONG_DOUBLE_NOTE)),
        'counts': {
            'structs_unions': len(su_keys), 'enums': len(en_keys),
            'typedefs': len([k for k in nodes if k[0] == 'td']),
            'globals': len(globals_), 'functions': len(functions),
        },
    }
    with open(os.path.join(args.out, 'interop_index.json'), 'w', encoding='utf-8') as f:
        json.dump(index, f, indent=1, sort_keys=True)

    # ---- it_selftest.c ----
    sample_globals = [g for g in globals_ if g['name'] in ('cycle_count', 'key')]
    sample_funcs = [f for f in functions if f['name'] in ('play', 'draw_frame', 'add_floor', 'handle_player_input')]
    sel = [HEADER_TMPL % {'dwarf': args.dwarf, 'functions': args.functions, 'scope': args.scope}]
    sel.append('/* Compile-only sanity check: does NOT link or run against the game. */')
    sel.append('#include "it_types.h"')
    sel.append('#include "it_globals.h"')
    sel.append('#include "it_funcs.h"')
    sel.append('#include <stddef.h>')
    sel.append('')
    sel.append('#define IT_CT_ASSERT(cond, tag) typedef char it_ct_assert_##tag[(cond) ? 1 : -1]')
    for key in order:
        n = nodes[key]
        if key[0] == 'su' and n.get('byte_size'):
            sel.append('IT_CT_ASSERT(sizeof(%s %s) == %d, sz_%s);' % (n['kind'], n['name'], n['byte_size'], n['name']))
            for (mname, mtype, moff) in n['members']:
                sel.append('IT_CT_ASSERT(offsetof(%s %s, %s) == %d, off_%s_%s);' % (
                    n['kind'], n['name'], mname, moff, n['name'], mname))
    sel.append('')
    sel.append('void it_selftest_touch(void) {')
    for g in sample_globals:
        sel.append('    (void)&IT_G_%s;' % g['emit_name'])
    for fn in sample_funcs:
        sel.append('    (void)IT_F_%s;' % fn['emit_name'])
    if not sample_globals and not sample_funcs:
        sel.append('    /* no sample symbols matched in this scope */')
    sel.append('}')
    with open(os.path.join(args.out, 'it_selftest.c'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(sel) + '\n')

    # ---- it_types_check.c (standalone, runnable, no image access) ----
    chk = [HEADER_TMPL % {'dwarf': args.dwarf, 'functions': args.functions, 'scope': args.scope}]
    chk.append('/* Standalone host program: verifies sizeof/offsetof of every generated')
    chk.append(' * struct/union against the DWARF-reported layout. Touches no game memory. */')
    chk.append('#include "it_types.h"')
    chk.append('#include <stddef.h>')
    chk.append('#include <stdio.h>')
    chk.append('')
    chk.append('int main(void) {')
    chk.append('    int failures = 0;')
    for key in order:
        n = nodes[key]
        if key[0] != 'su' or not n.get('byte_size'):
            continue
        chk.append('    if (sizeof(%s %s) != %d) { failures++; printf("FAIL sizeof(%s %s) = %%u expected %d\\n", (unsigned)sizeof(%s %s)); }'
                    ' else { printf("PASS sizeof(%s %s)\\n"); }' % (
                        n['kind'], n['name'], n['byte_size'], n['kind'], n['name'], n['byte_size'],
                        n['kind'], n['name'], n['kind'], n['name']))
        for (mname, mtype, moff) in n['members']:
            chk.append('    if (offsetof(%s %s, %s) != %d) { failures++; printf("FAIL offsetof(%s %s, %s) = %%u expected %d\\n", (unsigned)offsetof(%s %s, %s)); }'
                        ' else { printf("PASS offsetof(%s %s, %s)\\n"); }' % (
                            n['kind'], n['name'], mname, moff, n['kind'], n['name'], mname, moff,
                            n['kind'], n['name'], mname, n['kind'], n['name'], mname))
    chk.append('    printf(failures == 0 ? "ALL PASS\\n" : "%d FAILURES\\n", failures);')
    chk.append('    return failures ? 1 : 0;')
    chk.append('}')
    with open(os.path.join(args.out, 'it_types_check.c'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(chk) + '\n')

    # ---- INTEROP_NOTES.md ----
    notes = []
    notes.append('# Interop generator notes\n')
    notes.append('Generated by `carrier/gen/gen_interop.py` from `%s` + `%s`, scope=`%s`.\n' % (
        args.dwarf, args.functions, args.scope))
    notes.append('## Counts (KNOWN, from this run)\n')
    notes.append('| item | count |\n|---|---:|\n')
    for k, v in index['counts'].items():
        notes.append('| %s | %d |\n' % (k, v))
    notes.append('\nCU scope: %d compile units in scope out of %d total in the DWARF.\n' % (
        len(scope_cus), sum(1 for d in dies.values() if d['tag'] == 'DW_TAG_compile_unit')))
    notes.append('\n## Calling convention detection (KNOWN)\n')
    notes.append('DWARF carries no `DW_AT_calling_convention` anywhere in this dump (checked). '
                  'Convention is instead read from COFF symbol table decoration '
                  '(`carrier/gen`\'s `--functions` sibling `coff_symbols.json`, or defaults to '
                  'cdecl if no COFF symbol matches the VA): a MinGW `__stdcall` symbol is '
                  'decorated `_name@N`; a plain `_name` is `__cdecl`. '
                  'Verified on `create_post` (`_create_post`, cdecl) and `_WinMain@16` (stdcall).\n')
    stdcall_fns = [f for f in functions if f['calling_convention'] == 'stdcall']
    notes.append('%d of %d emitted functions detected as `__stdcall`.\n' % (len(stdcall_fns), len(functions)))
    inferred_fns = [f for f in functions if f['convention_evidence'].startswith('INFERRED')]
    if inferred_fns:
        notes.append('%d functions had no matching COFF symbol and default to cdecl (INFERRED): %s\n' % (
            len(inferred_fns), ', '.join(sorted(f['name'] for f in inferred_fns))[:2000]))
    notes.append('\nEmbedded function-pointer *type* members (e.g. Allegro driver vtables) always '
                  'render as `__cdecl` (INFERRED): DWARF subroutine_type DIEs used only as pointer '
                  'targets have no associated COFF symbol to check, so this is a default, not a '
                  'measurement. Most reachable examples are Allegro internal method tables, which '
                  'are cdecl by convention (`AL_METHOD`); Win32 callback typedefs (WNDPROC etc.) '
                  'would need `__stdcall` and are NOT auto-detected -- flag if one is found broken.\n')
    notes.append('\n## long double substitution (KNOWN)\n')
    if LONG_DOUBLE_NOTE:
        notes.append('MSVC `long double` == `double` (8 bytes); GCC x86 `long double` is 80-bit '
                      'extended precision stored in %s byte(s). Every occurrence reachable in this '
                      'scope was emitted as `unsigned char[N]` instead of `long double` to preserve '
                      'the original byte layout; no bridging to a real float80 type is attempted '
                      '(matches win32_pilot.md SS3\'s x87-softfloat-if-needed stance).\n' % sorted(set(LONG_DOUBLE_NOTE)))
    else:
        notes.append('Not reachable in this scope (`long double` appears in the DWARF file overall '
                      'but no in-scope function/global in this run referenced it).\n')
    notes.append('\n## Opaque / unrepresentable types (%d)\n' % len(opaque_reasons))
    if opaque_reasons:
        for o in opaque_reasons:
            notes.append('- `%s`: %s\n' % (o.get('name'), o.get('reason')))
    else:
        notes.append('None in this scope.\n')
    notes.append('\n## Struct/union/typedef name conflicts across CUs (%d)\n' % len(conflicts))
    if conflicts:
        for c in conflicts[:50]:
            if 'canonical_size' in c:
                notes.append('- `%s %s`: canonical size %s at offset %s conflicts with size %s at offset %s '
                              '(kept the canonical one; DIEs likely describe genuinely different types that '
                              'happen to share a tag name)\n' % (
                                  c['kind'], c['name'], c['canonical_size'], c['canonical_offset'],
                                  c['conflict_size'], c['conflict_offset']))
            else:
                notes.append('- `%s %s`: %s (canonical offset %s, conflicting offset %s)\n' % (
                    c['kind'], c['name'], c['note'], c['canonical_offset'], c['conflict_offset']))
        if len(conflicts) > 50:
            notes.append('- ... and %d more (see interop_index.json `type_conflicts`)\n' % (len(conflicts) - 50))
    else:
        notes.append('None -- every duplicated struct/union/enum/typedef definition across the CUs in scope agreed.\n')
    notes.append('\n## Name collisions (disambiguated with `__<cu-basename>`)\n')
    notes.append('Functions: %d collisions. Globals: %d collisions.\n' % (
        len(func_collisions), len(global_collisions)))
    for c in func_collisions[:50]:
        notes.append('- function `%s` defined in %d CUs: %s\n' % (c['name'], c['count'], ', '.join(c['cus'])))
    for c in global_collisions[:50]:
        notes.append('- global `%s` defined in %d CUs: %s\n' % (c['name'], c['count'], ', '.join(c['cus'])))
    notes.append('\n## Topological-order anomalies (%d)\n' % len(cycle_anomalies))
    if cycle_anomalies:
        for a in cycle_anomalies:
            notes.append('- %s\n' % a)
    else:
        notes.append('None -- no by-value/typedef dependency cycles found (expected for valid C).\n')
    notes.append('\n## Bitfields\n')
    notes.append('None found anywhere in the DWARF dump (`DW_AT_bit_size` occurs 0 times); '
                  'no bitfield support was implemented.\n')
    notes.append('\n## Compile / check results\n')
    notes.append('Not run by this invocation. To verify (32-bit MSVC):\n\n')
    notes.append('```\n"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat"\n'
                  'cd %s\ncl /nologo /c /W3 /TC it_selftest.c\n'
                  'cl /nologo /W3 /TC it_types_check.c /Fe:it_types_check.exe\n'
                  'it_types_check.exe\n```\n' % args.out)
    notes.append('`it_selftest.c` must produce zero errors (its `IT_CT_ASSERT` lines are compile-time '
                  'sizeof/offsetof checks -- any mismatch is a hard compile error, not a warning). '
                  '`it_types_check.exe` must print `ALL PASS`. See the repo history / prior run notes '
                  'for the last verified result on this exact output.\n')
    with open(os.path.join(args.out, 'INTEROP_NOTES.md'), 'w', encoding='utf-8') as f:
        f.write(''.join(notes))

    print('done: %d globals, %d functions, %d structs/unions, %d enums, %d typedefs' % (
        len(globals_), len(functions), len(su_keys), len(en_keys), len([k for k in nodes if k[0] == 'td'])),
        file=sys.stderr)


if __name__ == '__main__':
    main()
