# The x87 finding: `double` is not enough for Icy Tower

Date: 2026-09-07. Subject: the HYPOTHESIS of `win32_pilot.md` §3 — *"x87 must be
represented faithfully enough that physics bits match the oracle (HYPOTHESIS:
`double` is not enough where GCC kept 80-bit intermediates; the oracle
comparison decides, and softfloat x87 is the fallback)."*

Labels as in `win32_pilot.md`: **KNOWN** = read from the binary or observed in a
run; **INFERRED** = reasoned; **HYPOTHESIS** = a design bet awaiting evidence.

**Verdict: the hypothesis is confirmed, and it is now decided rather than open.**
`line_intersect` (0x406b80) discriminates. With `pf_x87_t = double` the LIFTED
form differs from the ORIGINAL bytes on roughly 1 vector in 450 of the vector
set used here; with a software 80-bit extended type it is bit-exact over
200 000 vectors. All numbers below are reproducible with
`carrier/lift/harness/lift_check.py` and `carrier/lift/harness/x87_cw_probe.py`.

---

## 1. Why `jump_player` could not decide it, and `line_intersect` can (KNOWN)

`jump_player` (checked EQUAL over 200 000 vectors with the `double` model and
another 200 000 with the software one, see `carrier/lift/README.md` §6) does only two FP *arithmetic* operations:
`fadd st(1)` (`sx+sx`) and `fmul dword [0x4d7130]` where that constant is
exactly `-2.0f`. Both are scalings by a power of two — exact in binary floating
point at any precision — and the result is immediately written back to memory as
a `double` (`fst qword`). A model that is exact for every operation it performs
cannot be told apart from the hardware, whatever its internal width.

`line_intersect` is the opposite shape, and every part of the difference matters:

| property | `jump_player` | `line_intersect` |
|---|---|---|
| FP arithmetic | 2 ops, both exact powers of two | `fidivr` ×2, `fimul` ×2, `fadd`/`faddp` ×2 — none exact |
| intermediates spilled to memory | yes, as `double` | **no** — everything stays in the x87 register stack until the final `fistp` |
| final consumer | `fst qword` (a store rounds to double either way) | `fistp` = **truncation to int32**, a discontinuity |
| amplification | none | the quotient is multiplied by `dx1`/`dy1`, up to ±2³¹ |

The last two rows are the mechanism. The quotient `ua = N1/D` is a ratio of two
`int32`s, so it needs up to 64 significant bits; rounding it to 53 rather than 64
introduces a relative error of about 2⁻⁵³ instead of 2⁻⁶⁴. Multiplying by an
integer of magnitude up to 2³¹ turns that into an absolute error of up to ~2⁻²²,
and `fistp` then truncates — so whenever the exact value of `ua*d + 0.5` sits
within that distance of an integer, the two models fall on opposite sides and the
stored coordinate differs by exactly 1.

## 2. The precision-control field decides the answer (KNOWN)

The x87 rounds every arithmetic result to the significand width selected by the
**PC** field of the control word, not to the register width. So "is `double`
enough" is not a property of the code alone; it is a property of the control word
the function inherits.

```
___mingw_CRTStartup   0x401020   call 0x4b2850 <__fpreset>
__fpreset             0x4b2850   push ebp ; mov ebp,esp ; fninit ; pop ebp ; ret
```

**KNOWN**, from `artifacts/disasm.txt`: Icy Tower's MinGW startup calls
`__fpreset`, which is a bare `FNINIT`. `FNINIT` leaves `CW = 0x037F`, i.e.
**PC = 11 (64-bit significand, full extended precision)** and RC = 00
(round to nearest even). This is *not* the MSVC/CRT default of `0x027F`
(PC = 10, 53-bit significand).

**INFERRED**: nothing in the game changes PC afterwards. Every `FLDCW` in the
image (`grep fldcw artifacts/disasm.txt`) appears as one half of a
`fnstcw` / `mov ah,0x0C` / `fldcw` / `fistp` / `fldcw` pair — GCC 4.4's
float-to-int cast idiom, which sets RC to "toward zero" and restores the saved
word immediately afterwards.

