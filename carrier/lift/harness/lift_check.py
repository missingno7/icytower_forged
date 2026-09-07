#!/usr/bin/env python3
"""lift_check.py -- offline equivalence check for a candidate C form
(LIFTED/NATIVE/SRC) against the ORIGINAL x86 bytes.

Thin delegator (notes/extraction_plan.md S3): the offline-oracle ENGINE
(unicorn setup, FPU control-word rule, memory/call-trace domain comparison,
negative controls, harness .exe invocation) now lives in the port_forge
submodule as tools/pf_win32_offline_oracle.py; the Icy-Tower-specific
per-function SPECS dict and vector generators live alongside this file as
icytower_specs.py ("specs are code (generators), so a module path is
acceptable" -- notes/extraction_plan.md S3). See both files' own module
docstrings for the full design.

This wrapper exists only so callers (build scripts, README-documented
commands, habit) keep using lift_check.py's ORIGINAL command line
unchanged while the mechanism is shared framework code: every flag this
script ever took (--image, --form, --exe, --toolchain, --funcs, --vectors,
--seed, --fault, --census, --json) is passed straight through, and this
wrapper injects --specs-module (pointing at icytower_specs.py, next to
this file) and --harness-dir (this directory -- where lift_check.exe,
native_check.exe, src_check.exe and gcc_check_*.exe already live, and
where guest.bin/vectors_*.bin/results_*.bin have always been written)
automatically, so no caller needs a new flag. See carrier/win32_policy.json
for the analogous hand-curated-JSON convention used elsewhere in this
extraction; this module is the "specs are code" version of the same idea.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_offline_oracle as _impl  # noqa: E402

SPECS_MODULE = os.path.join(HERE, "icytower_specs.py")


def main():
    argv = sys.argv[1:]
    if not any(a == "--specs-module" or a.startswith("--specs-module=") for a in argv):
        argv = ["--specs-module", SPECS_MODULE] + argv
    if not any(a == "--harness-dir" or a.startswith("--harness-dir=") for a in argv):
        argv = ["--harness-dir", HERE] + argv
    sys.argv = [sys.argv[0]] + argv
    return _impl.main()


if __name__ == "__main__":
    sys.exit(main())
