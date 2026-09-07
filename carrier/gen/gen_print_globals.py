#!/usr/bin/env python3
"""gen_print_globals.py - generates gen/it_print_globals.inc: a flat, typed
C table of every DWARF-named game global (carrier/gen/interop_index.json)
plus every struct member (carrier/gen/it_types.h + it_types_check.c), for
carrier/src/print_globals.cpp's `--print-globals` at shutdown.

Why generated rather than hand-written (see win32_pilot.md milestone 9a /
carrier/NOTES.md "Headless, frame oracle, named globals, .itr workload"):
`--print-globals name1,name2,...` must work for ANY named global the DWARF
knows about, not just Icy Tower's own score/floor/combo - project-specific
names are supplied on the command line (by scripts/play.py), never baked
into carrier source.

Inputs (read-only):
    carrier/gen/interop_index.json  - the `globals` list (name, va, type, cu)
    carrier/gen/it_types.h          - every struct's members, in DWARF order
    carrier/gen/it_types_check.c    - authoritative sizeof/offsetof values

Output: carrier/gen/it_print_globals.inc

Usage: python gen_print_globals.py [repo_root]

Thin delegator (notes/extraction_plan.md S1): the generator itself now lives
in the port_forge submodule as tools/pf_win32_gen_print_globals.py, which
takes an explicit --gen-dir instead of assuming it lives alongside its
inputs. This wrapper keeps the documented `[repo_root]` positional
convention: repo_root/carrier/gen is passed as --gen-dir.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TOOLS_DIR = os.path.join(DEFAULT_ROOT, 'port_forge', 'tools')
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_gen_print_globals as _impl  # noqa: E402


def main(argv):
    repo_root = argv[1] if len(argv) > 1 else DEFAULT_ROOT
    gen_dir = os.path.join(repo_root, 'carrier', 'gen')
    return _impl.main([argv[0], '--gen-dir', gen_dir])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
