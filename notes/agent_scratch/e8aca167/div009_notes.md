
## Divergence 009 - a host DEVICE ENUMERATION reached the digest through the arena (2026-09-07)

**The report from the operator.** `replays/human_test.txt` +
`replays/human_test.digest` were recorded at 16:15. Fast replays at ~17:00,
~18:30 and ~19:05 were **EQUAL (2293 ticks)**. From ~19:40 *every* replay
DIFFERED at **T=237, the first gameplay tick**, while the game outcome stayed
byte-identical (run to the game's own exit: `last_game.itr` score 2386 @0x4a,
floor 100 @0x4e). HEAD build and the recording-time commit rebuilt clean in a
worktree produced **identical streams to each other**; `--interactive`,
`--window=normal`, relative vs absolute paths and `PF_KEY_ASCII=0/1` changed
nothing. No git-tracked input had changed.

### A. How the channel was named: two `--report` JSONs, no new instrumentation

Diffing the current run's `--report` against `artifacts/verify_all35_report.json`
(17:02, the EQUAL era) - the only things that ever differed were counters:

```
                     19:40+ (DIFFER)   17:02 (EQUAL)     delta
arena.top              28466576         28468560        -1984 bytes
arena.live_blocks           2692             2712          -20 blocks
msvcrt.dll!malloc          61266            61290          -24
msvcrt.dll!realloc          1571             1575           -4
msvcrt.dll!free            62842            62846           -4
msvcrt.dll!strncat            21               27           -6
```

`-24 malloc / -4 free = -20 live blocks` closes exactly. **`strncat` is the
tell**: `--trace-imports=strncat` named its only caller,
`_al_sane_strncpy+0x28`, copying six strings out of *host* heap addresses.

**MEASURED in `artifacts/disasm.txt`, the whole mechanism:**
`_get_win_digi_driver_list` (wddsnd.c, 0x47ac18) calls
`DirectSoundEnumerateA(DSEnumCallback=0x47bcf4, NULL)` (0x47ac4d).
`DSEnumCallback` (0x47bcf4) skips the NULL-GUID "primary" entry and, per real
render device, does `_al_malloc(strlen(description)+1)` (0x47bd24) +
`_al_sane_strncpy` (0x47bd58) into `_dsalmix_name_list[16]` @0x4ed3a0, stores
the GUID pointer in `_dsalmix_guid_list[16]` @0x4ed3e0 and bumps
`_dsalmix_count` @0x4ecb64 (capped at 16 - `cmp $0xf`, 0x47bd68).
`_get_win_digi_driver_list` then, per device, calls `_get_dsalmix_driver` +
`_driver_list_append_driver` (0x47ac89/0x47aca7) and mallocs a 0xb8-byte
`DIGI_DRIVER` copy for `DIGI_DIRECTX(i)` (0x47acc3, id built at 0x47acd6 as
`AL_ID('D','X','A'+i,' ')`). **~10 arena blocks, ~992 bytes and exactly 3
`strncat` calls per device**, so for any run `strncat == 3*devices + 3`.

**Read straight out of the tick-237 snapshot** (`_dsalmix_count` @0x4ecb64 and
the arena the name pointers point into - no new carrier code needed):

```
_dsalmix_count = 6
  [0] 'Reproduktory (Realtek(R) Audio)'            [3] 'Reproduktory (Steam Streaming Speakers)'
  [1] 'Sluchatka (Oculus Virtual Audio Device)'    [4] 'Reproduktory (Steam Streaming Microphone)'
  [2] 'Realtek Digital Output (Realtek(R) Audio)'  [5] 'Reproduktory (Voice.ai Audio Cable)'
```

- exactly the six `Status=OK` **render** endpoints `Get-PnpDevice -Class
AudioEndpoint` lists on this host. The other four endpoints it lists are
`Status=Unknown` NVIDIA HDMI monitor-audio endpoints (BenQ LCD x2,
MAG321UX OLED x2). **Two of those were live during the EQUAL era and are gone
now** - a monitor / DisplayPort link dropped between 19:23 and 19:40.
`Get-CimInstance Win32_VideoController` on this host lists 4 adapters
(NVIDIA RTX 4090, AMD Radeon, vorpX Virtual Display, Virtual Desktop Monitor),
which is why HDMI audio endpoints come and go here at all.

