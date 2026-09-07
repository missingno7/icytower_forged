#!/usr/bin/env python3
"""gen_src_headers.py -- generates src/icytower/game_types.h, allegro_types.h,
game_state.h, game_funcs.h and state.c from DWARF debug info of the original
icytower15.exe, so the clean port (win32_pilot.md SS7a, src/README.md) no
longer carries hand-transcribed struct layouts.

This is deliberately NOT a second DWARF parser. It reuses gen_interop.py's
parsed model verbatim: the same `parse_dwarf`/`resolve_type`/`decl`/
`emit_struct_body`/`topo_order` machinery that already produces
carrier/gen/it_types.h (verified: 801/801 sizeof/offsetof checks PASS
against real MSVC, see INTEROP_NOTES.md) is reused here unmodified, so
game_types.h's struct bodies are byte-for-byte the same recovery
gen_interop.py already proved correct -- this script only adds:

  1. an origin split so the game's own structs (Tplayer, Tmap, Tfloor, ...)
     land in game_types.h while public library types the game merely uses
     (BITMAP, RGB, SAMPLE, fixed, ...) land in allegro_types.h;
  2. parameter NAME capture for game_funcs.h;
  3. plain extern/prototype/definition text instead of address-cast macros
     -- no VA ever appears in src/, unlike it_globals.h/it_funcs.h (win32_
     pilot.md SS7a: src/ is address-free; carrier/gen/ is not).

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

Thin delegator (notes/extraction_plan.md S1): the generator itself now lives
in the port_forge submodule as tools/pf_win32_gen_src_headers.py,
parametrized on the CU path prefix, the purity-safe guard macro names, the
per-file include guard prefix, the parameter renames and the purity-banned
prefix list this project used to hardcode (see that tool's own module
docstring). This wrapper injects all of them from carrier/win32_policy.json
so the documented `--scope game` invocation above keeps working unchanged;
a caller MAY still pass any of these explicitly to override the project
defaults injected here.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TOOLS_DIR = os.path.join(ROOT, 'port_forge', 'tools')
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_gen_src_headers as _impl  # noqa: E402

POLICY_PATH = os.path.join(HERE, '..', 'win32_policy.json')


def _inject(argv, flag, value):
    if flag in argv:
        return argv
    return argv + [flag, value]


def main():
    with open(POLICY_PATH, encoding='utf-8') as f:
        policy = json.load(f)

    argv = list(sys.argv[1:])
    argv = _inject(argv, '--cu-prefix', policy['cu_prefix'])
    argv = _inject(argv, '--bindings-guard', policy['bindings_guard'])
    argv = _inject(argv, '--upstream-guard', policy['upstream_guard'])
    argv = _inject(argv, '--include-guard-prefix', policy['src_headers_include_guard_prefix'])
    argv = _inject(argv, '--purity-banned-prefixes', ','.join(policy['purity_banned_prefixes']))
    argv = _inject(argv, '--orig-prefix', policy['orig_prefix'])
    argv = _inject(argv, '--names', os.path.normpath(os.path.join(ROOT, policy['names_json'])))
    if '--param-rename' not in argv:
        for name, replacement in policy['param_renames'].items():
            argv += ['--param-rename', '%s=%s' % (name, replacement)]

    sys.argv = [sys.argv[0]] + argv
    _impl.main()


if __name__ == '__main__':
    main()
