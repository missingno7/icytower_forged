# `pf_lift` — the LIFTED form generator

Status: **working pilot**, 2026-09-07. This is milestone 11a of
`win32_pilot.md` §8 for four functions: *generate C from the original bytes*
and *verify it offline against the original bytes*. Binding it into the running
carrier (the 5-byte entry patch) is still pending and is not claimed here.

The fourth function, `line_intersect` (0x406b80), was added to answer the x87
question of `win32_pilot.md` §3, and it does: **`pf_x87_t = double` is not
enough** — see §6b and `artifacts/lift_x87_finding.md`.

Labels as in `win32_pilot.md`: KNOWN = read from the binary or observed in a
run; INFERRED = reasoned; HYPOTHESIS = a design bet awaiting evidence.

```
pf_lift.py                 the lifter (CLI)
lifted/pf_rt.h             generated runtime header (the only shared code)
lifted/pf_x87_soft.h       generated software 80-bit x87 backend (-DPF_X87_SOFT)
lifted/lifted_<f>.c        generated C, one per function
lifted/lifted_<f>.json     metadata: blocks, instructions, refusals,
                           external calls, memory ranges + symbolic names
harness/lift_check.py      offline equivalence check (unicorn = ORIGINAL side)
harness/lift_check.c       the LIFTED side of the same check
harness/pf_harness_mem.h   the one macro that differs between carrier and check
harness/build.cmd          builds harness/lift_check.exe      (pf_x87_t = double)
harness/build_soft.cmd     builds harness/lift_check_soft.exe (software 80-bit)
                           plus harness/x87_soft_selftest.exe
harness/x87_soft_selftest.c/.py
                           differential self-test of the software backend
                           against exact rational arithmetic (no oracle)
harness/x87_cw_probe.py    standalone: why the x87 answer depends on the FPU
                           control word (artifacts/lift_x87_finding.md §2)
build_check.cmd            `cl /c /W3` of the four generated files
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
harness\build_soft.cmd              rem the same sources, -DPF_X87_SOFT
python harness\lift_check.py        rem runs the offline equivalence check
python harness\lift_check.py --exe harness\lift_check_soft.exe \
       --vectors 50000 --seed 1 --census      rem the 80-bit backend
python harness\x87_soft_selftest.py          rem the softfloat vs exact maths
python harness\x87_cw_probe.py               rem the control-word experiment
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

The `.json` also records the three facts the carrier needs to choose an x87
backend for that function (§6b): `x87_arith_ops`, `x87_fistp_ops` and
`x87_control_word_used`. Today: `update_frame` 0/0/false, `is_solid` 0/0/false,
`jump_player` 3/0/false, `line_intersect` 6/2/**true**.

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
(40 bytes for `line_intersect`, 16 for `is_solid`, 8 for the other two) and
zeroed on entry. "Deepest offset reached" now means *inside* a block too: the
`push edi` / `fidivr [esp]` / `add esp,4` idiom in `line_intersect` dips 4 bytes
below every block-entry ESP, and sizing the frame from block entries alone made
that push a spurious refusal.

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
**Indirect `jmp` (switch tables) is a refusal** — none of the four candidates
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

> **Honesty note.** *None of the four pilot candidates contains a single
> `call`* (`imports_used = []`, `indirect_calls = 0`, per
> `notes/promotion_candidates.md` §2). The call-lowering path above therefore
> has **zero test coverage**; every call site it emits is preceded by a
> `/* UNVERIFIED PATH */` comment, and it must be exercised (e.g. on
> `play_jump_sound`, which calls `play_sound`) before it is trusted.

## 6. x87, the control word, and the HYPOTHESIS (now decided)

The FPU is an 8-entry array plus a top index and a control word, all ordinary
function locals (`pf_x87_t pf_fr[8]; unsigned pf_ftop, pf_fsw, pf_fcw;`), so a
lifted function stays re-entrant without a CPU struct. Register-form x87 opcodes
are decoded from the **instruction bytes**, not from capstone's operand list,
because capstone renders `D8 C1` (`ST0 = ST0+ST1`) and `DC C1`
(`ST1 = ST1+ST0`) with the same single-operand text.

Modelled: `fld`/`fst`/`fstp` (m32fp, m64fp, `st(i)`), `fild` (m16/m32/m64),
`fist`/`fistp` (m16/m32), `fadd/fmul/fsub/fsubr/fdiv/fdivr` in the D8/DC memory
and register forms and the DE popping forms, the **integer** memory forms
`fiadd/fimul/fisub/fisubr/fidiv/fidivr/ficom/ficomp` (DA = m32int,
DE = m16int), `fcom(p)`, `fcompp`, `fucom(p)`, `fucompp`, `fcomi/fucomi/
fcomip/fucomip`, `fchs`, `fabs`, `fldz`, `fld1`, `ftst`, `fxch`, `ffree`,
`fninit`, `fnstsw ax`, and **`fnstcw`/`fldcw`**.

The GCC 4.4 float-compare idiom is modelled exactly. `pf_fcmp` returns the
status-word condition bits the hardware sets (`ST0>src`→0, `<`→C0=0x0100,
`=`→C3=0x4000, unordered→C3|C2|C0=0x4500); `fnstsw ax` writes those plus TOP
into AX; `test ah,0x45` is then `ZF ⇔ ST0 > src` and `test ah,5` is
`ZF ⇔ ST0 ≥ src and ordered`. `sahf` and the FCOMI EFLAGS form are modelled
too (with SF/OF left undefined, so a signed condition after them is a refusal).

### 6a. The control word (KNOWN)

`pf_fcw` starts at `PF_CW_INIT = 0x037F` and is a real modelled value:

- **KNOWN**: `___mingw_CRTStartup` (0x401020) calls `__fpreset` (0x4b2850),
  which is a bare `FNINIT`. `FNINIT` leaves `CW = 0x037F` — **PC = 11, a 64-bit
  significand** (full extended precision), RC = 00 (nearest-even). That is *not*
  the MSVC/CRT `0x027F` (53-bit significand).
- GCC 4.4 casts a float to int with
  `fnstcw save; ax=save; ah=0x0C; fldcw trunc; fistp; fldcw save`. `pf_lift`
  models `fnstcw`/`fldcw` as reads and writes of `pf_fcw`, and `PF_TOI32/
  PF_TOI16` consult its RC field, so `fistp` really truncates inside that idiom
  and really rounds-to-nearest outside it. All four rounding modes are
  implemented; NaN and out-of-range sources give the integer indefinite
  (`0x80000000` / `0x8000`), as masked hardware does.
- Arithmetic under a *non-default* PC/RC is **not** modelled. In any function
  containing an `FLDCW`, pf_lift emits `PF_CW_ARITH(va, pf_fcw)` before every
  rounding x87 operation; it is a hard `PF_TRAP` if PC/RC are not the FNINIT
  defaults. `line_intersect` never fires it (the truncating word is live only
  across the `fistp` itself), so this is a guard, not a modelled path.

### 6b. The bet, and its result (KNOWN — the hypothesis is settled)

`pf_x87_t` and every operation on it go through one macro API
(`PF_ADD/PF_SUB/PF_MUL/PF_DIV/PF_FI32/PF_FF64/PF_TOI32/PF_TOF64/pf_fcmp/…`).
Two implementations sit behind it, chosen at compile time, and **no generated
`.c` file changes between them**:

| backend | selected by | `pf_x87_t` |
|---|---|---|
| the original HYPOTHESIS | default | `double` |
| the fallback of `win32_pilot.md` §3 | `-DPF_X87_SOFT` | software 80-bit extended (`pf_x87_soft.h`, 434 lines, 370 of them code) |

**`jump_player` cannot decide the bet** (unchanged from the previous version of
this file): its only FP arithmetic is `sx+sx` and `×(-2.0f)`, both exact powers
of two, and every result is stored back as a `double`. 200 000 vectors on each
backend, EQUAL, and no divergence is *possible*.

**`line_intersect` (0x406b80) decides it, against `double`.** It divides
(`fidivr` twice), multiplies by non-powers of two (`fimul`), and keeps **every
intermediate in the x87 register stack** — nothing is spilled to memory as a
`float` or a `double` — before truncating with `fistp` into a screen coordinate.

```
50 000 vectors/seed, 4 seeds, EAX + *px + *py compared bit-for-bit
  pf_x87_t = double     DIFFER  107 / 103 / 105 / 128 vectors of 50 000
  software 80-bit       EQUAL   on all four seeds (200 000 vectors)
