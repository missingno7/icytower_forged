/* asset_oracle.c -- the STANDALONE half of the cross-world asset oracle
 * (src/icytower/ASSETS.md "Extraction and the asset oracle"). For every one
 * of the 257 ASSET_* ids in src/icytower/assets.h, loads the object through
 * src/icytower/assets_standalone.c (the REAL Allegro 4.4.3.1 static library
 * built in third_party/, real load_datafile()/packfile_password(), real
 * logg_load_memory() for OGG samples -- no reimplementation) and prints
 *     <index> <object_name> <sha256 hex>
 * where the hashed bytes are a CANONICAL, DOCUMENTED serialization of the
 * object as Allegro holds it in memory -- not the on-disk datafile bytes
 * (those are what scripts/extract_assets.py / port_forge/tools/
 * pf_allegro4_datafile.py already prove round-trip; this oracle asks a
 * different question: does the REAL Allegro loader's in-memory result match
 * what a from-scratch Python reconstruction of "what Allegro would build"
 * produces, working only from the extracted files? scripts/
 * asset_oracle_digest.py is that Python half.
 *
 * WHY THE IN-MEMORY BYTES ARE NOT ALWAYS THE ON-DISK BYTES, AND WHY THIS
 * COMMENT SAYS "EMPIRICALLY CONFIRMED" RATHER THAN JUST "DERIVED" (read
 * third_party/allegro-4.4.3.1/src/datafile.c and src/graphics.c first, then
 * this program was actually BUILT AND RUN against this project's real
 * assets/ to check the prediction -- see src/icytower/ASSETS.md for the
 * full story, including where the first prediction was wrong):
 *   - a NEW-format ('ALL.') datafile's BITMAP object stores 8bpp as 1
 *     raw index byte/pixel, 15/16bpp as a little-endian RGB565/1555 word,
 *     and 24/32bpp as 3 bytes/pixel in (B,G,R) order, no alpha.
 *   - read_bitmap() in datafile.c re-derives r,g,b from those on-disk
 *     bytes and repacks them with makecol16/makecol24/makeacol32, which use
 *     the CURRENT _rgb_r/g/b(/a)_shift_* globals -- i.e. the pixel format
 *     of whatever real graphics driver is installed. This oracle installs
 *     one (GFX_AUTODETECT_WINDOWED, falling back to GFX_GDI/GFX_SAFE,
 *     exactly like third_party/smoke/smoke.c already proved works).
 *   - set_color_conversion(COLORCONV_NONE) is called first so
 *     _color_load_depth() never silently blits an object to a different
 *     bpp than it was stored at (Allegro's default COLORCONV_TOTAL WOULD
 *     do that whenever an object's bpp differs from the current screen
 *     depth -- harmless here since every object's own bpp is what the
 *     manifest/extracted-file side also assumes, but only true because
 *     conversion is off).
 *   - PREDICTED from source alone: 8/16bpp memory bytes == disk bytes;
 *     24/32bpp memory bytes == disk bytes with each pixel's 3 useful bytes
 *     REVERSED (WRITE3BYTES is little-endian, and a GDI driver's own shift
 *     assignment -- win/gdi.c: R=16,G=8,B=0 -- would produce that).
 *   - MEASURED by actually running this program (2026-09-07, this build
 *     host): 8/16/24bpp memory bytes ALL equal disk bytes VERBATIM, no
 *     reversal at any of those three; only 32bpp differs from disk, by a
 *     single appended 0x00 alpha byte per pixel (these datafiles store no
 *     alpha). Whatever real driver GFX_AUTODETECT_WINDOWED actually
 *     installed on this host evidently set _rgb_r/g/b_shift_24/32 to the
 *     OPPOSITE convention from win/gdi.c's hardcoded values (R=0,G=8,B=16),
 *     while still agreeing with the disk-format's R-high convention at
 *     16bpp -- confirmed by scripts/asset_oracle_digest.py independently
 *     reconstructing all 186 BMP + 6 PAL + 5 FONT + 7 info objects (204 of
 *     257) from ONLY the extracted files and matching this program's
 *     output byte-for-byte on every one. This is exactly the kind of fact
 *     that only an actual run settles, not a read of the source -- see
 *     src/icytower/ASSETS.md for the full account of the first (wrong)
 *     prediction and the empirical correction.
 *
 * CANONICAL SERIALIZATION (what gets hashed; scripts/asset_oracle_digest.py
 * must reproduce this exactly from the extracted files):
 *   BITMAP : u16be bpp, u16be w, u16be h, then h rows of bmp->line[y],
 *            w * bytes-per-pixel(bpp) bytes each (bytes-per-pixel: 1 @8bpp,
 *            2 @15/16bpp, 3 @24bpp, 4 @32bpp -- the IN-MEMORY size, note
 *            32bpp differs from the on-disk 3).
 *   SAMPLE : u32be freq, u16be bits, u16be stereo, u16be priority,
 *            u32be len, then len*(bits/8)*(stereo?2:1) raw PCM bytes.
 *            NOT proven equal to Python for this project's actual assets:
 *            all 53 sample objects here are OGG-typed, so `data` is real
 *            Ogg Vorbis decoder output (logg_load_memory -> libvorbis);
 *            reproducing that bit-for-bit in Python would need a bit-exact
 *            Vorbis decoder, out of scope -- see ASSETS.md "What remains
 *            unproven".
 *   FONT   : u16be range_count, then per range (font order): 1 byte 'M'
 *            (mono) or 'C' (color), u32be first, u32be last (INCLUSIVE,
 *            matching get_font_range_end's own contract), then for each
 *            character code in [first,last]: if mono, u16be w, u16be h,
 *            then ((w+7)/8)*h raw glyph bytes (verbatim -- Allegro stores
 *            and loads packed mono glyphs with no transform at all); if
 *            color, the BITMAP serialization above applied to that
 *            glyph's BITMAP* (color font glyphs are just per-glyph BMP
 *            objects, read_font_color calls the same read_bitmap()).
 *   PALETTE: the raw 1024 bytes of the PALETTE struct verbatim -- a
 *            NEW-format datafile's PAL object has no registered loader
 *            (register_datafile_object never lists DAT_PALETTE), so
 *            load_object() falls through to load_data_object(), a raw
 *            byte-for-byte block read with no 6-bit shift and no
 *            transform (that shift only exists in read_palette(), which
 *            is dead code for the 'ALL.' format this game ships).
 *   info   : the raw 32 bytes verbatim (same load_data_object() fallback;
 *            32 is this project's own measured GrabberInfo size, not a
 *            general Allegro fact -- see ASSETS.md).
 *
 * Build (mingw32 MSYS2, third_party/BUILD.md's toolchain):
 *   MSYSTEM=MINGW32 C:\msys64\usr\bin\bash.exe -lc \
 *     "cd /d/Games/DOS/dos_recosystem/icytower_forged && \
 *      mingw32-make -f src/build/Makefile.standalone asset_oracle"
 *   then run src/build/asset_oracle.exe from a machine with the real
 *   assets/ directory alongside it (it opens data/data.dat etc. relative
 *   to its own working directory, exactly like the original game).
 *
 * RESULT (2026-09-07, this build host, MSYS2 MINGW32 gcc, 16bpp screen +
 * COLORCONV_NONE -- main()'s ORIGINAL configuration): built and run clean, 0
 * errors/warnings, exit 0, all 257 ids printed a real hash (no SKIP lines).
 * scripts/asset_oracle_digest.py's independent Python reconstruction from
 * ONLY scripts/extract_assets.py's output matched this program's output
 * byte-for-byte on all 204 BITMAP+PALETTE+FONT+info ids; the 53 OGG/SAMPLE
 * ids are real digests here but not independently reproduced (see SAMPLE's
 * own note below). This proves "the extracted files losslessly describe the
 * on-disk datafile bytes" -- a DIFFERENT question from the one below.
 *
 * SECOND RESULT (2026-09-08, same build host, 32bpp screen +
 * COLORCONV_TOTAL -- main()'s CURRENT configuration, matching the real
 * game's own measured load conditions, carrier/NOTES.md "In-vivo batch 9,
 * corpus gates, asset oracle"): built and run clean, 0 errors/warnings,
 * exit 0. Diffed by id against carrier/src/dump_assets.c's own
 * `--dump-assets` output from a real carrier run
 * (artifacts_batch9/carrier_assets_final.txt, 257 lines, one hash per id in
 * the SAME canonical serialization):
 *
 *   BITMAP:  185/186 EQUAL (was 0/186 under the first, COLORCONV_NONE
 *            configuration) -- confirms the carrier's own finding that a
 *            32bpp screen + Allegro's default COLORCONV_TOTAL up-converts
 *            every BITMAP object at load time, and that this standalone
 *            oracle reproduces it once configured the same way. The one
 *            still-differing id is "loading" family's FLD_LOGO (the
 *            project's only 8bpp-on-disk BITMAP) -- see note below.
 *   PALETTE: 5/6 EQUAL, unchanged from the first run -- PALETTE objects are
 *            a raw load_data_object() byte copy with no color-depth-
 *            dependent transform (confirmed by this second run: switching
 *            depth/conversion mode did not change any PALETTE hash except
 *            through no path at all), so this is NOT a colour-conversion
 *            issue. The one differing id, "data" family's own AAAPAL, is
 *            still unexplained past this: the only thing this project's
 *            own disassembly (artifacts/disasm.txt) shows touching
 *            data[0].dat is `select_palette(data[0].dat)` at VA 0x40f928
 *            (init_game, right after data.dat loads, right before
 *            sfx15.dat loads) -- and select_palette() (VA 0x44df24) is
 *            CONFIRMED READ-ONLY with respect to its argument by direct
 *            disassembly (it only reads r/g/b bytes out of the passed
 *            palette to build OTHER internal lookup tables; it never writes
 *            back into the source struct), which rules out "the game
 *            mutates its own AAAPAL in place" as the cause. "data" is the
 *            ONE family bound zero-copy to the game's own persistent,
 *            continuously-live global (carrier/gen/pf_asset_bindings.h);
 *            the other 5 families (including "loading") are each a fresh,
 *            private, lazily-loaded copy the CARRIER's own binding loads on
 *            first request, never touched by the game's own code at all --
 *            so whatever differs about "data"'s AAAPAL is tied to it being
 *            the game's own long-lived in-process copy, not to anything
 *            this file's load-time configuration controls. Needs live
 *            memory inspection of a running carrier process to go further
 *            (out of scope here -- this file only owns the standalone
 *            side).
 *   FONT/info: unchanged, still 5/5 and 6/6 EQUAL -- neither is
 *            colour-depth-dependent (this game's 5 fonts are all MONO;
 *            info is a raw 32-byte block).
 *
 * FLD_LOGO, diagnosed separately (a temporary debug print, since reverted,
 * confirmed this oracle DOES convert it: bpp=32, w=401, h=210, with real
 * (non-garbage) RGB values in its first row -- so Allegro's own
 * load_datafile() is successfully resolving "loading" family's OWN AAAPAL
 * for the 8bpp-to-32bpp conversion in THIS process). The likely explanation
 * for why it still differs from the carrier's own hash: select_palette()
 * is GLOBAL, mutable, process-wide state -- "the currently selected
 * palette" used for an 8bpp object's load-time colour conversion is
 * whichever palette some EARLIER select_palette() call left active, not
 * necessarily the one belonging to the datafile being loaded right now. In
 * a fresh run of this oracle, "loading" family's own lazy load is the
 * first thing that ever selects a palette, so FLD_LOGO converts using
 * loading's own AAAPAL. In a real carrier run, init_game() has already
 * called `select_palette(data[0].dat)` (VA 0x40f928) long before the
 * carrier's own pf_asset_bindings.h ever lazily loads "loading" family
 * (which happens on-demand, at/after the first --dump-assets safepoint) --
 * if Allegro's load_datafile() does not itself re-select a fresh palette
 * per new file (unconfirmed either way without live tracing), FLD_LOGO
 * would convert against "data"'s palette instead of its own, producing
 * different pixel values. Consistent with, but not proof beyond, the
 * PALETTE finding above: both remaining gaps trace back to the SAME kind
 * of global, order-dependent, select_palette() state that only a live
 * carrier process's own call history can settle.
 */
