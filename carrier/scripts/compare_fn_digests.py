#!/usr/bin/env python3
"""compare_fn_digests.py - milestones 11-12 (win32_pilot.md sec 3/7/8a).

Compares two --fn-digest-out files: the per-invocation sensor records written
by carrier/src/bind.cpp. One line per invocation, identical in shape for all
three forms of a function (ORIGINAL sensed with hardware breakpoints, LIFTED
and NATIVE sensed by the binding stub):

    fn=<name> k=<n> T=<tick> args=<a0,a1,..> pre=<sha256> post=<sha256> \
        eax=<hex> form=<original|lifted|native>

`form` is deliberately NOT compared - comparing two forms is the whole point.
`raweax` is deliberately NOT compared either: it only appears for a function
whose it_funcs.h prototype returns void, where `eax` reads `void` on every
form and the raw register value is recorded purely as evidence (see
carrier/NOTES.md "Milestones 11-12" for the measured case that motivated it).
Everything else must match invocation for invocation:

    args   the caller's own arguments
    pre    digest of the function's comparison domain BEFORE the call
    post   digest of the same domain AFTER the call
    eax    the return value

A `pre` (or `args`) mismatch is reported distinctly from a `post`/`eax`
mismatch, because it means something different: the two runs had already
diverged UPSTREAM of this call, so the function itself is not (yet) implicated
- exactly the distinction notes/promotion_candidates.md sec 5 asks for.

Usage:
    python compare_fn_digests.py FILE_A FILE_B
Exit code: 0 if EQUAL, 1 if they differ, 2 on a usage/parse error.

Thin delegator (notes/extraction_plan.md S3): the comparator itself now lives
in the port_forge submodule as tools/pf_win32_compare_fn_digests.py (no
project literal in it -- this file has no policy to inject, since
compare_fn_digests.py's CLI never carried one). This wrapper exists only so
callers (gates.ps1, bind_all.py, habit) keep working with an unchanged CLI
while the mechanism is shared framework code. See carrier/win32_policy.json.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_compare_fn_digests as _impl  # noqa: E402

if __name__ == "__main__":
    sys.exit(_impl.main(sys.argv))
