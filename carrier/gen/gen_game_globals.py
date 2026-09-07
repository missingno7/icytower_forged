#!/usr/bin/env python3
"""gen_game_globals.py - generates gen/game_globals.inc: the {VA, size} table
of every game-owned (not Allegro/CRT/DirectX) .data/.bss global, used by
carrier/src/det.cpp's tick-sensor digest (milestones 5-7, win32_pilot.md).

Why this table exists (see carrier/NOTES.md "Milestones 5-7"): hashing the
FULL .data+.bss range makes two otherwise-identical --det runs diverge,
because Windows randomizes the load address of every dynamically-loaded DLL
(ddraw/dinput/dsound/msvcrt) per PROCESS, and pointers into those DLLs'
objects get baked directly into scattered Allegro-internal globals
throughout .bss. Restricting the digest to game-owned globals only sidesteps
the whole category at its source.

Inputs (read-only, never modified by this script):
    carrier/gen/interop_index.json  - the `globals` list (name, va, type)
    artifacts/coff_symbols.json     - full COFF symbol table, used only to
                                       find each global's size

Output: carrier/gen/game_globals.inc

Usage: python gen_game_globals.py [repo_root]

Thin delegator (notes/extraction_plan.md S1): the generator itself now lives
in the port_forge submodule as tools/pf_win32_gen_digest_domain.py,
parametrized on the .data/.bss ranges, the excluded host-identity globals
and the size overrides this project used to hardcode (see that tool's own
module docstring; this project's values live in carrier/win32_policy.json's
"digest_domain" section). This wrapper keeps the documented `[repo_root]`
positional convention.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TOOLS_DIR = os.path.join(DEFAULT_ROOT, 'port_forge', 'tools')
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_gen_digest_domain as _impl  # noqa: E402


def main(argv):
    repo_root = argv[1] if len(argv) > 1 else DEFAULT_ROOT
    policy_path = os.path.join(repo_root, 'carrier', 'win32_policy.json')
    sys.argv = [
        argv[0],
        '--ownership', 'dwarf-cu',
        '--interop', os.path.join(repo_root, 'carrier', 'gen', 'interop_index.json'),
        '--coff-symbols', os.path.join(repo_root, 'artifacts', 'coff_symbols.json'),
        '--policy', policy_path,
        '--policy-key', 'digest_domain',
        '--out', os.path.join(repo_root, 'carrier', 'gen', 'game_globals.inc'),
    ]
    return _impl.main()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
