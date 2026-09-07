# NATIVE form — `update_frame` and `is_solid`

Status: **working pilot**, 2026-09-07. Milestone 11b of `win32_pilot.md` §8
for the two functions milestone 11a already lifted and verified
(`carrier/lift/README.md`): *write readable C by hand* and *verify it
offline against the original bytes*, the same way the LIFTED form was
verified. Binding either form into the running carrier (the 5-byte entry
patch, `win32_pilot.md` §3) is still pending and is not claimed here.

```
native_update_frame.c      hand-written NATIVE form of update_frame (0x406ac4)
native_is_solid.c          hand-written NATIVE form of is_solid (0x4166dc)
README.md                  this file
```

Verification harness (extends `carrier/lift/harness/`, not duplicated here):

```
carrier/lift/harness/native_check.c     NATIVE-side driver, same wire
                                         protocol as lift_check.c, calling
                                         native_update_frame/native_is_solid
                                         instead of the generated lifted_*
carrier/lift/harness/build_native.cmd   builds harness/native_check.exe
carrier/lift/harness/lift_check.py      extended with --form {lifted,native}
                                         (default lifted); --form native
                                         points --exe at native_check.exe and
                                         --funcs at update_frame,is_solid by
                                         default. Vector generation, the
                                         unicorn oracle and the diff are
                                         byte-for-byte the same code path as
                                         the LIFTED check (carrier/lift/
                                         README.md §7) — only the candidate
                                         binary changes.
```

Run: `python carrier/lift/harness/lift_check.py --form native --vectors 20000
--json artifacts/native_equivalence.json`. Results are in
`artifacts/native_equivalence.json` and summarized in §4 below.

## 1. What each function does

### `update_frame` (VA 0x00406ac4, `main.c` decl_line 2826, no callees)

Per-tick maintenance, called once per game tick from `play()`
(`win32_pilot.md` §2). Two independent pieces of state, read from the
generated globals/types headers (`carrier/gen/it_globals.h`,
`carrier/gen/it_types.h`):

1. **Reward-bar decay/growth.** `reward_time` (int, counts down to 0) and
   `reward_scale` (`fixed` = `int32_t`, a HUD value) implement a three-band
   ramp: while `reward_time` is still counting (`!= 0`), `reward_scale`
   grows by `0xccd` (3277) once `reward_time > 60`, drains by `0x199a`
   (6554) once `reward_time <= 9`, and is left untouched in the 10–60
   plateau in between; either way `reward_time` decrements by 1. This block
   is, per DWARF, literally the inlined call `update_reward(reward_time)` at
   `main.c:2828` (`artifacts/dwarf_info.txt`:
   `DW_TAG_inlined_subroutine`, `abstract_origin` → subprogram named
   `update_reward`, `entry_pc 0x00406ad1`) — GCC 4.4 inlined it into
   `update_frame`, so there is no separate `update_reward` symbol to call in
   the NATIVE form; its body is reproduced in place, matching what the
   compiler actually emitted.
2. **The active player's per-tick counters**, via `ply[player_id]`
   (`Tplayer *ply[1000]`, `int player_id`, both `main.c` globals): the death
   animation counter `dead` advances by 8 while it is in progress and still
   inside its animation range (`0 < dead <= 0x12b` = 299); the edge-of-floor
   indicator counter `edge_drawn` advances while `edge != 0`; the walk-cycle
   `frame` counter advances once every 10 ticks of the volatile
   `logic_count` global (`timer.c`).

No return value. Writes: `reward_scale` (conditionally), `reward_time`
(conditionally), `Tplayer.dead`/`edge_drawn`/`frame` (each conditionally) —
exactly the domain in `notes/promotion_candidates.md` §4.1, extended to the
full 184-byte `Tplayer` in the harness (cheap and already typed, per that
note).

### `is_solid` (VA 0x004166dc, `map.c` decl_line 122, `int is_solid(Tmap *m, int cx, int cy)`)

Pure predicate, no writes at all (confirmed by reading every store in
`artifacts/disasm.txt` between 0x4166dc and 0x416747 — there are none),
which is exactly why `notes/promotion_candidates.md` §3.2 calls it "the best
negative-control candidate": the offline check requires `*m` to come back
byte-identical on every vector.

Given a pixel coordinate `(cx, cy)`, it answers "is there floor here, and if
so at what on-screen Y": convert `cy` to a tower row `y` (16 pixels/tile,
counted from the top of the fixed 32-row `Tmap.room[]` array downward — see
`carrier/gen/it_types.h`'s `struct Tfloor` and `struct Tmap`), bounds-check
`y`, bail out if that row is `empty`, convert `cx` to a tile column `x`, bail
out if `x` falls outside the row's `[start_tile, end_tile]` span, and
otherwise return the floor's solid pixel Y adjusted for `Tmap.offset` (the
map's current vertical scroll position).

