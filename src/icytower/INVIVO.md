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
