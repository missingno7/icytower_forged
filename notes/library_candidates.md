# Library replacement candidates for Icy Tower 1.5.1 (icytower15.exe)

Research-only. No binaries downloaded. Goal: identify EXISTING compatible 32-bit
Windows DLLs/library builds that could replace the statically-linked Allegro
4.4.1 / logg / libogg / libvorbis code, to inform a later rebuild-from-source
against DLL import libs (not binary patching of the shipped .exe).

Labeling: **KNOWN** = verified directly (pefile dump of shipped binaries, or
git checkout/diff of upstream source at tagged versions). **INFERRED** = stated
by a secondary source (search result / fetched page) without independent
verification by me.

---

## 0. Ground truth from the shipped binaries (pefile, KNOWN)

`assets/icytower15.exe` (3,753,885 bytes, PE32, TimeDateStamp 2011-12-06,
no version resource, no exports). Import table:
`DDRAW.dll, dinput.dll, dsound.dll, GDI32.dll, KERNEL32.dll, msvcrt.dll (x2
descriptors), OLE32.dll, libpng3.dll, pthreadGC2.dll, SHELL32.DLL, USER32.dll,
WINMM.DLL, WSOCK32.DLL`. **No alleg44.dll / ogg / vorbis import** — confirms
Allegro 4.4.1, logg, libogg and libvorbis are statically linked into the .exe,
exactly as the task states.

- `assets/libpng3.dll` (127,488 B) — GnuWin32 build. FileVersion/ProductVersion
  **1.2.34.3276**, `LibToolFileVersion 3:34:0`, built 2008-12-20 12:38:41 UTC,
  `CompanyName: GnuWin32 <http://gnuwin32.sourceforge.net>`. Imports
  KERNEL32.DLL, msvcrt.dll, **zlib1.dll**. 237 exports incl. `DllGetVersion`
  and the full `png_*` API (png_create_read_struct, png_set_read_fn,
  png_read_row, png_get_*, …).
- `assets/zlib1.dll` (75,264 B) — GnuWin32 build. FileVersion/ProductVersion
  **1.2.3.2027**, built 2005-07-20 16:05:43 UTC, `SpecialBuild: GNU for Win32
  <gnuwin32.sourceforge.net>`. 73 exports (deflate/inflate/gz* API).
- `assets/pthreadGC2.dll` (47,822 B) — FileVersion/ProductVersion **2.8.0.0**,
  built 2009-07-31 00:03:28 UTC, `Comment: GNU C build -- longjmp thread
  exiting`, `Info: http://sources.redhat.com/pthreads-win32/`. Imports
  KERNEL32.DLL, msvcrt.dll, **WSOCK32.DLL** (present in this specific build;
  not required by pthread_create/mutex usage itself). 115 exports incl.
  `pthread_create`, `pthread_mutex_*`, `pthread_cond_*`, `pthread_cancel`, etc.

These two DLLs (libpng3, zlib1) are *already* dynamically linked and shipped
in `assets/` — they are not recovery targets, only version-identification
targets per the task. `pthreadGC2.dll` is likewise already a real DLL import.

---

## 1. Allegro 4.4.x

### 1.1 Official source availability (KNOWN)
- `liballeg.org/old.html` (KNOWN, fetched): only **Allegro 4.4.3.1** is listed
  for download today — `allegro-4.4.3.1.zip` (4.9M), `.tar.gz` (4.5M), `.7z`
  (3.1M). **Source only**, no Windows binaries on this page. Page states
  "These versions are not supported and may have critical security bugs!"
  and points to two third parties for binaries: Matthew Leverton's site and
  devpaks.org.
- GitHub `liballeg/allegro5` tags `4.4.1` and `4.4.3.1` exist and were
  checked out directly (`git clone --depth 1`, then `git fetch tag 4.4.1`).
  The GitHub **Releases** page only lists Allegro 5.2.x releases with binary
  assets — 4.4.x has no GitHub Release / no attached binaries there.
- `github.com/liballeg/allegro_winpkg` (INFERRED from page fetch) — this is a
  Windows *packaging* repo but its own release version numbers (1.5.0 …
  1.15.0) do not correspond to Allegro versions; it appears to package
  Allegro 5, not 4.4. Not further pursued.

### 1.2 Prebuilt Windows 4.4.3 DLL packages found

