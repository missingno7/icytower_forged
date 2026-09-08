# icytower15.exe — static-lifting recon notes

Target: `assets/icytower15.exe`, Icy Tower 1.5.1, PE32, ImageBase 0x400000, entry RVA
0x1110 (VA 0x401110), no relocations, no TLS, MinGW GCC 4.4.1, statically-linked
Allegro 4.4 (Win32 driver set) + libvorbis/libogg + libpng + zlib (dynamic, zlib1.dll)
+ pthreads-win32 (dynamic, pthreadGC2.dll). Full DWARF2 debug info + COFF symbol
table (11634 raw table lines / 8880 parsed symbol records) intact.

Every claim below is tagged **KNOWN** (seen directly in disassembly/DWARF/COFF,
address given) or **INFERRED** (reasoned from KNOWN facts / general Allegro 4
source knowledge, not directly re-verified in this pass).

Bulky data lives in `artifacts/`:
- `functions.json` — 2465 functions {name, va, size, compile_unit, origin}
- `compile_units.txt` — 148 DWARF CUs + producer + origin, plus CRT objects with
  COFF symbols but no DWARF CU
- `coff_symbols.json`, `dwarf_subprograms.json`, `dwarf_cus.json` — raw parsed data
- `objdump_x.txt`, `objdump_t.txt`, `dwarf_info.txt`, `disasm.txt` — full raw dumps
  produced by objdump (disasm.txt = full linear disassembly, 12.6MB)
- `tools_recon/*.py` — rerunnable parsers (parse_coff.py, parse_dwarf_info.py,
  build_functions.py)

## 1. Function/CU counts (KNOWN)

2465 total functions (1858 with full DWARF subprogram low_pc/high_pc coverage,
607 recovered from COFF-only symbols with no debug info — mostly CRT/libgcc
thunks and import stubs). Origin split (function count / .text bytes / % of
.text's 761928 bytes):

| origin      | funcs | bytes  | % of .text |
|-------------|------:|-------:|-----------:|
| allegro     |  1531 | 468724 | 61.5%      |
| other (vorbis/ogg mostly) | 311 | 129296 | 17.0% |
| game        |   253 | 125641 | 16.5%      |
| libgcc/crt  |   370 |  27787 |  3.6%      |

148 DWARF compile units: 25 game CUs (`F:\projects\icytower\trunk\source\*.c`),
~115 Allegro CUs (`C:\Lib\allegro4\src\**\*.c`, incl. `src\win\*.c` Windows
drivers and the `logg.c` addon), 2 libvorbis/libogg CUs compiled with a
*different* toolchain (`GNU C 4.2.1-sjlj (mingw32-2)` vs. the rest's 4.4.1 —
these were prebuilt library objects), and a handful of libgcc/mingw-runtime
CUs. libvorbis/libogg contributes ~26 more source files that only have COFF
symbols (no DWARF) — full encoder+decoder (analysis.c, mdct.c, floor0/1.c,
res0.c, psy.c, block.c, etc.) is linked in via Allegro's `logg` addon.

## a. Entry / startup chain (KNOWN, full call chain traced in disasm.txt)

```
0x401110 _WinMainCRTStartup           (entry point)
  -> SetThreadLocale(2)
  -> 0x401020 ___mingw_CRTStartup
       -> SetUnhandledExceptionFilter(0x401150 __gnu_exception_handler)
       -> ___cpu_features_init, __fpreset
       -> ___getmainargs
       -> __pei386_runtime_relocator (0x4b24f0)   [VirtualProtect x2 here, see (m)]
       -> ___main (0x4b2720)                       [runs C static-init list]
       -> 0x4b2740 _main  (mingw-generated main.c wrapper)
            -> GetCommandLineA, GetStartupInfoA, GetModuleHandleA
            -> 0x406cb0 WinMain@16  (game main.c, Allegro-mangled)
                 pushes &_mangled_main (0x415f10, game's real main(), main.c)
                 tail-calls:
                 -> 0x4628d8 Allegro `_WinMain` (allegro.c) — parses argv,
                    invokes the passed function pointer == game's main()
       -> __cexit -> ExitProcess
```
`_mangled_main` (0x415f10, main.c, 1938 bytes) is the game's actual `main()`
after Allegro's `END_OF_MAIN()`/mangling macros. Its first actions (KNOWN,
disasm 0x415f10-0x415ffe): `LoadLibraryA("exchndl.dll")` (optional crash-dump
helper — on failure just skips it, jumps to 0x416668, no hard failure),
`install_allegro_version_check`, `register_png_file_type`,
`get_executable_name`+`replace_filename`+`chdir` (cwd = exe dir),
`get_logfile_path`+`fopen` (opens **log.txt** for writing, matches the log.txt
seen in assets/). From here it proceeds into `init_game()` (0x40e7dc) and the
menu/gameplay loop (see item l).

