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

## Milestones 8-9

Status: **done for the in-process rewind, with one measured structural
failure recorded for the cross-process form** (win32_pilot.md SS8 rows 8 and
9). A stable safepoint snapshot of the running carrier exists, an in-process
restore from it is certified by replay equality per
notes/portforge_capsule.md SS D ("restore -> suffix == cold -> suffix"), the
negative control names the restore tick, and a snapshot can be inspected
offline by named global and traced instruction by instruction.

New code (all of it inert unless one of the six new options is passed):
`carrier/src/snapshot.hpp`/`snapshot.cpp` (capture, restore, the trap-flag
trace window), `carrier/scripts/certify_snapshot.py` (the proof script),
`carrier/scripts/pf_inspect.py` (milestone 9's offline inspector). Small
additions elsewhere: `det.hpp`/`det.cpp` gained the pinned RNG
(`det_wrap_rand`/`det_wrap_srand`, `--rng-selftest`) and
`det_state_save`/`det_state_load` (the carrier-owned snapshot component) and
call the two snapshot hooks from `safepoint_hit`; `bind.hpp`/`bind.cpp`
gained `bind_state_save`/`bind_state_load`; `imports.cpp`/`wrappers.cpp`
wrap `rand`/`srand`; `main.cpp` parses/forwards the new options;
`trace.cpp` calls `snapshot_report_json`; `build.cmd` compiles
`snapshot.cpp`.

New options: `--snapshot-at-tick T --snapshot-out DIR`,
`--restore-from DIR`, `--restore-at-tick T2`, `--restore-fault`,
`--trace-window N [--trace-window-out PATH]`, `--rng-selftest`.

**Both gates re-verified with the final binary** (assets restored from
`artifacts/assets_backup/` + `artifacts/log_original_baseline.txt` before
every run, as everywhere in this file):

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --digest-out ../artifacts_task/m89_g1_1.txt
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --digest-out ../artifacts_task/m89_g1_2.txt
python carrier/scripts/compare_digests.py artifacts_task/m89_g1_1.txt artifacts_task/m89_g1_2.txt
   -> EQUAL (876 ticks, ...)                                            [G1]

carrier.exe ... --bind update_frame=src      --fn-digest-out ../artifacts_task/m89_g2_src.txt
carrier.exe ... --bind update_frame=original --fn-digest-out ../artifacts_task/m89_g2_orig.txt
python carrier/scripts/compare_fn_digests.py artifacts_task/m89_g2_src.txt artifacts_task/m89_g2_orig.txt
   -> EQUAL (877 invocations, ... [src] vs ... [original])               [G2]
```

### A. RNG pinning (win32_pilot.md SS5 "pin the LCG"), finally needed

Through milestone 7 `rand`/`srand` were deliberately left UNWRAPPED, and
that was correct for replay: the effective seed comes from `time()`
(notes/replay_format.md sec 2), which `det_wrap_time` already pins, and
msvcrt's LCG has no other host-entropy input. What milestone 8 changes is
not determinism but **reachability**: the RNG state lived inside msvcrt.dll's
per-thread CRT data, i.e. outside the guest image, the arena and the guest
stack - outside every component a snapshot can capture. A rewind would have
rewound the game and left the RNG running forward.

`det_wrap_rand`/`det_wrap_srand` (`det.cpp`, IAT-wrapped like `Sleep`/`time`
et al., always installed, forwarding to the real msvcrt outside `--det`)
move that state into `g_rng_state`, a carrier static that is part of
`DetSavedState`. **KNOWN**, the generator is
`state = state*214013 + 2531011; return (state>>16) & 0x7fff`, default state 1.

Two independent verifications, both required by the task:

1. **Unit check against the real msvcrt.dll `rand()`** - `--rng-selftest`
   (runs right after `imports_init` has resolved the real entry points,
   before any guest code; exits 0 on match, 4 on mismatch):

   ```
   carrier.exe --rng-selftest
      -> rng-selftest: OK (5000 values, 0 mismatches)
   ```

   1000 values for each of 5 seeds (1, 12345, 0, 2531011, 0xdeadbeef).

2. **G1 unchanged, byte for byte.** The `--digest-out` stream of a pinned
   run is EQUAL to the stream produced by the binary from *before* the
   pinning (`artifacts_task/m8_base1.txt` vs `m8_g1a.txt`: `EQUAL (876
   ticks)`), i.e. the pinned LCG reproduces msvcrt's sequence in vivo over
   the whole replay, not only in the unit test.

`rand()` is called 14 times up to tick 400 in this workload (manifest
`rng_calls`), all from the main thread - so the single global state is
sufficient here. INFERRED limitation, documented rather than fixed: msvcrt's
own `rand` is per-thread; if a future workload calls it from two threads,
this wrapper merges them.

### B. What a snapshot is

A directory, format id `portforge-win32-carrier-snapshot-v1`: a JSON
manifest plus six raw component files, each with its own sha256 in the
manifest (`port_forge/src/core/sha256.hpp`, the same class the tick digest
uses). Measured at tick 400 of `scripts/newgame.txt`:

| component | file | address | size | what it is |
|---|---|---|---:|---|
| CONTEXT | `context.bin` | - | 716 | the main thread's full `CONTEXT`: integer regs, EIP, EFLAGS, `FloatSave` and `ExtendedRegisters` |
| .data | `data.bin` | 0x004bc000 | 95 988 | the FULL section |
| .bss | `bss.bin` | 0x004dd000 | 223 608 | the FULL section |
| arena | `arena.bin` | 0x20000000 | 89 781 680 | the deterministic heap arena, used range only |
| stack | `stack.bin` | 0x0e1fef30 | 4 304 | the guest stack's LIVE range, `[ESP, 0x0e200000)` |
| carrier | `carrier.bin` | - | 1 816 | `DetSavedState` + `BindSavedState` |
| | `manifest.json` | | ~2.3 KB | tick, per-component sha256, image identity, decoded carrier state |
| **total** | | | **90 108 112** | |

The FULL sections are captured, not just the 151 game-owned globals the
per-tick digest hashes: Allegro's timer queue, `key[]` state, mixer state
and menu state all live in those sections and all have to come back for the
guest to keep running.

**The x87 control word round-trips.** MEASURED: the VEH's exception context
on this host arrives with `ContextFlags = 0x0001007f`, i.e.
`CONTEXT_FULL | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS |
CONTEXT_EXTENDED_REGISTERS` - so both the legacy `FloatSave` block and the
FXSAVE area are present and are restored by `NtContinue`. The manifest
records the control word twice, from the context and from a live `fnstcw`
executed at the safepoint; both read **0x037F**, matching win32_pilot.md
SS6a's `FNINIT`/PC=64 finding exactly. That is the value that has to survive
a rewind for x87-heavy code (`jump_player`, `line_intersect`) to stay
bit-equal.

The carrier-owned component (`det.hpp`'s `DetSavedState`, `bind.hpp`'s
`BindSavedState`) is win32_pilot.md SS6's "PortForge-owned externalized
state": virtual clock ms and `g_units_reported`, the tick index, the
`--input-script` cursor, the real-key ring buffer, the `--record-input`
key-held set, the pinned RNG state, the arena bump pointer, and the
per-invocation sensor's counters. Without it a rewind would move the game
back and leave the clock, the input cursor, the RNG and `k=` running
forward.

Deliberately NOT captured: host objects (HWND, COM device pointers, kernel
HANDLEs, GDI objects, DirectSound mixer position). The manifest says so in a
`host_objects` field, and records `capture_pid` - see part G.

### C. Where the snapshot is taken, and the ordering that makes the proof trivial

At the existing tick safepoint, VA 0x4124f4 inside `play()`, from inside
det.cpp's VEH on the guest main thread - the one place in this carrier where
a full, resumable `CONTEXT` exists and no host call is in flight
(win32_pilot.md SS6; notes/portforge_capsule.md SS D "a snapshot may only be
taken at a declared semantic SAFE POINT"). `--snapshot-at-tick` /
`--restore-from` therefore arm the same DR0 sensor `--digest-out` uses
(`DetOptions::force_safepoint`).

`safepoint_hit` calls the two hooks in this order:

```
snapshot_on_safepoint_pre(ctx)   // a due RESTORE happens here
int T = det_current_tick()       // ... so T is already the restored tick
<write the digest line for T>
snapshot_on_safepoint_post(ctx)  // a due CAPTURE happens here
<--stop-at-tick check>
```

That ordering is what makes the certification a plain row-by-row file
comparison with no fixups: the capturing run's T=400 digest line and the
restoring run's first line are computed from the same memory at the same
safepoint, so they must be the same string.

### D. How the restore works, and the one structural constraint

`--restore-from DIR` (at the first safepoint) or `--restore-at-tick T2
--restore-from DIR` (rewind while running; `--restore-from` may be omitted
when `--snapshot-out` names the same directory - the in-run form).

1. Read all six files and the manifest, verify every component's sha256
   against the manifest, check `CONTEXT`/section/`CarrierState` sizes.
   Everything that can allocate or take a lock happens **here**, before any
   thread is suspended.
2. Refuse loudly if `saved.Esp < live Esp` - see the constraint below.
3. Suspend every other thread in the process (`CreateToolhelp32Snapshot` /
   `SuspendThread`; measured: 20 of them in a normal run - the window
   thread, the parked timer thread, and cnc-ddraw's own workers), `memcpy`
   the four memory components, resume them. This removes the torn-write half
   of the concurrency hazard; it does not remove the rewound-lock half (part
   H).
4. `det_state_load` / `bind_state_load` restore the carrier-owned state.
5. Optionally inject the `--restore-fault` byte (part F).
6. Overwrite `*ctx` with the saved CONTEXT, then put the **live** debug
   registers back: DR0-DR3 belong to *this* run's sensor table (a different
   `--bind`, `--record-input`, ... produces a different table), and rewinding
   them would disarm the very safepoint breakpoint that has to keep firing.
   `ContextFlags` gets `CONTEXT_DEBUG_REGISTERS` OR'd in so `NtContinue`
   actually reloads them. `det_veh_handler` then sets `EFLAGS.RF` on the way
   out, so the breakpoint at the restored EIP - which IS the safepoint,
   0x4124f4 - does not immediately re-trigger.

**The constraint (KNOWN, enforced):** a VEH runs on the interrupted thread's
own stack, i.e. on the guest stack, with its frames strictly BELOW the
current ESP. The restore overwrites `[saved.Esp, 0x0e200000)`. That is safe
exactly when `saved.Esp >= live Esp`. Both are safepoints inside `play()` at
the same call depth, and MEASURED they are not merely comparable but
identical: `esp=0e1fef30` on all 876 safepoint lines of the milestone-7
workload (one distinct value across the whole digest file).
`snapshot.cpp` checks this and aborts with a named message rather
than smashing its own frame; the fix if it ever fires is a scratch stack for
the write-back, not a relaxed check.

### E. Certification (the proof)

`carrier/scripts/certify_snapshot.py`, three subcommands (`rewind`,
`restore`, `fn`). **Why it compares rows POSITIONALLY and not keyed by
tick** - a MEASURED trap worth recording: the carrier tick index is
`virtual_ms/20`, and `play()`'s catch-up branch can consume two game ticks
inside one 20 ms window, so a digest stream legitimately contains a REPEATED
tick number. In this workload `T=659` appears twice. A first draft of the
comparison keyed rows by tick, silently kept a different one of the two
duplicates on each side, and reported a single-tick "divergence" at T=659
that did not exist - reproducibly, three runs in a row. It was disproved by
dumping raw `.data`+`.bss` at T=659 from both passes
(`DET_DUMP_MEM_TICK`/`DET_DUMP_MEM_PATH`, the diagnostic milestone 5-7 left
in place) and finding **zero** differing bytes in all 151 game globals, and
by recomputing the sha256 offline: identical. Recorded because "compare
streams by index, not by a key you assumed was unique" is a framework-level
lesson, and because the near-miss is exactly the kind of false divergence
that burns a day.

```
# 1. cold run: capture the anchor at tick 400, run to 1000
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt \
  --stop-at-tick 1000 --run-seconds 60 --digest-out ../artifacts_task/m8_cold.txt \
  --snapshot-at-tick 400 --snapshot-out ../artifacts_task/snap400

# 2. in-run rewind: capture at 400, rewind at 700, run to 1000
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt \
  --stop-at-tick 1000 --run-seconds 120 --digest-out ../artifacts_task/m8_rewind.txt \
  --snapshot-at-tick 400 --snapshot-out ../artifacts_task/snap_rw --restore-at-tick 700 \
  --bind update_frame=src --fn-digest-out ../artifacts_task/m8_rewind_fn.txt

python carrier/scripts/certify_snapshot.py rewind artifacts_task/m8_rewind.txt \
       --anchor 400 --cold artifacts_task/m8_cold.txt
python carrier/scripts/certify_snapshot.py fn artifacts_task/m8_rewind_fn.txt
```

Results:

```
in-run rewind: 575 rows before the rewind (T=126..699), 602 rows after (T=400..1000); anchor T=400
EQUAL (301 rows, ticks T=400..699, m8_rewind.txt[pre-rewind] vs m8_rewind.txt[post-rewind])
EQUAL (602 rows, ticks T=400..1000, m8_cold.txt[cold from anchor] vs m8_rewind.txt[post-rewind])
EQUAL (301 invocations, k=276..576, m8_rewind_fn.txt[pre-rewind] vs m8_rewind_fn.txt[post-rewind])
```

Read as notes/portforge_capsule.md SS D's contract:

- line 2 is `restore(anchor) -> suffix == cold -> anchor -> suffix` **within
  one process**: ticks 400..699 are replayed a second time after the rewind
  and every one of the 301 digests is byte-identical;
- line 3 is the same contract **across processes**: the 602-row suffix
  produced after the rewind equals, row for row, the suffix an INDEPENDENT
  cold run produced from the same anchor tick, all the way to
  `--stop-at-tick 1000`;
- line 4 is the same contract one level finer, on `bind.cpp`'s
  per-invocation sensor: 301 `update_frame` invocations (in its `src` form,
  the address-free clean port) replay with identical `args`/`pre`/`post`
  after the rewind. The sensor's `k` index goes backwards at the rewind
  because the counters are part of the snapshot - which is what lets the two
  passes be lined up at all.

Reproduced three times independently (`m8_rewind{,2,3}.txt`) plus once with
the rewind at a different tick (`--restore-at-tick 500`,
`m8_rewind500.txt`), all EQUAL.

### F. Negative control

`--restore-fault` flips bit 0 of one byte of the restored `.bss` -
`reward_scale` at 0x4fac28, chosen because it is inside .bss AND inside the
per-tick digest scope (`carrier/gen/game_globals.inc`), so the very first
line written after the restore must already differ.

```
carrier.exe ... --snapshot-at-tick 400 --snapshot-out ../artifacts_task/snap_fault \
    --restore-at-tick 700 --restore-fault --digest-out ../artifacts_task/m8_fault.txt
python carrier/scripts/certify_snapshot.py rewind artifacts_task/m8_fault.txt --anchor 400 \
       --cold artifacts_task/m8_cold.txt
```

```
snapshot: --restore-fault fired: flipped bit0 of [0x004fac28] (reward_scale, restored .bss)
FIRST DIFFERENCE at tick T=400 (row 1)
  [pre-rewind]:  228018f267de38e2b1fd48d9cccde36b80aac426b18b6d9d9b965233982869cb
  [post-rewind]: 347ef3994aa5bafaf263ef7dff3674ac86d99590fc869cd8922859e30d86e08c
```

Exactly the restore tick, row 1, and nothing earlier - the comparator names
the fault where it was injected.

### G. The cross-process restore FAILS, and why (MEASURED, load-bearing)

The task's first certification form is two separate processes: a cold run
that snapshots at 400, then `carrier.exe --restore-from DIR` which starts
fresh, reaches its own first safepoint (T=126) and restores. **This does not
work, and the reason is structural, not a bug in the snapshot.**

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt \
  --stop-at-tick 1000 --run-seconds 90 --digest-out ../artifacts_task/m8_xproc_restore.txt \
  --restore-from ../artifacts_task/snap400
```

```
snapshot: WARNING - this snapshot was captured by pid 24084, this is pid 35924. ...
snapshot: restored T=400 ... in 1572.12 ms (20 other thread(s) suspended ...)
400 228018f267de38e2b1fd48d9cccde36b80aac426b18b6d9d9b965233982869cb esp=0e1fef30 ...
=== UNHANDLED EXCEPTION ===
code=0xc0000005 addr=0x00486ac3 (_parallelogram_map+0x3e7)
  #0 _parallelogram_map_standard+0x14c  #1 _pivot_scaled_sprite_flip+0x85
  #2 draw_frame+0xff5                   #3 play+0x1813   #4 _mangled_main+0x3e9
```

Two facts in that output, both important:

1. **The state restore itself is exact.** The single digest line the run
   managed to write, `400 228018f2...`, is byte-identical to the cold run's
   T=400 line. Registers, x87 state, `.data`, `.bss`, the arena, the stack
   and the carrier state all came back correctly across a process boundary.
2. **The first frame that touches a host-backed resource dies.** The crash
   is inside Allegro's sprite blitter, reached from `draw_frame`, on pointers
   `0x001c0000` / `0x003b0000` / `0x00110000` - DirectDraw surface memory
   addresses, which the restore wrote back from a *different* process's
   `.bss` (they occur at 0x4ebe7c / 0x4ebedc / 0x4ebf2c in Allegro's own
   .bss and again inside arena-resident `BITMAP.line[]` arrays). A second
   run of the same command died differently but in the same class:
   `0xc0000005` at `key_dinput_handle+0x38` on the WINDOW thread, i.e. a
   rewound DirectInput COM device pointer.

This is the same category milestone 5-7 already measured from the other side
(carrier/NOTES.md "the digest region": ~20-30 Allegro/CRT/DirectX globals
hold host-object identities that vary per process) - and win32_pilot.md SS6
predicted it in words: "Raw host handles and COM pointers are **not** durable
state; they are re-bound." Nothing re-binds them yet.

**Why an exclusion list cannot rescue it** (the second honest attempt, and
why it was not built): the obvious generic fix is to keep the LIVE value for
every word that two independent cold runs disagree on. That census is
derivable - `pf_inspect.py diff` now produces it offline (part I) - but it
cannot work for the arena: the stale pointers also live inside `BITMAP`
structs in arena blocks that, in the restoring process at tick 126, have not
been allocated yet. There is no live value to keep. Re-binding requires
re-creating the host resources and writing their new addresses into the
restored structures - i.e. exactly the logical-handle / COM-proxy layer
win32_pilot.md SS6 escalation step (3) describes and SS9 defers ("No
logical-handle layer until restore proves a resource cannot be re-bound").
Restore has now proved it. Recorded as the milestone's main open problem.

Consequences applied instead of a silent failure: the manifest records
`capture_pid`, and a restore into a different pid prints a loud WARNING that
names the hazard and points here. The in-run rewind (`--restore-at-tick`) is
the supported form, and it is what the certification above uses.

The cheapest path to making the cross-process form work is probably NOT a
proxy layer but win32_pilot.md SS8 row 9a's headless mode: with Allegro's
GDI/memory driver there is no DirectDraw surface and no DirectInput device,
so `screen` and every game bitmap would be ordinary arena memory at
deterministic addresses. Flagged, not attempted here.

### H. Hazards, one by one

- **Critical sections held by another thread at the restore instant**
  (KNOWN hazard, mitigated but not eliminated). `.data`/`.bss` contain
  `CRITICAL_SECTION` objects - Allegro's `wnd` critical section among them -
  whose `LockCount`/`OwningThread`/`RecursionCount` are rewound along with
  everything else. If the window thread held one at the restore instant, it
  will later leave a lock whose counters were rewound. Mitigation
  implemented: every other thread is suspended for the duration of the
  `memcpy`s, which removes torn writes; it does not remove the rewound-lock
  problem. Not observed in any of the ~10 in-run rewinds run here (all
  reached `--stop-at-tick 1000` cleanly and all certified EQUAL), which is
  evidence but not a proof. INFERRED reason it is survivable in practice:
  in `--det` the timer thread is parked inside one `WaitForSingleObject`
  ("parked timer thread" above) and the DirectInput thread is never spawned,
  so the only real contender is the window thread's message pump.
- **DirectSound mixer position** - not snapshotted, and in `--det` it does
  not advance under carrier control either: Allegro's mixer runs in its timer
  thread, which is parked. Audio after a rewind resumes wherever the host
  buffer happens to be; it is presentation, not state (win32_pilot.md SS4a),
  and is not in any comparison domain.
- **Key state.** Restoring `.data`/`.bss` restores Allegro's `key[]` array
  wholesale, and the carrier state restores the `--input-script` cursor, so
  scripted input resumes from the anchor's cursor - measured: the manifest's
  `input_script_cursor` is 11 at tick 400 and the rewound pass replays events
  11.. again, which is why the two passes are digest-equal. Real (`--input=
  real`) events queued between the anchor and the rewind are dropped: the
  real-key ring buffer is part of the snapshot and is rewound with it.
- **Arena bump pointer vs. allocations made after the snapshot.** Safe *by
  construction* because the arena is bump-only: rewinding `g_arena_offset`
  simply forgets every allocation made after the anchor, and the same
  sequence of `malloc` calls hands out the same addresses again. This is the
  one place the milestone 5-7 "leak-only allocator" gap turns into a feature.
- **File positions.** MEASURED, not assumed. A run traced with
  `--trace-imports fopen,fclose,fread,fwrite,fseek,fgets,fputc,_open,_close,
  _read,_write,_lseek,CreateFileA,CloseHandle` (2153 records,
  `artifacts_task/m8_io_trace.log`) shows **zero** file-I/O imports of any
  kind after the game logs `" play started"` - the last `_lseek`/`_read`
  (Allegro's `normal_getc` refilling a `PACKFILE`) precedes it by 11 log
  lines, and the only calls after it are `log2file`'s own
  `fopen`(+0x4a)/`fclose`(+0x8f) pairs, 73 balanced pairs over the run: the
  game opens, appends one line and closes `log.txt` per message. So **no
  file handle is read, written or seeked across a tick inside `play()`**,
  and no logical file table is needed for this workload. Three descriptors
  opened at startup are never closed (`csv_open`, one of `load_frames`'s two,
  `load_sounds`) - a pre-existing leak in the game, and irrelevant here since
  nothing touches them during the tick window. `log.txt` is an append-only
  side effect, compared as a file, never restored.
- **The `--report` JSON's binding counters after a rewind** are the
  RESTORED timeline's counters, not the number of lines physically written
  (`invocations_sensed: 877` for a run whose `--fn-digest-out` has 1178
  lines). That is correct - they are snapshot state - but it is a trap when
  reading a rewound run's report.

### I. Milestone 9: inspection and the instruction microscope

**`carrier/scripts/pf_inspect.py`** reads a snapshot directory OFFLINE and
decodes it with the GENERATED interop metadata, never hand-written offsets:
`carrier/gen/interop_index.json` (name, VA, C type, CU per global),
`carrier/gen/it_types.h` (struct members in order, with types),
`carrier/gen/it_types_check.c` (the authoritative `sizeof`/`offsetof` values
the carrier's own build verifies), `artifacts/coff_symbols.json` (each
global's size, by the same gap-to-next-symbol rule `gen_game_globals.py`
uses). It optionally maps the guest EXE's `.text`/`.rdata` so `char *`
globals dereference into strings.

```
python carrier/scripts/pf_inspect.py show artifacts_task/snap400
  snapshot artifacts_task/snap400
    format=portforge-win32-carrier-snapshot-v1 tick=400 safepoint=0x004124f4
    eip=0x004124f4 esp=0x0e1fef30 ebp=0x0e1ff938 eflags=0x00000202 x87cw=0x037f
    carrier state: virtual_ms=8000 (tick 400) rng_state=993538943 (0x3b38337f)
                   rng_calls=14 input_script_cursor=11 arena_bump=89781680
    reward_time     @0x004fec68  int    = 0
    reward_scale    @0x004fac28  fixed  = 0 (fixed 16.16 = 0)
    player_id       @0x004fe518  int    = 462
    ...
    ply[462] = 0x2123fa58  (Tplayer, 184 bytes)
      +0    x       double = 555.0        +8   y     double = 431.0
      +16   sx      double = -0.16564...  +24  sy    double = 0.0
      +60   frame   int    = 0            +76  dead  int    = 0
      +84   angle   fixed  = 0 (fixed 16.16 = 0)   ... (full struct dump)
```

`show --globals a,b,c` / `show --all` / `player DIR [--index N]` cover the
rest. Note the arena pointer: `ply[462]` is `0x2123fa58`, inside the
deterministic arena, which is why a snapshot can dereference it at all.

**`diff` reports the first difference as (global, member, byte)**, and
splits differences by whether they are inside the per-tick digest scope
(`carrier/gen/game_globals.inc`) - the distinction that decides whether a
difference is a certification failure or not:

```
python carrier/scripts/pf_inspect.py diff artifacts_task/snap400 artifacts_task/snap401
  FIRST DIFFERING GLOBAL: gFLDADMutex @0x004bc004 (pthread_mutex_t, 4 bytes, fld_adspot.c)
    first differing byte: +0 (VA 0x004bc004)  A=58 B=48
  FIRST DIFFERING GLOBAL INSIDE THE PER-TICK DIGEST SCOPE:
    someCounter__play @0x004dd320 (int, 4 bytes, main.c)  +0  A=275 B=276
  13 named game global(s) differ (6 of them inside the digest scope): ...
```

**An independent offline re-derivation of the milestone 5-7 finding**, worth
recording as a cross-check: diffing two snapshots taken at the SAME tick by
two INDEPENDENT cold processes reports

```
python carrier/scripts/pf_inspect.py diff artifacts_task/snap400 artifacts_task/snap400b
  No differing global is inside the per-tick digest scope ...
  7 named game global(s) differ (0 of them inside the digest scope):
     gFLDADMutex  sLogMutex__log2file  gFLDADThread .p+0
     eyecandy_selection .caption+0   gravity_selection .caption+0
     floor_size_selection .caption+0   scroll_speed_selection .caption+0
```

- exactly the 3 host-identity globals `gen_game_globals.py` excludes by name
and the 4 `Tmenu_selection` `caption` fields it caps to 8 bytes, and nothing
else. The generator's two hand-justified decisions are now independently
confirmed, by name and by member, from snapshots.

**`--trace-window N` - the microscope.** After a restore, the main thread is
single-stepped with `EFLAGS.TF` from inside the same VEH (this is
win32_pilot.md SS3's "trap-flag stepping through a VEH over a bounded
window", the recovery for what native execution gives up). Each step logs
index, EIP, `symbols_describe` name (from `artifacts/functions.json`) and the
registers that changed; the window closes itself after N and clears TF.

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt \
  --stop-at-tick 1000 --run-seconds 180 --digest-out ../artifacts_task/m9_trace_ticks.txt \
  --snapshot-at-tick 400 --snapshot-out ../artifacts_task/snap_trace --restore-at-tick 700 \
  --trace-window 2000 --trace-window-out ../artifacts_task/m9_trace2000.txt
```

The 2000-instruction trace from the restored tick-400 state passes, in
order, through:

```
play -> rest -> rest_callback -> tim_win32_rest -> Sleep(carrier wrapper)
     -> _handle_timer_tick (x3 blocks) -> sys_directx_lock_mutex/unlock_mutex
     -> voice_get_position -> _mixer_get_position
     -> play -> handle_player_input -> poll_control -> poll_joystick/nojoy_ret0
     -> is_left -> is_right -> is_fire -> jump_player -> play_jump_sound -> play_sound
```

i.e. exactly the `play() -> handle_player_input -> ...` path the milestone
asks to demonstrate, with names. Carrier-side code (the `Sleep` wrapper)
shows as `<unknown>(near ...)`, which - as the "Diagnostics" section above
already notes for the crash handler - is itself the tell that the trace left
the guest. **The trace window does not perturb the replay**: the same run's
digest stream is still `EQUAL (301 rows)` / `EQUAL (602 rows)` under
`certify_snapshot.py`.

### J. Measurements

| quantity | value |
|---|---|
| snapshot size at tick 400 | 90 108 112 B total; 89 781 680 B (99.6 %) of that is the heap arena |
| non-arena snapshot size | 326 432 B (.bss 223 608, .data 95 988, stack 4 304, CONTEXT 716, carrier 1 816) |
| capture time | 1.55 - 1.61 s (3 runs) - essentially all of it writing the arena, ~58 MB/s |
| restore time | 1.53 - 1.59 s (read + sha256-verify + write back 90 MB) |
| trace window | 2000 instructions, no measurable run-time cost at this size |
| G1 run wall clock, no snapshot | 2.70 / 2.72 s |
| same run with one capture + one rewind | 6.54 s (i.e. +3.8 s ~= one capture + one restore) |
| certification | 301 rows in-process, 602 rows against an independent cold run, 301 fn-sensor invocations - all EQUAL |
| RNG unit check | 5000 values across 5 seeds, 0 mismatches vs the real msvcrt.dll `rand()` |

The arena dominates everything. It is 90 MB at tick 400 only because the
deterministic arena is **leak-only** (a pre-existing gap this file already
records under "Milestones 5-7"): with real reuse the live set would be a
few MB and both times would drop by two orders of magnitude. That is the
single highest-value follow-up for snapshot cost.

### K. Known gaps / open problems (milestones 8-9)

- **The cross-process restore (`--restore-from` in a fresh process) does not
  work** - part G. The memory/CPU restore is exact; host-object identities
  (DirectDraw surface memory, DirectInput COM pointers) are not re-bound, and
  cannot be by an exclusion list because the stale pointers also sit in arena
  blocks the restoring process has not allocated yet. Needs either the
  logical-handle/COM-proxy layer (win32_pilot.md SS6 step 3) or headless mode
  (SS8 row 9a). The carrier warns loudly instead of failing silently.
- **Snapshot cost is the leak-only arena** - part J. 90 MB / 1.5 s per
  capture, ~326 KB / <5 ms without it.
- **Rewound critical sections are a real, unmitigated hazard** - part H.
  Suspending the other threads removes torn writes, not rewound lock
  counters. Not observed to bite in ~10 rewinds; a longer or `--pace=real`
  session is the way to provoke it.
- **One snapshot per run.** `--snapshot-at-tick` fires once; there is no way
  to capture both before and after a rewind in the same process, which is
  what a `pf_inspect diff` of a rewound state against its cold twin would
  want. Worked around here by comparing digest streams instead.
- **`--restore-fault` flips a fixed byte** (`reward_scale`, 0x4fac28) rather
  than an operator-chosen address; enough for the negative control the
  verdict contract requires, not a general fault injector.
- **The trace window is armed only by a restore.** A `--trace-window` given
  without any restore never opens. Tracing from a cold run at an arbitrary
  tick would be a small generalization (arm it at a `--trace-at-tick`), not
  needed for this milestone.
- **`pf_inspect.py`'s type engine is deliberately shallow**: it renders
  primitives, `fixed` (16.16), fixed-size arrays (first 8 elements), C
  strings, pointers and structs to depth 2, using `it_types_check.c`'s
  offsets. Function-pointer members, unions and the 4 DWARF-opaque types
  (`carrier/gen/INTEROP_NOTES.md`) print as raw hex.
- **The snapshot format is not the PortForge wire format.**
  `portforge-win32-carrier-snapshot-v1` is a directory of raw components
  with a JSON manifest, not `pf-dos-rm-snapshot-v5`'s 4 KiB-page-with-sha256
  shape (notes/portforge_capsule.md SS D). Page-level hashing is what would
  make incremental snapshots cheap; deliberately deferred until the arena's
  size problem is fixed, since paging a 90 MB leak is the wrong optimisation.

## Divergences 004 and 005, and the exit-time key storm

Follow-up pass (2026-09-07) on the three queued carrier problems in
notes/living_record.md: **004** (the bump-only heap arena exhausts itself in
the main menu), **005** (a human recording replays one tick early), and the
exit-time "release every DIK code" log storm. All three gates re-verified with
the final binary, assets restored from `artifacts/assets_backup/` +
`artifacts/log_original_baseline.txt` before every run, as everywhere in this
file (`carrier/scripts/restore_assets.ps1`, `carrier/scripts/gates.ps1`):

```
G1  compare_digests.py    -> EQUAL (876 ticks)
G2  compare_fn_digests.py -> EQUAL (877 invocations, [src] vs [original])
G3  certify_snapshot.py rewind -> EQUAL (301 rows T=400..699) and
                                  EQUAL (602 rows T=400..1000, cold vs post-rewind)
    certify_snapshot.py fn     -> EQUAL (301 invocations, k=276..576)
```

### A. Divergence 004: a real allocator inside the same fixed-address arena

**Problem, restated with the measurement.** `--det` redirected
`malloc`/`calloc`/`realloc`/`free` to a 256 MiB bump allocator at 0x20000000
that never reclaimed anything (Milestones 5-7). The main MENU creates and
destroys a full-screen ~800 KB bitmap every frame, ~40 MB/s, so a human
sitting at the menu exhausted the arena in ~450 ticks and crashed in
`main_menu_callback` on the first failed allocation (`replays/second_human`).
Reproduced on the first run of this pass: `det: arena exhausted (requested
819200, used 267969472/268435456)`.

**What replaced it** (`det.cpp`, same fixed region, same base, same size):
an explicit **doubly-linked free list, first fit from the head, LIFO
insertion, immediate boundary-tag coalescing of both neighbours**, plus a
bump `top` for bytes never handed out before. Chosen over size-class free
lists because it is strictly smaller (one list, one search, one merge rule)
and because every step is a pure function of the call sequence - no
addresses, no timestamps, no size-class hashing, no per-run policy - which is
what "provably deterministic given a deterministic call sequence" needs.

Layout; all block offsets and sizes are multiples of 16:

| range | contents |
|---|---|
| `[0, 64)` | `ArenaCtl`: `top`, `free_head`, `hwm`, live/peak stats, call counters |
| `[64, top)` | blocks: `ArenaHdr`(16) + payload + `ArenaFtr`(8); a free block keeps its list links in its own payload |
| `[top, 256 MiB)` | never touched |

**All allocator state is therefore inside the arena region**, which is what
makes the snapshot work unchanged: `snapshot.cpp`'s existing "arena"
component (`0x20000000`, `DetSavedState::arena_offset` bytes) now captures
the control block, every header/footer and the whole free list as ordinary
bytes. `det_state_save` reads `arena_offset` out of the control block instead
of a carrier global; `det_state_load` does **nothing** for the arena, because
the memcpy of the component has already restored the allocator exactly -
including which blocks were free and in what free-list order, which is what
makes the post-rewind allocation sequence reproduce the pre-rewind addresses.
No new snapshot component and no new carrier global were added. G3 verifies
this end to end.

Semantics kept exactly (`det_wrap_*`): `free(NULL)` is a no-op; a pointer the
arena did not hand out is leaked, not passed to the real heap, exactly as the
bump allocator did (counted as `n_free_foreign`, measured: **1** per run - an
msvcrt-internal block); `realloc(NULL,n) == malloc(n)`; growth copies
`min(old,new)` and frees the old block; a shrink that fits stays in place; a
failed `realloc` leaves the original block valid. Payload alignment is **16**
(msvcrt's x86 `malloc` guarantees 8; 16 is a strict superset and keeps the
block arithmetic trivial). **`calloc` now memsets explicitly** - the bump form
could rely on fresh `VirtualAlloc` pages being zero, a recycled block cannot;
that one line is load-bearing.

**Proof 2 - the menu no longer exhausts it, and the high-water mark is
bounded.** `carrier/scripts/menu_idle.txt` (new) never presses ENTER, so
`play()` is never entered and every tick is a menu tick; UP/DOWN toggles keep
the menu doing real work.

```
carrier.exe --det --pace=fast --input=script --input-script scripts/menu_idle.txt --run-seconds 90

det: shutdown at T=101598 (virtual_ms=2031972, 1015986 main-thread Sleep calls, 0 safepoints)
det: arena high-water 28564816 bytes (27.24 MB), live 23988976 bytes in 2463 blocks,
     calls malloc=3871328 calloc=3238 realloc=1426 free=3872128 (foreign 1, bad 0)
```

**101 598 carrier ticks in the main menu** (the bump arena died at ~450),
3.87 million allocations matched by 3.87 million frees, and the arena
**high-water mark stayed at 28 564 816 bytes (27.24 MB)** - flat, not
growing. The same run under the old allocator would have needed ~3.1 GB.

**Proof 3 - snapshot size.** Same `--snapshot-at-tick 400`, same
`scripts/newgame.txt` workload:

| | arena component | whole snapshot directory |
|---|---:|---:|
| before (bump-only) | 89 781 680 B | 90 110 309 B |
| after (free list) | **28 468 560 B** | **28 797 189 B** |

68.3 % smaller, and now bounded by the peak LIVE footprint instead of by
total allocation volume. Gameplay high-water for the G1 workload:
29 697 392 B (28.32 MB), peak live 27 222 320 B in 2 712 blocks. `--report`
gained an `"arena"` object carrying all of these.

**Proof 4 - `replays/third_human.txt` no longer crashes.**
`python scripts/play.py --play-replay third_human` runs to the end:
`det: shutdown at T=1983 ... 810 safepoints`, clean `--stop-at-tick` exit, no
access violation (it used to die on arena exhaustion). Its digest still starts
at T=248 against the recorded 249 - expected; see part B's "old recordings are
invalid".

### B. Divergence 005: the tick index was never the whole coordinate

**The instrument first (`--trace-input PATH`, `-` = stderr).** New diagnostic,
working in BOTH modes and with either input provider, deliberately, so a
record run and a replay run are directly diffable. Every captured / delivered
/ stamped key event logs:

```
ms=<virtual clock> T=<carrier tick, ms/20> sub=<Sleep call within T>
sleepn=<total Sleep calls> cyc=<guest cycle_count @0x506938>
pre=<cycle_count before this Sleep's _handle_timer_tick> post=<after>
sp=<play() safepoints so far> tid=<thread> site=<exact call site>
```

plus one line at every guest tick boundary (every `_handle_timer_tick` call
that moved `cycle_count`).

**What it showed, immediately.** The guest's idle loops call `rest(1)`, so
`det_wrap_Sleep` runs **~10 times per carrier tick** (measured: 4000 Sleep
calls for 400 ticks, `Sleep(2)` each). `T = virtual_ms/20` therefore only
advances once every ten Sleep calls, and the guest's own 20 ms tick
(`cycle_count` at 0x506938) is incremented at **sub=0** of each carrier tick,
inside `_handle_timer_tick`, i.e. before the input handover in that same
Sleep call. The two providers were not using the same handover point:

| provider | where it handed the event over |
|---|---|
| `--input=script` (`deliver_due_input`) | can only ever fire at **sub=0** - the first Sleep call whose T reaches the event's tick |
| `--input=real` (`drain_real_key_queue`, before this pass) | ran on **every** Sleep call, so a real key captured at an arbitrary instant was delivered at whatever sub-tick position 0..9 the human happened to hit |

Both stamped and scheduled on `T` alone, so the recording looked
self-consistent - but `T` does not determine the handover point, and the
handover point is what decides which **game** tick sees the key: `play()`
consumes `cycle_count` as soon as the sub=0 Sleep call returns, so an event
handed over at sub=0 is seen by game tick T, and the same event handed over at
sub>=1 is only seen by game tick T+1. A human's key, captured 9 times out of
10 at sub>=1, was therefore stamped T and acted on at T+1 during recording,
and acted on at T during replay: gameplay starts one tick early. That is
exactly the measured 249 vs 248.

**Negative control (measured, not argued).** `DET_INPUT_DELIVER_SUB=k`
(diagnostic env var, same opt-in style as the pre-existing
`DET_DUMP_MEM_TICK`) holds every scripted event back to Sleep call `k` of its
tick:

```
DET_INPUT_DELIVER_SUB=0 -> first digest 01510fe913396b61...  (identical to the G1 baseline)
DET_INPUT_DELIVER_SUB=1 -> first digest 8d016be773357ec1...
DET_INPUT_DELIVER_SUB=5 -> first digest 8d016be773357ec1...
DET_INPUT_DELIVER_SUB=9 -> first digest 8d016be773357ec1...
```

k=0 reproduces the baseline byte for byte; k=1/5/9 all differ from it **at the
very first digest line (T=126)** and agree with each other there. The sub-tick
position of the handover, not the tick index, is the coordinate that was
missing. This also explains the two experiments living_record.md 005 already
had: shifting *every* recorded event +1 tick matched 51 ticks and then broke
at T=300 (it corrects the 9-in-10 events captured at sub>=1 and breaks the
1-in-10 captured at sub=0), and pace-independence of a scripted run is
expected, since both paces deliver at sub=0.

**Fix 1 - one handover point for both providers.** `drain_real_key_queue` now
returns immediately unless this is the FIRST Sleep call of a new carrier tick
(`g_last_drain_tick`, part of `DetSavedState`), i.e. exactly the point
`deliver_due_input` uses. Stamp and delivery are then the same event at the
same place in both modes, and the tick index fully determines the handover in
both directions. Cost: at most one extra carrier tick (20 ms) of latency for a
human. Verified with `--trace-input` on a genuine SendInput session:
**capture** sub positions are spread uniformly over 0..9
(50/47/52/52/51/47/39/48/50/60 - the async race is real and still there),
**delivery** is 496/496 at sub=0.

**Fix 2 - a recording is what the provider handed over, not every
`_handle_key_press` the guest makes.** `--trace-input` on the first fixed
SendInput session showed stamps at sub>0 with no matching capture or delivery:
**the guest itself calls `_handle_key_press`** - Allegro's own key repeat,
driven from `_handle_timer_tick`, observed 240 ms after the original press
(Allegro 4's 250 ms default repeat delay). Those were being recorded, and
replaying them as ordinary scripted events put them at sub=0, a different
position from where the guest generated them. Measured effect: a session
diverged at its very first safepoint. `keypress_record_hit` /
`keyrelease_record_hit` now record only calls made from inside the carrier's
own handover (`g_in_delivery`). The guest's repeats are a *consequence* of
`key[]` plus Allegro's timer, both of which a replay reproduces on its own, so
dropping them loses no information - measured: the same session's recording
went from 365 to 172 events and became replay-EQUAL. `--inject-real-test` is
unaffected by construction (it calls `key_dinput_handle_scancode` from inside
`deliver_due_input`, so the real function's own behaviour stays inside the
flag) - re-verified: `EQUAL (876 ticks)` for the synthetic round trip, with a
24-event recording.

**OLD RECORDINGS ARE INVALID.** The stamping rule changed meaning: under the
old rule an event stamped `T` was acted on by the game at game tick `T+1`
whenever it happened to be captured at sub>=1, and at `T` when captured at
sub=0. No uniform shift repairs that, because the per-event sub-position was
never recorded. `replays/first_human.txt`, `replays/second_human.txt` and
`replays/third_human.txt` therefore cannot replay bit-exactly and should be
treated as historical artefacts, not regression fixtures. Recordings made
after this pass are replayable by construction.

**Proof - a genuine real-keyboard round trip.**
`carrier/scripts/sendinput_session.py` (new). A helper *thread* cannot post
into DirectInput, so the session uses **SendInput**, which injects at the
Win32 input-stack level: the events reach the focused guest window's
DirectInput keyboard exactly like a physical key press, on the guest's own
window thread, at whatever real instant they are sent. **MEASURED: SendInput
does reach this build's DirectInput path** - `key_dinput_handle_scancode`
fires for every injected key (496 captures in one 20 s session), so the
fallback the task allowed for was not needed. The script launches
`--det --pace=real --input=real --record-input R --digest-out D`, waits for
the guest's `AllegroWindow`, forces it to the foreground (AttachThreadInput,
since `SetForegroundWindow` from a background process is restricted), holds
ENTER to start a game, then presses/releases LEFT/RIGHT/SPACE at randomized
real times (`random.uniform(0.012, 0.19)` s) for ~20-30 s, and refuses to send
anything while the foreground window is not the guest's.

```
python carrier/scripts/sendinput_session.py --tag rt1 --seconds 20 --run-seconds 50 --seed 101
python carrier/scripts/sendinput_session.py --tag rt2 --seconds 24 --run-seconds 55 --seed 909090
```

Each then replays the recording with `--det --pace=fast --input=script`,
bounded by the record digest's last tick, and compares:

```
rt1: recorded 403 events, 1442 digest ticks (T=351..1801) -> EQUAL (1442 ticks)
rt2: recorded 524 events, 1914 digest ticks (T=194..2106) -> EQUAL (1914 ticks)
```

Two further sessions with the final binary were also EQUAL (1421 ticks / 172
events; 801 ticks), and one session run before fix 2 was EQUAL over 1549
ticks. In every EQUAL run the record and replay `--report` JSONs agree exactly
on the arena (`top`, `high_water`, `live_bytes`, `peak_live_bytes`,
`live_blocks`) and on the pinned RNG (`state`, `calls`).

**What did NOT come out equal, and what that is (honest).** Four of the eight
post-fix sessions diverged. They are not an input-coordinate failure:

- replaying the same recording twice is EQUAL to itself (`si2_rp1` vs
  `si2_rp2`: `EQUAL (1311 ticks)`), so the replay side is deterministic;
- in the one diverging session that had the new instrumentation (`rt3`,
  divergence at T=2468), the record and replay runs agree **exactly** on the
  pinned RNG state and call count (1623840384 / 50) and on every arena
  statistic - so neither the allocator nor a different RNG code path is
  involved;
- every diverging session had the desktop stealing the foreground from the
  guest repeatedly (rt3: **20** times, as Chrome and Explorer windows opened
  during the run; the script logs each one). A foreign window taking the
  foreground makes the guest's window thread run Allegro's DirectDraw
  switch-out/switch-in handling, and moves the real mouse over the guest
  window - two live channels that `--input=real` does **not** park and that a
  key recording does not capture.

So the residue correlates with an uncontrolled *desktop* channel, not with the
key coordinate; it is characterised, not root-caused. See "Known gaps" below.

### C. The exit-time key storm, collapsed

At process exit Allegro's keyboard shutdown drives
`key_dinput_handle_scancode` once for **every** DIK code, all inside a single
carrier tick. With `--input=real` that produced hundreds of individual "no
Allegro mapping ... dropped" and "capture queue full" lines. The handling is
unchanged (unmapped events dropped, over-capacity events dropped, and the
recording-hygiene rule in `keyrelease_record_hit` untouched); only the LOGGING
is aggregated: counts accumulate per tick and are emitted as ONE summary line
when the tick advances (from `drain_real_key_queue`, main thread) or at
`det_shutdown`. A single isolated event still prints its own detailed line, so
the diagnostic is not lost for the non-storm case.

Reproduced deliberately, with a real ESC hold through SendInput so the guest
reaches its OWN clean shutdown (`assets/log.txt` ends `Exiting Allegro` /
`Done...`, stderr shows `carrier_shutdown: guest _cexit`):

```
python carrier/scripts/sendinput_session.py --tag storm2 --quit-via-menu --run-seconds 90

det: T=254 real-key event storm collapsed: 510 dropped (417 unmapped, 93 queue-full),
     255 delivered, 255 distinct DIK codes 0x00..0xff - this is Allegro's
     keyboard-shutdown release sweep, not gameplay input
```

**One line instead of 510**, and zero remaining raw "no Allegro mapping" /
"queue full" lines in the whole run's stderr. The recording-hygiene rule held:
the file contains only the three genuine ESC events and none of the 255
shutdown releases (part B's `g_in_delivery` gate drops the whole sweep on its
own, because the sweep happens outside any carrier handover; the "currently
held" filter is kept unchanged as the second line of defence).

### New/changed options and files (this pass)

| what | where |
|---|---|
| `--trace-input PATH` (`-` = stderr) | `main.cpp`, `det.hpp`, `det.cpp` |
| `DET_INPUT_DELIVER_SUB=k` diagnostic env var | `det.cpp`, `deliver_sub_slot` |
| `"arena"` and `"rng"` objects in `--report` | `trace.cpp`, `det_arena_stats` |
| `scripts/menu_idle.txt` | 3600 ticks of pure menu, no ENTER |
| `scripts/sendinput_session.py` | genuine real-keyboard record/replay round trip |
| `scripts/restore_assets.ps1`, `scripts/gates.ps1` | the asset restore + G1/G2/G3 runners this file's convention describes |
| `DetSavedState.last_drain_tick` | new field (carrier.bin grows by 4 bytes; snapshot directories written by older builds are not readable by this one) |

### Known gaps / open problems (this pass)

- **A genuine-keyboard session is only EQUAL when the desktop leaves the guest
  alone.** Four of eight sessions diverged, always ones where the foreground
  was taken from the guest repeatedly. Not root-caused; the measurements above
  rule out the input coordinate, the allocator and the RNG. The two concrete
  suspects are Allegro's `WM_ACTIVATEAPP` switch-out/switch-in path and the
  **real mouse**, which `--input=real` never parks (only the keyboard is).
  Parking the DirectInput mouse the same way the keyboard is parked, and
  recording/replaying activation events, is the obvious next step.
- **Old recordings are invalid** (part B) - `replays/*.txt` from before this
  pass cannot replay bit-exactly, by design of the fix, not by defect.
- **`--trace-input` writes one line per guest tick boundary** as well as per
  key event, so a long run's trace is large (~440 KB for 1900 ticks). Fine as
  a diagnostic, not something to leave on.
- **The arena never returns pages to the OS.** `top` shrinks when the tail
  block is freed, but the 256 MiB reservation stays committed for the whole
  run. Irrelevant for determinism; relevant if a future pass wants the
  carrier's RSS to follow the game's.
- **First fit is O(free-list length).** Measured fine here (3.87 M allocations
  in the menu-idle run with no observable slowdown), because coalescing keeps
  the free list short. A workload that fragments badly would want size
  classes; the interface would not change.

### One harness trap worth recording: the asset-restore race

A gate run of this pass reported a single G3 "cold vs post-rewind" difference
at T=400 that did not reproduce. Cause: `carrier.exe` relaunches itself as a
child (fix #3 above), so a just-finished run can still be tearing down - and
still rewriting `assets/profiles/` - when the next run's restore starts, which
leaves a slightly different profile on disk and therefore a different game
state at `play()` entry. G1 was byte-stable across the same pair of runs, and
the identical G3 command re-run on its own was EQUAL, which is what identified
it as a harness race rather than a carrier defect.
`carrier/scripts/restore_assets.ps1` now waits for the whole `carrier`
process tree to be gone and retries the copy before returning. Anything that
scripts these runs back to back needs the same wait.


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

## Milestone 12 at scale

Scaling proof (2026-09-07): every one of the **35** clean functions in
`src/icytower/` (`src/icytower/PROMOTIONS.md`, batches 1-4) is now bound
into the running carrier and compared against the original machine code
over the operator's own recording (`replays/human_test.txt`, 2293 gameplay
ticks, 100 floors, score 2386; baseline per-tick digest
`replays/human_test.digest`). Milestones 11-12 above proved the mechanism
for 3 functions; this pass is that mechanism applied to all 35, plus the
infrastructure it needed that didn't exist yet: a 35-entry binding table
(was 3, `kMaxFns=8`), a stub that can re-push up to 10 argument dwords (was
hardcoded to 4), a build that compiles all 35 `src/` functions into the
carrier (was 2), and a mixed MSVC/GCC toolchain link for the 5 functions
with real x87 floating point. `carrier/scripts/bind_all.py` (new) automates
the per-function A/B loop. **Ran under this task's serialized-access rule:
no other process touched `carrier.exe` while any run below was in flight.**

### A. Gate

```
carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt --stop-at-tick 2528 --digest-out ../artifacts_ms12/gate_unbound.txt
python carrier/scripts/compare_digests.py artifacts_ms12/gate_unbound.txt replays/human_test.digest
```

Result: **`EQUAL (2293 ticks, ...)`** - the unbound carrier reproduces the
operator's own recording bit-exactly before anything else in this pass
touches it.

### B. Binding-table capacity, widened (real bugs this pass's scale found)

Two limits from Milestones 11-12 turned out to be too narrow for the full
35, both found by literally trying to bind every function and having the
stub corrupt a call rather than by inspection first:

- **`kMaxFns` (8 stub slots) -> 35.** Mechanical: `BIND_STUB(8)` through
  `BIND_STUB(34)` added, `kStubs[]` extended. No behavior change to the
  existing 3.
- **`kMaxArgs` (4 argument dwords, hardcoded into the stub's re-push) -> 10.**
  MEASURED gap: `set_control` takes 6 cdecl dwords
  (`Tcontrol*,int,int,int,int,int`) and `getFloorData` takes 5
  (`Tmap*,int,int*,int*,int*`); `line_intersect` takes 10. The
  milestone-11 stub's section (b) re-pushed exactly 4 dwords "regardless of
  the real arity" (that comment's own words) - correct for <=4 args, but a
  **silent argument-count bug** for anything wider: the 5th/6th argument
  would never reach the callee, reading whatever garbage happened to be on
  the stack past the 4 re-pushed dwords instead. Fixed by widening the
  re-push to `kMaxArgs`(10) repeats of the same self-correcting
  `push dword ptr [esp+48]` idiom (each `push` shifts `esp`, so the same
  literal offset walks one more dword back through the caller's frame every
  repeat - carrier/src/bind.cpp's own comment at the stub works out why no
  offset arithmetic needed to change, only the repeat count and the
  post-call `add esp, 40`) and bumping the post-call cleanup from 16 to 40.
  `sense_entry` (ORIGINAL-form capture) needed no change - it already
  looped `for i < kMaxArgs`, so raising the constant was enough. This is
  exactly the class of bug the task's own DR-budget contract exists to let
  the harness catch per-function rather than trusting the mechanism by
  inspection; found before it produced a false EQUAL only because
  `set_control`/`getFloorData` happened to be in this batch.

### C. Mixed-toolchain x87 build (win32_pilot.md SS6a's rule, applied inside the carrier)

`jump_player`, `line_intersect`, `new_rand`, `update_particle`,
`create_particle` (the task's own "particles" = the latter two) have real
x87 floating point. The carrier is MSVC-built; MSVC's `cl.exe` on this
32-bit target has no `/arch` override and compiles `double` through
plain-double/SSE codegen, which measurably DIFFERs from the original's
genuine 80-bit x87 intermediates for `line_intersect`/`new_rand`/the two
particle functions (PROMOTIONS.md's own MSVC-vs-GCC numbers: e.g.
`new_rand` MSVC DIFFER 6079/20000, GCC `-mfpmath=387 -mno-sse2` EQUAL
80000/80000). Handled properly rather than accepted, per the task:

1. **Compiler**: 32-bit MinGW GCC 16.2.0 already on this host
   (`C:\msys64\mingw32\bin\gcc.exe`) - the same compiler
   `carrier/lift/harness/GCC_X87.md`'s offline proof already used, now
   pointed at the carrier build instead of a standalone harness exe.
2. **Flags**: `-m32 -mfpmath=387 -mno-sse2 -O2 -fno-asynchronous-unwind-tables`
   - the first three reproduce win32_pilot.md SS6a's rule (genuine x87
   codegen, no SSE); `-fno-asynchronous-unwind-tables` drops GCC's
   `.eh_frame`/CFI sections, which MSVC's linker does not consume and does
   not need (no C++ exceptions cross these functions).
3. **Headers**: the SAME `carrier/gen/pf_bindings_src.h` MSVC's own `src/`
   compile step force-includes (`-include pf_bindings_src.h`) - it is
   plain `#define`/`typedef` (BINDINGS_NOTES.md), no MSVC-only syntax, so
   GCC accepts it unchanged. **Verified this pass**: `gcc.exe -m32
   -mfpmath=387 -mno-sse2 -O2 -fno-asynchronous-unwind-tables -Wall -I gen
   -include pf_bindings_src.h -c src/icytower/new_rand.c` compiles with 0
   errors (1 harmless pre-existing `/*` -inside-a-comment warning in
   `line_intersect.c`, unrelated to this pass).
4. **Object format check**: `dumpbin /headers` on a GCC-produced `.o`
   reports `14C machine (x86)`, 32-bit COFF - the same object format MSVC's
   objects use.
5. **Symbol decoration**: `nm` on the GCC objects shows `T _new_rand`,
   `T _jump_player`, `T _line_intersect`, `T _create_particle`/
   `T _update_particle`/`T _reset_particles` (all of `particle.c` compiled
   as one GCC TU, since `reset_particles` has no FP of its own and there is
   no reason to split the file) - a single leading underscore, exactly
   MSVC's own `__cdecl` decoration for a C-linkage name on this target.
   **No `_name@N` stdcall-style suffix on either side** (both are cdecl).
6. **Link**: a standalone test first (`cl /c` an MSVC translation unit
   declaring `extern int __cdecl new_rand(void);` etc., then
   `link ... linktest.obj new_rand.o jump_player.o line_intersect.o
   particle.o kernel32.lib`) produced a working `.exe` with **no
   `lib.exe`/wrapping step** - MSVC's `link.exe` accepts a GCC-produced COFF
   `.o` exactly like its own `.obj`. Only then wired into `build.cmd`'s real
   link line (`obj_gcc\jump_player.o obj_gcc\line_intersect.o
   obj_gcc\new_rand.o obj_gcc\particle.o`, alongside the existing MSVC
   `.obj` files). `cl`'s own compile step for the mixed link prints
   `Command line warning D9024 : unrecognized source file type
   '...jump_player.o', object file assumed` for each - informational, not
   an error; `cl` treats an unrecognized extension exactly as a linker
   input, correctly.
7. **CRT**: neither needed - every global these 4 files touch resolves to a
   fixed guest address via `pf_bindings_src.h` macros (no `malloc`, no libc
   calls), so no extra runtime library is linked for the GCC objects.
8. **`build.cmd` cmd.exe trap, MEASURED and fixed**: the first wiring
   attempt restored `%PATH%` from inside a parenthesized
   `if errorlevel 1 ( ... )` block after each GCC invocation; this host's
   `%PATH%` contains `...Program Files (x86)...` (an unescaped `)`), which
   corrupts cmd.exe's block parser the moment that value is substituted
   inside `(...)` - failed with the exact, textbook symptom `\Microsoft was
   unexpected at this time.`. Fixed by never referencing a PATH-bearing
   variable inside a parenthesized block: `gcc.exe`'s own directory is
   prepended to `%PATH%` once (verified no collision - `mingw32\bin` ships
   no `link.exe`/`cl.exe`) and left there for the rest of the script,
   with each `if errorlevel 1 ...` written as two plain, unparenthesized
   statements. **Separately MEASURED**: `gcc.exe` invoked by its full path
   with `%PATH%` unmodified fails silently (exit 1, zero stderr, no object
   produced) under the `vcvars32.bat` environment - it needs its own bin
   directory ON `%PATH%` (presumably to resolve a sibling DLL/`cc1.exe`
   dependency this MSYS2 build does not resolve purely by co-location).
   Both traps are documented in `build.cmd`'s own comments at the point
   they were fixed.

Net: `jump_player.c`, `line_intersect.c`, `new_rand.c`, `particle.c` are
compiled by GCC with genuine x87 codegen and linked straight into
`carrier.exe`; every other `src/icytower/*.c` file compiles through the
existing MSVC step, unchanged in kind from Milestones 11-12.

### D. `carrier/scripts/bind_all.py` and the per-function results

New script, run from `carrier/`: for each of the 35 functions, restores
assets, runs `--bind <fn>=original --fn-digest-out A` (the one
DR-sensed ORIGINAL-form function that run's DR budget allows), restores
assets again, runs `--bind <fn>=src --fn-digest-out B --digest-out
B_ticks`, then `compare_fn_digests.py A B` and `compare_digests.py B_ticks`
against the baseline. A function with 0 records in both A and B is reported
as **UNVERIFIED IN VIVO** rather than compared (`compare_fn_digests.py`
itself refuses to call empty input EQUAL). Full per-function output:
`artifacts_ms12/bind_all_summary.json`; per-function raw records:
`artifacts_ms12/<fn>_{A_original,B_src,B_ticks}.txt`.

```
python carrier/scripts/bind_all.py
```

**27 of 35 EQUAL**, all with matching per-tick global digests:

| function | invocations | function | invocations |
|---|---:|---|---:|
| update_frame | 2294 | init_control | 2 |
| jump_player | 517 | get_level | 703 |
| getFloorData | 3561 | reset_particles | 1 |
| reset_map | 1 | scroll_scroller | 25 |
| add_combo | 3 | cycle_counter | 2529 |
| line_intersect | 2418 | fps_counter | 50 |
| get_gamepad | 1 | get_demo | 1947 |
| is_up | 38 | get_controls | 1 |
| is_down | 38 | switchedToProgram | 1 |
| is_left | 2331 | new_rand | 138255 |
| is_right | 1480 | update_particle | 113730 |
| is_fire | 2329 | create_particle | 446 |
| is_pause | 2293 | | |
| is_enter | 19 | | |
| is_any | 48 | | |

The 5 x87 functions (`jump_player`, `line_intersect`, `new_rand`,
`update_particle`, `create_particle`) are in this EQUAL list **bit-exact,
sha256, no epsilon** - the GCC x87 build (part C) closes the gap
PROMOTIONS.md's own MSVC numbers document; `update_particle`/
`create_particle`'s domain additionally covers the `seed` global (8 bytes
@ 0x4ff108) since both mutate it as a side effect through up to 3
`new_rand()` calls each, and that stayed EQUAL too across 113730 and 446
real invocations respectively.

**1 of 35 DIFFER: `add_jump_sequence`** (real, reproducible, not a harness
bug):

```
FIRST DIFFERENCE fn=add_jump_sequence k=0 T=330 field=post
  original: post=df3f6198... (== pre - ORIGINAL wrote nothing this call)
  src:      post=4cbbd8ca... (!= pre - SRC wrote a new jump-sequence entry)
```

Identical arguments, identical pre-state, different result -
`compare_fn_digests.py`'s own rule: this convicts the function, not
upstream divergence. Root cause, confirmed against `artifacts/disasm.txt`
0x4040f4-0x404146: the ORIGINAL has a guard `src/icytower/
add_jump_sequence.c` is missing -

```
mov 0x8(%edx),%ecx     ; ecx = js->num  (Tgd_jump_sequence.num, offset 8)
test %ecx,%ecx
je 404148              ; if (js->num == 0) return;   <-- not in the recovered source
mov 0xeaa4(%eax),%ebx  ; (then) ebx = gd->jumpPosts
cmp $0x1387,%ebx       ; jumpPosts > 4999 guard (this one IS in the source)
```

`src/icytower/add_jump_sequence.c` checks only `jumpPosts > 4999`; the
original ALSO returns early whenever `js->num == 0`, checked first. At
T=330 (k=0) `js->num` was 0, so the ORIGINAL took the early return and
wrote nothing while the recovered SRC form (lacking that guard)
unconditionally appended. The per-tick global digest still came back EQUAL
over the whole 2293-tick recording for this run (`add_jump_sequence.c`'s
extra write is off the digest's own scope, or its effect was masked
downstream this run) - **a real recovery bug that the per-tick gate alone
would have missed**, exactly the class of defect per-function in-vivo
verification exists to catch. Not fixed here (`src/` is out of scope for
this pass, per the task); flagged as a background task
(`task_8dd77dd1`, "Fix missing guard in add_jump_sequence.c") with the
disassembly evidence above, and recorded as DIFFER in
`src/icytower/INVIVO.md`.

**7 of 35 UNVERIFIED IN VIVO** on `replays/human_test.txt` (0 invocations,
both forms): `is_solid`, `set_control`, `check_control_key`,
`restart_scroller`, `switchedFromProgram`, `clickedCloseButton`,
`ok_to_play`. Alternative workloads tried (task's own suggestion), all
cheap (existing scripts, no new recording needed):

- **`restart_scroller`**: `carrier/scripts/menu_idle.txt` (idles at the
  main menu 3600+ ticks) DOES reach it - **57 invocations**, bound
  `original` vs `src`. All 57 common records EQUAL; the ORIGINAL run
  produced one additional record after the last common one before its own
  `--run-seconds` wall-clock budget elapsed (the two sensing mechanisms -
  hardware breakpoints for ORIGINAL, the entry-patch stub for SRC - have
  different per-call CPU overhead, so a fixed real-time budget under
  `--pace=fast` does not tick the exact same number of times in each run;
  this is a harness timing artifact, not a functional difference in
  `restart_scroller` itself - every invocation both runs share is
  bit-exact). Verdict: **EQUAL (57 common invocations)**, workload
  `menu_idle.txt`.
- **`is_solid`**: still 0 invocations on `menu_idle.txt` too - matches
  Milestones 11-12's own finding (part E there) that `scripts/newgame.txt`
  doesn't reach it either. Its only static callers are
  `handle_player_collision_{original,old,combo}`
  (notes/promotion_candidates.md SS2); no available recording takes that
  branch. Genuinely unverified in vivo; the offline harness result stands
  (PROMOTIONS.md: EQUAL, 20000 vectors).
- **`set_control`, `check_control_key`, `ok_to_play`**: checked
  `artifacts/disasm.txt` for ANY reference to their VAs (0x4017d4,
  0x401808, 0x406a50) - direct call, indirect/data reference, anything.
  **Zero hits beyond the function's own disassembly listing, for all
  three.** These are not "the wrong workload" - they are **dead code in
  this compiled binary**: nothing in the 253-function game-scope call
  graph reaches them, directly or through a function pointer. (`PROMOTIONS.md`
  already flagged this for `ok_to_play`'s negative control; this pass
  confirms it's not reachable at all, not just hard to trigger.) No
  in-vivo workload can verify a function the binary itself never calls;
  the offline harness result is the only evidence that will ever exist for
  these three (all EQUAL, 20000 vectors, PROMOTIONS.md).
- **`switchedFromProgram`, `clickedCloseButton`**: DO have real callers -
  confirmed as Allegro callback registrations (`movl $0x406a5c,...` /
  `$0x406a7c,...` at 3 and 1 sites respectively, `artifacts/disasm.txt`),
  the same mechanism the gate run's own stderr already shows
  (`det: switch_in_cb = 00406A6C ... switch_out_cb= 00406A5C ...`) -
  `switchedToProgram` (the sibling callback) WAS invoked once in the gate
  run. These fire on real OS window events (focus lost / close-button
  click), which `carrier/NOTES.md` "Environment isolation" part B's
  window-activation controlled channel can reach in principle but no
  existing input SCRIPT can (a script only injects keyboard events) -
  not attempted this pass; flagged below.
- **`.itr` replay-menu playback**: not attempted. Driving the replay menu
  to play `profiles/MissingNO/replays/*.itr` needs the exact main-menu ->
  replay-browser key sequence, which is not yet known and would need
  trial-and-error menu navigation to discover - not cheap by this pass's
  own bar, unlike `menu_idle.txt` (already existed, worked on the first
  try). `carrier/scripts/play_itr.txt` was therefore **not created** this
  pass; left for whichever future pass needs `is_solid` or the `.itr`
  playback path specifically (`is_solid` is the one candidate here that
  actually has real, if hard-to-reach, callers).

### E. All 35 bound at once

```
carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt --stop-at-tick 2528 --run-seconds 180 --bind-file carrier/scripts/all35_src.bindfile --fn-digest-out ../artifacts_ms12/all35_fn.txt --digest-out ../artifacts_ms12/all35_ticks.txt --report ../artifacts_ms12/all35_report.json
python carrier/scripts/compare_digests.py artifacts_ms12/all35_ticks.txt replays/human_test.digest
```

(`--bind-file` needs an ABSOLUTE path - MEASURED: a path relative to
`carrier/`, e.g. `scripts\all35_src.bindfile`, fails with `bind: FATAL -
--bind-file '...' could not be opened`, because `bind_init()` opens it
AFTER the carrier's own startup `_chdir` into `assets\`
(carrier/NOTES.md fix #6), unlike the CLI's other path options which are
resolved before that chdir happens.)

Result: **`EQUAL (2293 ticks, ...)`** against the baseline - all 35 clean
functions running simultaneously, in place of the corresponding original
machine code, for the entire 2293-tick human recording, reproduce it
bit-exactly.

**win32_pilot.md SS8a metrics, from `--report`'s `"binding"` object:**

| metric | value |
|---|---:|
| functions bound (form=src) | 35 |
| functions still ORIGINAL (253 game-scope total - 35) | 218 |
| crossings ORIGINAL -> src | 137176 |
| invocations sensed | 137176 |
| domain read failures | 0 |
| faults injected | 0 |
| original `.text` bytes no longer executed (sum of the 35 function sizes, PROMOTIONS.md) | 2028 |
| crossings src -> ORIGINAL (native_to_original) | not instrumented - 0 by construction (see below) |

**In-vivo finding this run only, not visible from any single-function A/B
test**: with all 35 bound, `new_rand`'s own entry-patch crossing count
drops from 138255 (measured testing `new_rand` alone, part D) to **342**.
Root cause: `update_particle`/`create_particle`'s `src/` forms call
`new_rand()` as an ordinary C symbol (`pf_bindings_src.h` leaves a
promoted function's own name free rather than redirecting it,
BINDINGS_NOTES.md "Exclusion") - when `update_particle`/`create_particle`
are ALSO bound to `src`, that call resolves directly to the linked-in
`new_rand` function body at LINK time and **never touches new_rand's own
patched guest VA (0x406984) at all**, so the entry-patch sensor there
never sees it. The 342 that remain are whatever still-ORIGINAL game code
calls `new_rand` directly through its guest address. `113730 +
446*<=3 - 342` is NOT expected to reconcile the two numbers exactly (not
every particle call rerolls `color`, and `create_particle` draws 3 `new_rand()`s
only on the found-a-slot path), but the direction and the order of
magnitude match: nearly all of the 138255 calls counted when `new_rand`
was tested alone were actually `update_particle`/`create_particle`'s own
calls, invisible to the guest-VA sensor the moment those two callers are
also `src`-bound. `"crossings_native_to_original"` stays "not instrumented
... 0 by construction" (`bind_report_json`'s own comment, updated this
pass) - the src-to-src call above is the one case that could have looked
like a crossing and provably is not one (it never reaches the guest VA in
either direction).

### Known gaps / open problems (Milestone 12 at scale)

- **`add_jump_sequence` DIFFERs** - a genuine recovery bug (part D above),
  flagged as a background task, not fixed by this pass.
- **7 of 35 unverified in vivo** on any workload tried; 3 of those
  (`set_control`, `check_control_key`, `ok_to_play`) are dead code in this
  binary (0 references anywhere in `artifacts/disasm.txt` beyond their own
  body) and can never be verified in vivo by construction, not just by
  workload choice. `is_solid` has real but unreached callers; a play
  script that forces the `handle_player_collision_{original,old,combo}`
  branch is still needed (Milestones 11-12 already flagged this same gap).
  `switchedFromProgram`/`clickedCloseButton` need a real OS window event
  (focus loss / close-button click), reachable via the environment-
  isolation controlled channel but not via any keyboard input script; not
  attempted.
- **`carrier/scripts/play_itr.txt` was not created** - the replay-menu key
  sequence to drive `profiles/MissingNO/replays/*.itr` playback is not yet
  known; would need exploratory menu navigation first. Left for a future
  pass targeting `is_solid` specifically.
- **`restart_scroller`'s A/B comparison used a different workload
  (`menu_idle.txt`) than the other 34** (`replays/human_test.txt` never
  reaches it) - its 57-invocation EQUAL result is not cross-checked against
  the human recording's own per-tick digest the way the other 34 are,
  because `menu_idle.txt` and `human_test.txt` are different recordings
  with no shared baseline.
- **The x87 GCC objects are not covered by any purity/regression gate of
  their own** beyond compiling clean and linking clean - `scripts/
  check_native_layer.py` (src/README.md) only scans `src/icytower/*.c`
  source text, which is toolchain-agnostic and already passed before this
  pass touched anything; there is no automated check that re-verifies
  `-mfpmath=387 -mno-sse2` specifically stayed in `build.cmd`'s GCC
  invocations if someone edits that file later.
- **`kMaxArgs=10` is exactly the widest real arity among these 35
  (`line_intersect`)**, not a safety margin - a future 36th function with
  an 11th argument would need this raised again, the same way this pass
  had to raise it from 4.

## Headless, frame oracle, named globals, .itr workload

Follow-up pass (2026-09-07) on win32_pilot.md SS8 row 9a and its own
"Suggested next passes": true headless (no DirectDraw), a presentation-
independent frame oracle above `blit_to_screen`, a generic named-globals
printer, and a workload that drives the game's own replay browser. New
files: `carrier/src/headless.{hpp,cpp}`, `carrier/src/frame.{hpp,cpp}`,
`carrier/src/print_globals.{hpp,cpp}`, `carrier/gen/gen_print_globals.py`
(+ generated `carrier/gen/it_print_globals.inc`), `carrier/scripts/
play_itr.txt`. Small additions: `main.cpp` (option parsing/wiring/env
passthrough), `trace.cpp` (three new `--report` sections), `build.cmd`
(compiles the three new .cpp files), `scripts/play.py` (Icy Tower globals
list for `--play-replay`). **Ran under this task's serialized-access rule**
(restore before every launch, no concurrent `carrier.exe`), except one
`itr5` diagnostic run that was killed after running far past its
`--run-seconds` budget under heavy host load from back-to-back prior
runs - not treated as a `--run-seconds` regression (see item 4).

**Gates re-verified with the final binary** (assets restored before every
launch, per the existing convention):

```
G1 (scripts/newgame.txt)                        EQUAL (876 ticks)
G2 (compare_fn_digests.py, update_frame)         EQUAL (877 invocations)
human_test (replays/human_test.txt vs .digest)   EQUAL (2293 ticks)
all-35 bound (--bind-file all35_src.bindfile)    EQUAL (2293 ticks)
```

Unchanged from every prior pass - none of this pass's new mechanisms are
wired into the default automated code path (see item 1's own finding on
why `--headless` stayed opt-in rather than becoming the implied default).

### 1. True headless: mechanism works, full operation does not (on this host)

**Mechanism, as specified.** `set_gfx_mode(int card, int w, int h, int v_w,
int v_h)` (graphics.c, VA 0x450688) and `install_sound(int digi, int midi,
const char *cfg)` (sound.c, VA 0x4417b0) are Allegro-internal functions, not
imports - so they cannot be IAT-wrapped. Rather than a 5-byte entry-patch
trampoline (which would need to relocate and re-execute the patched bytes
just to let the ORIGINAL driver-init code still run with different
arguments), this pass reused the existing hardware-breakpoint table
(det.hpp's `det_register_breakpoint`, the same DR0-DR3 table the tick
safepoint and `_switch_in`/`_handle_mouse_input` share): the breakpoint
fires with `EIP == VA`, i.e. *before* the callee's own prologue runs, so
`CONTEXT->Esp` is exactly `[retaddr][arg0][arg1]...` as the caller's `call`
left it - the callback rewrites those guest-memory dwords in place and
returns; the existing RF-flag single-step-over then lets the *unmodified*
original instruction execute next, reading the new argument values as if
the caller had pushed them. No bytes patched, no trampoline, no relocated
prologue.

`headless.cpp`: `--headless` arms a breakpoint at `set_gfx_mode`'s entry
that rewrites `card -> GFX_GDI` (0x47444942, `carrier/gen/pf_lib_bindings.h`
line 420) and `w,h -> 640,480`, leaving `v_w`/`v_h` alone (GFX_GDI has no
virtual-screen/page-flip concept). `--no-sound` arms a second breakpoint at
`install_sound`'s entry that rewrites `digi,midi -> DIGI_NONE,MIDI_NONE`
(0,0 - Allegro's public `digi.h`/`midi.h` API, stable across the whole 4.x
series, not FOURCC-encoded like `GFX_*`, so not re-derived from
disassembly). Both breakpoints are persistent (not disarmed after the first
hit), since `set_gfx_mode` has up to 18 call sites in `init_game`/
`options.c`'s own resolution-testing retry loop (`artifacts/disasm.txt`
0x40db91-0x40fe3b, all pushing small literal `card` values 1,2,3,... - NOT
Allegro's public `GFX_*` FOURCC constants, confirmed by cross-checking
`install_sound`'s own observed original args, which WERE the real
`DIGI_AUTODETECT`/`MIDI_AUTODETECT` value -1 exactly as documented).

**MEASURED, load-bearing finding: `--headless` gets the guest through
`set_gfx_mode` successfully (assets/log.txt shows "Graphics mode set." and
the full startup sequence through "MAIN MENU LOOP", byte-identical in kind
to a non-headless run) but the run then stalls before a single safepoint,
reproduced every way tried:**

```
--window=hidden                              stuck at virtual T=36, 0 safepoints, 20+ real
                                              DirectInput key-violation storm, window rect
                                              (-32000,-32000) - the Windows "minimized" sentinel
--headless --window=minnoactive              same: 3 extra set_gfx_mode(-1,0,0,...) "reset" calls
                                              observed (Allegro's own internal cleanup pattern
                                              after a failed driver init), then guest _cexit
--headless --interactive                     window genuinely shown+focused (no violations,
                                              no reset calls this time), STILL stuck at T=36
--headless --interactive --window=normal     window on-screen at a real desktop position,
                                              still stuck at T=36 for a FULL 300 real seconds
                                              (watchdog fired at exactly --run-seconds, so this
                                              is a genuine stall, not merely slow rendering)
```

Every variant reaches the identical wall: virtual T=36 (the tick right after
`newgame.txt`'s scripted ENTER press at T=20 is delivered), then nothing -
`assets/log.txt` never advances past "MAIN MENU LOOP", and stderr shows a
repeating `SUPPRESSED window switch in`/`switch out` flap. This is a real,
reproducible interaction between the forced `GFX_GDI` mode and this host's
window/focus behavior (not a bug in the breakpoint mechanism itself -
`--no-sound` alone, using the IDENTICAL breakpoint technique, works
perfectly: see item 4 below), not root-caused further within this pass's
budget. Candidates for a follow-up: GDI's software blit path may need a
window that is never minimized/hidden even transiently (this host's
automated-run window policy minimizes by default - see "Environment
isolation"); or the GDI driver's own window-recreation-on-mode-set may be
racing with Allegro's own switch-callback machinery in a way cnc-ddraw's
DirectDraw path does not.

**Consequence for the task's own decision point** ("if it changes...
report... and decide"): since forcing GDI does not merely shift a digest
value here but prevents the run from ever reaching a digest at all,
`--headless` was **not** wired to be implied by the default hidden window -
doing so would have silently broken every existing automated run's timing
assumptions, not just moved a hash. `main.cpp`'s `resolve_headless()` was
implemented, tested, and reverted to explicit-opt-in-only; its own comment
records the measurement and the reasoning. `--headless` remains available,
correctly installed, and its argument-rewrite is proven correct by its own
stderr line and by `assets/log.txt`'s "Graphics mode set." - but is
EXPERIMENTAL/BROKEN for actually reaching gameplay on this host, and is
flagged as such rather than claimed working.

### 2. Frame oracle: presentation-independence proven where reachable; headless comparison blocked by item 1

`frame.cpp`: a breakpoint at `blit_to_screen`'s entry (VA 0x40b6bc, `void
blit_to_screen(BITMAP *)`) reads the one cdecl argument directly - confirmed
by disassembly to be the SAME bitmap at every one of its ~20 call sites, not
just the `play()` one (main.c's `swap_screen`, VA 0x4dd194, the game's own
off-screen back buffer - `draw_frame()` has just finished rendering into it;
`screen`, VA 0x4dda8c, is the driver-owned front buffer and is deliberately
NOT what this sensor reads, per win32_pilot.md sec 4a). BITMAP layout
(`carrier/gen/it_types.h`): `w`@0, `h`@4, `vtable`@28 (`GFX_VTABLE
*`, whose OWN first member is `color_depth`, matching Allegro's public
`bitmap_color_depth(bmp)` macro exactly), `line[]`@64 (one row pointer per
row). `bpp = (color_depth+7)/8` (Allegro's own `BYTES_PER_PIXEL` macro); the
digest itself never interprets pixel format - it `sha256`s `w*bpp` raw bytes
per row via each row's own `line[]` pointer, so it is correct for
8/15/16/24/32bpp without caring which. `--frame-digest-every N` (default 1)
throttles by call count; `T <sha256> w h bpp` is written per emitted line,
`T` from the SAME `det_tick()` the per-tick digest uses, so the two streams
line up. `--frame-dump-at T PATH` writes one binary PPM (P6) at the first
call observed at that tick, converting 8bpp (via a direct call to Allegro's
own `get_palette`, VA 0x44c47c - safe because the sensor runs synchronously
on the guest's own main thread) /15/16/24/32bpp to 24-bit RGB for a human to
look at.

**Proof, where reachable** (headless is blocked by item 1, so this is
`--window=hidden` vs `--window=hidden`, both cnc-ddraw - the achievable half
of the task's own comparison, and still exactly the claim win32_pilot.md
sec 4a makes: presentation is below the boundary):

```
frameA.txt vs frameB.txt (two independent runs, scripts/newgame.txt, T=20..700)
  683 digest lines each, BYTE-IDENTICAL FILES
--frame-dump-at 200 PATH -> 640x480, 4 bpp, 921615-byte PPM (15-byte header +
  640*480*3 pixel bytes exactly), real (non-degenerate) pixel data confirmed
  by inspection (a plausible dark menu-background RGB triple repeating, not
  all-zero/garbage)
```

The `--headless` (GDI) vs `--window=normal` (cnc-ddraw) comparison the task
also asked for could not be run: item 1's stall means no headless run ever
reaches a single `blit_to_screen` call past the menu's own idle-loop
rendering, so there is no frame stream to compare. This is the SAME
limitation as item 1, not a new one - once item 1's stall is root-caused,
this comparison is the natural next proof (the frame-oracle mechanism
itself needs no further work; only a working headless run to feed it).

### 3. Generic named-globals summary at shutdown: `--print-globals`

`carrier/gen/gen_print_globals.py` (new) parses the SAME three sources
`carrier/scripts/pf_inspect.py` already parses offline in Python
(`interop_index.json`'s globals list, `it_types.h`'s struct members,
`it_types_check.c`'s authoritative sizeof/offsetof) and emits
`carrier/gen/it_print_globals.inc`: every global and every struct member,
pre-resolved (typedefs chased down to a primitive kind/size or a known
struct name, pointer count and array dims split out) so the RUNTIME walker
(`carrier/src/print_globals.cpp`) never parses a type string - only table
lookups. `--print-globals expr1,expr2,...` accepts a plain global name
optionally followed by `[N]`/`[other_global_name]` indexing (the index can
be a literal or another live global's own value - read at evaluation time,
which is what makes `ply[player_id]` work generically) and `->field`/
`.field` member access (both auto-dereference, so either spelling works
regardless of whether the current value is a pointer or a direct struct).
No struct/global/field name is hard-coded in `print_globals.cpp` -
everything it knows comes from the generated table; Icy-Tower-specific
names are supplied on the command line as data. Printed as `global <expr> =
<value>` to stdout at shutdown (while the guest's memory is still mapped in
this same process, called from `carrier_shutdown()` before
`TerminateProcess`) and as a `"print_globals"` array in `--report`'s JSON.

**Why `score`/`floor`/`combo` aren't literal top-level DWARF globals, and
what the generic path names for them instead:** they are fields of the
per-player `Tplayer` struct (`src/icytower/game_types.h`), reached through
the two globals that DO exist standalone - `player_id` (which slot is
active) and `ply` (`Tplayer *ply[1000]`). DWARF's own field names are
`score`, `level` (the floor counter - the game's own source literally calls
it `level`, not `floor`) and `best_combo`/`latest_combo` (there is no
single field named plainly `combo`). Verified end to end:

```
global player_id = 462
global ply[player_id]->score = 0
global ply[player_id]->level = 0
global ply[player_id]->best_combo = 0
global ply[player_id]->latest_combo = <a stale/uninitialized-looking value at T<126>
```

(all zero/uninitialized-looking here because this smoke test stopped before
`play()`'s first safepoint - the mechanism, not the specific values, is what
this proves). `scripts/play.py`'s `--play-replay` now passes exactly this
five-expression list (`ICY_TOWER_GLOBALS`) via `--print-globals` and
`--report`, and prints them labeled right after the EQUAL/first-difference
verdict (`print_globals_summary`), reading the structured JSON rather than
scraping console text.

### 4. `--no-sound`: works, and does change the digest (as warned)

Same breakpoint mechanism as item 1's `set_gfx_mode` sensor, at
`install_sound`'s entry - and, unlike headless, this one works completely
cleanly: a full `--det --no-sound` run of `scripts/newgame.txt` reaches
**876 safepoints** (identical count to every other G1 run) with
`install_sound(digi=-1,midi=-1) -> (DIGI_NONE,MIDI_NONE)` observed exactly
once, confirming the game's own default install call really does request
`DIGI_AUTODETECT`/`MIDI_AUTODETECT` (-1,-1 - Allegro's real constants,
corroborating the values item 1 assumed for its own `set_gfx_mode`
override). As the task anticipated ("report whether the game's logic reads
the install result into digest-domain globals"): **yes** -
`compare_digests.py` against the G1 baseline reports `FIRST DIFFERENCE at
tick T=126` (the very first digest line), confirming Allegro voice ids
(`checkMusicVoiceID` et al., already flagged as digest-scope in
"Environment isolation" part D) do change under `--no-sound`. Kept **OFF by
default**, as instructed, so gates stay comparable; the option exists and
is proven functional for anyone who deliberately wants a device-less run
and accepts the absolute-hash shift.

### 5. `.itr` workload: navigation solved, final selection not yet - `carrier/scripts/play_itr.txt`

Milestone 12 at scale's own "Known gaps" flagged this as "not attempted...
would need exploratory menu navigation first." This pass did that
navigation, by disassembly rather than trial-and-error where possible:

- `main_menu` (VA 0x4bd380, `notes/asset_census.md`): "Play Game /
  Instructions / Profile / High Scores / Load Replay / Options / Exit",
  item 0 default-selected (same as `newgame.txt`). **MEASURED**: 4x
  `KEY_DOWN` only reaches item 3 ("High Scores" - confirmed by `"
  high scores selected"` in `assets/log.txt`); one tap is evidently
  absorbed somewhere. 5x `KEY_DOWN` reliably reaches item 4 ("Load
  Replay" - confirmed by `" load replay selected"` /
  `"   opening profiles/MissingNO/replays/"`, reproduced across every rerun
  of this pass).
- `__mangled_main`'s own dispatch (VA 0x4163f4-0x416403) calls
  `replay_selector(ctrl=0x5000c8)` DIRECTLY when "Load Replay" is confirmed
  - there is no separate file-browser menu screen to navigate through
  first. `run_demo()` (which `replay_selector`'s caller invokes with the
  loaded replay) calls `new_game()` then `play()` directly (VA 0x415e96) -
  i.e. the EXISTING safepoint/tick-sensor/digest infrastructure applies to
  `.itr` playback completely unchanged, no new plumbing needed there.
  `--print-globals num_itr_files` (VA 0x4dd744, `it_globals.h`) read back
  **3** on the `MissingNO` profile, confirming the file list is populated
  (not the reason selection fails).
- `replay_selector` (VA 0x41d258) does **not** confirm a selection via
  `poll_control()`/`is_any()` despite that being the more obvious read of
  its own polling loop (0x41d3d1-0x41d423 decrements a counter while a
  control key is held - a still-unidentified mechanism, working hypothesis
  an idle/attract-mode timeout, not confirmed). The REAL confirm path is
  Allegro's buffered keypress queue - `keypressed()`/`readkey()`
  (0x41d416/0x41d664/0x41d671) - dispatched through a scancode-indexed jump
  table at VA 0x4d7b18; both `KEY_ENTER`(67) and `KEY_SPACE`(75) dispatch to
  0x41d9dd, the actual "confirm the highlighted entry" handler (it checks a
  per-entry "is this a directory" byte at `0x50093c + cursor*24` before
  calling `play_menu_select()` and setting the loop's own exit flag).

**Not yet solved**: every script variant tried (holding `KEY_ENTER`, a short
tap, moving the cursor off entry 0 first with `KEY_DOWN` in case entry 0 is
a non-file marker) left virtual `T` advancing into the tens of thousands of
ticks with **zero safepoints** - i.e. `0x41d9dd` was never observed to fire,
even though the SAME direct-injection `_handle_key_press` mechanism reaches
`is_enter`/menu confirmation correctly everywhere else in this project
(`newgame.txt`'s own proven ENTER-hold). Not root-caused within this pass's
budget; `carrier/scripts/play_itr.txt` is left checked in with the full
disassembly evidence and three concrete follow-up candidates in its own
header comment (keycode=0 interacting with `readkey()`'s packing;
`clear_keybuf()` at `replay_selector`'s entry; the still-unexplained
`poll_control`/`is_any` counter being a precondition gate on the
`keypressed()` path) - a future pass with milestone 9's `--trace-window`
live single-step trace should settle this far faster than more static
disassembly reading.

### New/changed options and files (this pass)

| what | where |
|---|---|
| `--headless`, `--no-sound` (bare flags) | `main.cpp`, `headless.hpp/.cpp` |
| `--frame-digest-every N`, `--frame-digest-out PATH`, `--frame-dump-at T PATH` | `main.cpp`, `frame.hpp/.cpp` |
| `--print-globals expr1,expr2,...` | `main.cpp`, `print_globals.hpp/.cpp`, `gen/gen_print_globals.py`, `gen/it_print_globals.inc` |
| `"headless"`, `"frame_oracle"`, `"print_globals"` objects in `--report` | `trace.cpp` |
| `carrier/scripts/play_itr.txt` | new (navigation half working, see item 5) |
| `ICY_TOWER_GLOBALS`, `print_globals_summary` | `scripts/play.py` (`--play-replay`) |

### Known gaps / open problems (this pass)

- **`--headless` does not reach gameplay on this host** (item 1) - the
  argument-rewrite mechanism is proven correct (log evidence), but a
  genuine post-mode-set stall (not merely slower rendering) blocks every
  variant tried. Root cause not identified; three candidate directions are
  listed in item 1. Kept explicit-opt-in, not implied by default.
- **The headless-vs-cnc-ddraw frame-oracle comparison could not be run**
  (item 2) - directly blocked by the above. The frame-oracle mechanism
  itself is proven deterministic on the reachable (non-headless) comparison.
- **`.itr` playback's final "confirm selection" step is unsolved** (item 5)
  - navigation to `replay_selector` is solved and reproducible; the
  `readkey()`-dispatched confirm handler was not observed to fire despite
  the same key-injection mechanism working everywhere else in this project.
- **`print_globals`'s array/struct-path parser is deliberately scoped**: one
  level of `[index]` per array dimension (up to 2 dims, matching the
  generator's own cap), and no arithmetic/comparison expressions - it walks
  a chain of index/member accesses, nothing more. Sufficient for every
  Icy Tower global this pass needed; a future user wanting e.g. a computed
  offset would need the generator/walker extended.
- **The `itr5` diagnostic run needed a manual kill** after running for
  several real minutes without its `--run-seconds 60` watchdog visibly
  firing, following a burst of many back-to-back `carrier.exe` launches on
  a loaded host; a controlled, isolated re-run of the exact same command
  immediately afterward (the `--run-seconds 10` sanity check in this pass's
  own working notes) completed in ~2 seconds, so this was not chased
  further as a `--run-seconds`/watchdog regression - flagged in case a
  future pass sees it recur under similar host load.

## Binding table generated (2026-09-07)

Removes the last piece of per-function hand scaffolding `carrier/src/
bind.cpp` still carried after "Milestone 12 at scale": a 35-row C++ literal
(`kFns[]`) giving each bound function's name/VA/argc/return-shape/lifted-
native-src pointers, plus one hand-written `dom_<fn>()`/`fa_<fn>()` C++
function per row encoding its comparison domain. Both are now generated.

**The binding table.** `carrier/gen/gen_bind_table.py` -> `carrier/gen/
bind_table.inc` (generated, DO-NOT-EDIT, included by `bind.cpp`). One row
per function `carrier/gen/scan_src_defs.py` finds defined in
`src/icytower/*.c` that also has a VA in `carrier/gen/interop_index.json`
(cross-checked against `carrier/gen/it_funcs_table.inc` - the generator
exits non-zero on any VA/size disagreement between the two, rather than
trusting either silently); VA/size/prototype come from that same JSON,
argc/return-shape are parsed straight out of the prototype string, and
whether a `lifted_<fn>`/`native_<fn>` form exists is parsed out of
`build.cmd`'s own link line (not merely out of `carrier/lift/lifted/`'s
directory listing, which holds more generated candidates than are actually
linked into `carrier.exe` - MEASURED: 17 `.c` files there, only 3 linked).

**The comparison domain, ONE source of truth.** Every domain the old
`dom_<fn>()` functions hand-coded turned out to reduce to one of five small,
generic shapes: a fixed global, an argument-relative pointer (+ optional
byte offset), the `ply[player_id]` double indirection `update_frame` uses,
or the "counter, then the slot it now indexes" shape `add_combo`/
`add_jump_sequence` use. `bind.cpp` gained ONE generic engine
(`hash_one_region()`/`fault_addr_generic()`) that walks a small `Region`
array per function instead of one bespoke function per row; the array
contents come from a new hand-curated data file, `carrier/gen/
fn_domains.json` (regions + which region `--fault-inject` targets, `-1` for
none). **Preferred-option (single generated/declared table shared with the
offline harness) was evaluated and not used**: `carrier/lift/harness/
lift_check.py`'s own `SPECS` dict is also data-shaped (`"domain": [(va,
len), ...]`), but its VAs are fixed SYNTHETIC test addresses the harness's
vector generator places arguments at for its own offline run (e.g. its
`PLAYER_VA` is a scratch address, never the real `ply[player_id]` slot the
running carrier must resolve dynamically) - reusing it byte-for-byte was not
possible without re-deriving exactly the per-function symbolic knowledge
(which argument, which global) `fn_domains.json` already states directly.
`fn_domains.json`'s own header documents this choice; region shapes were
cross-checked by hand against the harness `SPECS` entry of the same name
wherever the two plainly correspond (is_solid/jump_player/getFloorData/
line_intersect/...). **Default domain for a function absent from
`fn_domains.json`**: empty region list (EAX only, when the prototype returns
one) and no `--fault-inject` target - the same default the pre-existing code
already used for `get_gamepad`'s sibling `get_controls` and `ok_to_play`.

**Widened as a mechanical consequence, not a new decision**: `kMaxFns`
35->42 (one stub slot per row the generator can now produce, including
functions beyond the historical 35 - see below); `kMaxArgs` unchanged at 10
(`gen_bind_table.py` itself refuses, loudly, to emit a row for a function
with more than 10 cdecl arguments, so this ceiling and the generated table
can never silently drift apart the way milestone 12's hand-maintained
`kMaxArgs=4` once did).

**One row per function now, not just the historical 35.** Because the
generator derives rows mechanically from whatever `src/icytower/*.c`
currently defines, it picked up every function added since - `add_floor`,
`reset_player`, `update_player` (batch 6), `handle_player_collision_
original`, `play_jump_sound` (batch 7's call-trace-domain work landing
concurrently in `carrier/lift/harness` - not touched by this pass) - for
**40 rows** total. Two more, `draw_buffer` and `start_reward`, are scanned
but excluded via a new small hand-curated exception list, `carrier/gen/
build_blockers.json`: both reference Allegro/asset-seam symbols (`makecol`,
`textprintf_ex`, `asset_font`, `asset_bitmap`) the carrier's `src/` compile
step does not resolve (LNK2019 unresolved externals, MEASURED first) - a
separate asset-seam integration task (`src/icytower/ASSETS.md`), out of
scope here.

**The MSVC/GCC-x87 file split, also mechanized.** `scan_src_defs.py --list-
build-files {msvc,gcc}` (new) replaces the old hand-listed 4-file GCC set:
a file goes to `gcc` if it contains a genuine `double`/`float` token in code
(comments/string literals stripped first), `msvc` otherwise, and only files
defining at least one real game function are considered at all (excludes
`assets_standalone.c`'s harness-only helpers automatically). `build.cmd`
calls this twice (source list, then again with `--ext .obj`/`.o` to derive
the matching link-line object list from the SAME classification, not a
third hand-maintained list) and drops the result straight into `cl`/`gcc`.
**Measured, not assumed, this pass**: `particle.c` no longer needs the GCC
x87 build - its current recovered source uses Allegro's `fixed` (a plain
`int32_t` typedef, `allegro_types.h:148`) throughout, not `double`, per its
own header comment ("integer-only... not float"); the old hardcoded
build.cmd GCC list was stale on this point (inherited from an earlier
milestone-12-era description of this file). The mechanical detector moved
it to the MSVC list; `update_particle`/`create_particle`'s in-vivo EQUAL
result below (unchanged, still exercised through `bind_all.py`'s existing
35-function pass, X87_FUNCTIONS bookkeeping unaffected) confirms this was
safe. `update_player.c`/`handle_player_collision_original.c` (real
`(double)` arithmetic, confirmed by inspection) are correctly newly routed
to GCC; `add_floor` (`map.c`) stays on GCC too, matching PROMOTIONS.md's own
"GCC is the toolchain of record" note for that function even though its
sibling functions in the same file (`getFloorData`/`reset_map`/`get_level`)
are pure integer.

**Gates, re-verified against the regenerated table** (assets restored
before every run, as everywhere in this file):

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --digest-out ../artifacts_bt/g1a.txt
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt --stop-at-tick 1000 --run-seconds 60 --digest-out ../artifacts_bt/g1b.txt
python carrier/scripts/compare_digests.py artifacts_bt/g1a.txt artifacts_bt/g1b.txt
  -> EQUAL (876 ticks, ...)

carrier.exe ... --bind update_frame=src      --fn-digest-out ../artifacts_bt/g2A.txt
carrier.exe ... --bind update_frame=original --fn-digest-out ../artifacts_bt/g2B.txt
python carrier/scripts/compare_fn_digests.py artifacts_bt/g2A.txt artifacts_bt/g2B.txt
  -> EQUAL (877 invocations, ... [src] vs ... [original])

carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt --stop-at-tick 2528 --run-seconds 180 --bind-file <abs>/scripts/all35_src.bindfile --digest-out ../artifacts_bt/all35_ticks.txt
python carrier/scripts/compare_digests.py artifacts_bt/all35_ticks.txt replays/human_test.digest
  -> EQUAL (2293 ticks, ...)
```

All three unchanged from every prior pass - confirms the generated table
reproduces the milestone-11/12 binding behavior for the original 35
byte-for-byte (same VAs, same forms bindable, same digest results), not
just "compiles".

**New rows verified in vivo** (`carrier/scripts/bind_all.py --fn
reset_player,update_player,add_floor,handle_player_collision_original,
play_jump_sound`, `replays/human_test.txt`): `reset_player` EQUAL (1
invocation), `update_player` **EQUAL** (2293 invocations, GCC x87 build -
the batch-6 offline proof holds in vivo too), `add_floor` **DIFFER**
(`FIRST DIFFERENCE fn=add_floor k=5 T=220 field=post` - identical args and
pre-state through k=4, a real divergence in the recovered source's 6th
floor-generation call within tick 220; per-tick digest also first differs
at T=237; root cause not investigated, `src/` out of scope for this pass -
flagged as a background task with this evidence, the same treatment
`add_jump_sequence`'s own earlier DIFFER got), `handle_player_collision_
original` UNVERIFIED IN VIVO (0 invocations - `human_test.txt` never takes
the collision branch that reaches it, the same class of gap already
documented for `is_solid`), `play_jump_sound` EQUAL but **vacuously** (void
return + the stated default empty domain - no call-trace mechanism in
`bind.cpp` yet - so this only certifies 46 crash-free invocations, not
behavioral equivalence). Full detail and the raw per-invocation records:
`src/icytower/INVIVO.md` "Binding table generated" section.

### Known gaps / open problems (this pass)

- **`add_floor` DIFFERs in vivo** - RESOLVED by the "Divergence 008" pass
  below; the bullet as written at the time follows. A newly-found, real divergence (see
  above), not fixed this pass (verification only, per this task's own
  scope). Evidence recorded in `src/icytower/INVIVO.md`; needs the same
  disassembly-vs-source root-causing `add_jump_sequence`'s DIFFER already
  got.
- **`handle_player_collision_original` and `play_jump_sound` have only the
  default (empty) comparison domain** - meaningful verification needs the
  call-trace domain mechanism `carrier/lift/harness/lift_check.py`'s own
  `SPECS` already uses offline for both (their real effect is a
  `play_sound()` call argument, not a memory write), which `bind.cpp` does
  not implement. `handle_player_collision_original`'s domain COULD be
  partially expressed today (its own `Tplayer`-via-`ply[player_id]` and
  `any1X` global writes are real and harness-expressible per PROMOTIONS.md's
  own batch-6 note) but was left at the default this pass to keep
  `fn_domains.json`'s first version to the patterns already proven by the
  original 35, rather than adding an under-tested new region combination
  under this task's time budget.
- **`draw_buffer`/`start_reward` cannot be bound at all** - missing
  Allegro/asset-seam symbol resolution in the carrier's `src/` compile step
  (`carrier/gen/build_blockers.json`), a separate integration task.
- **`carrier/src/bind.hpp`'s `BindSavedState` arrays are still sized `[8]`**
  - RESOLVED by the "Divergence 008" pass below; the bullet as written at
  the time follows.
  (a stale comment: `// == bind.cpp's kMaxFns`) while `bind.cpp`'s
  `bind_state_save`/`bind_state_load` loop `i < kMaxFns` (now 42) writing
  into them - a pre-existing out-of-bounds write (predates this pass;
  already present when `kMaxFns` was 35) that this pass's `kMaxFns` increase
  makes marginally larger, not something this pass introduced. Exercised
  only by `--snapshot-at-tick`/`--restore-at-tick` (G3 in `scripts/
  gates.ps1`, not one of this task's required gates), so not hit by any run
  above; not fixed here (`bind.hpp`/`snapshot.cpp` are outside this task's
  stated scope) - flagged as a background task.


## Divergence 008 - `add_floor`'s `rand()` reached the wrong C library (2026-09-07)

Closes the two open problems the "binding table generated" pass left above:
`add_floor` DIFFERing in vivo, and `BindSavedState`'s `[8]` overflow.

### A. What differed, named by field

`carrier/scripts/bind_all.py --fn add_floor` over `replays/human_test.txt`
reported `FIRST DIFFERENCE fn=add_floor k=5 T=220 field=post` with identical
`args=004f8b18` and identical `pre` through k=4. Two snapshots at tick 220
(one unbound = the ORIGINAL machine code, one `--bind add_floor=src`) and
`pf_inspect.py diff` name the bytes:

```
carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt \
    --stop-at-tick 221 --snapshot-at-tick 220 --snapshot-out ../artifacts_task/d008_orig
carrier.exe ... --bind add_floor=src --snapshot-out ../artifacts_task/d008_src
python carrier/scripts/pf_inspect.py diff artifacts_task/d008_orig artifacts_task/d008_src

FIRST DIFFERING GLOBAL INSIDE THE PER-TICK DIGEST SCOPE:
  map @0x004f8b18 (Tmap, 784 bytes, main.c)
  first differing byte: +172 (VA 0x004f8bc4)  A=14 B=17
  member: .room+172
```

172 = `room[7]` (7 x `sizeof(Tfloor)` = 168) + 4 = **`Tfloor.start_tile`**;
`.end_tile` at +176 differs too. ORIGINAL `{start_tile=20, end_tile=29}`,
SRC `{23, 34}` - same `level` (6), same branch taken, different `rand()`
values. k=5 is the first `add_floor` invocation that calls `rand()` at all
(level 0 is a checkpoint, levels 1..4 are empty filler rows, 0 draws each -
`notes/layout_determinism.md` SS2), which is why the split lands exactly
there and not earlier.

### B. Root cause: a binding gap, not a semantic error

`src/icytower/map.c` calls plain `rand()`. `gen/pf_bindings_src.h` binds
every game global and game function by plain name, but `rand` is on
`gen_bindings.py`'s `RESERVED_CRT_WINDOWS_IDENTS` skip list and is not a
game-scope symbol at all - so nothing bound it, and the link resolved it to
the **carrier's own** statically-linked CRT:

```
carrier/obj/carrier.map:  0001:000aa60d  _rand  100ab60d f  libucrt:rand.obj
mingw32 nm carrier/obj_gcc/map.o:        U _rand
```

That is a different generator from the **guest's** msvcrt `rand` import,
whose IAT slot (0x514944) the carrier owns and, in `--det`, replaces with
`det.cpp`'s pinned LCG - the one `new_game()`'s
`srand(Treplay.random_seed)` seeds and `snapshot.cpp` captures. MEASURED,
same two tick-220 snapshots, `pf_inspect.py show`:

| run | pinned `rng_state` | `rng_calls` |
|---|---|---:|
| ORIGINAL | 0xea58d532 | 14 |
| SRC (pre-fix) | 16944 - *still exactly the seed* | 4 |

The `src` form advanced the game's own RNG **zero** times over all 30
initial floors; the ORIGINAL drew 10 (5 real floors x 2 draws each). Both
runs show `rec_seed = 16944`, and re-running the pinned LCG from
`srand(16944)` in Python reproduces the ORIGINAL floor exactly - `r1=22602`
-> `width = 6 + 22602 % 9 = 9`, `start_tile = 5 + 4173 % 21 = 20`,
`end_tile = 29` - which is positive proof that `map.c`'s recovered RULES
were correct and only its `rand()` SOURCE was wrong. (The same model also
lands the state on 0xea58d532 after exactly 10 draws, matching `rng_calls`.)

The x87 `fidivr`/`fmuls` ratio, the two magic-multiply divisors, the
checkpoint/empty cadence and the `floor_size_modifiers` clamp were all
re-checked against `artifacts/disasm.txt` 0x4167dc..0x416a3c line by line
and are correct; the float constants at 0x4d6dc0/0x4d6dc4 read out of the PE
as 300.0f and 10.0f, exactly as `map.c` has them.

### C. Why 160 000 offline vectors missed it

`carrier/lift/harness/lift_check.py` force-includes
`carrier/lift/harness/pf_harness_rand.h`, which redirects `map.c`'s
`rand()` to `harness_rand()` - the same LCG, seeded per vector - on BOTH
sides of the comparison (unicorn hooks the guest thunk and emulates the LCG
in Python; the compiled candidate calls `harness_rand()`). That is correct
for an offline oracle, and it is exactly why the oracle is blind here: it
substitutes a shim for the library call, so it tests the *algorithm* and
structurally cannot test *which copy of the library the real build reaches*.

It was **not** a missing precondition. `gen_add_floor`'s directed level pool
already contained `level = 5`, and at that index `floor_shrink` was already
nonzero, so the exact in-vivo branch was being generated on every run. The
in-vivo difficulty settings were read out of the snapshot for confirmation:
`Treplay+0x8c..0x9c` = `{floor_shrink=1, floor_size=1, start_speed=5,
speed_increase=1, gravity=1}`.

### D. The fix

`gen/gen_bindings.py` gained `GUEST_CRT_IMPORTS` - a deliberately tiny,
hand-curated set of C-library names that a promoted `src/` function must
share the GUEST's copy of. It emits, into `pf_bindings_src.h`:

```c
#include <stdlib.h>   /* FIRST, so the macros rewrite CALLS, never declarations */
/* rand  -> msvcrt.dll!rand IAT slot VA=0x00514944  (the original's own thunk) */
typedef int (__cdecl *PFN_crt_rand)(void);
#define rand (*(PFN_crt_rand *)0x00514944)
/* ... and the same for srand at 0x0051495c */
```

An indirect call through the guest's IAT slot is bit-for-bit what the
original machine code does (`call _rand` @0x4bad18 -> `jmp *[0x514944]`),
so it picks up whatever the carrier installed there - the pinned LCG in
`--det`, the real msvcrt otherwise - and is counted by the import census
like any other guest import. Slot VAs are looked up in `imports.json` (the
same evidence file `gen_imports.py` reads), never hand-typed; the generator
aborts if a name is missing, and refuses to guess when a name has two
distinct slots (this image imports `msvcrt!_stat` twice - MEASURED while
writing the loader). `srand` is bound for the same reason even though no
promoted function calls it yet, so the seed and the draws can never end up
split across two generators.

`map.o` no longer references `_rand` at all (`nm`: only `_get_demo` and
`_memmove` remain undefined). `src/icytower/map.c` is unchanged apart from a
header note recording that its `rand()` is part of its binding surface.

### E. Results after the fix

```
python carrier/scripts/bind_all.py --fn add_floor --out-dir ../artifacts_task/d008_bind
  -> EQUAL (533 invocations)
     per-tick digest: EQUAL (2293 ticks) vs replays/human_test.digest

python carrier/lift/harness/lift_check.py --form src --funcs add_floor --vectors 20000 --seed {20260907,1,2,3}
  -> [add_floor/SRC] EQUAL over 20000 vectors            (x4 seeds, MSVC)
python carrier/lift/harness/lift_check.py --form src --toolchain gcc --funcs add_floor --vectors 20000 --seed {20260907,1,2,3}
  -> [add_floor/SRC/GCC] EQUAL over 20000 vectors        (x4 seeds, GCC x87)
negative control (GCC, --fault add_floor:5:0)
  -> DIFFER at vector 5 ... Tmap+0x0 (VA 0x00792000)     (oracle still discriminates)

carrier.exe --det --pace=fast --input=script --input-script ../replays/human_test.txt \
    --stop-at-tick 2528 --run-seconds 300 --bind-file <abs>/scripts/all_src.bindfile \
    --digest-out ../artifacts_task/d008_allsrc_ticks.txt
python carrier/scripts/compare_digests.py artifacts_task/d008_allsrc_ticks.txt replays/human_test.digest
  -> EQUAL (2293 ticks)      <- ALL 40 generated binding-table rows bound to src at once

powershell scripts/gates.ps1 -Tag d008
  G1  EQUAL (876 ticks)   G2  EQUAL (877 invocations)
  G3a EQUAL (301 rows pre/post-rewind; 602 rows cold-from-anchor)
  G3b in-run rewind restored the sensor to k=276
```

`carrier/scripts/all_src.bindfile` is new: every row of `gen/bind_table.inc`
(40 - `draw_buffer`/`start_reward` have no linked `src` form). The
historical `all35_src.bindfile` is deliberately left untouched so the
milestone-12 "all 35 bound at once" measurement stays reproducible as taken.

### F. Regression guard added to the offline generator

`lift_check.py`'s `gen_add_floor` paired its hand-picked
`ADD_FLOOR_LEVEL_POOL` with `floor_shrink = 0 if k % 2 == 0` and a random
`floor_size`, which made the DIRECTED half of the generator one-sided: an
even-indexed pooled level was NEVER seen with `floor_shrink != 0` (the
float-ratio branch) and an odd-indexed one never with `floor_shrink == 0` -
half of every hand-picked boundary went untested in the branch it was picked
for. The first `len(pool) * 2 * 5 = 750` vectors now enumerate the full
`(pooled level) x (floor_shrink in {0,1}) x (floor_size in 0..4)`
cross-product deterministically (`ADD_FLOOR_DIRECTED`), which contains the
exact in-vivo triple `(level=5, floor_shrink=1, floor_size=1)` at index 56;
the random families beyond it are unchanged. This cannot catch a recurrence
of divergence 008 itself (see SS C - no offline harness can), but it does
close the coverage hole the investigation exposed while looking for one.

### G. `BindSavedState[8]` overflow, fixed

**What the array is for.** Not nested/re-entrant stub state - the stub keeps
none (its `id` rides on the guest stack, `bind_stub_common`). `BindSavedState`
holds the sensor's PER-BOUND-FUNCTION counters - `g_invocations[]`,
`g_crossings[]`, `g_records[]`, one slot per binding-table row - which live
in `bind.cpp` statics rather than guest memory, so an in-process rewind
(`--snapshot-at-tick` + `--restore-at-tick`) has to rewind them too;
otherwise the `k=` index in `--fn-digest-out` would keep counting up across
the rewind and `compare_fn_digests.py` could not line the two passes up
(milestone 8). It is embedded by value in `snapshot.cpp`'s `CarrierState`
and written verbatim into `carrier.bin`.

**The bug.** The three arrays were still `[8]`, carrying a comment that
claimed "== bind.cpp's kMaxFns" long after `kMaxFns` had grown 8 -> 35 ->
42, while `bind_state_save`/`bind_state_load` loop `i < kMaxFns`. Every
`--snapshot-at-tick`/`--restore-at-tick` therefore wrote
`3 x (42 - 8) x 8 = 816` bytes past the end of a stack-allocated
`CarrierState`, and read them back the same way. Nothing checked the two
against each other.

**The fix, sized from the generated table rather than restated.**
`bind.hpp` now declares `kBindMaxFns` and sizes all three arrays from it;
`bind.cpp`'s `kMaxFns` is defined AS `kBindMaxFns`, so the snapshot struct
and the loops can no longer disagree. Two `static_assert`s make future drift
a build error instead of a silent overrun: `kNumFns <= kMaxFns` (the
GENERATED table is the authority on how many rows exist) and
`sizeof(kStubs)/sizeof(kStubs[0]) == kMaxFns` (a short `kStubs` initializer
would otherwise zero-fill silently). `PF_CARRIER_STATE_VERSION` 1 -> 2,
because `carrier.bin`'s size and shape changed; snapshots taken by older
builds are now rejected with a legible reason instead of "wrong size".

**Proof, on the all-bound run** (40 rows bound - the configuration that used
to overrun by the full 816 bytes):

```
carrier.exe --det --pace=fast --input=script --input-script scripts/newgame.txt \
    --stop-at-tick 1000 --bind-file <abs>/scripts/all_src.bindfile \
    --digest-out d008_allR.txt --fn-digest-out d008_allF.txt \
    --snapshot-at-tick 400 --snapshot-out d008_allsnap --restore-at-tick 700

python scripts/certify_snapshot.py rewind d008_allR.txt --anchor 400 --cold d008_g1a.txt
  -> EQUAL (301 rows, T=400..699, pre-rewind vs post-rewind)
  -> EQUAL (602 rows, T=400..1000, cold-from-anchor vs post-rewind)
```

and every function re-invoked after the rewind resumes at exactly the `k` it
held at the snapshot (checked directly over `d008_allF.txt`; the 12 rows
never invoked again after T=400 have nothing to check). `carrier.bin` is now
2712 bytes. Note `certify_snapshot.py fn` is NOT applicable to a multi-
function `--fn-digest-out` - it assumes a single function's `k` sequence and
will report a spurious FIRST DIFFERENCE on an interleaved file; use it as
`gates.ps1` G3b does, on a single `--bind <fn>=src` run.

## In-vivo pass, batch 7 + .itr workload (2026-09-07)

Verifies batch 7's 3 promoted functions (`play_jump_sound`,
`handle_player_collision_original`, `start_reward`) in vivo, unblocks
`draw_buffer`/`start_reward` from `build_blockers.json`, runs the generated
binding table's remaining unverified rows over `replays/human_test.txt` +
`scripts/newgame.txt`/`scripts/menu_idle.txt`, and investigates
`scripts/play_itr.txt`. Full detail: `src/icytower/INVIVO.md` "In-vivo
pass, batch 7 + .itr workload".

**`build_blockers.json` unblocked** (`draw_buffer.c`, `start_reward.c`):
both files' own header comments already carried the exact carrier-world
compile recipe that resolves their LNK2019s
(`/FIpf_bindings_src.h /FIpf_lib_bindings.h /FIpf_asset_bindings.h`) - what
was missing was `build.cmd` using it. `carrier/gen/scan_src_defs.py` gained
`--extra-fi {base,extra}`, a CALL-syntax scan (`name(`, not a bare-token
scan - `control.c`'s `check_control_key(..., int key)` parameter is
spelled `key`, colliding with Allegro's own `key[]` global as a bare token,
which a naive scan would wrongly flag) against `pf_lib_bindings.h`'s bound
names plus the 5 asset-seam accessors; `build.cmd`'s MSVC compile step now
splits into two `cl` invocations (base: `pf_bindings_src.h` only; extra:
all three headers) driven by that partition. `build_blockers.json`'s
`"files"` is now `{}`; `gen_bind_table.py` picked up both functions
automatically (40 -> 42 rows).

**`kBindMaxFns` raised 42 -> 60** (`carrier/src/bind.hpp`/`bind.cpp`,
matching `BIND_STUB(42..59)` added): a concurrently-running batch-8 pass
added a 43rd binding-table row (`draw_scroller`) mid-build, tripping
`bind.cpp`'s `kNumFns <= kMaxFns` static_assert. Same mechanical bump this
constant has taken twice before (8 -> 35 -> 42); raised with headroom this
time. `draw_scroller` itself is batch 8's function, out of this pass's
scope, and is not in any bindfile this pass touches.

**Results**: `start_reward` EQUAL in vivo (3 invocations, `replays/
human_test.txt` - the recording DOES trigger a reward, contrary to this
task's own "if no reward happens" caveat). `draw_buffer` UNVERIFIED IN
VIVO on all three workloads tried (`human_test.txt`, `newgame.txt`,
`menu_idle.txt` - 0 invocations each); its own header comment (a debug/
profile text overlay, not a normal-play screen) is the likely reason no
scripted key sequence reaches it. `handle_player_collision_original`
UNVERIFIED IN VIVO on `human_test.txt` (already known) and now also
`newgame.txt` (tried this pass, still 0 - the recording's `collision_type`
never selects the `_original` dispatch target within 500 gameplay ticks).
All-bound run, 42 rows (`carrier/scripts/all_src.bindfile` extended with
`draw_buffer=src`/`start_reward=src`, `draw_scroller` deliberately
excluded): **EQUAL (2293 ticks)** against `replays/human_test.digest`.

**`.itr` workload** (`carrier/scripts/play_itr.txt`): one real bug found
and fixed in `carrier/src/det.cpp` - `deliver_due_input()` hardcoded
`_handle_key_press`'s ASCII argument to `0` for every synthetic key press;
real Allegro's DirectInput driver (`third_party/allegro-4.4.1/src/win/
wkeybd.c`) only does that for non-printable keys, passing a real ASCII
value (13/32/27) for ENTER/SPACE/ESC via `ToAscii()`. Fixed
(`ascii_for_allegro_code()`); verified inert for ordinary gameplay input
(G1 and the `human_test.txt` gate both still EQUAL after the change,
exactly as expected - `is_left`/`is_right`/... read the scancode-indexed
`key[]` array, set unconditionally regardless of this argument). This fix
alone did NOT make the replay-browser confirm fire: disassembling
`_replay_selector` in full (0x41d330-0x41d9dd) found its dispatch
(0x41d671) is indexed purely by `readkey()`'s SCANCODE half, never the
ASCII half. The likely real gate, also found this pass: a local debounce
counter (`-0x42c(%ebp)`, init 1000 at entry) decrements only while
`is_any(ctrl)` is true and not already exactly 0; once it hits exactly 0
while a button is held, every frame takes a "movement" branch that never
consults `keypressed()`/`readkey()` at all. `play_itr.txt`'s own key holds
(5 nav taps + a 360-tick first `KEY_ENTER` hold) are very likely enough to
drain this counter before the second `KEY_ENTER`. Two variants (a released
tap instead of a held second `KEY_ENTER`; every hold in the script shortened
to the minimum, tested from scratch copies, not committed) were tried as
cheap tests of this theory - neither reached `play()` either, so the exact
mechanism is still not fully confirmed. **Outcome: `play_itr.txt` still
does not reach `play()`; 0 additional functions gained in-vivo coverage
from it this pass.** Full disassembly evidence and the candidate follow-ups
this pass would try next are in `play_itr.txt`'s own header comment and
`src/icytower/INVIVO.md`.

**Gates re-verified after this pass's `det.cpp`/`bind.hpp`/`bind.cpp`
changes**: `newgame.txt` two-run determinism EQUAL (876 ticks), `human_test.txt`
vs `replays/human_test.digest` EQUAL (2293 ticks) - both before and after
the `.itr` investigation's det.cpp change, and again after the final
all-bound run.

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

