#!/usr/bin/env python3
"""gen_bindings.py -- generate carrier/gen/pf_bindings.h and pf_bindings_types.h.

Purpose (win32_pilot.md SS7a): let address-free clean C in src/ -- which
declares game globals as ordinary externs (`extern int reward_scale;`,
`extern Tplayer *ply[1000];`) and calls other game functions by their plain
names -- compile INTO the carrier and operate on the ORIGINAL game memory at
the ORIGINAL addresses, with zero address literals or carrier types in src/
itself.

This script does NOT re-parse DWARF. It is a second, small generator that
sits on top of the *already-generated and already-MSVC-verified* output of
gen_interop.py (`interop_index.json`, `it_globals.h`, `it_funcs.h`, see
INTEROP_NOTES.md): interop_index.json is the enumeration of game-scope
globals/functions (name, va, type/prototype, cu); it_globals.h / it_funcs.h
already contain, per name, a correct C cast expression back to the original
address (`(*(T*)VA)` for a global, `((PFN_name)VA)` for a function, PFN_name
declared alongside). gen_bindings.py reuses those expressions verbatim under
the PLAIN name instead of the IT_G_/IT_F_ prefixed one, which is exactly the
address-binding macro clean C needs to compile in place. Re-deriving the
cast expressions independently (re-parsing DWARF type strings) would risk
silently diverging from the header pair that INTEROP_NOTES.md says was
verified against real MSVC; reuse avoids that class of bug entirely.

Outputs:
  pf_bindings.h        GENERATED, DO-NOT-EDIT. One #define per game-scope
                        global and (non-excluded) function, binding the
                        plain name to its original address. Forced-included
                        (`/FI`) only when src/ is compiled INTO the carrier.
  pf_bindings_types.h  GENERATED, DO-NOT-EDIT. Carrier-side type provider:
                        just `#include "it_types.h"` plus the DO-NOT-EDIT
                        banner. src/ gets its own, hand-owned copy of the
                        types later (SS7a); this header is only for code
                        that compiles *into* the carrier today.

Exclusion (`--exclude name[,name...]`): a function about to be compiled
natively into the carrier from src/ must keep its own name -- redirecting
`foo` to `((PFN_foo)0x...)` while src/foo.c also *defines* `foo` is a
duplicate-definition / self-redirection bug, not a binding. Names passed to
--exclude are simply omitted from pf_bindings.h; the carrier build passes
the names of the functions currently promoted to NATIVE (win32_pilot.md
SS3's binding table is the runtime side of the same idea, this is the
compile-time side).

Guest-owned CRT imports (`GUEST_CRT_IMPORTS`, divergence 008): a promoted
function in src/ that calls a C-library function the GAME imports -- today
just `rand()`, in map.c's add_floor() -- must reach the *guest's* import,
not the carrier's own statically-linked CRT. They are two different
functions with two different states: the guest's msvcrt `rand` is reached
through the IAT slot the carrier owns and (in --det) replaces with det.cpp's
pinned LCG, which `srand()` seeds and the snapshot captures; the carrier's
own `rand` is a separate, never-seeded UCRT generator. Linking src/ code to
the latter silently drew the whole tower layout from the wrong stream (see
notes/living_record.md divergence 008). So for each name below this header
emits a call-through-the-IAT-slot macro, exactly what the original machine
code's `call _rand -> jmp *[slot]` thunk does. The slot VA comes from
imports.json (the same evidence file gen_imports.py reads), never from a
hand-typed address.

CRT/Windows identifier collisions: INTEROP_NOTES.md's generator renames
type names that collide with real CRT/UCRT types it must coexist with
(`FILE`->`it_orig_FILE` etc, see CRT_RENAME there) because the *type name*
is only used internally and can safely be renamed. A plain-name #define
cannot be renamed the same way without breaking the reason it exists (src/
must be able to write the literal identifier) -- so instead, any game
global/function name colliding with a reserved CRT/Windows identifier is
SKIPPED (no macro emitted) and reported, exactly as INTEROP_NOTES.md reports
struct/typedef collisions instead of silently guessing. As of this run the
game-scope name set has zero such collisions (see BINDINGS_NOTES.md).
"""

import argparse
import datetime
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

GLOBAL_DEFINE_RE = re.compile(
    r'^#define IT_G_([A-Za-z_][A-Za-z0-9_]*) (\(\*\(.*\)0x[0-9a-fA-F]+\))$')
