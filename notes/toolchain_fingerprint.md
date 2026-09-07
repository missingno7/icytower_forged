# Compiler fingerprint experiment: TDM-GCC 4.4.1 vs the embedded bytes

Answers notes/external_research.md SS1 / queued experiment A. Labels:
KNOWN (measured directly), INFERRED (a conclusion drawn from KNOWN facts),
UNEXPLAINED (residual gap, flagged rather than rationalized away).

## 1. Toolchain archived (KNOWN)

Both SJLJ point releases were reconstructed from SourceForge and verified:

| variant | gcc --version | dest |
|---|---|---|
| tdm-1 | `gcc.exe (TDM-1 mingw32) 4.4.1` | `third_party/tdm-gcc-4.4.1-tdm-1/mingw32/` |
| tdm-2 | `gcc.exe (TDM-2 mingw32) 4.4.1` | `third_party/tdm-gcc-4.4.1-tdm-2/mingw32/` |

Each assembled from the official `gcc-4.4.1-tdm-N-{core,g++}.tar.gz` component
archives plus `binutils-2.19.1-mingw32-bin`, `mingwrt-3.16-mingw32-{dev,dll}`,
`w32api-3.13-mingw32-dev` extracted (never installed/executed) from the
matching bundle installer `tdm-mingw-1.908.0-4.4.1[-2].exe` (an NSIS
installer, opened read-only with 7-Zip). Full file/size/sha256 provenance:
`third_party/MANIFEST.json`, produced by `scripts/fetch_third_party.py
--toolchain` (idempotent, re-verifies `gcc --version` on every run).

**Independent corroboration (KNOWN, artifacts/dwarf_info.txt):** one CU's
`DW_AT_producer` is `GNU AS 2.19.1` — exactly the binutils version bundled
with both variants — alongside 145 CUs at `GNU C 4.4.1` and 2 at `GNU C
4.2.1-sjlj (mingw32-2)` (the prebuilt libogg, unchanged from
notes/external_research.md). No CU's producer string carries flags (GCC only
records them with `-grecord-gcc-switches`, not used here) — flag recovery is
necessarily empirical, which is what this experiment does.

Allegro 4.4.1 fetched as a git worktree (`third_party/allegro-4.4.1`, tag
`4.4.1`, commit `38624f977d957c32fd7a7ade31ee308a740f4022`) alongside the
existing 4.4.3.1 checkout. Its `CMakeLists.txt` sets `ALLEGRO_NO_ASM 1`
unconditionally (matches the earlier "C-only blitters unconditional in 4.4
CMake" finding) and `-DALLEGRO_STATICLINK` for a static build; the one
CMake-generated header needed to compile (`alplatf.h`) is a 15-line
platform-detection stub, byte-identical to 4.4.3.1's, reproduced by hand in
`third_party/build-allegro-4.4.1/include/` rather than running CMake's own
old-MinGW-unfriendly configure step.

## 2. Method (KNOWN)

`tools_recon/fingerprint_compare.py`: for each (toolchain, flag-set, CU),
compile with TDM-GCC 4.4.1, slice every named function out of the resulting
COFF object (symbol address to the next symbol's address — verified stable:
GCC 4.4.1 emits functions in source order for all CUs tested here, no
function reordering observed at any optimization level), mask that
function's own COFF relocations (`dir32`/`rel32`, 4 bytes each — the
absolute-address/call-target fields the linker would fill in) to 0 at the
same offsets on both sides, and compare against the ORIGINAL bytes read
from `assets/icytower15.exe` at `file_offset = va - 0x400000 - 0x1000 +
0x400 = va - 0x400C00` (this EXE carries no `.reloc` section — fixed
0x400000 image base, so VA-to-file-offset needs no relocation table).

