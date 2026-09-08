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

## In-vivo pass, batch 7 + .itr workload (2026-09-07)

Task: verify batch 7's 3 promoted functions (`play_jump_sound`,
`handle_player_collision_original`, `start_reward` — `src/icytower/
PROMOTIONS.md` batch 7) in vivo, unblock `draw_buffer`/`start_reward`'s
`build_blockers.json` entries if the asset-seam wiring can be added
mechanically, then run `bind_all.py` over every generated-binding-table
function not yet marked here, trying `newgame.txt`/`menu_idle.txt` for
anything `human_test.txt` doesn't reach.

### `build_blockers.json` unblocked: `draw_buffer`, `start_reward`

Both files' own header comments already documented and had verified the
exact carrier-world compile recipe needed
(`/FIpf_bindings_src.h /FIpf_lib_bindings.h /FIpf_asset_bindings.h`) — what
was missing was `carrier/build.cmd` actually using it. `carrier/gen/
scan_src_defs.py` gained `--extra-fi {base,extra}`: it CALL-syntax-scans
(`name(`, not a bare-token scan — see the function's own docstring for why:
`control.c`'s `check_control_key(Tcontrol *c, int key)` parameter is
spelled `key`, which collides with Allegro's own `key[]` keyboard array as
a bare token, and a naive scan would wrongly force-include
`pf_lib_bindings.h` into `control.c`, macro-substituting the PARAMETER name
into a syntax error) each MSVC-list file for pf_lib_bindings.h's bound
names or the 5 asset-seam accessor functions, and partitions the list into
files needing only `pf_bindings_src.h` vs files needing all three headers.
`build.cmd` now runs two `cl` invocations for the MSVC group instead of
one. Result: exactly `draw_buffer.c`/`start_reward.c` land in the "extra"
group, `control.c` (and everything else) stays in "base" — MEASURED, both
compile 0 errors/0 warnings, and `carrier/gen/build_blockers.json`'s
`"files"` entry is now `{}`. `gen_bind_table.py` picked both up
automatically on the next build (42 rows, up from 40).

A concurrently-running pass (batch 8) added a 43rd row (`draw_scroller`)
partway through this one, tripping `bind.cpp`'s
`kBindMaxFns`-vs-`bind_table.inc` `static_assert` (42 -> 43). Raised
`kBindMaxFns` 42 -> 60 (`carrier/src/bind.hpp`) with matching
`BIND_STUB(42..59)` definitions (`carrier/src/bind.cpp`) — the same
mechanical bump this constant has taken twice before (8 -> 35 -> 42), with
headroom this time. `draw_scroller` itself is out of this pass's scope
(batch 8's own function) and is deliberately left out of every bindfile
this pass touches.

### Batch 7 functions, in vivo

| function | VA | workload | invocations | verdict |
|---|---|---|---:|---|
| `play_jump_sound` | 0x406ecc | human_test.txt | 46 | EQUAL, vacuous (see "Binding table generated" above — unchanged this pass) |
| `handle_player_collision_original` | 0x407e10 | human_test.txt | 0 | UNVERIFIED IN VIVO (unchanged) |
| `handle_player_collision_original` | 0x407e10 | newgame.txt (876-tick gate recording) | 0 | UNVERIFIED IN VIVO — also tried this pass; the recording's `collision_type` still never selects the `_original` (0) dispatch target within the recording's 500 gameplay ticks. Still unreached by any workload tried across this project; the 4 other `collision_type` variants (`_old`/`_combo`/`_vector`/`_vector_2`) are not promoted at all, so which value the game actually starts with was not independently confirmed. |
| `start_reward` | 0x407c38 | human_test.txt | 3 | **EQUAL** — `carrier/scripts/bind_all.py --fn start_reward --input-script replays/human_test.txt --stop-at-tick 2528`: 3 invocations, `compare_fn_digests.py` EQUAL, per-tick digest EQUAL (2293 ticks vs `replays/human_test.digest`). The operator's own recording DOES trigger a reward this pass's task brief flagged as possibly absent — MEASURED, not assumed. |
| `draw_buffer` | 0x4191c8 | human_test.txt, newgame.txt, menu_idle.txt | 0 (all three) | UNVERIFIED IN VIVO — reached by no scripted workload in this project. `draw_buffer`'s own header comment (recovered from `profile.c`) gives the likely reason: it draws a `\n`-separated text buffer via `asset_font(ASSET_DATA_FONT_MONO)`, consistent with a debug/profile text overlay rather than any screen an input script's key sequence (main menu, gameplay, idle menu) passes through. |

### All-bound run, 42 functions (`draw_buffer`/`start_reward` added)

`carrier/scripts/all_src.bindfile` extended with `draw_buffer=src` and
`start_reward=src` (`draw_scroller` deliberately excluded — batch 8's own
function, out of this pass's scope):

```
carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt \
  --stop-at-tick 2528 --run-seconds 300 \
  --bind-file <abs>/carrier/scripts/all_src.bindfile --digest-out ticks.txt
python carrier/scripts/compare_digests.py ticks.txt replays/human_test.digest
```

Result: **EQUAL (2293 ticks)** — 42 `src/` forms (every generated
binding-table row this project has verified, including this pass's two)
replacing the original machine code simultaneously for the entire
recording.

### `.itr` workload (`carrier/scripts/play_itr.txt`)

Goal: get the game's own replay browser to confirm and play the first
`.itr` in `assets/profiles/MissingNO/replays/`. Navigation into the
browser was already solved (a prior pass); this pass's task was the
still-open "why does the confirm not fire".

**One real bug found and fixed** (`carrier/src/det.cpp`,
`deliver_due_input()`): every synthetic key press was calling
`_handle_key_press(0, scancode)` — a HARDCODED `0` for the `keycode`
(ASCII/unicode) argument. MEASURED against `third_party/allegro-4.4.1/src/
win/wkeybd.c`'s real DirectInput handler (`handle_key_press`, lines
250-292): it calls `ToAscii(vkey, ...)` and passes the RESULT, which is 0
only for non-printable keys (arrows, etc. — matching what the carrier
already sent) but a real ASCII value for printable ones — 13 (`\r`) for
`KEY_ENTER`, 32 (`' '`) for `KEY_SPACE`, 27 (ESC) for `KEY_ESC`. Fixed with
a small `ascii_for_allegro_code()` lookup, used only by the default
(non-`--inject-real-test`) synthetic press path. Verified this changes
nothing observable for ordinary gameplay input: `G1` (two `newgame.txt`
runs, `--stop-at-tick 1000`) still `EQUAL (876 ticks)` against each other,
and the `human_test.txt` gate still `EQUAL (2293 ticks)` against
`replays/human_test.digest` — expected, since ordinary movement/action
input (`is_left`/`is_right`/...) reads the scancode-indexed `key[]` array,
set unconditionally at `_handle_key_press`'s first line regardless of this
argument; only Allegro's separate `readkey()`-buffered queue was affected.

**This fix alone did not make the confirm fire.** Disassembling
`_replay_selector` (0x41d258, `artifacts/disasm.txt` 0x41d330-0x41d9dd)
found the actual dispatch (`0x41d671: call _readkey; sar $8,%eax; ...; jmp
*0x4d7b18(,%eax,4)`) is indexed purely by the SCANCODE half of `readkey()`'s
return value, never the ASCII half — so the keycode argument was never the
blocker for DISPATCH itself (it may still matter elsewhere; fixing it was
correct regardless of this specific mechanism). The real gate found:
`_replay_selector` keeps a local debounce counter (`-0x42c(%ebp)`,
initialized to `0x3e8` = 1000 at entry, 0x41d330) that is decremented once
per loop iteration ONLY while `is_any(ctrl)` is true (some tracked control
button — `CTRL_ENTER` included, `src/icytower/control.c`'s own
`CTRL_ENTER=0x20` bit is inside `is_any`'s `~CTRL_PAUSE` mask) AND the
counter is not already exactly 0 (0x41d650-0x41d671); the MOMENT it hits
exactly 0 while a button is still held, every subsequent frame takes a
"movement" branch (0x41d3eb `je 41d6a8`, `is_down`/`is_up` handling) that
never consults `keypressed()`/`readkey()` at all, until the button is
released (`is_any()` false again resets/keeps the counter at 0 via
`0x41d40c`, and THAT idle branch unconditionally checks `keypressed()`).
`carrier/scripts/play_itr.txt`'s script holds keys (5 nav taps plus a
360-tick `KEY_ENTER` hold entering the browser) for what is very likely
enough total held-iterations to drain this counter to 0 well before the
second `KEY_ENTER` at T=2100 — leaving `is_any()`-gated navigation as the
only reachable branch for the rest of the run. **Not fully confirmed**:
tried a released (tap, not held) second `KEY_ENTER` (press@2100/
release@2120) as the cheapest test of "does a clean release-edge reach the
idle/`keypressed()` branch" — it also did not reach `play()` (still 0
safepoints, `assets/log.txt` unchanged after "opening
profiles/MissingNO/replays/"), so either the counter was already stuck at
0 well before this tap (consistent with the drain theory) or a further,
not-yet-identified factor is also in play. `carrier/scripts/play_itr.txt`
itself is left with its original event sequence (the tap experiment was
run from a scratch copy, not committed) plus this section's findings
referenced from its own header comment update below.

**Outcome: `.itr` workload still does not reach `play()`; 0 additional
functions gained coverage from it this pass.** `play()` requires a
`run_demo()`/gameplay dispatch this pass never reached, so no new function
verdicts came from this workload — the 3 batch-7 functions and
`draw_buffer` above were all verified (or found unreachable) via
`human_test.txt`/`newgame.txt`/`menu_idle.txt` instead.

### Updated summary (this pass)

**31 EQUAL** (29 already listed + `start_reward`; `draw_buffer` still has no
in-vivo verdict), **1 DIFFER** (`add_jump_sequence`, unchanged, background
task `task_8dd77dd1`), **9 unverified in vivo** (the 8 already listed +
`draw_buffer`, now with a workload-reachability explanation rather than a
missing bind), **0 functions with no bindable form** (both former
`build_blockers.json` entries are resolved) — **42 of 42** generated
binding-table rows (as of this pass; `draw_scroller`, batch 8's own 43rd
row, is out of scope here) now have a `src` form linked and bound, and all
42 are simultaneously bound EQUAL over `replays/human_test.txt` (previous
section).

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

## In-vivo pass, batch 9 + corpus gates (2026-09-08)

Six binding-table rows had no in-vivo verdict anywhere in this document
yet: the four `collision.c` variants `carrier.exe` can now bind
(PROMOTIONS.md batch 9), `draw_star_field` (compiles/links/binds cleanly
since the "Allegro inline primitives" pass but was never run in vivo), and
`draw_scroller` (batch 8's own row - verified once by hand in
`carrier/NOTES.md` but never given a formal entry here). Ran
`carrier/scripts/bind_all.py --fn <these six>` over all three of this
project's scripted workloads (`replays/human_test.txt`,
`carrier/scripts/newgame.txt`, `carrier/scripts/play_itr.txt`):

| function | VA | human_test.txt | newgame.txt | play_itr.txt |
|---|---|---|---|---|
| `handle_player_collision_old` | 0x407fd8 | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) |
| `handle_player_collision_vector` | 0x408d08 | **EQUAL (2293)** | **EQUAL (876)** | **EQUAL (157)** |
| `handle_player_collision_vector_2` | 0x4088c8 | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) |
| `handle_player_collision_combo` | 0x408358 | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) |
| `draw_star_field` | - | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) | UNVERIFIED IN VIVO (0) |
| `draw_scroller` | - | **EQUAL (50)** | **EQUAL (88)** | **EQUAL (382)** |