```

First difference of the default seed, named exactly by the comparator:

```
[line_intersect/LIFTED] DIFFER at vector 227 (107 of 50000 vectors differ):
  at '*py+0x0 (VA 0x00794004)'  original 0xa3  lifted 0xa4
  line_intersect(-484594473, 3591436, 385321175, 733949708, 0, 0, 0, 1, ...)
  ua = N1/D = 3993/7168;  exact ua*dy1 + 0.5 = 406852760 exactly
  80-bit quotient is 2.32e-20 BELOW exact  -> fistp truncates to 406852759
  double quotient is 1.59e-17 ABOVE exact  -> fistp truncates to 406852760
```

The intermediate that loses the precision is the **quotient held in ST**, not a
double-rounding on a store; the `fimul` by `dy1 ≈ 7.3e8` and the truncation are
what make a 10⁻¹⁷ relative error visible as a whole-pixel difference. Full
write-up, including the second-order finding that **unicorn powers up with
`FPCW = 0x0000` (PC = *single* precision)** and therefore must be given
`FNINIT; FLDCW 0x037F` before every call, is in
`artifacts/lift_x87_finding.md`; `harness/x87_cw_probe.py` reproduces the
control-word sensitivity in six vectors.

The software backend is also checked **without** the oracle:
`harness/x87_soft_selftest.py` runs 40 000 random `add`/`sub`/`mul`/`div` pairs
(int32-valued doubles, huge x small, and arbitrary finite doubles) through it and
requires the packed (sign, biased exponent, 64-bit significand) triple to equal
the exact rational result rounded to nearest-even at 64 bits. **39 966
operations checked, 0 mismatches.** That separates "the softfloat is right" from
"the softfloat agrees with unicorn".

Consequence: **`double` is not the default any lifted game function should be
trusted with** unless its x87 arithmetic is provably exact or immediately
rounded to `double` by a store.

## 7. Offline equivalence check (no game, no carrier)

Both sides run against the **same flat guest address space** `0x400000..0x800000`,
built once from the PE image:

| side | executor | memory |
|---|---|---|
| ORIGINAL | the original bytes in **unicorn**, entered with `FNINIT; FLDCW 0x037F` (real 80-bit x87 — see §6a; unicorn otherwise powers up at PC = *single*) | the image mapped at 0x400000 |
| LIFTED | the generated C compiled by 32-bit MSVC into `harness/lift_check.exe` (or `lift_check_soft.exe`, the same sources with `-DPF_X87_SOFT`) | the same image bytes in an in-process copy, reached through `PF_MEM` |

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

`--census` keeps going instead of stopping at the first difference and reports
how many vectors differ; `--exe` selects which build of the LIFTED side to run.

| function | vectors | domain compared | `pf_x87_t = double` | software 80-bit |
|---|---:|---|---|---|
| `update_frame` | 4 seeds × 50 000 | `reward_time` 4B + `reward_scale` 4B + `Tplayer` 184B (no return value) | **EQUAL** | **EQUAL** |
| `is_solid` | 4 seeds × 50 000 | EAX + `Tmap` 772B (must be unchanged — negative control) | **EQUAL** | **EQUAL** |
| `jump_player` | 4 seeds × 50 000 | EAX + `Tplayer` 184B, bit-exact | **EQUAL** | **EQUAL** |
| `line_intersect` | 4 seeds × 50 000 | EAX + `*px` 4B + `*py` 4B, bit-exact | **DIFFER** 107/103/105/128 per seed | **EQUAL** |

`line_intersect`'s vectors are not uniform noise. About 20 % are constructed so
that `ua*d + 0.5` is *exactly* an integer (`_boundary` in `lift_check.py`, which
solves a linear congruence for it), which puts `fistp` exactly on its truncation
boundary; the rest are game-scale coordinates, full-range `int32` (so the
`imul`s wrap), degenerate/parallel segments (`D == 0`, i.e. `x/0` → ±inf and
`0/0` → indefinite), the exact ±0.5 boundary (`dx1 == D`, so `ua*dx1` is an
integer), and `|ua*dx1|` pushed to the 2³¹ edge. Measured on the default seed:
of 50 000 vectors, 24 310 pass the `0 ≤ ua, ub ≤ 1` guard and reach the `fistp`,
10 339 of those sit exactly on a truncation boundary, 1 531 of those also have an
`ua` that is not binary-finite (the only ones where the two models *can* differ),
and **107** actually do.

Denormals are unreachable through this function (all ten parameters are `int`,
and `|ua| ≥ 2⁻³¹`); the subnormal, NaN and ±inf coverage lives in `jump_player`,
whose `sx` is a `double` read straight from `Tplayer`.

Negative control (`--fault FUNC:VECTOR:BYTE` flips one bit of the lifted result):
the comparator names it exactly, e.g.
`DIFFER at vector 11 … Tplayer+0x20 (VA 0x00790020) original 0xff lifted 0xfe`,
and for the new function, on both backends,
`DIFFER at vector 5 … *px+0x2 (VA 0x00794002) original 0x00 lifted 0x01`.

`line_intersect`'s comparison domain is provably complete: the function contains
exactly two stores outside its own frame, `mov [eax], edx` at 0x406c5d and
`mov [eax], ecx` at 0x406c76, i.e. the two `int *` out-parameters.

The check has now found four real bugs before the code passed:

1. `PF_PUSH(v)` moved the top index before evaluating `v`, so `fld st(0)` pushed
   the wrong register. `jump_player` vector 13 (`sx = -11.0`) named it at
   `Tplayer+0x1e`: `sy` was `-12.0` instead of `-22.0`.
2. The softfloat's `pf_mul` exponent was one too low (every product came out
   halved) — caught by a direct unit test of the backend, not by the oracle.
3. The softfloat's division produced 64 quotient bits with **nothing below
   them**, so it truncated where the hardware rounds; `line_intersect` vector 2
   of seed 20260907 gave a quotient one ulp low and a screen coordinate one
   pixel off.
4. The 32-bit cdecl ABI returns a `double` in `ST(0)`, and `FLD` of a signalling
   NaN quiets it — so `pf_to_f64`, a pure bit-shuffling routine, silently set the
   quiet bit. `jump_player` vector 37732 of seed 1 (`sx = 0xfff028e75c6699e9`)
   stored `0xfff828e75c6699e9`. FST now moves raw bit patterns
   (`pf_bits32`/`pf_bits64` return integers, never a float).

Caveat, stated rather than hidden: the oracle restores only the union of the
comparison domain and the per-vector write regions between vectors. That is
sufficient because these four functions provably write nowhere else (verified
by reading every store in the disassembly), and it is *not* a general property —
a function with wider writes needs a full image restore on the oracle side too
(the lifted side already does a full restore per vector).

## 8. Refusals (KNOWN — this is the complete list)

Any instruction, operand form or pattern below aborts the lift with the address
and the mnemonic, writes the refusal into `lifted_<name>.json` and exits 1.
There is no silent fallback anywhere.

**Decoding / layout**: capstone cannot decode; an instruction crosses the end of
the function; a branch target outside `[va, va+size)`.

**Instructions**: anything not listed in §5/§6. Now *supported* (added for
`line_intersect`): `imul` in all three forms, `mul`, `adc`/`sbb` (value only —
see below), `fnstcw`/`fldcw`, the integer x87 memory forms (`fidivr`, `fimul`,
`fiadd`, `fisub(r)`, `fidiv`, `ficom(p)`), and `fistp` under a modelled rounding
mode. Still **refused**: `cmov*`, `loop*`, `jecxz`, `rol`/`ror`, `shld`/`shrd`,
`bt`/`bts`, `xadd`, `cmpxchg`, string operations, `xchg`, `bswap`, all SSE/MMX,
`hlt`, and any x87 with a prefix.

Still-refused x87 in particular, since it is the surface that blocks other
gameplay functions: `fld`/`fstp` **m80** (`DB /5`, `DB /7`), `fistp` **m64**
(`DF /7`), `fbld`/`fbstp`, `frndint`, `fprem`/`fprem1`, `fscale`, `fxtract`,
`fsqrt` (neither backend has a correctly rounded square root — refused rather
than approximated), the transcendentals `fsin`/`fcos`/`fptan`/`fpatan`/`f2xm1`/
`fyl2x`/`fyl2xp1`, the constant loads other than `fldz`/`fld1`
(`fldpi`/`fldl2e`/`fldl2t`/`fldlg2`/`fldln2`), and the environment instructions
`fnstenv`/`fldenv`/`fnsave`/`frstor`/`fnclex`.

`adc`/`sbb` are an **UNVERIFIED PATH**: the value is modelled exactly (the
carry-in comes from the resolved reaching flag definition) but the flags they
*produce* are marked undefined, so consuming them is a refusal. No lifted
function so far contains either instruction, and every emitted site carries an
`/* UNVERIFIED PATH */` comment.

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

**Run time** (the generated code, not the lifter), all `PF_TRAP(va, why)`, all
loud: `#DE` on integer division by zero or a quotient that does not fit in 32
bits; `PF_CW_ARITH` when a rounding x87 operation is reached with a non-default
PC/RC (§6a); and, in the software backend only, an extended denormal operand or
an extended-range underflow (both unreachable from `int`/`float`/`double`
inputs). The carrier must provide `void pf_trap(unsigned, const char*)`; the
harness does.

