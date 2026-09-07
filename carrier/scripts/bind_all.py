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
"""
import argparse, json, os, re, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
CARRIER_DIR = os.path.dirname(HERE)
ROOT = os.path.dirname(CARRIER_DIR)

# The 35 src/icytower functions (src/icytower/PROMOTIONS.md, batches 1-4).
# Order matches PROMOTIONS.md's own table order.
ALL_FUNCTIONS = [
    "update_frame", "is_solid",
    "jump_player", "getFloorData", "reset_map", "add_combo", "line_intersect",
    "get_gamepad", "is_up", "is_down", "is_left", "is_right", "is_fire",
    "is_pause", "is_enter", "is_any",
    "set_control", "init_control", "check_control_key", "get_level",
    "add_jump_sequence", "reset_particles", "scroll_scroller",
    "restart_scroller", "cycle_counter", "fps_counter", "get_demo",
    "get_controls", "switchedFromProgram", "switchedToProgram",
    "clickedCloseButton",
    "new_rand", "update_particle", "create_particle", "ok_to_play",
]

# Functions whose src/ form has real x87 floating point and was built with
# the mingw32 GCC -mfpmath=387 -mno-sse2 mixed-toolchain path (build.cmd),
# per the task brief's explicit list ("particles" = update_particle +
# create_particle).
X87_FUNCTIONS = {"jump_player", "line_intersect", "new_rand",
                  "update_particle", "create_particle"}


def restore_assets():
    # Wait for any carrier.exe process tree to fully exit (restore_assets.ps1's
    # own documented race: it can still be tearing down / rewriting
    # assets/profiles/ right after a run).
    for _ in range(100):
        r = subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             "if (Get-Process carrier -ErrorAction SilentlyContinue) { 'Y' } else { 'N' }"],
            capture_output=True, text=True)
        if r.stdout.strip() != "Y":
            break
        time.sleep(0.1)
    r = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                         os.path.join(HERE, "restore_assets.ps1")],
                        capture_output=True, text=True, cwd=CARRIER_DIR)
    if r.returncode != 0:
        print("bind_all: WARNING - restore_assets.ps1 exited %d\n%s\n%s"
              % (r.returncode, r.stdout, r.stderr), file=sys.stderr)


def run_carrier(args_list, out_prefix, log_dir):
    exe = os.path.join(CARRIER_DIR, "carrier.exe")
    cmd = [exe] + args_list
    log_path = os.path.join(log_dir, out_prefix + "_stderr.txt")
    with open(log_path, "w") as logf:
        r = subprocess.run(cmd, cwd=CARRIER_DIR, stdout=logf, stderr=subprocess.STDOUT)
    return r.returncode, log_path


def count_invocations(fn_digest_path):
    if not os.path.exists(fn_digest_path):
        return 0
    n = 0
    with open(fn_digest_path) as f:
        for line in f:
            if line.strip():
                n += 1
    return n


def compare_fn(path_a, path_b):
    r = subprocess.run([sys.executable, os.path.join(HERE, "compare_fn_digests.py"),
                         path_a, path_b], capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def compare_ticks(path_a, path_b):
    r = subprocess.run([sys.executable, os.path.join(HERE, "compare_digests.py"),
                         path_a, path_b], capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fn", default=None,
                     help="comma-separated function name(s); default: all 35")
    ap.add_argument("--out-dir", default=os.path.join(ROOT, "artifacts_ms12"))
    ap.add_argument("--input-script", default=os.path.join("..", "replays", "human_test.txt"))
    ap.add_argument("--baseline", default=os.path.join(ROOT, "replays", "human_test.digest"))
    ap.add_argument("--stop-at-tick", default="2528")
    ap.add_argument("--run-seconds", default="180")
    args = ap.parse_args()

    fns = args.fn.split(",") if args.fn else ALL_FUNCTIONS
    os.makedirs(args.out_dir, exist_ok=True)

    common = ["--det", "--pace=fast", "--input=script",
              "--input-script", args.input_script,
              "--stop-at-tick", args.stop_at_tick,
              "--run-seconds", args.run_seconds]

    results = []
    for fn in fns:
        print("=== %s ===" % fn, flush=True)
        a_path = os.path.join(args.out_dir, "%s_A_original.txt" % fn)
        b_path = os.path.join(args.out_dir, "%s_B_src.txt" % fn)
        b_ticks = os.path.join(args.out_dir, "%s_B_ticks.txt" % fn)
        b_report = os.path.join(args.out_dir, "%s_B_report.json" % fn)

        restore_assets()
        rc_a, log_a = run_carrier(
            common + ["--bind", "%s=original" % fn, "--fn-digest-out", a_path],
            "%s_A" % fn, args.out_dir)

        restore_assets()
        rc_b, log_b = run_carrier(
            common + ["--bind", "%s=src" % fn, "--fn-digest-out", b_path,
                      "--digest-out", b_ticks, "--report", b_report],
            "%s_B" % fn, args.out_dir)

        n_a = count_invocations(a_path)
        n_b = count_invocations(b_path)

        row = {"fn": fn, "rc_a": rc_a, "rc_b": rc_b, "n_a": n_a, "n_b": n_b,
               "x87": fn in X87_FUNCTIONS}

        if rc_a != 0 or rc_b != 0:
            row["verdict"] = "RUN FAILED (rc_a=%d rc_b=%d - see %s / %s)" % (rc_a, rc_b, log_a, log_b)
        elif n_a == 0 and n_b == 0:
            row["verdict"] = "UNVERIFIED IN VIVO (0 invocations - workload never reaches it)"
        else:
            rc_cmp, out_cmp = compare_fn(a_path, b_path)
            row["fn_compare"] = out_cmp
            rc_tk, out_tk = compare_ticks(b_ticks, args.baseline)
            row["tick_compare"] = out_tk
            if rc_cmp == 0 and rc_tk == 0:
                row["verdict"] = "EQUAL (%d invocations)" % n_a
            else:
                row["verdict"] = "DIFFER: %s | ticks: %s" % (out_cmp, out_tk)

        print("    %s" % row["verdict"], flush=True)
        results.append(row)

    summary_path = os.path.join(args.out_dir, "bind_all_summary.json")
    with open(summary_path, "w") as f:
        json.dump(results, f, indent=2)
    print("\nWrote %s" % summary_path)

    n_equal = sum(1 for r in results if r["verdict"].startswith("EQUAL"))
    n_differ = sum(1 for r in results if r["verdict"].startswith("DIFFER"))
    n_unverified = sum(1 for r in results if r["verdict"].startswith("UNVERIFIED"))
    n_failed = sum(1 for r in results if r["verdict"].startswith("RUN FAILED"))
    print("EQUAL=%d DIFFER=%d UNVERIFIED=%d RUN_FAILED=%d (of %d)" %
          (n_equal, n_differ, n_unverified, n_failed, len(results)))
    return 0 if n_differ == 0 and n_failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
