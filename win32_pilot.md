# PortForge Win32 carrier pilot — Icy Tower

Status: **living design record**, 2026-09-07. Rewritten from the original
CyberStorm concept note after reconnaissance of the actual pilot target.
Evidence lives in `notes/` (capsule, recon, census, replay format) and
`artifacts/`; this file states decisions and their evidence labels.

Labels used throughout:

```text
KNOWN      verified in the binary, its DWARF, or a run
INFERRED   strongly supported by evidence, not directly observed
HYPOTHESIS design bet awaiting runtime evidence
TEMPORARY  compatibility workaround that must be retired
```

---

## 1. Purpose

> **Carrier**: a deterministic execution substrate that preserves the
> original game's behaviour while allowing individual implementations to be
> substituted and automatically judged against the original through replay
> and state comparison.

Build a Win32 equivalent of the PortForge carrier for one small target,
using the **host machine** as the execution substrate instead of a VM:

```text
original icytower15.exe
    ↓
mechanically isolated executable carrier   (loader owns the image + boundaries)
    ↓
instrumented, controllable execution       (every import is a trampoline)
    ↓
deterministic replay                       (time, input, RNG pinned)
    ↓
safepoint snapshots / restore
    ↓
inspection, stepping, tracing, comparison
    ↓
gradual replacement with readable native code, verified against the original
```

The carrier is disposable. Its only job is to give PortForge enough control
over the original program to record, replay, snapshot, compare and replace.
Mechanisms are target-independent (generated from PE data); everything
Icy-Tower-specific is data in policy tables and notes, so the same workflow
can be pointed at MissionForce: CyberStorm next.

## 2. What Icy Tower actually is (KNOWN unless marked)

