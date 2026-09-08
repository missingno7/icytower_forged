
## Divergence 011 - promoted src/ code reached the CARRIER's C library instead of the GUEST's, and three facilities of shared state split in half (2026-09-08)

The follow-up to batch 13's flagged "run-to-exit hang/qualification" item.
It is not a bug in any of PROMOTIONS.md batch 14's sixteen functions. All
sixteen are correct as recovered; the defect is one layer down, in this
carrier's own generator, and it is divergence 008's mechanism arriving at
three much larger facilities at once.

### 0. What the baseline actually had to be

The task's own recipe -- "bind every row EXCEPT batch 14's sixteen" -- is
not expressible with a bindfile, for divergence 010's reason: `play=src`
routes every promoted callee of `play()` through the LINKER, whether or
not that callee has a row. MEASURED, first run of this pass:
`--bind-file all_rows_src.bindfile` (which then had no batch-14 row at
all) still crashed inside batch-14 code. So the baseline is the fully
UNBOUND run, and the isolating experiment is `play=original` plus ONE
batch-14 function (or one file's worth) -- an ORIGINAL caller reaches a
bound callee through the guest VA and `bind.cpp`'s `jmp` patch, which is
the one direction the linker cannot short-circuit.

Baseline, `replays/human_test.txt` run to the game's own exit
(`--stop-at-tick 3200`, the recording's own post-game high-score entry
runs to input-script tick 3047), fully unbound: **2293 ticks, digest
EQUAL to `replays/human_test.digest`, `last_game.itr` score=2386
floor=100, `log.txt` reaches `Done...`**, exit 0. Reproduced twice.

### 1. The `.itr` witness needed one masked column before it could be an oracle

`save_replay`'s in-vivo oracle is the FILE (the offline oracle proves the
call trace and the bytes handed to each `pack_fwrite`; that they reach the
disk is an in-vivo fact). Calibrating it first, on two UNBOUND runs back
to back, found the file is not bit-stable on this host: **exactly six
bytes differ, all inside `tc_s_data[0]` and `tc_s_data[1]`, and nothing
else.**

That column is `play.c` 3641, `demo->tc_s_data[n] = 50.0 * accMusics /
totMusics`, and `accMusics` accumulates
`voice_get_position(checkMusicVoiceID)` -- the playback cursor of a real
DirectSound voice, i.e. the host's audio clock, which no carrier wrapper
pins (divergence 009 normalized the device COUNT, not the cursor). It is
pre-existing nondeterminism in the ORIGINAL machine code, and it is by
construction outside the integrity check: `calc_replay_checksum` hashes
`tc_c_data`/`tc_q_data`/`tc_t_data` and **not** `tc_s_data`/`tc_f_data`
(PROMOTIONS.md batch 14 finding 3).

New tool: `carrier/scripts/compare_itr.py`, which knows `save_replay`'s
on-disk write order (not the struct order), masks exactly that column,
compares everything else byte for byte -- the checksum field included --
and names the field of the first difference. `--no-mask` reproduces the
six-byte host-audio delta as a calibration control.

### 2. Bisection: two of the sixteen crash, and both crash the same way

Per group, `play=original`, run to exit:

| bound | result |
|---|---|
| `hisc.c` (qualify/sort/enter_hisc_table) | clean |
| `replay.c` (hash, calc_replay_checksum(_131), destroy_replay, save_replay) | **STATUS_HEAP_CORRUPTION 0xc0000374** |
| `profile.c`+`config.c` (get_rank, get_rank_id, save_profile, save_config, myDeleteFile) | **STATUS_HEAP_CORRUPTION 0xc0000374** |
| `fade.c` (fadeIn, fadeOut) | clean |
| `scroller.c` (init_scroller) | clean |

