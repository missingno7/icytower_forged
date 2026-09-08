# Handover — Icy Tower Win32 carrier pilot

Written 2026-09-08 at the end of the first two-day session. Read this first;
then `notes/living_record.md` (ledger), `win32_pilot.md` (design record),
`ROADMAP.md` (end-state), `carrier/NOTES.md` (evidence per mechanism),
`src/icytower/PROMOTIONS.md` and `INVIVO.md` (per-function verdicts).

## 1. Where things stand

| area | state |
|---|---|
| Carrier | Original code runs natively inside `carrier/carrier.exe` (MSVC x86 + mingw32 GCC objects). Deterministic mode (`--det`), recording/replay, safepoint snapshots with certified in-process rewind, frame oracle above the renderer, per-function binding (ORIGINAL/LIFTED/NATIVE/src) with per-invocation sensors, windowless automated runs. |
| Framework | The target-independent mechanism lives in **port_forge `main`** (`src/platform/win32/*.hpp`, `tools/pf_win32_*.py`, `tools/pf_allegro4_datafile.py`, `tools/pf_win32_recovery_audit.py`, `docs/90`). The submodule tracks `main`; `experimental/win32` is level with `main` and only for future experiments. Project policy: `carrier/win32_policy.hpp` + `carrier/win32_policy.json`. |
| Clean source | `src/icytower/`: 75 functions offline-verified + `play()` + batch 15 (see §5), ~45 KB of 118 KB game code. The whole gameplay tick path, the game-over/high-score/replay-save path and `draw_frame` are clean and verified in vivo (62+ functions) over the human recording, the game's own `.itr` replay and the scripted session. |
| Libraries | Boundary censused; header-level ABI; Allegro 4.4.3.1 + logg + loadpng built from source in `third_party/` (fetch script); `src/` compiles unchanged against upstream headers. Original toolchain TDM-GCC 4.4.1 archived. |
| Assets | 257 datafile objects extracted losslessly (`scripts/extract_assets.py`, local cache only, licence forbids redistribution); asset-id seam generated; carrier-vs-standalone asset oracle 186/186 bitmaps, all fonts/info, 5/6 palettes (the last one is a runtime mutation). |
| Corpora | `replays/human_test.txt` (operator, 100 floors / score 2386), `carrier/scripts/newgame.txt`, `carrier/scripts/play_itr.txt` (game-native replay), `menu_idle.txt`. |

## 2. How to resume (commands)

Build and gates (from `carrier/`, MSVC vcvars32 is called by the script):

```bash
cmd /c .\build.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/gates.ps1
```

Expected (as of the closing run): G1 EQUAL (876 ticks), G2 EQUAL (876 invocations), G3a/G3b EQUAL,
G4 EQUAL (2293 ticks, human recording, all rows bound vs stored baseline),
G5a/G5b EQUAL (157 ticks, `.itr` workload).

Verify one function in vivo, all workloads:

```bash
python carrier/scripts/bind_all.py --fn NAME --out-dir artifacts/x --input-script replays/human_test.txt --baseline replays/human_test.digest --stop-at-tick 2528
```

Offline oracle for a recovered function (shims delegate to port_forge tools):

```bash
python carrier/lift/harness/lift_check.py --form src --toolchain gcc NAME
python carrier/scripts/recovery_audit.py            # required before in vivo
python scripts/check_native_layer.py                # purity gate for src/
```

Record / replay a human session (normal speed, windowed):

```bash
python scripts/play.py --record-replay NAME
python scripts/play.py --play-replay NAME
```

Rules that cost real time to learn (all evidenced in the ledger):
- one carrier-running task at a time (shared `assets/`, focus); restore
  `assets/tower.cfg`, `profiles/`, `log.txt` before every run
  (`carrier/scripts/restore_assets.ps1`);
- float code: GCC `-m32 -mfpmath=387 -mno-sse -mno-sse2 -O2`; MSVC only for
  integer code; comparisons must keep x87 unordered semantics;
- clean code's runtime/library calls must bind to the guest's import slots
  (enforced at build by the bindings generator);
- safepoints and sensors at function boundaries, both guest VA and bound
  symbol; stored-baseline gates (G4/G5) are mandatory, two-run gates miss
  drifts;
- host enumerations before the first safepoint are channels (DirectSound
  normalized; see determinism audit for what remains).

## 3. Open items, ordered

1. In-vivo pass for batch 15 (see §5) and removal of its temporary
   `build_blockers.json` / `scan_exclude` entries.
