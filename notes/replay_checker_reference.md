# Reference: the 2010 replay_checker (assets/replay_checker)

Status: reference note, 2026-09-07. The operator added a third-party source
tree, `assets/replay_checker/` (Icy Tower.cpp/.h, replay.h,
replay_checker.cpp, VS2008 project, Release/replay_checker.exe; files dated
2010). It is a clean-room re-implementation of Icy Tower's simulation core
written to validate `.itr` replays of **Icy Tower 1.3** (`ITR130` header,
5-byte packed `{int frames; u8 keys}` macros, hash over header+macros).
It is NOT part of the pilot's code; it lives under the ignored `assets/`
tree and is used as evidence and as a semantic map, never copied.

## What it confirms (KNOWN by comparison with our own findings)

- **Floating point**: `__asm fninit` at start and `FTOI(f) = (int)(__int64)(f)`
  with the comment that VS2008's SSE path gives wrong results — independent
  confirmation of win32_pilot.md §6a (the game computes at x87 PC=64 with
  CW 0x037F; SSE/double builds diverge).
- **RNG**: `rand_p` is exactly msvcrt's LCG (`x*214013+2531011 >> 16 & 0x7fff`)
  with the seed carried explicitly — the same generator the carrier pinned.
- **Replay = seed + key stream**: `validate_replay` seeds from the header,
  runs `play_frame` once per frame with the macro's keys, and declares the
  replay valid only if the player falls exactly at the last macro. The
  1.5.1 format we decoded (`ITR140`, `Treplay.random_seed`, RLE
  `Trecord{key_flags,cycle_count}`) is the successor; the checker rejects
  our `.itr` files ("File is not a valid replay") because of the header
  version, so it cannot validate 1.5.1 replays as-is.

## What it documents (INFERRED for 1.5.1 until verified against the binary)

`new_floor()` is a readable statement of the layout algorithm as of 1.3:

- floors are generated on every 4th call (`pad` 0..3), one call per 16
  pixels of scroll, 32 calls at start (7 floors ring buffer);
- every 50th floor (every 500th above floor 1000) spans the full width
  (`start=0, end=40`);
- otherwise length = `6 + rand % (9 - count/30)` below floor 240, then a
  fixed schedule 6/6/5/4/3/2 by height (with one dummy `rand` call between
  floors 240 and 600), and `start = 5 + rand % (30 - length)`;
- so floor k is a pure function of (seed, k): no timing, no player state.

`play_frame` documents the physics constants (dx ±0.3 per frame with 0.7
reversal damping and 0.9 friction, jump dy = -2·|dx| clamped to ≥12.2,
gravity 0.8, wall bounce ×-0.9, x clamp 85..555), the scroll speed table by
height, the 1500-frame speed ramp, the combo rules (timer 100, score +=
combo_floor²) and the collision test through `line_intersection` (the same
Bourke formula as the binary's `line_intersect`, with a `denominator == 0`
early return the 1.5.1 binary does NOT have — see divergence 006).

## How the pilot uses it

1. As a **semantic guide** when promoting `add_floor`, `handle_player_*`,
   `jump_player`, collision and combo code into `src/`: names and structure
   from here, bytes and verification from the 1.5.1 binary and the oracle.
   Where 1.5.1 differs (it has `ITR140`, rejump handling, the `arg2` forced
   jump in `jump_player`, different `line_intersect` guards), the binary
   wins and the difference is recorded.
2. As an **independent check of the layout claim**: once `add_floor` is
   recovered, compare its rules with `new_floor` and list the deltas.
3. Not as a replay oracle for 1.5.1 replays (format mismatch), unless a
   small `ITR140` reader is added to it for experiments; the carrier's own
   record/replay remains the whole-application oracle (ROADMAP §3).

Licence/authorship of the tool is not stated in the files; treat it as
read-only reference material.