### 8a. What the refusals actually block (KNOWN, measured)

Running the lifter over **all 253 game functions** of `artifacts/functions.json`
(`--out` to a scratch directory, then reading the `refusals` field of each
`lifted_<name>.json`) gives the exact blocker census:

```
game functions: 253   lift cleanly: 65
   85  call lowering        e.g. save_garbled_data @0x401344:
                            "call 0x004bad28 has no prototype in the interop header"
   29  frame model          e.g. read_line @0x401460: "lea of a frame address"
   28  types / ABI          e.g. an unnamed function @0x4017d4: "no PFN_ typedef"
    8  indirect jmp (switch table)      e.g. blit_to_screen @0x40b6bc
   22  branch target outside the function (tail jumps into Allegro/CRT helpers)
   10  rep / repe / repne string operations
    1  fistp m64 (DF /7)    calc_replay_checksum @0x41bac4
    1  flags of a shift are consumed    draw_reward @0x4070fc
```

Reading that as a work list, in order of what it would unblock:

1. **Call lowering (85 functions).** Not a missing *instruction* — the direct
   call path exists but refuses because `it_funcs.h` only declares *game*
   functions, so a call into Allegro or the CRT has no typed prototype. This is
   also the path with zero test coverage (§5). Fixing it means extending the
   interop generator to the library boundary, and then actually exercising it.