`handle_player_collision_vector` is the ONLY collision variant any workload
ever selects — `collision_type` (VA 0x4dd140) has exactly one store in the
whole image, `new_game()`'s own unconditional `movl $0x2,...`
(`src/icytower/collision.c`'s own header comment), so `_old`/`_vector_2`/
`_combo` are reachable jump-table targets in principle but never chosen by
any recording. `_vector`'s invocation count equals the workload's own
per-tick digest tick count on all three (one collision check per gameplay
tick: 2293/876/157) — as strong an in-vivo confirmation as a compile-time-
constant selector allows. `draw_star_field` stays unreached (same
eye-candy/background-effect class as `draw_buffer`, verified nowhere in
this project). `draw_scroller` is EQUAL on all three (50/88/382
invocations, scaling with how much of each workload runs past the main
menu — the `.itr` workload's own long post-playback idle-menu tail
accounts for most of its 382).

`newgame.txt` had no stored baseline digest before this pass (unlike
`human_test.txt`); one unbound run's digest was captured and cross-checked
self-consistent against a second unbound run (`EQUAL, 876 ticks`) before
using it as `bind_all.py --baseline`. The `.itr` workload's own baseline is
now committed as `replays/itr_last_game.digest` (`carrier/NOTES.md`
"corpus gates").

**Updated running total**: 33 EQUAL (31 already listed + `handle_player_
collision_vector` + `draw_scroller`), 1 DIFFER (`add_jump_sequence`,
unchanged), 12 unverified in vivo (the 9 already listed + `handle_player_
collision_old`/`_vector_2`/`_combo`, plus `draw_star_field` already
counted there) — every one of the 48 generated binding-table rows
(`carrier/gen/bind_table.inc`) now has an in-vivo verdict recorded
somewhere in this document.

## In-vivo pass, batch 10 — `draw_frame` via the frame oracle (2026-09-08)

`draw_frame` (`src/icytower/draw_frame.c`, `PROMOTIONS.md` batch 10) is the
per-frame renderer: ~120 library calls per invocation (blit/draw_sprite/
draw_256_sprite/textprintf_ex/...), so `bind_all.py`'s ordinary per-invocation
sensor (first-call args/pre/post only) covers roughly 15% of what one call
actually does — that file's own header comment says so. The AUTHORITATIVE
check is the in-vivo **frame oracle** `carrier/NOTES.md` "Headless, frame
oracle" already built: a breakpoint at `blit_to_screen`'s entry hashes the
COMPLETE rendered frame (`swap_screen`, the game's own off-screen back
buffer `draw_frame()` has just finished painting) every tick
(`--frame-digest-out`), which is presentation-independent and sees every
pixel `draw_frame` touched, not just its first library call.

### 0. Carrier build

`draw_frame.c` had picked up a stale `build_blockers.json` entry from a
different, concurrently-running task earlier in this project's history (the
entry's own text: "not owned by this task" / "obj_gcc\draw_frame.o ->
draw_replay_hud, ___mingw_sprintf unresolved") — the file is now committed
clean (`src/`'s own batch 10), so the block was removed and the real cause
fixed at the carrier-build layer (`carrier/build.cmd`, not the file):

1. This MSYS2 mingw32 GCC's `<stdio.h>` defaults `__USE_MINGW_ANSI_STDIO=1`,
   redirecting `sprintf` (draw_frame.c's `scrollerText` formatting — the
   first GCC-routed file to call it directly) to MinGW's own
   `__mingw_sprintf` (`libmingwex.a`), which the FINAL link (`cl`/`link.exe`,
   linking only `kernel32`/`user32`/`psapi`) never sees. Fixed with
   `-D__USE_MINGW_ANSI_STDIO=0` on both GCC compile lines, landing back on
   the classic `_sprintf` symbol name.
2. That classic name is itself not exported by VS2022's Universal CRT
   (only by an MSVC-compiled call site's own `<stdio.h>` inline wrapper) —
   the well-known "migrating a legacy-CRT object" gap, fixed by adding
   `legacy_stdio_definitions.lib` to the final link line.

Two generator gaps the file itself already worked around (`#ifndef`-guarded,
documented in its own header) were investigated for a real fix, not edited
in the file (out of this task's scope either way):

- `draw_sprite`/`rotate_sprite`/`fixtoi`/`ftofix` — real Allegro `AL_INLINE`
  bodies with a branch or arithmetic of their own (not the bare vtable
  passthrough `pf_lib_bindings.h` already curates for 23 other names).
  **Fixed in the generator**: `port_forge/tools/pf_win32_gen_lib_bindings.py`
  gained a new curated list, `AL_INLINE_BRANCHING_OR_MATH`, emitting the
  same four upstream-faithful bodies (hand-transcribed from
  `allegro/inline/draw.inl`/`fmaths.inl`, both third_party 4.4.1 and 4.4.3.1
  checked) as `#ifndef`-guarded `static` helpers + forwarding macros into
  `pf_lib_bindings.h` itself. Regenerated; `draw_frame.c`'s own `#ifndef`
  guards now see these already defined (force-included ahead of its own
  text) and its private copies become dead code automatically, with no edit
  to that file.
- `demo->data`/`.cycle_count` member-access collisions with the top-level
  `data`/`cycle_count` globals — investigated against both routes the task
  named. Neither is safe to apply globally: `cycle_count` is used BARE by
  `src/icytower/timer.c` (`cycle_counter(void) { cycle_count++; }`, already
  in the MSVC build), and `data` is used BARE by `carrier/gen/pf_asset_bindings.h`
  itself (`if (!strcmp(family,"data")) return data;` — force-included into
  the SAME translation unit as `draw_frame.c` for the asset-seam calls it
  needs). Skipping either name from `MEMBER_ACCESS_COLLISIONS` (or a
  second, `stars`-style member-safe header) would fix `draw_frame.c` and
  break one of those two files in the same build — exactly the already-
  documented `stars`/`start_reward.c` shape of this bug, just doubled up.
  Left as `draw_frame.c`'s own in-file `#undef` (correct and necessary,
  not merely convenient: it is the only place in the translation unit that
  can un-define these two names for the REST of one file's own text
  without touching either of the other two consumers) — not removed, not
  edited.

Gates after these fixes (`carrier/scripts/gates.ps1`, restoring pristine
assets before every launch): **G1-G5 all EQUAL** (G1 876 ticks; G2 877
invocations; G3a EQUAL 301 rows T=400..699 / EQUAL 602 rows T=400..1000; G3b
EQUAL 301 invocations k=276..576; G4 EQUAL 2293 ticks vs
`replays/human_test.digest`; G5a EQUAL 157 ticks itr-replayed-twice; G5b
EQUAL 157 ticks itr all-bound vs `replays/itr_last_game.digest`).

### 1. Frame oracle: unbound vs `draw_frame=src`, all three workloads

Per workload: one unbound run (`draw_frame` at its original address) and
one `--bind draw_frame=src` run, both with `--frame-digest-out` (every
tick) AND the ordinary `--digest-out` (per-tick game-global digest).

| workload | frame oracle (unbound vs src) | ticks (unbound vs src) | ticks vs stored baseline |
|---|---|---|---|
| `replays/human_test.txt` (2293 ticks) | **EQUAL (2381 frame lines)** | **EQUAL (2293 ticks)** | **EQUAL**, both forms, vs `replays/human_test.digest` |
| `carrier/scripts/newgame.txt` (876 ticks) | **EQUAL (983 frame lines)** | **EQUAL (876 ticks)** | no stored baseline (per batch 9); unbound-vs-src is the check |
| `carrier/scripts/play_itr.txt` (157 ticks) | **EQUAL on every common line** (see note) | **EQUAL (157 ticks)** | **EQUAL**, both forms, vs `replays/itr_last_game.digest` |

**`play_itr.txt` process-exit note** (this session, this host, not a
`draw_frame` finding): `--run-seconds`'s watchdog fires in the PARENT
`carrier.exe`, but the game relaunches itself as a child process, and that
child does not exit on its own once `.itr` playback ends and the menu's
idle loop takes over — MEASURED: the digest-out file reaches its expected,
stable content (157 ticks, matching `replays/itr_last_game.digest`
byte-for-byte) and the process then sits idle indefinitely rather than
exiting, in every run this pass (`gates.ps1`'s own G5 hit the identical
hang first, independently of this file). Worked around at the test-harness
level only (no `carrier/src` change): poll the digest-out file for size
stability, then force-terminate `carrier.exe`. Because this poll is a real
wall-clock race against a live idle-menu render loop, and the DR-sensed
`--bind draw_frame=original` form runs dramatically slower per call than
the compiled `--bind draw_frame=src` form, the two independently-stopped
runs' RAW FRAME-BLIT counts differ by anywhere from single digits to (for
the per-invocation fn-sensor below) low hundred-thousands of extra idle-menu
frames — every one of which is still **content-EQUAL on every commonly
reached line**; only the trailing length differs, and only in the idle
tail past the last real gameplay tick. The per-tick game-global digest
(157/157, deterministic, not wall-clock-paced) is unaffected and is the
gate that matters; it is EQUAL against the stored baseline for both forms.

### 2. Per-invocation fn sensor (`carrier/scripts/bind_all.py --fn draw_frame`)

| workload | verdict |
|---|---|
| `replays/human_test.txt` | **EQUAL (2294 invocations)** |
| `carrier/scripts/newgame.txt` | **EQUAL (877 invocations)** |
| `carrier/scripts/play_itr.txt` | **EQUAL for the first 44808 common invocations** (0 differences on any compared record); the ORIGINAL-form (DR-sensed) run's own poll-stop landed at 44808 idle-menu-tail invocations, the `src`-form run's landed later (293900) for the same reason as the frame-oracle note above — a wall-clock stopping-point artifact of this session's `play_itr.txt` process-exit issue, not a content divergence |

### Conclusion

`draw_frame` is **EQUAL** in vivo on all three of this project's scripted
workloads, at both the frame-oracle level (every pixel `blit_to_screen`
receives) and the per-tick game-global digest level (the value that
actually gates correctness). Recorded here rather than left "compile-only"
as `PROMOTIONS.md` batch 10 had it pending.

## In-vivo pass, batch 11 — library globals referenced by clean code (2026-09-08)

`PROMOTIONS.md` batch 11 promoted `poll_control`, `handle_player_input` and
`blit_to_screen`, all offline-EQUAL, but `blit_to_screen.c` **compiled and
would not link**: its own transcription of Allegro's `fixsin` (upstream
`static inline`, so the ORIGINAL binary never called it as a distinct
function — it is folded straight into `blit_to_screen`'s own compiled
bytes) reads `_cos_tbl`, Allegro's 512-entry quarter-wave table, and that
global had no binding anywhere. Not a missed case of an existing mechanism:
`carrier/gen/pf_lib_bindings.h`'s allow-list (`artifacts/lib_boundary.json`)
is a CALL/READ-EDGE census of the ORIGINAL binary, so it can only ever name
a library symbol the original game code itself directly called or read —
and the original never referenced `_cos_tbl` by name, only executed the
inlined bytes that happen to index it. A clean-room port that writes the
same inline body out non-inlined needs a real binding the call-graph-derived
allow-list was never going to produce.

### 0. The generator fix (generic, not a `_cos_tbl` special case)

`port_forge/tools/pf_win32_gen_lib_bindings.py` gained a SECOND, mechanical
source of library-scope names, on top of the allow-list: `--src-scan-dir`
scans every `*.c` file in a project's clean-room source tree (comments/
string literals stripped) for two narrow signals —

