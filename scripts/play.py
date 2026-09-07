#!/usr/bin/env python3
"""Play Icy Tower through the Win32 carrier (carrier/carrier.exe).

Self-contained rather than importing port_forge/scripts/player_runtime.py:
that module's declared-runtime contract requires a portforge.project.json
"player" capability declaration (default_runtime + oracle/generated/port
runtimes, each with an adapter, and - for ArtifactV2 replay - a reviewed
boundary_profile/implementation_plan) plus a game.json program identity.
Neither exists in this project (see the repo root: only game-recon JSON
files and win32_pilot.md, no portforge.project.json/game.json), and the
carrier's own execution model - a single native Win32 process that maps one
already-built guest EXE and drives it with its own --det/--input/--record-
input/--play-replay surface (carrier/NOTES.md, win32_pilot.md sec 5) - is a
different shape than player_runtime's DOS/Amiga/etc. interpreter runtimes
and ArtifactV2 replay format entirely. Forcing the contract on would mean
inventing a project manifest and a fake "adapter" with no real oracle/
generated/port distinction to declare. If a real player_runtime-shaped
contract is ever adopted for the carrier project, revisit this decision -
see carrier/NOTES.md "Input policy and recording".

Usage:
    python scripts/play.py
        Build carrier/carrier.exe if missing (carrier/build.cmd), verify
        assets/icytower15.exe's sha256 against the fingerprint pinned below,
        then run it interactively:
            --det --pace=real --input=real --interactive
        (deterministic virtual clock, real human keyboard/joystick, real
        time so a human can play at normal speed - see win32_pilot.md sec
        5a and carrier/NOTES.md "Input policy and recording").

        --interactive is what allows the guest window to be shown normally
        and brought to the foreground; WITHOUT it (every automated run,
        including --play-replay below) the window is created
        WS_EX_NOACTIVATE and shown SW_SHOWMINNOACTIVE, and the guest's own
        SetForegroundWindow calls are suppressed, so a carrier run can never
        interrupt whatever the operator is doing - carrier/NOTES.md
        "Environment isolation".

    python scripts/play.py --record-replay NAME
        Same, plus:
            --record-input replays/NAME.txt --digest-out replays/NAME.digest
        Play a session, close the window (or let --run-seconds elapse) when
        done. replays/ is created at the repo root if it doesn't exist yet.

    python scripts/play.py --play-replay NAME [--pace real|fast]
        Deterministically replays replays/NAME.txt:
            --det --pace=<fast|real> --input=script
            --input-script replays/NAME.txt
            --digest-out replays/NAME.replay.digest
        then compares the new digest stream against replays/NAME.digest with
        carrier/scripts/compare_digests.py and prints EQUAL or the first
        differing tick. --pace defaults to fast (instant replay for CI-style
        checking); pass --pace real to watch it play back at human speed.

    python scripts/play.py --no-det
        Runs the raw oracle: no --det, no --input (real keyboard, real
        time/RNG, exactly as the game would run standalone) - for
        diagnostic/comparison purposes only, never deterministic.

    --keep-state
        Skip restoring assets/tower.cfg, assets/profiles/, assets/log.txt
        from artifacts/assets_backup/ + artifacts/log_original_baseline.txt
        after the run (restored by default - see carrier/NOTES.md, every
        det-proof run in this project restores this mutable state first so
        each run starts from the same profile/config/hiscore baseline).

Every invocation prints the exact carrier.exe command line it runs.
"""

import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CARRIER_DIR = ROOT / "carrier"
CARRIER_EXE = CARRIER_DIR / "carrier.exe"
BUILD_CMD = CARRIER_DIR / "build.cmd"
IMAGE = ROOT / "assets" / "icytower15.exe"
REPLAYS_DIR = ROOT / "replays"
ASSETS_BACKUP = ROOT / "artifacts" / "assets_backup"
LOG_BASELINE = ROOT / "artifacts" / "log_original_baseline.txt"
COMPARE_DIGESTS = CARRIER_DIR / "scripts" / "compare_digests.py"

# Computed 2026-09-07 from the assets/icytower15.exe currently checked in
# (`python -c "import hashlib; print(hashlib.sha256(open('assets/icytower15.exe','rb').read()).hexdigest())"`).
# Also independently confirmed by --record-input's own "# image_sha256:"
# header line on a real run (det.cpp's sha256_file_hex).
FINGERPRINT_SHA256 = "7570c6b0c7cddf6180d7c421bdc7d7bc1486c47a6d62cc6fde90670f62d4388d"
FINGERPRINT_SIZE = 3753885