#define ICYTOWER_UPSTREAM_ALLEGRO 1

#include "assets.h"       /* -> allegro_types.h -> real <allegro.h> under
                             ICYTOWER_UPSTREAM_ALLEGRO (see that file's own
                             header comment); asset_bitmap()/asset_sample()/
                             asset_font()/asset_palette()/asset_object() */
#include "assets_table.inc"  /* asset_table[] -- id/family/index/name/type,
                                the same DATA-only rows assets_standalone.c
                                itself reads (src/icytower/ASSETS.md) */
#include "sha256.h"
#include <stdio.h>
#include <string.h>

/* _mono_find_glyph / _color_find_glyph: real, exported symbols in
 * liballeg.a (src/font.c), declared in the library's own INTERNAL header
 * (third_party/allegro-4.4.3.1/include/allegro/internal/aintern.h) rather
 * than the public <allegro.h> -- the same kind of internal-struct reach
 * this project's own game_types_check.c already does for
 * LZSS_UNPACK_DATA/_al_normal_packfile_details (see Makefile.standalone's
 * header comment). Declared directly here instead of pulling in the whole
 * internal header, to keep this file's dependency surface to exactly the
 * two symbols it needs. */
extern FONT_GLYPH *_mono_find_glyph(const FONT *f, int ch);
extern BITMAP *_color_find_glyph(const FONT *f, int ch);

