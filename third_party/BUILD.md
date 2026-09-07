# Building the static Allegro 4.4.3.1 used by the LIBRARIES coastline

Ground truth for the config reproduced here: `win32_pilot.md` §7b/§7c,
`notes/library_compat_verdict.md` §8 ("Recommended simplest path, per
library"), `notes/library_boundary.md` §2 ("Build configuration that must
be reproduced") and §5.4 criterion (ii). Fetch is `scripts/fetch_third_party.py`
(clones `third_party/allegro-4.4.3.1/`, writes `third_party/MANIFEST.json`).
This build was done under MSYS2 mingw32 (32-bit): `gcc` 16.2.0, `cmake`
4.4.3, `ninja` 1.13.2, all from `/mingw32/bin`.

## 1. Source fetched

- `git clone --branch 4.4.3.1 --depth 1 https://github.com/liballeg/allegro5`
  — upstream tag exists and was used directly; **no fallback to the AGS
  fork was needed.**
- Commit: `8a386b2754608b66d46f10abf993abf8ab16902d` (recorded in
  `third_party/MANIFEST.json`).
- `ALLEGRO_VERSION_STR` in this checkout's `include/allegro/base.h` is
  `"4.4.3"` (the `.1` in the git tag `4.4.3.1` is a maintenance re-tag —
  Allegro's 4.4 line reuses the same in-code version string across point
  releases). The built `allegro_id` therefore reads
  `"Allegro 4.4.3, MinGW32.s"`, not `"...4.4.3.1..."` — expected, not a bug
  (§4 below).

## 2. Dependency packages installed

```
pacman -Sy
pacman -S --noconfirm --needed mingw-w64-i686-libpng
```

Installed `mingw-w64-i686-libpng` 1.6.58-1 (a 1.x release, per
`notes/library_compat_verdict.md`'s note that any 1.x is fine for the
build even though the shipped game used 1.2.34). `libogg`
(1.3.6)/`libvorbis` (1.3.7)/`zlib` (1.3.2) were already present in the
mingw32 install.

## 3. CMake options used, and why they differ from the task's guess

The task assumed `-DWANT_ASM=off`/`WANT_MMX`/`WANT_ALLOW_SSE` CMake
options. **None of these exist in tag 4.4.3.1's `CMakeLists.txt`.** Reading
it directly (`third_party/allegro-4.4.3.1/CMakeLists.txt:355-357`):

```cmake
set(ALLEGRO_NO_ASM 1)
# ALLEGRO_MMX left undefined
# ALLEGRO_SSE left undefined
```

Assembler/MMX/SSE blitters are hard-disabled, unconditionally, for every
platform in this CMake-based tree (they were only ever wired up in the old
`configure`-based 4.2/4.4.1 build; the CMake port that landed for 4.4.x
dropped the option entirely and ships C-only blitters, `src/c/*`, always).
So "C-only blitters, no assembler" — the exact config
`notes/library_boundary.md` §2 says must be reproduced — is the **only**
thing this CMakeLists.txt can produce; there is nothing to disable.
Verified post-build in §5.

Actual configure command used:

```sh
cmake -G Ninja \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DSHARED=off \
  -DWANT_EXAMPLES=off -DWANT_TESTS=off -DWANT_TOOLS=off -DWANT_DOCS=off \
  -DWANT_ALLEGROGL=off -DWANT_JPGALLEG=off \
  -DWANT_LOADPNG=on -DWANT_LOGG=on \
  ../allegro-4.4.3.1
ninja
```

run from `third_party/build-allegro-4.4.3.1/` inside the MSYS2 MINGW32
shell (`MSYSTEM=MINGW32`, `PATH` picking up `/mingw32/bin/{gcc,cmake,ninja}`).

Notes on the options:
- `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` — **not a source patch**, a CMake
  invocation flag. CMake 4.4.3 refuses tag 4.4.3.1's
  `cmake_minimum_required(VERSION 2.6)` outright ("Compatibility with
  CMake < 3.5 has been removed"); this flag tells CMake to apply pre-3.5
  policies without erroring. No `third_party/patches/` file was needed —
  see §6.
- `-DSHARED=off` — the actual option name (task guessed `-DWANT_ASM=off`
  as a possible synonym; it is not — `SHARED` is Allegro's own name for
  what `win32_pilot.md` calls `ALLEGRO_STATICLINK`). Confirmed by the
  built `allegro_id` string ending `.s` (§5).
- `-DCMAKE_BUILD_TYPE=Release` (not `Debug`) — `DEBUGMODE` is only ever
  defined via `CMAKE_C_FLAGS_DEBUG` (`CMakeLists.txt:319`), so any
  non-Debug build type has it off. Verified in §5 (zero call sites to
  `_al_assert`, matching the embedded binary exactly).
- `-DWANT_ALLEGROGL=off -DWANT_JPGALLEG=off` — not part of the boundary
  census (`notes/library_boundary.md`/`library_compat_verdict.md` list
  only `logg` and `loadpng` as addons the game uses); disabled to avoid
  pulling in OpenGL/libjpeg for no reason. `WANT_LOADPNG`/`WANT_LOGG` stay
  on (both default `on` anyway; passed explicitly for clarity).
- Windows drivers (DirectDraw/DirectSound/DirectInput/GDI) are not
  individually toggleable — `CMakeLists.txt`'s `if(WIN32)` block
  (`:593-631`) always requires and links all four
  (`find_package(DDraw/DInput/DSound/DXGuid)`, `gdi32` unconditionally via
  `PLATFORM_LIBS`) and hard-fails the configure step if any DirectX header/
  import lib is missing. MSYS2 mingw32 ships `ddraw.h`/`dinput.h`/
  `dsound.h` and `libddraw.a`/`libdinput.a`/`libdsound.a`/`libdxguid.a`
  out of the box; all four were found (confirmed in the configure log).
- `PNG`/`ZLIB`/`OGG`/`VORBIS`/`VORBISFILE` all resolved via CMake's
  `find_package`/`find_library` to the **`.dll.a` import libraries**
  (`/mingw32/lib/lib{png,z,ogg,vorbis,vorbisfile}.dll.a`), not the `.a`
  static archives that also exist there. This is **not** a mistake: it
  matches `notes/library_compat_verdict.md` §8 exactly — Allegro core +
  logg + loadpng are statically linked into one `.a` (`ALLEGRO_STATICLINK`
  = "static link" in the task wording), while libpng/zlib/libogg/libvorbis
  stay as DLLs, the same architecture the original shipped game used
  (`libpng3.dll`/`zlib1.dll` shipped as DLLs already; libogg/libvorbis were
  statically linked *in the original*, but `library_compat_verdict.md` §8
  explicitly recommends switching those two to MSYS2's prebuilt DLLs going
  forward — "zero adapter... unaffected by the version gap").

## 4. Build result

```
third_party/build-allegro-4.4.3.1/lib/liballeg.a    1,472,744 bytes
third_party/build-allegro-4.4.3.1/lib/libloadpng.a      13,352 bytes
third_party/build-allegro-4.4.3.1/lib/liblogg.a           6,714 bytes
```

Build completed in 137 ninja steps, **zero errors**, warnings only
(unused variables, fallthrough, sign-compare, function-pointer-cast —
all pre-existing in the upstream source, none touched).

## 5. Patches needed: none

`third_party/patches/` was not created — tag 4.4.3.1 compiles clean under
GCC 16.2.0 (task anticipated possible failures under "GCC 14"; the
installed toolchain is newer still, GCC 16.2.0, and it built without any
source modification). The only workaround needed was the CMake invocation
flag in §3 (`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`), which is not a source
patch.

## 6. Verification against the required config (see also §5.4 in
`notes/library_boundary.md`)

Run from the MSYS2 MINGW32 shell against `lib/liballeg.a`:

- **No `_asm`/MMX symbols**: `nm liballeg.a | grep -iE '_asm|mmx'` →
  empty. Confirms C-only blitters (§3).
- **`al_assert` present as dead code, zero call sites** (matches the
  embedded binary's own "`al_assert` exists as code, 0 call sites"
  finding in `notes/library_boundary.md` §2 exactly): `_al_assert` is
  defined (`00000750 T _al_assert`) because its *body* isn't gated by
  `#ifdef DEBUGMODE` (`src/allegro.c:574`), but every call site is the
  `ASSERT()` macro, which expands to nothing when `DEBUGMODE` is undefined
  (`include/allegro/debug.h:35-39`). `nm liballeg.a | grep 'U _al_assert'`
  → empty (no undefined/call-site references anywhere in the archive).
- **`allegro_id` string**: `strings liballeg.a | grep 'Allegro 4'` →
  `Allegro 4.4.3, MinGW32.s` — static-link marker (`.s` suffix) present,
  version differs from `4.4.3.1` only because of the tag/string mismatch
  explained in §1.

## 7. Smoke test

`third_party/smoke/smoke.c` — `allegro_init`, `install_timer`,
`install_keyboard`, `set_color_depth(16)`, `set_gfx_mode
(GFX_AUTODETECT_WINDOWED, ...)` falling back to `GFX_GDI`, `create_bitmap`
+ `blit` + `textout_ex`, a real ~1-second wait driven by `install_int_ex`
(100 ticks at 100 Hz, not a bare `Sleep`), then `logg_load`/`load_png`
called on a deliberately nonexistent path (proves real linkage — see the
comment in `smoke.c` about why `if(0){...}` would not have forced the
linker to pull in the addons, and was replaced with genuine calls that
must return `NULL`), then `allegro_exit`.

Compile command (MSYS2 MINGW32 shell, from `third_party/smoke/`):

```sh
gcc -mfpmath=387 -DALLEGRO_STATICLINK \
  -I../allegro-4.4.3.1/include -I../build-allegro-4.4.3.1/include \
  -I../allegro-4.4.3.1/addons/loadpng -I../allegro-4.4.3.1/addons/logg \
  smoke.c -o out/smoke.exe \
  -L../build-allegro-4.4.3.1/lib -lloadpng -llogg -lalleg \
  -lpng -lvorbisfile -lvorbis -logg -lz \
  -ldinput -lddraw -ldxguid -lwinmm -ldsound -lole32 -lcomdlg32 -lgdi32 -luser32 -lkernel32 \
  -lmingw32
```

(`-m32` was omitted: this MSYS2 mingw32 `gcc` is a native 32-bit-only
i686-w64-mingw32 toolchain, not a multilib compiler, so `-m32` is neither
needed nor accepted.)

Result: `smoke.exe` is a 32-bit PE (`objdump -p` confirms `Intel 80386`),
1,272,126 bytes. Its direct DLL import table
(`objdump -p smoke.exe | grep 'DLL Name'`) lists `ddraw.dll`, `dsound.dll`,
`GDI32.dll`, `KERNEL32.dll`, `msvcrt.dll`, `ole32.dll`, `libpng16-16.dll`,
`USER32.dll`, `libvorbisfile-3.dll`, `WINMM.DLL` — `libpng16-16.dll` and
`libvorbisfile-3.dll` appearing here (they were **absent** in an earlier
build where the calls sat under `if(0){...}`, because GCC folds away a
constant-false branch before the linker ever sees it) is the proof that
`logg_load`/`load_png` genuinely link, not just compile.

Run with a watchdog:
```sh
timeout 15 ./smoke.exe; echo "EXIT=$?"
```
Result: **`EXIT=0`**, twice in a row, well under the 15-second watchdog.
Every failure path in `smoke.c` returns a distinct non-zero code
(1=init, 2=gfx mode, 3=create_bitmap, 4=logg/loadpng unexpectedly
succeeded on a missing file), so exit 0 means the window opened, the
1-second `install_int_ex` timer loop actually ran to completion, and both
addon calls correctly returned `NULL` on the missing paths.

Runtime DLLs needed next to `smoke.exe` (copied from `/mingw32/bin/` for
this run, not committed): `libpng16-16.dll`, `libvorbisfile-3.dll`,
`libvorbis-0.dll`, `libogg-0.dll`, `zlib1.dll`, `libgcc_s_dw2-1.dll`,
`libwinpthread-1.dll`.

## 8. Criterion (ii) sample check (`notes/library_boundary.md` §5.4, row ii)

**INFERRED, not KNOWN** — GCC 16.2.0 (this build) vs. GCC 4.4.1 (the
embedded binary, TDM-GCC SJLJ per `notes/library_boundary.md` §2). This is
a much larger generation gap than the task's "GCC 14" assumption, which
makes the divergence below, if anything, an *upper bound* on what a
closer-vintage compiler would show.

Method: compiled `src/timer.c`, `src/blit.c`, `src/color.c`,
`src/c/cblit32.c` individually with `gcc -O2 -c` (plus the include paths
and `-DALLEGRO_STATICLINK -DALLEGRO_SRC` needed to parse the headers; no
other flags — the task's request), then compared the defined-function
name set per object file (`nm`) against the DWARF-recovered function list
for the same `compile_unit` path in `artifacts/functions.json` (excluding,
for `timer.c`, the 3 functions DWARF attributes to the game's own
`F:\projects\icytower\trunk\source\timer.c`, a different file that
happens to share a basename).

| CU | embedded (GCC 4.4.1) fn count | recompiled (GCC 16.2.0) fn count | name-set match | verdict |
|---|---:|---:|---|---|
| `src/c/cblit32.c` | 5 | 5 | 5/5 exact (`_linear_blit32`, `_linear_blit32_end`, `_linear_blit_backward32`, `_linear_clear_to_color32`, `_linear_masked_blit32`) | **MATCH** |
| `src/timer.c` | 17 | 18 | 17/17 embedded names present, +1 extra (`_install_timer.part.0`, a GCC IPA partial-inlining clone of `install_timer` with no counterpart in the GCC 4.4.1 output) | **MATCH with one modern-GCC-only clone** |
| `src/blit.c` | 7 (`masked_blit`, `blit`, `get_replacement_mask_color`, `dither_blit`, `blit_from_24`, `blit_from_32`, `_blit_between_formats`) | 6 (`_blit`, `_blit_from_256`, `_dither_blit`, `_get_replacement_mask_color`, `_masked_blit`, `__blit_between_formats`) | 5/7 common; **`blit_from_24`/`blit_from_32` vanish** (GCC 16 inlines both into `blit()`, GCC 4.4.1 did not), while **`blit_from_256` appears as a standalone symbol in the new build but not the old one** (GCC 4.4.1 apparently inlined it; GCC 16 did not) | **MISMATCH** — inlining heuristics for single-call-site static helpers reversed direction between the two compiler generations |
| `src/color.c` | 21 | 22 | 21/21 embedded names present, +1 extra (`bestfit_init`, a ~10 KB table-building routine that is a standalone symbol under GCC 16 but has no separate DWARF entry for this CU under GCC 4.4.1 — most likely inlined into its single caller in the original build) | **MISMATCH** |

Overall: **2 of 4 sampled CUs match cleanly on function-name-set and
count** (`cblit32.c` exactly; `timer.c` up to one compiler-only clone); **2
of 4 diverge in shape** (`blit.c`, `color.c`), both driven by differing
single-call-site static-function inlining decisions between GCC 4.4.1 and
GCC 16.2.0 — not evidence of a different source file, just evidence that
"shape identity" (`notes/library_boundary.md` §5.4 row ii's own bar: "byte
identity is not required; shape identity is") is compiler-generation
sensitive and does not fully hold across a 12-major-version GCC gap. This
does not contradict CU provenance (row i, met via exact path match) or the
recommendation to treat these files as third-party-external; it is exactly
the caveat row ii already flagged as "not done" and open. Per-function byte
sizes were also pulled (`nm -S`) but are not reported as a clean
byte-for-byte table here: COFF object files do not reliably carry ELF-style
`st_size` metadata, so `nm -S`'s "distance to next symbol" fallback
produces implausible sizes for a few boundary symbols (e.g. `_rest_int`
reported as covering the entire `.text` section) — a real byte-size
comparison would need DWARF line-table extraction from both builds, which
was out of scope for this pass.
