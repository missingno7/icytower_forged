/* draw_frame_check.c -- the COMPILED-CANDIDATE half of the draw_frame
 * ordered-call-trace cross-check (PROMOTIONS.md batch 10).
 *
 * Harness-only; nothing under src/ is changed or seam-ed for it
 * (win32_pilot.md SS7a).  It is a standalone `main()`, not part of
 * gcc_check.c's SPECS-driven vector protocol, for one reason that
 * protocol cannot express: draw_frame's comparison domain is not a fixed
 * byte range but an ORDERED SEQUENCE of ~250 library calls per
 * invocation, and lift_check.py's call-trace mechanism records per callee
 * only a count plus the arguments of its FIRST call (PROMOTIONS.md batch
 * 8's documented limitation).  So this driver:
 *
 *   1. reads a vector file (magic "DFV1") produced by the Python side --
 *      one whole seeded game state per vector: profile, map.room[32],
 *      Tplayer, Tparticle stars[512], every referenced BITMAP's w/h/
 *      colour depth, the Treplay and its strings, the three menu
 *      captions, the scripted new_rand() sequence;
 *   2. builds that state in HOST memory (no guest addresses anywhere --
 *      this is the standalone world, src/icytower/state.c supplies the
 *      globals);
 *   3. calls src/icytower/draw_frame.c's draw_frame() once per vector
 *      against stub Allegro entry points and a stub GFX_VTABLE, each of
 *      which appends one line to an ordered text trace;
 *   4. prints that trace plus the memory-domain globals draw_frame writes.
 *
 * The Python side runs the ORIGINAL bytes at 0x40929c in unicorn over the
 * SAME state with the same stubs hooked, renders its trace in the same
 * textual form, and diffs the two.  Pointers are rendered as stable
 * symbols (data[N] / custom.frame[i] / bmp / swap_screen / font / buf#k)
 * so the two address spaces never have to agree, exactly the problem
 * call_trace_stubs.c's pf_untranslate() solves for the fixed-slot domain.
 *
 * Build (32-bit MinGW GCC, x87 -- win32_pilot.md SS6a):
 *   gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Wall -Isrc/icytower \
 *       -I<port_forge>/tools/win32_oracle \
 *       -include pf_harness_msvc_types.h \
 *       carrier/lift/harness/draw_frame_check.c \
 *       src/icytower/draw_frame.c src/icytower/control.c \
 *       src/icytower/state.c -o draw_frame_check.exe
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro_api.h"
#include "assets.h"
#include "assets_table.inc"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

#define MAXBMP 256

/* ------------------------------------------------------------------ */
/* storage the standalone world needs but state.c does not supply       */
/* (these are Allegro's own globals, declared in allegro_api.h)         */
/* ------------------------------------------------------------------ */
FONT *font;
volatile char key[127];
int *allegro_errno;
BITMAP *screen;
static int errno_storage;

/* ------------------------------------------------------------------ */
/* trace                                                                */
/* ------------------------------------------------------------------ */
static FILE *out;
static void *buf_syms[16];
static int nbuf_syms;

static BITMAP bmps[MAXBMP];         /* index == datafile object index    */
static BITMAP cframes[15];
static BITMAP font_bmp;             /* stand-in for Allegro's `font`     */
static BITMAP dest_bmp, swap_bmp;
static GFX_VTABLE vt16, vt8;

static void sym(const void *p)
{
    int i;
    if (p == (void *)&dest_bmp) { fputs("bmp", out); return; }
    if (p == (void *)&swap_bmp) { fputs("swap_screen", out); return; }
    if (p == (void *)&font_bmp) { fputs("font", out); return; }
    for (i = 0; i < MAXBMP; i++)
        if (p == (void *)&bmps[i]) { fprintf(out, "data[%d]", i); return; }
    for (i = 0; i < 15; i++)
        if (p == (void *)&cframes[i]) { fprintf(out, "custom.frame[%d]", i); return; }
    for (i = 0; i < nbuf_syms; i++)
        if (p == buf_syms[i]) { fprintf(out, "buf#%d", i); return; }
    buf_syms[nbuf_syms] = (void *)p;
    fprintf(out, "buf#%d", nbuf_syms);
    nbuf_syms++;
}