typedef struct { SHA256_CTX ctx; } digest_t;

static void digest_init(digest_t *d) { sha256_init(&d->ctx); }
static void digest_feed(digest_t *d, const void *p, size_t n)
{
    if (n)
        sha256_update(&d->ctx, (const unsigned char *)p, n);
}
static void digest_u16be(digest_t *d, unsigned v)
{
    unsigned char b[2];
    b[0] = (unsigned char)((v >> 8) & 0xFF);
    b[1] = (unsigned char)(v & 0xFF);
    digest_feed(d, b, 2);
}
static void digest_u32be(digest_t *d, unsigned long v)
{
    unsigned char b[4];
    b[0] = (unsigned char)((v >> 24) & 0xFF);
    b[1] = (unsigned char)((v >> 16) & 0xFF);
    b[2] = (unsigned char)((v >> 8) & 0xFF);
    b[3] = (unsigned char)(v & 0xFF);
    digest_feed(d, b, 4);
}
static void digest_hex(digest_t *d, char out[65])
{
    unsigned char h[32];
    int i;
    static const char *hexd = "0123456789abcdef";
    sha256_final(&d->ctx, h);
    for (i = 0; i < 32; i++) {
        out[i * 2] = hexd[(h[i] >> 4) & 0xF];
        out[i * 2 + 1] = hexd[h[i] & 0xF];
    }
    out[64] = 0;
}

