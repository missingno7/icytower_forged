# Standalone build against the real upstream Allegro 4.4.3.1

Proves the LIBRARIES-coastline header swap (`src/README.md`,
`src/icytower/GENERATED.md`, `carrier/gen/LIB_BINDINGS_NOTES.md`
"Standalone build swap"): every `.c` file under `src/icytower/` compiles
**unchanged** with 32-bit GCC against the REAL upstream Allegro 4.4.3.1
headers/static libs built in `third_party/` (`third_party/BUILD.md`),
exactly as it compiles against `src/icytower/allegro_types.h` /
`allegro_api.h`'s own stand-ins today.

## How the swap is selected

One new preprocessor guard, `ICYTOWER_UPSTREAM_ALLEGRO`, generated into
`allegro_types.h`/`allegro_api.h` by `carrier/gen/gen_src_headers.py` /
`carrier/gen/gen_lib_bindings.py` (never hand-edited). It sits alongside
the existing `ICYTOWER_BINDINGS_ACTIVE` guard so exactly one of three
states is ever active in a translation unit:

| macro defined | allegro_types.h | allegro_api.h |
|---|---|---|
| (neither) -- standalone, stand-in world | its own `#pragma pack(1)` BITMAP/RGB/SAMPLE/DATAFILE/PACKFILE/FONT/... structs | its own 100 function prototypes, 26 extern globals, 150 constants |
| `ICYTOWER_BINDINGS_ACTIVE` -- carrier world | skipped (`carrier/gen/pf_bindings_src.h` supplies the same names as address-backed macros) | skipped (`pf_lib_bindings.h` supplies the same names as address-backed macros) |
| `ICYTOWER_UPSTREAM_ALLEGRO` -- **this build** | `#include <allegro.h>` (+ a small CRT-reserved-name shim, see below) | `#include <logg.h>` only (the allow-list's 2 non-core-Allegro entries) |

No file under `src/icytower/*.c` mentions any of these macros or even
`#include`s `allegro_api.h`/`allegro_types.h` directly -- every one of them
reaches `allegro_types.h` transitively through `game_types.h`'s own
`#include "allegro_types.h"`. Selecting the swap is therefore a single
compiler flag, `-DICYTOWER_UPSTREAM_ALLEGRO`, on the command line; **zero
bytes of `src/icytower/*.c` change**, confirmed by `git diff --stat` showing
no `.c` file under `src/icytower/` touched by this work (see "Generator
changes" below for the only files that did change, all of them generated).

## Build

```
MSYSTEM=MINGW32 C:\msys64\usr\bin\bash.exe -lc \
  "cd /d/Games/DOS/dos_recosystem/icytower_forged && \
   mingw32-make -f src/build/Makefile.standalone clean all run"
```

`src/build/Makefile.standalone` compiles the 8 promoted-function `.c` files
plus `state.c` into `src/build/libicytower.a` with:

```
gcc -m32 -mfpmath=387 -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
    -Ithird_party/allegro-4.4.3.1/include \
    -Ithird_party/build-allegro-4.4.3.1/include \
    -Ithird_party/allegro-4.4.3.1/addons/logg \
    -Isrc/icytower -c <file>.c
```

then links `src/build/standalone_smoke.c` against `libicytower.a` +
`liballeg.a`/`liblogg.a` (`third_party/build-allegro-4.4.3.1/lib/`, built
per `third_party/BUILD.md`) + logg's own DLL-import deps + the Win32/DirectX
libs `liballeg.a` itself needs (same set `third_party/smoke/smoke.c`
links against) into `src/build/standalone_smoke.exe`, a 32-bit PE
(`i386`, confirmed with `objdump -f`).

Toolchain actually used: MSYS2 MINGW32, `gcc.exe (Rev3, Built by MSYS2
project) 16.2.0`, `i686-w64-mingw32` (native 32-bit -- `-m32` is accepted
but a no-op, kept because it is this task's specified invocation, same as
`third_party/BUILD.md` SS7 notes for `third_party/smoke/smoke.c`).

## Compile results, per file (`-Wall`, upstream world)

| file | result |
|---|---|
| `add_combo.c` | 0 errors, 0 warnings |
| `control.c` | 0 errors, 0 warnings |
| `is_solid.c` | 0 errors, 0 warnings |
| `jump_player.c` | 0 errors, 0 warnings |
| `line_intersect.c` | 0 errors, **1 pre-existing warning** (`-Wcomment`, "`/*` within comment" at its own line 22, `*px_out/*py_out` -- in the hand-recovered prose comment, predates this work, not touched by it, harmless) |
| `map.c` | 0 errors, 0 warnings |
| `update_frame.c` | 0 errors, 0 warnings |
| `state.c` | 0 errors, 0 warnings |
| `allegro_types.h` / `allegro_api.h` (parsed via the 8 files above + a standalone `#include "allegro_api.h"` parse-only check) | 0 errors, 0 warnings |

All 9 required files link into `libicytower.a`; `standalone_smoke.exe`
links and **runs to completion, exit code 0**:

```
=== standalone_smoke: clean src/icytower/ linked against real upstream Allegro 4.4.3.1 ===
PASS reset_map: every room[].empty == -1
PASS reset_map: every room[].level/.sign == 0
PASS reset_map: offset == 0
PASS getFloorData: fx1 == start_tile*16-2
PASS getFloorData: fx2 == end_tile*16+17
PASS getFloorData: fy matches the row-top+offset formula
PASS getFloorData: empty row leaves outputs untouched
PASS is_left: -1 (all-bits-set true) when key[KEY_LEFT] is down
PASS is_left: 0 when key[KEY_LEFT] is up
PASS update_frame: reward_time decremented
PASS update_frame: reward_scale grew by 0xccd (>60 band)
PASS update_frame: dead advanced by 8
PASS update_frame: edge_drawn advanced
PASS update_frame: frame advanced (logic_count %% 10 == 0)
PASS update_frame: reward_scale drained by 0x199a (<=9 band)
PASS update_frame: reward_time decremented again
=== 0 failure(s) ===
```

`key[KEY_LEFT]` is the **real** `extern volatile char key[]` linked from
`liballeg.a` (`allegro/keyboard.h`), not a stand-in -- proves the real
static library's data symbol resolves and is writable, not just that the
header parses. `reset_map`/`getFloorData`/`update_frame` all run against
plain `Tmap`/`Tplayer` structs and `game_state.h` externs (storage from
`state.c`, linked into the same binary); `allegro_init()` is deliberately
never called, matching the task brief -- none of these four functions
touch screen/gfx/timer driver state.

## Out of scope: `game_types_check.c`

`game_types_check.c` is **not** part of the 9-file/6-header set this task
names (`16 functions across 9 .c files plus ... game_types.h, game_state.h,
game_funcs.h, allegro_types.h, allegro_api.h, state.c`) and is deliberately
not built here. Its `sizeof`/`offsetof` assertions check the DWARF-recovered
**address-backed** layout of Allegro's *internal* structs -- e.g.
`LZSS_PACK_DATA`/`LZSS_UNPACK_DATA`/`_al_normal_packfile_details`, which
upstream's public `<allegro.h>` never exposes at all (confirmed: compiling
it under `-DICYTOWER_UPSTREAM_ALLEGRO` fails with "invalid use of undefined
type" on exactly those three, plus natural-alignment offset mismatches on
`BITMAP` et al., since upstream's real struct layout is not
`#pragma pack(1)`). That check only means something against
`src/icytower`'s own stand-in (or the carrier's address-backed one); it was
never meant to hold against upstream, and the task's own file list omits it.

