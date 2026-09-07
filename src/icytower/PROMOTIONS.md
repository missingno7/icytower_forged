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
| `start_reward` | 0x407c38 | 472 | main.c | **EQUAL** (GCC x87, 20000/20000); MSVC DIFFER 1725/20000, 100% confined to `stars[]`/`seed` (new_rand-propagated precision, 0 logic divergences — exhaustive per-vector scan, not sampling) | `reward_time=0x50; reward_scale=0`; tier (0..9) from a 9-threshold cascade on the points argument (6/14/24/34/49/69/99/139/199). If `itrcheck==0`: if `!options.flash && tier>2`, spawn `(tier-2)*16` confetti particles into `stars[512]` via `create_particle()` + two `new_rand()` draws each (`sy = -(((new_rand()%500+500)<<16)/100)`, `sx = (((new_rand()%1000-500)<<16)*count)/100` — divisors and `sy`'s negation both re-derived by direct unicorn block execution, not assumed); `reward_bmp = asset_bitmap(ASSET_DATA_REWARD_000+tier)` runs regardless of `flash`/tier. `play_sound(combo_sound[tier],0,0)` always runs. Returns `tier`. Two recovery mistakes (reward_bmp nesting, `/50` vs `/100` + `sy` sign) caught and fixed by the 20000-vector check — see the `start_reward` entry in `artifacts/src_equivalence.json` for the full derivation. | pending |

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
