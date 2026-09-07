#!/usr/bin/env python3
"""scan_src_defs.py -- lists the top-level function names DEFINED in
src/icytower/*.c (excluding state.c, which only supplies extern storage for
the standalone build and never defines a function -- src/README.md).

Purpose (win32_pilot.md SS7a, item 1 of the "src binding" pass): build.cmd
needs gen_bindings.py's --exclude list to contain exactly the functions
src/ currently defines, so pf_bindings_src.h does not also try to redirect
those names to their own original address (BINDINGS_NOTES.md's "Exclusion"
section - a function compiled natively into the carrier must keep its own
plain name). Hand-maintaining that list would silently drift the moment a
new file is added to src/icytower/ (exactly the class of bug BINDINGS_NOTES.md
warns about for renaming); this script derives it mechanically from the
source files themselves instead, every time build.cmd runs.

Usage:
    python scan_src_defs.py [--src-dir DIR] [--format csv|lines]
Prints the comma-separated (default) or newline-separated function names to
stdout; build.cmd captures stdout with `for /f` into an --exclude argument.
Warns (stderr) about any src/*.c file with zero definitions found (this
would silently break the exclusion list, so it must not go unnoticed) but
still exits 0 -- an empty src/ is a legitimate, if unusual, state.

--list-build-files {msvc,gcc} ("binding table generated" pass, carrier/
NOTES.md): prints the comma-separated list of src/icytower/*.c FILES (not
function names) build.cmd should compile through the named toolchain -- see
port_forge's tools/pf_win32_scan_src_defs.py module docstring for the rest.

Thin delegator (notes/extraction_plan.md S1): the generator itself now lives
in the port_forge submodule as tools/pf_win32_scan_src_defs.py, parametrized
on the literals this project used to hardcode (src/icytower's exact path,
the state.c exclusion, the 5 asset-seam accessor names) -- see
carrier/win32_policy.json. This wrapper injects exactly those values so
build.cmd's existing CLI (--src-dir, --format, --list-build-files,
--extra-fi, --prefix, --ext) keeps working unchanged; a caller MAY still
pass --exclude/--gen-dir/--asset-seam-funcs explicitly to override the
project defaults injected here.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TOOLS_DIR = os.path.join(ROOT, 'port_forge', 'tools')
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_scan_src_defs as _impl  # noqa: E402

POLICY_PATH = os.path.join(HERE, '..', 'win32_policy.json')


def _inject(argv, flag, value):
    """Append flag+value to argv only if the caller did not already pass
    that flag -- an explicit caller argument always wins over the project
    default this shim would otherwise inject."""
    if flag in argv:
        return argv
    return argv + [flag, value]


def main():
    with open(POLICY_PATH, encoding='utf-8') as f:
        policy = json.load(f)

    argv = list(sys.argv[1:])
    argv = _inject(argv, '--src-dir', os.path.normpath(os.path.join(ROOT, policy['src_dir'])))
    argv = _inject(argv, '--exclude', ','.join(policy['scan_exclude']))
    argv = _inject(argv, '--asset-seam-funcs', ','.join(policy['asset_seam_funcs']))
    if '--list-build-files' in argv:
        argv = _inject(argv, '--gen-dir', os.path.normpath(os.path.join(ROOT, policy['gen_dir'])))

    sys.argv = [sys.argv[0]] + argv
    _impl.main()


if __name__ == '__main__':
    main()
