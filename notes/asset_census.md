# Asset census — Icy Tower 1.5.1

Read-only survey of everything the game treats as *data*: what is embedded in
`icytower15.exe`, what lives in external files, how the code addresses it, and
what a symbolic `load_asset(ID)` binding would need. Companion to
`win32_pilot.md` §3/§7a/§7b and `notes/binary_recon.md` §i.

Labels: **KNOWN** verified in the binary/DWARF/parsed file · **INFERRED**
strongly supported · **HYPOTHESIS** design bet.

Tools (all read-only w.r.t. `assets/`): `tools_recon/assets_pe_resources.py`,
`assets_datafile.py`, `assets_dwarf_tables.py`, `assets_dump_tables.py`,
`assets_refscan.py`, `assets_convert.py`, `assets_manifest.py`,
`assets_exe_read.py`. Outputs: `artifacts/asset_manifest.json`,
`artifacts/assets_extract/`.

---

## 0. Headline numbers (KNOWN)

| | count |
|---|---:|
| PE resources in the EXE | **2** (RT_ICON `1`, RT_GROUP_ICON `ALLEGRO_ICON`), 2236 B total |
| initialised variables in `.data`/`.rdata` from CUs under the game's source tree | **48** (of which ~34 are genuine game data tables; the rest is bundled loadpng.c / strptime.c / timer.c state) |
| game globals with a fixed address, all sections | 157 (46 `.data`, 2 `.rdata`, 109 `.bss`) |
| embedded media blobs (font/bitmap/sound bytes in the image) | **0** |
| external Allegro datafiles | **7** (3 shipped `data/*.dat` + 4 character `.dat`) |
| datafile objects, all files | **257** — BMP 186, OGG 53, PAL 6, FONT 5, info 7 |
| password-protected datafiles | **2** (`data.dat`, `sfx15.dat` — password `CHEESE`) + `loading.dat` (`(c) Free Lunch Design`) |
| distinct `data[N]` object indices compiled into `.text` | **42** (153 member reads) + 22 `sfx[N]` + 7 computed-index sites |
| references by object *name* string | **0** |

---

## 1. What is embedded in the EXE

### 1a. PE resources (KNOWN — `artifacts/pe_resources.json`)

| type | name | VA | size | note |
|---|---|---|---:|---|
| RT_ICON | 1 | 0x005170bc | 2216 | 32×32, 8 bpp, BITMAPINFOHEADER |
| RT_GROUP_ICON | `ALLEGRO_ICON` | 0x00517964 | 20 | the name Allegro looks up at window creation |

No RT_VERSION, no RT_RCDATA, no RT_BITMAP, no manifest. The `.rsrc` section is
2424 B; **the icon is the only binary asset in the PE resource tree.**

### 1b. No compiled-in media (KNOWN)

A signature scan of `.data` (0x4bc000–0x4d3734) and `.rdata` (0x4d4000–0x4dc964)
for `\x89PNG`, `OggS`, `RIFF`, `BM`, `MThd`, `slh!`, `ALL.` finds only Allegro's
own *format-detection string literals* (`OggS` @0x4d9500, `RIFF`/`WAVE`/`fmt `
@0x4d9fb8, `MThd`/`MTrk` @0x4da330). Nothing decodable as an image, font,
palette or sound is compiled into the image.

### 1c. Compiled-in tables owned by game CUs (KNOWN — 48, `artifacts/assets_extract/embedded_tables.json`)

Contents dumped by `assets_dump_tables.py`; menus by `menu_tables.json`.