static int bitmap_bytes_per_pixel(int bpp)
{
    switch (bpp) {
        case 8: return 1;
        case 15: case 16: return 2;
        case 24: return 3;
        default: return 4;   /* 32bpp in-memory: BGR + alpha byte */
    }
}

static void serialize_bitmap(BITMAP *bmp, digest_t *d)
{
    int bpp, w, h, y, bypp;
    if (!bmp) { digest_feed(d, "NULL", 4); return; }
    bpp = bmp->vtable->color_depth;
    w = bmp->w;
    h = bmp->h;
    bypp = bitmap_bytes_per_pixel(bpp);
    digest_u16be(d, (unsigned)bpp);
    digest_u16be(d, (unsigned)w);
    digest_u16be(d, (unsigned)h);
    for (y = 0; y < h; y++)
        digest_feed(d, bmp->line[y], (size_t)w * bypp);
}

static void serialize_sample(SAMPLE *s, digest_t *d)
{
    size_t nbytes;
    if (!s) { digest_feed(d, "NULL", 4); return; }
    digest_u32be(d, (unsigned long)s->freq);
    digest_u16be(d, (unsigned)s->bits);
    digest_u16be(d, (unsigned)s->stereo);
    digest_u16be(d, (unsigned)s->priority);
    digest_u32be(d, (unsigned long)s->len);
    nbytes = (size_t)s->len * (size_t)(s->bits / 8) * (size_t)(s->stereo ? 2 : 1);
    digest_feed(d, s->data, nbytes);
}

