/* play_check.c -- the compiled side of play_xcheck.py.  PROMOTIONS.md
 * batch 12.
 *
 * play() is ONE function that never returns until the game is over, and
 * whose body reaches ~45 game/Allegro/CRT callees, so it has no offline
 * comparison domain as a whole (see play.c's header, and PROMOTIONS.md
 * batch 11's "why there is no play_tick.c").  What CAN be checked
 * offline are the three regions that src/icytower/play.c recovers as
 * self-contained `static` helpers and that the original emits as
 * straight-line, call-free (or vtable-only) code:
 *
 *   R1  clear_replay_telemetry()  main.c 3473-3480, 0x411a61-0x411ab0
 *   R2  draw_pause_curtain()      main.c 4122-4124, 0x412d55-0x412db6
 *   R3  collect_game_data()       main.c 4389-4418, 0x4137ab-0x4138ee
 *
 * This file #includes src/icytower/play.c verbatim -- so the source
 * under test is byte-identical to the one the three compile worlds
 * build -- and supplies storage plus inert stubs for everything else
 * play() names, since a translation unit that contains play() must link
 * against all of it even when only the three helpers are called.
 *
 * Output is one line per vector on stdout: an ordered call trace for R2,
 * a domain digest for R1/R3.  play_xcheck.py compares that against the
 * ORIGINAL bytes run under unicorn from the same seeded state.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro_api.h"
#include "game_types.h"

/* ---- Allegro globals the generated headers declare but do not define */
BITMAP *screen;
GFX_DRIVER *gfx_driver;
volatile char key[127];
fixed _cos_tbl[512];

/* ---- the source under test, verbatim -------------------------------- */
#include "play.c"

/* ---- ordered call trace (R2's vtable slots) -------------------------- */
#define TRACE_MAX 4096
static char trace_buf[TRACE_MAX][64];
static int trace_n;

static void trace_add(const char *fmt, int a, int b, int c, int d)
{
    if (trace_n < TRACE_MAX)
        sprintf(trace_buf[trace_n], fmt, a, b, c, d);
    trace_n++;
}

static void tr_vline(BITMAP *bmp, int x, int y1, int y2, int color)
{
    (void)bmp;
    trace_add("vline|bmp|%d|%d|%d|%d", x, y1, y2, color);
}

static void tr_hline(BITMAP *bmp, int x1, int y, int x2, int color)
{
    (void)bmp;
    trace_add("hline|bmp|%d|%d|%d|%d", x1, y, x2, color);
}

/* ---- FNV-1a 64, the digest both sides compute ------------------------ */
static unsigned long long fnv(const void *p, int n)
{
    const unsigned char *b = (const unsigned char *)p;
    unsigned long long h = 14695981039346656037ULL;
    int i;
    for (i = 0; i < n; i++) {
        h ^= b[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* ---- the seeded world ------------------------------------------------ */
static Treplay g_replay;
static Trecord g_records[4096];
static Tgame_data g_gd;
static Tplayer g_player;
static Tplayer *g_ply0;
static BITMAP g_bmp;
static GFX_VTABLE g_vt;

static unsigned char *vec;
static long vec_n;
static long vp;

static int rd_i32(void)
{
    int v;
    memcpy(&v, vec + vp, 4);
    vp += 4;
    return v;
}

int main(int argc, char **argv)
{
    FILE *f;
    int nvec, region, i, j;

    if (argc < 2) {
        fprintf(stderr, "usage: play_check <vectors.bin>\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    vec_n = ftell(f);
    fseek(f, 0, SEEK_SET);
    vec = (unsigned char *)malloc(vec_n);
    if (fread(vec, 1, vec_n, f) != (size_t)vec_n) {
        fprintf(stderr, "short read\n");
        return 2;
    }
    fclose(f);

    vp = 0;
    if (rd_i32() != 0x31594C50) {              /* "PLY1" */
        fprintf(stderr, "bad magic\n");
        return 2;
    }
    region = rd_i32();
    nvec = rd_i32();

    g_ply0 = &g_player;
    ply[0] = &g_player;
    player_id = 0;
    demo = &g_replay;
    gameData = &g_gd;
    g_replay.data = g_records;

    g_bmp.vtable = &g_vt;
    g_vt.vline = tr_vline;
    g_vt.hline = tr_hline;
    swap_screen = &g_bmp;

    for (i = 0; i < nvec; i++) {
        trace_n = 0;

        if (region == 1) {
            /* R1: seed tc_posts and the five telemetry channels */
            g_replay.tc_posts = rd_i32();
            for (j = 0; j < 100; j++) {
                g_replay.tc_c_data[j] = (float)rd_i32();
                g_replay.tc_q_data[j] = (float)rd_i32();
                g_replay.tc_t_data[j] = (float)rd_i32();
                g_replay.tc_s_data[j] = (float)rd_i32();
                g_replay.tc_f_data[j] = (float)rd_i32();
            }
            clear_replay_telemetry();
            printf("%d|%016llx|%016llx|%016llx|%016llx|%016llx\n",
                   g_replay.tc_posts,
                   fnv(g_replay.tc_c_data, 400), fnv(g_replay.tc_q_data, 400),
                   fnv(g_replay.tc_t_data, 400), fnv(g_replay.tc_s_data, 400),
                   fnv(g_replay.tc_f_data, 400));

        } else if (region == 2) {
            /* R2: the pause curtain -- 640 ordered vtable calls */
            (void)rd_i32();                      /* vector id, unused */
            draw_pause_curtain(swap_screen);
            printf("%d", trace_n);
            for (j = 0; j < trace_n && j < TRACE_MAX; j++)
                printf(";%s", trace_buf[j]);
            printf("\n");

        } else if (region == 3) {
            /* R3: the end-of-game census */
            memset(&g_gd, 0, sizeof(g_gd));
            memset(&g_player, 0, sizeof(g_player));
            g_player.level = rd_i32();
            g_player.score = rd_i32();
            g_player.best_combo = rd_i32();
            g_player.no_combo_top_floor = rd_i32();
            g_player.biggest_lost_combo = rd_i32();
            for (j = 0; j < 5; j++)
                g_player.ccc[j] = rd_i32();
            for (j = 0; j < 5; j++)
                g_player.jcTop[j] = rd_i32();
            g_replay.size = rd_i32();
            for (j = 0; j < 4096; j++)
                g_records[j].key_flags = 0;
            /* 320 slots always, whatever `size` says -- see play_xcheck.py */
            for (j = 0; j < 320; j++)
                g_records[j].key_flags = (unsigned char)rd_i32();
            collect_game_data();
            printf("%d|%d|%d|%d|%d|", g_gd.score, g_gd.floor, g_gd.combo,
                   g_gd.no_combo_top_floor, g_gd.biggest_lost_combo);
            for (j = 0; j < 5; j++)
                printf("%d,", g_gd.ccc[j]);
            for (j = 0; j < 5; j++)
                printf("%d,", g_gd.jc[j]);
            printf("|%d|%d|%d\n", g_gd.left, g_gd.right, g_gd.jump);
        }
    }
    return 0;
}