- a **function** reference needs CALL syntax (`name(`), excluding a
  member/vtable-style access (`a->name(`/`a.name(`) immediately before it —
  the same already-vetted precedent `pf_win32_scan_src_defs.py`'s own
  `file_needs_extra_fi()` uses for exactly this reason;
- a **global** reference needs an explicit `extern <type> name;`
  declaration of that exact name somewhere in the source — precisely the
  shape `blit_to_screen.c` already writes for `_cos_tbl`
  (`extern fixed _cos_tbl[];`).

A first, unguarded draft (bare identifier tokens, no signal at all) was
tried and rejected: it matched 17 names in this project's own
`src/icytower/`, and 16 of the 17 were false positives — `FONT_VTABLE`
struct-member names (`extract_font_range`, `font_height`,
`get_font_range_begin`, ...) only ever written as
`offsetof(struct FONT_VTABLE, <member>)` in the generated
`game_types_check.c`, and ordinary local variables (`ms`, `msg`, `mx`, `my`,
`name`, `length`, `palette`, `set_clip`) that merely share a spelling with
some unrelated Allegro-CU symbol. The two-signal version above finds
**exactly one** name in this project: `_cos_tbl` (0 functions, 1 global, 0
ambiguous) — logged in `carrier/gen/LIB_BINDINGS_NOTES.md`'s new "Extra
symbols" section every regeneration.

