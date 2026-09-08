
---

## In-vivo pass -- play() RESOLVED (divergence 010, 2026-09-08)

This section supersedes findings 2 and 5 of the batch-12 section above.
Both are fixed; `play()` is now verified in vivo. The full account,
including the disassembly evidence for every line changed, is in
`carrier/NOTES.md` "Divergence 010"; what follows is the src/-side
summary.

### Finding 2 (the tick safepoint) was not harmless after all

Batch 12 filed "`--digest-out`/`--stop-at-tick` produce 0 records once
`play` is bound" as a *harmless carrier-layer limitation*. It was the
opposite: it was the reason finding 5 could not be localized, and the
same defect had also silently disabled the FRAME oracle (batch 12's own
"silent GAP from T=236 straight to T=2725" is that, not a property of the
recording). Both instruments were anchored at a guest VA that a promoted
caller never executes:

* `carrier/gen/pf_bindings_src.h` leaves every promoted name FREE
  ("excluded (compiled natively, name kept free)"), so `play.c`'s call to
  `update_player`/`blit_to_screen`/`draw_frame`/... is resolved by the
  LINKER to the carrier's own symbol. The guest address is never
  executed and `bind.cpp`'s entry patch is never crossed.
* Corollary that matters for every future in-vivo pass on a big
  function: **`--bind play=src` also switches every promoted callee of
  `play()` to its src form**, bindfile row or not. An A/B that means
  "what does this function's own body change" must bind the callees on
  both sides (`carrier/scripts/all_rows_src_no_play.bindfile` vs
  `all_rows_src.bindfile`).

Fixed carrier-side: the tick safepoint is now a FUNCTION-BOUNDARY sensor
at `update_player`'s entry (one call site in the whole image,
unconditional in the tick loop), resolved per run to whichever of its two
entry addresses that run can reach; the frame oracle arms both addresses
and de-duplicates. Baseline consequence: `replays/human_test.digest` and
`replays/itr_last_game.digest` were regenerated (same line counts, every
T shifted down by one, because the sensor now sits at main.c 3703 rather
than 4369).

### Finding 5 (score 662 / floor 50) -- four defects in `play.c`, one fatal

With the safepoint restored, three A/B iterations named each first
differing global and the `play.c` line behind it:

1. **T=236, `checkMusicVoiceID` 3 vs 0** -- main.c 3498 was recovered as
   `play_sound(custom.bg_music, 0, 0)`. `0x414199` loads `0x4fac08`,
   which is `custom` + 1232 = `offsetof(Tcustom, yo)`
   (`carrier/gen/it_types_check.c`), not +1240 = `bg_music`. Now
   `play_sound(custom.yo, 0, 0)` -- the character's "Yo!" at the start of
   a game.
   Two more member/argument errors of the same family were found by
   auditing every `custom.*` reference in `play()`'s disassembly against
   the file (the multiset of members did not match: the original uses
   `wazup` twice and `yo` once, the file used `yo` twice, `bg_music`
   once, `wazup` never):
   * main.c 4130 and 4199, the two pause screens: `custom.yo` ->
     `custom.wazup` (`0x412ea0`/`0x413468` load `0x4fac0c` = +1236).
   * main.c 3504 and 3553: `play_sample(bg_beat, 128, 1000, 1, TRUE)` ->
     `play_sample(bg_beat, 0, 128, 1000, TRUE)`. `0x4141c5`-`0x4141e8`
     pushes vol=0, pan=0x80, freq=0x3e8, loop=1; the recovered line had
     the four numbers shifted one argument left (the leading vol=0 was
     dropped).

2. **T=331, `gdLastJumpDiff` 4 vs 2 -- THE BUG.** main.c 3864:

   ```c
   level = (get_level(&map, (int)ply[player_id]->y) - 1) / 10;  /* was */
   level = (get_level(&map, (int)ply[player_id]->y) - 1) / 5;   /* is  */
   ```

   `0x412879`-`0x41288c` is the signed magic-divide idiom `lea
   -0x1(%eax),%ecx / mov $0x66666667,%ebx / imul %ebx / sar %edx / sar
   $0x1f,%ecx / sub %ecx,%edx`. `0x66666667` is the magic constant for
   BOTH `/5` and `/10`; the SHIFT decides -- `sar %edx` with no count is
   one bit = `/5`; `/10` would be `sar $0x2,%edx`. Reading the constant
   and not the shift halved every floor the player reached, for the whole
   game: `ply->level`, the combo accounting, the scroll-speed steps keyed
   on `ply->level`, the saved floor and the score all followed. That is
   exactly the factor of two batch 12 measured (level 26 vs 13, floor 100
   vs 50, score 2386 vs 662).

3. Found while checking the OTHER `0x66666667` site in the same function
   (`0x412a0c`, the genuine `/10`, `sar $0x2`): its final `sub` has the
   operands reversed (`sub %edx,%ecx`), so what it stores is `-(x/10)`.
   main.c 4031: `stars[p].sy = ((new_rand() % 200) << 16) / 10;` ->
   `-(((new_rand() % 200) << 16) / 10)`. The "aight" stars fly UP.
   Not observable in the digest domain (particles live in `stars[]`,
   which is hashed, but the star burst needs `!options.flash`), so this
   one is fixed on disassembly evidence alone.

### Verdicts

| workload | check | result |
|---|---|---|
| human_test | per-tick digest, callees src on both sides | EQUAL, 2293/2293 ticks |
| human_test | frame oracle, `every=1` | EQUAL, 2752/2752 frames |
| human_test | run to the game's own exit, `--bind play=src` alone | `last_game.itr` score=2386 floor=100; `log.txt` reaches `Done...` |
| human_test | same, `--bind-file all_rows_src.bindfile` | score=2386 floor=100 |
| newgame | digest / frames, unbound vs all-bound | EQUAL 876 ticks / EQUAL |
| .itr | G5a unbound twice / G5b all-bound vs baseline | EQUAL 157 / EQUAL 157 |
| gates | G1 / G2 / G3a / G3b / G4 / G5 | all EQUAL |

The game-over / results / high-score / initials regions of `play.c`
remain IN-VIVO-PENDING in the strict sense that no oracle compares them
frame-for-frame (they are `readkey()`- and wall-clock-driven, so two
launches legitimately render a different number of frames there) -- but
their COMMITTED outcome is now checked end to end: the run reaches
`Done...` and writes the witness `.itr` with the right score and floor
through those very screens.

### Scope note

`logfile.c`, `draw_reward.c`, `screenshot.c` and `sound.c` (batch 13,
untracked and in flight while this ran; `logfile.c` does not link -
`pthreadGC2.dll` needs guest-IAT bindings) were temporarily excluded from
the build for this pass, so `play=src` pulled in exactly batch 12's
verified callee set. Batch 13's own in-vivo pass re-measures with them in.
