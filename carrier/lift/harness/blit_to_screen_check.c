/* blit_to_screen_check.c -- the COMPILED-CANDIDATE half of the
 * blit_to_screen ordered-call-trace cross-check (PROMOTIONS.md batch 11).
 *
 * Harness-only; nothing under src/ is changed or seam-ed for it
 * (win32_pilot.md SS7a).  A standalone `main()`, not part of gcc_check.c's
 * SPECS-driven vector protocol, for the same reason draw_frame_check.c is
 * (PROMOTIONS.md batch 10): blit_mode 3 issues 480 `blit` calls whose
 * arguments all differ, and lift_check.py's shared call-trace mechanism
 * records per callee only a COUNT plus the arguments of its FIRST call --
 * it would compare 1/480th of that mode.  So this driver:
 *
 *   1. reads a vector file (magic "BTS1") produced by the Python side --
 *      Allegro's _cos_tbl[512] once, then per vector: the debug flag, the
 *      seven F-key states, the incoming blit_mode, logic_count, the
 *      player's level/x/y, the bitmap's w/h, and whether the screen's
 *      vtable carries an acquire/release hook at all;
 *   2. builds that state in HOST memory (the standalone world --
 *      src/icytower/state.c supplies the game globals, this file supplies
 *      Allegro's own);
 *   3. calls src/icytower/blit_to_screen.c's blit_to_screen() once per
 *      vector against stub entry points and two stub GFX_VTABLEs, each of
 *      which appends one line to an ordered text trace;
 *   4. prints that trace plus blit_mode, the one memory-domain global.
 *
 * The Python side (blit_to_screen_xcheck.py) runs the ORIGINAL bytes at
 * 0x40b6bc in unicorn over the SAME state with the same callees hooked,
 * renders its trace in the same textual form, and diffs the two.  The two
 * BITMAPs are rendered as the stable symbols "bmp" and "screen" so the two
 * address spaces never have to agree.
 *
 * _cos_tbl arrives in the vector file rather than being linked from a real
 * Allegro: blit_to_screen.c's `fixsin` stand-in declares it as an ordinary
 * extern (that global has no binding in any generated header -- see that
 * file's own comment), and shipping the ORIGINAL image's own 512 entries
 * here means the wobble mode is checked against the real table, not a
 * re-derived one.
 *
 * Build (32-bit MinGW GCC, x87 -- win32_pilot.md SS6a):
 *   gcc -m32 -mfpmath=387 -mno-sse2 -O2 -Wall -Isrc/icytower \
 *       -I<port_forge>/tools/win32_oracle \
 *       -include pf_harness_msvc_types.h \
 *       carrier/lift/harness/blit_to_screen_check.c \
 *       src/icytower/blit_to_screen.c src/icytower/state.c \
 *       -o blit_to_screen_check.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro_api.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"     /* blit_to_screen's own prototype */

/* ---- storage the standalone world needs but state.c does not supply -- */
volatile char key[127];
BITMAP *screen;
fixed _cos_tbl[512];        /* see the header comment */

static FILE *out;
static BITMAP bmp_obj, screen_obj;
static GFX_VTABLE bmp_vt, screen_vt;
static Tplayer player;

static void p_arg(const void *p)
{
    if (p == (const void *)&bmp_obj) fputs("|bmp", out);
    else if (p == (const void *)&screen_obj) fputs("|screen", out);
    else fprintf(out, "|?%p", p);
}
static void i_arg(int v) { fprintf(out, "|%d", v); }

/* ---- stub Allegro entry points ------------------------------------- */
void blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w, int h)
{
    fputs("blit", out); p_arg(src); p_arg(dst);
    i_arg(sx); i_arg(sy); i_arg(dx); i_arg(dy); i_arg(w); i_arg(h);
    fputc('\n', out);
}

