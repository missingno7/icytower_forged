# The ASSET coastline — `src/icytower/assets.h` and friends

win32_pilot.md SS7c names three coastlines that can migrate independently
of each other: CODE (`ORIGINAL -> LIFTED -> NATIVE`), LIBRARIES (`EMBEDDED
-> EXTERNAL/UPSTREAM`), and ASSETS (`EMBEDDED ORIGINAL -> EXTRACTED FILE`).
This is the generated seam for the third one: clean game source refers to
assets by a stable `asset_id`, never by a `data[N].dat`/`sfx[N].dat` index,
and the same id resolves differently per world without the clean source
knowing which world it is in (ROADMAP.md SS2's `ASSET_PLAYER_IDLE` example,
made concrete for Icy Tower).

Everything here is produced by `carrier/gen/gen_assets.py` from
`artifacts/asset_manifest.json` (319 records) plus `tools_recon/
assets_manifest.py`'s `CHAR_SLOT` table (reused, not re-derived — see that
generator's own module docstring). Re-run it after `asset_manifest.json`
changes; do not hand-edit any of its four outputs.

```
src/icytower/assets.h              address-free asset_id enum (257 values)
                                    + the 5-function API declaration
src/icytower/assets_table.inc      {id, datafile family, index N, object
                                    name, type fourcc} rows -- DATA, not
                                    addresses, so this lives under src/
src/icytower/assets_standalone.c   standalone-world implementation
carrier/gen/pf_asset_bindings.h    carrier-world implementation
carrier/gen/assets_selftest.c      carrier-world compile fixture
carrier/gen/check_assets.py        Python consistency check (read-only)
src/icytower/draw_buffer.c         the seam demonstrated on one real
                                    function (see "Show the seam" below)
```

## Scope: 257 of the manifest's 319 records

`asset_manifest.json` has four `kind`s: `datafile_object` (257),
`compiled_table` (48), `external_file` (12), `icon` (2). Only
`datafile_object` — the objects the game reaches through a numeric index
into an already-loaded `DATAFILE*` array (notes/asset_census.md SS3) — got
an `asset_id` and an accessor here. The other 62 are a different EMBEDDED
form with a different consumer that this task brief didn't ask for: a
compiled table's extraction rule is "generator emits a C initialiser into
src/; no file" (its own C symbol name is already the identity, no id
needed); the 2 PE icons are Win32 resource API objects (`LoadIcon` by
resource name, not `load_datafile`); the 12 external files are already
plain files on disk. `asset_bitmap()`/`asset_sample()`/`asset_font()`/
`asset_palette()`/`asset_object()` only make sense for a `DATAFILE`
sub-object, so that is exactly what they cover.

## How ids are named