| VA | size | name | CU | meaning |
|---|---:|---|---|---|
| 0x004bc000 | 4 | `pink` | custom.c | the magenta key colour written into palette slot 0 of every character |
| 0x004bc020/24/28 | 4 each | `hasFocus`, `lastFocus`, `window` | main.c | window state scalars (not assets) |
| 0x004bc040 | 60 | `hisc_names` | main.c | 15 hiscore-table titles ("Best Scores" … "Quintuple Jump Sequence") |
| 0x004bc080 | 60 | `category_names` | main.c | 15 short category labels, same order |
| 0x004bc0c0 | 180 | `hints` | main.c | 45 game-over hint strings |
| 0x004bc17c | 24 | `start_speeds` | main.c | `{5,4,3,2,1,0}` custom-game speed steps |
| 0x004bc194 | 4 | `version_str` → `"1.5.1"` | main.c | version banner |
| 0x004bc198 / 0x004bc1a8 | 16 each | `snd_volume_slider`, `msc_volume_slider` | main.c | slider min/max/step |
| 0x004bc1c0 | 888 | `ctrl_menu` | main.c | 6 × `Tmenu` — LEFT/RIGHT/JUMP/PAUSE/ReJump/Back |
| 0x004bc540 | 444 | `snd_menu` | main.c | Sound / Music / Back |
| 0x004bc700 | 740 | `gfx_menu` | main.c | Character / Start floor / Eye Candy / Fullscreen / Back |
| 0x004bca00 | 148 | `game_menu` | main.c | Back |
| 0x004bcaa0 | 444 | `profile_menu` | main.c | View / Change Profile / Back |
| 0x004bcc60 | 592 | `opt_menu` | main.c | GFX / Sound / Controls / Back |
| 0x004bcec0 | 740 | `custom_menu` | main.c | Start Game / Speed / Floors / Gravity / Back |
| 0x004bd1c0 | 444 | `play_menu` | main.c | Classic / Custom / Back |
| 0x004bd380 | 1036 | `main_menu` | main.c | Play Game / Instructions / Profile / High Scores / Load Replay / Options / Exit |
| 0x004bd7a0 | 740 | `replay_menu` | main.c | Play Again / Watch Replay / Save Replay / View Profile / Main Menu |
| 0x004bdaa0 | 156 | `scroller_greetings` | main.c | the title-screen marquee text |
| 0x004bdb3c | 7 | `init_string` | main.c | **obfuscated datafile password**, see §5a |
| 0x004bdb60 | 20 | `floor_size_modifiers` | map.c | `{2,0,-2,-4,-6}` — floor-width steps |
| 0x004bdb80 | 40 | `max_speed` | player.c | `{12.0,12.0,12.2,12.2,12.0}` doubles |
| 0x004bdba8 | 24 | `gravity_modifier` | player.c | `{-0.2,0.0,0.2}` doubles |
| 0x004bdbc0 | 20 | `jcLabels` | profile.c | "Singles in a Row:" … "Quintuples in a Row:" |
| 0x004bdbe0 | 48 | `rankLables` | profile.c | `no rank,F,E,D,C,B,A,*,**,***,****,*****` |
| 0x004bdc20 | 48 | `rankFloors` | profile.c | rank thresholds `{0,50,100,150,200,300,400,500,600,750,1000,1500}` |
| 0x004bdc60 | 48 | `rankCombos` | profile.c | `{0,0,7,15,25,35,70,120,200,300,400,650}` |
| 0x004bdca0 | 48 | `rankCCCs` | profile.c | `{0,0,55,65,75,85,95,105,115,125,135,145}` |
| 0x004bdce0 | 48 | `rankNMLs` | profile.c | `{0,0,0,0,0,0,0,400,500,600,700,1200}` |
| 0x004bdd20 | 40 | `comboNames` | profile.c | `Good:,Sweet:,Great:,Super:,WOW:,Amazing:,Extreme:,Fantastic:,Splendid:,No way!` |
| 0x004bdd60 | 4 | `sort_method` | replay.c | default replay-list sort |
| 0x004bdd80..0x004bde34 | 32/32/52/52/12 | `full_weekdays`,`abb_weekdays`,`full_month`,`abb_month`,`ampm` | strptime.c | date parsing, third-party-ish |
| 0x004d7dd0 | 6 | `REPLAY_HEADER` = `"ITR140"` | replay.c | `.itr` magic (`.rdata`) |
| 0x004d80cc | 4 | `tm_year_base` | strptime.c | 1900 |

Plus two Allegro-owned `.data` entries the filter kept (`_png_screen_gamma`,
`_png_compression_level` in loadpng.c; `_vsync_speed`, `_retrace_hpp_value` in
timer.c) — library state, not game data.

