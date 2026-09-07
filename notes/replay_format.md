> CORRECTION 2026-09-08 (batch 11, src/icytower/handle_player_input.c, verified offline): the 0x80 key_flags bit is an idempotent END-OF-INPUT terminator written when ply->dead is set, stored at data[rec_pos+1..+2] without advancing the cursor; decoder and encoder use rec_pos with a one-record bias. The 'sentinel per frame' reading below is superseded.

# Icy Tower 1.5.1 replay-recording system — decoded

Scope: `assets/icytower15.exe`, game CU `replay.c` (+ shared struct defs also
emitted into `game_data.c`/`main.c` CUs), gameplay driver `play()` (0x411a00).
Claims tagged **KNOWN** (seen directly in `artifacts/dwarf_info.txt` or
`artifacts/disasm.txt`, address/offset given) or **INFERRED** (reasoned from
KNOWN facts, not independently re-verified byte-for-byte).

## 1. Replay struct layout + 8-byte per-frame record

**KNOWN (DWARF):** two typedefs at `<0xd9c4>`/`<0xd9d3>` (game_data.c CU,
decl_file 12), duplicated verbatim in the main.c CU (`<0x1b1a3>`) — same
offsets both places.

`Treplay`, **2220 bytes** (`0x8ac`, matches `create_replay`'s
`malloc(0x8ac)` at 0x41cce8). Key fields (offset dec/hex : name):
0/0x0 `header` char[6]; 8 `size` int (record count); 12/0xc `name` char[32];
44/0x2c `date` char[32]; 76/0x4c `checksum` uint; 80/0x50 `score`;
84/0x54 `floor`; 88/0x58 `combo`; 92/0x5c `no_combo_top_floor`;
96/0x60 `biggest_lost_combo`; 100/0x64 `ccc[5]`; 120/0x78 `jc[5]`;
140/0x8c `floor_shrink`; 144/0x90 `floor_size`; 148/0x94 `start_speed`;
152/0x98 `speed_increase`; 156/0x9c `gravity`; 160/0xa0 `rejump`;
**164/0xa4 `random_seed` (int)**; 168/0xa8 `comment` char[42];
212/0xd4 `tc_posts`; 216/0xd8 five `int[100]` stat arrays
(`tc_c_data`/`tc_q_data`/`tc_t_data`/`tc_s_data`/`tc_f_data`, 400B each);
**2216/0x8a8 `data`: `Trecord *`** — pointer to the flat per-frame record
array. `random_seed` living inside the header is the answer to §2: the
map/RNG seed is part of every saved replay.

`Trecord`, **8 bytes** (`<0xd991>`, decl_file 12 line 13): offset 0
`key_flags` (unsigned char); offset 4 `cycle_count` (int, 3 bytes padding).

**KNOWN (disasm, `handle_player_input` 0x40b3e4):** not "one record per
tick" — a **run-length-encoded key-state changelog**. `key_flags` is the
held input mask, `cycle_count` is how many more ticks it holds. Evidence:
same-state tick → `40b6b4: incl 0x4(%esi)` extends the *current* record's
`cycle_count`; changed-state tick (0x40b5de-0x40b5fa) bumps cursor `rec_pos`
and writes a **new** `Trecord{key_flags, cycle_count=0}`; playback
(0x40b4c3-0x40b60c) decodes `record.key_flags` then `dec`rements
`cycle_count`, advancing `rec_pos` only once it hits 0 (0x40b4d9).

**Bit layout of `key_flags` / live control byte `Tcontrol+0x20` (KNOWN,
`is_up/is_down/is_left/is_right/is_fire/is_enter/is_pause`,
0x401844-0x4018cd, each `movzbl 0x20(%eax); and $mask`):**
0x01 left (`is_left` 0x401874), 0x02 right (`is_right` 0x401888),
0x04 up (`is_up` 0x401844), 0x08 down (`is_down` 0x40185c),
0x10 fire/jump (`is_fire` 0x4018a0), 0x20 enter (`is_enter` 0x4018d0),
0x40 pause (`is_pause` 0x4018b8).

