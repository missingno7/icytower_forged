
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
| original bytes recovered (offline-verified) | 8518 | 14007 |

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
