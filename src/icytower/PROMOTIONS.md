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

## Batch 5 (2026-09-07 — add_floor pass)

`add_floor` (0x4167dc, 608 bytes, map.c) — the tower layout generator,
`is_solid`/`getFloorData`/`get_level`'s producer counterpart, and the
subject of `notes/layout_determinism.md`/`notes/layout_rules_1.5.1.md`.
The one function this pass set out to recover; no other candidate was in
scope.

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `add_floor` | 0x4167dc | 608 | map.c | **EQUAL** (4 seeds x 20000 = 80000 vectors, MSVC `src_check.exe`); **EQUAL** (4 seeds x 20000 = 80000 vectors, GCC `-m32 -mfpmath=387 -mno-sse2 -O2`, `gcc_check_x87_nosse_O2.exe`) | Domain: the whole 772-byte `Tmap` (32 x `Tfloor`, no return value). GCC x87 is the toolchain of record — the `floor_shrink!=0 && new k<=2999` branch keeps a `fidivr`/`fmuls` pair on the x87 register stack, the same shape as `line_intersect`/`new_rand`'s x87-sensitivity; MSVC (plain `float`/SSE) happened to also come back EQUAL on all 4 seeds tested, unlike those two functions, but the vector generator does not specifically target this ratio's float-truncation boundary the way `line_intersect`'s `_boundary()` helper does, so an MSVC divergence here is "not found in 80000 vectors", not "ruled out" — see `notes/layout_rules_1.5.1.md` SS3. Two bugs found and fixed this pass, both below. Re-verified after divergence 008 (see the addendum below this table): still **EQUAL** on 4 seeds x 20000 for both toolchains, now with a strengthened directed vector class. | **EQUAL in vivo** (533 invocations, `replays/human_test.txt`; per-tick digest EQUAL 2293 ticks) |

Negative control: `--fault add_floor:5:0`, 200 vectors, comparator names
`Tmap+0x0 (VA 0x00792000)` exactly — full detail in
`artifacts/src_equivalence.json`. Re-run after divergence 008 (GCC, 100
vectors): still `DIFFER at vector 5 ... Tmap+0x0 (VA 0x00792000)`, so the
strengthened generator has not blunted the oracle.

### Addendum — divergence 008 (2026-09-07): the rand() binding

`add_floor` passed 160 000 offline vectors and then **DIFFERed in vivo** at
`k=5 T=220 field=post`. The recovered rules were not at fault: the
divergence was that `map.c`'s `rand()` linked to the **carrier's own** CRT
instead of the **guest's** msvcrt import, so it drew the tower layout from a
never-seeded generator with a state of its own. `src/icytower/INVIVO.md`
"Divergence 008" carries the byte-level evidence (`map.room[7].start_tile`/
`.end_tile`, and the pinned `rng_state`/`rng_calls` measured on both sides);
`notes/living_record.md` entry 008 carries the narrative. Fixed in
`carrier/gen/gen_bindings.py` (`GUEST_CRT_IMPORTS` binds `rand`/`srand` to
the guest's IAT slots), not in `map.c`, which is unchanged apart from a new
header note.

**Two things this pass changed about how `add_floor` is verified**, both
worth carrying forward to the next promoted function that calls a library
function:

1. **The offline oracle cannot see a binding.** `lift_check.py`
   force-includes `carrier/lift/harness/pf_harness_rand.h`, which redirects
   `map.c`'s `rand()` to the harness's own per-vector-seeded LCG on BOTH
   sides. That is the right thing for an offline check — but it means the
   harness tests the *algorithm* and structurally cannot test *which copy of
   the library the real build reaches*. In vivo stays the authority for any
   function whose behaviour depends on library STATE.
2. **The directed vector class was one-sided and is not any more.**
   `gen_add_floor` paired its hand-picked level pool with
   `floor_shrink = 0 if k % 2 == 0`, so an even-indexed pooled level was
   never tested with `floor_shrink != 0` (the float-ratio branch) and an
   odd-indexed one never with `floor_shrink == 0` — half of every boundary
   went untested in the branch it was chosen for. It now enumerates the full
   `(pooled level) x (floor_shrink in {0,1}) x (floor_size in 0..4)`
   cross-product: 750 directed vectors, which include the exact in-vivo
   precondition (`level=5, floor_shrink=1, floor_size=1` — MEASURED from
   `Treplay+0x8c..0x90` in a tick-220 snapshot of `human_test.txt`).

### Bug 1: the harness let a raw guest VA reach a real pointer dereference

`add_floor` is the first promoted function to call `get_demo()`
*internally* (not through its own parameter) and then dereference the
result (`get_demo()->floor_shrink`). `get_demo()`'s own PROMOTIONS.md entry
above says its pointer return needs "no host/guest translation ... needed
anywhere" — true when nothing dereferences it (its own offline test),
false the moment a caller does. `src_check.c`'s vector-driven `demo` write
stored a guest VA (the wire format every pointer-shaped write uses); the
very first multi-vector run segfaulted because nothing translated it to a
host pointer before `add_floor` dereferenced it. Fixed with the same
"second translation, driver-side, immediately before the call" pattern
`update_frame`'s `ply[player_id]` fixup already established in
`src_check.c`'s own header comment. `gcc_check.c` never had this bug — its
`demo` is a plain global the driver already assigns through `tr()`
explicitly.

### Bug 2: two wrong magic-multiply divisors

`room[31].tiles` (`k/500`, capped at 10 past `k=4999`) and `room[31].sign`
(`(new level)/5`, on the 1-in-50 checkpoints that get one) are each a
magic-multiply reciprocal in the disassembly (`0x10624dd3>>5`,
`0x66666667>>1`), not a literal-constant `idiv`. An early hand-read
guessed 20 and 10 by eyeballing the constants against a standard
magic-number table — both wrong. The first 20000-vector run (after bug 1's
fix) DIFFERed at `Tmap+0x2fc` (`tiles`); both divisors were re-derived
correctly by brute-force testing the magic-multiply arithmetic against
every plausible small divisor over a 0..20000 sweep, the same technique
`particle.c`'s `create_particle()` note already documents for its own two
magic constants. Full arithmetic and the SAME/CHANGED/UNKNOWN comparison
against 1.3's `new_floor()` are in `notes/layout_rules_1.5.1.md`.

### Harness changes (additive, none touching `src/`)

- `carrier/lift/harness/lift_check.py`: `Oracle` gained a `rand()`-thunk
  hook (`UC_HOOK_CODE` at `RAND_THUNK_VA=0x4bad18`, the `_rand` IAT thunk
  unicorn cannot otherwise reach — msvcrt.dll is never mapped there) that
  emulates msvcrt's LCG in Python from a per-vector seed and simulates the
  `ret` itself; `Oracle.call()` now loops `emu_start()`, resuming from
  wherever the hook redirects, instead of a single `emu_start` per call.
  Plus `gen_add_floor` + the `add_floor` `SPECS` entry (additive).
- `carrier/lift/harness/pf_harness_rand.h` (new, harness-only): force-included
  macro `#define rand harness_rand`, pulling `<stdlib.h>` in first so only
  `map.c`'s own token is redirected. `carrier/lift/harness/harness_rand.c`
  (new): the compiled-candidate half — the same LCG, its own state word
  `harness_rand_state`, set by both drivers from a scratch VA
  (`RAND_SEED_VA=0x794020`) immediately before calling `add_floor`.
- `carrier/lift/harness/src_check.c`: `add_floor` dispatch branch (with the
  bug-1 `demo`-pointer fixup) + 2 new extern declarations.
- `carrier/lift/harness/build_src.cmd`: file list extended with
  `harness_rand.c`; `/FIpf_harness_rand.h` added.
- `carrier/lift/harness/gcc_check.c`: extended to wire `add_floor` — `demo`
  given its own plain-global storage (synced via `tr()`, correctly, from
  the start) alongside `collision_type`/`max_speed`/`seed`; also needed
  `ctrl`/`hasFocus`/`closeButtonClicked` storage purely to satisfy the
  linker once `main_state.c` (for `get_demo`) was linked in.
- `carrier/lift/harness/build_src_gcc.sh`/`build_src_gcc.cmd`: file list
  extended with `map.c`, `main_state.c`, `harness_rand.c`;
  `-include pf_harness_rand.h` added. `gcc_check_x87_nosse_O2.exe`
  (the `--toolchain gcc` default) rebuilt in place.
- `carrier/gen/pf_bindings_src.h`/`_types.h`,
  `carrier/lift/harness/pf_bindings_harness.h`/`_types.h`: regenerated via
  `scan_src_defs.py`'s auto-scanned `--exclude` list (41 names now, up from
  39 — `add_floor` plus the file-static data table `floor_size_modifiers`,
  which also needed excluding: it collided with the SAME "declaring an
  already-address-bound plain name is a syntax error" class `src/README.md`
  already documents for functions, just for a `static const` data table
  instead of a function definition).

### Totals (updated)

| | batch 5 (this pass) | cumulative (5 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 1 | 36 |
| original bytes recovered | 608 | 2636 |

## Purity gate (updated)

```
python scripts/check_native_layer.py
check_native_layer: scanned 25 file(s) under .../src, 0 violation(s)
```

## Compile (both worlds, batch 5)

```
standalone: cl /nologo /c /W3 /TC /Isrc\icytower
            src\icytower\update_frame.c src\icytower\is_solid.c
            src\icytower\jump_player.c src\icytower\map.c src\icytower\add_combo.c
            src\icytower\add_jump_sequence.c src\icytower\line_intersect.c
            src\icytower\control.c src\icytower\particle.c src\icytower\scroller.c
            src\icytower\timer.c src\icytower\main_state.c src\icytower\state.c
            src\icytower\new_rand.c src\icytower\ok_to_play.c
            -- 0 errors, 0 warnings

carrier:    python carrier\gen\scan_src_defs.py --src-dir src\icytower   (40 function
            names + floor_size_modifiers, auto-scanned)
            python carrier\gen\gen_bindings.py --exclude <scanned names> ^
                --guard-define ICYTOWER_BINDINGS_ACTIVE ^
                --out carrier\gen\pf_bindings_src.h --types-out carrier\gen\pf_bindings_src_types.h

            cl /nologo /c /W3 /TC /Icarrier\gen /FIpf_bindings_src.h
               src\icytower\update_frame.c src\icytower\is_solid.c src\icytower\jump_player.c
               src\icytower\map.c src\icytower\add_combo.c src\icytower\add_jump_sequence.c
               src\icytower\line_intersect.c src\icytower\control.c src\icytower\particle.c
               src\icytower\scroller.c src\icytower\timer.c src\icytower\main_state.c
               src\icytower\new_rand.c src\icytower\ok_to_play.c
            -- 0 errors, 0 warnings (carrier world)
```

## Batch 6 (2026-09-07 — player physics core)

Targets, in the task brief's dependency order from `handle_player_input`
(0x40b3e4) and `play()`: `handle_player_collision_*`, `move_player`/
`update_player`, floor lookups (already done), combo/score helpers. No
`move_player` function exists under that name — `player.c`'s physics trio
is `reset_player`/`jump_player`(done)/`update_player`, so `update_player`
is the "gravity, dx damping, wall bounce" target the brief describes.

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `reset_player` | 0x418550 | 296 | player.c | EQUAL (20000 MSVC + 80000 GCC x87) | Zero-fills every `Tplayer` field the game considers "reset" (status/velocity/counters/combo state) EXCEPT `x`/`y` (new_game sets those separately, notes/player_start_randomness.md) and `angle` (only jump_player's own field, per its own PROMOTIONS.md entry, sets `angle`/`rotate` together). No x87, no callees, no branches — a straight-line store sequence, same shape as `reset_map`. `ccc[5]`/`jcTop[5]`/`jc[5]` are zeroed in reverse-index order in the disassembly (a scheduling artifact, not meaningful); this file writes them via a plain forward `for` loop, bit-identical either way. | pending |
| `update_player` | 0x418740 | 651 | player.c | **EQUAL** (GCC `-m32 -mfpmath=387 -mno-sse2 -O2`, modulo a 7/80000 signaling-NaN-payload wrinkle below — not a logic error); MSVC DIFFER 1873/80000, precision-only | The per-tick physics step: clamps `sy` to `[-100.0, max_speed[collision_type]]` and `sx` to `[-max_speed[collision_type], max_speed[collision_type]]`, integrates `x += sx`/`y += sy`, caps `y` at 1000.0, bounces `x` off the two screen edges (85.0/555.0: clamp to the edge, `sx *= -0.9`, and if the rebound speed is `< -4.0` or `> 4.0` record a hard bounce in `p->bounce`, `±20`), then — unless `status == 0` — applies gravity (`sy += 0.8 + gravity_modifier[get_demo()->gravity]`) and, if still in the launch phase (`status == 1`) with `sy` now strictly positive, advances `status` to 2. Recovered by hand-tracing the x87 register-stack traffic in `artifacts/disasm.txt` (0x418740-0x4189cb) instruction by instruction, then validated by directly executing the ORIGINAL bytes in unicorn against a Python double model over 20000 vectors (the new_rand.c/create_particle.c methodology) — this caught two real mistakes before any C source was written, both below. `gravity_modifier[3]` (VA 0x4bdba8, DWARF-confirmed `double[3]`) is a new global this pass starts reading (already declared in `game_state.h`, unused until now). | pending |

Every row's negative control: `--fault <fn>:5:0`, 200 vectors, comparator
names the exact byte (`Tplayer+0x0 (VA 0x00790000)` for both) — full detail
in `artifacts/src_equivalence.json`.

### Two real recovery mistakes caught by the unicorn cross-check

1. **Strict vs. non-strict `test $0x45,%ah` reading.** A first hand-trace
   treated every `fucom(p(p))?`/`fnstsw`/`test $0x45,%ah` pair as the same
   "unordered-or-less-than" NaN guard divergence 006 (`notes/living_record.md`)
   already established for `line_intersect`. That reading is right for the
   `sy`/`sx` clamp thresholds (`-100.0`/`max_speed[collision_type]`, where
   the clamp target equals the compared constant, so strict-vs-non-strict
   is unobservable) but **wrong** for the screen-edge `x` thresholds
   (555.0/85.0) and the `|bounce speed| >= 4.0` threshold: paired with
   `je`, the 0x45 mask (which includes the C3/equal flag) decodes to
   **strict** greater-than, not `>=` — the wall-clamp's `sx *= -0.9` side
   effect only fires on the strict side. The unicorn cross-check's first
   mismatch landed exactly on an engineered `x_new == 555.0` vector,
   immediately exposing the wrong reading before it reached `update_player.c`.
2. **`x_new`/`y_new` compared from the same unrounded x87 register the
   non-popping `fstl` stored from**, not from a second 64-bit-rounded
   memory read — the double-vs-80-bit gap already established for
   `line_intersect`/`new_rand`/`add_floor`, this time in a *comparison*
   rather than a final truncation. 3 of 20000 cross-check vectors rounded
   `x + sx` to exactly 555.0/85.0 as a plain `double` while the ORIGINAL
   still took the wall-bounce branch. Not fixable in a `double`-only Python
   model; ordinary C (`if (p->x > 555.0)`) reproduces it once compiled with
   real x87 arithmetic (GCC `-mfpmath=387`), confirmed by the GCC census
   below finding 0 divergences of this kind.

### Residual GCC-side wrinkle: signaling-NaN payload quieting (reported, not a bug)

7 of 80000 GCC-toolchain vectors (1/1/5/0 across the 4 canonical seeds)
DIFFER, always at exactly one bit — `Tplayer+0x16` (`sx` byte 6) or `+0x1e`
(`sy` byte 6), the byte holding bit 51 of that field's IEEE-754 double, the
mantissa's own top bit (the quiet/signaling flag for a NaN). In every one
of the 7, `sx` or `sy` was fed a genuine **signaling** NaN by the vector
generator's random-bit-pattern pool (exponent all-1s, bit 51 clear, some
other mantissa bit set); the clamp logic correctly leaves the value
untouched (neither `<` nor `>` is true for NaN, matching plain C semantics
— confirmed: every OTHER bit of the payload, and every other field, matches
exactly), but the ORIGINAL binary's specific instruction encoding preserves
the signaling bit through the comparison chain while GCC `-O2`'s own
instruction selection for the identical source-level chain quiets it (sets
bit 51) as an incidental side effect of whichever x87 compare/move sequence
it picks. A legitimate game double can never *become* a signaling NaN
through ordinary IEEE-754 arithmetic (only quiet NaNs ever propagate that
way — an SNaN can only enter memory as a directly-constructed bit pattern),
so this has no reachable gameplay consequence. Reported per
win32_pilot.md's "never a percentage, always the exact byte" rule rather
than chased into GCC's instruction-selection internals, which plain C
source cannot steer without inline asm (purity-gate-banned). Full detail,
including the exact per-seed counts, in `artifacts/src_equivalence.json`'s
`update_player.gcc` entry.

### Skipped this pass

| function | VA | size | CU | why skipped |
|---|---|---:|---|---|
| `handle_player_collision_original` | 0x407e10 | 456 | main.c | Fully hand-traced (Tplayer field layout, the two `is_solid()` foot-probe calls at `p->x∓11`, the `any11`/`any12`/`any21`/`any22`/`any23` globals, `sound_landing` gating, and the two-feet-agree edge-detection logic), but calls `play_sound()` with a sound-handle global at VA 0x4dd300 that has **no DWARF-recovered name anywhere** in `artifacts/dwarf_info.txt` (confirmed: zero hits searching for the address directly) and is absent from `carrier/gen/interop_index.json`'s full 156-global list — the same "cannot declare an address-free extern for an unnamed global" class of gap `play_jump_sound` was skipped for in batch 2. Not a call-trace-domain case like `play_jump_sound` (this function's *own* writes — the `any1X`/`any2X` globals and `Tplayer` fields — are a real, harness-expressible comparison domain); the blocker is purely the one unnamed argument to the downstream call. |
| `handle_player_collision_old` | 0x407fd8 | 894 | main.c | Not attempted this pass (headroom, not difficulty) — same collision-handling family as `_original`, likely shares its unnamed-global dependency; re-triage once that has a general fix. |
| `handle_player_collision_combo` | 0x408358 | 1390 | main.c | Not attempted this pass (headroom) — largest of the five collision variants; calls `line_intersect` (already promoted) among others. |
| `handle_player_collision_vector` | 0x408d08 | 1071 | main.c | Not attempted this pass (headroom). |
| `handle_player_collision_vector_2` | 0x4088c8 | 1086 | main.c | Not attempted this pass (headroom). |
| `start_reward` | 0x407c38 | 472 | main.c | Reads the named global `DATAFILE *data` (VA 0x4dd23c) at a **computed** index (`data[esi+0x5a].dat`, `esi` derived from the reward-type argument) — one of the asset seam's "7 computed-index sites (2 not yet itemized)" `notes/asset_census.md`/`src/icytower/ASSETS.md` already flag as unmapped to a named `asset_id`. Needs the computed-index range mapped against `assets_table.inc` first (an asset-seam task, not a physics one); its other callees (`create_particle`, `new_rand`, `play_sound`) are already promoted or address-free. Deferred rather than guessed. |

### Call-trace domain