Down to single functions: `destroy_replay=src` alone crashes;
`save_profile=src` alone crashes; `save_replay`, `calc_replay_checksum`,
`hash`, `save_config`, `myDeleteFile`, `get_rank`, `get_rank_id` alone do
not. Both crashes land at the identical point -- `assets/log.txt` ends at
`  saving replay: profiles/MissingNO/replays/last_game.itr` and never
reaches the next line, `  saving config and scores` -- with the same
stack: guest `_mangled_main` -> three frames in the 0x100xxxxx module ->
`ntdll` heap. **0x10000000 is `carrier.exe`'s own image base**
(`build.cmd`'s `/BASE:0x10000000`), so those three frames are the
carrier's own statically linked UCRT. Batch 13's report of a HANG at this
point was the same defect seen through a `--run-seconds` budget.

### 3. Root cause: `free()` in clean src/ was not the guest's `free()`

`carrier/gen/pf_bindings_src.h` binds game globals and game functions to
guest addresses, and `GUEST_CRT_IMPORTS` binds a hand-curated few CRT
names to the guest's own IAT slots (divergence 008 added `rand`/`srand`
for exactly this reason). Everything else in a `src/icytower/*.c` file
resolves the ordinary way: **the carrier's own linked-in C library.**

`malloc`/`calloc`/`realloc`/`free` were not in that list. And in `--det`
they are the four slots `carrier/src/det.cpp` replaces with
`det_wrap_malloc`/`calloc`/`realloc`/`free` over the fixed-address arena
(`port_forge/src/platform/win32/arena.hpp`), so that every guest heap
pointer is snapshot-capturable and reproducible across runs. So:

* `destroy_replay(r)` -- `r` and `r->data` were `malloc`'d by the still-
  ORIGINAL `create_replay`, i.e. by the ARENA. `free()` compiled into the
  carrier handed both to the carrier's UCRT `HeapFree`.
* `save_profile(p)` -- the four page buffers come from the still-ORIGINAL
  `profile_data_page_general/basic/advanced/extra`, again the arena, and
  the same four `free()` calls.

The reverse direction is just as real and merely not yet exercised by any
corpus workload: a block `malloc`'d by src/ and freed by ORIGINAL guest
code reaches `det_wrap_free` with a pointer the arena does not own and is
silently LEAKED (`arena_count_free_foreign`) -- a slow divergence instead
of a loud one.

### 4. The same argument, two facilities further, one of which fails SILENTLY

Fixing only the heap made both crashes disappear and immediately exposed
the second half, which is the more instructive one because nothing
crashes:

**`time`/`clock`.** `det.cpp` wraps both (`det_wrap_time`,
`det_wrap_clock`): in `--det` they answer from a pinned virtual epoch and
from the recording's own `T time <v>` channel, which is what makes
`play()`'s three-clock anti-cheat telemetry (`clock` /
`QueryPerformanceCounter` / `time`, main.c 3513-3661) reproducible. Note
that `QueryPerformanceCounter` was already bound by batch 12 and the other
two were not -- one third of one instrument pointed at the guest and two
thirds at the host. MEASURED, with the heap fixed and the clock not:
`save_profile=src` completed normally and wrote a profile whose
`Last updated:` line read the REAL host date **2026-09-08** where the
ORIGINAL form wrote the det-pinned **2026-09-07**. One line of one text
file, no crash, and no per-tick digest difference at all -- the digest
samples 151 game globals inside the tick loop, and everything here happens
after it. `play.c` has thirteen `time(NULL)` and three `clock()` call
sites and has been promoted since batch 12, so this was live in every
`play=src` run since then; `tc_c_data`/`tc_t_data` are hashed by
`calc_replay_checksum`, so it was reaching the saved `.itr`'s checksum.

**The stdio family** (`fopen`/`fclose`/`fwrite`/`fprintf`/`fputs`/
`fputc`/`vfprintf`). A `FILE *` is a handle into ONE C library's stream
table, and `profile.c`'s `save_profile` hands the one it opens straight to
the still-ORIGINAL guest function `save_control`, which writes to it with
the guest's msvcrt. `game_funcs.h` even types that parameter
`it_orig_FILE *` -- the DWARF-reconstructed MSVC shape -- precisely
because the two are different structs for the same pointer value, so no
compiler can catch it. Once `fopen` is bound the whole family must be, or
the handle splits mid-file; `logfile.c`'s `log2file` is the other src/
stream user and has to stay internally consistent with it.