FUNC_DEFINE_RE = re.compile(
    r'^#define IT_F_([A-Za-z_][A-Za-z0-9_]*) '
    r'(\(\(PFN_[A-Za-z_][A-Za-z0-9_]*\)0x[0-9a-fA-F]+\))$')

# Identifiers that must not be shadowed by an object-like macro because a
# real CRT/UCRT header or windows.h either defines them as a macro itself
# (redefinition warning/error, or silent breakage of *their* meaning in any
# TU that includes both) or ships them as a well-known function whose
# system-header declaration would be textually mangled by our macro. This
# list is deliberately conservative (better to skip a name that is actually
# safe than to emit one that breaks a system header two files away).
RESERVED_CRT_WINDOWS_IDENTS = {
    # stdio.h
    'fopen', 'fclose', 'fread', 'fwrite', 'printf', 'sprintf', 'fprintf',
    'fscanf', 'scanf', 'sscanf', 'fputs', 'fgets', 'fputc', 'fgetc', 'getc',
    'putc', 'getchar', 'putchar', 'remove', 'rename', 'tmpfile', 'tmpnam',
    'perror', 'ferror', 'feof', 'fflush', 'fseek', 'ftell', 'rewind',
    'setvbuf', 'vprintf', 'vfprintf', 'vsprintf', 'stdin', 'stdout', 'stderr',
    # stdlib.h
    'malloc', 'calloc', 'realloc', 'free', 'abort', 'exit', '_exit',
    'atexit', 'system', 'getenv', 'rand', 'srand', 'qsort', 'bsearch',
    'abs', 'labs', 'div', 'ldiv', 'atoi', 'atol', 'atof', 'strtol',
    'strtoul', 'strtod', 'mblen', 'mbtowc', 'wctomb', 'errno',
    # string.h
    'strcpy', 'strncpy', 'strcat', 'strncat', 'strcmp', 'strncmp', 'strchr',
    'strrchr', 'strstr', 'strlen', 'strtok', 'strerror', 'strpbrk',
    'strspn', 'strcspn', 'memcpy', 'memmove', 'memset', 'memcmp', 'memchr',
    'index', 'rindex',
    # math.h
    'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'atan2', 'sinh', 'cosh',
    'tanh', 'exp', 'log', 'log10', 'pow', 'sqrt', 'ceil', 'floor', 'fabs',
    'fmod', 'frexp', 'ldexp', 'modf',
    # time.h
    'time', 'clock', 'difftime', 'mktime', 'asctime', 'ctime', 'gmtime',
    'localtime', 'strftime',
    # ctype.h
    'isalpha', 'isdigit', 'isalnum', 'isspace', 'isupper', 'islower',
    'ispunct', 'iscntrl', 'isprint', 'isgraph', 'isxdigit', 'toupper',
    'tolower',
    # windows.h macros/typedefs that are object-like and commonly break
    # when shadowed by an unrelated object macro
    'small', 'far', 'near', 'pascal', 'cdecl', 'interface', 'IN', 'OUT',
    'OPTIONAL', 'CONST', 'VOID', 'TRUE', 'FALSE', 'NULL', 'ERROR', 'DELETE',
    'IGNORE', 'min', 'max', 'byte', 'WORD', 'DWORD', 'LONG', 'SHORT',
    'BOOL', 'BYTE', 'HANDLE', 'HWND', 'LPVOID',
    # C keywords (never valid identifiers, listed defensively)
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
    'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
    'inline', 'int', 'long', 'register', 'restrict', 'return', 'short',
    'signed', 'sizeof', 'static', 'struct', 'switch', 'typedef', 'union',
    'unsigned', 'void', 'volatile', 'while',
}