`x87_cw_probe.py` runs the ORIGINAL bytes in unicorn at three control words on
six hand-constructed vectors and asks which lifted model the hardware agrees
with:

```
unicorn power-on FPCW = 0x0000
CW = 0x037F  (PC = 64 bits) -> matches 80-bit model 6/6, double model 0/6, neither 0/6
CW = 0x027F  (PC = 53 bits) -> matches 80-bit model 0/6, double model 6/6, neither 0/6
CW = 0x007F  (PC = 24 bits) -> matches 80-bit model 0/6, double model 1/6, neither 5/6
```

Two consequences:

* At the control word the game actually runs with, `double` is **wrong**.
* **unicorn powers up with `FPCW = 0x0000`, i.e. PC = 00 = *single* precision.**
  An oracle that does not set the control word is not modelling this program at
  all — it is modelling a 24-bit FPU. `lift_check.py`'s `Oracle.call` therefore
  executes `FNINIT; FLDCW 0x037F` before every call, and `PF_CW_INIT` in
  `pf_rt.h` is `0x037F` to match. (Writing `UC_X86_REG_FPCW` directly had no
  effect in this unicorn build; executing the two instructions does.)

## 3. Which intermediate loses the precision (KNOWN, worked example)

First differing vector of the default seed (20260907), vector 227 of the
`line_intersect` set:

```
line_intersect(x1=-484594473, y1=3591436, x2=385321175, y2=733949708,
               x3=0, y3=0, x4=0, y4=1, &px, &py)

dx1 = 869915648   dy1 = 730358272   dx3 = 0   dy3 = 1
D  = dx1*dy3 - dx3*dy1 = 869915648
N1 = dx3*py  - dy3*px  = 484594473
ua = N1/D = 3993/7168 = 0.55705915178571428...      (exact, not binary-finite)
exact ua*dy1 + 0.5 = 406852760                      (EXACTLY an integer)
```

| model | rounded `ua` minus exact `ua` | `fistp` of `ua*dy1 + 0.5` | `*py` |
|---|---:|---:|---:|
| 80-bit (hardware, and the softfloat) | −2.3233e−20 | 406852759 | **410444195** |
| `double` | +1.5860e−17 | 406852760 | **410444196** |

The harness names it byte-for-byte:

```
[line_intersect/LIFTED] DIFFER at vector 227 (107 of 50000 vectors differ):
  kind='comparison domain'  at='*py+0x0 (VA 0x00794004)'
  original=0xa3  lifted=0xa4
```

**Characterisation.** The intermediate that loses precision is the *quotient*
`ua = N1/D` produced by `fidivr` and held in ST — a single rounding, to 64 bits
on the hardware and to 53 in the `double` model. It is **not** double-rounding on
a store: `line_intersect` never spills an x87 value to memory as a `float` or a
`double`; the only memory traffic on the FP side is `fild`/`fidivr`/`fimul`
(integers in) and `fistp` (an integer out). The subsequent `fimul` by `dy1`
(≈7.3e8) and the `fadd` of `0.5f` are what turn a 10⁻¹⁷ relative error into a
half-unit absolute error, and `fistp`'s truncation is what makes it visible as a
whole-pixel difference. The sign of the divergence goes both ways (in this vector
the 80-bit quotient is *below* the exact value and the double quotient *above*),
so it is not a systematic bias that could be papered over with a nudge.

## 4. Numbers (KNOWN)

50 000 vectors per seed, four seeds, comparing `EAX` plus the 8 bytes `*px`,
`*py` bit-for-bit against the ORIGINAL bytes in unicorn at `CW = 0x037F`.
`--census` counts every differing vector instead of stopping at the first.

