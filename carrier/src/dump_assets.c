/* dump_assets.c -- the CARRIER side of the cross-world asset oracle
 * (src/icytower/ASSETS.md "--dump-assets proposal"; src/build/asset_oracle.c's
 * own header comment for the canonical serialization this file must
 * reproduce byte-for-byte). Exposes ONE entry point, pf_dump_assets(PATH),
 * called by det.cpp at the first safepoint (a tick guaranteed to be AFTER
 * the guest's own init_game() has loaded every datafile -- that happens
 * once, early, well before play() is ever reached). For every id in
 * src/icytower/assets_table.inc, resolves the object through the GUEST'S
 * OWN loaded DATAFILE (carrier/gen/pf_asset_bindings.h's
 * asset_bitmap()/asset_sample()/asset_font()/asset_palette()/asset_object(),
 * zero-copy for "data"/"sfx", the real embedded Allegro loader for the rest
 * -- see that header's own comment) and prints
 *     <index> <object_name> <sha256hex>
 * one line per id, in asset_table[]'s own order -- directly diffable
 * against src/build/asset_oracle.c's stdout or scripts/
 * asset_oracle_digest.py's output.
 *
 * Compiled as its OWN cl invocation (carrier/build.cmd), force-including
 * pf_bindings_src.h + pf_lib_bindings.h + pf_asset_bindings.h -- the same
 * recipe draw_buffer.c/start_reward.c already use for the "Allegro/asset-
 * seam" group (those headers redefine BITMAP and collide with windows.h,
 * so no C++ TU in carrier/src may see them -- win32_pilot.md SS7a). Exposes
 * a single plain-C-signature function so main.cpp/det.cpp can declare it
 * `extern "C"` without including any header this file needs.
 *
 * FONT canonical serialization needs two pieces of Allegro-internal
 * knowledge src/build/asset_oracle.c reaches via real, exported functions
 * (_mono_find_glyph/_color_find_glyph/is_mono_font, declared in
 * third_party/allegro-4.4.3.1/include/allegro/internal/aintern.h) that
 * pf_lib_bindings.h does NOT bind -- its 100-function allow-list is a call-
 * edge census of what src/icytower's OWN recovered source actually calls,
 * and no game file calls any of these three itself, so there is no call
 * site to validate a new VA binding against (the project's own standing
 * policy for adding bindings, carrier/gen/LIB_BINDINGS_NOTES.md). Worked
 * around locally instead of adding new, unvalidated bindings:
 *
 *   - "is this FONT mono or color": upstream font.c's is_mono_font(f) is
 *     itself nothing but a check of f->vtable->render_char against the
 *     mono/color vtables' own render_char slot. Both underlying functions
 *     are real, DWARF-recovered symbols with a known VA in THIS game's own
 *     binary (artifacts/functions.json, allegro4/src/font.c compile unit):
 *     mono_render_char = 0x0045dfb0, color_render_char = 0x0045edb4.
 *     Comparing f->vtable->render_char against these two constants is
 *     exactly the same test is_mono_font would run, with no new binding.
 *   - "the Nth character's glyph": FONT_VTABLE's get_font_ranges/
 *     get_font_range_begin/get_font_range_end are ALREADY ordinary vtable
 *     slots (carrier/gen/it_types.h's FONT_VTABLE, ported over from the
 *     game's own struct recovery) -- called directly here, exactly like
 *     BITMAP's vtable->line() dispatch elsewhere in this project. Only the
 *     per-character glyph pointer needs a local walk of FONT->data's own
 *     singly-linked list, using the FONT_MONO_DATA/FONT_COLOR_DATA layout
 *     upstream Allegro's aintern.h documents (int begin, end; void
 *     **items; void *next) -- a fixed ABI fact of the Allegro version this
 *     game statically links (4.4.1) and this project's own standalone
 *     oracle build (4.4.3.1) alike; both were already cross-checked byte-
 *     for-byte on 204/257 objects without this file's help
 *     (src/icytower/ASSETS.md).
 */