# ---------------------------------------------------------------------
# Guest-owned CRT imports (divergence 008) -- see this module's docstring.
#
# Hand-curated on purpose, and deliberately TINY: binding a CRT name in
# src/'s force-included header is exactly the "shadow a system function"
# hazard RESERVED_CRT_WINDOWS_IDENTS exists to avoid, so a name earns a
# place here only when a promoted src/ function genuinely must share the
# GUEST's copy of that function's state.
#
#   rand   map.c's add_floor() is the game's only live gameplay consumer of
#          libc rand() (notes/layout_determinism.md SS1). Its stream IS the
#          tower layout, seeded by new_game()'s srand(Treplay.random_seed);
#          in --det the carrier replaces the guest's msvcrt slot with
#          det.cpp's pinned LCG so a replay is reproducible and the RNG
#          state is snapshot-capturable. The carrier's own linked-in CRT
#          rand() is a different generator with a different, never-seeded
#          state -- MEASURED: with add_floor bound to src, the pinned state
#          stayed at the seed (16944) and rng_calls stopped at 4, while the
#          ORIGINAL form advanced it 10 times to 0xea58d532 over the same
#          30 initial floors.
#   srand  not used by any promoted function today, but listed for the same
#          reason and to keep the seed/draw pair from ever splitting across
#          two generators if one is promoted later.
#
# Each entry maps a plain C name to the msvcrt import whose IAT slot it must
# call through; the slot VA is looked up in imports.json, never typed here.
GUEST_CRT_IMPORTS = {
    'rand':  ('msvcrt.dll', 'rand',  'int',  '(void)'),
    'srand': ('msvcrt.dll', 'srand', 'void', '(unsigned)'),
}

# System headers that must be pulled in BEFORE the macros above are defined,
# so the macro rewrites CALLS in src/ and never the library's own
# declaration text. Same trick, same reason, as carrier/lift/harness/
# pf_harness_rand.h uses for the offline harness build.
GUEST_CRT_PRE_INCLUDES = ['<stdlib.h>']


def load_import_slots(path):
    """imports.json -> {(dll_lower, name): [iat_slot_va_int, ...]}.

    imports.json is a flat list of [dll, name, "0x...."] triples (the same
    file carrier/gen/gen_imports.py consumes). Anything else is a hard
    error rather than a guess.

    A name CAN legitimately appear more than once with different slots (this
    image imports msvcrt!_stat twice, MEASURED), so the duplicate is kept
    rather than rejected here; ambiguity is only fatal for a name
    GUEST_CRT_IMPORTS actually asks for, where picking one of two slots
    would be a guess.
    """
    raw = json.loads(Path(path).read_text(encoding='utf-8'))
    slots = {}
    for entry in raw:
        if not (isinstance(entry, list) and len(entry) == 3):
            raise ValueError('imports.json: unexpected entry %r' % (entry,))
        dll, name, va = entry
        slots.setdefault((dll.lower(), name), []).append(int(va, 16))
    return slots


def emit_guest_crt_imports(slots, mem_macro):
    """The `#define rand ...` block, as a list of output lines."""
    lines = []
    lines.append('/* ------------------------------------------------------------------ */')
    lines.append('/* CRT functions the GUEST imports: call through the guest IAT slot,   */')
    lines.append('/* not the carrier\'s own linked-in CRT (divergence 008 -- see          */')
    lines.append('/* gen_bindings.py\'s docstring and notes/living_record.md).            */')
    lines.append('/* ------------------------------------------------------------------ */')
    for inc in GUEST_CRT_PRE_INCLUDES:
        lines.append('#include %s  /* pulled in FIRST: the macros below must rewrite '
                      'calls in src/, never this header\'s own declarations */' % inc)
    lines.append('')
    emitted = []
    for name in sorted(GUEST_CRT_IMPORTS):
        dll, imp, ret, params = GUEST_CRT_IMPORTS[name]
        found = sorted(set(slots.get((dll.lower(), imp), [])))
        if not found:
            raise ValueError('imports.json has no %s!%s -- GUEST_CRT_IMPORTS is '
                              'out of date with the guest image' % (dll, imp))
        if len(found) > 1:
            raise ValueError('imports.json has %d distinct IAT slots for %s!%s '
                              '(%s) -- which one src/ should call through is a '
                              'guess, so GUEST_CRT_IMPORTS refuses to bind it'
                              % (len(found), dll, imp,
                                 ', '.join('0x%08x' % v for v in found)))
        va = found[0]
        addr = '0x%08x' % va
        if mem_macro:
            addr = '%s(%s)' % (mem_macro, addr)
        lines.append('/* %s  -> %s!%s IAT slot VA=0x%08x  (the original\'s own '
                      '`call _%s -> jmp *[slot]`) */' % (name, dll, imp, va, imp))
        lines.append('typedef %s (__cdecl *PFN_crt_%s)%s;' % (ret, name, params))
        lines.append('#define %s (*(PFN_crt_%s *)%s)' % (name, name, addr))
        emitted.append(name)
    lines.append('')
    return lines, emitted

