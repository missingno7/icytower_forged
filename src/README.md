# `src/` — the clean port

Recovered semantics only, exactly as win32_pilot.md §7a defines this layer:
no PortForge type, no carrier header, no guest address. The carrier, the
lifter output (`carrier/lift/`) and the generated interop (`carrier/gen/`)
never live here.

```
src/icytower/allegro_types.h GENERATED (carrier/gen/gen_src_headers.py):
                             public library types the game CUs use by value
                             or pointer (BITMAP, DATAFILE, SAMPLE, FONT,
                             PACKFILE, RGB, PALETTE, JOYSTICK_INFO, fixed,
                             ...) -- Allegro's own, recovered here only so
                             this layer compiles without a carrier header;
                             a real Allegro dependency replaces this file
                             later (see its own header comment).
src/icytower/game_types.h   GENERATED (carrier/gen/gen_src_headers.py):
                             every struct/union/enum/typedef the game CUs
                             (F:\projects\icytower\trunk\source\*.c)
                             declare and that is reachable from a game-CU
                             global or function -- Tplayer, Tmap, Tfloor
                             and every other native game type -- reusing
                             carrier/gen/gen_interop.py's DWARF parser and
                             type IR so the layouts are the same DWARF
                             recovery it_types.h already proved correct
                             (INTEROP_NOTES.md), not a second transcription
                             of it. Layouts must stay binary-compatible
                             with the original process memory for as long
                             as state stays address-backed (see the file's
                             own header comment); this is the one place
                             that fact is recorded, because nothing else
                             here may mention it.
src/icytower/game_types_check.c GENERATED: standalone sizeof/offsetof
                             PASS/FAIL host program proving every
                             game_types.h/allegro_types.h layout against
                             the DWARF-reported one; touches no game
                             memory. `cl /W3 /TC game_types_check.c && 
                             game_types_check.exe` must print `ALL PASS`.
src/icytower/game_state.h   GENERATED: ordinary extern declarations of
                             every game-CU global (real DWARF type, array
                             bounds, volatile preserved), grouped by
                             originating source file, typed with
                             game_types.h. update_frame.c/is_solid.c today
                             only touch five of them (reward_time,
                             reward_scale, player_id, ply, logic_count).
src/icytower/game_funcs.h   GENERATED: prototypes of every game-CU
                             function, original names and DWARF parameter
                             names -- this layer's own header, never an
                             address.
src/icytower/state.c        GENERATED: zero-initialized storage for every
                             game_state.h extern, used only when this
                             layer is built OUTSIDE the carrier
                             (win32_pilot.md SS7a: "a state.c defines the
                             globals and the bindings header is absent")
src/icytower/update_frame.c  recovered game logic, one file per function
src/icytower/is_solid.c
src/icytower/GENERATED.md   generator report: counts, opaque types,
                             identifier/name collisions, purity-gate
                             renames -- see there for detail
```

The five generated files above are produced by
`carrier/gen/gen_src_headers.py` (which imports `carrier/gen/gen_interop.py`
as a module and reuses its DWARF parser/type IR rather than re-deriving it)
and carry a DO-NOT-EDIT header naming their inputs:

```
python carrier\gen\gen_src_headers.py --dwarf artifacts\dwarf_info.txt ^
    --functions artifacts\functions.json --out src\icytower --scope game
```

Re-run it whenever `artifacts/dwarf_info.txt` changes; do not hand-edit any
of its five outputs. See `src/icytower/GENERATED.md` for this run's counts
and `carrier/gen/gen_src_headers.py`'s own module docstring for how it
differs from `gen_interop.py`'s carrier-side output (no address, no
`IT_`/`PF_`-prefixed name, ever, in anything under `src/`).

## What may be in a file here

- the game's own logic, as ordinary C, with the original function and
  parameter names (`update_frame`, `is_solid(Tmap *m, int cx, int cy)`, …);
- globals used as plain identifiers (`reward_scale`, `ply[player_id]->x`),
  never as an address or a cast;
