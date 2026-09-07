# Icy Tower 1.5.1 -- player start position (Tplayer.x/.y)

Scope: `assets/icytower15.exe`, `notes/layout_determinism.md` +
`notes/replay_format.md` reused directly. Tplayer: `x` offset 0, `y` offset
8 (`src/icytower/game_types.h`). Player pointer accessed everywhere via the
same indirection: `mov 0x4fe518,%reg` (index) then
`mov 0x4ff128(,%reg,4),%reg` (pointer table lookup) -> `Tplayer*`.

## 1. `reset_player` (0x418550) -- KNOWN, does NOT write x/y

Full disassembly read (0x418550-0x418677): first two writes are
`fldz`/`fstl 0x10(%eax)` and `fstl 0x18(%eax)` (`sx`,`sy` = 0.0), then a
long run of `movl $0,<offset>(%eax)` for every other field
(`status,jump_key,frame,...,ccc[],jc[]`) plus one `fstpl 0x20(%eax)`
(`max_s`=0.0). **Offsets 0x0 and 0x8 (`x`,`y`) never appear.** Matches
`notes/layout_determinism.md`'s prior finding.

**KNOWN, exhaustive:** `reset_player` has exactly **one call site in the
whole binary**: `new_game` **0x40de65** (grepped `call ... 418550` across
`disasm.txt`, single hit).

## 2. `new_game` (0x40dc9c) -- KNOWN, writes x/y as hardcoded constants

Sequence, in order:
- 0x40dcba: `rand()` -> `%0x18ff8` -> stored as double at global `0x4ff108`
  (0x40dccf) -- this is `new_srand`'s seed, per `layout_determinism.md`
  §3/§1. Runs **before** everything below on the `demo==NULL` path; on the
  `demo!=NULL` path this instruction still executes first (it's before the
  `demo` NULL-check at 0x40dd1b), so it runs unconditionally every
  `new_game()` call.
- 0x40de24: `srand(eax)` -- the real per-game reseed (fresh time-seed
  result, or reloaded `demo->random_seed`, per `layout_determinism.md` §1).
- 0x40de3c: `reset_map()` (0x4166a4).
- 0x40de44-0x40de54: `for(i=0;i<30;i++) add_floor()` -- builds floors 0..29
  from the just-reseeded libc `rand()` stream.
- **0x40de65: `call 418550 <reset_player>`** on the player-table pointer
  (`0x4fe518`/`0x4ff128` indirection, re-loaded fresh at 0x40de56-0x40de5b).
- **Immediately after, same pointer reloaded again (0x40de6a-0x40de6f) and
  written directly, KNOWN:**
  ```
  40de76: movl $0x00000000,(%eax)      ; x low32
  40de7c: movl $0x40690000,0x4(%eax)   ; x high32  -> x = 200.0
  40de83: movl $0x00000000,0x8(%eax)   ; y low32
  40de8a: movl $0x407af000,0xc(%eax)   ; y high32  -> y = 431.0
  40de91: movl $0x00000000,0x34(%eax)  ; status = 0 (out of struct order)
  40de98: movl $0xd2f1a9fc,0x10(%eax)  ; sx low32
  40de9f: movl $0x3f50624d,0x14(%eax)  ; sx high32 -> sx = 0.001
  ```
  Verified by direct IEEE-754 decode (`struct.unpack('<d',...)`): the two
  writes at 0x40de76/0x40de7c produce exactly **`200.0`**, and
  0x40de83/0x40de8a produce exactly **`431.0`**. This is byte-identical to
  the 1.3 reference's `init_state` (`assets/replay_checker/Icy Tower.cpp`:
  `x=200, y=431`). **1.5.1 does NOT differ from 1.3 here** -- the task's
  premise that it does is not supported by the disassembly.

**Neither constant is computed from any register, rand() result, or memory
read** -- both are `movl $imm32,...` with the literal encoded in the
instruction, i.e. **compile-time constants**, not derived from the seed,
`new_rand`, or the just-built floor data.

**Control-flow check (KNOWN):** both `new_game()` paths -- fresh
`demo==NULL` (time-seed) and `demo!=NULL` (replay-seed reuse, including
first-time replay playback) -- funnel through the *same* instruction
stream from 0x40de0c (post-reseed) through 0x40de91 (the x/y/status
writes); the branch at 0x40dd1b/0x40dda7 that distinguishes them resolves
long before this point, and there is no second branch in between. So the
x=200.0/y=431.0 write is unconditional and identical for every call.

## 3. `play()` init (0x411a00+) -- KNOWN, only READS x/y, never writes

