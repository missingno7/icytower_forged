# `logg_load_memory` and its 7-function extension — provenance inspection

Date: 2026-09-07. Read-only inspection per notes/library_compat_verdict.md
(§1, the "logg_load_memory" correction), notes/library_boundary.md item 6,
notes/external_research.md §2 (queued experiment C). Sources:
`artifacts/functions.json`, `artifacts/dwarf_info.txt`, `artifacts/disasm.txt`,
upstream `third_party/allegro-4.4.3.1/addons/logg/logg.c` (checked out).

## 1. The 7 functions (KNOWN)

CU `C:\Lib\allegro4\addons\logg\logg.c` has 18 functions; 11 match upstream
`logg.c` byte-for-byte in name/signature (`logg_load`, `logg_get_buffer_size`,
`logg_set_buffer_size`, `logg_open_file_for_streaming`, `read_ogg_data`,
`logg_play_stream`, `logg_get_stream`, `logg_update_stream`,
`logg_stop_stream`, `logg_restart_stream`, `logg_destroy_stream`). The other 7
(667 B total, sum verified exactly against library_compat_verdict.md §2):

| function | VA | size | DWARF params |
|---|---|---:|---|
| `logg_vf_memfile_seek` | 0x41ff3c | 51 | `(context, offset:ogg_int64_t, whence:int)` |
| `logg_vf_memfile_tell` | 0x41ff70 | 11 | `(context)` |
| `logg_vf_memfile_close` | 0x420034 | 21 | `(context)` |
| `_ov_header_fseek_wrap` | 0x420338 | 43 | `(f:FILE*, off:ogg_int64_t, whence:int)` |
| `logg_vf_memfile_read` | 0x420364 | 52 | `(ptr, size, nmemb, context)` |
| `logg_load_internal` | 0x4204a4 | 296 | `(pVorbisFile:OggVorbis_File*) -> SAMPLE*` |
| `logg_load_memory` | 0x420688 | 193 | `(pData, iSize:size_t) -> SAMPLE*` |

DWARF struct `logg_memfile_context` (12 B, decl_file 1 = logg.c, lines 18-22):
`{ void* pData; int iDataSize; int iCursor; }` — the memory-reader struct.

