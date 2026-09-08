
## Divergence 008 - `add_floor`'s `rand()` reached the wrong C library (2026-09-07)

Closes the two open problems the "binding table generated" pass left above:
`add_floor` DIFFERing in vivo, and `BindSavedState`'s `[8]` overflow.

### A. What differed, named by field

`carrier/scripts/bind_all.py --fn add_floor` over `replays/human_test.txt`
reported `FIRST DIFFERENCE fn=add_floor k=5 T=220 field=post` with identical
`args=004f8b18` and identical `pre` through k=4. Two snapshots at tick 220
(one unbound = the ORIGINAL machine code, one `--bind add_floor=src`) and
`pf_inspect.py diff` name the bytes:

```
carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt \
    --stop-at-tick 221 --snapshot-at-tick 220 --snapshot-out ../artifacts_task/d008_orig
carrier.exe ... --bind add_floor=src --snapshot-out ../artifacts_task/d008_src
python carrier/scripts/pf_inspect.py diff artifacts_task/d008_orig artifacts_task/d008_src

FIRST DIFFERING GLOBAL INSIDE THE PER-TICK DIGEST SCOPE:
  map @0x004f8b18 (Tmap, 784 bytes, main.c)
  first differing byte: +172 (VA 0x004f8bc4)  A=14 B=17
  member: .room+172
```

172 = `room[7]` (7 x `sizeof(Tfloor)` = 168) + 4 = **`Tfloor.start_tile`**;
`.end_tile` at +176 differs too. ORIGINAL `{start_tile=20, end_tile=29}`,
SRC `{23, 34}` - same `level` (6), same branch taken, different `rand()`
values. k=5 is the first `add_floor` invocation that calls `rand()` at all
(level 0 is a checkpoint, levels 1..4 are empty filler rows, 0 draws each -
`notes/layout_determinism.md` SS2), which is why the split lands exactly
there and not earlier.

### B. Root cause: a binding gap, not a semantic error

`src/icytower/map.c` calls plain `rand()`. `gen/pf_bindings_src.h` binds
every game global and game function by plain name, but `rand` is on
`gen_bindings.py`'s `RESERVED_CRT_WINDOWS_IDENTS` skip list and is not a
game-scope symbol at all - so nothing bound it, and the link resolved it to
the **carrier's own** statically-linked CRT:

```
carrier/obj/carrier.map:  0001:000aa60d  _rand  100ab60d f  libucrt:rand.obj
mingw32 nm carrier/obj_gcc/map.o:        U _rand
```

That is a different generator from the **guest's** msvcrt `rand` import,
whose IAT slot (0x514944) the carrier owns and, in `--det`, replaces with
`det.cpp`'s pinned LCG - the one `new_game()`'s
`srand(Treplay.random_seed)` seeds and `snapshot.cpp` captures. MEASURED,
same two tick-220 snapshots, `pf_inspect.py show`:

| run | pinned `rng_state` | `rng_calls` |
|---|---|---:|
| ORIGINAL | 0xea58d532 | 14 |
| SRC (pre-fix) | 16944 - *still exactly the seed* | 4 |

The `src` form advanced the game's own RNG **zero** times over all 30
initial floors; the ORIGINAL drew 10 (5 real floors x 2 draws each). Both
runs show `rec_seed = 16944`, and re-running the pinned LCG from
`srand(16944)` in Python reproduces the ORIGINAL floor exactly - `r1=22602`
-> `width = 6 + 22602 % 9 = 9`, `start_tile = 5 + 4173 % 21 = 20`,
`end_tile = 29` - which is positive proof that `map.c`'s recovered RULES
were correct and only its `rand()` SOURCE was wrong. (The same model also
lands the state on 0xea58d532 after exactly 10 draws, matching `rng_calls`.)

The x87 `fidivr`/`fmuls` ratio, the two magic-multiply divisors, the
checkpoint/empty cadence and the `floor_size_modifiers` clamp were all
re-checked against `artifacts/disasm.txt` 0x4167dc..0x416a3c line by line
and are correct; the float constants at 0x4d6dc0/0x4d6dc4 read out of the PE
as 300.0f and 10.0f, exactly as `map.c` has them.

### C. Why 160 000 offline vectors missed it

