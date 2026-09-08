
## Batch 12 (2026-09-08 -- `play()`, the whole game loop)

Task: recover `play()` (0x411a00, 17420 bytes, main.c 3405-5021) as clean
source, in one pass, as ONE file bound at ONE address -- the sequencing
batch 11 argued for and could not do without reading the other 9347 bytes.

**One function promoted, 17420 bytes, the largest single function in the
image.**  `src/icytower/play.c`, 1741 lines: the outer tick loop, the
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

`play()` stays one function, as batch 11 required, with five `static`
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

- `carrier/lift/harness/play_xcheck.py` (new, 392 lines): the three-region
  oracle.
- `carrier/lift/harness/play_check.c` (new, 202 lines) and
  `play_check_stubs.c` (new, 110 lines): the compiled side.
- `carrier/lift/harness/play_callsite_census.py` (new, 178 lines): the
  completeness check.
- `src/icytower/play.c` (new, 1741 lines, 17420 original bytes).
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
`src/icytower/play.c` (new, 1741 lines, 17420 original bytes),
`carrier/lift/harness/{play_xcheck.py,play_check.c,play_check_stubs.c,play_callsite_census.py}`
(new), `src/icytower/ASSETS.md` (+3 resolved rows / the open paragraph),
`notes/binary_recon.md` (item l, the catch-up correction),
`artifacts/src_equivalence.json` (the `play` entry).
