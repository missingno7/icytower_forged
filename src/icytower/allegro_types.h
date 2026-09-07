/* GENERATED FILE -- DO NOT EDIT.
 * Produced by tools/pf_win32_gen_src_headers.py (which reuses tools/pf_win32_gen_interop.py's
 * DWARF parser and type IR -- see that file for the parsing itself) from:
 *   artifacts/dwarf_info.txt
 *   artifacts/functions.json
 * scope=game. Re-run tools/pf_win32_gen_src_headers.py to regenerate; do not hand-edit.
 *
 * These are Allegro's (and, where the game shares a CRT/library
 * header transitively, that library's own) PUBLIC types -- BITMAP,
 * DATAFILE, SAMPLE, FONT, PACKFILE, RGB, PALETTE, JOYSTICK_INFO, fixed
 * and friends -- recovered here only because the game CUs use them by
 * value or by pointer and this port must still compile without any
 * carrier header. Once the port takes a real Allegro dependency, THIS
 * FILE goes away and these names come from Allegro's own headers
 * instead; nothing else in src/icytower/ should assume otherwise.
 *
 * A type here is "not game_types.h" purely because it also gets
 * redeclared, per DWARF, inside at least one compile unit outside
 * F:\projects\icytower\trunk\source\ -- i.e. some non-game object
 * in the original binary (Allegro, libpng, zlib, pthreads-win32,
 * DirectX headers, the CRT) also defines this exact type, so it is not
 * the game's own. See tools/pf_win32_gen_src_headers.py's
 * compute_type_origin() docstring in gen_interop.py for the exact rule.
 *
 * Skipped when ICYTOWER_BINDINGS_ACTIVE is defined: the generated bindings header
 * (carrier/gen/pf_bindings_src.h, force-included only when src/ is
 * compiled INTO the carrier) already supplied every one of these names
 * with the same layout, via its own copy of it_types.h -- see
 * src/README.md.
 *
 * Skipped, in favour of the REAL upstream <allegro.h>, when ICYTOWER_UPSTREAM_ALLEGRO is
 * defined: this is the standalone-build LIBRARIES-coastline swap
 * (carrier/gen/LIB_BINDINGS_NOTES.md "Standalone build swap") --
 * no source file under src/icytower changes at all to select it, because every
 * src/ file reaches this header only through game_types.h's own
 * `#include "allegro_types.h"`, never directly; defining ICYTOWER_UPSTREAM_ALLEGRO on the
 * compiler command line (`-DICYTOWER_UPSTREAM_ALLEGRO`) is the entire swap. The CRT-reserved
 * subset below (it_orig_size_t, it_orig_FILE, ...) is NOT Allegro's
 * own and upstream <allegro.h> never defines these renamed names, so
 * it is emitted in this branch too -- see CRT_RESERVED_NAMES in
 * gen_src_headers.py.
 */
#ifndef ICYTOWER_ALLEGRO_TYPES_H
#define ICYTOWER_ALLEGRO_TYPES_H

#if defined(ICYTOWER_UPSTREAM_ALLEGRO)

#include <allegro.h>  /* real upstream Allegro 4.4.3.1 -- BITMAP, DATAFILE, */
                      /* SAMPLE, FONT, PACKFILE, RGB, PALETTE, JOYSTICK_INFO, */
                      /* fixed and friends now come from here, not below */

/* CRT-reserved names upstream <allegro.h> does not define under these
 * renamed identifiers (it never needed to -- it just uses plain FILE/
 * size_t/time_t itself); game_types.h still references them by these
 * names, so they are emitted unconditionally here. */
#pragma pack(push, 1)

/* forward declarations */
struct it_orig__iobuf;

/* enumerations */
/* none in this scope */

/* struct / union bodies and typedefs, in dependency order */
struct it_orig__iobuf {
    char *_ptr;
    int _cnt;
    char *_base;
    int _flag;
    int _file;
    int _charbuf;
    int _bufsiz;
    char *_tmpfname;
};
typedef unsigned int it_orig_size_t;
typedef struct it_orig__iobuf it_orig_FILE;
typedef long it_orig_time_t;

