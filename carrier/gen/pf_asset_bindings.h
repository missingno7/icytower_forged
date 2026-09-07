/* pf_asset_bindings.h -- GENERATED FILE. DO NOT EDIT.
 * Produced by carrier/gen/gen_assets.py.
 * Generated: 2026-09-07 12:38:27 UTC
 *
 * Carrier-world implementation of src/icytower/assets.h's 5-function
 * API (win32_pilot.md SS7c, notes/asset_census.md SS6 "Binding model"):
 * an asset_id resolves to the SAME object in the SAME DATAFILE the
 * original game code already reads -- zero copy, zero re-parsing.
 *
 * Two families ("data", "sfx") have a PERSISTENT DATAFILE* global
 * the original init_game() fills once and keeps for the rest of the
 * process (notes/asset_census.md SS3): `data` @0x4dd23c, `sfx`
 * @0x4dd240, already bound to those addresses under their plain names
 * by pf_bindings_src.h (force-included ahead of this file) -- so this
 * file reads them exactly as game code would: `data[N].dat`.
 *
 * The other five families ("loading", and every "char:<name>") have
 * NO persistent global in the original: `load_datafile("data/loading.dat")`
 * fills a local used once for the splash and never kept; a character's
 * datafile only ever passes through the single, ephemeral `custom.df`
 * slot while that one character happens to be selected in the menu, and
 * is overwritten the moment a different character is chosen (KNOWN,
 * game_types.h's Tcustom -- one struct, not an array). There is
 * therefore no single "the object the game already loaded" for those
 * five families to bind to zero-copy the way "data"/"sfx" can.
 *
 * DESIGN DECISION (disclosed in src/icytower/ASSETS.md): this file
 * gives each of those five families its own PRIVATE, lazily-loaded,
 * cached-forever DATAFILE*, loaded through the real embedded Allegro
 * loader (load_datafile/packfile_password, bound to their original
 * addresses by pf_lib_bindings.h -- force-included ahead of this file
 * too) at the same relative path and with the same password the
 * original uses (src/icytower/assets_table.inc's asset_datafile_family[]).
 * This reads the exact same bytes through the exact same decoder the
 * original ships, so it is still "zero re-implementation", just not
 * "the identical in-memory object at every moment" -- the honest
 * fact for these five families is that the original itself doesn't
 * keep one either.
 *
 * OGG-type objects (all sample-kind objects in this game -- no shipped
 * datafile uses the SAMP type, notes/asset_census.md SS2a) need a real
 * decode, not a cast: asset_sample() calls the embedded libvorbis addon's
 * own `logg_load_memory` (pf_lib_bindings.h, VA=0x420688 -- the exact
 * function every game family already funnels OGG objects through) on
 * the object's raw payload. SAMP-type objects, if any datafile ever
 * ships one, are returned as a zero-copy cast like every other kind.

 * Force-included (/FI), after pf_bindings_src.h and pf_lib_bindings.h,
 * only when address-free src/ is compiled INTO the carrier
 * (win32_pilot.md SS7a's trick, reused here for the ASSET coastline).
 * Re-run gen_assets.py to regenerate; do not hand-edit.
 */

#ifndef PF_ASSET_BINDINGS_H
#define PF_ASSET_BINDINGS_H

#include <string.h>
#include "assets.h"
#include "assets_table.inc"

/* one cache slot per asset_datafile_family[] row, same index -- only
 * the "loading" and "char:*" rows ever populate theirs, since
 * "data"/"sfx" are special-cased below to the persistent globals and
 * never reach this array at all. */
static DATAFILE *pf_asset_cache[ASSET_DATAFILE_FAMILY_COUNT];

static DATAFILE *pf_asset_family(const char *family)
{
    unsigned i;

    if (!strcmp(family, "data"))
        return data;   /* persistent global, plain name via pf_bindings_src.h */
    if (!strcmp(family, "sfx"))
        return sfx;    /* persistent global, plain name via pf_bindings_src.h */

    for (i = 0; i < ASSET_DATAFILE_FAMILY_COUNT; i++) {
        if (strcmp(asset_datafile_family[i].name, family) != 0)
            continue;
        if (!pf_asset_cache[i]) {
            packfile_password(asset_datafile_family[i].password);   /* (const char *)0 -- unencrypted */
            pf_asset_cache[i] = load_datafile(asset_datafile_family[i].path);
            packfile_password((const char *)0);
        }
        return pf_asset_cache[i];
    }
    return (DATAFILE *)0;
}

static const struct asset_table_row *pf_asset_row(asset_id id)
{
    return &asset_table[id];
}

static void *pf_asset_raw(asset_id id)
{
    const struct asset_table_row *row = pf_asset_row(id);
    DATAFILE *base = pf_asset_family(row->datafile);
    return base[row->index].dat;
}

/* static, like every other name pf_bindings_src.h/pf_lib_bindings.h
 * introduce: this header may be force-included ahead of more than one
 * native .c file's compile, and each gets its own internal-linkage
 * copy instead of colliding at link time (the same reason
 * it_globals.h emits a `static X *g_name_p` per name). assets.h never
 * declares these with external linkage in this world (guarded by
 * ICYTOWER_BINDINGS_ACTIVE), so there is no extern/static clash. */
static BITMAP *asset_bitmap(asset_id id) { return (BITMAP *)pf_asset_raw(id); }
static FONT *asset_font(asset_id id) { return (FONT *)pf_asset_raw(id); }
static PALETTE *asset_palette(asset_id id) { return (PALETTE *)pf_asset_raw(id); }
static void *asset_object(asset_id id) { return pf_asset_raw(id); }

static SAMPLE *asset_sample(asset_id id)
{
    const struct asset_table_row *row = pf_asset_row(id);
    DATAFILE *base = pf_asset_family(row->datafile);
    DATAFILE *obj = &base[row->index];
    if (!strncmp(row->type_fourcc, "OGG", 3))
        return logg_load_memory(obj->dat, (it_orig_size_t)obj->size);
    return (SAMPLE *)obj->dat;   /* SAMP-type object: already a real SAMPLE*, zero copy */
}

#endif /* PF_ASSET_BINDINGS_H */
