#!/usr/bin/env python3
"""certify_snapshot.py - milestone 8 (win32_pilot.md sec 6/8 row 8).

The anchor-certification contract from notes/portforge_capsule.md sec D:

    "cold -> anchor -> suffix"  ==  "restore(anchor) -> suffix"

compared ROW BY ROW over the common suffix. Two forms, one per subcommand:

  rewind  FILE --anchor T [--cold COLD]
      One --digest-out file from an in-run rewind
      (`--snapshot-at-tick T --restore-at-tick T2` in ONE carrier run). The
      stream contains the rewind as a tick that goes BACKWARDS; this splits
      it there and compares the pre-rewind rows from the anchor against the
      post-rewind rows. With --cold it additionally compares the post-rewind
      suffix against an INDEPENDENT cold run's suffix from the same anchor,
      which is the cross-process half of the same contract.

  restore --cold COLD --restore RESTORED --anchor T
      Two separate runs: a cold run that snapshotted at T, and a run started
      with `--restore-from DIR` that restored to T at its first safepoint.

  fn      FILE
      The same certification one level finer: a --fn-digest-out stream from
      an in-run rewind (bind.cpp's per-invocation sensor). The rewind shows
      up as the per-function invocation index `k` going backwards, because
      the sensor's counters are part of the snapshot (bind.hpp's
      BindSavedState). Records are compared field for field except `form`
      and `raweax`, exactly as compare_fn_digests.py does.

Why positional, not keyed by tick: the carrier's tick index is
virtual_ms/20, and play()'s catch-up branch can consume two game ticks
inside one 20 ms window, so a digest stream legitimately contains REPEATED
tick numbers (measured: T=659 appears twice in the milestone-7 workload).
Keying rows by tick silently drops one of them and manufactures a
difference that is not there - so rows are compared in order, tick and
digest together.

Usage / exit codes: 0 on EQUAL, 1 on a difference, 2 on a usage error.

Thin delegator (notes/extraction_plan.md S3): the certifier itself now lives
in the port_forge submodule as tools/pf_win32_certify_snapshot.py (no project
literal in it -- this file has no policy to inject, since certify_snapshot.py's
CLI never carried one). This wrapper exists only so callers (gates.ps1,
habit) keep working with an unchanged CLI while the mechanism is shared
framework code. See carrier/win32_policy.json.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_certify_snapshot as _impl  # noqa: E402

if __name__ == "__main__":
    sys.exit(_impl.main(sys.argv))