# Hand-curated, NOT a blanket scan of every struct member name in scope:
# a global name unsafe to bind through a plain #define because it is ALSO
# used, somewhere in the ORIGINAL game source, as a struct member name that
# src/ code actually writes as a member access (`something.NAME`) --
# `#define jump_sound (*(...)0x4dd2b0)` would also rewrite the token
# `jump_sound` inside `custom.jump_sound` (Tcustom's own, unrelated member),
# which the C preprocessor cannot distinguish from a bare identifier.
# Deliberately NOT derived from scanning every struct in it_types.h for a
# member with this name: game_types.h has ~450 members across ~50 structs,
# and this project's naming style reuses short, common words often (data,
# stars, ctrl, count, ...) -- names that are members of SOME struct but
# never appear as a member ACCESS anywhere src/ code would write are not
# actually unsafe, and a blanket skip would silently break a name (`data`,
# `stars`) that carrier/gen/pf_asset_bindings.h or a promoted src/ file
# already binds and uses correctly as a bare identifier (found and reverted
# during play_jump_sound.c's promotion, PROMOTIONS.md batch 7 "Mechanism A/B"
# section -- an earlier version of this fix scanned every struct and turned
# out to be far too broad). Add a name here only after confirming BOTH (a)
# it collides with a real member name AND (b) some src/*.c file actually
# writes `<expr>.<name>` or `<expr>-><name>` for that member.
MEMBER_ACCESS_COLLISIONS = {
    'jump_sound',   # Tcustom.jump_sound[3] (VA 0x4fa738+1212), collides with
                     # the unrelated top-level global `jump_sound` @0x4dd2b0;
                     # play_jump_sound.c writes `custom.jump_sound[i]`.
}


HEX_ADDR_IN_BODY_RE = re.compile(r'0x[0-9a-fA-F]+')


def wrap_addresses(body, macro):
    """Wrap every address literal in a reused cast-expression body with
    macro(...), e.g. '(*(fixed *)0x4fac28)' -> '(*(fixed *)PF_MEM(0x4fac28))'.
    Used by --mem-macro so an offline harness can redirect every binding at
    an in-process copy of the image instead of the real address range,
    without hand-editing a single cast (win32_pilot.md SS7a)."""
    return HEX_ADDR_IN_BODY_RE.sub(lambda m: '%s(%s)' % (macro, m.group(0)), body)


def parse_header_defines(path, pattern):
    out = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = pattern.match(line.rstrip('\n'))
            if m:
                out[m.group(1)] = m.group(2)
    return out