2. **`lea` of a frame address (29).** Taking the address of a local is
   ordinary C; the frame model refuses it because `pf_stk` is not at the
   original address. This needs the frame to become a real, addressable region
   rather than a scratch array — a design change, not a decoding one.
3. **Unnamed functions (28)** are a DWARF/interop gap, not a lifter gap.
4. **Switch tables (8)** and **string operations (10)** are ordinary missing
   features.
5. **`fistp m64`** blocks exactly one function, and it is a relevant one:
   `calc_replay_checksum`. Three lines of decoder plus `PF_TOI64`.

Note what is *no longer* on this list: `imul`, `mul`, `fnstcw`/`fldcw` and the
integer x87 forms, which were the top of it before `line_intersect`.

## 9. Known dependencies and non-claims

- `(int)x >> n` is used for `sar`. C leaves this implementation-defined; MSVC
  documents an arithmetic shift, and the equivalence check confirms it for these
  functions on this compiler. A different compiler needs re-verification.
- `pf_fcmp` writes only the C0/C2/C3 bits plus TOP into the status word. No
  other status-word bit (exception flags, C1) is modelled; nothing in these four
  functions reads one.
- The x87 **tag word** is not modelled (`ffree` is a comment); stack
  overflow/underflow does not fault the way hardware would. Nor are the
  exception flags or unmasked exceptions: every trap the model can hit is a
  `PF_TRAP`, not an x87 exception.
