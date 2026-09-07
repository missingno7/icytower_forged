# The 1.5.1 tower layout rules -- what `add_floor` actually does

Scope: `add_floor` (VA 0x4167dc, 608 bytes, `void add_floor(Tmap *m)`), promoted
into `src/icytower/map.c` this pass. This is the operator-facing answer to
"what creates the layout": the prose below states the rules; the table
compares them, point by point, against 1.3's `new_floor()`
(`assets/replay_checker/Icy Tower.cpp`, read-only reference material per
`notes/replay_checker_reference.md` -- a 2010 clean-room reimplementation,
not a disassembly of a real 1.3 binary, so treat its own fidelity to
whatever Free Lunch Design actually shipped as itself unverified).

## 1. The rules, in prose

Every call to `add_floor(m)` does exactly two things: it **shifts** the
32-row ring buffer `m->room[]` up by one (`room[i] = room[i+1]` for
`i < 31`), discarding the oldest row and freeing `room[31]`; then it
**fills `room[31]`** according to the rules below, using `room[31].level`
itself as the running floor-index counter (call its value on entry `k`;
every reachable return writes `k+1` back into `.level`). No wall-clock,
tick counter, or player state is read anywhere in this function --
`notes/layout_determinism.md` SS2 already established this from the
disassembly; recovering the source confirms it line by line.

**Cosmetic bookkeeping, every call:** `room[31].tiles` is set unconditionally,
before any branch below, to `k/500` capped at `10` once `k>4999`. Nothing in
`map.c`'s consumers (`is_solid`, `getFloorData`, `get_level`) reads it; it
is presumably a rendering hint (e.g. a background-tile-set index) for a
not-yet-promoted drawing function.

