#!/usr/bin/env python3
"""recovery_audit.py -- catch a recovered function's wrong-struct-member and
wrong-magic-divide-constant defects before the in-vivo pass (Divergence 010,
carrier/NOTES.md, notes/living_record.md): a global-reference audit (every
absolute-address reference in the ORIGINAL disassembly, resolved to
(global, member) via carrier/gen/interop_index.json + it_types.h, compared
against the recovered .c source) and a magic-divide audit (every
`imul $magic` / `mul $magic` reciprocal-division idiom decoded to the
divisor it implements, compared against the divisor written in source).

Usage:
    python recovery_audit.py --function play [play_jump_sound ...]
    python recovery_audit.py --src-dir src/icytower   # every promoted
                                                        # function found there
Exit code: 0 if every audited function passes both audits (a mismatch fully
covered by carrier/recovery_audit_policy.json's allow-lists does not count
against this), 1 on any unresolved mismatch, 2 on a usage/parse error.

Thin delegator (notes/extraction_plan.md S3): the mechanism itself lives in
the port_forge submodule as tools/pf_win32_recovery_audit.py. This wrapper
injects --repo (this project's root), --policy (carrier/recovery_audit_policy.json,
the project's own MEASURED allow-list -- see that file's own '_purpose' and
per-entry '_evidence') and --image (assets/icytower15.exe, if present) so the
documented bare invocation above keeps working unchanged; a caller MAY still
pass --repo/--policy/--image/--interop-index/--gen-dir/--disasm-text/
--functions-json/--coff-symbols/--addr2line explicitly to override any one of
them.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CARRIER_DIR = os.path.dirname(HERE)
ROOT = os.path.dirname(CARRIER_DIR)
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_recovery_audit as _impl  # noqa: E402


def _inject(argv, flag, value):
    """Append flag+value only if the caller did not already pass that flag
    -- an explicit caller argument always wins over the project default
    this shim would otherwise inject."""
    if flag in argv:
        return argv
    return argv + [flag, value]


def main():
    argv = list(sys.argv[1:])
    argv = _inject(argv, '--repo', ROOT)
    default_policy = os.path.join(CARRIER_DIR, 'recovery_audit_policy.json')
    if os.path.exists(default_policy):
        argv = _inject(argv, '--policy', default_policy)
    default_image = os.path.join(ROOT, 'assets', 'icytower15.exe')
    if os.path.exists(default_image):
        argv = _inject(argv, '--image', default_image)
    return _impl.main([sys.argv[0]] + argv)


if __name__ == "__main__":
    sys.exit(main())