## b. Threads (KNOWN — exactly 5 non-main threads possible, all found by
exhaustive search of `__beginthread`/`pthread_create` call sites; `CreateThread`
is not imported and never called directly)

`__beginthread` (0x4baf80, mingw wrapper around `CreateThread`) has exactly
**4** call sites in the whole binary:

| caller (Allegro fn)                 | spawns                              | CU |
|--------------------------------------|--------------------------------------|----|
| `tim_win32_low_perf_init` 0x478350   | `tim_win32_low_perf_thread` 0x4783bc | wtimer.c |
| `tim_win32_high_perf_init` 0x4784cc  | `tim_win32_high_perf_thread` 0x478584| wtimer.c |
| `init_directx_window` 0x478b7c       | `wnd_thread_proc` 0x4790b8           | wwnd.c |
| `_win_input_register_event` 0x4799b4 | `input_thread_proc` 0x479a40         | winput.c |

Only one of the two timer threads is active at once (perf-counter probe picks
high- or low-res). `pthread_create` (imported from pthreadGC2.dll, IAT slot
`__imp__pthread_create` @0x514a58) has exactly **1** call site:
`fldads_start` (0x403ac8, game `fld_adspot.c`) calls it indirectly
(`call *0x514a58`) with entry point `fldads_threadmain` (0x404014,
fld_adspot.c) and NULL arg, storing the handle in `gFLDADThread`. This is
called once from `init_game()` (0x40ea13) — the ad-listing HTTP fetch thread.

So at gameplay time there are up to **5 background threads**: window-message
thread, timer thread (one of two variants), input thread, and (only near
startup, short-lived) the ad-fetch pthread.

**Key finding (KNOWN): the Allegro timer thread does not run gameplay logic.**
Game code installs exactly 2 interrupt callbacks via Allegro's
`install_int`/`install_int_ex`, both trivial counter bumps, both in game
`timer.c`:
```
0x41fee4 install_timers():
    install_timer()
    install_int(fps_counter,   1000)   ; 0x41fea4, once/sec
    install_int(cycle_counter, 20)     ; 0x41fed4, every 20ms
```
- `cycle_counter` (0x41fed4): `++*(int*)0x506938` — increments a single global
  dword every 20ms. Nothing else.
- `fps_counter` (0x41fea4): swaps two small FPS-accounting counters at
  0x506978/0x506948 and 0x506958/0x506968 once a second — display-only stats.

All other `install_int`/`install_int_ex` call sites found (21 total, listed in
`artifacts/disasm.txt`) belong to **Allegro internals**, not game code:
keyboard.c (key-repeat), joystick.c (dead-zone poll), midi.c/digmid.c
(software MIDI sequencer tick), gui.c (dialog blink), timer.c
(`install_timer` bootstrap itself), mouse.c (cursor blink), wtimer.c/wthread.c
(the timer/window thread's own internal service ticks). None of these touch
gameplay state.

The global at **VA 0x506938** (the 20ms tick count) is read/reset ~50 places
across `main.c`'s menu and gameplay code (init_game, play, main_menu_callback,
do_replay_menu, etc.) — it is the single frame-pacing signal the main thread
polls (see item l for the exact wait loop). Thread synchronization between the
window thread and main thread uses Win32 critical sections
(`EnterCriticalSection`/`LeaveCriticalSection`, e.g. 0x47efbf in wthread.c) and
a `CreateEventA`-based waitable event (0x47ee8c in wthread.c), standard
Allegro `wthread.c` primitives — not seen used by game code directly.

## c. Time sources (KNOWN)

- `timeGetTime` — called only inside `tim_win32_low_perf_thread` (wtimer.c,
  0x4783ca/0x4783d8/0x47842b/0x478453/0x47845e). Never called by game code.
