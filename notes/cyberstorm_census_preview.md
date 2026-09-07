# CyberStorm (CSTORM.EXE) — PE census preview

Read-only `pefile` inspection of `cyberstorm_forged\assets\CSTORM.EXE`
(736 256 bytes), 2026-09-07, for the generality check in
`notes/extraction_plan.md`. Facts only; nothing was run.

## Facts

| fact | value |
|---|---|
| format | **PE32** (Magic 0x10b), Machine 0x14c = i386 |
| ImageBase | **0x400000** (same as `icytower15.exe`) |
| SizeOfImage / entry RVA | 0x126000 (vs Icy Tower's 0x38c000) / 0x7fe98 |
| Subsystem / Characteristics / DllCharacteristics | 2 = WINDOWS_GUI / 0x8182 (EXECUTABLE_IMAGE, 32BIT_MACHINE, LINE_NUMS_STRIPPED, BYTES_REVERSED_HI) / 0x0 — no ASLR, no DEP opt-in |
| **relocations** | **PRESENT** — `.reloc` 0x118000 + 0xb064, **21 926 fixups** |
| **TLS** | **absent** (no TLS data directory) |
| **debug info** | **NONE** — no DEBUG data directory: no CodeView (no RSDS/NB10), no DWARF sections, no `.stab` |
| exports | 2, from `CSTORM.exe`: `W?AppAbout$n(pnvuiuil)l`, `W?MainWndProc$n(pnvuiuil)l` |
| toolchain tell | the `W?`…`$n(…)` export mangling is **Watcom C++** (not MinGW/GCC, not MSVC) |
| resources | 9 types (CURSOR/ICON/MENU/DIALOG/STRING/ACCELERATOR/GROUP_*/VERSION) |

## Sections (7)

| name | RVA | raw size | characteristics |
|---|---|---|---|
| `BEGTEXT` | 0x1000 | 0x88600 | 0x60000020 code / exec / read |
| `DGROUP` | 0x8a000 | 0x1d000 | 0xc0000040 data / read / write |
| `.bss` | 0xa7000 | 0x6d800 | 0xc0000080 uninit / read / write |
| `.idata` / `.edata` / `.reloc` / `.rsrc` | 0x115000 / 0x117000 / 0x118000 / 0x124000 | 0x1200 / 0x200 / 0xb200 / 0x1c00 | 0xc0000040 / 0x40000040 / 0x42000040 / 0x40000040 |

`BEGTEXT`/`DGROUP` are Watcom segment names, not the `.text`/`.data` spelling
any section-name matcher would look for.

## Imports (162 entries, 6 descriptors)

`KERNEL32.dll` 39 + `KERNEL32.DLL` 40 (two case-distinct descriptors),
`USER32.dll` 56 + `USER32.DLL` 2, `GDI32.dll` 22, **`_INMM.dll` 3**.

`_INMM.dll` is a sidecar DLL beside the EXE — the same situation
`libpng3.dll`/`pthreadGC2.dll`/cnc-ddraw's `ddraw.dll` create for Icy Tower,
and the reason DLL resolution must be a config list rather than the hardcoded
`needs_assets_path()` pair in `carrier/src/imports.cpp`. The case-differing
duplicate descriptors mean the census key must be `(lowercase(dll), name)`.

## What this changes for the extraction plan

- **Applies unchanged**: PE mapper, import census → generated trampolines, the
  wrapper set, VEH + stack walk, DR breakpoint table, argument sensor, virtual
  clock mechanism, arena, RNG pinning, snapshot codec, binding table + 5-byte
  entry patch, the lifter and its offline oracle, the four verdict scripts.
- **Needs a non-DWARF symbol source**: every DWARF-driven generator
  (`gen_interop`, `gen_bindings`, `gen_lib_bindings`, `gen_src_headers`,
  `gen_print_globals`, `gen_game_globals`) and `pf_inspect.py`. Candidates
  already in port_forge: `tools/pf_structural_discovery.py`, `tools/pf_match.py`,
  `scripts/function_fingerprint.py` (Watcom runtime signatures).
- **Assumptions that break**: "no relocations ⇒ must load at ImageBase" (fixed
  base becomes an *option*, rebasing becomes possible); `.text`/`.data` section
  names; DWARF-CU ownership as the digest domain; case-sensitive DLL keys; and
  every Allegro-shaped policy field (no `_handle_timer_tick`, no
  `blit_to_screen`, no `hw_to_mycode`) — which is why they must be fields.
