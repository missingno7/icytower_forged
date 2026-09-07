# GCC toolchain experiment for win32_pilot.md SS6a (2026-09-07)

Question: `src/icytower/line_intersect.c` compiled by 32-bit MSVC into
`src_check.exe` DIFFERs from the original bytes on 997/20000 vectors
(`artifacts/src_equivalence.json`, `src/icytower/PROMOTIONS.md`), because
MSVC computes it in 64-bit `double`/SSE precision while the original binary
(built by an old MinGW GCC, per `game_types.h`'s provenance comments) keeps
every intermediate on the real 80-bit x87 register stack. win32_pilot.md
SS6a already names the predicted fix: "GCC `-mfpmath=387`, the original
toolchain family". Does compiling the **same, unmodified**
`src/icytower/line_intersect.c` with the 32-bit MinGW GCC now installed
(`C:\msys64\mingw32\bin\gcc.exe`, GCC 16.2.0, target `i686-w64-mingw32`)
actually make it bit-equal?

**Short answer: `-mfpmath=387` alone does not. `-mfpmath=387 -mno-sse2`
does — 0/20000 domain differences on every seed, both `-O1` and `-O2`.**
Disabling SSE2 turned out to be the load-bearing flag, not `-mfpmath=387`;
`-O0` and `-ffloat-store` both defeat it even with SSE2 disabled.

New files (all additive; nothing under `carrier/src` or the existing
MSVC-form harness behaviour was touched):

```
gcc_check.c              GCC-toolchain SRC-side driver (wire-protocol twin
                          of src_check.c), wired up for line_intersect and
                          jump_player only
build_src_gcc.sh          builds one flag variant: build_src_gcc.sh OUT.exe
                          [gcc flags...], via MSYSTEM=MINGW32 bash
build_src_gcc.cmd         cmd.exe wrapper around build_src_gcc.sh
lift_check.py             + --toolchain {msvc,gcc} (default msvc = byte-
                          for-byte unchanged behaviour). --toolchain gcc
                          requires --form src, defaults --exe to
                          gcc_check_x87_nosse_O2.exe (the verified-correct
                          build below) and --funcs to
                          line_intersect,jump_player
gcc_check_x87_*.exe       the flag-set variants tested (SS1)
```

## 0. Why a separate driver instead of reusing src_check.c/pf_bindings_harness.h

`src_check.c`'s MSVC build force-includes the generated (72KB)
`pf_bindings_harness.h`, which itself pulls in `it_funcs.h` and
`pf_bindings_types.h` -- portability surface this two-function experiment
does not need and did not want to take on. `gcc_check.c` instead compiles
`line_intersect.c`/`jump_player.c` in the plain **STANDALONE** world
(`game_state.h`'s ordinary `extern` declarations, win32_pilot.md SS7a) and
supplies storage for the only two globals `jump_player.c` reads
(`collision_type`, `max_speed`) itself, syncing them from the guest image
at the two known VAs immediately before each call (both read-only from
`jump_player`'s side, so no write-back is needed). `line_intersect.c`
touches no globals at all. This is why `gcc_check.exe` is a **wholly
GCC-compiled** executable, never linked against an MSVC object file --
the cheapest of the three routes the task named, and it sidesteps the
cdecl name-mangling question entirely (moot here, since no cross-compiler
link ever happens).

## 1. Flag matrix (line_intersect, 4 seeds x 20000 vectors = 80000 total)

Every run compares EAX + `*px` + `*py` against the same unicorn oracle
`lift_check.py` already uses, over seeds `20260907, 1, 424242, 987654321`
(the four canonical seeds from `artifacts/lift_x87_finding.md`).

Two **independent** effects show up in the raw diff count, and they must be
separated to answer the SS6a question honestly:

- **EAX (return-value) diffs**: **948 / 985 / 1011 / 989** per seed, byte-for-byte
  **identical across all nine builds tested below, GCC and MSVC alike**
  (confirmed with a per-vector probe, not just the first-difference report).
  This is a toolchain-independent, pre-existing discrepancy, unrelated to
  floating-point precision -- see SS4. It is **not** part of the SS6a
  question and is called out separately.
- **Domain diffs** (`*px`/`*py`, only reached when EAX already agrees): this
  is what SS6a is actually about, and it is what varies by flag set below.

`948 + 49 = 997`, `985 + ... `, etc. -- summing the two exactly reproduces
`src/icytower/PROMOTIONS.md`'s headline "997/20000" figure for the MSVC
build, confirming the two effects are additive and the split is real, not
an artifact of how this experiment counts.

Every cell below is a directly measured domain-diff count for that exact
executable/seed pair (`carrier/lift/harness/gcc_check_x87_*.exe`), not an
extrapolation.

| flags | domain diffs, seeds 20260907 / 1 / 424242 / 987654321 | verdict |
|---|---|---|
| `-mfpmath=387 -O2` (SSE2 present, default) | 24 / 16 / 20 / 26 | DIFFER |
| `-mfpmath=387 -O1` (SSE2 present) | 24 / 16 / 20 / 26 (identical to O2 -- same instruction selection for this function) | DIFFER |
| `-mfpmath=387 -O0` (SSE2 present) | 49 / 33 / 46 / 48 (= MSVC exactly -- see SS3) | DIFFER |
| `-mfpmath=387 -O2 -ffloat-store` | 49 / 33 / 46 / 48 (= MSVC exactly) | DIFFER |
| `-mfpmath=387 -O1 -ffloat-store` | 49 / 33 / 46 / 48 (= MSVC exactly) | DIFFER |
| `-mfpmath=387 -O0 -ffloat-store` | 49 / 33 / 46 / 48 (= MSVC exactly) | DIFFER |
| **`-mfpmath=387 -mno-sse2 -O2`** | **0 / 0 / 0 / 0** | **EQUAL** |
| **`-mfpmath=387 -mno-sse2 -O1`** | **0 / 0 / 0 / 0** | **EQUAL** |
| `-mfpmath=387 -mno-sse2 -O0` | **343 / 330 / 338 / 346** (worse than MSVC) | DIFFER |
| `-mfpmath=387 -mno-sse2 -O2 -ffloat-store` | 49 / 33 / 46 / 48 (= MSVC exactly) | DIFFER |
| `-mfpmath=387 -mno-sse2 -O1 -ffloat-store` | 49 / 33 / 46 / 48 (= MSVC exactly) | DIFFER |
| `-mfpmath=387 -mno-sse2 -O0 -ffloat-store` | 49 / 33 / 46 / 48 (= MSVC exactly) | DIFFER |
| `-mfpmath=sse -msse2 -O2` (pure-SSE negative control) | 49 / 33 / 46 / 48 (= MSVC exactly, vector-for-vector) | DIFFER (expected) |
| MSVC `src_check.exe` (baseline, `artifacts/src_equivalence.json`) | 49 / 33 / 46 / 48 | DIFFER (expected) |

`-O0` (SSE2 present) landing exactly on MSVC's 49/33/46/48 -- rather than
between the SSE2-O2 case (24/16/20/26) and the SSE2-absent-O0 case
(343/330/338/346) -- makes sense once SS3 is read: at `-O0` every named
`double` local is stored to memory at its own assignment regardless of
SSE2, so `D`/`ua`/`ub` are already ordinary rounded 64-bit doubles by the
time the final conversion runs; whether that conversion is then `cvttsd2sil`
or `fistp` off an already-53-bit value makes no further difference, which
is exactly what the matching counts show.

`jump_player`: **EQUAL in all nine builds, all four seeds, 0 EAX diffs and 0
domain diffs every time** (36/36 runs) -- confirmed as the control: its only
FP ops (`sx+sx`, `sx*-2.0`, comparisons) are exact under any precision, so
no flag choice can make it differ, and none did.

First difference of the default seed (20260907), representative of every
DIFFERing build above except `-O0`:

```
[line_intersect/SRC] DIFFER: *py+0x0 (VA 0x00794004) original 0xa3 lifted 0xa4
```

`-O0`'s first difference comes far earlier (vector 2), because it starts
losing precision immediately rather than only at the ~10% of vectors the
`_boundary()` generator constructs to sit exactly on the truncation edge
(`carrier/lift/README.md` SS7).

Negative control, both functions (byte-flip named exactly, as with every
other form):
```
line_intersect: DIFFER at vector 5: *px+0x2 (VA 0x00794002) original 0x00 lifted 0x01
jump_player:    DIFFER at vector 11: Tplayer+0x0 (VA 0x00790000) original 0x49 lifted 0x48
```

## 2. Why `-mfpmath=387` alone is not enough: SSE2 steals the final truncation

Disassembling `-mfpmath=387 -O2` (SSE2 present, the flags the task first
asked to try) shows the arithmetic genuinely on the x87 unit (`fmul`,
`fadd`, `fdivp`, `fcomi`) -- but the final `(int)(ua*dx1 + 0.5)` conversion
is **not** an `fistp`:

```asm
fildl   4(%esp)
fmul    %st(1), %st        # x87: ua * dx1
flds    LC2                # 0.5f
fadd    %st, %st(1)        # x87: ... + 0.5   (still 80-bit in ST)
fxch    %st(1)
fstpl   8(%esp)             # <-- STORE to memory as a 64-bit double: ROUNDS to 53 bits here
...
cvttsd2sil 8(%esp), %eax    # SSE2 truncate-to-int, off the ALREADY-ROUNDED double
```

GCC 16.2's default code generator prefers SSE2's `cvttsd2si` for
`(int)double` on any target where SSE2 is available (mingw32's baseline
target has SSE2 by default, independent of `-mfpmath`), because that
conversion does not need the x87 unit at all. To feed it, the compiler
materialises the x87 result as an ordinary 64-bit `double` first -- exactly
the double-rounding step `win32_pilot.md` SS6a and
`artifacts/lift_x87_finding.md` identify as the fidelity gap, reintroduced
by the compiler's own instruction selection even though `-mfpmath=387` put
the arithmetic on x87. `-mfpmath` selects which unit computes float **ops**;
it does not control which unit performs a float **conversion**.

Adding `-mno-sse2` removes that option. The same function then compiles to
the textbook `fnstcw`/`orb $0x0C,%ah`/`fldcw`/**`fistpl`**/`fldcw`
round-toward-zero idiom -- byte-for-byte the same idiom
`carrier/lift/README.md` SS6a describes for the *original* GCC 4.4 output
and for the LIFTED form's modelled `fistp` path -- truncating directly off
the 80-bit ST(0), with no memory round-trip:

```asm
fnstcw  14(%esp)
fildl   (%esp)
movzwl  14(%esp), %eax
fmul    %st(1), %st
flds    LC2
orb     $12, %ah            # PC=11, round toward zero
movw    %ax, 12(%esp)
fadd    %st, %st(1)
fxch    %st(1)
fldcw   12(%esp)
fistpl  4(%esp)              # <-- truncate straight from the x87 register, 80-bit source
fldcw   14(%esp)
```

That single instruction-selection change is what takes the domain diffs
from 16-26/20000 to **0/20000** on every seed.

## 3. Does the build have to keep intermediates in registers to match?

**Yes, and this is the second load-bearing condition, independent of
SSE2:**

- `-mno-sse2 -O2` / `-O1`: **EQUAL**. `D`, `ua`, `ub` stay in x87 registers
  across the range-check branches (visible in the `-S` output: `fdiv`,
  `fcomi`, `fdivp` chain with no intervening `fstpl`/`fldl` of those
  variables) all the way to the final `fistpl`.
- `-mno-sse2 -O0`: **DIFFER, and *worse* than the MSVC `double` baseline**
  (343-346/20000 vs. MSVC's 33-49/20000). At `-O0` GCC's naive codegen
  spills every named local to its stack slot immediately after computing
  it, as an ordinary 64-bit `double` store -- with no optimizer pass to
  keep a value resident in a register across statements. `D` gets
  rounded to 53 bits at its own assignment, then `ua` is computed from the
  already-rounded `D` and immediately rounded again at *its* assignment,
  and so on -- **multiple** compounding roundings instead of MSVC's single
  one, which is why `-O0` loses more precision than doing nothing x87-
  specific at all. `-mfpmath=387` was honored (the ops are still x87
  instructions and the final step is still `fistp`), but it is necessary,
  not sufficient: without at least `-O1`'s register allocation, the values
  never survive on the stack long enough for the extra bits to matter.
- `-mno-sse2 -O2/-O1 -ffloat-store`: **DIFFER**, reproducing MSVC's exact
  49/33/46/48 counts on every seed. `-ffloat-store` is documented to force
  a store/reload after every floating-point assignment specifically to
  strip excess x87 precision for strict IEEE conformance -- i.e. it does,
  by design, exactly what defeated `-O0` above, but as an explicit,
  optimization-level-independent flag. It is the one flag in this
  experiment that behaves exactly as its name promises, and it is the
  wrong one for this goal.
- Explicit `-fexcess-precision=fast` at `-O2` changed nothing (972/20000
  total, identical to the flag-less baseline) -- GCC's un-flagged default
  for this dialect (no `-std=` given, i.e. `gnu17`) was already `fast`.
  `-std=c99 -fexcess-precision=standard` made it *worse* (1291/20000
  total), consistent with "standard" being the strict, round-at-every-step
  mode.

## 4. A second, unrelated finding: the EAX gap is not a precision question

The 948-1011-per-seed EAX mismatches are **identical, vector-for-vector,
across all nine builds** -- including `gcc_check_sse_O2.exe` (pure SSE,
no x87 at all) and the existing MSVC `src_check.exe`. That rules out
floating-point precision as the cause entirely: no amount of x87 fidelity
can change a value every build already agrees on. The default seed's first
instance (vector 401) is a fully-degenerate input -- segment 3-4 identical
to segment 1-2 (`x3=x1, y3=y1, x4=x2, y4=y2`), making `D = 0` and
`px = py = 0`, so `ua = N1/D = 0/0`. The oracle (real hardware, via
unicorn) returns EAX=1 (intersection found); every recompiled form,
GCC and MSVC alike, returns EAX=0. Since it reproduces identically
regardless of toolchain or precision model, this looks like a genuine,
pre-existing gap between `src/icytower/line_intersect.c`'s handling of the
`ua`/`ub` "unordered" (NaN) comparison on the `D == 0` degenerate path and
the original binary's actual branch structure at that address -- worth a
follow-up, but **out of scope for the SS6a floating-point question** this
experiment targets, and not touched here.

## 5. Verdict for win32_pilot.md SS6a (paste-ready)

> **GCC toolchain experiment, resolved 2026-09-07** (`carrier/lift/harness/GCC_X87.md`).
> Compiling the unmodified `src/icytower/line_intersect.c` with 32-bit
> MinGW GCC 16.2.0 and `-mfpmath=387` alone does **not** reach bit-equality
> with the original (16-26/20000 vectors still differ per seed, because
> GCC's default code generation truncates `(int)double` via SSE2's
> `cvttsd2sil` off a memory-rounded 64-bit value even when the arithmetic
> itself runs on x87 -- `-mfpmath` selects the unit for float *operations*,
> not for float-to-int *conversions*). Adding `-mno-sse2` removes that
> escape hatch and restores the textbook `fnstcw`/`fldcw`/`fistpl` idiom,
> giving **bit-exact equality with the original over 80000 vectors (4
> seeds x 20000) at `-O1` and `-O2`** -- confirming SS6a's rule as stated,
> with one addition: the flag needed is **`-mfpmath=387 -mno-sse2`**, not
> `-mfpmath=387` alone, on any GCC modern enough to default to SSE2.
> `-O0` and `-ffloat-store` both defeat it regardless of `-mno-sse2` --
> both force every named `double` local to round-trip through a 53-bit
> memory store at each assignment (respectively by omission of register
> allocation, and by explicit design), which is precisely the double-
> rounding this whole exercise exists to avoid; **`-O0`, in particular,
> compounds that loss at every one of `line_intersect`'s three FP locals
> and ends up worse than MSVC's plain `double` baseline (343-346/20000 vs.
> 33-49/20000)**. `jump_player` stayed EQUAL under every flag combination
> tested (36/36 runs), confirming it as a precision-insensitive control.
> Recommended standalone-port flags per SS6a, now measured rather than
> inferred: `-m32 -mfpmath=387 -mno-sse2 -O1` (or `-O2`) -- no
> `-ffloat-store`, no `-O0`. A separate, toolchain-independent EAX
> discrepancy (SS4 above) remains on `line_intersect`'s fully-degenerate
> (`D == 0`) inputs regardless of these flags; it is not a floating-point
> fidelity gap and needs its own investigation before this function is
> promoted past "pending a bit-exact build config" in
> `src/icytower/PROMOTIONS.md`.