- **SNaN quieting on `FLD` is not modelled.** Real hardware signals `#IA` on
  `FLD m32/m64` of a signalling NaN and (masked) loads the quieted NaN; the
  unicorn oracle used here does not, and both backends match unicorn. No game
  path produces an SNaN — they only appear because `jump_player`'s vector set
  feeds random 64-bit patterns as `sx` — but this is an oracle-fidelity gap, not
  a verified equivalence. See `artifacts/lift_x87_finding.md` §6.
- **The control word at entry is an assumption about the caller.** The lifted
  code starts from `PF_CW_INIT = 0x037F` because the game's `__fpreset`/`FNINIT`
  leaves that (KNOWN, §6a). Inside the carrier — an MSVC process whose own CRT
  sets `0x027F` — this becomes a *precondition the entry stub should assert*,
  not something the lifted code can check for itself.
- `adc`/`sbb` and the whole call-lowering path are generated but exercised by
  nothing (§5, §8).
- The flag dataflow used **identity** semantics for its reaching-definition
  sets, which meant it never converged on a function with a back edge: the
  lifter *hung* instead of refusing. None of the four lifted functions has a
  loop, so this only appeared when scanning all 253 game functions (§8a).
  `FlagDef` now compares by (address, kind, width), and the four generated
  files are byte-identical before and after the fix.
