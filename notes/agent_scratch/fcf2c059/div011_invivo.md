
## In-vivo pass -- batch 14, and divergence 011 (2026-09-08)

PROMOTIONS.md batch 14 promoted sixteen functions on `play()`'s coastline
-- the game-over, replay and profile halves -- and closed with "this pass
did not run carrier.exe: none of it is on the gameplay tick path, so
`replays/human_test.txt` alone cannot exercise it". That is right, and it
is also why the in-vivo pass for this batch is different in kind from
every earlier one: **its oracle is not a digest, it is the FILES the game
writes.** The full carrier-side account is `carrier/NOTES.md` "Divergence
011"; this is the src/-side summary.

### 0. The workload: the recording run to the game's OWN exit

`replays/human_test.txt` with `--stop-at-tick 3200` (2528 stops inside the
tick loop; the recording's own post-game high-score entry runs to
input-script tick 3047). Fully unbound this reaches `Done...` and leaves
behind: `last_game.itr` at **score 2386 / floor 100**, a new personal best
`MissingNO_best_jj2_4.itr`, `MissingNO.itp`, `MissingNO_stats.txt`,
`tower.cfg`, `log.txt`. Every one of batch 14's sixteen executes on that
path.

Note what could NOT be used as the baseline: "every bindfile row except
batch 14's". Divergence 010 -- `play=src` reaches its promoted callees
through the LINKER, row or no row -- means such a run still executes
batch-14 code. MEASURED: it crashed. The isolating experiment has to be
`play=original` plus the function under test, so that the ORIGINAL caller
reaches the bound callee through the guest VA and `bind.cpp`'s patch.

### 1. Two of the sixteen crashed, and the cause was NOT in src/

`destroy_replay=src` alone, and `save_profile=src` alone, each ended the
run in `STATUS_HEAP_CORRUPTION` (0xc0000374) at the identical point:
`log.txt` stops after `  saving replay: .../last_game.itr` and never
reaches `  saving config and scores`. Both functions are correct as
recovered. What was wrong is that a `src/icytower/*.c` file compiled into
the carrier resolved `free()` to the CARRIER's own statically linked CRT,
while the blocks it was freeing had been `malloc`'d by still-ORIGINAL
guest code out of `det.cpp`'s fixed-address arena:

* `destroy_replay` frees `r` and `r->data`, both from `create_replay`;
* `save_profile` frees the four buffers `profile_data_page_*` returned.

Divergence 008's mechanism (`rand()` reaching the wrong C library), one
facility larger. Fixed in `carrier/gen/gen_bindings.py` by binding
`malloc`/`calloc`/`realloc`/`free` through the guest's own IAT slots, the
way `rand`/`srand`/`mkdir`/`stricmp`/`QueryPerformanceCounter` already
were. No `src/icytower/*.c` file was edited for it.

### 2. And one that did NOT crash: the profile's date was the host's

With the heap fixed, `save_profile=src` ran to completion and wrote a
`MissingNO_stats.txt` whose only difference from the original form's was:

```
-Last updated:          2026-09-07
+Last updated:          2026-09-08
```

`time()` in clean src/ was reaching the carrier's CRT and reading the REAL
host clock, where `det.cpp`'s `det_wrap_time` answers the guest from a
pinned virtual epoch and the recording's own clock channel. `clock()` had
the same split. This matters well beyond one text line: `play.c` (promoted
since batch 12, thirteen `time(NULL)` sites and three `clock()` sites)
feeds those two clocks into the three-clock anti-cheat telemetry at
main.c 3513-3661, whose `tc_c_data`/`tc_t_data` columns
`calc_replay_checksum` HASHES into every saved `.itr`. Also fixed in the
generator (`time`, `clock`), together with the stdio family
(`fopen`/`fclose`/`fwrite`/`fprintf`/`fputs`/`fputc`/`vfprintf`) -- needed
because `save_profile` hands the `FILE *` it opens to the still-ORIGINAL
`save_control`, which writes to it with the guest's msvcrt.

**Standing rule this adds for future recoveries.** Every import the
carrier WRAPS is an import clean src/ must call through, because a wrapped
slot has carrier-owned state behind it. `gen_bindings.py` now enforces
that mechanically -- it parses `carrier/src/wrappers.cpp`'s own wrap table
and fails the build if src/ CALLS a wrapped name that
`GUEST_CRT_IMPORTS` does not bind. The old report could not have caught
this: it skipped everything on the reserved-CRT list, which is exactly
where `malloc`, `free`, `time` and `clock` live.

