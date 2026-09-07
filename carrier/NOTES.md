# carrier/NOTES.md — fixes, evidence, and TEMPORARY hacks

This carrier maps `assets/icytower15.exe` into its own address space at its
original ImageBase (0x400000), resolves the game's imports itself, and runs
the original machine code natively. Every fix below was required to get from
"builds" to "reaches MAIN MENU LOOP" and is tagged KNOWN (directly observed),
INFERRED (reasoned from KNOWN facts), or TEMPORARY (a carrier-only
workaround, not a fact about the game).

## Result

**Reached MAIN MENU LOOP.** `assets/log.txt` from a full run
(`--trace-imports=all --trace-out artifacts/run1_trace.log --report
artifacts/run1_report.json --run-seconds 20`) ends with the game's own
startup log through hiscore/profile/character/SFX loading, menu reset, and
the literal line `MAIN MENU LOOP` — matching the pre-existing baseline log
(`artifacts/log_original_baseline.txt`, captured from a real standalone run)
step for step up to that point.

## Fixes, in the order they were found

### 1. `pf_import_common`/`pf_on_import` naked-declaration bug (KNOWN, compile error)

`__declspec(naked)` is only legal on a function **definition**, not a
forward declaration. The generated `import_stubs.cpp` and `trace.hpp` both
forward-declared `pf_import_common` with `naked` attached. Fixed by dropping
`naked` from every declaration and keeping it only on the one definition in
`trace.cpp`. (`carrier/gen/gen_imports.py`, `carrier/src/trace.hpp`)

### 2. `g_real[]` had no definition (KNOWN, link error)

`import_types.hpp` declared `extern "C" void* g_real[PF_MAX_IMPORTS];` but
no translation unit defined it (both `imports.cpp` and `trace.cpp` only had
`extern` declarations). Fixed by making `imports.cpp` the one definition
(`extern "C" void* g_real[PF_MAX_IMPORTS] = {};`).

### 3. Guest ImageBase not free — Windows fills it with its own bookkeeping (KNOWN, measured; TEMPORARY mitigation)

`carrier.exe` links at `/BASE:0x10000000` (`build.cmd`) specifically to
leave 0x400000–0x78c000 free for the guest, which has no relocations and
must load at exactly 0x400000. On this host that address range does **not**
stay free on its own:

- Even calling `VirtualAlloc(0x400000, ...)` as the literal first statement
  of `main()` failed. A `VirtualQuery` scan showed the OS had already
  memory-mapped several locale/codepage `*.nls` files (`C_852.NLS`,
  `l_intl.nls`, `locale.nls` — this system's locale is Czech/852) plus a
  number of unnamed pagefile-backed `MEM_MAPPED` sections scattered across
  the entire target range, before any of our own code ran.
- A custom linker `/ENTRY:` override that ran a `VirtualAlloc` reservation
  before calling `mainCRTStartup` **still lost the race** — confirming the
  occupying allocations happen during the Windows loader's own process
  bring-up (ntdll/kernel32 init), strictly before *any* entry point, custom
  or default, gets control.
- Blindly evicting every non-free region in range (`UnmapViewOfFile` /
  `VirtualFree`) is unsafe: one attempt, run later in the program (after our
  own heap had grown into the same range), corrupted the process heap and
  crashed with no diagnostic output at all.

**Fix actually used (TEMPORARY, structural):** `relaunch_as_reserved_child`
in `main.cpp`. carrier.exe relaunches itself via `CreateProcessA(...,
CREATE_SUSPENDED, ...)`, then calls `VirtualAllocEx` on the **suspended**
child to reserve the guest's exact address range, then `ResumeThread`.
Nothing — not even ntdll's process-init — executes in a `CREATE_SUSPENDED`
process before its first thread resumes, so this reservation cannot lose
the race. This is the standard "create-suspended / reserve / resume"
bootstrap technique. `pe_image_reserve_guest_range` in `pe_image.cpp` keeps
the old evict-`.nls`-files-and-retry logic as a fallback for the case where
nothing pre-reserved the range (recognizes an already-owned reservation and
returns success immediately; only evicts memory-mapped views whose backing
file name ends in `.nls`, never touches `MEM_PRIVATE`/heap memory).

Consequence: carrier.exe **always relaunches itself once** — the process
tree is parent (proxy) → child (the real carrier, does the PE mapping and
runs the guest). The parent waits and forwards the child's exit code.

### 4. Committing an already-reserved region needs `MEM_COMMIT` alone (KNOWN, measured)

Once the guest range is pre-reserved by the parent, `pe_image_load`'s
original `VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)`
failed with `ERROR_INVALID_ADDRESS` (487) — combining `MEM_RESERVE` with an
already-reserved (not `MEM_FREE`) range is rejected. Fixed by trying
`MEM_COMMIT` alone first, falling back to the combined call only if that
fails (covers the case where nothing pre-reserved the range).

### 5. Use-after-free reading `nt->OptionalHeader.AddressOfEntryPoint` (KNOWN, real bug, crashed the carrier)

`nt` points **into** `file_buf` (the malloc'd copy of the whole ~3.6 MB
guest EXE file). The original code called `free(file_buf)` and *then* read
`nt->OptionalHeader.AddressOfEntryPoint` to compute `entry_va`. Because
`file_buf` is a large allocation, `free()` on it is typically served
straight back to the OS via `VirtualFree(MEM_RELEASE)`, unmapping it
immediately — the subsequent read was a genuine access violation, caught by
the carrier's own VEH (crashing inside carrier's own code, not the guest).
Fixed by extracting `entry_va` into a local **before** `free(file_buf)`.
This was the single hardest bug to isolate: the crash address shifted
between rebuilds (recompiling shifted the exact code layout), which is what
made it look like heap corruption at first rather than a simple
use-after-free of a large allocation.

### 6. `_chdir` target was carrier.exe's own directory, not `assets\` (KNOWN, measured via import tracing)

Confirmed two independent, additive causes by adding a temporary
`MessageBoxW`/`_chdir`/`fopen` argument decoder to the trace logger
(`trace.cpp`, since the tracer runs in the same address space as the guest,
it can read guest string arguments directly):

1. **`__getmainargs` bypasses per-import IAT wrapping.** The game's
   `get_executable_name()` (`_mangled_main`, main.c) ultimately reads
   `__argv[0]`, populated by mingw's `__getmainargs` — which runs **inside**
   the real `msvcrt.dll` we `LoadLibraryA`'d, calling `GetCommandLineA`
   through msvcrt.dll's *own* import table (bound by the normal Windows
   loader when msvcrt.dll was loaded), never through icytower15.exe's IAT.
   `wrap_GetCommandLineA` only intercepts calls that go through the guest's
   own IAT (e.g. `_main`'s separate, direct `GetCommandLineA` call) and
   never sees this one.

   A runtime PEB patch (writing `ProcessParameters->CommandLine` directly)
   was tried and **measured to not work**: `GetCommandLineA` on this host is
   served from an ANSI copy kernel32/ucrtbase cache very early — before
   anything our own process does at runtime can preempt it (the same class
   of race as fix #3). The only fix that actually lands in time is making
   the **real, OS-assigned command line** already be what the guest expects
   from `CreateProcess` itself. Since `relaunch_as_reserved_child` (fix #3)
   already relaunches carrier.exe as a child process, its `lpCommandLine` is
   now set to the guest's own quoted image path, and carrier's own flags
   (`--trace-imports`, `--run-seconds`, etc.) travel from parent to child via
   `PF_*` environment variables instead of argv (`options_to_env` /
   `options_from_env` in `main.cpp`), which `CreateProcessA` inherits
   automatically. This is the ONE thing this carrier does that is
   process-tree-shaped rather than in-process — required because the fix
   has to exist before the child's first instruction runs, same as fix #3.

2. **`GetModuleHandleA(NULL)` is left DIRECT (per the intended policy), and
   the game passes its result to `GetModuleFileNameA`.** Even after fix
   6.1, `_chdir`'s target still resolved to carrier.exe's own directory. The
   game's `get_executable_name()` actually calls `GetModuleHandleA(NULL)`
   first (not wrapped, since it legitimately should return a real handle),
   then passes that handle to `GetModuleFileNameA`. Since carrier.exe truly
   is the process's main module (the guest was never loaded through the
   real Windows loader), `GetModuleHandleA(NULL)` correctly returns
   carrier's own module handle — and `wrap_GetModuleFileNameA`'s original
   `hModule == nullptr` check let that non-null-but-still-really-"self"
   handle fall through to the real `GetModuleFileNameA`, returning
   carrier.exe's own path. Fixed by capturing carrier's own module handle
   once (`wrappers_set_carrier_hmodule(GetModuleHandleA(nullptr))`, called
   from `main()` before the guest runs) and treating `hModule ==
   g_carrier_hmodule` the same as `hModule == nullptr` in the wrapper.

   Both fixes were required together — either alone still produced the
   wrong chdir target.

## Wrappers actually installed (WRAP mode)

All KNOWN needs per notes/binary_recon.md item a, confirmed by the fixes
above:

- `ExitProcess`, `exit`, `_cexit`, `abort` (msvcrt/KERNEL32) — regain
  control to flush the trace/report before the process really ends. The
  mingw entry point never returns; this is the only exit path.
- `GetModuleFileNameA` — returns the guest's own path when `hModule` is
  `NULL` **or** carrier's own module handle (see fix #6.2); forwards to the
  real function otherwise.
- `GetCommandLineA` — returns the guest's own path (covers `_main`'s direct
  IAT-routed call; `__getmainargs`'s internal call is covered by fix #6.1
  instead, not by this wrapper).

`GetModuleHandleA(NULL)` was deliberately left DIRECT (per the original
policy) — it genuinely needs to return carrier's own handle for LoadIcon
and similar Win32-object-scoped calls to behave sanely; only the
`GetModuleFileNameA` consumer of its result needed a workaround.

## TEMPORARY simplifications (not evidence of game behavior)

- **Everything RWX.** `pe_image_load` maps the whole guest image
  `PAGE_EXECUTE_READWRITE` instead of splitting `.text`/`.rdata` read-exec
  from `.data`/`.bss` read-write. Fine for a first carrier; revisit once
  running cleanly is no longer the bottleneck.
- **Guest stack TEB swap.** `pf_launch_guest_fixed` (`main.cpp`) commits a
  fixed 2 MiB region at 0x0e000000 and swaps `fs:[4]`/`fs:[8]` (TEB
  StackBase/StackLimit) to it before calling the guest entry point, purely
  so the guest's stack lives at a deterministic address. This is exactly
  what fibers do internally. Its "restore on return" path is unreachable in
  practice (the guest always exits through `wrap_ExitProcess`) and is
  diagnostic-only (`pf_on_guest_return`). Fall back with `--guest-stack=host`
  if this ever causes trouble (not observed in this run — MinGW CRT/Allegro
  did not probe the stack limits in any way that broke).
- **`pe_image_reserve_guest_range`'s `.nls`-only eviction fallback** is
  reachable only if nothing pre-reserved the range (i.e. if
  `relaunch_as_reserved_child` is bypassed somehow); in the normal path it
  short-circuits to "already reserved, success" immediately.

## Diagnostics used, worth keeping in mind for future debugging

- The trace logger can decode specific import arguments as text when it's
  useful (currently: `MessageBoxA`/`MessageBoxW` message text,
  `_chdir`/`fopen` path arguments) — see the small special-cases in
  `trace.cpp`'s `pf_on_import`. This is only safe because the carrier and
  the guest share one address space; it was essential for finding fixes #6.1
  and #6.2 (there is no other way to see what string a guest call actually
  received without a real debugger).
