#!/usr/bin/env python3
"""bind_all.py - Milestone 12 at scale (carrier/NOTES.md): loops the
Milestone 11 A/B experiment (carrier/NOTES.md "Milestones 11-12" part D)
over all 35 src/icytower functions (src/icytower/PROMOTIONS.md), one
function at a time, over the operator's own recording
(replays/human_test.txt, 2293 gameplay ticks / 100 floors / score 2386;
baseline per-tick digest replays/human_test.digest).

Per function <fn>:
    run A = --bind <fn>=original --fn-digest-out A   (DR-sensed: the DR
            budget, carrier/NOTES.md "Milestones 11-12" part B, allows
            exactly ONE ORIGINAL-form function per run, hence the loop)
    run B = --bind <fn>=src      --fn-digest-out B --digest-out B_ticks
    compare_fn_digests.py A B      -> EQUAL (n invocations) or FIRST DIFFERENCE
    compare_digests.py B_ticks baseline -> per-tick global digest EQUAL/DIFFER

A function the workload never invokes (0 records in both A and B) is
reported as "unverified in vivo" rather than compared (compare_fn_digests.py
itself refuses to call that EQUAL - "the sensor produced nothing").

Usage:
    python bind_all.py [--fn NAME[,NAME...]] [--out-dir DIR]
                        [--input-script PATH] [--baseline PATH]
                        [--stop-at-tick N]

Run from carrier/ (same convention as gates.ps1). Restores the pristine
assets before every carrier.exe launch, the same convention every measured
run in carrier/NOTES.md uses, and waits out any still-tearing-down previous
carrier.exe process first (restore_assets.ps1's own documented race).

Thin delegator (notes/extraction_plan.md S3): the loop mechanism itself now
lives in the port_forge submodule as tools/pf_win32_bind_all.py, parametrized
on the carrier executable path, its fixed command-line flags, the 35
function names, the x87 subset, and the recording/baseline/pre-launch-script
paths this project used to hardcode (see that tool's own module docstring).
This wrapper injects every one of those from carrier/win32_policy.json's
"bind_all" section (plus this project's own restore_assets.ps1 as the
pre-launch hook) so the documented invocations above (and in
carrier/NOTES.md, src/icytower/INVIVO.md, src/icytower/PROMOTIONS.md) keep
working unchanged; a caller MAY still pass --policy/--carrier-exe/
--pre-launch-script/... explicitly to override any one of them.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CARRIER_DIR = os.path.dirname(HERE)
ROOT = os.path.dirname(CARRIER_DIR)
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_bind_all as _impl  # noqa: E402

POLICY_PATH = os.path.join(CARRIER_DIR, 'win32_policy.json')


def _inject(argv, flag, value):
    if flag in argv:
        return argv
    return argv + [flag, value]


def main():
    argv = list(sys.argv[1:])
    argv = _inject(argv, '--policy', POLICY_PATH)
    argv = _inject(argv, '--policy-key', 'bind_all')
    argv = _inject(argv, '--carrier-exe', os.path.join(CARRIER_DIR, 'carrier.exe'))
    argv = _inject(argv, '--pre-launch-script', os.path.join(HERE, 'restore_assets.ps1'))
    argv = _inject(argv, '--out-dir', os.path.join(ROOT, 'artifacts_ms12'))
    argv = _inject(argv, '--input-script', os.path.join(ROOT, 'replays', 'human_test.txt'))
    argv = _inject(argv, '--baseline', os.path.join(ROOT, 'replays', 'human_test.digest'))
    argv = _inject(argv, '--stop-at-tick', '2528')
    argv = _inject(argv, '--run-seconds', '180')

    sys.argv = [sys.argv[0]] + argv
    return _impl.main()


if __name__ == "__main__":
    sys.exit(main())
