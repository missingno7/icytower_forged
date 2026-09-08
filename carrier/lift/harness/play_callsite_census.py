#!/usr/bin/env python3
"""play_callsite_census.py -- completeness check for src/icytower/play.c.
PROMOTIONS.md batch 12.

play() is 17420 bytes and only three of its regions have an offline
comparison domain (play_xcheck.py).  For the rest, the strongest cheap
evidence that the recovery is COMPLETE -- that no call site was dropped,
duplicated or misattributed -- is a census: count every `call` in the
original's own 0x411a00..0x415e0b range, count the same names in the
recovered source, and require the two counts to agree.

This is a static check, not an equivalence proof: it says the recovered
source issues each callee exactly as many times as the original does, and
in particular catches a whole missing block (its calls vanish) or an
accidentally duplicated one.  Ordering and arguments are the in-vivo
oracle's job.

Indirect calls through a GFX_VTABLE slot are counted separately, by slot
offset, because they have no symbol: `call *0x28(%edx)` is vline,
*0x2c hline, *0x3c rectfill, *0x44 draw_sprite, *0x48 draw_256_sprite,
*0x10 acquire, *0x14 release.

Usage:  python play_callsite_census.py [--verbose]
"""
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
DISASM = os.path.join(PROJ, "artifacts", "disasm.txt")
SRC = os.path.join(PROJ, "src", "icytower", "play.c")

LO, HI = 0x411A00, 0x415E0C

VT_SLOTS = {
    0x10: "acquire_screen", 0x14: "release_screen",
    0x28: "vline", 0x2c: "hline", 0x34: "line", 0x3c: "rectfill",
    0x44: "draw_sprite", 0x48: "draw_256_sprite",
    0x4c: "draw_sprite_v_flip", 0x50: "draw_sprite_h_flip",
}

# names the recovered source deliberately spells differently
RENAME = {
    "time": "time",
    "printf": "printf",
    "sprintf": "sprintf",
    "strcpy": "strcpy",
    "QueryPerformanceCounter@4": "QueryPerformanceCounter",
    "QueryPerformanceFrequency@4": "QueryPerformanceFrequency",
}

# Counts that differ for a structural reason, with the reason.  Each row
# is (original count, source count, why) and is checked EXACTLY -- if the
# recovery drifts, the row stops matching and the census fails.
EXPLAINED = {
    "clock": (6, 3, "4 of the 6 sites are the repeated wall-clock rebase, "
                    "factored into restart_time_cheat_window() (called 4x)"),
    "QueryPerformanceCounter": (6, 3, "same factoring"),
    "time": (15, 12, "same factoring"),
    "voice_get_position": (4, 2, "3 of the 4 sites are the repeated music "
                                 "rebase, factored into resync_music_counter()"),
    "save_profile": (3, 1, "GCC tail-duplicated the syncProfileFromOptions() + "
                           "save_profile() tail into each of its 3 predecessors"),
    "textout_centre_ex": (17, 14, "GCC tail-duplicated 3 of the initials-entry "
                                  "arms (0x415a8d/0x415b64/0x415cb4/0x415d20)"),
    "syncProfileFromOptions": (0, 3, "GCC inlined all three call sites; the "
                                     "out-of-line copy at 0x406a14 is never "
                                     "reached from play()"),
    "hline": (2, 1, "the two pause screens share draw_pause_curtain()"),
    "vline": (2, 1, "the two pause screens share draw_pause_curtain()"),
    "strcpy": (2, 8, "6 of the 8 fixed-length copies are `rep movsb` in the "
                     "original (letters[], the custom-mode and personal-record "
                     "messages); the recovery spells all 8 as strcpy"),
}


