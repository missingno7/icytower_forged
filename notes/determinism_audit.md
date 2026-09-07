# Determinism audit — Icy Tower Win32 carrier (2026-09-07)

Scope: every host channel that can reach the per-tick verdict domain (the 151
game-owned globals of `carrier/gen/game_globals.inc`), audited on the binary
built by this "Environment isolation" pass. Every verdict below is a
**measurement**, not a citation: each row names the experiment that produced
it and the **first differing tick** (or EQUAL) it produced.

Workload for every row unless stated otherwise (this is gate G1):

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt \
            --stop-at-tick 1000 --run-seconds 90 --digest-out <D> --report <R>
```

876 consumed ticks, `play()` first reaches its safepoint at T=126. Assets are
restored from `artifacts/assets_backup/` before every launch
(`carrier/scripts/restore_assets.ps1`).

Runner: `carrier/scripts/audit_matrix.ps1` (per-channel knob
`DET_ISOLATE_OFF=<ad,mouse,switch,window>` turns exactly one isolation OFF so
a channel can be measured against the fully isolated reference) and
`carrier/scripts/desktop_perturb_test.py` (perturbs the real desktop on
purpose). Raw digests, reports and stderr: `artifacts/determinism_audit/`.

---

## Verdict table

| # | channel | verdict | first differing tick | evidence |
|---|---|---|---|---|
| 1 | **Window activation** (WM_ACTIVATE → `_win_switch_*` → `_switch_in/_switch_out` → the game's `switchedToProgram`/`switchedFromProgram`) | **SUPPRESSED** in `--input=script/none`, **RECORDED** in `--input=real` | positive control **T=300**; suppression **EQUAL** | A1, A2, A3, A4 |
| 2 | **Window show / foreground policy** (guest `ShowWindow`/`SetForegroundWindow`/`SetWindowPos`/`CreateWindowExA`, and the carrier's own `SetForegroundWindow`) | **DETERMINISTIC** (presentation only; below the §4a boundary) | **EQUAL** | B1, B2 |
| 3 | **Mouse** (DirectInput → `_handle_mouse_input` → `mouse_x/y/b`) | **SUPPRESSED** (parked in every carrier-owned run) | **EQUAL**, with and without parking | C1, C2, C3 |
| 4 | **Network ad fetch** (`pthread_create(fldads_threadmain)` → HTTP → `gpAdCache`/`pFLDAd`/…) | **SUPPRESSED** in `--det` | **T=126** — it *does* reach the digest | D1, D2 |
| 5 | **Unrestored mutable files** (`tower.cfg`, `profiles/`, `log.txt`) | **DETERMINISTIC** for this workload | **EQUAL** over 3 back-to-back runs with no restore | E1 |
| 6 | **Real-time pace** (`--pace=real` ×3) | **DETERMINISTIC** | **EQUAL** ×3, and EQUAL to `--pace=fast` | F1 |
| 7 | **Joystick** | **DETERMINISTIC** (static: no device) | n/a (`joyGetPosEx` count = 0) | G1 |
| 8 | **Wall clock `time()`** | **RECORDED** (replayed from the recording); constant epoch in script mode | positive control **T=126** | H1, H2, H3 |
| 9 | **`clock()` / QPC / `timeGetTime`** | **DETERMINISTIC** (proved irrelevant, not merely cited) | **EQUAL** under a 123 456 ms perturbation | H4 |
| 10 | **Other time/date imports** (`localtime`, `mktime`, `_ftime`, `GetLocalTime`, `GetSystemTime`, `strftime`, `asctime`) | **DETERMINISTIC** (never called in this workload; `localtime`/`mktime` only on profile/replay save) | n/a (call count 0) | I1 |
| 11 | **Environment variables** (`getenv`) | **DETERMINISTIC** on this host (all 3 names unset) — **STILL OPEN** as a channel | n/a | J1 |
| 12 | **Audio init result** | **STILL OPEN** | not controlled | K1 |
| 13 | **Window-thread writes to shared carrier/guest state** | **STILL OPEN** (bounded: 1 `malloc` + 1 `free` at startup) | not controlled | L1 |
| 14 | Allegro timer threads | DETERMINISTIC (parked; prior pass) | — | `carrier/NOTES.md` "parked timer thread" |
| 15 | Keyboard | SUPPRESSED (script) / RECORDED (real) — prior pass | — | divergence 002/005, closed |
| 16 | RNG (`rand`/`srand`) | DETERMINISTIC (LCG pinned in carrier memory) | — | `--rng-selftest`: 5000/5000 |
| 17 | Heap address layout | DETERMINISTIC (fixed-address free-list arena) | — | divergence 004, closed |

---

## Evidence

### A. Window activation

**A1 — the channel is real, and it is in the verdict domain.** `_switch_out`
(0x465808) and `_switch_in` (0x4657e4) are 35-byte `void f(void)` loops over
`switch_out_cb[8]` @0x4ea060 / `switch_in_cb[8]` @0x4ea080, reached only from
the two tail `jmp`s at the end of `_win_switch_out` (0x47a3d4) /
`_win_switch_in` (0x47a47c), which `directx_wnd_proc`'s WM_ACTIVATE arm calls
(0x479678 and 0x4793bb/0x47941f). MEASURED at run time — the tables hold
exactly one entry each:

```
det: switch_in_cb = 00406A6C 0 0 0 0 0 0 0      (switchedToProgram)
det: switch_out_cb= 00406A5C 0 0 0 0 0 0 0      (switchedFromProgram)
```

`src/icytower/main_state.c` (recovered source) shows what they do:
`hasFocus = 1` / `hasFocus = 0`. `hasFocus` (0x4bc020) and `lastFocus`
(0x4bc024) are both inside the 151-global digest scope, and `play()` compares
them at 0x411c6b and restarts the game music on a change (writing
`checkMusicVoiceID` @0x4bc174, also in scope).

**A2 — positive control (the delivery path works and the channel moves the
verdict).** `carrier/scripts/newgame_switch.txt` = `newgame.txt` plus
`300 switch out` / `400 switch in`:

```
python carrier/scripts/compare_digests.py iso_all.txt switch_posctrl.txt
   -> FIRST DIFFERENCE at tick T=300
