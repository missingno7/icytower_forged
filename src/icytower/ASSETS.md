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

**The 7 computed-index sites** (notes/asset_census.md SS3/SS5b/SS8): 7
code locations compute a `data[N]` index at runtime (`shl $0x4` on a
variable) rather than using a literal `N`, each with a hard-coded *base*
object the generator can name but not the per-site range/stride, which is
the author's intent, not a mechanical fact:

| VA | function | expression | base object | generator can name |
|---|---|---|---|---|
| 0x407c7d | `start_reward` | `data[0x5a + i]` | 90 = `REWARD000` (`ASSET_DATA_REWARD000`) | base only, not the 10-wide range |
| 0x409383 | `draw_frame` | `data[bg_stripe_ids[i] + 1]` | 1 = `BGTILE` (`ASSET_DATA_BGTILE`) | base only, not the 6-wide range (`BGTILE`..`BGTILE_5`) |
| 0x4095ff | `draw_frame` | `data[v + 2]` | floor/sign strip base | base object identity only |
| 0x4146e4 | `play` | `data[local]` | results background | base object identity only |
| 0x4149e6 | `play` | `data[local]` | results background | base object identity only |

The census's own table (SS3) enumerates exactly these 5 VAs plus one
explicitly-ruled-out row (`view_profile` 0x419d34, "rank tables, not
datafile" — not a `data[N]` site at all, listed there only to record that
it was checked and rejected), while its prose states "**Seven** sites
compute the index" and SS8 repeats "the 7 computed-index sites need
per-site ranges". This document does not paper over that gap: 2 of the 7
are not individually itemized by VA anywhere in the current census, so
this generator — correctly — did not attempt to synthesize per-index ids
for any of the 7, computed-only or not; each needs the range/stride read
off the loop that drives it before a symbolic constant (or constant range)
can be emitted, which is future work on `notes/asset_census.md`, not on
this generator.

**Character slot names** are hand-recovered too, but already done and
reused rather than re-derived: `tools_recon/assets_manifest.py`'s
`CHAR_SLOT` table was read off `load_frames`/`load_sounds`'s disassembly
(notes/asset_census.md SS3) once, and every id/table row for the 4
character datafiles here is generated from that same table — not a second,
independent transcription.

**Not attempted by this generator at all**: the 62 non-`datafile_object`
manifest records (see "Scope" above) — out of scope for
`asset_bitmap()`/etc, not merely deferred.