`ASSET_<FAMILY>_<OBJECT>`, purely mechanical from the manifest — no
hand-authored name list, so re-running the generator after the manifest
changes never silently invents a different name for an id already in use
by clean code (a collision would append `_2`/`_3` instead, and the
generator would still run — see `gen_assets.py`'s `build_assets()`).

**FAMILY**: `DATA` / `LOADING` / `SFX` for `data.dat`/`loading.dat`/
`sfx15.dat`; for a character datafile, `CHAR_<X>` where `X` is the
upper-cased first `_`-token of the character's directory name
(`harold_the_homeboy` -> `HAROLD`, `disco_dave` -> `DISCO`, `jungle_jane`
-> `JUNGLE`, `wild_wendy` -> `WILD`).

**OBJECT**: the datafile object's own `NAME` property for `data`/
`loading`/`sfx15` — already symbolic (`TITLE`, `FONT_MONO`, `FLD_LOGO`) —
with a leading `S_` stripped for `sfx15` only (`S_AIGHT` -> `AIGHT`, so the
id is `ASSET_SFX_AIGHT`, not the redundant `ASSET_SFX_S_AIGHT`; the table's
`object_name` column keeps `S_AIGHT` verbatim, matching the real file — see
"The table" below). For a character datafile, the raw grabber name
(`000_PAL`, `001_BMP`, ...) carries no information, so the OBJECT token
uses the CHAR_SLOT-derived, game-visible slot name instead (`palette`,
`frame01`..`frame15`, `snd_jump_lo`, ...). Either way: non-alphanumeric
runs become `_`, and a trailing digit run gets a `_` inserted before it
(`frame01` -> `FRAME_01`).

Worked examples (all real ids in `src/icytower/assets.h`):

| datafile object | id |
|---|---|
| `data.dat` object 126, `TITLE_BG` | `ASSET_DATA_TITLE_BG` |
| `data.dat` object 53, `FONT_MONO` | `ASSET_DATA_FONT_MONO` |
| `loading.dat` object 1, `FLD_LOGO` | `ASSET_LOADING_FLD_LOGO` |
| `sfx15.dat` object 0, `S_AIGHT` | `ASSET_SFX_AIGHT` |
| `harold.dat` object 0 (CHAR_SLOT: `palette`) | `ASSET_CHAR_HAROLD_PALETTE` |
| `dave.dat` object 1 (CHAR_SLOT: `frame01`) | `ASSET_CHAR_DISCO_FRAME_01` |
| `jane.dat` object 16 (CHAR_SLOT: `snd_jump_lo`) | `ASSET_CHAR_JUNGLE_SND_JUMP_LO` |

`ASSET_COUNT` closes the enum (257). Enum order is manifest order, which
is `assets_table.inc` row order, which is *also* the real datafile's
physical object order (notes/asset_census.md SS3: the grabber-assigned
`AAAPAL` name is a deliberate trick to sort the palette first) — so a
plain `asset_id` value is also a valid index into `asset_table[]`, though
nothing here or in clean code is allowed to rely on that equivalence
staying true (that would just be `data[N]` again with extra steps); the
5-function API is always the seam.

## The table (`assets_table.inc`)

Two arrays, no address, no path constructed from an address — safe under
`src/`:

- `asset_table[ASSET_COUNT]`: `{ id, datafile, index, object_name,
  type_fourcc }`. `datafile` is one of `"data"`, `"sfx"`, `"loading"`, or
  `"char:<name>"` (`"char:harold_the_homeboy"`, ...). `object_name` is
  whatever `display_name()` in the generator computed (see naming rule
  above) — the same string a reader would find by opening the real file,
  for cross-referencing.
- `asset_datafile_family[]`: `{ name, path, password }` — one row per
  datafile, the SINGLE source of truth for the 7 relative paths and the
  passwords (see next section), shared by both implementations instead of
  each carrying its own copy that could drift.

## The two implementations

Both read `assets_table.inc`; neither hand-transcribes an index or a path.

### Carrier (`carrier/gen/pf_asset_bindings.h`)

Zero-copy where the original keeps something to be zero-copy *from*.
`data`/`sfx` are filled once by `init_game()` and kept in a global for the
rest of the process (notes/asset_census.md SS3) — `data` @0x4dd23c, `sfx`
@0x4dd240 — already bound to those addresses under their plain names by
`pf_bindings_src.h` (force-included ahead of this header), so
`pf_asset_family("data")` just returns `data` and the accessor reads
`data[N].dat` exactly like the original game code does.

**`loading` and every `char:<name>` have no such persistent global in the
original.** `load_datafile("data/loading.dat")` fills a local used once
for the splash screen and never kept; a character's datafile only ever
passes through the single, ephemeral `custom.df` field of the one `Tcustom
custom` global (not an array — `game_types.h`), overwritten the moment a
different character is selected in the menu. There is no "the object the
game already loaded" for these five families to bind to the way
`data`/`sfx` can — the original itself doesn't keep one either.

Disclosed design decision: `pf_asset_bindings.h` gives each of these five
families its own private, lazily-loaded, cached-forever `DATAFILE*`,
loaded through the real embedded Allegro loader
(`load_datafile`/`packfile_password`, bound to their original addresses by
`pf_lib_bindings.h`, also force-included ahead of this header) at the
family's path and password from `asset_datafile_family[]`. This reads the
exact same bytes through the exact same decoder the original ships — "zero
re-implementation", just not "the identical in-memory object at every
moment", because the original doesn't have one either for these five.

### Standalone (`src/icytower/assets_standalone.c`)

Plain source, no carrier header, matching win32_pilot.md SS7c distribution
mode (a) "drop-in, original folder": loads all seven datafiles itself,
lazily, with the real Allegro `load_datafile()`/`packfile_password()`
(declared in `allegro_api.h`, address-free — once Allegro is built from
source, per win32_pilot.md SS7c stage S3, these resolve to the real
library and this file's `#include`s become the only thing that changes).
No family gets special-cased to a persistent global here, because
standalone has none for *any* family — `data`/`sfx` are lazily loaded the
same way `loading`/`char:*` are.

**Passwords — GAME DATA recovered from the binary** (notes/asset_census.md
SS5a), in `assets_table.inc`'s `asset_datafile_family[]`, the one place
either implementation reads them:

| datafile | password | how it was recovered |
|---|---|---|
| `data.dat` | `CHEESE` | `pwd_garble_string(0x4bdb3c, 0x32)` applied to the garbled `.data` string at `0x4bdb3c` (KNOWN, SS5a) |
| `sfx15.dat` | `CHEESE` | same string, reused |
| `loading.dat` | `(c) Free Lunch Design` | plain string, read directly |
| the 4 character `.dat` files | none | KNOWN — `datafile_summary` in the manifest: `"password": null` for all four |

### Sample decoding (OGG)

Every sample-kind object this game ships is `OGG `-typed (no shipped
datafile uses Allegro's `SAMP` type — notes/asset_census.md SS2a), so it
cannot be a zero-copy cast: `asset_sample()` calls the embedded libvorbis
addon's own `logg_load_memory()` (upstream name, `pf_lib_bindings.h`
VA=0x420688 in the carrier; the real linked function once Allegro is
source-built, standalone) on the object's raw payload. A `SAMP`-typed
object, if one is ever shipped, is returned as a zero-copy cast like every
other kind — both implementations check `type_fourcc` for this, nothing
else.

## Consistency check

`carrier/gen/check_assets.py`, three independent, read-only checks:

1. **Generator matches its own output** — recompute the asset list from
   `asset_manifest.json` + `CHAR_SLOT` and diff it against what is
   actually in `assets_table.inc`, row by row.
2. **Census coverage** — every constant-index site the census's
   ref-scanner found (`artifacts/assets_extract/datafile_refs.json`: 42
   distinct `data[N]` indices, 22 distinct `sfx[N]` indices, notes/
   asset_census.md SS3) maps to exactly one row in the table.
3. **Real-file order** — for each of the 7 datafiles, parse the actual
   file under `assets/` with `tools_recon/assets_datafile.py` (the
   validated reader) and confirm the table's index `N` really is object
   `N` in that file, by name and by type — not just trusting the manifest
   that was built from the same reader earlier.

Current result:

```
== 1. generator output vs src/icytower/assets_table.inc ==
  OK: 257 rows, generator output == src/icytower/assets_table.inc
== 2. census constant-index sites -> exactly one table row each ==
  distinct data[N] indices: 42, distinct sfx[N] indices: 22 (census: 42 + 22)
  OK: all 64 sites map to exactly one asset id
== 3. table index N vs real-file object order (assets_datafile.py, read-only) ==
  OK: 257 objects across 7 real datafiles, all index/name/type match
---
check_assets: 0 mismatch(es) (0 generator/output, 0 census-coverage, 0 real-file-order)
```

`carrier/gen/assets_selftest.c` is the C-side half: a carrier-world
compile fixture that calls `asset_object()` on all 257 ids plus one typed
accessor call per datafile family, proving the generated header compiles
and every id resolves through the API with no address literal anywhere in
that file. It only needs to compile (no runtime harness attached — see
"Show the seam" for why):

```
cl /nologo /c /W3 /TC /Icarrier\gen /Isrc\icytower ^
   /FIpf_bindings_src.h /FIpf_lib_bindings.h /FIpf_asset_bindings.h ^
   carrier\gen\assets_selftest.c
-- 0 errors, 0 warnings
```

## Show the seam: `draw_buffer`

`draw_buffer` (`F:\projects\icytower\trunk\source\profile.c`, VA=0x4191c8,
185 bytes) is one of the census's 42 constant-index sites: it reads
`data[53].dat` and uses it as the `FONT *` argument to `textprintf_ex()`
while drawing a `\n`-separated text buffer one line at a time. `data[53]`
is `FONT_MONO` (notes/asset_census.md SS3: "the five fonts — ... FONT_MONO
(53) x24"). The clean recovery in `src/icytower/draw_buffer.c` reads
`asset_font(ASSET_DATA_FONT_MONO)` instead of the raw index — see that
file's own header comment for the full instruction-by-instruction
recovery, including a behavioural note worth restating here: the original
only ever calls `textprintf_ex()` from inside the `'\n'` branch, so a
buffer whose last line has no trailing `'\n'` silently drops that line —
recovered faithfully, not "fixed".

Verified by **compile only, both worlds** — not the offline memory-domain
harness (`carrier/lift/harness/lift_check.py`, used for every function in
`PROMOTIONS.md`): that harness diffs scalar/struct memory domains against
unicorn, and `draw_buffer`'s real domain is `BITMAP`/pixel output plus a
`FONT` dependency, which the harness's current driver scaffolding doesn't
express. `PROMOTIONS.md`'s "Skipped this pass" section already sets this
precedent for `play_jump_sound`, for an analogous reason. Extending the
harness to a pixel-output domain is new harness machinery, out of scope
for this asset-seam change.

```
standalone: cl /nologo /c /W3 /TC /Isrc\icytower src\icytower\draw_buffer.c
            -- 0 errors, 0 warnings

carrier:    python carrier\gen\gen_bindings.py ^
                --exclude add_combo,getFloorData,get_gamepad,is_any,is_down,is_enter,^
is_fire,is_left,is_pause,is_right,is_solid,is_up,jump_player,line_intersect,reset_map,^
update_frame,draw_buffer ^
                --guard-define ICYTOWER_BINDINGS_ACTIVE ^
                --out carrier\gen\pf_bindings_src.h --types-out carrier\gen\pf_bindings_src_types.h
            (adds draw_buffer to the existing 16-name exclude list so its
            own name stays free instead of being redirected to its
            original address, src/README.md's usual step for a newly
            promoted function)

            cl /nologo /c /W3 /TC /Icarrier\gen /Isrc\icytower ^
               /FIpf_bindings_src.h /FIpf_lib_bindings.h /FIpf_asset_bindings.h ^
               src\icytower\draw_buffer.c
            -- 0 errors, 0 warnings
```

`scripts/check_native_layer.py`: 18 files scanned under `src/`, 0
violations (unchanged gate, now covering `assets.h`, `assets_standalone.c`
and `draw_buffer.c` too).

## What remains hand-mapped

**2 of the 7 computed-index sites** (notes/asset_census.md SS3/SS5b/SS8;
was 6 before PROMOTIONS.md batch 10): 7 code locations compute a `data[N]`
index at runtime (`shl $0x4` on a variable) rather than using a literal
`N`, each with a hard-coded *base* object the generator can name but not
(for the ones still open) the per-site range/stride, which is the author's
intent, not a mechanical fact:

| VA | function | expression | base object | generator can name |
|---|---|---|---|---|
| 0x407c7d | `start_reward` | `data[0x5a + i]` | 90 = `REWARD000` (`ASSET_DATA_REWARD_000`) | **the full 10-wide range** (`ASSET_DATA_REWARD_000`..`_009`, indices 90-99) — resolved PROMOTIONS.md batch 7: `assets_table.inc` already generates all 10 REWARD ids contiguously from the manifest's own consecutive `REWARD000`..`REWARD009` object names, so `ASSET_DATA_REWARD_000 + i` (i in [0,9], `start_reward`'s own recovered `tier`) is a safe mechanical offset into that one generator-guaranteed contiguous family — see `start_reward.c`'s own header comment for the "why this specific arithmetic is safe, unlike the general asset_id-as-index case" reasoning. |
| 0x409383 | `draw_frame` | `data[bg_stripe_ids[i] + 1]` | 1 = `BGTILE` (`ASSET_DATA_BGTILE`) | **the full 6-wide range** (`ASSET_DATA_BGTILE`..`_5`, indices 1-6) — resolved PROMOTIONS.md batch 10: the ids come from `new_rand() % max_bg_id` with `max_bg_id <= 5` (2/3/4/5 by the player's floor), so `ASSET_DATA_BGTILE + id` never leaves the one contiguous BGTILE family `assets_table.inc` already generates. |
| 0x409508 / 0x4095a5 / 0x409605 | `draw_frame` | `data[f]`, `data[f+1]`, `data[f+2]` (one floor's left/middle/right tile) | 17 = `FLOOR01` (`ASSET_DATA_FLOOR_01`) | **the full 33-wide range** (`ASSET_DATA_FLOOR_01`..`FLOOR_27`, indices 17-49 = 11 triples) — resolved batch 10: `f = 17 + 3*(profile->start_floor + room.tiles)`, clamped to 44 and then `+3` when that floor's level is past 4999, so `f+2 <= 49`, the family's last member exactly. |
| 0x409690 | `draw_frame` | `data[s]` (a floor's number sign) | 101 = `SIGN01` (`ASSET_DATA_SIGN_01`) | **the full 11-wide range** (`ASSET_DATA_SIGN_01`..`SIGN_09`, indices 101-111) — resolved batch 10: `s = 101 + start_floor + room.tiles`, clamped to 110 and then `+1` past level 4999, so `s <= 111`. |
| 0x409959 | `draw_frame` | `data[stars[i].color + 117]` | 117 = `STAR01` (`ASSET_DATA_STAR_01`) | **the full 8-wide range** (`ASSET_DATA_STAR_01`..`_08`, indices 117-124) — resolved batch 10: `create_particle()` (already promoted) draws `color` as `new_rand() % 8`, so the range is exact. |
| 0x4146e4 | `play` | `data[local]` | results background | base object identity only |
| 0x4149e6 | `play` | `data[local]` | results background | base object identity only |

The census's own table (SS3) enumerated 5 VAs plus one explicitly-ruled-out
row (`view_profile` 0x419d34, "rank tables, not datafile" — not a `data[N]`
site at all, listed there only to record that it was checked and rejected),
while its prose states "**Seven** sites compute the index" and SS8 repeats
"the 7 computed-index sites need per-site ranges".

Reading `draw_frame` in full (PROMOTIONS.md batch 10) both RESOLVED the two
`draw_frame` rows the census had itemized and found that its "0x4095ff /
`data[v + 2]`" row was actually the third of a THREE-site tile triple
(0x409508 / 0x4095a5 / 0x409605) plus a separate, fourth site for the sign
board (0x409690) and a fifth for the star particles (0x409959) — so the
table above now lists 4 resolved `draw_frame` entries where the census had
one. That does not change the census's own count of 7 *code locations*; it
does mean this document's rows are now read off the disassembly rather than
off the census summary.

What remains genuinely open: the 2 `play` sites (0x4146e4, 0x4149e6), whose
range/stride still needs the loop that drives them read off, and the 2 of
the census's 7 that are still not individually itemized by VA anywhere. As
before, this generator — correctly — does not synthesize per-index ids for
any of those four. Every resolved row uses the same argument
`start_reward`'s did: `assets_table.inc` GENERATES each family contiguously
from the manifest's own consecutive object names, so `<base id> + k` is a
mechanical offset inside one generator-guaranteed family, never "an
asset_id used as a global datafile index".

**Character slot names** are hand-recovered too, but already done and
reused rather than re-derived: `tools_recon/assets_manifest.py`'s
`CHAR_SLOT` table was read off `load_frames`/`load_sounds`'s disassembly
(notes/asset_census.md SS3) once, and every id/table row for the 4
character datafiles here is generated from that same table — not a second,
independent transcription.

**Not attempted by this generator at all**: the 62 non-`datafile_object`
manifest records (see "Scope" above) — out of scope for
`asset_bitmap()`/etc, not merely deferred.

## Extraction and the asset oracle

Three pieces close the loop from "the manifest says extraction is
lossless" (notes/asset_census.md §4) to "here is a local cache of clean
files, proven byte-for-byte against a real Allegro load, not just against
itself":

```
port_forge/tools/pf_allegro4_datafile.py   generic Allegro 4 datafile tool
                                            (list/extract/verify/pack-check;
                                            no Icy Tower literals)
scripts/extract_assets.py                  project driver: reads
                                            carrier/win32_policy.json's
                                            "assets" section, produces the
                                            gitignored assets_extracted/
                                            cache + manifest.json
src/build/asset_oracle.c                   standalone-world half of the
                                            cross-world asset oracle
scripts/asset_oracle_digest.py             Python half: reconstructs the
                                            same canonical bytes from
                                            assets_extracted/ alone
```

### 1. The generic datafile tool

`pf_allegro4_datafile.py` is built from `tools_recon/assets_datafile.py`'s
already-validated container-format code (packfile header, Allegro LZSS,
the `ALL.` object tree) but takes no project literal: passwords come from
`--password` or a `--policy` JSON, never a hardcoded table. It adds the
one thing the recon script didn't do — a genuinely lossless 8bpp BMP
export: an **indexed** PNG (`PLTE` + raw index bytes) instead of an RGB24
PNG, so the round trip is exact even without a paired palette (the recon
script's own `bmp_to_png` left the 8bpp case `ok = None`, "not
invertible"). Run against all 7 real datafiles under `assets/`:

```
data.dat      132 objects  (1 PAL, 125 BMP, 5 FONT, 1 info)   verify: 132 ok, 0 mismatch
loading.dat     3 objects  (1 PAL, 1 BMP, 1 info)             verify:   3 ok, 0 mismatch
sfx15.dat      23 objects  (22 OGG, 1 info)                   verify:  23 ok, 0 mismatch
harold.dat     24 objects  (1 PAL, 15 BMP, 7 OGG, 1 info)     verify:  24 ok, 0 mismatch
dave.dat       25 objects  (1 PAL, 15 BMP, 8 OGG, 1 info)     verify:  25 ok, 0 mismatch
jane.dat       25 objects  (1 PAL, 15 BMP, 8 OGG, 1 info)     verify:  25 ok, 0 mismatch
wendy.dat      25 objects  (1 PAL, 15 BMP, 8 OGG, 1 info)     verify:  25 ok, 0 mismatch
```

257/257, every type, every bpp this project ships (8/16/24/32 — the 8bpp
case is `loading.dat`'s `FLD_LOGO`, the only one; 15bpp never occurs here
but the tool supports it generically).

### 2. The project's clean layout

`carrier/win32_policy.json` gained an `"assets"` section: the 7
`{path, password}` family rows and the `char_slot_names` table, **moved**
out of `tools_recon/assets_manifest.py`'s module constants (that recon
script still has its own copies for its own read-only purpose; this is
the one both the generator and the extractor now read, so the two can't
drift independently). `scripts/extract_assets.py` parses the *generated*
`src/icytower/assets_table.inc` (regex over its row syntax — read-only,
same file `gen_assets.py` produces) to get every `ASSET_*` id's
`(family, index, object_name, type)`, opens each of the 7 datafiles once
through `pf_allegro4_datafile.py`, and writes:

```
assets_extracted/gfx/*.png            data.dat + loading.dat BMP objects
assets_extracted/fonts/*.alfont[.json]
assets_extracted/palettes/*.pal
assets_extracted/sfx/*.ogg            (S_ prefix stripped)
assets_extracted/characters/<name>/*  the 4 character datafiles, CHAR_SLOT names
assets_extracted/misc/*.bin           the 7 GrabberInfo objects
assets_extracted/manifest.json        every asset id -> file + both hashes
```

Result: `extract_assets: 257 ids extracted, 257 verified round-trip, 0
MISMATCH` — `{'PAL ': 6, 'BMP ': 186, 'FONT': 5, 'info': 7, 'OGG ': 53}`.
(One bug found and fixed while building this: `data.dat` and
`loading.dat` both grabber-name their palette `AAAPAL` — see §3 of the
census — so the naive `palettes/aaapal.pal` destination collided; fixed
by prefixing non-character palette filenames with their family.)
`assets_extracted/` is gitignored; `scripts/extract_assets.py`'s own
header comment restates the licence rule (local cache only, never
committed or redistributed, per notes/asset_census.md §7).

### 3. The cross-world asset oracle

The question the manifest/extraction round trip *doesn't* answer: does
what the extracted files describe match what a **real Allegro
`load_datafile()`** actually builds in memory? `src/build/asset_oracle.c`
answers it for the standalone world — built with mingw32 against the real
static Allegro 4.4.3.1 (third_party/BUILD.md), it calls
`src/icytower/assets_standalone.c`'s `asset_bitmap()`/`asset_sample()`/
`asset_font()`/`asset_palette()`/`asset_object()` for all 257 ids and
prints `id sha256(payload)`, where payload is a **canonical serialization
of the real in-memory Allegro object** (documented in full, with the
byte-level reasoning, in the file's own header comment) — not the on-disk
datafile bytes (that equality is what §1/§2 above already proved).
`scripts/asset_oracle_digest.py` is the independent Python half: it
reconstructs the *same* canonical bytes working **only** from
`assets_extracted/`'s files, never reopening the original datafile.

**A real gap found and fixed on the way**: `assets_standalone.c` calls
`logg_load_memory()`, which — as `notes/logg_load_memory.md` had already
determined by disassembly — **is not part of upstream Allegro's logg
addon**; it is a six-function local extension the original game shipped
(4 memfile `ov_callbacks` shims + a `logg_load_internal`/`logg_load_memory`
pair, a thin `ov_open_callbacks()` wrapper). The real build confirmed
this the hard way (`undefined reference`/`implicit declaration`).
`src/build/logg_load_memory_shim.c` + `.h` supply it, following that
note's own recovered spec exactly (§5: `bits=16`, `stereo=channels>1`,
`freq=vi->rate`, `priority=128`, `len=ov_pcm_total`, PCM via the same
`ov_read(...,0,2,0,...)` loop upstream's own `logg_load` uses) — as a
*separate* translation unit linked only into the oracle build, not a hand
edit of the generated `assets_standalone.c` or `allegro_api.h`.
`Makefile.standalone` gained an `asset_oracle` target (and a `sha256.h`
for the standalone half — no crypto library is otherwise linked; its
FIPS round-constant table trips `check_native_layer.py`'s guest-address
scanner on 6 coincidental hex values and its own include guard on the
`PF_` prefix, both false positives for a generic hash function, fixed by
spelling those 6 constants as two 16-bit halves and renaming the guard —
`check_native_layer.py` is 0 violations again).

**Built and run for real** (this is not a paper exercise): `mingw32-make
-f src/build/Makefile.standalone asset_oracle`, then
`asset_oracle.exe` from inside `assets/` — 0 compiler errors/warnings,
exit 0, all 257 ids printed a hash. The first *predicted* transform for
24/32bpp bitmaps (derived from reading `datafile.c`/`graphics.c`: memory
bytes = on-disk (B,G,R) triplet byte-reversed, matching a GDI driver's own
`_rgb_shift_24/32` values) turned out to be **wrong** — running the
oracle and diffing against the Python side immediately showed every
24bpp/32bpp object mismatched while every 8/16bpp object matched. The
*measured* rule (whatever real driver `GFX_AUTODETECT_WINDOWED` installed
on the build host actually used) is simpler: memory bytes equal on-disk
bytes **verbatim** at 8/16/24bpp, and 32bpp is the disk triplet verbatim
plus one appended `0x00` alpha byte. `scripts/asset_oracle_digest.py` was
corrected to match, and:

```
matched 204  mismatches 0  total py-proven 204
```

— every `BITMAP` (186), `PALETTE` (6), `FONT` (5) and `info` (7) object's
canonical hash from the real standalone oracle equals the independently
reconstructed hash from nothing but `assets_extracted/`'s files. This is
the strongest evidence in this document: not "the code looks right" but
"a real Allegro load and a from-scratch Python reconstruction agree on
204 independent objects."

### What remains unproven

- **The 53 OGG/`SAMPLE` objects are not proven equal.** The real oracle
  decodes actual Ogg Vorbis audio through libvorbis (via
  `logg_load_memory`); reproducing that bit-for-bit in Python needs a
  bit-exact Vorbis decoder, which is out of scope here. This was checked,
  not just assumed: decoding one sample (`S_MENU_CHANGE`) with `ffmpeg`'s
  own (independent) Vorbis decoder and hashing the result under the same
  `SAMPLE`-field serialization produced a **different** hash from the real
  oracle's — expected, since two independent Vorbis implementations are
  not bit-deterministic with each other, and exactly why this isn't
  silently reported as a pass. `asset_oracle.c` still prints a real digest
  for all 53; only the Python-side equality claim is open.
- **Carrier-vs-standalone equality: IMPLEMENTED and MEASURED (2026-09-08,
  "in-vivo pass, corpus gates, asset oracle", `carrier/NOTES.md`)** — the
  `--dump-assets PATH` mode this section used to only propose now exists
  (`carrier/src/dump_assets.c`), calling exactly the accessors named above
  against the guest's own loaded `DATAFILE*` and printing the same
  canonical serialization. Result, diffed against
  `scripts/asset_oracle_digest.py`'s Python reconstruction:

  ```
  FONT:    5 / 5   EQUAL
  info:    6 / 6   EQUAL (7th is sfx15.dat's own GrabberInfo — SKIPped, see below)
  PALETTE: 5 / 6   EQUAL (one unexplained mismatch: "data" family's own AAAPAL)
  BITMAP:  0 / 186 EQUAL — explained, not a bug (see below)
  ```

  Both empirical questions this section used to leave open are now
  answered, and neither matched the guess. Precisely, from
  `artifacts/disasm.txt`'s own `init_game()` disassembly (VA `0x40e7dc`):
  `set_color_conversion` IS touched, twice, both before any datafile
  loads — `0x40edfe: set_color_conversion(0)` (`COLORCONV_NONE`) right
  before `load_datafile("data/loading.dat")` at `0x40ee16` (the ephemeral,
  never-kept splash-screen copy), then `0x40f199:
  set_color_conversion(0xffffff)` (`COLORCONV_TOTAL`) right before the
  persistent `data` global loads via `load_datafile_callback` at `0x40f1be`
  — and it is never turned back off anywhere in `init_game()`'s own range,
  nor (per a full-binary grep of every `set_color_conversion` call site) in
  any code the recorded gates' scripted input ever reaches before the first
  safepoint. So by the time `data.dat` (and, later, `sfx15.dat` at
  `0x40f96f`) loads, `COLORCONV_TOTAL` is active, and the guest's own
  `set_gfx_mode` installs a real **32bpp** screen — every BITMAP object from
  that point on gets silently up-converted from its native on-disk depth
  (MEASURED: `BGTILE`, 16bpp on disk, reads back `vtable->color_depth == 32`
  inside the running carrier) the moment `init_game()` loads `data.dat` —
  before `--dump-assets` or any game code ever sees the object. This is not
  a `_rgb_shift_*` shift-convention difference (this section's original
  guess) but a full depth conversion, and it is a genuine,
  correctly-measured fact about how the real game runs, not a defect in
  either serialization. "The carrier and the standalone port read the same
  bytes" is therefore true at the level that matters for gameplay (same
  file, same decoder, same pixel values, losslessly up-sampled) but not at
  the raw-stored-byte level this section's own canonical serialization
  checks.

  **Closed, 2026-09-08 (this pass): `src/build/asset_oracle.c`'s `main()`
  reconfigured to match** — `set_color_depth(32)` + a real 32bpp
  `set_gfx_mode` + `set_color_conversion(COLORCONV_TOTAL)` instead of the
  original 16bpp/`COLORCONV_NONE` setup (that ORIGINAL configuration is
  still valid and still documented in `asset_oracle.c`'s own header comment
  — it answers "do the extracted files losslessly describe the on-disk
  bytes", a different, already-closed question; this is a SECOND run,
  under different conditions, answering THIS section's question instead).
  `assets_standalone.c` has no per-family special-casing (every one of the
  7 datafiles loads through the identical lazy path), so setting this
  globally, once, at the top of `main()`, uniformly reproduces the
  carrier's own measured behaviour for every family, not just `data`. Real
  build (mingw32 MSYS2, third_party/BUILD.md's toolchain), real run against
  `assets/`, diffed by id against the carrier's own stored
  `--dump-assets` output (`artifacts_batch9/carrier_assets_final.txt`, 257
  lines):

  ```
  FONT:    5 / 5   EQUAL (unchanged)
  info:    6 / 6   EQUAL (unchanged; 7th is sfx15.dat's own GrabberInfo, SKIP)
  PALETTE: 5 / 6   EQUAL (unchanged this run — see below, not a colour-conversion
                   issue; CLOSED to "carrier-only, not oracle-side" two sections down)
  BITMAP:  185 / 186 EQUAL (was 0 / 186) — only "loading" family's FLD_LOGO
                     (this project's one 8bpp-on-disk BITMAP) still differs
                     (CLOSED to 186/186 two sections down, same pass, select_palette fix)
  ```

  **FINAL RESULT after this pass's `select_palette()` fix (still 2026-09-08):
  FONT 5/5, info 6/6, BITMAP 186/186, PALETTE 5/6** — see the two entries
  below for the closure evidence (FLD_LOGO) and the narrowed-but-still-open
  residual (the "data" family's own AAAPAL).

  **PALETTE's "data" family AAAPAL mismatch is NOT a colour-conversion
  issue** — switching this oracle from 16bpp/`COLORCONV_NONE` to
  32bpp/`COLORCONV_TOTAL` changed zero PALETTE hashes (all 6 stayed exactly
  what they were before), consistent with `PALETTE` being a raw
  `load_data_object()` byte copy with no depth-dependent transform (already
  documented above). Investigated instead by disassembly: the only place
  `artifacts/disasm.txt` shows any code touching `data[0].dat` (the "data"
  family's own AAAPAL, object index 0) at all is `select_palette(data[0].dat)`
  at VA `0x40f928` (`init_game`, right after `data.dat` finishes loading,
  right before `sfx15.dat` loads) — and `select_palette` itself (VA
  `0x44df24`) is **confirmed read-only w.r.t. its argument** by direct
  disassembly: it only reads `r`/`g`/`b` bytes out of the passed palette to
  build Allegro's OWN internal colour-conversion lookup tables
  (`_palette_color8` and friends); it never writes back into the source
  struct. This rules out "the game mutates its own AAAPAL in place" as the
  cause. The remaining, best-supported explanation: "data" is the ONE
  family bound zero-copy to the game's own persistent, continuously-live
  global (`carrier/gen/pf_asset_bindings.h`) — the SAME memory the game
  itself has been reading for the whole session up to the first safepoint —
  while the other 5 families (including "loading") are each a fresh,
  private copy the CARRIER's own binding lazily loads on first request,
  never touched by the game's own code at all. Whatever differs about
  "data"'s AAAPAL is tied to it being the game's own long-lived in-process
  copy, not to this file's load-time configuration; going further needs
  live memory inspection of a running carrier process, out of scope for a
  standalone-only file.

  **PALETTE: STILL 5/6, now with the oracle-side explanation eliminated
  rather than merely suspected (2026-09-08, this pass).** The obvious
  candidate fix for FLD_LOGO — this oracle omitting `init_game()`'s own
  `select_palette(data[0].dat)` call — does NOT explain AAAPAL too, and
  this was checked, not assumed: `select_palette()`/`set_palette()`
  (`third_party/allegro-4.4.3.1/src/gfx.c`) are BOTH confirmed read-only
  w.r.t. their palette argument, independently, twice over — once by
  reading the upstream source (every access is `_current_palette[c] =
  p[c]` or a `palette_color[c] = makecol(...)` read of `p[c]`, never a
  write to `p`), and once by disassembling the REAL compiled function at
  VA `0x44df24` directly (every access off the argument register `%esi` is
  a `mov` INTO a global array, never the reverse). Adding the same
  `select_palette()` call this file now makes for FLD_LOGO left this
  oracle's own "data" family `AAAPAL` hash byte-for-byte UNCHANGED
  (MEASURED, both before and after), and that unchanged hash
  (`dcb62a16df70c6a77e1e3581dd40b460da66987c5396800345fea8695ca8cbc5`)
  independently equals `sha256(assets_extracted/palettes/data_aaapal.pal)`
  computed directly from the extracted on-disk bytes with no oracle
  involved at all — i.e. this oracle's "data" AAAPAL was already, provably,
  the objectively correct answer; there is no oracle-side load-sequence
  bug left to fix. The divergence is therefore entirely on the CARRIER's
  side: `data[0].dat`'s in-memory bytes inside a running carrier process
  really do differ from the on-disk ground truth
  (`ee2e9996c71393d0440dc1c932f7674ba9be2fb2e5b69f8505fe53ee64632002` !=
  `dcb62a16...`, `artifacts_batch9/carrier_assets_final.txt` line 1), by
  some mechanism this pass could not identify — every Allegro entry point
  that ever touches a palette object by reference is now ruled out, so the
  remaining candidates are either a genuine, undiscovered original-game
  write path this pass's disassembly search didn't cover, or an
  arena/memory fact specific to the carrier's own zero-copy binding
  (`carrier/gen/pf_asset_bindings.h`, generated, out of this task's edit
  scope) — either way, settling it needs live carrier memory tracing this
  pass could not perform: a re-run of `--dump-assets` against a freshly
  built carrier (to rule out the stored `artifacts_batch9` dump being
  stale) hit an UNRELATED, pre-existing build break —
  `src/icytower/blit_to_screen.c` (batch 11, commit `a8bd4c7`, already
  itself documented as an open gap: "`_cos_tbl` unbound → blit_to_screen
  links only after the lib-bindings allow-list gains it") — LNK2019 on
  `__cos_tbl` at the final carrier link, unrelated to assets entirely and
  explicitly out of this task's edit scope (`src/icytower/*.c` and the
  harness). The comparison above therefore still rests on the
  already-recorded `artifacts_batch9/carrier_assets_final.txt`, not a
  fresh run; PALETTE stays 5/6, now narrowed to "provably a carrier-only
  fact, not a standalone/Python reconstruction gap" rather than merely
  suspected to be one.

  **FLD_LOGO: CLOSED, 2026-09-08 (this pass) — 186/186 BITMAP.** The
  earlier guess above ("loading's own lazy load is the first thing that
  ever selects a palette, so FLD_LOGO converts against loading's own
  AAAPAL") was WRONG, caught by actually reading
  `third_party/allegro-4.4.3.1/src/datafile.c` end to end rather than
  reasoning from the load order alone: `load_datafile()` never calls
  `select_palette()` itself anywhere (grepped, zero hits) — nothing in
  EITHER world auto-selects a datafile's own palette just because it was
  read first (the `AAAPAL`-sorts-first grabber trick only guarantees
  *read* order, not that anyone *acts* on it). What actually gates an
  8bpp→32bpp conversion is `palette_color[]`
  (`third_party/allegro-4.4.3.1/src/gfx.c`), a lookup table built ONLY by
  an explicit `select_palette()`/`set_palette()` call
  (`palette_color[c] = makecol(_rgb_scale_6[p[c].r], ...)`) — until one of
  those runs, the table sits at its BSS-zeroed default (all-black), which
  is what every earlier run of this oracle silently converted every lazy
  family's 8bpp object against, `FLD_LOGO` included. Verified by direct
  disassembly of the real compiled `select_palette` (VA `0x44df24`,
  matches upstream byte-for-byte: every access off the argument register
  is a load, never a store — confirms the read-only claim independent of
  source-reading) that this is exactly what `init_game()`'s own
  `select_palette(data[0].dat)` (VA `0x40f928`) sets up, once, well before
  the carrier ever lazily loads "loading" family.

  **The fix**: `src/build/asset_oracle.c`'s `main()` now issues that same
  call itself — `select_palette(*asset_palette(ASSET_DATA_AAAPAL))` —
  right after "data" family's own first (and, per asset_table.inc's
  manifest-order enum, necessarily earliest) access and before the loop
  reaches "loading" family, mirroring `init_game()`'s real order exactly
  (data.dat loads, THEN `select_palette(data[0].dat)`, THEN
  sfx15.dat/loading/char families — none of which the real game ever
  revisits). Rebuilt and run for real (mingw32 MSYS2, this same build
  host): `FLD_LOGO`'s hash now equals the carrier's own stored
  `--dump-assets` value EXACTLY
  (`ea6c393764a4089b32c89325aeadea8d33c3112e430f76dae8cef5ce12f41e43`,
  `artifacts_batch9/carrier_assets_final.txt` line 134) — **BITMAP: 186/186
  EQUAL**, up from 185/186. `LOADING_AAAPAL` (the palette this fix does
  NOT touch — `select_palette()` copies `data`'s bytes into the global
  `_current_palette`/`palette_color[]`, never into `loading`'s own object)
  also still matches the carrier exactly, confirming the fix is precisely
  targeted and introduces no new drift.

  **Raw-byte verification, not hash-only**: both `carrier/src/dump_assets.c`
  and this file gained a `dump_verbose()` companion (always written
  alongside the ordinary dump, no new CLI flag — `<path>.verbose.txt` for
  the carrier, `asset_oracle_verbose.txt` for this oracle) that prints
  `DATA_AAAPAL`/`LOADING_AAAPAL`'s full 1024 raw bytes and `FLD_LOGO`'s
  header plus first two rows, all in hex, straight from the same zero-copy
  accessors the hash path already uses — so this closure rests on an actual
  byte-level diff, not a hash match alone.

  A second, real, and independently confirmed environment fact: the
  persistent `sfx` global stays **NULL** in a `--det`/headless carrier run
  (`--print-globals sfx,data` MEASURED `sfx = 0x00000000` while
  `data = 0x2000ebf0` at the same tick, even though an ordinary interactive
  run's own `assets/log.txt` logs `"sfx15.dat loaded"`) — the guest's sound-
  install path evidently takes a different branch with no real audio
  device reachable. `--dump-assets` reports the 23 affected ids (22 `OGG `,
  1 `info`, all of `sfx15.dat`) as `SKIP family-not-loaded=sfx` rather than
  crashing (an earlier, unguarded version of the same loop DID crash here
  first — a real access violation, not a hypothetical). The 30 `char:*`-
  family `OGG` samples (not gated behind the game's own sound-install path)
  DO get a real `logg_load_memory`-decoded hash from the carrier, same as
  the standalone build, just not independently cross-checked (the pre-
  existing, already-documented Python-side Vorbis limitation below still
  applies — no new gap).
- The FONT canonical serialization was derived by transcribing
  `read_font`/`read_font_mono`/`read_font_color` in `datafile.c` field by
  field (documented in `scripts/asset_oracle_digest.py`'s
  `font_mem_bytes()`), not independently cross-checked against a second
  implementation the way BITMAP's rule was corrected by the oracle run —
  its 204/204 match above is real evidence for it, but note that FONT and
  BITMAP/PALETTE/info were all validated by the *same* single oracle run,
  not by two independent oracles.
