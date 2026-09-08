#!/usr/bin/env python3
"""batch13_check.py -- offline oracle for PROMOTIONS.md batch 13's three
memory/call-trace-domain functions: get_version_str (0x406960),
syncProfileFromOptions (0x406a14), destroy_game_data (0x40418c).

Not a lift_check.py SPECS entry, for three different reasons per function
(the same "additive, standalone oracle on the same engine" convention as
draw_frame_xcheck.py/play_xcheck.py, not a new SPECS row):

  - get_version_str's comparison domain is the NUL-terminated byte
    content at the returned pointer, not the pointer VALUE (a fresh
    string literal in the compiled candidate is never at the same
    address as the original's .rdata literal) -- the generic engine's
    "domain" is a fixed list of (va, len) memory regions plus an EAX
    scalar; it has no "read a C string starting here" primitive.
  - syncProfileFromOptions's own VA (0x406a14) is a real out-of-line
    copy, but it takes no arguments and its only comparison domain is
    four struct fields off a runtime pointer (`profile`) -- doable as a
    plain SPECS entry, but included here to keep this batch's three
    small, related checks in one file rather than three.
  - destroy_game_data's only observable effect is CALLING free() with a
    pointer -- Mechanism B (the call-trace domain pf_win32_offline_oracle
    already provides for play_sound/poll_joystick) applies directly: hook
    free()'s IAT-thunk entry VA, capture the one argument, never actually
    execute the thunk (so no real heap state is touched, matching how
    play_sound's own trace hook never executes play_sample either).

Usage:
  python batch13_check.py --vectors 20000 --seed 20260908
  python batch13_check.py --fault
"""
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(PROJ, "port_forge", "tools"))
import pf_win32_offline_oracle as pf  # noqa: E402

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")
CHECK_EXE = os.path.join(HERE, "batch13_check.exe")

GET_VERSION_STR_VA = 0x406960
SYNC_PROFILE_VA = 0x406a14
DESTROY_GAME_DATA_VA = 0x40418c
FREE_THUNK_VA = 0x4bad08

G_OPTIONS_FLASH = 0x4fe528
G_OPTIONS_JUMP_HOLD = 0x4fe530
G_OPTIONS_MSC_VOLUME = 0x4fe54c
G_OPTIONS_SND_VOLUME = 0x4fe550
G_PROFILE = 0x4dd27c

PROFILE_VA = 0x7e0000          # scratch Tprofile, 0x550 bytes (see off.c probe)
SZ_PROFILE = 0x550
PROFILE_FLASH_OFF = 0x4dc
PROFILE_JUMP_HOLD_OFF = 0x4e0
PROFILE_MSC_VOLUME_OFF = 0x528
PROFILE_SND_VOLUME_OFF = 0x52c

FREE_SLOT_VA = 0x7e1000         # {call_count, arg0} -- mechanism B shape
_CT_FREE = {"va": FREE_THUNK_VA, "argc": 1, "slot": FREE_SLOT_VA}


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def si32(v):
    return struct.pack("<i", ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000)


def run_candidate(*args):
    out = subprocess.check_output([CHECK_EXE] + list(args))
    return out.decode("ascii", "replace").strip()


# --------------------------------------------------------------------- (1)
def check_get_version_str(oracle, fault=False):
    eax, _ = oracle.call(GET_VERSION_STR_VA, [], [], [])
    off = eax - pf.GUEST_BASE
    end = oracle.guest.index(b"\x00", off)
    original = oracle.guest[off:end].decode("latin1")
    candidate = run_candidate("get_version_str")
    if fault:
        # Negative control, the same shape as every other --fault in this
        # project: flip one bit of the CANDIDATE result after it comes back
        # from the compiled side, and require the comparator to name it.
        candidate = candidate[:-1] + chr(ord(candidate[-1]) ^ 0x01)
    ok = (original == candidate)
    print("get_version_str: original=%r candidate=%r -> %s"
          % (original, candidate, "EQUAL" if ok else "DIFFER"))
    return ok


