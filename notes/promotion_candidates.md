# First native-promotion candidates — Icy Tower carrier

Scope: rank `origin=="game"` functions (`artifacts/functions.json`) as
candidates for the first per-function LIFTED/NATIVE experiment (win32_pilot.md
§3, milestones 10–12). KNOWN = read directly from `artifacts/disasm.txt` /
`artifacts/dwarf_info.txt` / `carrier/gen/interop_index.json`. INFERRED =
reasoned from KNOWN facts, not independently re-verified byte-for-byte. Full
per-candidate data (globals, struct fields, comparison domains) is in
`artifacts/promotion_candidates.json`; this file is the summary + method +
verification-mechanism proposal.

## 1. Method (KNOWN)

- **Call graph**: parsed every `call 0x...` edge per function block in
  `disasm.txt` (2952 blocks), BFS from the four anchors `play` (0x411a00),
  `update_frame` (0x406ac4), `handle_player_input` (0x40b3e4), `draw_frame`
  (0x40929c). 450 functions reachable total, **124 of the 253 game-origin
  functions** are statically reachable from gameplay.
- **Size window**: 50–400 bytes → 58 of those 124 qualify.
- **Leaf/near-leaf**: no non-game callees and no indirect calls → 24 of the 58.
- **Switch tables**: `jmp *0xADDR(,%reg,4)` pattern per function → **0 of the
  58** in-window reachable candidates have one (the 54 switch sites in
  `binary_recon.md` item n live elsewhere in the binary, mostly non-game or
  outside the size window).
- **x87 instruction count**: mnemonic match against the full x87 set per
  instruction line.
- **Callback scan**: grepped the whole disasm for each shortlisted VA
  appearing as an immediate operand outside a call-target position (i.e.
  address-taken / stored in a function-pointer table). **0 hits for all 8** —
  none of them are reachable from Win32 (WNDPROC, driver vtables, menu
  callback arrays) any other way than the direct calls listed below.
- **Globals/structs**: cross-referenced every absolute-address operand and
  every struct-pointer parameter against `carrier/gen/interop_index.json`
  (156 typed globals, 242 typed functions) and `artifacts/dwarf_info.txt`
  `DW_AT_data_member_location` offsets (not against hand-guessed struct
  layout — every offset below is DWARF-confirmed).

## 2. Top 8 candidates

| # | name | VA | size | CU | x87 | callers (game) | struct/globals |
|---|------|----|-----:|----|----:|---|---|
| 1 | `update_frame` | 0x406ac4 | 120 | main.c | 0 | play | reward_time/reward_scale/player_id/ply, `Tplayer.dead/edge/edge_drawn` |
| 2 | `is_solid` | 0x4166dc | 107 | map.c | 0 | handle_player_collision_{original,old,combo} | `Tmap.room[].{empty,start_tile,end_tile}` |
| 3 | `getFloorData` | 0x416770 | 107 | map.c | 0 | handle_player_collision_{combo,vector,vector_2} | `Tmap.room[].{empty,start_tile,end_tile,tiles}` |
| 4 | `jump_player` | 0x418678 | 198 | player.c | **22** | handle_player_input | collision_type, max_speed[5], `Tplayer.{status,sx,sy,max_s,rotate,angle}` |
| 5 | `add_combo` | 0x40414c | 62 | game_data.c | 0 | play | `Tgame_data.{comboPosts,combos[]}`, `Tgd_combo.{start,end,length}` |
| 6 | `reset_map` | 0x4166a4 | 53 | map.c | 0 | new_game | `Tmap.room[].{empty,level,sign}`, `Tmap.offset` |
| 7 | `line_intersect` | 0x406b80 | 302 | main.c | **37** | handle_player_collision_{combo,vector,vector_2} | one unnamed .rdata literal; explicit FPU control-word save/switch/restore |
| 8 | `play_jump_sound` | 0x406ecc | 141 | main.c | 5 | handle_player_input | `Tplayer.sy`; 2 unnamed thresholds + 3 unnamed sound-handle globals; calls `play_sound` (game) |

All 8: `imports_used = []`, `indirect_calls = 0`, `switch_tables = 0`,
`callback_address_taken = false`. #8 is the one deliberate near-leaf (calls
one other game function, never an import directly), included because the
criteria explicitly allow it and it forces the verification design to handle
a "no local writes, verify the downstream call instead" case (§4).

Reachability paths (shortest, KNOWN from the BFS):

```
update_frame        <- (is itself an anchor)
is_solid             <- play -> handle_player_collision_original -> is_solid
getFloorData         <- play -> handle_player_collision_combo -> getFloorData
jump_player          <- handle_player_input -> jump_player
add_combo            <- play -> add_combo
reset_map            <- play -> do_replay_menu -> run_demo -> new_game -> reset_map   (COLD path, see below)
line_intersect       <- play -> handle_player_collision_combo -> line_intersect
play_jump_sound      <- handle_player_input -> play_jump_sound
```