Not tables but data-shaped string constants worth naming: the default ad record
written by `reset_options` (0x4181cc) — dates `"2001-12-22"` (0x4d70c8) and
`"1111-22-33"` (0x4d70d3), URL `http://www.freelunchdesign.com/?src=it14_game`
(0x4d70e0), image `"default.dat"` (0x4d710e), and `file_size_ex("data/com/default.dat")`
stored in `Toptions+0x250`.

The 109 `.bss` game globals (`stars`, `map`, `ply`, `greeting_scroller`,
`options`, `hisc_tables`, `eyecandy_selection`, …) are **uninitialised** — they
are filled at runtime, so they are state, not assets.

---

## 2. External asset files

### 2a. Allegro 4 datafiles (KNOWN — parsed end-to-end, `artifacts/asset_manifest.json`)

| file | on disk | header | packed | password | objects | composition |
|---|---:|---|---|---|---:|---|
| `data/data.dat` | 872 669 | `0x3629651b` | yes (LZSS → 6 007 681 B) | **`CHEESE`** | 132 | 1 PAL, 125 BMP (114×16 bpp, 11×24 bpp), 5 FONT, 1 GrabberInfo |
| `data/loading.dat` | 15 033 | `0x7b1c2e2d` | yes (→ 85 687 B) | **`(c) Free Lunch Design`** | 3 | 1 PAL, 1 BMP (8 bpp, 401×210), 1 GrabberInfo |
| `data/sfx15.dat` | 1 177 195 | `0x36296514` | **no** (`F_NOPACK`) | **`CHEESE`** | 23 | 22 OGG, 1 GrabberInfo |
| `characters/harold_the_homeboy/harold.dat` | 116 313 | `slh!` | yes | none | 24 | 1 PAL, 15 BMP (16 bpp), 7 OGG, 1 info |
| `characters/disco_dave/dave.dat` | 378 097 | `slh!` | yes | none | 25 | 1 PAL, 15 BMP (16 bpp), 8 OGG, 1 info |
| `characters/jungle_jane/jane.dat` | 355 952 | `slh!` | yes | none | 25 | 1 PAL, 15 BMP (32 bpp), 8 OGG, 1 info |
| `characters/wild_wendy/wendy.dat` | 411 958 | `slh!` | yes | none | 25 | 1 PAL, 15 BMP (24 bpp), 8 OGG, 1 info |

Container format, as implemented and validated in `assets_datafile.py`:

```text
file    : magic32                       raw, = MAGIC ^ encrypt_id_mask(password)
                                        MAGIC = 'slh!' packed | 'slh.' plain
body    : XOR with password[(offset) mod len]   (offset counted from byte 0)
          then Allegro LZSS (N=4096, F=18, THRESHOLD=2, window zeroed, r=N-F)
datafile: 'ALL.' count32  then count × object
object  : { 'prop' type4cc size32 bytes }*      properties (NAME, DATE, ORIG,
                                                 XPOS/XSIZ/YPOS/YSIZ …)
          type4cc filesize32 datasize32 data[datasize]
```

`encrypt_id` mask = `XOR over i of password[i] << ((i&3)*8)`, then `^ 42` for the
new format. For `CHEESE` that is `0x45450d3a`; `0x736c6821 ^ 0x45450d3a =
0x3629651b`, exactly `data.dat`'s first four bytes (KNOWN).

Object types seen: `PAL `, `BMP `, `FONT`, `info`, and **`OGG `** — a
*game-registered* datafile type. `PNG ` (0x504e4720) is registered by
`loadpng_init` (0x41b990 → `register_png_datafile_object`) and its loader
`load_datafile_png` (0x41b8fc) reads `size` raw bytes and calls
`load_memory_png` — so PNG objects would be byte-verbatim PNG files, but **no
shipped datafile contains one**. `OGG ` objects likewise hold complete Ogg
Vorbis streams (verified: every one of the 53 starts with `OggS`).

### 2b. Plain external files (KNOWN)

