/* batch14b_check.c -- the COMPILED-CANDIDATE half of PROMOTIONS.md
 * batch 14's ORDERED-CALL-TRACE oracle for the eight coastline
 * functions whose comparison domain is "which library calls happened,
 * in what order, with what arguments (and, for the file writers, with
 * what BYTES)":
 *
 *   destroy_replay   (src/icytower/replay.c)
 *   save_replay      (src/icytower/replay.c)
 *   myDeleteFile     (src/icytower/config.c)
 *   save_config      (src/icytower/config.c)
 *   init_scroller    (src/icytower/scroller.c)
 *   fadeOut          (src/icytower/fade.c)
 *   fadeIn           (src/icytower/fade.c)
 *   save_profile     (src/icytower/profile.c)
 *
 * Harness-only; nothing under src/ is changed or seam-ed for it
 * (win32_pilot.md SS7a).  A standalone main(), not a lift_check.py SPECS
 * row, for the reasons batches 10/12/13 already established -- and most
 * sharply here: save_replay() issues over five hundred pack_fwrite()
 * calls with a different buffer each time, which mechanism B's
 * "count plus the arguments of the FIRST call" would summarise into
 * nothing.
 *
 * Protocol: argv[1] is a vector file, one vector per line, fields
 * separated by '|'.  The trace for each vector is printed between
 * "V <n>" and "E", one call per line, in order, followed by whatever
 * memory-domain facts that vector's function writes.  Pointers are
 * rendered as stable SYMBOLS and strings as their CONTENT AT CALL TIME,
 * so the guest and host address spaces never have to agree --
 * draw_frame_xcheck.py's convention, reused for the fourth batch.
 *
 * pack_fwrite's `buf` is rendered as the HEX OF THE BYTES it would
 * write, not as a symbol: the sequence of those hex strings IS the .itr
 * file, which is the whole point of tracing save_replay at all.
 *
 * Build: see build_batch14.sh.
 */
#include "pf_harness_batch14.h"
#undef sprintf
#undef fopen
#undef fwrite
#undef fprintf
#undef fputs
#undef fputc
#undef fclose
#undef free
#undef time
#undef localtime
#undef mkdir

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro_api.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

/* ------------------------------------------------------------------ */
/* storage the standalone world needs but state.c does not supply      */
/* ------------------------------------------------------------------ */
FONT *font;
volatile char key[127];
int *allegro_errno;
BITMAP *screen;
GFX_DRIVER *gfx_driver;
static int errno_storage;

/* ------------------------------------------------------------------ */
/* trace                                                               */
/* ------------------------------------------------------------------ */
static FILE *out;

static struct { const void *p; const char *name; } syms[40];
static int nsyms;

static void bind_sym(const void *p, const char *name)
{
    if (p && nsyms < 40) { syms[nsyms].p = p; syms[nsyms].name = name; nsyms++; }
}

static const char *sym(const void *p)
{
    int i;
    if (!p) return "NULL";
    for (i = 0; i < nsyms; i++)
        if (syms[i].p == p) return syms[i].name;
    return "?";
}

/* Trace records are compared LINE BY LINE, so a traced string that
 * itself contains a newline (a page builder's output, a multi-line
 * scroller text) would split one record into several and desynchronise
 * the whole block.  Every string that reaches the trace goes through
 * this, and batch14b_check.py's esc() produces byte-for-byte the same
 * escaping. */
static void pstr(const char *s)
{
    const unsigned char *b = (const unsigned char *)s;
    for (; *b; b++) {
        if (*b == '\\')
            fputs("\\\\", out);
        else if (*b == '"')
            fputs("\\\"", out);
        else if (*b >= 0x20 && *b < 0x7f)
            fputc(*b, out);
        else
            fprintf(out, "\\x%02x", *b);
    }
}

static void hexout(const void *p, long n)
{
    const unsigned char *b = (const unsigned char *)p;
    long i;
    for (i = 0; i < n; i++)
        fprintf(out, "%02x", b[i]);
}

