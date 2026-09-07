# `pf_lift` — the LIFTED form generator

Status: **working pilot**, 2026-09-07. This is milestone 11a of
`win32_pilot.md` §8 for three functions: *generate C from the original bytes*
and *verify it offline against the original bytes*. Binding it into the running
carrier (the 5-byte entry patch) is still pending and is not claimed here.

Labels as in `win32_pilot.md`: KNOWN = read from the binary or observed in a
run; INFERRED = reasoned; HYPOTHESIS = a design bet awaiting evidence.

```
pf_lift.py                 the lifter (CLI)
lifted/pf_rt.h             generated runtime header (the only shared code)
lifted/lifted_<f>.c        generated C, one per function
lifted/lifted_<f>.json     metadata: blocks, instructions, refusals,
                           external calls, memory ranges + symbolic names
harness/lift_check.py      offline equivalence check (unicorn = ORIGINAL side)
harness/lift_check.c       the LIFTED side of the same check
harness/pf_harness_mem.h   the one macro that differs between carrier and check
build_check.cmd            `cl /c /W3` of the three generated files
```

## 1. Usage

```
python pf_lift.py --image ..\..\assets\icytower15.exe \
                  --functions ..\..\artifacts\functions.json \
                  --interop ..\gen --func update_frame --out lifted
```

`--func` also takes `0x406ac4`. Every run rewrites `lifted/pf_rt.h`, one
`lifted_<name>.c` and one `lifted_<name>.json`.

Compile check (produces the numbers in §7):

```
build_check.cmd                     rem cl /c /W3 /TC /I..\..\gen
harness\build.cmd                   rem builds harness\lift_check.exe
python harness\lift_check.py        rem runs the offline equivalence check
```

## 2. What is generated

`<ret> __cdecl lifted_<name>(<params>)` with **exactly** the prototype
`PFN_<name>` in `carrier/gen/it_funcs.h` (the generator refuses if the interop
VA and the requested VA disagree), so the carrier can later bind it at the
original address with the entry patch and no caller changes.

Every generated file carries a DO-NOT-EDIT header naming the generator, the
image SHA-256 (`7570c6b0…4388d`), the function VA and byte size, and the
instruction/block counts actually lifted.

- **Memory is the original memory.** Every memory operand becomes a plain
  typed access at the computed address, through the single seam `PF_MEM(addr)`,
  which is the identity by default. No memory abstraction, no CPU struct.
- **Registers are C locals** (`unsigned int r_eax…r_edi`). 8/16-bit parts go
  through `PF_GET8L/PF_GET8H/PF_GET16` and `PF_SET*`.
- **Only the locals actually used are declared**, so `/W3` stays silent.

## 3. Frame model (ESP/EBP are symbolic, never values)

`push ebp; mov ebp,esp; sub esp,N` is recognised and ESP/EBP are then tracked
as *offsets* relative to the incoming stack pointer (frame offset 0 = the
return-address slot):

| frame offset | meaning | lowering |
|---|---|---|
| `>= 4` | incoming cdecl argument, byte `off-4` of the argument block | `PF_A32(k)` — read from `pf_arg`, a byte-exact `memcpy` copy of the parameters |
| `[0,4)` | return address | **refusal** |
| `< 0` | locals, saved registers, outgoing pushes | `PF_S32(k)` — a real scratch array `pf_stk` |

`push`/`pop` of ordinary registers really store to and load from `pf_stk`, so
GCC's callee-save prologue round-trips without being special-cased; only
`push ebp` / `pop ebp` / `leave` are symbolic. The (ESP, EBP, saved-EBP-slot)
triple is propagated over the CFG and **any disagreement at a block merge, any
use of ESP/EBP as a value or index, and any `ret` with a non-zero ESP offset is
a refusal.** The scratch array is sized from the deepest offset reached
(16 bytes for `is_solid`, 8 for the other two) and zeroed on entry.

Writing to an incoming argument slot is a **refusal**: `pf_arg` is a copy, so
the caller's slot would not be updated, and pf_lift will not lie about that.

## 4. Flags

Computed *only where consumed*. Each flag-defining instruction gets a static
kind (`sub`, `add`, `logic`, `sahf`, `fcomi`, or `undef`) and materialises at
most `pf_fa`, `pf_fb`, `pf_fres`; a forward dataflow pass over the CFG resolves
which definition reaches each `jcc`/`setcc`, and definitions that reach no
consumer emit nothing at all. Conditions expand exactly:

```
ZF (r==0)   SF (r>>31)&1
sub: CF (a<b unsigned)   OF ((a^b)&(a^r))>>31
add: CF (r<a)            OF ((a^r)&(b^r))>>31
logic: CF=OF=0        neg == sub(0,src)      inc/dec: CF stays undefined
signed:   g = !ZF && SF==OF     l = SF!=OF     le = ZF || SF!=OF
unsigned: a = !CF && !ZF        b = CF         be = CF || ZF
```

Refusals here are loud, not approximate: no reaching definition, two reaching
definitions of different kinds, a condition needing a flag the definition
leaves undefined (`jg` after `sahf`, `ja` after `inc`, anything after a shift
or a divide), or a parity/overflow condition code.

## 5. Control flow, calls, imports

Basic blocks are reconstructed from branch targets; blocks unreachable from the
entry (GCC's inter-block `nop` / `lea esi,[esi]` padding — 2 blocks in
`is_solid`, 4 in `jump_player`) are listed in the JSON and not emitted. Every
fall-through becomes an explicit `goto`, so the emitted control flow does not
depend on block layout, and a label is only emitted where something jumps to it.
**Indirect `jmp` (switch tables) is a refusal** — none of the three candidates
has one, matching `notes/promotion_candidates.md` §1.

Calls are real C calls, because the guest and the lifted code share the process:

- `call 0x4xxxxx` → a typed call through the `PFN_<callee>` typedef from
  `it_funcs.h` at the original address, with arguments read from the modelled
  stack pushes that precede the call.
- `call dword ptr [0x514xxx]` → `(*(T (__cdecl **)(…))0x514xxx)(…)` with the
  prototype taken from `IMPORT_PROTOS` in `pf_lift.py` (a small hand table:
  `rand`, `srand`, `abs`, `sin`, `cos`, `tan`, `atan2`, `sqrt`, `pow`, `floor`,
  `ceil`, `fabs`, `memset`, `memcpy`, `strlen`); the IAT slot → name map comes
  from pefile. An import that is not in the table is a refusal.

> **Honesty note.** *None of the three pilot candidates contains a single
> `call`* (`imports_used = []`, `indirect_calls = 0`, per
> `notes/promotion_candidates.md` §2). The call-lowering path above therefore
> has **zero test coverage**; every call site it emits is preceded by a
> `/* UNVERIFIED PATH */` comment, and it must be exercised (e.g. on
> `play_jump_sound`, which calls `play_sound`) before it is trusted.

## 6. x87 and the HYPOTHESIS

The FPU is an 8-entry array plus a top index, both ordinary function locals
(`pf_x87_t pf_fr[8]; unsigned pf_ftop, pf_fsw;`), so a lifted function stays
re-entrant without a CPU struct. Register-form x87 opcodes are decoded from the
**instruction bytes**, not from capstone's operand list, because capstone
renders `D8 C1` (`ST0 = ST0+ST1`) and `DC C1` (`ST1 = ST1+ST0`) with the same
single-operand text.

Modelled: `fld`/`fst`/`fstp` (m32fp, m64fp, `st(i)`), `fild` (m16/m32/m64),
`fist`/`fistp` (m16/m32), `fadd/fmul/fsub/fsubr/fdiv/fdivr` in the D8/DC memory
and register forms and the DE popping forms, `fcom(p)`, `fcompp`, `fucom(p)`,
`fucompp`, `fcomi/fucomi/fcomip/fucomip`, `fchs`, `fabs`, `fldz`, `fld1`,
`fsqrt`, `ftst`, `fxch`, `ffree`, `fninit`, `fnstsw ax`.

The GCC 4.4 float-compare idiom is modelled exactly. `pf_fcmp` returns the
status-word condition bits the hardware sets (`ST0>src`→0, `<`→C0=0x0100,
`=`→C3=0x4000, unordered→C3|C2|C0=0x4500); `fnstsw ax` writes those plus TOP
into AX; `test ah,0x45` is then `ZF ⇔ ST0 > src` and `test ah,5` is
`ZF ⇔ ST0 ≥ src and ordered`. `sahf` and the FCOMI EFLAGS form are modelled
too (with SF/OF left undefined, so a signed condition after them is a refusal).

`FIST`/`FISTP` use round-to-nearest-even; **`fnstcw`/`fldcw` are refused**, so a
function that switches the rounding mode (e.g. `line_intersect`) cannot reach
that code path silently.

### The bet

`pf_x87_t` is a single `typedef double` in `lifted/pf_rt.h`. That typedef *is*
the HYPOTHESIS of `win32_pilot.md` §3; swapping it for a software 80-bit type
needs no change to any generated file.

### Result for `jump_player` (KNOWN)

**No divergence in 80 000 vectors, and none is possible for this function.**

