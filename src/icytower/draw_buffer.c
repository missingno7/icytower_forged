/* draw_buffer.c -- draws a `\n`-separated text buffer as a stack of lines,
 * one `textprintf_ex` call per line, and reports back where the next line
 * would start.
 *
 * ASSET seam demo (win32_pilot.md SS7c, notes/asset_census.md SS3/SS6,
 * src/icytower/ASSETS.md): this is one of the census's 42 constant-index
 * `data[N].dat` sites -- `data[53].dat` used as a `FONT *` -- rewritten to
 * use the generated `asset_id`/`asset_font()` seam instead of the raw
 * index. `data[53]` is `FONT_MONO` (notes/asset_census.md SS3: "the five
 * fonts -- ... FONT_MONO (53) x24"), so the clean form reads
 * `asset_font(ASSET_DATA_FONT_MONO)`.
 *
 * Original source: F:\projects\icytower\trunk\source\profile.c, decl_line
 * 513 (artifacts/dwarf_subprograms.json); prototype from
 * src/icytower/game_funcs.h: `int draw_buffer(BITMAP *bmp, char *buffer,
 * int x, int y)`.
 *
 * Recovered instruction-by-instruction from artifacts/disasm.txt
 * (0x4191c8..0x419280, 185 bytes, F:\projects\icytower\trunk\source\
 * profile.c) -- no DWARF parameter names beyond the prototype's, so `bmp`,
 * `buffer`, `x`, `y` are kept as game_funcs.h already names them; `line`,
 * `p`, `n`, `c` are this recovery's own names for the disassembly's
 * `-0x118(%ebp)` stack buffer, `%ebx`, `%edx` and `%al`.
 *
 * Behaviour (faithful to the byte sequence, not just its intent):
 *   - walks `buffer` one byte at a time, accumulating characters into a
 *     280-byte local line buffer (`-0x118` = 280 in the original's stack
 *     frame -- an unchecked copy, exactly as in the disassembly: nothing
 *     here bounds-checks `n` against `sizeof(line)`, and neither did the
 *     original);
 *   - on '\n', NUL-terminates the accumulated line, draws it with
 *     `textprintf_ex(bmp, asset_font(ASSET_DATA_FONT_MONO), x, y,
 *     makecol(30, 20, 10), -1, "%s", line)`, advances `y` by 10, and
 *     starts the next line;
 *   - on the terminating NUL, stops -- WITHOUT drawing whatever was
 *     accumulated since the last '\n'. A buffer whose last line has no
 *     trailing '\n' silently drops that last line; this is the original's
 *     actual behaviour (the only `textprintf_ex` call site is inside the
 *     '\n' branch), not a simplification, so callers must pass a
 *     trailing '\n' if the last line is meant to be visible;
 *   - returns the final `y` (the caller's usual "next free y" idiom for
 *     stacking more UI below this block); an empty `buffer` returns the
 *     original `y` unchanged and draws nothing.
 *
 * No FPU, no other game function called, no game global touched --
 * `add_combo`-class (win32_pilot.md SS7a/PROMOTIONS.md) recovery, offline-
 * verifiable in principle by carrier/lift/harness/lift_check.py's memory-
 * domain diffing (its inputs would be `buffer`'s bytes, `x`, `y`, and the
 * `bmp`/output-pixels domain) -- NOT run here: the harness's existing
 * driver scaffolds scalar-domain vectors (win32_pilot.md SS7a's
 * PROMOTIONS.md entries), and extending it to a `BITMAP`/pixel-output
 * domain plus a `FONT` dependency is new harness machinery, out of scope
 * for this asset-seam change (matching the precedent already set for
 * `play_jump_sound` in PROMOTIONS.md's "Skipped this pass" section, for an
 * analogous reason). Verified here by COMPILE ONLY, both worlds:
 *   standalone: cl /nologo /c /W3 /TC /Isrc\icytower src\icytower\draw_buffer.c
 *               -- 0 errors, 0 warnings
 *   carrier:    cl /nologo /c /W3 /TC /Icarrier\gen /Isrc\icytower
 *               /FIpf_bindings_src.h /FIpf_lib_bindings.h /FIpf_asset_bindings.h
 *               src\icytower\draw_buffer.c
 *               -- 0 errors, 0 warnings (src/icytower/ASSETS.md "Show the
 *               seam" has the exact carrier-world command and the
 *               pf_bindings_src.h regeneration it depends on)
 */
#include "allegro_api.h"
#include "assets.h"

int draw_buffer(BITMAP *bmp, char *buffer, int x, int y)
{
    char line[280];
    char *p = buffer;
    char c = *p;
    int n;

    if (c == 0)
        return y;                      /* empty buffer: nothing to draw */

    n = 0;
    for (;;) {
        if (c != '\n') {
            line[n++] = c;
            c = *(++p);
            if (c == 0)
                break;                  /* trailing partial line: dropped, see above */
            continue;
        }

        line[n] = 0;
        {
            int color = makecol(30, 20, 10);
            textprintf_ex(bmp, asset_font(ASSET_DATA_FONT_MONO), x, y, color, -1, "%s", line);
        }
        y += 10;
        n = 0;

        c = *(++p);
        if (c == 0)
            break;
    }
    return y;
}