Two scores per function: **byte_exact_masked** (same length AND masked
bytes identical) and **mnemonic_seq_ratio** (capstone-disassembled
mnemonic-sequence similarity, 0..1, independent of the masked-out
immediate's value). Flag matrix: `-O0/-O1/-O2/-O3/-Os` x
with/without `-fomit-frame-pointer` x default/`-march=i686`/`-march=pentium`
x `-mfpmath=387` always (the default for this 32-bit non-SSE compiler; no
`-ffast-math`) = 30 combinations x 2 toolchains = 60 compiles per CU.

Targets: the four recovered game CUs the task named
(`src/icytower/{update_frame,is_solid,add_combo,control}.c`, compiled
standalone against `src/icytower`'s own stand-in headers — no Allegro
needed, `state.c` not required since only `-c` compiles were done, no
link), plus five Allegro/logg CUs compiled from the real
`third_party/allegro-4.4.1` source tree
(`src/timer.c`, `src/color.c`, `src/blit.c`, `src/c/cblit32.c`,
`addons/logg/logg.c`) against its own headers with
`-DALLEGRO_STATICLINK` (`cblit32.c` additionally needs `-DALLEGRO_COLOR32`,
which its `#ifdef` guard requires to emit any code at all; `logg.c` needs
`vorbis/vorbisfile.h`/`ogg/ogg.h`, copied from MSYS2's mingw32 package into
`third_party/vorbis-headers/` since MSYS2's own CRT headers are too new for
GCC 4.4.1 to parse directly). All nine source files compiled cleanly under
TDM-GCC 4.4.1 with **zero errors** at every flag combination tried.

4440 raw (target, toolchain, flags) rows in `artifacts/toolchain_fingerprint.json`
(`raw_rows`), plus a per-CU ranked-flag-set `summary_best_flag_set_per_cu`
and a `per_function_best` table.

## 3. Score table: best flag set per CU (aggregate, both toolchains — IDENTICAL)

tdm-1 and tdm-2 scored **identically** on every target tested (no
byte/mnemonic difference anywhere in the matrix) — the tdm-1→tdm-2 fix
(contemporary forum reports: "tdm-1 had a code-generation bug") does not
touch any of the code paths these 75 functions exercise.

| CU (group) | n fns | best flags | byte-exact (masked) | avg mnemonic ratio |
|---|---:|---|---:|---:|
| game/control.c | 11 | **-Os** | 9/11 (82%) | 0.972 |
| game/control.c | 11 | -O1 | 8/11 (73%) | 0.952 |
| allegro/timer.c | 17 | **-O2** | 8/17 (47%) | 0.978 |
| allegro/color.c | 20 | **-O2** | 7/20 (35%) | 0.991 |
| allegro/addons/logg/logg.c | 11 | -Os | 4/11 (36%) | 0.838 |
| allegro/c/cblit32.c | 5 | -O2 (tie -O3) | 1/5 (20%) | 0.973 |
| allegro/blit.c | 7 | **-O2** | 0/7 (0%) | **0.997** |
| game/is_solid.c | 1 | -O2 (tie -O3) | 0/1 | 0.935 |
| game/update_frame.c | 1 | -O2 (tie -O3) | 0/1 | 0.833 |
| game/add_combo.c | 1 | -O1+pentium | 0/1 | 0.818 |

`-march`/`-fomit-frame-pointer` variants never beat the bare optimization
level for any CU's aggregate score (full ranked-list in the JSON's
`summary_best_flag_set_per_cu`) — `-mfpmath=387` (always on) is the only
non-default flag that ever helps, and it is the compiler's own default here
so it changes nothing versus a bare `-Om`.

## 4. Concluded build configuration

- **Allegro 4.4.1 (library CUs): `-O2`, no `-march`, no
  `-fomit-frame-pointer` (INFERRED, strong).** `timer.c` and `color.c` each
  have 7-8 functions **byte-exact** (masked) at plain `-O2` and none at any
  other level; every non-exact function in those two CUs is still >0.96
  mnemonic-ratio at `-O2` specifically, with 1-3-byte length deltas
  (register-allocation/scheduling variance, not a different compiler or
  optimization level). `-O2` is also GCC's and Allegro's own conventional
  release setting, so this is an expected, not surprising, result — the
  fingerprint experiment now gives it byte-level evidence instead of
  convention alone.