/* ------------------------------------------------------------------ */
/* script                                                              */
/* ------------------------------------------------------------------ */
static int   script_fopen_ok;
static int   script_packfopen_ok;
static int   script_exists;          /* file_exists() result */
static int   script_rest_n;          /* rest() calls per spin */
static int   script_rest_i;
static int   script_gfx_null;
static int   script_load_replay_ok;
static long  script_time;
static int   script_tm[9];
static const char *script_dir;
static int   script_checksum;

/* ------------------------------------------------------------------ */
/* CRT redirects declared by pf_harness_batch14.h                      */
/* ------------------------------------------------------------------ */
static FILE fake_file;
static char page_general[]  = "general-page\n";
static char page_basic[]    = "basic-page\n";
static char page_advanced[] = "advanced-page\n";
static char page_extra[]    = "extra-page\n";

int h_sprintf(char *dst, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsprintf(dst, fmt, ap);
    va_end(ap);
    fprintf(out, "sprintf \""); pstr(fmt);
    fprintf(out, "\" -> \""); pstr(dst); fprintf(out, "\"\n");
    return n;
}
FILE *h_fopen(const char *path, const char *mode)
{
    fprintf(out, "fopen \""); pstr(path); fprintf(out, "\" \""); pstr(mode);
    fprintf(out, "\" -> %s\n", script_fopen_ok ? "file" : "NULL");
    return script_fopen_ok ? &fake_file : (FILE *)0;
}
size_t h_fwrite(const void *buf, size_t sz, size_t n, FILE *f)
{
    fprintf(out, "fwrite %s %u %u [", (f == &fake_file) ? "file" : "?",
            (unsigned)sz, (unsigned)n);
    hexout(buf, (long)(sz * n));
    fprintf(out, "]\n");
    return n;
}
int h_fprintf(FILE *f, const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsprintf(tmp, fmt, ap);
    va_end(ap);
    fprintf(out, "fprintf %s \"", (f == &fake_file) ? "file" : "?");
    pstr(fmt); fprintf(out, "\" -> \""); pstr(tmp); fprintf(out, "\"\n");
    return n;
}
int h_fputs(const char *s, FILE *f)
{
    fprintf(out, "fputs %s \"", (f == &fake_file) ? "file" : "?");
    pstr(s); fprintf(out, "\"\n");
    return 0;
}
int h_fputc(int c, FILE *f)
{
    fprintf(out, "fputc %d %s\n", c, (f == &fake_file) ? "file" : "?");
    return c;
}
int h_fclose(FILE *f)
{
    fprintf(out, "fclose %s\n", (f == &fake_file) ? "file" : "?");
    return 0;
}
void h_free(void *p)
{
    fprintf(out, "free %s\n", sym(p));
}
time_t h_time(time_t *t)
{
    fprintf(out, "time %s\n", t ? "&t" : "NULL");
    if (t) *t = (time_t)script_time;
    return (time_t)script_time;
}
struct tm *h_localtime(const time_t *t)
{
    static struct tm the_tm;
    fprintf(out, "localtime %ld\n", t ? (long)*t : 0L);
    the_tm.tm_sec  = script_tm[0]; the_tm.tm_min  = script_tm[1];
    the_tm.tm_hour = script_tm[2]; the_tm.tm_mday = script_tm[3];
    the_tm.tm_mon  = script_tm[4]; the_tm.tm_year = script_tm[5];
    the_tm.tm_wday = script_tm[6]; the_tm.tm_yday = script_tm[7];
    the_tm.tm_isdst = script_tm[8];
    return &the_tm;
}
int h_mkdir(const char *path)
{
    fprintf(out, "mkdir \""); pstr(path); fprintf(out, "\"\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Allegro / game-function stubs                                       */
/* ------------------------------------------------------------------ */
static PACKFILE fake_packfile;
static BITMAP bmp_swap, bmp_screen, bmp_tmp, bmp_src;
static GFX_VTABLE vt8, vt16;
static GFX_DRIVER the_gfx;
static Tcontrol the_ctrl;
static Thisc_table the_tables[15];
static Treplay old_replay;
static FONT the_font;
static char rank_label_store[12][16];

int delete_file(const char *name)
{
    fprintf(out, "delete_file \""); pstr(name); fprintf(out, "\"\n");
    return 0;
}
void log2file(const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(tmp, fmt, ap);
    va_end(ap);
    fprintf(out, "log2file \""); pstr(fmt);
    fprintf(out, "\" -> \""); pstr(tmp); fprintf(out, "\"\n");
}
int get_configfile_path(char *buffer, it_orig_size_t buflen)
{
    fprintf(out, "get_configfile_path %d\n", (int)buflen);
    strcpy(buffer, script_dir);
    return 1;
}
int get_profile_dir_for_profile(char *buffer, it_orig_size_t buflen,
                                const char *pname)
{
    fprintf(out, "get_profile_dir_for_profile %d \"", (int)buflen);
    pstr(pname); fprintf(out, "\"\n");
    strcpy(buffer, script_dir);
    return 1;
}
PACKFILE *pack_fopen(const char *path, const char *mode)
{
    fprintf(out, "pack_fopen \""); pstr(path); fprintf(out, "\" \""); pstr(mode);
    fprintf(out, "\" -> %s\n", script_packfopen_ok ? "packfile" : "NULL");
    return script_packfopen_ok ? &fake_packfile : (PACKFILE *)0;
}
long pack_fwrite(const void *buf, long size, PACKFILE *f)
{
    fprintf(out, "pack_fwrite %s %ld [", (f == &fake_packfile) ? "packfile" : "?",
            size);
    hexout(buf, size);
    fprintf(out, "]\n");
    return size;
}
int pack_fclose(PACKFILE *f)
{
    fprintf(out, "pack_fclose %s\n", (f == &fake_packfile) ? "packfile" : "?");
    return 0;
}
void save_options(Toptions *o, PACKFILE *f)
{
    fprintf(out, "save_options %s %s\n", (o == &options) ? "options" : "?",
            (f == &fake_packfile) ? "packfile" : "?");
}
void save_hisc_table(Thisc_table *t, PACKFILE *f)
{
    int i, k = -1;
    for (i = 0; i < 15; i++) if (t == &the_tables[i]) k = i;
    fprintf(out, "save_hisc_table table%d %s\n", k,
            (f == &fake_packfile) ? "packfile" : "?");
}
int file_exists(const char *name, int attrib, int *aret)
{
    (void)aret;
    fprintf(out, "file_exists \""); pstr(name);
    fprintf(out, "\" %d -> %d\n", attrib, script_exists);
    return script_exists;
}
Treplay *load_replay(const char *name)
{
    fprintf(out, "load_replay \""); pstr(name);
    fprintf(out, "\" -> %s\n", script_load_replay_ok ? "old" : "NULL");
    return script_load_replay_ok ? &old_replay : (Treplay *)0;
}
int generate_profile_checksum(Tprofile *p)
{
    (void)p;
    fprintf(out, "generate_profile_checksum -> %d\n", script_checksum);
    return script_checksum;
}
Tcontrol *get_controls(void)
{
    fprintf(out, "get_controls\n");
    return &the_ctrl;
}
void save_control(Tcontrol *c, it_orig_FILE *f)
{
    fprintf(out, "save_control %s %s\n", (c == &the_ctrl) ? "ctrl" : "?",
            ((void *)f == (void *)&fake_file) ? "file" : "?");
}
char *profile_data_page_general(Tprofile *p, char *filler)
{
    (void)p;
    fprintf(out, "profile_data_page_general \""); pstr(filler);
    fprintf(out, "\"\n");
    return page_general;
}
char *profile_data_page_basic(Tprofile *p)
{ (void)p; fprintf(out, "profile_data_page_basic\n"); return page_basic; }
char *profile_data_page_advanced(Tprofile *p)
{ (void)p; fprintf(out, "profile_data_page_advanced\n"); return page_advanced; }
char *profile_data_page_extra(Tprofile *p)
{ (void)p; fprintf(out, "profile_data_page_extra\n"); return page_extra; }

int text_height(const FONT *f)
{
    fprintf(out, "text_height %s\n", sym(f));
    return 17;
}
int text_length(const FONT *f, const char *s)
{
    fprintf(out, "text_length %s \"", sym(f)); pstr(s); fprintf(out, "\"\n");
    return (int)strlen(s) * 9;
}
BITMAP *create_bitmap(int w, int h)
{
    fprintf(out, "create_bitmap %d %d\n", w, h);
    return &bmp_tmp;
}
void destroy_bitmap(BITMAP *b) { fprintf(out, "destroy_bitmap %s\n", sym(b)); }
void blit(BITMAP *s, BITMAP *d, int sx, int sy, int dx, int dy, int w, int h)
{
    fprintf(out, "blit %s %s %d %d %d %d %d %d\n",
            sym(s), sym(d), sx, sy, dx, dy, w, h);
}
void set_trans_blender(int r, int g, int b, int a)
{ fprintf(out, "set_trans_blender %d %d %d %d\n", r, g, b, a); }
void drawing_mode(int mode, BITMAP *pat, int x, int y)
{ fprintf(out, "drawing_mode %d %s %d %d\n", mode, sym(pat), x, y); }
void solid_mode(void) { fprintf(out, "solid_mode\n"); }
int makecol(int r, int g, int b)
{ fprintf(out, "makecol %d %d %d\n", r, g, b); return 0x123456; }
void blit_to_screen(BITMAP *b) { fprintf(out, "blit_to_screen %s\n", sym(b)); }
void rest(unsigned int ms)
{
    fprintf(out, "rest %u\n", ms);
    script_rest_i++;
    if (script_rest_i >= script_rest_n) { script_rest_i = 0; cycle_count = 1; }
}

/* vtable slots fade.c reaches through rectfill()/draw_sprite() */
static void vt_rectfill(BITMAP *b, int x1, int y1, int x2, int y2, int c)
{ fprintf(out, "rectfill %s %d %d %d %d %d\n", sym(b), x1, y1, x2, y2, c); }
static void vt_draw_sprite(BITMAP *b, BITMAP *s, int x, int y)
{ fprintf(out, "draw_sprite %s %s %d %d\n", sym(b), sym(s), x, y); }
static void vt_draw_256_sprite(BITMAP *b, BITMAP *s, int x, int y)
{ fprintf(out, "draw_256_sprite %s %s %d %d\n", sym(b), sym(s), x, y); }

/* ------------------------------------------------------------------ */
/* the world                                                           */
/* ------------------------------------------------------------------ */
static Treplay the_replay;
static Trecord the_records[512];
static Tprofile the_profile;
static Tscroller the_scroller;
static char scroller_text[8192];

static void world_init(void)
{
    int i;
    nsyms = 0;
    memset(&vt8, 0, sizeof vt8);
    memset(&vt16, 0, sizeof vt16);
    vt8.color_depth = 8;
    vt16.color_depth = 16;
    vt8.rectfill = vt_rectfill;   vt16.rectfill = vt_rectfill;
    vt8.draw_sprite = vt_draw_sprite; vt16.draw_sprite = vt_draw_sprite;
    vt8.draw_256_sprite = vt_draw_256_sprite;
    vt16.draw_256_sprite = vt_draw_256_sprite;
    bind_sym(&bmp_swap, "swap");
    bind_sym(&bmp_screen, "screen");
    bind_sym(&bmp_tmp, "tmp");
    bind_sym(&bmp_src, "src");
    bind_sym(&the_font, "font");
    bind_sym(page_general, "page_general");
    bind_sym(page_basic, "page_basic");
    bind_sym(page_advanced, "page_advanced");
    bind_sym(page_extra, "page_extra");
    bind_sym(&the_replay, "replay");
    bind_sym(the_records, "records");
    swap_screen = &bmp_swap;
    screen = &bmp_screen;
    for (i = 0; i < 15; i++)
        hisc_tables[i] = &the_tables[i];
    /* save_profile() INLINES get_rank(), which reads all five rank
     * tables.  They are pinned to a fixed, boring state on both sides
     * (all thresholds 0 -> rank 11 always) so that save_profile's own
     * trace is not entangled with the rank search batch14_check.py
     * already tests exhaustively on its own. */
    for (i = 0; i < 12; i++) {
        rankFloors[i] = rankCombos[i] = rankNMLs[i] = rankCCCs[i] = 0;
        sprintf(rank_label_store[i], "rank%d", i);
        rankLables[i] = rank_label_store[i];
    }
    script_rest_i = 0;
}

/* ------------------------------------------------------------------ */
/* vector parsing                                                      */
/* ------------------------------------------------------------------ */
#define MAXF 32
static char *fld[MAXF];
static int nfld;

static void split(char *line)
{
    nfld = 0;
    fld[nfld++] = line;
    while (*line) {
        if (*line == '|') { *line = 0; if (nfld < MAXF) fld[nfld++] = line + 1; }
        line++;
    }
}
static int fi(int i) { return (i < nfld) ? (int)strtol(fld[i], (char **)0, 0) : 0; }

/* de-escape a vector field: \n -> newline, \\ -> backslash */
static void unescape(char *d, const char *s, int cap)
{
    int n = 0;
    while (*s && n < cap - 1) {
        if (*s == '\\' && s[1]) {
            s++;
            *d++ = (*s == 'n') ? '\n' : *s;
            s++;
        } else {
            *d++ = *s++;
        }
        n++;
    }
    *d = 0;
}

/* fill a Treplay from a deterministic per-vector LCG -- the SAME one the
 * Python side runs, so the two structures are byte-identical without
 * shipping 2 KB per vector through the text protocol. */
static unsigned int lcg_state;
static unsigned int lcg(void)
{
    lcg_state = lcg_state * 1103515245u + 12345u;
    return lcg_state;
}
static void fill_replay(Treplay *r, unsigned int seed, int size)
{
    unsigned char *b = (unsigned char *)r;
    unsigned int i;
    lcg_state = seed;
    for (i = 0; i < sizeof(Treplay); i++)
        b[i] = (unsigned char)(lcg() >> 16);
    memcpy(r->header, "ITR15", 6);
    r->size = size;
    r->data = the_records;
    for (i = 0; i < (unsigned int)size; i++) {
        the_records[i].key_flags = (unsigned char)(lcg() >> 16);
        the_records[i].cycle_count = (int)lcg();
    }
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    FILE *vf;
    char line[16384];
    int n = 0;

    allegro_errno = &errno_storage;
    out = stdout;
    if (argc < 2) { fprintf(stderr, "usage: batch14b_check <vectors>\n"); return 2; }
    vf = fopen(argv[1], "rb");
    if (!vf) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    while (fgets(line, sizeof line, vf)) {
        char *nl = strpbrk(line, "\r\n");
        /* A vector line longer than `line` would be SPLIT by fgets and its
         * tail parsed as a vector of its own -- which is exactly how the
         * first over-long init_scroller text surfaced, as an "unknown
         * vector kind" naming the middle of a scroller message.  Refuse
         * loudly instead of silently comparing garbage. */
        if (!nl && !feof(vf)) {
            fprintf(stderr, "vector line %d longer than %d bytes\n",
                    n, (int)sizeof line);
            return 2;
        }
        if (nl) *nl = 0;
        if (!line[0]) continue;
        split(line);
        world_init();
        printf("V %d\n", n++);

        if (!strcmp(fld[0], "destroy_replay")) {
            /* 1 r-null | 2 data-null */
            memset(&the_replay, 0, sizeof the_replay);
            the_replay.data = fi(2) ? (Trecord *)0 : the_records;
            destroy_replay(fi(1) ? (Treplay *)0 : &the_replay);

        } else if (!strcmp(fld[0], "myDeleteFile")) {
            /* 1 path | 2 file */
            myDeleteFile(fld[1], fld[2]);

        } else if (!strcmp(fld[0], "save_config")) {
            /* 1 dir | 2 pack_fopen-ok */
            script_dir = fld[1];
            script_packfopen_ok = fi(2);
            save_config();

        } else if (!strcmp(fld[0], "init_scroller")) {
            /* 1 text(escaped) | 2 w | 3 h | 4 horiz */
            int i, rows, tlen;
            unescape(scroller_text, fld[1], (int)sizeof scroller_text);
            tlen = (int)strlen(scroller_text);
            memset(&the_scroller, 0, sizeof the_scroller);
            init_scroller(&the_scroller, &the_font, scroller_text,
                          fi(2), fi(3), fi(4));
            printf("sc %d %d %d %d %d %d %d\n",
                   the_scroller.horizontal, the_scroller.font_height,
                   the_scroller.width, the_scroller.height,
                   the_scroller.offset, the_scroller.rows,
                   the_scroller.length);
            rows = the_scroller.rows;
            if (rows < 0) rows = 0;
            if (rows > 512) rows = 512;
            for (i = 0; i < rows; i++)
            {
                printf("line %d %d \"", i,
                       (int)(the_scroller.lines[i] - scroller_text));
                pstr(the_scroller.lines[i]);
                printf("\"\n");
            }
            printf("text [");
            /* tlen was taken BEFORE the call: init_scroller() overwrites
             * newlines with NULs in place, so strlen() afterwards would
             * report only the first line. */
            hexout(scroller_text, (long)tlen + 1);
            printf("]\n");

        } else if (!strcmp(fld[0], "fadeOut")) {
            /* 1 speed | 2 gfx-null | 3 w | 4 h | 5 depth8 | 6 rest_n */
            script_gfx_null = fi(2);
            the_gfx.w = fi(3); the_gfx.h = fi(4);
            gfx_driver = script_gfx_null ? (GFX_DRIVER *)0 : &the_gfx;
            bmp_tmp.vtable = fi(5) ? &vt8 : &vt16;
            bmp_swap.vtable = &vt16;
            bmp_screen.vtable = &vt16;
            script_rest_n = fi(6);
            cycle_count = 0;
            fadeOut(fi(1));

        } else if (!strcmp(fld[0], "fadeIn")) {
            /* 1 speed | 2 gfx-null | 3 w | 4 h | 5 depth8 | 6 rest_n */
            script_gfx_null = fi(2);
            the_gfx.w = fi(3); the_gfx.h = fi(4);
            gfx_driver = script_gfx_null ? (GFX_DRIVER *)0 : &the_gfx;
            bmp_src.vtable = fi(5) ? &vt8 : &vt16;
            bmp_tmp.vtable = &vt16;
            script_rest_n = fi(6);
            cycle_count = 0;
            fadeIn(&bmp_src, fi(1));

        } else if (!strcmp(fld[0], "save_replay")) {
            /* 1 path | 2 file | 3 seed | 4 size | 5 newdate | 6 packok
             * 7 load_ok | 8 time | 9..17 tm[9] */
            int i, ret;
            fill_replay(&the_replay, (unsigned int)strtoul(fld[3], (char **)0, 0),
                        fi(4));
            memset(&old_replay, 0, sizeof old_replay);
            strcpy(old_replay.date, "OLD DATE 1999");
            script_packfopen_ok = fi(6);
            script_load_replay_ok = fi(7);
            script_time = strtol(fld[8], (char **)0, 0);
            for (i = 0; i < 9; i++) script_tm[i] = fi(9 + i);
            ret = save_replay(fld[1], fld[2], &the_replay, fi(4), fi(5));
            printf("ret %d\n", ret);
            printf("date [");
            hexout(the_replay.date, 32);
            printf("]\n");
            printf("size %d checksum %d\n", the_replay.size, the_replay.checksum);

        } else if (!strcmp(fld[0], "save_profile")) {
            /* 1 dir | 2 handle | 3 exists | 4 fopen_ok | 5 checksum
             * 6 time | 7..15 tm[9] */
            int i, ret;
            memset(&the_profile, 0, sizeof the_profile);
            strncpy(the_profile.handle, fld[2], 31);
            script_dir = fld[1];
            script_exists = fi(3);
            script_fopen_ok = fi(4);
            script_checksum = fi(5);
            script_time = strtol(fld[6], (char **)0, 0);
            for (i = 0; i < 9; i++) script_tm[i] = fi(7 + i);
            ret = save_profile(&the_profile);
            printf("ret %d\n", ret);
            printf("checksum %d\n", the_profile.checksum);
            printf("saveDate [");
            hexout(the_profile.saveDate, 16);
            printf("]\n");

        } else {
            fprintf(stderr, "unknown vector kind %s\n", fld[0]);
            return 2;
        }
        printf("E\n");
    }
    fclose(vf);
    return 0;
}