`carrier/lift/harness/lift_check.py` force-includes
`carrier/lift/harness/pf_harness_rand.h`, which redirects `map.c`'s
`rand()` to `harness_rand()` - the same LCG, seeded per vector - on BOTH
sides of the comparison (unicorn hooks the guest thunk and emulates the LCG
in Python; the compiled candidate calls `harness_rand()`). That is correct
for an offline oracle, and it is exactly why the oracle is blind here: it
substitutes a shim for the library call, so it tests the *algorithm* and
structurally cannot test *which copy of the library the real build reaches*.

It was **not** a missing precondition. `gen_add_floor`'s directed level pool
already contained `level = 5`, and at that index `floor_shrink` was already
nonzero, so the exact in-vivo branch was being generated on every run. The
in-vivo difficulty settings were read out of the snapshot for confirmation:
`Treplay+0x8c..0x9c` = `{floor_shrink=1, floor_size=1, start_speed=5,
speed_increase=1, gravity=1}`.

### D. The fix

`gen/gen_bindings.py` gained `GUEST_CRT_IMPORTS` - a deliberately tiny,
hand-curated set of C-library names that a promoted `src/` function must
share the GUEST's copy of. It emits, into `pf_bindings_src.h`:

```c
#include <stdlib.h>   /* FIRST, so the macros rewrite CALLS, never declarations */
/* rand  -> msvcrt.dll!rand IAT slot VA=0x00514944  (the original's own thunk) */
typedef int (__cdecl *PFN_crt_rand)(void);
#define rand (*(PFN_crt_rand *)0x00514944)
/* ... and the same for srand at 0x0051495c */
```

An indirect call through the guest's IAT slot is bit-for-bit what the
original machine code does (`call _rand` @0x4bad18 -> `jmp *[0x514944]`),
so it picks up whatever the carrier installed there - the pinned LCG in
`--det`, the real msvcrt otherwise - and is counted by the import census
like any other guest import. Slot VAs are looked up in `imports.json` (the
same evidence file `gen_imports.py` reads), never hand-typed; the generator
aborts if a name is missing, and refuses to guess when a name has two
distinct slots (this image imports `msvcrt!_stat` twice - MEASURED while
writing the loader). `srand` is bound for the same reason even though no
promoted function calls it yet, so the seed and the draws can never end up
split across two generators.

`map.o` no longer references `_rand` at all (`nm`: only `_get_demo` and
`_memmove` remain undefined). `src/icytower/map.c` is unchanged apart from a
header note recording that its `rand()` is part of its binding surface.

### E. Results after the fix

```
python carrier/scripts/bind_all.py --fn add_floor --out-dir ../artifacts_task/d008_bind
  -> EQUAL (533 invocations)
     per-tick digest: EQUAL (2293 ticks) vs replays/human_test.digest

python carrier/lift/harness/lift_check.py --form src --funcs add_floor --vectors 20000 --seed {20260907,1,2,3}
  -> [add_floor/SRC] EQUAL over 20000 vectors            (x4 seeds, MSVC)
python carrier/lift/harness/lift_check.py --form src --toolchain gcc --funcs add_floor --vectors 20000 --seed {20260907,1,2,3}
  -> [add_floor/SRC/GCC] EQUAL over 20000 vectors        (x4 seeds, GCC x87)
negative control (GCC, --fault add_floor:5:0)
  -> DIFFER at vector 5 ... Tmap+0x0 (VA 0x00792000)     (oracle still discriminates)

carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt \
    --stop-at-tick 2528 --run-seconds 300 --bind-file <abs>/scripts/all_src.bindfile \
    --digest-out ../artifacts_task/d008_allsrc_ticks.txt
python carrier/scripts/compare_digests.py artifacts_task/d008_allsrc_ticks.txt replays/human_test.digest
  -> EQUAL (2293 ticks)      <- ALL 40 generated binding-table rows bound to src at once

powershell scripts/gates.ps1 -Tag d008
  G1  EQUAL (876 ticks)   G2  EQUAL (877 invocations)
  G3a EQUAL (301 rows pre/post-rewind; 602 rows cold-from-anchor)
  G3b in-run rewind restored the sensor to k=276
```

`carrier/scripts/all_src.bindfile` is new: every row of `gen/bind_table.inc`
(40 - `draw_buffer`/`start_reward` have no linked `src` form). The
historical `all35_src.bindfile` is deliberately left untouched so the
milestone-12 "all 35 bound at once" measurement stays reproducible as taken.

