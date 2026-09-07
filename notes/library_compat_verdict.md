# Library compatibility verdict — replacing the statically linked third-party code

Date: 2026-09-07. Per-symbol pass over the exact boundary in
`notes/library_boundary.md` / `artifacts/lib_boundary.json`, applying the
classification scheme of `win32_pilot.md` §7b. Full row-by-row table:
`artifacts/library_compat_table.md` (generated, 138 rows) and
`artifacts/library_compat.json`. This document is the narrative: totals,
adapters, corrections found while verifying, and the recommended path.

Method: no binary archive was downloaded (per instructions). Every
PUBLIC/INTERNAL classification below was checked against the **actual header
text** of `liballeg/allegro5` tag `4.4.3.1` and, where relevant,
`adventuregamestudio/lib-allegro` tag `v4.4.3.1-agspatch-3` and the `logg`
addon's own `.c`/`.h`, fetched directly (`raw.githubusercontent.com`,
`api.github.com`) and grepped line-by-line for `AL_FUNC`/`AL_VAR`/`AL_ARRAY`/
`AL_PRINTFUNC` declarations — this is KNOWN evidence (I read the actual
declaration), not INFERRED. Two things could not be settled without a binary
download and are listed in §7.

## 1. Headline verdict

**Still yes, with two corrections that change the work list.** 137 of 138
required symbols map cleanly onto existing library sources with **zero**
struct/ABI adapters beyond the one already known (`_win_hcursor`). But:

- **One of the "2 logg" entry points does not exist upstream.**
  `logg_load_memory` (and 6 helper functions behind it, 667 of the addon's
  2041 B) is custom code the Icy Tower developers added to their private copy
  of Allegro's `logg.c`. It is not in Allegro 4.4.1, 4.4.3.1, or the AGS
  fork — all three ship the *same* 11-function, 4730-byte `logg.c`. This is
  new evidence beyond `library_boundary.md`, which treated logg as 100 %
  reused (§7).
- **The AGS `lib-allegro` fork ships no Windows DLL.** Its Windows CI
  (`.cirrus.yml`) builds only MSVC static `.lib` files (`alleg-static.lib`,
  `alleg-static-mt.lib`, debug variants); the `lib-allegro_release_i386.tar.gz`
  / `_amd64.tar.gz` release assets that `notes/library_candidates.md` flagged
  "confidence: MEDIUM" as a Windows DLL are, per that same CI file, the
  **Linux** task's output (`dpkg --print-architecture` names the archive,
  and it packages `include/` + `lib/`, i.e. a `.so` build). This was
  confirmed from the CI script and the GitHub Releases API metadata, not by
  opening the archive. **Conclusion: no viable prebuilt Windows Allegro
  4.4.x DLL exists anywhere found.** Source build is not just cleaner, it is
  the only path (§6).

Everything else confirms `library_boundary.md`'s original call: the boundary
is a clean header-level ABI, gfx.h/palette.h/joystick.h/file.h/sound.h/
font.h/datafile.h are byte-identical 4.4.1→4.4.3.1, and I independently
re-verified `BITMAP`, `RGB`/`PALETTE`, `GFX_DRIVER`, `FONT`, `PACKFILE`
field-for-field against `carrier/gen/it_types.h` (DWARF-recovered from the
actual binary) — every field name, order and pointer type matches the
4.4.3.1 header text exactly. **0 rows classified PUBLIC BUT ABI-DIFFERENT.**

## 2. Totals by classification (138 rows: 100 functions + 26 globals + 12 distinct callback receivers covering 24 registrations)

| classification | count | rows |
|---|---:|---|
| PUBLIC + AVAILABLE | 97 | 96 Allegro-family functions (incl. `_WinMain`, `_install_allegro_version_check`, macro-invoked but genuinely public) + `logg_load` |
| GLOBAL SHARED STATE | 25 | all Allegro globals except `_win_hcursor`: `key`, `screen`, `font`, `gfx_driver`, `joy`, `mouse_x/y/b`, `gui_fg/bg_color`, `allegro_errno`, `system_driver`, 12× `_rgb_*_shift_*` |
| CALLBACK EDGE | 12 receivers / 24 sites | `set_display_switch_callback`(6), `png_set_read_fn`(2), `for_each_file_ex`(2), `load_datafile_callback`(2), `register_datafile_object`(2), `register_bitmap_file_type`(2), `png_set_write_fn`(2), `install_int`(2), `_WinMain`(1), `set_close_button_callback`(1), `qsort`(1), `pthread_create`(1) |
| INTERNAL NOT EXPORTED | 3 | `_color_load_depth`, `_fixup_loaded_bitmap` (functions, `include/allegro/internal/aintern.h`), `_win_hcursor` (global, `include/allegro/platform/aintwin.h`) |
| UNKNOWN | 1 | `_logg_load_memory` — not present in any candidate source (§1) |
| PUBLIC BUT ABI-DIFFERENT | 0 | — |

