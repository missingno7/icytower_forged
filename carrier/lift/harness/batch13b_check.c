/* batch13b_check.c -- the COMPILED-CANDIDATE half of PROMOTIONS.md
 * batch 13's ordered-call-trace oracle for the six tick-path functions
 * whose comparison domain is "which library calls happened, in what
 * order, with what arguments":
 *
 *   play_sound       (src/icytower/sound.c)
 *   startGameMusic   (src/icytower/sound.c)
 *   stopGameMusic    (src/icytower/sound.c)
 *   log2file         (src/icytower/logfile.c)
 *   take_screenshot  (src/icytower/screenshot.c)
 *   draw_reward      (src/icytower/draw_reward.c)
 *
 * Harness-only; nothing under src/ is changed or seam-ed for it
 * (win32_pilot.md SS7a).  A standalone main(), not a lift_check.py SPECS
 * row, for the reason batch 10 and batch 12 already established: the
 * SPECS call-trace mechanism (mechanism B) records per callee a COUNT
 * plus the arguments of its FIRST call only, and every function here
 * either calls one callee several times with different arguments
 * (take_screenshot's sprintf/exists loop) or is defined entirely by the
 * ORDER of several different callees (stopGameMusic's three
 * independently-guarded teardowns).  It also could not be a SPECS row
 * even in principle: batch 11's blocker for play_sound was that
 * pf_harness_calltrace.h redirects the NAME play_sound to a stub for the
 * whole SPECS build, which would rename play_sound's own definition.
 * This executable never links that harness, so the two builds are
 * disjoint and both redirects stand.
 *
 * Protocol: argv[1] is a vector file, one vector per line, fields
 * separated by '|'.  The trace for each vector is printed between
 * "V <n>" and "E", one call per line, in order, followed by the
 * memory-domain globals that vector's function writes.  Pointers are
 * rendered as stable SYMBOLS (sample/midi/bmp/reward/sub/file) and
 * strings as their CONTENT AT CALL TIME, so the guest and host address
 * spaces never have to agree -- draw_frame_xcheck.py's convention.
 *
 * Build: see build_batch13.sh.
 */
#include "pf_harness_batch13.h"
#undef fopen
#undef vfprintf
#undef vsprintf
#undef fputc
#undef fclose
#undef sprintf

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
/* (Allegro's own globals, declared in allegro_api.h)                  */
/* ------------------------------------------------------------------ */
FONT *font;
volatile char key[127];
int *allegro_errno;
BITMAP *screen;
static int errno_storage;

/* ------------------------------------------------------------------ */
/* trace                                                               */
/* ------------------------------------------------------------------ */
static FILE *out;

static struct { const void *p; const char *name; } syms[16];
static int nsyms;

static void bind_sym(const void *p, const char *name)
{
    if (p && nsyms < 16) { syms[nsyms].p = p; syms[nsyms].name = name; nsyms++; }
}

static const char *sym(const void *p)
{
    int i;
    if (!p) return "NULL";
    for (i = 0; i < nsyms; i++)
        if (syms[i].p == p) return syms[i].name;
    return "?";
}

/* ------------------------------------------------------------------ */
/* the traced callees                                                  */
/* ------------------------------------------------------------------ */
static int  script_rand[64], nscript_rand, iscript_rand;
static int  script_play_sample_ret;
static int  script_exists_hits;      /* exists() returns 1 this many times */
static int  iexists;
static int  script_fopen_ok;
static const char *script_logpath;

/* ---- Allegro audio ---- */
int play_sample(const SAMPLE *s, int vol, int pan, int freq, int loop)
{
    fprintf(out, "play_sample %s %d %d %d %d\n", sym(s), vol, pan, freq, loop);
    return script_play_sample_ret;
}
void set_volume(int digi, int midi)
{
    fprintf(out, "set_volume %d %d\n", digi, midi);
}
int play_midi(MIDI *m, int loop)
{
    fprintf(out, "play_midi %s %d\n", sym(m), loop);
    return 0;
}
void voice_stop(int voice)      { fprintf(out, "voice_stop %d\n", voice); }
void stop_sample(const SAMPLE *s) { fprintf(out, "stop_sample %s\n", sym(s)); }
void stop_midi(void)            { fprintf(out, "stop_midi\n"); }

/* ---- game callee ---- */
int new_rand(void)
{
    int v = nscript_rand ? script_rand[iscript_rand % nscript_rand] : 0;
    iscript_rand++;
    fprintf(out, "new_rand -> %d\n", v);
    return v;
}

