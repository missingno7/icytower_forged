#!/usr/bin/env python3
"""compare_digests.py - milestone 7 (win32_pilot.md sec 7/8, E).

Compares two --digest-out files line by line ("T <sha256> esp=.. ebp=.. ...",
one line per consumed game tick, written by carrier/src/det.cpp's safepoint
sensor at VA 0x4124f4). Prints EQUAL if every tick's sha256 matches, or the
first differing tick (and, if useful, the first differing register) if not.

Usage:
    python compare_digests.py FILE_A FILE_B
Exit code: 0 if EQUAL, 1 if they differ, 2 on a usage/parse error.

Thin delegator (notes/extraction_plan.md S3): the comparator itself now lives
in the port_forge submodule as tools/pf_win32_compare_digests.py (no project
literal in it -- this file has no policy to inject, since compare_digests.py's
CLI never carried one). This wrapper exists only so callers (gates.ps1,
bind_all.py, habit) keep working with an unchanged CLI while the mechanism is
shared framework code. See carrier/win32_policy.json.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_compare_digests as _impl  # noqa: E402

if __name__ == "__main__":
    sys.exit(_impl.main(sys.argv))
