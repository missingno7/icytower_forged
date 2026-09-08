
## Batch 14 (2026-09-08 -- play()'s coastline: the game-over, replay and profile halves)

Task: promote the functions still ORIGINAL on `play()`'s wider coastline
-- batch 13's closing paragraph named nineteen, "all of them in the
game-over, replay-menu and high-score halves".  **Thirteen of the
nineteen are promoted here**, plus three more that came with them
(`get_rank`, `hash`, `calc_replay_checksum_131`): sixteen functions,
5208 original bytes, verified by two new standalone oracles.

One correction to that list of nineteen, made by looking the names up in
`artifacts/functions.json` rather than assuming: **`file_exists` is not a
game function at all.**  It is Allegro's own
(`C:\Lib\allegro4\src\file.c`, 0x446150, 197 bytes), the same class as
`exists` / `delete_file` / `pack_fopen`, and belongs on the library side
of the ledger.  `save_profile` below calls it; nothing recovers it.  So
the nineteen were really eighteen game functions, and **five remain**.

### Part 1 -- the pure and memory-domain half

`carrier/lift/harness/batch14_check.py` + `batch14_check.c` (new): a
standalone oracle on the shared `pf_win32_offline_oracle` engine, driven
by a BINARY vector file (the payloads are raw structure images -- a
2220-byte `Treplay`, a 180-byte `Thisc[5]` -- which the '|'-separated
text protocol batch 13 used cannot carry).  **20000 vectors per function
per seed over four seeds (20260908, 1, 777, 424242) = 80000 each.**

| function | VA | size | CU | offline result | domain | carrier bind |
|---|---|---:|---|---|---|---|
| `qualify_hisc_table` | 0x404994 | 39 | hisc.c | **EQUAL** (80000) | return value + the whole 180-byte `Thisc posts[5]` | pending |
| `sort_hisc_table` | 0x4049bc | 147 | hisc.c | **EQUAL** (80000) | the whole 180-byte `Thisc posts[5]` | pending |
| `enter_hisc_table` | 0x405790 | 136 | hisc.c | **EQUAL** (80000) | " (its `strcpy` tail call emulated on the unicorn side) | pending |
| `get_rank_id` | 0x418a84 | 76 | profile.c | **EQUAL** (80000) | return value | pending |
| `get_rank` | 0x418ad0 | 82 | profile.c | **EQUAL** (80000) | the returned STRING's CONTENT (batch 13's `get_version_str` convention) | pending |
| `hash` | 0x41b9c8 | 71 | replay.c | **EQUAL** (80000) | return value | pending |
| `calc_replay_checksum_131` | 0x41ba10 | 177 | replay.c | **EQUAL** (80000) | return value | pending |
| `calc_replay_checksum` | 0x41bac4 | 676 | replay.c | **EQUAL** (80000) | return value | pending |

Negative control, all seven vector kinds (`batch14_check.py --fault`),
each detected and each named:

```
qualify_hisc_table         1 of 40   (ret 1FAULT vs ret 1)
sort_hisc_table            1 of 40   (posts 2020...969800FAULT)
enter_hisc_table           1 of 40   (posts 01ff7f...b70100FAULT)
get_rank                   1 of 40   (rank_id 0FAULT vs rank_id 0)
hash                       1 of 40   (ret 2137970045FAULT)
calc_replay_checksum_131   1 of 40   (ret -1452276412FAULT)
calc_replay_checksum       1 of 40   (ret 1805878246FAULT)
```

### Part 2 -- the ordered-call-trace half

`carrier/lift/harness/batch14b_check.py` + `batch14b_check.c` +
`pf_harness_batch14.h` (new, all built by `build_batch14.sh`).  The
unicorn side hooks every library, CRT and unpromoted game callee at its
own VA and appends one record per call, IN ORDER, with its arguments;
the compiled side stubs the same callees and prints the same records.
`pack_fwrite` / `fwrite` buffers are rendered as the HEX OF THE BYTES
they would write, because for `save_replay` and `save_profile` the
sequence of those buffers IS the file.