/* ---- Allegro file / graphics ---- */
int exists(const char *name)
{
    int r = (iexists < script_exists_hits) ? 1 : 0;
    iexists++;
    fprintf(out, "exists \"%s\" -> %d\n", name, r);
    return r;
}
void get_palette(RGB *p)
{
    (void)p;
    fprintf(out, "get_palette\n");
}
static BITMAP sub_bitmap;
BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int w, int h)
{
    fprintf(out, "create_sub_bitmap %s %d %d %d %d\n", sym(parent), x, y, w, h);
    return &sub_bitmap;
}
int save_bitmap(const char *name, BITMAP *bmp, const RGB *pal)
{
    (void)pal;
    fprintf(out, "save_bitmap \"%s\" %s\n", name, sym(bmp));
    return 0;
}
void destroy_bitmap(BITMAP *bmp)
{
    fprintf(out, "destroy_bitmap %s\n", sym(bmp));
}
void stretch_sprite(BITMAP *dst, BITMAP *src, int x, int y, int w, int h)
{
    fprintf(out, "stretch_sprite %s %s %d %d %d %d\n",
            sym(dst), sym(src), x, y, w, h);
}
static void vt_pivot_scaled_sprite_flip(BITMAP *dst, BITMAP *src,
                                        fixed x, fixed y, fixed cx, fixed cy,
                                        fixed angle, fixed scale, int vflip)
{
    fprintf(out, "pivot_scaled_sprite_flip %s %s %d %d %d %d %d %d %d\n",
            sym(dst), sym(src), (int)x, (int)y, (int)cx, (int)cy,
            (int)angle, (int)scale, vflip);
}

/* ---- game callee (logfile) ---- */
int get_logfile_path(char *buffer, it_orig_size_t buflen)
{
    fprintf(out, "get_logfile_path %d\n", (int)buflen);
    strcpy(buffer, script_logpath);
    return 1;
}

/* ---- CRT / pthreads redirects declared by pf_harness_batch13.h ---- */
static FILE fake_file;

