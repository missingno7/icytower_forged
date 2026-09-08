#!/usr/bin/env python3
"""gen_lib_bindings.py -- generate carrier/gen/pf_lib_bindings.h,
carrier/gen/pf_lib_bindings_types.h and src/icytower/allegro_api.h.

See this project's notes/library_boundary.md and win32_pilot.md SS7b for
what this closes: the library-call layer game-scope pf_bindings.h does not
cover (100 Allegro-family functions, 26 library globals the game reads).

Thin delegator (notes/extraction_plan.md S1): the generator itself now lives
in the port_forge submodule as tools/pf_win32_gen_lib_bindings.py,
parametrized on the CU path prefix, the purity-safe guard macro names and
the standalone-header's own include guard this project used to hardcode
(see that tool's own module docstring). This wrapper injects all of them,
plus this project's --allegro-api-out path, from carrier/win32_policy.json
so the zero-argument invocation this generator has always supported keeps
working unchanged; a caller MAY still pass any of these explicitly to
override the project defaults injected here.

Also injects --src-scan-dir = win32_policy.json's src_dir (src/icytower),
so every run mechanically re-derives the "extra symbols" list a call/read-
edge allow-list can never see on its own (PROMOTIONS.md batch 11: `_cos_tbl`,
a data table only referenced because a clean-room port writes an upstream
`static inline` -- fixsin -- out non-inlined). See the framework tool's own
docstring "--extra-symbols / --src-scan-dir" for the mechanism.

Also now injects --out/--types-out/--notes-out = win32_policy.json's gen_dir
(carrier/gen). Closes a real, previously-documented gap (carrier/NOTES.md
"In-vivo draw_frame via the frame oracle": the framework tool's own --out/
--types-out/--notes-out default to a path under port_forge/tools/, not this
project's carrier/gen/, because this wrapper never injected them -- a
zero-argument run silently wrote its three generated files to the WRONG
directory, worked around by hand every prior pass ("passing all three
explicitly"). This wrapper now injects the project's own carrier/gen/ path
for all three, same as every other _-out argument here, so the documented
zero-argument invocation actually regenerates carrier/gen/pf_lib_bindings.h
in place.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
TOOLS_DIR = os.path.join(ROOT, 'port_forge', 'tools')
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_gen_lib_bindings as _impl  # noqa: E402

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
    argv = _inject(argv, '--include-guard', policy['lib_bindings_include_guard'])
    argv = _inject(argv, '--allegro-api-out',
                    os.path.normpath(os.path.join(ROOT, 'src', 'icytower', 'allegro_api.h')))
    argv = _inject(argv, '--src-scan-dir', os.path.normpath(os.path.join(ROOT, policy['src_dir'])))
    gen_dir = os.path.normpath(os.path.join(ROOT, policy['gen_dir']))
    argv = _inject(argv, '--out', os.path.join(gen_dir, 'pf_lib_bindings.h'))
    argv = _inject(argv, '--types-out', os.path.join(gen_dir, 'pf_lib_bindings_types.h'))
    argv = _inject(argv, '--notes-out', os.path.join(gen_dir, 'LIB_BINDINGS_NOTES.md'))

    sys.argv = [sys.argv[0]] + argv
    return _impl.main()


if __name__ == '__main__':
    sys.exit(main())