#include "assets.h"
#include "assets_table.inc"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* pf_bindings_src.h (force-included ahead of this file) macro-redirects the
 * bare token `data` to the persistent `DATAFILE *data` global's address
 * (`#define data (*(DATAFILE **)0x4dd23c)`, needed by pf_asset_bindings.h's
 * OWN already-expanded function bodies above this point in the same
 * translation unit). Below this line, this file needs to write `s->data`
 * (SAMPLE's own payload field) and `f->data` (FONT's own range-list head) -
 * both real struct members spelled `data`, both textually rewritten into a
 * syntax error by that macro exactly like the project's own documented
 * "stars"/"jump_sound" member-access collisions (carrier/gen/
 * gen_bindings.py's MEMBER_ACCESS_COLLISIONS comment; carrier/build.cmd's
 * "member-access-safe group"). Unlike `stars`, nothing in THIS file ever
 * needs the top-level `data` global directly (asset_bitmap()/asset_font()/
 * asset_sample()/asset_palette()/asset_object(), all resolved above this
 * line, already did that lookup) - so a plain #undef here, local to the
 * rest of this one file, is the narrowest possible fix (no second
 * generated header needed, unlike draw_star_field.c's case). */
#undef data

/* A compact, self-contained SHA-256 (same public-domain algorithm src/build/
 * sha256.h uses for src/build/asset_oracle.c - Brad Conte's implementation,
 * https://github.com/B-Con/crypto-algorithms). NOT a #include of that file:
 * pf_bindings_src.h (force-included ahead of this file, needed for
 * asset_bitmap()/BITMAP/FONT/.../the "data"/"sfx" globals) macro-redirects
 * every GAME global's bare name to an address-cast expression, and
 * sha256.h's own struct field is spelled `data` (`unsigned char data[64]`) -
 * MEASURED: textually identical to the game global `data` (the persistent
 * DATAFILE* pf_asset_bindings.h itself reads), so `#define data (*(DATAFILE
 * **)0x4dd23c)` rewrites that struct member declaration into a syntax error
 * the moment the two are compiled together (the exact blunt-textual-#define
 * failure mode carrier/gen/gen_bindings.py's own MEMBER_ACCESS_COLLISIONS
 * comment and PROMOTIONS.md's "stars"/"jump_sound" collisions already
 * document - this is a foreign header's identifier landing in the same
 * trap, not a new bug class). Reimplemented locally instead, with every
 * identifier prefixed `dsha_` and cross-checked against pf_bindings_src.h's
 * and pf_lib_bindings.h's own full macro name lists (carrier/NOTES.md
 * "in-vivo pass, corpus gates, asset oracle") to rule out a second
 * collision. Same round constants, same algorithm, same output. */
typedef struct {
    unsigned char dsha_buf[64];
    uint32_t dsha_buflen;
    unsigned long long dsha_bitlen;
    uint32_t dsha_state[8];
} dsha_ctx_t;

static const uint32_t dsha_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define DSHA_ROTR(a,b) (((a) >> (b)) | ((a) << (32-(b))))

static void dsha_transform(dsha_ctx_t *c, const unsigned char blk[])
{
    uint32_t a,b,cc,dd,e,ff,g,h,i,j,t1,t2,m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((uint32_t)blk[j] << 24) | ((uint32_t)blk[j+1] << 16) |
               ((uint32_t)blk[j+2] << 8) | ((uint32_t)blk[j+3]);
    for (; i < 64; ++i) {
        uint32_t s0 = DSHA_ROTR(m[i-15],7) ^ DSHA_ROTR(m[i-15],18) ^ (m[i-15] >> 3);
        uint32_t s1 = DSHA_ROTR(m[i-2],17) ^ DSHA_ROTR(m[i-2],19) ^ (m[i-2] >> 10);
        m[i] = s1 + m[i-7] + s0 + m[i-16];
    }
    a=c->dsha_state[0]; b=c->dsha_state[1]; cc=c->dsha_state[2]; dd=c->dsha_state[3];
    e=c->dsha_state[4]; ff=c->dsha_state[5]; g=c->dsha_state[6]; h=c->dsha_state[7];
    for (i = 0; i < 64; ++i) {
        uint32_t S1 = DSHA_ROTR(e,6) ^ DSHA_ROTR(e,11) ^ DSHA_ROTR(e,25);
        uint32_t ch = (e & ff) ^ (~e & g);
        t1 = h + S1 + ch + dsha_k[i] + m[i];
        uint32_t S0 = DSHA_ROTR(a,2) ^ DSHA_ROTR(a,13) ^ DSHA_ROTR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        t2 = S0 + maj;
        h=g; g=ff; ff=e; e=dd+t1; dd=cc; cc=b; b=a; a=t1+t2;
    }
    c->dsha_state[0]+=a; c->dsha_state[1]+=b; c->dsha_state[2]+=cc; c->dsha_state[3]+=dd;
    c->dsha_state[4]+=e; c->dsha_state[5]+=ff; c->dsha_state[6]+=g; c->dsha_state[7]+=h;
}

static void dsha_init(dsha_ctx_t *c)
{
    c->dsha_buflen = 0;
    c->dsha_bitlen = 0;
    c->dsha_state[0]=0x6a09e667; c->dsha_state[1]=0xbb67ae85;
    c->dsha_state[2]=0x3c6ef372; c->dsha_state[3]=0xa54ff53a;
    c->dsha_state[4]=0x510e527f; c->dsha_state[5]=0x9b05688c;
    c->dsha_state[6]=0x1f83d9ab; c->dsha_state[7]=0x5be0cd19;
}

static void dsha_update(dsha_ctx_t *c, const unsigned char *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        c->dsha_buf[c->dsha_buflen++] = p[i];
        if (c->dsha_buflen == 64) {
            dsha_transform(c, c->dsha_buf);
            c->dsha_bitlen += 512;
            c->dsha_buflen = 0;
        }
    }
}