- `QueryPerformanceCounter`/`QueryPerformanceFrequency` — called in
  `tim_win32_high_perf_thread`/`_init` (wtimer.c, 0x478598 etc.) **and directly
  by game code**: 6 call sites inside `play()` (0x411b30, 0x412c7c, 0x413075,
  0x41361e, 0x41431a, 0x414430). These are one-shot session-timing/statistics
  reads (session start/end timestamps for hiscore/anti-cheat bookkeeping,
  alongside a `time()` and a `clock()` call right next to the first one at
  0x411b1f/0x411b42) — not a per-frame pacing source.
- `Sleep` — 3 direct call sites (CRT/Allegro internal); the gameplay wait loop
  uses Allegro's `rest()` wrapper (0x45dea8) instead, which itself calls Sleep.
- **The actual frame-pacing mechanism (KNOWN, exact code at 0x4132aa in
  `play()`):**
  ```
  4132aa: mov eax,[0x506938]      ; cycle_counter
  4132af: test eax,eax
  4132b1: jne  4124f4             ; nonzero -> go run a frame
  4132b8: rest(2)                 ; Sleep(2ms)
  4132c4: mov eax,[0x506938]
  4132c9: je 4132b8               ; still zero -> sleep again
  4132cd: jmp 4124f4              ; -> run a frame
  ```
  i.e. the main thread busy-waits in 2ms `Sleep` slices until the 20ms Allegro
  timer tick has incremented `*(int*)0x506938`, then proceeds to run exactly
  one game-logic frame and (implicitly) resets/consumes the counter. **This
  makes 0x506938 the smallest deterministic-time interception point**: forcing
  its value directly (or intercepting the `cycle_counter`/`install_int` timer
  callback) fully controls game time without touching Sleep/QPC at all.

## d. Input (KNOWN)

- **Keyboard**: exclusively via DirectInput. `wkeybd.c` only contains
  `key_dinput_*`/`key_directx_*` functions (0x46cd54-0x46d9d0) — no
  `GetAsyncKeyState`/`GetKeyboardState`/`ToAscii` fallback is compiled in.
  `key_dinput_handle_scancode` (0x46d5a8) is the DirectInput callback that
  fills Allegro's `key[]` array; this runs on the input thread
  (`input_thread_proc`, winput.c), asynchronously from the main thread.
- Game code (`control.c`) never calls `poll_keyboard`; `poll_control()`
  (0x401958, control.c) only calls Allegro's `poll_joystick()` (0x43e654) each
  frame — keyboard state is kept current by the async DirectInput thread, and
  `is_up`/`is_down`/`is_left`/`is_right`/`is_fire`/`is_pause`/`is_enter`/
  `is_any`/`check_control_key` (0x401844-0x401958) directly index the `key[]`
  array. `init_control`/`load_control`/`save_control` handle key-binding
  config (tower.cfg).
