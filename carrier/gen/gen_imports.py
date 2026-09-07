#!/usr/bin/env python3
"""gen_imports.py - generates carrier/gen/import_table.inc, import_stubs.cpp,
and import_names.inc from <repo_root>/imports.json.

Input format (imports.json): a JSON array of [dll, name, iat_slot_va_hex]
triples, one per statically-imported function in icytower15.exe's import
directory.

Run: python gen_imports.py <repo_root>/imports.json <repo_root>/carrier/gen

Regenerate whenever imports.json changes. Do not hand-edit the outputs.

Thin delegator (notes/extraction_plan.md S1): the generator itself now lives
in the port_forge submodule as tools/pf_win32_gen_imports.py (no project
literal in it -- this file has no policy to inject, since gen_imports.py's
CLI never carried one). This wrapper exists only so callers (build.cmd,
scripts, habit) keep working with an unchanged CLI while the mechanism is
shared framework code. See carrier/win32_policy.json.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_gen_imports as _impl  # noqa: E402

if __name__ == "__main__":
    sys.exit(_impl.main())