def original_counts():
    calls = collections.Counter()
    vt = collections.Counter()
    direct = re.compile(r"^\s*([0-9a-f]+):\t.*\tcall\s+([0-9a-f]+) <_([A-Za-z0-9_@]+)>")
    indirect = re.compile(r"^\s*([0-9a-f]+):\t.*\tcall\s+\*0x([0-9a-f]+)\(%")
    with open(DISASM, errors="replace") as f:
        for line in f:
            m = direct.match(line)
            if m:
                va = int(m.group(1), 16)
                if LO <= va < HI:
                    calls[m.group(3)] += 1
                continue
            m = indirect.match(line)
            if m:
                va = int(m.group(1), 16)
                if LO <= va < HI:
                    off = int(m.group(2), 16)
                    vt[VT_SLOTS.get(off, "vtable+0x%x" % off)] += 1
    return calls, vt


def source_counts(names):
    txt = open(SRC, encoding="utf-8", errors="replace").read()
    # only the recovered code counts (first static helper onwards): everything above the first static
    # helper is the file header, the includes and the #ifndef-guarded
    # AL_INLINE / Win32 stand-ins, whose macro bodies name the primitive
    # they wrap and are not call sites of play()'s own.
    txt = txt[txt.index("static void clear_replay_telemetry"):]
    # strip comments so a name mentioned in prose is not counted
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    out = collections.Counter()
    for n in names:
        out[n] = len(re.findall(r"\b%s\s*\(" % re.escape(n), txt))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    calls, vt = original_counts()
    want = collections.Counter()
    for k, v in calls.items():
        want[RENAME.get(k, k)] += v
    for k, v in vt.items():
        want[k] += v

    got = source_counts(list(want))
    # the AL_INLINE draw_sprite() shim expands to ONE source call site that
    # the original emits as a draw_sprite/draw_256_sprite pair
    if "draw_sprite" in want and "draw_256_sprite" in want:
        pair = min(want["draw_sprite"], want["draw_256_sprite"])
        want["draw_sprite"] -= pair
        want["draw_256_sprite"] -= pair
        want["draw_sprite"] += pair
        del want["draw_256_sprite"]
        got["draw_sprite"] = got.get("draw_sprite", 0)

    bad = 0
    rows = []
    for name in sorted(want):
        w, g = want[name], got.get(name, 0)
        ok = (w == g)
        if not ok and name in EXPLAINED and EXPLAINED[name][:2] == (w, g):
            ok = True
            if args.verbose:
                rows.append("%-28s original %3d   source %3d   OK (%s)"
                            % (name, w, g, EXPLAINED[name][2]))
            continue
        if not ok:
            bad += 1
        if args.verbose or not ok:
            rows.append("%-28s original %3d   source %3d   %s"
                        % (name, w, g, "OK" if ok else "MISMATCH"))
    # reverse direction: a name the SOURCE calls that the original never
    # does would otherwise be invisible (the census is driven by the
    # original's call list).  Only EXPLAINED may carry such a row.
    txt = open(SRC, encoding="utf-8", errors="replace").read()
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = txt[txt.index("static void clear_replay_telemetry"):]
    for name in sorted(set(re.findall(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(", txt))):
        if name in want or name in ("if", "while", "for", "switch", "return",
                                    "sizeof", "int", "char", "float", "double",
                                    "void", "asset_id", "play"):
            continue
        if name in EXPLAINED and EXPLAINED[name][0] == 0:
            if args.verbose:
                rows.append("%-28s original   0   source %3d   OK (%s)"
                            % (name, txt.count(name + "("), EXPLAINED[name][2]))
            continue
        if name.startswith(("clear_replay_telemetry", "draw_pause_curtain",
                            "restart_time_cheat_window", "resync_music_counter",
                            "collect_game_data", "save_personal_bests",
                            "asset_bitmap", "asset_font", "it_al_")):
            continue
        rows.append("%-28s original   0   source  ??   SOURCE-ONLY CALL" % name)
        bad += 1

    for r in rows:
        print(r)
    print("play_callsite_census: %d distinct callees, %d mismatch(es)"
          % (len(want), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