- **Joystick**: two compiled drivers, `_joystick_directx` (wjoydx.c, DirectInput)
  and `_joystick_win32` (wjoyw32.c, legacy `joyGetPosEx`); Allegro autodetects
  at `install_joystick()`. `fld's `gamepad.txt` (string evidence,
  "gamepad.txt is missing, setting defaults") supplies a custom button-mapping
  layer game-side (see `get_gamepad`/`get_gamepad_value` in control.c/main.c).
- **Mouse**: `_mouse_directx` (wmouse.c) DirectInput driver; game barely uses
  it (menu navigation only, string evidence "no gamepad or joystick found,
  play with keyboard only" confirms controls are keyboard-first).

## e. Window, callbacks, driver structs (KNOWN)

- Window is created and pumped on its **own thread**, confirmed:
  `init_directx_window` (wwnd.c) spawns `wnd_thread_proc` (0x4790b8) via
  `__beginthread`; `create_directx_window` (0x478eb8) does the
  `RegisterClass`/`CreateWindowEx` inside that thread.
- WNDPROC: `directx_wnd_proc@16` (0x4791e0, wwnd.c) is the registered window
  procedure (1280 bytes, big switch on message id — WM_ACTIVATE,
  WM_CLOSE/WM_DESTROY, WM_SYSCOMMAND, etc). `_user_wnd_proc` (0x4ec0a0, a data
  pointer) lets game code hook close/activate events —
  `switchedFromProgram`/`switchedToProgram`/`clickedCloseButton` in main.c are
  registered this way (Allegro's `set_window_close_hook` family).
- `SetTimer` is called once (0x4796b4, wwnd.c) — a WM_TIMER nudge so the
  window thread's message pump doesn't block indefinitely in `GetMessage`.
- `SetUnhandledExceptionFilter` — set once, at CRT startup (0x40102e) to
  `__gnu_exception_handler` (0x401150), the mingw-runtime SEH→C-`signal()`
  translator (SIGFPE/SIGSEGV/SIGILL/SIGTRAP, see item m). Not game code.
- `atexit` — called 3x, all inside mingw CRT/DLL-support glue
  (0x4b26f1/0x4b735c/0x4bb00e), not from game code.
- Compiled-in Allegro Win32 driver structs (data symbols, all found via COFF):
  **GFX**: `_gfx_directx_accel`, `_gfx_directx_soft`, `_gfx_directx_safe`,
  `_gfx_directx_win`, `_gfx_directx_ovl`, `_gfx_gdi`.
  **DIGI**: `_digi_directsound`, `_digi_none`.
  **MIDI**: Allegro's built-in software wavetable player (`digmid.c` CU
  present) driving through the DIGI driver — no native MCI/native-MIDI driver
  symbol found.
  **KEYBOARD**: `_keyboard_directx`. **MOUSE**: `_mouse_directx`.
  **JOYSTICK**: `_joystick_directx`, `_joystick_win32`.
  **TIMER**: `_timer_win32_high_perf`, `_timer_win32_low_perf`.
  **SYSTEM**: `_system_directx`.
  Driver-list arrays (`__gfx_driver_list`, `__digi_driver_list`,
  `__midi_driver_list`, `__keyboard_driver_list`, `__joystick_driver_list`,
  `__timer_driver_list`, `__system_driver_list`) exist at fixed VAs (all in
  `cygming-crtend.c`'s data area / .data, per COFF) — this is the standard
  Allegro `vtable.c` driver-registration table, all runtime-selected by
  autodetect, no other platforms compiled in (no X11/Linux/DOS drivers, this
  is a Windows-only build).

## f. Dynamic loading (KNOWN — exhaustive `LoadLibraryA` call-site search, 5 total)

| call site | DLL string | caller | on failure |
|-----------|-----------|--------|------------|
| 0x415f29 | `exchndl.dll` | `_mangled_main` (game main.c, startup) | branches around exchndl-specific init (0x416668), continues normally — optional crash-dump helper only |
| 0x47c4e3 | `DDRAW.DLL` | `get_dx_ver` (Allegro wdxver.c) | version-probe fails, DirectX-version detection falls back / assumes older DX |
| 0x47c568 | `DINPUT.DLL` | `get_dx_ver` | same |
| 0x47c6a3 | `DSETUP.DLL` | `get_dx_ver` | same |
| 0x47c70d | `DINPUT.DLL` (2nd path) | `get_dx_ver` | same |

`GetProcAddress` is called 7 times total, all inside the same small cluster of
Allegro DirectX-version-probing / driver-capability code (`get_dx_ver`,
`wjoyhelp.c`-style helpers) — used to pull `DllGetVersion` or similar
version-query exports out of the just-loaded DLL. No other DLLs
(no `libgcj_s.dll`, no plugin loading) are loaded dynamically by this binary
at runtime; DDRAW.DLL/DINPUT.DLL/DSOUND.DLL/GDI32/etc. used for actual
gameplay are regular *import-table* (static) imports (see imports.json), not
LoadLibrary'd.

## g. Graphics (INFERRED driver selection logic from Allegro source + KNOWN
compiled-in set from item e / imports)

Compiled-in gfx drivers are the DirectDraw family (`_gfx_directx_*`) and GDI
(`_gfx_gdi`) — confirms `GFX_AUTODETECT`-style selection is available between
hardware-accelerated DirectDraw and a GDI software fallback; no OpenGL/D3D
driver compiled in. `DirectDrawCreate` is imported (imports.json) and only
referenced through the `_gfx_directx_*` driver code. Per-frame screen output
(KNOWN): `blit_to_screen` (main.c, 0x40b6bc, 1415 bytes) is called once per
frame from `play()` right after the frame-pacing wait (0x413326, see item l) —
this blits/pageflips the game's off-screen `BITMAP` to the driver's `screen`.
`draw_frame` (main.c, 0x40929c, 8518 bytes — the single largest game function)
does all the per-frame drawing into that off-screen bitmap before the blit.
Color depth / resolution: not independently re-verified this pass beyond the
driver list (tower.cfg on disk carries the runtime-configured depth/res —
worth a follow-up `set_gfx_mode` argument trace if needed).

## h. Audio (KNOWN)

- **DIGI (sample) drivers compiled in**: `_digi_directsound` (wdsound.c,
  `digi_directsound_*` functions — full DirectSound mixer: lock/unlock voice,
  set frequency/pan/volume, position) and `_digi_none`. No `digi_win`
  (waveOut) driver symbol was found compiled in — DirectSound is the only real
  digital-audio backend in this build; `wdsinput.c` also present
  (`digi_directsound_rec_*`, DirectSound *capture* API — recording support,
  presumably unused by the game itself).
- **MIDI**: no native MIDI driver; Allegro's own software wavetable
  synthesizer (`digmid.c` CU present, mixes MIDI through the DIGI/DirectSound
  path) is what plays any MIDI content, if used.
- **OGG Vorbis**: the `logg.c` Allegro addon (`C:\Lib\allegro4\addons\logg\
  logg.c`) plus a *full* statically-linked libvorbis + libogg (`vorbisfile.c`,
  `vorbisenc.c`, `block.c`, `mdct.c`, `floor0.c`/`floor1.c`, `res0.c`,
  `sharedbook.c`, `psy.c`, `analysis.c`, `bitwise.c`, `framing.c`, etc. — ~26
  source files, 311 functions, 129KB of .text, see origin=`other`/vorbis) is
  linked in. Game main.c has `getSampleFromOggDatafile`/`load_sound` — ogg
  decoding happens synchronously via Allegro's mixer path (`mixer.c` CU),
  **not** on a separate thread — no thread creation call site was found
  anywhere near the vorbis/logg code (all 5 thread-spawn sites were already
  fully accounted for in item b).

## i. Filesystem (KNOWN — string evidence in .rdata/.data + call-site checks)

Files referenced by literal path strings found in the binary:
`data/data.dat`, `data/loading.dat`, `data/sfx15.dat` (with fallback message
"could not load data/sfx15.dat"), `data/com/default.dat`,
`data/com/temp.dat`, `tower.cfg`, `gamepad.txt`, `allegro.log`,
`screenshots/icytower_%04d.png`, `characters/%s/`, `profiles/` (+ per-profile
files, "failed to open profile stats \"%s\" for writing"), `replays/` (+
`.itr`-style replay files, string evidence "saving replay: %s",
version-gated: "You need Icy Tower 1.2/1.3 to view this replay"), and
**log.txt** (opened for writing at game startup in `_mangled_main`, see item
a — confirmed present on disk in assets/log.txt). The game writes during
normal play: profile stats, hiscore tables ("Creating hiscore tables"
string), replays (on demand), screenshots (on demand, `take_screenshot`
0x41002c), and continuously appends to log.txt (`log2file`, 0x40da58, called
from many places incl. inside `play()`'s frame loop at 0x411b13).

## j. Network (KNOWN)

`httpget.c` (game CU) implements a minimal HTTP client over Winsock
(`HTTPGet`/`HTTPRequest`/`HTTPFetchInternal`/`SplitURL`/`extractHTTPResponse`,
0x405890-0x4061ab) used by `fld_adspot.c` to fetch
`http://www.icytower.com/icytower_pc.csv` (string evidence, "Could not fetch
ad listing from ..."). This runs on the dedicated pthread spawned by
`fldads_start` (0x403ac8, called once from `init_game()` at 0x40ea13) — **not
on the main thread**, so it cannot block gameplay/menu rendering; on failure
the ad system just has no ad to show (cache files `data/com/default.dat` /
`data/com/temp.dat` back it). This is a startup-only fetch, not per-frame or
per-session-repeating (single call site).