DWARF (`artifacts/dwarf_info.txt`, subprogram `is_solid`) names the
parameters `m`, `cx`, `cy` and two locals `x`, `y` declared together at
`map.c:123` — used exactly as named here.

## 2. Local/global name mapping (original register/offset → NATIVE name)

`update_frame` (registers from `carrier/lift/lifted/lifted_update_frame.c`,
which is annotated instruction-by-instruction against
`artifacts/disasm.txt`):

| original | NATIVE name | source |
|---|---|---|
| `[0x4fec68]` (eax at entry) | `reward_time` (via `reward_time_p`) | `IT_G_reward_time`, `main.c` |
| `[0x4fac28]` | `reward_scale` (via `reward_scale_p`) | `IT_G_reward_scale`, `main.c` |
| `[0x4fe518]` | `player_id` | `IT_G_player_id`, `main.c` |
| `[0x4ff128 + eax*4]` | `(*ply)[player_id]` | `IT_G_ply` (`Tplayer *ply[1000]`), `main.c` |
| `ecx` after that load | `p` (`Tplayer *`) | — |
| `[ecx+0x4c]` | `p->dead` | `Tplayer.dead` (offset confirmed: 5 doubles × 8B = 40, + 9 ints to `dead` = 40+9×4=76=0x4c) |
| `[ecx+0x58]` | `p->edge` | offset 88 = 0x58 |
| `[ecx+0x5c]` | `p->edge_drawn` | offset 92 = 0x5c |
| `[ecx+0x3c]` | `p->frame` | offset 60 = 0x3c |
| `[0x506958]` | `logic_count` | `IT_G_logic_count`, `timer.c`, `volatile int` |
| `idiv 0xa` / `test edx,edx` | `logic_count % 10 == 0` | see §3 |

`is_solid`:

