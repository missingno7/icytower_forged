#!/usr/bin/env python3
"""pf_inspect.py - milestone 9 (win32_pilot.md sec 8 row 9): read a carrier
snapshot OFFLINE and print the game's own state with names and real types.

A snapshot directory (carrier/src/snapshot.cpp, format
`portforge-win32-carrier-snapshot-v1`) is a flat address space plus a
manifest. This tool re-assembles that address space and decodes it with the
GENERATED interop metadata - never with hand-written offsets:

    carrier/gen/interop_index.json  every game global: name, VA, C type, CU
    carrier/gen/it_types.h          every struct's members, in order, with
                                    their declared types (generated from DWARF)
    carrier/gen/it_types_check.c    the authoritative sizeof/offsetof values
                                    for those structs (the same numbers the
                                    carrier's own build verifies under MSVC)
    artifacts/coff_symbols.json     each global's size (gap to the next symbol),
                                    the same rule gen_game_globals.py uses

Subcommands
-----------
  show DIR [--globals a,b,c] [--all]
      Named globals with typed values. With no --globals, prints the
      gameplay summary the milestone asks for: reward_time, reward_scale,
      player_id, ply[player_id]->{x,y,sx,sy,dead,frame,...}, the tick
      counters, and the carrier's own externalized state (virtual clock,
      script cursor, RNG).

  player DIR [--index N]
      Full Tplayer struct dump for ply[N] (default: ply[player_id]).

  diff DIR_A DIR_B
      Compares two snapshots BY NAMED GLOBAL and reports the first
      difference as (global, member, byte offset) - never as a percentage.
      Falls back to raw section bytes for anything outside a named global.

Usage: python carrier/scripts/pf_inspect.py <subcommand> ... [--repo ROOT]
Exit code: 0 on success (diff: 0 when the snapshots are equal), 1 on a
difference, 2 on a usage/parse error.

Thin delegator (notes/extraction_plan.md S3): the mechanism itself now lives
in the port_forge submodule as tools/pf_win32_inspect.py, parametrized on
every path this project used to compute via hand-inferred repo-relative
joins (see that tool's own module docstring, including the three pieces of
icytower_forged-specific behavior it explicitly flags as NOT generalized:
DEFAULT_GLOBALS, the player_id/ply/Tplayer names, and Allegro's `fixed`
type). This wrapper injects --repo (and --image, if the conventional asset
exists) so the documented bare invocation above keeps working unchanged; a
caller MAY still pass --repo/--image/--interop-index/--coff-symbols/
--gen-dir/--game-globals-inc explicitly to override any one of them.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CARRIER_DIR = os.path.dirname(HERE)
ROOT = os.path.dirname(CARRIER_DIR)
TOOLS_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'port_forge', 'tools'))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_win32_inspect as _impl  # noqa: E402


def _inject_before_subcommand(argv, flag, value):
    # pf_win32_inspect.py's --repo/--image are top-level argparse options
    # that must precede the subcommand token (argparse subparsers consume
    # everything after it) -- so injected defaults are PREPENDED, not
    # appended, and only if the caller did not already supply the flag
    # somewhere.
    if flag in argv:
        return argv
    return [flag, value] + argv


def main():
    argv = list(sys.argv[1:])
    argv = _inject_before_subcommand(argv, '--repo', ROOT)
    default_image = os.path.join(ROOT, 'assets', 'icytower15.exe')
    if os.path.exists(default_image):
        argv = _inject_before_subcommand(argv, '--image', default_image)

    return _impl.main([sys.argv[0]] + argv)


if __name__ == "__main__":
    sys.exit(main())