## Generator changes made

Both changes are additive (a new guard alongside the existing one); neither
touches `carrier/src/`, `carrier/lift/`, or `notes/`.

1. **`carrier/gen/gen_src_headers.py`** (`allegro_types.h`):
   - New `UPSTREAM_GUARD = 'ICYTOWER_UPSTREAM_ALLEGRO'` constant next to
     `BINDINGS_GUARD`.
   - `emit_types_section()` gained an optional `name_filter` parameter, used
     to carve out a `CRT_RESERVED_NAMES` subset (`gi.RESERVED_TYPE_RENAMES`'s
     values: `it_orig_size_t`, `it_orig_FILE`, `it_orig__iobuf`,
     `it_orig_time_t`, ...) from the `'library'`-origin type set.
   - `allegro_types.h`'s writer now emits `#if defined(ICYTOWER_UPSTREAM_ALLEGRO)`
     first: `#include <allegro.h>`, followed by the CRT-reserved subset
     **unconditionally** (see "Why the CRT shim" below), then
     `#elif !defined(ICYTOWER_BINDINGS_ACTIVE)` with the original full
     stand-in body, unchanged.

2. **`carrier/gen/gen_lib_bindings.py`** (`allegro_api.h`):
   - Same `UPSTREAM_GUARD` constant.
   - `allegro_api.h`'s writer now emits `#if defined(ICYTOWER_UPSTREAM_ALLEGRO)`:
     `#include <logg.h>` only (the allow-list's 2 non-core-Allegro entries,
     `logg_load`/`logg_load_memory`; `<allegro.h>` already came in via
     `allegro_types.h`), then `#elif !defined(ICYTOWER_BINDINGS_ACTIVE)` with
     the original 100-function/26-global/150-constant stand-in body,
     unchanged.

