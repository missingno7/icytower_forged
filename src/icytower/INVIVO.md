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