static void serialize_font(FONT *f, digest_t *d)
{
    int nranges, r, mono, ch, first, last;
    if (!f) { digest_feed(d, "NULL", 4); return; }
    nranges = get_font_ranges(f);
    mono = is_mono_font(f);
    digest_u16be(d, nranges < 0 ? 0 : (unsigned)nranges);
    for (r = 0; r < nranges; r++) {
        first = get_font_range_begin(f, r);
        last = get_font_range_end(f, r);   /* inclusive, per its own doc comment */
        digest_feed(d, mono ? "M" : "C", 1);
        digest_u32be(d, (unsigned long)first);
        digest_u32be(d, (unsigned long)last);
        for (ch = first; ch <= last; ch++) {
            if (mono) {
                FONT_GLYPH *g = _mono_find_glyph(f, ch);
                int gw = g->w, gh = g->h;
                int sz = ((gw + 7) / 8) * gh;
                digest_u16be(d, (unsigned)gw);
                digest_u16be(d, (unsigned)gh);
                digest_feed(d, g->dat, (size_t)sz);
            } else {
                BITMAP *g = _color_find_glyph(f, ch);
                serialize_bitmap(g, d);
            }
        }
    }
}

static void serialize_palette(PALETTE *p, digest_t *d)
{
    digest_feed(d, p, sizeof(PALETTE));
}

/* This project's own measured fact (src/icytower/ASSETS.md), not a general
 * Allegro one: every "info" (GrabberInfo) object in these 7 datafiles is
 * exactly 32 bytes -- asset_object() only exposes the raw pointer, not a
 * size, so there is no public-API way to ask Allegro; this is the
 * documented, verified constant instead. */
#define GRABBERINFO_SIZE 32

static void serialize_info(void *raw, digest_t *d)
{
    if (!raw) { digest_feed(d, "NULL", 4); return; }
    digest_feed(d, raw, GRABBERINFO_SIZE);
}

