#!/usr/bin/env python3
"""compare_digests.py - milestone 7 (win32_pilot.md sec 7/8, E).

Compares two --digest-out files line by line ("T <sha256> esp=.. ebp=.. ...",
one line per consumed game tick, written by carrier/src/det.cpp's safepoint
sensor at VA 0x4124f4). Prints EQUAL if every tick's sha256 matches, or the
first differing tick (and, if useful, the first differing register) if not.

Usage:
    python compare_digests.py FILE_A FILE_B
Exit code: 0 if EQUAL, 1 if they differ, 2 on a usage/parse error.
"""
import sys


def parse(path):
    """Yields (tick, sha256, regs_dict) for each line."""
    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) < 2:
                raise ValueError(f"{path}:{lineno}: malformed line: {line!r}")
            tick = int(parts[0])
            sha = parts[1]
            regs = {}
            for kv in parts[2:]:
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    regs[k] = v
            yield tick, sha, regs


def main(argv):
    if len(argv) != 3:
        print(f"usage: {argv[0]} FILE_A FILE_B", file=sys.stderr)
        return 2

    path_a, path_b = argv[1], argv[2]
    try:
        rows_a = list(parse(path_a))
        rows_b = list(parse(path_b))
    except (OSError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    n = min(len(rows_a), len(rows_b))
    first_diff = None
    for i in range(n):
        ta, sha_a, regs_a = rows_a[i]
        tb, sha_b, regs_b = rows_b[i]
        if ta != tb:
            print(f"tick-index mismatch at line {i + 1}: {path_a} has T={ta}, {path_b} has T={tb}")
            return 1
        if sha_a != sha_b:
            first_diff = (ta, sha_a, sha_b, regs_a, regs_b)
            break

    if first_diff is not None:
        ta, sha_a, sha_b, regs_a, regs_b = first_diff
        print(f"FIRST DIFFERENCE at tick T={ta}")
        print(f"  {path_a}: {sha_a}  {regs_a}")
        print(f"  {path_b}: {sha_b}  {regs_b}")
        reg_diffs = [k for k in regs_a if regs_a.get(k) != regs_b.get(k)]
        if reg_diffs:
            print(f"  differing registers: {', '.join(reg_diffs)}")
        else:
            print("  registers match at this tick; the .data/.bss memory digest differs")
        return 1

    if len(rows_a) != len(rows_b):
        longer, shorter = (path_a, path_b) if len(rows_a) > len(rows_b) else (path_b, path_a)
        last_common_t = rows_a[n - 1][0] if n > 0 else "(none)"
        print(f"EQUAL for the first {n} common ticks (last common T={last_common_t}), "
              f"but {longer} has {abs(len(rows_a) - len(rows_b))} more line(s) than {shorter} "
              f"(one run consumed more ticks before stopping - not a content mismatch).")
        return 1

    print(f"EQUAL ({n} ticks, {path_a} vs {path_b})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