| function | VA | size | CU | offline result | domain | carrier bind |
|---|---|---:|---|---|---|---|
| `destroy_replay` | 0x41bd68 | 54 | replay.c | **EQUAL** (80000) | ordered trace of `free` | pending |
| `myDeleteFile` | 0x40cd28 | 63 | config.c | **EQUAL** (80000) | ordered trace of `sprintf` / `delete_file` | pending |
| `save_config` | 0x40e130 | 154 | config.c | **EQUAL** (80000) | ordered trace of `log2file` / `get_configfile_path` / `pack_fopen` / `save_options` / 15 x `save_hisc_table` / `pack_fclose` | pending |
| `init_scroller` | 0x41f278 | 200 | scroller.c | **EQUAL** (80000) | ordered trace of `text_height` / `text_length`, plus the whole `Tscroller` (its `lines[]` compared as OFFSETS into the text) and the text buffer it rewrites in place | pending |
| `fadeOut` | 0x40bf5c | 609 | fade.c | **EQUAL** (4800) | ordered trace of `create_bitmap` / `blit` / `draw_sprite` / `set_trans_blender` / `drawing_mode` / `makecol` / `rectfill` / `solid_mode` / `blit_to_screen` / `rest` / `destroy_bitmap` | pending |
| `fadeIn` | 0x40c1c0 | 424 | fade.c | **EQUAL** (4800) | " | pending |
| `save_replay` | 0x41dd78 | 1227 | replay.c | **EQUAL** (1600) | ordered trace of `sprintf` / `log2file` / `time` / `localtime` / `load_replay` / `pack_fopen` / ~525 x `pack_fwrite` WITH THE BYTES / `pack_fclose`, plus the return value and `Treplay.date` / `.size` / `.checksum` | pending |
| `save_profile` | 0x41a3b8 | 1073 | profile.c | **EQUAL** (20000) | ordered trace of `get_profile_dir_for_profile` / `file_exists` / `mkdir` / `sprintf` / `time` / `localtime` / `generate_profile_checksum` / `fopen` / `fwrite` WITH THE BYTES / `get_controls` / `save_control` / `fclose` / `fprintf` / the four `profile_data_page_*` / `fputs` / `fputc` / `free` / `log2file`, plus `Tprofile.checksum` and `.saveDate` | pending |

**Vector budget, stated rather than implied.**  These eight cost between
2 and ~550 traced calls per vector, so `--vectors N` is split by an
explicit WEIGHT table instead of evenly, and every result line prints the
count it actually ran.  Per seed: 20000 each for `destroy_replay`,
`myDeleteFile`, `save_config` and `init_scroller`; 5000 for
`save_profile`; 1200 each for the two fades; 400 for `save_replay`.
Four seeds.  Total across both parts: **911 200 vectors, differ 0.**

Negative control, all eight (`batch14b_check.py --fault`), each detected
and each named:

```
destroy_replay   1 of 20   (free records)
myDeleteFile     1 of 20   (sprintf "%s%s" -> "replays/x")
save_config      1 of 20   (log2file "  saving config and scores")
init_scroller    1 of 20   (text_height font)
fadeOut          1 of 20   (create_bitmap 0 1753)
fadeIn           1 of 20   (create_bitmap 640 600)
save_replay      1 of 20   (sprintf "%s%s" -> "a/")
save_profile     1 of 20   (get_profile_dir_for_profile 1024 "")
```

### Findings

**1. The `.itr` date field carries a hidden watermark, and it is inside
the checksum.**  `save_replay` copies the 31-byte literal
`"              ICYTOWERISGREAT "` into `Treplay.date` (0x41ddfe, a
`rep movsb`) and then immediately `sprintf`s over the same buffer
(0x41de2a).  That reads as a dead store and is not one: the sprintf
format is `"%2d %3s %4d"`, exactly 11 characters plus a NUL, so bytes
12..30 of the 32-byte field keep `"  ICYTOWERISGREAT "` -- and
`calc_replay_checksum` hashes all 32 bytes of `date`.  A `.itr` whose
date field was rebuilt by anything that does not know about the tag fails
the integrity check.  This is the one apparently-dead store in the batch
that had to be reproduced exactly, and `replay.c` says so at the site.