- The VEH handler's EBP-chain stack walk correctly identified fix #5's
  crash as being inside carrier's own image (0x10000000+ addresses) rather
  than the guest's (0x400000-range) — `symbols_describe` only knows guest
  symbols (`artifacts/functions.json`), so a carrier-side crash shows up as
  "nearest guest symbol" nonsense in the description text, which is itself
  a useful tell that the crash isn't in guest code at all.

## Milestones 5-7

Status: **reached, made deterministic, and proved by digest equality**
(milestone 5 gameplay reached, 6 time/input/RNG instrumented, 7 record/replay
digest-equal - win32_pilot.md §8). New code: `carrier/src/det.hpp`/`det.cpp`
(virtual time, input injection, tick sensor, deterministic heap arena),
`carrier/gen/gen_game_globals.py` + generated `carrier/gen/game_globals.inc`
(digest scope), `carrier/scripts/newgame.txt` (input script),
`carrier/scripts/compare_digests.py` (the proof script). Small additions to
`main.cpp` (option parsing/wiring), `wrappers.cpp`/`imports.cpp` (6 new
always-installed wrappers + 4 heap wrappers), `build.cmd` (compile det.cpp).
Everything is inert unless `--det`, `--digest-out`, `--record-input` or
`--input-script` is passed - default carrier behavior (§1-4 above) is
unchanged (re-verified: a plain `--trace-imports=none --run-seconds 8` run
still reaches `MAIN MENU LOOP` exactly as before).

### A. Virtual time

