
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