FILE *h_fopen(const char *path, const char *mode)
{
    fprintf(out, "fopen \"%s\" \"%s\" -> %s\n", path, mode,
            script_fopen_ok ? "file" : "NULL");
    return script_fopen_ok ? &fake_file : (FILE *)0;
}
int h_vfprintf(FILE *f, const char *fmt, va_list ap)
{
    char tmp[512];
    int n = vsprintf(tmp, fmt, ap);
    fprintf(out, "vfprintf %s \"%s\" -> \"%s\"\n",
            (f == &fake_file) ? "file" : "?", fmt, tmp);
    return n;
}
int h_vsprintf(char *dst, const char *fmt, va_list ap)
{
    int n = vsprintf(dst, fmt, ap);
    fprintf(out, "vsprintf %s \"%s\"\n",
            (dst == last_log) ? "last_log" : "?", fmt);
    return n;
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
int h_sprintf(char *dst, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsprintf(dst, fmt, ap);
    va_end(ap);
    fprintf(out, "sprintf \"%s\" -> \"%s\"\n", fmt, dst);
    return n;
}
int h_mutex_lock(void *m)
{
    fprintf(out, "mutex_lock %s\n",
            (m == (void *)&sLogMutex__log2file) ? "sLogMutex" : "?");
    return 0;
}
int h_mutex_unlock(void *m)
{
    fprintf(out, "mutex_unlock %s\n",
            (m == (void *)&sLogMutex__log2file) ? "sLogMutex" : "?");
    return 0;
}

/* ------------------------------------------------------------------ */
/* the world                                                           */
/* ------------------------------------------------------------------ */
static SAMPLE snd_sample, snd_bgmusic, snd_bgbeat;
static MIDI   midi_bgmidi;
static Tplayer the_player;
static BITMAP  dest_bmp, reward_bitmap;
static GFX_VTABLE vt;

static void world_init(void)
{
    nsyms = 0;
    memset(&vt, 0, sizeof vt);
    vt.pivot_scaled_sprite_flip = vt_pivot_scaled_sprite_flip;
    dest_bmp.vtable = &vt;
    reward_bitmap.vtable = &vt;
    sub_bitmap.vtable = &vt;
    bind_sym(&snd_sample, "sample");
    bind_sym(&snd_bgmusic, "bg_music");
    bind_sym(&snd_bgbeat, "bg_beat");
    bind_sym(&midi_bgmidi, "bg_midi");
    bind_sym(&dest_bmp, "bmp");
    bind_sym(&reward_bitmap, "reward");
    bind_sym(&sub_bitmap, "sub");
}

/* ------------------------------------------------------------------ */
/* vector parsing                                                      */
/* ------------------------------------------------------------------ */
#define MAXF 24
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

static double f_double(const char *hex)
{
    unsigned long long bits = strtoull(hex, (char **)0, 16);
    double d;
    memcpy(&d, &bits, 8);
    return d;
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    FILE *vf;
    char line[1024];
    int n = 0;

    allegro_errno = &errno_storage;
    out = stdout;
    if (argc < 2) { fprintf(stderr, "usage: batch13b_check <vectors>\n"); return 2; }
    vf = fopen(argv[1], "rb");
    if (!vf) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    while (fgets(line, sizeof line, vf)) {
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = 0;
        if (!line[0]) continue;
        split(line);
        world_init();
        iscript_rand = iexists = 0;
        printf("V %d\n", n++);

        if (!strcmp(fld[0], "play_sound")) {
            int s_null;
            itrcheck          = fi(1);
            options.snd_volume = fi(2);
            fast_forward      = fi(3);
            fast_fast_forward = fi(4);
            player_id         = fi(5);
            the_player.x      = f_double(fld[6]);
            ply[player_id]    = &the_player;
            s_null            = fi(7);
            any11             = fi(10);
            nscript_rand = 1; script_rand[0] = fi(11);
            play_sound(s_null ? (SAMPLE *)0 : &snd_sample, fi(8), fi(9));
            printf("any11 %d\n", any11);

        } else if (!strcmp(fld[0], "startGameMusic")) {
            options.msc_volume = fi(1);
            custom.bg_music = fi(2) ? &snd_bgmusic : (SAMPLE *)0;
            custom.bg_midi  = fi(3) ? &midi_bgmidi : (MIDI *)0;
            bg_beat         = fi(4) ? &snd_bgbeat  : (SAMPLE *)0;
            gameMusicVoiceID = fi(5);
            script_play_sample_ret = fi(6);
            startGameMusic();
            printf("gameMusicVoiceID %d\n", gameMusicVoiceID);

        } else if (!strcmp(fld[0], "stopGameMusic")) {
            gameMusicVoiceID = fi(1);
            custom.bg_music = fi(2) ? &snd_bgmusic : (SAMPLE *)0;
            custom.bg_midi  = fi(3) ? &midi_bgmidi : (MIDI *)0;
            stopGameMusic();
            printf("gameMusicVoiceID %d\n", gameMusicVoiceID);

        } else if (!strcmp(fld[0], "log2file")) {
            /* fld: 1 itrcheck | 2 logname-empty | 3 fopen-ok | 4 logpath |
             *      5 fmt | 6 kind ('s' or 'd' or '-') | 7 arg1 | 8 arg2 */
            itrcheck = fi(1);
            script_fopen_ok = fi(3);
            script_logpath = fld[4];
            if (fi(2)) logfilename__log2file[0] = 0;
            else       strcpy(logfilename__log2file, "already/resolved.txt");
            memset(last_log, 0, sizeof last_log);
            switch (fld[6][0]) {
            case 's': log2file(fld[5], fld[7]); break;
            case 'd': log2file(fld[5], fi(7)); break;
            case 'S': log2file(fld[5], fld[7], fld[8]); break;
            case 'D': log2file(fld[5], fi(7), fi(8)); break;
            case 'M': log2file(fld[5], fld[7], fi(8)); break;
            default:  log2file(fld[5]); break;
            }
            printf("last_log \"%s\"\n", last_log);

        } else if (!strcmp(fld[0], "take_screenshot")) {
            number__take_screenshot = fi(1);
            dest_bmp.w = fi(2);
            dest_bmp.h = fi(3);
            script_exists_hits = fi(4);
            /* the >9999 arm calls log2file(), so this vector kind has to
             * pin log2file's own inputs too -- the Python side writes the
             * identical three into the guest. */
            itrcheck = 0;
            script_fopen_ok = 0;
            script_logpath = "logs/it.txt";
            logfilename__log2file[0] = 0;
            key[KEY_F1] = 0;
            take_screenshot(&dest_bmp);
            printf("number__take_screenshot %d\n", number__take_screenshot);

        } else if (!strcmp(fld[0], "draw_reward")) {
            options.flash = fi(1);
            reward_scale  = (fixed)strtol(fld[2], (char **)0, 0);
            reward_bitmap.w = fi(3);
            reward_bitmap.h = fi(4);
            reward_bmp = &reward_bitmap;
            draw_reward(&dest_bmp);

        } else {
            fprintf(stderr, "unknown vector kind %s\n", fld[0]);
            return 2;
        }
        printf("E\n");
    }
    fclose(vf);
    return 0;
}
