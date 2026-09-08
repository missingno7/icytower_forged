
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
