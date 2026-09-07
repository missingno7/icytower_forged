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
