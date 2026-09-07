/* assets_standalone.c -- standalone-world implementation of
 * src/icytower/assets.h's 5-function API (win32_pilot.md SS7c,
 * notes/asset_census.md SS6 "Binding model", SS7 mode (a) "drop-in,
 * original folder"). GENERATED FILE -- produced by
 * carrier/gen/gen_assets.py, generated 2026-09-07 12:38:27 UTC. Re-run it to regenerate;
 * do not hand-edit.
 *
 * Loads every one of the 7 datafiles (data.dat, loading.dat, sfx15.dat,
 * the 4 character .dat files) itself, lazily, with the REAL Allegro
 * load_datafile()/packfile_password() (allegro_api.h -- address-free,
 * same declarations this port would use against a real <allegro.h>,
 * see that file's own header comment): once Allegro is built from
 * source (win32_pilot.md SS7c stage S3) these resolve to the real
 * library instead of a guest address, so this file needs no carrier
 * header at all -- it is plain source, exactly like every other file
 * in this directory.
 *
 * Passwords: GAME DATA recovered from the binary (notes/asset_census.md
 * SS5a) -- 'CHEESE' for data.dat/sfx15.dat, '(c) Free Lunch Design' for
 * loading.dat, no password for the (unencrypted) character datafiles.
 * CLEARLY MARKED, single source of truth: src/icytower/
 * assets_table.inc's asset_datafile_family[] table (shared with the
 * carrier implementation, carrier/gen/pf_asset_bindings.h, so the two
 * never carry independently-typo-able copies).
 *
 * OGG-type objects (every sample-kind object this game ships) are
 * decoded through the real Allegro logg addon's logg_load_memory(),
 * the same upstream function the carrier binds to its original
 * address (carrier/gen/pf_asset_bindings.h's own header comment).
 */
#include "allegro_api.h"
#include "assets_table.inc"
#include <string.h>

/* one cache slot per asset_datafile_family[] row, same order */
static DATAFILE *pf_standalone_cache[ASSET_DATAFILE_FAMILY_COUNT];

static DATAFILE *assets_standalone_family(const char *family)
{
    unsigned i;
    for (i = 0; i < ASSET_DATAFILE_FAMILY_COUNT; i++) {
        if (strcmp(asset_datafile_family[i].name, family) != 0)
            continue;
        if (!pf_standalone_cache[i]) {
            packfile_password(asset_datafile_family[i].password);
            pf_standalone_cache[i] = load_datafile(asset_datafile_family[i].path);
            packfile_password((const char *)0);
        }
        return pf_standalone_cache[i];
    }
    return (DATAFILE *)0;
}

static void *assets_standalone_raw(asset_id id)
{
    const struct asset_table_row *row = &asset_table[id];
    DATAFILE *base = assets_standalone_family(row->datafile);
    return base[row->index].dat;
}

BITMAP *asset_bitmap(asset_id id) { return (BITMAP *)assets_standalone_raw(id); }
FONT *asset_font(asset_id id) { return (FONT *)assets_standalone_raw(id); }
PALETTE *asset_palette(asset_id id) { return (PALETTE *)assets_standalone_raw(id); }
void *asset_object(asset_id id) { return assets_standalone_raw(id); }

SAMPLE *asset_sample(asset_id id)
{
    const struct asset_table_row *row = &asset_table[id];
    DATAFILE *base = assets_standalone_family(row->datafile);
    DATAFILE *obj = &base[row->index];
    if (!strncmp(row->type_fourcc, "OGG", 3))
        return logg_load_memory(obj->dat, (it_orig_size_t)obj->size);
    return (SAMPLE *)obj->dat;
}