Read by the game: `gamepad.txt` (Allegro config, opened with
`set_config_file("gamepad.txt")` at 0x40f03a), `characters/<name>/<name>.txt`
(tagged character script — `[datafile]`, `[frames]`, `[jumplo]`, `[jumpmed]`,
`[jumphi]`, `[pause]`, `[death]`, `[greeting]`, `[edge]`, `[bgmusic]`),
`characters/_template/_template.png`, plus loose `.wav`/`.ogg`/`.mid` named by a
character script. Optional `password.txt` (8 bytes, `check_beta_tester`
0x40e580, §5a).

Documentation only, never opened by the game: `readme.txt`, `itrcheck.txt`,
`ogg_license.txt`, `characters/characters.txt`, `icytower.url`.

Not part of the game: `ddraw.dll`, `ddraw.ini`, `Shaders/`, `cache/`,
`cnc-ddraw config.exe` (the user's cnc-ddraw), `unins000.*` (installer),
`libpng3.dll`, `zlib1.dll`, `pthreadGC2.dll` (third-party runtime).

### 2c. Files the game writes (state, not assets)

`tower.cfg` — an **Allegro packfile** (`slh!`, LZSS, *no* password;
3632 B unpacked from 792 B): `Toptions` followed by the hiscore tables, with
`generate_options_checksum` (0x4181cc) at offset 4. `log.txt`,
`profiles/<n>/<n>.itp` (+`generate_profile_checksum` 0x418a14),
`profiles/<n>/<n>_stats.txt`, `profiles/<n>/replays/*.itr`
(+`calc_replay_checksum` 0x41bac4, header `ITR140`),
`screenshots/icytower_%04d.png`, and the ad cache `data/com/{ads.csv,
default.dat, temp.dat}` (absent in this install).

---

## 3. Reference model — how the code addresses assets

**By numeric index into the loaded `DATAFILE` array. Never by object name.**
(KNOWN — `assets_refscan.py`, `artifacts/assets_extract/datafile_refs.json`.)

Pipeline in `init_game` (0x40e560 …):

```text
packfile_password("(c) Free Lunch Design")   0x40ee0a
load_datafile("data/loading.dat")            0x40ee16   → FLD logo splash
packfile_password(NULL)                      0x40ee38
pwd_garble_string(init_string=0x4bdb3c, 0x32)0x40f181   → "CHEESE"
packfile_password(0x4bdb3c)                  0x40f1aa
load_datafile_callback("data/data.dat",
                       datafile_callback_slow=0x407bf0) 0x40f1be → global `data` @0x4dd23c
packfile_password(NULL)                      0x40f1d7
packfile_password(0x4bdb3c)                  0x40f95b
load_datafile_callback("data/"+sfx_file,
                       datafile_callback=0x407c14)      → global `sfx`  @0x4dd240
packfile_password(NULL)                      0x40f9a9
```

`DATAFILE` is 16 bytes (`dat,type,size,prop`), so `data[N].dat` compiles to
`mov 0x4dd23c,%r ; mov (N*16)(%r),%r`. 42 distinct indices are reached that way
(153 member reads); the busiest are the five fonts —
`FONT_SMALL` (54) ×35, `FONT_MONO` (53) ×24, `FONT_MED_WHITE` (52) ×21,
`FONT_MED_BLACK` (51) ×13, `FONT_BIG_WHITE` (50) ×4.

Seven sites compute the index (`shl $0x4` on a variable), all with a **hard-coded
base**:

| VA | function | expression | base object |
|---|---|---|---|
| 0x407c7d | `start_reward` | `data[0x5a + i]` | 90 = `REWARD000` (…`REWARD009`) |
| 0x409383 | `draw_frame` | `data[bg_stripe_ids[i] + 1]` | 1 = `BGTILE` (…`BGTILE5`) |
| 0x4095ff | `draw_frame` | `data[v + 2]` | floor/sign strip |
| 0x4146e4, 0x4149e6 | `play` | `data[local]` | results background |
| 0x419d34 | `view_profile` | rank tables, not datafile | — |

Sounds: `getSampleFromOggDatafile(sfx, N)` (0x40ca2c) is called 22× in
`init_game` with a literal `N`, e.g. `sfx[8] S_GOOD → combo_sound[0]`,
`sfx[18] S_SWEET`, `sfx[2] S_BG_BEAT → bg_beat`, `sfx[11] S_MENU_CHANGE`.

Characters (KNOWN, `custom.c`): `check_characters` scans `characters/` with
Allegro's `for_each_file_ex`; `load_character` (0x40fe78) builds `"%s/%s.txt"`
and calls `load_character_bmp` (0x4031cc), which honours `[datafile]` (→
`load_datafile`, uses object **1** as the menu preview and object **0** as the
palette) or `[frames]` (→ `load_bitmap`, i.e. PNG through loadpng).
`load_frames` (0x402874) copies object **0** (1024 B palette) into `ply+0xbc`,
overwrites entry 0 with `pink`, then loads objects **1…15** as the 15 animation
frames. `load_sounds` (0x40212c) walks the object list to `DAT_END` to count it,
then calls `loadCustomSoundDF(df, N)` for **16 jump lo, 17 jump med, 18 jump hi,
19 "yo", 20 "wazup", 21 "falling", 22 "edge", 23 "bg music"** — accepting either
a `SAMP` object or an `OGG ` object at each index.

**Order dependence**: object *order* inside `data.dat`, `sfx15.dat` and every
character `.dat` is load-bearing, because the index is the identity. Absolute
*file offsets* are never used — the loader walks the stream — so repacking that
preserves order is safe; reordering is not.

---

## 4. Extraction feasibility

Everything was parsed and can be written out; samples are in
`artifacts/assets_extract/` (nothing was written into `assets/`).

| type | lossless plain-file form | evidence |
|---|---|---|
| `OGG ` (53) | **verbatim `.ogg`** — the payload *is* the Ogg stream | `sfx15/S_MENU_CHANGE.ogg`, `S_MENU_CHOOSE.ogg` |
| `PNG ` (0) | would be verbatim `.png` | loader reads `size` bytes → `load_memory_png` |
| `BMP ` (186) | **24-bit PNG, bit-exact round trip** for 15/16/24/32 bpp; 8 bpp needs its `PAL ` alongside (index → RGB is not invertible on its own) | `data/TITLE.png` (400×340, 24 bpp, round-trip **True**), `data/STAR01.png` (16 bpp, **True**), `jane_013.png` (32 bpp, **True**), `wendy_001.png` (24 bpp, **True**), `FLD_LOGO.png` (8 bpp + `AAAPAL`) |
| `PAL ` (6) | 1024 B `256×(r,g,b,filler)`, 6-bit components; verbatim `.pal` or a text dump | `data/AAAPAL.pal`, `AAAPAL.pal.txt` |
| `FONT` (5) | **no standard container.** Header is `int16 height=0` (new format), `int16 ranges`, then per range `int8 mono, int32 first, int32 last`, then glyphs (`FONT_MONO` mono 6×12 for U+20..7E; `FONT_BIG_WHITE` colour 28×53). Faithful form = keep the payload (`.alfont`) + a manifest, or emit a glyph sheet PNG + a metrics JSON | `data/FONT_MONO.alfont` (1549 B), `FONT_BIG_WHITE` header dumped |
| `SAMP` (0 here; accepted for characters) | `int16 bits, int16 stereo, int16 freq, int16 priority, int32 len` + PCM → RIFF/WAVE, verbatim PCM | `sample_info()` in `assets_datafile.py` |
| `info` (7) | 32 B grabber bookkeeping, runtime-irrelevant | verbatim |

Bitmap storage bytes-per-pixel (KNOWN, checked against all 186 payloads with
zero mismatches): 8 bpp → 1, 15/16 bpp → 2 (little-endian RGB565), **24 *and* 32
bpp → 3 (B,G,R; Allegro does not store alpha in datafiles)**.

Conclusion (KNOWN): **every shipped object can be written to an ordinary file
with no loss.** The only objects without a natural standard container are the 5
fonts; the manifest plus the raw payload is the faithful form, and repacking
`.alfont` reproduces the object byte-for-byte.

---

## 5. Coupling problems

### 5a. Obfuscated password

`init_string` @0x4bdb3c holds `"qyuj}h"`; `pwd_garble_string(s, key)` (0x4073c4)
does `s[i] ^= (uint8)(key - i)` in place and is called once with `key = 0x32`,
yielding **`CHEESE`** (KNOWN). It is an *involution applied to a `.data` buffer*,
so after `init_game` the global holds the plaintext — a snapshot taken before and
after startup differs at 0x4bdb3c, and any generator that emits `.data`
initialisers must emit the **garbled** bytes to stay byte-equal to the image.

`beta.c` has a second scheme, `garble_string(buf, len)` (0x401318):
`buf[i] ^= (uint8)(0x3bf + 0x89*i)`. Its only live caller is
`check_beta_tester` (0x40e580), which reads 8 bytes from `password.txt`,
un-garbles them and `memcmp`s against a `testers` list. `save_garbled_data`,
`load_garbled_data`, `load_plain_data` and `create_post` are compiled in but
**have no call sites** — dead beta-upload code. It protects credentials, not
assets.

### 5b. Hand-coded indices

The 42 constant `data[N]` sites and the 22 `sfx[N]` sites are *the* coupling: an
extracted-file layout must preserve the index→object mapping or every reference
has to be rewritten. This is mechanical (the mapping is in
`artifacts/asset_manifest.json`) but it is the reason `load_asset(ID)` must exist
before the datafiles are unpacked.

### 5c. Layout assumptions the code makes about object payloads

`load_frames` copies exactly `0x400` bytes out of `data[0].dat` (the palette) and
then overwrites entry 0 with `pink` — a hand-coded byte length over an Allegro
`PALETTE`. `load_sounds` determines the object count by scanning for
`type == -1` (`DAT_END`) rather than trusting a header. Character frame count is
hard-wired to 15 (`edi` from 0x10 to 0x100 step 0x10).

### 5d. Checksums / anti-tamper (none of it covers the asset datafiles)

`generate_options_checksum` (0x4181cc) over `Toptions` fields (including
`file_size_ex("data/com/default.dat")`), `generate_profile_checksum` (0x418a14),
`calc_replay_checksum` / `calc_replay_checksum_131` (0x41bac4 / 0x41ba10),
`uberChecksum` @0x4dd25c, and the `-itrcheck` mode that prints
`<itrcheck_results …>`. All of these are weighted sums over *save-file* fields.
**No integrity check is applied to `data/*.dat` or the character datafiles**
(KNOWN — the only bytes checked are the *size* of the ad image). So substituting
extracted assets cannot trip an anti-tamper path; substituting a profile,
replay or `tower.cfg` can.

### 5e. Ad system

`fld_adspot.c` fetches `http://www.icytower.com/icytower_pc.csv` on a startup
thread and caches into `data/com/`. `reset_options` bakes a default ad record
(URL + `default.dat` + its file size) into `Toptions`. A standalone build that
drops the ad system must still write the same `Toptions` bytes if `tower.cfg`
compatibility matters.

### 5f. String-compared data

Character scripts are matched with literal tags (`[datafile]`, `[frames]`,
`[jumplo]`…) and `%s/%s.txt` is built from the directory name, so a character
directory's *name* is part of its identity (it also becomes the profile's
`lastProfile`-style stored value). `hisc_names`/`category_names` index-align with
`hisc_tables` (15 entries each).