# --------------------------------------------------------------------- (2)
def check_sync_profile(oracle, rng, nvec, fault):
    fails = 0
    for k in range(nvec):
        flash = rng.getrandbits(32)
        jump_hold = rng.getrandbits(32)
        msc_volume = rng.getrandbits(32)
        snd_volume = rng.getrandbits(32)
        marker = bytes(rng.getrandbits(8) for _ in range(SZ_PROFILE))

        writes = [
            (G_OPTIONS_FLASH, si32(flash)),
            (G_OPTIONS_JUMP_HOLD, si32(jump_hold)),
            (G_OPTIONS_MSC_VOLUME, si32(msc_volume)),
            (G_OPTIONS_SND_VOLUME, si32(snd_volume)),
            (G_PROFILE, u32(PROFILE_VA)),
            (PROFILE_VA, marker),
        ]
        domain = [(PROFILE_VA, SZ_PROFILE)]
        _, original = oracle.call(SYNC_PROFILE_VA, [], writes, domain)

        cand_out = run_candidate(
            "sync_profile", str(flash & 0xFFFFFFFF), str(jump_hold & 0xFFFFFFFF),
            str(msc_volume & 0xFFFFFFFF), str(snd_volume & 0xFFFFFFFF),
            marker.hex())
        cand_bytes = bytes.fromhex(cand_out)
        if fault and k == 5:
            cand_bytes = bytearray(cand_bytes)
            cand_bytes[PROFILE_FLASH_OFF] ^= 0xFF
            cand_bytes = bytes(cand_bytes)

        if cand_bytes != original:
            fails += 1
            if fails <= 3:
                for i in range(len(original)):
                    if original[i] != cand_bytes[i]:
                        print("  DIFFER at vector %d: profile+0x%x (VA 0x%x) "
                              "original 0x%02x candidate 0x%02x"
                              % (k, i, PROFILE_VA + i, original[i], cand_bytes[i]))
                        break
    print("sync_profile: %d of %d vectors differ" % (fails, nvec))
    return fails


# --------------------------------------------------------------------- (3)
def check_destroy_game_data(oracle, rng, nvec, fault):
    fails = 0
    for k in range(nvec):
        gd_ptr = rng.getrandbits(32)
        oracle.mu.mem_write(FREE_SLOT_VA, u32(0) + u32(0))
        eax, _ = oracle.call(DESTROY_GAME_DATA_VA, [gd_ptr], [], [])
        count, arg0 = struct.unpack("<II", bytes(oracle.mu.mem_read(FREE_SLOT_VA, 8)))

        cand_out = run_candidate("destroy_game_data", "%08x" % gd_ptr)
        # "free_called=<n> arg=<hex>"
        parts = dict(p.split("=") for p in cand_out.split())
        cand_count = int(parts["free_called"])
        cand_arg = int(parts["arg"], 16)
        if fault and k == 5:
            cand_arg ^= 0xFF

        if (count, arg0) != (cand_count, cand_arg):
            fails += 1
            if fails <= 3:
                print("  DIFFER at vector %d: free()'s arg -- original 0x%08x "
                      "(called %d) candidate 0x%08x (called %d)"
                      % (k, arg0, count, cand_arg, cand_count))
    print("destroy_game_data: %d of %d vectors differ" % (fails, nvec))
    return fails


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--vectors", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--fault", action="store_true",
                     help="negative control: corrupt one candidate result "
                          "at vector 5 and confirm the comparator reports it")
    args = ap.parse_args()

    guest = pf.build_guest(IMAGE)
    oracle = pf.Oracle(guest, call_traces=[_CT_FREE])

    rng = random.Random(args.seed)
    ok = check_get_version_str(oracle, fault=args.fault)
    if args.fault and ok:
        print("NEGATIVE CONTROL FAILED: get_version_str fault not detected")
        sys.exit(1)
    fails2 = check_sync_profile(oracle, rng, args.vectors if not args.fault else 200, args.fault)
    fails3 = check_destroy_game_data(oracle, rng, args.vectors if not args.fault else 200, args.fault)

    if not args.fault and (not ok or fails2 or fails3):
        sys.exit(1)
    if args.fault and (fails2 == 0 or fails3 == 0):
        print("NEGATIVE CONTROL FAILED: fault injection was not detected")
        sys.exit(1)


if __name__ == "__main__":
    main()