- The lifted objects have **not** been bound into the running carrier yet. This
  deliverable is milestone 11a's *generation* and *offline verification* only;
  replay-equality inside the carrier (`win32_pilot.md` §7) is the next step.

## 10. Line counts

| file | lines | kind |
|---|---:|---|
| `pf_lift.py` | 2261 | hand-written lifter (incl. the 230-line `pf_rt.h` and 434-line `pf_x87_soft.h` templates it emits) |
| `harness/lift_check.py` | 593 | hand-written harness (ORIGINAL side + diff; shared with the NATIVE form) |
| `harness/lift_check.c` | 130 | hand-written harness (LIFTED side) |
| `harness/x87_cw_probe.py` | 104 | hand-written (the control-word experiment) |
| `harness/pf_harness_mem.h` | 24 | hand-written |
| `lifted/pf_rt.h` | 227 | generated (runtime + the `double` backend) |
| `lifted/pf_x87_soft.h` | 434 | generated (370 non-comment lines: the software 80-bit backend) |
| `lifted/lifted_update_frame.c` | 179 | generated (40 instructions, 14 blocks) |
| `lifted/lifted_is_solid.c` | 162 | generated (44 instructions, 8 of 10 blocks) |
| `lifted/lifted_jump_player.c` | 235 | generated (68 instructions, 14 of 18 blocks) |
| `lifted/lifted_line_intersect.c` | 355 | generated (124 instructions, 10 of 12 blocks) |
| **generated total** | **1592** | for 727 bytes of original x86 |

Roughly 40 % of each generated file is the original disassembly reproduced as
comments, one instruction per lifted statement group, which is what makes the
output reviewable against `artifacts/disasm.txt` by eye.

`pf_lift.py` is well over the 1200-line target the task set, and 664 of its
2261 lines are the two runtime templates it emits verbatim. The rest of the
overage is concentrated where it was not optional: the byte-level x87 decoder
(~230 lines, needed because capstone's operand list is ambiguous for D8/DC), the
ESP/EBP frame propagation with its refusals (~130 lines), and the flag dataflow
(~120 lines). The call-lowering path (~60 lines) and `adc`/`sbb` remain the two
parts that could be deleted today without losing anything that is verified.
