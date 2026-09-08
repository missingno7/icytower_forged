
---

## Batch 12 addendum -- play() verified in vivo; four recovery defects the census could not see (divergence 010, 2026-09-08)

Batch 12 shipped `src/icytower/play.c` with an honest caveat: 502 of
17420 bytes verified by region oracles, the remaining 16.9 KB checked
only by an exact CALL-SITE CENSUS (82 callees, 0 mismatches), and a
known-but-unexplained in-vivo divergence (score 662 / floor 50 instead of
the witness 2386 / 100). That divergence is now diagnosed and fixed, and
`play()` is EQUAL in vivo on all three workloads. Full account:
`carrier/NOTES.md` "Divergence 010" and `src/icytower/INVIVO.md`.

### The four defects, and why each survived batch 12's checks

| # | `play.c` | defect | what the census saw |
|---|---|---|---|
| 1 | line 916 (main.c 3864) | `(get_level(...) - 1) / 10` should be `/ 5` | the census compares WHICH callee is called with WHAT arguments. `get_level(&map, (int)y)` is the right callee with the right arguments in both forms; the error is in what the CALLER does with the RESULT, and a divisor is not a call at all. Invisible by construction. |
| 2 | line 593 (3498) | `play_sound(custom.bg_music, ...)` should be `custom.yo` | the census matched the callee (`play_sound`) and the argument COUNT, but the argument is a pointer loaded from a struct member - `custom.bg_music` and `custom.yo` are both `SAMPLE *` from the same global, 8 bytes apart. |
| 3 | lines 1122, 1177 (4130, 4199) | `custom.yo` should be `custom.wazup` | same shape as #2; both are on the ESC/pause screens, which no workload in the corpus enters, so no in-vivo check reached them either. Found by AUDIT, not by a run (see below). |
| 4 | line 598, 653 (3504, 3553) | `play_sample(bg_beat, 128, 1000, 1, TRUE)` should be `(bg_beat, 0, 128, 1000, TRUE)` | the four numeric arguments were shifted one place left (the leading `vol=0` dropped). Arity was right, so the census passed. |

### The check that did catch it, and the two new ones worth keeping

**Caught it:** the per-tick digest, once the tick safepoint was rebuilt so
that it survives its own subject being promoted (`carrier/NOTES.md`
"Divergence 010" section 2). With it, each iteration was: run
`play=original` vs `play=src` -> `compare_digests.py` names the first
differing tick -> `--snapshot-at-tick` on both sides -> `pf_inspect diff`
names the first differing GLOBAL by name -> that global's writer in
`play.c` -> the disassembly at that write. Three iterations, three
defects, then EQUAL.

**New check 1 -- GLOBAL-REFERENCE AUDIT (cheap, offline, catches #2/#3/#4
without a run).** For a recovered function, extract every absolute
address its disassembly references, resolve each to `(global, member)`
using `carrier/gen/interop_index.json` + `it_types_check.c` offsets, and
compare the MULTISET against the members the recovered `.c` actually
names. For `play()` the mismatch was immediate and unambiguous: the
original references `custom.wazup` twice and `custom.yo` once; the file
named `custom.yo` twice, `custom.bg_music` once, `custom.wazup` never.
The same sweep over ALL globals `play()` touches turned up nothing else
(the only "unnamed" hits were Allegro's `key[]`/`num_itr_files`
neighbourhood and `stars + 0x3000`, the particle loop's end sentinel).
This is a structural check the call-site census does not subsume, and it
costs one script run.

**New check 2 -- MAGIC-DIVIDE SHIFT AUDIT (catches #1).** GCC's signed
division by a constant is `imul $M` + `sar $k` + sign fixup, and ONE magic
constant serves several divisors: `0x66666667` is `/5` at `sar` 1 and
`/10` at `sar` 2. Reading the constant without the shift is a plausible,
silent, off-by-a-factor error. `play()` alone has two `0x66666667` sites
with DIFFERENT shifts (`0x412885` = `/5`, `0x412a0c` = `/10`), and the
second one also has its final `sub` operands reversed (`sub %edx,%ecx`),
i.e. it computes `-(x/10)`, which was a third recovery defect (`stars[p].sy`,
main.c 4031). Any future recovery should list every `imul $magic` in its
range and record magic + shift + `sub` operand order per site.

### Status change

`play()` is no longer "recovered, offline-partial, in-vivo-pending". It is
verified in vivo on `human_test.txt` (per-tick digest EQUAL 2293/2293,
frame oracle EQUAL 2752/2752 frames, and the run to the game's own exit
saves the witness `score=2386 floor=100`), on `newgame.txt` (digest and
frames EQUAL) and on the `.itr` workload (G5a/G5b EQUAL, 157 ticks). All
of G1-G5 pass. The offline picture is unchanged - the same 502 bytes have
region oracles - but the authority for the rest is now an in-vivo per-tick
digest over the whole 2293-tick game rather than a call-site census.