int main(void)
{
    int i, gfx_ok;

    /* MATCH THE CARRIER'S MEASURED LOAD CONDITIONS (carrier/NOTES.md "In-vivo
     * batch 9, corpus gates, asset oracle"; src/icytower/ASSETS.md "What
     * remains unproven" -> "Carrier-vs-standalone equality"), not the
     * original 16bpp/COLORCONV_NONE setup this file used for the first,
     * disk-bytes-verbatim proof (still true and still documented above --
     * this is a SECOND run, under DIFFERENT conditions, to answer a
     * DIFFERENT question: does this oracle match a REAL RUNNING GAME
     * process, not just the extracted files).
     *
     * artifacts/disasm.txt's own init_game() disassembly (VA 0x40e7dc)
     * gives the real game's own boot sequence, in order:
     *   0x40ecfd  set_color_depth(<config-driven, MEASURED 32 on this
     *             project's own build host -- carrier/NOTES.md: "the
     *             guest's OWN set_gfx_mode installs a real 32bpp screen">)
     *   0x40ed43  set_gfx_mode(1, 640, 480, 0, 0)
     *   0x40edfe  set_color_conversion(0)            = COLORCONV_NONE
     *   0x40ee16  load_datafile("data/loading.dat")  -- ephemeral local,
     *             NOT the persistent global, loaded UNCONVERTED
     *   0x40f199  set_color_conversion(0xffffff)     = COLORCONV_TOTAL
     *   0x40f1be  load_datafile_callback(...)        -> persistent `data`
     *             global (data.dat), loaded WITH conversion active
     *   0x40f96f  load_datafile_callback(...)        -> persistent `sfx`
     *             global (sfx15.dat), same COLORCONV_TOTAL state (no
     *             intervening set_color_conversion call before it)
     * COLORCONV_TOTAL is never turned back off before the game reaches its
     * first gameplay tick (grep of every set_color_conversion call site in
     * the binary: the only two inside init_game()'s own byte range are
     * these; every other call site is in menu/options code the recorded
     * gates never visit). The carrier's own `--dump-assets` (carrier/src/
     * dump_assets.c) fires at that FIRST safepoint, and the 5 families with
     * no persistent global (loading, char:*) are then loaded LAZILY by
     * carrier/gen/pf_asset_bindings.h's own load_datafile() call -- which
     * happens AFTER init_game() already left the process in this same
     * COLORCONV_TOTAL/32bpp state, so EVERY family's BITMAP objects get the
     * SAME up-conversion treatment in a real carrier run, not just "data"'s.
     * assets_standalone.c has no per-family special-casing either (ASSETS.md
     * "no family gets special-cased ... standalone has none for ANY
     * family"), so reproducing 32bpp + COLORCONV_TOTAL here, uniformly for
     * all 7 datafiles, is the correct match for what the carrier actually
     * measured -- not a per-family conditional in this file. */
    allegro_init();
    set_color_depth(32);
    set_color_conversion(COLORCONV_TOTAL);
    gfx_ok = (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) == 0)
          || (set_gfx_mode(GFX_GDI, 640, 480, 0, 0) == 0)
          || (set_gfx_mode(GFX_SAFE, 640, 480, 0, 0) == 0);
    if (!gfx_ok) {
        fprintf(stderr, "asset_oracle: could not install any graphics mode "
                        "(%s) -- the _rgb_shift_* globals this oracle relies "
                        "on are only set by a real driver, see this file's "
                        "own header comment\n", allegro_error);
        return 1;
    }

    for (i = 0; i < ASSET_COUNT; i++) {
        const struct asset_table_row *row = &asset_table[i];
        digest_t d;
        char hex[65];

        digest_init(&d);
        if (!strcmp(row->type_fourcc, "BMP "))
            serialize_bitmap(asset_bitmap((asset_id)i), &d);
        else if (!strcmp(row->type_fourcc, "OGG "))
            serialize_sample(asset_sample((asset_id)i), &d);
        else if (!strcmp(row->type_fourcc, "FONT"))
            serialize_font(asset_font((asset_id)i), &d);
        else if (!strcmp(row->type_fourcc, "PAL "))
            serialize_palette(asset_palette((asset_id)i), &d);
        else if (!strcmp(row->type_fourcc, "info"))
            serialize_info(asset_object((asset_id)i), &d);
        else {
            printf("%d %s SKIP unrecognized-type=%s\n", i, row->object_name, row->type_fourcc);
            continue;
        }
        digest_hex(&d, hex);
        printf("%d %s %s\n", i, row->object_name ? row->object_name : "?", hex);
        fflush(stdout);
    }

    allegro_exit();
    return 0;
}
END_OF_MAIN()