A found name is bound exactly like an ordinary allow-list entry (same
DWARF resolution against the scope='all' model, same collision checks,
same emission into `pf_lib_bindings.h` / `pf_lib_bindings_types.h` /
`src/icytower/allegro_api.h`) — `_cos_tbl` now has a real
`#define _cos_tbl (*(fixed (*)[512])0x4ce100)` and a real
`extern fixed _cos_tbl[512];` declaration in the standalone world.

**One coupled fix, needed because `_cos_tbl` alone would have broken the
build a different way**: `blit_to_screen.c`'s own `#ifndef fixsin` guard
wraps BOTH its private `it_al_fixsin` body AND a bare
`extern fixed _cos_tbl[];` redeclaration. Binding `_cos_tbl` to a macro
without also pre-defining `fixsin` would make the force-included
`pf_lib_bindings.h` macro-substitute `_cos_tbl` INSIDE that file's own
extern line the moment its text is lexed — `extern fixed
(*(fixed(*)[512])0x4ce100)[];`, a syntax error, not a link error. Fixed the
same way batch 10 already closed `draw_sprite`/`rotate_sprite`/`fixtoi`/
`ftofix`: `fixsin` joins `AL_INLINE_BRANCHING_OR_MATH` (its real
`fmaths.inl:199` body, hand-transcribed, depending on the now-bound
`_cos_tbl`), so `blit_to_screen.c`'s own `#ifndef fixsin` sees it already
defined (force-included ahead of its own text) and skips its ENTIRE local
block — extern redeclaration included — with no edit to that file. The
generator only emits this entry when `_cos_tbl` is itself bound
(`cos_tbl_bound` check), so a project where the scan finds nothing gets no
broken `fixsin` body either. `fixcos` (same table, different phase) is
deliberately NOT added — nothing in this project's clean-room source calls
it; this list is curated per named need, not per upstream family.

Also fixed in the same pass, a real (if minor) pre-existing bug: the
project wrapper `carrier/gen/gen_lib_bindings.py` never injected
`--out`/`--types-out`/`--notes-out`, so its documented zero-argument
invocation silently wrote all three generated files under
`port_forge/tools/` instead of `carrier/gen/` (already flagged in
`carrier/NOTES.md`'s "Asset oracle" section as a real gap, worked around by
hand every prior pass). Now injects all three from `win32_policy.json`'s
`gen_dir`.

### 1. Carrier build

`carrier/build.cmd` (MSVC x86, `vcvars32.bat`) — clean: `blit_to_screen.c`
now compiles AND links (previously `LNK2019: unresolved external symbol
_cos_tbl`). `carrier.exe` built.

**Gates re-verified (`carrier/scripts/gates.ps1`)**: **G1-G5 all EQUAL** —
G1 EQUAL (876 ticks), G2 EQUAL (877 invocations), G3a/G3b EQUAL (301 rows/
invocations each), G4 EQUAL (2293 ticks vs `replays/human_test.digest`),
G5a EQUAL (157 ticks, itr replayed twice unbound), G5b EQUAL (157 ticks,
itr all-bound vs `replays/itr_last_game.digest`).

### 2. Per-function in-vivo pass, all three workloads

`carrier/scripts/bind_all.py --fn poll_control,handle_player_input,blit_to_screen`,
each vs `original` (DR-sensed), per-invocation AND per-tick digest, over
all three scripted workloads (`newgame`/`play_itr`'s per-tick comparison
against a freshly-captured UNBOUND baseline digest for that workload, no
stored baseline existing for `newgame.txt` per batch 9/10 precedent):

| function | `human_test.txt` (2293 ticks) | `newgame.txt` (876 ticks) | `play_itr.txt` (157 ticks) |
|---|---|---|---|
| `poll_control` | EQUAL (2341 invocations) | EQUAL (962 invocations) | EQUAL on every common record (see note) |
| `handle_player_input` | EQUAL (2293 invocations) | EQUAL (876 invocations) | **EQUAL (157 invocations)** |
| `blit_to_screen` | EQUAL (2381 invocations) | EQUAL (983 invocations) | EQUAL on every common record (see note) |

Per-tick game-global digest: **EQUAL** for all three functions on all three
workloads (`human_test.txt` vs `replays/human_test.digest`; `play_itr.txt`
vs `replays/itr_last_game.digest`; `newgame.txt` vs this pass's own fresh
unbound baseline).

**`play_itr.txt` note, same wall-clock-tail class `draw_frame`'s own INVIVO
entry already documents**: `poll_control` and `blit_to_screen` are invoked
every tick of the idle menu loop that follows `.itr` playback, so the
`--bind fn=original` (DR-sensed, slower per call) and `--bind fn=src`
runs — each independently stopped by `--run-seconds`' wall-clock watchdog —
make a different number of idle-tail calls before stopping (784 and 3677
extra records respectively this run). Every COMMON record compares equal
(last common invocation: `poll_control` k=20812 T=20735; `blit_to_screen`
k=17423 T=17463); only the trailing length differs. `handle_player_input`
is naturally immune to this artifact — unlike the other two, it fires only
on a REAL gameplay tick, never during the idle-menu tail, so its count
(157) matches exactly on both sides with no caveat needed. This is a new,
notable fact about the shape of the artifact, not a new instance of it.

### 3. Frame oracle: unbound vs `blit_to_screen=src` / `handle_player_input=src`

