# Generated src/icytower headers
Produced by `tools/pf_win32_gen_src_headers.py` (reusing `carrier/gen/gen_interop.py`'s DWARF parser) from `artifacts/dwarf_info.txt` + `artifacts/functions.json`, scope=`game`.
## Counts
| item | count |
|---|---:|
| game-CU globals | 156 |
| game-CU functions | 253 |
| game structs/unions (game_types.h) | 50 |
| library structs/unions (allegro_types.h) | 20 |
| game typedefs (game_types.h) | 91 |
| library typedefs (allegro_types.h) | 18 |
| opaque types | 4 |

CU scope: 25 compile units in scope out of 148 total in the DWARF.

## Opaque / unrepresentable types (4)
- ` BITMAP` member `line`: array member with unknown bound (C flexible array, DWARF contributes 0 bytes to the struct)
- ` RLE_SPRITE` member `dat`: array member with unknown bound (C flexible array, DWARF contributes 0 bytes to the struct)
- ` FONT_GLYPH` member `dat`: array member with unknown bound (C flexible array, DWARF contributes 0 bytes to the struct)
- `struct pthread_mutex_t_`: no DW_AT_byte_size (declaration-only, never fully defined in scope)

## Name collisions (disambiguated with `__<cu-basename>`)
Functions: 0 collisions. Globals: 0 collisions.

## Purity-gate identifier renames (0)
A DWARF-original name colliding with scripts/check_native_layer.py's banned-prefix list (`PF_`, `pf_`, `IT_G_`, `IT_F_`, `PFN_`, `lifted_`) is renamed with an `icy_orig_` prefix; nothing else about it changes. Seen so far: Allegro's own PACKFILE_VTABLE member-naming convention ("packfile function") coincidentally uses the same `pf_` prefix.

None in this scope.

## Type origin promotions (0)
A library type is normally never allowed to depend on a game type (allegro_types.h must stand alone); any entity this run had to promote to game_types.h to keep that true is listed here.

None -- no library-origin type in this scope depended on a game-origin type.

## Struct/union/typedef name conflicts across CUs (6, whole-DWARF, from gen_interop.compute_canonical)
None of the whole-DWARF conflicts gen_interop.py recorded involve a type reachable in this scope.

## Topological-order anomalies (0)
None -- no by-value/typedef dependency cycles found.

## Verification
```
python scripts/check_native_layer.py
cl /nologo /c /W3 /TC src\icytower\update_frame.c src\icytower\is_solid.c src\icytower\state.c
cl /nologo /c /W3 /TC /Icarrier\gen /FIcarrier\gen\pf_bindings_src.h src\icytower\update_frame.c src\icytower\is_solid.c
cl /nologo /W3 /TC src\icytower\game_types_check.c /Fe:src\icytower\game_types_check.exe
src\icytower\game_types_check.exe
```