def load_index(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--index', default=str(HERE / 'interop_index.json'))
    ap.add_argument('--imports', default=str(HERE.parent.parent / 'imports.json'),
                     help='imports.json (the guest PE import table, [dll, name, '
                          'iat_slot_va] triples) -- the evidence source for '
                          'GUEST_CRT_IMPORTS\' slot addresses.')
    ap.add_argument('--globals-header', default=str(HERE / 'it_globals.h'))
    ap.add_argument('--funcs-header', default=str(HERE / 'it_funcs.h'))
    ap.add_argument('--out', default=str(HERE / 'pf_bindings.h'))
    ap.add_argument('--types-out', default=str(HERE / 'pf_bindings_types.h'))
    ap.add_argument('--exclude', default='',
                     help='comma-separated names to omit (functions being '
                          'compiled natively into the carrier from src/)')
    ap.add_argument('--mem-macro', default=None,
                     help='wrap every address literal in the emitted casts '
                          'with MACRO(...), e.g. --mem-macro PF_MEM turns '
                          '(*(fixed*)0x4fac28) into (*(fixed*)PF_MEM(0x4fac28)). '
                          'For an offline harness that redirects PF_MEM at an '
                          'in-process copy of the image instead of the real '
                          'address range (win32_pilot.md SS7a); the real '
                          'carrier build omits this (the image is mapped at '
                          'its real address, so the cast is already correct).')
    ap.add_argument('--guard-define', default=None,
                     help='additionally #define NAME 1 right after this '
                          "header's own include guard opens. Lets src/ "
                          'headers (game_types.h, game_state.h) detect '
                          '"a generated bindings header is force-included '
                          'ahead of me" via #ifndef NAME without ever '
                          'referencing a carrier-reserved identifier '
                          'themselves (scripts/check_native_layer.py bans '
                          'PF_/IT_G_/IT_F_/PFN_ prefixes in src/); NAME '
                          'should therefore be an ordinary, non-reserved '
                          'name, e.g. ICYTOWER_BINDINGS_ACTIVE.')
    args = ap.parse_args()

    exclude = set(n.strip() for n in args.exclude.split(',') if n.strip())
    mem_macro = args.mem_macro

    index = load_index(args.index)
    try:
        import_slots = load_import_slots(args.imports)
    except (OSError, ValueError) as e:
        sys.stderr.write('gen_bindings.py: %s: %s\n' % (args.imports, e))
        return 1
    idx_globals = index['globals']
    idx_functions = index['functions']

    global_bodies = parse_header_defines(args.globals_header, GLOBAL_DEFINE_RE)
    func_bodies = parse_header_defines(args.funcs_header, FUNC_DEFINE_RE)

    missing_globals = [g['name'] for g in idx_globals if g['name'] not in global_bodies]
    missing_funcs = [f['name'] for f in idx_functions if f['name'] not in func_bodies]
    if missing_globals or missing_funcs:
        sys.stderr.write(
            'gen_bindings.py: interop_index.json and the generated headers '
            'disagree -- regenerate it_globals.h/it_funcs.h first.\n'
            'missing from it_globals.h: %r\n'
            'missing from it_funcs.h: %r\n' % (missing_globals, missing_funcs))
        return 1

    reserved_collisions = []  # (kind, name)
    member_collisions = []    # (kind, name) -- see MEMBER_ACCESS_COLLISIONS above
    excluded_globals = []
    excluded_functions = []
    emitted_globals = []
    emitted_functions = []

    lines = []
    lines.append('/* pf_bindings.h -- GENERATED FILE. DO NOT EDIT.')
    lines.append(' * Produced by carrier/gen/gen_bindings.py from:')
    lines.append(' *   %s' % Path(args.index).name)
    lines.append(' *   %s (reused cast expressions)' % Path(args.globals_header).name)
    lines.append(' *   %s (reused PFN_* typedefs + cast expressions)' % Path(args.funcs_header).name)
    lines.append(' *   %s (IAT slot VAs for GUEST_CRT_IMPORTS)' % Path(args.imports).name)
    lines.append(' * Generated: %s UTC' % datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'))
    if exclude:
        lines.append(' * Excluded (compiled natively, name kept free): %s' %
                      ', '.join(sorted(exclude)))
    if mem_macro:
        lines.append(' * Addresses wrapped with --mem-macro: %s(...)' % mem_macro)
    if args.guard_define:
        lines.append(' * Also defines the purity-safe guard: %s' % args.guard_define)
    lines.append(' *')
    lines.append(' * See win32_pilot.md SS7a: this header is forced-included (/FI) ONLY')
    lines.append(' * when address-free clean C from src/ is compiled INTO the carrier.')
    lines.append(' * It maps every game-scope global/function PLAIN name to its original')
    lines.append(' * address, so `extern int reward_scale;` and a bare call to a game')
    lines.append(' * function in src/ resolve to (*(T*)VA) / ((PFN)VA) at build time --')
    lines.append(' * no address literal and no carrier type ever appears in src/ itself.')
    lines.append(' * Re-run gen_bindings.py to regenerate; do not hand-edit.')
    lines.append(' */')
    lines.append('')
    lines.append('#ifndef PF_BINDINGS_H')
    lines.append('#define PF_BINDINGS_H')
    lines.append('')
    if args.guard_define:
        lines.append('#define %s 1  /* purity-safe "bindings are active" signal for src/ */' %
                      args.guard_define)
        lines.append('')
    lines.append('#include "pf_bindings_types.h"  /* struct/enum/typedef layouts */')
    lines.append('#include "it_funcs.h"           /* PFN_<name> typedefs, reused verbatim */')
    lines.append('')
    try:
        crt_lines, emitted_crt_imports = emit_guest_crt_imports(import_slots, mem_macro)
    except ValueError as e:
        sys.stderr.write('gen_bindings.py: %s\n' % e)
        return 1
    lines.extend(crt_lines)
    lines.append('/* ------------------------------------------------------------------ */')
    lines.append('/* globals: <name> -> (*(T*)VA), identical to it_globals.h IT_G_<name> */')
    lines.append('/* ------------------------------------------------------------------ */')
    lines.append('')

    for g in sorted(idx_globals, key=lambda x: x['name']):
        name = g['name']
        if name in RESERVED_CRT_WINDOWS_IDENTS:
            reserved_collisions.append(('global', name))
            lines.append('/* SKIPPED: "%s" collides with a reserved CRT/Windows identifier; '
                          'see BINDINGS_NOTES.md */' % name)
            continue
        if name in MEMBER_ACCESS_COLLISIONS:
            member_collisions.append(('global', name))
            lines.append('/* SKIPPED: "%s" collides with a struct/union member name elsewhere '
                          'in scope -- a plain #define would also rewrite that member access '
                          '(e.g. `x.%s`); see MEMBER_ACCESS_COLLISIONS in gen_bindings.py '
                          'and PROMOTIONS.md batch 7 */' % (name, name))
            continue
        if name in exclude:
            excluded_globals.append(name)
            lines.append('/* excluded by --exclude: %s */' % name)
            continue
        lines.append('/* %s  VA=%s  type=%s  cu=%s */' %
                      (name, g['va'], g['type'], g['cu']))
        body = global_bodies[name]
        if mem_macro:
            body = wrap_addresses(body, mem_macro)
        lines.append('#define %s %s' % (name, body))
        emitted_globals.append(name)
    lines.append('')

    lines.append('/* ------------------------------------------------------------------ */')
    lines.append('/* functions: <name> -> ((PFN_<name>)VA), identical to it_funcs.h      */')
    lines.append('/* IT_F_<name>. A name in --exclude is omitted so its own native       */')
    lines.append('/* definition in src/ is not redirected to the original address.       */')
    lines.append('/* ------------------------------------------------------------------ */')
    lines.append('')

    for f in sorted(idx_functions, key=lambda x: x['name']):
        name = f['name']
        if name in RESERVED_CRT_WINDOWS_IDENTS:
            reserved_collisions.append(('function', name))
            lines.append('/* SKIPPED: "%s" collides with a reserved CRT/Windows identifier; '
                          'see BINDINGS_NOTES.md */' % name)
            continue
        if name in MEMBER_ACCESS_COLLISIONS:
            member_collisions.append(('function', name))
            lines.append('/* SKIPPED: "%s" collides with a struct/union member name elsewhere '
                          'in scope; see MEMBER_ACCESS_COLLISIONS in gen_bindings.py and '
                          'PROMOTIONS.md batch 7 */' % name)
            continue
        if name in exclude:
            excluded_functions.append(name)
            lines.append('/* excluded by --exclude (compiled natively): %s */' % name)
            continue
        lines.append('/* %s  VA=%s  cu=%s */' % (name, f['va'], f['cu']))
        lines.append('/* prototype: %s */' % f['prototype'])
        body = func_bodies[name]
        if mem_macro:
            body = wrap_addresses(body, mem_macro)
        lines.append('#define %s %s' % (name, body))
        emitted_functions.append(name)
    lines.append('')
    lines.append('#endif /* PF_BINDINGS_H */')
    lines.append('')

    Path(args.out).write_text('\n'.join(lines), encoding='utf-8')

    types_lines = [
        '/* pf_bindings_types.h -- GENERATED FILE. DO NOT EDIT.',
        ' * Produced by carrier/gen/gen_bindings.py.',
        ' * Generated: %s UTC' % datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'),
        ' *',
        ' * Carrier-side type provider for pf_bindings.h: the struct/union/enum/',
        ' * typedef layouts every game-scope global and function prototype needs,',
        ' * recovered from DWARF by gen_interop.py into it_types.h. This header',
        ' * exists so pf_bindings.h has one clearly-named type dependency; it is',
        ' * only used by code compiled INTO the carrier (see win32_pilot.md SS7a).',
        ' * src/ will get its OWN, hand-owned copy of these types later, at which',
        ' * point src/ stops depending on this file.',
        ' * Re-run gen_bindings.py to regenerate; do not hand-edit.',
        ' */',
        '',
        '#ifndef PF_BINDINGS_TYPES_H',
        '#define PF_BINDINGS_TYPES_H',
        '',
        '#include "it_types.h"',
        '',
        '#endif /* PF_BINDINGS_TYPES_H */',
        '',
    ]
    Path(args.types_out).write_text('\n'.join(types_lines), encoding='utf-8')

    summary = {
        'globals_total': len(idx_globals),
        'globals_emitted': len(emitted_globals),
        'globals_excluded': excluded_globals,
        'functions_total': len(idx_functions),
        'functions_emitted': len(emitted_functions),
        'functions_excluded': excluded_functions,
        'guest_crt_imports': emitted_crt_imports,
        'reserved_collisions': reserved_collisions,
        'member_name_collisions': member_collisions,
        'out': args.out,
        'types_out': args.types_out,
        'mem_macro': mem_macro,
        'guard_define': args.guard_define,
    }
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