Before the first `handle_player_input()` call (0x411f2a, matches
`notes/replay_format.md` §1), `play()` does:
```
411ed2-411ede: reload ply ptr, fldl (%eax)        ; read x
411eef-411efc: fistpl -0x940(%ebp)                ; round x to int (screen coord)
411f08-411f11: fldl 0x8(%eax); fistpl -0x924(%ebp) ; read y, round to int
```
Both are `fldl` (load, not `fstl`/`fstpl` store) -- pure reads, used to
seed an integer screen-position local (likely initial camera/scroll
framing). **No write to offsets 0x0/0x8 anywhere in `play()`** (checked
every `fstpl (%reg)`/`fstpl 0x8(%reg)` site in `disasm.txt` — the ones
inside `play()`'s address range, 0x412042/0x412833/0x4130fa, target `%esi`,
not the ply-table pointer pattern, i.e. unrelated locals).

## 4. Replay playback path -- KNOWN, reuses the identical code

`do_replay_menu` (0x410f98) on selecting a replay calls `run_demo`
(0x4110e7 -> 0x415e0c). `run_demo`:
```
415e32: call load_replay   ; populates demo (0x4dd250) from the .itr file
415e4c / 415e8d: call new_game   ; the SAME 0x40dc9c analyzed in §2
415e96: call play
```
`new_game()` is not specialized for replay playback -- it is the literal
same function, so it reseeds libc `rand()` from `demo->random_seed`
(reloaded from the just-loaded `.itr`'s header, per
`notes/layout_determinism.md` §1), regenerates floors 0-29 from that seed,
calls `reset_player()`, then writes the same hardcoded `x=200.0,y=431.0`.
**There is no separate "replay start position" -- live play and replay
playback produce byte-identical Tplayer.x/y at game start.**

## 5. Answers

**(1) Is start x reproducible from the `.itr` header alone?** Yes, but
trivially: it is not even a function of the seed. `x=200.0`/`y=431.0` is a
fixed binary constant baked into `new_game()`'s code, identical on every
call (fresh game or replay), independent of `Treplay.random_seed`,
`new_rand`, or the generated floor 0-29 tiles.

**(2) Does start x depend on `new_rand`'s pre-reseed seed?** No (KNOWN).
`new_rand`'s state (global double `0x4ff108`, reseeded at 0x40dccf every
`new_game()` call from the pre-reseed libc `rand()` value) is never read
back into `Tplayer.x`/`.y` -- confirmed by `layout_determinism.md` §3
(exhaustive review of `new_rand`'s 18 call sites: all cosmetic, none touch
`Tplayer`) and independently here (the x/y writes at 0x40de76-0x40de8a are
literal immediates, not FPU loads from `0x4ff108`). So the question of
"is the pre-`new_game` libc draw count deterministic" is moot for start
position specifically -- it matters for `new_rand`'s cosmetic seed (per-run
particle/blit jitter reproducibility, out of scope here), not for x/y.

Replay playback does not "re-derive" x from the record stream, nor does
"the first record fix the position" -- the position is set once, by
`new_game()`, **before** `play()`'s tick loop (and therefore before
`handle_player_input`/`Trecord` decoding) ever runs.

**(3) What must the carrier record?** Nothing beyond what
`notes/layout_determinism.md` already concluded (`random_seed` +
`floor_shrink`/`floor_size`/`start_speed`/`speed_increase`/`gravity`) --
start position needs **zero additional state**: it is a constant the
carrier can hardcode (`x=200.0,y=431.0`) rather than reproduce from a
recorded value.

## 6. Open question -- the operator's observation (INFERRED, unresolved)

Given §2-5, the *Tplayer struct* start x cannot vary between games in this
binary. The operator's observed variance is therefore most likely **not**
a read of `Tplayer.x` at the true first frame, but one of (not traced this
pass, flagged for follow-up):
- Screen/camera scroll framing, which *is* seed-dependent once floors > 0
  are drawn (`getFloorData`/`is_solid`, `src/icytower/map.c`) -- floor 0
  itself is a deterministic checkpoint row per `layout_determinism.md`'s
  case table, so this would only show up after floor 0.
- Live divergence from recorded/live input acting within the first few
  ticks after spawn (expected behavior, not nondeterminism in the spawn
  value itself).
- `Tprofile.start_floor` (DWARF-confirmed field exists,
  `artifacts/dwarf_info.txt` offsets 0x1b5e3/0x2670f/0x2fd9f) -- if this
  lets a game start higher in the tower, a non-floor-0 spawn could pick up
  seed-dependent tile geometry. **Not traced this session** -- no call
  site for `start_floor` located; would need a follow-up grep for its
  offset in `disasm.txt`.