**2. `save_replay`'s on-disk order is NOT the struct order.**
`notes/replay_format.md` SS4 stopped its trace at the fourth field and
recorded the rest as INFERRED, "a near-verbatim `Treplay`
serialization".  Finishing the trace shows three places where that
inference is wrong, all unambiguous in the object code: `checksum`
(offset 0x4c) is written near the END, after `comment` (0xa8), not in
struct position; the five 100-float statistics columns are written
INTERLEAVED BY INDEX (`c q t s f`, `c q t s f`, ...) as 500 separate
4-byte writes, not as five arrays; and each `Trecord` is written REVERSED
and SHORT -- `cycle_count` (4 bytes) first, then `key_flags` (1 byte),
five bytes on disk against the struct's eight.  The `ccc[5]` and `jc[5]`
arrays are likewise ten separate 4-byte writes.  Any reader built from
the struct layout alone would desynchronise at the first record.

**3. `calc_replay_checksum` hashes only three of the five statistics
columns, and its accumulator is provably `unsigned`.**  `tc_s_data` and
`tc_f_data` -- 800 bytes of every replay's tail -- are written by
`save_replay` and never hashed.  The three that are hashed go through the
one x87 section in this batch, and its C shape is pinned by two details
that would otherwise look like noise: the 64-bit slot fed to `fildll`
has its HIGH dword explicitly zeroed (so the accumulator is widened as an
UNSIGNED 32-bit value, not sign-extended), and the `fistpll` +
take-low-dword pair is GCC's idiom for `(unsigned int)<floating
expression>`.  Together they force
`sum = (unsigned)(sum + col[i] * ((i + k) % m));` as three separate
statements -- fusing them, or making `sum` an `int`, moves the truncation
points and changes the result.

**4. The screen-size null guard in both fades is `SCREEN_W`/`SCREEN_H`,
not a hand-written check.**  Six times across `fadeIn`/`fadeOut` the
object code loads `gfx_driver` (0x4dda84), tests it, and substitutes 0
for the size.  That is Allegro's own
`#define SCREEN_W (gfx_driver ? gfx_driver->w : 0)` expanding in place --
identified by the shape repeating identically in pairs, and confirmed by
counting down `GFX_DRIVER` to find `w`/`h` at +0x6c/+0x70.  Recovering it
as a defensive `if (gfx_driver)` would have been observationally
identical and wrong about the source.

**5. `fadeOut`'s last draw goes to `screen`, not `swap_screen`.**
Everything inside its animation loop targets the game's own
`swap_screen` (0x4dd194) and reaches the display through
`blit_to_screen`; the final black `rectfill` after `destroy_bitmap`
targets Allegro's `screen` (0x4dda8c) directly (0x40c148 reloads it).
The asymmetry is in the object code, and it leaves the visible
framebuffer black while the game's own buffer still holds the last
blended frame.  `fadeIn` never touches `swap_screen` at all.

**6. `enter_hisc_table` does not insert -- it overwrites the last
"candidate" slot, and the search is subtler than it looks.**  A running
threshold starts at 10 000 000 (the score ceiling); an entry qualifies
only if it is strictly below the running threshold, and qualifying lowers
the threshold to that entry's own value; the LAST qualifier wins.  On a
descending-sorted table that selects the lowest score; on a partly-filled
one (trailing zeroes) it selects the first free slot.  Both behaviours
fall out of the same three lines, and `sort_hisc_table` -- a stable
insertion sort, descending, unsigned -- does the reordering afterwards.

**7. `save_profile` writes two files and `sprintf`s a buffer onto
itself.**  `sprintf(path, "%s%s.itp", path, p->handle)` (0x41a4a1, and
again for `_stats.txt` at 0x41a5c0) is undefined by the letter of
C99 7.19.6.6 and is what the original does; it is kept, and GCC's
`-Wrestrict` / `-Wformat-overflow` warnings on those two lines are the
recovered code correctly reproducing it, not a defect introduced here.
The stats file's own header literal says `ICY TOWER 1.4 PROFILE` in a
1.5.1 binary.