| fact | value | source |
|---|---|---|
| binary | Icy Tower 1.5.1, PE32 GUI, MinGW GCC 4.4.1, 2011-12 | `notes/binary_recon.md` §a |
| image | base 0x400000, SizeOfImage 0x38c000, **no relocations, no TLS** | pefile |
| debug info | **full DWARF** + COFF symbol table; 2465 functions, 148 CUs | `artifacts/functions.json` |
| .text ownership | Allegro 4.4.1 (static, C-only drivers) 61.5 %, libvorbis/ogg 17 %, **game 16.5 % (253 functions, 28 C files)**, CRT 3.6 % | recon §o |
| game main loop | `play()` 0x411a00 waits on tick global 0x506938 via `rest(2)`, runs one tick at **0x4124f4**, `draw_frame` 0x40929c, `blit_to_screen` 0x40b6bc | recon §l |
| time | 20 ms Allegro timer thread increments 0x506938 (`cycle_counter`); game QPC/clock/time calls in `play()` are anti-cheat telemetry, not simulation inputs (KNOWN, `notes/replay_format.md`) | recon §c |
| input | keyboard 100 % DirectInput on Allegro's input thread → `key[]`; game reads `key[]` through control.c; joystick polled per frame; mouse only in menus | recon §d |
| threads | main + Allegro timer thread + Allegro window thread + DirectInput thread + one startup-only ad-fetch pthread. **Game logic runs only on the main thread.** | recon §b |
| RNG | msvcrt `rand`/`srand`; seeded in `new_game`/`init_game`; tower layout is a pure function of the seed | recon §k |
| graphics | Allegro DirectDraw drivers + GDI fallback, 640x480; oracle runs with **cnc-ddraw** (user-installed for performance, not correctness) | recon §g, user |
| audio | DirectSound (Allegro mixer), software MIDI; ogg decoded synchronously on main thread | recon §h |
| files | data/*.dat, tower.cfg, gamepad.txt, profiles/, characters/, replays/*.itr, screenshots/*.png, log.txt | recon §i |
| network | one HTTP GET of an ad list at startup on its own thread; failure is logged and ignored | recon §j |
| own replay system | `Treplay` header carries `random_seed`; RLE `Trecord{key_flags,cycle_count}` stream of left/right/fire; same `handle_player_input` path for record and playback | `notes/replay_format.md` |
| lifting hazards | 54 switch tables, x87 FPU everywhere, no self-modifying code, no asm blitters | recon §m–n |
| imports | 320 across 13 DLLs; census in `notes/import_census.md` | census |

## 3. Architecture decision: function-granular mixed execution

**Decision (2026-09-07, revised the same day after the historical-projects
review):** the carrier is a 32-bit host process that maps the original image
at its real base and treats **the function as the unit of ownership**. Every
game function exists in exactly one of three forms at any time, and all three
are reached through the same identity: the original address.

```text
ORIGINAL   the original bytes, executing natively on the host CPU
           (bootstrap form of every function; the oracle forever)
LIFTED     automatically generated C, faithful to the bytes
           (the experiment: can a generated form hold as the carrier?)
NATIVE     clean, readable C written by a person or an AI
           (the goal; verified against ORIGINAL/LIFTED from equal state)
```

```text
carrier.exe (32-bit, based at 0x10000000, /FIXED)
  ├── PE image mapper         maps icytower15.exe at 0x400000
  ├── import resolver         fills the guest IAT; every slot → trampoline/wrapper
  ├── trampolines (generated) count + trace, tail-jump to the host function
  ├── wrappers (evidence-gated) time, input, RNG, heap, exit, paths
  ├── binding table           original address → {ORIGINAL | LIFTED | NATIVE}
  │                           installed as a 5-byte entry patch through a
  │                           counting stub, so direct calls, indirect calls,
  │                           callbacks, vtables and timers all reach the
  │                           bound form without per-site work
  ├── generated interop (from DWARF, not hand-written)
  │       icytower_globals.h   every global at its original address, real type
  │       icytower_types.h     every struct/enum layout
  │       icytower_funcs.h     every function's prototype at its original
  │                            address (typed call-by-address = Interop::call)
  ├── guest stack             fixed address, TEB limits patched (TEMPORARY)
  └── fault diagnostics       VEH → EIP, function name, stack walk
```

How this answers the historical-project lessons (OpenRCT2, OpenLoco, re3):

| lesson | mechanism here |
|---|---|
| separate code ownership from state ownership | all three forms operate on the original memory at the original addresses; state migration is a later, separate coastline |
| one explicit cheap boundary, no per-function bridges | the binding table + entry patch; calling an unpromoted function from C is a typed call to its address from the generated header |
| original addresses as identity | address is the key of the binding table, the trace, the coverage map and the verdict |
| hooks in both directions | ORIGINAL→NATIVE via the entry patch; NATIVE→ORIGINAL via the typed address call; both counted at the stub |
| callbacks / function pointers | the patch is at the callee, so the path to it is irrelevant |
| no manual scaffolding | globals, types, prototypes, trampolines, stubs and thunks are generated from PE + DWARF; hand-written code is the policy tables and the evidence-gated wrappers |
| monotonic carrier removal | per function: ORIGINAL → LIFTED → NATIVE, never back; measured (§8a) |
| oracle verification | replay + safepoint snapshot + per-tick and per-call comparison (§7) |

The scaffolding that made those projects expensive came from having no
symbols, no types and no generator, not from using the original EXE as the
carrier. Icy Tower ships full DWARF, so that scaffolding is generated.

Why ORIGINAL is the bootstrap form and not a whole-program lift: a lifter
for ~760 KB of x87-heavy code plus its verification is the "scaffolding
larger than the game" failure mode (capsule §B), and 78 % of .text is Allegro
and libvorbis, which should be replaced by their real sources at the library
boundary rather than lifted. Native execution of the original bytes reaches
replay, snapshots and comparison fastest, and that infrastructure is what
every LIFTED and NATIVE claim is verified with. The lifter is then tested
where it matters, on game functions, one at a time, with the same verdict.

LIFTED form constraints (so the lifter stays small): memory is accessed at
original addresses through plain pointers (no memory abstraction, the image is
in place); registers live in locals; flags are computed by the lifter only
where consumed; calls go to original addresses with the original convention;
x87 must be represented faithfully enough that physics bits match the oracle
(HYPOTHESIS: `double` is not enough where GCC kept 80-bit intermediates; the
oracle comparison decides, and softfloat x87 is the fallback).

What native execution gives up, and how it is recovered when needed:

| lost by not lifting | recovered by |
|---|---|
| instruction-level trace / single-step | trap-flag stepping through a VEH over a bounded window; unicorn or PortForge `Interp32` as an offline per-function reference executor |
| control over thread interleaving | thread virtualization by entry point for the two Allegro threads that reach game state (§5) |
| snapshot = registers + flat memory | guest pages + guest heap + guest stack + PortForge-owned externalized state at a safepoint (§6) |

Framework placement: pf/76 forbids a project from privately reimplementing a
framework surface. Today no `win32` platform exists in port_forge and its
build is 64-bit MinGW g++, while the carrier must be a 32-bit MSVC process.
The carrier therefore lives in `carrier/` of this repository as a **dated
duplicate**: once its shape stabilizes, the target-independent parts (PE
mapper, trampoline generator, binding table, DWARF interop generator, policy
table format, trace/report formats, verdict records) move to
`port_forge/src/platform/win32` on branch `experimental/win32`, and
`config/platforms-v1.json` gains `win32`.

## 4. Import census and escalation policy

Census results (`notes/import_census.md`, 320 imports):

| class | count | meaning here |
|---|---:|---|
| DIRECT | 123 | IAT gets the real address (still counted through the trampoline) |
| WRAP | 162 | trampoline with tracing; no behaviour change |
| DETERMINISTIC | 31 | must be controlled for replay; see §5 for the minimal set |
| SHIM | 4 | the DirectX factories; cnc-ddraw already is the DirectDraw shim |
| UNKNOWN | 0 | |

89 % of the surface needs no reimplementation. The game's own OS surface
(imports called from game CUs, not Allegro/CRT) is 67 functions excluding the
libpng wrapper calls: CRT file and string functions, `rand`/`srand`, `qsort`,
QPC, `ShellExecuteA`, `LoadCursorA`, WSOCK32, `pthread_create`.

Escalation order, applied per import and only on evidence:

```text
DIRECT  →  trampoline (count/trace)  →  deterministic wrapper  →  shim  →  reimplement
```

Two boundaries the static census cannot see, both to be measured at runtime:

- **COM vtables.** DirectDraw/DirectSound/DirectInput work happens through
  vtable calls on host objects. Because the guest runs in-process, these calls
  need no proxy to *work*. A proxy is introduced only if (a) snapshot restore
  needs logical surface/buffer/device identity, or (b) first-divergence
  hunting needs the call stream. Method-offset map: census "COM boundary".
- **GetProcAddress.** Logged by the trampoline; census pending runtime evidence.

Pre-existing shim, not PortForge-owned: cnc-ddraw (`assets/ddraw.dll`,
https://github.com/FunkyFr3sh/cnc-ddraw). The carrier binds `DirectDrawCreate`
to it by default (`--ddraw=local`) because that is the oracle's configuration.

## 4a. Layering: compatibility below, determinism above

Compatibility ("make a 2000s API work on a modern GPU") and determinism
("make the game observe the same external world on every run") are different
problems and are solved in different layers. PortForge sits on the
game-facing side of every compatibility wrapper.

```text
game functions: ORIGINAL / LIFTED / NATIVE
        │
        ▼
PortForge-controlled boundary          record / replay / virtual time / sensors
        │
        ▼
compatibility backend                  Allegro drivers, cnc-ddraw, host DirectSound/DirectInput
        │
        ▼
modern Windows / hardware
```

Consequences already applied: cnc-ddraw is a backend, not a PortForge
component; deterministic replay must never depend on the presentation path
(renderer, refresh rate, vsync); the first graphics oracle is the game's
own off-screen `BITMAP` before `blit_to_screen` (a render-state comparison
above the backend), with host framebuffer comparison as a later, weaker
oracle; audio output is delegated, but any buffer/callback state visible to
game logic is carrier-controlled (none found so far: Allegro's mixer runs
in its timer thread and game code only issues `play_sample`-style calls,
INFERRED from the census).

Three clocks, kept separate:

| clock | owner | Icy Tower |
|---|---|---|
| host wall time | Windows | never read by game logic (KNOWN: `timeGetTime` only in Allegro's timer thread; QPC/clock/time in `play()` are telemetry) |
| game logical time | PortForge | the 20 ms tick count (0x506938) and the Allegro timer callbacks that feed it |
| presentation time | backend | when `blit_to_screen` reaches the screen; irrelevant to state |

Observable-time inventory (KNOWN, from imports + disassembly): `timeGetTime`,
`QueryPerformanceCounter/Frequency`, `Sleep`, `WaitForSingleObject`,
`WaitForMultipleObjects`, `MsgWaitForMultipleObjects`, `SetTimer` (one Allegro
housekeeping timer in the window proc), `clock`, `time`, Allegro
`install_int` callbacks. **Absent**: `GetTickCount`, `timeSetEvent`,
`timeBeginPeriod`, `RDTSC` (zero occurrences in the disassembly). No
calibrated busy loops found; the only wait loop is the tick wait in `play()`.

Escape hatch, evidence so far: the one place where "how much time passed"
becomes state is the catch-up branch in `play()` (several ticks processed
back-to-back when 0x506938 > 1). Under virtual time that count is
deterministic because ticks are delivered synchronously from the `Sleep`
wrapper. If replay divergence ever points at a race between the main loop
and an asynchronous callback, the next step is instrumenting yield points
and defining a scheduling budget, not CPU emulation. Single-core affinity is
available as a diagnostic mode only.

External-boundary census, generated from PE imports + DWARF + known library
code, classified by causal relevance rather than by API family:

| class | meaning | Icy Tower examples (from `notes/import_census.md`) |
|---|---|---|
| PURE / PASSTHROUGH | deterministic given its arguments; call host directly | CRT string/math, GDI object creation, `CloseHandle` |
| COMPATIBILITY | delegated to a backend that makes it work on modern hosts | `DirectDrawCreate` → cnc-ddraw; DirectSound/DirectInput → host |
| NONDETERMINISTIC | result or event recorded and replayed | tick delivery, key events, joystick poll, `rand`/`srand`, `time`/`clock`/QPC |
| SIDE EFFECT | filesystem/network/audio; virtualize, record, suppress, sandbox or compare | data files (read-only, hashed), profile/config/replay writes (compared), log.txt (compared), ad fetch (suppressed), audio (delegated) |
| UNKNOWN | instrument until causal relevance is known | `GetProcAddress` targets, COM vtable traffic |

This maps onto the DIRECT/WRAP/DETERMINISTIC/SHIM table in §4 but asks a
different question: not "what does the API do" but "can its result change
game state". The runtime trampoline census answers it per run.

Inner and outer replay:

```text
game-native replay (.itr)   optional workload, semantic clue, test corpus
PortForge carrier replay    generic whole-application deterministic machinery
```

The `.itr` system (KNOWN, `notes/replay_format.md`) is exactly the clean
boundary hoped for: keyboard and joystick are merged into one canonical byte
by `poll_control`, and `handle_player_input` consumes either that byte or the
recorded stream, so **input enters the simulation at one place**. The
developers considered `random_seed` + per-tick left/right/fire sufficient.
It covers gameplay only, not startup, menus, profiles or settings, and it
assumes identical timing, RNG and library behaviour. The carrier's own
record/replay therefore stays generic and complete (every NONDETERMINISTIC
boundary above), and the `.itr` corpus is used as a workload: play an `.itr`
through ORIGINAL, capture the per-tick oracle, then replay it with one
function switched to LIFTED and to NATIVE and compare. If the built-in
replay is not deterministic under the carrier, the first divergence names
the external channel the carrier still has to capture. This is a pilot
shortcut, not a PortForge principle; future targets will have no such
system.

## 5. Determinism model (HYPOTHESIS until milestone 7 proves it)

Sources of nondeterminism that reach game state, from the evidence:

| source | reaches game state via | control point |
|---|---|---|
| 20 ms timer thread | `cycle_counter` → 0x506938, consumed by `play()` | virtualize the Allegro timer thread: do not create it; deliver ticks synchronously from the main thread's `Sleep` wrapper (`rest()` is the only wait in the loop), calling Allegro's own `_handle_timer_tick` by symbol address |
| keyboard | DirectInput thread → `_handle_key_press/_release` → `key[]` | record at the Allegro key-event functions (symbol addresses), replay by injecting the same calls at tick boundaries; DirectInput thread parked in replay mode |
| joystick | `poll_joystick()` per frame → `joyGetPosEx` / IDirectInputDevice::GetDeviceState | record/replay the poll result; absent hardware = constant |
| RNG | msvcrt `rand`/`srand` from game code | wrap: record the seed, pin the LCG (msvcrt's `rand` is `x = x*214013+2531011; return (x>>16)&0x7fff`) so replay does not depend on the host msvcrt |
| wall clock | `time`, `clock`, QPC in `play()`, `localtime` | wrap with virtual clock; matters for save-file bytes and statistics, not gameplay (being verified) |
| ad fetch | separate thread, results only feed ad display | stub in replay mode |
| menu/profile/character choice | keyboard + files | covered by the input record and a fixed assets snapshot |

The game's own replay system (`replay.c`) records 8 bytes per tick; if its
record is exactly the control.c input state, it doubles as an independent
oracle for our input recording (see `notes/replay_format.md`).

Replay identity, in PortForge terms (pf/41): the heartbeat is the game tick
(one consumption of 0x506938 at 0x4124f4); input coordinates are
`tick:N`; forwarded host calls whose results reach game state are recorded
events, never silent passthrough (capsule §J.3).

### 5a. Input source is an exclusive policy (requirement, 2026-09-07)

Observed during milestone 6 work: scripted events and the real keyboard both
reached Allegro's key state at the same time. Mixing is a defect. The carrier
must own the input source as one exclusive setting:

```text
--input=real     real keyboard/joystick only; recording allowed
--input=script   scripted events only; the DirectInput path is parked
--input=none     no input (diagnostic)
```

Every key event delivered to the game is attributable to exactly one source,
and the recorder logs the source with the event. This generalizes: each
NONDETERMINISTIC channel (time, input, RNG, network) has exactly one active
provider per run, host or carrier, never both.

## 6. Snapshot model (HYPOTHESIS)

Safepoint: VA 0x4124f4, once per tick, no host call in flight on the main
thread. Other threads are either parked (timer, input in deterministic mode)
or stateless with respect to game logic (window thread pumps messages).

```text
guest state
  main-thread registers at the safepoint (ESP/EBP into the guest stack)
  image pages 0x400000..0x78c000 (.data/.bss; .text unchanged)
  guest heap  (msvcrt malloc family is imported → redirect to a fixed-address
              arena so heap contents are ordinary guest pages)   [when needed]
  guest stack (fixed address region)

PortForge-owned externalized state
  virtual time, replay cursor, RNG state
  logical file table: path, mode, offset (for files open across ticks)
  Allegro-visible resource identities that hold host handles:
    DirectDraw surfaces, DirectSound buffers, DirectInput devices, HWND,
    GDI objects, critical sections/events
```

Restore strategy, in escalation order: (1) restore guest pages and re-enter
at the safepoint with host objects still alive in the same process
("in-process rewind" — valid as long as the host objects the guest references
still exist and their state is re-synchronized, e.g. surfaces are redrawn on
the next frame anyway); (2) reopen files and re-seek from the logical table;
(3) only if a resource cannot be re-synchronized, introduce a proxy that owns
its logical state. Raw host handles and COM pointers are **not** durable
state; they are re-bound.

Heap: the msvcrt `malloc`/`calloc`/`realloc`/`free` imports are the single
control point. libpng3/zlib/pthread allocate host-side memory, but only
transiently (PNG decode) or opaquely (one mutex); recorded as INFERRED, to be
checked by the import trace.

## 7. Verification model

Oracle = the original code running inside the carrier with recording on.
Candidate = same carrier with one function redirected to native C.

```text
snapshot at tick N (or cold start) + recorded inputs
        ├── carrier: original function
        └── carrier: native replacement
compare per tick: guest .data/.bss/heap digests, registers at the safepoint,
                  import call stream (counts + args), frame image digest
first difference → named (address, byte, tick), never a percentage
```

Per-function granularity: hook the promoted function's entry/return
(prologue patch or IAT-style dispatch by symbol) and compare its post-state
against the original executed from the same pre-state; this catches state the
game overwrites before the next tick (pf/72, pf/89 in capsule §E). Negative
control: inject a one-byte fault into one role and require the comparator to
name it at that tick.

## 7a. Where the clean code lives, and how it stays clean (2026-09-07)

`src/` is the clean port, following the sibling projects' convention
(aladdin_forged/src/README.md): recovered semantics only, **no PortForge
type, no carrier header, no guest address**. The carrier, the lifter output
and the generated interop never live there.

```text
src/icytower/*.c, *.h      readable C; globals declared as ordinary externs
                           (`extern int reward_scale; extern Tplayer ply[];`),
                           types in the port's own headers (initially
                           recovered from DWARF, then owned by the port)

carrier/gen/pf_bindings.h  GENERATED: maps each extern name the port uses to
                           its original address (`#define reward_scale
                           (*(int*)0x5069xx)`), forced-included (`/FI`) only
                           when src/ is compiled INTO the carrier

carrier binding table      original function address → native_<name>
                           (5-byte entry patch); callers never change
```

The same source therefore compiles in two worlds: inside the carrier it
operates on the original memory at the original addresses (state stays
address-backed, code ownership migrates first); standalone, a `state.c`
defines the globals and the bindings header is absent (state ownership
migrates later, per function group, on its own evidence). A purity check
(pattern: aladdin's `scripts/check_native_layer.py`) refuses any literal
guest address or carrier include in `src/`, and is the tier-0 gate for
promotion. The offline oracle (unicorn on the original bytes,
`carrier/lift/harness`) verifies a `src/` function before it is bound in
vivo; the replay comparison verifies it after.

## 8. Milestones and status

| # | milestone | status |
|---|---|---|
| 1 | executable/runtime surface documented | done — `notes/` |
| 2 | smallest carrier skeleton (loader, IAT, trampolines, VEH) | done — `carrier/`, NOTES.md |
| 3 | original code executes through the carrier | done — run1 |
| 4 | visible startup/menu ("MAIN MENU LOOP" in log.txt) | done — run1, log matches baseline except divergence 001 |
| 5 | gameplay reached | done — `carrier/scripts/newgame.txt`, `carrier/NOTES.md` "Milestones 5-7" |
| 6 | time/input/RNG instrumented for determinism | done — `carrier/src/det.hpp`/`det.cpp` (`--det`), `carrier/NOTES.md` |
| 7 | record and replay a short gameplay sequence, digest-equal | done — `carrier/scripts/compare_digests.py`: EQUAL across 3 `--det --pace=fast` runs; negative control diverges at the moved tick; non-`--det` runs diverge — `carrier/NOTES.md` |
| 8 | safepoint snapshot at 0x4124f4 and restore | pending |
| 9 | inspection/tracing from a snapshot | pending |
| 9a | presentation-independent frame oracle (digest of the game's back buffer at `blit_to_screen`) and headless run (Allegro GDI driver into a hidden window, no sound device) | planned; feasibility: the game selects Allegro's config via `set_config_file` at 0x40f03a, so the driver choice can be overridden by an argument sensor at `set_gfx_mode` or `override_config_file`; a window handle is still required by Win32 (hidden, not absent) |
| 10 | pick one small exercised game function | pending |
| 11a | LIFTED form: generate C from its bytes, bind it, verify replay-equal | done — `update_frame` and `jump_player` bound at their original VAs and replay-equal to ORIGINAL (per-invocation and per-tick); `carrier/NOTES.md` "Milestones 11-12" |
| 11b | NATIVE form: readable C, bind it, verify replay-equal | done — `update_frame` bound and replay-equal; `is_solid`'s NATIVE form binds but this workload never reaches it (gap recorded) |
| 12 | automatic comparison of all three forms from equivalent state, with negative control | done — `carrier/src/bind.cpp` + `carrier/scripts/compare_fn_digests.py`: ORIGINAL/LIFTED/NATIVE all EQUAL (877 invocations, 876 ticks); `--fault-inject update_frame:k=300` is named as `k=300 field=post` and nothing earlier |

## 8a. Migration map (metrics reported by the carrier)

| metric | source |
|---|---|
| game functions by form: ORIGINAL / LIFTED / NATIVE | binding table |
| original .text bytes still executed per replay | entry-stub counters + coverage |
| ORIGINAL→NATIVE and NATIVE→ORIGINAL crossings per replay | stub counters, typed-thunk counters |
| globals still address-backed (all, until state migration starts) | generated globals header |
| imports not yet modelled for determinism | policy table vs runtime census |
| unmodelled host interactions seen at runtime (GetProcAddress, COM) | trampoline trace |
| replay divergence point (tick, address) for each candidate | comparator verdict |
| hand-written lines vs generated lines in the carrier | build report |

## 9. What is deliberately not built

- No Win32 emulation, no DLL reimplementation, no DirectX reimplementation.
- No whole-program x86 lifter; lifting is per function and verified per function.
- No generic thread scheduler; only the two Allegro threads that reach game
  state are virtualized, by entry point.
- No logical-handle layer until restore proves a resource cannot be re-bound.
- No arbitrary-instruction snapshots; tick safepoints only.

## 10. Rejected approaches

| approach | why rejected |
|---|---|
| whole-program x86→C recompilation as the bootstrap carrier (original note §5.2) | lifter + verification cost dominates before any oracle exists; replaced by per-function LIFTED form verified against ORIGINAL (§3) |
| PortForge `Interp32` as the executor | DOS4GW-shaped service model (INT dispatch), no TEB/FS model, single-threaded; useful only as an offline per-function reference |
| proxying every DirectX COM object up front | no evidence it is needed for execution; deferred to snapshot/divergence evidence |
| controlling every DETERMINISTIC import | only five sources reach game state (§5); the rest is Allegro plumbing whose timing is never read back |

## 11. Reusable vs Icy-Tower-specific (running tally)

Reusable (candidate `port_forge/src/platform/win32`): PE mapper, IAT
resolver + generated trampolines, prototype-free import tracer/counter,
policy table format, VEH diagnostics with symbol resolution, fixed guest
stack, virtual clock wrapper, rand pinning, malloc arena, safepoint snapshot
codec, tick-indexed input script, per-tick comparator/verdict.

Icy-Tower-specific: symbol addresses (0x506938, 0x4124f4, Allegro internal
entry points), the policy table contents, the assets snapshot, cnc-ddraw
binding, the ad-fetch stub.