| original | NATIVE name | source |
|---|---|---|
| `[ebp+8]` (a0) | `m` (param) → `map` (PF_MEM'd) | DWARF param `m` |
| `[ebp+0xc]` (a1) | `cx` (param) | DWARF param `cx` |
| `[ebp+0x10]` (a2) | `cy` (param) | DWARF param `cy` |
| `29 - ((cy+1)>>4)` | `y` | DWARF local `y`, `map.c:123` |
| `cx >> 4` | `x` | DWARF local `x`, `map.c:123` |
| `[eax]` of `room[y]` | `map->room[y].empty` | `Tfloor.empty`, offset 0 |
| `[eax+4]` | `map->room[y].start_tile` | offset 4 |
| `[eax+8]` | `map->room[y].end_tile` | offset 8 |
| `[ebx+0x300]` | `map->offset` | `Tmap.offset`, offset 768 = 0x300 (`32 * sizeof(Tfloor)` = `32*24`) |

## 3. Where readability required an argued equivalence, not a transcription

Both functions are small enough that almost every line is a direct
transcription of one or two original instructions (visible side by side with
`artifacts/disasm.txt` and the LIFTED form in the tables above). Three
places are not, and are argued here plus checked empirically by the 20 000
vectors in §4:

1. **`logic_count % 10 == 0` for the `idiv`/`cdq`/`test edx,edx` sequence**
   (`update_frame`, `artifacts/disasm.txt` 0x00406b0f–0x00406b20). C's `%` on
   a signed `int` truncates toward zero with the dividend's sign — the same
   thing x86 `idiv` computes, and the same reasoning `carrier/lift/README.md`
   §9 already relies on for `sar`/`>>`. The divisor is the compile-time
   constant `10`, so there is no divide-by-zero or `#DE`-quotient-overflow
   case to reproduce (`PF_TRAP` in the LIFTED form exists for exactly that
   risk with a *runtime* divisor, which `update_frame` never has).
2. **`map->offset % 16` for the `and eax,0x8000000f` / `js` / `dec;or
   0xfffffff0;inc` sequence** (`is_solid`, 0x00416719–0x00416731). This is
   GCC 4.4's idiom for signed `%` by a power of two without `cmov`, worked
   out by hand in `native_is_solid.c`'s comment: for `offset >= 0` the AND
   alone gives `offset & 0xf`, which *is* `offset % 16`; for `offset < 0`
   the sign check plus correction subtracts 16 whenever the low 4 bits are
   nonzero and returns exactly 0 when they are — both cases equal to C's
   truncating `%`. Same "constant divisor, no `#DE` case" argument as above.
3. **`(int)x >> n` for `sar`** (`(cy+1)>>4` and `cx>>4` in `is_solid`).
   Carried over from `carrier/lift/README.md` §9: right shift of a negative
   `int` is implementation-defined in C; MSVC documents it as arithmetic
   (matching `sar`), and this is exactly what the 20 000-vector run checks,
   with `cy` and `cx` pools in the generator deliberately covering negative
   and boundary values (`notes/promotion_candidates.md` §4.2, mirrored in
   `carrier/lift/harness/lift_check.py`'s `gen_is_solid`).

No floating point appears in either function (`x87 = 0` for both in
`notes/promotion_candidates.md` §2), so the `win32_pilot.md` §3 x87 HYPOTHESIS
is not exercised here — that remains `jump_player`'s question
(`carrier/lift/README.md` §6).

## 4. Offline equivalence check (no game, no carrier)

Same route as the LIFTED check (`carrier/lift/README.md` §7): ORIGINAL runs
the real bytes in unicorn (32-bit x86, real 80-bit x87 — irrelevant here
since neither function touches the FPU); NATIVE runs `native_update_frame`/
`native_is_solid`, compiled unchanged by 32-bit MSVC into
`harness/native_check.exe`, with `PF_MEM()` redirected by the same
force-included `harness/pf_harness_mem.h` at an in-process copy of the
image. Vector generation (`gen_update_frame`, `gen_is_solid` in
`lift_check.py`) and the comparison domains are identical to the LIFTED
check — same RNG seed (20260907), same boundary-value pools, same struct
byte layout.

| function | vectors | domain compared | result |
|---|---:|---|---|
| `update_frame` | 20 000 | `reward_time` 4B + `reward_scale` 4B + `Tplayer` 184B (no return value) | **EQUAL** |
| `is_solid` | 20 000 | EAX + `Tmap` 772B (must be unchanged — negative control) | **EQUAL** |

No divergence found; first-difference reporting was never exercised in the
clean run because there was nothing to report.

Negative control (`--fault FUNC:VECTOR:BYTE`, flips one bit of the NATIVE
result before the comparator sees it), run once per function, 200 vectors
each:

```
update_frame: DIFFER at vector 5: reward_time+0x3 (VA 0x004fec6b) original 0x00 native 0x01
is_solid:     DIFFER at vector 5: Tmap+0x3 (VA 0x00792003) original 0xff native 0xfe
```

Both named exactly (function, vector, field+offset, VA, original byte,
injected byte) — the comparator catches a real divergence for both
functions, so the 20 000-vector EQUAL result above is not a check that
cannot fail. Full JSON: `artifacts/native_equivalence.json`.

## 5. Line counts (NATIVE vs LIFTED, same functions)

| function | NATIVE lines | LIFTED lines |
|---|---:|---:|
| `update_frame` | 83 (`native_update_frame.c`) | 179 (`lifted_update_frame.c`, 40 instructions / 14 blocks) |
| `is_solid` | 74 (`native_is_solid.c`) | 162 (`lifted_is_solid.c`, 44 instructions / 8 blocks) |

Both NATIVE files are under half the length of their LIFTED counterparts for
the same observable behaviour, with no register locals, no `goto`, and no
`PF_R32`/`PF_W32`/`PF_A32` calls scattered through the body — `PF_MEM()` is
used exactly once per global and once per incoming pointer parameter (see
each file's header comment), which is the "single line/macro" dependency
the task asked for; everything after that translation is plain, typed C
(`p->dead`, `map->room[y].empty`, …).

## 6. What did not need arguing

- Both functions' writes and control flow map one-to-one onto their LIFTED
  counterparts and `artifacts/disasm.txt`; no instruction was reordered or
  restructured beyond straight-line-to-`if/else` translation of the
  `test`/`cmp`+`jcc` pairs (verified by reading every branch target).
- `fixed` (`int32_t`) arithmetic on `reward_scale` is plain 32-bit
  wraparound `+=`/`-=`, identical in GCC and MSVC for this code shape (no
  UB-exploiting optimization applies to a single `+=`/`-=` on a global at
  `/W3`).
- `Tplayer`/`Tmap`/`Tfloor` field offsets used above were computed from
  `carrier/gen/it_types.h` under its `#pragma pack(push, 1)` and cross-checked
  against every `[ecx+N]`/`[eax+N]` offset in `artifacts/disasm.txt` — no
  offset was guessed.
- Neither function calls anything (`notes/promotion_candidates.md` §2:
  `imports_used = []`, zero callees for both), so the call-lowering
  UNVERIFIED-PATH caveat in `carrier/lift/README.md` §5 does not apply here.

## 7. Non-claims

- Binding either NATIVE symbol into the running carrier via the entry patch
  (`win32_pilot.md` §3) is not done here; this is the offline verification
  step only, matching what milestone 11a claimed for the LIFTED form.
- The harness extension (`--form native`, `native_check.c`) reuses the
  existing vector generators and oracle unchanged; it does not add new
  comparison domains or a new fault-injection mechanism beyond what
  `lift_check.py` already had for LIFTED.