Regenerated with the documented commands (`src/README.md`,
`carrier/gen/LIB_BINDINGS_NOTES.md`); `git diff --stat` confirms only
`src/icytower/allegro_types.h`, `src/icytower/allegro_api.h`,
`carrier/gen/pf_lib_bindings.h`, `carrier/gen/pf_lib_bindings_types.h`
(the last two: 1-line generation-timestamp diff only) changed --
`src/icytower/GENERATED.md` and `carrier/gen/LIB_BINDINGS_NOTES.md` are
byte-identical to before, and no `.c` file anywhere changed.

### Why the CRT shim (`it_orig_size_t` et al.) is unconditional

`compute_type_origin()` puts a type in the `'library'` bucket (which
`allegro_types.h` owns) whenever it is *also* declared outside the game
CUs -- true of Allegro's own public types, but equally true of CRT types
the game shares with libpng/zlib/etc. (`it_orig_size_t` is `size_t` under
its purity/collision-safe rename, used by `game_types.h`'s
`CSVParseContext.iDocSize`). Upstream `<allegro.h>` was never the source of
these names (it just uses plain `size_t`/`FILE`/`time_t` itself), so
skipping them under `ICYTOWER_UPSTREAM_ALLEGRO` the same way the Allegro-own
types are skipped left `game_types.h` referencing an undefined type
(`unknown type name 'it_orig_size_t'` -- caught by the first compile
attempt, see below). Fix: split `'library'`-origin types into the
CRT-reserved subset (always emitted) and everything else (skipped under
`ICYTOWER_UPSTREAM_ALLEGRO`, since that part genuinely does come from
`<allegro.h>`).

## Duplicate/prototype conflicts anticipated by the task, and what actually happened

- **`BITMAP`/`PALETTE`/`RGB`/`DATAFILE`/`PACKFILE`/`SAMPLE`/`FONT`/
  `JOYSTICK_INFO`/`fixed`/... duplicate type definitions**: avoided by
  construction -- `allegro_types.h`'s stand-in bodies are entirely skipped
  under `ICYTOWER_UPSTREAM_ALLEGRO`, so `<allegro.h>` is the only definer.
  No struct-redefinition error occurred once the CRT-shim gap (above) was
  fixed.
- **`install_int` (`AL_FUNC`) / other prototype conflicts**: avoided the
  same way -- `allegro_api.h`'s 100 stand-in prototypes are skipped
  entirely under the same guard, so upstream's `AL_FUNC`-declared
  signatures are the only ones the compiler ever sees. No conflicting-types
  error occurred for any of the 100.