# carrier/NOTES.md "Headless, frame oracle, named globals, .itr workload"
# item 3: score/floor/combo are NOT their own top-level DWARF globals - they
# are fields of the per-player Tplayer struct (src/icytower/game_types.h),
# reached through the two globals that DO exist standalone: `player_id`
# (which player slot is active) and `ply` (Tplayer* ply[1000]). The carrier's
# --print-globals is generic (carrier/src/print_globals.cpp, carrier/gen/
# gen_print_globals.py) - it does not hard-code any of these names; this
# list is project-specific DATA passed on the command line, same as any
# other --print-globals caller would supply. `level` is the DWARF field name
# for the floor counter (Tplayer.level, offset 40); best_combo/latest_combo
# are the two combo-tracking fields DWARF actually names (there is no single
# field simply called "combo").
ICY_TOWER_GLOBALS = [
    "player_id",
    "ply[player_id]->score",
    "ply[player_id]->level",
    "ply[player_id]->best_combo",
    "ply[player_id]->latest_combo",
]


class PlayError(RuntimeError):
    pass


def verify_image() -> None:
    if not IMAGE.is_file():
        raise PlayError(f"missing {IMAGE}")
    size = IMAGE.stat().st_size
    if size != FINGERPRINT_SIZE:
        raise PlayError(
            f"{IMAGE.name}: size {size}; play.py pins {FINGERPRINT_SIZE} "
            "(assets/icytower15.exe does not match the expected build)"
        )
    got = hashlib.sha256(IMAGE.read_bytes()).hexdigest()
    if got != FINGERPRINT_SHA256:
        raise PlayError(
            f"{IMAGE.name}: sha256 {got}\n"
            f"play.py pins {FINGERPRINT_SHA256}\n"
            "this is a different executable than the one the carrier was "
            "built/proven against - update the fingerprint in scripts/play.py "
            "only if you deliberately mean to switch builds"
        )


def build_carrier_if_missing() -> None:
    if CARRIER_EXE.is_file():
        return
    if not BUILD_CMD.is_file():
        raise PlayError(f"missing {BUILD_CMD} and no prebuilt {CARRIER_EXE}")
    print(f"$ {BUILD_CMD}")
    result = subprocess.run(["cmd", "/c", str(BUILD_CMD)], cwd=CARRIER_DIR)
    if result.returncode != 0:
        raise PlayError(f"carrier/build.cmd failed (exit {result.returncode})")
    if not CARRIER_EXE.is_file():
        raise PlayError(f"build.cmd reported success but {CARRIER_EXE} is still missing")


def restore_assets(keep_state: bool) -> None:
    if keep_state:
        return
    if not ASSETS_BACKUP.is_dir() or not LOG_BASELINE.is_file():
        print(
            f"play.py: warning - {ASSETS_BACKUP} or {LOG_BASELINE} missing, "
            "cannot restore mutable asset state; pass --keep-state to silence this",
            file=sys.stderr,
        )
        return
    backup_cfg = ASSETS_BACKUP / "tower.cfg"
    backup_profiles = ASSETS_BACKUP / "profiles"
    if backup_cfg.is_file():
        shutil.copyfile(backup_cfg, ROOT / "assets" / "tower.cfg")
    if backup_profiles.is_dir():
        dest = ROOT / "assets" / "profiles"
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(backup_profiles, dest)
    shutil.copyfile(LOG_BASELINE, ROOT / "assets" / "log.txt")


def run_carrier(args: list[str]) -> int:
    cmd = [str(CARRIER_EXE)] + args
    print("$", subprocess.list2cmdline(cmd))
    return subprocess.run(cmd, cwd=CARRIER_DIR).returncode


def last_tick_of(digest_path: Path) -> int | None:
    """Returns the tick number on the last non-empty line of a digest file,
    or None if it can't be determined. Used to give --play-replay a
    --stop-at-tick bound (the carrier has no other way to know when a
    replayed script "is done" - it keeps running, exactly like a live human
    session would, until told to stop or the guest exits on its own)."""
    try:
        lines = [ln for ln in digest_path.read_text().splitlines() if ln.strip()]
    except OSError:
        return None
    if not lines:
        return None
    try:
        return int(lines[-1].split()[0])
    except (ValueError, IndexError):
        return None


def print_globals_summary(report_path: Path) -> None:
    """Prints the carrier's own --print-globals evaluation (carrier/src/
    print_globals.cpp, "print_globals" array in --report's JSON) right next
    to the EQUAL/first-difference verdict, per carrier/NOTES.md "Headless,
    frame oracle, named globals, .itr workload" item 3 - the score/floor/
    combo values the replay actually reached, not just whether the digest
    stream matched."""
    try:
        report = json.loads(report_path.read_text())
    except (OSError, ValueError) as error:
        print(f"play.py: could not read {report_path} for --print-globals ({error})", file=sys.stderr)
        return
    rows = report.get("print_globals")
    if not rows:
        return
    print("play.py: final globals (score/floor/combo):")
    for row in rows:
        if "error" in row:
            print(f"  {row['expr']} = <error: {row['error']}>")
        else:
            print(f"  {row['expr']} = {row['value']}")


def compare_digests(a: Path, b: Path) -> int:
    cmd = [sys.executable, str(COMPARE_DIGESTS), str(a), str(b)]
    print("$", subprocess.list2cmdline(cmd))
    return subprocess.run(cmd).returncode