**Caveat (KNOWN in this static BFS, INFERRED as a general claim):**
`reset_map` is reachable only through the replay-menu/new-game setup path in
this call graph, not the per-tick physics loop — it runs once per game start,
not every frame. Kept in the top 8 for struct diversity (it is the *producer*
of `Tmap`, complementing `is_solid`/`getFloorData` as *consumers*, same CU,
same struct, zero x87), but a live gameplay trace should confirm the normal
"press enter to start" flow also reaches it before it is used as a milestone
10 target.

## 3. Descriptions (condensed — full text with byte/offset evidence in the JSON)

1. **`update_frame`** (KNOWN offsets, INFERRED intent) — per-tick maintenance:
   decays/grows a fixed-point `reward_scale` HUD value based on `reward_time`,
   and advances the current player's `dead` death-animation counter (+8/tick,
   capped by range test against 0/299) and `edge_drawn` counter. Zero callees.
2. **`is_solid`** (KNOWN) — pure predicate: row-indexes `Tmap.room[32]` from a
   pixel Y, bounds-checks, and returns 0 or a solid-surface Y. No writes at
   all — best negative-control candidate (assert `*map` untouched).
3. **`getFloorData`** (KNOWN struct, INFERRED field roles) — same row lookup
   as `is_solid`, writes floor-bottom/floor-top/offset through 3 `int*` outs.
4. **`jump_player`** (KNOWN offsets, INFERRED gameplay semantics) — **the
   physics pick**. Computes launch velocity from current horizontal speed,
   clamps against `max_speed[collision_type]`, writes `Tplayer.sy`/`max_s`
   through a chain of `fadd`/`fld`/`fmuls`/`fucom`, and conditionally starts
   the jump-spin animation (`rotate`/`angle`). `arg2!=0` is a separate
   "forced jump" (bounce) branch: `sy = -(arg2*12)` via `fildl`/`fstpl`. This
   is exactly the win32_pilot.md §3 HYPOTHESIS case — GCC keeps 80-bit x87
   intermediates that a naive `double`-only NATIVE/LIFTED form may round
   differently at the last bit.
5. **`add_combo`** (KNOWN, DWARF-cross-checked) — bounds-checks
   `Tgame_data.comboPosts` against 5000 and appends a copied `Tgd_combo`
   record; a clean, fully-typed, zero-ambiguity struct-mutation example.
6. **`reset_map`** (KNOWN) — zero-fills `Tmap.room[32].{empty,level,sign}`
   and `Tmap.offset`; smallest candidate (53B); deliberately leaves
   `start_tile`/`end_tile`/`tiles` stale (consumers gate on `empty` first).
7. **`line_intersect`** (KNOWN) — **alternate x87 pick**: 2D line-segment
   intersection, 37 x87 instructions, includes an explicit
   `fnstcw`/`fldcw`-guarded round-toward-zero `fistpl` pair around the final
   int conversion — the single best "x87 control-word fidelity" stress case
   in the whole shortlist if `jump_player` alone proves inconclusive.
8. **`play_jump_sound`** (KNOWN) — reads `Tplayer.sy`, picks 1 of 3 unnamed
   sound-handle globals by threshold, calls `play_sound` (game function).
   Its own writes are empty; its comparison domain is the *downstream call*,
   not its own post-state (see §4.3).

**Unnamed globals** (KNOWN addresses, no DWARF/COFF symbol recovered by
`gen_interop.py`): float literals at 0x4d7130/0x4d7134 (`jump_player`),
0x4d6cb4 (`line_intersect`), 0x4d6cc4/0x4d6cc8 and word globals
0x4fabf4/0x4fabf8/0x4fabfc (`play_jump_sound`). These must still be read
verbatim by address in both LIFTED and NATIVE forms — they have no `IT_G_*`
macro to hang a name on, so the comparator has to know their raw VAs.

## 4. Suggested comparison domain (per function; full detail in JSON)

1. `update_frame`: `reward_scale` (4B) + `Tplayer.dead`/`edge_drawn` (or the
   full 184B `Tplayer` struct, cheap and already typed).
2. `is_solid`: EAX only; `*map` must be byte-identical (negative control).
3. `getFloorData`: the 3 output dwords through caller-supplied pointers.
4. `jump_player`: EAX + `Tplayer.{status,sy,max_s,rotate,angle}`; **`sy`/
   `max_s` need a bit-exact dword compare, not an epsilon compare** — that
   bit-exactness *is* the experiment.
5. `add_combo`: `Tgame_data.comboPosts` (4B) + the newly-appended
   `combos[old comboPosts]` (12B).
6. `reset_map`: full `Tmap` (772B) — cheap, and precise fields don't save much.
7. `line_intersect`: EAX + the 2 output dwords (only valid when EAX==1); the
   x87 control word must be observed unchanged after return.