Excluded from these 138 rows (out of the per-row request, but part of the
95-import boundary): the 95 direct-DLL imports (msvcrt 46, libpng3 35,
WSOCK32 9, KERNEL32 3, USER32 1, SHELL32 1) are system CRT/OS calls or
already-dynamic libpng3.dll — not Allegro-family, not a replacement
decision, see §5. 6 of the 30 raw callback refs are game→game
(`for_each_directory`×4, `handle_menu`×2, the game's own dispatchers) and
are not a library edge at all.

No `fixed`-point inline math (`itofix`, `fixmul`, `fixadd`, …, from
`allegro/inline/*.inl`) appears among the 100 names — the game never calls
Allegro's fixed-math API, so there is nothing in this boundary that is
secretly a compiled-in inline rather than a real library call. Every one of
the 100 names resolved to a top-level `AL_FUNC`/`AL_PRINTFUNC` declaration
(exact `file:line` in `artifacts/library_compat_table.md`), never a `static`
or `AL_INLINE` body.

## 3. Calling convention and ABI

Both the MinGW and MSVC 4.4.x platform headers produce a **cdecl** public
API (`almngw32.h`: no convention keyword on `AL_FUNC`, GCC's i386 default is
cdecl; `almsvc.h`: `AL_FUNC` expands with an explicit `__cdecl`). The game
itself is GCC-compiled cdecl throughout (COFF confirms no `@N` stdcall
decoration on any of the 100 names, matching `import_names.inc`). No
calling-convention adapter is needed regardless of which candidate toolchain
builds Allegro.

Export mechanism (`library_candidates.md` §1.4, re-confirmed by header read):
MinGW's `almngw32.h` collapses `_AL_DLL` to nothing when `ALLEGRO_SRC` is
defined, so a MinGW-built `alleg44.dll` relies entirely on GNU ld's default
auto-export of every non-static global — no `.def` file restricts it. A
`_color_load_depth`/`_fixup_loaded_bitmap` DLL-export gap therefore only
matters if Allegro becomes a **separate DLL** from the vendored `loadpng.c`;
under the recommended source-build (§6), both are compiled into the same
static link unit and the calls are ordinary intra-program C linkage — no
export table involved at all.

## 4. Struct layouts — verified field-for-field, all match

Compared `carrier/gen/it_types.h` (recovered from the shipped binary's own
DWARF) against the fetched 4.4.3.1 headers:

| type | it_types.h fields | 4.4.3.1 header | match |
|---|---|---|---|
| `BITMAP` | w,h,clip,cl,cr,ct,cb,vtable,write_bank,read_bank,dat,id,extra,x_ofs,y_ofs,seg,line[] | `gfx.h:273-289`, identical field order/types | exact |
| `RGB`/`PALETTE` | r,g,b,filler; `RGB[256]` | `palette.h:26-32` | exact |
| `GFX_DRIVER` | (not separately materialized in it_types.h — game only reads `gfx_driver->w/h` via the `SCREEN_W`/`SCREEN_H` macros in `gfx.h:304-305`) | `gfx.h:80-116`, `w,h` at the documented offset after 24 method pointers + 4 char* fields | consistent |
| `FONT` | data,height,vtable | matches `font.h` order | exact |
| `PACKFILE`/`PACKFILE_VTABLE` | vtable,userdata,is_normal_packfile,normal{...} | matches `file.h` | exact |
| `SAMPLE`, `MIDI`, `DATAFILE` | as recovered | unchanged 4.4.1→4.4.3.1 per `library_candidates.md` §1.6 git diff | exact |

`JOYSTICK_INFO` and `DIALOG` never materialize as named struct types in
`it_types.h` — the game never dereferences their fields directly (only via
Allegro's own accessor functions/macros), so there is no struct-layout risk
to check for them.

## 5. The 95 DLL imports and the vendored addon (not per-row, contextual)

msvcrt (46)/KERNEL32 (3)/USER32 (1)/SHELL32 (1)/WSOCK32 (9) = 60 imports are
the system CRT and Win32 API — ship with Windows, not a recovery or
replacement target (`library_candidates.md` §0, §6). libpng3.dll (35, all
from vendored `loadpng.c`) is version 1.2.34.3276 GnuWin32 — the exact
package is still on SourceForge, dated identically to the shipped DLL; no
replacement needed. Same for `zlib1.dll` (1.2.3.2027) and `pthreadGC2.dll`
(2.8.0.0, pthreads-win32) — both already dynamically linked and shipped in
`assets/`.

## 6. Adapters required

1. **`_win_hcursor` shim** (confirmed). One-liner:
   `#include <allegro/platform/aintwin.h>` (declares `AL_VAR(HCURSOR,
   _win_hcursor)` at line 129), or a 3-line `extern HCURSOR _win_hcursor;`
   platform helper if avoiding the internal header. `main_menu_callback`
   writes it twice, right after `LoadCursorA(NULL, IDC_HAND)`.
2. **loadpng addon compiled from upstream source**, into the same static
   link unit as Allegro core. No adapter needed for `_color_load_depth`/
   `_fixup_loaded_bitmap` under this path — they resolve as ordinary
   intra-unit calls, exactly as upstream `loadpng.c` itself does.
3. **logg compiled from Allegro source** against external libogg/libvorbis
   — but **only covers `logg_load`**. `logg_load_memory` needs a genuine
   recovery/reimplementation: ~7 functions implementing a memory-backed
   `ov_open_callbacks()` reader (seek/tell/read/close over an in-memory
   buffer instead of a `FILE*`), following the pattern already visible in
   the stock `logg_open_file_for_streaming`/`read_ogg_data` but targeting a
   buffer. libvorbisfile's callback API has supported in-memory sources
   since 1.x; only Icy Tower's own glue is missing. Estimated scope: 667 B /
   7 functions, small enough to lift or hand-write and verify by replay
   digest (same oracle model as any other promoted function, §7 of
   `win32_pilot.md`).
4. **No struct/ABI shim** — §4 found zero mismatches.
5. **No calling-convention shim** — §3 found cdecl on both sides throughout.

## 7. Downloads needed for KNOWN verification (not performed — binary archives)

These remain INFERRED because confirming them requires opening a binary
archive, which this pass did not do:

| file | source | size | what it would confirm |
|---|---|---|---|
| `Allegro443_MinGW5302.tar.7z` | `sourceforge.net/projects/unofficialallegro5distribution/files/` | 7.2 MB | whether this "Unofficial Allegro Library Distribution" actually contains a MinGW-built `alleg44.dll` with the expected AL_VAR data exports (`key[]`, `screen`, `font`, …) — never opened by `library_candidates.md` either |
| `Allegro443_MinGW4814.tar.7z` | same page | 7.6 MB | same, older MinGW build |
| `mingw-w64-i686-libogg-1.3.6-1-any.pkg.tar.zst` (or equivalent) | `packages.msys2.org`, package `mingw-w64-i686-libogg` | ~0.21 MB | actual export table of `libogg-0.dll` (low risk: game never calls libogg directly, only `logg.c` does, against fully public API) |
| `mingw-w64-i686-libvorbis-1.3.7-3-any.pkg.tar.zst` | `packages.msys2.org`, package `mingw-w64-i686-libvorbis` | ~0.35 MB | export table of `libvorbis-0.dll`/`libvorbisfile-3.dll` (same low-risk note) |

Given §1's finding that the AGS fork's Windows CI produces no DLL at all,
the SourceForge archives above are now the *only* remaining candidates for a
prebuilt Windows Allegro DLL; §8 recommends not pursuing them (source build
is required for logg/loadpng regardless, so there is little to save).

## 8. Recommended simplest path, per library

- **Allegro 4.4.x**: build from source — upstream `liballeg/allegro5` tag
  `4.4.3.1` (identical Windows-relevant headers to the AGS fork, §1.6 of
  `library_candidates.md`) — with the **same toolchain family as the
  original** (MinGW/TDM-GCC), `ALLEGRO_STATICLINK`, C-only blitters
  (`src/c/*`, no `src/i386/*`), `DEBUGMODE` off. This matches
  `win32_pilot.md` §6a's requirement (GCC `-mfpmath=387`/x87 fidelity) and
  is now also the *only* option — no usable prebuilt Windows DLL exists
  (§1, §7). Do not use the AGS fork's `.lib` files: they are MSVC-toolset
  static libraries, the wrong toolchain family for this project's x87/FNINIT
  determinism requirement, and static libs sidestep the DLL question
  entirely rather than answering it.
- **logg addon**: compile `addons/logg/logg.c` from the same Allegro source
  tree, **plus** recover/rewrite the `logg_load_memory` extension (§6.3) —
  the one piece of this boundary that is not pure third-party.
- **libogg / libvorbis**: link against MSYS2 `mingw-w64-i686-libogg`
  (1.3.6) / `mingw-w64-i686-libvorbis` (1.3.7) prebuilt DLLs. ABI-stable
  across the whole 1.x line (`library_candidates.md` §3.4, header diff
  1.1.4→1.3.6 and 1.2.3→1.3.7 both purely additive/cosmetic). Zero adapter:
  the game never calls these directly (0 edges), only `logg.c` does, and
  `logg.c`'s own calls are unaffected by the version gap.
- **libpng3.dll / zlib1.dll**: already shipped at the exact matching
  GnuWin32 versions (1.2.34.3276 / 1.2.3.2027); use as-is. No replacement
  needed.
- **pthreadGC2.dll**: already shipped (2.8.0.0); optional low-risk upgrade
  to pthreads-win32 2.9.1 if desired, not required — the game's usage
  (`pthread_create` + mutex, one ad-fetch thread) is a tiny, stable subset.

## 9. Third-party code excluded permanently from the recovery workload

By CU family, bytes from `artifacts/lib_boundary.json.ownership` (KNOWN
unless noted):

| region | bytes | functions | status |
|---|---:|---:|---|
| `allegro_core` | 289 796 | 906 | excludable — reached only through the 97 PUBLIC+AVAILABLE names + 25 globals + 12 callback receivers above |
| `allegro_c` (C blitters) | 106 148 | 270 | excludable — `stretch_blit`/`stretch_sprite` are its only 2 direct game edges, both public |
| `allegro_win` | 64 664 | 306 | excludable — `_WinMain` is its only direct game edge, public by macro |
| `allegro_misc` | 6 075 | 31 | excludable — no direct game edges found |
| `allegro_addon` (logg) | 2 041 | 18 | **partially excludable**: 1 374 B / 11 functions match stock upstream `logg.c` exactly and are excludable; **667 B / 7 functions do not exist upstream** (`logg_load_memory` and its helpers) and belong in the recovery workload, not this table |
| `libvorbis` + `libvorbisfile` + `libogg` | 121 232 | 296 | excludable — **zero** direct game edges (all reached through `logg_load`/`logg_load_memory`) |
| `crt`/`libgcc`/import thunks | 35 851 | 385 | excludable — replaced by whatever CRT the standalone build links |
| vendored `loadpng.c`/`savepng.c`/`regpng.c` | 4 260 | 15 | excludable — upstream addon, re-vendor the same 3 files |
| vendored `strptime.c`, `timecompat.c` | 2 270 | 5 | excludable — BSD/glibc-derived compat shims, replace with host CRT or re-vendor |
| **total excluded** | **631 670** | | **84.1 % of `.text`**, revised down from `library_boundary.md`'s 632 337 B by the 667 B logg correction |

Still flagged ambiguous, unchanged from `library_boundary.md` (out of this
verdict's scope — not Allegro-family): `csv.c` (671 B, 6 fn), `httpget.c`
(2 327 B, 10 fn) — no upstream identified, treated as game-owned until one
is found.

**Recovery workload, revised**: 116 113 B / 217 functions / 18 CUs (the
game's own code, unchanged) **plus** the 667 B / 7-function
`logg_load_memory` extension identified in §1, **minus** whatever `csv.c`/
`httpget.c` turn out to be. The library-boundary total explicitly excluded
forever shrinks by exactly that same 667 B, from 632 337 B to 631 670 B.

## 10. What this changes in the existing notes

`notes/library_boundary.md` §1's "loadpng is safely externalizable" claim
stands. Its "the game never calls libvorbis, libvorbisfile or libogg
directly (0 edges)... reached through exactly two logg entry points" claim
(§3a) is corrected: one of those two entry points is not itself upstream
code. `notes/library_candidates.md` §1.2.B's "Confidence: MEDIUM" on
`lib-allegro_release_i386.tar.gz` as a Windows DLL is resolved to **KNOWN:
it is not** (it is the Linux CI task's `.tar.gz`, per `.cirrus.yml`).