The oracle is unicorn, i.e. QEMU's x87, which carries real 80-bit `floatx80`
intermediates — that is what makes the question answerable offline at all.
Over 20 000 vectors on each of four seeds, comparing EAX and all 184
bytes of `Tplayer` bit-for-bit, the `double` model matched the 80-bit oracle on
every vector, including NaNs, ±inf, denormals, `DBL_MAX`, `DBL_MAX/2` and
random 64-bit patterns for `sx`.

Why (KNOWN, from the constants in the image): the only FP *arithmetic* in
`jump_player` is `fadd st(1)` (`sx+sx`) and `fmul dword ptr [0x4d7130]` where
`0x4d7130` is exactly `-2.0f`. Both are scalings by a power of two, i.e. exact
in binary floating point, in both 64- and 80-bit. Everything else is a load, a
compare against exact values (`max_speed[]` doubles 12.0/12.2, the `-22.0f` at
`0x4d7134`), a store, or an integer path (`fild`/`fstp` for the forced-jump
branch). The only place where the two could part is `sx+sx` overflowing the
double range while staying inside the 80-bit range — and the result is stored
back with `fst qword`, which rounds `±3.59e308` to `±inf` anyway, so the stored
bits agree. The 8 covered paths (forced jump 1200, early return 934,
`sx≥0`/`sx<0` × clamp/no-clamp × rotate, 213–388 each in the default 4 000-vector
run) all match.

**Conclusion for the pilot: `double` is sufficient for `jump_player`, so this
function does *not* discriminate the hypothesis.** The bet is still open. The
escalation target named by `notes/promotion_candidates.md` §6 —
`line_intersect` (0x406b80, 37 x87 instructions, a real `fnstcw`/`fldcw`-guarded
`fistpl`, and genuine multiplies/divides whose intermediates are *not* powers of
two) — is where the hypothesis can actually fail. pf_lift currently refuses it
(see §8), so testing the hypothesis properly needs `imul`, `fnstcw`/`fldcw` and
a control-word-aware `pf_rint` first.

## 7. Offline equivalence check (no game, no carrier)

Both sides run against the **same flat guest address space** `0x400000..0x800000`,
built once from the PE image:

| side | executor | memory |
|---|---|---|
| ORIGINAL | the original bytes in **unicorn** (real 80-bit x87) | the image mapped at 0x400000 |
| LIFTED | the generated C compiled by 32-bit MSVC into `harness/lift_check.exe` | the same image bytes in an in-process copy, reached through `PF_MEM` |

The chosen route is the cheap one named in the task: `harness/pf_harness_mem.h`
is force-included (`cl /FIpf_harness_mem.h`) and redefines `PF_MEM(a)` to
`pf_guest + (a - 0x400000)`. No `VirtualAlloc` at 0x400000, no second emulator.
The **generated .c files are byte-identical in both configurations** — only that
one macro differs. Pointer arguments are guest VAs (`Tplayer *` = 0x790000),
never host pointers, which is sound because the lifted code never dereferences a
C pointer; it funnels every address through `PF_MEM`.

Per vector we compare the return value and the raw bytes of the comparison
domain from `notes/promotion_candidates.md` §4 and report `EQUAL` or the first
differing byte, named as *(vector, field+offset, VA, original byte, lifted byte)*.

| function | vectors | domain compared | result |
|---|---:|---|---|
| `update_frame` | 20 000 | `reward_time` 4B + `reward_scale` 4B + `Tplayer` 184B (no return value) | **EQUAL** |
| `is_solid` | 20 000 | EAX + `Tmap` 772B (must be unchanged — negative control) | **EQUAL** |
| `jump_player` | 20 000 (+3 more seeds × 20 000) | EAX + `Tplayer` 184B, bit-exact | **EQUAL** |

Negative control (`--fault FUNC:VECTOR:BYTE` flips one bit of the lifted result):
the comparator names it exactly, e.g.
`DIFFER at vector 11 … Tplayer+0x20 (VA 0x00790020) original 0xff lifted 0xfe`.

The check found one real bug in the lifter before it passed: `PF_PUSH(v)` moved
the top index before evaluating `v`, so `fld st(0)` pushed the wrong register.
`jump_player` vector 13 (`sx = -11.0`) named it at `Tplayer+0x1e`: `sy` was
`-12.0` instead of `-22.0`.

Caveat, stated rather than hidden: the oracle restores only the union of the
comparison domain and the per-vector write regions between vectors. That is
sufficient because these three functions provably write nowhere else (verified
by reading every store in the disassembly), and it is *not* a general property —
a function with wider writes needs a full image restore on the oracle side too
(the lifted side already does a full restore per vector).