**Correction to the "7 functions" count**: `_ov_header_fseek_wrap` has
`DW_AT_decl_file 2` (all 6 others are file 1 = logg.c) and its signature
(`FILE*, ogg_int64_t, int`) plus body (`fseek(f,off,whence)` guarded by a
NULL check) match libvorbisfile's own internal `_ov_header_fseek_wrap`
(used to build `OV_CALLBACKS_DEFAULT`'s stdio seek callback) — confirmed by
`objdump -t` on our own stock build,
`third_party/build-allegro-4.4.3.1/addons/logg/CMakeFiles/logg.dir/logg.c.obj`,
which contains the identical static symbol `__ov_header_fseek_wrap` even
though it is never referenced from `logg.c`'s own source text. INFERRED:
this function is genuine vendored **libvorbisfile** code, not an Icy Tower
addition — it landed in the "logg CU, 7 extra functions" bucket only because
our compile-unit attribution bins by address range, and the linker placed it
adjacent to the memory-loading code. The real custom-code count is **6
functions / 624 B**: the 4 memfile callbacks + `logg_load_internal` +
`logg_load_memory`.

## 2. Is it a thin `ov_open_callbacks` wrapper over shared sample code? YES (KNOWN)

`logg_load_internal` (0x4204a4) is byte-for-byte what upstream 4.4.3.1's
monolithic `logg_load` does *after* opening the stream: `_al_malloc(sizeof
SAMPLE)`; set `bits=16` (0x10), `stereo=(vi->channels>1)`, `freq=vi->rate`,
`priority=128` (0x80), `len=ov_pcm_total`, `loop_start=0`, `loop_end=len`;
`_al_malloc(len*4)` for `data` (`shl $0x2,%eax` = `sizeof(short)*2ch`); loop
`ov_read(&ovf, buf, logg_bufsize, 0, 2, 0, &bitstream)` into `data+offset`;
`ov_clear`; `free(buf)`; `return samp`. Every field offset/constant in the
disassembly (`0x10,0x80,0x10/0x14/0x18/0x20` struct offsets) matches
`third_party/allegro-4.4.3.1/addons/logg/logg.c:44-69` line for line.

`logg_load` (0x4205cc) and `logg_load_memory` (0x420688) are structurally
identical front-ends: both build an `ov_callbacks{read,seek,close,tell}`
4-word struct on the stack (`rep movsl`, 4 dwords) and call
`ov_open_callbacks(datasource, &ovf, NULL, 0, callbacks)`, then on success
tail into `logg_load_internal(&ovf)`; on failure both `strncpy` the same
error string into `allegro_error` and free their resource. The only
difference: `logg_load` opens a `FILE*` via `fopen` and copies the global
`OV_CALLBACKS_DEFAULT` array (VA 0x4d81ec, referenced identically by
`logg_open_file_for_streaming`); `logg_load_memory` `malloc`s a 12-byte
`logg_memfile_context{pData,iDataSize,iCursor=0}` and points the callback
struct at `logg_vf_memfile_{read,seek,close,tell}` (VAs hard-coded as
immediates at 0x4206b4-0x4206c9, in the struct order `read,seek,close,tell`
— matches `ov_callbacks`' declared field order). The 4 memfile callbacks
themselves are trivial bounds-checked `memcpy`/cursor arithmetic over
`pData/iDataSize/iCursor` (`logg_vf_memfile_read` clamps to
`iDataSize-iCursor`; `_seek` handles `SEEK_SET/CUR/END` via the 3-way
`test/cmp` chain at 0x41ff48-0x41ff5c; `_close` just `free`s the context).

**Answer: yes** — a minimal, correct `ov_open_callbacks` memory wrapper
feeding the exact code upstream already used for `SAMPLE` construction,
refactored out as `logg_load_internal` and shared by both entry points.

## 3. Known public snippet or local extension? LOCAL EXTENSION (unresolved) — INFERRED

WebSearch for `logg_load_memory ov_open_callbacks SAMPLE` and
`allegro.cc logg pData iDataSize iCursor` surfaced two on-topic threads —
"A4 - logg streaming from memory" (allegro.cc/forums/thread/613661) and
"Loading OGG as SAMPLE with LOGG" (allegro.cc/forums/thread/606019) — plus
the standalone `opensnc.sourceforge.net/logg/` LOGG project page. Both
allegro.cc threads returned HTTP 500 on fetch (site issue, not confirmed
content) and the sourceforge page's cached text names no memory-loading
function. No source matching the exact identifiers `logg_load_memory`,
`logg_vf_memfile_read/seek/close/tell`, or the `pData/iDataSize/iCursor`
field names was found. The technique itself (4-callback `ov_callbacks`
struct over a buffer+cursor) is the standard, widely-documented
`ov_open_callbacks` memory-loading idiom — not exotic — so independent
reinvention is plausible and unremarkable. **Verdict: LOCAL EXTENSION
(unresolved)** — provenance not traced to a specific public source; small
enough (§5) that provenance doesn't matter for recovery.

## 4. Callers (KNOWN)

- `getSampleFromOggDatafile` (0x40ca2c): reads a `DATAFILE` entry at
  `base + index*16` — `dat` pointer (off 0) and `size` (off 8) — and tail
  `jmp`s straight into `logg_load_memory(pData=dat, iSize=size)`. No
  wrapping logic at all.
- `loadCustomSoundDF` (0x402040): checks the datafile object's type dword at
  `+4` against `0x53414d50` ("PMAS"); if it matches, treats the object as an
  already-decoded `SAMPLE*` (steals `dat->dat`, zeroes it to avoid
  double-free); otherwise tail-`jmp`s to `getSampleFromOggDatafile` → OGG
  bytes decoded via `logg_load_memory`.
- Callers of `getSampleFromOggDatafile`: `_init_game` (0x40f9dd, 0x40f9f7, …
  16+ sites) with fixed indices into the datafile loaded via
  `load_datafile_callback` (global `0x4dd240`) — this is the `sfx15.dat`
  fixed-slot sound table (per notes/asset_census.md, `sfx15.dat` has 23
  objects / 22 OGG, password `CHEESE`). `load_sounds` (0x40212c,
  `custom.c`) is the character-pack loader that walks a datafile object list
  to the `DAT_END` (type == -1) sentinel to count entries
  (notes/asset_census.md line 231) and, for OGG-tagged entries, resolves
  through `loadCustomSoundDF`/`getSampleFromOggDatafile` the same way.
  `loadCustomSoundFILE` (0x401fc0) is a sibling for standalone files
  (`load_sample` for `.wav`, else tail-`jmp` to `logg_load` — not
  `logg_load_memory`).

## 5. Clean reimplementation size and oracle domain

A clean reimplementation is small: the 4 memfile callbacks (~20-25 lines
total — bounds-checked read, 3-way seek, 1-line tell/close) +
`logg_load_memory` itself (~15 lines: malloc context, build `ov_callbacks`,
`ov_open_callbacks`, delegate) + `logg_load_internal` (~25 lines, but this
is a **mechanical extraction** of upstream `logg_load`'s existing body, not
new logic — see §2). **Estimate: ~45-60 lines new/moved, of which only
~35-40 are genuinely novel** (the internal-factoring is copy/rename).
Trivial recovery burden, as notes/external_research.md §2 already concluded.

Oracle comparison domain (what a reimplementation must match): the returned
`SAMPLE*`'s 7 scalar fields set by `logg_load_internal` — `bits` (always
16), `stereo` (0/1 from `vi->channels`), `freq` (`vi->rate`), `priority`
(always 128), `len` (`ov_pcm_total`), `loop_start` (always 0), `loop_end`
(`== len`) — plus the `data` buffer's `len*channels*2` PCM bytes, produced
by the identical `ov_read(..., 0 /*ENDIANNESS*/, 2 /*16-bit*/, 0 /*unsigned
false = signed*/, ...)` loop already used by upstream `logg_load`. `param`
(the 8th `SAMPLE` field) is never written by either path — match that too
(leave uninitialized/whatever `_al_malloc` yields, do not zero it
speculatively).

## Verdict

**LOCAL EXTENSION (unresolved)** for the 6 genuinely custom functions
(`logg_vf_memfile_{read,seek,close,tell}`, `logg_load_internal`,
`logg_load_memory`) — a minimal, mechanically-derived `ov_open_callbacks`
memory wrapper over upstream's own sample-building code, no public source
matched. `_ov_header_fseek_wrap` is separately classified **UPSTREAM**
(libvorbisfile's own internal function, mis-binned into this CU by
address-range attribution) and should be excluded from the "custom code"
tally in future totals.
