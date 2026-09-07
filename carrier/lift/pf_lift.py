#!/usr/bin/env python3
"""pf_lift.py -- per-function x86-32 -> C lifter for the Icy Tower Win32 carrier.

Produces the LIFTED form (win32_pilot.md SS3) of one game function: a C file
defining `lifted_<name>` with EXACTLY the prototype the generated interop header
gives for that VA, so the carrier can bind it at the original address.

Thin delegator (notes/extraction_plan.md S3): the lifter itself now lives in
the port_forge submodule as tools/pf_win32_lift.py -- the mechanism was
already target-independent (it only reads --image/--functions/--interop), and
the two Icy Tower literals found during the move (the generated .c header's
hardcoded "icytower15.exe" filename, and a hardcoded [0x400000, 0x800000)
"is this displacement inside the image" heuristic) were fixed there to derive
from the --image argument itself, so no policy field was needed here. This
wrapper exists only so callers (harness scripts, habit) keep working with an
unchanged CLI while the mechanism is shared framework code. The two runtime
headers this tool writes (pf_rt.h, pf_x87_soft.h) now live at
port_forge/tools/win32_lift/lift_rt.hpp and x87_soft.hpp -- a deliberate
deviation from the plan's `src/platform/win32/lift_rt.hpp` target, because
that directory is S2 work (a large policy-struct layout + a
check_boundaries.py DAG entry, see notes/extraction_plan.md SS2.1) not yet
done. No new field was added to carrier/win32_policy.json: the CLI already
carries every path this tool needs as an argument.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_lift as _impl  # noqa: E402

if __name__ == "__main__":
    sys.exit(_impl.main())
