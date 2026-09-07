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