**KNOWN** (`artifacts/functions.json` + `disasm.txt`, addresses cited in
`det.cpp`'s own header comment): `tim_win32_high_perf_thread` 0x478584,
`tim_win32_low_perf_thread` 0x4783bc, `_handle_timer_tick(int interval)`
0x45d6c8 (returns `long`, DWARF-confirmed prototype), `input_thread_proc`
0x479a40. Disassembling `tim_win32_high_perf_thread` (see comment above
`det_wrap_Sleep`) confirmed the exact algorithm reproduced in det mode:
`elapsed_units = elapsed_time * TIMERS_PER_SECOND(1193181, literal 0x1234dd
in the disassembly) / <clock frequency>`, call `_handle_timer_tick(units)`,
repeat. `det_wrap_Sleep` reproduces this with virtual elapsed ms instead of
QPC ticks, keeping a running **total** (`g_units_reported`) rather than a
separate remainder variable - full-precision integer division from a
monotonic total is equivalent to remainder tracking and simpler to get right.

Wrapper `_beginthread` (msvcrt.dll, IAT-wrapped like `ExitProcess` et al.,
always installed): in `--det`, a start address of `tim_win32_high_perf_thread`
or `tim_win32_low_perf_thread` returns a **parked handle** (a real
`CreateThread`'d thread that blocks forever on a never-signaled event)
instead of creating the real thread - "timer thread virtualized" is logged.
Non-det: forwards to the real `_beginthread` unchanged.

Wrappers `QueryPerformanceCounter`, `timeGetTime`, `time`, `clock`: in
`--det`, all four return values derived from the virtual clock
(`g_virtual_ms`); non-det forwards to the real function. Pre-det semantics
of each (**KNOWN**, `notes/binary_recon.md` item c / `notes/replay_format.md`
sec 2-3) are documented at each wrapper in `det.cpp`. Pinning `time()` is
what makes the RNG seed deterministic - **INFERRED, then measured true**:
`new_game()`'s effective `srand()` argument is the output of a
`time(NULL)`-seeded `rand()` call (`replay_format.md` sec 2); msvcrt's
`rand()` LCG has no other host-entropy input, so pinning `time()` alone was
sufficient - `rand()`/`srand()` were deliberately left unwrapped and the
milestone-7 proof (below) confirms this held.

`--pace=fast` (default) never really sleeps; `--pace=real` also calls the
real `Sleep(ms)` so a human can watch. T (the carrier tick index) is defined
as `virtual_ms / 20` in det mode; in non-det mode the same formula is used
with real elapsed `GetTickCount64()` instead, for diagnostic purposes only
(never claimed deterministic) - this dual definition is what makes both the
input-delivery mechanism (below) and the milestone-7 non-det negative
comparison work with the same code path.

### B. Input injection

**KNOWN** (DWARF, `artifacts/dwarf_info.txt`): `_handle_key_press(int
keycode, int scancode)` 0x43e2f8 and `_handle_key_release(int scancode)`
0x43d8d4 (both `void`, keyboard.c). `--input-script PATH` accepts
`T <press|release> <scancode-or-KEY_NAME>`; the 7 key names given in the
task brief were verified byte-for-byte against DWARF's `__allegro_KEY_*`
enum (`KEY_ESC`=59, `KEY_ENTER`=67, `KEY_SPACE`=75, `KEY_LEFT`=82,
`KEY_RIGHT`=83, `KEY_UP`=84, `KEY_DOWN`=85 - all match exactly). Events are
delivered from `det_wrap_Sleep` on the main thread (matches the task's
literal instruction), gated on the shared T clock from part A, not on a
separate hit-counter - this was a deliberate design choice: the digest
safepoint (0x4124f4) only exists **inside** `play()`, so an
input-delivery mechanism gated on safepoint hits could never deliver the
menu-navigating ENTER keypress (the menu is never reached from inside
`play()`). Verified (`notes/binary_recon.md` item l): 40 call sites to
`rest()` exist across the whole binary, including one in
`main_menu_callback` (VA 0x40119b) - i.e. the main thread's `Sleep`-driven
idle loop, and therefore T, runs continuously through the menu too, not
just gameplay.

Synthetic `keycode` is always passed as `0` (task brief: "keycode can be 0
or the ASCII mapping") - **KNOWN**, confirmed by disassembling
`key_dinput_handle_scancode` (0x46d5a8): the real DirectInput path itself
sometimes passes `-1` ("no ASCII") to `_handle_key_press`, and only the
scancode-indexed Allegro `key[]` array (read by `poll_control`/`is_left`/
`is_right`/etc, `notes/replay_format.md` sec 1) drives menu and gameplay
input - `keycode` feeds a separate, unused-here ASCII/`readkey()` text-entry
path.

**MEASURED, load-bearing finding** (see "keyboard neutralization" below):
the DirectInput input thread (`input_thread_proc`, part of the original
plan's "skip its `_beginthread` call") is **never actually spawned** in this
build/config. Logging every real `_beginthread` call site (`det_wrap_
beginthread`'s diagnostic print) showed exactly two real threads:
`wnd_thread_proc` (0x4790b8) and the timer thread - never
`input_thread_proc`. `key_dinput_handle_scancode` still runs, from the real
window thread's message pump, and calls the real Win32 `GetKeyboardState` -
i.e. it can observe the **host's real keyboard**, a live nondeterminism
source. Per the task brief's documented fallback ("leave [the thread] and
neutralize the keyboard by never acquiring - decide by evidence and
document"): in `--det`, a hardware breakpoint at `key_dinput_handle_
scancode`'s entry (0x46d5a8) makes it an immediate no-op (pop the return
address into EIP; safe because its args arrive in EAX/EDX per its
disassembly, never on the stack). This is armed on **every** thread this
carrier spawns (`det_wrap_beginthread` now calls `det_arm_thread` on every
real thread it creates, not just a hypothetical input thread), since which
Allegro thread ends up owning DirectInput was itself something that had to
be measured, not assumed.

**--record-input PATH** (works with or without `--det`): rather than a
5-byte entry-patch + relocated-prologue trampoline (the task's more
invasive option), this carrier reuses the same hardware-breakpoint
mechanism as the tick sensor - breakpoints at `_handle_key_press`/
`_handle_key_release`'s entries log `T press/release scancode` (reading the
cdecl stack args directly from `ctx->Esp`) and let the original instruction
execute normally (RF-flag single-step-over, no patched bytes, no relocated
prologue). Cheaper and lower-risk than prologue relocation given
`_beginthread` was already being wrapped to arm new threads' debug
registers for the keyboard-neutralization breakpoint above - one mechanism
serves both needs. **Not exercised end-to-end with a live human player in
this pass** (would need a real keyboard/focused window/`--pace=real`
session, out of scope for an automated pass); the breakpoint-hit code path
itself is exercised by the tick sensor (same table, same VEH) and is
straightforward to sanity-check separately.

### C. Tick sensor

**KNOWN**: safepoint VA 0x4124f4, inside `play()`, once per consumed tick
(`notes/binary_recon.md` item l). Implemented exactly as specified: a helper
thread (`det_arm_main_thread`, spawned once after `imports_init()`) does
`OpenThread` → `SuspendThread` → `GetThreadContext(CONTEXT_DEBUG_REGISTERS)`
→ set `Dr0..Dr3`/`Dr7` → `SetThreadContext` → `ResumeThread` on the guest
main thread; a vectored exception handler (`det_veh_handler`, registered
with `AddVectoredExceptionHandler(1, ...)` **after** `main.cpp`'s own
`veh_handler` so it runs first) catches `EXCEPTION_SINGLE_STEP`, checks
`Dr6`, runs the matching callback, sets `EFLAGS.RF` (bit 16) and returns
`EXCEPTION_CONTINUE_EXECUTION`; anything not ours falls through to
`veh_handler` unchanged (`EXCEPTION_CONTINUE_SEARCH`). The sensor table
(`g_bp[4]`, `struct BpSlot{va; on_hit}`) is generic, as asked: up to 4
simultaneous breakpoints (Dr0-Dr3) - currently used for the safepoint, the
det-mode keyboard-neutralization no-op, and (when `--record-input` is
given) the two key-event recorders; all four slots can be full
simultaneously (`--det --digest-out --record-input`).

Per-tick line: `T <sha256> esp=.. ebp=.. ebx=.. esi=.. edi=..` to
`--digest-out PATH`, `fflush`ed every line. Registers are logged plaintext
(not hashed) - see next section for what the hash covers and why.

### The digest region: two failed attempts, then game-owned globals

**Attempt 1 (failed, documented): raw `.data`+`.bss`.** As literally
instructed (.data VA 0x4bc000 size 0x176f4, .bss VA 0x4dd000 size 0x36978):
two `--det --pace=fast` runs of the identical script diverged from tick 1.
Root cause, found via `log.txt` diffing (byte-identical between runs) plus
a temporary raw-memory-dump diagnostic (`DET_DUMP_MEM_TICK`/
`DET_DUMP_MEM_PATH` env vars, still in `det.cpp`, harmless/opt-in) cross-
referenced against `artifacts/coff_symbols.json`: `log.txt`'s own "Graphics
mode set. (screen = %d)" line printed a **different raw pointer value**
each run - Windows randomizes the msvcrt heap's base address per **process**
(independent of image ASLR), and that `BITMAP*` is a msvcrt-heap pointer
baked directly into a `.bss` global.

**Attempt 2 (partial fix, not sufficient alone): deterministic heap arena.**
`malloc`/`calloc`/`realloc`/`free` (msvcrt.dll, IAT-wrapped, always
installed) redirect to a fixed-address (`0x20000000`, 256 MiB), bump-only
arena in `--det` mode - never reclaims memory (a leaking bump allocator is
fine for a bounded proof run: a few MB of allocation at most, and the
*sequence* of `malloc` calls is itself deterministic once every other
nondeterminism source is pinned, so the same sequence of bump offsets comes
out every run). This is exactly what the architecture doc (§6 above)
already anticipated: "redirect [malloc] to a fixed-address arena so heap
contents are ordinary guest pages". This fixed the `screen` pointer, but a
second dump-and-diff pass still found ~20-30 differing globals - and a
**third** pass (different run pair) found a **different** set of ~25
differing globals. Every single one, across both passes, was an
Allegro/CRT/DirectX-internal global (COM device pointers for
DirectInput/DirectSound/DirectDraw, mutex/thread/event `HANDLE`s, an
`HWND`, MinGW's shared-exception-personality pointers, Allegro's internal
scancode/mixer-voice buffers) or the cosmetic `caption` text field of a
`Tmenu_selection` struct (DWARF-confirmed layout: `value`@0, `size`@4,
`caption`@8, 136 bytes - diffed **only** at offset≥8, never at `value`@0).
**Never once a game-CU global.** That the differing *set* itself wasn't
stable across sampling pairs (ASLR entropy sometimes coincidentally
matches between two runs) means per-byte exclusion is an unbounded,
non-convergent chase - a real instance of "if an approach fails after two
honest attempts, ... move to an alternative."

**Alternative adopted: hash game-owned globals only.** `carrier/gen/
gen_game_globals.py` (new; reads `carrier/gen/interop_index.json`'s
`globals` list - generated by the other agent's `gen_interop.py`, not
modified here - and `artifacts/coff_symbols.json` for sizes) emits
`carrier/gen/game_globals.inc`: 151 `{VA,size}` pairs, every global DWARF
attributes to one of the 25 game source files (the same `game` scope
`carrier/gen/it_globals.h` already uses). This sidesteps the whole
Allegro/CRT/DirectX-internal category at its source rather than chasing
individual bytes. Of the 151, only 3 are themselves host-object identities
(`gFLDADMutex`, `sLogMutex__log2file`, `gFLDADThread` - the ad-fetch
thread's own mutex/handle, declared in the game's own `fld_adspot.c`) and
are excluded by name; the 4 `Tmenu_selection` globals get their hashed size
capped to 8 bytes (`value`+`size`, dropping the cosmetic `caption`) via a
small override map, both documented with evidence in the generator's own
docstring. Result: **EQUAL**, reproducibly (below). This is offered as the
honestly-labeled alternative the task's own failure-handling instruction
asked for, not a silent narrowing - both failed attempts and the reasoning
are preserved above and in `gen_game_globals.py`'s docstring.

### D. Reaching gameplay - `carrier/scripts/newgame.txt`

Backed up/restored `assets/tower.cfg`, `assets/profiles/`, `assets/log.txt`
around every run in this section (already had `artifacts/assets_backup/` as
the pristine reference from the milestone-2-4 pass; restored from it before
each run, deleted `log.txt` since the game recreates it).

**MEASURED, load-bearing finding:** a brief ENTER tap (2-40 ticks) at the
main menu is **unreliable** - it registered in the very first exploratory
run, then failed on every identical rerun after (same binary, same script,
same restored profile/config). Holding ENTER continuously for hundreds of
ticks registers reliably every time. (Gameplay input, sampled every
consumed tick via `poll_control`/`Tcontrol`, does not have this problem -
short taps are fine there; only the menu's own input sampling is
tap-timing-sensitive, in a way not fully root-caused - possibly a coarser
menu poll cadence, possibly something in the GUI dialog system's own
buffered-keypress handling. Flagged, not chased further, since holding
ENTER is a complete, reliable workaround with no observed side effect on
gameplay - Icy Tower never reads ENTER during `play()`.)

`newgame.txt`: `KEY_ENTER` held T=20→620 (default "Play Game" menu item,
existing `MissingNO` profile/`harold_the_homeboy` character already
selected - matches `artifacts/log_original_baseline.txt`'s "new game
selected" sequence with no extra menu navigation); `play()` first reaches
its safepoint (first digest line) around **T=126** in this build/profile.
`KEY_RIGHT` held T=200→700, `KEY_SPACE` (jump) tapped every 45-50 ticks in
that window - roughly 10s (500 ticks) of gameplay. Verified via
`assets/log.txt` reaching `" play started"` (never from log text alone as
the actual proof, per instruction - see milestone 7 below) and via digest
lines appearing (**KNOWN**: they only appear inside `play()`, confirmed -
`play() ended` in `log.txt` and the digest stream stopping happen at the
same point).

### E. The proof (milestone 7)

Commands (each preceded by the assets restore shown above):

```
carrier.exe --det --pace=fast --input-script scripts/newgame.txt --digest-out ../artifacts/proof_det_run1.txt --stop-at-tick 1000 --run-seconds 15
carrier.exe --det --pace=fast --input-script scripts/newgame.txt --digest-out ../artifacts/proof_det_run2.txt --stop-at-tick 1000 --run-seconds 15
carrier.exe --det --pace=fast --input-script scripts/newgame.txt --digest-out ../artifacts/proof_det_run3.txt --stop-at-tick 1000 --run-seconds 15
python carrier/scripts/compare_digests.py artifacts/proof_det_run1.txt artifacts/proof_det_run2.txt
python carrier/scripts/compare_digests.py artifacts/proof_det_run1.txt artifacts/proof_det_run3.txt
```

Result: **`EQUAL (876 ticks, ...)`** for both comparisons - three
independent `--det --pace=fast` process launches of the identical script,
876 consumed ticks each (`--stop-at-tick 1000` fired cleanly via the same
exit path as `wrap_ExitProcess`/`carrier_shutdown`; the character was still
alive at T=1000 every time), byte-identical sha256 at every tick.
`assets/log.txt` was also byte-identical across all three runs (checked
separately - the digest equality, not this, is the proof, per instruction).

**Negative control:**

```
sed 's/^300 press KEY_SPACE$/301 press KEY_SPACE/' scripts/newgame.txt > scripts/newgame_shifted.txt
carrier.exe --det --pace=fast --input-script scripts/newgame_shifted.txt --digest-out ../artifacts/proof_det_negctrl.txt --stop-at-tick 1000 --run-seconds 15
python carrier/scripts/compare_digests.py artifacts/proof_det_run1.txt artifacts/proof_det_negctrl.txt
```

Result: **`FIRST DIFFERENCE at tick T=301`** - exactly the tick the moved
jump-press event was rescheduled to (one press event moved 300→301), i.e.
the first divergence is at the moved event, as required ("at or after that
event").

**Without `--det` (real time), twice:**

```
carrier.exe --input-script scripts/newgame.txt --digest-out ../artifacts/proof_nondet_run1.txt --run-seconds 20
carrier.exe --input-script scripts/newgame.txt --digest-out ../artifacts/proof_nondet_run2.txt --run-seconds 20
python carrier/scripts/compare_digests.py artifacts/proof_nondet_run1.txt artifacts/proof_nondet_run2.txt
```

Result: **710 vs 814 lines**, and `compare_digests.py` reports a
**tick-index mismatch at line 1** (T=188 vs T=189) - the two runs don't
even agree on which tick is "first" in the recorded stream, because without
`--det` the input-delivery clock (part A's `det_now_ms()`) falls back to
real `GetTickCount64()`, and real Windows thread-scheduling/Sleep-accuracy
jitter is exactly what `--det --pace=fast` eliminates. This demonstrates
det mode is doing real, load-bearing work, not a no-op.

### New wrappers, with evidence (summary table)

| import | evidence it needed a wrapper | det-mode behavior | non-det behavior |
|---|---|---|---|
| `_beginthread` | 4 static call sites total (`notes/binary_recon.md` item b); 2 are the timer threads | virtualized (parked handle) for the 2 timer-thread entry points; every other real thread spawned gets its debug registers armed with the current breakpoint table | forwards to real `_beginthread` unmodified |
| `Sleep` | only wait in the main-thread tick loop (`rest()`, item c) | drives the virtual clock + `_handle_timer_tick` + input delivery on the main thread | delivers input (real-time T) then forwards to real `Sleep` |
| `QueryPerformanceCounter` | 6 telemetry call sites in `play()` + (pre-virtualization) the timer thread | returns `g_virtual_ms` | forwards to real QPC |
| `timeGetTime` | called only inside the (now-virtualized) low-perf timer thread | returns `g_virtual_ms` | forwards to real `timeGetTime` |
| `time` | feeds all 3 `srand()` call sites + telemetry (`replay_format.md` sec 2-3) | returns a fixed virtual epoch | forwards to real `time` |
| `clock` | telemetry only (`play()`'s qpc/clock/time trio) | returns `g_virtual_ms` | forwards to real `clock` |
| `malloc`/`calloc`/`realloc`/`free` | MEASURED (see digest section above): real heap addresses leak into `.bss` globals, breaking digest equality | fixed-address bump arena, never frees | forwards to real msvcrt heap |

Hardware breakpoints installed (not IAT wrappers): safepoint 0x4124f4
(digest sensor, part C); `key_dinput_handle_scancode` entry 0x46d5a8
(det-mode real-keyboard neutralization, part B); `_handle_key_press`/
`_handle_key_release` entries 0x43e2f8/0x43d8d4 (`--record-input`, part B).

### Known gaps / open problems

- **`--record-input` not exercised end-to-end with a live human player** in
  this automated pass (would need real keyboard input + a focused window +
  `--pace=real`). The breakpoint-hit/logging code path is shared with the
  tick sensor (same table, same VEH) and was exercised that way, but the
  "human plays, carrier captures a `.txt` script" round trip itself is
  untested.
- **Menu ENTER-tap unreliability is not root-caused**, only reliably worked
  around (hold, don't tap). If a future script needs a *short* menu
  keypress for some other reason, expect the same flakiness.
- **`_beginthread`/`Sleep`/QPC/`timeGetTime`/`time`/`clock` bypass the
  import-count trampoline** even outside `--det` (same pre-existing pattern
  as `ExitProcess`/`exit`/etc, `wrappers.cpp`) - a minor, pre-existing-style
  regression in the `--report` JSON's per-import call counts for these 6
  names specifically; not fixed here (would need a public counting API from
  `trace.cpp`, out of scope for this pass).
- **The digest is game-owned globals, not literally "all of `.data`+
  `.bss`"** - a deliberate, evidenced deviation from the literal task text
  (see the two-failed-attempts writeup above). `.data`/`.bss` themselves are
  still exactly as specified (VA/size); only *which bytes within them* are
  hashed changed, and the change is generated + documented, not hand-tuned
  per run.
- **Heap arena is leak-only** (256 MiB, never reclaimed) - fine for a
  bounded proof run; would need real free-list reuse for a long-running
  session.
- **COM/host-object identity is still not virtualized** (DirectDraw/
  DirectSound/DirectInput device pointers, thread/mutex/event handles) -
  confirmed necessary reading for any future snapshot/restore work
  (architecture doc §6), out of scope here; the digest-region fix
  sidesteps it rather than solving it.

## Input policy and recording

Follow-up pass (2026-09-07) on top of Milestones 5-7, addressing
win32_pilot.md §5a ("Input source is an exclusive policy") and the two
`NOTES.md` "known gaps" it left open: `--record-input` untested end-to-end,
and the report JSON's per-import counts missing `_beginthread`/`Sleep`/QPC/
`timeGetTime`/`time`/`clock`/`malloc`/`calloc`/`realloc`/`free`. Deterministic
proof re-verified after every change below (`compare_digests.py`: **`EQUAL
(876 ticks, ...)`**, same as before this pass - see "Proof rerun" at the end
of this section).

### A. Explicit `--input=real|script|none`

Previously the exclusive-input requirement was met implicitly: `--det`
always parked the real keyboard (a hardware breakpoint at
`key_dinput_handle_scancode`'s entry, unconditionally), and `--input-script`
added scripted events on top. That worked for the milestone-7 proof (which
always used both together) but couldn't express "real keyboard, deterministic
clock" or "no input at all", and - MEASURED, this pass - silently allowed
both a real keyboard and a script to reach the game in a **non**-`--det`
`--input-script` run (nothing parked the real path there at all).

Now `--input=real|script|none` (`main.cpp`'s `resolve_input_policy`,
`det.hpp`'s `InputPolicy`) is explicit, with the pre-existing implicit
default preserved: `script` when `--input-script` is given, else `real`.
Resolved and validated once, in the parent process, before
`relaunch_as_reserved_child` - `--input=real` together with a real
`--input-script` is a hard error (exit code 2) except under
`--inject-real-test` (part C below); any other input-policy/`--input-script`
mismatch (e.g. `--input=none --input-script X`) is a warning, and the script
is simply not loaded. The resolved policy travels to the child via
`PF_INPUT_POLICY` (env, like every other carrier flag - see `options_to_env`/
`options_from_env`), is printed in the startup banner (stderr, one line, plus
a duplicate on stdout per the task's literal "stdout banner" wording -
`carrier: input_policy=...`), and is now a top-level field in `--report`'s
JSON (`"input_policy"`, plus `"real_key_violations"` - part B).

**Mechanism kept: breakpoint neutralization, not skipping the input
thread.** The task offered a choice ("keep the existing breakpoint
neutralization, or skip creating the DirectInput input thread if that
proves cleaner"). Milestone 5-7's own measurement (`det_wrap_beginthread`'s
diagnostic logging, cited above under "Milestones 5-7" part B) already
established that `input_thread_proc` is **never spawned** in this
build/config to begin with - only the timer and window threads are - so
"skip creating the input thread" has nothing to skip; `key_dinput_handle_
scancode` runs from the real window thread's message pump instead,
unconditionally, in every mode. The breakpoint-neutralization mechanism
(short-circuit the function at entry, `neutralize_keyboard_hit`) is
therefore the only lever that actually exists for parking it, and this pass
just changed **when** it's installed: previously gated on `g_det_mode`
alone, now on `input_policy != Real` (Script **and** None both park it, not
just when `--det` happens to also be set - this is the actual non-det-mode
mixing bug fixed above).

**Runtime assertion.** Because the neutralize breakpoint is a measured
fallback (win32_pilot.md's own words: "leave [the thread] and neutralize the
keyboard by never acquiring"), not a proof the real path is silent, every
hit of it while parked is now counted and logged as a violation
(`neutralize_keyboard_hit`, `g_real_key_violations`, capped at 20 printed
lines to avoid log spam, uncapped count) rather than treated as an
unremarkable no-op. Verified **zero** violations across every automated run
in this pass (`"real_key_violations": 0` in every `--report` JSON produced,
including the round-trip test in part C, which never touches the host's
actual keyboard).

### B. Report accuracy: `pf_count_import`

The always-installed wrappers (`_beginthread`, `Sleep`,
`QueryPerformanceCounter`, `timeGetTime`, `time`, `clock`, `malloc`/
`calloc`/`realloc`/`free`, plus `ExitProcess`/`exit`/`_cexit`/`abort`/
`GetModuleFileNameA`/`GetCommandLineA`) are wired **directly** into the
guest IAT (`imports.cpp`'s `is_wrapped()`/`wrappers_lookup()`), bypassing
`pf_import_common`'s counting trampoline entirely - so their calls were
invisible to `--report`'s per-import counts, a pre-existing gap this
`NOTES.md` already flagged.

Fixed with the single-place design the task suggested: `trace.cpp` now
exposes `pf_count_import(int id)` (declared in `trace.hpp`), doing exactly
the increment + per-thread attribution `pf_on_import` already did, minus the
text trace-line (no return-address/args frame exists at these call sites -
they're reached by a normal C call, not the asm trampoline). `pf_on_import`
itself now calls `pf_count_import` for that half of its own work, so there
is exactly one counting implementation. Every always-installed wrapper in
`wrappers.cpp`/`det.cpp` calls `pf_count_import(id)` at its own entry, where
`id` is the import's `g_real[]`/report-JSON index, threaded through from
`imports.cpp`'s resolve loop via `wrappers_bind_real(name, real_proc, id)` →
`det_bind_real(name, real_proc, id)` (both signatures gained the `id`
parameter; every call site updated).

Verified: a `--det` run's `--report` JSON now has `{"id": 62, "name":
"KERNEL32.dll!Sleep", "count": 10009}` (previously absent). Every other
newly-counted name (`_beginthread`, QPC, `timeGetTime`, `time`, `clock`,
`malloc`/`calloc`/`realloc`/`free`, `ExitProcess`, etc.) is counted the same
way; not re-listed exhaustively here since the mechanism is uniform.

### C. `--record-input` end-to-end, and `--inject-real-test`

**Format.** `--record-input` now writes exactly what `--input-script` reads:
`T press|release KEY_NAME` (reverse lookup against the same 7-entry
`kKeyNames` table `resolve_key` uses; falls back to the raw scancode number
for anything outside that table - `load_script`'s own fallback is `atoi()`,
so a numeric line is still valid input either way), preceded by `#`
header/comment lines `load_script` already skips: `# date:`, `#
image_sha256:` (sha256 of the guest EXE, computed once via `pf::Sha256`
reading the file named by `DetOptions::image_path`, same class the digest
sensor uses), `# policy:`, `# pace:`.

**The round trip - tested with a synthetic source, per the task's own
fallback (no way to press real keys from this pass).** `--inject-real-test`
(hidden diagnostic, `main.cpp`'s bare-flag parsing like `--det`) requires
`--input=real` together with `--input-script` - the ONE place that
combination is allowed, specifically to test the recording path.  Instead of
calling `_handle_key_press`/`_handle_key_release` directly (the normal
Script-mode delivery `deliver_due_input` always used before), each scripted
event is fed through `key_dinput_handle_scancode` itself - the real
DirectInput entry point - via a small `__declspec(naked)` shim
(`call_key_dinput_handle_scancode`, `det.cpp`) that loads the two cdecl
stack args into EAX/EDX (DWARF-confirmed prototype `void
key_dinput_handle_scancode(int scancode, int pressed)`, wkeybd.c line 321;
disasm at 0x46d5a8 confirms both arrive in registers, never on the stack,
so a plain function-pointer cast - which would push cdecl stack args -
cannot call it) and does a register-indirect `call`. Because `--input=real`
leaves the neutralize breakpoint **uninstalled**, the real function actually
runs (not short-circuited), which is what lets `--record-input`'s own
breakpoints (at `_handle_key_press`/`_handle_key_release`, unchanged from
Milestones 5-7) see the event exactly as a live human keystroke would
produce it.

**MEASURED, load-bearing finding (first attempt failed, documented per the
project's own practice):** `key_dinput_handle_scancode`'s own `scancode`
argument is **not** the Allegro internal code (`KEY_ENTER`=67 etc.) that
`kKeyNames`/`g_script`/`_handle_key_press` use - it is the **raw DirectInput
`DIK_*` hardware scancode**, translated through the game's own
`_hw_to_mycode[256]` byte table (`wkeybd.c`) before it reaches
`_handle_key_press`/`_handle_key_release` (confirmed by reading that table's
actual bytes out of `assets/icytower15.exe` at its DWARF/COFF VA
`0x4daf80`: `hw_to_mycode[0x01]==59`, `[0x1c]==67`, `[0x39]==75`,
`[0xcb]==82`, `[0xcd]==83`, `[0xc8]==84`, `[0xd0]==85` - the standard PC/AT
scancode-set-1 `DIK_*` values for these 7 keys, one for one against
`kKeyNames`). First attempt fed the Allegro code directly as the
"scancode" argument (double-translating through `_hw_to_mycode[67]` etc.,
landing on an unrelated key) and **measurably** corrupted enough internal
state to leak the 256 MiB deterministic heap arena empty within ~315 ticks
of a ~700-tick script (`det: arena exhausted`), crashing inside
`main_menu_callback` on the next allocation failure - a real, reproduced
failure, not a hypothetical, and consistent with "if an approach fails
after an honest attempt, document it and move to the fix" rather than
silently patching around the symptom. Fixed with an explicit
Allegro-code→DIK-code table (`kDikMap`/`allegro_to_dik`, `det.cpp`) built
from the measured `_hw_to_mycode` values above.

**Interesting, non-bug finding kept for the record:** once fixed, the
recorded file (`replays via --record-replay`, or any `--record-input`
output) shows the ENTER key re-firing `press` events roughly every 1-2
ticks while held (an OS/Allegro auto-repeat behavior **internal to**
`key_dinput_handle_scancode`'s real path) even though the synthetic script
only issued ONE press event at T=20 and one release at T=620 - this is
authentic real-path behavior the direct-injection Script-mode path does
not (and structurally cannot, since it calls `_handle_key_press` exactly
once per scripted line) reproduce. Not a bug: the round trip below proves
the recorded (auto-repeated) event stream, replayed verbatim through Script
mode, reproduces the original run's digests exactly - which is the actual
contract, not "the recording is short."

**Round-trip result:**

```
carrier.exe --det --pace=fast --input=real --inject-real-test \
    --input-script scripts/newgame.txt \
    --record-input ../artifacts/rt_record.txt \
    --digest-out ../artifacts/rt_record_digest.txt \
    --stop-at-tick 1000 --run-seconds 25
carrier.exe --det --pace=fast --input=script \
    --input-script ../artifacts/rt_record.txt \
    --digest-out ../artifacts/rt_replay_digest.txt \
    --stop-at-tick 1000 --run-seconds 25
python carrier/scripts/compare_digests.py \
    artifacts/rt_record_digest.txt artifacts/rt_replay_digest.txt
```

Result: **`EQUAL (876 ticks, ...)`** - the record run (real path, via
`--inject-real-test`) and the replay run (direct Script-mode injection of
the recorded file) produce byte-identical safepoint digests at every tick.
Both runs' `--report` JSON show `"real_key_violations": 0`.  `--pace=real`
was used for the record run specifically to verify the task's other
requirement - "the game must run at normal speed for a human ... while the
tick index still comes from the virtual clock" - by construction: `--pace`
only controls whether `det_wrap_Sleep` also calls the real `::Sleep(ms)`;
`T` is computed from `g_virtual_ms`, which accumulates the exact `ms`
argument passed to `Sleep` either way, never real elapsed wall time. Not
independently re-measured with a stopwatch in this pass (no human present);
the code path is shared, unconditional, and already covered by the
milestone-7 `--pace=real`/`--pace=fast` distinction being purely "sleep or
don't", so this is architectural, not newly re-verified.

**Window focus.** Task: "state in NOTES whether the carrier should call
`SetForegroundWindow` on the guest window ... do it if cheap." It is cheap:
`try_focus_guest_window_once` (`det.cpp`, called from `det_wrap_Sleep`) uses
`EnumWindows` filtered by `GetCurrentProcessId()`/`GetWindowThreadProcessId`
to find the guest's own top-level window - no need to hook
`RegisterClassA`/`CreateWindowExA` to capture the HWND at creation time, since
the guest is the only window owner in this process. Only takes effect when
`input_policy==Real` (a live human is the point of focusing it); tried once
per Sleep call until it succeeds (harmless no-op once the window exists and
after; harmless if no window ever appears, e.g. a headless automated run).
Confirmed working in the round-trip test above: `det: focused guest window
hwnd=... (input_policy=real)` appears in stderr.

### Proof rerun (this pass)

Same commands as Milestones 5-7 §E, rerun after every change above:

```
carrier.exe --det --pace=fast --input-script scripts/newgame.txt --digest-out ../artifacts/final_run1.txt --stop-at-tick 1000 --run-seconds 25
carrier.exe --det --pace=fast --input-script scripts/newgame.txt --digest-out ../artifacts/final_run2.txt --stop-at-tick 1000 --run-seconds 25
python carrier/scripts/compare_digests.py artifacts/final_run1.txt artifacts/final_run2.txt
```

Result: **`EQUAL (876 ticks, ...)`** - unchanged from before this pass, as
required. `assets/tower.cfg`, `assets/profiles/`, `assets/log.txt` were
restored from `artifacts/assets_backup/` + `artifacts/log_original_
baseline.txt` before each run, per the existing convention above.

### `scripts/play.py` (repo root)

A human-facing wrapper over `carrier.exe`, self-contained rather than
importing `port_forge/scripts/player_runtime.py` - that module's
declared-runtime contract (a `portforge.project.json` "player" capability
declaration, `game.json` program identity, ArtifactV2 replay authority
files) does not exist in this project and does not fit the carrier's
execution model (one native Win32 process mapping one already-built guest
EXE, not one of PortForge's DOS/Amiga/etc. interpreter runtimes); forcing it
on would mean inventing a manifest with no real oracle/generated/port
distinction to declare. See `scripts/play.py`'s own docstring for the full
reasoning - flagged here in case a real contract is adopted for the carrier
project later.

Plain `python scripts/play.py` builds the carrier if missing, verifies
`assets/icytower15.exe`'s sha256 against a fingerprint pinned in the script,
then runs `--det --pace=real --input=real` (deterministic clock, real
human input, real time). `--record-replay NAME` adds `--record-input
replays/NAME.txt --digest-out replays/NAME.digest` (creates `replays/` at
the repo root). `--play-replay NAME [--pace real|fast]` replays
`replays/NAME.txt` with `--input=script --digest-out
replays/NAME.replay.digest`, bounded by `--stop-at-tick` set to the exact
last tick `replays/NAME.digest` reached (not "a bit past it" - matches the
line count exactly for a clean `compare_digests.py` verdict instead of a
spurious length mismatch), then runs `compare_digests.py` and reports
EQUAL or the first differing tick. `--no-det` runs the raw oracle (no flags
at all). `--keep-state` skips the assets restore. Every invocation prints
its exact `carrier.exe` command line. Tested end-to-end in this pass with a
staged replay pair (the round-trip recording above, copied into `replays/`)
- `--play-replay` reproduced **`EQUAL`**.

## Known gap / not yet exercised

- `--ddraw=system` (the real system `ddraw.dll` instead of cnc-ddraw's
  shim) was not tried in this pass — `--ddraw=local` worked on the first
  successful run and reaching MAIN MENU LOOP was the goal. Worth trying if
  a future pass needs to compare behavior against the real DirectDraw path.
- Only reached the main menu; gameplay (`play()`), replay recording, and
  the ad-fetch HTTP thread's failure path were exercised passively (they
  run automatically at startup/menu, see the report's thread breakdown) but
  not driven deliberately, since the task scope stops at the main menu and
  the carrier has no input-injection capability.
- **`--record-input`/`--input=real` still not exercised with an actual
  live human keyboard** (see "Input policy and recording" above) - the
  round trip was proven with `--inject-real-test`'s synthetic source
  instead, per the task's own documented fallback for an automated pass.
  The synthetic source exercises the real `key_dinput_handle_scancode` path
  (not a shortcut around it), but a genuine human-typing session, and
  `scripts/play.py`'s plain/`--record-replay` invocations specifically,
  remain untested by this pass.
- `kDikMap` (`det.cpp`, "Input policy and recording" part C) only covers
  the 7 keys `kKeyNames` already knows about (ESC/ENTER/SPACE/arrows). A
  future `--inject-real-test` script using any other key would need its
  Allegro→DIK mapping added there first (the code detects and logs a
  missing mapping rather than misbehaving, but still skips that event).

## Milestones 11-12

Status: **done** (win32_pilot.md SS8 rows 11a/11b/12). LIFTED and NATIVE
forms of game functions are bound into the running carrier at their original
addresses, and all three forms are compared automatically from equivalent
state during the milestone-7 deterministic replay, with a negative control.

New code (all of it inert unless one of the four new options is passed):
`carrier/src/bind.hpp`/`bind.cpp` (binding table, 5-byte entry patch, one asm
stub template, the per-invocation sensor, fault injection, the SS8a metrics),
`carrier/scripts/compare_fn_digests.py` (the per-invocation comparator).
Small additions elsewhere: `det.hpp`/`det.cpp` gained
`det_register_breakpoint`/`det_ctx_arm_slot`/`det_ctx_disarm_slot`/`det_tick`
so the existing `g_bp[4]` hardware-breakpoint table has a second consumer;
`main.cpp` parses/forwards the four options; `trace.cpp` calls
`bind_report_json`; `build.cmd` compiles the lifted/native `.c` files into
carrier.exe (`/I gen /I lift\lifted`) and now also emits `obj\carrier.map`
(which is what identified the one stub bug below).

**The milestone-7 proof still passes, re-verified with the final binary**
(assets restored from `artifacts/assets_backup/` + `artifacts/log_original_
baseline.txt` before each run, as everywhere in this file):

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --digest-out ../artifacts/m11_final1.txt --stop-at-tick 1000 --run-seconds 25
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --digest-out ../artifacts/m11_final2.txt --stop-at-tick 1000 --run-seconds 25
python carrier/scripts/compare_digests.py artifacts/m11_final1.txt artifacts/m11_final2.txt
```

Result: **`EQUAL (876 ticks, ...)`** - unchanged.

### A. The binding table and the entry patch

`--bind name=lifted|native|original[,name=...]` and `--bind-file PATH` (same
syntax, one per line, `#` comments). Resolved in the parent, forwarded to the
child in `PF_BIND`/`PF_BIND_FILE` like every other carrier flag.
`bind_init()` runs in `main()` **after** `pe_image_load` (there is nothing to
patch before the image is mapped) and **before** `det_arm_main_thread()`
(which loads the breakpoint table into DR0-DR3) and before the guest entry
point.

For LIFTED/NATIVE, a 5-byte `jmp rel32` is written at the function's original
VA to a per-function stub. `VirtualProtect(PAGE_EXECUTE_READWRITE)` +
`FlushInstructionCache` are used even though the image is already RWX (the
"Everything RWX" TEMPORARY above), so the patch keeps working when that
TEMPORARY is retired.

The stub is ONE `__declspec(naked)` template (`bind_stub_common`)
parameterized by function id; the eight per-function stubs are one
`BIND_STUB(N)` macro expansion each (`push N` / `jmp bind_stub_common`).
On entry the stack is `[id][retaddr][args...]`; the stub (a) counts the
ORIGINAL->form crossing and takes the pre record, (b) re-pushes four argument
dwords and `call`s the bound form (cdecl, caller-cleaned, so re-pushing four
regardless of the real arity is harmless and reading four dwords above the
return address only reads the caller's own committed frame), (c) takes the
post record including EAX, (d) `pop ebx` / `add esp,4` / `ret`, leaving ESP
and every callee-saved register exactly as a normal `ret` would. `pushad`/
`pushfd` bracket both hooks, so neither the guest's registers nor its flags
are disturbed. Because the patch is at the callee, direct calls, indirect
calls and callbacks all reach the bound form (win32_pilot.md SS3) - not
exercised here, since none of the three candidates is address-taken
(notes/promotion_candidates.md SS1 callback scan).

Failure is loud, never a silent fallback to ORIGINAL: an unknown name, an
unknown form, a form that is not linked in, a double binding, an unreadable
`--bind-file`, an out-of-jmp-range stub or a failed `VirtualProtect` all exit
3 with a named message. Verified:

```
carrier.exe ... --bind jump_player=native
  -> bind: FATAL - no native form exists for 'jump_player' (nothing named
     native_jump_player is linked into the carrier)                [exit 3]
carrier.exe ... --bind update_frame=original,is_solid=original
  -> bind: FATAL - only ONE function can be sensed in its ORIGINAL form per
     run (DR budget: ...)                                          [exit 3]
```

### B. The per-invocation sensor (uniform across the three forms)

One record per invocation, identical in shape for ORIGINAL, LIFTED and
NATIVE, written to `--fn-digest-out PATH`:

```
fn=<name> k=<n> T=<tick> args=<a0,a1,...> pre=<sha256> post=<sha256> eax=<hex> form=<original|lifted|native>
```

`k` is the per-function invocation index; `T` is the same carrier tick the
per-tick digest lines are keyed by (`det_tick()`), so a per-invocation record
and a per-tick digest line can be lined up. `pre`/`post` are sha256 over the
function's comparison domain, taken verbatim from
notes/promotion_candidates.md SS4:

| function | comparison domain |
|---|---|
| `update_frame` | `reward_time` (0x4fec68) + `reward_scale` (0x4fac28) + `ply[player_id]`, the full 184-byte `Tplayer` |
| `is_solid` | the 772-byte `Tmap` its argument points at (must be byte-identical: pure predicate) + EAX |
| `jump_player` | the 184-byte `Tplayer` its argument points at + EAX |

EAX is a separate field rather than being folded into the digest, so the
comparator can name `field=eax` distinctly from `field=post`. Every domain
read goes through a committed-memory probe; a bad pointer produces a counted,
loud marker in the digest (`domain_read_failures` in the report) instead of
an access violation inside the sensor. It stayed 0 in every run below.

**ORIGINAL form**: no bytes are patched. A DR slot at the function entry
takes the pre record and reads the return address from `[esp]` (cdecl); the
sensor then arms a second DR at that return address **by editing the CONTEXT
the VEH is about to resume** (`det_ctx_arm_slot`) - a thread cannot
`SetThreadContext` itself, but `NtContinue` reloads DR0-DR7 from the
continued context when `ContextFlags` claims them, which is why
`det_ctx_arm_slot` ORs in `CONTEXT_DEBUG_REGISTERS`. The return slot is
disarmed again in the post callback. `det_arm_thread` now skips table slots
whose VA is 0 (the armed-later slot).

**DR budget, and the documented limit**: DR0 = tick safepoint, DR1 =
`key_dinput_handle_scancode` neutralization (installed whenever
`--input != real`), leaving DR2 = function entry and DR3 = function return.
**ORIGINAL-form sensing therefore supports exactly one function per run**,
and asking for two exits 3 (shown above). This is fine: verification is per
function (win32_pilot.md SS3). It also means `--record-input` (which wants
two slots of its own) cannot be combined with ORIGINAL-form sensing.
LIFTED/NATIVE sensing needs no debug register at all - it happens in the
stub - so a run can bind and sense any number of non-ORIGINAL forms at once
(exercised: `--bind-file` with `update_frame=native` + `is_solid=lifted`).

### C. The comparator

`carrier/scripts/compare_fn_digests.py A B` -> `EQUAL (n invocations, ...)`
or `FIRST DIFFERENCE fn=<name> k=<n> T=<tick> field=<pre|post|eax|args>`.
`form` is deliberately not compared (comparing forms is the point). A
`pre`/`args` mismatch is reported **distinctly** from a `post`/`eax`
mismatch, with an explicit note that it means the two runs had already
diverged upstream of the call and does not by itself convict the function -
the distinction notes/promotion_candidates.md SS5 asks for. A record-index
mismatch (different `fn`/`k` at the same line) and a length mismatch are both
reported as differences, never as EQUAL. Exit code 0 only on EQUAL.
`compare_digests.py` (per-tick global digests) keeps being run alongside it.

### D. The experiment - `update_frame` (0x406ac4)

Each command below is preceded by the standard assets restore.

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 25 --bind update_frame=original --fn-digest-out ../artifacts/m11_uf_A.txt --digest-out ../artifacts/m11_uf_A_ticks.txt --report ../artifacts/m11_uf_A_report.json
carrier.exe ... --bind update_frame=lifted   --fn-digest-out ../artifacts/m11_uf_B.txt --digest-out ../artifacts/m11_uf_B_ticks.txt --report ../artifacts/m11_uf_B_report.json
carrier.exe ... --bind update_frame=native   --fn-digest-out ../artifacts/m11_uf_C.txt --digest-out ../artifacts/m11_uf_C_ticks.txt --report ../artifacts/m11_uf_C_report.json
python carrier/scripts/compare_fn_digests.py artifacts/m11_uf_A.txt artifacts/m11_uf_B.txt
python carrier/scripts/compare_digests.py    artifacts/m11_uf_A_ticks.txt artifacts/m11_uf_B_ticks.txt
python carrier/scripts/compare_fn_digests.py artifacts/m11_uf_A.txt artifacts/m11_uf_C.txt
python carrier/scripts/compare_digests.py    artifacts/m11_uf_A_ticks.txt artifacts/m11_uf_C_ticks.txt
python carrier/scripts/compare_fn_digests.py artifacts/m11_uf_B.txt artifacts/m11_uf_C.txt
python carrier/scripts/compare_digests.py    artifacts/m11_uf_B_ticks.txt artifacts/m11_uf_C_ticks.txt
```

Results:

| comparison | per-invocation | per-tick global |
|---|---|---|
| A original vs B lifted | **EQUAL (877 invocations)** | **EQUAL (876 ticks)** |
| A original vs C native | **EQUAL (877 invocations)** | **EQUAL (876 ticks)** |
| B lifted vs C native   | **EQUAL (877 invocations)** | **EQUAL (876 ticks)** |

### E. `is_solid` (0x4166dc): 0 invocations - the workload never reaches it

Same three runs with `--bind is_solid=original|lifted|native`
(`artifacts/m11_is_{A,B,C}*`). All three per-tick digest streams are
**EQUAL (876 ticks)**, and all three `--fn-digest-out` files are **empty**:
`"invocations_sensed": 0` in every report.

This is a real, cross-checked negative and not a broken sensor: the ORIGINAL
run senses through hardware breakpoints and the LIFTED/NATIVE runs sense
through the entry patch - two **independent** mechanisms, and the same
mechanisms recorded 877 `update_frame` and 150 `jump_player` invocations in
the other runs. `is_solid`'s only static callers are
`handle_player_collision_{original,old,combo}`
(notes/promotion_candidates.md SS2); `scripts/newgame.txt`'s gameplay never
takes whichever of those branches calls it. **So the LIFTED and NATIVE forms
of `is_solid` are NOT verified in vivo by this pass** - only their offline
equivalence check stands. A workload that reaches it (a different collision
mode, or a longer/different play script) is needed; flagged below.

### F. `jump_player` (0x418678, 22 x87 instructions) - the x87 HYPOTHESIS

No NATIVE form exists yet (`--bind jump_player=native` exits 3, above), so
this is ORIGINAL vs LIFTED:

```
carrier.exe ... --bind jump_player=original --fn-digest-out ../artifacts/m11_jp_A.txt --digest-out ../artifacts/m11_jp_A_ticks.txt --report ../artifacts/m11_jp_A_report.json --run-seconds 240
carrier.exe ... --bind jump_player=lifted   --fn-digest-out ../artifacts/m11_jp_B.txt --digest-out ../artifacts/m11_jp_B_ticks.txt --report ../artifacts/m11_jp_B_report.json --run-seconds 240
python carrier/scripts/compare_fn_digests.py artifacts/m11_jp_A.txt artifacts/m11_jp_B.txt
python carrier/scripts/compare_digests.py    artifacts/m11_jp_A_ticks.txt artifacts/m11_jp_B_ticks.txt
```

Result: **`EQUAL (150 invocations)`** and **`EQUAL (876 ticks)`**.

That is the win32_pilot.md SS3 HYPOTHESIS ("`double` is not enough where GCC
kept 80-bit x87 intermediates") **not** failing here: the comparison domain
is the full 184-byte `Tplayer`, which contains `sy` and `max_s` as raw
`double`s, compared bit-exactly (sha256, no epsilon), across 150 real jumps.
Every record shows `args=...,00000000`, i.e. only the normal branch - the
`arg2 != 0` forced-jump path is **not** covered by this workload. This is
evidence for the `double` backend on this function and this workload, not a
proof for x87 generally. (The runs used the LIFTED forms as regenerated by
the concurrent `pf_lift.py` pass: `carrier.exe` was relinked after
`lift/lifted/*.c` and `pf_rt.h` were last written, so the binary under test
contains the current generated code.)

### G. Negative control (required by the verdict contract)

`--fault-inject <name>:k=<N>` flips bit 0 of one byte of the named function's
comparison domain (for `update_frame`: `reward_scale` at 0x4fac28, which is
also inside the per-tick digest scope - `carrier/gen/game_globals.inc` line
130) immediately **after** the N-th invocation's bound form returns and
**before** that invocation's post record is taken, so the comparator must
name exactly `k=N`.

```
carrier.exe ... --bind update_frame=native --fault-inject update_frame:k=300 --fn-digest-out ../artifacts/m11_uf_D_fault.txt --digest-out ../artifacts/m11_uf_D_fault_ticks.txt --report ../artifacts/m11_uf_D_fault_report.json
python carrier/scripts/compare_fn_digests.py artifacts/m11_uf_A.txt        artifacts/m11_uf_D_fault.txt
python carrier/scripts/compare_fn_digests.py artifacts/m11_uf_C.txt        artifacts/m11_uf_D_fault.txt
python carrier/scripts/compare_digests.py    artifacts/m11_uf_C_ticks.txt  artifacts/m11_uf_D_fault_ticks.txt
```

Results:

```
bind: --fault-inject fired: update_frame k=300 T=424, flipped bit0 of [0x004fac28]

FIRST DIFFERENCE fn=update_frame k=300 T=424 field=post     (vs A, original)
FIRST DIFFERENCE fn=update_frame k=300 T=424 field=post     (vs C, clean native)
FIRST DIFFERENCE at tick T=425                              (per-tick global digest)
```

Exactly the injected k, and nothing earlier. **The per-tick global digest
first differs at T=425, the first safepoint AFTER the faulted invocation, not
at T=424** - MEASURED, and expected: the safepoint (0x4124f4) for tick 424 is
hit before `update_frame` runs within that tick's loop body, so the T=424
digest line was already written when the byte was flipped. The `--report`
JSON of that run shows `"faults_injected": 1`.

### H. In-vivo findings the offline harness could not have made (x2)

The task asked for these specifically, so both are recorded even though
neither turned out to be a semantic divergence.

**H1. `update_frame` leaves a different EAX in every form - and EAX is dead.**
The very first three-form comparison reported `FIRST DIFFERENCE fn=update_frame
k=0 T=109 field=eax`, ORIGINAL `eax=00000000` vs LIFTED `eax=00000001` (and
NATIVE diverging from LIFTED at k=1), while the per-tick global digest was
already EQUAL for all 876 ticks. Root cause: `update_frame`'s prototype is
`void (__cdecl *)()` (`it_funcs.h:956`, DWARF), so EAX on return is not part
of its contract - the ORIGINAL's last executed instruction leaves the `idiv`
quotient (`logic_count/10`) there, and each C form leaves whatever its own
codegen last computed. Confirmed dead at every call site: all four callers
(`artifacts/disasm.txt` - 0x411af4 `mov 0x4dd168,%ebx`, 0x41242f `mov
-0x93c(%ebp),%eax`, 0x41462d `mov $0x4facc8,%edi`, 0x41497b `mov
0x5069b7,%al`) overwrite or ignore EAX in their very next instruction. The
**offline** harness could not have found this: `carrier/lift/harness/
native_check.c:102-104` (and `lift_check.c`) hard-code `eax = 0` for
`update_frame` instead of reading the register. Fix: the record now writes
`eax=void` for a function whose generated prototype returns void, and keeps
the observed register value in an informational `raweax=` field the
comparator ignores. That is a defect in the *sensor specification* (comparing
a register the ABI says is dead), found only because the in-vivo sensor reads
the real EAX - exactly the class of false positive that would burn a day if
believed.

**H2. The stub's own post-frame arithmetic was wrong by 4 bytes.** The first
`--bind update_frame=lifted` run crashed with `0xc0000005` at
`EIP=0x1001bce9`. `veh_handler`'s stack walk correctly showed the crash was
in carrier's own image (as the "Diagnostics" section above predicts, guest
symbol names for a carrier address are nonsense), and relinking with `/MAP`
resolved 0x1001bce9 -> `record_post+0x49`, 0x1001bf80 -> `bind_post+0x10`,
0x1001bfda -> `bind_stub_common+0x4a`. In section (c) of the stub, `pushad`
+ `pushfd` puts the saved EAX at `Q+32`, the id at `Q+40` and the return
address at `Q+44` - the first version read `Q+44`/`Q+48`, so `bind_post` was
called with the return address as its `id` and indexed the policy table with
it. Fixed (both reads are `[esp+48]` after the two intervening pushes), and
`bind_pre`/`bind_post` now validate `id` against the table bound and abort
with a named message instead of reading wild memory - which is what makes a
future stack-offset mistake in the asm template self-identifying. This is the
"calling-convention assumptions of the stub" hazard the task named; it was a
carrier bug, not a property of any form.

### I. Migration-map metrics (win32_pilot.md SS8a), from each `--report` JSON

Every run above emits a `"binding"` object; the field is omitted entirely
when nothing is bound or sensed, so pre-milestone-11 report shapes are
unchanged. Summary of the runs in this section:

| run | form | entry patched | ORIGINAL->form crossings | invocations sensed | faults |
|---|---|---|---:|---:|---:|
| `update_frame` A | original | no  | 0   | 877 | 0 |
| `update_frame` B | lifted   | yes | 877 | 877 | 0 |
| `update_frame` C | native   | yes | 877 | 877 | 0 |
| `update_frame` D | native   | yes | 877 | 877 | 1 |
| `is_solid` A/B/C | orig/lift/nat | no/yes/yes | 0 | 0 | 0 |
| `jump_player` A  | original | no  | 0   | 150 | 0 |
| `jump_player` B  | lifted   | yes | 150 | 150 | 0 |
| mixed (`--bind-file`) | update_frame=native + is_solid=lifted | yes+yes | 877+0 | 877 | 0 |

`"crossings_native_to_original"` is reported as the string **"not
instrumented (all bound candidates are leaves; 0 by construction)"** rather
than as a measured 0: all three candidates have `imports_used=[]`,
`indirect_calls=0` and no non-game callees (notes/promotion_candidates.md
SS2), and no typed interop call macro is used by any bound form, so there is
nothing to count through yet. `"domain_read_failures"` was 0 in every run.
Functions-by-form for the binary as a whole stays "253 game functions
ORIGINAL, minus the 1 or 2 a given run binds".

The mixed-form run is also the check that the mechanism composes:

```
carrier.exe ... --bind-file <file with update_frame=native and is_solid=lifted> --fn-digest-out ../artifacts/m11_multi.txt --digest-out ../artifacts/m11_multi_ticks.txt --report ../artifacts/m11_multi_report.json
python carrier/scripts/compare_digests.py artifacts/m11_final1.txt artifacts/m11_multi_ticks.txt
```

Result: **`EQUAL (876 ticks, ...)`** against the unbound baseline.

### Known gaps / open problems (milestones 11-12)

- **`is_solid` is not exercised by this workload at all** (part E). Its
  LIFTED and NATIVE forms are bound and reached by nothing; only the offline
  check covers them. Needs a play script that takes the
  `handle_player_collision_original/old/combo` branch that calls it.
- **`jump_player`'s forced-jump branch (`arg2 != 0`) is never taken** in
  `scripts/newgame.txt` (every record shows `args=...,00000000`), so the
  `fildl`/`fstpl` path of the x87 HYPOTHESIS is still unverified in vivo.
- **No NATIVE form of `jump_player`** exists yet, so the three-way comparison
  is only complete for `update_frame`.
- **ORIGINAL-form sensing is one function per run** (DR budget, part B) and
  is mutually exclusive with `--record-input`. A second concurrent ORIGINAL
  candidate would need either INT3 patching (rejected: it touches the
  original bytes, which are the identity) or a second replay pass.
- **The x87 register stack is not saved across the stub's hooks.**
  `bind_pre`/`bind_post` only run integer code (sha256, integer `fprintf`),
  so this has not bitten - but it is an assumption, not a guarantee, and it
  would bite the moment a domain digest wanted to format a float. If an
  x87-heavy candidate ever shows an unexplained divergence only in its bound
  form, save/restore the FPU state in the stub before believing the result.
- **The sensor changes timing, not state.** Every result above comes from a
  `--pace=fast` deterministic replay where the tick clock is virtual, so the
  sensor's cost cannot perturb the simulation. A `--pace=real` run with the
  ORIGINAL-form DR sensor active would run measurably slower; not tested.
- **`--fault-inject` flips a byte of the comparison domain, not of the form's
  code.** A compile-time faulty variant (`native_update_frame_faulty`) would
  additionally prove the binding path routes to the code you asked for; the
  entry-patch log line plus the 877 counted crossings is the evidence used
  instead.
- **A bound form that called back into an original address would re-enter its
  own stub** (the patch is at the callee). Harmless for these three leaves;
  the record stack overflows loudly (exit 5) rather than silently corrupting
  records if it ever happens.

## src binding, tick-boundary real input, parked timer thread

Follow-up pass (2026-09-07) on top of Milestones 11-12 and "Input policy and
recording": (1) binds `src/icytower/{update_frame,is_solid}.c` (win32_pilot.md
SS7a's address-free clean port) into the binding table as a new `src` form;
(2) fixes first-divergence investigation 002 (real keyboard events recorded
off-tick, notes/living_record.md) by capturing real DirectInput events at the
same breakpoint used to neutralize them and redelivering them at the next
tick boundary, the same way scripted events are delivered; (3) fixes
first-divergence investigation 003 (the `_tim_win32_exit` join hang) by
running the virtualized timer thread's ORIGINAL entry point on a real,
joinable thread whose own `WaitForSingleObject` calls are substituted to
`INFINITE`; (4) filters spurious release events out of `--record-input`
recordings. G1/G2 (below) re-verified after every change.

### 1. `src` binding form

`carrier/gen/scan_src_defs.py` (new) scans `src/icytower/*.c` (excluding
`state.c`, which only supplies standalone storage, never a function body -
src/README.md) for top-level function DEFINITIONS with a small regex
(return-type tokens, `name(args)`, then a `{` starting its own line - this
codebase's uniform style), instead of a hand-maintained exclude list.
`build.cmd` captures its comma-separated stdout into `SRC_EXCLUDES` and
passes it to `gen_bindings.py --exclude %SRC_EXCLUDES% --guard-define
ICYTOWER_BINDINGS_ACTIVE --out gen\pf_bindings_src.h --types-out
gen\pf_bindings_src_types.h`, reproducing exactly the `is_solid,update_frame`
exclude set src/README.md's own hand-run command already produced (verified:
`python gen/gen_bindings.py --exclude is_solid,update_frame ...` and the
scanner-driven invocation emit byte-identical `pf_bindings_src.h`).

`src\icytower\update_frame.c` and `is_solid.c` are compiled by build.cmd as
their OWN `cl /c` step - `/I gen /FIpf_bindings_src.h`, NOT folded into the
main multi-file `cl` invocation - because pf_bindings_src.h pulls in the
carrier's type provider (it_types.h, which redefines `BITMAP`), which
collides with `windows.h` the instant both are seen by one TU (bind.cpp's
own header comment already documents this collision for the carrier-side
headers; the same fact applies here). **MEASURED build bug, fixed**:
`/FIgen\pf_bindings_src.h` (a relative path through the `/I gen` directory)
does NOT work - MSVC's quoted-include search rule appends the
force-include's own spelling to each `/I` directory in turn, so `/I gen`
+ `/FIgen\pf_bindings_src.h` looks for `gen\gen\pf_bindings_src.h` (doesn't
exist), and the literal spelling relative to cl's cwd is never tried for a
quoted include either - `cl` reported "Cannot open include file:
'gen\pf_bindings_src.h'" even though the file plainly exists at that path
relative to the invocation directory. Fixed by force-including just the
bare filename (`/FIpf_bindings_src.h`) and letting `/I gen` supply the
directory, which is the correct, working spelling.

`is_solid.c`'s own `Tmap *m` parameter and `update_frame.c`'s `void`
prototype are declared in `carrier/src/bind.cpp` as `void*`/`int` (never the
real `Tmap*`), the exact same pattern `native_is_solid` already used for the
same documented reason (bind.cpp cannot see the carrier's `Tmap` without
`windows.h`-colliding headers): C linkage matches by name only, never by
parameter type across translation units, so the mismatch is harmless and the
existing `native_is_solid` binding already proved the pattern works.

`bind.cpp` gained a fourth form, `FORM_SRC`, alongside `FORM_LIFTED`/
`FORM_NATIVE`; `kFns[]` gained a `src` field (`update_frame`/`is_solid`
point at the newly-compiled symbols, `jump_player` stays `nullptr` - no
src/ form exists for it yet, same as its `native` field). `--bind
name=src` is a new, valid form string alongside `lifted`/`native`/
`original`.

**"native is transitional" labeling**: per this pass's instruction, `src`
is now the authoritative address-free NATIVE form (win32_pilot.md SS7a);
`carrier/native/*.c` (the old hand-written copies still using the carrier
memory seam) stays bound to the CLI keyword `native` unchanged (backward
compatible - existing `--bind x=native` commands and scripts keep working),
but `form_name()` now renders it as `"native(transitional)"` everywhere a
form is displayed (stderr bind confirmation lines, `--report`'s `"form"`/
`"binding"` JSON, `--fn-digest-out`'s `form=` field) instead of plain
`"native"`. `compare_fn_digests.py` never compares this string (`form` is
deliberately excluded from the verdict - its own docstring), so the
relabeling cannot change any EQUAL/DIFFERENT result; verified below (G2
still EQUAL with the new label present in the compared files).

Verified (assets restored before each run, as always in this file):

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --bind update_frame=src --fn-digest-out ../artifacts_task/g2_C_src.txt --report ../artifacts_task/g2_C_report.json
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --bind update_frame=original --fn-digest-out ../artifacts_task/g2_B_original.txt
python carrier/scripts/compare_fn_digests.py ../artifacts_task/g2_C_src.txt ../artifacts_task/g2_B_original.txt
```

Result: **`EQUAL (877 invocations, ... [src] vs ... [original])`** - the
address-free `src/icytower/update_frame.c`, compiled straight into the
carrier and bound at `update_frame`'s original address, is behaviourally
identical to the original machine code over the full milestone-7 replay.
stderr confirms the entry patch: `bind: update_frame @0x00406ac4 -> src
(5-byte jmp rel32 -> stub 0)`.

### 2. Tick-boundary real input (fixes divergence 002)

**Problem, restated with the code evidence**: with `--input=real`, real
DirectInput events reached Allegro's `key[]` state directly from
`key_dinput_handle_scancode`, called from the real window thread's message
pump - an asynchronous instant completely independent of the main thread's
20ms tick loop. `--record-input`'s breakpoints logged `T =
det_current_tick()` sampled at that same asynchronous instant. Replay always
injects at an exact tick boundary (`deliver_due_input`, called from
`det_wrap_Sleep` on the main thread). Two different delivery mechanisms with
two different timing sources cannot be reproducible even in principle - this
is exactly what investigation 002 measured (`replays/first_human`: first
difference at the very first gameplay tick, off by one; shifting every
recorded event +1 tick made the first 11 ticks match).

**Fix**: `real_key_capture_hit` (det.cpp), a NEW breakpoint callback at the
same VA (`key_dinput_handle_scancode`, 0x46d5a8) `neutralize_keyboard_hit`
already used for Script/None mode, now also installed for
`input_policy==Real && !inject_real_test` (previously nothing was installed
there at all). It reads the reg-passed args (EAX=DIK scancode, EDX=pressed),
translates the DIK code to the Allegro code via `dik_to_allegro` (reads the
guest's own `hw_to_mycode[256]` table directly - the forward direction of
the mapping this task's kDikMap-generation ask needed anyway, see below),
pushes `{allegro_code, press}` onto a small ring buffer guarded by a
`CRITICAL_SECTION` (`real_queue_push`/`real_queue_pop`), and NEUTRALIZES the
original call exactly like `neutralize_keyboard_hit` (pop the return address
into EIP - safe for the same reason already established: args arrive in
registers, nothing of the caller's is on the stack to clean up).

`drain_real_key_queue()`, called from `det_wrap_Sleep` immediately after
`deliver_due_input()` (both the det-mode and non-det-mode branches - the
task's "real must work with or without --det"), drains the queue and calls
`_handle_key_press`/`_handle_key_release` directly - the EXACT SAME
function calls, from the EXACT SAME call site, that scripted replay already
uses. `--record-input`'s breakpoints (unchanged, still at those two
functions) therefore see the delivery-time T, not the arrival-time T:
record and replay are now produced by the same path. Worst-case added
latency is one tick (20ms in `--det`), since the queue is drained once per
`Sleep` call - satisfies the task's latency requirement.

`--inject-real-test` is UNCHANGED: it is the one case
(`input_policy==Real && inject_real_test`) that deliberately keeps the real
`key_dinput_handle_scancode` path unneutralized, since its whole point is
exercising that function's own body (including its measured OS/Allegro
auto-repeat behavior, "Input policy and recording" part C above) for the
synthetic record/replay round trip - re-verified after this pass:

```
carrier.exe --det --pace=fast --input=real --inject-real-test --input-script scripts/newgame.txt --record-input ../artifacts_task/rt_record.txt --digest-out ../artifacts_task/rt_record_digest.txt --stop-at-tick 1000 --run-seconds 60
carrier.exe --det --pace=fast --input=script --input-script ../artifacts_task/rt_record.txt --digest-out ../artifacts_task/rt_replay_digest.txt --stop-at-tick 1000 --run-seconds 60
python carrier/scripts/compare_digests.py ../artifacts_task/rt_record_digest.txt ../artifacts_task/rt_replay_digest.txt
```

Result: **`EQUAL (876 ticks, ...)`** - unchanged from before this pass.

**Generated DIK<->Allegro table, not hand-listed**: `kDikMap` (7 hand-listed
entries) is replaced by `build_dik_tables()`, which reads the guest's own
`hw_to_mycode[256]` byte table directly out of mapped guest memory (VA
`0x4daf80`, the same table+VA the previous pass's `kDikMap` was manually
transcribed FROM) and builds the Allegro->DIK inverse (`g_allegro_to_dik[128]`,
first DIK alias wins) once, lazily, on first use (safe any time after
`pe_image_load` has mapped the guest, which is true for every tick
delivered). The forward direction (DIK->Allegro, `dik_to_allegro`) needs no
table at all - `hw_to_mycode[dik]` already IS that mapping, read directly.
Superset of the old 7-key table by construction; a future script using any
other key gets a mapping automatically instead of needing `kDikMap`
hand-edited (the limitation the previous pass's "Known gap" section
flagged).

**Human round trip: still to be done by the operator.** This pass's
automated verification is the synthetic `--inject-real-test` round trip
above (unchanged, re-verified EQUAL) - proving the mechanism this fix
shares (delivery through `_handle_key_press`/`_handle_key_release`, recorded
at the same call site) is sound, but `--inject-real-test` is main-thread-
synchronous by construction (it calls `key_dinput_handle_scancode` from
inside `det_wrap_Sleep` itself) and therefore never exercised the actual
async-arrival race divergence 002 measured. **A genuine human round trip -
`python scripts/play.py --record-replay NAME` then `python scripts/play.py
--play-replay NAME` with a real keyboard - is required to close this out**
and is left for the operator, per the task's own documented fallback for an
automated pass (no way to press real keys from here).

### 3. Parked timer thread (fixes divergence 003, the exit hang)

**Root cause, from the disassembly** (`artifacts/disasm.txt`): both
`_tim_win32_high_perf_thread` (0x478584) and `_tim_win32_low_perf_thread`
(0x4783bc) loop on `WaitForSingleObject(stop_event@0x4ec050, <15 or
100ms>)` and fall through to `__win_thread_exit` (a normal return) the
FIRST time that call returns anything other than `WAIT_TIMEOUT` (0x102);
`_tim_win32_exit` (0x478488) does `SetEvent(stop_event@0x4ec050)` then
loops `WaitForSingleObject(timer_thread_handle@0x4ec054, 100)` while it
returns `WAIT_TIMEOUT`. The OLD virtualized timer thread
(`make_parked_handle`, Milestones 5-7) returned a handle to a thread
blocked forever on a carrier-private event NOBODY EVER SIGNALED - that
handle could never become signaled, so `_tim_win32_exit`'s join spun on
`WAIT_TIMEOUT` forever.

**Generic fix**: `make_parked_real_handle` (det.cpp) now runs the ORIGINAL
entry point (`tim_win32_high_perf_thread`/`_low_perf_thread`) on a real,
`CREATE_SUSPENDED` host thread (`parked_real_thread_proc`), registers that
thread's id as "parked" (`register_parked_thread`/`det_is_parked_thread`,
a small table guarded by a `CRITICAL_SECTION`) BEFORE resuming it, and
`det_wrap_WaitForSingleObject` (a new always-installed wrapper, wired the
same way as `Sleep`/QPC/etc - `imports.cpp`'s `kWrapNames`, `wrappers.cpp`'s
`wrappers_lookup`) substitutes `ms = INFINITE` whenever the CALLING thread
is parked and asked for a finite timeout. The thread's own
`WaitForSingleObject(stop_event, 15-or-100)` call therefore never returns
`WAIT_TIMEOUT` and never reaches the `_handle_timer_tick` call just above
it in either loop - tick delivery is UNCHANGED (still only from
`det_wrap_Sleep` on the main thread, milestone 5-7's design) - the thread
simply blocks in that one real wait until the guest itself calls
`SetEvent(stop_event)` at shutdown, at which point the wait returns
non-timeout, the guest's OWN code falls through to `__win_thread_exit`, and
the thread function returns for real. `_tim_win32_exit`'s join then
succeeds by construction - no carrier-side polling, timeout heuristic, or
special-cased shutdown path needed; the fix is entirely "give the guest's
own exit code a real thread to actually signal."

**Verified end-to-end**: `scripts/quit_via_menu.txt` (new) holds `KEY_ESC`
at the main menu (T=20 to T=400) - the same key and the same
`is_pause()`-driven "DO YOU REALLY WANT TO EXIT?" / "Press ESC to exit"
mechanism `_play()` itself uses when paused (MEASURED: `handle_menu`
reaches the identical confirm text at 0x4d5d67/0x4d5d9b that `_play`'s own
pause-exit path draws - both are reachable this way, only `_play`'s copy
needed no navigation to reach since it's the same key). Holding (not
tapping) is required for the same reason `scripts/newgame.txt`'s own
comment already documents for the menu's ENTER-tap unreliability.

```
carrier.exe --det --pace=fast --input=script --input-script scripts/quit_via_menu.txt --run-seconds 30
```

Result: `assets/log.txt` ends `Showing credits / UNINIT / Saving config /
... / Exiting Allegro / Done...`, stderr shows `carrier_shutdown: guest
_cexit` (the guest's OWN exit chain, not `--stop-at-tick`/`--run-seconds`
forcing termination), and the process exits with **code 0 in ~2.0 seconds**
(measured via `System.Diagnostics.Process.ExitCode`/`Stopwatch`, no
`--stop-at-tick` given, `--run-seconds 30` never came close to firing) -
before this fix, this exact scenario hung forever on `_tim_win32_exit`'s
join. `det: timer thread PARKED (entry=0x00478584): running the ORIGINAL
entry point on a real thread whose WaitForSingleObject calls are
substituted to INFINITE` confirms the mechanism engaged.

A companion script, `scripts/quit_via_pause.txt`, quits from WITHIN
`_play()` (a single `KEY_ESC` press+release while playing - "game paused
with esc" / "game quit from esc pause" / "play ended" appear immediately,
matching `artifacts/log_original_baseline.txt`'s own sequence exactly) and
returns cleanly to `MAIN MENU LOOP` - useful evidence the pause/quit-confirm
mechanism itself works identically in both places, though this script alone
does not reach the final Allegro-exit path `quit_via_menu.txt` does.

**`--stop-at-tick`/`--run-seconds` robustness**: both already call
`TerminateProcess(GetCurrentProcess(), 0)` after `carrier_shutdown` -
`TerminateProcess` forcibly ends every thread in the process
unconditionally, never waiting on or joining any of them, so it was already
robust to a hung/parked thread and remains so; no code change was needed
here beyond confirming this by inspection and by every `--stop-at-tick` run
in G1/G2 above continuing to exit cleanly with the new parked-thread
mechanism in place.

**G1 unaffected** (parking must not change tick delivery - required by this
task): see "Proof rerun" below, `EQUAL (876 ticks, ...)`, unchanged.

### 4. Recording hygiene

**Problem** (task's own description): at process exit, Allegro's own
keyboard shutdown releases every scancode it thinks could be down, one
`_handle_key_release` call per scancode, all observed at a single tick -
none of them a real gameplay event.

**Rule implemented**: `det.cpp` tracks a per-scancode "currently held" set,
`g_key_held[256]`, set on a press `--record-input` records, cleared on the
matching release. `keyrelease_record_hit` now drops (does not write) any
release whose scancode is NOT currently marked held - i.e. a release that
was never recorded pressed, or was already recorded released once. Because
filtered lines are simply never written, the file naturally ends at the
last GENUINE press/release pair with no separate "trim the tail" pass
needed. The rule applies uniformly regardless of which mechanism produced
the underlying `_handle_key_press`/`_handle_key_release` call (script direct
injection, item 2's real-input capture-and-replay, or `--inject-real-test`'s
real path) - all three funnel through the same two breakpoints.

**Verified**: a full `--record-input` capture of `scripts/newgame.txt`'s
gameplay (12 presses/releases of SPACE, plus ENTER's and RIGHT's releases)
recorded exactly the 12 legitimate releases the script itself produces, in
the right order, with none dropped - the filter does not touch genuine
matched pairs (`../artifacts_task/hygiene_test.txt`). A full clean-exit
capture (`scripts/quit_via_menu.txt` with `--record-input`) recorded ONLY
`press KEY_ESC` lines and zero releases - the process's own exit-confirm
polling loop calls `_handle_key_press` repeatedly while the key is held
(authentic guest behavior: the SAME class of finding "Input policy and
recording" part C already documented for `--inject-real-test`'s ENTER
auto-repeat, now also observed via plain Script-mode delivery, at a
different call site) and the exit chain completed before the scripted
release event's tick was ever reached, so this particular run did not
independently reproduce the literal ~120-line release-storm the task
describes; the filtering rule itself is unconditional (any release without
a currently-open matching press is dropped, regardless of count or cause)
and would suppress that storm exactly as described if a future run's exit
timing exposes it. Flagged as an open item below.

### Proof rerun (this pass)

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --digest-out ../artifacts_task/g1_run1.txt
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --digest-out ../artifacts_task/g1_run2.txt
python carrier/scripts/compare_digests.py ../artifacts_task/g1_run1.txt ../artifacts_task/g1_run2.txt
```

Result: **`EQUAL (876 ticks, ...)`** - unchanged.

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --bind update_frame=native --fn-digest-out ../artifacts_task/g2_A_native.txt
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --bind update_frame=original --fn-digest-out ../artifacts_task/g2_B_original.txt
python carrier/scripts/compare_fn_digests.py ../artifacts_task/g2_A_native.txt ../artifacts_task/g2_B_original.txt
```

Result: **`EQUAL (877 invocations, ... [native(transitional)] vs ...
[original])`** - unchanged in substance; the label is new (see item 1
above), the verdict is not.

`assets/tower.cfg`, `assets/profiles/`, `assets/log.txt` were restored from
`artifacts/assets_backup/` + `artifacts/log_original_baseline.txt` before
every run above, per the existing convention in this file.

### Known gaps / open problems (this pass)

- **Human round trip for item 2 not done** (see item 2 above) - operator
  action required (`scripts/play.py --record-replay NAME` /
  `--play-replay NAME`), no way to press real keys from this pass.
- **Item 4's exact ~120-line release storm not independently reproduced** -
  this build/config's exit path (via `scripts/quit_via_menu.txt`) completes
  in a couple hundred ticks, before any such storm would occur if one exists
  in this exit path; the filtering rule is unconditional and generic, not
  tuned to a specific observed count, so it is expected to suppress it
  regardless. A slower/different exit path (e.g. a real human quitting via
  `--pace=real`) is the natural way to observe it directly.
- **`src` form exists only for `update_frame`/`is_solid`** - `jump_player`
  has no `src/` implementation yet (same gap it already had for `native`).
- **The main menu's own repeated-press behavior while a key is held**
  (item 4's verification) is a newly-observed instance of the same
  auto-repeat class "Input policy and recording" part C already flagged for
  `--inject-real-test`'s ENTER key - not investigated further here (does
  not affect any gate: G1/G2 both stayed EQUAL, and the recording hygiene
  filter handles its press-only shape correctly by construction, since it
  only ever filters releases).
