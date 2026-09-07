# Promotion log — `src/icytower/`

One row per game function recovered into this directory (win32_pilot.md
SS7a/SS7b), in promotion order. "Offline result" is the outcome of
`carrier/lift/harness/lift_check.py --form src --vectors 20000` (unicorn on
the ORIGINAL bytes vs. `harness/src_check.exe`, compiled straight from this
directory) plus one negative-control byte-flip per function; full detail is
in `artifacts/src_equivalence.json`. "Carrier bind" is the win32_pilot.md
SS3 entry-patch step (original address -> this form) — **not attempted by
this log**; every row below is offline-verified only, per `src/README.md`
SS"Offline verification before anything is bound".

## Batch 1 (earlier pass)

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `update_frame` | 0x406ac4 | 120 | main.c | EQUAL (20000) | zero x87, zero callees | pending |
| `is_solid` | 0x4166dc | 107 | map.c | EQUAL (20000) | pure predicate, negative-control-friendly (no writes) | pending |

## Batch 2 (this pass — win32_pilot.md scale test)

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `jump_player` | 0x418678 | 198 | player.c | **EQUAL** (20000, `double`) | x87 HYPOTHESIS case per win32_pilot.md SS3, but its only FP ops (`sx+sx`, `sx*-2.0`, comparisons) are exact under any x87 precision — the LIFTED form already proved `double` sufficient here (carrier/lift/README.md SS6b), confirmed again for this independently-recovered source. Recovered by hand-tracing the x87 register-stack traffic in the disassembly (`fld`/`fxch`/`fucom`/`fstp` sequence); resisted quick readability — required working out that "keep ST(0), drop ST(1)" is `fstp st(1)`'s actual effect (easy to misread as the reverse) before the branch structure made sense. | pending |
| `getFloorData` | 0x416770 | 107 | map.c | EQUAL (20000) | pure integer row lookup, same `y = 29-((cy+1)>>4)` idiom as `is_solid`; reuses its already-verified `% 16` sign idiom for the offset | pending |
| `reset_map` | 0x4166a4 | 53 | map.c | EQUAL (20000) | smallest candidate; `empty` is set to -1, not 0 (promotion_candidates.md's "zero-fills" description was INFERRED and slightly imprecise — verified now) | pending |
| `add_combo` | 0x40414c | 62 | game_data.c | EQUAL (20000) | original writes 3 struct fields via 3 separate stores (re-reading `comboPosts` between each); simplified to one struct assignment (`Tgd_combo` has no padding, so this is bit-identical, not just equivalent) — the only case this pass where the clean form deliberately diverges from the disassembly's literal instruction sequence | pending |
| `line_intersect` | 0x406b80 | 302 | main.c | **bug fixed 2026-09-07** — GCC `-mfpmath=387 -mno-sse2 -O2`/`-O1`: **EQUAL 0/20000** on all 4 canonical seeds (was DIFFER, 997/20000 census); MSVC `cl.exe` (no `/arch` override): DIFFER, but now only the known x87-precision gap — **49/33/46/48 of 20000** per seed (was 997/985/1011/989), all `comparison domain` (`*px`/`*py`), 0 EAX diffs | the win32_pilot.md SS6a x87 escalation case, plus a second, independent bug found and fixed this pass (`carrier/lift/harness/GCC_X87.md` SS4). **The bug**: the original x87 code guards `ua`/`ub` with `fucom`/`fnstsw`/`test $0x45,%ah`/`je`, which only branches (returns 0) on the *ordered* true case — an unordered (NaN) comparison always falls through, i.e. the original never rejects a NaN `ua`/`ub`. The recovered source instead wrote the first two guards as `if (!(ua >= 0.0)) return 0;` / `if (!(ub >= 0.0)) return 0;`, which — because `NaN >= 0.0` is false and its negation is true — DOES reject on NaN, disagreeing with hardware on the fully-degenerate `D == 0 && numerator == 0` case (e.g. segments sharing their start point: `ua = ub = 0.0/0.0 = NaN`). There, the original binary falls through all four guards, runs `fistp` on `NaN + 0.5` (masked #IA -> "integer indefinite" `0x80000000`), writes `x1 - 2147483648`/`y1 - 2147483648` to `*px_out`/`*py_out`, and returns **`EAX = 1`** — while every recompiled form (GCC and MSVC alike, before this fix) returned `EAX = 0` on those ~5% of vectors (948-1011/20000 per seed, toolchain-independent). **The fix**: rewrite the two guards as direct `if (ua < 0.0) return 0;` / `if (ub < 0.0) return 0;` (the `ua > 1.0`/`ub > 1.0` guards were already written this way and needed no change) — C's IEEE-754 comparison semantics then reproduce the same "NaN never fails a guard" fall-through as the hardware's unordered `fucom`, with no change to arithmetic order (`ua`/`ub` still kept in local `double`s, same operation order as the disassembly, same `(int)(ua*dx1+0.5)` truncation) and no change to the pre-existing, separately-tracked x87-precision gap. Re-cross-checked against the already-offline-verified `carrier/lift/lifted/lifted_line_intersect.c`. Negative control (byte-flip) re-run and unchanged: `DIFFER at vector 5: *px+0x2 (VA 0x00794002) original 0x00 lifted 0x01`. | **pending a bit-exact build config** (GCC `-m32 -mfpmath=387 -mno-sse2 -O1`/`-O2` is now proven EQUAL; MSVC is not); do not bind at 0x406b80 until the carrier build adopts one |
| `get_gamepad` | 0x4017fc | 10 | control.c | EQUAL (20000, trivial — 0 args) | `return &gamepad;` — address-free because the compiler computes the address, no literal appears in source | pending |
| `is_up` | 0x401844 | 22 | control.c | EQUAL (20000) | one of 8 `Tcontrol.flags` bit-test predicates, see below | pending |
| `is_down` | 0x40185c | 22 | control.c | EQUAL (20000) | " | pending |
| `is_left` | 0x401874 | 17 | control.c | EQUAL (20000) | " | pending |
| `is_right` | 0x401888 | 22 | control.c | EQUAL (20000) | " | pending |
| `is_fire` | 0x4018a0 | 22 | control.c | EQUAL (20000) | " | pending |
| `is_pause` | 0x4018b8 | 22 | control.c | EQUAL (20000) | " | pending |
| `is_enter` | 0x4018d0 | 22 | control.c | EQUAL (20000) | " | pending |
| `is_any` | 0x4018e8 | 22 | control.c | EQUAL (20000) | tests every bit except `CTRL_PAUSE` (mask `~0x40`) | pending |

`is_up`..`is_any` (all in `src/icytower/control.c`): `Tcontrol.flags` (an
`unsigned char`, DWARF-confirmed but with no bitfield names — game_types.h
has only the raw byte) packs one bit per direction/action. The bit
assignment (`CTRL_LEFT=0x01` .. `CTRL_PAUSE=0x40`) was **not** recoverable
from DWARF; it was read off each function's `and eax, <mask>` in
`artifacts/disasm.txt`, one mask per function — the only construct in this
CU that resisted full DWARF-backed recovery. All eight return the x86
"boolean as -1/0" idiom (all-bits-set for true), not 0/1, confirmed by the
offline check.

## Skipped this pass

| function | VA | size | CU | why skipped |
|---|---|---:|---|---|
| `play_jump_sound` | 0x406ecc | 141 | main.c | Two independent reasons, either alone sufficient: **(1)** its real comparison domain, per `notes/promotion_candidates.md` SS4.3, is "which of 3 sound-handle values reached the downstream `play_sound()` call" — a call-trace comparison, not a memory-domain one, and `carrier/lift/harness/lift_check.py` only does memory-domain diffing; extending it to trace calls is new harness machinery, out of scope for an additive per-function spec. **(2)** the 3 sound-handle globals it reads (VAs 0x4fabf4/0x4fabf8/0x4fabfc) have no DWARF-recovered name (confirmed against `carrier/gen/interop_index.json`'s full 156-global list) — `src/` cannot declare an address-free extern for them without inventing a name and a binding outside the generated pipeline (`carrier/gen/gen_bindings.py` only emits bindings for names already in `interop_index.json`). Both are "the harness cannot model this domain" in the sense win32_pilot.md's task brief anticipates; not attempted. |

## Totals

| | this pass | cumulative (both passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 14 | 16 |
| functions skipped (documented) | 1 | 1 |
| clean source lines (`wc -l` of the .c files) | 341 | 468 |
| original bytes recovered | 903 | 1130 |

## Batch 3 (2026-09-07 — this pass)

15 functions, grouped by CU, all pure integer (0 x87 instructions in any of
them — confirmed reading `artifacts/disasm.txt` instruction by instruction
for every one) and either leaf or calling only plain globals already bound
through `pf_bindings_src.h`/`pf_bindings_harness.h`. Picked in the task
brief's stated order of preference: category (1) (gameplay-reachable —
`cycle_counter`/`fps_counter` directly drive `logic_count`, the same global
`update_frame.c` already depends on; `control.c`'s three additions complete
the per-tick input CU batch 2 started) first, falling back to category (3)
(remaining leaf functions) for the rest, since no tractable category-(2)
candidate (an Allegro- or asset-calling function) survived triage this pass
without either needing the not-yet-built call-trace domain or depending on
an unpromoted function — see "Skipped this pass" below.

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `set_control` | 0x4017d4 | 38 | control.c | EQUAL (20000) | DWARF-named via `DW_AT_abstract_origin` resolution (see control.c's header comment) — `gen_interop.py`'s name pass never follows that back, so this function had no name anywhere in the generated pipeline (`it_funcs.h`, `functions.json`) despite DWARF actually carrying one; `--exclude set_control` on `gen_bindings.py` is therefore a documented no-op (nothing was ever bound under that name to redirect). Binds the 5 remappable keys at once. | pending |
| `init_control` | 0x401790 | 67 | control.c | EQUAL (20000) | Recovered as a literal inlined call to `set_control()` (5 defaults) plus 4 more field stores — the original source's own structure, not a hand re-inlining. | pending |
| `check_control_key` | 0x401808 | 58 | control.c | EQUAL (20000) | Same -1/0 idiom as `is_up`..`is_any`; true if `key` matches any of the 7 bindable fields (not `use_joy`/`flags`). | pending |
| `get_level` | 0x416748 | 40 | map.c | EQUAL (20000) | Third `y = 29-((cy+1)>>4)` row-lookup consumer alongside `is_solid`/`getFloorData`; unlike those two, does not gate on `room[y].empty`. | pending |
| `add_jump_sequence` | 0x4040f4 | 87 | game_data.c | EQUAL (20000) | `add_combo`'s sibling in the same CU: same bounded-append-then-increment shape, `Tgd_jump_sequence` (3 plain ints, no padding) written as one struct assignment for the same bit-identical reason `add_combo.c` already established for `Tgd_combo`. `jumpPosts` offset (0xeaa4) and `jumps[]` base (0xeaa8) cross-checked against `artifacts/dwarf_info.txt`'s `DW_AT_data_member_location` directly (60068/60072 bytes), not just against the disassembly. | pending |
| `reset_particles` | 0x418420 | 27 | particle.c | EQUAL (20000) | Zeroes only `intensity` (the "free slot" marker) across all 512 elements of the array `p` points at, not a single particle and not the whole struct. | pending |
| `scroll_scroller` | 0x41f0c0 | 14 | scroller.c | EQUAL (20000) | Single field add (`offset += step`). | pending |
| `restart_scroller` | 0x41f0d0 | 28 | scroller.c | EQUAL (20000) | Snaps `offset` to `height` (vertical) or `width` (horizontal). | pending |
| `cycle_counter` | 0x41fed4 | 16 | timer.c | EQUAL (20000) | `cycle_count++` — the free-running tick counter driving the game's own pacing. | pending |
| `fps_counter` | 0x41fea4 | 45 | timer.c | EQUAL (20000) | Once-per-second sample-and-reset of `frame_count`/`logic_count` into `fps`/`lps`; `logic_count` is the same global `update_frame.c`/`is_solid.c` already depend on (`src/README.md`'s five-global list). | pending |
| `get_demo` | 0x40696c | 10 | main.c | EQUAL (20000) | `return demo;` — a raw pointer VALUE relayed verbatim, never dereferenced, so (unlike `get_gamepad`/`get_controls`) no host/guest translation is needed anywhere. | pending |
| `get_controls` | 0x406978 | 10 | main.c | EQUAL (20000) | `return &ctrl;` — same "compiler computes the address" idiom as `get_gamepad`, address-free. | pending |
| `switchedFromProgram` | 0x406a5c | 15 | main.c | EQUAL (20000) | Allegro window-focus-lost callback: `hasFocus = 0`. | pending |
| `switchedToProgram` | 0x406a6c | 15 | main.c | EQUAL (20000) | Allegro window-focus-gained callback: `hasFocus = 1`. | pending |
| `clickedCloseButton` | 0x406a7c | 15 | main.c | EQUAL (20000) | Allegro window-close-button callback: `closeButtonClicked = 1`. | pending |

Every row's negative control: one bit of the SRC side's vector-5 result
flipped by the harness (`--fault <fn>:5:0`, 200 vectors), comparator names
the exact byte (e.g. `Tcontrol+0x0 (VA 0x00796000)`, `jumpPosts+0x0 (VA
0x007aeaa4)`, `frame_count+0x0 (VA 0x00506978)`) and reports "1 of 200
vectors differ" — full detail in `artifacts/src_equivalence.json`.

### Skipped this pass

| function | VA | size | CU | why skipped |
|---|---|---:|---|---|
| `update_particle` | 0x41843c | 83 | particle.c | Calls `new_rand()` (0x406984, main.c, not yet promoted) twice — an x87 float LCG with its own control-word save/restore, structurally similar in difficulty to `line_intersect`'s x87 escalation. Promoting it would mean either (a) executing a copy of `new_rand`'s original bytes from inside `harness/src_check.exe`, which the offline harness's `pf_guest` buffer cannot do (it is a plain `malloc` region, deliberately not `VirtualAlloc`'d executable — `src_check.c`'s own header comment explains why a fixed-address executable mapping was ruled out), or (b) promoting `new_rand` itself first. Neither is attempted this pass; left for a future batch once `new_rand` is recovered. |
| `create_particle` | 0x418490 | 192 | particle.c | Same reason as `update_particle`: calls `new_rand()` twice (to seed `sx`/`sy`) and depends on it for its interesting behavior. |
| `destroy_game_data` | 0x40418c | 12 | game_data.c | Tail-jumps straight into `free()` (`jmp _free`, no `call`) — semantically a one-line `free(gd)` wrapper, but its only observable effect is heap-allocator-internal state with no comparison domain the offline harness can express (unlike a memory write, freeing a block leaves no game-owned bytes to diff), so a 20000-vector offline pass would only ever prove "did not crash", not "matches". Deferred, not attempted. |
| `get_version_str` | 0x406960 | 10 | main.c | Returns a literal `.rdata` string address (0x4d4b20), not a named global — recovering it cleanly needs the actual string bytes extracted from the image, which this pass did not do (no existing artifact carries them). Deferred. |
| `ok_to_play` | 0x406a50 | 10 | main.c | `return 1;` unconditionally, no globals, no domain worth a dedicated harness entry beyond EAX alone; left out of this batch's 15 for headroom, not for any recovery difficulty (trivially `int ok_to_play(void) { return 1; }` whenever picked up). |

### Call-trace domain

**Not implemented this pass.** None of the 15 functions promoted calls
Allegro or an asset accessor (the task brief's category (2)); the two
functions that do call something interesting (`update_particle`,
`create_particle`) call a *game* function (`new_rand`), not a library
import, and are deferred above for that reason rather than triggering the
call-trace-domain work. `carrier/lift/harness/lift_check.py`/`src_check.c`
still only support the memory-domain comparison this pass builds on
additively; a future pass that actually reaches an Allegro-calling
candidate (e.g. `draw_star_field`, `init_scroller`, `load_control`/
`save_control` calling `fread`/`fwrite`) is where that machinery would
first be needed.

### Totals (updated)

| | batch 3 (this pass) | cumulative (3 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 15 | 31 |
| functions skipped (documented, all passes) | 6 | 7 |
| clean source lines (git-diff insertions for this pass's touched files) | 316 | 784 |
| original bytes recovered | 485 | 1615 |

`git diff --stat`-style file list this pass: `control.c` (+73 lines: 3 new
functions — `set_control`, `init_control`, `check_control_key` — plus header
comment), `map.c` (+35 lines: `get_level` plus header comment),
`add_jump_sequence.c` (31 lines, new file), `particle.c` (33 lines, new
file), `scroller.c` (33 lines, new file), `timer.c` (44 lines, new file),
`main_state.c` (67 lines, new file).

`draw_buffer` (`src/icytower/ASSETS.md`, compile-only, pixel-output domain
not memory-diffable) stays outside this table's counts, unchanged from
that document.

## Purity gate (updated)

```
python scripts/check_native_layer.py
check_native_layer: scanned 23 file(s) under .../src, 0 violation(s)
```

## Compile (both worlds, batch 3)

```
standalone: cl /nologo /c /W3 /TC /Isrc\icytower
            src\icytower\update_frame.c src\icytower\is_solid.c
            src\icytower\jump_player.c src\icytower\map.c src\icytower\add_combo.c
            src\icytower\add_jump_sequence.c src\icytower\line_intersect.c
            src\icytower\control.c src\icytower\particle.c src\icytower\scroller.c
            src\icytower\timer.c src\icytower\main_state.c src\icytower\state.c
            -- 0 errors, 0 warnings

carrier:    python carrier\gen\scan_src_defs.py --src-dir src\icytower   (35 names,
            auto-scanned -- carrier\gen\SCAN_SRC_DEFS.py's whole purpose)
            python carrier\gen\gen_bindings.py --exclude <scanned 35 names> ^
                --guard-define ICYTOWER_BINDINGS_ACTIVE ^
                --out carrier\gen\pf_bindings_src.h --types-out carrier\gen\pf_bindings_src_types.h
            -- 0 reserved_collisions, functions_excluded: 31 of 242 (4 of the 35
               scanned names are not in the 242-function game-scope set --
               see the "harness_changes" note in artifacts/src_equivalence.json's
               "pass_2026-09-07_batch3" entry)

            cl /nologo /c /W3 /TC /Icarrier\gen /FIpf_bindings_src.h
               src\icytower\update_frame.c src\icytower\is_solid.c src\icytower\jump_player.c
               src\icytower\map.c src\icytower\add_combo.c src\icytower\add_jump_sequence.c
               src\icytower\line_intersect.c src\icytower\control.c src\icytower\particle.c
               src\icytower\scroller.c src\icytower\timer.c src\icytower\main_state.c
            -- 0 errors, 0 warnings (carrier world)
```

No function promoted this pass calls an Allegro import, so — as in batch
2 — the `/FIcarrier\gen\pf_lib_bindings.h` question did not arise.

`git diff --stat`-style file list this pass: `jump_player.c` (81 lines, 198
original bytes), `map.c` (68 lines, 160 original bytes for 2 functions),
`add_combo.c` (27 lines, 62 original bytes), `line_intersect.c` (117 lines
as of the 2026-09-07 EAX-bug fix above; 302 original bytes; GCC-x87-build
EQUAL, MSVC still DIFFER on the pre-existing precision-only gap — see
above), `control.c` (86 lines, 903
- 198 - 160 - 62 - 302 = 181 original bytes for 9 functions).

## Purity gate

```
python scripts/check_native_layer.py
check_native_layer: scanned 14 file(s) under .../src, 0 violation(s)
```

## Compile (both worlds, win32_pilot.md SS7a)

```
cl /nologo /c /W3 /TC /Isrc\icytower src\icytower\update_frame.c src\icytower\is_solid.c
   src\icytower\jump_player.c src\icytower\map.c src\icytower\add_combo.c
   src\icytower\line_intersect.c src\icytower\control.c src\icytower\state.c
   -- 0 errors, 0 warnings (standalone world)

cl /nologo /c /W3 /TC /Icarrier\gen /FIpf_bindings_src.h
   src\icytower\update_frame.c src\icytower\is_solid.c src\icytower\jump_player.c
   src\icytower\map.c src\icytower\add_combo.c src\icytower\line_intersect.c
   src\icytower\control.c
   -- 0 errors, 0 warnings (carrier world; carrier/gen/pf_bindings_src.h and
      carrier/lift/harness/pf_bindings_harness.h were regenerated with the
      wider --exclude list covering all 14 new names)
```

No function promoted this pass calls an Allegro import directly, so the
`/FIcarrier\gen\pf_lib_bindings.h` question from the task brief did not
arise; `play_sound` (a game function, not Allegro) already has a prototype
in the generated `game_funcs.h` and would need no special handling if
`play_jump_sound` is revisited later.

## Batch 4 (2026-09-07 — this pass)

`new_rand` (0x406984, main.c) first — the game's own x87 float LCG that
blocked `update_particle`/`create_particle` in batch 3 (see that batch's
"Skipped this pass" table) — then its two integer callers, now that they
can call the clean `new_rand()` directly instead of needing to execute a
copy of its original bytes. `ok_to_play` closes out the rest of batch 3's
skip list that was skipped for headroom, not difficulty.

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `new_rand` | 0x406984 | 128 | main.c | **EQUAL** (80000 = 4 seeds × 20000, GCC `-m32 -mfpmath=387 -mno-sse2 -O1`/`-O2`); MSVC DIFFER 6079/20000 (seed 20260907), precision-only | x87 float LCG: `x = 1.4294484665 * seed; seed = x; if (x > 65535.0) { do { x -= 65535.0; } while (x > 65535.0); seed = x; } return (int)((x - (int)x) * 65535.0);`. The `do/while` (not a one-shot `if`) matters: the original's `fucom`/`je` pair after the fold is a real loop, reachable whenever `|seed|` is large enough that `MULTIPLIER*seed` exceeds `2*MODULUS` — a one-shot `if` would only be exact for the bounded, in-gameplay range of `seed`. Recovered by hand-simulating the x87 stack traffic in `artifacts/disasm.txt` (0x406984-0x406a03), then *directly executing the original bytes in unicorn* (not just reading the disassembly) over ~3200 independently-generated seeds against a Python double re-implementation: 0 EAX mismatches, confirming the algorithm; the ~38% of vectors where the returned *seed* differed in its last bit or two is exactly the double-vs-80-bit gap win32_pilot.md SS6a predicts, reproduced again below at the source level. One real mistake this cross-check caught before it reached source: the `fsubr %st,%st(1)` instruction computes `ST(1) = ST(0) - ST(1)` (i.e. `x - MODULUS`), not the reversed `MODULUS - x` the mnemonic's name alone suggests — an early hand-trace had this backwards and the unicorn cross-check's very first mismatch (at the exact seed engineered to sit on the fold boundary) caught it immediately. | pending |
| `update_particle` | 0x41843c | 83 | particle.c | **EQUAL** (80000, GCC); MSVC DIFFER 6143/20000, precision-only (propagates `new_rand`'s) | Advances `x`/`y` by `sx`/`sy` (all `fixed` 16.16, plain int32 — no FPU of its own), adds a constant `0x4ccd` to `sy` (gravity), ages `intensity--`, and — 1 time in 5 (`new_rand() % 5 == 1`) — rerolls `color` to `new_rand() % 8`. Validated the same way as `new_rand`: original bytes run in unicorn over 2000 random (particle, seed) pairs against a hand-written Python model, 0 mismatches — after the model's own first draft was caught using Python's floor-style `%` instead of C's truncating one on the negative-seed vectors (the fix, `new_rand() % 5`/`% 8` in the C source itself, needs no such care — C's `%` already matches `idiv`). | pending |
| `create_particle` | 0x418490 | 192 | particle.c | **EQUAL** (80000, GCC); MSVC DIFFER 5181/20000, precision-only (propagates `new_rand`'s) | Scans `p[0..511]` for the first `intensity == 0` slot; on a miss, returns 0 and touches nothing (confirmed against the disassembly's `xor si,si` path directly, not assumed — batch 3's skip note did not commit to a return value). On a hit: `x`/`y` become `x<<16`/`y<<16`, `intensity = 0xff`, `color = new_rand() % 8`, and `sx`/`sy` are drawn from two more `new_rand()` calls as `(((new_rand()%50)-25)<<16) / 10` and `/ 50` respectively. The two integer divisors (10 and 50) were **not** guessed from update_particle.c's visually similar `%5`/`%8` pattern — a first attempt assumed `/5` and `/10` by that analogy and was wrong; both were re-derived by testing the disassembly's two magic-number multiply/shift sequences (`0x66666667`/shift 2, `0x51eb851f`/shift 4) against a dense sweep of plausible plain-C divisors until an exact match was found (10 and 50), then confirmed against 2000 unicorn-executed vectors covering both the found-a-slot and no-free-slot paths, 0 mismatches. | pending |
| `ok_to_play` | 0x406a50 | 10 | main.c | EQUAL (20000, trivial — 0 args, 0 domain bytes, EAX only) | `return 1;` unconditionally. Negative control **not attempted**: its comparison domain is empty (no memory bytes, only a constant EAX), and the harness's `--fault` mechanism flips one *domain* byte — there is none here to flip. Not the same as an oversight; every other promoted function so far has had at least one domain byte. | pending |

Every row's negative control (except `ok_to_play`, above): one bit of the
SRC side's vector-5 result flipped by the harness (`--fault <fn>:5:0`, 200
vectors), comparator names the exact byte — `seed+0x0 (VA 0x004ff108)` for
`new_rand`, `Tparticle+0x0 (VA 0x007a4000)` for `update_particle`,
`Tparticle[512]+0x0 (VA 0x007a4000)` for `create_particle` — full detail in
`artifacts/src_equivalence.json`.

### GCC x87 build (win32_pilot.md SS6a, second measurement)

`new_rand`/`update_particle`/`create_particle` are this project's second
independently-recovered x87 function family (after `line_intersect`), and
the result reproduces SS6a's rule exactly: `harness/gcc_check.c` (extended
from wiring only `line_intersect`/`jump_player` to also wire these four,
`seed` given read-**write** storage+sync since — unlike `jump_player`'s
read-only globals — `new_rand` mutates `seed` and that mutation is the
compared domain) built with 32-bit MinGW GCC 16.2.0 at `-m32 -mfpmath=387
-mno-sse2 -O1`/`-O2` is bit-exact over 80000 vectors (4 canonical seeds ×
20000) for all three functions; MSVC (plain `double`/SSE) DIFFERs on
26-31% of vectors, all attributable to the same double-vs-80-bit precision
gap, not a logic error (each function's own row above, and
`artifacts/src_equivalence.json`, separate the two). The differ *fraction*
is far higher than `line_intersect`'s (0.1-5%) because `new_rand` has no
single "mostly exact" truncation point — every call re-derives its result
from a fresh multiply-fold-fractional-part chain, so precision sensitivity
does not average out the way it does across `line_intersect`'s wider
[0,1] `ua`/`ub` range.

### Totals (updated)

| | batch 4 (this pass) | cumulative (4 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 4 | 35 |
| functions skipped (documented, all passes) | 4 (destroy_game_data, get_version_str carried over; `update_particle`/`create_particle` graduated out of the skip list) | 3 |
| original bytes recovered | 413 (128 + 83 + 192 + 10) | 2028 |

`git diff --stat`-style file list this pass: `new_rand.c` (new file, 91
lines with comments), `ok_to_play.c` (new file, 12 lines), `particle.c`
(+69 lines: `update_particle`/`create_particle` added, header comment
rewritten to drop the "deferred" framing).

## Generator gap fix (2026-09-07, this pass)

**Problem** (found while promoting this batch): some concrete function
instances have DWARF that carries only `DW_AT_abstract_origin` on their
`DW_TAG_subprogram` — GCC's output for a function that is *also* inlined
somewhere splits its DWARF into an "abstract instance" (name, return type,
full parameter list, but no `DW_AT_low_pc`) and one out-of-line
`DW_TAG_subprogram` per real, addressed, callable copy (`DW_AT_low_pc`/
`DW_AT_high_pc`, but only `DW_AT_abstract_origin` — no name of its own,
and its `formal_parameter` children carry only `DW_AT_abstract_origin` +
`DW_AT_location`, no type). `carrier/gen/gen_interop.py`'s
`collect_functions()` and `carrier/gen/gen_src_headers.py`'s
`collect_functions_named()` both required a direct `DW_AT_name` and
silently skipped anything without one — exactly `set_control`'s situation
(control.c's own header comment already flagged this as a "documented
no-op" for `gen_bindings.py --exclude`; the actual root cause is here).

**Fix**: `carrier/gen/gen_interop.py` gained `follow_origin(off, dies)`,
which walks `DW_AT_abstract_origin`/`DW_AT_specification` chains to the
DIE that actually carries the declaration, used in `collect_functions()`
(subprogram name/return-type/external/prototyped, and per-parameter type
when a formal_parameter itself carries only an origin reference) whenever
`DW_AT_name` is absent. `gen_src_headers.py`'s `collect_functions_named()`
(needs parameter *names* too) reuses the same `gi.follow_origin()` helper.
Also fixed, found only because this batch's `particle.c` is the first
`src/` file to `#include "game_funcs.h"`: every one of that generated
file's prototypes is now wrapped `#ifndef <name>` / `#endif`, because
`carrier/gen/pf_bindings_src.h`, force-included ahead of it, `#define`s
every *not-yet-promoted* game function's plain name to an address-cast
expression — declaring that same name again (unconditionally, as before)
macro-expands into a syntax error (MSVC C2059), not a harmless
redeclaration; the guard makes `game_funcs.h` declare a name only when no
such macro already won it.

**Result**: 11 previously-unnamed functions gained real names —
`set_control` (0x4017d4), `generate_checksum` (0x404a50), `new_srand`
(0x406a04), `syncProfileFromOptions` (0x406a14), `update_reward`
(0x406a8c), `is_custom_replay` (0x406b3c), `hash3` (0x418184), `hash2`
(0x4189cc), `get_rank_id` (0x418a84), `get_rank` (0x418ad0), `hash`
(0x41b9c8) — taking the game-scope function count from 242 to 253, 0
regressions (nothing lost a name). `artifacts/functions.json` /
`tools_recon/build_functions.py` were **not** the source of the gap (that
pipeline is COFF/disassembly-based, independent of `gen_interop.py`'s
DWARF walk) and were not touched. Regenerated: `it_types.h`, `it_globals.h`,
`it_funcs.h`, `it_funcs_table.inc`, `interop_index.json`,
`INTEROP_NOTES.md`, `game_funcs.h` (+11 prototypes, `#ifndef`-guarded),
`game_types.h`/`game_state.h`/`state.c` (byte-identical — no type or
global changed), `pf_bindings_src.h`/`pf_bindings_harness.h`/
`pf_bindings.h` (+ their `_types.h` twins), all via the existing
generators, no hand-editing.

**Verification**:
- Purity gate: `python scripts/check_native_layer.py` — scanned 25 file(s), 0 violation(s).
- Carrier-world compile: `cl /nologo /c /W3 /TC /Icarrier\gen /FIpf_bindings_src.h` over all 14 promoted-batch `src\icytower\*.c` files (batches 1-4) — 0 errors, 0 warnings.
- Upstream-world build: `mingw32-make -f src\build\Makefile.standalone` — `libicytower.a` + `standalone_smoke.exe` build clean; `standalone_smoke.exe` run: 16/16 PASS, 0 failure(s) (that Makefile's `SOURCES` list is a hand-maintained subset predating batches 3-4 and was left untouched — this is a regression check on the existing target).
- Offline harness, all 31 batch 1-3 functions rerun at 20000 vectors each: 30/31 still EQUAL, unchanged; `line_intersect` still DIFFERs under MSVC exactly as already documented (the pre-existing, separately-tracked x87 precision gap) — confirming this pass's generator changes introduced no regression.
