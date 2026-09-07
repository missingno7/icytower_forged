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

Thin delegator (notes/extraction_plan.md S1): the generator itself now lives
in the port_forge submodule as tools/pf_win32_gen_interop.py, parametrized on
the CU path prefix and names.json this project used to hardcode/auto-guess
(see that tool's own module docstring). This wrapper injects both from
carrier/win32_policy.json so the documented `--scope game` invocation above
keeps working unchanged; a caller MAY still pass --cu-prefix/--names
explicitly to override the project defaults injected here.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TOOLS_DIR = os.path.join(ROOT, 'port_forge', 'tools')
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_gen_interop as _impl  # noqa: E402

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
    argv = _inject(argv, '--names', os.path.normpath(os.path.join(ROOT, policy['names_json'])))

    sys.argv = [sys.argv[0]] + argv
    _impl.main()


if __name__ == '__main__':
    main()