**KNOWN, important nuance:** what's actually *persisted* into
`Trecord.key_flags` is masked `AND 0x93` (bits 0,1,4,7) at 0x40b5e1 before
the change-comparison and store — keeping **left/right/fire only**;
up/down/enter/pause are dropped from the recorded stream (matches Icy
Tower's real controls: up/down are unused, enter/pause are menu-only).

**Write/consume site (KNOWN):** `handle_player_input()` (0x40b3e4), called
once per consumed tick from `play()` at **0x411f2a**. Branches on global
bool `recording` (VA `0x4f8e28`, DWARF name confirmed):
- `recording != 0` → capture (0x40b59c): calls `poll_control()` (0x401958)
  for fresh input, RLE-encodes into `demo->data[rec_pos]`.
- `recording == 0` → playback (0x40b406 fallthrough): decodes
  `demo->data[rec_pos-1]`, writes its `key_flags` into the live control
  struct (`mov %cl,0x20(%ebx)` @ 0x40b4cb) so downstream physics
  (`is_left`/`is_right`/`is_fire`, `jump_player`) treats it like live input.

Both branches share global `demo` (VA `0x4dd250`, `Treplay*`) and global
`rec_pos` (VA `0x4fec58`) — `demo` is set up by `create_replay()` for a new
recording or by `load_replay()` for playback; same code path either way.

**Input source (KNOWN, `poll_control` 0x401958):** merges keyboard *and*
joystick into one mask. Keyboard: Allegro key-state array at VA `0x506988`,
indexed by each control's configured scancode, OR'd bit-by-bit
(0x401977-0x4019ea). Joystick: `poll_joystick()` (0x43e654) then digital
d-pad/button state (0x506aa0-0x506ab4) plus an analog-to-digital lookup
table (0x506bc0/0x4f8758), OR'd into the same byte (0x4019f4-0x401a7c). So
joystick input is captured and indistinguishable from keyboard in the
saved replay (both just set bit 0/1/4).

**Side finding (KNOWN location, INFERRED purpose):** after game end,
`play()` (lexical block 0x4137ab-0x413930, locals `keys_pressed`,
`key_flag`, `last_keys`) re-reads the completed `demo->data` array
(`0x41387f`) against 7 fixed bit-patterns from VA `0x4d6c60` — a post-hoc
scan for special hold-sequences (cheat-code/reward triggers), not a
nondeterminism source, but proof the full stream stays meaningful post-game.

## 2. RNG seeding for a new game

**KNOWN — 3 `srand()` call sites (main.c):**
1. `init_game()` @ **0x40ef53**: `srand(time(NULL))` (`time()` @ 0x40ef4b) —
   generic startup seed before any game exists.
2. `new_game()` @ **0x40e0a0**: `srand(time(NULL))` (`time()` @ 0x40e098).
3. `new_game()` @ **0x40de24**: `srand(eax)`, `eax` still holding the
   untouched result of the **single `rand()` call** right after step 2
   (0x40e0a5) — the generator is re-seeded with the *first output* of the
   time-seeded generator.

**KNOWN:** that `rand()` result (0x40e0a5) is stored into global `rec_seed`
(VA `0x4fe7a8`) at 0x40e0aa **and** into `demo->random_seed`
(`Treplay+0xa4`) at 0x40e0b5, where `demo` = `0x4dd250` freshly set by
`create_replay()` (0x40e01a, right after `recording=1` @ 0x40dff8).

So the *effective* seed — the argument to the final `srand()` @ 0x40de24,
the state that actually drives every later `rand()` this game — **is
stored verbatim in the replay header at `Treplay+164`**. A carrier only
needs that one int; the `time(NULL)` step is a discarded intermediate.

**KNOWN:** `map.c add_floor()` (0x416956/0x4169bc/0x4169d3, per
`notes/binary_recon.md` item k) is the only *gameplay* `rand()` consumer
(`stars.c`/`fld_adspot.c` also call it but are cosmetic/ad-selection).
**Conclusion:** tower/map layout is fully reproducible from
`Treplay.random_seed` alone, and that field is saved on every new game.

## 3. Playback mechanism and nondeterministic sources

**KNOWN:** no separate "replay-playing" function exists — `play()` calls
the same `handle_player_input()` every tick; the global `recording` flag
selects which half runs (§1). `do_replay_menu` (0x410f98) / `run_demo`
(0x415e0c) just call `load_replay()` (0x41cde8) to populate `demo`, clear
`recording`, and hand off into the normal `play()` loop.

**KNOWN — not exactly one record per tick.** The RLE encoding (§1) means
one `Trecord` can cover many ticks. What *is* one-per-tick is the decode
step: each consumed tick calls `handle_player_input()` once.

**KNOWN — pacing is independent of the replay format** (per
`notes/binary_recon.md` item l): gated by tick counter VA `0x506938`,
incremented every 20ms by `cycle_counter` (0x41fed4). Safepoint **0x4124f4**.

**KNOWN — QPC/clock()/time() in `play()` are anti-cheat telemetry, not
inputs.** `assets/itrcheck.txt` (shipped with the game, read this pass)
documents it directly: "Time data is recorded in four different ways...
`<entry clk="50.00" qpc="49.94" tme="50.00" dns="50.13" flr="123" />`... If
the game was played at its intended speed, all four... should be close to
50.00." This matches DWARF locals in `play()`: `qpc_start/end/elapsed/
totQPCTimes` (QPC sites 0x412c7c/0x413075 etc.), `clockTimeStart/End/
Elapsed/totClockTimes` (`clock()`), `timeTimeStart/End/Elapsed/
totTimeTimes` (`time()`), plus `time_cheat_count` in the same lexical
scope. **INFERRED:** the 4th value ("dns") is the nominal elapsed time from
`0x506938`'s tick delta × 20ms (no extra syscall needed) — the baseline the
three real clocks are compared against. None of these feed back into
physics/RNG/the record stream; they land in the replay's statistics tail,
read by `icytower15.exe -check file.itr -sd` — confirmed statistics/
anti-cheat only.

## 4. On-disk `.itr` file format

**KNOWN (`create_replay` 0x41cce8):** allocates the 2220-byte header, then
`malloc(0x20 + frameCount*8)` for the record array (`data` field @
0x8a8); the `+0x20` slack is never touched by the zero-init loop (indexes
from offset 0) — INFERRED padding, not a sub-header. Copies a 6-byte magic
from `REPLAY_HEADER` (**VA `0x4d7dd0`**, `const char[6]`, DWARF-confirmed)
into `header`, a 7-byte tag from VA `0x4d7a7e` into `name`, literal ASCII
`"no date"` into `date` (placeholder, overwritten by `save_replay`). Exact
magic bytes not extracted — `disasm.txt`/`dwarf_info.txt` carry code only,
not `.rdata` contents; would need a raw hex dump at VA 0x4d7dd0.

**KNOWN (`save_replay` 0x41dd78):** formats a real date via `time()`+
`localtime()` into `date`; computes `calc_replay_checksum()` (0x41bac4)
into `checksum` (offset 0x4c); opens the file via Allegro `pack_fopen`
(0x445afc — a `PACKFILE`, not raw `fopen`); writes sequentially via
`pack_fwrite` (0x4449c8): 6B `header`, 4B `size`, 32B `name`, 32B `date`,
... (trace stopped here; INFERRED the rest follows struct order through
`checksum`/`score`/.../`random_seed`/`comment`/`tc_*`, then the
`frameCount*8`-byte `Trecord[]` — i.e. a near-verbatim `Treplay`
serialization, symmetric with `load_replay` 0x41cde8 per
`notes/binary_recon.md`).

**KNOWN — checksum covers the entire input stream, not just metadata**
(`calc_replay_checksum_131` 0x41ba10, fully disassembled): mixes
`random_seed`(0xa4)/`rejump`(0xa0)/`score+1`/`floor+1`/`combo+1` via
shift/multiply; a rolling-multiplier hash (mult starts 0x75, +=0x75/byte)
over `name[32]` and `date[32]`; then loops over **every one of the `size`
records**, hashing `(key_flags*5 + cycle_count*3) * position`. Any bit
changed anywhere in the input stream changes this checksum. The larger
`calc_replay_checksum` (0x41bac4, 676B, presumably the "current" vs. this
legacy "131" version) wasn't step-traced. Version-gating string
(`"You need Icy Tower 1.2/1.3..."`) carried over from `notes/binary_recon.md`
item i, not re-verified here.

**`itrcheck.txt`** independently confirms design intent: replays are
checked via `icytower15.exe -check file.itr` (built into the exe since
1.4), flags `-jumps`/`-combos`/`-keys`/`-sd`/`-all`/`-tiny` dump an XML
summary — `-keys` dumps "all key presses", confirming the record stream is
literally the keypress log decoded in §1.

**Could an original `.itr` feed the carrier?** INFERRED yes in principle —
it holds everything needed (seed + RLE key-state stream) in a documented,
checksummed format — but would need: (a) header parse for `size`/
`random_seed`/`checksum`, (b) the `frameCount*8`-byte record array,
(c) verifying or ignoring `checksum` (covers not-yet-fully-mapped `tc_*`
tail too), (d) driving the carrier loop with the RLE decode logic (§1), not
a flat 1-record-per-tick read.

**No `replays/` directory currently exists** (confirmed: `assets/` has
`Shaders`, `cache`, `characters`, `data`, `profiles`, `screenshots`,
`tower.cfg`, `itrcheck.txt`, etc., no `replays/`). The game creates it on
demand via `mkdir("replays")` (0x413b52-0x413b59, guarded by a
first-auto-save flag @ `0x4dd188`) the first time it auto-saves a new best
replay; a player can also produce one manually via the in-game "save
replay" prompt (`replay_selector`/`draw_replay_selector`,
0x41d258/0x41be58) invoking `save_replay()`.

## 5. Carrier design conclusion

**"`srand` seed (`Treplay.random_seed`, offset 164) + 8-byte RLE record
stream + the 20ms tick counter (`0x506938`)" is sufficient to define the
deterministic gameplay-input surface** — KNOWN, supported throughout:
- Seed captured once/game, stored in the header (§2).
- The only nondeterministic physics input is the merged keyboard+joystick
  control byte, RLE-encoded into the same struct/cursor used for playback
  (§1) — playback proves this by construction, running identical physics
  code from decoded records instead of live polling.
- Tower generation is the only other gameplay `rand()` consumer, seeded
  from the same stored value (§2).
- Wall-clock/QPC reads are demonstrably read-only anti-cheat telemetry,
  not simulation inputs (§3, corroborated by `itrcheck.txt`).
- `itrcheck.exe`'s `-jumps`/`-combos` flags reconstruct jump/combo events
  purely from the `.itr` file with no live engine — Free Lunch Design's own
  tooling already treats "seed + record stream" as a complete, replayable
  description of a run.

**What remains outside this core (KNOWN/INFERRED):**
- **Menu navigation, profile selection, character choice, difficulty**:
  all happen before `create_replay()`/the reseed in `new_game()`; not
  captured in `Trecord`. A carrier needs these set up out-of-band (profile
  @ `0x4dd27c`, `curr_char`/`characters` @ `0x4dd270`/`0x4fdcc8`,
  difficulty lookup via `0x4fe518`/`0x4ff128`) before replaying the stream.
- **`enter`/`pause` bits** are masked out of the recorded stream (§1) —
  pause/menu-confirm during live play isn't reproducible from the replay
  (though also provably inert to game state, since pause just halts ticks).
- **Sound**: sample/music calls are driven by game-state transitions
  (jumps/combos/floors) which are themselves fully determined by
  input+seed — INFERRED a deterministic consequence, not an independent
  input, but not part of the recorded surface either way.
- **`0x80`-sentinel record path** in `handle_player_input`
  (0x40b634/0x40b645, reached when a difficulty-table field is nonzero, or
  a stored record's `key_flags` has its sign bit set) writes a marker
  record + a zeroed record instead of the normal RLE update. Location
  KNOWN; exact semantics (INFERRED: practice-mode/segment-boundary marker)
  unresolved — flagged for follow-up if full round-trip fidelity matters.
- **Checksum tail fields** (`tc_c_data`/`tc_q_data`/`tc_t_data`/
  `tc_s_data`/`tc_f_data`, offsets 216-2216, 1600B total per-column stats)
  fold into `calc_replay_checksum` (the untraced 676B version) — KNOWN to
  exist, INFERRED to be derived/recomputable stats rather than additional
  inputs, since nothing writes to them from `handle_player_input`/
  `poll_control`.