- **`key`/`screen`/`font` declarations**: same skip; upstream's real
  `extern volatile char key[]` (`allegro/keyboard.h`, an unsized `AL_ARRAY`,
  not the stand-in's sized `key[127]`) is what `standalone_smoke.c` actually
  pokes -- see "`sizeof(key)` on an unsized array" below for the one thing
  that tripped on this.
- **`KEY_*`/`GFX_*`/`DRAW_MODE_*`/`MASK_COLOR_*` constant collisions**:
  avoided the same way -- `allegro_api.h`'s 150 hand-transcribed `#define`s
  are skipped entirely under `ICYTOWER_UPSTREAM_ALLEGRO`, so upstream's own
  `KEY_A = __allegro_KEY_A` (enum-backed) etc. are the only definitions.
  No macro-redefinition warning occurred.
- **`it_orig_size_t` undefined type**: not anticipated by name in the task
  brief, but the same category of problem -- see "Why the CRT shim" above;
  this was the one real, generator-level fix this pass needed.
- **`main` macro collision**: `allegro/platform/alwin.h` `#define`s `main`
  to `_mangled_main` (Allegro's WinMain-trampoline convention). Harmless
  for `game_funcs.h`'s own DWARF-recovered `_mangled_main` **prototype**
  (the original binary really has a function by that exact name, for the
  same reason -- it too was built against an Allegro that does this), but
  `standalone_smoke.c`'s own `int main(void)` collided with it (conflicting
  parameter lists). This is a test-harness-only concern, not a
  `src/icytower/` one: fixed with `#undef main` in `standalone_smoke.c`
  after the includes, before defining `main`.
- **`sizeof(key)` on an unsized array**: `AL_ARRAY` expands to
  `extern type name[]` (no bound in the header -- the real bound is
  `KEY_MAX+1` in `liballeg.a`'s actual data segment), so
  `memset(key, 0, sizeof(key))` doesn't compile against upstream the way it
  would against the stand-in's sized `key[127]`. Fixed in
  `standalone_smoke.c` (test-harness code, not `src/icytower/`) with an
  explicit `for (i = 0; i <= KEY_MAX; i++) key[i] = 0;` loop.

## Do the two worlds differ in anything other than the include path?

**No**, with the two additive exceptions logged above (both in generated
headers `src/icytower/*.c` never references directly):

1. `allegro_types.h` now emits a small always-on CRT-reserved-name shim
   inside the `ICYTOWER_UPSTREAM_ALLEGRO` branch, absent from the plain
   stand-in branch (which doesn't need it -- it defines everything itself).
   This is not a semantic difference for any game code: the same names,
   same layouts, same values either way.
2. `allegro_api.h`'s upstream branch includes `<logg.h>` where the stand-in
   branch declares `logg_load`/`logg_load_memory` by hand; both give the
   same two prototypes.

No `src/icytower/*.c` file differs in one byte between the two builds
(`git diff --stat` on this session's changes touches zero `.c` files under
`src/icytower/`); the swap is exactly the one compiler flag,
`-DICYTOWER_UPSTREAM_ALLEGRO`, the task asked for.

## Regression checks

```
python scripts/check_native_layer.py
  -> check_native_layer: scanned 14 file(s) under .../src, 0 violation(s)

cl /nologo /c /W3 /TC /Isrc\icytower src\icytower\update_frame.c src\icytower\is_solid.c
   src\icytower\jump_player.c src\icytower\map.c src\icytower\add_combo.c
   src\icytower\line_intersect.c src\icytower\control.c src\icytower\state.c
  -> 0 errors, 0 warnings (standalone stand-in world, unaffected by this change)

cl /nologo /c /W3 /TC /Icarrier\gen /FIpf_bindings_src.h
   src\icytower\update_frame.c src\icytower\is_solid.c src\icytower\jump_player.c
   src\icytower\map.c src\icytower\add_combo.c src\icytower\line_intersect.c
   src\icytower\control.c
  -> 0 errors, 0 warnings (carrier world, unaffected by this change)
```

Neither `game_types.h`/`game_state.h`/`game_funcs.h`/`state.c` nor any
`.c` file changed, so both baselines above are unaffected by construction;
run anyway as the task's explicit regression gate, both clean.