8. `play_jump_sound`: no local domain — verify which of the 3 sound-handle
   values was passed as arg0 to the downstream `play_sound` call, via the
   existing call-trace stub (win32_pilot.md §3's binding-table counter),
   not a register/memory digest of `play_jump_sound` itself.

## 5. Per-function verification mechanism (PROPOSED — none of this exists yet)

`carrier/src/main.cpp:432` already registers one VEH
(`AddVectoredExceptionHandler(1, veh_handler)`), today used only for
crash diagnostics (EIP/register dump + EBP-chain stack walk on unhandled
exceptions — KNOWN, read directly from `main.cpp`). No hardware-breakpoint
code exists yet (`grep DR0/DR1/CONTEXT_DEBUG_REGISTERS` in `carrier/src/*`:
zero hits) and the tick safepoint at 0x4124f4 itself is still milestone 8,
**pending** per win32_pilot.md §8. The design below is new work that reuses
the *existing* VEH registration point rather than adding a second handler:

```
per candidate function F at VA entry, size S (return address = call site,
recovered from the return-address-on-stack convention of cdecl):

  1. install two hardware breakpoints via SetThreadContext on the guest's
     one game-logic thread (the main thread; win32_pilot.md §4a — only two
     Allegro threads ever touch game state, and F executes on the main one):
       DR0 = entry      (F's VA)     DR7 execute-breakpoint, len=1
       DR1 = return-site (read once from [ESP] at the DR0 hit, then armed)
  2. VEH sees EXCEPTION_SINGLE_STEP with DR6 bit0 (DR0) set:
       - capture argument dwords from the cdecl stack: [ESP+4], [ESP+8], ...
         up to the arity from the typed prototype in it_funcs.h (no guessing
         — arity is already generated)
       - compute and store a digest (e.g. FNV-1a or CRC32, cheap) of the
         function's pre-declared comparison domain (§4): named globals by
         address, struct fields by (base-pointer-from-argument + offset),
         each already typed by carrier/gen/it_types.h so the digest routine
         is generated once per candidate, not hand-written per byte range
       - re-arm DR1 at the return address now known from [ESP]
  3. VEH sees EXCEPTION_SINGLE_STEP with DR6 bit1 (DR1) set:
       - capture EAX (return value) for non-void prototypes
       - capture a second digest of the same comparison domain (post-state)
       - emit one record: {va, invocation_index, args[], pre_digest,
         post_digest, eax}; invocation_index is a per-VA monotonic counter,
         the coordinate the diff runs on (not wall-clock time — matches the
         tick-indexed replay model in win32_pilot.md §5/§6)
       - disarm DR1, DR0 stays armed for the next call
  4. run the same replayed input script (notes/replay_format.md §5: seed +
     RLE key stream + 20ms tick counter) twice — once with F bound to
     ORIGINAL, once with F bound to LIFTED (or NATIVE) via the binding-table
     entry patch already specified in win32_pilot.md §3 — and diff the two
     record streams by invocation_index: first differing pre_digest means
     the harness itself desynced upstream of F (bug in the replay driver,
     not F); first differing post_digest with equal pre_digest and args is
     the actual verdict, named as (va, invocation_index, byte) per §7,
     never a percentage.
```

Why DR0/DR1 (not INT3 patching): does not touch the guest's original bytes
(win32_pilot.md §3's "original address is the identity" requirement — the
binding-table's 5-byte entry patch is reserved for *routing* which form
runs, not for instrumentation) and both breakpoints share the one existing
VEH so no second exception-dispatch layer is added. DR2/DR3 stay free for a
second concurrently-verified candidate (e.g. running #1 and #4 in the same
replay pass) — up to 4 candidates instrumented per session before running
out of debug registers, which comfortably covers milestone 10–11's
one-function-at-a-time plan with headroom for #4's negative-control byte-
fault injection (§7) to run alongside the primary candidate in the same
pass.

For `play_jump_sound` (§4.3's no-local-domain case): no DR1 needed at all —
the *existing* import/game-call trace stub already logs args at every
counted call site (win32_pilot.md §3's trampoline mechanism); the verdict
for this class of near-leaf function is a diff of that trace, keyed by the
same invocation_index, not a new digest mechanism.

## 6. Recommendation

- **First pick (no x87): `update_frame`** (0x406ac4, 120B). Zero callees,
  zero x87, is itself one of the four call-graph anchors, globals already
  typed by name (`reward_time`, `reward_scale`, `player_id`, `ply`), and its
  comparison domain is tiny (one fixed value + two int fields of one
  `Tplayer`). Cleanest possible milestone-10/11a target.
- **Second pick (x87, physics): `jump_player`** (0x418678, 198B, 22 x87
  instructions). Directly tests the win32_pilot.md §3 HYPOTHESIS about
  80-bit x87 intermediates vs. `double`, on the exact mechanic (jump launch
  velocity) where a one-bit rounding difference is player-visible (jump
  height/combo threshold). `line_intersect` (37 x87, explicit control-word
  save/restore) is the escalation target if `jump_player` alone doesn't
  surface a fidelity gap.
- `is_solid`/`getFloorData`/`add_combo`/`reset_map` round out the set as
  low-risk, high-confidence, fully-typed struct-mutation/predicate examples
  to exercise the comparator machinery itself before spending it on the
  harder x87 cases.
