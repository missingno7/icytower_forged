
## Batch 14 addendum -- all sixteen verified in vivo; the defect was in the carrier's CRT seam, not in the recovered source (divergence 011, 2026-09-08)

The in-vivo pass batch 14's own "In vivo (for the carrier task -- NOT run
by this pass)" section asked for, run end to end. Full accounts:
`src/icytower/INVIVO.md` "In-vivo pass -- batch 14, and divergence 011"
and `carrier/NOTES.md` "Divergence 011".

### Status change

| function | was | now |
|---|---|---|
| all sixteen (`qualify_hisc_table`, `sort_hisc_table`, `enter_hisc_table`, `get_rank_id`, `get_rank`, `save_profile`, `hash`, `calc_replay_checksum_131`, `calc_replay_checksum`, `destroy_replay`, `save_replay`, `save_config`, `myDeleteFile`, `fadeOut`, `fadeIn`, `init_scroller`) | offline-verified, `carrier bind: pending` | **offline-verified AND in-vivo EQUAL** |

**No `src/icytower/*.c` file was edited.** All sixteen are correct exactly
as this batch recovered them; the two failures found in vivo were both in
the carrier's own generator, one layer below src/.

### The three things the offline oracle structurally could not see, and what each turned out to be

Batch 14's own closing list named three. All three were real, and two of
them hid a defect:

1. **"`save_replay`/`save_profile`/`save_config` really touch the
   filesystem."** They do. `last_game.itr`, `MissingNO_best_jj2_4.itr`,
   `MissingNO.itp`, `MissingNO_stats.txt` and `tower.cfg` all appear and
   all compare equal to an unbound run's -- the `.itr` files byte for
   byte under `carrier/scripts/compare_itr.py` (new; it knows this
   batch's own on-disk write order and masks one column, below), the rest
   plain byte-identical. Getting there needed the heap fix: `free()` in
   `destroy_replay` and `save_profile` was the CARRIER's `free()`, while
   the blocks came from still-ORIGINAL guest code out of the carrier's
   deterministic arena -- `STATUS_HEAP_CORRUPTION`, twice, reproducibly,
   at the same instruction.

2. **"`pack_fopen`'s mode string is load-bearing"** (`"wp"` for
   `save_config`, `"wb"` for `save_replay`). Confirmed in vivo, indirectly
   but conclusively: `tower.cfg` written by a bound run is byte-identical
   to the original's, and it is LZSS-compressed, which only `"wp"`
   produces.

3. **"The fades block on `cycle_count`."** They do, and they are fine:
   `fadeIn`/`fadeOut` bound produce a per-tick digest EQUAL over all 2293
   ticks and a byte-identical frame-oracle stream. The concern that
   motivated the note -- that `rest()` might not reach the guest's own
   Allegro -- does not arise: Allegro entry points go through
   `pf_lib_bindings.h` to guest VAs, a different mechanism from the CRT
   seam that did break.

### One correction to a finding, and one addition

**Finding 3 gains an in-vivo consequence.** "`calc_replay_checksum`
hashes only three of the five statistics columns" is not just a curiosity
about the format: it is what makes a byte-level `.itr` oracle possible at
all on this host. `tc_s_data` is `play.c` 3641's music-sync channel,
`50.0 * accMusics / totMusics`, and `accMusics` accumulates
`voice_get_position()` on a live DirectSound voice. MEASURED: two
UNBOUND runs of the same recording differ in exactly six bytes of
`last_game.itr`, all inside `tc_s_data[0..1]`. Because that column is one
of the two the checksum skips, the stored checksum is still stable
(0x8180e34d across every run in this pass), and masking the column leaves
a genuine bit-exact oracle for everything else -- including the `date`
field with its `ICYTOWERISGREAT` watermark (finding 1) and all three
hashed columns.

**A new item for the file's own "what the offline oracle cannot see"
list, learned the expensive way.** An offline oracle links the recovered
function against the HARNESS's C library and compares behaviour; in vivo
the same function links against the CARRIER's, and the original ran
against the GUEST's. Wherever those differ in STATE rather than in
behaviour -- the heap, the stdio stream table, a clock the carrier pins
for determinism -- an offline EQUAL says nothing. `save_profile` is the
sharpest example: offline its `time`/`localtime` pair is stubbed and
compared as a trace record, and it passed 20000 vectors x 4 seeds; in
vivo, before the fix, it wrote the real host date into
`MissingNO_stats.txt` where the original wrote the deterministic one.
Nothing crashed and no digest moved. The carrier now fails the BUILD for
this class (`gen_bindings.py` parses `carrier/src/wrappers.cpp`'s wrap
table and refuses any wrapped import that src/ calls but does not bind),
so a future batch inherits the check rather than the lesson.

### Verdict summary

| workload | check | result |
|---|---|---|
| human_test to the game's own exit, each of the five source files' functions bound alone | per-tick digest / witness / written files | EQUAL 2293 / score 2386 floor 100 `Done...` / all byte-identical |
| human_test, all sixteen bound with every earlier row and `play` | same | EQUAL 2293 / 2386 / 100 / all byte-identical |
| human_test | frame oracle `every=1` | byte-identical, 2752 frames |
| newgame | digest / frames | EQUAL 876 / EQUAL 982 |
| `.itr` workload | digest / frames / written files | EQUAL 157 / 841 common ticks 0 mismatching / byte-identical |
| gates.ps1 | G1-G5b | all EQUAL |
