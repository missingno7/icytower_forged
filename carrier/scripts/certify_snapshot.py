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
"""
import argparse
import sys


def read_rows(path):
    """[(tick, sha256, raw_line)] in file order."""
    rows = []
    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            s = line.strip()
            if not s:
                continue
            parts = s.split()
            if len(parts) < 2:
                raise ValueError(f"{path}:{lineno}: malformed digest line: {s!r}")
            rows.append((int(parts[0]), parts[1], s))
    if not rows:
        raise ValueError(f"{path}: no digest rows")
    return rows


def index_of_tick(rows, tick, path):
    for i, (t, _, _) in enumerate(rows):
        if t == tick:
            return i
    raise ValueError(f"{path}: no row for anchor tick T={tick} "
                     f"(stream covers T={rows[0][0]}..{rows[-1][0]})")


def split_at_rewind(rows, path):
    """Index of the first row whose tick is LOWER than its predecessor's."""
    for i in range(1, len(rows)):
        if rows[i][0] < rows[i - 1][0]:
            return i
    raise ValueError(f"{path}: no rewind found (no tick ever goes backwards) - "
                     f"was --restore-at-tick given?")


def compare(a, b, label_a, label_b):
    """Row-by-row over the common prefix length. Returns 0 or 1."""
    n = min(len(a), len(b))
    for i in range(n):
        ta, sa, _ = a[i]
        tb, sb, _ = b[i]
        if ta != tb:
            print(f"FIRST DIFFERENCE at row {i + 1}: {label_a} has T={ta}, {label_b} has T={tb} "
                  f"(the two streams do not even agree on which tick comes next)")
            return 1
        if sa != sb:
            print(f"FIRST DIFFERENCE at tick T={ta} (row {i + 1})")
            print(f"  {label_a}: {sa}")
            print(f"  {label_b}: {sb}")
            return 1
    print(f"EQUAL ({n} rows, ticks T={a[0][0]}..{a[n - 1][0]}, {label_a} vs {label_b})")
    return 0


def cmd_rewind(args):
    rows = read_rows(args.file)
    cut = split_at_rewind(rows, args.file)
    before, after = rows[:cut], rows[cut:]
    if after[0][0] != args.anchor:
        print(f"error: the rewind lands on T={after[0][0]}, not the given --anchor {args.anchor}",
              file=sys.stderr)
        return 2
    pre = before[index_of_tick(before, args.anchor, args.file):]
    print(f"in-run rewind: {len(before)} rows before the rewind "
          f"(T={before[0][0]}..{before[-1][0]}), {len(after)} rows after "
          f"(T={after[0][0]}..{after[-1][0]}); anchor T={args.anchor}")
    rc = compare(pre, after, f"{args.file}[pre-rewind]", f"{args.file}[post-rewind]")
    if rc == 0 and args.cold:
        cold = read_rows(args.cold)
        rc = compare(cold[index_of_tick(cold, args.anchor, args.cold):], after,
                     f"{args.cold}[cold from anchor]", f"{args.file}[post-rewind]")
    return rc


def cmd_restore(args):
    cold = read_rows(args.cold)
    restored = read_rows(args.restore)
    if restored[0][0] != args.anchor:
        print(f"warning: the restored stream starts at T={restored[0][0]}, not --anchor "
              f"{args.anchor}", file=sys.stderr)
    return compare(cold[index_of_tick(cold, args.anchor, args.cold):], restored,
                   f"{args.cold}[cold from anchor]", args.restore)


def read_fn_rows(path):
    """[(k, dict)] in file order, from a --fn-digest-out stream."""
    rows = []
    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            rec = {}
            for tok in s.split():
                if "=" not in tok:
                    raise ValueError(f"{path}:{lineno}: token without '=': {tok!r}")
                k, v = tok.split("=", 1)
                rec[k] = v
            if "k" not in rec:
                raise ValueError(f"{path}:{lineno}: no k= field: {s!r}")
            rows.append((int(rec["k"]), rec))
    if not rows:
        raise ValueError(f"{path}: no invocation records (was the function reached at all?)")
    return rows


def compare_fn(a, b, label_a, label_b):
    # Same exclusions as compare_fn_digests.py: `form` is what a three-form
    # comparison is FOR, and `raweax` is informational for void functions.
    skip = ("form", "raweax")
    n = min(len(a), len(b))
    for i in range(n):
        ka, ra = a[i]
        kb, rb = b[i]
        if ka != kb:
            print(f"FIRST DIFFERENCE at record {i + 1}: {label_a} has k={ka}, {label_b} has k={kb}")
            return 1
        for field in sorted(set(ra) | set(rb)):
            if field in skip:
                continue
            if ra.get(field) != rb.get(field):
                print(f"FIRST DIFFERENCE fn={ra.get('fn')} k={ka} T={ra.get('T')} field={field}")
                print(f"  {label_a}: {ra.get(field)}")
                print(f"  {label_b}: {rb.get(field)}")
                return 1
    print(f"EQUAL ({n} invocations, k={a[0][0]}..{a[n - 1][0]}, {label_a} vs {label_b})")
    return 0


def cmd_fn(args):
    rows = read_fn_rows(args.file)
    cut = None
    for i in range(1, len(rows)):
        if rows[i][0] < rows[i - 1][0]:
            cut = i
            break
    if cut is None:
        print(f"error: {args.file}: no rewind found (the invocation index k never goes "
              f"backwards) - was --restore-at-tick given?", file=sys.stderr)
        return 2
    before, after = rows[:cut], rows[cut:]
    anchor_k = after[0][0]
    pre = [r for r in before if r[0] >= anchor_k]
    print(f"in-run rewind: {len(before)} records before the rewind, {len(after)} after; "
          f"the rewind restored the sensor to k={anchor_k}")
    return compare_fn(pre, after, f"{args.file}[pre-rewind]", f"{args.file}[post-rewind]")


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("rewind", help="certify an in-run rewind digest stream")
    r.add_argument("file")
    r.add_argument("--anchor", type=int, required=True)
    r.add_argument("--cold", help="an independent cold run's digest stream")
    r.set_defaults(fn=cmd_rewind)

    s = sub.add_parser("restore", help="certify a cold run against a --restore-from run")
    s.add_argument("--cold", required=True)
    s.add_argument("--restore", required=True)
    s.add_argument("--anchor", type=int, required=True)
    s.set_defaults(fn=cmd_restore)

    g = sub.add_parser("fn", help="certify an in-run rewind's per-invocation sensor stream")
    g.add_argument("file")
    g.set_defaults(fn=cmd_fn)

    args = p.parse_args(argv[1:])
    try:
        return args.fn(args)
    except (OSError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