**The whole day, from the `strncat` count of every `--report` in the repo**
(`devices = (strncat-3)/3`):

| clock | reports | strncat | devices | arena.top |
|---|---|---:|---:|---:|
| 11:42 | `artifacts/run1_report.json` | 21 | **6** | - |
| 12:01 - 19:23 | 100+ reports incl. `verify_all35_report.json` (17:02) and `artifacts_task/invivo_batch7/*` (19:23) | 27 | **8** | 28468560 |
| 20:04+ | this pass | 21 | **6** | 28466576 |

That is the drift, dated, with the last EQUAL-era run at 19:23 and the first
DIFFERing run at ~19:40 - and 11:42 shows the host was in the *same* 6-device
state earlier the same morning.

### B. Why an enumeration moves a digest that never hashes it

None of the enumerated values is stored in a game global.
`_dsalmix_name_list` / `_dsalmix_guid_list` / `_dsalmix_count` are **Allegro's**
globals, and the digest hashes only the 151 **game-owned** globals of
`carrier/gen/game_globals.inc`. The channel is the shared **deterministic
arena**: those per-device allocations are an ALLOCATION PREFIX, and every
later arena address is displaced by it. `pf_inspect` on a tick-237 snapshot:
**27 of the 151 digest-domain globals hold arena pointers**.

**Per-global attribution, done without a recording-time snapshot** by taking
two snapshots at T=237 that differ only in the number of devices delivered
(`DET_DSOUND_DEVICES=1` vs `=3`, part C):

```
python carrier/scripts/pf_inspect.py diff artifacts/div009/snap_f1 artifacts/div009/snap_posctrl3
  FIRST DIFFERING GLOBAL INSIDE THE PER-TICK DIGEST SCOPE:
    swap_screen @0x004dd194 (BITMAP *, main.c)  A=0x2000e3f0  B=0x2000eb90
  25 named game global(s) differ (19 of them inside the digest scope):
    swap_screen data demo gameData profiles profile combo_sound bg_beat
    bg_menu speaker menu_sounds sounds menu_params.font play_char.bmp
    custom.frame characters greeting_scroller.fnt ply gameover_bmp
```

**Every one of the 19 is a pointer**, shifted by the same 0x7a0; no
value-carrying global differs. That is the model, confirmed by construction.

### C. The other init-time host channels, and why each is NOT the cause

Enumerated before the experiment, and each settled by evidence from the same
`--report` / snapshot pair rather than by assertion:

| candidate | in the digest domain? | how established | could it have changed 17:00 -> 19:40? |
|---|---|---|---|
| **DirectSound device list** | **yes, indirectly** (arena displacement) | `--report` counter deltas + disasm + snapshot read of `_dsalmix_count` | **YES - it did** (8 -> 6) |
| Audio voice ids (`checkMusicVoiceID`, `gameMusicVoiceID`, `sounds[]`, `combo_sound[]`, `menu_sounds[]`, `speaker[]`, `bg_beat`, `bg_menu`, `custom.jump_sound[]`) | yes (`game_globals.inc`) | in the .inc by name; `pf_inspect` shows `sounds`/`combo_sound`/... differing only in their **pointer** halves between the two device counts | no - the voice-id *values* were identical in every run; only the SAMPLE pointers moved |
| `DirectSoundCreate` device identity | no | traced `lpGuid` == `_dsalmix_guid_list[0]`, an Allegro global outside the 151 | unchanged (device 0 is still Realtek) |
| desktop/display queries (`GetDeviceCaps` x1, `desktop_color_depth`, `get_desktop_resolution`) | no game global holds them | absent from `game_globals.inc`; count constant (1) in every report of the day | screen resolution unchanged (3840x2160x32 on all 3 active adapters) |
| `GetTempPathA` | n/a | **imported but called 0 times** - it appears in no `--report`'s `imports` array, in any run of the day | n/a |
| `getenv` | forwarded, instrumented | `environment.getenv_names`: `ALLEGRO` x1, `PRINTF_EXPONENT_DIGITS` x13, `SCREEN_GAMMA` x2, **all unset**, in both eras | no |
| `GetModuleFileNameA` x4 / cwd strings | already carrier-owned (wrapper substitutes the guest path) | `kWrapNames`; counts identical in both eras | no - and the drift reproduced with relative *and* absolute paths |
| `GetVersion` x2 / `GetVersionExA` x1 | no game global holds them | not in `game_globals.inc`; counts constant | no |
| locale / timezone / date strings | n/a | **no `localtime`/`_localtime`/`_ftime`/`GetLocalTime`/`GetTimeZoneInformation` call appears in any `--report`** | n/a |
| joystick / gamepad (`joyGetNumDevs` 1, `joyGetDevCapsA` 16, `joyGetPosEx` 0) | `got_joystick` is in the domain | counts identical in every report of the day; `got_joystick` reads 1 | no |
| DirectInput enumeration (`DirectInputCreateA` x3) | not normalized (see gaps) | counts identical in both eras | no |
| hiscore / profile contents | yes | restored from `artifacts/assets_backup` before every run (unchanged since 10:58) | no |
| pinned RNG seed derivation | yes | `rng.state` / `rng.calls` identical in both eras; `time()` values come from the recording (`time_replayed=9/13`) | no |

### D. The fix: normalize the enumeration, do not exclude the globals

Excluding those 19 would be wrong - they are genuine game state, not host
identities like the 3 names `gen_game_globals.py` already drops. The rule is
written down generically in **`carrier/win32_policy.json`
`digest_domain.host_enumeration_policy`**:

> In a carrier-owned deterministic run no host ENUMERATION result may reach
> the guest. The carrier performs the enumeration itself into carrier memory
> and delivers a constant, carrier-owned list of fixed size with fixed-length
> synthetic names; only an opaque host handle needed to keep the subsequent
> open/create call working may be passed through, and only where that handle
> is provably outside the digest domain.

