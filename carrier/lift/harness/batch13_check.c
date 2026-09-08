/* batch13_check.c -- compiled-candidate side for batch13_check.py
 * (PROMOTIONS.md batch 13: get_version_str, syncProfileFromOptions,
 * destroy_game_data). Standalone world (game_types.h/game_state.h, no
 * carrier bindings) -- links src/icytower/main_state.c,
 * src/icytower/state.c (the generated standalone global storage), and
 * src/icytower/destroy_game_data.c (the last compiled with
 * -Dfree=harness_capture_free so its one call is captured rather than
 * actually freeing an unallocated pointer -- see destroy_game_data.c's
 * own header comment).
 *
 * One-shot CLI, run fresh per vector by batch13_check.py (this project's
 * existing convention for a compiled-candidate side that is cheap to
 * start, e.g. get_gamepad-style trivial functions; not the persistent
 * lift_check.exe / gcc_check_*.exe wire-protocol harness the SPECS-table
 * functions use, since none of these three has a big enough vector count
 * or setup cost to need one):
 *
 *   batch13_check.exe get_version_str
 *       -> prints the returned string
 *   batch13_check.exe sync_profile <flash> <jump_hold> <msc_volume> \
 *                                  <snd_volume> <profile_marker_hex>
 *       -> seeds *profile with the marker bytes, sets `options`, calls
 *          syncProfileFromOptions(), prints the resulting profile bytes
 *          as hex (same length as the marker)
 *   batch13_check.exe destroy_game_data <gd_ptr_hex>
 *       -> calls destroy_game_data(gd_ptr), prints
 *          "free_called=<n> arg=<hex>"
 */
#include "game_types.h"
#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *get_version_str(void);
extern void syncProfileFromOptions(void);
extern void destroy_game_data(Tgame_data *gd);

static int g_free_called = 0;
static void *g_free_arg = (void *)0;

/* Called instead of libc free() only inside destroy_game_data.c's own
 * translation unit (that file is compiled with -Dfree=harness_capture_free
 * for this harness build only -- see build_batch13.sh). Mirrors
 * play_sound's existing `#define play_sound harness_trace_play_sound`
 * call-trace convention (pf_harness_calltrace.h). */
void harness_capture_free(void *p)
{
    g_free_called++;
    g_free_arg = p;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void hex_decode(const char *s, unsigned char *out, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = (unsigned char)((hexval(s[2 * i]) << 4) | hexval(s[2 * i + 1]));
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;

    if (!strcmp(argv[1], "get_version_str")) {
        printf("%s", get_version_str());
        return 0;
    }

    if (!strcmp(argv[1], "sync_profile")) {
        static Tprofile buf;
        size_t n = strlen(argv[6]) / 2;
        size_t i;
        hex_decode(argv[6], (unsigned char *)&buf, n);
        profile = &buf;
        options.flash = (int)strtoul(argv[2], NULL, 10);
        options.jump_hold = (int)strtoul(argv[3], NULL, 10);
        options.msc_volume = (int)strtoul(argv[4], NULL, 10);
        options.snd_volume = (int)strtoul(argv[5], NULL, 10);
        syncProfileFromOptions();
        for (i = 0; i < n; i++)
            printf("%02x", ((unsigned char *)&buf)[i]);
        return 0;
    }

    if (!strcmp(argv[1], "destroy_game_data")) {
        unsigned long v = strtoul(argv[2], NULL, 16);
        destroy_game_data((Tgame_data *)(size_t)v);
        printf("free_called=%d arg=%08lx\n", g_free_called, (unsigned long)(size_t)g_free_arg);
        return 0;
    }

    return 2;
}