`blit_to_screen` IS the frame-digest sample point (mode 0's `blit`); binding
it changes the instrument, so the unbound baseline was captured first, per
`PROMOTIONS.md` batch 11's own stated caveat. `handle_player_input`'s frame
oracle checks that binding the input seam does not change what gets drawn.

| workload | `blit_to_screen=src` vs unbound | `handle_player_input=src` vs unbound |
|---|---|---|
| `replays/human_test.txt` | **EQUAL (2381 frame lines)** | **EQUAL (2381 frame lines)** |
| `carrier/scripts/newgame.txt` | **EQUAL (983 frame lines)** | **EQUAL (983 frame lines)** |
| `carrier/scripts/play_itr.txt` | EQUAL on every common line (610 common ticks, last common T=652; +19 idle-tail lines) | EQUAL on every common line (+17 idle-tail lines) | 

Same `play_itr.txt` wall-clock-tail artifact as §2 and as `draw_frame`'s own
entry above — content-EQUAL everywhere the two runs both reached, length
differs only in the idle tail.

### 4. All-bound: `carrier/scripts/all_src.bindfile` gains three rows

`poll_control=src`, `handle_player_input=src`, `blit_to_screen=src` added
(draw_frame/draw_scroller, verified in vivo by a concurrently-running pass,
deliberately left out — not this pass's own rows). Re-ran with
`--bind-file`:

| workload | per-tick digest | frame digest |
|---|---|---|
| `replays/human_test.txt` | **EQUAL (2293 ticks)** vs `replays/human_test.digest` (also reconfirmed via `gates.ps1` G4) | **EQUAL (2381 frame lines)** vs unbound |
| `carrier/scripts/newgame.txt` | **EQUAL (876 ticks)** vs this pass's own unbound baseline | **EQUAL (983 frame lines)** vs unbound |
| `carrier/scripts/play_itr.txt` | **EQUAL (157 ticks)** vs `replays/itr_last_game.digest` (also reconfirmed via `gates.ps1` G5b) | EQUAL on every common line (same wall-clock-tail artifact) |

**The score/floor witness, everything bound**: `replays/human_test.txt` run
to the game's own exit (`--stop-at-tick 3200` — 2528 truncates the
recording's own post-game high-score-entry key sequence, which runs to
input-script tick 3047; `--run-seconds 180`) with
`--bind-file carrier/scripts/all_src.bindfile`: per-tick digest still
**EQUAL (2293 ticks)** vs `replays/human_test.digest`, and the saved
`assets/profiles/MissingNO/replays/last_game.itr` reads **score 2386 @0x4a,
floor 100 @0x4e** — unchanged from divergence 009's own witness, now with
three more functions bound.

### Conclusion

`poll_control`, `handle_player_input` and `blit_to_screen` are **EQUAL** in
vivo on all three of this project's scripted workloads — per-invocation,
per-tick digest, and (for the two with real presentation/input-seam
stakes) the frame oracle — both individually and all-bound together with
every other promoted function, and the human_test score-2386/floor-100
witness is unchanged with everything bound. The generator gap that blocked
`blit_to_screen` from linking at all (`_cos_tbl`) is closed generically:
`--src-scan-dir` mechanically re-derives any future name in this same
class (a clean-room port writing an upstream inline body out non-inlined),
not just this one.

## In-vivo pass -- play(), the whole game loop (2026-09-08)

`PROMOTIONS.md` batch 12 recovered `play()` (0x411a00, 17420 bytes) as one
file, offline-verified only for 502 of its 17420 bytes (three self-contained
regions plus an exact 82-callee census) and marked the rest -- the tick
body's ORDER, the pacing, and the whole game-over/results/rank/initials UI
-- **IN-VIVO-PENDING**, authoritative only via the running carrier. This is
that pass, run by the carrier task.

### 0. Generator gaps closed (carrier world only; both standalone worlds
already compiled clean per PROMOTIONS.md batch 12)

`play.c` reaches four names the generated headers never covered
(`QueryPerformanceCounter`/`QueryPerformanceFrequency`, `mkdir`, `stricmp`,
plus `LARGE_INTEGER`) and, separately, four Allegro screen-state primitives
(`SCREEN_W`/`SCREEN_H`/`acquire_screen`/`release_screen`) with no macro yet
in `pf_lib_bindings.h` (`hline`/`vline`/`rectfill` turned out to already be
bound, from an earlier concurrent pass). Both closed generically, in
`port_forge/tools/`, not as one-off patches in `play.c` (which stays
`#ifndef`-guarded and untouched):

- **`port_forge/tools/pf_win32_gen_bindings.py`**: `GUEST_CRT_IMPORTS`
  entries gained a calling-convention field (`normalize_crt_import_entry()`
  accepts both the old 4-element `[dll, imp, ret, params]` shape, implicit
  `__cdecl`, and a new 5-element `[dll, imp, conv, ret, params]` shape).
  Needed because `QueryPerformanceCounter`/`Frequency` are real Win32
  `__stdcall` APIs -- binding one through a `__cdecl`-typed function
  pointer is a real stack-cleanup ABI bug (the callee already cleans its
  own arguments; a `__cdecl` caller doing it a second time corrupts esp),
  not a style choice. `carrier/gen/gen_bindings.py` (this project's own
  copy, not yet a port_forge shim -- see its docstring) mirrors the same
  fix and adds the four entries: `mkdir`->`_mkdir`, `stricmp`->`_stricmp`
  (msvcrt, `__cdecl`), `QueryPerformanceCounter`/`Frequency` (kernel32,
  `__stdcall`). `LARGE_INTEGER` is supplied by a new hand-written header,
  `carrier/gen/pf_win32_crt_shim_types.h` (claims the real `_WINDOWS_`
  include-guard name so play.c's own `#ifndef _WINDOWS_` fallback --
  which would otherwise get its `QueryPerformanceCounter`/`Frequency`
  prototype declarations mangled by the new macros -- goes dead), pulled
  in via `GUEST_CRT_PRE_INCLUDES` alongside `<stdlib.h>` (already there for
  `rand`/`srand`) and a new `<string.h>` (so `stricmp`'s real old-name
  declaration, if any, is parsed before the macro shadows it, the same
  precedent `rand` already established).
- **`port_forge/tools/pf_win32_gen_lib_bindings.py`**: a fourth curated
  class, `AL_INLINE_SCREEN_STATE` (`SCREEN_W`/`SCREEN_H`, plain object-like
  macros off the `gfx_driver` global; `acquire_screen`/`release_screen`,
  zero-arg statement macros off the `screen` global) -- distinct from the
  existing vtable-dispatch and branching/math classes because these read an
  existing bound library GLOBAL rather than computing from arguments. Same
  collision checks as the other two classes, plus a check that the
  required global is itself bound.
- `MEMBER_ACCESS_COLLISIONS` on `data`/`rejump`: investigated, NOT added.
  `play.c` already carries its own file-scope `#undef data` / `#undef
  rejump` after the force-included headers (exactly the `stars`/
  `cycle_count`/`data` precedent batches 8/10 established) -- adding these
  names to the generator's skip-list would break `carrier/gen/
  pf_asset_bindings.h`'s own bare `data` reference (batch 10's
  `draw_frame.c` note already documents this exact hazard), so no policy
  change was needed or made.

### 1. Build

`play.c` auto-classified into the GCC x87 group by
`scan_src_defs.py --list-build-files` (real `double` telemetry --
`clockSpeed = 1000.0 * clockElapsed / clockElapsed / 20.0` and friends, per
PROMOTIONS.md batch 12's own finding 1) -- no `carrier/build.cmd` edit
needed, the file-list derivation is fully mechanical. Regenerated
`pf_bindings_src.h`/`pf_bindings_src_no_stars.h` (also picked up three
STALE excludes the checked-in generated file had missed: `destroy_game_data`,
`get_version_str`, and a misspelled `sync_profile_from_options` that should
have read `syncProfileFromOptions` -- all three now correctly excluded,
matching files that already exist in `src/icytower/`). `carrier/build.cmd`
end to end: **`OK: carrier\carrier.exe built`**, 0 errors, the same 5
`-Wformat-overflow` warnings PROMOTIONS.md batch 12 already documented (the
`best_replay_names[]` `sprintf` sites) and no new ones -- confirming the
CRT-import/screen-state fallbacks in `play.c`'s own header stayed
`#ifndef`-dead (no macro-redefinition or conflicting-declaration warnings
for any of the eight names).

