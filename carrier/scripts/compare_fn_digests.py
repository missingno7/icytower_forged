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
"""
import sys

FIELDS = ("fn", "k", "T", "args", "pre", "post", "eax", "form")


def parse(path):
    """Returns a list of dicts, one per invocation record."""
    rows = []
    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rec = {}
            for tok in line.split():
                if "=" not in tok:
                    raise ValueError(f"{path}:{lineno}: token without '=': {tok!r}")
                k, v = tok.split("=", 1)
                rec[k] = v
            missing = [k for k in FIELDS if k not in rec]
            if missing:
                raise ValueError(f"{path}:{lineno}: missing field(s) {missing}: {line!r}")
            rec["k"] = int(rec["k"])
            rec["T"] = int(rec["T"])
            rows.append(rec)
    return rows


def main(argv):
    if len(argv) != 3:
        print(f"usage: {argv[0]} FILE_A FILE_B", file=sys.stderr)
        return 2

    path_a, path_b = argv[1], argv[2]
    try:
        a = parse(path_a)
        b = parse(path_b)
    except (OSError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    if not a or not b:
        print(f"error: no invocation records in "
              f"{path_a if not a else path_b} (the sensor produced nothing - "
              f"was the function reached at all?)", file=sys.stderr)
        return 2

    forms_a = sorted({r["form"] for r in a})
    forms_b = sorted({r["form"] for r in b})

    n = min(len(a), len(b))
    for i in range(n):
        ra, rb = a[i], b[i]
        if ra["fn"] != rb["fn"] or ra["k"] != rb["k"]:
            print(f"RECORD-INDEX MISMATCH at line {i + 1}: "
                  f"{path_a} has fn={ra['fn']} k={ra['k']}, "
                  f"{path_b} has fn={rb['fn']} k={rb['k']} "
                  f"(the two runs did not even make the same calls)")
            return 1
        for field in ("args", "pre", "post", "eax"):
            if ra[field] != rb[field]:
                print(f"FIRST DIFFERENCE fn={ra['fn']} k={ra['k']} T={ra['T']} field={field}")
                print(f"  {path_a}: {field}={ra[field]}  (form={ra['form']}, T={ra['T']})")
                print(f"  {path_b}: {field}={rb[field]}  (form={rb['form']}, T={rb['T']})")
                if field in ("args", "pre"):
                    print(f"  NOTE: a '{field}' mismatch means the two runs had ALREADY "
                          f"diverged BEFORE this call - the state {ra['fn']} was entered "
                          f"with differs. This does not by itself convict {ra['fn']}'s "
                          f"{rb['form']} form; look for an earlier record (or an earlier "
                          f"per-tick global digest difference, compare_digests.py) that "
                          f"caused it.")
                else:
                    print(f"  This IS a verdict on {ra['fn']}: identical arguments and "
                          f"identical pre-state, different result.")
                return 1

    if len(a) != len(b):
        longer, shorter = (path_a, path_b) if len(a) > len(b) else (path_b, path_a)
        print(f"EQUAL for the first {n} common invocations (last common "
              f"fn={a[n - 1]['fn']} k={a[n - 1]['k']} T={a[n - 1]['T']}), but {longer} has "
              f"{abs(len(a) - len(b))} more record(s) than {shorter} - one run made more "
              f"calls before stopping, which is itself a difference.")
        return 1

    print(f"EQUAL ({n} invocations, {path_a} [{'+'.join(forms_a)}] vs "
          f"{path_b} [{'+'.join(forms_b)}])")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
