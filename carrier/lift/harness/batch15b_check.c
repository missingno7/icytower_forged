/* batch15b_check.c -- the COMPILED-CANDIDATE half of PROMOTIONS.md
 * batch 15's ORDERED-CALL-TRACE oracle for the five functions whose
 * effect is a sequence of library calls plus the memory they fill:
 *
 *   create_replay    (src/icytower/replay.c)
 *   load_replay      (src/icytower/replay.c)
 *   getGameDataXML   (src/icytower/game_data.c)
 *   draw_results     (src/icytower/results.c)
 *   my_alert         (src/icytower/alert.c)
 *
 * Harness-only; nothing under src/ is changed or seam-ed for it
 * (win32_pilot.md SS7a).  A standalone main(), not a lift_check.py SPECS
 * row, for the reasons batches 10/12/13/14 established -- and most
 * sharply here: load_replay() issues 30 distinct pack_fread() calls plus
 * 500 more in a loop plus two per record, and mechanism B's "count plus
 * the arguments of the FIRST call" would compare a fiftieth of it.
 *
 * Protocol: argv[1] is a BINARY vector file, a flat sequence of
 *
 *     u32 kind | u32 payload_len | payload_len bytes
 *
 * records (batch14_check.c's protocol, for its reason: several payloads
 * here are raw structure images -- a 2220-byte Treplay, a whole .itr
 * file -- which a '|'-separated text line cannot carry).  The trace for
 * each vector is printed between "V <n>" and "E", one call per line, in
 * order, followed by whatever memory-domain facts that vector's function
 * writes.  Pointers are rendered as stable SYMBOLS and strings as their
 * CONTENT AT CALL TIME, so the guest and host address spaces never have
 * to agree -- draw_frame_xcheck.py's convention, reused for the fifth
 * batch.
 *
 * The control layer (poll_control / is_any / is_left / is_right /
 * is_fire / is_enter) is STUBBED here even though all six are already
 * promoted, and deliberately: running them for real would make my_alert's
 * oracle a test of the keyboard and joystick globals instead of a test
 * of my_alert's own decision logic, and would make its branch coverage
 * depend on synthesising Tcontrol bit patterns.  Each stub answers from
 * a per-vector QUEUE, so the vector generator drives the dialog directly.
 * The six functions have their own vector oracles from batches 3 and 4.
 *
 * Build: see build_batch15.sh.
 */
#include "pf_harness_batch15.h"
#undef malloc
#undef free
#undef sprintf
#undef stricmp

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro_api.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "assets.h"
#include "assets_table.inc"

/* ------------------------------------------------------------------ */
/* storage the standalone world needs but state.c does not supply      */
/* ------------------------------------------------------------------ */
FONT *font;
volatile char key[127];
int *allegro_errno;
BITMAP *screen;
GFX_DRIVER *gfx_driver;
int gui_fg_color;
int gui_bg_color;
static int errno_storage;

/* the .rodata constant state.c zero-fills -- see pf_harness_batch15.h
 * note 4.  replay.c reaches this array through the REPLAY_HEADER macro. */
const char pf_b15_header[6] = { 'I', 'T', 'R', '1', '4', '0' };

/* ------------------------------------------------------------------ */
/* trace                                                               */
/* ------------------------------------------------------------------ */
static FILE *out;

static struct { const void *p; char name[16]; } syms[160];
static int nsyms;

