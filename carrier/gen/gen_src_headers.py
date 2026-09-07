#!/usr/bin/env python3
"""gen_src_headers.py -- generates src/icytower/game_types.h, allegro_types.h,
game_state.h, game_funcs.h and state.c from DWARF debug info of the original
icytower15.exe, so the clean port (win32_pilot.md SS7a, src/README.md) no
longer carries hand-transcribed struct layouts.

This is deliberately NOT a second DWARF parser. It imports carrier/gen/
gen_interop.py as a module and reuses its parsed model verbatim: the same
`parse_dwarf`/`resolve_type`/`decl`/`emit_struct_body`/`topo_order` machinery
that already produces carrier/gen/it_types.h (verified: 801/801 sizeof/
offsetof checks PASS against real MSVC, see INTEROP_NOTES.md) is reused
here unmodified, so game_types.h's struct bodies are byte-for-byte the same
recovery gen_interop.py already proved correct -- this script only adds:

  1. an origin split (gen_interop.compute_type_origin, added additively to
     gen_interop.py by this same change) so the game's own structs
     (Tplayer, Tmap, Tfloor, ...) land in game_types.h while public library
     types the game merely uses (BITMAP, RGB, SAMPLE, fixed, ...) land in
     allegro_types.h -- gen_interop.py's own it_types.h does not need this
     distinction, so it never had it;
  2. parameter NAME capture for game_funcs.h (gen_interop.collect_functions
     only resolves parameter TYPES, since it_funcs.h only needs typedefs);
  3. plain extern/prototype/definition text instead of address-cast macros
     -- no VA ever appears in src/, unlike it_globals.h/it_funcs.h, which
     is the entire point of this being a SEPARATE generator from
     gen_interop.py rather than a flag on it (win32_pilot.md SS7a: src/ is
     address-free; carrier/gen/ is not).

Usage:
    python gen_src_headers.py --dwarf artifacts/dwarf_info.txt \
        --functions artifacts/functions.json --out src/icytower --scope game

Outputs (into --out):
    allegro_types.h      public library types (Allegro/CRT) the game uses
    game_types.h          the game's own struct/union/enum/typedef layouts
    game_types_check.c    standalone sizeof/offsetof PASS/FAIL host program
    game_state.h          extern declarations of every game-CU global
    game_funcs.h           prototypes of every game-CU function
    state.c                zero-initialized storage for every game_state.h
                            extern (standalone-build world only)
    GENERATED.md            counts, opaque types, collisions

No guest address (hex or decimal) is ever written into any of these files;
scripts/check_native_layer.py is the gate that proves it.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import gen_interop as gi  # noqa: E402  (reused DWARF parser + type IR)

BINDINGS_GUARD = 'ICYTOWER_BINDINGS_ACTIVE'

# Standalone-build LIBRARIES-coastline swap (carrier/gen/LIB_BINDINGS_NOTES.md
# "Standalone build swap"): when this is #defined, allegro_types.h's stand-in
# struct/typedef bodies are skipped and it #includes the REAL upstream
# <allegro.h> instead. Mirrors BINDINGS_GUARD's shape (one macro, checked
# with a plain #if/#elif, no other file needs to know it exists) so the two
# skip conditions never fight: exactly one of BINDINGS_ACTIVE, UPSTREAM, or
# neither is ever defined in a given translation unit.
UPSTREAM_GUARD = 'ICYTOWER_UPSTREAM_ALLEGRO'

# Hand-curated, same spirit and same narrow scope as carrier/gen/
# gen_bindings.py's MEMBER_ACCESS_COLLISIONS (found together, PROMOTIONS.md
# batch 7): a DWARF parameter NAME that collides with an Allegro/game global
# name some OTHER force-included bindings header (carrier/gen/
# pf_lib_bindings.h here -- Allegro's own `extern volatile char key[...]`)
# binds to a macro. game_funcs.h emits a plain forward DECLARATION (never a
# definition -- see this file's own header comment, point 3), so the
# parameter name has zero semantic effect on any caller; renaming it here
# only in the prototype text is enough to stop the macro from also
# rewriting it (`int check_control_key(Tcontrol *c, int key)` ->
# `int check_control_key(Tcontrol *c, int (*(volatile char(*)[127])0x...))`
# otherwise -- a syntax error, found compiling start_reward.c, the first
# src/ file to include BOTH game_funcs.h and (transitively, for
# asset_bitmap()) pf_lib_bindings.h in the same translation unit).
PROTOTYPE_PARAM_RENAMES = {
    'key': 'key_arg',   # check_control_key(Tcontrol *, int key) -- collides
                         # with Allegro's `key[]` keyboard-state array via
                         # pf_lib_bindings.h.
}

# scripts/check_native_layer.py's BANNED_IDENT_RE, reproduced here (not
# imported -- it is a purity-gate detail, not part of the DWARF model this
# script otherwise reuses from gen_interop.py) only so this generator can
# avoid ever emitting a name the gate would reject, instead of emitting one
# and finding out from the gate's own output. A handful of genuine 2011
# Allegro/game identifiers collide with it by pure coincidence -- Allegro's
# own PACKFILE_VTABLE member-naming convention ("packfile function") uses
# exactly the `pf_` prefix the gate reserves for PortForge's carrier
# vocabulary. Any such name is renamed with an `icy_orig_` prefix (parallel
# to gen_interop.py's own `it_orig_` CRT-collision rename) and the rename is
# recorded in GENERATED.md; nothing else about the name changes.
# `pf_` (lowercase) is deliberately NOT here: Allegro's PACKFILE_VTABLE members
# (pf_fclose, pf_getc, ...) are the library's real names and must be kept; the
# purity gate bans only the carrier's specific pf_* tokens.
PURITY_BANNED_PREFIXES = ('PF_', 'IT_G_', 'IT_F_', 'PFN_', 'lifted_')


def purity_safe_ident(name, renames):
    for p in PURITY_BANNED_PREFIXES:
        if name.startswith(p):
            safe = 'icy_orig_' + name
            renames.append((name, safe))
            return safe
    return name


def apply_purity_renames(order, nodes, functions, globals_):
    renames = []
    for key in order:
        node = nodes[key]
        node['name'] = purity_safe_ident(node['name'], renames)
        if node['kind'] in ('struct', 'union'):
            node['members'] = [(purity_safe_ident(mname, renames), mtype, moff)
                                for (mname, mtype, moff) in node['members']]
        elif node['kind'] == 'enum':
            node['values'] = [(purity_safe_ident(vname, renames), vval)
                               for (vname, vval) in node['values']]
    for fn in functions:
        fn['emit_name'] = purity_safe_ident(fn['emit_name'], renames)
        fn['params'] = [(purity_safe_ident(pname, renames), ptype) for (pname, ptype) in fn['params']]
    for g in globals_:
        g['emit_name'] = purity_safe_ident(g['emit_name'], renames)
    return renames


# --------------------------------------------------------------------------
# Function collection with parameter NAMES (gen_interop.collect_functions
# only keeps parameter TYPES, since it_funcs.h only needs PFN_ typedefs).
# Reuses resolve_type/parse_name/parse_ref/parse_int/sanitize_ident from the
# imported module instead of re-deriving type resolution.
# --------------------------------------------------------------------------

def collect_functions_named(dies, cu_off, canonical, cache, out):
    cu_die = dies[cu_off]
    cu_name = gi.parse_name(cu_die['attrs'].get('DW_AT_name'))
    for child in cu_die['children']:
        cd = dies[child]
        if cd['tag'] != 'DW_TAG_subprogram':
            continue
        a = cd['attrs']
        if 'DW_AT_low_pc' not in a:
            continue
        low = a['DW_AT_low_pc']
        low = int(low, 16) if low.startswith('0x') else gi.parse_int(low)
        name = gi.parse_name(a.get('DW_AT_name'))
        origin = None
        if not name:
            # Concrete instance carrying only DW_AT_abstract_origin/
            # DW_AT_specification (e.g. a function also inlined elsewhere,
            # per DW_AT_inline on the origin DIE) -- see
            # gen_interop.follow_origin()'s docstring; without this, such a
            # function has no name anywhere in game_funcs.h even though
            # DWARF does carry one (e.g. set_control, control.c).
            origin = gi.follow_origin(child, dies)
            name = gi.parse_name(origin['attrs'].get('DW_AT_name')) if origin else None
        if not name:
            continue
        decl_off = origin['offset'] if origin is not None else child
        decl_a = origin['attrs'] if origin is not None else a
        ret = gi.resolve_type(gi.parse_ref(decl_a.get('DW_AT_type')), dies, canonical, cache) \
            if 'DW_AT_type' in decl_a else {'kind': 'void'}
        params = []
        variadic = False
        for c in dies[decl_off]['children']:
            pd = dies[c]
            if pd['tag'] == 'DW_TAG_formal_parameter':
                pa = pd['attrs']
                pname = gi.parse_name(pa.get('DW_AT_name'))
                pt_off = gi.parse_ref(pa.get('DW_AT_type'))
                if pname is None or pt_off is None:
                    p_origin = gi.follow_origin(c, dies)
                    po_a = p_origin['attrs'] if p_origin else {}
                    pname = pname or gi.parse_name(po_a.get('DW_AT_name'))
                    pt_off = pt_off if pt_off is not None else gi.parse_ref(po_a.get('DW_AT_type'))
                pname = pname or ('a%d' % (len(params) + 1))
                ptype = gi.resolve_type(pt_off, dies, canonical, cache)
                params.append((gi.sanitize_ident(pname), ptype))
            elif pd['tag'] == 'DW_TAG_unspecified_parameters':
                variadic = True
        out.append({
            'name': name, 'va': low, 'external': 'DW_AT_external' in decl_a,
            'prototyped': 'DW_AT_prototyped' in decl_a, 'ret': ret, 'params': params,
            'variadic': variadic, 'cu': cu_name,
        })


# --------------------------------------------------------------------------
# Origin lookup built on gi.compute_type_origin(), with the anonymous-entity
# fallback (types never grouped by compute_canonical, so absent from its
# result) resolved by checking that single DIE's own CU directly.
# --------------------------------------------------------------------------

def make_origin_lookup(dies, origin_map):
    def cu_is_game(cu_off):
        cu_die = dies.get(cu_off)
        name = gi.parse_name(cu_die['attrs'].get('DW_AT_name')) if cu_die else None
        return bool(name) and name.lower().replace('/', '\\').startswith(gi.GAME_PREFIX)

    def origin_of(node):
        off = node.get('offset')
        val = origin_map.get(off)
        if val is None:
            d = dies.get(off)
            val = cu_is_game(d.get('cu')) if d else False
        return 'game' if val else 'library'
    return origin_of


def fixup_anonymous_origin(order, nodes, origin_of_key):
    """An anonymous struct/union/enum DIE (node['anon'] is True) is never
    grouped by compute_canonical (it has no name to group by), so
    make_origin_lookup()'s fallback classified it from its OWN occurrence's
    CU -- which is unreliable: an anonymous type nested inside a shared
    library struct (e.g. Allegro's MIDI) is textually re-declared, nested
    and all, inside every CU that includes the header, INCLUDING game CUs,
    so which CU's copy happens to be the one reachable from this run's
    game-scope type graph is arbitrary and says nothing about origin
    (observed: MIDI's anonymous nested struct came out attributed to
    custom.c, a game CU, purely because custom.c's redeclaration of MIDI
    is the one this run's graph walk reached first).

    The reliable signal for an anonymous type is its ENCLOSING named
    type's origin, already computed correctly above -- inherit it via the
    reverse of entity_deps() (who has this anonymous entity as a byval/
    named-use dependency). Falls back to the original per-DIE-CU guess
    only for an anonymous entity with no reachable named parent at all."""
    from collections import defaultdict
    reverse = defaultdict(set)
    for key in order:
        for dep in gi.entity_deps(key, nodes[key]):
            if dep in nodes:
                reverse[dep].add(key)
    for key in order:
        node = nodes[key]
        if not node.get('anon'):
            continue
        parents = reverse.get(key, set())
        parent_origins = {origin_of_key[p] for p in parents if p in origin_of_key}
        if parent_origins:
            origin_of_key[key] = 'library' if 'library' in parent_origins else 'game'


def propagate_game_origin(order, nodes, origin_of_key):
    """A library type must never depend on a game type (allegro_types.h
    must not #include game_types.h). If one does -- an edge case, not
    observed in this scope -- promote it (and transitively anything that
    depends on it) to 'game' rather than emit an unresolvable include
    cycle, and report it so GENERATED.md can name the anomaly."""
    promoted = []
    changed = True
    while changed:
        changed = False
        for key in order:
            if origin_of_key[key] != 'library':
                continue
            node = nodes[key]
            for dep in gi.entity_deps(key, node):
                if dep in origin_of_key and origin_of_key[dep] == 'game':
                    origin_of_key[key] = 'game'
                    promoted.append({'key': str(key), 'name': node.get('name'), 'forced_by_dep': str(dep)})
                    changed = True
                    break
    return promoted


# --------------------------------------------------------------------------
# Struct/typedef/enum body emission, split by origin, reusing
# gi.emit_struct_body / gi.decl for the bodies themselves.
# --------------------------------------------------------------------------

def emit_types_section(order, nodes, origin_of_key, want_origin, opaque_reasons, name_filter=None):
    """name_filter, if given, additionally restricts every key emitted (forward
    decl, enum, struct body, typedef) to nodes whose name it accepts -- used
    to carve the CRT-reserved subset (it_orig_size_t, it_orig_FILE, ...) out
    of the 'library' origin so it can be emitted separately from Allegro's
    own types (see CRT_RESERVED_NAMES / the ICYTOWER_UPSTREAM_ALLEGRO split
    in main())."""
    keep = (lambda n: True) if name_filter is None else name_filter
    su_keys = [k for k in order if k[0] == 'su' and origin_of_key[k] == want_origin and keep(nodes[k]['name'])]
    # forward decls: every su entity of this origin, name-sorted (matches
    # gen_interop.py's it_types.h convention -- decl order is irrelevant to
    # a forward declaration, so a stable independent sort reads best)
    fwd_keys = sorted(su_keys, key=lambda k: nodes[k]['name'])
    en_keys = [k for k in order if k[0] == 'en' and origin_of_key[k] == want_origin and keep(nodes[k]['name'])]

    lines = []
    lines.append('/* forward declarations */')
    for k in fwd_keys:
        n = nodes[k]
        lines.append('%s %s;' % (n['kind'], n['name']))
    lines.append('')
    lines.append('/* enumerations */')
    if not en_keys:
        lines.append('/* none in this scope */')
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
    emitted = set()
    body_keys = []  # (key) in emission order, for the check-file generator to reuse
    for key in order:
        if origin_of_key[key] != want_origin:
            continue
        n = nodes[key]
        if not keep(n['name']):
            continue
        if key[0] == 'su':
            if n['name'] in emitted:
                continue
            emitted.add(n['name'])
            lines.extend(gi.emit_struct_body(n, opaque_reasons))
            body_keys.append(key)
        elif key[0] == 'td':
            lines.append('typedef %s;' % gi.decl(n['inner'], n['name']))
            body_keys.append(key)
    lines.append('')
    return '\n'.join(lines), body_keys


# CRT-reserved names (gi.RESERVED_TYPE_RENAMES's it_orig_-prefixed values):
# these ride along in allegro_types.h's 'library' origin bucket only because
# gen_interop.compute_type_origin's rule is "declared outside the game CUs",
# which is also true of any CRT header the game transitively pulls in -- they
# are NOT Allegro's own types, so upstream's real <allegro.h> never defines
# them under these renamed names (it doesn't need to: it just uses plain
# FILE/size_t/time_t itself). Skipping them under ICYTOWER_UPSTREAM_ALLEGRO
# the same way the Allegro-origin types are skipped would leave
# game_types.h's `it_orig_size_t iDocSize;` (etc.) referencing an undefined
# type, so they are emitted unconditionally instead -- see main()'s allegro_
# types.h writer.
CRT_RESERVED_NAMES = frozenset(gi.RESERVED_TYPE_RENAMES.values())


def banner(generator_name, args, extra=''):
    lines = [
        '/* GENERATED FILE -- DO NOT EDIT.',
        ' * Produced by carrier/gen/%s (which reuses carrier/gen/gen_interop.py\'s' % generator_name,
        ' * DWARF parser and type IR -- see that file for the parsing itself) from:',
        ' *   %s' % args.dwarf,
        ' *   %s' % args.functions,
        ' * scope=%s. Re-run carrier/gen/%s to regenerate; do not hand-edit.' % (args.scope, generator_name),
    ]
    if extra:
        lines.append(' *')
        for l in extra.splitlines():
            lines.append(' * %s' % l if l else ' *')
    lines.append(' */')
    return '\n'.join(lines) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dwarf', required=True)
    ap.add_argument('--functions', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--scope', choices=['game', 'all'], default='game')
    ap.add_argument('--names', default=None,
                     help='names.json (win32_pilot.md mechanism A) -- see '
                          'gen_interop.py --names for the full description. '
                          'Default: <repo>/src/icytower/names.json if present.')
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    print('parsing DWARF...', file=sys.stderr)
    dies = gi.parse_dwarf(args.dwarf)
    print('DIEs: %d' % len(dies), file=sys.stderr)
    gi.ANON_TYPEDEF_NAMES = gi.compute_anon_typedef_names(dies)
    canonical, type_conflicts = gi.compute_canonical(dies, gi.ANON_TYPEDEF_NAMES)
    cache = {}

    names_path = args.names
    if names_path is None:
        default_names = os.path.normpath(os.path.join(
            os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'icytower', 'names.json'))
        if os.path.exists(default_names):
            names_path = default_names
    gi.NAMES_TABLE = gi.load_names_table(names_path)
    if gi.NAMES_TABLE:
        print('names table: %d hand-curated name(s) loaded from %s' %
              (len(gi.NAMES_TABLE), names_path), file=sys.stderr)

    scope_cus = gi.in_scope_cus(dies, args.scope)
    print('in-scope CUs: %d' % len(scope_cus), file=sys.stderr)

    functions = []
    globals_ = []
    for cu_off in scope_cus:
        collect_functions_named(dies, cu_off, canonical, cache, functions)
        gi.collect_globals(dies, cu_off, canonical, cache, globals_)

    coff_va_map = gi.load_coff_va_map(args.functions)
    for fn in functions:
        conv, evidence = gi.calling_convention_for(fn['va'], coff_va_map)
        fn['calling_convention'] = conv
        fn['convention_evidence'] = evidence

    func_collisions = gi.resolve_collisions(functions, lambda f: gi.sanitize_ident(f['name']), lambda f: f['cu'])
    for g in globals_:
        base = gi.sanitize_ident(g['name']) + ('__%s' % gi.sanitize_ident(g['local_static_of']) if g['local_static_of'] else '')
        g['_basename'] = base
    global_collisions = gi.resolve_collisions(globals_, lambda g: g['_basename'], lambda g: g['cu'])

    functions.sort(key=lambda f: (f['cu'], f['emit_name']))
    globals_.sort(key=lambda g: (g['cu'], g['emit_name']))

    # ---- reachable named types ----
    seen_ids = set()
    nodes = {}
    for fn in functions:
        gi.collect_reachable(fn['ret'], seen_ids, nodes)
        for (_pname, ptype) in fn['params']:
            gi.collect_reachable(ptype, seen_ids, nodes)
    for g in globals_:
        gi.collect_reachable(g['type'], seen_ids, nodes)

    order, cycle_anomalies = gi.topo_order(nodes)

    purity_renames = apply_purity_renames(order, nodes, functions, globals_)

    origin_map = gi.compute_type_origin(dies, canonical, gi.ANON_TYPEDEF_NAMES, gi.GAME_PREFIX)
    origin_lookup = make_origin_lookup(dies, origin_map)
    origin_of_key = {key: origin_lookup(nodes[key]) for key in nodes}
    fixup_anonymous_origin(order, nodes, origin_of_key)
    promoted = propagate_game_origin(order, nodes, origin_of_key)

    opaque_reasons = []
    lib_body, lib_keys = emit_types_section(order, nodes, origin_of_key, 'library', opaque_reasons)
    game_body, game_keys = emit_types_section(order, nodes, origin_of_key, 'game', opaque_reasons)
    # CRT-reserved subset of 'library' (it_orig_size_t, it_orig_FILE, ...):
    # needed in EVERY world, including ICYTOWER_UPSTREAM_ALLEGRO, since
    # upstream <allegro.h> never defines these renamed names -- see
    # CRT_RESERVED_NAMES's docstring above. Thrown-away opaque list: none of
    # this subset is ever opaque (verified against GENERATED.md's 4 opaque
    # entries), so there is nothing for a second call to double-count.
    crt_body, crt_keys = emit_types_section(
        order, nodes, origin_of_key, 'library', [],
        name_filter=lambda name: name in CRT_RESERVED_NAMES)

    # ---- allegro_types.h ----
    out_allegro = os.path.join(args.out, 'allegro_types.h')
    allegro_extra = (
        'These are Allegro\'s (and, where the game shares a CRT/library\n'
        'header transitively, that library\'s own) PUBLIC types -- BITMAP,\n'
        'DATAFILE, SAMPLE, FONT, PACKFILE, RGB, PALETTE, JOYSTICK_INFO, fixed\n'
        'and friends -- recovered here only because the game CUs use them by\n'
        'value or by pointer and this port must still compile without any\n'
        'carrier header. Once the port takes a real Allegro dependency, THIS\n'
        'FILE goes away and these names come from Allegro\'s own headers\n'
        'instead; nothing else in src/icytower/ should assume otherwise.\n\n'
        'A type here is "not game_types.h" purely because it also gets\n'
        'redeclared, per DWARF, inside at least one compile unit outside\n'
        'F:\\projects\\icytower\\trunk\\source\\ -- i.e. some non-game object\n'
        'in the original binary (Allegro, libpng, zlib, pthreads-win32,\n'
        'DirectX headers, the CRT) also defines this exact type, so it is not\n'
        'the game\'s own. See carrier/gen/gen_src_headers.py\'s\n'
        'compute_type_origin() docstring in gen_interop.py for the exact rule.\n\n'
        'Skipped when %s is defined: the generated bindings header\n'
        '(carrier/gen/pf_bindings_src.h, force-included only when src/ is\n'
        'compiled INTO the carrier) already supplied every one of these names\n'
        'with the same layout, via its own copy of it_types.h -- see\n'
        'src/README.md.\n\n'
        'Skipped, in favour of the REAL upstream <allegro.h>, when %s is\n'
        'defined: this is the standalone-build LIBRARIES-coastline swap\n'
        '(carrier/gen/LIB_BINDINGS_NOTES.md "Standalone build swap") --\n'
        'no source file under src/icytower changes at all to select it, because every\n'
        'src/ file reaches this header only through game_types.h\'s own\n'
        '`#include "allegro_types.h"`, never directly; defining %s on the\n'
        'compiler command line (`-D%s`) is the entire swap. The CRT-reserved\n'
        'subset below (it_orig_size_t, it_orig_FILE, ...) is NOT Allegro\'s\n'
        'own and upstream <allegro.h> never defines these renamed names, so\n'
        'it is emitted in this branch too -- see CRT_RESERVED_NAMES in\n'
        'gen_src_headers.py.' %
        (BINDINGS_GUARD, UPSTREAM_GUARD, UPSTREAM_GUARD, UPSTREAM_GUARD)
    )
    with open(out_allegro, 'w', encoding='utf-8') as f:
        f.write(banner('gen_src_headers.py', args, allegro_extra))
        f.write('#ifndef ICYTOWER_ALLEGRO_TYPES_H\n')
        f.write('#define ICYTOWER_ALLEGRO_TYPES_H\n\n')
        f.write('#if defined(%s)\n\n' % UPSTREAM_GUARD)
        f.write('#include <allegro.h>  /* real upstream Allegro 4.4.3.1 -- BITMAP, DATAFILE, */\n')
        f.write('                      /* SAMPLE, FONT, PACKFILE, RGB, PALETTE, JOYSTICK_INFO, */\n')
        f.write('                      /* fixed and friends now come from here, not below */\n\n')
        f.write('/* CRT-reserved names upstream <allegro.h> does not define under these\n')
        f.write(' * renamed identifiers (it never needed to -- it just uses plain FILE/\n')
        f.write(' * size_t/time_t itself); game_types.h still references them by these\n')
        f.write(' * names, so they are emitted unconditionally here. */\n')
        f.write('#pragma pack(push, 1)\n\n')
        f.write(crt_body)
        f.write('\n#pragma pack(pop)\n\n')
        f.write('#elif !defined(%s)\n\n' % BINDINGS_GUARD)
        f.write('#pragma pack(push, 1)\n\n')
        f.write(lib_body)
        f.write('\n#pragma pack(pop)\n\n')
        f.write('#endif /* %s / !%s */\n\n' % (UPSTREAM_GUARD, BINDINGS_GUARD))
        f.write('#endif /* ICYTOWER_ALLEGRO_TYPES_H */\n')

    # ---- game_types.h ----
    out_types = os.path.join(args.out, 'game_types.h')
    types_extra = (
        'Covers every struct/union/enum/typedef the game CUs\n'
        '(F:\\projects\\icytower\\trunk\\source\\*.c) declare and that is reachable\n'
        'from a game-CU global or function -- Tplayer, Tmap, Tfloor, and every\n'
        'other Txxx/native game type, plus `fixed` and Allegro\'s own public\n'
        'types pulled in from allegro_types.h.\n\n'
        'These layouts MUST stay binary-compatible with the ORIGINAL process\n'
        'memory for as long as game state stays address-backed\n'
        '(win32_pilot.md SS7a: "state stays address-backed, code ownership\n'
        'migrates first") -- a `Tplayer *`/`Tmap *` this port\'s code touches\n'
        'may in fact be a pointer at the original game\'s address, wearing this\n'
        'struct\'s layout as a lens. Field order, size and padding here are\n'
        'therefore taken from DWARF byte-for-byte, not from the compiler\'s own\n'
        'alignment rules (`#pragma pack(push, 1)` plus explicit `_pad_N` filler\n'
        'members wherever DWARF\'s member offsets show a gap the compiler would\n'
        'not have left on its own), and generated fresh every run instead of\n'
        'hand-maintained. src/icytower/game_types_check.c is the generated,\n'
        'buildable proof that every sizeof/offsetof below still matches DWARF.\n'
        'Once state ownership migrates to this port (a later, separate step\n'
        'per SS7a), this header becomes free to diverge from the original\n'
        'layout, and this comment should go with it.\n\n'
        'When this file is compiled INTO the carrier (src/ built with the\n'
        'generated bindings force-included, see game_state.h and\n'
        'src/README.md), the carrier\'s own type provider has already defined\n'
        'these same names with the same layout, so the bodies below are\n'
        'skipped rather than redefined -- see the %s guard.' % BINDINGS_GUARD
    )
    with open(out_types, 'w', encoding='utf-8') as f:
        f.write(banner('gen_src_headers.py', args, types_extra))
        f.write('#ifndef ICYTOWER_GAME_TYPES_H\n')
        f.write('#define ICYTOWER_GAME_TYPES_H\n\n')
        f.write('#include "allegro_types.h"\n\n')
        f.write('#ifndef %s\n\n' % BINDINGS_GUARD)
        f.write('#pragma pack(push, 1)\n\n')
        f.write(game_body)
        f.write('\n#pragma pack(pop)\n\n')
        f.write('#endif /* !%s */\n\n' % BINDINGS_GUARD)
        f.write('#endif /* ICYTOWER_GAME_TYPES_H */\n')

    # ---- game_types_check.c ----
    out_check = os.path.join(args.out, 'game_types_check.c')
    check_lines = []
    check_lines.append(banner('gen_src_headers.py', args,
                               'Standalone host program: verifies sizeof/offsetof of every\n'
                               'game_types.h/allegro_types.h struct/union against the DWARF-reported\n'
                               'layout. Touches no game memory -- the numbers below are struct\n'
                               'sizes and member byte offsets, never addresses (scripts/\n'
                               'check_native_layer.py allows plain integers; it only bans literals\n'
                               'that resolve into the guest image/heap/stack ranges).'))
    check_lines.append('#include "game_types.h"')
    check_lines.append('#include <stddef.h>')
    check_lines.append('#include <stdio.h>')
    check_lines.append('')
    check_lines.append('int main(void) {')
    check_lines.append('    int failures = 0;')
    for key in lib_keys + game_keys:
        if key[0] != 'su':
            continue
        n = nodes[key]
        if not n.get('byte_size'):
            continue
        check_lines.append(
            '    if (sizeof(%s %s) != %d) { failures++; printf("FAIL sizeof(%s %s) = %%u expected %d\\n", (unsigned)sizeof(%s %s)); }'
            ' else { printf("PASS sizeof(%s %s)\\n"); }' % (
                n['kind'], n['name'], n['byte_size'], n['kind'], n['name'], n['byte_size'],
                n['kind'], n['name'], n['kind'], n['name']))
        for (mname, _mtype, moff) in n['members']:
            check_lines.append(
                '    if (offsetof(%s %s, %s) != %d) { failures++; printf("FAIL offsetof(%s %s, %s) = %%u expected %d\\n", (unsigned)offsetof(%s %s, %s)); }'
                ' else { printf("PASS offsetof(%s %s, %s)\\n"); }' % (
                    n['kind'], n['name'], mname, moff, n['kind'], n['name'], mname, moff,
                    n['kind'], n['name'], mname, n['kind'], n['name'], mname))
    check_lines.append('    printf(failures == 0 ? "ALL PASS\\n" : "%d FAILURES\\n", failures);')
    check_lines.append('    return failures ? 1 : 0;')
    check_lines.append('}')
    with open(out_check, 'w', encoding='utf-8') as f:
        f.write('\n'.join(check_lines) + '\n')

    # ---- game_state.h + state.c ----
    out_state_h = os.path.join(args.out, 'game_state.h')
    out_state_c = os.path.join(args.out, 'state.c')
    state_extra = (
        'Ordinary extern declarations of every game-CU global (win32_pilot.md\n'
        'SS7a). No address appears here or anywhere else in src/: each name is\n'
        'declared exactly as an application would declare a global it does not\n'
        'own the storage for. Two things give it real storage and a real\n'
        'address, depending on which world this file is compiled into:\n\n'
        '  - INTO THE CARRIER: the generated bindings header\n'
        '    (carrier/gen/pf_bindings_src.h, or its PF_MEM-wrapped harness\n'
        '    twin) is force-included (/FI) ahead of every other token in the\n'
        '    translation unit. It #defines each of these names to\n'
        '    (*(T*)original_address), so an extern re-declaration of an\n'
        '    already-macro-expanded name would not parse. The %s\n'
        '    guard, defined by that same generated header, is what lets this\n'
        '    file detect that and skip its own declarations.\n'
        '  - STANDALONE: no bindings header is force-included, so the guard\n'
        '    is undefined, the extern declarations below are the only\n'
        '    declaration of these names, and state.c supplies their storage\n'
        '    (win32_pilot.md SS7a: "a state.c defines the globals and the\n'
        '    bindings header is absent").\n\n'
        'A global whose original C source declared it `static` (file scope or\n'
        'function-local) is marked as such in its comment -- that is a note\n'
        'about the ORIGINAL program\'s linkage, kept for provenance; every name\n'
        'below is still declared `extern` here uniformly and given storage in\n'
        'state.c, exactly like carrier/gen/it_globals.h treats the same\n'
        'distinction as a comment rather than a different declaration.\n'
        'A name is disambiguated with a __<file> suffix only where two\n'
        'different game CUs actually collide on it (see GENERATED.md).' % BINDINGS_GUARD
    )
    by_cu = {}
    cu_order = []
    for g in globals_:
        if g['cu'] not in by_cu:
            by_cu[g['cu']] = []
            cu_order.append(g['cu'])
        by_cu[g['cu']].append(g)
    cu_order.sort()

    with open(out_state_h, 'w', encoding='utf-8') as f:
        f.write(banner('gen_src_headers.py', args, state_extra))
        f.write('#ifndef ICYTOWER_GAME_STATE_H\n')
        f.write('#define ICYTOWER_GAME_STATE_H\n\n')
        f.write('#include "game_types.h"\n\n')
        f.write('#ifndef %s\n\n' % BINDINGS_GUARD)
        if not globals_:
            f.write('/* no game-CU globals in this scope */\n\n')
        for cu in cu_order:
            f.write('/* ---- %s ---- */\n' % cu)
            for g in by_cu[cu]:
                note = ''
                if not g['external']:
                    if g['local_static_of']:
                        note = '  /* static local in %s(), %s */' % (g['local_static_of'], cu)
                    else:
                        note = '  /* static in %s */' % cu
                f.write('extern %s;%s\n' % (gi.decl(g['type'], g['emit_name']), note))
            f.write('\n')
        f.write('#endif /* !%s */\n\n' % BINDINGS_GUARD)
        f.write('#endif /* ICYTOWER_GAME_STATE_H */\n')

    with open(out_state_c, 'w', encoding='utf-8') as f:
        f.write(banner('gen_src_headers.py', args,
                        'Standalone storage for every extern declared in game_state.h. Only\n'
                        'used when src/ is built OUTSIDE the carrier (win32_pilot.md SS7a:\n'
                        '"standalone, a state.c defines the globals and the bindings header\n'
                        'is absent"). When src/ is compiled INTO the carrier, the generated\n'
                        'bindings header supplies these names as address-backed macros\n'
                        'instead and this file is not part of that build at all.\n\n'
                        'Every global is zero-initialized (`= {0}`), matching how the OS\n'
                        'loader zero-fills the original .bss at process start; nothing here\n'
                        'claims state ownership has moved, only that a standalone build has\n'
                        'somewhere to put these bytes (src/README.md "Offline verification").'))
        f.write('#include "game_state.h"\n\n')
        for cu in cu_order:
            f.write('/* ---- %s ---- */\n' % cu)
            for g in by_cu[cu]:
                f.write('%s = {0};\n' % gi.decl(g['type'], g['emit_name']))
            f.write('\n')

    # ---- game_funcs.h ----
    out_funcs = os.path.join(args.out, 'game_funcs.h')
    funcs_extra = (
        'Prototypes of every game-CU function, original names and DWARF\n'
        'parameter names. This is the port\'s OWN header -- unlike\n'
        'carrier/gen/it_funcs.h, no address, PFN_ typedef or IT_F_ macro ever\n'
        'appears here; a function this port has recovered defines its body in\n'
        'its own src/icytower/<name>.c, and everything else here is just a\n'
        'plain forward declaration so those files can call each other and be\n'
        'called, same as any ordinary C program.\n\n'
        'Calling convention is explicit only where DWARF/COFF evidence says it\n'
        'is not the MSVC default __cdecl (see carrier/gen/INTEROP_NOTES.md\n'
        '"Calling convention detection"); today that is WinMain alone\n'
        '(__stdcall, verified from its COFF decoration _WinMain@16).\n\n'
        'Each prototype is wrapped in `#ifndef <name>`/`#endif`: when this\n'
        'file is compiled INTO the carrier/harness world (win32_pilot.md\n'
        'SS7a, ICYTOWER_BINDINGS_ACTIVE), carrier/gen/pf_bindings_src.h or\n'
        '.../pf_bindings_harness.h is force-included first and #defines the\n'
        'plain name of every game function NOT excluded (i.e. not yet\n'
        'promoted into src/) to an address-cast expression -- declaring\n'
        'such a name again here would macro-expand into a syntax error, not\n'
        'a harmless redeclaration. A name IS still declared here whenever no\n'
        'such macro exists: every promoted (--exclude-d) function, and every\n'
        'name in the plain standalone world where no bindings header is\n'
        'force-included at all.'
    )
    with open(out_funcs, 'w', encoding='utf-8') as f:
        f.write(banner('gen_src_headers.py', args, funcs_extra))
        f.write('#ifndef ICYTOWER_GAME_FUNCS_H\n')
        f.write('#define ICYTOWER_GAME_FUNCS_H\n\n')
        f.write('#include "game_types.h"\n\n')
        by_cu_fn = {}
        cu_order_fn = []
        for fn in functions:
            if fn['cu'] not in by_cu_fn:
                by_cu_fn[fn['cu']] = []
                cu_order_fn.append(fn['cu'])
            by_cu_fn[fn['cu']].append(fn)
        cu_order_fn.sort()
        for cu in cu_order_fn:
            f.write('/* ---- %s ---- */\n' % cu)
            for fn in by_cu_fn[cu]:
                conv = '__stdcall ' if fn['calling_convention'] == 'stdcall' else '__cdecl '
                params = [gi.decl(pt, PROTOTYPE_PARAM_RENAMES.get(pn, pn)) for (pn, pt) in fn['params']]
                if fn['variadic']:
                    params.append('...')
                paramstr = ', '.join(params) if params else ('void' if fn['prototyped'] else '')
                ret_str = gi.decl(fn['ret'], '')
                note = '' if fn['external'] else '  /* static in %s */' % cu
                proto = '%s %s%s(%s);%s' % (ret_str, conv, fn['emit_name'], paramstr, note)
                f.write('#ifndef %s\n%s\n#endif\n' % (fn['emit_name'], proto))
            f.write('\n')
        f.write('#endif /* ICYTOWER_GAME_FUNCS_H */\n')

    # ---- GENERATED.md ----
    out_report = os.path.join(args.out, 'GENERATED.md')
    lib_su = sum(1 for k in lib_keys if k[0] == 'su')
    game_su = sum(1 for k in game_keys if k[0] == 'su')
    lib_td = sum(1 for k in lib_keys if k[0] == 'td')
    game_td = sum(1 for k in game_keys if k[0] == 'td')
    lines = []
    lines.append('# Generated src/icytower headers\n')
    lines.append('Produced by `carrier/gen/gen_src_headers.py` (reusing '
                  '`carrier/gen/gen_interop.py`\'s DWARF parser) from `%s` + `%s`, scope=`%s`.\n' % (
                      args.dwarf, args.functions, args.scope))
    lines.append('## Counts\n')
    lines.append('| item | count |\n|---|---:|\n')
    lines.append('| game-CU globals | %d |\n' % len(globals_))
    lines.append('| game-CU functions | %d |\n' % len(functions))
    lines.append('| game structs/unions (game_types.h) | %d |\n' % game_su)
    lines.append('| library structs/unions (allegro_types.h) | %d |\n' % lib_su)
    lines.append('| game typedefs (game_types.h) | %d |\n' % game_td)
    lines.append('| library typedefs (allegro_types.h) | %d |\n' % lib_td)
    lines.append('| opaque types | %d |\n' % len(opaque_reasons))
    lines.append('\nCU scope: %d compile units in scope out of %d total in the DWARF.\n' % (
        len(scope_cus), sum(1 for d in dies.values() if d['tag'] == 'DW_TAG_compile_unit')))
    lines.append('\n## Opaque / unrepresentable types (%d)\n' % len(opaque_reasons))
    if opaque_reasons:
        for o in opaque_reasons:
            extra = (' member `%s`' % o['member']) if 'member' in o else ''
            lines.append('- `%s%s`%s: %s\n' % (o.get('kind', ''), (' ' + o.get('name', '')) if o.get('name') else '',
                                                 extra, o.get('reason')))
    else:
        lines.append('None in this scope.\n')
    lines.append('\n## Name collisions (disambiguated with `__<cu-basename>`)\n')
    lines.append('Functions: %d collisions. Globals: %d collisions.\n' % (
        len(func_collisions), len(global_collisions)))
    for c in func_collisions:
        lines.append('- function `%s` defined in %d CUs: %s\n' % (c['name'], c['count'], ', '.join(c['cus'])))
    for c in global_collisions:
        lines.append('- global `%s` defined in %d CUs: %s\n' % (c['name'], c['count'], ', '.join(c['cus'])))
    lines.append('\n## Purity-gate identifier renames (%d)\n' % len(purity_renames))
    lines.append('A DWARF-original name colliding with scripts/check_native_layer.py\'s '
                  'banned-prefix list (`PF_`, `pf_`, `IT_G_`, `IT_F_`, `PFN_`, `lifted_`) is '
                  'renamed with an `icy_orig_` prefix; nothing else about it changes. Seen so '
                  'far: Allegro\'s own PACKFILE_VTABLE member-naming convention '
                  '("packfile function") coincidentally uses the same `pf_` prefix.\n\n')
    if purity_renames:
        for (orig, safe) in purity_renames:
            lines.append('- `%s` -> `%s`\n' % (orig, safe))
    else:
        lines.append('None in this scope.\n')
    lines.append('\n## Type origin promotions (%d)\n' % len(promoted))
    lines.append('A library type is normally never allowed to depend on a game type '
                  '(allegro_types.h must stand alone); any entity this run had to promote '
                  'to game_types.h to keep that true is listed here.\n\n')
    if promoted:
        for p in promoted:
            lines.append('- `%s` (%s) forced into game_types.h by its dependency on `%s`\n' % (
                p['name'], p['key'], p['forced_by_dep']))
    else:
        lines.append('None -- no library-origin type in this scope depended on a game-origin type.\n')
    lines.append('\n## Struct/union/typedef name conflicts across CUs (%d, whole-DWARF, from gen_interop.compute_canonical)\n' %
                  len(type_conflicts))
    relevant_names = {nodes[k]['name'] for k in (lib_keys + game_keys) if k[0] in ('su', 'td')}
    relevant_conflicts = [c for c in type_conflicts if c['name'] in relevant_names]
    if relevant_conflicts:
        for c in relevant_conflicts:
            lines.append('- `%s %s`: see carrier/gen/INTEROP_NOTES.md (kept the canonical definition)\n' %
                          (c['kind'], c['name']))
    else:
        lines.append('None of the whole-DWARF conflicts gen_interop.py recorded involve a type reachable in this scope.\n')
    lines.append('\n## Topological-order anomalies (%d)\n' % len(cycle_anomalies))
    if cycle_anomalies:
        for a in cycle_anomalies:
            lines.append('- %s\n' % a)
    else:
        lines.append('None -- no by-value/typedef dependency cycles found.\n')
    lines.append('\n## Verification\n')
    lines.append('```\npython scripts/check_native_layer.py\n'
                  'cl /nologo /c /W3 /TC src\\icytower\\update_frame.c src\\icytower\\is_solid.c src\\icytower\\state.c\n'
                  'cl /nologo /c /W3 /TC /Icarrier\\gen /FIcarrier\\gen\\pf_bindings_src.h '
                  'src\\icytower\\update_frame.c src\\icytower\\is_solid.c\n'
                  'cl /nologo /W3 /TC src\\icytower\\game_types_check.c /Fe:src\\icytower\\game_types_check.exe\n'
                  'src\\icytower\\game_types_check.exe\n```\n')
    with open(out_report, 'w', encoding='utf-8') as f:
        f.write(''.join(lines))

    print('done: %d globals, %d functions, %d game structs/typedefs, %d library structs/typedefs, '
          '%d opaque, %d promotions' % (
              len(globals_), len(functions), game_su + game_td, lib_su + lib_td,
              len(opaque_reasons), len(promoted)), file=sys.stderr)


if __name__ == '__main__':
    main()