- `#include`s of other files in `src/icytower/`, and of the C standard
  library;
- prose comments explaining behaviour, carried over from the verified form
  this file was recovered from.

## What may not, enforced by `scripts/check_native_layer.py` at tier 0

- a guest address, in hex or decimal, anywhere a literal could resolve into
  the image, guest heap or guest stack ranges;
- an `#include` of anything under `carrier/`, `carrier/gen/`, or
  `port_forge`, or of `it_types.h`/`it_globals.h`/`it_funcs.h`/`pf_*.h` by
  name;
- an identifier starting with `PF_`, `pf_`, `IT_G_`, `IT_F_`, `PFN_` or
  `lifted_` — the carrier's and the lifter's vocabulary;
- inline asm.

Run it with `python scripts/check_native_layer.py` (defaults to scanning
this directory). Empty is a legitimate state and the gate reports it as
such rather than printing a pass.

## How this compiles into two worlds

The same `.c` file compiles unchanged whether it ends up running inside the
carrier, on the original game's memory, or standalone with its own storage
— win32_pilot.md SS7a's contract, proven first by
`carrier/gen/bindings_selftest.c` and now exercised here by
`update_frame.c`/`is_solid.c` themselves:

```
carrier world:
  cl /c /W3 /TC /Icarrier\gen /FIcarrier\gen\pf_bindings_src.h
     src\icytower\update_frame.c src\icytower\is_solid.c

standalone world:
  cl /c /W3 /TC /Isrc\icytower
     src\icytower\update_frame.c src\icytower\is_solid.c src\icytower\state.c
```

`pf_bindings_src.h` is `carrier/gen/pf_bindings.h` regenerated with the
functions defined in this directory passed to `--exclude` (so their own
names stay free instead of being redirected to their original address —
BINDINGS_NOTES.md), plus `--guard-define ICYTOWER_BINDINGS_ACTIVE`:

```
python carrier\gen\gen_bindings.py --exclude update_frame,is_solid ^
    --guard-define ICYTOWER_BINDINGS_ACTIVE ^
    --out carrier\gen\pf_bindings_src.h --types-out carrier\gen\pf_bindings_src_types.h
```

The default `carrier/gen/pf_bindings.h` (no `--exclude`) is left untouched
by this so it stays the complete, current binding table for everything not
yet promoted out of the carrier. `/FI` force-includes `pf_bindings_src.h`
before this directory's own tokens are preprocessed, so every plain name a
file here declares as an ordinary extern or calls directly is already a
macro expanding to the original address by the time the compiler sees it —
no address literal ever appears in the source text. `ICYTOWER_BINDINGS_ACTIVE`
is the purity-safe signal (an ordinary name, none of the banned prefixes
above) that `game_types.h`/`game_state.h` check to skip their own type/extern
declarations in that world, since `pf_bindings_src.h` already supplied
equivalent ones.

Standalone, no bindings header is force-included, so
`ICYTOWER_BINDINGS_ACTIVE` is undefined: `game_types.h` defines its own
struct layouts, `game_state.h`'s extern declarations are live, and
`state.c` gives them storage. Both builds were verified with `cl /c /W3
/TC` (0 errors, 0 warnings): see `artifacts/src_equivalence.json` §compile.

## Offline verification before anything is bound

`carrier/lift/harness/` extends the same offline oracle used for the LIFTED
and NATIVE forms (`carrier/lift/README.md` §7, `carrier/native/README.md`
§4) to this layer: `lift_check.py --form src` builds
`harness/src_check.exe` from these files with `carrier/gen/
pf_bindings_harness.h` (the `--mem-macro PF_MEM` twin of
`pf_bindings_src.h`, generated the same way) force-included, so every game
global resolves through `PF_MEM()` into an in-process copy of the image
instead of the real address range, and runs it against the original bytes
in unicorn — 20 000 vectors per function, both EQUAL, plus a one-shot
negative control per function that the comparator names exactly. Results:
`artifacts/src_equivalence.json`.