static void dsha_final(dsha_ctx_t *c, unsigned char out[32])
{
    uint32_t i = c->dsha_buflen;
    if (c->dsha_buflen < 56) {
        c->dsha_buf[i++] = 0x80;
        while (i < 56) c->dsha_buf[i++] = 0x00;
    } else {
        c->dsha_buf[i++] = 0x80;
        while (i < 64) c->dsha_buf[i++] = 0x00;
        dsha_transform(c, c->dsha_buf);
        memset(c->dsha_buf, 0, 56);
    }
    c->dsha_bitlen += (unsigned long long)c->dsha_buflen * 8;
    c->dsha_buf[63] = (unsigned char)(c->dsha_bitlen);
    c->dsha_buf[62] = (unsigned char)(c->dsha_bitlen >> 8);
    c->dsha_buf[61] = (unsigned char)(c->dsha_bitlen >> 16);
    c->dsha_buf[60] = (unsigned char)(c->dsha_bitlen >> 24);
    c->dsha_buf[59] = (unsigned char)(c->dsha_bitlen >> 32);
    c->dsha_buf[58] = (unsigned char)(c->dsha_bitlen >> 40);
    c->dsha_buf[57] = (unsigned char)(c->dsha_bitlen >> 48);
    c->dsha_buf[56] = (unsigned char)(c->dsha_bitlen >> 56);
    dsha_transform(c, c->dsha_buf);
    for (i = 0; i < 4; ++i) {
        out[i]      = (unsigned char)((c->dsha_state[0] >> (24 - i*8)) & 0xff);
        out[i+4]    = (unsigned char)((c->dsha_state[1] >> (24 - i*8)) & 0xff);
        out[i+8]    = (unsigned char)((c->dsha_state[2] >> (24 - i*8)) & 0xff);
        out[i+12]   = (unsigned char)((c->dsha_state[3] >> (24 - i*8)) & 0xff);
        out[i+16]   = (unsigned char)((c->dsha_state[4] >> (24 - i*8)) & 0xff);
        out[i+20]   = (unsigned char)((c->dsha_state[5] >> (24 - i*8)) & 0xff);
        out[i+24]   = (unsigned char)((c->dsha_state[6] >> (24 - i*8)) & 0xff);
        out[i+28]   = (unsigned char)((c->dsha_state[7] >> (24 - i*8)) & 0xff);
    }
}

typedef struct { dsha_ctx_t dsha; } digest_t;

