
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
