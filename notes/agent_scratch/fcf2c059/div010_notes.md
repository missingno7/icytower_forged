
## Divergence 010 - a promoted CALLER does not reach its promoted CALLEE through the guest's address space, and `play.c` divided the floor by 10 instead of 5 (2026-09-08)

Two defects, one carrier-side and one in recovered source, found in one
pass because the first one was hiding the second.

### 0. Scope note: what was in the build while this was measured

`src/icytower/logfile.c`, `draw_reward.c`, `screenshot.c` and `sound.c`
were untracked, in-flight batch-13 files when this pass ran, and
`logfile.c` does not link at all yet (unresolved
`_pthread_mutex_lock`/`_pthread_mutex_unlock` - `pthreadGC2.dll` needs
guest-IAT bindings one indirection deeper than the Allegro ones). All
four were TEMPORARILY excluded from the build for the duration of this
pass (`carrier/gen/build_blockers.json` + `win32_policy.json`'s
`scan_exclude`, both reverted before committing) so that `--bind
play=src` dragged in exactly batch 12's already-verified callee set and
nothing else. Every number below was measured with that build. When
batch 13 lands, its own in-vivo pass re-measures with those four in.

### 1. The mechanism both defects share

`carrier/gen/pf_bindings_src.h`'s header lists every promoted function as
**"Excluded (compiled natively, name kept free)"**. That is not a detail
of the generator - it is the fact that decides where a call goes:

* a call to `update_player(...)` from ORIGINAL guest code is a `call
  0x418740` and lands on the guest entry (which, if that function is
  bound, holds `bind.cpp`'s 5-byte `jmp rel32`);
* the same call from a `src/icytower/*.c` file compiled INTO the carrier
  is resolved **by the linker**, to the carrier's own `_update_player`.
  The guest VA is never executed, and `bind.cpp`'s entry patch is never
  crossed.

Two consequences, both measured this pass:

**(a) Binding `play=src` implicitly binds its whole promoted subtree.**
`--bind play=src` with no other row is NOT "one function changed": every
promoted callee `play()` reaches runs as src too. So the A/B that
isolates `play()`'s own body has to bind the callees on BOTH sides -
which is what `carrier/scripts/all_rows_src_no_play.bindfile` and
`all_rows_src.bindfile` (every row of `gen/bind_table.inc`, without and
with `play`) exist for. `carrier/scripts/all_src.bindfile` cannot do it:
it has no `draw_frame`/`draw_scroller` row, so with `play=original`
those two run ORIGINAL while with `play=src` they run src, and the
comparison is not about `play` any more. MEASURED: with the callees
bound on both sides, `all_rows_src_no_play` reproduces the fully-unbound
digest byte-for-byte (2293/2293 ticks), so the instrument itself is
neutral.

**(b) Every guest-VA sensor aimed at a promoted callee of a promoted
caller stops firing - silently, and in a way that reads like "no
difference".** Two of this carrier's three in-vivo instruments had this
defect:

| sensor | was | symptom with `play` bound |
|---|---|---|
| tick safepoint | `VA 0x4124f4`, INSIDE `play()`'s own bytes | 0 digest lines, 0 snapshots, `--stop-at-tick` never fires |
| frame oracle | `blit_to_screen`'s guest entry `0x40b6bc` | stream stops at the last frame ORIGINAL code drew (INVIVO batch 12's "silent GAP from T=236 to T=2725") |

The per-invocation `bind.cpp` sensor is the one that survives, because it
is a synchronous trampoline rather than an address.

### 2. The tick safepoint is now a FUNCTION-BOUNDARY sensor