2. Recovery of what remains ORIGINAL: `new_game`, `init_game`'s ~20 callees,
   the menu screens (`main_menu_callback`, `select_profile`, `view_scores`,
   `replay_selector`, `draw_replay_selector`, `key_to_str` if not done in
   batch 15), the entry `_mangled_main`/`WinMain`; then the six `logg_load_memory`
   functions and `_win_hcursor` adapter for the standalone build.
3. Standalone transition (win32_pilot.md §7c S3–S5): a plain-source replay
   driver (virtual clock driving `install_int` callbacks, key injection at
   tick boundaries, pinned `rand`, FPU control word, per-tick digest of game
   state), `src/` + real Allegro linked as `icytower.exe` in drop-in mode
   (reads the user's own datafiles), compared against the carrier on the
   corpora. Expected first divergences: timer semantics, DirectInput
   scancodes, blitter colour conversion.
4. Framework debts listed in `notes/extraction_plan.md` §4a: binding engine
   and stubs, `print_globals`, report/manifest formats still project-side;
   `pf_gate_all` never completed (clones ~20 sibling projects).
5. Determinism audit leftovers (minor): audio device absence, a real
   gamepad, `getenv` on hosts that set `ALLEGRO`/`SCREEN_GAMMA`, GDI headless
   parked (hidden-window mode is the supported headless).
6. Next target: `D:\Games\DOS\dos_recosystem\cyberstorm_forged` (CSTORM.EXE:
   PE32, 21 926 relocations, no debug info, Watcom). The mechanisms transfer;
   every DWARF-driven generator needs a non-DWARF symbol source
   (`notes/cyberstorm_census_preview.md`).

## 4. Divergence ledger (numbers to know)

001 network body; 002/005 recording tick coordinate (sub-tick handover);
003 timer-thread join; 004 bump arena; 006 NaN guard; 007 missing guard
found by a human recording; 008 CRT `rand` bound to the carrier's runtime;
009 host audio endpoints shifting arena pointers; 010 magic-divide misread in
`play` (+3 defects, two audits born); 011 heap/clock/stdio bindings. All
resolved; each has its check.

## 5. Batch 15 (last task of the session) and the closing gate run

Batch 15 (committed): `key_to_str` → `menu_keys.c`, `create_replay` +
`load_replay` → `replay.c`, `getGameDataXML` → `game_data.c`, `draw_results`
→ `results.c`, `my_alert` → `alert.c` (first interactive function). All
offline EQUAL over 233 600 vectors with full instruction coverage and
negative controls; the `.itr` format is closed against all 13 real files
(magic `ITR140`, tag `Harold`; every stored checksum reproduces,
`scripts/itr_real_files_check.py`). Cumulative: 81 offline-verified functions
+ `play` (in vivo) + 2 compile-only; 52 939 original bytes recovered (45 % of
game code), 36 021 offline-verified; recovery audit 84/84.

Not yet done for batch 15 (first item for the next session): the in-vivo
pass (bind each over the three workloads; `do_replay_menu` is still ORIGINAL
because six of its callees are), and removal of the temporary
`scan_exclude` entries for `alert.c`/`game_data.c`/`menu_keys.c`/`results.c`
in `carrier/win32_policy.json` (they compile clean in the carrier world)
plus a hand `--exclude` for `floor_size_modifiers`. Generator gaps to
close: `state.c` zero-fills `REPLAY_HEADER` (real `.rodata`, `"ITR140"`), so
a standalone build would write unreadable replay files; a member-collision
case (`ctrl` global vs `menu_params.ctrl`) that `#undef` cannot solve.

Closing gate run (2026-09-08, final build, `carrier/scripts/gates.ps1`):
G1 EQUAL (876 ticks); G2 EQUAL (876 invocations, src vs original); G3a
EQUAL (300 rows pre/post rewind; 601 rows cold vs post-rewind); G3b EQUAL
(300 invocations); G4 EQUAL (2293 ticks, human recording, all rows bound vs
stored baseline); G5a EQUAL (157 ticks, `.itr` replayed twice); G5b EQUAL
(157 ticks, all bound vs stored baseline); purity gate 0 violations over 56
files. (The counts moved by one at the start when the tick safepoint moved
to `update_player`'s entry in divergence 010; the stored baselines were
regenerated then with the score/floor witness.)

Both repositories were clean and pushed at handover: icytower_forged
`main`, port_forge `main` (submodule pinned), `experimental/win32` level
with `main`.
