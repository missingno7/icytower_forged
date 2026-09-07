#!/usr/bin/env python3
"""batch11_coverage.py -- branch-coverage census for the two batch-11
functions (poll_control 0x401958, handle_player_input 0x40b3e4).

"EQUAL over N vectors" only means something if the vectors actually reach
the branches.  This script answers that directly rather than by argument:
it drives the SAME generators lift_check.py uses (icytower_specs.py's
SPECS[name]["gen"]), runs the ORIGINAL bytes in unicorn exactly the way the
engine's Oracle does, and counts how many vectors execute each interesting
basic-block address -- read off artifacts/disasm.txt, one per recovered
branch, listed in ADDRS below.

It is a measurement tool, not part of the equivalence check; nothing here
touches src/.

  python carrier/lift/harness/batch11_coverage.py --vectors 3000 --seed 20260908
"""
import argparse
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(PROJ, "port_forge", "tools"))
import pf_win32_offline_oracle as pf          # noqa: E402
import icytower_specs as spec                 # noqa: E402
from unicorn import UC_HOOK_CODE              # noqa: E402

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")

# name -> {label: VA}.  Every VA is the first instruction of the block that
# arm reaches, taken from artifacts/disasm.txt.
ADDRS = {
    "poll_control": {
        "joystick arm (use_joy != 0)":      0x4019f4,
        "  axis1.d1 -> gamepad.up":         0x401a02,
        "  axis1.d2 -> gamepad.down":       0x401a14,
        "  axis0.d1 -> gamepad.left":       0x401a26,
        "  axis0.d2 -> gamepad.right":      0x401a74,
        "  button loop body":               0x401a55,
        "  button loop b == 32 test":       0x401a4c,
        "keyboard arm (joystick_only==0)":  0x401977,
        "  key_up set":                     0x401984,
        "  key_down set":                   0x401995,
        "  key_left set":                   0x4019a6,
        "  key_right set":                  0x4019b7,
        "  key_fire set":                   0x4019c8,
        "  key_enter set":                  0x4019d9,
        "  key_pause set":                  0x4019ea,
        "epilogue (every path reaches it)": 0x4019ee,
    },
    "handle_player_input": {
        "epilogue (also the NULL-ctrl exit)": 0x40b4b7,
        "playback arm":                     0x40b406,
        "  rp in range":                    0x40b4c0,
        "  cycle_count > 0 (decrement)":    0x40b608,
        "  advance rec_pos":                0x40b4d9,
        "  rp past demo->size (flags=0)":   0x40b423,
        "recording arm":                    0x40b59c,
        "  0x80 sentinel written":          0x40b645,
        "  extend current run":             0x40b6b4,
        "  start a new record":             0x40b5ec,
        "steer: left held":                 0x40b437,
        "  left brake (sx > 0)":            0x40b455,
        "steer: right held":                0x40b4ff,
        "  right brake (sx < 0)":           0x40b519,
        "steer: neither (sx *= 0.9)":       0x40b614,
        "jump: rejump != 0 arm":            0x40b53c,
        "jump: rejump == 0 arm":            0x40b475,
        "  jump_player called (latch)":     0x40b668,
        "  jump_key latched to -1":         0x40b68c,
        "  jump_key cleared on release":    0x40b4a4,
        "  profile->total_jumps++":         0x40b591,
    },
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vectors", type=int, default=3000)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--funcs", default="poll_control,handle_player_input")
    args = ap.parse_args()

    guest = pf.build_guest(IMAGE)
    for name in args.funcs.split(","):
        s = spec.SPECS[name]
        oracle = pf.Oracle(guest, call_traces=s.get("call_traces"))
        hits = {k: 0 for k in ADDRS[name]}
        seen = set()

        def hook(uc, address, size, data):
            seen.add(address)

        for va in set(ADDRS[name].values()):
            oracle.mu.hook_add(UC_HOOK_CODE, hook, begin=va, end=va)

        rng = random.Random(args.seed)
        for k in range(args.vectors):
            oracle.restore([(va, n) for va, n in s["domain"]])
            call_args, writes = s["gen"](rng, k)
            seen = set()
            try:
                oracle.call(s["va"], call_args, writes, s["domain"])
            except Exception:
                continue
            for label, va in ADDRS[name].items():
                if va in seen:
                    hits[label] += 1

        print("%s -- %d vectors" % (name, args.vectors))
        for label in ADDRS[name]:
            flag = "  " if hits[label] else "**"
            print("  %s %-36s %6d" % (flag, label, hits[label]))
        print()


if __name__ == "__main__":
    main()