Implemented as one new always-installed IAT wrapper,
**`det_wrap_DirectSoundEnumerateA`** (`det.cpp`, wired like `getenv` /
`pthread_create` through `imports.cpp`'s `kWrapNames` and `wrappers.cpp`).
Whenever the carrier owns determinism (`--det`, or input policy != real) it
runs the real enumeration into carrier statics and then calls the guest's own
callback with **1** device whose description is the constant
`"PortForge Deterministic Audio Device 0"`. The one host value passed through
is the **GUID bytes**, copied into carrier memory - and a GUID lands only in
Allegro's `_dsalmix_guid_list`, outside the digest domain.

Delivering exactly one device is not a behavioural change here: DirectSound
lists the default render device first among the real ones, and the guest was
already opening index 0 - **MEASURED**, the traced
`DirectSoundCreate(lpGuid=...)` from `digi_dsoundmix_detect+0x5b` equals
`_dsalmix_guid_list[0]`. `--report` now also carries
`environment.dsound_host_devices` / `dsound_delivered` / `dsound_host_names`
(the host's real list, verbatim) and stderr prints it, so a future drift of
this class is one diff away instead of a day of bisection.

Two audit knobs, in the `DET_ISOLATE_OFF` / `DET_PERTURB_*` style, never used
by a gate: **`DET_ISOLATE_OFF=dsound`** (forward the host list unchanged) and
**`DET_DSOUND_DEVICES=N`** (deliver N synthetic devices).

### E. Proof

```
f1  vs f2   (two fresh normalized replays)              EQUAL (2293 ticks)
f2  vs f3   (a third, after the re-baseline)            EQUAL (2293 ticks)
negctrl (DET_ISOLATE_OFF=dsound) vs the pre-fix build   EQUAL (2293 ticks)  <- the fix changes nothing else
f1  vs negctrl                                          FIRST DIFFERENCE T=237
f1  vs posctrl3 (DET_DSOUND_DEVICES=3)                  FIRST DIFFERENCE T=237  <- positive control
f1  vs --no-sound                                       FIRST DIFFERENCE T=237  (known, "Headless..." SS4)
run to the game's own exit: last_game.itr score 2386 @0x4a, floor 100 @0x4e;
  that run's digest stream                              EQUAL (2293 ticks) to f1
all-bound (--bind-file carrier/scripts/all_src.bindfile) vs the new baseline
                                                        EQUAL (2293 ticks)
```

`strncat` is now a **constant 6** in every normalized run (1 device) - 12 under
`DET_DSOUND_DEVICES=3`, 21 with the isolation off on this host, 27 on the host
as it stood at 17:00. It is the cheapest possible regression guard for this
channel and is recorded as such in `win32_policy.json`.

**Re-baseline (authorized by the score/floor check above):**
`replays/human_test.digest` now holds the normalized stream; the pre-fix file
is kept as **`replays/human_test.digest.pre-009`**.
`replays/human_test.replay.digest` is left untouched - it is the 16:17 twin of
the pre-009 baseline and only means anything as that pair.

### New/changed options and files (this pass)

| what | where |
|---|---|
| `det_wrap_DirectSoundEnumerateA` (new always-installed wrapper) | `src/det.cpp`, `src/det.hpp`, `src/imports.cpp` (`kWrapNames`), `src/wrappers.cpp` |
| `DET_ISOLATE_OFF=dsound`, `DET_DSOUND_DEVICES=N` | `src/det.cpp` |
| `environment.dsound_normalized / dsound_host_devices / dsound_delivered / dsound_host_names` in `--report` | `src/det.cpp` (`det_environment_json`), `src/trace.cpp` (env buffer 2048 -> 4096) |
| `digest_domain.host_enumeration_policy` | `carrier/win32_policy.json` |
| `replays/human_test.digest` re-baselined; `replays/human_test.digest.pre-009` kept | `replays/` |
| digests, reports, snapshots, traces for every run above | `artifacts/div009/` |

### Known gaps / open problems (this pass)

- **The 17:00 digest was NOT reproduced.** The two vanished devices' exact
  description strings are unknown (their lengths are constrained only in
  aggregate, by the 1984-byte / 20-block delta), so the pre-009 baseline
  cannot be regenerated on today's host. The delta is accounted for
  arithmetically, not byte-for-byte; that residue is why the re-baseline path
  was taken and why `.pre-009` is kept.
- **Other host enumerations are not normalized.** `DirectInputCreateA` x3 and
  its device enumeration, and `joyGetDevCapsA` x16 / `joyGetNumDevs` x1, run
  on this host and were constant across the drift window (`got_joystick`
  reads 1, `joyGetPosEx` 0) - but they have the same shape and are the next
  suspects if this signature (identical game outcome, digest differing from
  the first tick, `arena.top` moved) reappears. Listed in
  `win32_policy.json` under `unaudited_enumerations`.
- **A host with NO DirectSound render device is still unmodelled**: the
  wrapper then delivers 0 devices, Allegro falls back to its WaveOut mixer,
  and the digest is not comparable. `notes/determinism_audit.md` row 12 stays
  open for that *absence* case; the *device-list* half of it is now closed.
- **The identity of device 0 is still host-chosen.** Its GUID is passed
  through so real audio keeps working; nothing in the digest domain depends on
  it on this host, but a host where `DirectSoundCreate` failed on device 0 and
  succeeded on another would take a different path.
- **The arena remains the amplifier.** Any host-varying allocation prefix -
  not only an enumeration - becomes a digest difference. The structural cure
  (a game-owned arena separate from the pre-game init arena, or hashing
  pointer *identities* rather than addresses) is bigger than this fix and was
  not attempted.