### F. Regression guard added to the offline generator

`lift_check.py`'s `gen_add_floor` paired its hand-picked
`ADD_FLOOR_LEVEL_POOL` with `floor_shrink = 0 if k % 2 == 0` and a random
`floor_size`, which made the DIRECTED half of the generator one-sided: an
even-indexed pooled level was NEVER seen with `floor_shrink != 0` (the
float-ratio branch) and an odd-indexed one never with `floor_shrink == 0` -
half of every hand-picked boundary went untested in the branch it was picked
for. The first `len(pool) * 2 * 5 = 750` vectors now enumerate the full
`(pooled level) x (floor_shrink in {0,1}) x (floor_size in 0..4)`
cross-product deterministically (`ADD_FLOOR_DIRECTED`), which contains the
exact in-vivo triple `(level=5, floor_shrink=1, floor_size=1)` at index 56;
the random families beyond it are unchanged. This cannot catch a recurrence
of divergence 008 itself (see SS C - no offline harness can), but it does
close the coverage hole the investigation exposed while looking for one.

### G. `BindSavedState[8]` overflow, fixed

**What the array is for.** Not nested/re-entrant stub state - the stub keeps
none (its `id` rides on the guest stack, `bind_stub_common`). `BindSavedState`
holds the sensor's PER-BOUND-FUNCTION counters - `g_invocations[]`,
`g_crossings[]`, `g_records[]`, one slot per binding-table row - which live
in `bind.cpp` statics rather than guest memory, so an in-process rewind
(`--snapshot-at-tick` + `--restore-at-tick`) has to rewind them too;
otherwise the `k=` index in `--fn-digest-out` would keep counting up across
the rewind and `compare_fn_digests.py` could not line the two passes up
(milestone 8). It is embedded by value in `snapshot.cpp`'s `CarrierState`
and written verbatim into `carrier.bin`.

**The bug.** The three arrays were still `[8]`, carrying a comment that
claimed "== bind.cpp's kMaxFns" long after `kMaxFns` had grown 8 -> 35 ->
42, while `bind_state_save`/`bind_state_load` loop `i < kMaxFns`. Every
`--snapshot-at-tick`/`--restore-at-tick` therefore wrote
`3 x (42 - 8) x 8 = 816` bytes past the end of a stack-allocated
`CarrierState`, and read them back the same way. Nothing checked the two
against each other.

**The fix, sized from the generated table rather than restated.**
`bind.hpp` now declares `kBindMaxFns` and sizes all three arrays from it;
`bind.cpp`'s `kMaxFns` is defined AS `kBindMaxFns`, so the snapshot struct
and the loops can no longer disagree. Two `static_assert`s make future drift
a build error instead of a silent overrun: `kNumFns <= kMaxFns` (the
GENERATED table is the authority on how many rows exist) and
`sizeof(kStubs)/sizeof(kStubs[0]) == kMaxFns` (a short `kStubs` initializer
would otherwise zero-fill silently). `PF_CARRIER_STATE_VERSION` 1 -> 2,
because `carrier.bin`'s size and shape changed; snapshots taken by older
builds are now rejected with a legible reason instead of "wrong size".

**Proof, on the all-bound run** (40 rows bound - the configuration that used
to overrun by the full 816 bytes):

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt \
    --stop-at-tick 1000 --bind-file <abs>/scripts/all_src.bindfile \
    --digest-out d008_allR.txt --fn-digest-out d008_allF.txt \
    --snapshot-at-tick 400 --snapshot-out d008_allsnap --restore-at-tick 700

python scripts/certify_snapshot.py rewind d008_allR.txt --anchor 400 --cold d008_g1a.txt
  -> EQUAL (301 rows, T=400..699, pre-rewind vs post-rewind)
  -> EQUAL (602 rows, T=400..1000, cold-from-anchor vs post-rewind)
```

and every function re-invoked after the rewind resumes at exactly the `k` it
held at the snapshot (checked directly over `d008_allF.txt`; the 12 rows
never invoked again after T=400 have nothing to check). `carrier.bin` is now
2712 bytes. Note `certify_snapshot.py fn` is NOT applicable to a multi-
function `--fn-digest-out` - it assumes a single function's `k` sequence and
will report a spurious FIRST DIFFERENCE on an interleaved file; use it as
`gates.ps1` G3b does, on a single `--bind <fn>=src` run.