- **Game code (`control.c`): `-Os`, not `-O2` (INFERRED).** 9/11
  predicates byte-exact at `-Os` (one more than `-O1`'s 8/11, and `-O2`
  produces **zero** exact matches for this CU — plain `-O2`'s codegen for
  these trivial predicates diverges from the original at exactly the
  instructions `-Os`/`-O1` get right). This is the most interesting single
  finding: **the game's own object code and the statically-linked Allegro
  library were very likely built at different optimization levels** —
  consistent with Code::Blocks' default new-project template using `-Os`/
  `-O0`-ish "Debug"-adjacent settings for the game project while Allegro
  was built once, separately, as a release static library (`-O2`, its own
  convention). This matches the project's known bundled-MinGW / Code::Blocks
  10.05 toolchain story (notes/external_research.md SS1) better than "one
  flag set for everything" would.
- **`is_any` / `check_control_key` (control.c) and `add_combo`/`is_solid`/
  `update_frame` never reach byte-exact at any flag/march combination
  tested (UNEXPLAINED, see SS5).**

## 5. What remains unexplained

- **`blit.c`: 0/7 byte-exact but 0.997 average mnemonic ratio at `-O2`** —
  the closest possible near-miss without being a match. `blit_from_24`
  (3683 B) and `blit_from_32` (3437 B) are 1.000 mnemonic-ratio (every
  instruction mnemonic in the same order) yet 1-3 bytes longer than the
  original. This is the strongest single piece of evidence that **the
  overall toolchain identification is correct** but something below
  mnemonic level differs for large/hot functions specifically — likely
  scheduling/alignment padding (`-falign-functions`/`-falign-loops`
  defaults, not swept here) or a minor point-release difference within the
  4.4.1 family (this experiment only had tdm-1/tdm-2 to try, both scored
  identically; a plain, non-TDM 4.4.1 mainline build was not available to
  test).
- **`add_combo`, `is_solid`, `update_frame`: no flag set reaches byte
  parity, and even the mnemonic ratio is mediocre (0.82-0.94).** These are
  the three files whose logic was reconstructed from disassembly/behavior
  rather than compiled straight from an unmodified upstream source (unlike
  the Allegro files) — `add_combo.c`'s own file comment already flags that
  the original does three separate field stores where the recovered C uses
  one struct assignment (62 B original vs. 44-52 B compiled — the compiled
  form is smaller, i.e. semantically equivalent but not the same source
  shape the original author wrote). `is_solid`/`update_frame` are 1 and 4
  bytes off in length at their best level with high-0.8s/0.93 mnemonic
  ratio — plausibly a single instruction-selection choice apiece (e.g. a
  `lea`/`shr` vs. `sar` idiom), not a wrong toolchain. Recommendation for a
  follow-up pass: try hand-editing the C source's expression *shape*
  (operation order, explicit temporaries) for these three functions against
  `-Os`/`-O1`, the same way `add_combo.c`'s own comment already documents
  the original's three-separate-store pattern, rather than sweeping more
  compiler flags — the residual gap here looks source-shape-shaped, not
  flag-shaped.
- **`logg.c`: only 36% (4/11) at its best level (`-Os`).** Expected to be
  weaker than plain Allegro: the embedded CU is a heavily vendored file (18
  functions vs. upstream's 11 — notes/external_research.md SS2), so the 7
  game-added functions sitting in the same translation unit change register
  pressure/spill decisions for the neighboring stock functions even though
  their own bytes were never compared.
- **DWARF hint check (KNOWN, weak signal):** `artifacts/dwarf_info.txt` has
  1447 `DW_OP_fbreg` (stack-resident variable) locations against 552
  `DW_OP_reg*` (register-resident) across the whole binary — consistent
  with some register allocation happening everywhere (rules out pure
  `-O0`), but not CU-specific enough by itself to distinguish `-Os` from
  `-O1`/`-O2` (this dump format carries no `is_stmt`/view markers to check
  separately per the task's suggestion — DWARF version/producer here is
  GCC 4.4.1's plain line-table form, no discriminators). The byte-level
  score table above is the stronger signal and is what SS4's conclusions
  rest on.

## 6. Reproducing this experiment

```
python scripts/fetch_third_party.py --toolchain   # idempotent; re-verifies gcc --version
python tools_recon/fingerprint_compare.py --out artifacts/toolchain_fingerprint.json
```
`--groups "game/control.c" "allegro/timer.c"` restricts to a subset. Compile
scratch lives in `third_party/fingerprint_work/` (not committed).
