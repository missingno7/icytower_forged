
## Environment isolation

Follow-up pass (2026-09-07) closing the two suspects "Divergences 004 and 005"
left open (Allegro's WM_ACTIVATEAPP switch path and the real MOUSE), taking
ownership of the remaining desktop-visible channels, and running the
determinism audit that was interrupted earlier. Full audit with per-channel
first-differing ticks: `notes/determinism_audit.md`; raw digests, reports and
stderr: `artifacts/determinism_audit/`.

**All three gates re-verified with the final binary** (assets restored from
`artifacts/assets_backup/` + `artifacts/log_original_baseline.txt` before every
launch, `carrier/scripts/gates.ps1`):

```
G1  compare_digests.py    -> EQUAL (876 ticks)
G2  compare_fn_digests.py -> EQUAL (877 invocations, [src] vs [original])
G3  certify_snapshot.py rewind -> EQUAL (301 rows T=400..699) and
                                  EQUAL (602 rows T=400..1000, cold vs post-rewind)
    certify_snapshot.py fn     -> EQUAL (301 invocations, k=276..576)
SendInput round trip (sendinput_session.py) -> EQUAL (1050 and 1252 ticks)
```

**One honest deviation from the task's stated expectation, up front:**
suppressing the ad-fetch thread does NOT leave G1's digest *values* unchanged
- it changes them from tick 126 - because the live thread was writing into the
verdict domain (four digest-scope globals, measured; part E below). G1's actual
contract, "two independent runs of this build are EQUAL over 876 ticks", holds
and was re-verified; only the absolute hashes moved, which
`notes/living_record.md` already records as comparable within one build only.

### A. No focus stealing for automated runs (item 1)

**MEASURED** (`artifacts/disasm.txt`): `create_directx_window` (0x478eb8)
creates the window with `dwStyle=0xCA0000` (WS_CAPTION|WS_SYSMENU|
WS_MINIMIZEBOX - note: no WS_VISIBLE) and then calls
`ShowWindow(hwnd, SW_SHOWNORMAL)` / `SetForegroundWindow(hwnd)` /
`UpdateWindow(hwnd)`; `set_video_mode` (wddmode.c) repeats the ShowWindow +
SetForegroundWindow pair when the graphics mode is set. Those two calls are
the whole mechanism by which the guest window lands in front of the operator.

New option **`--interactive`** (bare flag; `scripts/play.py` passes it for its
interactive and `--record-replay` modes, and `carrier/scripts/sendinput_session.py`
passes it because SendInput needs the window focused). Only an `--interactive`
run may take the foreground. Without it, four new always-installed IAT wrappers
(`det.cpp`, wired like `Sleep`/`time`/etc):

| import | non-interactive behaviour | interactive |
|---|---|---|
| `ShowWindow` | activating show commands (SW_SHOWNORMAL/SHOWMAXIMIZED/SHOW/RESTORE/SHOWDEFAULT) are substituted with `SW_SHOWMINNOACTIVE` (or `SW_HIDE` under `--window=hidden`); every other command passes through | unchanged |
| `SetForegroundWindow` | suppressed, returns TRUE (Allegro ignores the result) | unchanged |
| `SetWindowPos` | `SWP_NOACTIVATE` ORed in | unchanged |
| `CreateWindowExA` | `WS_VISIBLE` stripped, `WS_EX_NOACTIVATE` added | unchanged |

and the carrier's own `try_focus_guest_window_once` is now gated on
`--interactive` instead of `input_policy==Real`.

New option **`--window=normal|minnoactive|hidden`** (default: `normal` when
`--interactive`, else `minnoactive`).

**cnc-ddraw did NOT refuse a hidden window** - the documented fallback was not
needed:

```
win_hidden vs iso_all   EQUAL (876 ticks)   # --window=hidden
win_normal vs iso_all   EQUAL (876 ticks)
no_window  vs iso_all   EQUAL (876 ticks)   # policy removed entirely
```

`SW_SHOWMINNOACTIVE` is nevertheless kept as the default, because a minimized
window is easier for an operator to notice and reason about than an invisible
one. G1 is unchanged in all three modes, which is what SS4a predicts
(presentation is below the PortForge boundary).

### B. Window activation as a controlled channel (item 2)

**The choke point, chosen by evidence.** `_switch_out` (0x465808) and
`_switch_in` (0x4657e4) are 35-byte `void f(void)` loops over
`switch_out_cb[8]` @0x4ea060 / `switch_in_cb[8]` @0x4ea080, reached ONLY from
the two tail `jmp`s at the end of `_win_switch_out` (0x47a3d4) /
`_win_switch_in` (0x47a47c). MEASURED at run time, each table holds exactly
one entry - the game's own callbacks:

```
det: switch_in_cb = 00406A6C ...   (switchedToProgram:   hasFocus = 1)
det: switch_out_cb= 00406A5C ...   (switchedFromProgram: hasFocus = 0)
```

`hasFocus` (0x4bc020) and `lastFocus` (0x4bc024) are inside the 151-global
digest scope, and `play()` compares them at 0x411c6b/0x411cd7 and restarts the
game music on a change (writing `checkMusicVoiceID` @0x4bc174, also in scope).

**`set_display_switch_mode` was RULED OUT by evidence**, not by preference:
`_win_switch_out`'s disassembly branches on `get_display_switch_mode` only to
decide whether to *also* reset an event and drop the thread priority - both
arms end in the same `jmp _switch_out` (0x47a416, 0x47a476). No switch mode
stops the callbacks, and the game already runs in SWITCH_BACKGROUND
(`_win_reset_switch_mode` @0x47a508 calls `set_display_switch_mode(3)`).

**Mechanism: a 5-byte `jmp rel32` entry patch, not a hardware breakpoint.**
The DR budget is fully spoken for (DR0 tick safepoint, DR1 key_dinput, DR2/DR3
reserved for bind.cpp's ORIGINAL-form sensing that gate G2 needs), so the
patch mechanism bind.cpp already uses for LIFTED/NATIVE forms is reused
(`patch_entry_jmp`, `det.cpp`). Both patched functions are `void f(void)`
reached by a tail jmp, so the stub's plain `ret` is convention-correct.
Installed whenever the carrier owns determinism (`--det`, or input policy not
real); a plain un-det `--input=real` oracle run is untouched.

- `--input=script|none`: the stub counts and returns. **SUPPRESSED** - nothing
  the operator does can reach the game's callbacks.
- `--input=real`: the stub counts and QUEUES the event; it is handed to the
  game from the main thread at the next tick boundary - the same handover
  point keys use (divergence 005's rule) - by running the guest's own callback
  table, and written to `--record-input` as `T switch in|out`.
- replay: a `T switch in|out` line is delivered at its tick through the same
  call.

**Positive control (the channel is real and the delivery works):**
`carrier/scripts/newgame_switch.txt` = `newgame.txt` + `300 switch out` /
`400 switch in`:

```
compare_digests.py iso_all.txt switch_posctrl.txt -> FIRST DIFFERENCE at tick T=300
```

exactly the scheduled tick.

**Record/replay round trip: EQUAL (876 ticks)** (`--inject-real-test` feeds the
scripted switch through Allegro's real `_win_switch_in`/`_win_switch_out`, so
the capture hook sees it exactly as a genuine WM_ACTIVATE would):

```
carrier.exe --det --pace=fast --input=real --inject-real-test \
   --input-script scripts/newgame_switch.txt --record-input rt_switch.txt \
   --digest-out rt_switch_rec.txt --stop-at-tick 1000
carrier.exe --det --pace=fast --input=script --input-script rt_switch.txt \
   --digest-out rt_switch_rep.txt --stop-at-tick 1000
compare_digests.py -> EQUAL (876 ticks)
```

**MEASURED, load-bearing, first attempt failed:** that round trip first
diverged at **T=301**. Cause: `_win_switch_out` also calls
`key_dinput_unacquire`, which releases every held key through
`_handle_key_release` - so the record run released the held `KEY_RIGHT` at
T=300 and the replay (which only re-ran the callbacks) did not. Fixed by
setting the recorder's `g_in_delivery` flag across the WHOLE switch handover,
so those releases are recorded as ordinary key events at the same tick; the
recording now shows `300 release KEY_RIGHT` next to `300 switch out` and the
round trip is EQUAL.

**Genuine SendInput session with two deliberate focus thefts: EQUAL (1872
ticks)** - `python carrier/scripts/desktop_perturb_test.py --test record-focus`
(a new harness that creates its OWN top-most helper window and activates it
with a real SendInput click, so no window of the operator's is ever touched):

```
recorded 407 events: 397 key, 1 switch, 9 time
VERDICT record-focus: EQUAL (1872 ticks)
```

**The environment fact that limits what a desktop perturbation can prove
here.** `--test script-focus` (the G1 workload at `--pace=real` while the
helper steals the foreground twice) is **EQUAL** to the undisturbed baseline -
but with the new `DET_TRACE_WNDMSG=1` diagnostic (subclasses the guest window,
logs every activation-class message, forwards unchanged) the reason is not
only the neutralization:

```
T=0   0x1c(WM_ACTIVATEAPP,1) 0x86(WM_NCACTIVATE,1) 0x06(WM_ACTIVATE,1) 0x07(WM_SETFOCUS)
T=139 0x86(WM_NCACTIVATE,0)  0x08(WM_KILLFOCUS)
T=200 0x86(WM_NCACTIVATE,1)  0x07(WM_SETFOCUS)
T=414 0x86(WM_NCACTIVATE,0)  0x08(WM_KILLFOCUS)
```

**On this host the guest window receives WM_ACTIVATE exactly once, at
creation, and never again**; every later foreground change arrives as
WM_NCACTIVATE + WM_SETFOCUS/WM_KILLFOCUS only. Allegro's `directx_wnd_proc`
acts only on WM_ACTIVATE (message 6, disasm 0x4793eb), so
`switchedFromProgram` is unreachable from the desktop in this configuration
and `hasFocus` stays 1 all run (`pf_inspect`: `hasFocus=1 lastFocus=1` at
T=126). Reproduced with the isolation fully off
(`DET_ISOLATE_OFF=window,switch`), with a normal-sized non-WS_EX_NOACTIVATE
window that genuinely held the foreground, and by minimizing/restoring the
guest window from another process. So the suppression half is proven by
construction plus the positive control above, not by a desktop perturbation -
stated as a limit, not glossed.

### C. Mouse (item 3)

**Census, which is what decided the policy** (the task's "find the readers of
mouse_x/mouse_y/mouse_b in game code"): across the whole binary exactly ONE
game function reads them - `main_menu_callback` (main.c): `mouse_b` (0x4e8cf8)
x4 at 0x410174/0x410186/0x4101ec/0x410e5a, `mouse_x` (0x4e8ce8) x1 at
0x410153, `mouse_y` (0x4e8cec) x1 at 0x410e3b - and it writes `lastMouseB`
(0x4dd268), which IS in the digest scope. Every other reader is Allegro's own
(mouse.c, gui.c's `default_mouse_*`). That single reader sits inside a branch
guarded by `pFLDAd != 0` (0x410140): it is the click hit-test on the fetched
**ad banner**.

**Decision, documented rather than assumed: PARK the mouse in every
carrier-owned run** (record and script alike) instead of recording it - there
is nothing worth recording, and parking removes one of the two suspects the
previous pass left open. Mechanism: the same 5-byte entry patch, on
`_handle_mouse_input` (0x45f9bc, `void f(void)`), which is the single path
from the driver's `mouse_dinput_handle` to the public `mouse_x/y/b` globals.
`mouse_dinput_handle` itself still runs, so the DirectInput buffer keeps being
drained (no spin, no overflow).

**Proof** (`desktop_perturb_test.py --test script-mouse`, G1 workload at
`--pace=real` with `--window=normal` so the window is really on screen, while
the helper drags the real cursor across it):

```
mouse events parked = 3769   (undisturbed run: 1)
EQUAL (876 ticks) vs the undisturbed baseline
```

**Negative control, and the honest limit** (`--test script-mouse-negctrl`,
same jiggle with `DET_ISOLATE_OFF=mouse` so the mouse is NOT parked): also
**EQUAL**. With ads suppressed `pFLDAd` is NULL (`pf_inspect`:
`pFLDAd=0x00000000`, `giAdCacheSize=0`) so `main_menu_callback`'s hit-test
branch is never entered and the mouse provably cannot reach the digest on this
workload. Parking it is defence in depth against a run where an ad IS present,
not a fix for a measured divergence.

### D. Recorded clock values (item 4)

`det_wrap_time` now has three cases, in priority order:

1. the loaded script carries `T time <v>` lines -> return them in order
   (a replay reproduces its recording's tower exactly);
2. `--record-input` is active -> answer from the REAL clock and append
   `T time <v>` to the recording;
3. otherwise -> the constant virtual epoch (1700000000 + virtual_ms/1000),
   exactly as milestones 5-7 defined it.

So **G1 is byte-identical to before this pass** for this channel (a script run
carries no `T time` lines). `time()` is called only 4 times in a G1 run and 9
times in a ~30 s SendInput session, so the cost is negligible. Only the guest
main thread may consume a recorded value; an off-thread call is counted,
logged and answered from the constant epoch (`time_offthread` in `--report`).

**Proof - two SendInput recordings 221 s apart, different towers, each replays
EQUAL:**

| session | first recorded `time` | `seed` | `rec_seed` | pinned RNG state | replay | desktop focus thefts |
|---|---|---|---|---|---|---|
| clk1 | 1788790382 | 3753.731673029 | **16509** | 960646151 | **EQUAL (1050 ticks)** | 2 |
| clk2 | 1788790603 | 18448.0 | **17211** | 1578811253 | **EQUAL (1252 ticks)** | 6 |
| G1 (constant epoch) | - | 22663.0 | 28469 | 993538943 | - | 0 |

(seeds read with `pf_inspect show <snap> --globals seed,rec_seed` from a
snapshot taken at each recording's first digest tick.)

Both sessions had the operator's desktop repeatedly take the foreground and
were still EQUAL - the residue "Divergences 004 and 005" left open ("4 of 8
sessions diverged, always ones where the foreground was taken") did not
reproduce once in the four genuine sessions of this pass.

**`clock()`/QPC/`timeGetTime` are proved irrelevant rather than cited.** Two
new opt-in diagnostics, in the style of `DET_DUMP_MEM_TICK`:
`DET_PERTURB_CLOCK=<ms>` offsets what those three return,
`DET_PERTURB_TIME=<secs>` offsets `time()`:

```
perturb_clock vs iso_all   EQUAL (876 ticks)              # negative control
perturb_time  vs iso_all   FIRST DIFFERENCE at tick T=126 # positive control
```

That upgrades `notes/replay_format.md`'s "the QPC/clock/time calls in play()
are anti-cheat telemetry" from a citation to a measurement, and is why those
three are left pinned to the virtual clock instead of being recorded.

### E. The determinism audit (item 5)

Full table with per-channel first-differing ticks:
**`notes/determinism_audit.md`**; artifacts in `artifacts/determinism_audit/`.
Runner: `carrier/scripts/audit_matrix.ps1` (per-channel knob
`DET_ISOLATE_OFF=<ad,mouse,switch,window>` turns exactly one isolation off) and
`carrier/scripts/desktop_perturb_test.py`. Headline rows:

```
iso_all vs iso_all2        EQUAL (876 ticks)           reproducible reference
no_ad   vs iso_all         FIRST DIFFERENCE at T=126   network ad thread
no_mouse vs iso_all        EQUAL                       mouse
no_switch vs iso_all       EQUAL                       window activation
no_window vs iso_all       EQUAL                       window show/foreground policy
perturb_clock vs iso_all   EQUAL                       clock()/QPC/timeGetTime
perturb_time  vs iso_all   FIRST DIFFERENCE at T=126   time()
pace_real{1,2,3}           EQUAL                       real-time pace x3
norestore{2,3} vs 1        EQUAL                       unrestored mutable files
switch_posctrl vs iso_all  FIRST DIFFERENCE at T=300   activation positive control
```

**The network ad fetch DOES reach the digest domain** (the task's question):
`fldads_start` (0x403ac8) is the binary's only `pthread_create` call site,
always with start routine `fldads_threadmain` (0x404014), and nothing joins it
(`pthread_join` is not imported at all). Five `fld_adspot.c` globals are inside
the 151-global scope: `giAdCacheSize`, `gpAdCache`,
`localFilename__fldads_get_local_cache_name`, `pFLDAdBitmap`, `pFLDAd`. New
wrapper `det_wrap_pthread_create` makes that one call a no-op in `--det`; the
game observes the constant "no ads" (`pFLDAd` stays NULL, no
socket/connect/recv appears in the import census). `pf_inspect diff` of two
snapshots at T=126 (suppressed vs live) names the four differing digest-scope
globals - the thread's own `localFilename__...` (empty vs `cache/ads.csv`) plus
three arena-pointer shifts (`swap_screen`, `data`, `menu_params+52`) caused by
its interleaved `malloc` calls.

**getenv: 16 calls over 3 distinct names per run, all unset on this host** -
`PRINTF_EXPONENT_DIGITS` x13 (`___mingw_pformat`), `SCREEN_GAMMA` x2
(`really_load_png`), `ALLEGRO` x1 (`find_allegro_resource`); zero call sites
in game code. A new `det_wrap_getenv` records name / call count / whether a
value was found into `--report`'s `environment.getenv_names` and forwards the
real value unchanged.

**Joystick: static.** `joyGetNumDevs` 1, `joyGetDevCapsA` 16, **`joyGetPosEx`
0** in a G1 run; `assets/log.txt` says `gamepad has 0 buttons`; `got_joystick`
(in the digest scope) reads 1. Enumerated once at startup, never polled.

**Still open, named:** the audio init result (Allegro voice ids reach
digest-scope globals; no experiment with the device removed), environment
variables on a differently-configured host, and the window thread's one
`malloc`+`free` into the shared deterministic arena (measured, bounded, fixed
at startup). See `notes/determinism_audit.md` "What remains STILL OPEN".

### New/changed options and files (this pass)

| what | where |
|---|---|
| `--interactive` (bare flag) | `main.cpp`, `det.hpp`, `det.cpp`, `scripts/play.py`, `carrier/scripts/sendinput_session.py` |
| `--window=normal / minnoactive / hidden` | `main.cpp`, `det.hpp`, `det.cpp` |
| 6 new always-installed wrappers: `ShowWindow`, `SetForegroundWindow`, `SetWindowPos`, `CreateWindowExA`, `pthread_create`, `getenv` | `imports.cpp` (`kWrapNames`), `wrappers.cpp` (`wrappers_lookup`), `det.cpp` |
| 3 new entry patches: `_switch_in`, `_switch_out`, `_handle_mouse_input` | `det.cpp` (`det_install_entry_patches`, `patch_entry_jmp`) |
| recording/script line shapes `T switch in|out` and `T time <secs>` | `det.cpp` (`load_script`, `deliver_due_input`, `drain_real_key_queue`, `det_wrap_time`) |
| `"environment"` object in `--report` | `trace.cpp`, `det_environment_json` |
| `DET_ISOLATE_OFF`, `DET_PERTURB_CLOCK`, `DET_PERTURB_TIME`, `DET_TRACE_WNDMSG` diagnostics | `det.cpp` |
| `DetSavedState.time_cursor` / `.switch_queue_*` | `det.hpp` (carrier.bin grows; snapshot dirs from older builds are not readable by this one) |
| `carrier/scripts/audit_matrix.ps1`, `carrier/scripts/desktop_perturb_test.py`, `carrier/scripts/newgame_switch.txt` | new |
| `notes/determinism_audit.md`, `artifacts/determinism_audit/` | new |

### Known gaps / open problems (this pass)

- **`switch out` cannot be produced from the desktop on this host** (part B):
  the OS delivers WM_NCACTIVATE/WM_SETFOCUS/WM_KILLFOCUS but not WM_ACTIVATE
  after window creation, and Allegro listens only to WM_ACTIVATE. The
  suppression half is therefore proven by construction plus the positive
  control, not by a desktop perturbation. A different backend
  (`--ddraw=system`) or another host may behave differently and should be
  re-measured with `DET_TRACE_WNDMSG=1`.
- **Audio init result is unmodelled** (`notes/determinism_audit.md` row 12):
  Allegro voice ids from `play_sample` land in digest-scope globals
  (`checkMusicVoiceID` reads 3 at T=126), so a host without a sound device
  would diverge. No experiment was run with the device removed.
- **The window thread makes 1 `malloc` + 1 `free`** into the shared
  deterministic arena at startup - a genuine cross-thread write to
  carrier-owned state, reproducible here but not modelled.
- **`getenv` values are forwarded**, so a host that sets `ALLEGRO` or
  `SCREEN_GAMMA` is not covered by any of this pass's proofs.
- **Old recordings stay invalid** and now additionally lack the `switch`/`time`
  lines, so they cannot be replayed under the new rules either.
- **`DET_TRACE_WNDMSG` subclasses the guest window** with
  `SetWindowLongA(GWL_WNDPROC, ...)`. It is a diagnostic, off by default, and
  the shutdown line reports the resulting wndproc so a subclass is never
  mistaken for the original.