**Gates (`carrier/scripts/gates.ps1`), play compiled in but NOT bound:**
**G1-G5 all EQUAL** -- G1 EQUAL (876 ticks), G2 EQUAL (877 invocations),
G3a/G3b EQUAL (301 rows/invocations), G4 EQUAL (2293 ticks vs
`replays/human_test.digest`), G5a EQUAL (157 ticks, itr replayed twice
unbound), G5b EQUAL (157 ticks, itr all-bound vs
`replays/itr_last_game.digest`).

### 2. A structural carrier-layer fact, discovered before any gameplay
question could even be asked

The documented recipe (`carrier.exe --bind play=src ... --digest-out ...`)
does not produce a per-tick digest at all: **0 lines, every time**, for
ANY run with `play` bound to any non-original form. Root cause, MEASURED
(`det: shutdown at T=... 0 safepoints ...` in every such run's own stderr):
`carrier/src/det.cpp`'s tick safepoint (`VA_SAFEPOINT = 0x4124f4`, "main.c
play(): once per consumed game tick") is a hardware EXECUTION breakpoint
anchored at a fixed address INSIDE `play()`'s own ORIGINAL machine code.
Binding `play` to `src` patches its ENTRY (0x411a00) with a 5-byte jump
straight to the compiled form; EIP never revisits ANY address in the
original `0x411a00-0x415e0b` range again for the rest of that process's
life, including 0x4124f4, so the safepoint (and therefore `--stop-at-tick`,
which is checked only inside it) never fires again either. This is a real,
previously-undiscoverable-before-this-pass limitation (no earlier batch
ever bound the function the safepoint itself lives inside) -- not a
recovered-source question, and not fixed this pass (a robust replacement
anchor needs either a disassembly-derived address inside the FRESHLY
COMPILED play.o, re-derived every build, or a broader carrier rework; both
are real future work, not attempted here to avoid destabilizing every
other function's already-verified gate).

**What still works, unaffected, and is what this pass actually verifies
with:** `--frame-digest-out` (hooked at the presentation layer via a
breakpoint on `blit_to_screen`'s own entry, not a `play()`-internal
address) and `--fn-digest-out` (bind.cpp's own synchronous entry/exit
trampoline wrapping the redirected call itself, no hardware breakpoint at
all -- confirmed working, see below). `--print-globals` (evaluated at
`carrier_shutdown()`, independent of any tick sensor) is the third tool,
used throughout this pass's own investigation.

### 3. Per-invocation fn sensor: EQUAL

`--bind play=original --fn-digest-out A` (needs `--stop-at-tick 3200`, not
2528 -- `play()` does not RETURN until the recording's own post-game
high-score sequence finishes, at input-script tick 3047; 2528 kills the
process before the return-site breakpoint ever fires, an easy trap this
pass fell into once and is recording here) vs `--bind play=src
--fn-digest-out B`, `human_test.txt`:

**EQUAL (1 invocation)** -- `fn=play k=0 T=220 args= pre=e3b0c44...
post=e3b0c44... eax=00000000 form=original` (both sides; `pre`/`post` are
SHA256 of an empty domain, correct for a function with no `fn_domains.json`
entry -- the same "EAX only" default class as `get_gamepad`/`ok_to_play`).

### 4. Frame oracle: gameplay region EQUAL, results/rank/initials-entry
region diverges in TIMING only (already documented as IN-VIVO-PENDING)

Comparing frame-digest files by MATCHED TICK VALUE (not by line position --
see below for why line position is the wrong axis once `play` breaks the
tick safepoint) between an unbound baseline and `--bind play=src`, both run
with `--run-seconds` alone (no `--stop-at-tick`, which would only limit the
unbound side and misalign the comparison):

| workload | common T (gameplay, T<2528) | mismatches | common T (T>=2528) | mismatches |
|---|---:|---:|---:|---:|
| `human_test.txt` | 86 | **0** | 172 | 105 (first at T=2725) |
| `human_test.txt`, `play+handle_player_input+poll_control` | 86 | **0** | 172 | 105 (first at T=2725) |

**Why line position is the wrong comparison axis**: `--digest-out`'s tick
index no longer exists once `play` is bound (finding 2 above), so
frame-digest's own "T" (`det_tick() == virtual_ms/20`, a virtual-clock
label, not a game-tick index) is the only per-line timestamp left, and it
does not accumulate at the same real-time rate in the two runs' RESPECTIVE
idle-menu tails -- an unbound run whose OWN `--stop-at-tick` still works
stops cleanly at the boundary a stored baseline was captured at; a
play-bound run cannot stop early at all and continues, at whatever
wall-clock rate its own idle loop happens to render at on this host, for
the rest of `--run-seconds`. Comparing by LINE POSITION conflates "same
line number" with "same moment", which they are not; comparing by matched
T value is the correct fix and is what the table above does.

**The T>=2725 mismatches are the ALREADY-DOCUMENTED IN-VIVO-PENDING
caveat, not a new bug**: `play.c`'s own header says outright that "the
game-over half's UI loops (results card, rank slide, initials entry,
high-score commit) are driven by `readkey()`/`keypressed()` and by
wall-clock time" and are IN-VIVO-PENDING for exactly that reason. T=2725
falls inside that region (past T=2528, the recording's own truncation
point for ordinary gameplay, before T=3047 where the whole script ends) --
a real-time-driven UI screen legitimately need not render frame-identical
across two separate process launches even when the FINAL COMMITTED outcome
of that screen is deterministic. Two independent things confirm this read
is right, not a rationalization: (a) `--print-globals` at true completion
reads IDENTICAL `fast_forward`/`fast_fast_forward`/`recording`/
`someCounter__play` (0/0/1/2293) in both forms, and (b) `assets/log.txt`
reaches `player qualified for highscore` -> `saving config and scores` ->
`replay_menu launched` -> ... -> `Done...` in both, matching PROMOTIONS.md
batch 11's own already-established score-witness recipe.

### 5. A second, SEPARATE, and NOT harmless finding: the final SCORE differs

Section 4's "gameplay region EQUAL" check only sampled tick-labelled frames
T<=235 in common (86 samples) -- `play=src`'s own frame-digest file has a
huge, silent GAP from T=236 straight to T=2725 (essentially no frames
recorded across the entire middle of the game), so "0 mismatches below
2528" was accidentally checking only the very OPENING of the recording, not
the bulk of it. Following up with `--print-globals`
(`player_id,ply[player_id]->level,ply[player_id]->dead,ply[player_id]->y,
someCounter__play`) at matched-ish tick counts finds the divergence is
real and already well underway by the middle of the game:

| | `someCounter__play` | `ply[player_id]->level` | `ply[player_id]->y` |
|---|---:|---:|---:|
| unbound | 518 | **26** | 232 |
| `--bind play=src` | 545 | **13** | 61.9675 |