Deliberately NOT bound: `sprintf`, `vsprintf`, `printf`. They carry no
cross-boundary state (both `sprintf`s write into a buffer the CALLER
owns), and log2file's own in-vivo oracle -- `assets/log.txt` byte-identical
between an unbound and an all-bound run, batch 13 -- is standing evidence
that the carrier's own formatter already produces the guest's bytes.

### 5. The fix, and the check that would have found it in batch 8

`carrier/gen/gen_bindings.py`:

* `GUEST_CRT_IMPORTS` gains thirteen entries -- `malloc`, `calloc`,
  `realloc`, `free`, `time`, `clock`, `fopen`, `fclose`, `fwrite`,
  `fprintf`, `fputs`, `fputc`, `vfprintf` -- all `msvcrt.dll`, `__cdecl`
  by `DLL_DEFAULT_CONVENTION`, every slot VA looked up in `imports.json`.
  `FILE *` is spelled `void *` for the same reason
  `pthread_mutex_lock`'s mutex is: this header is force-included ahead of
  everything, so a parameter type it emits must not depend on which
  `<stdio.h>` the consuming TU will later see, and `FILE *` converts both
  ways implicitly in C with no cast and no warning.
* `GUEST_CRT_PRE_INCLUDES` gains `<stdio.h>`, `<stdarg.h>` and
  `<time.h>`, so the new macros rewrite src/'s CALLS and never a system
  header's own declarations. Not optional: without `<time.h>` the
  `time`/`clock` macros mangle `ucrt/time.h(144)`/`(548)` into C2059 in
  TUs that never call either function (MEASURED).

**And the generic check, which is the part worth keeping.** The rule the
three facilities share is mechanical: *every import the carrier WRAPS is
an import src/ must call through*, because a wrapped slot is by definition
a slot with carrier-owned state behind it. `carrier/src/wrappers.cpp`'s
`kWrapTable` is the authoritative list of those, so
`load_carrier_wrapped_imports()` now PARSES that table out of the C++
source (one list, not a copy that can rot) and
`find_wrapped_imports_unbound()` fails the build if `src/` calls a wrapped
name `GUEST_CRT_IMPORTS` does not bind.

Two details make it a real check rather than a warning:

* It deliberately does NOT skip `RESERVED_CRT_WINDOWS_IDENTS`. That
  exclusion is exactly what hid this for three batches: the existing
  `find_unbound_referenced_imports` report classified `malloc`, `free`,
  `time` and `clock` as "expected noise" because they are ordinary libc
  names. Being on the reserved list only means a name needs the
  pre-include treatment; it says nothing about whether it may be left
  unbound.