## k. Randomness / replay (KNOWN)

RNG is plain **libc `rand()`/`srand()`** (mingw CRT, a simple LCG) — no custom
PRNG found in game code. `srand()` is called exactly 3 times, all in main.c:
twice in `new_game()` (0x40de24, 0x40e0a0) and once in `init_game()`
(0x40ef53). `rand()` is called from: `map.c`'s `add_floor()` (tower layout
generation — 3 call sites, 0x416956/0x4169bc/0x4169d3), `stars.c`
(`init_star_field`/`scroll_star_field` — cosmetic background only),
`fld_adspot.c` (`fldads_get_random_ad` — ad selection, not gameplay), and
`beta.c`'s `create_post` (anti-tamper/beta-tester watermark generation, not
gameplay). **This means the tower layout is fully reproducible from the
`srand()` seed alone.**

Game's own replay system (`replay.c`, game CU, 0x41b9ac-0x41ea88): 14
functions — `create_replay`, `load_replay`, `save_replay`, `destroy_replay`,
`replay_selector`/`draw_replay_selector` (UI), `calc_replay_checksum`/
`calc_replay_checksum_131` (integrity/tamper check, versioned), `get_replay_
property`, `update_file_list`, `add_itr_file`. `create_replay()` (0x41cce8,
KNOWN from disasm) allocates a fixed ~2220-byte header (magic value + version
word read from a static table at 0x4d7dd0/0x4d7dd4, a 7-byte format tag copied
from 0x4d7a7e, an embedded literal "no date" placeholder) followed by
`malloc(0x20 + frameCount*8)` — **i.e. the replay format is a header plus a
flat array of one 8-byte record per recorded frame.** This strongly indicates
per-frame input recording (not full game-state snapshots) at a fixed 8
bytes/frame, consistent with "seed once via srand() + record inputs every
tick" — exactly the deterministic-replay shape useful for a lifted/carrier
build: reseed rand() and feed the same 8-byte-per-frame input stream through
the 0x506938-tick-driven main loop to get bit-identical playback. (The exact
bit layout of the 8-byte record was not decoded this pass — recommended
follow-up: dump `save_replay`/0x41dd78 in detail.)