```

Exactly the tick the switch-out was scheduled for.

**A3 — why `set_display_switch_mode` was NOT used** (the task offered it as an
alternative): RULED OUT BY EVIDENCE. `_win_switch_out`'s disassembly branches
on `get_display_switch_mode` only to decide whether to *also* reset an event
and drop the thread priority — **both arms end in the same `jmp _switch_out`**
(0x47a416 and 0x47a476), so no switch mode stops the callbacks. The game
already runs in SWITCH_BACKGROUND (`_win_reset_switch_mode` @0x47a508 calls
`set_display_switch_mode(3)`). The callback dispatchers are the only real
choke point, so that is where the carrier takes ownership — a 5-byte
`jmp rel32` entry patch to a carrier stub (no debug register is spent: DR0 is
the tick safepoint, DR1 the keyboard, DR2/DR3 are reserved for bind.cpp's
ORIGINAL-form sensing that gate G2 needs).

**A4 — record / replay round trip, EQUAL.** Recording format gained one line
shape, `T switch in|out`, delivered at the same tick-boundary handover point
keys use (divergence 005's rule) by running the guest's own callback table.

```
carrier.exe --det --pace=fast --input=real --inject-real-test \
    --input-script scripts/newgame_switch.txt \
    --record-input rt_switch.txt --digest-out rt_switch_rec.txt --stop-at-tick 1000
carrier.exe --det --pace=fast --input=script --input-script rt_switch.txt \
    --digest-out rt_switch_rep.txt --stop-at-tick 1000