def valid_replay_name(name: str) -> bool:
    return bool(name) and all(c.isalnum() or c in "_-." for c in name) and name not in (".", "..")


def take_value(argv: list[str], flag: str) -> tuple[list[str], str | None]:
    """Removes `flag NAME` (or `flag=NAME`) from argv, returns (remaining, NAME|None)."""
    out = list(argv)
    for i, tok in enumerate(out):
        if tok == flag and i + 1 < len(out):
            value = out[i + 1]
            del out[i : i + 2]
            return out, value
        if tok.startswith(flag + "="):
            value = tok.split("=", 1)[1]
            del out[i]
            return out, value
    return out, None


def main(argv: list[str]) -> int:
    argv = list(argv)
    if "-h" in argv or "--help" in argv:
        print(__doc__)
        return 0

    keep_state = "--keep-state" in argv
    if keep_state:
        argv.remove("--keep-state")

    argv, pace = take_value(argv, "--pace")
    pace = pace or "fast"
    if pace not in ("real", "fast"):
        raise PlayError(f"--pace must be real or fast, got {pace!r}")

    argv, record_name = take_value(argv, "--record-replay")
    argv, play_name = take_value(argv, "--play-replay")
    no_det = "--no-det" in argv
    if no_det:
        argv.remove("--no-det")

    modes_selected = sum(x is not None for x in (record_name, play_name)) + (1 if no_det else 0)
    if modes_selected > 1:
        raise PlayError("--record-replay, --play-replay and --no-det are mutually exclusive")
    if argv:
        raise PlayError(f"unrecognized argument(s): {' '.join(argv)}")

    build_carrier_if_missing()
    verify_image()

    try:
        if record_name is not None:
            if not valid_replay_name(record_name):
                raise PlayError(f"--record-replay NAME must be a simple filename stem, got {record_name!r}")
            REPLAYS_DIR.mkdir(parents=True, exist_ok=True)
            script_path = REPLAYS_DIR / f"{record_name}.txt"
            digest_path = REPLAYS_DIR / f"{record_name}.digest"
            code = run_carrier([
                "--det", "--pace=real", "--input=real", "--interactive",
                "--record-input", str(script_path),
                "--digest-out", str(digest_path),
            ])
            print(f"play.py: recorded {script_path}")
            print(f"play.py: recorded digest {digest_path}")
            return code

        if play_name is not None:
            if not valid_replay_name(play_name):
                raise PlayError(f"--play-replay NAME must be a simple filename stem, got {play_name!r}")
            script_path = REPLAYS_DIR / f"{play_name}.txt"
            baseline_digest = REPLAYS_DIR / f"{play_name}.digest"
            if not script_path.is_file():
                raise PlayError(f"no recorded replay at {script_path} (record one first with --record-replay {play_name})")
            replay_digest = REPLAYS_DIR / f"{play_name}.replay.digest"
            replay_report = REPLAYS_DIR / f"{play_name}.replay.report.json"
            replay_args = [
                "--det", f"--pace={pace}", "--input=script",
                "--input-script", str(script_path),
                "--digest-out", str(replay_digest),
                "--report", str(replay_report),
                "--print-globals", ",".join(ICY_TOWER_GLOBALS),
            ]
            # The carrier has no other way to know a replayed script "is
            # done" - it keeps running past the last scripted event exactly
            # like a live session would. Stop at EXACTLY the last tick the
            # original recording's digest reached (not "a bit past it" -
            # the safepoint writes that tick's digest line before checking
            # --stop-at-tick, so this reproduces the same number of digest
            # lines as the baseline for an apples-to-apples EQUAL, instead
            # of a spurious "N more lines" length mismatch from replaying
            # a few ticks further than the recording did).
            stop_tick = last_tick_of(baseline_digest)
            if stop_tick is not None:
                replay_args += ["--stop-at-tick", str(stop_tick)]
            else:
                print(
                    f"play.py: warning - could not read a last tick from {baseline_digest}; "
                    "running without --stop-at-tick (may not terminate on its own)",
                    file=sys.stderr,
                )
            code = run_carrier(replay_args)
            if code != 0:
                return code
            if not baseline_digest.is_file():
                print(
                    f"play.py: no {baseline_digest} to compare against "
                    f"(replay itself ran fine - digest is at {replay_digest})",
                    file=sys.stderr,
                )
                print_globals_summary(replay_report)
                return 0
            verdict = compare_digests(baseline_digest, replay_digest)
            print_globals_summary(replay_report)
            return verdict

        if no_det:
            return run_carrier(["--interactive"])

        # Plain interactive default.
        return run_carrier(["--det", "--pace=real", "--input=real", "--interactive"])
    finally:
        restore_assets(keep_state)


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except (OSError, PlayError) as error:
        print(f"play.py: {error}", file=sys.stderr)
        sys.exit(1)