### 3. The `.itr` byte oracle, and its one masked column

Two UNBOUND runs of the same recording, back to back, differ in exactly
six bytes of `last_game.itr` -- all inside `tc_s_data[0]` and
`tc_s_data[1]`, nothing else. That column is `play.c` 3641's music-sync
channel, fed by `voice_get_position()` on a real DirectSound voice: the
host's audio clock, pre-existing nondeterminism in the ORIGINAL machine
code, and (by batch 14's own finding 3) one of the two columns
`calc_replay_checksum` does NOT hash. `carrier/scripts/compare_itr.py`
masks exactly it, knows `save_replay`'s on-disk write order, and compares
everything else -- the checksum field included -- byte for byte.

### 4. Verdicts

Per file, `play=original` + that file's functions, human_test to the
game's own exit. "files EQUAL" = `log.txt`, `tower.cfg`, `MissingNO.itp`,
`MissingNO_stats.txt` byte-identical to the unbound run's and
`last_game.itr` EQUAL under `compare_itr.py`:

| bound | per-tick digest | witness | files |
|---|---|---|---|
| `hisc.c` -- qualify_hisc_table, sort_hisc_table, enter_hisc_table | EQUAL 2293 | 2386 / 100, `Done...` | EQUAL |
| `replay.c` -- hash, calc_replay_checksum_131, calc_replay_checksum, destroy_replay, save_replay | EQUAL 2293 | 2386 / 100, `Done...` | EQUAL |
| `profile.c` + `config.c` -- get_rank, get_rank_id, save_profile, save_config, myDeleteFile | EQUAL 2293 | 2386 / 100, `Done...` | EQUAL |
| `fade.c` + `scroller.c` -- fadeIn, fadeOut, init_scroller | EQUAL 2293 | 2386 / 100, `Done...` | EQUAL |

All sixteen bound at once, on top of every earlier row and `play` itself
(`carrier/scripts/all_rows_src.bindfile`, now 78 rows):

| workload | check | result |
|---|---|---|
| human_test | per-tick digest vs the STORED `replays/human_test.digest` | EQUAL, 2293 ticks |
| human_test | run to the game's own exit | score 2386 / floor 100, `Done...` |
| human_test | frame oracle `every=1` vs unbound | byte-identical file, 2752 frames |
| human_test | the whole `assets/` tree vs unbound | byte-identical except the two `.itr` files the run WRITES, and both of those EQUAL under `compare_itr.py` with the same 0x8180e34d checksum |
| human_test | all thirteen `.itr` files in the profile, one by one | EQUAL |
| newgame | digest / frames vs unbound | EQUAL 876 / EQUAL 982 |
| `.itr` (play_itr.txt) | digest vs unbound | EQUAL 157 |
| `.itr` | frames vs unbound | 841 common ticks, 0 mismatching (16 extra trailing frames on the unbound side -- the known idle-menu wall-clock tail) |
| `.itr` | whole `assets/` tree | byte-identical |
| gates | G1 / G2 / G3a / G3b / G4 / G5a / G5b | all EQUAL |

`all_src.bindfile` gained the same sixteen rows, so `gates.ps1`'s G4 and
G5b (stored-baseline gates) cover batch 14 from here on.

**Every function in batch 14 is now verified in vivo.** Unlike batch 13's
`take_screenshot`, none of the sixteen is an unreached bind: all sixteen
execute on the run-to-exit path, and the four groups above each changed at
least one thing the run depends on.

### 5. Scope note

`src/icytower/{hisc,replay,profile,config,fade,scroller}.c` were NOT
edited by this pass -- the recovered source was right, and the fix was one
layer down. `create_replay` and `load_replay` (PROMOTIONS.md batch 15,
appended to `replay.c` while this pass measured) are deliberately absent
from the bindfiles; they nonetheless ran as src in every `play=src` run
above through divergence 010's linker route, and every one of those runs
was byte-identical -- a data point for batch 15, not a verdict from here.
Four untracked in-flight batch-15 files (`alert.c`, `game_data.c`,
`menu_keys.c`, `results.c`) do not compile in the carrier world yet and
are temporarily blocked in `carrier/gen/build_blockers.json` +
`win32_policy.json`; see `carrier/NOTES.md` "Divergence 011" SS8 for the
removal condition.