compare_digests.py  ->  EQUAL (876 ticks)
```

The recording contains `1 switch in`, `300 switch out`, `400 switch in`.
`--inject-real-test` feeds the scripted switch through Allegro's real
`_win_switch_in/_win_switch_out`, so the capture hook sees it exactly as a
genuine WM_ACTIVATE would.

**MEASURED, load-bearing, first attempt failed:** that round trip first
diverged at **T=301**. Cause: `_win_switch_out` *also* calls
`key_dinput_unacquire`, which releases every held key through
`_handle_key_release` — so the record run released the held `KEY_RIGHT` at
T=300 and the replay (which only re-ran the callbacks) did not. Fixed by
setting the recorder's `g_in_delivery` flag across the WHOLE switch handover,
so those releases are recorded as ordinary key events at the same tick. After
the fix the recording shows `300 release KEY_RIGHT` next to `300 switch out`
and the round trip is EQUAL.

**A5 — genuine SendInput session with two deliberate focus thefts.**
`python carrier/scripts/desktop_perturb_test.py --test record-focus`: a
`--det --pace=real --input=real --interactive` session driving the real
DirectInput keyboard through SendInput, while a helper window (created by the
test, so no window of the operator's is touched) takes the foreground twice at
known times.

```
recorded 407 events: 397 key, 1 switch, 9 time
VERDICT record-focus:  EQUAL (1872 ticks)
```

This is the first genuine-keyboard session in this project that stayed EQUAL
**while the foreground was repeatedly taken from the guest** — the exact
residue `carrier/NOTES.md` "Divergences 004 and 005" left open.

**A6 — the neutralized direction, and an environment fact that limits what
the desktop can prove here.** `--test script-focus` runs the G1 workload at
`--pace=real` while the same helper steals the foreground twice: **EQUAL** to
the undisturbed `--pace=fast` baseline. But the honest reading needs one more
measurement. With `DET_TRACE_WNDMSG=1` (a diagnostic that subclasses the guest
window and logs every activation-class message it receives, forwarding
unchanged), a run in which the helper demonstrably took and returned the
foreground shows:

```
T=0   msg=0x1c(WM_ACTIVATEAPP,1)  0x86(WM_NCACTIVATE,1)  0x06(WM_ACTIVATE,1)  0x07(WM_SETFOCUS)
T=139 msg=0x86(WM_NCACTIVATE,0)   0x08(WM_KILLFOCUS)
T=200 msg=0x86(WM_NCACTIVATE,1)   0x07(WM_SETFOCUS)
T=414 msg=0x86(WM_NCACTIVATE,0)   0x08(WM_KILLFOCUS)
```

i.e. **on this host the guest window receives WM_ACTIVATE exactly once, at
creation, and never again** — every later foreground change arrives as
WM_NCACTIVATE + WM_SETFOCUS/WM_KILLFOCUS only. Allegro's `directx_wnd_proc`
acts *only* on WM_ACTIVATE (message 6, disasm 0x4793eb), so
`switchedFromProgram` is unreachable from the desktop in this
configuration and `hasFocus` stays 1 for the whole run (confirmed by
`pf_inspect`: `hasFocus = 1`, `lastFocus = 1` at T=126). Reproduced with the
isolation fully OFF (`DET_ISOLATE_OFF=window,switch`) and with the guest
window normal-sized (0,0,640,480), non-`WS_EX_NOACTIVATE`, and genuinely
foreground; also reproduced by minimizing/restoring the guest window from
another process.

Consequences, stated plainly:

- the desktop-perturbation test cannot, on this host, produce a `switch out`;
  the `script-focus` EQUAL is therefore consistent with neutralization but is
  not by itself proof of it. The proof of the mechanism is A2 (delivering the
  event *does* change the digest at exactly its tick) plus the fact that the
  only path into the game's callbacks is now the carrier's own stub;
- the "WM_ACTIVATEAPP switch-out/in path" suspect that
  `carrier/NOTES.md` "Divergences 004 and 005" recorded for the 4 diverging
  human sessions is **not** reachable on this host+backend. The other suspect,
  the real mouse, is real as a *volume* of events (row 3) but also cannot
  reach the digest while ads are suppressed (C3). Neither is convicted; both
  are now owned by the carrier regardless.

### B. Window show / foreground policy

**B1 — no automated run takes the operator's foreground.** MEASURED:
`create_directx_window` (0x478eb8) creates the window with `dwStyle=0xCA0000`
(no `WS_VISIBLE`) and then calls `ShowWindow(hwnd, SW_SHOWNORMAL)`,
`SetForegroundWindow(hwnd)`, `UpdateWindow(hwnd)`; `set_video_mode`
(wddmode.c) repeats the `ShowWindow` + `SetForegroundWindow` pair. Without
`--interactive` the carrier now substitutes `SW_SHOWMINNOACTIVE` (or `SW_HIDE`
under `--window=hidden`), suppresses `SetForegroundWindow` (returns TRUE),
ORs `SWP_NOACTIVATE` into `SetWindowPos`, and adds `WS_EX_NOACTIVATE` at
`CreateWindowExA`. The carrier's own `try_focus_guest_window_once` is now
gated on `--interactive` instead of `input_policy==Real`.
`--report`'s `environment` object for a G1 run:
`"showwindow_substituted": 2, "setforeground_suppressed": 2`.

**B2 — G1 is unchanged, in all three window modes.**

```
win_hidden vs iso_all   EQUAL (876 ticks)     # cnc-ddraw initialises fine with SW_HIDE
win_normal vs iso_all   EQUAL (876 ticks)
no_window  vs iso_all   EQUAL (876 ticks)     # policy removed entirely
```

DirectDraw did **not** refuse a hidden window, so the documented
`SW_SHOWMINNOACTIVE` fallback was not needed — it is nevertheless the default,
because a minimized window is easier to notice and to reason about than an
invisible one.

### C. Mouse

**C1 — census (the task's question: who reads `mouse_x`/`mouse_y`/`mouse_b`
in game code).** Across the whole binary, exactly ONE game function reads
them: `main_menu_callback` (main.c) — `mouse_b` (0x4e8cf8) ×4 at 0x410174,
0x410186, 0x4101ec, 0x410e5a; `mouse_x` (0x4e8ce8) ×1 at 0x410153; `mouse_y`
(0x4e8cec) ×1 at 0x410e3b — and it writes `lastMouseB` (0x4dd268), which IS in
the digest scope. Every other reader is Allegro's own (`mouse.c`, `gui.c`'s
`default_mouse_*`). That single reader sits inside a branch guarded by
`pFLDAd != 0` (0x410140): it is the click hit-test on the fetched **ad
banner**. Decision, therefore: **park the mouse in every carrier-owned run**
(record and script alike) rather than record it — there is nothing worth
recording, and it removes one of the two suspects the previous pass left open.
Mechanism: a 5-byte entry patch on `_handle_mouse_input` (0x45f9bc, `void
f(void)`), the single path from `mouse_dinput_handle` to the public
`mouse_x/y/b` globals. `mouse_dinput_handle` itself still runs, so the
DirectInput buffer keeps being drained.

**C2 — a helper moving the real cursor over the guest window changes
nothing.** `desktop_perturb_test.py --test script-mouse` runs the G1 workload
at `--pace=real` with `--window=normal` (window genuinely on screen) while the
test drags the cursor across it 198 times:

```
mouse events parked = 3769   (undisturbed run: 1)
EQUAL (876 ticks) vs the undisturbed baseline
```

**C3 — negative control, and an honest limit.** The same jiggle with the mouse
NOT parked (`DET_ISOLATE_OFF=mouse`, 199 cursor sweeps) is also **EQUAL**. So
on this workload the mouse provably cannot reach the digest — because the ad
thread is suppressed, `pFLDAd` is NULL (`pf_inspect`: `pFLDAd = 0x00000000`,
`giAdCacheSize = 0`) and `main_menu_callback`'s hit-test branch is never
entered. Parking the mouse is defence in depth against a run where an ad *is*
present, not a fix for a measured divergence.

### D. Network ad fetch

**D1 — it reaches the verdict domain.** `fldads_start` (0x403ac8) is the
binary's ONLY `pthread_create` call site, always with start routine
`fldads_threadmain` (0x404014); nothing ever joins it (`pthread_join` is not
imported — the only pthreadGC2 imports are create / mutex_lock /
mutex_unlock). Five `fld_adspot.c` globals are inside the 151-global digest
scope: `giAdCacheSize` (0x4dd020), `gpAdCache` (0x4dd024),
`localFilename__fldads_get_local_cache_name` (0x4dd040), `pFLDAdBitmap`
(0x4dd30c), `pFLDAd` (0x4dd310). So the answer to the task's question is
**yes**, and it is stubbed in det mode.

**D2 — measured, both directions.**

```
no_ad vs iso_all      FIRST DIFFERENCE at tick T=126     # the ad thread DOES move the digest
iso_all vs iso_all2   EQUAL (876 ticks)                  # with it suppressed, the run is reproducible
```

`pf_inspect diff` of two snapshots at T=126 (ad thread suppressed vs live)
names the differing globals:

```
FIRST DIFFERING GLOBAL INSIDE THE DIGEST SCOPE:
  localFilename__fldads_get_local_cache_name @0x004dd040  A='' vs B='cache/ads.csv'