**Row cadence.** Only 1 floor-slot in 5 is an actual platform. The other 4
are marked empty (`empty=-1`, `level=k+1`, nothing else touched -- `start_
tile`/`end_tile`/`sign` are left at whatever stale value the row already
had, exactly like `reset_map`'s "empty rows are never zeroed" convention)
and consume no `rand()`.

**Checkpoint floors** pre-empt the row cadence and span the full width
(`start_tile=0`, `end_tile=40`), consuming no `rand()`: every 250th floor
while `k<=5004`, then every 2500th floor beyond that --
`(k%250==0 && k<=5004) || k%2500==0`. Every 50th checkpoint (by the
*new*, post-increment level: `(k+1-1)%50==0`, i.e. every 50th value of
`k+1`) additionally gets a visible **sign**: `room[31].sign = (k+1)/5`.
Every other checkpoint, and every non-checkpoint floor (real or empty),
has `sign=0`.

**Real floors** (`k%5==0`, not a checkpoint) draw a **width** in tiles,
one of three ways, keyed on `get_demo()->floor_shrink`:

| condition | width | rand() |
|---|---|---|
| `floor_shrink==0` | `6 + rand()%10` (range 6..15) | 1 |
| `floor_shrink!=0`, new `k<=2999` | `rand()` is drawn **unconditionally first**; a float ratio is derived from `k` (`q = k/-5 + 300`; `ratio = q/300.0 * 10.0`, all in `float`, computed the way the original's x87 code keeps it -- see SS2); if `ratio<1.0`, width=6 and the just-drawn `rand()` is **discarded**; otherwise `width = 6 + rand()%(int)ratio` | 1 (always drawn, sometimes unused) |
| `floor_shrink!=0`, new `k>2999` | a fixed staircase by height: 6 (`k<=5004`), 5 (`k<=7504`), 4 (`k<=10004`), 3 (`k<50005`), 2 (else) | 0 |

The width is then adjusted by `floor_size_modifiers[get_demo()->floor_size]`
-- a static table, `{2, 0, -2, -4, -6}`, VA 0x4bdb60, **not range-checked**
against `floor_size` before indexing it. If the adjusted width is `<=0` it
is clamped to 1 with a fixed 29-wide placement range; otherwise the
placement range is `30-width`. Either way exactly one more `rand()` places
it: `start_tile = 5 + rand()%range`, `end_tile = start_tile+width`.

So a real floor draws **2** `rand()` calls when `floor_shrink==0` or
(`floor_shrink!=0 && new k<=2999`), and **1** otherwise; checkpoint and
empty floors draw **0** -- exactly the KNOWN table in
`notes/layout_determinism.md` SS2, now attached to the code that produces
it.

## 2. SAME / CHANGED / UNKNOWN against 1.3's `new_floor()`

| aspect | 1.3 (`new_floor`, `Icy Tower.cpp`/`.h`) | 1.5.1 (`add_floor`) | verdict |
|---|---|---|---|
| when a floor is actually generated | caller-side throttle: `floors->pad` counts calls, only every 4th call's body runs (`pad<4: pad++; return`) -- the function itself has no "skip" outcome, only "run" or "count the call and do nothing" | callee-side: **every** call advances `k` and either fills a real floor or marks the row empty (`k%5!=0`) -- "skip" is now a real, stored `Tfloor` state (`empty=-1`), not a no-op | **CHANGED** -- cadence logic moved from caller-throttle to an internal empty/real split, and the ratio changed (1-in-4 -> 1-in-5) |
| what triggers a call at all | (not in `new_floor` itself; the reference's caller polls at a fixed 16px-of-scroll cadence, `notes/replay_checker_reference.md` SS"documents") | `new_game()`: fixed `for(i=0;i<30;i++)`. `play()`: on demand, gated by the player's climbed-height marker crossing the highest-generated marker, at most once/tick (`notes/layout_determinism.md` SS2 "Callers") | **CHANGED** -- 1.5.1's caller is height-driven, not a fixed per-pixel-of-scroll schedule |
| per-floor stored fields | `FLOOR{start,end}` only (`Icy Tower.h`) -- no level, no empty flag, no sign, no tile-set hint; the game floor index is *derived* elsewhere (`floor_from_y` takes `screen_y` and computes `floor_level` as an out-param) | `Tfloor{empty,start_tile,end_tile,level,sign,tiles}` -- the floor index is *stored per row*, not derived | **CHANGED** -- 1.5.1 makes the index (and two cosmetic fields) part of the persisted row state |
| ring buffer size | 7 rows (`FLOOR floor[7]`, `start` wraps mod 7) | 32 rows (`Tfloor room[32]`) | **CHANGED** (implementation detail, likely tracks a taller on-screen viewport) |
| full-width checkpoint floors exist | yes: every 50th floor, every 500th above floor 1000 (`count<=1000 ? !(count%50) : !(count%500)`) | yes: every 250th floor up to `k<=5004`, every 2500th beyond -- same *shape* (sparser checkpoints at height), 5x larger thresholds and a different switchover point (1000 -> 5004) | **CHANGED** (mechanism preserved, numbers rescaled) |
| checkpoint "sign" marker | not present -- `FLOOR` has no field for it | every 50th checkpoint gets `sign=(new level)/5`; the rest get `sign=0` | **NEW in 1.5.1** |
| cosmetic tile-set hint (`tiles`) | not present | every call, `k/500` capped at 10 | **NEW in 1.5.1** |
| length/width schedule shape | early phase: `rand()`-scaled by height (`6+rand%(9-count/30)`, `count<240`); then a **dummy `rand()` draw-and-discard** quirk (`240<=count<600`: draw one `rand()`, ignore it, width fixed at 6); then a fixed descending staircase 6/5/4/3/2 by height (600/1000/1500/2000/10000) | early phase: an `rand()`-scaled width via a **float ratio** derived from `k` (not integer `count/30`); the SAME draw-and-sometimes-discard quirk reappears, but the discard condition is now `ratio<1.0` (a float compare) instead of a `count` range test; then a fixed descending staircase 6/5/4/3/2 by height (2999/5004/7504/10004/50005) | **CHANGED** -- qualitatively the same three-phase shape (including the "draw a rand() and maybe throw it away" oddity, preserved across versions in spirit if not in mechanism), thresholds roughly 10-25x larger and the early-phase formula reimplemented in floating point |
| `floor_shrink` (a whole extra top-level branch) | absent -- `new_floor`'s schedule is a pure function of `count` alone | present: `get_demo()->floor_shrink==0` swaps to an entirely different, height-blind schedule (`6+rand()%10`) that ignores everything above | **NEW in 1.5.1** (a difficulty-setting input `new_floor` has no equivalent of) |
| `floor_size` / `floor_size_modifiers` | absent | present: a static `int[5]` offset added to width, indexed by `get_demo()->floor_size`, unconditionally on every real floor | **NEW in 1.5.1** |
| x-offset formula | `start = 5 + rand()%(30-length)` | `start_tile = 5 + rand()%max_w`, `max_w = 30-width` in the ordinary case | **SAME** core formula; 1.5.1 adds a clamp-to-1-width/29-range fallback for when `floor_size_modifiers` pushes the adjusted width to `<=0`, a case 1.3's version (minimum length 2, `floor_size_modifiers`-free) can never reach |
| rand() call count per generated floor | 0 (checkpoint), 1 (`count>=600`, no length-rand), or 2 (`count<600`, length-rand + placement-rand) | 0 (checkpoint/empty), 1 (`floor_shrink!=0 && new k>2999`), or 2 (`floor_shrink==0` or `floor_shrink!=0 && new k<=2999`) | **SAME shape** (the {0,1,2} pattern survives intact); **CHANGED** specifics (which condition lands in which bucket) |
| RNG algorithm | `rand_p`: `seed=seed*214013+2531011; return (seed>>16)&0x7fff` (msvcrt's classic LCG, explicit seed parameter) | the game's own `rand()` (`_rand`->msvcrt IAT), same algorithm -- independently confirmed by `notes/replay_checker_reference.md`'s "RNG" bullet | **SAME** |
| does 1.3's reference itself reflect a real 1.3 binary exactly | -- | -- | **UNKNOWN** -- `assets/replay_checker/` is a 2010 clean-room reimplementation validated against `.itr` replays of *some* 1.3 build, not a disassembly; treat every "1.3" row above as "as the reference documents it," not as independently disassembly-verified the way the 1.5.1 column is |
| whether `tiles`/`sign` have a not-yet-promoted reader | -- | both are written every real/checkpoint call but read by nothing in `src/icytower/map.c`'s existing consumers (`is_solid`, `getFloorData`, `get_level`) | **UNKNOWN** -- plausible they feed a drawing function (background tile selection, on-screen height-marker text) that has not been promoted yet; not ruled out, not confirmed |

## 3. Verification

`carrier/lift/harness/lift_check.py --form src --funcs add_floor` (MSVC,
`src_check.exe`) and `--toolchain gcc` (`gcc_check_x87_nosse_O2.exe`, 32-bit
MinGW GCC 16.2.0, `-m32 -mfpmath=387 -mno-sse2 -O2`) both report **EQUAL**
over 4 seeds x 20000 vectors = 80000 vectors each (160000 total), comparing
the whole 772-byte `Tmap` (no return value). Negative control
(`--fault add_floor:5:0`) DIFFERs and names the flipped byte exactly
(`Tmap+0x0 (VA 0x00792000)`). GCC x87 is the toolchain of record because
`add_floor`'s width formula (the `k<=2999, floor_shrink!=0` branch) is x87-
sensitive in the same way `line_intersect`/`new_rand` are (a `fidivr`/`fmuls`
pair kept on the x87 register stack, not spilled to memory between them);
MSVC (plain `float`/SSE) came back EQUAL too on all 4 seeds tested here, but
that is not a guarantee the way GCC x87's bit-for-bit match against the
unicorn-executed original bytes is -- the vector generator (`gen_add_floor`
in `lift_check.py`) does not specifically construct float-truncation-
boundary inputs for this ratio the way `line_intersect`'s `_boundary()`
helper does for `ua*dx1+0.5`, so an adversarial MSVC-vs-original divergence
here has not been ruled out, only not found in 80000 semi-random vectors.

### Harness-only rand() shim (why it was needed)

`add_floor` is the first promoted function to call the game's own `rand()`.
Two problems, both harness-only (nothing in `src/icytower/map.c` itself
knows about either):

1. **unicorn has no msvcrt.dll mapped.** The game's `_rand` thunk
   (0x4bad18: `jmp *[0x514944]`, an IAT slot) is unreachable there --
   `call 0x4bad18` would fault. Fixed by hooking that address in
   `lift_check.py`'s `Oracle` (`UC_HOOK_CODE` at `RAND_THUNK_VA`): the hook
   emulates msvcrt's LCG in Python from a per-vector seed (written to a
   scratch VA, `RAND_SEED_VA = 0x794020`, by `gen_add_floor`), writes EAX,
   and simulates the eventual `ret` itself (pop the return address, redirect
   EIP, `emu_stop()`) instead of letting the `jmp` execute. `Oracle.call()`
   loops `emu_start()` until it reaches the real return address, resuming
   from wherever the hook redirected it in between.
2. **the compiled candidate (`src_check.exe`/`gcc_check_*.exe`) must draw
   from the exact same LCG, seeded the same way.** `carrier/lift/harness/
   pf_harness_rand.h` (force-included: `/FI` on MSVC, `-include` on GCC)
   redirects `map.c`'s `rand()` calls to `harness_rand()`
   (`carrier/lift/harness/harness_rand.c`, the same LCG, its own state word
   `harness_rand_state`), pulling `<stdlib.h>` in first so only `map.c`'s
   own token is renamed, never `<stdlib.h>`'s own declaration text. Both
   drivers set `harness_rand_state` from `RAND_SEED_VA` immediately before
   calling `add_floor`. `src/icytower/map.c` itself calls plain, ordinary
   `rand()` throughout -- this redirection exists only for the offline
   harness's own build and is documented as harness-only in
   `pf_harness_rand.h`'s own header comment, exactly like `src_check.c`'s
   pre-existing `tr()`/`untr()` memory-model shims.

### A second harness bug this pass found and fixed

`add_floor` calls `get_demo()` internally (not through its own parameter),
and `get_demo()` just relays the `demo` global's pointer *value* verbatim
(its own `PROMOTIONS.md` entry: "never dereferenced ... so no host/guest
translation is needed anywhere" -- true for `get_demo()` alone, false for a
caller that dereferences the result). `src_check.c`'s vector-driven `demo`
write stores a **guest VA** (matching the wire format every other pointer-
shaped write uses); nothing translated it to a host pointer before
`add_floor`'s `get_demo()->floor_shrink` dereferenced it, so the very first
multi-vector run segfaulted. Fixed the same way `update_frame`'s
`ply[player_id]` pointer already needed a second, driver-side translation
(`src_check.c`'s own header comment): overwrite the `demo` slot with the
translated host pointer immediately before calling `add_floor`.
`gcc_check.c` never had this bug -- its `demo` is a plain global the driver
already assigns via `tr()` explicitly, the same way `jump_player`'s
`collision_type`/`max_speed` are synced there.

### Two wrong divisors this pass found and fixed

`room[31].tiles` and `room[31].sign` are each computed with a magic-multiply
reciprocal in the disassembly (`0x10624dd3>>5` and `0x66666667>>1`), not a
plain `idiv` with a literal constant. An early hand-read guessed 20 and 10
respectively, by eyeballing the constants against a standard table of
magic-number-division constants -- both guesses were wrong. The first
20000-vector run (after the `demo`-pointer fix above) reported a DIFFER at
`Tmap+0x2fc` (`tiles`); both divisors were then re-derived correctly (500
and 5) by brute-force testing the magic-multiply arithmetic against every
plausible small divisor over a 0..20000 sweep in Python -- the same
technique `particle.c`'s `create_particle()` note already documents using
for its own two magic constants (`/10` and `/50`, also initially
mis-guessed by pattern-matching). `tiles = k/500` (not `k/20`) reaches its
cap of 10 exactly where the `k<=4999` gate switches to it (`4999/500==9`),
which is a much more natural design than `/20` (which would already be
`>10` by `k=200`) -- a useful sanity check in hindsight, not something that
was used to derive the fix.