static void bind_sym(const void *p, const char *name)
{
    if (p && nsyms < 160) {
        syms[nsyms].p = p;
        strncpy(syms[nsyms].name, name, 15);
        syms[nsyms].name[15] = 0;
        nsyms++;
    }
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
 * itself contains a newline (every one of getGameDataXML's fourteen
 * sprintf outputs does) would split one record into several and
 * desynchronise the whole block.  Every string that reaches the trace
 * goes through this, and batch15b_check.py's esc() produces byte-for-byte
 * the same escaping. */
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
/* heap: a bump allocator over an arena pre-filled with 0xA5           */
/* ------------------------------------------------------------------ */
/* 0x60000 -- the SAME number batch15b_check.py's ARENA_END - S_ARENA is.
 * It has to be, because create_replay(size) with a NEGATIVE size asks for
 * 0x20 + size * 8 as a size_t, i.e. ~4 GB, and whether that request is
 * refused is then part of the compared trace. */
#define ARENA_BYTES 0x60000
static unsigned char arena[ARENA_BYTES];
static long arena_used;
static int  malloc_seq;
static int  script_malloc_fail;     /* which malloc call returns NULL, -1 none */
static char heap_names[64][12];
static int  n_heap;

static void heap_reset(void)
{
    memset(arena, 0xA5, sizeof arena);
    arena_used = 0;
    malloc_seq = 0;
    n_heap = 0;
}

void *h_malloc(size_t n)
{
    void *p;
    /* the comparison is UNSIGNED on purpose: `arena_used + (long)n` would
     * wrap negative for the ~4 GB request a negative create_replay(size)
     * produces and quietly succeed. */
    if (malloc_seq == script_malloc_fail
        || n > (size_t)(ARENA_BYTES - arena_used)) {
        fprintf(out, "malloc %lu -> NULL\n", (unsigned long)n);
        malloc_seq++;
        return (void *)0;
    }
    p = arena + arena_used;
    arena_used += ((long)n + 15) & ~15L;
    if (n_heap < 64) {
        sprintf(heap_names[n_heap], "heap%d", n_heap);
        bind_sym(p, heap_names[n_heap]);
        n_heap++;
    }
    fprintf(out, "malloc %lu -> %s\n", (unsigned long)n, sym(p));
    malloc_seq++;
    return p;
}

void h_free(void *p)
{
    fprintf(out, "free %s\n", sym(p));
}

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

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int h_stricmp(const char *a, const char *b)
{
    int r = 0;
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    while (*x || *y) {
        r = lower(*x) - lower(*y);
        if (r) break;
        x++; y++;
    }
    r = (r > 0) ? 1 : (r < 0 ? -1 : 0);
    fprintf(out, "stricmp \""); pstr(a);
    fprintf(out, "\" \""); pstr(b); fprintf(out, "\" -> %d\n", r);
    return r;
}

/* ------------------------------------------------------------------ */
/* script                                                              */
/* ------------------------------------------------------------------ */
#define QMAX 64
typedef struct { int n, i; int v[QMAX]; } Queue;

static int qnext(Queue *q)
{
    if (q->i < q->n) return q->v[q->i++];
    return 0;
}

static Queue q_any_c, q_any_m;
static Queue q_left_c, q_left_m, q_right_c, q_right_m;
static Queue q_fire_c, q_fire_m, q_enter_m;

static int rest_count;
static int script_cycle_every;
static int script_esc_on, script_esc_off;
static int script_enter_on, script_enter_off;
static int script_close_at;

static int script_packopen1, script_packopen2;

/* the .itr byte image pack_fread() serves */
static unsigned char filebuf[65536];
static long filelen, filepos;
static int packopen_seq;

/* ------------------------------------------------------------------ */
/* Allegro / game-function stubs                                       */
/* ------------------------------------------------------------------ */
static PACKFILE fake_packfile;
static BITMAP bmp_swap, bmp_screen, bmp_logo, bmp_target;
static GFX_VTABLE vt8, vt16;
static GFX_DRIVER the_gfx;
static Tprofile the_profile;

#define NOBJ 100
static BITMAP objs[NOBJ];
static char obj_names[NOBJ][12];
static char cat_store[15][16];

BITMAP *asset_bitmap(asset_id id)
{
    return &objs[asset_table[id].index];
}
FONT *asset_font(asset_id id)
{
    return (FONT *)(void *)&objs[asset_table[id].index];
}
SAMPLE *asset_sample(asset_id id) { (void)id; return 0; }
PALETTE *asset_palette(asset_id id) { (void)id; return 0; }
void *asset_object(asset_id id)   { (void)id; return 0; }

void log2file(const char *fmt, ...)
{
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(tmp, fmt, ap);
    va_end(ap);
    fprintf(out, "log2file \""); pstr(fmt);
    fprintf(out, "\" -> \""); pstr(tmp); fprintf(out, "\"\n");
}

PACKFILE *pack_fopen(const char *path, const char *mode)
{
    int ok = (packopen_seq++ == 0) ? script_packopen1 : script_packopen2;
    fprintf(out, "pack_fopen \""); pstr(path); fprintf(out, "\" \"");
    pstr(mode); fprintf(out, "\" -> %s\n", ok ? "packfile" : "NULL");
    if (ok) filepos = 0;
    return ok ? &fake_packfile : (PACKFILE *)0;
}
long pack_fread(void *buf, long size, PACKFILE *f)
{
    long n = filelen - filepos;
    if (n > size) n = size;
    if (n < 0) n = 0;
    if (n) memcpy(buf, filebuf + filepos, (size_t)n);
    filepos += n;
    fprintf(out, "pack_fread %s %ld -> %ld [",
            (f == &fake_packfile) ? "packfile" : "?", size, n);
    hexout(buf, n);
    fprintf(out, "]\n");
    return n;
}
/* This executable links src/icytower/replay.c WHOLE, so save_replay()
 * comes along even though no vector here calls it; its one otherwise
 * unresolved callee has to exist for the link to succeed.  It abort()s:
 * if a future vector ever did reach it the run would stop loudly instead
 * of silently comparing something that was never executed.  (batch14_check.c
 * established the convention; --gc-sections does not help, because on
 * this PE target ld resolves symbols before it collects sections.) */
long pack_fwrite(const void *buf, long size, PACKFILE *f)
{
    (void)buf; (void)size; (void)f;
    fprintf(stderr, "batch15b_check: pack_fwrite reached (save_replay is not "
                    "under test in this oracle)\n");
    abort();
    return 0;
}

int pack_fclose(PACKFILE *f)
{
    fprintf(out, "pack_fclose %s\n", (f == &fake_packfile) ? "packfile" : "?");
    return 0;
}

int text_length(const FONT *f, const char *s)
{
    fprintf(out, "text_length %s \"", sym(f)); pstr(s); fprintf(out, "\"\n");
    return (int)strlen(s) * 9;
}
int makecol(int r, int g, int b)
{
    fprintf(out, "makecol %d %d %d\n", r, g, b);
    return (r << 16) | (g << 8) | b;
}
void set_trans_blender(int r, int g, int b, int a)
{ fprintf(out, "set_trans_blender %d %d %d %d\n", r, g, b, a); }
void drawing_mode(int mode, BITMAP *pat, int x, int y)
{ fprintf(out, "drawing_mode %d %s %d %d\n", mode, sym(pat), x, y); }
void solid_mode(void) { fprintf(out, "solid_mode\n"); }
void blit(BITMAP *s, BITMAP *d, int sx, int sy, int dx, int dy, int w, int h)
{
    fprintf(out, "blit %s %s %d %d %d %d %d %d\n",
            sym(s), sym(d), sx, sy, dx, dy, w, h);
}
void vsync(void) { fprintf(out, "vsync\n"); }
void clear_keybuf(void) { fprintf(out, "clear_keybuf\n"); }
void rest(unsigned int ms)
{
    fprintf(out, "rest %u\n", ms);
    rest_count++;
    if (script_cycle_every > 0 && (rest_count % script_cycle_every) == 0)
        cycle_count = 1;
    if (rest_count == script_esc_on)    key[KEY_ESC] = 1;
    if (rest_count == script_esc_off)   key[KEY_ESC] = 0;
    if (rest_count == script_enter_on)  key[KEY_ENTER] = 1;
    if (rest_count == script_enter_off) key[KEY_ENTER] = 0;
    if (rest_count == script_close_at)  closeButtonClicked = 1;
}
void textprintf_ex(BITMAP *b, const FONT *f, int x, int y, int c, int bg,
                   const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(tmp, fmt, ap);
    va_end(ap);
    fprintf(out, "textprintf_ex %s %s %d %d %d %d \"", sym(b), sym(f),
            x, y, c, bg);
    pstr(tmp); fprintf(out, "\"\n");
}
void textprintf_right_ex(BITMAP *b, const FONT *f, int x, int y, int c, int bg,
                         const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(tmp, fmt, ap);
    va_end(ap);
    fprintf(out, "textprintf_right_ex %s %s %d %d %d %d \"", sym(b), sym(f),
            x, y, c, bg);
    pstr(tmp); fprintf(out, "\"\n");
}
void textprintf_centre_ex(BITMAP *b, const FONT *f, int x, int y, int c, int bg,
                          const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(tmp, fmt, ap);
    va_end(ap);
    fprintf(out, "textprintf_centre_ex %s %s %d %d %d %d \"", sym(b), sym(f),
            x, y, c, bg);
    pstr(tmp); fprintf(out, "\"\n");
}
void textout_centre_ex(BITMAP *b, const FONT *f, const char *s, int x, int y,
                       int c, int bg)
{
    fprintf(out, "textout_centre_ex %s %s \"", sym(b), sym(f));
    pstr(s); fprintf(out, "\" %d %d %d %d\n", x, y, c, bg);
}
void textout_right_ex(BITMAP *b, const FONT *f, const char *s, int x, int y,
                      int c, int bg)
{
    fprintf(out, "textout_right_ex %s %s \"", sym(b), sym(f));
    pstr(s); fprintf(out, "\" %d %d %d %d\n", x, y, c, bg);
}

void poll_control(Tcontrol *c, int reset)
{ fprintf(out, "poll_control %s %d\n", sym(c), reset); }
int is_any(Tcontrol *c)
{
    int v = qnext((c == &ctrl) ? &q_any_c : &q_any_m);
    fprintf(out, "is_any %s -> %d\n", sym(c), v);
    return v;
}
int is_left(Tcontrol *c)
{
    int v = qnext((c == &ctrl) ? &q_left_c : &q_left_m);
    fprintf(out, "is_left %s -> %d\n", sym(c), v);
    return v;
}
int is_right(Tcontrol *c)
{
    int v = qnext((c == &ctrl) ? &q_right_c : &q_right_m);
    fprintf(out, "is_right %s -> %d\n", sym(c), v);
    return v;
}
int is_fire(Tcontrol *c)
{
    int v = qnext((c == &ctrl) ? &q_fire_c : &q_fire_m);
    fprintf(out, "is_fire %s -> %d\n", sym(c), v);
    return v;
}
int is_enter(Tcontrol *c)
{
    int v = qnext(&q_enter_m);
    fprintf(out, "is_enter %s -> %d\n", sym(c), v);
    return v;
}

/* vtable slots the recovered files reach through Allegro's AL_INLINEs */
static void vt_rectfill(BITMAP *b, int x1, int y1, int x2, int y2, int c)
{ fprintf(out, "rectfill %s %d %d %d %d %d\n", sym(b), x1, y1, x2, y2, c); }
static void vt_draw_sprite(BITMAP *b, BITMAP *s, int x, int y)
{ fprintf(out, "draw_sprite %s %s %d %d\n", sym(b), sym(s), x, y); }
static void vt_draw_256_sprite(BITMAP *b, BITMAP *s, int x, int y)
{ fprintf(out, "draw_256_sprite %s %s %d %d\n", sym(b), sym(s), x, y); }
static void vt_acquire(BITMAP *b) { fprintf(out, "acquire %s\n", sym(b)); }
static void vt_release(BITMAP *b) { fprintf(out, "release %s\n", sym(b)); }

/* ------------------------------------------------------------------ */
/* the world                                                           */
/* ------------------------------------------------------------------ */
static Tgame_data the_gd;
static Treplay xml_replay;
static int qualified[5];
static int qvalues[5];
static char str_func[128];
static char str_txt[128];

static void world_init(void)
{
    int i;
    nsyms = 0;
    memset(&vt8, 0, sizeof vt8);
    memset(&vt16, 0, sizeof vt16);
    vt8.color_depth = 8;
    vt16.color_depth = 16;
    vt8.rectfill = vt_rectfill;             vt16.rectfill = vt_rectfill;
    vt8.draw_sprite = vt_draw_sprite;       vt16.draw_sprite = vt_draw_sprite;
    vt8.draw_256_sprite = vt_draw_256_sprite;
    vt16.draw_256_sprite = vt_draw_256_sprite;
    vt8.acquire = vt_acquire;               vt16.acquire = vt_acquire;
    vt8.release = vt_release;               vt16.release = vt_release;

    bind_sym(&bmp_swap, "swap");
    bind_sym(&bmp_screen, "screen");
    bind_sym(&bmp_logo, "logo");
    bind_sym(&bmp_target, "target");
    bind_sym(&ctrl, "ctrl");
    bind_sym(&menu_params.ctrl, "menuctrl");
    for (i = 0; i < NOBJ; i++) {
        sprintf(obj_names[i], "obj%d", i);
        bind_sym(&objs[i], obj_names[i]);
        objs[i].vtable = &vt16;
        objs[i].w = 32;
        objs[i].h = 24;
    }
    for (i = 0; i < 15; i++) {
        sprintf(cat_store[i], "cat%d", i);
        category_names[i] = cat_store[i];
    }
    swap_screen = &bmp_swap;
    screen = &bmp_screen;
    bmp_swap.vtable = &vt16;
    bmp_screen.vtable = &vt16;
    bmp_logo.vtable = &vt16;
    bmp_target.vtable = &vt16;
    profile = &the_profile;
    gfx_driver = &the_gfx;
    heap_reset();
    rest_count = 0;
    filepos = 0;
    filelen = 0;
    packopen_seq = 0;
    key[KEY_ESC] = 0;
    key[KEY_ENTER] = 0;
    closeButtonClicked = 0;
    cycle_count = 0;
    memset(&q_any_c, 0, sizeof q_any_c);  memset(&q_any_m, 0, sizeof q_any_m);
    memset(&q_left_c, 0, sizeof q_left_c); memset(&q_left_m, 0, sizeof q_left_m);
    memset(&q_right_c, 0, sizeof q_right_c); memset(&q_right_m, 0, sizeof q_right_m);
    memset(&q_fire_c, 0, sizeof q_fire_c); memset(&q_fire_m, 0, sizeof q_fire_m);
    memset(&q_enter_m, 0, sizeof q_enter_m);
    script_malloc_fail = -1;
    script_cycle_every = 1;
    script_esc_on = script_esc_off = -1;
    script_enter_on = script_enter_off = -1;
    script_close_at = -1;
    script_packopen1 = script_packopen2 = 1;
}

/* ------------------------------------------------------------------ */
/* payload readers                                                     */
/* ------------------------------------------------------------------ */
static unsigned char payload[262144];
static long pp;

static unsigned int rd32(void)
{
    unsigned int v = (unsigned int)payload[pp] | ((unsigned int)payload[pp+1] << 8)
                   | ((unsigned int)payload[pp+2] << 16)
                   | ((unsigned int)payload[pp+3] << 24);
    pp += 4;
    return v;
}
static int rdi(void) { return (int)rd32(); }
static void rdbytes(void *d, long n) { memcpy(d, payload + pp, (size_t)n); pp += n; }
static void rdqueue(Queue *q)
{
    int i;
    q->n = rdi();
    q->i = 0;
    if (q->n > QMAX) q->n = QMAX;
    for (i = 0; i < q->n; i++) q->v[i] = rdi();
}

enum { K_CREATE = 0, K_LOAD = 1, K_XML = 2, K_RESULTS = 3, K_ALERT = 4 };

int main(int argc, char **argv)
{
    FILE *vf;
    unsigned char hdr[8];
    int n = 0;
    int i;

    allegro_errno = &errno_storage;
    out = stdout;
    if (argc < 2) { fprintf(stderr, "usage: batch15b_check <vectors.bin>\n"); return 2; }
    vf = fopen(argv[1], "rb");
    if (!vf) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    while (fread(hdr, 1, 8, vf) == 8) {
        unsigned int kind = (unsigned int)hdr[0] | ((unsigned int)hdr[1] << 8)
                          | ((unsigned int)hdr[2] << 16) | ((unsigned int)hdr[3] << 24);
        unsigned int len  = (unsigned int)hdr[4] | ((unsigned int)hdr[5] << 8)
                          | ((unsigned int)hdr[6] << 16) | ((unsigned int)hdr[7] << 24);
        if (len > sizeof payload) { fprintf(stderr, "payload too big\n"); return 2; }
        if (fread(payload, 1, len, vf) != len) { fprintf(stderr, "short read\n"); return 2; }
        pp = 0;
        world_init();
        printf("V %d\n", n++);

        switch (kind) {
        case K_CREATE: {
            int size = rdi();
            Treplay *r;
            script_malloc_fail = rdi();
            r = create_replay(size);
            printf("ret %s\n", sym(r));
            if (r) {
                /* the `data` POINTER at +0x8a8 is an address and cannot be
                 * compared across the two address spaces; it is dumped as a
                 * SYMBOL on its own line instead. */
                long nb = 0x20 + (long)size * 8;
                if (nb < 0x20) nb = 0x20;
                if (nb > 8192) nb = 8192;
                printf("replay [");
                hexout(r, 0x8a8);
                printf("]\n");
                printf("data %s\n", sym(r->data));
                if (r->data) {
                    printf("records [");
                    hexout(r->data, nb);
                    printf("]\n");
                }
            }
            break;
        }
        case K_LOAD: {
            char name[128];
            Treplay *r;
            int namelen;
            script_packopen1 = rdi();
            script_packopen2 = rdi();
            script_malloc_fail = rdi();
            namelen = rdi();
            if (namelen > 127) namelen = 127;
            rdbytes(name, namelen);
            name[namelen] = 0;
            filelen = rdi();
            if (filelen > (long)sizeof filebuf) filelen = (long)sizeof filebuf;
            rdbytes(filebuf, filelen);
            r = load_replay(name);
            printf("ret %s\n", sym(r));
            if (r) {
                long nb = 0x20 + (long)r->size * 8;
                if (nb < 0x20) nb = 0x20;
                if (nb > 8192) nb = 8192;
                printf("replay [");
                hexout(r, 0x8a8);
                printf("]\n");
                printf("data %s\n", sym(r->data));
                if (r->data) {
                    printf("records [");
                    hexout(r->data, nb);
                    printf("]\n");
                }
            }
            break;
        }
        case K_XML: {
            int nc, nj, k;
            char *s;
            memset(&the_gd, 0, sizeof the_gd);
            rdbytes(&xml_replay, sizeof(Treplay));
            xml_replay.data = 0;
            the_gd.replay = &xml_replay;
            cmdline.jumps = rdi(); cmdline.combos = rdi(); cmdline.sd = rdi();
            cmdline.keys = rdi();  cmdline.tiny = rdi();
            the_gd.score = rdi(); the_gd.floor = rdi(); the_gd.combo = rdi();
            the_gd.no_combo_top_floor = rdi(); the_gd.biggest_lost_combo = rdi();
            for (k = 0; k < 5; k++) the_gd.ccc[k] = rdi();
            for (k = 0; k < 5; k++) the_gd.jc[k] = rdi();
            nc = rdi();
            the_gd.comboPosts = nc;
            for (k = 0; k < nc; k++) {
                the_gd.combos[k].start = rdi();
                the_gd.combos[k].end = rdi();
                the_gd.combos[k].length = rdi();
            }
            nj = rdi();
            the_gd.jumpPosts = nj;
            for (k = 0; k < nj; k++) {
                the_gd.jumps[k].start = rdi();
                the_gd.jumps[k].dist = rdi();
                the_gd.jumps[k].num = rdi();
            }
            the_gd.left = rdi(); the_gd.right = rdi(); the_gd.jump = rdi();
            s = getGameDataXML(&the_gd);
            printf("xml \"");
            pstr(s);
            printf("\"\n");
            printf("xmllen %d\n", (int)strlen(s));
            break;
        }
        case K_RESULTS: {
            char handle[33];
            int yy, showQ, logo8, obj8;
            bmp_logo.w = rdi();
            bmp_logo.h = rdi();
            logo8 = rdi();
            obj8 = rdi();
            bmp_logo.vtable = logo8 ? &vt8 : &vt16;
            for (i = 0; i < NOBJ; i++) objs[i].vtable = obj8 ? &vt8 : &vt16;
            yy = rdi();
            for (i = 0; i < 5; i++) qualified[i] = rdi();
            for (i = 0; i < 5; i++) qvalues[i] = rdi();
            showQ = rdi();
            for (i = 0; i < 15; i++) new_personal_best[i] = rdi();
            rdbytes(handle, 32);
            handle[32] = 0;
            memset(&the_profile, 0, sizeof the_profile);
            memcpy(the_profile.handle, handle, 32);
            draw_results(&bmp_target, &bmp_logo, yy, qualified, qvalues, showQ);
            break;
        }
        case K_ALERT: {
            int fnull, tnull, choice, hint, gnull, obj8, ret;
            int flen, tlen;
            fnull = rdi(); tnull = rdi(); choice = rdi(); hint = rdi();
            gnull = rdi();
            the_gfx.w = rdi(); the_gfx.h = rdi();
            obj8 = rdi();
            gfx_driver = gnull ? (GFX_DRIVER *)0 : &the_gfx;
            for (i = 0; i < NOBJ; i++) objs[i].vtable = obj8 ? &vt8 : &vt16;
            script_cycle_every = rdi();
            script_esc_on = rdi(); script_esc_off = rdi();
            script_enter_on = rdi(); script_enter_off = rdi();
            script_close_at = rdi();
            rdqueue(&q_any_c); rdqueue(&q_any_m);
            rdqueue(&q_left_c); rdqueue(&q_left_m);
            rdqueue(&q_right_c); rdqueue(&q_right_m);
            rdqueue(&q_fire_c); rdqueue(&q_fire_m);
            rdqueue(&q_enter_m);
            flen = rdi();
            if (flen > 127) flen = 127;
            rdbytes(str_func, flen); str_func[flen] = 0;
            tlen = rdi();
            if (tlen > 127) tlen = 127;
            rdbytes(str_txt, tlen); str_txt[tlen] = 0;
            ret = my_alert(fnull ? (char *)0 : str_func,
                           tnull ? (char *)0 : str_txt, choice, hint);
            printf("ret %d\n", ret);
            printf("gui %d %d\n", gui_fg_color, gui_bg_color);
            break;
        }
        default:
            fprintf(stderr, "unknown kind %u\n", kind);
            return 2;
        }
        printf("E\n");
    }
    fclose(vf);
    return 0;
}