#pragma pack(pop)

#elif !defined(ICYTOWER_BINDINGS_ACTIVE)

#pragma pack(push, 1)

/* forward declarations */
struct BITMAP;
struct DATAFILE;
struct DATAFILE_PROPERTY;
struct FONT;
struct FONT_GLYPH;
struct FONT_VTABLE;
struct GFX_VTABLE;
struct LZSS_PACK_DATA;
struct LZSS_UNPACK_DATA;
struct MIDI;
struct PACKFILE;
struct PACKFILE_VTABLE;
struct RGB;
struct RLE_SPRITE;
struct SAMPLE;
struct V3D;
struct V3D_f;
struct _al_normal_packfile_details;
struct it_anon_s_6b25;
struct it_orig__iobuf;

/* enumerations */
/* none in this scope */

/* struct / union bodies and typedefs, in dependency order */
struct it_orig__iobuf {
    char *_ptr;
    int _cnt;
    char *_base;
    int _flag;
    int _file;
    int _charbuf;
    int _bufsiz;
    char *_tmpfname;
};
typedef struct GFX_VTABLE GFX_VTABLE;
struct BITMAP {
    int w;
    int h;
    int clip;
    int cl;
    int cr;
    int ct;
    int cb;
    GFX_VTABLE *vtable;
    void *write_bank;
    void *read_bank;
    void *dat;
    unsigned long id;
    void *extra;
    int x_ofs;
    int y_ofs;
    int seg;
    unsigned char *line[0]; /* DWARF: unbounded array, kept as zero-length (see it_types.h notes) */
};
struct RGB {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char filler;
};
typedef int int32_t;
typedef int32_t fixed;
typedef struct V3D V3D;
typedef struct V3D_f V3D_f;
struct GFX_VTABLE {
    int color_depth;
    int mask_color;
    void *unwrite_bank;
    void (__cdecl *set_clip)(struct BITMAP *);
    void (__cdecl *acquire)(struct BITMAP *);
    void (__cdecl *release)(struct BITMAP *);
    struct BITMAP * (__cdecl *create_sub_bitmap)(struct BITMAP *, int, int, int, int);
    void (__cdecl *created_sub_bitmap)(struct BITMAP *, struct BITMAP *);
    int (__cdecl *getpixel)(struct BITMAP *, int, int);
    void (__cdecl *putpixel)(struct BITMAP *, int, int, int);
    void (__cdecl *vline)(struct BITMAP *, int, int, int, int);
    void (__cdecl *hline)(struct BITMAP *, int, int, int, int);
    void (__cdecl *hfill)(struct BITMAP *, int, int, int, int);
    void (__cdecl *line)(struct BITMAP *, int, int, int, int, int);
    void (__cdecl *fastline)(struct BITMAP *, int, int, int, int, int);
    void (__cdecl *rectfill)(struct BITMAP *, int, int, int, int, int);
    void (__cdecl *triangle)(struct BITMAP *, int, int, int, int, int, int, int);
    void (__cdecl *draw_sprite)(struct BITMAP *, struct BITMAP *, int, int);
    void (__cdecl *draw_256_sprite)(struct BITMAP *, struct BITMAP *, int, int);
    void (__cdecl *draw_sprite_v_flip)(struct BITMAP *, struct BITMAP *, int, int);
    void (__cdecl *draw_sprite_h_flip)(struct BITMAP *, struct BITMAP *, int, int);
    void (__cdecl *draw_sprite_vh_flip)(struct BITMAP *, struct BITMAP *, int, int);
    void (__cdecl *draw_trans_sprite)(struct BITMAP *, struct BITMAP *, int, int);
    void (__cdecl *draw_trans_rgba_sprite)(struct BITMAP *, struct BITMAP *, int, int);
    void (__cdecl *draw_lit_sprite)(struct BITMAP *, struct BITMAP *, int, int, int);
    void (__cdecl *draw_rle_sprite)(struct BITMAP *, const struct RLE_SPRITE *, int, int);
    void (__cdecl *draw_trans_rle_sprite)(struct BITMAP *, const struct RLE_SPRITE *, int, int);
    void (__cdecl *draw_trans_rgba_rle_sprite)(struct BITMAP *, const struct RLE_SPRITE *, int, int);
    void (__cdecl *draw_lit_rle_sprite)(struct BITMAP *, const struct RLE_SPRITE *, int, int, int);
    void (__cdecl *draw_character)(struct BITMAP *, struct BITMAP *, int, int, int, int);
    void (__cdecl *draw_glyph)(struct BITMAP *, const struct FONT_GLYPH *, int, int, int, int);
    void (__cdecl *blit_from_memory)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *blit_to_memory)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *blit_from_system)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *blit_to_system)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *blit_to_self)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *blit_to_self_forward)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *blit_to_self_backward)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *blit_between_formats)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *masked_blit)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *clear_to_color)(struct BITMAP *, int);
    void (__cdecl *pivot_scaled_sprite_flip)(struct BITMAP *, struct BITMAP *, fixed, fixed, fixed, fixed, fixed, fixed, int);
    void (__cdecl *do_stretch_blit)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int, int, int, int);
    void (__cdecl *draw_gouraud_sprite)(struct BITMAP *, struct BITMAP *, int, int, int, int, int, int);
    void (__cdecl *draw_sprite_end)(void);
    void (__cdecl *blit_end)(void);
    void (__cdecl *polygon)(struct BITMAP *, int, const int *, int);
    void (__cdecl *rect)(struct BITMAP *, int, int, int, int, int);
    void (__cdecl *circle)(struct BITMAP *, int, int, int, int);
    void (__cdecl *circlefill)(struct BITMAP *, int, int, int, int);
    void (__cdecl *ellipse)(struct BITMAP *, int, int, int, int, int);
    void (__cdecl *ellipsefill)(struct BITMAP *, int, int, int, int, int);
    void (__cdecl *arc)(struct BITMAP *, int, int, fixed, fixed, int, int);
    void (__cdecl *spline)(struct BITMAP *, const int *, int);
    void (__cdecl *floodfill)(struct BITMAP *, int, int, int);
    void (__cdecl *polygon3d)(struct BITMAP *, int, struct BITMAP *, int, V3D **);
    void (__cdecl *polygon3d_f)(struct BITMAP *, int, struct BITMAP *, int, V3D_f **);
    void (__cdecl *triangle3d)(struct BITMAP *, int, struct BITMAP *, V3D *, V3D *, V3D *);
    void (__cdecl *triangle3d_f)(struct BITMAP *, int, struct BITMAP *, V3D_f *, V3D_f *, V3D_f *);
    void (__cdecl *quad3d)(struct BITMAP *, int, struct BITMAP *, V3D *, V3D *, V3D *, V3D *);
    void (__cdecl *quad3d_f)(struct BITMAP *, int, struct BITMAP *, V3D_f *, V3D_f *, V3D_f *, V3D_f *);
    void (__cdecl *draw_sprite_ex)(struct BITMAP *, struct BITMAP *, int, int, int, int);
};
struct V3D {
    fixed x;
    fixed y;
    fixed z;
    fixed u;
    fixed v;
    int c;
};
struct V3D_f {
    float x;
    float y;
    float z;
    float u;
    float v;
    int c;
};
struct RLE_SPRITE {
    int w;
    int h;
    int color_depth;
    int size;
    signed char dat[0]; /* DWARF: unbounded array, kept as zero-length (see it_types.h notes) */
};
struct FONT_GLYPH {
    short w;
    short h;
    unsigned char dat[0]; /* DWARF: unbounded array, kept as zero-length (see it_types.h notes) */
};
typedef unsigned int it_orig_size_t;
struct SAMPLE {
    int bits;
    int stereo;
    int freq;
    int priority;
    unsigned long len;
    unsigned long loop_start;
    unsigned long loop_end;
    unsigned long param;
    void *data;
};
struct it_anon_s_6b25 {
    unsigned char *data;
    int len;
};
struct MIDI {
    int divisions;
    struct it_anon_s_6b25 track[32];
};
struct DATAFILE_PROPERTY {
    char *dat;
    int type;
};
typedef struct DATAFILE_PROPERTY DATAFILE_PROPERTY;
struct DATAFILE {
    void *dat;
    int type;
    long size;
    DATAFILE_PROPERTY *prop;
};
typedef struct RGB RGB;
typedef RGB PALETTE[256];
typedef struct BITMAP BITMAP;
typedef struct SAMPLE SAMPLE;
typedef struct MIDI MIDI;
typedef struct DATAFILE DATAFILE;
struct FONT {
    void *data;
    int height;
    struct FONT_VTABLE *vtable;
};
struct PACKFILE_VTABLE {
    int (__cdecl *pf_fclose)(void *);
    int (__cdecl *pf_getc)(void *);
    int (__cdecl *pf_ungetc)(int, void *);
    long (__cdecl *pf_fread)(void *, long, void *);
    int (__cdecl *pf_putc)(int, void *);
    long (__cdecl *pf_fwrite)(const void *, long, void *);
    int (__cdecl *pf_fseek)(void *, int);
    int (__cdecl *pf_feof)(void *);
    int (__cdecl *pf_ferror)(void *);
};
struct _al_normal_packfile_details {
    int hndl;
    int flags;
    unsigned char *buf_pos;
    int buf_size;
    long todo;
    struct PACKFILE *parent;
    struct LZSS_PACK_DATA *pack_data;
    struct LZSS_UNPACK_DATA *unpack_data;
    char *filename;
    char *passdata;
    char *passpos;
    unsigned char buf[4096];
};
typedef struct PACKFILE_VTABLE PACKFILE_VTABLE;
struct PACKFILE {
    const PACKFILE_VTABLE *vtable;
    void *userdata;
    int is_normal_packfile;
    struct _al_normal_packfile_details normal;
};
typedef struct FONT FONT;
struct FONT_VTABLE {
    int (__cdecl *font_height)(const FONT *);
    int (__cdecl *char_length)(const FONT *, int);
    int (__cdecl *text_length)(const FONT *, const char *);
    int (__cdecl *render_char)(const FONT *, int, int, int, BITMAP *, int, int);
    void (__cdecl *render)(const FONT *, const char *, int, int, BITMAP *, int, int);
    void (__cdecl *destroy)(FONT *);
    int (__cdecl *get_font_ranges)(FONT *);
    int (__cdecl *get_font_range_begin)(FONT *, int);
    int (__cdecl *get_font_range_end)(FONT *, int);
    FONT * (__cdecl *extract_font_range)(FONT *, int, int);
    FONT * (__cdecl *merge_fonts)(FONT *, FONT *);
    int (__cdecl *transpose_font)(FONT *, int);
};
struct LZSS_PACK_DATA {
    int state;
    int i;
    int c;
    int len;
    int r;
    int s;
    int last_match_length;
    int code_buf_ptr;
    unsigned char mask;
    char code_buf[17];
    unsigned char _pad_0[2];
    int match_position;
    int match_length;
    int lson[4097];
    int rson[4353];
    int dad[4097];
    unsigned char text_buf[4113];
    unsigned char _pad_1[3];
};
struct LZSS_UNPACK_DATA {
    int state;
    int i;
    int j;
    int k;
    int r;
    int c;
    int flags;
    unsigned char text_buf[4113];
    unsigned char _pad_0[3];
};
typedef struct it_orig__iobuf it_orig_FILE;
typedef long it_orig_time_t;
typedef struct PACKFILE PACKFILE;

#pragma pack(pop)

#endif /* ICYTOWER_UPSTREAM_ALLEGRO / !ICYTOWER_BINDINGS_ACTIVE */

#endif /* ICYTOWER_ALLEGRO_TYPES_H */