---

## 6. Binding model

Same shape as the function binding `ORIGINAL → LIFTED → NATIVE`, one level down:

```text
EMBEDDED ORIGINAL   the bytes where the original program keeps them
                    (exe VA, or the DATAFILE* the game already loaded)
EXTRACTED FILE      the same bytes as an ordinary file under assets/<clean path>
```

Manifest record (`artifacts/asset_manifest.json`, 319 records, schema
`portforge-asset-manifest/1`):

```json
{
  "id": "data/FONT_SMALL",
  "kind": "datafile_object",
  "origin": { "mode": "DATAFILE", "file": "data/data.dat", "index": 54,
              "object_name": "FONT_SMALL", "type_fourcc": "FONT",
              "unpacked_offset": 1003114, "size": 2395 },
  "format": "allegro_font",
  "geometry": null,
  "sha256": "…",
  "extract": "keep payload (.alfont) + manifest; …",
  "clean_path": "assets/data/font_small.alfont",
  "referenced_by": "numeric index data[54] compiled into .text"
}
```

`origin.mode` ∈ `EXE_RSRC` (VA+size) · `EXE_DATA` (VA+size+section+CU) ·
`DATAFILE` (file+index+name+fourcc) · `EXTERNAL` (path).

Resolution:

```c
/* src/icytower/assets.h — clean source, no address, no index */
const void *load_asset(asset_id id, int *size);
```