Policy: `icytower::kTickSafepoint` in `carrier/win32_policy.hpp` -
`{caller "play", callee "update_player", callee_va 0x418740}`.
Mechanism: `carrier/src/det.cpp` `resolve_tick_safepoint()`, called from
`det_arm_main_thread()` (i.e. after `bind_init`, which is when "is the
caller bound" first becomes knowable), arms DR0 at

* `callee_va` when the caller is ORIGINAL - still correct when the
  CALLEE is bound, because an exec breakpoint fires on the ADDRESS, and
  the call still lands there to hit the `jmp` patch; or
* `bind_src_symbol("update_player")` when the caller is bound.

`bind.cpp` grew three read-only accessors for this (`bind_is_bound`,
`bind_form_name`, `bind_src_symbol`); all three answer from the SAME
generated `kFns[]` row, so there is no second copy of anything.

**Why `update_player` and not the obvious candidates.** KNOWN, from
`artifacts/disasm.txt`: `call 418740 <_update_player>` occurs at exactly
ONE address in the whole 794 KB image, `0x411f3e` = main.c 3703, which is
unconditional in `play()`'s tick-loop body (`play.c` line 760, and
`play()` contains no `continue`/`goto`). One call site in the image +
unconditional in the loop = exactly one hit per consumed tick and no hit
outside the tick loop. By contrast `update_frame` (0x406ac4) has FOUR
call sites - 0x411aef (3493, prologue), 0x41242a (4100, tick body),
0x414628 (4700) and 0x414976 (4800), the last two inside the game-over UI
loops, which are `readkey()`/wall-clock driven and therefore run a
host-timing-dependent number of times; and `blit_to_screen` has 22 call
sites and its tick-body one sits inside the frame-skip modulo
`someCounter__play % ffstep == 0`.

**BASELINE CHANGE (the reason `replays/*.digest` moved).** `0x418740`'s
entry is main.c 3703, near the START of a tick; `0x4124f4` was main.c
4369, its END. The digest CONTENT is unchanged (the same 151 globals from
`carrier/gen/game_globals.inc`), but it is taken at a different point of
the same tick, so the streams legitimately differ. MEASURED, and exactly
as predicted: the same 2293 lines for `human_test.txt` and the same 157
for the `.itr` workload, with every `T` shifted down by exactly one
(`T=236..2527` instead of `237..2528`; `393..549` instead of `394..550`)
and ESP 4 bytes lower (`0e1fef2c` - the pushed return address at a callee
entry). `replays/human_test.digest` and `replays/itr_last_game.digest`
were regenerated from UNBOUND runs; the pre-010 files are kept as
`artifacts/div010/*.digest.pre010`.

### 3. The frame oracle: both addresses armed, one de-duplicated

`carrier/src/frame.cpp` now arms the framework sensor at
`kFrameOracle.present_fn_va` AND a second breakpoint at
`bind_src_symbol(kFrameOraclePresentFn)`. Unlike the tick safepoint this
cannot pick one: `blit_to_screen` has 22 call sites and a single run has
callers of both kinds. The only case where ONE call crosses both is an
ORIGINAL caller reaching a BOUND `blit_to_screen` (guest VA -> `jmp` ->
stub -> src symbol); those two hits are back-to-back on the same thread
with nothing able to interleave, so `g_skip_next_src` collapses them.
Cost: 2 of the 4 debug registers, so a run that wants the frame oracle
AND `bind.cpp`'s ORIGINAL-form entry/return sensing (2 more) no longer
fits - it fails loudly at arm time rather than dropping frames.

### 4. The real divergence: one character in `src/icytower/play.c`

With a safepoint that survives promotion, `play=original` vs `play=src`
over `replays/human_test.txt` named its first differing tick immediately,
and `pf_inspect diff` on two snapshots named the first differing GLOBAL.
Three iterations:

| first differing tick | first differing global | cause in `play.c` |
|---|---|---|
| T=236 (the very first) | `checkMusicVoiceID` 3 vs 0 | main.c 3498 recovered as `play_sound(custom.bg_music, 0, 0)`; 0x414199 loads `0x4fac08` = `custom` + 1232 = `offsetof(Tcustom, yo)`, not +1240 = `bg_music`. A different (NULL) `SAMPLE*` -> no voice allocated -> the next `play_sample` returned voice 0 instead of 3. The same pass fixed `play_sample(bg_beat, 128, 1000, 1, TRUE)` -> `(bg_beat, 0, 128, 1000, TRUE)` (0x4141c5-0x4141e8 pushes vol=0, pan=0x80, freq=0x3e8, loop=1: the recovered line had the four numbers shifted one argument left) and two `custom.yo` -> `custom.wazup` on the pause screens (0x412ea0/0x413468 load `0x4fac0c` = +1236). |
| T=331 | `gdLastJumpDiff` 4 vs 2 | **the score/floor bug** (below). |
| - | none - EQUAL, 2293/2293 | - |