void stretch_blit(BITMAP *src, BITMAP *dst, int sx, int sy, int sw, int sh,
                  int dx, int dy, int dw, int dh)
{
    fputs("stretch_blit", out); p_arg(src); p_arg(dst);
    i_arg(sx); i_arg(sy); i_arg(sw); i_arg(sh);
    i_arg(dx); i_arg(dy); i_arg(dw); i_arg(dh);
    fputc('\n', out);
}

/* ---- GFX_VTABLE slots blit_to_screen reaches through AL_INLINEs ----- */
static void vt_acquire(BITMAP *b) { fputs("acquire", out); p_arg(b); fputc('\n', out); }
static void vt_release(BITMAP *b) { fputs("release", out); p_arg(b); fputc('\n', out); }

static void vt_v_flip(BITMAP *b, BITMAP *s, int x, int y)
{
    fputs("draw_sprite_v_flip", out); p_arg(b); p_arg(s);
    i_arg(x); i_arg(y); fputc('\n', out);
}
static void vt_h_flip(BITMAP *b, BITMAP *s, int x, int y)
{
    fputs("draw_sprite_h_flip", out); p_arg(b); p_arg(s);
    i_arg(x); i_arg(y); fputc('\n', out);
}
static void vt_line(BITMAP *b, int x1, int y1, int x2, int y2, int c)
{
    fputs("line", out); p_arg(b);
    i_arg(x1); i_arg(y1); i_arg(x2); i_arg(y2); i_arg(c); fputc('\n', out);
}

/* ---- vector file reader -------------------------------------------- */
static FILE *vf;

static int ri(void)
{
    int v = 0;
    if (fread(&v, 4, 1, vf) != 1) { fprintf(stderr, "short read\n"); exit(2); }
    return v;
}

static double rd(void)
{
    double v = 0;
    if (fread(&v, 8, 1, vf) != 1) { fprintf(stderr, "short read\n"); exit(2); }
    return v;
}

int main(int argc, char **argv)
{
    unsigned int magic;
    int nvec, v, i;

    if (argc < 3) {
        fprintf(stderr, "usage: blit_to_screen_check <vectors.bin> <trace.txt>\n");
        return 2;
    }
    vf = fopen(argv[1], "rb");
    if (!vf) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    out = fopen(argv[2], "w");
    if (!out) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }

    if (fread(&magic, 4, 1, vf) != 1 || magic != 0x31535442u) {
        fprintf(stderr, "bad magic\n");
        return 2;
    }
    nvec = ri();
    for (i = 0; i < 512; i++)
        _cos_tbl[i] = (fixed)ri();

    for (v = 0; v < nvec; v++) {
        int has_acquire, has_release;

        memset(&bmp_vt, 0, sizeof(bmp_vt));
        memset(&screen_vt, 0, sizeof(screen_vt));
        memset(&bmp_obj, 0, sizeof(bmp_obj));
        memset(&screen_obj, 0, sizeof(screen_obj));
        memset((void *)key, 0, sizeof(key));

        debug = ri();
        blit_mode__blit_to_screen = ri();
        logic_count = ri();
        player.level = ri();
        bmp_obj.w = ri();
        bmp_obj.h = ri();
        for (i = 0; i < 7; i++)
            key[KEY_F2 + i] = (char)ri();
        has_acquire = ri();
        has_release = ri();
        player.x = rd();
        player.y = rd();

        player_id = 0;
        ply[0] = &player;

        bmp_vt.line = vt_line;
        screen_vt.acquire = has_acquire ? vt_acquire : 0;
        screen_vt.release = has_release ? vt_release : 0;
        screen_vt.draw_sprite_v_flip = vt_v_flip;
        screen_vt.draw_sprite_h_flip = vt_h_flip;
        bmp_obj.vtable = &bmp_vt;
        screen_obj.vtable = &screen_vt;
        screen_obj.w = 640;
        screen_obj.h = 480;
        screen = &screen_obj;

        fprintf(out, "#vec %d\n", v);
        blit_to_screen(&bmp_obj);
        fprintf(out, "#dom|%d\n", blit_mode__blit_to_screen);
    }
    fclose(out);
    fclose(vf);
    return 0;
}