* It scans CALL SITES, not bare identifiers (`scan_src_call_targets`,
  comments and string literals stripped, `.name`/`->name` excluded).
  Needed, and MEASURED: the first version flagged `msvcrt.dll!exit`,
  which `src/` never calls -- the token appears only in prose ("Press ESC
  to exit") and as Allegro's own `GFX_DRIVER`/`DIGI_DRIVER` vtable MEMBER
  `void (__cdecl *exit)(struct BITMAP *)`. Binding it would have rewritten
  those two declarations into syntax errors, the same
  `MEMBER_ACCESS_COLLISIONS` hazard from a new direction.

With the fix in, the check reports zero -- and it now guards every future
promotion, including the ones that will call `getenv`, `Sleep` or
`timeGetTime` from clean source.

### 6. Verdicts (all MEASURED this pass, after the fix)

Isolating runs, `play=original` + one file's functions, human_test to the
game's own exit. "files" = `assets/log.txt`, `assets/tower.cfg`,
`MissingNO.itp`, `MissingNO_stats.txt` byte-identical to the unbound run's,
and `last_game.itr` EQUAL under `compare_itr.py` (checksum included):

| bound | per-tick digest | witness | files |
|---|---|---|---|
| hisc.c (3) | EQUAL 2293 | 2386 / 100, `Done...` | all EQUAL |
| replay.c (5) | EQUAL 2293 | 2386 / 100, `Done...` | all EQUAL |
| profile.c+config.c (5) | EQUAL 2293 | 2386 / 100, `Done...` | all EQUAL |
| fade.c + scroller.c (3) | EQUAL 2293 | 2386 / 100, `Done...` | all EQUAL |

All-bound, every `bind_table.inc` row including batch 14's sixteen:

| run | result |
|---|---|
| `all_rows_src.bindfile` (play included), human_test to exit | digest EQUAL 2293 vs the STORED `replays/human_test.digest`; score 2386 / floor 100; `Done...` |
| `all_rows_src_no_play.bindfile`, same | identical |
| `all_src.bindfile`, same | identical |
| frame oracle `every=1`, unbound vs `all_rows_src` | **byte-identical files**, 2752/2752 frames |
| the whole `assets/` tree after the run, unbound vs `all_rows_src` | byte-identical except the two `.itr` files the run itself WRITES (`last_game.itr`, `MissingNO_best_jj2_4.itr`) -- and both of those EQUAL under `compare_itr.py`, same 0x8180e34d checksum |
| all thirteen `.itr` files in the profile, individually | EQUAL |
| newgame, unbound vs `all_rows_src`: digest / frames | EQUAL 876 ticks / EQUAL 982 frames |
| `.itr` workload, unbound vs `all_rows_src`: digest | EQUAL 157 ticks |
| `.itr` workload, frames | 841 common ticks, 0 mismatching; the unbound run has 16 extra TRAILING frames -- the idle-menu wall-clock tail this file already documents for `draw_frame`/`poll_control`/`blit_to_screen` |
| `.itr` workload, whole `assets/` tree written | byte-identical |
| `gates.ps1` G1 / G2 / G3a / G3b / G4 / G5a / G5b | all EQUAL |

`gates.ps1`'s G4 and G5b now drive the UPDATED `all_src.bindfile`, so
those two stored-baseline gates cover batch 14's sixteen from here on.

### 7. Bindfiles

`all_src.bindfile`, `all_rows_src.bindfile` and
`all_rows_src_no_play.bindfile` each gained the same sixteen rows
(`qualify_hisc_table`, `sort_hisc_table`, `enter_hisc_table`, `get_rank`,
`get_rank_id`, `save_profile`, `hash`, `calc_replay_checksum_131`,
`calc_replay_checksum`, `destroy_replay`, `save_replay`, `save_config`,
`myDeleteFile`, `fadeOut`, `fadeIn`, `init_scroller`), with the reasoning
in the block comment above them.

NOT added: `create_replay` and `load_replay`. PROMOTIONS.md batch 15
appended both to `src/icytower/replay.c` while this pass was measuring, so
`bind_table.inc` is now 80 rows rather than 78. They are that pass's own
functions and are not verified by this one. They DID run as src in every
`play=src` measurement above anyway (divergence 010's linker mechanism --
`save_personal_bests` calls `load_replay`), and every one of those runs
came out byte-identical, which is a data point for batch 15 and not a
verdict from this pass.

### 8. Scope note: four in-flight batch-15 files are temporarily blocked

`src/icytower/alert.c`, `game_data.c`, `menu_keys.c` and `results.c`
appeared UNTRACKED in this shared checkout mid-pass and do not compile in
the carrier world yet -- `game_data.c:124` C2059 (the `rejump` blunt-
`#define` collision `replay.c` already fixes locally with
`#ifdef`/`#undef`), `alert.c:188` C2059 (the same class for `ctrl`),
`menu_keys.c:66` C2065 `KEY_A`..`KEY_Z`. All four are listed in
`carrier/gen/build_blockers.json` and `win32_policy.json`'s
`scan_exclude`, with the reason and the removal condition in the blocker
entry itself. Unlike batch 13's equivalent window these are NOT reverted
before committing, because batch 15 has not landed its fixes and removing
them leaves the tree unbuildable (MEASURED, re-checked immediately before
the commit). They are batch 15's own files and own fixes; the day that
pass lands, delete all four entries from both files and re-run
`build.cmd`.