### Two carrier-world blockers closed, by request

`carrier/win32_policy.json`'s `scan_exclude` had grown two TEMPORARY
entries -- `profile.c` and `replay.c` -- with a note from the concurrent
carrier task asking batch 14 to fix both in `src/` itself and then remove
them.  Both are fixed:

- **`replay.c`** gains the `#ifdef <name>` / `#undef <name>` idiom
  `handle_player_input.c` and `draw_frame.c` already use, for the THREE
  `Treplay` / `Trecord` member names that are also bound top-level
  globals: `data` (vs `DATAFILE *data` @0x4dd23c), `rejump` (vs
  `int rejump` @0x4fdcd8) and `cycle_count` (vs
  `volatile int cycle_count` @0x506938).  Dropping all three for this
  translation unit is right on the merits, not just expedient: replay.c
  must never touch the datafile (that is ASSETS.md's seam), the live
  difficulty global (the replay carries its own copy -- that is the point
  of the field) or the tick counter (play()'s pacing).
- **`profile.c`** renames `get_rank` / `get_rank_id`'s parameter to
  `profile_arg`, matching `win32_policy.json`'s own `param_renames` and
  the prototypes `game_funcs.h` already carries.

Both files now compile clean in the carrier world (0 errors), so the two
`scan_exclude` entries can be removed.  This pass does not edit
`carrier/win32_policy.json` -- that file is the carrier task's.

### Generator gaps found (reported, not fixed)

1. **`MEMBER_ACCESS_COLLISIONS` again, two more names.**  The root cause
   is the one `gen_bindings.py`'s own comment already describes: a
   global's binding is a blunt textual `#define`, so any STRUCT MEMBER
   with the same name becomes a syntax error.  This batch adds `rejump`
   to the collision list (`data` and `cycle_count` were already known
   from `handle_player_input.c`), and the workaround is still a per-file
   `#undef`.  All of them would disappear under a context-sensitive
   rewrite.
2. **`scan_src_defs.py`'s file-level exclusion is invisible to
   `--exclude`.**  A file listed in `scan_exclude` contributes NO
   function names to `gen_bindings.py --exclude`, so every function it
   defines stays bound to its original address while `src/` also defines
   it -- a silent wrong-target hazard rather than a build error.  It is
   the right tool for `state.c` (which defines nothing) and a sharp edge
   for anything else; the carrier task's own note in `win32_policy.json`
   reasons about exactly this and chose it deliberately as a temporary
   measure.  Worth a warning in the scanner: "file X is scan-excluded but
   defines N functions".
3. **`game_funcs.h` types `save_control`'s second parameter as
   `it_orig_FILE *`** (the MSVC-shaped `FILE` reconstructed from DWARF),
   while `<stdio.h>`'s `fopen` returns the host `FILE *`.  In a build
   whose CRT is not that one the two are distinct types for the same
   pointer value, so `profile.c` carries an explicit cast at the single
   call site.  A generator that emitted the CRT handle types as an opaque
   `void *`, or that shipped the alias the harness gets from
   `pf_harness_msvc_types.h`, would remove the need.
4. **`build_batch13.sh`'s `-I.` no longer finds
   `pf_harness_msvc_types.h`.**  That header moved out of
   `carrier/lift/harness/` into `port_forge/tools/win32_oracle/` (it is
   in this working tree's deleted-file list), so any GCC harness build
   that still relies on `-I.` to reach it fails on
   `typedef unsigned __int64 uint64_t;`.  `build_batch14.sh` points at
   the new location explicitly and says why.

### One environment note worth recording

The MinGW32 GCC in this tree fails **silently** -- exit 1, no diagnostic
at all -- unless `C:\msys64\mingw32\bin` is on `PATH`, even when `gcc` is
invoked by absolute path: `cc1.exe` lives under `lib/gcc/...` and loads
its DLLs from `bin/`, so without it Windows refuses the image with
`STATUS_INVALID_IMAGE_FORMAT` (0xC000007B) and the driver reports
nothing at all.  `build_batch14.sh` sets it and carries the explanation.

### Target 2 (`init_game`) -- scoped, not recovered

`init_game` (0x40e7dc, 5788 bytes) was mapped but not attempted, and the
map is the deliverable: `artifacts/init_game_callmap.txt` (new) is its
complete ORDERED call sequence, produced mechanically from
`artifacts/disasm.txt` -- **243 direct call sites over 68 distinct
callees**, with a per-callee census.  What the map shows:

- The startup sequence really is "the call sequence IS its effect": 65 of
  the 243 calls are `log2file` and 15 are `draw_progress_bar`, i.e. a
  quarter of the function is its own progress narration, and a further 12
  `allegro_message` calls (each preceded by
  `set_gfx_mode(GFX_TEXT, 0, 0, 0)`) are its twelve distinct failure
  exits.  Only ONE `set_gfx_mode` of the fifteen sets a real mode
  (`GFX_AUTODETECT_WINDOWED`, 640x480, at 0x40ed43; a second at 0x40f522
  retries in mode 2).
- It reaches **at least twenty game functions that are still ORIGINAL** --
  `load_options`, `load_hisc_table`, `make_hisc_table`,
  `reset_hisc_table`, `reset_options`, `load_profile`, `create_profile`,
  `rebuild_profile_list`, `syncOptionsFromProfile`, `check_characters`,
  `get_profiles_dir`, `get_gamepad_value`, `pwd_garble_string`,
  `getSampleFromOggDatafile` (22 calls), `fldads_start`,
  `draw_progress_bar`, `install_timers`, `get_extension`, `get_filename`,
  `set_config_file` -- every one of which an ordered-trace oracle would
  have to stub.  That, not the branch structure, is the size of the job:
  it is a batch of its own, and it is the natural next one.
- Its asset half needs no new work: the three `packfile_password` /
  `load_datafile` pairs at 0x40ee0a / 0x40f1aa / 0x40f95b load
  `data/loading.dat` (password `"(c) Free Lunch Design"`),
  `data/data.dat` and `data/sfx15.dat` (both keyed from the obfuscated
  `init_string` @0x4bdb3c), which is exactly `notes/asset_census.md`
  SS5's table -- checked against the call map, nothing new found and
  nothing contradicted.

**Target 3 (the menu screens) was not reached.**
`main_menu_callback` (3741 B), `select_profile` (3070 B), `view_scores`
(2552 B) and `key_to_str` (2543 B) are untouched; no budget remained
after target 1.

### What is still ORIGINAL on play()'s coastline

Batch 13's list of nineteen, re-checked name by name:

| callee | state |
|---|---|
| `calc_replay_checksum`, `destroy_replay`, `enter_hisc_table`, `fadeIn`, `fadeOut`, `get_rank_id`, `init_scroller`, `myDeleteFile`, `qualify_hisc_table`, `save_config`, `save_profile`, `save_replay`, `sort_hisc_table` | **promoted (batch 14)** |
| `file_exists` | **not a game function** -- Allegro's own (0x446150, `C:\Lib\allegro4\src\file.c`); a correction to batch 13's list |
| `do_replay_menu` (0x410f98, 2661 B), `draw_results` (0x4076c0, 839 B), `getGameDataXML` (0x404254, 1855 B), `load_replay` (0x41cde8, 1136 B), `my_alert` (0x40cd68, 1770 B) | **still ORIGINAL** -- 7461 bytes, the whole remainder |

`load_replay` is `save_replay`'s exact inverse and is the obvious next
one now that the on-disk order above is known; `draw_results` is a
drawing function of the `draw_frame` family; `do_replay_menu` and
`my_alert` are interactive loops (`readkey`/`keypressed` plus a redraw),
the same shape the menu screens have.

### Purity gate (batch 14)

```
python scripts/check_native_layer.py
pf_native_purity: scanned 52 file(s) under .../src, 0 violation(s)
```

### Compile (all three worlds, batch 14)

The six files this batch touches (`hisc.c`, `replay.c`, `profile.c`,
`config.c`, `fade.c`, `scroller.c`): **0 errors in all three worlds.**

```
standalone (generated allegro_api.h, no bindings):
  gcc -m32 -mfpmath=387 -mno-sse -mno-sse2 -O2 -Wall -Isrc/icytower \
      -Iport_forge/tools/win32_oracle \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c src/icytower/{hisc,replay,profile,config,fade,scroller}.c
  -- 0 errors; warnings only on profile.c's two self-aliasing sprintf
     calls (finding 7 above -- the original's own behaviour)

standalone (upstream Allegro, real <allegro.h>):
  gcc -m32 -mfpmath=387 -Wall -DICYTOWER_UPSTREAM_ALLEGRO -DALLEGRO_STATICLINK \
      -Ithird_party/allegro-4.4.3.1/include \
      -Ithird_party/build-allegro-4.4.3.1/include \
      -Ithird_party/allegro-4.4.3.1/addons/logg -Isrc/icytower \
      -c <the same six>
  -- 0 errors, same two warnings (fade.c's SCREEN_W/SCREEN_H/rectfill/
     draw_sprite stand-ins are inside the #ifndef ICYTOWER_UPSTREAM_ALLEGRO
     block draw_frame.c already established, so the real macros win here)

carrier (scratch bindings, GCC -- carrier/gen is owned by the concurrent
carrier task and is NOT touched; the generator writes to a scratch dir):
  python carrier/gen/scan_src_defs.py --src-dir src/icytower
  python carrier/gen/gen_bindings.py \
      --exclude <scanned + hash,calc_replay_checksum,calc_replay_checksum_131,
                 destroy_replay,save_replay,get_rank_id,get_rank,save_profile,
                 floor_size_modifiers> \
      --guard-define ICYTOWER_BINDINGS_ACTIVE \
      --out <SCRATCH>/pf_bindings_src.h --types-out <SCRATCH>/pf_bindings_src_types.h
  gcc -m32 -Wall -DICYTOWER_BINDINGS_ACTIVE -Icarrier/gen -I<SCRATCH> -Isrc/icytower \
      -include <SCRATCH>/pf_bindings_src.h \
      -include carrier/gen/pf_lib_bindings.h \
      -include carrier/gen/pf_asset_bindings.h \
      -include port_forge/tools/win32_oracle/pf_harness_msvc_types.h \
      -c <every src/icytower/*.c>
  -- the six batch-14 files: 0 errors.  Across the whole directory the
     only failures are still draw_star_field.c (batch 8's documented
     `stars` MEMBER_ACCESS_COLLISIONS gap) and the three
     standalone-world fixtures assets_standalone.c / state.c /
     game_types_check.c, which are not part of a carrier build -- the
     same set batch 13 reported, so no regression.
  (The eight new names had to be added to --exclude BY HAND: scan_src_defs.py
   skips profile.c and replay.c entirely, per gap 2 above.)

offline oracles:
  ./carrier/lift/harness/build_batch14.sh
  python carrier/lift/harness/batch14_check.py  --vectors 140000 --seed <s>
  python carrier/lift/harness/batch14_check.py  --fault
  python carrier/lift/harness/batch14b_check.py --vectors 160000 --seed <s>
  python carrier/lift/harness/batch14b_check.py --fault
  -- part 1: 7 kinds (8 functions) x 20000 x 4 seeds = 560000, differ 0
  -- part 2: 8 functions, weight-split, x 4 seeds    = 351200, differ 0
  -- all fifteen faults detected
```

`icytower_specs.py` is untouched again -- the fifth batch running that
way -- and so is `lift_check.py`; both new oracles are standalone
executables on the shared engine.

### In vivo (for the carrier task -- NOT run by this pass)

This pass did not run `carrier.exe`.  Nothing in this batch is on the
gameplay tick path, so none of it can be exercised by
`replays/human_test.txt` alone: every function here runs after the tick
loop exits, or in the menus.  The natural sequencing:

```
rem 0. unbound baseline first, as always.
carrier.exe --replay replays\human_test.txt --frame-digest

rem 1. the pure ones -- no new link blocker, no filesystem effect.
carrier.exe --bind qualify_hisc_table=src,sort_hisc_table=src, ^
                   enter_hisc_table=src,get_rank_id=src,get_rank=src, ^
                   hash=src,calc_replay_checksum=src, ^
                   calc_replay_checksum_131=src ^
            --replay replays\human_test.txt --frame-digest

rem 2. the fades and the scroller -- these DO run at game over, so the
rem    digest must still match for all 2293 ticks and the run must still
rem    end on the score 2386 / floor 100 witness.
carrier.exe --bind fadeIn=src,fadeOut=src,init_scroller=src ^
            --replay replays\human_test.txt --frame-digest

rem 3. the file writers.  A bind that is never entered is not evidence
rem    (the standing rule), and the human_test replay never saves a
rem    profile or a replay -- so these need a workload that does, plus a
rem    BYTE COMPARISON of the produced .itr / .itp / _stats.txt /
rem    tower.cfg against the same files from an unbound run.  That
rem    comparison is the in-vivo half the offline oracle structurally
rem    cannot do: it proves which calls happen with which bytes, not that
rem    the bytes reach the disk.
```

Three things the offline oracle structurally cannot see here:

1. **`save_replay` / `save_profile` / `save_config` really touch the
   filesystem.**  The oracle proves the call sequence and the bytes; that
   a `.itr`, a `.itp` and a `tower.cfg` actually appear, and compare
   equal, is an in-vivo fact.
2. **`pack_fopen`'s mode string is load-bearing.**  `save_config` uses
   `"wp"` (Allegro's PACKED write -- `tower.cfg` is LZSS compressed) and
   `save_replay` uses `"wb"`.  Offline both are just argument strings; in
   vivo the difference is a different file format on disk.
3. **The fades block on `cycle_count`.**  Their
   `while (cycle_count <= 0) rest(2);` spin is driven by the real 20 ms
   timer in vivo and by a scripted counter offline, so pacing is not
   under offline test at all.

### Totals (updated)

| | batch 14 (this pass) | cumulative (14 passes) |
|---|---:|---:|
| functions promoted (offline-verified) | 16 | 75 |
| functions promoted (partially offline-verified, in-vivo-pending) | 0 | 1 (`play`) |
| functions promoted (compile-only) | 0 | 2 (`draw_buffer`, `draw_star_field`) |
| functions skipped (documented, all passes) | 5 (`do_replay_menu`, `draw_results`, `getGameDataXML`, `load_replay`, `my_alert`) | 21 distinct |
| original bytes recovered as clean source | 5208 | 44542 |
| original bytes offline-verified | 5208 | 27624 |

Batch 14's sixteen: `qualify_hisc_table`, `sort_hisc_table`,
`enter_hisc_table` (hisc.c, 322 B); `get_rank_id`, `get_rank`,
`save_profile` (profile.c, 1231 B); `hash`, `calc_replay_checksum_131`,
`calc_replay_checksum`, `destroy_replay`, `save_replay` (replay.c,
2205 B); `save_config`, `myDeleteFile` (config.c, 217 B); `fadeOut`,
`fadeIn` (fade.c, 1033 B); `init_scroller` (scroller.c, 200 B).

File list this pass:
`src/icytower/{hisc,replay,profile,config,fade}.c` (new),
`src/icytower/scroller.c` (+`init_scroller`),
`carrier/lift/harness/{batch14_check.py,batch14_check.c,batch14b_check.py,batch14b_check.c,pf_harness_batch14.h,build_batch14.sh}`
(new), `artifacts/init_game_callmap.txt` (new),
`artifacts/src_equivalence.json` (16 entries).