| build | `load_asset(ASSET_DATA_FONT_SMALL)` |
|---|---|
| carrier | returns `data[54].dat` through the generated `pf_bindings.h` (`data` is the global at 0x4dd23c). Zero copying, zero unpacking — the original loader already did the work. |
| standalone | opens `assets/data/font_small.alfont` (or the PNG/OGG for media) via a generated id→path table |

What the generator can do alone (KNOWN, from `datafile_refs.json` +
`asset_manifest.json`): every `data[N]`/`sfx[N]` constant-index site — `N` maps
to an object name, and the name maps to an id. What needs a hand-written mapping
(HYPOTHESIS until each is promoted): the 7 computed-index sites (the generator
can name the *base* — `REWARD000`, `BGTILE` — but the range and stride are the
author's intent), the character slot names (`frame01…frame15`, `snd_jump_lo`…) —
already recorded in `assets_manifest.py:CHAR_SLOT` from the disassembly — and the
`.data` table→id names, which are simply the DWARF symbol names and therefore
already symbolic.

Purity rule (extends `scripts/check_native_layer.py`): `src/` may contain
`ASSET_*` identifiers and must contain no datafile index literal, no `data[`
subscript on the raw global, and no path string under `data/`.

---

## 7. Distribution modes

**Licence (KNOWN — `assets/readme.txt` §DISTRIBUTION):** Icy Tower is freeware
and may be redistributed *"in its original form"*; explicitly **"You are not
allowed to repackage the game. The installation file … must be kept intact."**
and no inclusion in a compilation. Character scripts add: *"Sounds and images
contained in the datafile may not be reproduced without permission from Free
Lunch Design."* → **Redistributing extracted or repacked assets is not
permitted.** Extraction for local use and analysis is a separate question from
distribution; only the latter is constrained here.

| mode | what it requires | licence |
|---|---|---|
| **(a) drop-in, original folder** — the port runs from an unmodified install and reads `data/*.dat` (with the two passwords), `tower.cfg`, `profiles/`, `replays/`, `characters/` | an Allegro-4-compatible packfile+datafile reader (LZSS + `encrypt_id` + the object walker — ~200 lines, already written in `assets_datafile.py`); a `tower.cfg` reader/writer that reproduces `generate_options_checksum`; the `.itp`/`.itr` codecs | fine — ships no assets |
| **(b) clean standalone layout** — assets extracted to `assets/<kind>/<name>.png\|.ogg\|.pal\|.alfont` + `asset_manifest.json` | the extraction tool (done) and the id→path table; no Allegro datafile code at runtime | **may not be redistributed**; usable as a local build only, or generated on the user's machine by an installer step from their own copy |
| **(c) source-port only** — ship code, require the user's original install | mode (a)'s reader, plus a first-run "point me at your Icy Tower folder" step; optionally run the mode-(b) extractor locally into a cache | **the only redistributable form** |

Recommendation: build **(c) with (a) as the runtime path and (b) as a local,
user-run extraction cache**. That keeps the carrier and the standalone port
reading the *same* bytes, keeps `load_asset(ID)` as the single seam, and never
puts Free Lunch Design's art in the repository.

---

## 8. Open items

- `FONT` glyph-level decode is described but not implemented; needed only if the
  port wants to re-render text with its own font pipeline (HYPOTHESIS: keeping
  the payload and reusing Allegro's font renderer is enough for bit-equal
  frames).
- The 7 computed-index sites need per-site ranges before the generator can emit
  symbolic constants (INFERRED bases listed in §3).
- `data/com/default.dat` is absent from this install; its format (PNG vs Allegro
  bitmap) is INFERRED from `fldads_update_local_adimg`, not observed.
- `tower.cfg`'s field layout is only partially read; a full `Toptions` map is
  needed before mode (a) can write it back.