**Root cause, main.c 3864 / `play.c` line 916:**

```c
level = (get_level(&map, (int)ply[player_id]->y) - 1) / 10;   /* WRONG */
level = (get_level(&map, (int)ply[player_id]->y) - 1) / 5;    /* right */
```

The original is `0x412879`-`0x41288c`:

```
412879: lea    -0x1(%eax),%ecx        ; get_level(...) - 1
41287c: mov    $0x66666667,%ebx
412883: imul   %ebx
412885: sar    %edx                   ; <-- ONE-bit shift = /5
412887: sar    $0x1f,%ecx
41288a: sub    %ecx,%edx
```

The magic constant `0x66666667` is shared by `/5` and `/10`; the SHIFT is
what distinguishes them - 1 bit is `/5`, 2 bits is `/10`. The other
`0x66666667` site in the same function, `0x412a0c`, is the genuine `/10`
and does carry `sar $0x2` (it also has its final `sub` operands the other
way round, `sub %edx,%ecx`, so what it stores is `-(x/10)`: that is
`stars[p].sy` at main.c 4031, and the missing minus sign was fixed at the
same time - the "aight" stars fly UP).

So every floor the player reached was reported as **half of itself** for
the whole game. Everything downstream followed: `ply->level`, the combo
accounting, `gdLastJumpDiff`, the scroll-speed steps keyed on
`ply->level`, the saved `floor`, the score. The symptom batch 12 recorded
- "the player climbs at roughly HALF the rate, level 26 vs 13, final
score=662/floor=50 instead of 2386/100" - is exactly a factor of two, and
the fix restores it exactly.

**Why no offline oracle could see it, and what now would.** The
`(x-1)/10` line is inside the 16.9 KB of `play()` that PROMOTIONS.md
batch 12 verified only by an exact CALL-SITE CENSUS (82 callees, 0
mismatches). A census compares WHICH callee is called with WHAT
arguments; `get_level(&map, (int)y)` is called with the right callee and
the right arguments in both forms - the error is in what the caller does
with the RESULT, which no call-site census can see, and the divisor is
not a call at all. The region oracles batch 12 did build covered the
telemetry clear, the pause curtain and the end-of-game census; none of
them reaches main.c 3864. The check that catches this class is the one
this pass restored: a per-tick digest that keeps working after the
function under test is promoted. The generic lesson, also in
`notes/living_record.md`: **an instrument anchored at an address inside
the function it verifies stops being an instrument the moment that
function is promoted, and it fails SILENT-EMPTY, which reads like a
pass.**

### 5. Verdicts after the fix (all MEASURED this pass)

| check | result |
|---|---|
| G1 (newgame twice, unbound) | EQUAL, 876 ticks |
| G2 (`update_frame` src vs original, per-invocation) | EQUAL, 876 invocations |
| G3a (in-run rewind vs cold) | EQUAL, 300 rows T=400..699 and 601 rows T=400..1000 |
| G3b (rewind restores the fn sensor) | EQUAL, 300 invocations k=276..575 |
| G4 (human_test all-bound vs stored baseline) | EQUAL, 2293 ticks |
| G5a (.itr twice, unbound) | EQUAL, 157 ticks |
| G5b (.itr all-bound vs stored baseline) | EQUAL, 157 ticks |
| human_test, `play=original` vs `play=src` with callees src on both sides, per-tick digest | EQUAL, 2293/2293 |
| the same pair, frame oracle `every=1` | EQUAL, 2752/2752 frames, byte-identical files |
| newgame, unbound vs all-bound: digest / frames | EQUAL 876 ticks / EQUAL |
| human_test run to the game's own exit, `--bind play=src` ALONE (batch 12's exact repro) | `last_game.itr` **score=2386 floor=100**, `log.txt` reaches `Done...` |
| the same, `--bind-file all_rows_src.bindfile` | **score=2386 floor=100** |

Artifacts: `artifacts/div010/` (both baselines' pre-010 copies, the A/B
digests, the T=236 and T=331 snapshot pairs, the gate outputs).