static void s_arg(const char *s) { fprintf(out, "|s:%s", s ? s : ""); }
static void p_arg(const void *p) { fputc('|', out); sym(p); }
static void i_arg(int v) { fprintf(out, "|%d", v); }

/* ------------------------------------------------------------------ */
/* stub Allegro / game entry points                                     */
/* ------------------------------------------------------------------ */
void blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w, int h)
{
    fputs("blit", out); p_arg(src); p_arg(dst);
    i_arg(sx); i_arg(sy); i_arg(dx); i_arg(dy); i_arg(w); i_arg(h);
    fputc('\n', out);
}

int makecol(int r, int g, int b)
{
    fputs("makecol", out); i_arg(r); i_arg(g); i_arg(b); fputc('\n', out);
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
}

int text_length(const FONT *f, const char *s)
{
    fputs("text_length", out); p_arg(f); s_arg(s); fputc('\n', out);
    return 7 * (int)strlen(s) + 1;
}

void textout_ex(BITMAP *bmp, const FONT *f, const char *s,
                int x, int y, int color, int bg)
{
    fputs("textout_ex", out); p_arg(bmp); p_arg(f); s_arg(s);
    i_arg(x); i_arg(y); i_arg(color); i_arg(bg); fputc('\n', out);
}

void set_clip_rect(BITMAP *bmp, int x1, int y1, int x2, int y2)
{
    fputs("set_clip_rect", out); p_arg(bmp);
    i_arg(x1); i_arg(y1); i_arg(x2); i_arg(y2); fputc('\n', out);
}

/* The printf family: draw_frame only ever uses %d and one %1.2f, so the
 * varargs are logged as the raw dwords the ORIGINAL's own stack carries
 * (a double contributes two) -- the same convention the Python side's
 * vararg_dwords() applies to the unicorn trace. */
static void printf_family(const char *who, BITMAP *bmp, const FONT *f,
                          int x, int y, int color, int bg,
                          const char *fmt, va_list ap)
{
    const char *p;
    fputs(who, out); p_arg(bmp); p_arg(f);
    i_arg(x); i_arg(y); i_arg(color); i_arg(bg); s_arg(fmt);
    for (p = fmt; *p; p++) {
        if (*p != '%')
            continue;
        p++;
        while (*p && !strchr("diouxXeEfgGcsp%", *p))
            p++;
        if (!*p)
            break;
        if (*p == '%')
            continue;
        if (strchr("eEfgG", *p)) {
            union { double d; int w[2]; } u;
            u.d = va_arg(ap, double);
            i_arg(u.w[0]); i_arg(u.w[1]);
        } else if (*p == 's') {
            (void)va_arg(ap, const char *);
        } else {
            i_arg(va_arg(ap, int));
        }
    }
    fputc('\n', out);
}

void textprintf_ex(BITMAP *bmp, const FONT *f, int x, int y, int color,
                   int bg, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf_family("textprintf_ex", bmp, f, x, y, color, bg, fmt, ap);
    va_end(ap);
}

void textprintf_centre_ex(BITMAP *bmp, const FONT *f, int x, int y, int color,
                          int bg, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf_family("textprintf_centre_ex", bmp, f, x, y, color, bg, fmt, ap);
    va_end(ap);
}

void draw_reward(BITMAP *bmp)
{
    fputs("draw_reward", out); p_arg(bmp); fputc('\n', out);
}

static int rand_seq[512], rand_n, rand_i;

int new_rand(void)
{
    fputs("new_rand", out); fputc('\n', out);
    return rand_seq[(rand_i++) % rand_n];
}