## l. Main loop / frame pacing / safepoint (KNOWN)

Game's `main()` chain: `_mangled_main` (0x415f10) → `init_game()` (0x40e7dc,
5788 bytes: opens data.dat/loading.dat, installs drivers, calls
`install_timers()`, `fldads_start()`) → menu/gameplay dispatch among
`main_menu_callback` (0x4100f8, 3741 bytes), `play()` (0x411a00, **17420
bytes, the single largest function in the binary**), `do_replay_menu`
(0x410f98), `run_demo` (0x415e0c).

`play()` is the gameplay loop. Per-iteration structure (KNOWN, addresses in
item c): busy-wait on the 20ms tick counter (`0x506938`) via `rest(2)`
Sleep-polling, then jump to **0x4124f4** to run one frame of game/physics
logic (`handle_player_input`, `handle_player_collision_*`, `update_frame`
0x406ac4, etc.), then `draw_frame()` (0x40929c) renders into the off-screen
bitmap, then `blit_to_screen()` (0x40b6bc) presents it (call site 0x413326).
There's also a "catch-up" branch (0x4132e4-0x4132fa) that lets the loop run
ahead without the Sleep-wait while `cycle_counter > 7` is false, i.e. it can
process a couple of buffered ticks back-to-back before waiting again — worth
noting for a deterministic re-implementation (it's not strictly one-tick-in,
one-frame-out under load).

**Correction (PROMOTIONS.md batch 12, from reading `play()` in full).** The
catch-up branch is not a load-adaptive path: it is guarded by
`if (debug)` and then by `key[KEY_TAB] && key[KEY_LSHIFT]`
(0x4132a1 / 0x4132d2 / 0x4132db, main.c 4356-4363). `debug` has no store
anywhere in the image (PROMOTIONS.md batch 9), so **in vivo the branch is
unreachable** and the loop really is one-tick-in, one-frame-out: the only
reachable pacing is `while (!cycle_count) rest(2);` at main.c 4357. It is a
developer frame-step, not a catch-up. `src/icytower/play.c` recovers all
three arms as they are branched. Two further corrections batch 11 already
recorded stand: 0x4124f4 is the tick's END (`rest(2)`, main.c 4369), not its
start — the loop head is 0x411c30 — and there are four `draw_frame` and six
`blit_to_screen` call sites in `play()`, not one of each.