**Not implemented this pass.** `handle_player_collision_original`'s
blocker (an unnamed sound-handle global reached only through a
`play_sound()` call, never written by the function itself) is exactly the
class of problem the call-trace domain (unicorn hooks on calls leaving the
function under test; harness-side stubs recording the same on the compiled
side, per this batch's task brief) would solve directly — it would let the
comparison be "which handle reached `play_sound`'s first argument", the
same design `notes/promotion_candidates.md` SS5 already sketched for
`play_jump_sound`, without ever needing the global to have a name. Flagged
as the natural next step for whichever future pass returns to this
function family; not built this pass — after the two rounds of unicorn
cross-checking `update_player` needed to get its x87 semantics right (see
above), there was no remaining pass budget for a second, comparably-sized
piece of harness machinery (`harness_rand.c` for `add_floor` in batch 5 is
the closest precedent for the size of this undertaking).

### Harness changes (additive, none touching `src/`)

- `carrier/lift/harness/lift_check.py`: `gen_reset_player` + `gen_update_player`
  + their `SPECS` entries (additive); `SRC_BATCH6_FUNCS` added to the `--form
  src` default `--funcs` list; `--toolchain gcc`'s default `--funcs` gained
  `update_player`.
- `carrier/lift/harness/src_check.c`: `reset_player`/`update_player` extern
  declarations + dispatch branches (the latter reusing `add_floor`'s `demo`
  pointer-VALUE fixup pattern, since `update_player` also calls `get_demo()`
  internally and dereferences the result).
- `carrier/lift/harness/gcc_check.c`: extended to wire `reset_player`/
  `update_player` — `gravity_modifier[3]` given its own plain-global storage
  (synced via `tr()`, read-only from `update_player`'s side, same pattern as
  `max_speed`/`collision_type`); `demo` reused from the `add_floor` wiring.
- `carrier/lift/harness/build_src.cmd`, `build_src_gcc.sh`: file lists
  extended with `reset_player.c`/`update_player.c`.
- `carrier/lift/harness/pf_bindings_harness.h`/`_types.h` (harness-only —
  **not** `carrier/gen/pf_bindings_src.h`, which this pass deliberately does
  not touch; see below): regenerated via `scan_src_defs.py`'s auto-scanned
  `--exclude` list (42 names now, up from 40).
- `src/build/Makefile.standalone`: `SOURCES` extended to every currently-existing
  non-asset-seam `src/icytower/*.c` file (`add_jump_sequence.c`,
  `main_state.c`, `new_rand.c`, `ok_to_play.c`, `particle.c`, `reset_player.c`,
  `scroller.c`, `timer.c`, `update_player.c`) — this also **fixes a
  pre-existing link failure** (`undefined reference to get_demo`) that batch
  5's `add_floor` addition to `map.c` had silently introduced into this
  target without updating its own hand-maintained `SOURCES` list (found
  while trying to verify this pass's own standalone-world compile; unrelated
  to this pass's two promoted functions, fixed as encountered).

**`carrier/gen/pf_bindings_src.h`/`_types.h` intentionally NOT regenerated
this pass** — another agent is running the carrier build concurrently and
owns that file; regenerating it here would race that build. Verified the
carrier-world compile anyway against a scratch copy of the same generated
header, built into a temp directory outside `carrier/gen/`, with the
identical `--exclude` list `scan_src_defs.py` would produce (42 names +
`floor_size_modifiers`) — 0 errors, 0 warnings, then discarded. The exclude
list is scanned automatically at the next real carrier build per this task's
own instructions; `reset_player`/`update_player` need no manual listing
beyond this file existing.

### Totals (updated)

| | batch 6 (this pass) | cumulative (6 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 2 | 38 |
| functions skipped (documented, all passes) | 6 new this pass (5 `handle_player_collision_*` variants + `start_reward`) | 9 (6 new + `play_jump_sound`/`destroy_game_data`/`get_version_str` still carried) |
| original bytes recovered | 947 (296 + 651) | 3583 |

## Purity gate (updated)

```
python scripts/check_native_layer.py
check_native_layer: scanned 27 file(s) under .../src, 0 violation(s)
```

## Compile (both worlds, batch 6)

```
standalone: cl /nologo /c /W3 /TC /Isrc\icytower
            src\icytower\update_frame.c src\icytower\is_solid.c
            src\icytower\jump_player.c src\icytower\map.c src\icytower\add_combo.c
            src\icytower\add_jump_sequence.c src\icytower\line_intersect.c
            src\icytower\control.c src\icytower\particle.c src\icytower\scroller.c
            src\icytower\timer.c src\icytower\main_state.c src\icytower\state.c
            src\icytower\new_rand.c src\icytower\ok_to_play.c src\icytower\reset_player.c
            src\icytower\update_player.c
            -- 0 errors, 0 warnings

            mingw32-make -f src\build\Makefile.standalone (upstream Allegro headers/libs,
            SOURCES list extended -- see "Harness changes" above): libicytower.a +
            standalone_smoke.exe build clean; standalone_smoke.exe run: 16/16 PASS
            (also confirms the pre-existing get_demo link regression from batch 5 is fixed)

carrier:    python carrier\gen\scan_src_defs.py --src-dir src\icytower   (42 function
            names + floor_size_modifiers, auto-scanned)
            python carrier\gen\gen_bindings.py --exclude <scanned names> ^
                --guard-define ICYTOWER_BINDINGS_ACTIVE ^
                --out <SCRATCH>\pf_bindings_src.h --types-out <SCRATCH>\pf_bindings_src_types.h
            (scratch copy only -- carrier/gen/pf_bindings_src.h itself intentionally
            untouched this pass, see "Harness changes" above)

            cl /nologo /c /W3 /TC /Icarrier\gen /I<SCRATCH> /FIpf_bindings_src.h
               src\icytower\update_frame.c src\icytower\is_solid.c src\icytower\jump_player.c
               src\icytower\map.c src\icytower\add_combo.c src\icytower\add_jump_sequence.c
               src\icytower\line_intersect.c src\icytower\control.c src\icytower\particle.c
               src\icytower\scroller.c src\icytower\timer.c src\icytower\main_state.c
               src\icytower\new_rand.c src\icytower\ok_to_play.c src\icytower\reset_player.c
               src\icytower\update_player.c
            -- 0 errors, 0 warnings (carrier world, scratch bindings header)
```

## Batch 7 (2026-09-07 — the two recurring blockers)

Task: remove the two recurring "unnamed global" blockers batches 2 and 6
each hit once, then promote the functions they blocked, plus `start_reward`
(blocked separately by the asset-seam computed-index gap). Both blockers
turned out to be **misdiagnosed, not genuinely unnamed** — see "Mechanism A"
below — which meant the real remaining work was building the call-trace
comparison domain (mechanism B) both skip notes had already flagged as the
*other* thing standing in the way.

### Mechanism A: `src/icytower/names.json` (address → name table)

Built exactly as specified: a hand-curated `{address: {name, meaning,
evidence}}` table (`src/icytower/names.json`), consumed by
`carrier/gen/gen_interop.py`'s `collect_globals()` (and, by reuse,
`gen_src_headers.py`'s game_state.h/state.c output) whenever a
`DW_TAG_variable` DIE has an address but no `DW_AT_name` — the DWARF-nameless
case the task brief anticipated. `load_names_table()`/`NAMES_TABLE` are new,
real, tested code (`--names` argument on both generators, default
`src/icytower/names.json` if present).

**It ships empty (`"globals": {}`), and that is the actual finding, not a
shortcut.** Re-investigating the two addresses this task cited as unnamed —
0x4dd300 (blocking `handle_player_collision_original`) and
0x4fabf4/f8/fc (blocking `play_jump_sound`) — found both are DWARF-named
**aggregate members**, not nameless globals:

- 0x4dd300 is `sounds[8]` — DWARF names the whole array `SAMPLE *sounds[9]`
  at VA 0x4dd2e0 (`0x4dd300 - 0x4dd2e0 == 0x20 == 8 * sizeof(SAMPLE*)`),
  already declared `extern SAMPLE *sounds[9];` in game_state.h.
- 0x4fabf4/f8/fc is `custom.jump_sound[0..2]` — DWARF names `Tcustom`'s
  member `jump_sound[3]` at struct offset 1212 (`DW_AT_data_member_location`),
  and `custom` (VA 0x4fa738, `0x4fa738 + 1212 == 0x4fabf4`) is already
  declared `extern Tcustom custom;` with `Tcustom.jump_sound[3]` already in
  game_types.h.

Both were found by computing `address - candidate_aggregate_base` against
every already-named global/struct in scope and recognising the offset lands
inside it — a check batch 2/6's original "no DWARF name anywhere" searches
never ran (they only matched a literal top-level `DW_OP_addr`, which is
correct for a genuine top-level global but blind to a struct member or array
element, which DWARF records via `DW_AT_data_member_location`/array indexing
instead). A broader, exhaustive check confirms this is the whole story, not
a coincidence limited to these two: a scan of every `DW_TAG_variable` DIE in
all 25 game-scope CUs found **zero** with an address but no name — every
game-scope global DWARF describes already has a real one. See
`src/icytower/names.json`'s `_meta` for the full writeup, the positive-path
unit test (a synthetic nameless DIE, since this DWARF has no real one to
test against), and the purity-gate decision (`names.json` is a `.json` file;
`scripts/check_native_layer.py` only globs `*.c`/`*.h`, so it is already out
of the gate's scope with no code change needed — documented there rather
than editing the gate).

**One genuine, different naming collision found and fixed along the way**
(not what mechanism A was built for, but the same "address-binding macro
substitutes a bare token it shouldn't" family of bug): `custom.jump_sound`
tripped over a *second*, unrelated top-level DWARF global that also happens
to be spelled `jump_sound` (VA 0x4dd2b0, a COFF/DWARF-real, independently-
named global — apparently unrelated background-music state, given its
neighbours `_speaker`/`_bg_menu`/`_menu_sounds`/`_bg_beat`). `gen_bindings.py`
binds every game-scope name to a `#define`, textually, with no notion of "in
a member-access position" — `#define jump_sound (*(...)0x4dd2b0)` also
rewrites the `jump_sound` token inside `custom.jump_sound`, producing
`(*(Tcustom*)0x4fa738).(*(SAMPLE*(*)[3])0x4dd2b0)[2]` — a syntax error, not a
harmless one. Fixed with a small, hand-curated, narrowly-scoped
`MEMBER_ACCESS_COLLISIONS = {'jump_sound'}` set in `gen_bindings.py` (same
shape as the pre-existing `RESERVED_CRT_WINDOWS_IDENTS`, deliberately **not**
a blanket scan of every struct member name in scope — a first attempt at
that blanket version also skipped `data`/`stars`/`ctrl`/`cycle_count`/
`sort_method`, several of which `pf_asset_bindings.h` or this very batch's
own new files already bind and use correctly as bare identifiers; reverted
in favour of the narrow, evidence-gated list). A second, unrelated instance
of the *identical* class of bug was found and fixed the same way while
wiring `start_reward.c`: `check_control_key(Tcontrol *c, int key)`'s
generated prototype parameter name `key` collides with Allegro's own
`key[]` keyboard-state array once `pf_lib_bindings.h` is also
force-included (the first `src/` file to need both game_funcs.h and the
asset seam together) — fixed with `gen_src_headers.py`'s new
`PROTOTYPE_PARAM_RENAMES = {'key': 'key_arg'}`, applied only to the
*prototype* text (game_funcs.h is forward declarations only, so a parameter
name there is cosmetic; regenerated for real, diff confined to the 3
prototypes using that parameter name, byte-identical otherwise).

### Mechanism B: the call-trace comparison domain

Implemented generically in `carrier/lift/harness/lift_check.py`, exactly as
scoped: for a traced callee (name → VA/argc, read from
`interop_index.json`'s DWARF-recovered prototype, not hand-counted —
`load_call_targets()`), the ORIGINAL side installs a `UC_HOOK_CODE` hook at
the callee's own entry VA (the same "hook the callee's entry, stub a `ret`"
trick `_rand_hook` already established for the `_rand` IAT thunk), captures
`argc` stack dwords plus a call count into a fixed scratch slot
(`CALLTRACE_PLAY_SOUND_VA = 0x7c1000`, 16 bytes), and resumes past the call
without executing it. The COMPILED side redirects the same callee name to a
harness-only stub (`harness/pf_harness_calltrace.h` force-included,
`#define play_sound harness_trace_play_sound`, mirroring
`pf_harness_rand.h`'s `rand` redirect exactly) that writes the identical
shape into the identical scratch VA (`harness/call_trace_stubs.c`). Both
logs are then just another entry in the ordinary memory-domain list —
**no new comparator code, no new report format**: `lift_check.py`'s existing
diff/negative-control/census machinery covers it for free. `play_sound`
(still ORIGINAL-only, not promoted) is the one callee traced this pass;
the mechanism itself is callee-agnostic (`_CT_PLAY_SOUND` is just one
`{va, argc, slot}` entry a SPECS row lists under `"call_traces"`).

`play_sound` is excluded from `pf_bindings_harness.h`'s macro table
specifically (added to the harness-only `--exclude` list alongside the
auto-scanned src/ function names — **not** to `carrier/gen/pf_bindings_src.h`,
which correctly keeps redirecting `play_sound` to its original address for
the real carrier, since it is not promoted there), so there is no conflict
between the two force-included headers.

`asset_bitmap()` (needed by `start_reward.c`, below) turned out not to need
the call-trace mechanism at all: `carrier/gen/pf_asset_bindings.h`'s real
implementation for the `"data"` family is already a pure `data[N].dat`
memory read through the `data` global, so `call_trace_stubs.c` provides a
harness-only `asset_bitmap()` that does exactly that same read through the
harness's own PF_MEM-redirected `data` (a vector-populated scratch
`DATAFILE[10]` table) — verified via the ordinary memory domain, more
precisely than a call trace could (it directly checks the *computed
asset id*, not just "was some function called").

### Promoted this pass

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `play_jump_sound` | 0x406ecc | 141 | main.c | **EQUAL** (MSVC 20000/20000; GCC x87 20000/20000) | Reads `Tplayer.sy` (the launch speed `jump_player()` just set), compares against two `.rdata` float thresholds read directly from the image with `pefile` (−22.0, −15.0, not guessed), picks one of `custom.jump_sound[0..2]` (hi/med/lo), calls `play_sound(handle,1,1)`. No writes of its own — pure call-trace domain, `must_be_unchanged` on the whole `Tplayer`. `x87` comparison-only (no accumulated chain), so no MSVC/GCC precision gap of its own. | pending |
| `handle_player_collision_original` | 0x407e10 | 456 | main.c | **EQUAL** (MSVC 20000/20000; GCC x87 20000/20000) | The `collision_type==0` dispatch target of `play()`'s 5-way jump table (all 5 variants confirmed LIVE — see below). Two `is_solid(&map, x∓11, y)` foot probes → `any11`/`any12`; `any21`/`any22`/`any23` unconditionally zeroed. Both feet in air: status 0 or 2 → 3 (start falling), else unchanged. At least one foot down: status 1/2 → unchanged; status 0 → silent landing; anything else → `play_sound(sounds[8],1,1)` (the landing sound) THEN the same landing logic — `sy=0`, snap `y -= (tile_result-0x270f)`, `rotate=0`, `edge` = 0 (feet agree)/1 (left foot wins, edge)/2 (right-only). Both parameters confirmed unread anywhere in the function body — recovered as unused, not removed. | pending |
| `start_reward` | 0x407c38 | 472 | main.c | **EQUAL** (GCC x87, 20000/20000); MSVC DIFFER 1725/20000, 100% confined to `stars[]`/`seed` (new_rand-propagated precision, 0 logic divergences — exhaustive per-vector scan, not sampling) | `reward_time=0x50; reward_scale=0`; tier (0..9) from a 9-threshold cascade on the points argument (6/14/24/34/49/69/99/139/199). If `itrcheck==0`: if `!options.flash && tier>2`, spawn `(tier-2)*16` confetti particles into `stars[512]` via `create_particle()` + two `new_rand()` draws each (`sy = -(((new_rand()%500+500)<<16)/100)`, `sx = (((new_rand()%1000-500)<<16)*count)/100` — divisors and `sy`'s negation both re-derived by direct unicorn block execution, not assumed); `reward_bmp = asset_bitmap(ASSET_DATA_REWARD_000+tier)` runs regardless of `flash`/tier. `play_sound(combo_sound[tier],0,0)` always runs. Returns `tier`. Two recovery mistakes (reward_bmp nesting, `/50` vs `/100` + `sy` sign) caught and fixed by the 20000-vector check — see the `start_reward` entry in `artifacts/src_equivalence.json` for the full derivation. | **EQUAL in vivo** (in-vivo verification pass, 2026-09-07: build_blockers.json's LNK2019 blocker removed — `carrier/build.cmd` now compiles this file with `/FIpf_lib_bindings.h /FIpf_asset_bindings.h` in addition to `/FIpf_bindings_src.h`, mechanically detected by `carrier/gen/scan_src_defs.py --extra-fi`; bound and run via `carrier/scripts/bind_all.py --fn start_reward` over `replays/human_test.txt` — 3 invocations, EQUAL, per-tick digest EQUAL 2293 ticks) |

Every row's negative control: `--fault <fn>:5:0`, 200 vectors — comparator
names the exact byte (`Tplayer+0x0 (VA 0x00790000)` for the first two,
`reward_time+0x0 (VA 0x004fec68)` for `start_reward`) — full detail in
`artifacts/src_equivalence.json`.

### Corrections to earlier passes' claims (found while investigating this pass)

- **All five `handle_player_collision_*` variants are LIVE**, not "possibly
  dead code" as batch 6 hedged: `play()` dispatches to all five through one
  jump table on `collision_type` (0x4dd140, values 0..4; dispatch site
  0x4125a3, `jmp *0x4d60c4(,%eax,4)`, guarded `cmpl $0x4,collision_type;ja
  <default>`), each `call` site distinct and reachable. Only `_original`
  (`collision_type==0`) is promoted this pass; `_old`/`_combo`/`_vector`/
  `_vector_2` (894/1390/1071/1086 bytes respectively) are deferred for
  headroom, not because they might be unreachable.
- **`sounds[8]` and `custom.jump_sound[0..2]` were never actually unnamed**
  — see "Mechanism A" above. Both `PROMOTIONS.md` batch 2's and batch 6's
  skip reasons for this specific claim are superseded by this entry; their
  OTHER stated reason for each skip (the call-trace domain not existing
  yet) was correct and is what this pass actually had to build.

### Harness changes (additive, none touching `src/`)

- `carrier/lift/harness/lift_check.py`: `CALL_TARGETS`/`load_call_targets()`
  (mechanism B's generic callee table), `Oracle.__init__`'s new
  `call_traces` parameter + `_make_call_trace_hook()`, three new globals
  (`G_ITRCHECK`, `G_OPTIONS_FLASH`, `G_MAP`, `G_ANY11/12/21/22/23`,
  `G_SOUNDS`, `G_COMBO_SOUND`, `G_CUSTOM_JUMP_SOUND`, `G_REWARD_BMP`,
  `G_DATA`, `G_STARS`, `DATA_TABLE_VA`, `CALLTRACE_PLAY_SOUND_VA`),
  `gen_play_jump_sound`/`gen_handle_player_collision_original`/
  `gen_start_reward` + their `SPECS` entries (additive); `SRC_BATCH7_FUNCS`
  added to the `--form src` default `--funcs` list. One real bug caught and
  fixed in the generator itself: `gen_start_reward`'s first draft reused
  `rnd_double()`'s general special-value pool (includes ±inf/NaN/DBL_MAX)
  for `seed`, which hung `src_check.exe` outright — `new_rand()`'s
  recovered fold loop never terminates once the first multiply overflows to
  +inf. Fixed to bounded ranges only, matching `gen_new_rand`'s own
  (pre-existing, correct) choice for exactly this reason.
- `carrier/lift/harness/pf_harness_calltrace.h` (new, harness-only): the
  `play_sound` → `harness_trace_play_sound` redirect (mechanism B) plus an
  `asset_bitmap()` prototype (assets.h skips its own under
  `ICYTOWER_BINDINGS_ACTIVE`, since the carrier world normally gets it from
  `pf_asset_bindings.h`, which this harness deliberately does not
  force-include — see below).
- `carrier/lift/harness/call_trace_stubs.c` (new, harness-only): 
  `harness_trace_play_sound()` (mechanism B's compiled-side log) and
  `asset_bitmap()` (a minimal, harness-only `"data"`-family-only
  implementation reading through the same PF_MEM-redirected `data` global —
  see "Mechanism B" above for why this needed its own second translation of
  `data`'s pointer VALUE, the same class of fixup `src_check.c`'s
  `ply[player_id]`/`demo` fixups already established).
- `carrier/lift/harness/src_check.c`: three new extern declarations +
  dispatch branches; `handle_player_collision_original`'s branch reuses
  `update_frame`'s own `ply[player_id]` pointer-VALUE fixup pattern (this
  function also reads `ply[player_id]` internally, never as a parameter).
- `carrier/lift/harness/build_src.cmd`: file list extended with
  `call_trace_stubs.c` + the three new `src/icytower/*.c` files;
  `/FIpf_harness_calltrace.h` added.
- `carrier/lift/harness/gcc_check.c`/`build_src_gcc.sh`: extended to wire
  all three new functions (this pass's own GCC x87 measurement, not
  deferred) — `custom`/`itrcheck`/`options`/`reward_time`/`reward_scale`/
  `reward_bmp`/`combo_sound`/`stars`/`map`/`player_id`/`ply`/`any1*`/
  `sounds`/`data` all given their own plain-global storage (standalone
  world, same pattern as `collision_type`/`max_speed`/`seed`/`demo`
  already use), synced from the guest image before each call and written
  back after where mutated. `call_trace_stubs.c` linked in;
  `-include pf_harness_calltrace.h` added (command-line `-include`, not a
  `#include` inside `gcc_check.c` alone, so `play_jump_sound.c`/
  `start_reward.c`'s OWN translation units also get the `play_sound`
  redirect). `is_solid.c` also linked in (a new callee for this build,
  needed by `handle_player_collision_original.c`).
- `carrier/gen/gen_bindings.py`: `MEMBER_ACCESS_COLLISIONS` (see "Mechanism
  A" above).
- `carrier/gen/gen_src_headers.py`: `PROTOTYPE_PARAM_RENAMES` (see
  "Mechanism A" above); `game_funcs.h` regenerated for real (diff confined
  to 3 prototypes' `key`→`key_arg` parameter rename, everything else
  byte-identical).
- `carrier/gen/gen_interop.py`: `load_names_table()`/`NAMES_TABLE`,
  `collect_globals()`'s new-but-inert lookup (see "Mechanism A" above);
  `--names` argument on both `gen_interop.py` and `gen_src_headers.py`.
- `src/icytower/names.json` (new): mechanism A's table — ships empty, see
  above.

**`carrier/gen/pf_bindings_src.h` intentionally NOT regenerated this
pass** — another agent is running the carrier build concurrently and owns
that file, same reasoning batch 6 already recorded. Verified the
carrier-world compile anyway against a scratch copy (`scan_src_defs.py`'s
auto-scanned 45 names, `MEMBER_ACCESS_COLLISIONS`/`PROTOTYPE_PARAM_RENAMES`
included automatically since they live in the generator, not the invocation)
— 0 errors, 0 warnings, then discarded.

### Totals (updated)

| | batch 7 (this pass) | cumulative (7 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 3 | 41 |
| functions skipped (documented, all passes) | 4 collision variants + 2 carried (destroy_game_data, get_version_str) — net −3 from batch 6's skip list | 6 |
| original bytes recovered | 1069 (141 + 456 + 472) | 4652 |

`git diff --stat`-style file list this pass: `play_jump_sound.c` (new file,
141 original bytes), `handle_player_collision_original.c` (new file, 456
original bytes), `start_reward.c` (new file, 472 original bytes),
`names.json` (new file, mechanism A, empty table), `game_funcs.h`
(regenerated, 3 parameter renames only).

## Purity gate (updated)

```
python scripts/check_native_layer.py
check_native_layer: scanned 30 file(s) under .../src, 0 violation(s)
```

## Compile (both worlds, batch 7)

```
standalone: cl /nologo /c /W3 /TC /Isrc\icytower
            src\icytower\play_jump_sound.c src\icytower\handle_player_collision_original.c
            src\icytower\start_reward.c src\icytower\state.c
            -- 0 errors, 0 warnings

            mingw32-make -f src\build\Makefile.standalone (SOURCES extended with
            play_jump_sound.c/handle_player_collision_original.c -- start_reward.c
            deliberately left out, same "asset-seam file, out of scope for this
            basic smoke test" reasoning draw_buffer.c/assets_standalone.c already
            have, since it calls asset_bitmap()): libicytower.a + standalone_smoke.exe
            build clean; standalone_smoke.exe run: 16/16 PASS, 0 failure(s)
            (regression check: unaffected by this pass's two additions, since
            neither is referenced by the existing smoke test and a static archive
            only pulls in a referenced member -- play_sound() never needs to
            resolve)

carrier:    python carrier\gen\scan_src_defs.py --src-dir src\icytower   (45 function
            names, auto-scanned)
            python carrier\gen\gen_bindings.py --exclude <scanned 45 names> ^
                --guard-define ICYTOWER_BINDINGS_ACTIVE ^
                --out <SCRATCH>\pf_bindings_src.h --types-out <SCRATCH>\pf_bindings_src_types.h
            (scratch copy only -- carrier/gen/pf_bindings_src.h itself intentionally
            untouched this pass, same reasoning as batch 6)

            cl /nologo /c /W3 /TC /I<SCRATCH> /Icarrier\gen /Isrc\icytower ^
               /FIpf_bindings_src.h ^
               src\icytower\play_jump_sound.c src\icytower\handle_player_collision_original.c
            -- 0 errors, 0 warnings

            cl /nologo /c /W3 /TC /I<SCRATCH> /Icarrier\gen /Isrc\icytower ^
               /FIpf_bindings_src.h /FIpf_lib_bindings.h /FIpf_asset_bindings.h ^
               src\icytower\start_reward.c
            -- 0 errors, 0 warnings (asset-seam recipe, matching draw_buffer.c's own)

offline harness (both toolchains, all three functions, 20000 vectors each):
            python carrier\lift\harness\lift_check.py --form src ^
               --funcs play_jump_sound,handle_player_collision_original,start_reward ^
               --vectors 20000 --census
            -- play_jump_sound: EQUAL; handle_player_collision_original: EQUAL;
               start_reward: DIFFER 1725/20000 (precision-only, see above)

            python carrier\lift\harness\lift_check.py --form src --toolchain gcc ^
               --exe harness\gcc_check_x87_nosse_batch7.exe ^
               --funcs play_jump_sound,start_reward,handle_player_collision_original ^
               --vectors 20000 --census
            -- all three: EQUAL (0/20000)

full regression (all 41 promoted functions, default vector counts): 0 new
            DIFFERs beyond the already-documented precision-only ones
            (line_intersect, new_rand, update_particle, create_particle,
            update_player, start_reward) -- every function that was EQUAL
            before this pass is still EQUAL.
```

## Batch 8 (2026-09-07 -- collision variants + drawing layer)

Task: the four remaining `handle_player_collision_*` variants, plus the
drawing layer bottom-up from `draw_frame`. Neither target's premise
survived first contact with the disassembly unchanged -- both are recorded
below as findings, not just outcomes.

**Sandbox note**: this pass has no MSVC `cl.exe` available (checked: not on
`PATH`, no Visual Studio installation under `C:\Program Files*`) -- every
GCC-toolchain claim below used the real 32-bit MinGW GCC 16.2.0 at
`C:\msys64\mingw32\bin\gcc.exe` (present but not on `PATH` by default; its
own `lib*.a`/DLLs need `C:\msys64\mingw32\bin` prepended to `PATH` too, or
`cc1.exe` fails to load with no error text -- a real, silent trap the first
attempt hit). No function below was compared against MSVC this pass; every
"EQUAL" is GCC x87 only, exactly as the task brief's "verify each with
memory + call-trace domains, GCC x87" already specified for target (1).

### The four `handle_player_collision_*` variants: hand-traced enough to correct batch 6/7's premise, not promoted

Fully hand-traced `handle_player_collision_old` (0x407fd8, 894 bytes,
smallest of the four) instruction-by-instruction before deciding not to
promote it. Finding: **it is NOT "the same shape as `_original`"** --
`_original`'s two straight-line `is_solid()` foot probes are replaced here
by a genuine **iterative bisection loop** between the player's current
truncated integer position and the two candidate-position parameters
(real loop-back edges at 0x408052/0x40812f/0x40813e, converging `esi`
across iterations, each guarded by a signed-average idiom -- `shr
$0x1f`/`add`/`sar` -- applied twice, once per axis, before the eventual
`is_solid()` call at the converged point). Getting the rounding direction
of that bisection exactly right (this project's own track record --
`line_intersect`'s NaN-guard sign, `update_player`'s strict-vs-non-strict
`ah`-mask reading, `new_rand`'s `fsubr` operand order -- is three-for-three
on "an early hand-trace had this backwards, caught only by a
byte-for-byte unicorn cross-check") is exactly the kind of thing this
project does not ship without that cross-check, and building + running
that cross-check for a genuinely novel algorithm (not "port `_original`'s
already-proven shape") was judged not to fit this pass's remaining budget
alongside the drawing-layer work below. Not attempted past the hand-trace;
no C written, no risk of a wrong-but-plausible promotion.

A shallower pass (call-graph only, not full hand-trace) on the other
three confirms they are a *third*, different family again, not a repeat
of `_old`'s bisection either: `handle_player_collision_combo` (1390
bytes), `handle_player_collision_vector` (1071 bytes) and
`handle_player_collision_vector_2` (1086 bytes) each call
`line_intersect`/`getFloorData`/`makecol`/`play_sound` (2, 2, and 1 times
respectively for `play_sound`; `_vector_2` calls `line_intersect` FOUR
times) -- a line-segment-sweep collision algorithm, distinct again from
both `_original`'s two-probe check and `_old`'s bisection, and calling
`makecol` suggests a debug-overlay draw path inside the collision
handler itself. All three already-promoted callees (`line_intersect`,
`getFloorData`, `play_sound`'s call-trace domain) are available, so
nothing NEW blocks these the way batch 2/6's misdiagnosed "unnamed
global" once did -- the blocker is purely hand-trace effort for three
functions in the 1000+-byte range, each its own algorithm.

**Correction to batch 6's own skip note** (`handle_player_collision_old`'s
row): "likely shares its unnamed-global dependency" was speculation that
turned out both unnecessary (batch 7 already resolved the naming
question generally) and beside the real point -- the actual reason this
family stays unpromoted is algorithmic complexity distinct per variant,
not a shared blocker. `_old`/`_combo`/`_vector`/`_vector_2` are re-skipped
this pass for that reason, refined; still confirmed LIVE (batch 7's jump
table finding stands), still no C written, still headroom rather than a
discovered impossibility.

### The drawing layer: `draw_frame` is not decomposable into small helpers -- one real finding, `draw_scroller` promoted, `draw_star_field` promoted compile-only

**`draw_frame` (0x40929c, 8518 bytes, main.c) structure, for the record**:
read its full call list (64 `call` sites) before writing anything. It is
**one monolithic function**, not "draw_player/draw_floor(s)/draw_hud/
draw_combo/draw_text helper calls" the task brief's own hypothesis named
-- that decomposition does not exist in this binary. The 64 calls are:
15 `makecol`, 12 `textprintf_ex`, 10 `textout_ex`, 7 `text_length`, 6
`textprintf_centre_ex`, 3 `sprintf`, 2 `blit`, 2 `new_rand`, 2
`set_clip_rect`, 1 `strcpy`, 1 each of `is_left`/`is_fire`/`is_right`, and
exactly **one** call to a genuine game-scope helper function:
`draw_reward` (0x4070fc, already a separate function, not inlined here).
Per this batch's own task brief ("decompose draw_frame itself only if its
structure is a straight sequence of helper calls -- otherwise stop and
document its structure for the next batch"): its structure is direct,
heavy, inline use of ~10 different Allegro text/blit primitives across
~2100 disassembly lines with extensive branching (background stripes,
floor/sign strips, HUD, combo popups, at least one computed-index asset
read per `src/icytower/ASSETS.md`'s own "what remains hand-mapped" table,
two of the five listed sites are inside this very function) -- NOT a
straight sequence, so **not decomposed this pass**, per the task's own
stop condition. Any future pass attempting this function should expect a
single very large recovery, not a set of small pre-existing pieces to
extract.

**Real, small, standalone draw helpers exist elsewhere** (not as
`draw_frame`'s own children, but as separate functions in the same
"drawing layer" the task meant): `draw_scroller` (396 bytes,
scroller.c -- scroll_scroller.c/restart_scroller.c's own header comment
had already flagged this one as "not attempted"), `draw_star_field` (199
bytes, stars.c -- explicitly named in batch 3's "Call-trace domain" note
as the natural next call-trace candidate), `draw_table` (441 bytes,
hisc.c), `drawSlot` (328 bytes, main.c), `draw_reward` (581 bytes,
main.c), `draw_progress_bar` (486 bytes, main.c), `draw_results` (839
bytes, main.c). Of these, only `draw_scroller` and `draw_star_field` were
promoted this pass (see below); `draw_table`/`draw_reward`/
`draw_progress_bar`/`draw_results` call a MIX of named Allegro functions
and indirect `bmp->vtable-><slot>` calls (`drawSlot`/`draw_reward`/
`draw_results`/`draw_progress_bar` each have 1-4 such indirect calls
alongside their named ones) and were left for a future pass rather than
promoted with a partially-expressible domain.

**Mechanism B extended to Allegro-family callees** (`LIB_CALL_TARGETS` in
`carrier/lift/harness/lift_check.py`, sourced from `carrier/gen/
pf_lib_bindings.h`'s own generated VA/argc for `set_clip_rect`/
`textout_ex`/`textout_centre_ex` -- the same evidence class
`load_call_targets()` already used for game-scope callees, just read from
a different generated file since Allegro-family names are not in
`interop_index.json`, per `carrier/gen/LIB_BINDINGS_NOTES.md`). Two real
bugs found and fixed while extending it, both additive/harness-only, both
backward-compatible (full regression of every other GCC-verified function
rerun unaffected):

1. **FIRST-CALL-CAPTURE, not last-call.** The pre-existing hook
   overwrote a callee's logged arguments on EVERY call, keeping only the
   most recent -- fine when a traced callee is called at most once per
   invocation (true of every batch 7 `play_sound` use), but wrong for
   `draw_scroller`, whose `set_clip_rect` is called TWICE per drawn
   invocation: once with the interesting, argument-dependent clip
   rectangle, then again, always, to restore the clip to the whole
   bitmap (`bmp->w-1`/`bmp->h-1`) -- a deterministic call that would have
   silently masked the first, interesting one. Fixed in
   `Oracle._make_call_trace_hook` (only writes args when `count == 0` at
   entry; `count` itself still increments on every call) and mirrored in
   the compiled-candidate stubs (`call_trace_stubs.c`). Documented,
   known-remaining limitation: a bug that only affects row 2+ of a
   multi-row vertical scroller (while row 1 and the total call COUNT stay
   correct) would not be caught by this domain -- only the first call to
   a given callee has its arguments captured, not every call.
2. **Pointer arguments relayed opaquely into a traced call need reverse
   translation.** `draw_scroller`'s `bmp` parameter is genuinely
   dereferenced (`bmp->w`, `bmp->h`) so the driver's `tr()` correctly
   turns it into a real host pointer before the call -- but that SAME
   pointer is then relayed unchanged into `set_clip_rect`/`textout_ex`/
   `textout_centre_ex`, and the ORIGINAL side (unicorn, executing the
   real bytes directly in guest address space) never translates anything,
   so the two sides logged different numbers for the identical logical
   bitmap (found immediately: the very first run's own DIFFER, one byte
   of a host malloc address instead of `BMP_VA`'s own byte). Fixed with
   `pf_untranslate()` in `call_trace_stubs.c` (host pointer -> guest VA,
   the exact reverse of `PF_MEM()`), applied to the `bmp` argument in all
   three new stubs -- `sc->fnt`/`sc->text`/`sc->lines[i]` needed no such
   fix (opaque sentinel dwords stored as plain struct FIELDS, never
   themselves translated by any driver, so they already round-trip
   unchanged on both sides, unlike the `bmp` POINTER ARGUMENT itself).

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `draw_scroller` | 0x41f0ec | 396 | scroller.c | **EQUAL** (GCC x87, 20000/20000, call-trace + EAX domain; MSVC not run, no `cl.exe` in this sandbox) | Draws a `Tscroller`: horizontal branch (`sc->horizontal != 0`) draws one scrolling line via a single `textout_ex`; vertical branch draws `sc->rows` lines via `textout_centre_ex`, one call per row that survives TWO independently-computed guards (`top = (i-1)*font_height+offset` must not exceed `height`; `bottom = i*font_height+offset` must not be negative -- NOT the same quantity tested twice, confirmed by re-reading the disassembly a second time after an initial draft conflated them). Gated before anything is drawn (`-length<=offset<=width` horizontal / `-rows*font_height<=offset<=height` vertical); a culled call returns 0 and calls nothing at all. Writes no game memory of its own -- the entire comparison domain is the call-trace domain (see above) plus EAX. | pending |
| `draw_star_field` | 0x41f340 | 199 | stars.c | **compile-only** (standalone/upstream-Allegro world; carrier-world compile BLOCKED, two precise reasons below) -- same class as `draw_buffer.c` | Clears `sf`'s rectangle via one `rectfill` (unless `clear_color==-1`), then plots each of `sf->stars` stars via one `putpixel` each, colour ramped by depth (`sf->col1 + sf->col_step*((sf->depth-1)-star.z)`), position `(int)star.x+x, (int)star.y+y` (plain truncating cast, matching every other recovered position truncation in this project). No memory domain (pixels only) and no call-trace domain possible: `rectfill`/`putpixel` are `AL_INLINE` macros in real Allegro, confirmed by the disassembly itself (`call *0x3c(%ecx)`/`call *0x24(%ecx)` through `bmp->vtable`, not a `call <fixed VA>`) -- there is no stable address for mechanism B to hook. | not bindable yet (see below) |

Every negative control: `--fault <fn>:5:0`, 200 vectors (`draw_scroller`
only -- `draw_star_field` has no offline domain to fault); comparator
names the exact byte, `set_clip_rect_trace(count,bmp,x1,y1,x2,y2)+0x0 (VA
0x007c1100)` -- full detail in `artifacts/src_equivalence.json`.

### `draw_star_field`'s carrier-world compile: two real, out-of-scope gaps found, not fixed

Compiles clean against real upstream Allegro headers (standalone world --
`rectfill`/`putpixel` resolve through a real, linked `GFX_VTABLE`, which
is the whole point of that world). Does **not** compile against the
carrier's scratch bindings, for two independent reasons, both diagnosed
precisely and left as generator TODOs rather than worked around:

1. `rectfill`/`putpixel` have no declaration anywhere in
   `carrier/gen/pf_lib_bindings.h` or `allegro_api.h` -- confirmed absent
   by grep. `carrier/gen/gen_lib_bindings.py`'s 100-function allow-list
   (`carrier/gen/LIB_BINDINGS_NOTES.md`) is built from a DWARF callee
   scan of what the game calls BY NAME; a function the game only ever
   reaches through an inlined `bmp->vtable-><slot>` dispatch structurally
   never appears as a named callee, so it was never in scope for that
   generator to bind, the same "inline function" gap
   `LIB_BINDINGS_NOTES.md` already documents for `itofix`/etc, just for
   the graphics-primitive macros instead of the fixed-point math ones. A
   real fix (emitting the handful of `GFX_VTABLE`-dispatch macros
   upstream's own `gfx.h` defines) belongs in `gen_lib_bindings.py`, not
   attempted this pass.
2. `sf->stars` (`Tstar_field`'s int star-COUNT member) is rewritten into
   a syntax error by the same blunt textual `#define stars
   <address-cast>` bug class already found and fixed for `jump_sound` in
   batch 7 -- **except this instance cannot reuse that fix**:
   `start_reward.c` (already promoted) reads the top-level `Tparticle
   stars[512]` global BARE and needs its own `#define` to keep resolving,
   so adding `stars` to `MEMBER_ACCESS_COLLISIONS` (which SKIPS the
   `#define` entirely, batch 7's only tool) would fix this file and
   silently break that one in the same build. Confirmed by trying it:
   regressed `start_reward.c`'s own compile, reverted immediately.
   `carrier/gen/gen_bindings.py`'s own `MEMBER_ACCESS_COLLISIONS` comment
   now documents this as a named, deliberately-not-taken fix, needing a
   context-sensitive rewrite (skip a `.name`/`->name` occurrence, keep
   rewriting a bare one) the current mechanism cannot express.

### Skipped this pass

| function | VA | size | CU | why skipped |
|---|---|---:|---|---|
| `handle_player_collision_old` | 0x407fd8 | 894 | main.c | Fully hand-traced; NOT the same shape as `_original` (a genuine iterative bisection loop, see above) -- promoting it safely needs the same byte-for-byte unicorn cross-check this project's track record shows is required for a novel algorithm's rounding direction; not built this pass (headroom, alongside the drawing-layer work). |
| `handle_player_collision_combo` | 0x408358 | 1390 | main.c | Call-graph scan only (not hand-traced): calls `line_intersect`/`getFloorData`/`makecol`/`play_sound`, a line-sweep algorithm distinct from both `_original` and `_old` -- largest of the five variants, headroom. |
| `handle_player_collision_vector` | 0x408d08 | 1071 | main.c | Same line-sweep family as `_combo` (call-graph scan only); headroom. |
| `handle_player_collision_vector_2` | 0x4088c8 | 1086 | main.c | Same family, calls `line_intersect` FOUR times (call-graph scan only); headroom. |
| `draw_table` | 0x404a7c | 441 | hisc.c | Calls `makecol`/`textprintf_ex`/`textprintf_right_ex` (all named, call-trace-tractable) but not attempted this pass -- headroom after `draw_scroller`/`draw_star_field`. |
| `drawSlot` | 0x406fb4 | 328 | main.c | Mixes named calls (`makecol`, `textout_ex`) with 2 indirect `bmp->vtable` calls (offsets 0x3c=rectfill, 0xbc=an unidentified sprite-draw slot) -- a partially-expressible domain, not attempted. |
| `draw_reward` | 0x4070fc | 581 | main.c | `draw_frame`'s own one real helper call (see above); mixes a named `stretch_sprite` call with 1 indirect `bmp->vtable+0xa4` call -- not attempted. |
| `draw_results` | 0x4076c0 | 839 | main.c | Mixes `textprintf_ex`/`textprintf_right_ex`/`makecol`/`stricmp` (named) with 4 indirect `bmp->vtable` calls -- not attempted. |
| `draw_progress_bar` | 0x407a08 | 486 | main.c | Mixes `makecol`/`textout_centre_ex` (named) with 2 indirect `bmp->vtable` calls plus 2 bare `call *%eax` (function-pointer-variable calls, not even a vtable slot) -- not attempted. |
| `draw_frame` | 0x40929c | 8518 | main.c | Structure documented above (monolithic, not a helper-call sequence) per this batch's own stop condition; not decomposed. |

### Harness changes (additive, none touching `src/`)

- `carrier/lift/harness/lift_check.py`: `LIB_CALL_TARGETS` (Allegro-family
  callee VA/argc, from `pf_lib_bindings.h`) merged into `CALL_TARGETS`;
  `CALLTRACE_SET_CLIP_RECT_VA`/`_TEXTOUT_EX_VA`/`_TEXTOUT_CENTRE_EX_VA`
  scratch slots; `_CT_SET_CLIP_RECT`/`_CT_TEXTOUT_EX`/
  `_CT_TEXTOUT_CENTRE_EX`; `_blank_trace()` (generalizes
  `_blank_call_trace()` to any argc); `Oracle._make_call_trace_hook`
  generalized to first-call-capture (see above, backward-compatible);
  `gen_draw_scroller` + `draw_scroller` `SPECS` entry; `SRC_BATCH8_FUNCS`
  added to the `--form src` default `--funcs` list.
- `carrier/lift/harness/pf_harness_calltrace.h`: `harness_trace_
  set_clip_rect`/`_textout_ex`/`_textout_centre_ex` declarations +
  `#define` redirects, mirroring `play_sound`'s existing shape exactly.
- `carrier/lift/harness/call_trace_stubs.c`: the three stub definitions
  (first-call-capture, matching the Python hook) + `pf_untranslate()`
  (host pointer -> guest VA, the reverse of `PF_MEM()`, applied to every
  `bmp` argument the three new stubs log -- see finding 2 above).
- `carrier/lift/harness/pf_harness_msvc_types.h` (new, harness-only):
  `#define __int64 long long` -- plain GCC (no Windows SDK headers) does
  not define `__int64` on its own, and `allegro_api.h` (generated) has
  one line needing it (`typedef unsigned __int64 uint64_t;` for
  `file_size_ex`'s return type). `draw_scroller.c` is the first file this
  GCC harness build compiles that itself `#include`s `allegro_api.h`
  (`draw_buffer.c`, the only earlier such file, was never part of
  `build_src_gcc.sh`'s file list -- its own verification is MSVC compile-
  only). Confirmed reproducible in complete isolation before writing this
  shim; the real fix belongs in `carrier/gen/gen_lib_bindings.py`
  (spell the typedef portably), out of scope here.
- `carrier/lift/harness/gcc_check.c`/`build_src_gcc.sh`: extended to wire
  `draw_scroller` (no game global read or written -- dispatch is just
  `tr()`-translate `sc`/`bmp`, call, done) and force-include the new
  `pf_harness_msvc_types.h`; `gcc_check_x87_nosse_batch8.exe` built (this
  pass's own GCC x87 exe, `-mfpmath=387 -mno-sse2 -O2`).
- `carrier/gen/gen_bindings.py`: `MEMBER_ACCESS_COLLISIONS`'s own comment
  extended with the `stars` finding (deliberately NOT added to the set --
  see "two real, out-of-scope gaps" above).

**`carrier/gen/pf_bindings_src.h` intentionally NOT regenerated this
pass** -- same reasoning as batches 6/7 (another agent's concurrent
carrier build owns that file). Verified both new functions' carrier-world
compile against a scratch copy (`scan_src_defs.py`'s auto-scanned 48
names + `floor_size_modifiers`), built outside `carrier/gen/`, then
discarded; `draw_scroller`: 0 errors, 0 warnings. `draw_star_field`: see
"two real gaps" above (does not compile in this world yet, precisely
documented, not silently skipped).

### Totals (updated)

| | batch 8 (this pass) | cumulative (8 passes) |
|---|---:|---:|
| functions promoted (offline-verified, memory and/or call-trace domain) | 1 (`draw_scroller`) | 42 |
| functions promoted (compile-only, documented domain gap) | 1 (`draw_star_field`) | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 6 newly-documented this pass (5 draw helpers + `draw_frame` itself; the 4 collision variants refine batch 6/7's existing skip rows rather than adding new ones) | 12 distinct (4 collision variants + 6 new this pass + `destroy_game_data`/`get_version_str` carried) |
| original bytes recovered (offline-verified) | 396 | 5048 |

`git diff --stat`-style file list this pass: `draw_scroller.c` (new file,
396 original bytes), `draw_star_field.c` (new file, 199 original bytes,
compile-only), `carrier/lift/harness/lift_check.py` (+~140 lines,
additive), `carrier/lift/harness/pf_harness_calltrace.h` (+22 lines),
`carrier/lift/harness/call_trace_stubs.c` (+70 lines),
`carrier/lift/harness/pf_harness_msvc_types.h` (new file, harness-only),
`carrier/lift/harness/gcc_check.c` (+12 lines),
`carrier/lift/harness/build_src_gcc.sh` (+2 lines),
`carrier/gen/gen_bindings.py` (comment-only, no behaviour change).

## Purity gate (updated)

```
python scripts/check_native_layer.py
check_native_layer: scanned 32 file(s) under .../src, 0 violation(s)
```

## Compile (both worlds, batch 8)

```
standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK
      -Ithird_party/allegro-4.4.3.1/include
      -Ithird_party/build-allegro-4.4.3.1/include
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower
      -c <draw_scroller.c and draw_star_field.c, each with a one-line
          scratch #include swap '"allegro_api.h"' -> '<allegro.h>'>
  -- 0 errors, 0 warnings, both files
  (draw_scroller.c/draw_star_field.c deliberately left OUT of
  src/build/Makefile.standalone's own SOURCES list, same reasoning
  draw_buffer.c's own exclusion already established: that target's swap
  is real but this basic smoke build never performs the allegro_api.h ->
  <allegro.h> substitution itself)

carrier (scratch bindings, GCC -- no MSVC cl.exe in this sandbox):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower   (48 function
      names + floor_size_modifiers, auto-scanned, draw_scroller/
      draw_star_field included automatically)
  python carrier/gen/gen_bindings.py --exclude <scanned 48 names> ^
      --guard-define ICYTOWER_BINDINGS_ACTIVE ^
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
      (scratch copy only -- carrier/gen/pf_bindings_src.h itself
      intentionally untouched this pass, same reasoning as batches 6/7)

  gcc -m32 -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower
      -include <SCRATCH>/pf_bindings_src.h -include carrier/gen/pf_lib_bindings.h
      -include carrier/lift/harness/pf_harness_msvc_types.h
      -c src/icytower/draw_scroller.c
  -- 0 errors, 0 warnings

  (same command for draw_star_field.c: DOES NOT COMPILE -- see "two real
  gaps" above; not a passing result, recorded as such)

offline harness (GCC x87 only, no MSVC in this sandbox):
  python carrier/lift/harness/lift_check.py --form src --toolchain gcc ^
      --exe harness/gcc_check_x87_nosse_batch8.exe ^
      --funcs draw_scroller --vectors 20000 --census
  -- draw_scroller: EQUAL (0/20000)

  negative control: --fault draw_scroller:5:0, 200 vectors -- DIFFER at
      vector 5, comparator names set_clip_rect_trace(...)+0x0 exactly

full regression (13 GCC-toolchain-verified functions -- every function
      this sandbox CAN check without MSVC -- default/reduced vector
      counts): all still EQUAL, 0 regressions from this pass's
      Oracle._make_call_trace_hook generalization (first-call-capture) or
      any other change.
```

## Batch 9 (2026-09-07 -- the four remaining live collision algorithms)

Task: `handle_player_collision_old` / `_combo` / `_vector` / `_vector_2`
(894 / 1390 / 1071 / 1086 bytes), the four variants batches 6, 7 and 8 each
deferred; find where `collision_type` is set and which value the operator's
recording used. **All four promoted, EQUAL over 20000 vectors each (two
seeds), plus the `collision_type` question answered from the bytes.**

### The answer to "which variant does the recording use": `_vector`, always

`play()`'s 5-way jump table (batch 7's finding, unchanged) is at 0x4125a3,
`jmp *0x4d60c4(,%eax,4)`, guarded by `cmpl $0x4,collision_type; ja
<allegro_message>` at 0x412591. Reading the five table entries out of the
image at 0x4d60c4 and following each to its `call`:

| `collision_type` | table entry | variant |
|---:|---|---|
| 0 | 0x412616 | `handle_player_collision_original` (0x407e10) |
| 1 | 0x4125f9 | `handle_player_collision_old` (0x407fd8) |
| 2 | 0x4125dc | `handle_player_collision_vector` (0x408d08) |
| 3 | 0x4125bf | `handle_player_collision_vector_2` (0x4088c8) |
| 4 | 0x412633 | `handle_player_collision_combo` (0x408358) |

`collision_type` (0x4dd140) has **exactly one store in the entire image**:
`new_game()` (0x40dc9c) opens with `movl $0x2,0x4dd140` at 0x40dcb0,
unconditionally, immediately after its `log2file()` call. Every other
reference to 0x4dd140 in `artifacts/disasm.txt` is a load (the two `play()`
dispatch guards at 0x4120e5/0x412591, plus three reads in
`jump_player`/`update_player` indexing `max_speed[collision_type]`). It is
NOT an options field, NOT a `Treplay` difficulty field, NOT an .ini key and
NOT a command-line switch -- the task brief's hypotheses were each checked
against the bytes and none holds. `collision_type` is a leftover
development selector frozen at 2.

So **`replays/human_test.txt`, and every other run of this build, dispatches
to `handle_player_collision_vector`**. The other four are live in batch 7's
sense (each is a distinct, reachable jump-table case) but the selector that
would reach them never changes -- which also means
`handle_player_collision_original`, promoted in batch 7, is not the variant
the recording exercises.

### Correction: `_old` is NOT an iterative bisection

Batch 8's skip note called `handle_player_collision_old` "a genuine
iterative bisection loop ... converging `esi` across iterations, each
guarded by a signed-average idiom". **That reading was wrong.** The three
backward branches it cited are GCC tail duplication, not loop edges:
0x408131's `jl 408052` and 0x40813e's `jmp 408066` are the two arms of the
`iy < lastY` if/else (the compiler laid the `ix >= lastX` arm out after the
fall-through and had to re-enter the shared Y code), and 0x4081b0/0x4081b8
are the negate halves of two inlined `abs()`es. There is no loop-carried
variable, no convergence test, and `is_solid()` is called at most four
times, in two fixed pairs.

What `_old` actually does: `_original`'s two foot probes at the CURRENT
position; then, only if `my > lastY` -- which, since the midpoint always
lies on the current-position side, means the player's integer Y grew by 2 or
more, i.e. it fell at least two pixels -- ONE more probe pair at the
midpoint of this frame's move, rounded towards `(lastX, lastY)`. A
two-sample swept check, the family's crudest anti-tunnelling measure.

### The three algorithms, and what actually separates the sweep trio

| variant | algorithm | `any11/12` | `any21/22/23` | `makecol` | +-10000 guard | `fy+4` retry |
|---|---|---|---|---|---|---|
| `_original` | 2 foot probes | written | zeroed | - | - | - |
| `_old` | 2 foot probes + 1 midpoint re-probe | written | written | - | - | - |
| `_combo` | `_original`, then line sweep | written | zeroed | unconditional | no | no |
| `_vector` | line sweep only | untouched | untouched | inside `debug` gate | **yes** | no |
| `_vector_2` | line sweep only | untouched | untouched | unconditional | no | **yes** |

The line sweep (batch 8's call-graph scan correctly identified the family,
not its shape): `getFloorData(&map, (int)p->y, &fy, &fx1, &fx2)` for the
floor segment in the player's own row, retried at `lastY` if that row is
empty; then `line_intersect()` of that horizontal segment against each
foot's travel segment `((int)p->x -+ 11, (int)p->y + 1) -> (lastX -+ 11,
lastY)`. `p->edge` becomes 0 (both feet agree), 1 (left only) or 2 (right
only); if either foot crossed AND `p->status` is 2 or 3, the player lands:
landing sound, `sy = 0`, `p->y = fy - 1`, and `p->x` snapped so the foot
that crossed sits on the intersection X (`hx1 + 11` or `hx2 - 11`).

Three details visible only in the bytes, all reproduced:

1. **`_vector` is the only one with a sanity guard**: when BOTH feet cross,
   both intersection X values must lie in `[-10000, 10000]` or the whole
   landing is abandoned (the `p->edge` write still stands). That is this
   family's own defence against `notes/living_record.md` divergence 006's
   degenerate `line_intersect` result -- a fully-degenerate segment pair
   makes `ua`/`ub` NaN, the original's `fistp` yields "integer indefinite",
   and `x1 - 2147483648` lands far outside the guard. Reached by the
   directed corpus (59 of 1500), not merely inferred from reading.
2. **`_vector` returns early when neither `getFloorData()` probe finds a
   floor** (status 0/2 -> 3, done); `_combo` and `_vector_2` instead zero
   `fx1`/`fx2` and carry on with `fy` still holding the sentinel, sweeping
   against a degenerate segment at y = -12345678.
3. **`_vector_2`'s `fy + 4` retry still snaps to `fy - 1`, not `fy + 3`**:
   the original increments only its register copy of `fy` (0x408acc `add
   $0x4,%ebx`) and re-reads the untouched stack local for the landing
   (0x408be2). Getting this backwards would have been invisible in every
   vector where the first sweep already hit.

### The debug overlay: a statically-dead branch, verified anyway

All three sweep variants draw their own floor segment (red) and two probe
segments (yellow) on `screen` when `debug && key[KEY_F2]`:

- `debug` is 0x4dd160 (DWARF-named, already `extern int debug` in
  game_state.h). **It has no store anywhere in the image** -- all fourteen
  references in `artifacts/disasm.txt` are loads or `cmpl $0x0,...` -- so it
  is .bss-zero for the life of the process and the overlay is statically
  unreachable in vivo.
- `key[KEY_F2]` is 0x5069b8: `0x5069b8 - 0x506988` (Allegro's `key[127]`,
  per pf_lib_bindings.h) `== 0x30 == 48 == KEY_F2`.
- The draw is `call *0x34(%ecx)` off `BITMAP.vtable` (+0x1c). GFX_VTABLE
  +0x34 is `line` (allegro_types.h; batch 8 had already pinned +0x3c =
  `rectfill` in the same table), and that indirect call IS what Allegro 4's
  `line()` compiles to -- an `AL_INLINE` whose whole body is
  `bmp->vtable->line(bmp, ...)`
  (third_party/allegro-4.4.3.1/include/allegro/inline/draw.inl:72).

Rather than recover the branch and leave it untested, this pass verifies it:
one vector in five sets `debug`/`key[KEY_F2]` non-zero, and both the
`makecol` calls and the three vtable-`line` calls are compared through the
call-trace domain (see "Harness changes"). That matters beyond tidiness --
`_vector` is the only variant whose two `makecol()` calls sit INSIDE the
gate, so a `debug`-always-0 corpus would have compared nothing at all about
its colour handling.

### Promoted this pass

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `handle_player_collision_old` | 0x407fd8 | 894 | main.c | **EQUAL** (GCC x87, 20000 x 2 seeds = 40000) | Two foot probes + one midpoint re-probe (see the correction above). The midpoint rounds towards `(lastX, lastY)`; the disassembly writes the two arms asymmetrically (sign-correcting `shr $0x1f`/`add`/`sar` in one, bare `sar` in the other), but the halved value is an inlined `abs()`'s output and so never negative, where both idioms agree bit for bit -- and even at INT_MIN, which is even, they still agree. The only variant that writes `any21`/`any22`, and the only one that sets `any23 = 1` (its "landed on the re-probe" marker). | pending |
| `handle_player_collision_vector` | 0x408d08 | 1071 | main.c | **EQUAL** (GCC x87, 40000) | **The variant the game actually dispatches to** (see above). Pure line sweep, `+-10000` guard, `makecol` inside the debug gate, `any11..any23` never touched -- verified as unchanged, not merely unasserted, since all five are in the comparison domain. | pending |
| `handle_player_collision_vector_2` | 0x4088c8 | 1086 | main.c | **EQUAL** (GCC x87, 40000) | Line sweep + the `fy + 4` second sweep, unconditional `makecol`, no guard. Batch 8's call-graph note ("calls `line_intersect` FOUR times") is right, and this is why: two probes x two sweeps. | pending |
| `handle_player_collision_combo` | 0x408358 | 1390 | main.c | **EQUAL** (GCC x87, 40000) | `_original`'s foot probes verbatim (same `any1*` writes, same `0x270f` snap, same edge rules) and, only if those did not land the player, `_vector_2`'s sweep minus the retry. Its two `makecol()` calls are unconditional AND come first, so it calls Allegro twice even on the frames where the foot probes land and it returns early. | pending |

Comparison domain, identical for all four (264 bytes): the whole `Tplayer`,
all five `any11`/`any12`/`any21`/`any22`/`any23` globals, and three
call-trace slots (`play_sound`, `makecol`, vtable `line`). No return value
-- all five variants are `void(int, int)`.

Negative control, all four: `--fault <fn>:5:0`, 200 vectors -- each DIFFERs
at vector 5 and the comparator names `Tplayer+0x0 (VA 0x00790000)` exactly.

### Method: the unicorn cross-check came first, again

Per this project's own track record for a novel algorithm, no C was written
until a Python model of each of the four had been checked against the
ORIGINAL bytes executed in unicorn. The model delegated the three
already-promoted callees (`is_solid`, `getFloorData`, `line_intersect`) back
to the ORIGINAL bytes in the same emulator, so what was under test was
purely this pass's reading of the new control flow. Result: **0 mismatches
for all four, on the first run** -- 200 random vectors each, then 500
directed boundary vectors each.

That is the first time in this project a novel algorithm's first hand-trace
survived the cross-check unchanged, and it is worth saying why rather than
claiming the trace was simply better: all four functions are pure integer
control flow around three already-verified callees. Every FP operation in
all 4441 bytes is a `fistpl` truncation of `p->x`/`p->y` under the usual
local round-to-zero control word, plus `fildl`/`fisubl` on the landing
writes. There is no accumulated x87 chain, no unordered compare, no
magic-multiply divisor -- so none of the four failure modes that caught
earlier passes (`fsubr` operand order, `test $0x45,%ah` strictness,
NaN-guard sign, reciprocal-multiply divisor) could arise here at all.

The one place the first draft WAS nearly wrong is worth recording: a purely
random map/position pool reaches the interesting sweep outcomes far too
rarely to be a check. Measured: 2-3 of 400 vectors ever produced `edge != 0`,
the landing path essentially never fired through the sweep, and `_vector`'s
`+-10000` guard was never reached at all. The directed generator
(`_collision_directed()`, which inverts `getFloorData()`'s own
`y = 29 - ((cy+1)>>4)` / `fy = ((cy+1)>>4)*16 + offset%16` /
`fx1 = start_tile*16-2` / `fx2 = end_tile*16+17` arithmetic to place exactly
one floor, then puts each foot on, one pixel inside and one pixel outside
each of its two ends) is what turned it into one: edge=1 and edge=2 each
30-110 times per variant per 500 vectors, the landing path 135-255 times,
the guard-reject 59 of 1500.

### A real, out-of-scope generator gap found (reported, not fixed)

`carrier/gen/pf_lib_bindings.h` emits 23 "AL_INLINE vtable-dispatch macros",
including `#define line(a0, ...) ((a0)->vtable->line((a0), ...))` -- so
`line(screen, ...)`, the spelling the original source used, is correct in
the CARRIER world, and real `<allegro.h>` supplies the same inline in the
UPSTREAM world. The generated, no-bindings `src/icytower/allegro_api.h`
supplies **neither** the macro nor a declaration: that generator
(`port_forge/tools/pf_win32_gen_lib_bindings.py`) emits the AL_INLINE family
only into its bindings half. So `collision.c` compiles in two of the three
worlds and not in the plain-standalone one.

Not fixed here -- `port_forge/` is the shared framework submodule and another
agent is working in this tree. Worked around harness-only, with an `#ifndef
line`-guarded copy of pf_lib_bindings.h's own macro text in
`carrier/lift/harness/pf_harness_calltrace.h`, so the source under test stays
byte-identical in every world. Same class of finding, and same disposition,
as batch 8's two `draw_star_field` gaps.

### Harness changes (additive, none touching `src/`)

- `carrier/lift/harness/icytower_specs.py`: `makecol` added to
  `LIB_CALL_TARGETS` (VA 0x450c98, argc 3, same evidence class as batch 8's
  three); `CALLTRACE_MAKECOL_VA`/`CALLTRACE_LINE_VA`/`VTABLE_LINE_VA`/
  `SCREEN_BMP_VA`/`SCREEN_VTABLE_VA`/`G_DEBUG`/`G_KEY`/`G_SCREEN`/`KEY_F2`;
  `_CT_MAKECOL`/`_CT_LINE`; `_collision_random_map()`/
  `_collision_directed()`/`gen_collision()` plus the shared
  `_COLLISION_DOMAIN`/`_COLLISION_DOMAIN_NAMES`/`_COLLISION_TRACES`; four
  `SPECS` entries; `SRC_BATCH9_FUNCS`, added to both `DEFAULT_FUNCS['src']`
  and `DEFAULT_FUNCS['gcc']`.
- **The vtable call trace is new mechanism, and it needed no engine change.**
  `line` has no callee VA to hook -- it is an inlined vtable dispatch, which
  is exactly why batch 8 listed "2 indirect `bmp->vtable` calls" as a
  blocker for `drawSlot`/`draw_reward`/`draw_results`/`draw_progress_bar`.
  The trick that removes that blocker: point the guest `screen` global at a
  scratch `BITMAP` whose scratch `GFX_VTABLE` carries a **synthetic guest
  VA** (`VTABLE_LINE_VA = 0x7c4000`, otherwise unused) in its `+0x34` slot.
  The ORIGINAL side's `call *0x34(...)` then lands on an address the
  existing `Oracle._make_call_trace_hook` hooks like any named callee; the
  compiled side's dispatch writes the HOST address of `harness_trace_line()`
  into the same slot of its own image copy. Both sides log
  `{count, bmp, x1, y1, x2, y2, color}` into `CALLTRACE_LINE_VA`, and the
  ordinary memory-domain diff compares them -- no new comparator, no engine
  edit. This generalises: **any** `bmp->vtable->N(...)` call site is now
  harness-expressible.
- `carrier/lift/harness/pf_harness_calltrace.h`: `harness_trace_makecol`/
  `harness_trace_line` declarations, `#define makecol harness_trace_makecol`,
  and the `#ifndef line`-guarded AL_INLINE macro (see the generator gap
  above).
- `carrier/lift/harness/call_trace_stubs.c`: the two stub definitions.
  `harness_trace_makecol()` returns 0 because that is what the ORIGINAL
  side's stubbed callee returns (the engine's hook writes EAX = 0);
  `harness_trace_line()` reverse-translates its `bmp` argument through the
  existing `pf_untranslate()` for the reason batch 8 already documented.
- `carrier/lift/harness/icytower_harness_project_gcc.c`: `debug`/`key[127]`/
  `screen` storage (standalone world), a shared `collision_pre()`/
  `collision_post()` sync pair, and four dispatch branches. `collision_pre()`
  carries the one genuinely new fixup: a **two-level** pointer-value
  translation (guest `screen` -> host BITMAP -> host GFX_VTABLE) before
  overwriting the `line` slot -- one level deeper than the
  `ply[player_id]`/`demo`/`data` fixups this project already had.
- `carrier/lift/harness/build_src_gcc.sh`: `collision.c` added to the file
  list; `gcc_check_x87_nosse_batch9.exe` built (`-m32 -mfpmath=387
  -mno-sse2 -O2`).

**`carrier/gen/pf_bindings_src.h` intentionally NOT regenerated this pass**
-- same reasoning as batches 6/7/8 (another agent's concurrent carrier build
owns that file). The carrier-world compile was verified against a scratch
copy built outside `carrier/gen/` (`scan_src_defs.py`'s auto-scanned 51
names + `floor_size_modifiers`; the four new names are picked up
automatically), then discarded.

### Skipped this pass

None of this batch's own targets. Batch 8's remaining skip list
(`draw_table`, `drawSlot`, `draw_reward`, `draw_results`,
`draw_progress_bar`, `draw_frame`, `destroy_game_data`, `get_version_str`)
carries forward unchanged -- except that the **stated blocker for four of
them is now gone**: `drawSlot`/`draw_reward`/`draw_results`/
`draw_progress_bar` were skipped for "indirect `bmp->vtable` calls, a
partially-expressible domain", and the synthetic-vtable-VA trace above makes
exactly that domain expressible. `draw_progress_bar`'s two bare `call *%eax`
(function-pointer variables, not vtable slots) remain a different, still-open
case.

### Totals (updated)

| | batch 9 (this pass) | cumulative (9 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 4 | 46 |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 0 new; 4 graduated out of the list | 8 distinct |
| original bytes recovered (offline-verified) | 4441 (894 + 1071 + 1086 + 1390) | 9489 |

`git diff --stat`-style file list this pass: `collision.c` (new file, 4441
original bytes -- all four variants in one file, since they are one family
and share three of four algorithm halves),
`carrier/lift/harness/icytower_specs.py` (+~180 lines, additive),
`carrier/lift/harness/pf_harness_calltrace.h` (+~45 lines),
`carrier/lift/harness/call_trace_stubs.c` (+~40 lines),
`carrier/lift/harness/icytower_harness_project_gcc.c` (+~65 lines),
`carrier/lift/harness/build_src_gcc.sh` (+1 line).

## Purity gate (batch 9)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 34 file(s) under .../src, 8 violation(s)
```

**All 8 violations are in `src/build/sha256.h`**, an untracked file added to
this tree by the concurrently-running carrier agent (6 "guest address
literal" hits on SHA-256's own round constants, 2 "carrier-reserved
identifier" hits on its `PF_SHA256_H` include guard). **`collision.c`
contributes 0 violations**, and no file this batch touched appears anywhere
in the report. Recorded as measured rather than as a clean "0 violations":
the gate is currently not clean, for a reason belonging to the other agent's
work rather than this one's.

## Compile (all three worlds, batch 9)

```
standalone (generated allegro_api.h, no bindings -- the GCC harness's own world):
  built as part of build_src_gcc.sh below -- 0 errors, 0 warnings, with the
  one #ifndef-guarded `line` macro in pf_harness_calltrace.h that the
  generated allegro_api.h is missing (see "A real, out-of-scope generator
  gap" above)

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK
      -Ithird_party/allegro-4.4.3.1/include
      -Ithird_party/build-allegro-4.4.3.1/include
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower
      -c src/icytower/collision.c
  -- 0 errors, 0 warnings (no #include swap needed, unlike batch 8's two
     draw files: allegro_api.h's own ICYTOWER_UPSTREAM_ALLEGRO branch pulls
     in <allegro.h>, whose real AL_INLINE line() supplies what the generated
     half does not)

  mingw32-make -f src/build/Makefile.standalone
  -- collision.c ADDED to that target's SOURCES (it compiles clean in this
     world, so unlike draw_buffer.c/draw_scroller.c/draw_star_field.c/
     start_reward.c there is no reason to hold it out): libicytower.a +
     standalone_smoke.exe build clean, 0 warnings; standalone_smoke.exe run
     -- 0 failure(s), unchanged. play_sound()/makecol() never need to
     resolve, for the reason that list's own comment already gives: SOURCES
     only feeds a static archive and the final link pulls in only a
     referenced member.

carrier (scratch bindings, GCC -- no MSVC cl.exe in this sandbox, as batch 8):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower      (51 function
      names, auto-scanned; the four new ones picked up automatically)
  python carrier/gen/gen_bindings.py --exclude <scanned 51>,floor_size_modifiers ^
      --guard-define ICYTOWER_BINDINGS_ACTIVE ^
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h

  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower
      -include <SCRATCH>/pf_bindings_src.h -include carrier/gen/pf_lib_bindings.h
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h
      -c src/icytower/collision.c
  -- 0 errors, 0 warnings

offline harness (GCC x87 only):
  bash carrier/lift/harness/build_src_gcc.sh gcc_check_x87_nosse_batch9.exe ^
      -mfpmath=387 -mno-sse2 -O2
  python carrier/lift/harness/lift_check.py --form src --toolchain gcc ^
      --exe harness/gcc_check_x87_nosse_batch9.exe ^
      --funcs handle_player_collision_old,handle_player_collision_vector,^
              handle_player_collision_vector_2,handle_player_collision_combo ^
      --vectors 20000 --census
  -- all four: EQUAL (0/20000), and EQUAL again at --seed 424242
```

## Batch 10 (2026-09-08 -- `draw_frame`, the per-frame renderer)

Task: recover `draw_frame` (0x40929c, 8518 bytes, main.c) -- the largest
function in the image and the one batch 8 stopped at -- as readable
source, and verify it in the call-trace domain batch 9's
synthetic-vtable-VA trick opened up.

**Promoted, EQUAL over 18 000 random + 3 356 directed vectors** in an
ordered-call-trace + memory domain, ORIGINAL bytes vs the COMPILED
`src/icytower/draw_frame.c`, GCC x87 (`-m32 -mfpmath=387 -mno-sse2 -O2`).
No region is left compile-only.

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `draw_frame` | 0x40929c | 8518 | main.c | **EQUAL** (9 seeds x 2000 random = 18000, plus 4 directed campaigns x 839 = 3356; ordered call-trace of ~250 calls/invocation + 11-global memory domain; GCC x87 `-mfpmath=387 -mno-sse2 -O2`. MSVC not run -- no `cl.exe` in this sandbox, as batches 8/9) | Recovered as a FILE of 11 `static` helpers plus the composing `draw_frame` (see "Structure" below). Four of the five `data[N]` computed-index sites `src/icytower/ASSETS.md` still listed as open are resolved this pass. Three genuine findings in the original are recorded below, one of them a real stack-buffer overflow. | pending |

### Structure as recovered

Batch 8's reading of the BINARY still stands -- 0x40929c..0x40b3e1 is one
`.text` range, ~2100 disassembly lines, no internal call boundaries, and
exactly one call to a game-scope helper (`draw_reward`). What batch 8 did
not look at is the DWARF for this subprogram's own LOCALS, and that is
where the original C's seams are still visible: `DW_AT_decl_line` on the
23 locals brackets the body into contiguous, non-overlapping regions
(2491 `x`/`y`, 2492 `p_im`, 2493 `flip`, 2495 `cx`/`cy`, 2496 `ls`, 2498
`fo`, 2499 `so`, 2504 `max_bg_id`, 2549 `f`, 2563 `s`, 2565 `sy`, 2566
`sw`, 2569 `c1`, 2570 `c2`, 2605 `customFrame`, 2606 `oy`, 2607 `ox`,
2743 `myBuf`, 2744 `myPos`, 2773 `vcr`, 2774 `len`, 2786 `scrollerText`),
and `DW_AT_call_line` on its 30 `DW_TAG_inlined_subroutine` records pins
every Allegro AL_INLINE draw to its own source line (2542, 2552, 2555,
2558, 2568, 2582, 2621, 2624, 2638, 2639, 2651, 2699, 2700, 2705, 2712,
2718, 2720, 2778, 2780, 2781, 2782). So the decomposition below is not
invented for readability -- it is the original file's own paragraph
structure, read off DWARF and confirmed against the control flow:

| helper | VA range | source lines | what it draws |
|---|---|---|---|
| `draw_background` | 0x40930c-0x40942a | ~2505-2545 | the 5-deep scrolling background-stripe ring + 5 `blit`s |
| `draw_hurry_sign` | 0x40942b-0x409484 | 2542 | the HURRYUP banner |
| `draw_floors` | 0x409485-0x4098d7 | 2549-2570 | 32 rows: left/middle/right floor tiles, the sign board, its 5-call outlined number, and the debug floor number |
| `draw_stars` | 0x4098d8-0x409999 | 2582 | the 512 reward particles |
| `draw_player` | 0x40999a-0x409d9e | 2605-2651 | frame selection + exactly one sprite |
| `draw_side_rails` | 0x409d9f-0x40a09f | 2699-2700 | 5 x 2 SIDEBLOCK sprites |
| `draw_combo_meter` | 0x40a0a0-0x40a17e | 2705-2712 | meter frame, liquid slice, combo count |
| `draw_clock` | 0x40a17f-0x40a290 | 2718-2720 | CLOCK + rotated CLOCK_HAND |
| `draw_score` | 0x40a291-0x40a2f7 | -- | `draw_reward()` + the score line |
| `draw_replay_hud` | 0x40a2f8-0x40a5ba | 2743-2786 | REPLAY tag, custom-game settings, VCR panel, scrolling title, progress bar |
| `draw_debug_overlay` | 0x40a5bb-0x40a887 | -- | 8 diagnostic lines |

`src/icytower/draw_frame.c` is 931 lines (about half of that a header
comment and per-region commentary). Every helper is `static`, so the
compiler is free to inline the whole thing back into one body; the
composition reproduces the original's ordered call sequence exactly,
which is what the oracle below actually checks.

### The comparison domain, and why it needed a new oracle

`draw_frame` writes almost nothing: eleven globals (`frame_count`,
`last_stripe_y`, `bg_stripe_ids[5]`, `ply[player_id]->frame`,
`scroll_count`, `scroll_delay`, `*allegro_errno`). Everything else it
does is CALLS -- about 250 of them per invocation, in a specific order,
with specific arguments. That is the whole function.

`carrier/lift/harness/lift_check.py`'s existing call-trace mechanism
cannot express that: per callee it records a COUNT plus the arguments of
its FIRST call (PROMOTIONS.md batch 8's own documented limitation, added
there deliberately so `draw_scroller`'s always-identical clip-restore
call could not mask the interesting one). With 15 `draw_sprite()` sites
and 12 `textprintf_ex()` sites in a single `draw_frame` invocation,
first-call-capture would compare roughly 15% of what this function does.
Extending the shared engine to an ordered log would change a mechanism
five other functions already depend on, so this pass added a **separate,
additive oracle** instead, built on the same engine's `build_guest()` and
its FNINIT/FLDCW convention:

- `carrier/lift/harness/draw_frame_xcheck.py` (new) -- seeds one whole
  game state per vector (profile, `map.room[32]`, `Tplayer`,
  `Tparticle stars[512]`, every referenced BITMAP's w/h/colour depth, the
  `Treplay` and its strings, the three menu captions, a scripted
  `new_rand()` sequence), maps the real image, hooks every library callee
  VA **and** the five `GFX_VTABLE` slots `draw_frame` reaches through
  Allegro AL_INLINEs, executes the ORIGINAL bytes, and records an ordered
  trace.
- `carrier/lift/harness/draw_frame_check.c` (new) -- the compiled-
  candidate half: a standalone driver that rebuilds the same state in
  HOST memory (the standalone world, `src/icytower/state.c` supplies the
  globals), calls the real `draw_frame()` against stub Allegro entry
  points and a stub `GFX_VTABLE`, and writes the same ordered trace.
- `carrier/lift/harness/draw_frame_model.py` (new) -- a Python
  transliteration of the recovered C, used as an independent candidate
  (`--model`) for the "unicorn cross-check comes first" step this project
  applies to any novel algorithm (batches 4, 6, 9).

**Batch 9's synthetic-vtable-VA trick generalised.** Batch 9 pinned ONE
slot (`line`, +0x34) by pointing a scratch `GFX_VTABLE` slot at an
otherwise-unused guest VA the engine could hook. `draw_frame` needs five
at once -- +0x44 `draw_sprite`, +0x48 `draw_256_sprite`, +0x50
`draw_sprite_h_flip`, +0xa4 `pivot_scaled_sprite_flip`, +0xbc `rect` --
and it needs them on TWO vtables (a 16bpp and an 8bpp one), because
Allegro's `draw_sprite()` AL_INLINE branches on the SPRITE's colour depth
and dispatches through the DESTINATION's vtable. Both are in the oracle;
every vector randomises each bitmap's depth, so both arms of all 15
`draw_sprite()` sites are exercised (measured: 34/1500 vectors take the
8bpp arm at least once -- in fact all 1500 do, since 25% of ~150 bitmaps
are 8bpp).

**Two normalisation rules the domain needs, both learned the hard way:**

1. **Pointers are rendered as SYMBOLS**, not addresses (`data[N]`,
   `custom.frame[i]`, `bmp`, `swap_screen`, `font`, `buf#k`), so the
   guest and host address spaces never have to agree. Same problem batch
   8's `pf_untranslate()` solves for the fixed-slot domain, solved here
   by naming rather than translating -- which also makes a divergence
   readable ("`draw_sprite|bmp|data[19]|...`" instead of two malloc
   addresses).
2. **`const char *` arguments are resolved to their CONTENT AT CALL
   TIME**, not at comparison time. The first draft resolved them lazily
   and immediately produced a false DIFFER: `myBuf` and `scrollerText`
   are each reused for several different strings within one invocation
   (`"REPLAY"`, then `"<x> Floors"`, then `"<x> Speed"`, then the gravity
   caption), so a deferred read compares the LAST content written, not
   the one that call saw.

### Results

```
ORIGINAL bytes (unicorn, 0x40929c) vs COMPILED src/icytower/draw_frame.c
  random   9 seeds x 2000 vectors = 18000 : differ 0
  directed 4 campaigns x 839      =  3356 : differ 0
ORIGINAL bytes vs the Python model (draw_frame_model.py)
  random   4 seeds x 1000 vectors =  4000 : differ 0
mean 254 traced calls per vector; 11-global memory domain compared on
every vector as well
```

Directed campaign (`--directed`): the cross-product of player status x 21
`p->sx` boundary values (0, +-0.01, +-0.02, +-0.2 and each +-1ulp-ish
neighbour) x `p->sy` around +-3.0; `logic_count` x `map.offset` x `p->y`
around the idle-animation windows; `hurry_y` x `clock_angle` around
-100/200/250/480 and 0/1500; `p->edge` x `p->rotate` x `p->frame`;
`room.tiles` x `room.level` x `profile->start_floor` around the asset
clamps; and `recording` x `is_playing_custom_game` x `debug` x
comment-present x `scroll_count`.

Branch coverage measured over 1500 random vectors (the classes that could
plausibly have gone untested): background stripe generated 1138, >=2
stripes in one frame 354, 8bpp `draw_256_sprite` arm 1500, `h_flip` arm
1500, player `rotate_sprite` 326, `draw_reward` 770, replay progress bar
1055, custom-game settings 409, replay title scroller 1055, floor sign
digits 1500, debug overlay 264, player status 0/1/2-3/other
485/261/518/236, player on edge 574, combo liquid 877, hurry banner 1085,
clock face shake 387, clock hand shake 648, `ftofix` clock angle 1321,
idle frames 9/10/11/0 26/21/24/54, running-with-`frame>3` reset 53, floor
`f` clamp 20482 rows, sign `s` clamp 20482 rows, `level>4999` style bump
12901 rows, rows with no middle tiles 11938.

### Negative controls

Ten deliberate one-token faults injected into `src/icytower/draw_frame.c`,
each rebuilt and re-run over the same 200 vectors:

| fault | result |
|---|---|
| floor left cap `t*16-5` -> `t*16-4` | DIFFER 200/200 |
| sign outline `sy+7` -> `sy+8` | DIFFER 200/200 |
| clock hand pivot `34+ox` -> `35+ox` | DIFFER 200/200 |
| stars target `swap_screen` -> a bitmap | DIFFER 200/200 |
| combo liquid `219-in_combo` -> `218-...` | DIFFER 109/200 |
| clock scale `0.1706666` -> `0.1706667` | DIFFER 113/200 |
| stripe loop `while` -> `if` | DIFFER 96/200 |
| background blank threshold `40` -> `41` | DIFFER 2/200 |
| idle window `logic_count > 24` -> `> 23` | DIFFER 2/200 |
| replay progress bar `117` -> `116` | DIFFER 17/200 |
| side-rail scale `1.476` -> `1.477` | DIFFER 15/200 |
| scroller wrap `-250` -> `-251` | DIFFER 8/200 |
| sign clamp `s > 110` -> `s > 111` | candidate CRASHES (0xC0000005) -- the clamp is load-bearing: `s = 111` is SIGN09, the last member of its family, and 112 is a different object |
| floor clamp `f > 44` -> `f > 45` | DIFFER 0/200 -- an **equivalent mutant**, not a miss: `f = 17 + 3*(start_floor + tiles)` is always `== 2 (mod 3)`, so `f` is never 45 and the two guards are the same predicate. Recorded rather than quietly dropped. |

### Three findings in the ORIGINAL, recovered rather than corrected

1. **The reward star particles are drawn onto `swap_screen`, not onto
   `bmp`.** Every other draw in this function targets the `BITMAP *`
   argument; `draw_stars` (0x409965) loads the global `swap_screen`
   (0x4dd194) and passes THAT as the destination on both the
   `draw_sprite` and `draw_256_sprite` arms. `draw_reward(swap_screen)`
   (0x40a90d) does the same. Recovered faithfully -- and it is not a
   transcription slip, because the negative control "stars target
   `swap_screen` -> a bitmap" DIFFERs on 200/200 vectors, i.e. the oracle
   sees the destination.
2. **`sprintf(scrollerText, "%s%s%s", demo->name, " - ", demo->comment)`
   can overflow its own stack buffer by 6 bytes.** DWARF gives
   `scrollerText` as `char[70]` (`DW_OP_fbreg -102`, confirmed against
   the `lea -0x5e(%ebp)` at 0x40a3f4); `Treplay.name` is `char[32]` and
   `Treplay.comment` is `char[42]`, so the worst case is 31 + 3 + 41 + 1
   = 76 bytes. GCC's `-Wformat-overflow` flags it in the recovered source
   ("output between 4 and 76 bytes into a destination of size 70") --
   **this pass's only compiler warning, and it is a real defect in the
   original, not an artefact of the recovery.** Left uncorrected, per the
   same rule `draw_buffer.c`'s dropped-last-line already set.
3. **`ftofix`'s `ERANGE` guard is unreachable from this call site.**
   `draw_clock` feeds it `(clock_angle % 1500) * 0.1706666`, whose
   magnitude cannot exceed 255.8, so neither `*allegro_errno = ERANGE`
   branch can fire (measured: 0 of 1500 vectors, with `clock_angle`
   deliberately drawn from a +-100000 pool). `*allegro_errno` is in the
   compared memory domain anyway, so this is a measured fact, not an
   assumption.

### Asset seam: four of the five open computed-index sites resolved

`src/icytower/ASSETS.md` "What remains hand-mapped" listed five
itemized-but-unresolved `data[N]` computed-index sites; two of its rows
(VA 0x409383 and VA 0x4095ff) are inside `draw_frame`, and reading the
function resolved four sites in total. Each is resolved the way
`start_reward.c`'s `ASSET_DATA_REWARD_000 + tier` already was
(PROMOTIONS.md batch 7): the base object opens a run of consecutively-
named objects that `assets_table.inc` GENERATES contiguously from the
manifest's own consecutive object names, so `<base id> + k` is a
mechanical offset inside one generator-guaranteed family -- never "an
asset_id used as a global datafile index".

| VA | expression | resolved to | range argument |
|---|---|---|---|
| 0x409383 | `data[bg_stripe_ids[i] + 1]` | `ASSET_DATA_BGTILE + id` | `id = new_rand() % max_bg_id`, `max_bg_id <= 5`; BGTILE..BGTILE5 = data 1..6 |
| 0x409508 / 0x4095a5 / 0x409605 | `data[f]`, `data[f+1]`, `data[f+2]` | `ASSET_DATA_FLOOR_01 + (f - 17)` | `f = 17 + 3*(profile->start_floor + room.tiles)`, clamped to 44 then `+3` if `level > 4999`, so `f+2 <= 49`; FLOOR01..FLOOR27 = data 17..49, 33 objects = 11 triples, and 49 is the family's last member exactly |
| 0x409690 | `data[s]` | `ASSET_DATA_SIGN_01 + (s - 101)` | `s = 101 + start_floor + room.tiles`, clamped to 110 then `+1`, so `s <= 111`; SIGN01..SIGN09 = data 101..111 |
| 0x409959 | `data[stars[i].color + 117]` | `ASSET_DATA_STAR_01 + color` | `create_particle()` draws `color` as `new_rand() % 8`; STAR01..STAR08 = data 117..124 |

The two remaining sites the census counts are in `play()` (0x4146e4,
0x4149e6), untouched by this pass. `src/icytower/ASSETS.md`'s own table
is updated to match.

### Two real, out-of-scope generator gaps found (reported, not fixed)

1. **Two more `MEMBER_ACCESS_COLLISIONS` cases, both in one line of the
   debug overlay.** `demo->data[rec_pos].key_flags` and
   `.cycle_count` collide with the top-level globals `DATAFILE *data`
   (0x4dd23c) and `volatile int cycle_count` (0x506938), whose blunt
   textual `#define`s in `carrier/gen/pf_bindings_src.h` rewrite the
   member accesses into syntax errors -- the class batch 7 fixed for
   `jump_sound` and batch 8 could not fix for `stars`. These two are the
   `stars` case again (other, unpromoted code reads both globals bare, so
   skipping their `#define`s would break that code in the same build).
   **Worked around inside `draw_frame.c`** with a guarded `#undef data` /
   `#undef cycle_count` at the top of the file, which is legitimate here
   rather than merely expedient: this is exactly the file that must never
   touch the datafile global, because ASSETS.md's seam requires it to go
   through `asset_bitmap()`/`asset_font()` instead. The real fix is still
   the context-sensitive rewrite `gen_bindings.py`'s own comment already
   describes.
2. **`draw_sprite`, `rotate_sprite`, `fixtoi` and `ftofix` have no
   binding in any generated header.** `carrier/gen/pf_lib_bindings.h`
   emits the 23 AL_INLINEs that are a straight one-call vtable
   passthrough (so `draw_sprite_h_flip` and `rect` resolve in the carrier
   world), but by construction emits neither the ones with a branch or
   arithmetic of their own (`draw_sprite` branches on colour depth;
   `rotate_sprite` computes a pivot) nor the fixed-point family -- the
   "inline function" gap `carrier/gen/LIB_BINDINGS_NOTES.md` already
   documents for `itofix`/etc. The generated, no-bindings
   `src/icytower/allegro_api.h` emits none of the four, the same gap
   batch 8 found for `rectfill`/`putpixel` and batch 9 for `line`.
   Handled the way batch 9 handled `line`: an `#ifndef`-guarded,
   upstream-faithful definition of exactly the missing names inside
   `draw_frame.c`, so the source under test stays byte-identical in every
   world and real `<allegro.h>` always wins where it is present. A real
   fix belongs in `port_forge/tools/pf_win32_gen_lib_bindings.py`, which
   this task does not own.

### In vivo (for the carrier task -- NOT run by this pass)

This pass did not run `carrier.exe` (another agent owns the carrier).
`draw_frame` is called from four sites inside `play()` (0x41320e,
0x41416f, 0x414678, 0x4149a8), so binding it exercises every drawn frame,
and the frame oracle at `blit_to_screen` is the direct check:

```
python carrier\gen\scan_src_defs.py --src-dir src\icytower
python carrier\gen\gen_bindings.py --exclude <scanned names>,floor_size_modifiers ^
    --guard-define ICYTOWER_BINDINGS_ACTIVE ^
    --out carrier\gen\pf_bindings_src.h --types-out carrier\gen\pf_bindings_src_types.h
carrier\build.cmd                         (draw_frame.c picked up automatically)

carrier.exe --bind draw_frame=src --replay replays\human_test.txt --frame-digest
carrier.exe --replay replays\human_test.txt --frame-digest        (unbound baseline)
   -- per-frame digests at blit_to_screen must be EQUAL for all 2293 ticks,
      and the run must end on the same score 2386 / floor 100 witness
      (divergence 009's regenerated baseline)

carrier.exe --bind draw_frame=src --replay <the .itr workload> --frame-digest
```

Two things worth watching that the offline oracle structurally cannot
see, both of the class `notes/living_record.md` divergence 008 is about:
(a) `new_rand()` must resolve to the GUEST's copy -- `draw_background`
consumes 1 or 2 draws per generated stripe from the same `seed` the rest
of the game shares, so a wrong binding desynchronises the tower, not just
the wallpaper; (b) `swap_screen` must be the guest's own back buffer, not
a carrier-side one, or the star particles and the reward animation land
on a bitmap nothing blits.

### Harness changes (additive, none touching `src/`)

- `carrier/lift/harness/draw_frame_xcheck.py` (new, ~1060 lines): the
  ordered-call-trace oracle described above, three campaigns
  (`--random`, `--directed`, `--model`).
- `carrier/lift/harness/draw_frame_check.c` (new, ~424 lines): the
  compiled-candidate driver (its own `main()`; reads the vector file,
  rebuilds the state in host memory, stubs the Allegro entry points and a
  `GFX_VTABLE`, writes the trace).
- `carrier/lift/harness/draw_frame_model.py` (new, ~397 lines): the
  Python transliteration used by `--model`.
- `carrier/lift/harness/lift_check.py` / `icytower_specs.py`: **not
  touched.** This function does not fit the SPECS protocol (see "why it
  needed a new oracle"), and bending that protocol would have changed a
  mechanism five already-verified functions depend on.

**`carrier/gen/pf_bindings_src.h` intentionally NOT regenerated this
pass** -- same reasoning as batches 6/7/8/9 (another agent's concurrent
carrier build owns that file). The carrier-world compile was verified
against a scratch copy built outside `carrier/gen/`
(`scan_src_defs.py`'s auto-scanned list, which picks up `draw_frame` and
this file's 11 statics automatically), then discarded.

`src/build/Makefile.standalone`'s `SOURCES` is deliberately unchanged:
`draw_frame.c` is an asset-seam file (it calls `asset_bitmap()`/
`asset_font()`), so it is held out of the basic smoke archive for exactly
the reason `draw_buffer.c`/`start_reward.c` already are, and is
compile-checked separately below. That target still builds clean and
`standalone_smoke.exe` still reports 0 failure(s).

### Totals (updated)

| | batch 10 (this pass) | cumulative (10 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 1 | 47 |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 0 new; `draw_frame` graduated out of the list | 7 distinct |
| original bytes recovered (offline-verified) | 8518 | 18007 |

`git diff --stat`-style file list this pass: `src/icytower/draw_frame.c`
(new file, 931 lines, 8518 original bytes),
`carrier/lift/harness/draw_frame_xcheck.py` (new),
`carrier/lift/harness/draw_frame_model.py` (new),
`carrier/lift/harness/draw_frame_check.c` (new),
`src/icytower/ASSETS.md` (computed-index table updated),
`artifacts/src_equivalence.json` (`draw_frame` + `pass_2026-09-08_batch10`).

## Purity gate (batch 10)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 38 file(s) under .../src, 0 violation(s)
```

(Clean again -- batch 9's 8 violations were all in `src/build/sha256.h`,
an untracked file the concurrently-running carrier agent had added; it is
no longer in this tree.)

## Compile (all three worlds, batch 10)

```
standalone (generated allegro_api.h, no bindings):
  gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/draw_frame.c
  -- 0 errors, 0 warnings

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
      -Ithird_party/allegro-4.4.3.1/include \
      -Ithird_party/build-allegro-4.4.3.1/include \
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower \
      -c src/icytower/draw_frame.c
  -- 0 errors, 1 warning (the -Wformat-overflow on scrollerText: a REAL
     defect in the original, see "Three findings" above -- not silenced)
  (no #include swap needed: allegro_api.h's ICYTOWER_UPSTREAM_ALLEGRO
   branch pulls in <allegro.h>, whose real AL_INLINEs win over this
   file's #ifndef-guarded stand-ins)

  mingw32-make -f src/build/Makefile.standalone
  -- SOURCES unchanged (asset-seam file, held out like draw_buffer.c/
     start_reward.c): libicytower.a + standalone_smoke.exe build clean,
     standalone_smoke.exe run -- 0 failure(s), unchanged.

carrier (scratch bindings, GCC -- no MSVC cl.exe in this sandbox):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower
  python carrier/gen/gen_bindings.py --exclude <scanned>,floor_size_modifiers ^
      --guard-define ICYTOWER_BINDINGS_ACTIVE ^
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower \
      -include <SCRATCH>/pf_bindings_src.h \
      -include carrier/gen/pf_lib_bindings.h \
      -include carrier/gen/pf_asset_bindings.h \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/draw_frame.c
  -- 0 errors, 1 warning (the same -Wformat-overflow; plus 3
     -Wunused-function notices that belong to the generated
     pf_asset_bindings.h itself, not to this file)

offline oracle (GCC x87 only):
  gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      carrier/lift/harness/draw_frame_check.c src/icytower/draw_frame.c \
      src/icytower/control.c src/icytower/state.c \
      -o carrier/lift/harness/draw_frame_check.exe
  python carrier/lift/harness/draw_frame_xcheck.py --random --seed <s> --vectors 2000
  python carrier/lift/harness/draw_frame_xcheck.py --directed --seed <s>
  python carrier/lift/harness/draw_frame_xcheck.py --model --seed <s> --vectors 1000
  -- 18000 random + 3356 directed + 4000 model vectors: differ 0
```

## Batch 11 (2026-09-08 -- the input seam and the presentation seam)

Task: the per-tick functions `play()` calls, so the tick body becomes clean
source around the already-recovered physics/collision/map/draw code.

**Three functions promoted, all EQUAL**: `handle_player_input` (the single
input->simulation seam), `blit_to_screen` (the presentation seam, and the
point the carrier's `--frame-digest` oracle samples), and `poll_control`
(the keyboard/joystick reader `handle_player_input` calls, promoted so that
the seam could be verified end to end with nothing stubbed between the keys
and the physics).  Plus a complete, byte-accounted structure map of `play()`
itself -- see "play(): the structure, from the line table" below.

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `handle_player_input` | 0x40b3e4 | 728 | main.c | **EQUAL** (4 seeds x 20000 = 80000; memory domain 760 bytes -- Tcontrol + the whole Tplayer + `rec_pos` + the Trecord[64] window + `profile->total_jumps` + the `play_sound`/`poll_joystick` call-trace slots; GCC x87 `-m32 -mfpmath=387 -mno-sse2 -O2`) | The first promoted function whose callees are ALL already promoted, so nothing between the keys and the physics is stubbed: unicorn runs the original bytes of `is_left`/`is_right`/`is_fire`/`jump_player`/`play_jump_sound`/`poll_control`, and the compiled side calls the clean `src/icytower/` forms of the same six. Two corrections to `notes/replay_format.md` below. | ready once `pf_bindings_src.h` is regenerated (the `ctrl` -> `ctrl_arg` policy fix has landed) |
| `blit_to_screen` | 0x40b6bc | 1415 | main.c | **EQUAL** (5 seeds x 1500 random = 7500 + 2 directed campaigns x 529 = 1058; ORDERED call-trace of `blit`/`stretch_blit` + 5 `GFX_VTABLE` slots, plus the `blit_mode` memory domain; GCC x87) | Seven presentation modes behind a debug F-key override; in vivo only mode 0 (`blit`) is reachable because `debug` has no store anywhere in the image. New oracle `carrier/lift/harness/blit_to_screen_xcheck.py`, modelled on batch 10's `draw_frame_xcheck.py`. | **compiles clean; LINK blocked on one unbound Allegro data global** (`_cos_tbl`) -- see "Three generator gaps" |
| `poll_control` | 0x401958 | 294 | control.c | **EQUAL** (4 seeds x 20000 = 80000; Tcontrol + the `poll_joystick` call-trace slot) | Merges keyboard and joystick into one `Tcontrol.flags` byte. Its own file (not the bottom of `control.c`) because `control.c` has a parameter named `key` -- see "Two parameter-name collisions". | ready |

### The 0x80 sentinel: `notes/replay_format.md`'s open question, answered

That document records the sentinel path's location (0x40b634/0x40b645) but
leaves its trigger and meaning as INFERRED guesses -- "reached when a
difficulty-table field is nonzero", "practice-mode/segment-boundary marker".
**Both were wrong.**  Read off the disassembly and confirmed by the offline
oracle's own coverage census (358 of 2000 vectors take this arm):

- The trigger is `ply[player_id]->dead` (Tplayer+0x4c, loaded at 0x40b5b8 --
  the very field `play()` polls at 0x41246a/0x41249f to end the game), OR a
  record whose `key_flags` already has bit 7 set, i.e. a re-entry after the
  sentinel has been written once.
- The effect is a **two-record trailer** written at `data[rec_pos + 1]` and
  `data[rec_pos + 2]` -- `{0x80, 0}` then `{0x00, 0}` -- and `rec_pos` is
  **not** advanced.  Every subsequent dead tick rewrites the same two
  records rather than growing the stream.
- So it is an **end-of-input terminator, idempotent by design**: the
  recording stops growing the instant the player dies, and the terminator
  sits one past the last real record, exactly where a decoder walking
  `rp = rec_pos - 1` would meet it.

### The cursor is biased by one, and the two halves disagree about it

Also new, and load-bearing for anyone writing an `.itr` driver: the DECODER
reads `demo->data[rec_pos - 1]` (the DWARF local `rp`, decl_line 2394) while
the ENCODER extends `demo->data[rec_pos]` and writes its new record at
`demo->data[rec_pos + 1]` via a pre-increment.  The same global cursor, one
record apart.  That is not a bug -- it is why a recording can be replayed by
the same code without rewinding -- but a driver that assumes one convention
for both halves is off by one record.

Three decoder cases, in the original's own order, all measured by the
coverage census:

| case | effect |
|---|---|
| `rp < 0` | `rec_pos++`, and `ctrl->flags` is **left alone** -- the first tick of a playback only advances the cursor |
| `rp >= demo->size` | `ctrl->flags = 0` and the cursor does **not** advance -- past the end of a finished replay the player simply stops steering; not an error path, nothing is clamped |
| otherwise | `ctrl->flags = r->key_flags`; if `r->cycle_count > 0` decrement it **in place** (the replay buffer is consumed destructively), else advance `rec_pos` |

A record with `cycle_count == 0` therefore plays for exactly one tick.

### `gamepad` is the joystick's runtime remap table; the keyboard's is hard-coded

`poll_control`'s two halves are deliberately asymmetric, and that asymmetry
is the whole reason the global `Tgamepad gamepad` (0x4f8748) exists.  The
keyboard half OR-s in seven **immediate constants** (`orb $0x4,0x20(%ebx)`
etc. -- the same CTRL_* bit assignment `control.c`'s `is_up`..`is_pause`
already recovered from the consuming side, so producer and consumers now
agree bit for bit).  The joystick half instead **loads the bit to set out of
`gamepad`**, so a pad's four directions and 32 buttons are each remappable
to any control mask at runtime.  `gamepad.b[]` is `int[32]`, and the loop's
`b == 32` early exit (0x401a4c) is exactly that array's bound.

Only joystick 0 is ever read, and only six of its fields: `num_buttons`,
`stick[0].axis[1].d1/.d2` (up/down), `stick[0].axis[0].d1/.d2`
(left/right), and `button[i].b`.  `joystick_only` (the DWARF parameter name)
gates only the keyboard half -- with it set the pad is still polled.

### `blit_to_screen`: a function-static, and one draw that targets the wrong bitmap

`blit_mode` is a FUNCTION-STATIC (DWARF: decl_line 2268, `DW_OP_addr
0x4dd324`), i.e. state that must persist across calls AND stay at that one
address while state is address-backed.  Writing `static int blit_mode;` in
`src/` would give this port private storage that does not alias 0x4dd324, so
the carrier would silently keep two copies.  The generated headers already
solve this -- `gen_src_headers.py` mangles a function-static to
`<name>__<function>` and emits it as an ordinary extern with storage in
`state.c` and a binding at 0x4dd324 -- so `blit_to_screen.c` spells it
`blit_mode__blit_to_screen`.  That is the generator's spelling, not an
invention, and it is the one identifier in that file which is not the
original's own.

The seven modes: 0 straight `blit`; 1/2 h-flip and v-flip through the
screen's vtable; 3 a 480-scanline sine wobble whose amplitude is half the
player's `level`, in pixels; 4 a vertical wrap-scroll at `level % 480`; 5/6
2x and 4x player-following `stretch_blit` zooms.  Mode 4's two black `line`
calls target **`bmp`, not `screen`** -- the debug separators are drawn into
the back buffer, so they persist into whatever the next frame draws over.
Recovered as-is, the same class of finding as batch 10's "reward stars go to
`swap_screen`", and the negative control "mode4 bottom line y 479 -> 478"
DIFFERs on 83/529 vectors, so the oracle really does see the destination.
Only mode 0 is reachable in vivo (`debug` has no store anywhere in the
image, established in batch 9), but all seven are recovered and all seven
are driven by the oracle.

### The oracles

**`handle_player_input` / `poll_control` use the EXISTING SPECS mechanism**
(`carrier/lift/harness/icytower_specs.py`, additive rows) rather than a new
one, because their comparison domain really is a fixed byte range: 760 and
40 bytes respectively.  What is new is that the domain reaches through six
promoted callees on both sides.  Only two leaves are stubbed, both of which
leave the game entirely:

- `play_sound` (0x406da4) -- the existing `CALLTRACE_PLAY_SOUND_VA` slot,
  unchanged from batch 7.
- `poll_joystick` (0x43e654) -- new, argc 0, so the slot is a single
  call-count word.  Allegro reaches DirectInput through the joystick
  driver's vtable, which unicorn cannot run and the compiled side has no
  device for; what matters is that the pad is polled exactly when
  `c->use_joy` says so, and the `joy[]` contents both sides then read are
  the ones the vector generator seeded.

**`blit_to_screen` needed a new oracle** for exactly batch 10's reason, one
mode down: mode 3 issues 480 `blit` calls per invocation with 480 different
`d_x` values, and the shared call-trace mechanism records per callee a COUNT
plus the arguments of its FIRST call.  So
`carrier/lift/harness/blit_to_screen_xcheck.py` +
`blit_to_screen_check.c` are a standalone ordered-trace pair built on the
same engine, modelled directly on `draw_frame_xcheck.py`/`draw_frame_check.c`
and leaving `lift_check.py`/`icytower_specs.py`'s existing mechanism
untouched.  Batch 9's synthetic-vtable-VA trick is used for five slots at
once: `acquire` (+0x10), `release` (+0x14), `draw_sprite_v_flip` (+0x4c),
`draw_sprite_h_flip` (+0x50) on the screen's vtable, and `line` (+0x34) on
the bitmap's.  `acquire`/`release` are the interesting pair -- Allegro's
AL_INLINE tests each slot for NULL before calling, so every vector
randomises whether each is present.

Allegro's `_cos_tbl[512]` is shipped to the compiled side **in the vector
file, read out of the ORIGINAL image**, so mode 3's wobble is checked
against the real table rather than a re-derived one.

### Branch coverage, measured rather than argued

`carrier/lift/harness/batch11_coverage.py` (new) drives the SAME generators
`lift_check.py` uses, runs the ORIGINAL bytes in unicorn the way the engine's
Oracle does, and counts how many vectors execute each interesting basic-block
address (read off `artifacts/disasm.txt`, one per recovered branch).  Over
2000 vectors, every branch is reached; the rarest are the ones worth naming:

```
handle_player_input          poll_control
  playback arm        957      joystick arm          1112
  rp in range         341      button loop body      1063
  cycle_count > 0     175      b == 32 test          1053
  advance rec_pos     406      keyboard arm           888
  rp past size        376      key_up..key_pause  270-324 each
  recording arm       957
  0x80 sentinel       358
  extend current run   63
  start a new record  536
  left / right / neither  649 / 293 / 972
  left brake / right brake  324 / 118
  rejump!=0 / rejump==0 arm  274 / 1640
  jump_key latched     69
  jump_key cleared   1081
  profile->total_jumps  20
```

### Negative controls

Two kinds.  The engine's own one-bit `--fault` control for the two SPECS
functions (`--fault <fn>:5:0`, 200 vectors: comparator names
`Tcontrol+0x0 (VA 0x00796000)` exactly for both), and deliberate
**source mutations**, each rebuilt and re-run:

| mutation | result |
|---|---|
| `handle_player_input`: RLE mask 0x93 -> 0x92 | DIFFER v5, `Trecord[64]+0x40` |
| `handle_player_input`: sentinel 0x80 -> 0x40 | DIFFER v13, `rec_pos` |
| `handle_player_input`: cursor bias `rec_pos-1` -> `rec_pos` | DIFFER v0, `Tcontrol+0x20` |
| `handle_player_input`: `cycle_count > 0` -> `>= 0` | DIFFER v12, `rec_pos` |
| `handle_player_input`: sentinel written at +0/+1 not +1/+2 | DIFFER v13, `Trecord[64]+0x38` |
| `handle_player_input`: left brake 0.7 -> 0.71 | DIFFER v15, `Tplayer+0x10` (sx) |
| `handle_player_input`: idle decay 0.9 -> 0.89 | DIFFER v1, `Tplayer+0x10` |
| `handle_player_input`: jump latch -1 -> 1 | DIFFER v34, `Tplayer+0x38` (jump_key) |
| `handle_player_input`: `dead` test dropped | DIFFER v51, `rec_pos` |
| `handle_player_input`: left brake `> 0` -> `>= 0` | **EQUAL 0/400 -- an equivalent mutant, recorded not hidden.** At `sx == 0.0` the brake multiplies by 0.7 and stores the same value back, so the extra store is unobservable; `-0.0 * 0.7` is `-0.0`, and NaN fails both compares. The disassembly says strict (`fucom`/`test $0x45,%ah`/`jne`), the source says strict, and the two spellings are provably indistinguishable here. |
| `poll_control`: joystick up/down swapped | DIFFER v3, `Tcontrol+0x20` |
| `poll_control`: `CTRL_FIRE` 0x10 -> 0x20 | DIFFER v20, `Tcontrol+0x20` |
| `poll_control`: `joystick_only` gate inverted | DIFFER v1, `Tcontrol+0x20` |
| `poll_control`: button bound 32 -> 31 | DIFFER v0 -- **but only after the generator was strengthened.** With the first draft's uniform draw this mutant survived 400 vectors: some other button or key almost always contributed the same bit anyway. `gen_poll_control` now carries a directed class (every 9th vector) where button 31 is the ONLY one pressed, the keyboard half is off, and its `gamepad.b[31]` mask is a bit nothing else can supply. |
| `poll_control`: button bound 32 -> 33 | EQUAL 0/400 -- **equivalent in this domain, recorded.** Iterating once more reads `joy[0].button[32].b` and `gamepad.b[32]`, both one element past their arrays; those bytes are zero on both sides (guest .bss, and the harness's own zero-initialised storage), so the extra iteration is a no-op. The bound is verified from the low side only. |
| `blit_to_screen`: mode3 loop `y < 480` -> `< 479` | DIFFER 83/529 |
| `blit_to_screen`: mode3 amplitude `/2` -> `/4` | DIFFER 75/529 |
| `blit_to_screen`: mode3 phase `*5` -> `*4` | DIFFER 51/529 |
| `blit_to_screen`: mode4 bottom line y 479 -> 478 | DIFFER 83/529 |
| `blit_to_screen`: mode4 wrap `y-480` -> `y-479` | DIFFER 83/529 |
| `blit_to_screen`: mode5 x clamp 320 -> 319 | DIFFER 56/529 |
| `blit_to_screen`: mode5 window 320x240 -> 320x241 | DIFFER 170/529 |
| `blit_to_screen`: mode6 offset 80 -> 81 | DIFFER 96/529 |
| `blit_to_screen`: mode0 `bmp->w` -> `screen->w` | DIFFER 5/529 |
| `blit_to_screen`: F5 selects mode 4 not 3 | DIFFER 1/529 |
| `blit_to_screen`: `if (debug)` gate removed | DIFFER 6/529 |
| `blit_to_screen`: `release_screen()` dropped | DIFFER 341/529 |

### Two parameter-name collisions, and the mechanism that already existed

Promoting `handle_player_input` broke the CARRIER-world compile of **every**
translation unit that includes the generated `src/icytower/game_funcs.h` --
`collision.c`, `draw_frame.c`, `particle.c`, `play_jump_sound.c`,
`start_reward.c`, `map.c`, ... -- and not because of anything in the new
file.  DWARF names the parameter `ctrl` (decl_line 2389); `ctrl` is also a
game global (`Tcontrol ctrl` @0x5000c8) that `pf_bindings_src.h` turns into
a blunt textual `#define`; and `game_funcs.h`'s prototype is emitted only
when no such macro exists -- i.e. exactly once the function is promoted.  So
the prototype `void __cdecl handle_player_input(Tcontrol *ctrl);` appeared
for the first time this batch and macro-expanded into a syntax error
everywhere.

The project already had the right mechanism and it needed one data value:
`carrier/win32_policy.json`'s `param_renames`, which was already renaming
`key` -> `key_arg` for `check_control_key`.  Added `"ctrl": "ctrl_arg"`,
regenerated `src/icytower/`'s five generated files with the documented
`gen_src_headers.py` command, and the only content changes are three
prototypes (`handle_player_input`, `handle_menu`, `update_game_menu`) --
everything else differs only in a header-comment path that had already
drifted to the port_forge shim name.  `handle_player_input.c`'s definition
matches; `ctrl_arg` is the one identifier in that file which is not the
original's own.  **`carrier/gen/` was not touched** -- the fix is a policy
DATA value, which is precisely what `carrier/win32_policy.json` exists for.

The same investigation turned up a **pre-existing** instance of the same bug
that nothing had tripped over yet: `control.c`'s
`check_control_key(Tcontrol *c, int key)` collides with Allegro's global
`key[]` array (`#define key ...` in `pf_lib_bindings.h`), so that file has
never compiled in a carrier world with `pf_lib_bindings.h` force-included.
The policy already renamed the prototype's parameter; the definition now
matches it too.  Fixed as encountered, unrelated to this batch's own
functions.

Carrier-world compile after both fixes: **26 of 27 `src/icytower/*.c` clean**;
the one failure is `draw_star_field.c`, batch 8's already-documented
`stars` MEMBER_ACCESS_COLLISIONS gap, untouched by this pass.

### Three generator gaps found (two reported, one worked around)

1. **`_cos_tbl` has no binding in any generated header, and it is DATA.**
   `blit_to_screen`'s mode 3 calls `fixsin`, an Allegro AL_INLINE whose whole
   body is `_cos_tbl[((x - 0x400000 + 0x4000) >> 15) & 0x1FF]`.
   `carrier/gen/pf_lib_bindings.h` emits a library global only when the
   referencing GAME CU's own DWARF DIE carries the address; every game-CU
   reference to `_cos_tbl` is declaration-only (`DW_AT_declaration`, no
   `DW_AT_location`), while the DEFINING DIE -- in Allegro's own math.c CU,
   `<0x880f0>` -- does have `DW_OP_addr: 0x4ce100`.  So the fix is a
   generator one: when a lib global is referenced by declaration only,
   resolve it to its defining DIE.  Until then `blit_to_screen.c` compiles
   in the carrier world (the extern is legal) but **will not link** -- this
   is the one thing standing between this function and an in-vivo bind.
   Unlike batches 8/9/10's `rectfill`/`putpixel`/`line`/`draw_sprite` gaps,
   an `#ifndef` stand-in cannot substitute for a missing DATA binding.
   The value needed: `_cos_tbl`, VA 0x4ce100, `fixed[512]`,
   `C:\Lib\allegro4\src\math.c`.
2. **`MEMBER_ACCESS_COLLISIONS`, `data` and `cycle_count`, again.**
   `demo->data` (the `Trecord *`) and `r->cycle_count` (the RLE run length)
   collide with the top-level globals of the same names, exactly as batch 10
   found for `draw_frame.c`.  Worked around the same way -- a guarded
   `#undef data` / `#undef cycle_count` at the top of
   `handle_player_input.c` -- and legitimately so: this file must never
   touch either global (the datafile belongs behind ASSETS.md's seam, the
   tick counter is `play()`'s pacing).  The real fix is still the
   context-sensitive rewrite `gen_bindings.py`'s own comment describes.
3. **`acquire_screen`/`release_screen`/`itofix`/`fixsin`** have no macro in
   `pf_lib_bindings.h` and no declaration in the generated `allegro_api.h`
   -- the same AL_INLINE gap batch 8 found for `rectfill`/`putpixel`, batch
   9 for `line` and batch 10 for `draw_sprite`/`rotate_sprite`/`fixtoi`/
   `ftofix`.  Handled the same way: `#ifndef`-guarded, upstream-faithful
   stand-ins inside `blit_to_screen.c`, so the source under test is
   byte-identical in every world and real `<allegro.h>` always wins where
   present.

### play(): the structure, from the line table

`artifacts/decodedline.txt` did not exist before this pass.  `objdump
--dwarf=decodedline assets/icytower15.exe` decodes the binary's `.debug_line`
section, and the 790 rows that belong to `play()` turn its 17420 bytes into
an exact source-line map -- the same kind of evidence batch 10 used to split
`draw_frame`, but far stronger, because it is per-instruction rather than
per-local.  The `play()` slice plus the region summary below is now
`artifacts/play_line_map.txt`; the byte counts are sums of address deltas and
add up to 17420 exactly.

```
region                                                      lines      bytes
setup / declarations / first-frame draw                  3405-3539       772
TICK: preamble, music start/stop, telemetry              3540-3680       862
TICK: simulation core (start_reward x10, handle_player_
      input, update_player, particles)                   3681-3800      1682
TICK: collision dispatch (5 variants)                    3801-3830       199
TICK: reward/combo/score/floor/jump accounting, death    3831-4035      1894
TICK: screenshot key + anti-cheat telemetry+update_frame 4036-4116       647
TICK: pause block A (focus lost)                         4117-4185       951
TICK: pause block B (user pause)                         4186-4248       837
TICK: misc keys / logging                                4249-4318       607
TICK: frame-skip + draw_frame + blit_to_screen + rest()  4319-4370       394
post-game key-sequence scan                              4371-4460       459
replay saving (.itr)                                     4461-4630      1841
game over / high-score entry / rank                      4631-4990      5928
teardown / return                                        4991-5021       188
inlined clear() helper (main.c 972-975, 3 sites)          972- 975       159
                                                                    --------
                                                                       17420
```

Loop head **0x411c30** (line 3540); bottom test 0x41250a-0x412512 (lines
3534-3536); back edge `je 411c30` at 0x41251a; return at 0x412520 (lines
5020-5021).  **The tick body is source lines 3540..4370 = 8073 bytes, 46% of
`play()`.**

Two corrections to `notes/binary_recon.md` item l fall straight out of this:

- Its "best safepoint, 0x4124f4" is line **4369** -- the `rest(2)` that ENDS
  a tick, not the start of one.  The tick begins at the loop head 0x411c30,
  and `handle_player_input` runs at 0x411f2a (line 3702), well before
  0x4124f4.  0x4124f4 is still a perfectly good safepoint; it is just the
  bottom of the loop, not the top.
- It describes one `draw_frame` then `blit_to_screen` "at call site
  0x413326".  There are four `draw_frame` sites and six `blit_to_screen`
  sites across `play()`.  The per-tick pair is 0x41320e (line 4338) and
  0x413326 (line 4353); 0x413287 (line 4349) is a second present inside the
  same frame-skip block, and the remaining sites belong to the two pause
  blocks and the game-over screen.

### What is still ORIGINAL in the tick body

Every game function the tick body calls, after this batch:

| callee | state |
|---|---|
| `update_frame`, `handle_player_input`, `poll_control`, `update_player`, `jump_player`, `play_jump_sound`, `is_any`, `is_pause`, `is_left`, `is_right`, `is_fire`, `add_floor`, `get_level`, `add_combo`, `add_jump_sequence`, `new_rand`, `create_particle`, `update_particle`, `start_reward`, all five `handle_player_collision_*`, `draw_frame`, `blit_to_screen` | **promoted** (batches 1-11) |
| `play_sound` | 0x406da4, 215 B -- ORIGINAL |
| `log2file` | 0x40da58, 189 B -- ORIGINAL |
| `take_screenshot` | 0x41002c, 203 B -- ORIGINAL |
| `startGameMusic` | 0x40cb30, 144 B -- ORIGINAL |
| `stopGameMusic` | 0x40caf4, 58 B -- ORIGINAL |

**Five functions, 809 bytes.**  Everything else the tick body calls is
Allegro (`rest`, `blit`, `textout_centre_ex`, `voice_get_position`,
`voice_stop`, `stop_sample`, `play_sample`, `clear_keybuf`, `keypressed`,
`allegro_message`, and inlined `vline`/`hline` vtable dispatch) or CRT/Win32
(`time`, `clock`, `QueryPerformanceCounter`/`Frequency`).

None of the five was promoted this pass, each for a stated reason rather
than for headroom:

- `play_sound` is the callee EIGHT existing SPECS entries trace through
  `CALLTRACE_PLAY_SOUND_VA`, and `pf_harness_calltrace.h` redirects the
  plain name to the stub for the whole harness build -- promoting it means
  that redirect would rename its own DEFINITION.  Untangling it changes a
  mechanism eight already-verified functions depend on, the same reason
  batch 10 gave for not bending the SPECS protocol.  It also writes `any11`
  (0x4dd170) from an x87 pan computation off `ply[player_id]->x`, so it is
  not the three-line wrapper its size suggests.
- `startGameMusic`/`stopGameMusic` need three new Allegro call-trace slots
  (`set_volume`, `play_midi`, `stop_midi`) on top of the two already
  present: new harness machinery for 202 cosmetic bytes.
- `log2file` writes through a `FILE *` behind a mutex
  (`sLogMutex__log2file`, already on `carrier/win32_policy.json`'s
  digest-domain exclude list) -- its only observable effect is outside any
  domain the offline harness can express, the same class as
  `destroy_game_data`'s `free()` (batch 3).
- `take_screenshot` writes a PNG to disk; same class.

### Why there is no `play_tick.c` yet

The brief allowed for recovering the tick body as its own file if the line
table showed clean regions.  It does -- the table above is exactly that --
but the tick body still cannot be a separately-verifiable file, for a reason
the line table itself makes plain:

- **The tick body is not a leaf of `play()`; it is half of one function that
  shares ~47 stack locals with the other half.**  DWARF gives `play()` 47
  `DW_OP_breg5` (ebp-relative) locals declared at lines 3408-3468 --
  `scroll_acc`, `next_speed`, `game_over`, `falling`, `step_count`,
  `tot_scroll`, `midX`/`midY`, `numComboJumps`, `totComboFloors`,
  `startTime`/`endTime`, `lastJumpLength`, `oldUnlockedFloors`,
  `current_rank_id`, the twelve anti-cheat timing accumulators, and so on.
  The tick body reads and writes most of them; the game-over half reads the
  results.  A `play_tick()` with its own address would need every one of
  them in an invented context struct -- state the original does not have, at
  addresses the carrier cannot bind.
- Batch 10's answer for `draw_frame` (a file of `static` helpers composed by
  one function bound at ONE address) does apply here, and is the right shape
  for batch 12 -- but only if the WHOLE of `play()` is recovered, because
  binding is all-or-nothing per address.  That means the other 9347 bytes
  too: the replay-save block (1841 B, seven `save_replay` sites), the
  high-score/rank block (5928 B, the largest single region in the function),
  the post-game key scan and the teardown.
- So the honest sequencing is: recover `play()` as ONE file, in one pass,
  with the tick body's 8073 bytes as `static` helpers -- not a `play_tick.c`
  that could never be bound.  This batch's contribution to that is the map
  above, the five remaining ORIGINAL callees named with their blockers, and
  the two seams (`handle_player_input`, `blit_to_screen`) that the tick body
  now reduces to a single call each.

### Harness changes (additive)

- `carrier/lift/harness/icytower_specs.py`: `poll_joystick` added to
  `LIB_CALL_TARGETS`; `CALLTRACE_POLL_JOYSTICK_VA` (0x7c1600) and the
  batch-11 scratch VAs (`REPLAY_VA` 0x7c8000 -- a FULL-SIZE `Treplay`, since
  the existing `DEMO_VA`/`SZ_DEMO` stub is 0x100 bytes and stops well short
  of `size`@0x8 and `data`@0x8a8; `RECORDS_VA` 0x7c9000; `PROFILE_VA`
  0x7ca000); `_joy0`/`_gamepad`/`_control` helpers; `gen_poll_control` +
  `gen_handle_player_input`; two `SPECS` rows; `SRC_BATCH11_FUNCS` added to
  the `src` and `gcc` default `--funcs` lists.
- `carrier/lift/harness/pf_harness_calltrace.h`, `call_trace_stubs.c`:
  `harness_trace_poll_joystick()` and its `#define`.
- `carrier/lift/harness/icytower_harness_project_gcc.c`: `#include
  "allegro_api.h"` (for `JOYSTICK_INFO joy[8]`), storage for
  `recording`/`rec_pos`/`rejump`/`profile`/`gamepad`/`joy`, and the two new
  dispatch branches.  `handle_player_input`'s branch needs a **second-level**
  pointer fixup no earlier function did: not just `demo` but `demo->data`,
  the Trecord array base stored INSIDE the guest `Treplay` -- without it the
  compiled side dereferences a raw guest VA the moment the encoder or
  decoder indexes a record.
- `carrier/lift/harness/build_src_gcc.sh`: file list extended with
  `control.c`, `poll_control.c`, `handle_player_input.c`; and the 32-bit GCC
  is now also looked for at `/c/msys64/mingw32/bin/gcc.exe`, because under
  Git Bash (which does not mount the msys64 tree at `/`) the existing
  `/mingw32/bin/gcc.exe` probe fails and the bare-`gcc` fallback picks the
  64-bit compiler, whose `-m32` link dies with "cannot find -lkernel32".
- `carrier/lift/harness/blit_to_screen_xcheck.py` (new, 386 lines) and
  `blit_to_screen_check.c` (new, 192 lines): the ordered-trace pair.
- `carrier/lift/harness/batch11_coverage.py` (new, 122 lines): the
  branch-coverage census.
- `carrier/win32_policy.json`: `param_renames` gained `"ctrl": "ctrl_arg"`.
- `src/icytower/`'s five generated files regenerated with the documented
  command; `src/icytower/control.c`'s `check_control_key` parameter renamed
  to match its already-renamed prototype.
- `artifacts/play_line_map.txt` (new).

**`carrier/gen/pf_bindings_src.h` intentionally NOT regenerated this pass**
-- same reasoning as batches 6-10 (another agent's concurrent carrier build
owns that file).  The carrier-world compile was verified against a scratch
copy built outside `carrier/gen/`, then discarded.
`src/build/Makefile.standalone` deliberately untouched (another agent owns
it); the three new files are compile-checked separately in all three worlds
below.

### Totals (updated)

| | batch 11 (this pass) | cumulative (11 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 3 | 50 |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 5 new (`play_sound`, `log2file`, `take_screenshot`, `startGameMusic`, `stopGameMusic`) | 12 distinct |
| original bytes recovered (offline-verified) | 2437 (728 + 1415 + 294) | 20444 |

`git diff --stat`-style file list this pass:
`src/icytower/handle_player_input.c` (new, 289 lines, 728 original bytes),
`src/icytower/blit_to_screen.c` (new, 255 lines, 1415 original bytes),
`src/icytower/poll_control.c` (new, 129 lines, 294 original bytes),
`src/icytower/control.c` (+13/-5: the `key_arg` rename and its note),
`src/icytower/{game_types.h,game_state.h,game_funcs.h,state.c,allegro_types.h,GENERATED.md}`
(regenerated), `carrier/win32_policy.json` (+1 policy value),
`carrier/lift/harness/{icytower_specs.py,pf_harness_calltrace.h,call_trace_stubs.c,icytower_harness_project_gcc.c,build_src_gcc.sh}`,
`carrier/lift/harness/{blit_to_screen_xcheck.py,blit_to_screen_check.c,batch11_coverage.py}` (new),
`artifacts/play_line_map.txt` (new), `artifacts/src_equivalence.json`.

## Purity gate (batch 11)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 41 file(s) under .../src, 0 violation(s)
```

(One violation was found and fixed on the way: Allegro's `fixsin` spells its
quarter-turn offset `0x400000`, which is 64.0 in 16.16 fixed point and also
the guest image base.  `blit_to_screen.c` writes it `(64 << 16)` -- the same
constant, unmistakably an angle.)

## Compile (all three worlds, batch 11)

```
standalone (generated allegro_api.h, no bindings):
  gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/{poll_control,handle_player_input,blit_to_screen,control}.c
  -- 0 errors, 0 warnings

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
      -Ithird_party/allegro-4.4.3.1/include \
      -Ithird_party/build-allegro-4.4.3.1/include \
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower \
      -c src/icytower/{poll_control,handle_player_input,blit_to_screen,control}.c
  -- 0 errors, 0 warnings (the real AL_INLINEs win over this file's
     #ifndef-guarded stand-ins, and real _cos_tbl/fixsin come from Allegro)

carrier (scratch bindings, GCC -- no MSVC cl.exe in this sandbox):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower
  python carrier/gen/gen_bindings.py --exclude <scanned>,floor_size_modifiers ^
      --guard-define ICYTOWER_BINDINGS_ACTIVE ^
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower \
      -include <SCRATCH>/pf_bindings_src.h \
      -include carrier/gen/pf_lib_bindings.h \
      -include carrier/gen/pf_asset_bindings.h \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c <every src/icytower/*.c>
  -- 26 of 27 clean (0 errors, no warnings beyond pf_asset_bindings.h's own
     -Wunused-function notices); the one failure is draw_star_field.c,
     batch 8's already-documented `stars` MEMBER_ACCESS_COLLISIONS gap.
     blit_to_screen.c COMPILES but will not LINK until `_cos_tbl` gains a
     binding (see "Three generator gaps").

offline oracles (GCC x87 only):
  bash carrier/lift/harness/build_src_gcc.sh gcc_check_x87_nosse_batch11.exe \
       -mfpmath=387 -mno-sse2 -O2
  python carrier/lift/harness/lift_check.py --form src --toolchain gcc \
      --exe <...>/gcc_check_x87_nosse_batch11.exe \
      --funcs poll_control,handle_player_input --vectors 20000 --seed <s>
  -- 4 seeds (20260908, 1, 777, 424242) x 20000 x 2 functions
     = 160000 vectors: EQUAL

  gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      carrier/lift/harness/blit_to_screen_check.c src/icytower/blit_to_screen.c \
      src/icytower/state.c -o carrier/lift/harness/blit_to_screen_check.exe
  python carrier/lift/harness/blit_to_screen_xcheck.py --random --seed <s> --vectors 1500
  python carrier/lift/harness/blit_to_screen_xcheck.py --directed --seed <s>
  -- 5 seeds x 1500 random + 2 x 529 directed = 8558 vectors: differ 0

  python carrier/lift/harness/batch11_coverage.py --vectors 2000 --seed 20260908
  -- every recovered branch of both SPECS functions reached
```

### In vivo (for the carrier task -- NOT run by this pass)

This pass did not run `carrier.exe` (another agent owns the carrier).  Bind
order matters, because one of the three has a stated blocker:

```
python carrier\gen\scan_src_defs.py --src-dir src\icytower
python carrier\gen\gen_bindings.py --exclude <scanned names>,floor_size_modifiers ^
    --guard-define ICYTOWER_BINDINGS_ACTIVE ^
    --out carrier\gen\pf_bindings_src.h --types-out carrier\gen\pf_bindings_src_types.h
carrier\build.cmd

rem 0. unbound baseline FIRST -- blit_to_screen is the digest sample point.
carrier.exe --replay replays\human_test.txt --frame-digest

rem 1. poll_control -- ready now, no blocker.
carrier.exe --bind poll_control=src --replay replays\human_test.txt --frame-digest

rem 2. handle_player_input -- ready; the `ctrl` -> `ctrl_arg` policy fix has
rem    landed in carrier\win32_policy.json and src\icytower\game_funcs.h, so
rem    the regenerated pf_bindings_src.h above is all it needs.
carrier.exe --bind handle_player_input=src --replay replays\human_test.txt --frame-digest
carrier.exe --bind handle_player_input=src,poll_control=src ^
            --replay replays\human_test.txt --frame-digest
carrier.exe --bind handle_player_input=src --replay <the .itr workload> --frame-digest

rem 3. blit_to_screen -- do NOT attempt until `_cos_tbl` (VA 0x4ce100,
rem    fixed[512], C:\Lib\allegro4\src\math.c) has a binding in
rem    carrier\gen\pf_lib_bindings.h; the file compiles but will not link.
carrier.exe --bind blit_to_screen=src --replay replays\human_test.txt --frame-digest
```

Every bound run must stay EQUAL to the unbound baseline for all 2293 ticks
and end on the same score 2386 / floor 100 witness (divergence 009's
regenerated baseline).

Three things the offline oracle structurally cannot see, all of the class
`notes/living_record.md` divergence 008 is about:

1. `handle_player_input` in RECORD mode writes into `demo->data`, a heap
   block `create_replay()` allocated with the GUEST's `malloc`.  The carrier
   must reach that same block, not a carrier-side copy; the offline harness
   pins it at a scratch VA and so proves nothing about which allocator's
   memory the real build touches.
2. `poll_control` reads Allegro's `key[]` (0x506988) and `joy[]` (0x506a88),
   both written by the GUEST's own keyboard/joystick driver threads.  A
   binding that reached a carrier-side copy would produce a silently
   input-less run -- score 0, floor 1, per-tick digests perfectly
   self-consistent and completely wrong.  The score-2386 witness is the
   check that actually catches that, not the digest.
3. `blit_to_screen` IS the frame-digest sample point, so binding it changes
   the instrument; take the unbound baseline first (step 0 above).

## Batch 12 (2026-09-08 -- `play()`, the whole game loop)

Task: recover `play()` (0x411a00, 17420 bytes, main.c 3405-5021) as clean
source, in one pass, as ONE file bound at ONE address -- the sequencing
batch 11 argued for and could not do without reading the other 9347 bytes.

**One function promoted, 17420 bytes, the largest single function in the
image.**  `src/icytower/play.c`, 1737 lines: the outer tick loop, the
collision dispatch, the score/combo/floor accounting, the two pause
screens, the replay transport, the frame-skip and pacing, the post-game
census, the .itr saving, the results/rank/initials screen and the
high-score commit.

| function | VA | size | CU | offline result | carrier bind |
|---|---|---:|---|---|---|
| `play` | 0x411a00 | 17420 | main.c | **PARTIAL: 3 regions EQUAL (3617 vectors), the rest IN-VIVO-PENDING** -- plus a call-site census that is EXACT over all 82 callees | ready; no new link blocker (see "In vivo") |

### Why the result is PARTIAL, and what stands in for the rest

`play()` does not return until the game is over.  It reaches ~45 game /
Allegro / CRT callees, shares ~47 ebp-relative locals between its tick half
and its game-over half, and its outer loop is paced by the 50 Hz
`cycle_count` interrupt and by `readkey()`/`keypressed()`.  There is no
offline comparison domain for the whole of it -- batch 11 said so before
the recovery started, and reading it in full confirms it.  Three things
stand in:

**1. Three regions really are offline-checkable**, because `play.c`
recovers them as self-contained `static` helpers AND the original emits
them as straight-line code with no game calls.
`carrier/lift/harness/play_xcheck.py` (new) enters each region under
unicorn with a synthetic frame -- guest seeded, ebp/esp pointed at a
scratch stack, the locals the region reads written at their ebp offsets --
and stops at the first instruction past it.  That is legitimate here
because each region is entered with the same state on every path in the
original: R1 loads `demo` itself, R2 starts at the `xor %ebx,%ebx` that
initialises its own induction variable, and R3 reads only globals plus the
zero the compiler parked at ebp-0x934.

| region | original | size | domain | result |
|---|---|---:|---|---|
| R1 `clear_replay_telemetry` | 0x411a61-0x411ab0 (3473-3480) | 80 B | memory: `tc_posts` + the five 100-float telemetry channels, 2004 bytes | **EQUAL**, 1206 vectors |
| R2 `draw_pause_curtain` | 0x412d55-0x412db6 (4122-4124) | 98 B | **ORDERED call trace** of 640 `GFX_VTABLE` calls (`vline` +0x28 / `hline` +0x2c, alternating), batch 9's synthetic-vtable-VA trick | **EQUAL**, 1201 vectors |
| R3 `collect_game_data` | 0x4137ab-0x4138ee (4389-4418) | 324 B | memory: `Tgame_data`'s score/floor/combo/no_combo_top_floor/biggest_lost_combo/ccc[5]/jc[5] + left/right/jump | **EQUAL**, 1210 vectors |

502 of 17420 bytes, 4 seeds x 300 random + 17 directed = 3617 vectors,
differ 0.  R2 is exactly the case `lift_check.py`'s shared call-trace
mechanism cannot express (count + first call only), so it is traced the way
batch 10's `draw_frame_xcheck.py` and batch 11's `blit_to_screen_xcheck.py`
trace theirs; `lift_check.py`/`icytower_specs.py` are untouched again.

`carrier/lift/harness/play_check.c` **`#include`s `src/icytower/play.c`
verbatim**, so the source under test is byte-identical to what the three
compile worlds build; `play_check_stubs.c` supplies inert definitions for
the ~45 callees a TU containing `play()` must link against but that R1/R2/R3
never reach (each aborts rather than returning a made-up value).

**2. A call-site census, and it is EXACT.**
`carrier/lift/harness/play_callsite_census.py` (new) counts every `call` in
0x411a00..0x415e0b -- direct by symbol, indirect by `GFX_VTABLE` slot
offset -- and the same names in the recovered source.  **82 distinct
callees, 0 mismatches.**  Eight rows differ by a count, and each is
asserted EXACTLY together with its structural reason, so drift breaks the
census rather than hiding in it:

```
clock                    6 -> 3    the repeated wall-clock rebase is
QueryPerformanceCounter  6 -> 3      factored into restart_time_cheat_window()
time                    15 -> 12     (called 4x)
voice_get_position       4 -> 2    the music rebase -> resync_music_counter() (3x)
hline / vline            2 -> 1    the two pause screens share draw_pause_curtain()
save_profile             3 -> 1    GCC tail-duplicated the syncProfileFromOptions()
                                     + save_profile() tail into 3 predecessors
textout_centre_ex       17 -> 14   GCC tail-duplicated 3 initials-entry arms
strcpy                   2 -> 8    6 of the 8 fixed-length copies are `rep movsb`
                                     in the original; the recovery spells all 8
                                     as strcpy
syncProfileFromOptions   0 -> 3    all three sites inlined; the out-of-line copy
                                     at 0x406a14 is never reached from play()
```

The census also runs the reverse direction (a name the SOURCE calls that
the original never does), so a spurious call cannot hide in the direction
the census is not driven from.

**This is what caught the one real recovery error of the pass.**  Lines
4831-4833 draw the summary scroller's shadow.  They were first recovered as
three `hline`s; the census reported `rectfill` 4 vs 3 and `hline` 2 vs 6,
and reading 0x414e13/0x414e5d/0x414ea7 again showed `call *0x3c` --
`rectfill(swap_screen, 0, scrollerY, 639, scrollerY + 20/18/16, black)`,
three stacked translucent BANDS whose height shrinks, not three lines.
Fixed, and the census is exact from both directions.

A second error was caught by re-reading the join points rather than by a
tool, and is recorded for the same reason: `if (!ply->in_combo)
gdComboStart = level;` (3936-3937) belongs to the `level >= ply->level`
arm ONLY.  When the player has fallen below its own recorded floor the
original goes straight from 0x412b66 to 0x412b70 (line 3962) and never
touches `gdComboStart`; the first draft had it after the if/else, where it
would have run on both.

**3. The in-vivo oracle is the authority for the rest** -- commands below.

### Negative controls

Fourteen deliberate source mutations, each rebuilt and re-run:

| mutation | result |
|---|---|
| R1: telemetry loop bound 100 -> 99 | DIFFER 200/200 |
| R1: `tc_posts` reset dropped | DIFFER 176/200 |
| R1: `tc_s_data` channel not cleared | DIFFER 200/200 |
| R2: curtain step 2 -> 1 | DIFFER 8/8 |
| R2: `vline` bottom 480 -> 479 | DIFFER 8/8 |
| R2: `hline` right 640 -> 639 | DIFFER 8/8 |
| R2: `vline`/`hline` order swapped | DIFFER 8/8 |
| R3: `score = level*10 + score` -> `- score` | DIFFER 200/200 |
| R3: `key_flag[0]` 0x10 -> 0x20 | DIFFER 92/200 |
| R3: rising-edge test `!last_keys[j]` dropped | DIFFER 102/200 |
| R3: `jump`/`left` counters swapped | DIFFER 80/200 |
| R3: `gd->jc` from `p->jc` instead of `p->jcTop` | DIFFER 200/200 |
| R3: census walks `demo->size + 1` records | DIFFER 104/200 -- **but only after the generator was strengthened.** With a zeroed record tail the mutant survived 200 vectors: the extra record reads `key_flags == 0`, which cannot change a count. The R3 generator now always seeds 320 record slots whatever `size` says, so the record at index `size` is live data. Same class as batch 11's `poll_control` button-bound 33. |
| R3: inner census loop `j < 7` -> `j < 6` | **EQUAL 0/200 -- an equivalent mutant in this domain, recorded not hidden.** Only `key_flag[0..2]` (jump/left/right) reach `Tgame_data`; slots 3..6 (up/down/enter and the 0x80 end-of-input terminator) are counted into `keys_pressed[]` and never read back out, so no observable state depends on the bound. It is verified from the low side only -- the original's own `cmp $0x7` at 0x4138a5/0x4138c0 is read off the disassembly. |

### Five findings in the original

1. **`clockSpeed`, the `c` channel of the anti-cheat telemetry, is
   algebraically CONSTANT.**  0x4144d8-0x4144e2 computes
   `1000.0 * clockElapsed / clockElapsed / 20.0`, i.e. 50.0 whenever
   `clockElapsed > 0`, and `-0.05` otherwise.  GCC could not fold it (it
   may not assume `x/x == 1` for floating point), so the `fmul`/`fdivp`
   pair really is in the object code -- confirmed by compiling eight
   candidate spellings with the project's own `-m32 -mfpmath=387 -mno-sse2
   -O2` and matching the instruction triple.  The other three channels
   (`q` from QPC, `t` from `time()`, `s` from the music position) do vary,
   and all four are compared against the same nominal 50 ticks/second.
   Recovered exactly; not "corrected".
2. **The "New personal records!    " scroller text is written and then
   immediately overwritten.**  0x415bf8 `rep movsb`s it into
   `summary_scroller_message`, and 0x415c0b unconditionally `strcpy()`s the
   random hint (or the guest-mode notice) over the top of it.  Lines
   4754-4769, which evidently meant to append the per-category record list,
   emitted NO code at all, and `skipCategories`/`h`/`achs` (declared
   4750-4752) have no DWARF location.
3. **`sprintf(profile->best_replay_names[i], "%s_best_X_%d.itr",
   profile->handle, n)` can overflow.**  `best_replay_names[i]` is
   `char[32]` and `handle` is `char[32]`, so the worst case runs well past
   32.  GCC `-Wformat-overflow` flags five of the seven sites; left
   uncorrected, the same rule `draw_frame.c`'s `scrollerText` overflow
   already set.  **These five are this pass's only compiler warnings.**
4. **The `debug` arm of the end-of-game test (4050) reads inverted next to
   the recording arm**: `if (ply->dead <= 99) playing = FALSE` versus
   `if (ply->dead > 100) playing = FALSE`.  `debug` has no store anywhere in
   the image (batch 9), so the arm is unreachable; recovered exactly as
   branched.
5. **Three of the five telemetry channels are stored as
   `speed + totXTimes`**, where `totClockTimes`/`totQPCTimes`/`totTimeTimes`
   are initialised to 0 at 3453/3459/3463 and never accumulated -- dead
   scaffolding from an earlier averaging design that GCC folded to a single
   `fldz`.

### Two corrections to notes/binary_recon.md item l

- Its **"catch-up" branch (0x4132e4-0x4132fa) is not load-adaptive.**  It is
  guarded by `if (debug)` and then by `key[KEY_TAB] && key[KEY_LSHIFT]`
  (0x4132a1/0x4132d2/0x4132db, main.c 4356-4363).  `debug` has no store
  anywhere in the image, so in vivo the branch is unreachable and the loop
  really is one-tick-in, one-frame-out; the only reachable pacing is
  `while (!cycle_count) rest(2);` at 4357.  It is a developer frame-step,
  not a catch-up.  Recorded in `notes/binary_recon.md` item l.
- Batch 11's two corrections stand and are now written into that document
  as well (0x4124f4 is the tick's END, and there are four `draw_frame` /
  six `blit_to_screen` sites).

### ASSETS.md: the last open computed-index rows are closed

`src/icytower/ASSETS.md` listed "the 2 `play` sites (0x4146e4, 0x4149e6)"
as the only computed `data[N]` sites still open.  Both resolve, and a third
site nobody had itemized turns up in the same region:

- **0x4146e4 / 0x4149e6 -- `data[gameover_bmp_id]` is not a range at all.**
  The local takes exactly TWO literal values, assigned at
  0x413f3c/0x413f4f and 0x414fe3/0x414ffa: 55 = `GAMEOVER`
  (`ASSET_DATA_GAMEOVER`) and 62 = `HIGHSCORE` (`ASSET_DATA_HIGHSCORE`).
  It is a local only because ONE `draw_results()` call site serves both the
  "game over" and the "new highscore" card.  `play.c` spells the two ids
  themselves, so no arithmetic on an `asset_id` happens here at all.
- **0x414a4a / 0x414fb3 -- `data[74 + new_rank_id]`, the rank medal**:
  the full 12-wide `ASSET_DATA_RANK_00`..`RANK_11` family (data 74-85),
  because `get_rank_id()` indexes the 12-entry
  `rankLables[]`/`rankFloors[]` tables and `assets_table.inc` generates
  RANK_00..RANK_11 contiguously.  Same argument `start_reward`'s
  `ASSET_DATA_REWARD_000 + tier` used in batch 7.

Every other datafile reference in `play()` is literal: `FONT_BIG_WHITE`
(50), `FONT_MED_WHITE` (52), `FONT_SMALL` (54), `HEROFACE000` (58),
`TITLE_BG` (126).

### The structure `play.c` recovers

`play()` stays one function, as batch 11 required, with six `static`
helpers for the regions that are genuinely self-contained -- a readability
decomposition, `static` so the compiler can inline them straight back:

```
clear_replay_telemetry()       3473-3480    80 B    R1, EQUAL
restart_time_cheat_window()    3654-3661 x4         factors 4 identical sites
resync_music_counter()         4168-4170 x3         factors 3 identical sites
draw_pause_curtain()           4122-4124    98 B    R2, EQUAL (shared by both
                                                      pause screens)
collect_game_data()            4389-4418   324 B    R3, EQUAL
save_personal_bests()          4500-4624  ~1840 B   IN-VIVO-PENDING
play()                         everything else
```

Everything else stays inline in `play()` because it reads and writes the
shared frame.  `syncProfileFromOptions()` is called by name: the line map
calls it "the inlined clear() helper (main.c 972-975)", but it is really
`syncProfileFromOptions` (DWARF `<0x1c973>`, decl_line 971,
`DW_AT_inline = 1` so the ABSTRACT DIE has no `DW_AT_low_pc`) -- and an
out-of-line copy does exist, at 0x406a14, whose fourteen instructions are
exactly the four assignments GCC inlined at the three sites.  The name in
the line map is the one correction this pass makes to it.

### Four generator gaps found

1. **Win32 / CRT imports have no generated declarations at all.**  The
   generated headers carry the game and Allegro scopes; `play()` also
   reaches `QueryPerformanceCounter`/`QueryPerformanceFrequency` (KERNEL32)
   and `mkdir`/`stricmp` (MSVCRT) through the import table, which
   `artifacts/imports.json` already knows about.  There is no
   `pf_import_decls.h`, and `LARGE_INTEGER` (a real DWARF type, `<0x1aecb>`,
   used by a real DWARF local) has no generated definition either.  Handled
   here with `#ifndef _WINDOWS_`-guarded declarations plus a `LARGE_INTEGER`
   stand-in spelled with its named `u` member, which is valid against real
   `<windows.h>` too.  **This is the new gap class of the pass**; the fix is
   a generator one (emit declarations for the import table the way
   `pf_lib_bindings.h` emits them for Allegro).
2. **`hline` / `vline` / `rectfill` / `acquire_screen` / `release_screen` /
   `SCREEN_W` / `SCREEN_H`** have no macro in `pf_lib_bindings.h` and no
   declaration in the generated `allegro_api.h` -- the same AL_INLINE gap
   batches 8/9/10/11 reported for `rectfill`/`putpixel`/`line`/
   `draw_sprite`/`acquire_screen`.  Handled the same way: `#ifndef`-guarded,
   upstream-faithful stand-ins, so the source under test is byte-identical
   in every world and real `<allegro.h>` always wins.  (`SCREEN_W`/`SCREEN_H`
   are worth naming separately: they are Allegro MACROS, not functions, and
   the original's own NULL-guarded loads at 0x415547/0x41574f are exactly
   their expansion.)
3. **`MEMBER_ACCESS_COLLISIONS`, `data` again and `rejump` new.**
   `demo->data` (the Trecord stream) and `demo->rejump` (the replay's saved
   jump-hold setting) collide with the top-level globals of those names.
   Worked around the same way batches 10/11 did -- a guarded `#undef data` /
   `#undef rejump` -- and legitimately so: this file wants neither global
   (datafile objects come through ASSETS.md's seam, and the jump-hold
   setting is read from `options.jump_hold`, which is where `play()` reads
   it).
4. **A `DW_AT_inline` abstract DIE is not proof there is no address.**
   `gen_src_headers.py` emits a prototype for `syncProfileFromOptions`
   correctly, but nothing in the generated output distinguishes "inlined
   everywhere, out-of-line copy exists at 0x406a14" from "inlined
   everywhere, no copy emitted".  A caller cannot tell whether the
   prototype will link.  Reported, not attempted: the generator has the
   answer (`functions.json` / the disassembly's own symbol) and could
   annotate it.

### Harness changes (additive)

- `carrier/lift/harness/play_xcheck.py` (new, 407 lines): the three-region
  oracle.
- `carrier/lift/harness/play_check.c` (new, 203 lines) and
  `play_check_stubs.c` (new, 109 lines): the compiled side.
- `carrier/lift/harness/play_callsite_census.py` (new, 189 lines): the
  completeness check.
- `src/icytower/play.c` (new, 1737 lines, 17420 original bytes).
- `src/icytower/ASSETS.md`, `notes/binary_recon.md`,
  `artifacts/src_equivalence.json` updated.

**`carrier/gen/pf_bindings_src.h` intentionally NOT regenerated this pass**
-- same reasoning as batches 6-11 (another agent's concurrent carrier build
owns it).  The carrier-world compile was verified against a scratch copy
built outside `carrier/gen/`, then discarded.
`src/build/Makefile.standalone` deliberately untouched.

## Purity gate (batch 12)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 42 file(s) under .../src, 0 violation(s)
```

## Compile (all three worlds, batch 12)

```
standalone (generated allegro_api.h, no bindings):
  gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/play.c
  -- 0 errors, 5 warnings (all -Wformat-overflow on the original's own
     best_replay_names[] sprintf sites; see finding 3)

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
      -Ithird_party/allegro-4.4.3.1/include \
      -Ithird_party/build-allegro-4.4.3.1/include \
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower \
      -c src/icytower/play.c
  -- 0 errors, the same 5 warnings (the real AL_INLINEs and SCREEN_W/H win
     over this file's #ifndef-guarded stand-ins)

carrier (scratch bindings, GCC -- no MSVC cl.exe in this sandbox):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower
  python carrier/gen/gen_bindings.py --exclude <scanned>,floor_size_modifiers ^
      --guard-define ICYTOWER_BINDINGS_ACTIVE ^
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower \
      -include <SCRATCH>/pf_bindings_src.h \
      -include carrier/gen/pf_lib_bindings.h \
      -include carrier/gen/pf_asset_bindings.h \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c <every src/icytower/*.c>
  -- play.c: 0 errors.  27 of the 28 carrier-world files clean; the one
     failure is still draw_star_field.c, batch 8's documented `stars`
     MEMBER_ACCESS_COLLISIONS gap.  (assets_standalone.c, state.c and
     game_types_check.c are standalone-world-only fixtures and are not part
     of a carrier build.)

offline oracle (GCC x87 only):
  gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      carrier/lift/harness/play_check.c carrier/lift/harness/play_check_stubs.c \
      src/icytower/state.c -o carrier/lift/harness/play_check.exe
  python carrier/lift/harness/play_xcheck.py --all --vectors 300 --seed <s>
  python carrier/lift/harness/play_xcheck.py --all --directed --seed 20260908
  -- 4 seeds (20260908, 1, 777, 424242) x 300 x 3 regions + 17 directed
     = 3617 vectors: differ 0

completeness census:
  python carrier/lift/harness/play_callsite_census.py [--verbose]
  -- 82 distinct callees, 0 mismatches
```

### In vivo (for the carrier task -- NOT run by this pass)

This pass did not run `carrier.exe`.  `play()` is the outermost game
function on this coastline: binding it replaces the entire game loop, and
the frame-digest sample point (`blit_to_screen`) sits inside it, so the
unbound baseline has to come first.  `play()` is called from `run_demo`
(0x415e96) and from 0x4162f4, both of which stay ORIGINAL and will enter
the recovered code.

```
python carrier\gen\scan_src_defs.py --src-dir src\icytower
python carrier\gen\gen_bindings.py --exclude <scanned names>,floor_size_modifiers ^
    --guard-define ICYTOWER_BINDINGS_ACTIVE ^
    --out carrier\gen\pf_bindings_src.h --types-out carrier\gen\pf_bindings_src_types.h
carrier\build.cmd

rem 0. unbound baseline FIRST -- blit_to_screen is the digest sample point
rem    and it is INSIDE play().
carrier.exe --replay replays\human_test.txt --frame-digest

rem 1. play alone.  No new link blocker: play.c needs no Allegro DATA
rem    global that pf_lib_bindings.h lacks (unlike blit_to_screen's
rem    _cos_tbl), and every callee it names is either already promoted or
rem    still ORIGINAL at its own address.
carrier.exe --bind play=src --replay replays\human_test.txt --frame-digest

rem 2. play plus batch 11's two seams, which it calls once per tick each.
carrier.exe --bind play=src,handle_player_input=src,poll_control=src ^
            --replay replays\human_test.txt --frame-digest

rem 3. the .itr workload -- the ONLY path that exercises the itrcheck arms
rem    (3441/3494/3502/3549/3574/3706/3977/4062/4249/4319/4369/4421/4426),
rem    which the human_test replay never takes.
carrier.exe --bind play=src --replay <the .itr workload> --frame-digest
```

Every bound run must stay EQUAL to the unbound baseline for all 2293 ticks
and end on the same score 2386 / floor 100 witness (divergence 009's
regenerated baseline).

Four things the offline oracle structurally cannot see:

1. **The tick's ORDER.**  R1/R2/R3 check three call-free regions; the tick
   body's ~40 calls per tick, in order, with their arguments, are only
   checked in vivo.  The per-tick digest is what does it.
2. **The pacing.**  `while (!cycle_count) rest(2);` depends on a timer
   interrupt no offline harness runs, and the frame-skip modulus
   (`someCounter % ffstep`) depends on a function-static that persists
   across ticks.
3. **The game-over half's UI loops** (results card, rank slide, initials
   entry, high-score commit) are driven by `readkey()`/`keypressed()` and
   by wall-clock time.  Marked IN-VIVO-PENDING in `play.c`'s own header.
4. **`save_personal_bests()` writes files** -- up to eleven `.itr` files
   plus a profile -- the same class as `log2file`'s `FILE *` and
   `take_screenshot`'s PNG.  Its effect is outside any domain the offline
   harness can express; the in-vivo run's `replays\` directory is the check.

### Totals (updated)

| | batch 12 (this pass) | cumulative (12 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 0 | 50 |
| functions promoted (partially offline-verified, in-vivo-pending) | 1 (`play`) | 1 |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 0 new | 12 distinct |
| original bytes recovered as clean source | 17420 | 37864 |
| original bytes offline-verified | 502 (R1+R2+R3) | 20946 |

`git diff --stat`-style file list this pass:
`src/icytower/play.c` (new, 1737 lines, 17420 original bytes),
`carrier/lift/harness/{play_xcheck.py,play_check.c,play_check_stubs.c,play_callsite_census.py}`
(new), `src/icytower/ASSETS.md` (+3 resolved rows / the open paragraph),
`notes/binary_recon.md` (item l, the catch-up correction),
`artifacts/src_equivalence.json` (the `play` entry).

## Batch 13 (2026-09-08 -- the last ORIGINAL callees inside the tick path)

Task: promote the functions still ORIGINAL on the gameplay tick path, so
that the whole tick becomes clean source.  The authoritative list is batch
11's "What is still ORIGINAL in the tick body" table (`play_sound`,
`log2file`, `take_screenshot`, `startGameMusic`, `stopGameMusic`) plus the
by-name callees of batch 12's `play()` that batch 12 itself left ORIGINAL
(`draw_reward`, `syncProfileFromOptions` @ 0x406a14, `destroy_game_data`,
`get_version_str`, and the deferred draw helpers `draw_table` / `drawSlot`
/ `draw_results` / `draw_progress_bar`).  The list was re-derived from
`play.c`'s own call sites and `play_callsite_census.py`, not taken on
trust -- see "Scope re-derivation" at the end of this batch.

Batch 11 and batch 3 each named a *blocker* rather than a difficulty for
these, and the blockers turn out to be properties of ONE harness build, not
of the functions:

- batch 11 on `play_sound`: "`pf_harness_calltrace.h` redirects the plain
  name to the stub for the whole harness build -- promoting it means that
  redirect would rename its own DEFINITION."  True of the SPECS-table
  build.  This batch does not touch that build; it adds a SECOND, disjoint
  executable that never links the SPECS harness, so the eight existing
  entries that trace a stubbed `play_sound` are unchanged and `play_sound`
  still gets a real definition and a real oracle.
- batch 3 on `destroy_game_data`: "no comparison domain the offline harness
  can express ... freeing a block leaves no game-owned bytes to diff."
  True of a MEMORY domain.  "Which pointer reached `free()`" is a
  call-trace fact, and the call-trace domain batch 7/9 built expresses it.
- batch 3 on `get_version_str`: "recovering it cleanly needs the actual
  string bytes extracted from the image, which this pass did not do."
  Done this pass with `pefile`.

### Part 1 -- the three small non-Allegro ones

| function | VA | size | CU | offline result | notes | carrier bind |
|---|---|---:|---|---|---|---|
| `get_version_str` | 0x406960 | 10 | main.c | **EQUAL** (content domain) | `mov $0x4d4b20,%eax; ret`.  The `.rdata` run at 0x4d4b20 read out of the image with `pefile` is `"1.5.1"` (NUL-terminated, immediately followed by an unrelated `allegro_message` format string -- so the extraction demonstrably did not run past the end).  The recovered form returns a fresh literal, whose address is necessarily different, so the domain is the returned BYTES: the oracle executes the ORIGINAL bytes under unicorn, reads the C string at the guest VA in EAX, and compares to what the compiled candidate returns.  Closes batch 3's deferral. | pending |
| `syncProfileFromOptions` | 0x406a14 | 58 | main.c | **EQUAL** (20000) | DWARF has only an ABSTRACT instance (`DW_AT_inline = 1`, decl_line 971, no `DW_AT_low_pc`) -- all three call sites inside `play()` are inlined, which is why batch 12's census reads "syncProfileFromOptions 0 -> 3".  The out-of-line copy of the same fourteen instructions has its own address at 0x406a14 and is what this row recovers.  Four stores, in the disassembly's own order (msc_volume, snd_volume, jump_hold, flash -- neither struct's declaration order); both structs' offsets confirmed by a compiled `offsetof()` probe on this project's `game_types.h`, not by eyeballing.  Domain is the FULL 0x550-byte `Tprofile`, so "nothing else is written" is proven too, not assumed. | pending |
| `destroy_game_data` | 0x40418c | 12 | game_data.c | **EQUAL** (20000, call trace) | `push %ebp; mov %esp,%ebp; sub $0x8,%esp; leave; jmp _free` -- a tail call, so `free()`'s own `ret` pops straight back into *this* function's caller.  Domain: mechanism B, hook `free()`'s IAT-thunk entry VA (0x4bad08), capture the one argument, never execute the thunk -- so no real heap state is touched on either side, exactly as `play_sound`'s existing trace hook never executes `play_sample`.  Closes batch 3's deferral. | pending |

Negative control, all three (`batch13_check.py --fault`), each reported and
each naming the exact difference:

```
get_version_str: original='1.5.1' candidate='1.5.0' -> DIFFER
  DIFFER at vector 5: profile+0x4dc (VA 0x7e04dc) original 0x4a candidate 0xb5
sync_profile: 1 of 200 vectors differ
  DIFFER at vector 5: free()'s arg -- original 0x3af6d46f (called 1) candidate 0x3af6d490 (called 1)
destroy_game_data: 1 of 200 vectors differ
```

Harness: `carrier/lift/harness/batch13_check.py` + `batch13_check.c` +
`build_batch13.sh` (new) -- a standalone additive oracle on the shared
`pf_win32_offline_oracle` engine, in the same convention as
`draw_frame_xcheck.py` / `play_xcheck.py`, **not** a new `lift_check.py`
SPECS row; `icytower_specs.py` is untouched again.

### Part 2 -- the tick path's last ORIGINAL callees

Six functions, 1390 original bytes, all verified by ONE new ordered
call-trace oracle (`carrier/lift/harness/batch13b_check.py` +
`batch13b_check.c` + `pf_harness_batch13.h`, built by `build_batch13.sh`).
The unicorn side hooks every callee at its own VA and appends one record
per call, IN ORDER, with its arguments; the compiled side stubs the same
callees and prints the same records; the two sequences are compared
verbatim.  Pointers are rendered as stable symbols and `const char *`
arguments as their CONTENT AT CALL TIME, so the guest and host address
spaces never have to agree -- `draw_frame_xcheck.py`'s convention, reused
rather than reinvented.

**20000 vectors per function, per seed, over four seeds (20260908, 1,
777, 424242) = 80000 each, 480000 total: differ 0.**

| function | VA | size | CU | offline result | domain | carrier bind |
|---|---|---:|---|---|---|---|
| `play_sound` | 0x406da4 | 215 | main.c | **EQUAL** (80000) | ordered trace of `new_rand` + `play_sample`, plus the memory global `any11` | pending |
| `startGameMusic` | 0x40cb30 | 144 | main.c | **EQUAL** (80000) | ordered trace of `play_sample` / `set_volume` / `play_midi`, plus `gameMusicVoiceID` | pending |
| `stopGameMusic` | 0x40caf4 | 58 | main.c | **EQUAL** (80000) | ordered trace of `voice_stop` / `stop_sample` / `stop_midi`, plus `gameMusicVoiceID` | pending |
| `log2file` | 0x40da58 | 189 | main.c | **EQUAL** (80000) | ordered trace of `pthread_mutex_lock` / `get_logfile_path` / `fopen` / `vfprintf` / `vsprintf` / `fputc` / `fclose` / `pthread_mutex_unlock`, plus the FORMATTED text in `last_log` | **blocked** -- see "The one carrier-side blocker" below |
| `take_screenshot` | 0x41002c | 203 | main.c | **EQUAL** (80000) | ordered trace of `sprintf` / `exists` / `log2file` / `get_palette` / `create_sub_bitmap` / `save_bitmap` / `destroy_bitmap`, plus `number__take_screenshot` | pending |
| `draw_reward` | 0x4070fc | 581 | main.c | **EQUAL** (80000) | ordered trace of `stretch_sprite` and of GFX_VTABLE's +0xa4 `pivot_scaled_sprite_flip` | pending |

Negative control, all six (`batch13b_check.py --fault`), each detected and
each named:

```
play_sound       1 of 40 vectors differ
  original:  play_sample sample 255 -422 1000 0 FAULT
  candidate: play_sample sample 255 -422 1000 0
startGameMusic   1 of 40   (play_sample bg_music 255 128 1000 1)
stopGameMusic    1 of 40   (voice_stop 0)
log2file         1 of 40   (mutex_lock sLogMutex)
take_screenshot  1 of 40   (sprintf "screenshots/icytower_%04d.png" -> ...)
draw_reward      1 of 40   (spurious record injected into an empty trace)
```

`draw_reward`'s control needed one extra provision worth recording: a
vector with `options.flash >= 2` produces NO records at all (the function
draws nothing and writes nothing), so corrupting "the first record" would
have been a silent no-op and the control would have passed vacuously.  It
injects a spurious record into the empty trace instead, which puts
"correctly did nothing" under test rather than letting it pass for free.
The first `--fault` run caught exactly this and reported `NEGATIVE CONTROL
FAILED: draw_reward fault not detected`.

### Two blockers named in earlier batches, and what they actually were

- **batch 11 on `play_sound`:** "`pf_harness_calltrace.h` redirects the
  plain name to the stub for the whole harness build -- promoting it means
  that redirect would rename its own DEFINITION."  Correct, and still
  correct -- of the SPECS-table build.  It is not a property of the
  function.  `batch13b_check.exe` never links `pf_harness_calltrace.h`, so
  the two builds are disjoint: the eight existing SPECS entries keep
  tracing a stubbed `play_sound` through `CALLTRACE_PLAY_SOUND_VA`
  unchanged (their domain is "which value reached play_sound's arguments",
  which is unaffected by play_sound acquiring a real definition in a
  different executable), and `play_sound` still gets a real definition and
  a real oracle.  `icytower_specs.py` is untouched again -- the fourth
  batch running that way.
- **batch 11 on `log2file`:** "its only observable effect is outside any
  domain the offline harness can express."  This one was **wrong**, and
  reading the disassembly is what shows it.  At 0x40daca `log2file` runs
  the same format string and the same `va_list` through `vsprintf` into
  the game global `last_log` (0x4f89e8, `char [256]`, named in
  `interop_index.json`) -- so the fully FORMATTED text of the most recent
  log line lands in an ordinary, diffable byte array.  The oracle compares
  it.  The correction is recorded here rather than silently fixed: the
  earlier claim was reasoned from the function's *purpose* (it writes a
  file) instead of from its instructions.

### The variadic problem, and why the printf subset is closed

`log2file` is variadic, so the unicorn side has to format too.  Rather
than assume a subset, this batch measured one: every `call 0x40da58` in
the image preceded by a literal format pointer -- **187 of the 188 call
sites** -- was resolved with `pefile` and its format string parsed.  The
entire game uses exactly **three** conversions: `%s`, `%d`, `%-12s`.
Python's own `%` operator is byte-identical to C's for all three, so
`batch13b_check.py`'s formatter is closed over what the binary can
actually produce, not merely over what the vectors happen to exercise.
The va_list itself needs no ABI guesswork: on x86 cdecl it is a plain
pointer into the caller's argument block, which is why the original can
hand the SAME `%edi` to `vfprintf` and then to `vsprintf` and get the same
text twice.

### Findings

**1. `-mno-sse` is not redundant next to `-mno-sse2`, and this batch is
where that stopped being theoretical.**  With `-mno-sse2` alone, GCC still
reaches for SSE1's `cvttss2si` to convert a **float** expression to `int`
-- and to use it, it must first spill the 80-bit x87 value to a 32-bit
float slot, adding a rounding step the original does not have.
`play_sound`'s `(int)(pan_x * 192.0f + 32.0f)` is exactly that shape.  Cost,
measured: **1 differing vector in 20000** (seed 20260908, vector 18904:
`pan` -563 against the original's -562).  With `-mno-sse` GCC emits the
original's own sequence instruction for instruction --
`fstps`/`flds`, `fmuls`, `fadds`, `fnstcw`/`fldcw`/`fistpl` -- and the
vector matches.  Earlier batches are **not** silently affected, and the
reason is structural rather than lucky: `cvttss2si` applies only to FLOAT
sources, and every float-to-int conversion promoted before this batch
(`line_intersect`, `draw_frame`, and `draw_reward`'s own casts) converts
from **double**, which needs SSE2's `cvttsd2si` and was already forbidden.
`play_sound` is the first promoted function with a float-domain
conversion.  The flag is now in `build_batch13.sh` with this reasoning
next to it.

**2. objdump's AT&T rendering of the two-operand x87 subtracts is reversed
relative to Intel's** -- and it produced the one real recovery bug of this
batch.  `fsubrp %st,%st(1)` at 0x407287 READS as `st(1) = st(0) - st(1)`
and COMPUTES `st(1) = st(1) - st(0)`.  The naive reading gave
`y = 360 - (int)(fixtof(scale/2)*h - fixtof(scale)*120)`; the oracle
DIFFERED on its very first run, at `draw_reward` vector 4 (flash 0,
scale 24878, w 1, h 32 -- the pivot call's `y` argument came out 26546912
against the original's 21435104, i.e. y = 399 where the original computes
321).  The correct form is
`y = 360 - (int)(fixtof(scale)*120 - fixtof(scale/2)*h)`, and it matches on
every vector of every seed.  Recorded at length in `draw_reward.c`'s own
header too, because the same trap is waiting in any other x87 function in
this image that uses the non-commutative `fsub`/`fsubr`/`fdiv`/`fdivr`
pairs -- and unlike a precision gap, it is a **whole-number** error that a
small vector count can easily miss.

**3. `take_screenshot`'s loop calls `exists()` before its own bounds
check**, on every iteration including the last.  GCC cannot have
introduced that call (it would put a call on a path the source did not
have one on), so the original really evaluates it first, and the "too
many screenshots" arm's observable sequence is `sprintf, exists, log2file`
-- not `sprintf, log2file`.  Two source shapes produce exactly this and
cannot be told apart from the object code; `screenshot.c` spells the one
that makes the ordering explicit and documents the other.

**4. `draw_reward`'s vtable call is `rotate_scaled_sprite`, not a
hand-rolled pivot call**, and the thing that pins it is an asymmetry that
looks like a misreading: the POSITION arguments carry scaled offsets
(`(w * scale) / 2`) while the PIVOT arguments carry unscaled ones
(`w << 15`).  That asymmetry is Allegro's own -- `draw.inl:374-377`,
verbatim -- and checking it against the real header in `third_party/` is
what turned "some 9-argument vtable call" into a named AL_INLINE.

### The one carrier-side blocker

`src/icytower/logfile.c` compiles cleanly in all three worlds but will not
LINK into the carrier yet: `pthread_mutex_lock` / `pthread_mutex_unlock`
are unresolved.  This is a generator gap, not a defect in `src/` -- the
carrier task has already recorded it independently in
`carrier/gen/build_blockers.json` ("it needs a guest-IAT binding for
pthreadGC2.dll that does not exist yet").  Stating it precisely so it can
be closed mechanically:

- `pf_lib_bindings.h` binds a library name to a VA inside the embedded
  Allegro/logg copy.  pthreadGC2 is a real imported DLL, so there is no
  such VA -- the original reaches both functions INDIRECTLY, through the
  IAT slots at **0x514a5c** (`pthread_mutex_lock`) and **0x514a60**
  (`pthread_mutex_unlock`), resolved from the import directory with
  `pefile`.
- The binding a generator would emit is therefore one indirection deeper
  than the Allegro ones: `((PFN)(*(void **)0x514a5c))`, reading the slot
  the loader has already filled in, rather than a direct VA.
- `sLogMutex__log2file` (0x4bdb44) itself is already a normal game global
  and already bound; only the two entry points are missing.
- Until that lands, `logfile.c`'s row above stays **blocked** for the
  carrier and **EQUAL** offline.  The remaining five part-2 functions have
  no such gap: every callee they name is either already bound or a plain
  CRT symbol the carrier's own CRT supplies.

### Scope: what "the gameplay tick path" actually contains

The batch brief's candidate list was re-derived rather than taken on
trust, from `play.c`'s own call sites and from
`play_callsite_census.py --verbose` (82 distinct callees, 0 mismatches).
Result, corrected in two places:

- `draw_reward` is **not** a `play()` callee at all.  It is `draw_frame`'s
  ONE call to another game-scope function (`draw_frame.c` line 717), so it
  reaches the tick through the frame renderer, once per frame.  In scope,
  and promoted.
- `draw_table` (0x404a7c, hisc.c), `drawSlot` (0x406fb4) and
  `draw_progress_bar` (0x407a08) are **not** on the tick path and are not
  `play()` callees either -- they belong to the menu and high-score
  screens.  Not attempted; not blockers for the tick.
- `draw_results` (0x4076c0, 839 B) **is** a `play()` callee (2 sites,
  `play.c` lines 1396 and 1504) but both sites are in the game-over half,
  after the tick loop has exited.  Not attempted this batch; it is the
  next function on the `play()` coastline, not on the tick one.

### What is still ORIGINAL in the tick body

Nothing.  Batch 11's table, re-run after this batch:

| callee | state |
|---|---|
| every function in batch 11's table (`update_frame`, `handle_player_input`, `poll_control`, `update_player`, `jump_player`, `play_jump_sound`, `is_any`, `is_pause`, `is_left`, `is_right`, `is_fire`, `add_floor`, `get_level`, `add_combo`, `add_jump_sequence`, `new_rand`, `create_particle`, `update_particle`, `start_reward`, all five `handle_player_collision_*`, `draw_frame`, `blit_to_screen`) | promoted (batches 1-11) |
| `play_sound`, `log2file`, `take_screenshot`, `startGameMusic`, `stopGameMusic` | **promoted (batch 13)** -- batch 11's "five functions, 809 bytes" |
| `draw_reward` (via `draw_frame`) | **promoted (batch 13)** |
| `play` itself | promoted (batch 12) |

Everything else the tick body reaches is Allegro (`rest`, `blit`,
`textout_centre_ex`, `voice_get_position`, `voice_stop`, `stop_sample`,
`play_sample`, `clear_keybuf`, `keypressed`, `allegro_message`, the
inlined `vline`/`hline`/`pivot_scaled_sprite_flip` vtable dispatch,
`exists`, `get_palette`, `create_sub_bitmap`, `save_bitmap`,
`destroy_bitmap`, `stretch_sprite`) or CRT/Win32/pthreads (`time`,
`clock`, `QueryPerformanceCounter`/`Frequency`, `sprintf`, `fopen`,
`vfprintf`, `vsprintf`, `fputc`, `fclose`, `pthread_mutex_lock`/`unlock`).

The `play()` coastline is a different and longer list: 19 of its 82
callees remain ORIGINAL, all of them in the game-over, replay-menu and
high-score halves -- `calc_replay_checksum`, `destroy_replay`,
`do_replay_menu`, `draw_results`, `enter_hisc_table`, `fadeIn`, `fadeOut`,
`file_exists`, `getGameDataXML`, `get_rank_id`, `init_scroller`,
`load_replay`, `myDeleteFile`, `my_alert`, `qualify_hisc_table`,
`save_config`, `save_profile`, `save_replay`, `sort_hisc_table`.

## Purity gate (batch 13)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 47 file(s) under .../src, 0 violation(s)
```

## Compile (all three worlds, batch 13)

The four new files plus the two batch-13 additions to existing files
(`main_state.c`, `destroy_game_data.c`): **0 errors and 0 warnings in all
three worlds.**

```
standalone (generated allegro_api.h, no bindings):
  gcc -m32 -mfpmath=387 -mno-sse -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/{sound,logfile,screenshot,draw_reward,destroy_game_data,main_state}.c
  -- 0 errors, 0 warnings

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
      -Ithird_party/allegro-4.4.3.1/include \
      -Ithird_party/build-allegro-4.4.3.1/include \
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower \
      -c <the same six>
  -- 0 errors, 0 warnings (draw_reward.c's fixtoi/fixtof/rotate_scaled_sprite
     stand-ins are inside the same #ifndef ICYTOWER_UPSTREAM_ALLEGRO block
     draw_frame.c already uses, so the real AL_INLINEs win here)

carrier (scratch bindings, GCC -- carrier/gen is owned by the concurrent
carrier task and is NOT touched; the generator writes to a scratch dir):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower
  python carrier/gen/gen_bindings.py --exclude <scanned + this batch's 6 names
      + the 4 new file-static AL_INLINE stand-ins>,floor_size_modifiers \
      --guard-define ICYTOWER_BINDINGS_ACTIVE \
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower \
      -include <SCRATCH>/pf_bindings_src.h \
      -include carrier/gen/pf_lib_bindings.h \
      -include carrier/gen/pf_asset_bindings.h \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c <every src/icytower/*.c>
  -- 1013-line scratch binding header; the six batch-13 files: 0 errors,
     0 warnings.  Across the whole directory the only ERROR is still
     draw_star_field.c, batch 8's documented `stars`
     MEMBER_ACCESS_COLLISIONS gap, and the only src-file warnings are
     batch 10/12's known -Wformat-overflow sites in draw_frame.c and
     play.c.  (assets_standalone.c, state.c and game_types_check.c are
     standalone-world-only fixtures and are not part of a carrier build.)

offline oracles:
  ./carrier/lift/harness/build_batch13.sh
  python carrier/lift/harness/batch13_check.py  --vectors 20000 --seed 20260908
  python carrier/lift/harness/batch13_check.py  --fault
  python carrier/lift/harness/batch13b_check.py --vectors 120000 --seed <s>
  python carrier/lift/harness/batch13b_check.py --fault
  -- part 1: get_version_str EQUAL (content), sync_profile 0/20000,
     destroy_game_data 0/20000; all three faults detected
  -- part 2: 6 functions x 20000 vectors x 4 seeds (20260908, 1, 777,
     424242) = 480000 vectors, differ 0; all six faults detected
```

## In vivo (for the carrier task -- NOT run by this pass)

This pass did not run `carrier.exe`.  The natural sequencing, given that
`play()` (batch 12) is itself not yet bound in vivo:

```
rem 0. unbound baseline FIRST (blit_to_screen is the digest sample point).
carrier.exe --replay replays\human_test.txt --frame-digest

rem 1. the tick's audio seam alone -- no new link blocker.
carrier.exe --bind play_sound=src,startGameMusic=src,stopGameMusic=src ^
            --replay replays\human_test.txt --frame-digest

rem 2. plus the frame renderer's last game-scope callee.
carrier.exe --bind draw_reward=src,draw_frame=src ^
            --replay replays\human_test.txt --frame-digest

rem 3. take_screenshot: the human_test replay never presses F1, so this
rem    one needs a workload that does -- otherwise the bind is a no-op and
rem    proves nothing (the standing "a bind that is never entered is not
rem    evidence" rule).

rem 4. log2file: BLOCKED until the pthreadGC2 IAT binding exists (above).

rem 5. the whole tick as source, once play() itself is bound.
carrier.exe --bind play=src,<every name above>=src ^
            --replay replays\human_test.txt --frame-digest
```

Every bound run must stay EQUAL to the unbound baseline for all 2293 ticks
and end on the same score 2386 / floor 100 witness (divergence 009's
regenerated baseline).

Three things the offline oracle structurally cannot see here:

1. **`take_screenshot` and `log2file` really touch the filesystem.**  The
   oracle proves which calls happen with which arguments; that a PNG and a
   log line actually appear is an in-vivo fact, and the `screenshots/` and
   `log.txt` on disk after a bound run are the check.
2. **`play_sound`'s `new_rand()` draw is a determinism coupling, not a
   sound effect.**  A skipped or extra draw desynchronises the replay,
   which the frame digest catches immediately and the offline trace only
   catches for the one invocation under test.
3. **The mutex is real.**  `sLogMutex__log2file` is already on
   `carrier/win32_policy.json`'s digest-domain exclude list; nothing
   offline exercises contention on it.

## Totals (updated)

| | batch 13 (this pass) | cumulative (13 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 9 | 59 |
| functions promoted (partially offline-verified, in-vivo-pending) | 0 | 1 (`play`) |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 4 new (`draw_results`, `draw_table`, `drawSlot`, `draw_progress_bar` -- all off the tick path) | 16 distinct |
| original bytes recovered as clean source | 1470 | 39334 |
| original bytes offline-verified | 1470 | 22416 |

Batch 13's nine: `get_version_str`, `syncProfileFromOptions`,
`destroy_game_data` (part 1, 80 bytes) and `play_sound`,
`startGameMusic`, `stopGameMusic`, `log2file`, `take_screenshot`,
`draw_reward` (part 2, 1390 bytes).

File list this pass:
`src/icytower/{sound,logfile,screenshot,draw_reward,destroy_game_data}.c`
(new), `src/icytower/main_state.c` (+`get_version_str`,
+`syncProfileFromOptions`),
`carrier/lift/harness/{batch13_check.py,batch13_check.c,batch13b_check.py,batch13b_check.c,pf_harness_batch13.h,build_batch13.sh}`
(new), `artifacts/src_equivalence.json` (9 entries).

---

## Batch 12 addendum -- play() verified in vivo; four recovery defects the census could not see (divergence 010, 2026-09-08)

Batch 12 shipped `src/icytower/play.c` with an honest caveat: 502 of
17420 bytes verified by region oracles, the remaining 16.9 KB checked
only by an exact CALL-SITE CENSUS (82 callees, 0 mismatches), and a
known-but-unexplained in-vivo divergence (score 662 / floor 50 instead of
the witness 2386 / 100). That divergence is now diagnosed and fixed, and
`play()` is EQUAL in vivo on all three workloads. Full account:
`carrier/NOTES.md` "Divergence 010" and `src/icytower/INVIVO.md`.

### The four defects, and why each survived batch 12's checks

| # | `play.c` | defect | what the census saw |
|---|---|---|---|
| 1 | line 916 (main.c 3864) | `(get_level(...) - 1) / 10` should be `/ 5` | the census compares WHICH callee is called with WHAT arguments. `get_level(&map, (int)y)` is the right callee with the right arguments in both forms; the error is in what the CALLER does with the RESULT, and a divisor is not a call at all. Invisible by construction. |
| 2 | line 593 (3498) | `play_sound(custom.bg_music, ...)` should be `custom.yo` | the census matched the callee (`play_sound`) and the argument COUNT, but the argument is a pointer loaded from a struct member - `custom.bg_music` and `custom.yo` are both `SAMPLE *` from the same global, 8 bytes apart. |
| 3 | lines 1122, 1177 (4130, 4199) | `custom.yo` should be `custom.wazup` | same shape as #2; both are on the ESC/pause screens, which no workload in the corpus enters, so no in-vivo check reached them either. Found by AUDIT, not by a run (see below). |
| 4 | line 598, 653 (3504, 3553) | `play_sample(bg_beat, 128, 1000, 1, TRUE)` should be `(bg_beat, 0, 128, 1000, TRUE)` | the four numeric arguments were shifted one place left (the leading `vol=0` dropped). Arity was right, so the census passed. |

### The check that did catch it, and the two new ones worth keeping

**Caught it:** the per-tick digest, once the tick safepoint was rebuilt so
that it survives its own subject being promoted (`carrier/NOTES.md`
"Divergence 010" section 2). With it, each iteration was: run
`play=original` vs `play=src` -> `compare_digests.py` names the first
differing tick -> `--snapshot-at-tick` on both sides -> `pf_inspect diff`
names the first differing GLOBAL by name -> that global's writer in
`play.c` -> the disassembly at that write. Three iterations, three
defects, then EQUAL.

**New check 1 -- GLOBAL-REFERENCE AUDIT (cheap, offline, catches #2/#3/#4
without a run).** For a recovered function, extract every absolute
address its disassembly references, resolve each to `(global, member)`
using `carrier/gen/interop_index.json` + `it_types_check.c` offsets, and
compare the MULTISET against the members the recovered `.c` actually
names. For `play()` the mismatch was immediate and unambiguous: the
original references `custom.wazup` twice and `custom.yo` once; the file
named `custom.yo` twice, `custom.bg_music` once, `custom.wazup` never.
The same sweep over ALL globals `play()` touches turned up nothing else
(the only "unnamed" hits were Allegro's `key[]`/`num_itr_files`
neighbourhood and `stars + 0x3000`, the particle loop's end sentinel).
This is a structural check the call-site census does not subsume, and it
costs one script run.

**New check 2 -- MAGIC-DIVIDE SHIFT AUDIT (catches #1).** GCC's signed
division by a constant is `imul $M` + `sar $k` + sign fixup, and ONE magic
constant serves several divisors: `0x66666667` is `/5` at `sar` 1 and
`/10` at `sar` 2. Reading the constant without the shift is a plausible,
silent, off-by-a-factor error. `play()` alone has two `0x66666667` sites
with DIFFERENT shifts (`0x412885` = `/5`, `0x412a0c` = `/10`), and the
second one also has its final `sub` operands reversed (`sub %edx,%ecx`),
i.e. it computes `-(x/10)`, which was a third recovery defect (`stars[p].sy`,
main.c 4031). Any future recovery should list every `imul $magic` in its
range and record magic + shift + `sub` operand order per site.

### Status change

`play()` is no longer "recovered, offline-partial, in-vivo-pending". It is
verified in vivo on `human_test.txt` (per-tick digest EQUAL 2293/2293,
frame oracle EQUAL 2752/2752 frames, and the run to the game's own exit
saves the witness `score=2386 floor=100`), on `newgame.txt` (digest and
frames EQUAL) and on the `.itr` workload (G5a/G5b EQUAL, 157 ticks). All
of G1-G5 pass. The offline picture is unchanged - the same 502 bytes have
region oracles - but the authority for the rest is now an in-vivo per-tick
digest over the whole 2293-tick game rather than a call-site census.

## Batch 14 (2026-09-08 -- play()'s coastline: the game-over, replay and profile halves)

Task: promote the functions still ORIGINAL on `play()`'s wider coastline
-- batch 13's closing paragraph named nineteen, "all of them in the
game-over, replay-menu and high-score halves".  **Thirteen of the
nineteen are promoted here**, plus three more that came with them
(`get_rank`, `hash`, `calc_replay_checksum_131`): sixteen functions,
5208 original bytes, verified by two new standalone oracles.

One correction to that list of nineteen, made by looking the names up in
`artifacts/functions.json` rather than assuming: **`file_exists` is not a
game function at all.**  It is Allegro's own
(`C:\Lib\allegro4\src\file.c`, 0x446150, 197 bytes), the same class as
`exists` / `delete_file` / `pack_fopen`, and belongs on the library side
of the ledger.  `save_profile` below calls it; nothing recovers it.  So
the nineteen were really eighteen game functions, and **five remain**.

### Part 1 -- the pure and memory-domain half

`carrier/lift/harness/batch14_check.py` + `batch14_check.c` (new): a
standalone oracle on the shared `pf_win32_offline_oracle` engine, driven
by a BINARY vector file (the payloads are raw structure images -- a
2220-byte `Treplay`, a 180-byte `Thisc[5]` -- which the '|'-separated
text protocol batch 13 used cannot carry).  **20000 vectors per function
per seed over four seeds (20260908, 1, 777, 424242) = 80000 each.**

| function | VA | size | CU | offline result | domain | carrier bind |
|---|---|---:|---|---|---|---|
| `qualify_hisc_table` | 0x404994 | 39 | hisc.c | **EQUAL** (80000) | return value + the whole 180-byte `Thisc posts[5]` | pending |
| `sort_hisc_table` | 0x4049bc | 147 | hisc.c | **EQUAL** (80000) | the whole 180-byte `Thisc posts[5]` | pending |
| `enter_hisc_table` | 0x405790 | 136 | hisc.c | **EQUAL** (80000) | " (its `strcpy` tail call emulated on the unicorn side) | pending |
| `get_rank_id` | 0x418a84 | 76 | profile.c | **EQUAL** (80000) | return value | pending |
| `get_rank` | 0x418ad0 | 82 | profile.c | **EQUAL** (80000) | the returned STRING's CONTENT (batch 13's `get_version_str` convention) | pending |
| `hash` | 0x41b9c8 | 71 | replay.c | **EQUAL** (80000) | return value | pending |
| `calc_replay_checksum_131` | 0x41ba10 | 177 | replay.c | **EQUAL** (80000) | return value | pending |
| `calc_replay_checksum` | 0x41bac4 | 676 | replay.c | **EQUAL** (80000) | return value | pending |

Negative control, all seven vector kinds (`batch14_check.py --fault`),
each detected and each named:

```
qualify_hisc_table         1 of 40   (ret 1FAULT vs ret 1)
sort_hisc_table            1 of 40   (posts 2020...969800FAULT)
enter_hisc_table           1 of 40   (posts 01ff7f...b70100FAULT)
get_rank                   1 of 40   (rank_id 0FAULT vs rank_id 0)
hash                       1 of 40   (ret 2137970045FAULT)
calc_replay_checksum_131   1 of 40   (ret -1452276412FAULT)
calc_replay_checksum       1 of 40   (ret 1805878246FAULT)
```

### Part 2 -- the ordered-call-trace half

`carrier/lift/harness/batch14b_check.py` + `batch14b_check.c` +
`pf_harness_batch14.h` (new, all built by `build_batch14.sh`).  The
unicorn side hooks every library, CRT and unpromoted game callee at its
own VA and appends one record per call, IN ORDER, with its arguments;
the compiled side stubs the same callees and prints the same records.
`pack_fwrite` / `fwrite` buffers are rendered as the HEX OF THE BYTES
they would write, because for `save_replay` and `save_profile` the
sequence of those buffers IS the file.

| function | VA | size | CU | offline result | domain | carrier bind |
|---|---|---:|---|---|---|---|
| `destroy_replay` | 0x41bd68 | 54 | replay.c | **EQUAL** (80000) | ordered trace of `free` | pending |
| `myDeleteFile` | 0x40cd28 | 63 | config.c | **EQUAL** (80000) | ordered trace of `sprintf` / `delete_file` | pending |
| `save_config` | 0x40e130 | 154 | config.c | **EQUAL** (80000) | ordered trace of `log2file` / `get_configfile_path` / `pack_fopen` / `save_options` / 15 x `save_hisc_table` / `pack_fclose` | pending |
| `init_scroller` | 0x41f278 | 200 | scroller.c | **EQUAL** (80000) | ordered trace of `text_height` / `text_length`, plus the whole `Tscroller` (its `lines[]` compared as OFFSETS into the text) and the text buffer it rewrites in place | pending |
| `fadeOut` | 0x40bf5c | 609 | fade.c | **EQUAL** (4800) | ordered trace of `create_bitmap` / `blit` / `draw_sprite` / `set_trans_blender` / `drawing_mode` / `makecol` / `rectfill` / `solid_mode` / `blit_to_screen` / `rest` / `destroy_bitmap` | pending |
| `fadeIn` | 0x40c1c0 | 424 | fade.c | **EQUAL** (4800) | " | pending |
| `save_replay` | 0x41dd78 | 1227 | replay.c | **EQUAL** (1600) | ordered trace of `sprintf` / `log2file` / `time` / `localtime` / `load_replay` / `pack_fopen` / ~525 x `pack_fwrite` WITH THE BYTES / `pack_fclose`, plus the return value and `Treplay.date` / `.size` / `.checksum` | pending |
| `save_profile` | 0x41a3b8 | 1073 | profile.c | **EQUAL** (20000) | ordered trace of `get_profile_dir_for_profile` / `file_exists` / `mkdir` / `sprintf` / `time` / `localtime` / `generate_profile_checksum` / `fopen` / `fwrite` WITH THE BYTES / `get_controls` / `save_control` / `fclose` / `fprintf` / the four `profile_data_page_*` / `fputs` / `fputc` / `free` / `log2file`, plus `Tprofile.checksum` and `.saveDate` | pending |

**Vector budget, stated rather than implied.**  These eight cost between
2 and ~550 traced calls per vector, so `--vectors N` is split by an
explicit WEIGHT table instead of evenly, and every result line prints the
count it actually ran.  Per seed: 20000 each for `destroy_replay`,
`myDeleteFile`, `save_config` and `init_scroller`; 5000 for
`save_profile`; 1200 each for the two fades; 400 for `save_replay`.
Four seeds.  Total across both parts: **911 200 vectors, differ 0.**

Negative control, all eight (`batch14b_check.py --fault`), each detected
and each named:

```
destroy_replay   1 of 20   (free records)
myDeleteFile     1 of 20   (sprintf "%s%s" -> "replays/x")
save_config      1 of 20   (log2file "  saving config and scores")
init_scroller    1 of 20   (text_height font)
fadeOut          1 of 20   (create_bitmap 0 1753)
fadeIn           1 of 20   (create_bitmap 640 600)
save_replay      1 of 20   (sprintf "%s%s" -> "a/")
save_profile     1 of 20   (get_profile_dir_for_profile 1024 "")
```

### Findings

**1. The `.itr` date field carries a hidden watermark, and it is inside
the checksum.**  `save_replay` copies the 31-byte literal
`"              ICYTOWERISGREAT "` into `Treplay.date` (0x41ddfe, a
`rep movsb`) and then immediately `sprintf`s over the same buffer
(0x41de2a).  That reads as a dead store and is not one: the sprintf
format is `"%2d %3s %4d"`, exactly 11 characters plus a NUL, so bytes
12..30 of the 32-byte field keep `"  ICYTOWERISGREAT "` -- and
`calc_replay_checksum` hashes all 32 bytes of `date`.  A `.itr` whose
date field was rebuilt by anything that does not know about the tag fails
the integrity check.  This is the one apparently-dead store in the batch
that had to be reproduced exactly, and `replay.c` says so at the site.

**2. `save_replay`'s on-disk order is NOT the struct order.**
`notes/replay_format.md` SS4 stopped its trace at the fourth field and
recorded the rest as INFERRED, "a near-verbatim `Treplay`
serialization".  Finishing the trace shows three places where that
inference is wrong, all unambiguous in the object code: `checksum`
(offset 0x4c) is written near the END, after `comment` (0xa8), not in
struct position; the five 100-float statistics columns are written
INTERLEAVED BY INDEX (`c q t s f`, `c q t s f`, ...) as 500 separate
4-byte writes, not as five arrays; and each `Trecord` is written REVERSED
and SHORT -- `cycle_count` (4 bytes) first, then `key_flags` (1 byte),
five bytes on disk against the struct's eight.  The `ccc[5]` and `jc[5]`
arrays are likewise ten separate 4-byte writes.  Any reader built from
the struct layout alone would desynchronise at the first record.

**3. `calc_replay_checksum` hashes only three of the five statistics
columns, and its accumulator is provably `unsigned`.**  `tc_s_data` and
`tc_f_data` -- 800 bytes of every replay's tail -- are written by
`save_replay` and never hashed.  The three that are hashed go through the
one x87 section in this batch, and its C shape is pinned by two details
that would otherwise look like noise: the 64-bit slot fed to `fildll`
has its HIGH dword explicitly zeroed (so the accumulator is widened as an
UNSIGNED 32-bit value, not sign-extended), and the `fistpll` +
take-low-dword pair is GCC's idiom for `(unsigned int)<floating
expression>`.  Together they force
`sum = (unsigned)(sum + col[i] * ((i + k) % m));` as three separate
statements -- fusing them, or making `sum` an `int`, moves the truncation
points and changes the result.

**4. The screen-size null guard in both fades is `SCREEN_W`/`SCREEN_H`,
not a hand-written check.**  Six times across `fadeIn`/`fadeOut` the
object code loads `gfx_driver` (0x4dda84), tests it, and substitutes 0
for the size.  That is Allegro's own
`#define SCREEN_W (gfx_driver ? gfx_driver->w : 0)` expanding in place --
identified by the shape repeating identically in pairs, and confirmed by
counting down `GFX_DRIVER` to find `w`/`h` at +0x6c/+0x70.  Recovering it
as a defensive `if (gfx_driver)` would have been observationally
identical and wrong about the source.

**5. `fadeOut`'s last draw goes to `screen`, not `swap_screen`.**
Everything inside its animation loop targets the game's own
`swap_screen` (0x4dd194) and reaches the display through
`blit_to_screen`; the final black `rectfill` after `destroy_bitmap`
targets Allegro's `screen` (0x4dda8c) directly (0x40c148 reloads it).
The asymmetry is in the object code, and it leaves the visible
framebuffer black while the game's own buffer still holds the last
blended frame.  `fadeIn` never touches `swap_screen` at all.

**6. `enter_hisc_table` does not insert -- it overwrites the last
"candidate" slot, and the search is subtler than it looks.**  A running
threshold starts at 10 000 000 (the score ceiling); an entry qualifies
only if it is strictly below the running threshold, and qualifying lowers
the threshold to that entry's own value; the LAST qualifier wins.  On a
descending-sorted table that selects the lowest score; on a partly-filled
one (trailing zeroes) it selects the first free slot.  Both behaviours
fall out of the same three lines, and `sort_hisc_table` -- a stable
insertion sort, descending, unsigned -- does the reordering afterwards.

**7. `save_profile` writes two files and `sprintf`s a buffer onto
itself.**  `sprintf(path, "%s%s.itp", path, p->handle)` (0x41a4a1, and
again for `_stats.txt` at 0x41a5c0) is undefined by the letter of
C99 7.19.6.6 and is what the original does; it is kept, and GCC's
`-Wrestrict` / `-Wformat-overflow` warnings on those two lines are the
recovered code correctly reproducing it, not a defect introduced here.
The stats file's own header literal says `ICY TOWER 1.4 PROFILE` in a
1.5.1 binary.

### Two carrier-world blockers closed, by request

`carrier/win32_policy.json`'s `scan_exclude` had grown two TEMPORARY
entries -- `profile.c` and `replay.c` -- with a note from the concurrent
carrier task asking batch 14 to fix both in `src/` itself and then remove
them.  Both are fixed:

- **`replay.c`** gains the `#ifdef <name>` / `#undef <name>` idiom
  `handle_player_input.c` and `draw_frame.c` already use, for the THREE
  `Treplay` / `Trecord` member names that are also bound top-level
  globals: `data` (vs `DATAFILE *data` @0x4dd23c), `rejump` (vs
  `int rejump` @0x4fdcd8) and `cycle_count` (vs
  `volatile int cycle_count` @0x506938).  Dropping all three for this
  translation unit is right on the merits, not just expedient: replay.c
  must never touch the datafile (that is ASSETS.md's seam), the live
  difficulty global (the replay carries its own copy -- that is the point
  of the field) or the tick counter (play()'s pacing).
- **`profile.c`** renames `get_rank` / `get_rank_id`'s parameter to
  `profile_arg`, matching `win32_policy.json`'s own `param_renames` and
  the prototypes `game_funcs.h` already carries.

Both files now compile clean in the carrier world (0 errors), so the two
`scan_exclude` entries can be removed.  This pass does not edit
`carrier/win32_policy.json` -- that file is the carrier task's.

### Generator gaps found (reported, not fixed)

1. **`MEMBER_ACCESS_COLLISIONS` again, two more names.**  The root cause
   is the one `gen_bindings.py`'s own comment already describes: a
   global's binding is a blunt textual `#define`, so any STRUCT MEMBER
   with the same name becomes a syntax error.  This batch adds `rejump`
   to the collision list (`data` and `cycle_count` were already known
   from `handle_player_input.c`), and the workaround is still a per-file
   `#undef`.  All of them would disappear under a context-sensitive
   rewrite.
2. **`scan_src_defs.py`'s file-level exclusion is invisible to
   `--exclude`.**  A file listed in `scan_exclude` contributes NO
   function names to `gen_bindings.py --exclude`, so every function it
   defines stays bound to its original address while `src/` also defines
   it -- a silent wrong-target hazard rather than a build error.  It is
   the right tool for `state.c` (which defines nothing) and a sharp edge
   for anything else; the carrier task's own note in `win32_policy.json`
   reasons about exactly this and chose it deliberately as a temporary
   measure.  Worth a warning in the scanner: "file X is scan-excluded but
   defines N functions".
3. **`game_funcs.h` types `save_control`'s second parameter as
   `it_orig_FILE *`** (the MSVC-shaped `FILE` reconstructed from DWARF),
   while `<stdio.h>`'s `fopen` returns the host `FILE *`.  In a build
   whose CRT is not that one the two are distinct types for the same
   pointer value, so `profile.c` carries an explicit cast at the single
   call site.  A generator that emitted the CRT handle types as an opaque
   `void *`, or that shipped the alias the harness gets from
   `pf_harness_msvc_types.h`, would remove the need.
4. **`build_batch13.sh`'s `-I.` no longer finds
   `pf_harness_msvc_types.h`.**  That header moved out of
   `carrier/lift/harness/` into `port_forge/tools/win32_oracle/` (it is
   in this working tree's deleted-file list), so any GCC harness build
   that still relies on `-I.` to reach it fails on
   `typedef unsigned __int64 uint64_t;`.  `build_batch14.sh` points at
   the new location explicitly and says why.

### One environment note worth recording

The MinGW32 GCC in this tree fails **silently** -- exit 1, no diagnostic
at all -- unless `C:\msys64\mingw32\bin` is on `PATH`, even when `gcc` is
invoked by absolute path: `cc1.exe` lives under `lib/gcc/...` and loads
its DLLs from `bin/`, so without it Windows refuses the image with
`STATUS_INVALID_IMAGE_FORMAT` (0xC000007B) and the driver reports
nothing at all.  `build_batch14.sh` sets it and carries the explanation.

### Target 2 (`init_game`) -- scoped, not recovered

`init_game` (0x40e7dc, 5788 bytes) was mapped but not attempted, and the
map is the deliverable: `artifacts/init_game_callmap.txt` (new) is its
complete ORDERED call sequence, produced mechanically from
`artifacts/disasm.txt` -- **243 direct call sites over 68 distinct
callees**, with a per-callee census.  What the map shows:

- The startup sequence really is "the call sequence IS its effect": 65 of
  the 243 calls are `log2file` and 15 are `draw_progress_bar`, i.e. a
  quarter of the function is its own progress narration, and a further 12
  `allegro_message` calls (each preceded by
  `set_gfx_mode(GFX_TEXT, 0, 0, 0)`) are its twelve distinct failure
  exits.  Only ONE `set_gfx_mode` of the fifteen sets a real mode
  (`GFX_AUTODETECT_WINDOWED`, 640x480, at 0x40ed43; a second at 0x40f522
  retries in mode 2).
- It reaches **at least twenty game functions that are still ORIGINAL** --
  `load_options`, `load_hisc_table`, `make_hisc_table`,
  `reset_hisc_table`, `reset_options`, `load_profile`, `create_profile`,
  `rebuild_profile_list`, `syncOptionsFromProfile`, `check_characters`,
  `get_profiles_dir`, `get_gamepad_value`, `pwd_garble_string`,
  `getSampleFromOggDatafile` (22 calls), `fldads_start`,
  `draw_progress_bar`, `install_timers`, `get_extension`, `get_filename`,
  `set_config_file` -- every one of which an ordered-trace oracle would
  have to stub.  That, not the branch structure, is the size of the job:
  it is a batch of its own, and it is the natural next one.
- Its asset half needs no new work: the three `packfile_password` /
  `load_datafile` pairs at 0x40ee0a / 0x40f1aa / 0x40f95b load
  `data/loading.dat` (password `"(c) Free Lunch Design"`),
  `data/data.dat` and `data/sfx15.dat` (both keyed from the obfuscated
  `init_string` @0x4bdb3c), which is exactly `notes/asset_census.md`
  SS5's table -- checked against the call map, nothing new found and
  nothing contradicted.

**Target 3 (the menu screens) was not reached.**
`main_menu_callback` (3741 B), `select_profile` (3070 B), `view_scores`
(2552 B) and `key_to_str` (2543 B) are untouched; no budget remained
after target 1.

### What is still ORIGINAL on play()'s coastline

Batch 13's list of nineteen, re-checked name by name:

| callee | state |
|---|---|
| `calc_replay_checksum`, `destroy_replay`, `enter_hisc_table`, `fadeIn`, `fadeOut`, `get_rank_id`, `init_scroller`, `myDeleteFile`, `qualify_hisc_table`, `save_config`, `save_profile`, `save_replay`, `sort_hisc_table` | **promoted (batch 14)** |
| `file_exists` | **not a game function** -- Allegro's own (0x446150, `C:\Lib\allegro4\src\file.c`); a correction to batch 13's list |
| `do_replay_menu` (0x410f98, 2661 B), `draw_results` (0x4076c0, 839 B), `getGameDataXML` (0x404254, 1855 B), `load_replay` (0x41cde8, 1136 B), `my_alert` (0x40cd68, 1770 B) | **still ORIGINAL** -- 8261 bytes, the whole remainder |

`load_replay` is `save_replay`'s exact inverse and is the obvious next
one now that the on-disk order above is known; `draw_results` is a
drawing function of the `draw_frame` family; `do_replay_menu` and
`my_alert` are interactive loops (`readkey`/`keypressed` plus a redraw),
the same shape the menu screens have.

### Purity gate (batch 14)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 52 file(s) under .../src, 0 violation(s)
```

### Compile (all three worlds, batch 14)

The six files this batch touches (`hisc.c`, `replay.c`, `profile.c`,
`config.c`, `fade.c`, `scroller.c`): **0 errors in all three worlds.**

```
standalone (generated allegro_api.h, no bindings):
  gcc -m32 -mfpmath=387 -mno-sse -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/{hisc,replay,profile,config,fade,scroller}.c
  -- 0 errors; warnings only on profile.c's two self-aliasing sprintf
     calls (finding 7 above -- the original's own behaviour)

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
      -Ithird_party/allegro-4.4.3.1/include \
      -Ithird_party/build-allegro-4.4.3.1/include \
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower \
      -c <the same six>
  -- 0 errors, same two warnings (fade.c's SCREEN_W/SCREEN_H/rectfill/
     draw_sprite stand-ins are inside the #ifndef ICYTOWER_UPSTREAM_ALLEGRO
     block draw_frame.c already established, so the real macros win here)

carrier (scratch bindings, GCC -- carrier/gen is owned by the concurrent
carrier task and is NOT touched; the generator writes to a scratch dir):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower
  python carrier/gen/gen_bindings.py \
      --exclude <scanned + hash,calc_replay_checksum,calc_replay_checksum_131,
                 destroy_replay,save_replay,get_rank_id,get_rank,save_profile,
                 floor_size_modifiers> \
      --guard-define ICYTOWER_BINDINGS_ACTIVE \
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower \
      -include <SCRATCH>/pf_bindings_src.h \
      -include carrier/gen/pf_lib_bindings.h \
      -include carrier/gen/pf_asset_bindings.h \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c <every src/icytower/*.c>
  -- the six batch-14 files: 0 errors.  Across the whole directory the
     only failures are still draw_star_field.c (batch 8's documented
     `stars` MEMBER_ACCESS_COLLISIONS gap) and the three
     standalone-world fixtures assets_standalone.c / state.c /
     game_types_check.c, which are not part of a carrier build -- the
     same set batch 13 reported, so no regression.
  (The eight new names had to be added to --exclude BY HAND: scan_src_defs.py
   skips profile.c and replay.c entirely, per gap 2 above.)

offline oracles:
  ./carrier/lift/harness/build_batch14.sh
  python carrier/lift/harness/batch14_check.py  --vectors 140000 --seed <s>
  python carrier/lift/harness/batch14_check.py  --fault
  python carrier/lift/harness/batch14b_check.py --vectors 160000 --seed <s>
  python carrier/lift/harness/batch14b_check.py --fault
  -- part 1: 7 kinds (8 functions) x 20000 x 4 seeds = 560000, differ 0
  -- part 2: 8 functions, weight-split, x 4 seeds    = 351200, differ 0
  -- all fifteen faults detected
```

`icytower_specs.py` is untouched again -- the fifth batch running that
way -- and so is `lift_check.py`; both new oracles are standalone
executables on the shared engine.

### In vivo (for the carrier task -- NOT run by this pass)

This pass did not run `carrier.exe`.  Nothing in this batch is on the
gameplay tick path, so none of it can be exercised by
`replays/human_test.txt` alone: every function here runs after the tick
loop exits, or in the menus.  The natural sequencing:

```
rem 0. unbound baseline first, as always.
carrier.exe --replay replays\human_test.txt --frame-digest

rem 1. the pure ones -- no new link blocker, no filesystem effect.
carrier.exe --bind qualify_hisc_table=src,sort_hisc_table=src, ^
                   enter_hisc_table=src,get_rank_id=src,get_rank=src, ^
                   hash=src,calc_replay_checksum=src, ^
                   calc_replay_checksum_131=src ^
            --replay replays\human_test.txt --frame-digest

rem 2. the fades and the scroller -- these DO run at game over, so the
rem    digest must still match for all 2293 ticks and the run must still
rem    end on the score 2386 / floor 100 witness.
carrier.exe --bind fadeIn=src,fadeOut=src,init_scroller=src ^
            --replay replays\human_test.txt --frame-digest

rem 3. the file writers.  A bind that is never entered is not evidence
rem    (the standing rule), and the human_test replay never saves a
rem    profile or a replay -- so these need a workload that does, plus a
rem    BYTE COMPARISON of the produced .itr / .itp / _stats.txt /
rem    tower.cfg against the same files from an unbound run.  That
rem    comparison is the in-vivo half the offline oracle structurally
rem    cannot do: it proves which calls happen with which bytes, not that
rem    the bytes reach the disk.
```

Three things the offline oracle structurally cannot see here:

1. **`save_replay` / `save_profile` / `save_config` really touch the
   filesystem.**  The oracle proves the call sequence and the bytes; that
   a `.itr`, a `.itp` and a `tower.cfg` actually appear, and compare
   equal, is an in-vivo fact.
2. **`pack_fopen`'s mode string is load-bearing.**  `save_config` uses
   `"wp"` (Allegro's PACKED write -- `tower.cfg` is LZSS compressed) and
   `save_replay` uses `"wb"`.  Offline both are just argument strings; in
   vivo the difference is a different file format on disk.
3. **The fades block on `cycle_count`.**  Their
   `while (cycle_count <= 0) rest(2);` spin is driven by the real 20 ms
   timer in vivo and by a scripted counter offline, so pacing is not
   under offline test at all.

### Totals (updated)

| | batch 14 (this pass) | cumulative (14 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 16 | 75 |
| functions promoted (partially offline-verified, in-vivo-pending) | 0 | 1 (`play`) |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 5 (`do_replay_menu`, `draw_results`, `getGameDataXML`, `load_replay`, `my_alert`) | 21 distinct |
| original bytes recovered as clean source | 5208 | 44542 |
| original bytes offline-verified | 5208 | 27624 |

Batch 14's sixteen: `qualify_hisc_table`, `sort_hisc_table`,
`enter_hisc_table` (hisc.c, 322 B); `get_rank_id`, `get_rank`,
`save_profile` (profile.c, 1231 B); `hash`, `calc_replay_checksum_131`,
`calc_replay_checksum`, `destroy_replay`, `save_replay` (replay.c,
2205 B); `save_config`, `myDeleteFile` (config.c, 217 B); `fadeOut`,
`fadeIn` (fade.c, 1033 B); `init_scroller` (scroller.c, 200 B).

File list this pass:
`src/icytower/{hisc,replay,profile,config,fade}.c` (new),
`src/icytower/scroller.c` (+`init_scroller`),
`carrier/lift/harness/{batch14_check.py,batch14_check.c,batch14b_check.py,batch14b_check.c,pf_harness_batch14.h,build_batch14.sh}`
(new), `artifacts/init_game_callmap.txt` (new),
`artifacts/src_equivalence.json` (16 entries).

## Batch 14 addendum -- all sixteen verified in vivo; the defect was in the carrier's CRT seam, not in the recovered source (divergence 011, 2026-09-08)

The in-vivo pass batch 14's own "In vivo (for the carrier task -- NOT run
by this pass)" section asked for, run end to end. Full accounts:
`src/icytower/INVIVO.md` "In-vivo pass -- batch 14, and divergence 011"
and `carrier/NOTES.md` "Divergence 011".

### Status change

| function | was | now |
|---|---|---|
| all sixteen (`qualify_hisc_table`, `sort_hisc_table`, `enter_hisc_table`, `get_rank_id`, `get_rank`, `save_profile`, `hash`, `calc_replay_checksum_131`, `calc_replay_checksum`, `destroy_replay`, `save_replay`, `save_config`, `myDeleteFile`, `fadeOut`, `fadeIn`, `init_scroller`) | offline-verified, `carrier bind: pending` | **offline-verified AND in-vivo EQUAL** |

**No `src/icytower/*.c` file was edited.** All sixteen are correct exactly
as this batch recovered them; the two failures found in vivo were both in
the carrier's own generator, one layer below src/.

### The three things the offline oracle structurally could not see, and what each turned out to be

Batch 14's own closing list named three. All three were real, and two of
them hid a defect:

1. **"`save_replay`/`save_profile`/`save_config` really touch the
   filesystem."** They do. `last_game.itr`, `MissingNO_best_jj2_4.itr`,
   `MissingNO.itp`, `MissingNO_stats.txt` and `tower.cfg` all appear and
   all compare equal to an unbound run's -- the `.itr` files byte for
   byte under `carrier/scripts/compare_itr.py` (new; it knows this
   batch's own on-disk write order and masks one column, below), the rest
   plain byte-identical. Getting there needed the heap fix: `free()` in
   `destroy_replay` and `save_profile` was the CARRIER's `free()`, while
   the blocks came from still-ORIGINAL guest code out of the carrier's
   deterministic arena -- `STATUS_HEAP_CORRUPTION`, twice, reproducibly,
   at the same instruction.

2. **"`pack_fopen`'s mode string is load-bearing"** (`"wp"` for
   `save_config`, `"wb"` for `save_replay`). Confirmed in vivo, indirectly
   but conclusively: `tower.cfg` written by a bound run is byte-identical
   to the original's, and it is LZSS-compressed, which only `"wp"`
   produces.

3. **"The fades block on `cycle_count`."** They do, and they are fine:
   `fadeIn`/`fadeOut` bound produce a per-tick digest EQUAL over all 2293
   ticks and a byte-identical frame-oracle stream. The concern that
   motivated the note -- that `rest()` might not reach the guest's own
   Allegro -- does not arise: Allegro entry points go through
   `pf_lib_bindings.h` to guest VAs, a different mechanism from the CRT
   seam that did break.

### One correction to a finding, and one addition

**Finding 3 gains an in-vivo consequence.** "`calc_replay_checksum`
hashes only three of the five statistics columns" is not just a curiosity
about the format: it is what makes a byte-level `.itr` oracle possible at
all on this host. `tc_s_data` is `play.c` 3641's music-sync channel,
`50.0 * accMusics / totMusics`, and `accMusics` accumulates
`voice_get_position()` on a live DirectSound voice. MEASURED: two
UNBOUND runs of the same recording differ in exactly six bytes of
`last_game.itr`, all inside `tc_s_data[0..1]`. Because that column is one
of the two the checksum skips, the stored checksum is still stable
(0x8180e34d across every run in this pass), and masking the column leaves
a genuine bit-exact oracle for everything else -- including the `date`
field with its `ICYTOWERISGREAT` watermark (finding 1) and all three
hashed columns.

**A new item for the file's own "what the offline oracle cannot see"
list, learned the expensive way.** An offline oracle links the recovered
function against the HARNESS's C library and compares behaviour; in vivo
the same function links against the CARRIER's, and the original ran
against the GUEST's. Wherever those differ in STATE rather than in
behaviour -- the heap, the stdio stream table, a clock the carrier pins
for determinism -- an offline EQUAL says nothing. `save_profile` is the
sharpest example: offline its `time`/`localtime` pair is stubbed and
compared as a trace record, and it passed 20000 vectors x 4 seeds; in
vivo, before the fix, it wrote the real host date into
`MissingNO_stats.txt` where the original wrote the deterministic one.
Nothing crashed and no digest moved. The carrier now fails the BUILD for
this class (`gen_bindings.py` parses `carrier/src/wrappers.cpp`'s wrap
table and refuses any wrapped import that src/ calls but does not bind),
so a future batch inherits the check rather than the lesson.

### Verdict summary

| workload | check | result |
|---|---|---|
| human_test to the game's own exit, each of the five source files' functions bound alone | per-tick digest / witness / written files | EQUAL 2293 / score 2386 floor 100 `Done...` / all byte-identical |
| human_test, all sixteen bound with every earlier row and `play` | same | EQUAL 2293 / 2386 / 100 / all byte-identical |
| human_test | frame oracle `every=1` | byte-identical, 2752 frames |
| newgame | digest / frames | EQUAL 876 / EQUAL 982 |
| `.itr` workload | digest / frames / written files | EQUAL 157 / 841 common ticks 0 mismatching / byte-identical |
| gates.ps1 | G1-G5b | all EQUAL |

## Batch 15 (2026-09-08 -- the rest of play()'s coastline, and the first menu function)

Task: finish the five functions batch 14's closing table left ORIGINAL on
`play()`'s coastline, and start target 3 (the menu screens).  **Four of
the five are promoted here** (`load_replay`, `my_alert`, `draw_results`,
`getGameDataXML`), plus `create_replay`, which comes with `load_replay`
the way `hash` came with `calc_replay_checksum`, plus `key_to_str` from
the menu list: **six functions, 8397 original bytes**, verified by two
new standalone oracles.  `do_replay_menu` (2661 B) is the one that
remains, and the reason is stated below rather than glossed.

### The .itr format is closed, against real files

`notes/replay_format.md` SS4 left two literals explicitly open: "Copies a
6-byte magic from `REPLAY_HEADER` (**VA `0x4d7dd0`**) ... Exact magic
bytes not extracted -- would need a raw hex dump at VA 0x4d7dd0", and a
"7-byte tag from VA `0x4d7a7e`".  Both are now read out of the image with
pefile:

| VA | bytes | what |
|---|---|---|
| `0x4d7dd0` | **`"ITR140"`** | the `.itr` magic -- SIX bytes, NO terminator.  The file-format version is 1.4.0 even in a 1.5.1 binary, the same way `save_profile`'s stats header still says `ICY TOWER 1.4`. |
| `0x4d7a7e` | **`"Harold"`** | the default player name a fresh `Treplay` carries (7 bytes with its NUL), i.e. the game's default character. |

With those, batch 14's on-disk write order and batch 14's
`calc_replay_checksum` were run against **all thirteen real `.itr` files**
in `assets/profiles/MissingNO/replays` (read-only).  Every one parses
with **zero trailing bytes** and every one's **stored checksum is
reproduced exactly**:

```
MissingNO_best_cc1_84.itr          stored 57651884     computed 57651884     OK
MissingNO_best_combo_59.itr        stored 1830374282   computed 1830374282   OK
MissingNO_best_jj2_4.itr           stored -2122259635  computed -2122259635  OK
MissingNO_best_lost_combo_38.itr   stored 57191562     computed 57191562     OK
MissingNO_best_score_5059.itr      stored 57651884     computed 57651884     OK
last_game.itr                      stored -2122259635  computed -2122259635  OK
   ... 13 of 13, differ 0
```

That is a different KIND of evidence from the offline oracles below: it
checks the recovered format against files the real game wrote on a real
machine, not against the same bytes emulated.  It also confirms batch
14's finding 1 from the outside -- every one of those thirteen `date`
fields carries `"  ICYTOWERISGREAT "` in bytes 12..29.

### Part 1 -- the pure one

`carrier/lift/harness/batch15_check.py` + `batch15_check.c` (new).

| function | VA | size | CU | offline result | domain | carrier bind |
|---|---|---:|---|---|---|---|
| `key_to_str` | 0x416a9c | 2543 | menu.c | **EQUAL** (80000) | the destination string's CONTENT and length, plus a 64-byte `0xA5` canary on EACH side of it | pending |

`key_to_str` is the only function in this batch that is pure in the
project's strongest sense: a `call` census over its whole
0x416a9c..0x41748a range finds **zero call sites**, so the ORIGINAL side
runs with NO unicorn hooks at all.  The oracle does not sample: every run
enumerates **all of [-256, 512]** first -- 769 vectors, covering all 108
branches, the nineteen scancodes inside `KEY_MAX` that have no branch of
their own, both signs and both boundaries -- and spends the rest of
`--vectors` on random 32-bit ints to prove the default arm is really the
default.  20000 x 4 seeds = **80000**.

The canary is the point of the extra work.  A 108-branch dispatch that
writes string literals is exactly the shape where the wrong branch still
produces plausible output and an off-by-one literal length is invisible
unless the bytes AFTER the NUL are checked too.

**How the 108-entry table was produced, stated plainly:** not by reading
108 blocks by eye.  A throwaway decoder walked the compare chain, decoded
each target block into the bytes it writes (three shapes: immediate
`movb`/`movw`/`movl` stores, a `rep movsb` from `.rdata`, and GCC's
alignment-aware `movsb`/`movsw`/`rep movsl` idiom) and read the `.rdata`
sources out of the image with pefile.  `src/icytower/menu_keys.c` is that
decoder's output, reviewed against the disassembly; the decoder is
scaffolding and is not shipped, because the table it produced IS the
evidence and the oracle is what checks it.

### Part 2 -- the ordered-call-trace half

`carrier/lift/harness/batch15b_check.py` + `batch15b_check.c` +
`pf_harness_batch15.h` (new, all built by `build_batch15.sh`).

| function | VA | size | CU | offline result | domain | carrier bind |
|---|---|---:|---|---|---|---|
| `create_replay` | 0x41cce8 | 254 | replay.c | **EQUAL** (80000) | ordered trace of both `malloc`s and `free`, plus the WHOLE 0x8a8-byte `Treplay` head and the `Trecord` array | pending |
| `load_replay` | 0x41cde8 | 1136 | replay.c | **EQUAL** (1600) | ordered trace of `pack_fopen` x2 / ~534 x `pack_fread` WITH THE BYTES DELIVERED / `pack_fclose` x2 / `create_replay` / `calc_replay_checksum` / `log2file` / `destroy_replay`, plus the whole filled `Treplay` | pending |
| `getGameDataXML` | 0x404254 | 1855 | game_data.c | **EQUAL** (8000) | ordered trace of `malloc` and all 14 `sprintf` calls with their formats AND their outputs, plus the RETURNED BUFFER byte for byte | pending |
| `draw_results` | 0x4076c0 | 839 | main.c | **EQUAL** (48000) | ordered trace of `draw_sprite`/`draw_256_sprite` (vtable) / `textprintf_ex` / `textprintf_right_ex` / `stricmp` / `makecol` | pending |
| `my_alert` | 0x40cd68 | 1770 | main.c | **EQUAL** (16000) | ordered trace of 22 distinct callees including the whole input loop, plus the return value and `gui_fg_color`/`gui_bg_color` | pending |

**Vector budget, stated rather than implied.**  One `load_replay` vector
is ~540 traced calls against 3-5 for `create_replay`, so `--vectors N` is
split by an explicit WEIGHT table and every result line prints the count
it actually ran.  Per seed: 20000 `create_replay`, 12000 `draw_results`,
4000 `my_alert`, 2000 `getGameDataXML`, 400 `load_replay`.  Four seeds
(20260908, 1, 777, 424242).  Total across both parts: **233 600 vectors,
differ 0.**

Three mechanisms are new in this harness and each earns its place:

- **The heap is a bump allocator over an arena PRE-FILLED with `0xA5`,
  identically on both sides.**  `create_replay` provably leaves part of
  the block it returns uninitialised (finding 1 below), and a known fill
  is what turns "uninitialised" from an incomparable fact into a visible
  one: the domain can cover the WHOLE structure instead of stopping short
  of it, and the two sides agree byte for byte on the bytes nothing
  wrote.
- **The control layer is stubbed from per-vector ANSWER QUEUES**, even
  though `poll_control`/`is_left`/`is_right`/`is_fire`/`is_enter`/`is_any`
  are all already promoted.  Running them for real would make `my_alert`'s
  oracle a test of the keyboard and joystick globals rather than of
  `my_alert`'s decision logic, and would make its branch coverage depend
  on synthesising `Tcontrol` bit patterns.  All six have their own vector
  oracles from batches 3 and 4.
- **`key[]`, `closeButtonClicked` and `cycle_count` are driven by a
  TIMELINE keyed on the `rest()` count** -- the mechanism batch 14 used
  for the fades, generalised: the vector says "ESC goes down at rest 7 and
  up at rest 10, the close button at rest 40", and both sides run the
  identical little state machine inside their `rest` stub.  Without it the
  ESC branch is unreachable (a constant non-zero `key[KEY_ESC]` makes the
  pre-dialog drain loop spin forever on both sides).

Negative control, all six (`--fault`), each detected and each named:

```
key_to_str       1 of 769  (str "undefined")
create_replay    1 of 20   (malloc 2220 -> NULL)
load_replay      1 of 20   (pack_fopen "replays/x.itr" "rb" -> packfile)
getGameDataXML   1 of 20   (malloc 128000 -> heap0)
draw_results     1 of 20   (draw_sprite target logo 249 -20)
my_alert         1 of 20   (text_length obj51 "a b c d e f g h i j k l m n o p")
```

### Branch coverage, measured rather than argued

Both check scripts grew a `--coverage` flag: one `UC_HOOK_CODE` over the
function's range on the ORIGINAL side, a set of executed addresses, and
afterwards the fraction of the instruction addresses
`artifacts/disasm.txt` lists for that range that the campaign entered.
(objdump WRAPS a long encoding onto a second line that carries an address
but no mnemonic; counting those as instructions understates coverage by
exactly the number of wrapped encodings, which is a lot in this code.
Only lines with a mnemonic count.)

| function | executed / listed | residue |
|---|---:|---|
| `key_to_str` | 559 / 597 | 38, all alignment padding |
| `create_replay` | 74 / 75 | 1, `xchg %ax,%ax` |
| `load_replay` | **258 / 258** | none |
| `getGameDataXML` | 477 / 478 | 1, `nop` |
| `draw_results` | 214 / 219 | 5, all padding |
| `my_alert` | 366 / 376 | 10, all padding |

Every residual address was looked up in `artifacts/disasm.txt` and is
`nop`, `xchg %ax,%ax` or `lea 0x0(%esi),%esi`.  **Real-instruction
coverage is 100% for all six.**

The measurement did real work rather than confirming a hope:
`getGameDataXML`'s `"match"` arm at 0x404746 was NEVER entered by the
random vectors, because the verdict compares fifteen fields and fifteen
independent random comparisons mismatch with probability ~1.  A directed
sub-generator (25% of vectors copy the replay's fifteen comparands into
the game data) fixed it, and the coverage line is what said so.

### Findings

**1. `create_replay` clears `name` twice and never clears `date`, and the
checksum reads the difference.**  The 32-byte clearing loop is emitted
TWICE (0x41cd48 and 0x41cd58) and BOTH write `0xc(%ebx,%eax,1)` -- that
is `name`, at +0x0c, both times.  `date` (+0x2c) is never cleared; it only
ever receives the 8 bytes of `"no date"`.  So `date[8..31]` of a
freshly-created replay is whatever `malloc` handed back, and
`calc_replay_checksum` hashes ALL 32 bytes of `date`.  `save_replay` then
rewrites `date[0..30]` from its 31-byte watermark literal, leaving
`date[31]` as the one byte of the checksum's input that nothing in the
program ever defines.  (In all thirteen real `.itr` files that byte is 0,
so the arena happens to be zeroed in practice; nothing guarantees it.)
The recovered file writes the two loops the object code contains --
collapsing them would hide the bug while being observationally identical.

**2. A verified replay comes back with its `checksum` field left at 0.**
`load_replay` saves the field (0x41d214), zeroes it (0x41d217) so the
recomputation excludes it, and on the MATCH path jumps STRAIGHT to the
return without restoring it (0x41d228 -> 0x41ce0f).  Every replay this
function hands out therefore has `checksum == 0`, and `save_replay`'s
later `r->checksum = calc_replay_checksum(r)` is what makes it right
again.  Restoring it here would be tidier and wrong.

**3. `load_replay` checks no `pack_fread` return value, and has three
different failure shapes.**  A truncated file is not detected by the
reader at all; it is detected by the checksum, which is the only
integrity gate the function has.  The three failures are genuinely
different in the object code: `create_replay` returning NULL returns NULL
with no log line (0x41ce78); `pack_fopen` failing on the SECOND open
jumps INTO the mismatch tail (0x41ce91 -> 0x41d249), so it destroys the
replay but skips the log; only a real checksum mismatch logs
`"Checksum failed for %s: got %d, expected %d"`.

**4. `-tiny` is an EITHER/OR, not a filter, and the full `-check` output
never says whether the replay verified.**  0x4046e5 loads `cmdline.tiny`
and branches; the false arm (0x4047aa) appends every section and then
jumps STRAIGHT to the closing tag, and the true arm (0x4046f3) runs the
field comparison and emits `<result>match|mismatch</result>`.  Neither
arm can reach the other's work.  So the verdict element exists ONLY in
`-tiny` output.  That is surprising enough to be worth stating plainly;
it is unambiguous in the object code and the oracle covers both arms.

**5. `getGameDataXML`'s `<actual_results>` rows are gated on the CLAIMED
values.**  Both loops that build that block test `gd->replay->ccc[i]` /
`->jc[i]` (0x4043f0, 0x40442a -- the REPLAY's array) and then print
`gd->ccc[i]` / `gd->jc[i]` (0x4043f8, 0x404434 -- the MEASURED one).  A
level the player actually reached but the replay header does not claim is
therefore INVISIBLE in the XML, while its mismatch still counts toward the
verdict.  Reading the two loops as "the same loop over the actual data"
would be the natural mistake; the two different base registers are the
evidence.

**6. `draw_results` suppresses the personal-best badge for the guest
profile BY NAME.**  0x40781a compares `profile->handle` (the `Tprofile`
field at +6) with the literal `"guest"` at 0x4d4b86 using `stricmp` -- a
case-insensitive compare, so `"Guest"` and `"GUEST"` are suppressed too.
A profile literally called `guest` never gets a PB icon no matter what it
scores.  That is a name check, not a flag check, and nothing else in the
function looks at the profile.  The same function draws THREE categories
and not in category order: a five-int local cleared by `rep stos` with
only `[1] = 2` and `[2] = 1` written afterwards, walked three times, i.e.
`category_names[0]`, `[2]`, `[1]` -- Score, Floor, Best Combo.

**7. `my_alert` calls `text_length` a third time and throws the answer
away.**  0x40cd97 and 0x40cdba measure `func` and `txt`; 0x40cdef
measures whichever of the two was LONGER -- and `%eax` is overwritten by
the next `makecol` without ever being read.  A dead width computation,
and it has to be reproduced exactly: `text_length` is not a pure function
to the compiler, the call is in the object code, and an
ordered-call-trace oracle sees it.  Two more from the same function: a
NULL argument becomes the string `" "` for `text_length` ONLY (the later
`textprintf_centre_ex` at 0x40cfbe gets the raw `func`, NULL and all,
which both msvcrt and MinGW's printf render `"(null)"`), and the two
drain loops are NOT the same -- the one before the dialog waits on
`is_any` x2 plus `key[KEY_ESC]`, the one after adds `key[KEY_ENTER]`,
which is what stops the Enter that dismissed the dialog from immediately
activating whatever is behind it.

**8. `my_alert` borrows the game's back buffer as its undo, one pixel
short.**  0x40cf25 blits `screen` -> `swap_screen` before anything is
drawn and 0x40d350 blits `swap_screen` -> `screen` on the way out: the
double buffer is used as a saved rectangle.  Both blits are 639 x 479,
not 640 x 480, in the original, on both calls.  Kept.  The dialog itself
is drawn on `screen` between an `acquire_bitmap`/`release_bitmap` pair
(the GFX_VTABLE +0x10 / +0x14 AL_INLINEs), and the button sprites are
selected by an UNSIGNED comparison -- `cmp $1,%esi; sbb %eax,%eax` is
`sel == 0`, because `sel` only ever holds 0 or -1 and -1 is a huge
unsigned.  Recovering that as a signed `sel < 1` would be
observationally identical on those two values and wrong about the object
code.

**9. `key_to_str` is an if/else chain, not a `switch`, and the proof is
one out-of-order case.**  The object code tests the 108 values in SOURCE
order, and that order is not sorted: `KEY_SEMICOLON` (105) is tested
between `KEY_COLON` (68) and `KEY_QUOTE` (69), where the author grouped
the punctuation keys by keyboard position.  GCC expands a `switch` by
value (jump table or sorted binary search) and would have destroyed that
order.  Three of the table's entries reach the player's screen as quirks:
`KEY_R` renders the LOWERCASE `"r"` (0x416f28 stores 0x0072 -- the single
exception among twenty-six letters), `KEY_BACKSLASH` and
`KEY_BACKSLASH2` both render `\` (two physical keys the options screen
cannot tell apart), and `KEY_TILDE` renders the literal word `"TILDE"` in
capitals while every other punctuation key renders its glyph.

### The recovery audit, run on all six

`carrier/scripts/recovery_audit.py` (the delegator to port_forge's
`pf_win32_recovery_audit.py`, added by the concurrent record task in
`bf9570e` and already reporting 75/75) was run on this batch's six
BEFORE they were declared done, exactly as PROMOTIONS.md's batch-12
addendum asked:

```
python carrier/scripts/recovery_audit.py --function key_to_str create_replay        load_replay getGameDataXML draw_results my_alert
6/6 function(s) pass both audits.

python carrier/scripts/recovery_audit.py --src-dir src/icytower
84/84 function(s) pass both audits.
```

**No magic-divide sites in any of the six** -- none of them divides by a
constant at all, which the tool states rather than leaves implicit.

The global-reference audit initially FAILED two of them, and both
failures were real information about how the tool resolves addresses
rather than about the recovery.  Both are now entries in
`carrier/recovery_audit_policy.json` with their own derived evidence, as
that file's `_purpose` requires:

| function | mismatch | why it is a naming artifact, not a defect |
|---|---|---|
| `draw_results` | `profile.handle` in_source=1, in_original=0 | the ORIGINAL reaches the member THROUGH THE POINTER -- 0x40781a loads the `Tprofile *` global and 0x40781f does `add $0x6,%eax` -- so no absolute address for `handle` exists for the scan to match.  The pointer itself matches on both sides. |
| `my_alert` | `menu_params.ctrl` in_source=11 vs `menu_params.use_joy` in_original=11 | ONE address, two spellings.  `menu_params.ctrl` is at menu_params+8 == 0x4f8e40, and `Tcontrol`'s first member `use_joy` is at offsetof 0, so the same address is both.  The tool flattens the path and drops the intermediate struct name; the counts agreeing exactly (11 and 11) is what says they are the same eleven sites. |

Everything else resolved and matched: `REPLAY_HEADER` (the 4+2 split of
the six-byte magic memcpy), all five `cmdline` members, `category_names`,
`new_personal_best`, `profile`, `closeButtonClicked`, `cycle_count`,
`gfx_driver`, `gui_fg_color`, `gui_bg_color`, `key[59]` (= `KEY_ESC`),
`key[67]` (= `KEY_ENTER`), `screen`, `swap_screen`.  The tool's
"referenced on only one side at all" INFO lines name `data` (which clean
code must NOT name -- datafile objects go through ASSETS.md's
`asset_bitmap()`/`asset_font()` seam) and `rejump` (the `#undef`ed
collision, see gap 2), both expected.

### Generator gaps found

1. **`state.c` zero-fills a `.rodata` constant, and that is a SILENT data
   bug, not a build error.**  `const char REPLAY_HEADER[6] = {0}` is what
   the generator emits for every global, and its comment says why ("Every
   global is zero-initialized, matching how the OS loader zero-fills the
   original .bss").  But `REPLAY_HEADER` is not `.bss`: it is `.rodata`
   with a real value, `"ITR140"`.  A standalone build of `replay.c`
   therefore writes `.itr` files with a zero magic, which
   `load_replay` then refuses -- and nothing warns.  `gen_src_headers.py`
   already reads DWARF; a global whose DWARF location is in a read-only
   initialised section should carry its initialiser, or at minimum a
   `#error`-worthy comment.  Worked around only inside this batch's
   harness (`pf_harness_batch15.h` renames the symbol so the oracle can
   supply the real bytes).
2. **`MEMBER_ACCESS_COLLISIONS` reaches a case a `#undef` cannot fix.**
   `rejump` is one more file (`game_data.c`, for `Treplay.rejump`), and
   that one takes the established idiom.  `ctrl` does not: `my_alert`
   uses BOTH the global `ctrl` (@0x5000c8) and the member
   `menu_params.ctrl`, so dropping the binding for the file would leave
   the global with a declaration and no storage.  `alert.c` captures the
   global's address into a file-static `player_ctrl` while the macro is
   still live and only THEN `#undef`s it -- two textually identical arms,
   one per world.  It works, it is documented at the site, and it is the
   sharpest argument yet for the context-sensitive rewrite
   `gen_bindings.py`'s own comment describes.
3. **`scan_src_defs.py`'s file-level exclusion is still invisible to
   `--exclude`** (batch 14's gap 2, unchanged and now larger):
   `carrier/win32_policy.json`'s `scan_exclude` currently lists
   `alert.c`, `game_data.c`, `menu_keys.c` and `results.c` in addition to
   `state.c`, so four files' function names are missing from the
   generated `--exclude` list while `src/` defines them.  **All four now
   compile clean in the carrier world (0 errors), so those four entries
   can be removed.**  This pass does not edit `win32_policy.json` -- that
   file is the carrier task's.  The scanner should warn: "file X is
   scan-excluded but defines N functions".
4. **`floor_size_modifiers` still has to be added to `--exclude` by
   hand** for `map.c` to compile in the carrier world, exactly as batch
   14 reported; nothing changed.

### What is still ORIGINAL on play()'s coastline

| callee | state |
|---|---|
| `create_replay`, `load_replay`, `getGameDataXML`, `draw_results`, `my_alert` | **promoted (batch 15)** |
| `do_replay_menu` (0x410f98, 2661 B) | **still ORIGINAL** -- the whole remainder of the coastline |

`do_replay_menu` was scoped and not attempted, and the reason is its
callee list rather than its size: it reaches **six functions that are
still ORIGINAL themselves** -- `drawSlot` (x9), `replaceBadCharacters`
(x3), `get_string` (x3), `handle_menu`, `run_demo`, `replace_extension`
-- plus `exists`, `stretch_sprite` and the five `my_alert` calls this
batch just promoted.  An ordered-trace oracle would have to stub all six
with their DWARF prototypes, which is most of another batch's work, and
three of them (`get_string`, `handle_menu`, `run_demo`) are themselves
interactive loops.  It is the natural head of the next batch together
with those six.

### Target 2 (`init_game`) -- not attempted

Unchanged from batch 14: `artifacts/init_game_callmap.txt` is its
complete ordered call map (243 sites, 68 callees), and the ~20 still-ORIGINAL
game functions it reaches (`load_options`, `load_hisc_table`,
`make_hisc_table`, `reset_hisc_table`, `reset_options`, `load_profile`,
`create_profile`, `rebuild_profile_list`, `syncOptionsFromProfile`,
`check_characters`, `get_profiles_dir`, `get_gamepad_value`,
`pwd_garble_string`, `getSampleFromOggDatafile`, `fldads_start`,
`draw_progress_bar`, `install_timers`, `get_extension`, `get_filename`,
`set_config_file`) are still the size of the job.  No budget remained
after targets 1 and 3's first function.

### Target 3 (the menu screens) -- one of five

`key_to_str` (2543 B) is done.  `view_scores` (0x404c38, 2552 B),
`select_profile` (0x41acc0, 3070 B), `main_menu_callback` (0x4100f8,
3741 B), `replay_selector` (0x41d258, 2845 B) and
`draw_replay_selector` (0x41be58, 3726 B) are untouched.  `view_scores`
is the next cheapest (19 call sites, two of them the still-ORIGINAL
`draw_table` and `checkMenuFocus`).

### Purity gate (batch 15)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 56 file(s) under .../src, 0 violation(s)
```

### Compile (all three worlds, batch 15)

The five files this batch touches (`replay.c`, `menu_keys.c`,
`game_data.c`, `results.c`, `alert.c`): **0 errors in all three worlds.**

```
standalone (generated allegro_api.h, no bindings):
  gcc -m32 -mfpmath=387 -mno-sse -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/{replay,menu_keys,game_data,results,alert}.c
  -- 0 errors; warnings only on game_data.c's self-aliasing sprintf calls
     (finding 4's item 6 -- the original's own behaviour)

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
      -Ithird_party/allegro-4.4.3.1/include \
      -Ithird_party/build-allegro-4.4.3.1/include \
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower \
      -c <the same five>
  -- 0 errors, the same warnings (results.c's and alert.c's SCREEN_W/
     SCREEN_H/rectfill/draw_sprite/acquire_bitmap/release_bitmap stand-ins
     are inside the #ifndef ICYTOWER_UPSTREAM_ALLEGRO block draw_frame.c
     established, so the real macros win here)

carrier (scratch bindings, GCC -- carrier/gen is owned by the concurrent
carrier task and is NOT touched; the generator writes to a scratch dir):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower
  python carrier/gen/gen_bindings.py \
      --exclude <scanned + key_to_str,getGameDataXML,draw_results,my_alert,
                 floor_size_modifiers> \
      --guard-define ICYTOWER_BINDINGS_ACTIVE \
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower \
      -include <SCRATCH>/pf_bindings_src.h \
      -include carrier/gen/pf_lib_bindings.h \
      -include carrier/gen/pf_asset_bindings.h \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c <every src/icytower/*.c>
  -- the five batch-15 files: 0 errors.  Across the whole directory the
     only failures are still draw_star_field.c (batch 8's documented
     `stars` MEMBER_ACCESS_COLLISIONS gap) and the three standalone-world
     fixtures assets_standalone.c / state.c / game_types_check.c, which
     are not part of a carrier build -- the same set batches 13 and 14
     reported, so no regression.
  (The four new names had to be added to --exclude BY HAND: scan_src_defs.py
   skips alert.c / game_data.c / menu_keys.c / results.c entirely, per gap 3.)

offline oracles:
  ./carrier/lift/harness/build_batch15.sh
  python carrier/lift/harness/batch15_check.py  --vectors 20000  --seed <s>
  python carrier/lift/harness/batch15_check.py  --fault
  python carrier/lift/harness/batch15_check.py  --coverage
  python carrier/lift/harness/batch15b_check.py --vectors 100000 --seed <s>
  python carrier/lift/harness/batch15b_check.py --fault
  python carrier/lift/harness/batch15b_check.py --coverage
  -- part 1: 20000 x 4 seeds                        =  80000, differ 0
  -- part 2: 5 functions, weight-split, x 4 seeds   = 153600, differ 0
  -- all six faults detected; real-instruction coverage 100% on all six

recovery audit:
  python carrier/scripts/recovery_audit.py --batch batch15
  -- 0 magic-divide sites; 3 global residues, all explained above
```

`icytower_specs.py` is untouched again -- the sixth batch running that
way -- and so is `lift_check.py`; both new oracles are standalone
executables on the shared engine.

### In vivo (for the carrier task -- NOT run by this pass)

This pass did not run `carrier.exe`.  Nothing in this batch is on the
gameplay tick path, so none of it can be exercised by
`replays/human_test.txt` alone.  The natural sequencing:

```
rem 0. unbound baseline first, as always.
carrier.exe --replay replays\human_test.txt --frame-digest

rem 1. key_to_str -- pure, no link blocker, no side effect.  It is only
rem    entered from the options screen, so a digest run is a
rem    NOT-ENTERED bind (the standing rule: a bind that is never entered
rem    is not evidence) and needs a menu workload.
carrier.exe --bind key_to_str=src --replay replays\human_test.txt --frame-digest

rem 2. create_replay + load_replay.  The .itr workload DOES enter both.
carrier.exe --bind create_replay=src,load_replay=src ^
            --replay replays\play_itr.txt --frame-digest
rem    and then the strongest check this batch admits: a BYTE COMPARISON
rem    of a replay round-tripped through the bound reader against the
rem    same file read unbound, plus `icytower15.exe -check` agreeing.

rem 3. draw_results -- runs on the game-over screen, so human_test to the
rem    game's own exit reaches it; the frame oracle is the check.
carrier.exe --bind draw_results=src --replay replays\human_test.txt --frame-digest

rem 4. my_alert + getGameDataXML need workloads nothing in the corpus has:
rem    a dialog (quit_via_menu.txt comes closest) and a `-check` run.
```

Four things the offline oracle structurally cannot see here:

1. **`load_replay` really reads a file.**  The oracle proves the
   `pack_fread` sequence and the bytes it consumed; that a real `.itr` on
   disk produces the same `Treplay` is an in-vivo fact.  The thirteen
   real files above narrow the gap but do not close it: they were parsed
   by a PYTHON reimplementation of the read order, not by the compiled
   `load_replay`.
2. **`getGameDataXML` allocates 128000 bytes and never frees them.**
   Offline the arena absorbs it; in vivo it is a real 128 KB leak per
   `-check` invocation, which is harmless for a one-shot command-line
   mode and would not be for anything else.
3. **`my_alert`'s loop is driven by the real 20 ms timer.**  Its
   `while (!cycle_count) rest(2)` spin is scripted offline, so pacing --
   and therefore how many frames the dialog holds -- is not under offline
   test at all.
4. **`draw_results` and `my_alert` draw.**  The oracle proves which
   drawing calls happen with which arguments; that the resulting pixels
   match is what the frame oracle is for.

### Totals (updated)

| | batch 15 (this pass) | cumulative (15 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 6 | 81 |
| functions promoted (partially offline-verified, in-vivo-verified) | 0 | 1 (`play`) |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 1 on the coastline (`do_replay_menu`), 5 not attempted (`init_game`, `view_scores`, `select_profile`, `main_menu_callback`, `replay_selector`/`draw_replay_selector`) | 17 distinct (batch 14's 21, minus the four this pass promotes) |
| original bytes recovered as clean source | 8397 | 52939 |
| original bytes offline-verified | 8397 | 36021 |

Batch 15's six: `key_to_str` (menu_keys.c, 2543 B); `create_replay`,
`load_replay` (replay.c, 1390 B); `getGameDataXML` (game_data.c,
1855 B); `draw_results` (results.c, 839 B); `my_alert` (alert.c,
1770 B).

File list this pass:
`src/icytower/{menu_keys,game_data,results,alert}.c` (new),
`src/icytower/replay.c` (+`create_replay`, +`load_replay`),
`carrier/lift/harness/{batch15_check.py,batch15_check.c,batch15b_check.py,batch15b_check.c,pf_harness_batch15.h,build_batch15.sh}`
(new), `carrier/recovery_audit_policy.json` (2 entries),
`notes/replay_format.md` (SS4's two open literals closed),
`artifacts/src_equivalence.json` (6 entries).