The `src` form processes slightly MORE ticks (545 > 518) yet the player has
climbed to roughly HALF the floor -- and the final saved outcome
(`--bind play=src`, `--stop-at-tick 3200`, run to the game's own exit)
confirms it: `assets/profiles/MissingNO/replays/last_game.itr` reads
**score=662, floor=50** (bytes 0x4a/0x4e), not the divergence-009 witness
**score=2386, floor=100** -- MEASURED, and reproduced twice, byte-identical
both times (not a one-off race). `assets/log.txt` still reaches `Done...`;
the process still completes and saves cleanly -- the game just plays
differently, not incorrectly-terminates.

**Ruled out, with evidence, not assumption:**
- **Not a binding/generator-layer cause.** The compile is clean (0 errors,
  the same 5 pre-existing warnings). Every game-scope global this pass
  checked (`recording`, `fast_forward`, `fast_fast_forward`,
  `someCounter__play`, `itrcheck`, `debug`, `ply[player_id]->*`) reads a
  real, correctly-typed value through the ordinary bound macro -- none read
  garbage, none are unreadable except at true process-exit (after the
  guest's own `free(ply[player_id])`, expected). `QueryPerformanceCounter`/
  `Frequency`/`_mkdir`/`_stricmp` (this pass's own new bindings) show
  plausible, non-zero, non-crashing call counts in `--report`'s import
  census (7/4/5/229 respectively) -- a wrong calling convention would much
  more plausibly crash outright than produce a quietly-wrong floor number.
- **Not specific to the "everything bound" combination.** `--bind play=src`
  ALONE (no other function in the bind table at all) already reproduces
  score=662/floor=50 -- confirmed by a fresh, isolated run. Bisecting
  `all_src.bindfile`'s other ~48 rows into two halves plus `play` each
  reproduces the identical score=662/floor=50, ruling out an interaction
  with one specific OTHER bound function as the sole cause.
- **Not present (at least not observably) on the other two workloads' own
  narrower overlap**: `newgame.txt` (0 of 105 common frame samples
  mismatch, T=20..124) and `play_itr.txt` (0 of 373, T=20..392) show no
  divergence over the range each pair's frame-digests happen to overlap --
  but neither was run to ITS OWN final-score/floor outcome this pass
  (`newgame.txt` has no stored score baseline; `play_itr.txt`'s own natural
  end was not independently re-verified against `replays/itr_last_game.digest`'s
  known score). Absence of evidence on these two, not evidence of absence.

**Localization, as far as this pass could take it without a working
per-tick digest**: the two candidate regions are both squarely inside
`play()`'s own recovered text and both explicitly UNVERIFIED OFFLINE per
PROMOTIONS.md batch 12 (which checked only 502 of 17420 bytes) --
`main.c` 3681-3800 ("simulation core": collision dispatch + the inline
scrolling block, `src/icytower/play.c` lines ~740-825) and 3831-3969
("score/floor/combo/death accounting", `play.c` lines ~840-970, including
the `level = (get_level(&map, (int)ply[player_id]->y) - 1) / 10;` /
`ply[player_id]->level = level;` pair at `play.c` lines 900/965). Neither
line was independently confirmed wrong against the original disassembly
byte-for-byte in the time this pass had -- this is a STRONG localization
(where the bug almost certainly lives), not a final diagnosis (which exact
line/operator). **Per this task's own scope, `play.c` is NOT edited and no
further attempt to fix it was made; flagged as a background task instead**
(see `carrier/NOTES.md`'s own copy of this finding and the spawned task).

### 6. `all_src.bindfile` gains a `play=src` row, as instructed, with the
failure documented loudly rather than silently

The task's own recipe asked for exactly this row and this check; the row
is added (`carrier/scripts/all_src.bindfile`, its own comment states the
score divergence in full) because that is what was asked, NOT because the
result is a pass. Two consequences worth stating plainly:
`carrier/scripts/gates.ps1`'s own G4/G5b (both drive `--digest-out` through
this SAME bindfile) will now report 0 ticks for any FUTURE run, for the
reason in finding 2, not a new regression in those gates themselves; and
any future pass that runs this bindfile to completion will get
score=662/floor=50, not 2386/100, until whichever pass owns
`src/icytower/play.c` next investigates finding 5.

### Conclusion

`play()` compiles and links cleanly in the carrier world with two small,
generic generator fixes (calling-convention-aware `GUEST_CRT_IMPORTS`, a
fourth `AL_INLINE_SCREEN_STATE` class) and no `play.c` edit. It is EQUAL
per-invocation (1/1) and EQUAL, frame-for-frame, over the actual gameplay
region of all sampled ticks below the recording's own truncation point.
Past that point, this pass surfaces two DISTINCT findings: a harmless,
purely carrier-layer limitation (the tick safepoint cannot survive `play`
being bound, `--digest-out`/`--stop-at-tick` produce nothing once it is)
and a real, reproducible, NOT-yet-explained gameplay divergence (the
player climbs at roughly half the expected rate, final score 662/floor 50
instead of 2386/100) that this pass localizes to `play()`'s own
UN-verified-offline body but does not fix, per this task's own scope.

---

## In-vivo pass -- play() RESOLVED (divergence 010, 2026-09-08)

This section supersedes findings 2 and 5 of the batch-12 section above.
Both are fixed; `play()` is now verified in vivo. The full account,
including the disassembly evidence for every line changed, is in
`carrier/NOTES.md` "Divergence 010"; what follows is the src/-side
summary.

### Finding 2 (the tick safepoint) was not harmless after all

Batch 12 filed "`--digest-out`/`--stop-at-tick` produce 0 records once
`play` is bound" as a *harmless carrier-layer limitation*. It was the
opposite: it was the reason finding 5 could not be localized, and the
same defect had also silently disabled the FRAME oracle (batch 12's own
"silent GAP from T=236 straight to T=2725" is that, not a property of the
recording). Both instruments were anchored at a guest VA that a promoted
caller never executes:

* `carrier/gen/pf_bindings_src.h` leaves every promoted name FREE
  ("excluded (compiled natively, name kept free)"), so `play.c`'s call to
  `update_player`/`blit_to_screen`/`draw_frame`/... is resolved by the
  LINKER to the carrier's own symbol. The guest address is never
  executed and `bind.cpp`'s entry patch is never crossed.
* Corollary that matters for every future in-vivo pass on a big
  function: **`--bind play=src` also switches every promoted callee of
  `play()` to its src form**, bindfile row or not. An A/B that means
  "what does this function's own body change" must bind the callees on
  both sides (`carrier/scripts/all_rows_src_no_play.bindfile` vs
  `all_rows_src.bindfile`).

Fixed carrier-side: the tick safepoint is now a FUNCTION-BOUNDARY sensor
at `update_player`'s entry (one call site in the whole image,
unconditional in the tick loop), resolved per run to whichever of its two
entry addresses that run can reach; the frame oracle arms both addresses
and de-duplicates. Baseline consequence: `replays/human_test.digest` and
`replays/itr_last_game.digest` were regenerated (same line counts, every
T shifted down by one, because the sensor now sits at main.c 3703 rather
than 4369).

### Finding 5 (score 662 / floor 50) -- four defects in `play.c`, one fatal

With the safepoint restored, three A/B iterations named each first
differing global and the `play.c` line behind it:

1. **T=236, `checkMusicVoiceID` 3 vs 0** -- main.c 3498 was recovered as
   `play_sound(custom.bg_music, 0, 0)`. `0x414199` loads `0x4fac08`,
   which is `custom` + 1232 = `offsetof(Tcustom, yo)`
   (`carrier/gen/it_types_check.c`), not +1240 = `bg_music`. Now
   `play_sound(custom.yo, 0, 0)` -- the character's "Yo!" at the start of
   a game.
   Two more member/argument errors of the same family were found by
   auditing every `custom.*` reference in `play()`'s disassembly against
   the file (the multiset of members did not match: the original uses
   `wazup` twice and `yo` once, the file used `yo` twice, `bg_music`
   once, `wazup` never):
   * main.c 4130 and 4199, the two pause screens: `custom.yo` ->
     `custom.wazup` (`0x412ea0`/`0x413468` load `0x4fac0c` = +1236).
   * main.c 3504 and 3553: `play_sample(bg_beat, 128, 1000, 1, TRUE)` ->
     `play_sample(bg_beat, 0, 128, 1000, TRUE)`. `0x4141c5`-`0x4141e8`
     pushes vol=0, pan=0x80, freq=0x3e8, loop=1; the recovered line had
     the four numbers shifted one argument left (the leading vol=0 was
     dropped).

2. **T=331, `gdLastJumpDiff` 4 vs 2 -- THE BUG.** main.c 3864:

   ```c
   level = (get_level(&map, (int)ply[player_id]->y) - 1) / 10;  /* was */
   level = (get_level(&map, (int)ply[player_id]->y) - 1) / 5;   /* is  */
   ```

   `0x412879`-`0x41288c` is the signed magic-divide idiom `lea
   -0x1(%eax),%ecx / mov $0x66666667,%ebx / imul %ebx / sar %edx / sar
   $0x1f,%ecx / sub %ecx,%edx`. `0x66666667` is the magic constant for
   BOTH `/5` and `/10`; the SHIFT decides -- `sar %edx` with no count is
   one bit = `/5`; `/10` would be `sar $0x2,%edx`. Reading the constant
   and not the shift halved every floor the player reached, for the whole
   game: `ply->level`, the combo accounting, the scroll-speed steps keyed
   on `ply->level`, the saved floor and the score all followed. That is
   exactly the factor of two batch 12 measured (level 26 vs 13, floor 100
   vs 50, score 2386 vs 662).

3. Found while checking the OTHER `0x66666667` site in the same function
   (`0x412a0c`, the genuine `/10`, `sar $0x2`): its final `sub` has the
   operands reversed (`sub %edx,%ecx`), so what it stores is `-(x/10)`.
   main.c 4031: `stars[p].sy = ((new_rand() % 200) << 16) / 10;` ->
   `-(((new_rand() % 200) << 16) / 10)`. The "aight" stars fly UP.
   Not observable in the digest domain (particles live in `stars[]`,
   which is hashed, but the star burst needs `!options.flash`), so this
   one is fixed on disassembly evidence alone.

### Verdicts

| workload | check | result |
|---|---|---|
| human_test | per-tick digest, callees src on both sides | EQUAL, 2293/2293 ticks |
| human_test | frame oracle, `every=1` | EQUAL, 2752/2752 frames |
| human_test | run to the game's own exit, `--bind play=src` alone | `last_game.itr` score=2386 floor=100; `log.txt` reaches `Done...` |
| human_test | same, `--bind-file all_rows_src.bindfile` | score=2386 floor=100 |
| newgame | digest / frames, unbound vs all-bound | EQUAL 876 ticks / EQUAL |
| .itr | G5a unbound twice / G5b all-bound vs baseline | EQUAL 157 / EQUAL 157 |
| gates | G1 / G2 / G3a / G3b / G4 / G5 | all EQUAL |

The game-over / results / high-score / initials regions of `play.c`
remain IN-VIVO-PENDING in the strict sense that no oracle compares them
frame-for-frame (they are `readkey()`- and wall-clock-driven, so two
launches legitimately render a different number of frames there) -- but
their COMMITTED outcome is now checked end to end: the run reaches
`Done...` and writes the witness `.itr` with the right score and floor
through those very screens.

### Scope note

`logfile.c`, `draw_reward.c`, `screenshot.c` and `sound.c` (batch 13,
untracked and in flight while this ran; `logfile.c` does not link -
`pthreadGC2.dll` needs guest-IAT bindings) were temporarily excluded from
the build for this pass, so `play=src` pulled in exactly batch 12's
verified callee set. Batch 13's own in-vivo pass re-measures with them in.

## In-vivo pass -- batch 13, the whole gameplay tick path clean in vivo (2026-09-08)

The re-measurement the note above pointed at. `logfile.c`'s link blocker is
closed (`carrier/gen/gen_bindings.py`'s `GUEST_CRT_IMPORTS` gained
`pthread_mutex_lock`/`pthread_mutex_unlock`, bound through the guest's own
`pthreadGC2.dll` IAT slots 0x514a5c/0x514a60, `__cdecl`, `(void *)` params
-- `carrier/NOTES.md` "Batch 13 lands in vivo" has the full account,
including a real generator-level improvement: `GUEST_CRT_IMPORTS`'
calling convention is now inferred from the DLL, and both generators scan
src/ for any `imports.json` name referenced but not yet bound). All nine
batch-13 functions (`play_sound`, `startGameMusic`, `stopGameMusic`,
`log2file`, `take_screenshot`, `draw_reward`, `get_version_str`,
`syncProfileFromOptions`, `destroy_game_data`) are linked and bound.

Two concurrent batch-14 WIP files (`profile.c`, `replay.c`) hit real, own
compile blockers unrelated to batch 13 (a parameter name and a member
access each colliding with a bound top-level global, the same class of bug
batch 8 documented for `jump_sound`/`stars`) -- temporarily carried in
`carrier/win32_policy.json`'s `scan_exclude` and `carrier/gen/
build_blockers.json`. Both were reverted before this pass finished: batch
14 landed its own fix for both files (on the same checkout) within this
pass's own window, confirmed by a rebuild with the temporary entries
removed (0 errors, gates still all EQUAL). Neither file was touched by
this pass.

### Verdicts

| workload | check | result |
|---|---|---|
| newgame | all nine batch-13 rows + every earlier row bound, digest vs unbound | EQUAL, 876 ticks |
| human_test | same, digest vs unbound (gates.ps1 G4, `all_src.bindfile`, STORED baseline) | EQUAL, 2293 ticks |
| human_test | `assets/log.txt`, unbound vs all-bound (`--stop-at-tick 2528`, inside the tick loop) | byte IDENTICAL |
| .itr (play_itr.txt) | gates.ps1 G5a/G5b (STORED baseline) | EQUAL, 157 ticks |

A run-to-the-game's-own-exit comparison (past the tick loop) was attempted
and found NOT usable as clean evidence, for a reason outside batch 13's own
scope: batch 14's second commit landed more functions with a src form
(`qualify_hisc_table`, `save_replay`, `save_profile`, `save_config`,
`enter_hisc_table`, others) while this pass was measuring, and per
divergence 010 `play=src` unconditionally routes every one of `play()`'s
callees that now has ANY src form through the linker -- so the comparison
stopped being "original vs batch 13's six new rows" and became "100%
original vs 100% batch 14's new post-tick-loop code too". MEASURED: the
all-bound run stalls past `saving replay: .../last_game.itr` for the whole
`--run-seconds` budget instead of reaching `Done...`, while the tick-level
global state up to that point is PROVEN identical (G4/G5b) -- either a bug
in one of batch 14's new functions or a legitimate interactive wait
(`enter_hisc_table`'s name-entry `readkey()` loop, if the player now
newly qualifies) with no further scripted input in the corpus. `hisc.c`/
`replay.c`/`profile.c`/`config.c` are batch 14's own files; flagged
separately, not diagnosed further here.

`log2file`'s own required oracle (its formatted output actually reaching
both `assets/log.txt` and the game global `last_log`) is proven by the
byte-identical `log.txt` comparison inside the tick loop, on top of its
offline 80000-vector oracle. `take_screenshot` is **UNVERIFIED IN VIVO**:
none of the three corpus workloads presses F1 (MEASURED by grep), so the
bind is never entered by any recording in the corpus -- its offline oracle
(batch 13, 80000 vectors) stands alone, same standing rule INVIVO.md
already applies elsewhere in this file for a never-invoked bind.

### Status

The whole gameplay tick path -- `play()` and every callee it reaches, down
to the audio seam, the logger, and the frame renderer's own last game-scope
callee -- now runs as clean source in vivo, verified over all three corpus
workloads against STORED baselines recorded from real gameplay. `carrier/
NOTES.md` "Batch 13 lands in vivo" section 5 has the coverage census (as
of this pass's final rebuild, batch 14's concurrent commits included): 78
of 253 game-scope functions have a src form; the remaining 175 (80716
bytes) are fully ORIGINAL. Of batch 13's own 19-function `play()`
"coastline", batch 14 picked up 13 concurrently; 5 (8261 bytes --
`do_replay_menu`, `draw_results`, `getGameDataXML`, `load_replay`,
`my_alert`) remain fully ORIGINAL and are the next-nearest recovery
workload for a clean gameplay session past the tick loop.