| function | backend | seed 20260907 | seed 1 | seed 424242 | seed 987654321 |
|---|---|---|---|---|---|
| `line_intersect` | `pf_x87_t = double` | DIFFER 107/50000 | DIFFER 103/50000 | DIFFER 105/50000 | DIFFER 128/50000 |
| `line_intersect` | software 80-bit | **EQUAL** | **EQUAL** | **EQUAL** | **EQUAL** |
| `update_frame` | software 80-bit | EQUAL | EQUAL | EQUAL | EQUAL |
| `is_solid` | software 80-bit | EQUAL | EQUAL | EQUAL | EQUAL |
| `jump_player` | software 80-bit | EQUAL | EQUAL | EQUAL | EQUAL |

Total: **200 000 vectors EQUAL** with the software 80-bit backend on
`line_intersect`, against **443 differing vectors of 200 000** with `double`.

The rate is a property of the vector set, not of the function. Measured on the
default seed by exact rational arithmetic over the same 50 000 vectors:

| | vectors |
|---|---:|
| generated | 50 000 |
| pass the `0 ≤ ua, ub ≤ 1` guard and therefore reach the `fistp` | 24 310 |
| …of those, `ua·d + 0.5` is *exactly* an integer (constructed boundary) | 10 339 |
| …of those, `ua` is not a binary-finite rational, so the two models *can* differ | 1 531 |
| actually differ | **107** |

The last step is 7 %, not 50 %, and the reason is worth stating: the exact
product `ua·d` equals `m − 0.5`, which needs only ~30 bits and is therefore
exactly representable in *both* models. Whenever the error inherited from the
rounded quotient is smaller than half an ulp of `m − 0.5`, the product's own
rounding snaps it back onto `m − 0.5` and both models agree. The disagreement
needs the two errors to be comparable to that ulp *and* of opposite sign — a
genuine near-tie, which is exactly the regime the constructed vectors aim at.

Uniformly random coordinates hit it far more rarely: the exact value has to land
within about `|ua·d| · 2⁻⁵³` of an integer, i.e. with probability roughly
`|ua·d| · 2⁻⁵²` per vector. The finding is that the divergence **exists and is
reachable**, not that it is common — one whole-pixel difference in a collision
result is enough to make a replay diverge, and it is exactly the class of bug the
carrier exists to catch.

## 5. What was implemented (KNOWN)

`carrier/lift/lifted/pf_x87_soft.h` (434 lines, 370 of them code, generated by
`pf_lift.py`), a
minimal software 80-bit extended type selected by `-DPF_X87_SOFT`:

* sign + 15-bit biased exponent + 64-bit significand with an explicit integer
  bit — the real 80-bit register layout;
* add / sub / mul / div, all round-to-nearest-even at 64 bits (division is a
  restoring loop producing 65 quotient bits plus a sticky remainder, so the
  round bit below the significand really exists);
* compare (ordered/unordered), int16/int32/int64 and float/double conversions in,
  a single IEEE packer for float/double conversions out;
* `FIST`/`FISTP` honouring the RC field of the modelled control word, with the
  integer-indefinite `0x80000000` for NaN and out-of-range sources.

It is checked twice, independently:

* against the **oracle** — 200 000 vectors of `line_intersect` plus 200 000 of
  each other lifted function, all EQUAL (§4);
* against **exact arithmetic**, with no oracle at all —
  `carrier/lift/harness/x87_soft_selftest.py` runs 40 000 random
  `add`/`sub`/`mul`/`div` pairs (int32-valued doubles, huge x small, and
  arbitrary finite doubles) through the backend and requires the packed
  (sign, biased exponent, 64-bit significand) triple to equal the exact rational
  result rounded to nearest-even at 64 bits. **39 966 operations checked, 0
  mismatches.** That is what separates "the softfloat is correct" from "the
  softfloat agrees with unicorn".

**No generated `.c` file changes between the two backends.** The lifted code goes
through `PF_ADD/PF_SUB/PF_MUL/PF_DIV/PF_FI32/PF_TOI32/…` macros; `pf_rt.h`
picks the implementation.

Two bugs the oracle found in the softfloat before it passed, both worth
recording because they are the classic ones:

1. **Division truncated instead of rounding.** The 64 quotient bits were placed
   in the significand with nothing below them, so `pf_round128` never saw a round
   bit. `ua` came out one ulp low (`…aaaa` instead of `…aaab` on vector 2 of
   seed 20260907) and the whole result was off by one. Fixed by computing a 65th
   quotient bit.
2. **The x86 ABI quieted a signalling NaN.** A 32-bit `double`-returning function
   returns in `ST(0)`, and `FLD` of an SNaN quiets it. So `pf_to_f64` — a pure
   bit-shuffling routine — silently set the quiet bit on the way out, and
   `jump_player` vector 37732 of seed 1 (`sx` = `0xfff028e75c6699e9`, an SNaN)
   stored `0xfff828e75c6699e9`. Fixed by making the FST path move raw bit
   patterns (`pf_bits32`/`pf_bits64` return integers, never a float).

## 6. What is deliberately *not* claimed

* **SNaN quieting on `FLD` is not modelled by either backend.** Real hardware
  signals `#IA` on `FLD m32/m64` of a signalling NaN and, with the exception
  masked, loads the quieted NaN. The unicorn oracle used here does not, and both
  backends match unicorn. No game path can produce an SNaN (they only appear
  because the harness feeds random 64-bit patterns as `sx`), but this is an
  oracle-fidelity gap, not a verified equivalence.
* **Denormals cannot be exercised through `line_intersect`.** Its ten parameters
  are all `int`, and the only FP constant it loads is `0.5f`; `|ua| ≥ 2⁻³¹`, so
  no operand or result is ever subnormal. Subnormal coverage comes from
  `jump_player`, whose `sx` is a `double` read from `Tplayer` and whose vector
  set includes `5e-324`, `2.2250738585072014e-308` and random 64-bit patterns.
  The softfloat's own double↔extended conversions handle subnormal doubles;
  *extended* subnormals (|x| < 2⁻¹⁶³⁸²) are unreachable from any int/float/double
  input and are a hard `PF_TRAP`, never an approximation.
* **`FISTP` overflow to the integer indefinite is unreachable here.** The guard
  `0 ≤ ua ≤ 1` plus `|dx1| ≤ 2³¹` bounds `|ua·d + 0.5|` by `2³¹ − 0.5`, so the
  stored value always fits in `int32`. Both backends implement the indefinite
  (it is unit-tested), but `line_intersect` does not reach it.
* **Arithmetic under a non-default control word is not modelled.** `pf_lift`
  emits `PF_CW_ARITH(va, pf_fcw)` before every rounding x87 operation in a
  function that contains an `FLDCW`; it traps if PC/RC are not the FNINIT
  defaults. In `line_intersect` the truncating control word is live only across
  the `fistp` itself, so the guard never fires — but it would if some other
  function did arithmetic under it.
* **This is offline equivalence, not carrier equality.** The lifted objects are
  still not bound at 0x406b80 in the running carrier; replay-equality inside the
  carrier is a separate step (`win32_pilot.md` §7).

## 7. Consequence for the carrier

`pf_x87_t = double` must not be the default for any function whose x87
intermediates are neither exact nor immediately stored. Concretely:

* Every LIFTED game function that divides, multiplies non-powers-of-two, or feeds
  an x87 value into `fistp` needs `-DPF_X87_SOFT` until proven otherwise.
* The carrier's own binding must ensure the FPU control word at entry to a lifted
  function is the same 0x037F the original code runs with. The carrier is an MSVC
  process; its CRT sets `0x027F`. If the guest's `__fpreset` has run, the process
  word is 0x037F and both forms agree — but this is now a *checked precondition*,
  not an assumption, and is the natural thing for the entry stub to assert.
* NATIVE (hand-written) replacements have the same problem in a worse form: C
  `double` arithmetic on MSVC/SSE2 is 53-bit, so a readable rewrite of
  `line_intersect` cannot be bit-equal to the original unless it either uses the
  same software extended type or the verdict accepts a documented tolerance.
  That is a decision for milestone 11b, and it now has a number attached to it.
