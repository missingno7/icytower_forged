/* config.c -- save_config() and myDeleteFile(), two of play()'s
 * coastline callees that talk to the filesystem.
 *
 * Original source: F:\projects\icytower\trunk\source\main.c.
 *
 * PROMOTIONS.md batch 14.  Both were on batch 13's closing list of the
 * 19 play()-coastline functions still ORIGINAL.  Neither writes a game
 * global, so for both the comparison domain is the ORDERED trace of the
 * library calls with their arguments -- and, for save_config, the
 * per-call argument VALUES that identify which of the fifteen score
 * tables reached save_hisc_table in which order.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"

#include <stdio.h>

/* ---------------------------------------------------------------------
 * save_config  (0x40e130, 154 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x40e130..0x40e1c9.  Every string
 * read out of the image with pefile.
 *
 *   40e142  log2file("  saving config and scores")      (0x4d5032)
 *   40e158  get_configfile_path(path, 256)              -- the 0x100 is
 *           the literal at 40e147 and matches the frame's 0x108-byte
 *           buffer, so `char path[256]`.
 *   40e168  pack_fopen(path, "wp")                      (0x4d504d)
 *           -- "wp" is Allegro's PACKED write mode: tower.cfg is LZSS
 *           compressed.  save_replay()'s .itr uses "wb" (uncompressed)
 *           for the same call, so the two formats genuinely differ; the
 *           mode string is the only thing that says so.
 *   40e1b4  NULL -> log2file("    *** failed") (0x4d5050) and return.
 *           Note the function returns void either way: a failed config
 *           save is logged and swallowed.
 *   40e17e  save_options(&options, f)                   -- `options` is
 *           0x4fe528, the game global game_state.h declares; the
 *           original passes its ADDRESS as a literal, which is why this
 *           recovered form takes it with `&`.
 *   40e188  15 x save_hisc_table(hisc_tables[i], f), i ascending -- the
 *           loop bound 0xf is at 40e19c and `hisc_tables` is 0x4dd1c0,
 *           declared `Thisc_table *[15]` in game_state.h.  The tables
 *           are written in array order, which is the order view_scores()
 *           and load_config() expect.
 *   40e1a4  pack_fclose(f)
 */
void save_config(void)
{
    char path[256];
    PACKFILE *f;
    int i;

    log2file("  saving config and scores");
    get_configfile_path(path, 256);

    f = pack_fopen(path, "wp");
    if (f == 0) {
        log2file("    *** failed");
        return;
    }

    save_options(&options, f);
    for (i = 0; i < 15; i++)
        save_hisc_table(hisc_tables[i], f);

    pack_fclose(f);
}

/* ---------------------------------------------------------------------
 * myDeleteFile  (0x40cd28, 63 bytes)
 * ---------------------------------------------------------------------
 * Recovered from artifacts/disasm.txt 0x40cd28..0x40cd66.  Three
 * instructions of substance:
 *
 *   40cd51  sprintf(name, "%s%s", path, file)   -- format 0x4d4daa,
 *           read with pefile.  It is "%s%s" and NOT "%s/%s": every
 *           caller's `path` argument is expected to carry its own
 *           trailing separator, the same convention save_replay() uses
 *           for the identical concatenation.
 *   40cd59  delete_file(name)                   -- ALLEGRO's file
 *           deletion (0x44623c), not the CRT's remove()/unlink().  That
 *           matters for the carrier: it is a library call that already
 *           has a binding, not a CRT symbol.
 *
 * The buffer is 0x800 bytes of the 0x814-byte frame -> `char name[2048]`,
 * the same size save_replay() uses for the same job.  The return value
 * of delete_file() is discarded (the function is `void`), so a failed
 * delete is silent.
 */
void myDeleteFile(char *path, char *file)
{
    char name[2048];

    sprintf(name, "%s%s", path, file);
    delete_file(name);
}