/* ---- GFX_VTABLE slots draw_frame reaches through AL_INLINEs -------- */
static void vt_draw_sprite(BITMAP *bmp, BITMAP *s, int x, int y)
{
    fputs("draw_sprite", out); p_arg(bmp); p_arg(s); i_arg(x); i_arg(y);
    fputc('\n', out);
}
static void vt_draw_256_sprite(BITMAP *bmp, BITMAP *s, int x, int y)
{
    fputs("draw_256_sprite", out); p_arg(bmp); p_arg(s); i_arg(x); i_arg(y);
    fputc('\n', out);
}
static void vt_draw_sprite_h_flip(BITMAP *bmp, BITMAP *s, int x, int y)
{
    fputs("draw_sprite_h_flip", out); p_arg(bmp); p_arg(s); i_arg(x); i_arg(y);
    fputc('\n', out);
}
static void vt_pivot(BITMAP *bmp, BITMAP *s, fixed x, fixed y,
                     fixed cx, fixed cy, fixed angle, fixed scale, int v_flip)
{
    fputs("pivot_scaled_sprite_flip", out); p_arg(bmp); p_arg(s);
    i_arg(x); i_arg(y); i_arg(cx); i_arg(cy); i_arg(angle); i_arg(scale);
    i_arg(v_flip); fputc('\n', out);
}
static void vt_rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int c)
{
    fputs("rect", out); p_arg(bmp);
    i_arg(x1); i_arg(y1); i_arg(x2); i_arg(y2); i_arg(c); fputc('\n', out);
}

/* ---- the asset seam, driver-supplied ------------------------------ */
BITMAP *asset_bitmap(asset_id id) { return &bmps[asset_table[id].index]; }
FONT *asset_font(asset_id id) { return (FONT *)&bmps[asset_table[id].index]; }
SAMPLE *asset_sample(asset_id id) { (void)id; return 0; }
PALETTE *asset_palette(asset_id id) { (void)id; return 0; }
void *asset_object(asset_id id) { return &bmps[asset_table[id].index]; }

/* ------------------------------------------------------------------ */
/* vector file reader                                                   */
/* ------------------------------------------------------------------ */
static FILE *vf;

static int ri(void)
{
    int v;
    if (fread(&v, 4, 1, vf) != 1) { fprintf(stderr, "short vector file\n"); exit(2); }
    return v;
}

static double rd(void)
{
    double v;
    if (fread(&v, 8, 1, vf) != 1) { fprintf(stderr, "short vector file\n"); exit(2); }
    return v;
}

static void rs(char *dst, int cap)
{
    int n = ri(), i;
    for (i = 0; i < n; i++) {
        int c = fgetc(vf);
        if (i < cap - 1)
            dst[i] = (char)c;
    }
    dst[(n < cap - 1) ? n : cap - 1] = 0;
}

static void mkbmp(BITMAP *b, int w, int h, int depth)
{
    memset(b, 0, sizeof(*b));
    b->w = w;
    b->h = h;
    b->vtable = (depth == 8) ? &vt8 : &vt16;
}

static Tplayer player;
static Tprofile prof;
static Treplay replay;
static Trecord records[512];
static char cap_floor[64], cap_speed[64], cap_grav[64];

