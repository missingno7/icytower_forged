/* assets_selftest.c -- compile fixture proving carrier/gen/pf_asset_bindings.h's
 * contract: every one of the 257 generated `asset_id` values resolves
 * through the 5-function API (src/icytower/assets.h) without an address
 * literal anywhere in THIS file, and without linker collisions if more
 * than one native .c file eventually force-includes pf_asset_bindings.h
 * in the same carrier build (win32_pilot.md SS7c, src/icytower/ASSETS.md).
 *
 * Only needs to compile (task brief): this is a carrier-side fixture, not
 * src/, so it is not itself gated by scripts/check_native_layer.py -- but
 * it is written the same way src/ code would be anyway, exactly like
 * carrier/gen/bindings_selftest.c already does for the CODE coastline.
 *
 * Build (carrier world only -- this file has no standalone-world twin,
 * since its whole point is proving the carrier bindings link):
 *   cl /nologo /c /W3 /TC /Icarrier\gen /Isrc\icytower ^
 *      /FIpf_bindings_src.h /FIpf_lib_bindings.h /FIpf_asset_bindings.h ^
 *      carrier\gen\assets_selftest.c
 */
#include "assets.h"

int assets_selftest(void)
{
    int i;
    int nonnull = 0;

    /* every id, generic accessor -- proves the table has exactly
     * ASSET_COUNT rows and every row resolves to something */
    for (i = 0; i < ASSET_COUNT; i++) {
        if (asset_object((asset_id)i) != (void *)0)
            nonnull++;
    }

    /* one call through each typed accessor, one id per datafile family,
     * covering every asset kind this game ships (BMP/PAL/FONT/OGG/info) */
    (void)asset_bitmap(ASSET_DATA_TITLE);              /* data.dat, BMP  */
    (void)asset_palette(ASSET_DATA_AAAPAL);             /* data.dat, PAL  */
    (void)asset_font(ASSET_DATA_FONT_MONO);             /* data.dat, FONT */
    (void)asset_sample(ASSET_SFX_AIGHT);                /* sfx15.dat, OGG (decoded) */
    (void)asset_bitmap(ASSET_LOADING_FLD_LOGO);         /* loading.dat, BMP (lazy family) */
    (void)asset_object(ASSET_LOADING_GRABBERINFO);      /* loading.dat, info */
    (void)asset_palette(ASSET_CHAR_HAROLD_PALETTE);     /* char:harold_the_homeboy, PAL */
    (void)asset_bitmap(ASSET_CHAR_HAROLD_FRAME_01);     /* char:harold_the_homeboy, BMP */
    (void)asset_sample(ASSET_CHAR_HAROLD_SND_JUMP_LO);  /* char:harold_the_homeboy, OGG (decoded) */
    (void)asset_bitmap(ASSET_CHAR_DISCO_FRAME_15);      /* char:disco_dave, BMP */
    (void)asset_bitmap(ASSET_CHAR_JUNGLE_FRAME_15);     /* char:jungle_jane, BMP */
    (void)asset_bitmap(ASSET_CHAR_WILD_FRAME_15);       /* char:wild_wendy, BMP */

    return nonnull;
}