static void digest_init(digest_t *d) { dsha_init(&d->dsha); }
static void digest_feed(digest_t *d, const void *p, size_t n)
{
    if (n) dsha_update(&d->dsha, (const unsigned char *)p, n);
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
    dsha_final(&d->dsha, h);
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

/* FONT_MONO_DATA / FONT_COLOR_DATA (upstream allegro/internal/aintern.h) --
 * see this file's own header comment for why this is defined locally
 * rather than pulled from carrier/gen/it_types.h (the game itself never
 * references either struct, so the DWARF-driven generator never emitted
 * them). Both real structs share this exact layout. */
typedef struct { int begin, end; void **items; void *next; } FontRangeNode;

#define MONO_RENDER_CHAR_VA  ((void *)(uintptr_t)0x0045dfb0u)
#define COLOR_RENDER_CHAR_VA ((void *)(uintptr_t)0x0045edb4u)

static int font_is_mono(FONT *f)
{
    return (void *)(uintptr_t)f->vtable->render_char == MONO_RENDER_CHAR_VA;
}

static void serialize_font(FONT *f, digest_t *d)
{
    int nranges, r;
    int mono;
    if (!f) { digest_feed(d, "NULL", 4); return; }
    nranges = f->vtable->get_font_ranges(f);
    mono = font_is_mono(f);
    digest_u16be(d, nranges < 0 ? 0 : (unsigned)nranges);
    for (r = 0; r < nranges; r++) {
        int first = f->vtable->get_font_range_begin(f, r);
        int last = f->vtable->get_font_range_end(f, r);   /* inclusive, per its own doc comment */
        FontRangeNode *node = (FontRangeNode *)f->data;
        int steps = r;
        int ch;
        while (steps-- > 0 && node) node = (FontRangeNode *)node->next;
        digest_feed(d, mono ? "M" : "C", 1);
        digest_u32be(d, (unsigned long)first);
        digest_u32be(d, (unsigned long)last);
        for (ch = first; ch <= last; ch++) {
            int idx = ch - (node ? node->begin : first);
            if (mono) {
                struct FONT_GLYPH *g = node ? ((struct FONT_GLYPH **)node->items)[idx] : (struct FONT_GLYPH *)0;
                int gw = g ? g->w : 0, gh = g ? g->h : 0;
                int sz = ((gw + 7) / 8) * gh;
                digest_u16be(d, (unsigned)gw);
                digest_u16be(d, (unsigned)gh);
                if (g) digest_feed(d, g->dat, (size_t)sz);
            } else {
                BITMAP *g = node ? ((BITMAP **)node->items)[idx] : (BITMAP *)0;
                serialize_bitmap(g, d);
            }
        }
    }
}

static void serialize_palette(PALETTE *p, digest_t *d)
{
    digest_feed(d, p, sizeof(PALETTE));
}

/* This project's own measured fact (src/icytower/ASSETS.md, src/build/
 * asset_oracle.c's own copy of this constant), not a general Allegro one:
 * every "info" (GrabberInfo) object in these 7 datafiles is exactly 32
 * bytes. */
#define GRABBERINFO_SIZE 32

static void serialize_info(void *raw, digest_t *d)
{
    if (!raw) { digest_feed(d, "NULL", 4); return; }
    digest_feed(d, raw, GRABBERINFO_SIZE);
}

void pf_dump_assets(const char *path)
{
    FILE *f;
    int i;

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "dump_assets: could not open '%s' for write\n", path);
        return;
    }
    for (i = 0; i < ASSET_COUNT; i++) {
        const struct asset_table_row *row = &asset_table[i];
        digest_t d;
        char hex[65];
        DATAFILE *base;

        /* MEASURED this pass (carrier/NOTES.md "in-vivo pass, corpus gates,
         * asset oracle"): in a --det/headless carrier run, the persistent
         * `sfx` global (VA 0x4dd240) stays NULL - --print-globals sfx
         * confirms `sfx = 0x00000000` at a tick well past where a real
         * interactive run's own assets/log.txt logs "sfx15.dat loaded",
         * while the sibling `data` global loads fine (`data = 0x2000...`,
         * every "data"-family id below dumps cleanly). The guest's own
         * sound install path evidently takes a different branch when no
         * real audio device is reachable (this environment's DirectSound
         * enumeration is itself NORMALIZED to one synthetic device -
         * Divergence 009) and never calls load_datafile_callback for
         * sfx15.dat at all - a real library/environment-boundary fact, not
         * a bug in this file. Guarded generically (any family whose
         * resolved DATAFILE* is NULL, not just "sfx" by name) so a SKIP
         * line is written instead of dereferencing a null pointer. */
        base = pf_asset_family(row->datafile);
        if (!base) {
            fprintf(f, "%d %s SKIP family-not-loaded=%s\n", i, row->object_name, row->datafile);
            continue;
        }

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
            fprintf(f, "%d %s SKIP unrecognized-type=%s\n", i, row->object_name, row->type_fourcc);
            continue;
        }
        digest_hex(&d, hex);
        fprintf(f, "%d %s %s\n", i, row->object_name ? row->object_name : "?", hex);
    }
    fclose(f);
    fprintf(stderr, "dump_assets: wrote %d asset id(s) to '%s'\n", (int)ASSET_COUNT, path);
}