int main(int argc, char **argv)
{
    int nvec, v, i, k;

    if (argc < 3) {
        fprintf(stderr, "usage: draw_frame_check <vectors.bin> <trace.txt>\n");
        return 2;
    }
    vf = fopen(argv[1], "rb");
    if (!vf) { perror(argv[1]); return 2; }
    out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 2; }

    memset(&vt16, 0, sizeof(vt16));
    memset(&vt8, 0, sizeof(vt8));
    vt16.color_depth = 16;
    vt8.color_depth = 8;
    vt16.draw_sprite = vt8.draw_sprite = vt_draw_sprite;
    vt16.draw_256_sprite = vt8.draw_256_sprite = vt_draw_256_sprite;
    vt16.draw_sprite_h_flip = vt8.draw_sprite_h_flip = vt_draw_sprite_h_flip;
    vt16.pivot_scaled_sprite_flip = vt8.pivot_scaled_sprite_flip = vt_pivot;
    vt16.rect = vt8.rect = vt_rect;

    allegro_errno = &errno_storage;

    if (ri() != 0x31564644) { fprintf(stderr, "bad magic\n"); return 2; }
    nvec = ri();

    for (v = 0; v < nvec; v++) {
        nbuf_syms = 0;
        errno_storage = 0;
        rand_i = 0;

        prof.start_floor = ri();
        profile = &prof;
        player.level = ri();
        map.offset = ri();
        last_stripe_y = ri();
        for (i = 0; i < 5; i++)
            bg_stripe_ids[i] = ri();
        hurry_y = ri();
        options.flash = ri();
        options.jump_hold = ri();
        logic_count = ri();
        frame_count = ri();
        fps = ri();
        lps = ri();
        debug = ri();
        key[KEY_F2] = (char)ri();
        reward_time = ri();
        clock_angle = ri();
        recording = ri();
        is_playing_custom_game = ri();
        rec_pos = ri();
        replay.size = ri();
        scroll_count = ri();
        scroll_delay = ri();
        ctrl.flags = (unsigned char)ri();
        any11 = ri(); any12 = ri(); any13 = ri();
        any21 = ri(); any22 = ri(); any23 = ri();

        for (k = 0; k < 32; k++) {
            map.room[k].empty = ri();
            map.room[k].start_tile = ri();
            map.room[k].end_tile = ri();
            map.room[k].level = ri();
            map.room[k].sign = ri();
            map.room[k].tiles = ri();
        }

        player.x = rd(); player.y = rd(); player.sx = rd(); player.sy = rd();
        player.score = ri();
        player.status = ri();
        player.frame = ri();
        player.in_combo = ri();
        player.acc_level = ri();
        player.dead = ri();
        player.rotate = ri();
        player.angle = ri();
        player.edge = ri();
        player.latest_combo = ri();
        player_id = 0;
        ply[0] = &player;

        for (i = 0; i < 512; i++) {
            stars[i].intensity = ri();
            stars[i].x = ri();
            stars[i].y = ri();
            stars[i].color = ri();
        }

        k = ri();
        memset(bmps, 0, sizeof(bmps));
        for (i = 0; i < k; i++) {
            int idx = ri(), w = ri(), h = ri(), depth = ri();
            mkbmp(&bmps[idx], w, h, depth);
        }
        for (i = 0; i < 15; i++) {
            int w = ri(), h = ri(), depth = ri();
            mkbmp(&cframes[i], w, h, depth);
            custom.frame[i] = &cframes[i];
        }
        mkbmp(&font_bmp, 0, 0, 16);
        font = (FONT *)&font_bmp;
        mkbmp(&dest_bmp, 640, 480, 16);
        mkbmp(&swap_bmp, 640, 480, 16);
        swap_screen = &swap_bmp;

        rs(replay.name, sizeof(replay.name));
        rs(replay.comment, sizeof(replay.comment));
        rs(cap_floor, sizeof(cap_floor));
        rs(cap_speed, sizeof(cap_speed));
        rs(cap_grav, sizeof(cap_grav));
        floor_size_selection.caption[0] = cap_floor;
        scroll_speed_selection.caption[1] = cap_speed;
        gravity_selection.caption[2] = cap_grav;
        replay.floor_size = 0;
        replay.start_speed = 1;
        replay.gravity = 2;
        memset(records, 0, sizeof(records));
        i = ri();                       /* rec_key   */
        k = ri();                       /* rec_cycle */
        records[rec_pos % 512].key_flags = (unsigned char)i;
        records[rec_pos % 512].cycle_count = k;
        replay.data = records;
        demo = &replay;

        rand_n = ri();
        for (i = 0; i < rand_n; i++)
            rand_seq[i] = ri();

        fprintf(out, "#vec %d\n", v);
        draw_frame(&dest_bmp);
        fprintf(out, "#dom|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d\n",
                (int)frame_count, last_stripe_y,
                bg_stripe_ids[0], bg_stripe_ids[1], bg_stripe_ids[2],
                bg_stripe_ids[3], bg_stripe_ids[4],
                player.frame, scroll_count, scroll_delay, errno_storage);
    }
    fclose(out);
    fclose(vf);
    return 0;
}