**Best safepoint candidate (KNOWN address): VA 0x4124f4** — the single point
in `play()` reached exactly once per consumed 20ms tick, immediately after the
Sleep-wait loop returns and before any per-frame game logic or Allegro/Win32
calls begin. No host call is in flight there (the preceding instruction is
either the `test`/`jne` off `rest()` or the wait-loop's own `jmp`). This is
the natural "one simulation tick boundary" of the whole game.

## m. SEH / exceptions / setjmp / VirtualProtect / FPU (KNOWN)

- `SetUnhandledExceptionFilter` called once, at CRT start (0x40102e) →
  `__gnu_exception_handler@4` (0x401150) — this is the **stock mingw-runtiime**
  SEH-to-C-signal translator: it maps STATUS_INTEGER_DIVIDE_BY_ZERO /
  STATUS_FLOAT_* / STATUS_ACCESS_VIOLATION / STATUS_ILLEGAL_INSTRUCTION NT
  exception codes to `signal()`-registered SIGFPE/SIGSEGV/SIGILL handlers
  (the 6 `signal()` calls found, 0x401180-0x40129c, are all internal to this
  same function) — not game-specific exception handling.
- `VirtualProtect` called exactly twice, both inside
  `__pei386_runtime_relocator` (0x4b247d, 0x4b24bd), which is mingw-w64's
  runtime PE-relocation applier (relevant if the OS ever loads the image away
  from its preferred base — the binary carries no `.reloc` section per the
  brief, so in practice this code path is likely a no-op / early-outs, but the
  VirtualProtect capability is compiled in). Not used for self-modifying game
  code.
- `_setjmp` called 3 times: two in `loadpng.c` (`load_memory_png`,
  `load_png_pf`) and one in `savepng.c` (`really_save_png`) — these are all
  **libpng's standard `png_jmpbuf` error-longjmp pattern**, unrelated to
  gameplay control flow or thread/exception handling.
- FPU: heavy use throughout game code (9865 `fld`/`fstp`/`fadd`/`fmul`/`fdiv`
  instructions binary-wide) — physics/position math is float-based, x87 (not
  SSE) codegen (GCC 4.4.1 default `-mfpmath=387` for this target). MMX
  instructions are essentially absent (2 hits total) — the Allegro
  color-depth blitters/stretchers in this build are the portable C versions
  (`src/c/c*.c` CUs), not hand-written MMX asm.

## n. Static-lifting hazards (KNOWN counts)

- **54** indirect jump sites matching the `jmp *tbl(,%reg,4)` pattern
  (compiler-generated `switch` jump tables, all target .rdata tables, e.g.
  0x40bed0, 0x4125a3, 0x417f24, 0x41b009, 0x41d685 and 49 more — see
  `disasm.txt` grep `jmp    \*0x.*(,%`). These need jump-table recovery during
  lifting.
- **2215** indirect *call* sites (`call *reg`/`call *mem`) — a large fraction
  is import-table thunking (`call *0x514xxx` through IAT slots, the normal
  MinGW-import-stub pattern) rather than true function-pointer dispatch, but
  this also includes real Allegro vtable dispatch (driver struct function
  pointers — GFX_DRIVER/DIGI_DRIVER/etc., see item e) and game-side callback
  tables (menu button callback arrays, datafile object callbacks
  `datafile_callback`/`datafile_callback_slow` in main.c). Distinguishing
  IAT-thunk calls from genuine indirect dispatch will need a second pass
  correlating call targets against the import table.
- 52 `(bad)`-decoded byte spans in the linear disassembly — almost certainly
  literal-pool/padding bytes between functions rather than genuine
  data-in-.text or self-modifying code; not independently confirmed as
  benign, flagged for a follow-up pass if lifting chokes on them.
- No evidence of self-modifying code: `VirtualProtect` use is confined to CRT
  relocation-support code (item m), never called near the game's own .text
  range.
- Position-dependent code: the binary has **no base relocations**
  (`.reloc` absent) and is a plain non-PIC EXE — always loads at 0x400000,
  simplifying static VA-based lifting (no need to handle rebasing).

## o. .text ownership summary

See table in section 1. Allegro (incl. its statically-linked win/* drivers)
owns 61.5% of .text bytes, libvorbis/libogg ("other") 17.0%, game code 16.5%
(125641 bytes across 253 functions), CRT/libgcc 3.6%. `play()` (17420 bytes)
and `draw_frame()` (8518 bytes) alone account for ~21% of all game-code bytes.

## Suggested next passes (not done here, out of scope for this budget)

- Decode the exact 8-byte replay-frame record layout (`save_replay` 0x41dd78 /
  `load_replay` 0x41cde8) to nail down the input-recording bit format.
- Trace `set_gfx_mode` call args in `init_game()`/`options.c` to confirm
  requested color depth/resolution and GFX_AUTODETECT vs. forced DirectDraw.
- Classify the 2215 indirect-call sites into IAT-thunk vs. genuine
  function-pointer dispatch (cross-reference call targets against
  imports.json's IAT slot VAs).
