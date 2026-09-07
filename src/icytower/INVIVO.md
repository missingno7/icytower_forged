# In-vivo verification log — `src/icytower/`

Milestone 12 at scale (`carrier/NOTES.md` "Milestone 12 at scale", 2026-09-07):
every one of the 35 functions in this directory (`PROMOTIONS.md`) bound
into the running carrier at its original address (`carrier/src/bind.cpp`'s
5-byte entry patch, win32_pilot.md §3) and compared against the ORIGINAL
machine code, per function, over a real recording — not the offline
`lift_check.py` oracle (unicorn on synthetic vectors) `PROMOTIONS.md`
already reports, but the actual carrier running the actual game.

Primary workload: `replays/human_test.txt` (the operator's own recording —
2293 gameplay ticks, 100 floors, score 2386), baseline per-tick digest
`replays/human_test.digest`. Method for each function `<fn>`
(`carrier/scripts/bind_all.py`, automated): restore pristine assets, run
`--bind <fn>=original --fn-digest-out A` (hardware-breakpoint-sensed — the
one ORIGINAL-form function a run's DR budget allows), restore assets again,
run `--bind <fn>=src --fn-digest-out B --digest-out B_ticks`, then
`compare_fn_digests.py A B` (per-invocation: args/pre/post/eax, every
invocation) and `compare_digests.py B_ticks` against the baseline (the
whole simulation's per-tick state). A function the workload never invokes
(0 records in both A and B) is **UNVERIFIED IN VIVO**, not silently called
EQUAL.

## Results

| function | VA | invocations | verdict | workload |
|---|---|---:|---|---|
| `update_frame` | 0x406ac4 | 2294 | EQUAL | human_test.txt |
| `is_solid` | 0x4166dc | 0 | UNVERIFIED IN VIVO | human_test.txt, menu_idle.txt (both 0) |
| `jump_player` | 0x418678 | 517 | EQUAL (x87, GCC build) | human_test.txt |
| `getFloorData` | 0x416770 | 3561 | EQUAL | human_test.txt |
| `reset_map` | 0x4166a4 | 1 | EQUAL | human_test.txt |
| `add_combo` | 0x40414c | 3 | EQUAL | human_test.txt |
| `line_intersect` | 0x406b80 | 2418 | EQUAL (x87, GCC build) | human_test.txt |
| `get_gamepad` | 0x4017fc | 1 | EQUAL | human_test.txt |
| `is_up` | 0x401844 | 38 | EQUAL | human_test.txt |
| `is_down` | 0x40185c | 38 | EQUAL | human_test.txt |
| `is_left` | 0x401874 | 2331 | EQUAL | human_test.txt |
| `is_right` | 0x401888 | 1480 | EQUAL | human_test.txt |
| `is_fire` | 0x4018a0 | 2329 | EQUAL | human_test.txt |
| `is_pause` | 0x4018b8 | 2293 | EQUAL | human_test.txt |
| `is_enter` | 0x4018d0 | 19 | EQUAL | human_test.txt |
| `is_any` | 0x4018e8 | 48 | EQUAL | human_test.txt |
| `set_control` | 0x4017d4 | 0 | UNVERIFIED IN VIVO — dead code (0 references anywhere in artifacts/disasm.txt beyond its own body) | human_test.txt, menu_idle.txt (both 0) |
| `init_control` | 0x401790 | 2 | EQUAL | human_test.txt |
| `check_control_key` | 0x401808 | 0 | UNVERIFIED IN VIVO — dead code (0 references) | human_test.txt, menu_idle.txt (both 0) |
| `get_level` | 0x416748 | 703 | EQUAL | human_test.txt |
| `add_jump_sequence` | 0x4040f4 | 28 | **DIFFER** — first difference k=0, T=330, field=post; original leaves the domain unchanged (took an early return SRC does not have), SRC always writes. Root cause: the recovered source is missing the `if (js->num == 0) return;` guard the original has before its `jumpPosts > 4999` check (confirmed against `artifacts/disasm.txt` 0x4040f4-0x404146). See `carrier/NOTES.md` "Milestone 12 at scale" §D for the full disassembly evidence; flagged as background task `task_8dd77dd1`, not fixed in this pass. | human_test.txt |
| `reset_particles` | 0x418420 | 1 | EQUAL | human_test.txt |
| `scroll_scroller` | 0x41f0c0 | 25 | EQUAL | human_test.txt |
| `restart_scroller` | 0x41f0d0 | 57 (common) | EQUAL over all 57 common invocations; ORIGINAL run produced 1 further record after the last common one before its own `--run-seconds` wall-clock cutoff — a harness timing artifact (the two sensing mechanisms have different per-call overhead under a fixed real-time budget), not a functional difference. `human_test.txt` never reaches this function at all (0 invocations there). | menu_idle.txt |
| `cycle_counter` | 0x41fed4 | 2529 | EQUAL | human_test.txt |
| `fps_counter` | 0x41fea4 | 50 | EQUAL | human_test.txt |
| `get_demo` | 0x40696c | 1947 | EQUAL | human_test.txt |
| `get_controls` | 0x406978 | 1 | EQUAL | human_test.txt |
| `switchedFromProgram` | 0x406a5c | 0 | UNVERIFIED IN VIVO — has real callers (Allegro focus-lost callback, registered at 3 sites in artifacts/disasm.txt) but fires only on a real OS window-focus-loss event, which no input script can produce (its sibling `switchedToProgram` WAS invoked once in the gate run, via the same mechanism, on a focus-gain event) | human_test.txt |
| `switchedToProgram` | 0x406a6c | 1 | EQUAL | human_test.txt |
| `clickedCloseButton` | 0x406a7c | 0 | UNVERIFIED IN VIVO — Allegro close-button callback (registered at 1 site); fires only on a real window-close-button click, not reachable via an input script | human_test.txt |
| `new_rand` | 0x406984 | 138255 (tested alone); 342 (entry-patch crossings when all 35 are bound at once — see note below) | EQUAL (x87, GCC build) | human_test.txt |
| `update_particle` | 0x41843c | 113730 | EQUAL (x87, GCC build; domain includes the `seed` global, mutated via up to 2 `new_rand()` calls) | human_test.txt |
| `create_particle` | 0x418490 | 446 | EQUAL (x87, GCC build; domain includes `seed`, mutated via up to 3 `new_rand()` calls) | human_test.txt |
| `ok_to_play` | 0x406a50 | 0 | UNVERIFIED IN VIVO — dead code (0 references anywhere in artifacts/disasm.txt beyond its own body; `PROMOTIONS.md` already noted its offline negative control is inexpressible for the same underlying reason) | human_test.txt, menu_idle.txt (both 0) |

**Summary: 27 EQUAL, 1 DIFFER (`add_jump_sequence`), 7 unverified in vivo**
(of which 3 — `set_control`, `check_control_key`, `ok_to_play` — are dead
code in this binary and cannot be verified in vivo by any workload, not
just the ones tried; 1 — `is_solid` — has real but unreached callers; 2 —
`switchedFromProgram`, `clickedCloseButton` — need a real OS window event,
not an input script; 1 — `restart_scroller` — verified EQUAL, just on a
different workload than the other 34).

## The `new_rand` crossing-count note

`new_rand`'s own entry-patch crossing count depends on who else is bound at
the same time: tested alone (this table's primary column), every caller of
`new_rand` — including `update_particle`/`create_particle`'s ORIGINAL
machine code — reaches it through its patched guest VA (0x406984), giving
138255. With all 35 bound simultaneously (`carrier/NOTES.md` "Milestone 12
at scale" §E), `update_particle`/`create_particle`'s `src/` forms call
`new_rand()` as a direct C symbol (their generated `pf_bindings_src.h`
leaves a promoted function's own name un-redirected — `BINDINGS_NOTES.md`
"Exclusion") and never touch `new_rand`'s guest VA at all, so the
entry-patch sensor there sees only the 342 calls still-ORIGINAL code makes
directly. Both counts are genuine measurements of different things (an
isolated pairwise test vs. the fully-bound configuration), not a
contradiction or a bug.

## All 35 bound at once

```
carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt --stop-at-tick 2528 --bind-file <abs path>/carrier/scripts/all35_src.bindfile --digest-out ticks.txt
python carrier/scripts/compare_digests.py ticks.txt replays/human_test.digest
```

Result: **EQUAL (2293 ticks)** — all 35 `src/` forms running in place of
the corresponding original machine code, simultaneously, for the entire
recording. See `carrier/NOTES.md` "Milestone 12 at scale" §E for the full
win32_pilot.md §8a metrics table (137176 crossings/invocations, 0 domain
read failures, 0 faults injected, 2028 original `.text` bytes no longer
executed).

## All 40 bound at once (divergence 008 pass, 2026-09-07)

With `add_floor` fixed, every row of the generated binding table can be
bound simultaneously — the five rows the generator added after milestone 12
(`add_floor`, `reset_player`, `update_player`,
`handle_player_collision_original`, `play_jump_sound`) on top of the
original 35. `carrier/scripts/all_src.bindfile` is that list;
`all35_src.bindfile` is left untouched so the milestone-12 measurement above
stays reproducible exactly as it was taken.

```
carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt \
  --stop-at-tick 2528 --run-seconds 300 \
  --bind-file <abs>/carrier/scripts/all_src.bindfile --digest-out ticks.txt
python carrier/scripts/compare_digests.py ticks.txt replays/human_test.digest
```

Result: **EQUAL (2293 ticks)** — 40 `src/` forms replacing the original
machine code simultaneously for the entire recording.

## Binding table generated (2026-09-07)

`carrier/src/bind.cpp`'s hand-maintained 35-row table (name/VA/argc/
comparison-domain C++ function per row) was replaced by a generator
(`carrier/gen/gen_bind_table.py` → `carrier/gen/bind_table.inc`, DO-NOT-EDIT)
that derives every row mechanically from `scan_src_defs.py` (which
functions), `interop_index.json`/`it_funcs_table.inc` (VA/size/prototype),
`build.cmd`'s own link line (which `lifted_`/`native_` forms exist), and a
new hand-curated data file, `carrier/gen/fn_domains.json` (comparison-domain
regions — see that file's own header for why it, not `carrier/lift/
harness/lift_check.py`'s own `SPECS` dict, is the source of truth the
runtime table draws from). Because the generator produces one row per
function `scan_src_defs.py` finds with a real VA — not just the historical
35 — the binding table now also covers every function `src/icytower/`
currently defines: **40 rows** (the 35 above, unchanged in domain and
verdict, plus `add_floor`, `reset_player`, `update_player`,
`handle_player_collision_original`, `play_jump_sound`). Two more —
`draw_buffer`, `start_reward` — are scanned but excluded (`carrier/gen/
build_blockers.json`, hand-curated with a reason each): both reference
Allegro/asset-seam symbols (`makecol`, `textprintf_ex`, `asset_font`,
`asset_bitmap`) the carrier's `src/` compile step does not yet resolve
(LNK2019 unresolved externals, MEASURED) — a separate asset-seam
integration task, not attempted here, and out of scope for "do not touch
`carrier/lift/harness` or `src/`".

Gates re-verified against the regenerated table (assets restored before
every run, per this file's own convention):

| gate | result |
|---|---|
| G1 (`scripts/newgame.txt`, two runs) | **EQUAL (876 ticks)** |
| G2 (`compare_fn_digests.py`, `update_frame` src vs original) | **EQUAL (877 invocations)** |
| all-35 bound (`--bind-file all35_src.bindfile`) vs `human_test.digest` | **EQUAL (2293 ticks)** |

### New rows verified in vivo (`carrier/scripts/bind_all.py`, `replays/human_test.txt`)

| function | VA | invocations | verdict |
|---|---|---:|---|
| `reset_player` | 0x418550 | 1 | EQUAL |
| `update_player` | 0x418740 | 2293 | **EQUAL** (x87, GCC build) |
| `add_floor` | 0x4167dc | 533 | **EQUAL** (was DIFFER; see "Divergence 008" below) |
| `handle_player_collision_original` | 0x407e10 | 0 | UNVERIFIED IN VIVO — `replays/human_test.txt` never takes the collision branch that reaches it (same class of gap already documented for `is_solid` above); comparison domain is also the stated *default* (empty — no call-trace mechanism in `bind.cpp` yet, see `fn_domains.json`), so even a reaching workload would only be checking that both forms return without a fault, not that their game-state effects agree. |
| `play_jump_sound` | 0x406ecc | 46 | EQUAL, but **vacuously**: `play_jump_sound` returns `void` (no EAX comparison) and has the default *empty* domain (no call-trace mechanism, same reason as `handle_player_collision_original`), so this "EQUAL" only certifies that both forms ran 46 times without crashing — not that they had the same effect. A real check needs the call-trace domain `carrier/lift/harness/lift_check.py`'s own `SPECS` entry for this function already uses offline; not implemented in `bind.cpp` this pass. |

`draw_buffer` and `start_reward` have no `src` form linked into this build
(`build_blockers.json` above) and so cannot be bound or tested at all this
pass.

**Updated summary: 29 EQUAL (27 unchanged + `update_player` + `add_floor`;
`reset_player` and the vacuous `play_jump_sound` also EQUAL but noted
separately above), 1 DIFFER (`add_jump_sequence`), 8 unverified in vivo
(the 7 already listed plus `handle_player_collision_original`), 2 functions
with no bindable form at all (`draw_buffer`, `start_reward`, asset-seam
blocked).**

## Divergence 008 — `add_floor` (2026-09-07), RESOLVED

The DIFFER this table used to carry for `add_floor` was
`FIRST DIFFERENCE fn=add_floor k=5 T=220 field=post`, with identical
arguments (`args=004f8b18`) and identical pre-state through k=4. It was
**not** a fault in the recovered rules; it was a **binding** gap. Full
narrative in `notes/living_record.md` entry 008 and `carrier/NOTES.md`
"Divergence 008"; the short version, in the order the evidence arrived:

**Which bytes.** Two snapshots at tick 220 (one unbound, one
`--bind add_floor=src`) and `pf_inspect.py diff` name the field directly:

```
FIRST DIFFERING GLOBAL INSIDE THE PER-TICK DIGEST SCOPE:
  map @0x004f8b18 (Tmap, 784 bytes, main.c)
  first differing byte: +172 (VA 0x004f8bc4)  A=14 B=17
  member: .room+172
```

Offset 172 = `room[7]` (7 x 24) + 4 = **`Tfloor.start_tile`**, and the
adjacent `.end_tile` differs too: ORIGINAL `{start_tile=20, end_tile=29}`
vs SRC `{23, 34}` — same `level` (6), same branch of the generator, just
different numbers out of `rand()`.

**Why k=5 specifically.** k=5 is the first invocation that calls `rand()`
at all: `level` 0 is a checkpoint floor and levels 1..4 are empty filler
rows, all four of which draw 0 `rand()` (`notes/layout_determinism.md` §2's
table). Level 5 is the first *real* floor any game generates.

**Root cause.** `map.c` calls plain `rand()`. `carrier/gen/
pf_bindings_src.h` bound every game global and game function by name but
nothing bound `rand`, so the link resolved it to the **carrier's own**
statically-linked UCRT `rand` (`_rand ... libucrt:rand.obj` in
`carrier/obj/carrier.map`; `U _rand` in `carrier/obj_gcc/map.o`) — a
different generator, with a different state, that `srand()` never reaches —
instead of the **guest's** msvcrt `rand` import, whose IAT slot the carrier
owns and, in `--det`, replaces with `det.cpp`'s pinned LCG. MEASURED at the
same tick-220 snapshots:

| run | pinned `rng_state` | `rng_calls` |
|---|---|---:|
| ORIGINAL | 0xea58d532 | 14 |
| SRC (before the fix) | 16944 — *still exactly the seed* | 4 |

i.e. the `src` form advanced the game's own RNG **zero** times across all 30
initial floors, while the original drew 10 (5 real floors x 2 draws).
Re-running the pinned LCG from `srand(16944)` in Python reproduces the
ORIGINAL floor exactly (`r1=22602` -> width 9, `start_tile` 20,
`end_tile` 29), which is the positive proof that `map.c`'s recovered rules
were right all along and only its `rand()` *source* was wrong.

**Fix.** `carrier/gen/gen_bindings.py` gained `GUEST_CRT_IMPORTS`, which
emits into `pf_bindings_src.h`

```c
#include <stdlib.h>   /* first, so the macro rewrites CALLS, not declarations */
typedef int (__cdecl *PFN_crt_rand)(void);
#define rand (*(PFN_crt_rand *)0x00514944)
```

— a call through the guest's own IAT slot, which is exactly what the
original machine code's `call _rand -> jmp *[0x514944]` thunk does. The
slot VA comes from `imports.json`; nothing is hand-typed. `map.c` is
unchanged except for a header note recording that its `rand()` is part of
its binding surface.

**After the fix** (`carrier/scripts/bind_all.py --fn add_floor`,
`replays/human_test.txt`): `EQUAL (533 invocations)` per invocation, and
`EQUAL (2293 ticks)` for the whole-simulation per-tick digest against
`replays/human_test.digest`.