## 8. Refusals (KNOWN — this is the complete list)

Any instruction, operand form or pattern below aborts the lift with the address
and the mnemonic, writes the refusal into `lifted_<name>.json` and exits 1.
There is no silent fallback anywhere.

**Decoding / layout**: capstone cannot decode; an instruction crosses the end of
the function; a branch target outside `[va, va+size)`.

**Instructions**: anything not listed in §5/§6 — notably `imul`, `mul`, `adc`,
`sbb`, `cmov*`, `loop*`, `jecxz`, string operations, `xchg`, `bswap`, all SSE,
`fnstcw`/`fldcw`/`fnsave`/`frstor`, x87 with a prefix, and `hlt`. Observed:
`line_intersect` (0x406b80) refuses at `0x00406ba5 (imul esi, ebx)`.

**Control flow**: indirect `jmp` (switch table) or indirect `jcc`; falling off
the end of the function.

**Frame**: ESP/EBP used as a value or as an index register; a non-immediate ESP
adjustment; `lea` of a frame address (the frame is not at its original address);
a second `push ebp`; `pop ebp`/`leave` not matching the saved slot; `ret` with a
non-zero ESP offset; a merge point where two predecessors disagree on the stack
state; access to the return-address slot or below the modelled frame; a **write**
to an incoming argument slot.

**Flags**: no reaching definition; conflicting reaching definitions; a condition
that needs a flag the reaching definition leaves undefined; an unsupported
condition code (`p`, `np`, `o`, `no`).

**Types / ABI**: a parameter or return type whose stack size pf_lift cannot
prove (only 4-byte scalars/pointers, 8-byte `double`/`long long`, and `void` are
proven); a return type that is not void/int-like/pointer/`long long`/floating.

**Calls**: a direct call whose target has no `PFN_` typedef in the interop
header; an indirect call that is not an IAT slot; an IAT import missing from
`IMPORT_PROTOS`; a call argument that is not a 4-byte slot.

**Run time** (the generated code, not the lifter): `PF_TRAP(va, why)` on `#DE` —
division by zero or a quotient that does not fit in 32 bits. The carrier must
provide `void pf_trap(unsigned, const char*)`; the harness does.

## 9. Known dependencies and non-claims

- `(int)x >> n` is used for `sar`. C leaves this implementation-defined; MSVC
  documents an arithmetic shift, and the equivalence check confirms it for these
  functions on this compiler. A different compiler needs re-verification.
- `pf_fcmp` writes only the C0/C2/C3 bits plus TOP into the status word. No
  other status-word bit (exception flags, C1) is modelled; nothing in these
  three functions reads one.
- The x87 **tag word** is not modelled (`ffree` is a comment); stack
  overflow/underflow does not fault the way hardware would.
- The lifted objects have **not** been bound into the running carrier yet. This
  deliverable is milestone 11a's *generation* and *offline verification* only;
  replay-equality inside the carrier (`win32_pilot.md` §7) is the next step.
- FPU control-word fidelity is untested, by construction: any function that
  touches it is refused.

## 10. Line counts

| file | lines | kind |
|---|---:|---|
| `pf_lift.py` | 1561 | hand-written lifter (incl. the 135-line `pf_rt.h` template it emits) |
| `harness/lift_check.py` | 370 | hand-written harness (ORIGINAL side + diff) |
| `harness/lift_check.c` | 123 | hand-written harness (LIFTED side) |
| `harness/pf_harness_mem.h` | 24 | hand-written |
| **hand-written total** | **2078** | |
| `lifted/pf_rt.h` | 135 | generated |
| `lifted/lifted_update_frame.c` | 179 | generated (40 instructions, 14 blocks) |
| `lifted/lifted_is_solid.c` | 162 | generated (44 instructions, 8 of 10 blocks) |
| `lifted/lifted_jump_player.c` | 234 | generated (68 instructions, 14 of 18 blocks) |
| **generated total** | **710** | for 425 bytes of original x86 |

Roughly 40 % of each generated file is the original disassembly reproduced as
comments, one instruction per lifted statement group, which is what makes the
output reviewable against `artifacts/disasm.txt` by eye.

`pf_lift.py` is over the 1200-line target the task set. The overage is
concentrated in three places that were not optional: the byte-level x87 decoder
(~190 lines, needed because capstone's operand list is ambiguous for D8/DC), the
ESP/EBP frame propagation with its refusals (~120 lines), and the emitted
runtime template (135 lines). The call-lowering path (~60 lines) is the one part
that could be deleted today without losing anything the pilot verifies.