4 digest-scope globals differ: localFilename__…, swap_screen, data, menu_params(+52)
```

i.e. the thread's own state *plus* three arena-pointer shifts caused by its
interleaved `malloc` calls. Note the honest deviation from the task's
expectation ("then G1 must stay EQUAL"): suppressing the ad thread **does**
change the digest stream, because the live thread was writing into the
verdict domain. G1's contract — two independent runs of the same build are
EQUAL — is unaffected and still holds (876 ticks); what changed is the
absolute digest values, which `notes/living_record.md` already records as
comparable only within one carrier build.

The guest's observable result becomes the constant "no ads":
`WSAStartup` is still called once, but no `socket`/`connect`/`recv` call
appears in the `--report` import census, and `pFLDAd` stays NULL.

### E. Unrestored files

Three G1 runs back to back with **no** asset restore between them
(`norestore1/2/3`): `EQUAL (876 ticks)` for 2-vs-1 and 3-vs-1. So for this
workload the game's own writes to `tower.cfg`, `profiles/` and `log.txt` are
idempotent and the restore convention is precautionary rather than load
bearing. It is still load bearing against the *concurrent* teardown race
`carrier/NOTES.md` documents (a just-finished carrier child still rewriting
`assets/profiles/` while the next run's restore runs), which
`restore_assets.ps1` waits out.

### F. Real-time pace

```
pace_real1 vs iso_all      EQUAL (876 ticks)     # --pace=real == --pace=fast
pace_real2 vs pace_real1   EQUAL (876 ticks)
pace_real3 vs pace_real1   EQUAL (876 ticks)
```

By construction: `--pace` only decides whether `det_wrap_Sleep` also calls the
real `::Sleep(ms)`; T comes from the virtual clock either way.

### G. Joystick

`--report` import counts for a G1 run: `joyGetNumDevs` 1, `joyGetDevCapsA` 16,
**`joyGetPosEx` 0**. `assets/log.txt` says `gamepad has 0 buttons`, and
`got_joystick` (0x4f8b08, in the digest scope) reads 1 at T=126. So the
joystick is enumerated once at startup and never polled during the workload:
static, and constant across runs on this host. (A machine with a real gamepad
attached would make `poll_joystick`, called from `poll_control` every tick,
a live channel — not audited here.)

### H. Clocks

**H1 — `time()` reaches the verdict domain (positive control).**
`DET_PERTURB_TIME=86400` (a diagnostic env var added by this pass) shifts only
what `time()` returns:

```
perturb_time vs iso_all    FIRST DIFFERENCE at tick T=126
```

That is the first digest line of the run: a different `time()` gives a
different `srand()` seed and therefore a different tower.

**H2 — it is now recorded, not pinned, when a recording is being made.**
`det_wrap_time` has three cases in priority order: replay a `T time <v>` line
from the loaded script; else, if `--record-input` is active, answer from the
REAL clock and append `T time <v>`; else return the constant virtual epoch
(1700000000 + virtual_ms/1000) exactly as before. G1 (a script run with no
recorded values) is therefore byte-identical to before this pass. `time()` is
called only 4 times in a G1 run and 9 times in a ~30 s SendInput session, so
the recording cost is negligible. Only the guest main thread may consume a
recorded value; an off-thread call is counted, logged and answered from the
constant epoch (`time_offthread` in `--report`).

**H3 — two SendInput recordings made minutes apart.** See "Clock round trip"
below.

**H4 — `clock()` / QPC / `timeGetTime` do NOT reach the verdict domain.**
`DET_PERTURB_CLOCK=123456` shifts all three by 123 456 ms:

```
perturb_clock vs iso_all   EQUAL (876 ticks)
```

This upgrades `notes/replay_format.md`'s "the QPC/clock/time calls in play()
are anti-cheat telemetry" from a citation to a measurement, and is why they
are left pinned to the virtual clock instead of being recorded.

### I. Other time/date imports

Call-site census (`artifacts/disasm.txt`): `localtime` 4 sites
(`create_profile`, `save_profile`, `save_replay`, `timegm`), `mktime` 3 (all
inside `timegm`). `_ftime`, `strftime`, `asctime`, `difftime`, `GetLocalTime`,
`GetSystemTime`, `GetSystemTimeAsFileTime`, `GetTimeZoneInformation`,
`GetTickCount` have **no call site at all**. Runtime: none of them appears in
a G1 run's `--report` import census (count 0), i.e. they are reached only when
a profile or a replay is saved, which this workload never does. They consume
`time()`'s value, which is already recorded (H2).

### J. Environment variables

The task asked "getenv 13 calls: which?". MEASURED per G1 run — 16 calls over
3 distinct names, and **every one of them is unset on this host**:

| name | calls | host value | caller |
|---|---:|---|---|
| `PRINTF_EXPONENT_DIGITS` | 13 | NULL | `___mingw_pformat` (CRT) |
| `SCREEN_GAMMA` | 2 | NULL | `really_load_png` (vendored loadpng) |
| `ALLEGRO` | 1 | NULL | `find_allegro_resource` (Allegro) |

(The static census has 6 call sites: `___mingw_pformat`, `al_assert`,
`al_trace`, `find_allegro_resource` ×2, `really_load_png` — **zero from game
code**.) Verdict: DETERMINISTIC on this host, but recorded as STILL OPEN
because the carrier forwards the real value: a host that sets `ALLEGRO` or
`SCREEN_GAMMA` would change resource lookup / PNG gamma. The
`--report` `environment.getenv_names` array carries name, call count and
whether a value was found, so a future divergence can be attributed.

### K. Audio init result

Not controlled, and it can reach the verdict domain: `DirectSoundCreate` is
called twice and `midiOutOpen` once in a G1 run, and the digest-scope globals
`checkMusicVoiceID` (0x4bc174, reads 3 at T=126), `gameMusicVoiceID`,
`combo_sound`, `jump_sound`, `sounds`, `menu_sounds`, `speaker`, `bg_beat`,
`bg_menu` hold Allegro voice ids returned by `play_sample`. A host with no
sound device (or a different mixer voice count) would make `install_sound`
fail and those ids differ. **STILL OPEN**: no experiment was run with the
audio device removed, and no wrapper records the init result.

### L. Window / DirectInput thread writes

The `--report` thread breakdown for a G1 run shows exactly three threads:
the guest main thread, Allegro's window thread, and the parked timer thread.

- window thread (`wnd_thread_proc`): `CreateWindowExA`, `RegisterClassA`,
  the `MsgWaitForMultipleObjects`/`GetMessage`/`PeekMessage`/`DispatchMessage`
  pump, `GetKeyboardState` ×2, `SetEvent` ×9, `BeginPaint`/`EndPaint` ×4 —
  and **`malloc` ×1 + `free` ×1**, i.e. it does touch the shared deterministic
  arena. Measured as a fixed startup pair, so it is reproducible in practice,
  but it is a genuine cross-thread write to carrier-owned state. **STILL
  OPEN** (bounded).
- timer thread: `QueryPerformanceCounter` ×2 + one `WaitForSingleObject`, all
  before it parks. No state writes.
- the DirectInput "input thread" (`input_thread_proc`) is **never spawned** in
  this build (measured in a previous pass and unchanged here);
  `key_dinput_handle_scancode` and `mouse_dinput_handle` run from the window
  thread's own event-handler table (`*0x50aac8(,%eax,4)` at 0x4791b0), which
  is why both are captured/parked there.

---

## Clock round trip (item 4's proof)

Two `--det --pace=real --input=real --interactive` SendInput sessions
(`carrier/scripts/sendinput_session.py`), made minutes apart, each recorded
with `--record-input` and replayed with `--input=script`:

| session | first recorded `time` | `seed` (0x4ff108) | `rec_seed` (0x4fe7a8) | pinned RNG state | replay verdict | desktop focus thefts during the session |
|---|---|---|---|---|---|---|
| clk1 | 1788790382 | 3753.731673029 | **16509** | 960646151 | **EQUAL (1050 ticks)** | 2 |
| clk2 | 1788790603 | 18448.0 | **17211** | 1578811253 | **EQUAL (1252 ticks)** | 6 |
| G1 (constant epoch) | — (none) | 22663.0 | 28469 | 993538943 | — | 0 |

The two sessions are 221 s apart, get **different seeds and different towers**,
and each replays byte-exactly against its own recording (seeds read with
`pf_inspect show <snap> --globals seed,rec_seed` from a snapshot taken at each
recording's first digest tick; raw output in
`artifacts/determinism_audit/clk{1,2}_seed.txt`). That is the behaviour
`notes/living_record.md`'s FOLLOW-UP entry asked for ("record the observed
time()/clock values as events in record mode and replay them, so interactive
sessions vary while replays stay exact").

Both sessions had the operator's desktop repeatedly take the foreground (2 and
6 thefts) and were **still EQUAL** — the residue `carrier/NOTES.md`
"Divergences 004 and 005" left open ("a genuine-keyboard session is only EQUAL
when the desktop leaves the guest alone; 4 of 8 sessions diverged") did not
reproduce once in the four genuine sessions run in this pass
(`record_focus`, `clk1`, `clk2`, plus the earlier `script-*` perturbation
runs).

G1 is unchanged: a script run carries no `T time` lines, so the constant epoch
is still used and the digest stream is byte-identical to the pre-pass one for
every channel except the ad thread (D2).

---

## What remains STILL OPEN

1. **Audio init result** (row 12) — reaches the digest through Allegro voice
   ids; no wrapper records it and no no-sound-device experiment was run.
2. **Environment variables** (row 11) — inert on this host (all three names
   unset) but forwarded, so a differently-configured host is not covered.
3. **The window thread's one `malloc`/`free`** (row 13) — a cross-thread write
   into the deterministic arena; fixed and reproducible here, unmodelled in
   general.
4. **`switch out` cannot be produced from the desktop on this host** (A6), so
   the *suppression* half of the activation channel is proven by construction
   and by the positive control, not by a desktop perturbation.
5. **A real gamepad** would make `poll_joystick` (called from `poll_control`
   every tick) a live channel; not audited.
6. **COM/host-object identity** (DirectDraw surfaces, DirectSound buffers,
   DirectInput devices) is still not virtualized — unchanged from
   `carrier/NOTES.md` "Milestones 8-9"; it is what blocks cross-process
   snapshot restore.