**A. "Unofficial Allegro Library Distribution" (SourceForge) — INFERRED/KNOWN mix**
`sourceforge.net/projects/unofficialallegro5distribution/files/` (fetched
directly, KNOWN for filenames/sizes/dates):
- `Allegro443_MinGW5302.tar.7z` — 7.2 MB, dated 2016-08-19 (Allegro 4.4.3
  built with MinGW 5.3.0, per filename)
- `Allegro443_MinGW4814.tar.7z` — 7.6 MB, dated 2016-08-19 (Allegro 4.4.3
  built with an older MinGW, per filename)
- Project description (INFERRED, from page summary): "Binary distributions of
  Allegro 4.4 and Allegro 5.2+ for use with MinGW", including "static and
  dynamic libraries, debug and release monolith libraries, DLLs, examples,
  tests, demos, and documentation." If accurate, these archives should
  contain a MinGW-built `alleg44.dll` for 4.4.3. **Not independently opened
  in this pass — contents not verified beyond the project's own description.**
- Same author/family also maintains matching binaries at
  `allegro.cc/files/?v=4.4` (community file archive) — this page returned
  HTTP 500 on fetch and was not resolved; worth re-checking manually.

**B. Adventure Game Studio's `lib-allegro` fork — KNOWN (best candidate)**
`github.com/adventuregamestudio/lib-allegro` — "The fork of the official
Allegro 5 repository. Mainly for applying AGS-specific patches." Actively
maintained fork of Allegro **4.4.3.1** (tag base `4.4.3.1-agspatch`) with CI
release builds. Verified via GitHub API (`api.github.com/repos/.../releases`):

| Tag | Date | Relevant Windows assets |
|---|---|---|
| `v4.4.3.1-agspatch-3` | 2020-02-17 | `alleg-{debug-,}static{-mt,}.lib` (MSVC static import libs); `lib-allegro_{debug,release}{_static,}_i386.tar.gz` and `_amd64.tar.gz` |
| `v4.4.3.1-agspatch-2` | 2019-09-04 | same set, 14 assets |
| `v4.4.3.1-agspatch-1` | 2019-05-12 | same set, no arch split (10 assets) |
| `4.4.2-ags` | 2019-02-23 | single `lib-allegro.tar.gz` (base 4.4.2) |

`lib-allegro_release_i386.tar.gz` (713,424–715,495 B across the two most
recent tags) is the 32-bit **non-static** (dynamic/DLL) release build —
this is the closest match to "alleg44.dll for 32-bit Windows, Allegro
4.4.3.1-based." Toolchain for these archives is **INFERRED** (not confirmed
in README/CI text I could fetch) to be MinGW-w64, based on the `_i386`/
`_amd64` naming convention (GCC/binutils architecture triple style, as
opposed to MSVC's `x86`/`x64`); the `.lib` files alongside are separately
built for MSVC (`alleg-static.lib`, `alleg-static-mt.lib` = release/debug ×
single-thread/multi-thread CRT import-style static libs). CMakeLists.txt in
the repo does branch on MinGW vs MSVC vs "i386" (the `-arch i386` flag is a
macOS/Clang flag, not Windows-relevant, so that particular check should be
disregarded for the Windows build path). **Confidence: MEDIUM** that
`lib-allegro_release_i386.tar.gz` contains a directly usable `alleg44.dll`;
this should be unpacked and inspected before committing to it.

### 1.3 MSYS2 / vcpkg / Debian (KNOWN — none apply)
- MSYS2 `mingw-w64-i686-allegro` = **Allegro 5.2.11.1** (KNOWN, fetched
  packages.msys2.org), depends on libogg/libvorbis/etc. as separate packages.
  No `-static` legacy Allegro 4 package exists in MSYS2.
- vcpkg `allegro5` port = Allegro 5 only (KNOWN, multiple sources agree); no
  `allegro4` port exists in vcpkg.
- Conclusion: **no current package manager ships Allegro 4.4.x for Windows.**
  Only the two unofficial archive sources above (1.2.A, 1.2.B) are viable.

### 1.4 DLL export mechanism — header-verified (KNOWN, via source checkout)
Allegro 4.4's public API macros (`AL_FUNC`, `AL_VAR`, `AL_ARRAY`, `AL_METHOD`,
`AL_FUNCPTR`) are defined per-platform in `include/allegro/platform/al*.h`,
selected by `include/allegro/internal/alconfig.h`, and used throughout
`include/allegro/*.h` to declare every public function and public variable.
Confirmed by direct grep of the checked-out 4.4.3.1 tag:

- **`almngw32.h`** (MinGW32 target — this is what `alleg44.dll` for the
  target platform would be built from):
  ```
  #if (defined ALLEGRO_STATICLINK) || (defined ALLEGRO_SRC)
     #define _AL_DLL
  #else
     #define _AL_DLL   __declspec(dllimport)
  #endif
  #define AL_VAR(type, name)    extern _AL_DLL type name
  #define AL_ARRAY(type, name)  extern _AL_DLL type name[]
  #define AL_FUNC(type, name, args)  extern type name args
  ```
  **Key finding:** when building the library itself (`ALLEGRO_SRC` defined),
  `_AL_DLL` expands to **nothing** — there is no `__declspec(dllexport)`
  anywhere in the MinGW build path, for either functions or data. This means
  `alleg44.dll` built with MinGW/MinGW-w64 relies entirely on **GNU ld's
  default behavior of auto-exporting every global symbol** when linking a
  shared library (unless `--exclude-all-symbols` or a `.def` file restricts
  it — Allegro's own build does not ship a `.def` for this target). So the
  DLL's export table is effectively "every extern function and every
  `AL_VAR`/`AL_ARRAY` global the headers declare," including the public data
  symbols the task calls out: `key[]`, `screen`, `font`, `mouse_x`,
  `palette_color`, `joy[]`, `allegro_error`, `gfx_capabilities`, `os_type`,
  `allegro_errno`, etc. — all declared with `AL_VAR`/`AL_ARRAY` in
  `system.h`, `gfx.h`, `keyboard.h`, `mouse.h`, `joystick.h`, `palette.h`,
  `text.h`, `base.h`.

- **`almsvc.h`** (MSVC target) differs explicitly:
  ```
  #if defined ALLEGRO_STATICLINK
     #define _AL_DLL
  #elif defined ALLEGRO_SRC
     #define _AL_DLL   __declspec(dllexport)
  #else
     #define _AL_DLL   __declspec(dllimport)
  #endif
  #define AL_FUNC(type, name, args)  _AL_DLL type __cdecl name args
  #define AL_METHOD(type, name, args)  type (__cdecl *name) args
  ```
  MSVC builds **do** use explicit `__declspec(dllexport)` for both functions
  and data, and explicitly spell out `__cdecl`.

### 1.5 Calling convention (KNOWN)
Both platform headers ultimately produce **`cdecl`** on the public API:
MSVC states it explicitly (`__cdecl` in `AL_FUNC`/`AL_METHOD`); MinGW's
`AL_FUNC` has no convention keyword at all, which defaults to GCC's ordinary
i386 System V/cdecl calling convention (MinGW does not use `-mrtd`/stdcall
by default). So a MinGW-built `alleg44.dll` and an MSVC-built one both
present a cdecl C ABI — consistent with the task's expectation.

### 1.6 ABI differences 4.4.1 → 4.4.3.1 (KNOWN, via `git diff 4.4.1 4.4.3.1 -- include/`)
Full diff of `include/` between the two tags touches only 13 files, and
**none of the structural/public headers the task named changed**:
`gfx.h` (BITMAP), `palette.h` (PALETTE), `joystick.h` (JOYSTICK_INFO),
`file.h` (PACKFILE), `sound.h` (SAMPLE), `font.h`, `datafile.h` (DATAFILE)
are **byte-identical** between 4.4.1 and 4.4.3.1. Actual changes:
- `base.h`: version macros only (`ALLEGRO_WIP_VERSION 1→3`,
  `ALLEGRO_VERSION_STR "4.4.1"→"4.4.3"`, `ALLEGRO_DATE` updated).
- `almsvc.h`: `int64_t`/`uint64_t` now come from `<stdint.h>` on MSVC ≥ 1600
  (VS2010+) instead of being hand-defined — build-system detail, not ABI.
- `system.h`: added `OSTYPE_WIN7` constant (additive).
- `fli.h`, `midi.h`: additive declarations.
- `aintpsp.h`, `alpsp.h`, `alpspcfg.h`, `alosxcfg.h`, `alucfg.h`, `alunix*`:
  PSP/OS X/Unix-only, irrelevant to this Windows target.

**Conclusion: the public Windows-relevant ABI is unchanged from 4.4.1 through
4.4.3.1.** A 4.4.3.1-built `alleg44.dll` is struct-layout-compatible with code
written against 4.4.1 headers.

### 1.7 Runtime version check (KNOWN, via `src/allegro.c`)
`install_allegro()`/`allegro_init()` is a macro that calls
`_install_allegro_version_check(..., MAKE_VERSION(ALLEGRO_VERSION,
ALLEGRO_SUB_VERSION, ALLEGRO_WIP_VERSION))` — the version triple is baked in
**at compile time** from the headers the game was built against (4.4.1 →
4,4,1). The DLL-side check logic:
```c
#if ALLEGRO_SUB_VERSION & 1          /* odd = WIP/unstable series */
   version_ok = version == MAKE_VERSION(...);      /* exact match required */
#else                                  /* even = stable series (4.4 is) */
   version_ok = (MAKE_VERSION(VER, SUBVER, 0) == build_ver)
             && (ALLEGRO_WIP_VERSION >= build_wip); /* runtime >= build */
#endif
```
Since Allegro 4.4 is a stable (even) series, **any 4.4.x DLL whose WIP
version is ≥ the WIP version the program was built against will pass** —
i.e. a program built against 4.4.1 headers (WIP=1) will happily accept a
4.4.2 or 4.4.3/4.4.3.1 (WIP=3) DLL at runtime. This directly supports using
a newer 4.4.3.1-based DLL (§1.2.B) as a replacement, version-check-wise.

### 1.8 Static-link vs DLL-link program differences (KNOWN)
- `install_allegro`/`allegro_init()` bakes `ALLEGRO_VERSION_STR`'s numeric
  triple into the call regardless of static/DLL linking — this part of the
  "magic main" is unaffected by the linking mode.
- The `_WinMain` wrapper (`AL_FUNC(int, _WinMain, (void *_main, void *hInst,
  void *hPrev, char *Cmd, int nShow))`, declared in `alwin.h`) implements
  Allegro's `END_OF_MAIN()`/magic-main mechanism that replaces the program's
  real `WinMain`; this exists identically whether Allegro is linked
  statically or dynamically — it's not itself a DLL-vs-static differentiator.
- The actual differentiator is `ALLEGRO_STATICLINK`: when defined at
  compile time, `_AL_DLL` collapses to nothing on **both** MinGW and MSVC
  and the compiler emits direct calls/references into the statically-linked
  object code — there is no import table entry for `alleg44.dll` at all.
  This matches the KNOWN fact from §0 that `icytower15.exe`'s import table
  has no Allegro DLL. To use a DLL build, the game must be **recompiled**
  without `ALLEGRO_STATICLINK` (and linked against the DLL's import
  library) — the existing statically-linked .exe cannot be "hot-swapped"
  onto a DLL after the fact; this matches the task's framing of a rebuild,
  not a binary patch.

---

## 2. logg addon (KNOWN, via source checkout of tag 4.4.3.1)

Lives **inside the Allegro 4.4 source tree**, not a separate project:
`addons/logg/logg.c`, `addons/logg/logg.h`, `addons/logg/loggint.h`, plus a
pkg-config template `misc/logg.pc.in`. Built via the main Allegro CMake build
(`option(WANT_LOGG "Enable logg" on)`, `add_subdirectory(addons/logg)`).

Public API (from `logg.h`), all `SAMPLE*`/opaque-`LOGG_Stream*`-based:
```
SAMPLE*      logg_load(const char* filename);
int          logg_get_buffer_size(void);
void         logg_set_buffer_size(int size);
LOGG_Stream* logg_get_stream(const char* filename, int volume, int pan, int loop);
int          logg_update_stream(LOGG_Stream* s);
void         logg_destroy_stream(LOGG_Stream* s);
void         logg_stop_stream(LOGG_Stream* s);
int          logg_restart_stream(LOGG_Stream* s);
```
The header does support building logg itself as its own DLL
(`LOGG_DYNAMIC` + `ALLEGRO_WINDOWS` → `__declspec(dllexport/dllimport)`,
explicit `__cdecl`, same pattern as Allegro's MSVC config), but
`misc/logg.pc.in` explicitly comments `# always statically linked` in its
`Libs:` line. **No prebuilt logg DLL exists anywhere** (confirmed nothing
found in any of the searches above) — this addon is meant to be compiled
from source and linked into the application, exactly as the task expected.
Practical implication: logg is not something to "find a replacement DLL"
for — it should simply be recompiled from the (small, 3-file) Allegro 4.4.3.1
source against whichever libogg/libvorbis is chosen (§3).

---

## 3. libogg / libvorbis

### 3.1 Official binaries (KNOWN — none exist)
`xiph.org/downloads/` (fetched directly): **source tarballs only** —
libogg 1.3.6, libvorbis 1.3.7, `.tar.gz`/`.tar.xz`/`.zip`. No Windows DLL
binaries are offered by Xiph.Org itself; the page explicitly says Xiph "does
not primarily create software for the end-user."

### 3.2 MSYS2 mingw32 packages (KNOWN, fetched packages.msys2.org)
- `mingw-w64-i686-libogg` **1.3.6-1** → `/mingw32/bin/libogg-0.dll`
  (0.21 MB package / 0.54 MB installed).
- `mingw-w64-i686-libvorbis` **1.3.7-3** → three DLLs:
  `libvorbis-0.dll`, `libvorbisenc-2.dll`, `libvorbisfile-3.dll`
  (0.35 MB package / 2.28 MB installed).
Both are MinGW-w64 GCC builds for the **32-bit (i686)** target — cdecl,
directly usable from a MinGW-w64-built game executable. These are the
cleanest available "existing compatible 32-bit Windows DLL" candidates for
libogg/libvorbis.

### 3.3 vcpkg (INFERRED)
`x86-windows` triplet defaults to **static** linkage; dynamic linkage must be
requested explicitly (e.g. a custom triplet with
`VCPKG_LIBRARY_LINKAGE=dynamic`) to get `ogg.dll` / `vorbis.dll` /
`vorbisfile.dll` MSVC-built DLLs. Exact resulting filenames for a dynamic
x86-windows build were not confirmed by name in the sources fetched — flagged
INFERRED, would need a local vcpkg run to confirm.

### 3.4 ABI stability — verified by direct header diff (KNOWN)
Cloned `github.com/xiph/ogg` and `github.com/xiph/vorbis` and diffed public
headers across the full 1.x range actually used in the wild:
- `include/ogg/ogg.h`, tag `v1.1.4` vs `v1.3.6`: only comment typo fixes
  ("seperate"→"separate") and two **additive** function declarations
  (`ogg_stream_pageout_fill`, `ogg_stream_flush_fill`). `ogg_stream_state`,
  `ogg_page`, `ogg_packet`, `ogg_sync_state` struct layouts are **unchanged**.
- `include/vorbis/codec.h`, tag `v1.2.3` vs `v1.3.7`: only a URL scheme
  fix and one comment typo ("independant"→"independent"). `vorbis_info`,
  `vorbis_dsp_state`, `vorbis_block`, `vorbis_comment` struct layouts are
  **unchanged** across the entire span (2009 → 2020).
Conclusion: **libogg and libvorbis have been ABI-stable across the whole
1.x line**; any 1.1.x+ libogg / 1.2.x+ libvorbis Windows build should link
against code written for whatever specific 1.x version the game currently
embeds (exact embedded version not identified — no version string extracted
from the statically-linked .exe in this pass, since that would require
disassembly/recovery, out of scope for this search task).

### 3.5 Version-string → release-date table (KNOWN, from libvorbis `CHANGES` file in the xiph/vorbis git history)
Useful for identifying which libvorbis version produced a given
`"Xiph.Org libVorbis I <date>"` string, e.g. found in memory/strings of the
statically-linked .exe:

| libvorbis version | Release date | Embedded version string |
|---|---|---|
| 1.0.0 | 2002-07-19 | `Xiph.Org libVorbis I 20020717` |
| 1.0.1 | 2003-11-17 | `Xiph.Org libVorbis I 20030909` |
| 1.1.0 | 2004-09-22 | `Xiph.Org libVorbis I 20040629` |
| 1.1.1 | 2005-06-27 | `Xiph.Org libVorbis I 20050304` |
| 1.1.2 | 2005-11-27 | `Xiph.Org libVorbis I 20050304` (same build string as 1.1.1) |
| 1.2.0 | 2007-07-25 | `Xiph.Org libVorbis I 20070622` |
| 1.2.1 | (unreleased) | `Xiph.Org libVorbis I 20080501` |
| 1.2.2 | 2009-06-24 | `Xiph.Org libVorbis I 20090624` |
| 1.2.3 | 2009-07-09 | `Xiph.Org libVorbis I 20090709` |
| 1.3.0 | 2010-02-25 (unreleased staging) | — |
| 1.3.1 | 2010-02-26 | `Xiph.Org libVorbis I 20100325` |
| 1.3.2 | 2010-11-01 | `Xiph.Org libVorbis I 20101101` |
| 1.3.3 | 2012-02-03 | `Xiph.Org libVorbis I 20120203` |
| 1.3.4 | 2014-01-22 | `Xiph.Org libVorbis I 20140122` |
| 1.3.5 | 2015-03-03 | `Xiph.Org libVorbis I 20150105` |
| 1.3.6 | 2018-03-16 | `Xiph.Org libVorbis I 20180316` |
| 1.3.7 | 2020-07-04 | `Xiph.Org libVorbis I 20200704` |

Given the game's Allegro build/link date (2011-12-06, per the .exe
TimeDateStamp), the embedded libvorbis is most likely **1.2.3** or **1.3.1**
(the versions current in 2010–2011) — this should be confirmed by grepping
the actual .exe for one of the `Xiph.Org libVorbis I` strings above, which
was **not done here** (out of scope: this task was search-only).

---

## 4. libpng3.dll / zlib1.dll — availability of matching versions (KNOWN)

- Shipped `libpng3.dll` = GnuWin32 build, version **1.2.34.3276**
  (2008-12-20). SourceForge GnuWin32 project still hosts this **exact**
  version: `sourceforge.net/projects/gnuwin32/files/libpng/1.2.34-1/`,
  files dated **2008-12-20** (matches the DLL's embedded timestamp exactly):
  `libpng-1.2.34-1-bin.zip` (463.6 kB), `libpng-1.2.34-1-lib.zip` (132.5 kB),
  `libpng-1.2.34-1-setup.exe` (1.4 MB), plus src/doc/dep archives. Newer
  1.2.x GnuWin32 builds also remain listed (1.2.37, etc.), and GnuWin32's
  own summary page (`gnuwin32.sourceforge.net/packages/libpng.htm`) is still
  reachable.
- Shipped `zlib1.dll` = GnuWin32 build, version **1.2.3.2027** (2005-07-20).
  Matching GnuWin32 SourceForge page confirmed:
  `sourceforge.net/projects/gnuwin32/files/zlib/1.2.3/zlib-1.2.3.exe`.
- API stability check (KNOWN, via git diff of `pnggroup/libpng` tags
  `v1.2.5` → `v1.2.34` → `v1.2.59`, the oldest/shipped/newest 1.2.x
  versions): the exact signatures of `png_create_read_struct`,
  `png_set_read_fn`, and `png_read_row` in `png.h` are **byte-identical**
  across that entire range (the only change anywhere near them by 1.2.59 is
  a non-ABI-affecting `PNG_ALLOCATED` source annotation on
  `png_create_read_struct`). So the game's libpng usage is safe against any
  1.2.x libpng build, and since the exact shipped version's own GnuWin32
  package is still downloadable, **no replacement is actually needed** for
  libpng3.dll/zlib1.dll — they can be used as-is, or swapped for a newer
  1.2.x GnuWin32/other MinGW build with no API risk.

---

## 5. pthreadGC2.dll

Shipped version **2.8.0.0** (2009-07-31), `pthreads-win32` project
(`sources.redhat.com/pthreads-win32/`, now hosted at
`sourceware.org/pthreads-win32/`). Game's only usage per the task brief:
`pthread_create`/mutex for the ad-fetch thread — a tiny, ABI-conservative
subset of the API.

- Later official releases (KNOWN, via search results citing
  sourceware.org/pub/pthreads-win32 and the SourceForge `pthreads4w`
  mirror): **pthreads-w32-2-9-1-release** is the latest classic
  "pthreads-win32" release in the 2.x line still built as `pthreadGC2.dll`
  (GCC "GC" = exception-model variant matching the shipped DLL's own
  `Comment: GNU C build -- longjmp thread exiting`). Distribution points:
  `sourceware.org/pub/pthreads-win32/sources/pthreads-w32-2-9-1-release/`,
  `sourceware.org/pub/pthreads-win32/prebuilt-dll-2-9-1-release/` (prebuilt
  DLL/LIB/headers), and SourceForge project `pthreads4w`.
- A `WinBuilds/pthread-win32` GitHub fork (INFERRED, unverified) claims a
  2.10.0.0 build; not confirmed as an official/maintained continuation.
- Given the extremely small API surface actually used (`pthread_create`,
  mutex functions), **2.9.1's `pthreadGC2.dll` is a low-risk drop-in
  replacement** for the shipped 2.8.0.0 — pthreads-win32's public
  `pthread_t`/`pthread_mutex_t`/`pthread_attr_t` types were stable across
  this range (not independently re-diffed here; INFERRED from the library's
  well-known long-term API/ABI stability policy, not from a source diff).

---

## 6. MinGW runtime (KNOWN — not a recovery target)

`msvcrt.dll` is the **system** C runtime (confirmed as an import in
`icytower15.exe` itself, appearing twice in the import table — normal when
two separately-compiled static objects both reference it and get merged into
two IMAGE_IMPORT_DESCRIPTOR entries for the same DLL). Nothing to replace
here; it ships with Windows.

The MinGW/GCC startup glue statically linked into the .exe — `crt1.o`
(`_start`/`__main`), `gccmain.c`'s constructor-running `__main`, libgcc's
runtime support (EH unwinding tables, some soft-float/64-bit helpers) — are
**compile-time runtime support**, not "library code" in the sense of
Allegro/logg/libogg/libvorbis. They have no equivalent standalone DLL to swap
in (MinGW normally statically links libgcc unless `-shared-libgcc` was used,
which this build was not, per the absence of `libgcc_s_*.dll` in the import
table) and are correctly out of scope as a recovery/replacement target.

---

## Summary table

| Component | Shipped version (KNOWN) | Best replacement source found | Status |
|---|---|---|---|
| Allegro 4.4.1 (static) | 4.4.1, built 2011-12-06 | AGS `lib-allegro` tag `v4.4.3.1-agspatch-3`, `lib-allegro_release_i386.tar.gz` (32-bit dynamic build); fallback: SourceForge `Allegro443_MinGW5302.tar.7z` | Candidate found, contents not opened/verified |
| logg addon (static) | bundled w/ Allegro 4.4.1 | N/A — always source-built; use `addons/logg` from the 4.4.3.1 tree | No DLL exists anywhere; rebuild from source as expected |
| libogg (static) | unknown exact 1.x version | MSYS2 `mingw-w64-i686-libogg` 1.3.6-1 → `libogg-0.dll` | ABI-compatible per header diff; exact embedded version not extracted (out of scope) |
| libvorbis (static) | unknown exact 1.x version (likely 1.2.3 or 1.3.1 given 2011 build date) | MSYS2 `mingw-w64-i686-libvorbis` 1.3.7-3 → `libvorbis-0.dll`, `libvorbisenc-2.dll`, `libvorbisfile-3.dll` | ABI-compatible per header diff |
| libpng3.dll | 1.2.34.3276 (2008-12-20, GnuWin32) | Already shipped; identical GnuWin32 package still on SourceForge | No replacement needed |
| zlib1.dll | 1.2.3.2027 (2005-07-20, GnuWin32) | Already shipped; identical GnuWin32 package still on SourceForge | No replacement needed |
| pthreadGC2.dll | 2.8.0.0 (2009-07-31) | pthreads-win32 2.9.1 prebuilt `pthreadGC2.dll` (sourceware.org) | Low-risk drop-in, tiny API surface used |
| msvcrt.dll / MinGW CRT glue | system / static | N/A | Not a target |

## Licences (as identified)
- Allegro 4.4.x: zlib-like "Giftware License" (permissive).
- logg addon: same Allegro licence (bundled in-tree).
- libogg / libvorbis: BSD-style Xiph.Org licence (already documented in the
  shipped `assets/ogg_license.txt`).
- libpng: the libpng licence (permissive, zlib/libpng-style) — text embedded
  in `libpng3.dll`'s own version resource, captured verbatim in the PE dump
  above (GnuWin32 build notice chain from 1996 onward).
- zlib: zlib licence (permissive) — embedded in `zlib1.dll`'s version
  resource.
- pthreads-win32: **LGPL** (per the shipped DLL's own version resource:
  `Licence: LGPL`) — worth flagging since LGPL has redistribution
  obligations (e.g. re-linkability) unlike the other permissive licences
  here.
