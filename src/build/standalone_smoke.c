/* standalone_smoke.c -- proves the LIBRARIES-coastline header swap
 * (src/README.md, src/icytower/GENERATED.md, carrier/gen/LIB_BINDINGS_NOTES.md
 * "Standalone build swap") by linking the UNCHANGED clean-source .c files
 * under src/icytower/ against the REAL upstream Allegro 4.4.3.1 static
 * libraries built in third_party/ (see third_party/BUILD.md), then calling
 * four of the sixteen promoted functions and checking their results.
 *
 * This file is the test harness, not part of the clean source -- it is
 * allowed to know about the swap (-DICYTOWER_UPSTREAM_ALLEGRO), allowed to
 * poke the real Allegro `key[]` array directly, and allowed to construct
 * game structs by hand instead of loading them from the original process.
 * None of that is true of anything under src/icytower/ itself.
 *
 * Functions exercised:
 *   - reset_map(Tmap*)                          src/icytower/map.c
 *   - getFloorData(Tmap*, int, int*, int*, int*) src/icytower/map.c
 *   - is_left(Tcontrol*)                         src/icytower/control.c
 *   - update_frame(void)                         src/icytower/update_frame.c
 *
 * allegro_init() is deliberately never called: none of these four touch
 * screen/gfx_driver/timer state, only the plain data globals game_state.h
 * declares (this file gives them storage the same way state.c does, since
 * this file links against state.c's actual TU) and the real Allegro `key[]`
 * array, which is valid static/BSS storage the moment liballeg.a is linked
 * in, whether or not install_keyboard() ever ran.
 */
#define ICYTOWER_UPSTREAM_ALLEGRO 1

#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"   /* -> <allegro.h> + <logg.h> under the guard above;
                             * pulls in the real `key[]`/KEY_LEFT from liballeg.a */

/* <allegro.h> (allegro/platform/alwin.h) #defines `main` to `_mangled_main`
 * for its own WinMain trampoline convention -- harmless for the CLEAN
 * source (game_funcs.h's own `_mangled_main` prototype is a DWARF-recovered
 * declaration, never a definition, so the two coexist fine), but this test
 * harness's own `int main(void)` below is not written to that convention
 * (no END_OF_MAIN(), no argc/argv), so the macro is undone before defining
 * it -- a test-harness concern only, not a src/icytower/ one. */
#ifdef main
#undef main
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CTRL_LEFT 0x01

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { printf("PASS %s\n", msg); } \
        else { failures++; printf("FAIL %s\n", msg); } \
    } while (0)

int main(void)
{
    printf("=== standalone_smoke: clean src/icytower/ linked against real upstream Allegro 4.4.3.1 ===\n");

    /* ---- reset_map / getFloorData (src/icytower/map.c) ---- */
    {
        Tmap m;
        memset(&m, 0xAA, sizeof m);   /* poison, so reset_map's writes are the only reason this passes */

        reset_map(&m);

        int all_empty = 1, all_reset = 1;
        for (int i = 0; i < 32; i++) {
            if (m.room[i].empty != -1) all_empty = 0;
            if (m.room[i].level != 0 || m.room[i].sign != 0) all_reset = 0;
        }
        CHECK(all_empty, "reset_map: every room[].empty == -1");
        CHECK(all_reset, "reset_map: every room[].level/.sign == 0");
        CHECK(m.offset == 0, "reset_map: offset == 0");

        /* Give row 5 a floor and ask getFloorData for it. Row index y comes
         * from y = 29 - ((cy+1) >> 4) (map.c/is_solid.c's shared idiom), so
         * y == 5 needs cy in [ (24<<4)-1, (25<<4)-2 ] = [383, 398]; pick 384. */
        m.room[5].empty = 0;
        m.room[5].start_tile = 2;
        m.room[5].end_tile = 6;
        m.offset = 3;

        int fy = -1, fx1 = -1, fx2 = -1;
        int cy = 384;
        getFloorData(&m, cy, &fy, &fx1, &fx2);

        CHECK(fx1 == 2 * 16 - 2, "getFloorData: fx1 == start_tile*16-2");
        CHECK(fx2 == 6 * 16 + 17, "getFloorData: fx2 == end_tile*16+17");
        CHECK(fy == (((cy + 1) >> 4) << 4) + (m.offset % 16), "getFloorData: fy matches the row-top+offset formula");

        /* an empty row must leave the outputs untouched */
        int ufy = -111, ufx1 = -111, ufx2 = -111;
        getFloorData(&m, 0 /* row 29, still empty=-1 from reset_map */, &ufy, &ufx1, &ufx2);
        CHECK(ufy == -111 && ufx1 == -111 && ufx2 == -111, "getFloorData: empty row leaves outputs untouched");
    }

    /* ---- is_left (src/icytower/control.c), key[] poked via REAL Allegro ---- */
    {
        /* key[] here is the real upstream `extern volatile char key[]`
         * from liballeg.a (allegro/keyboard.h, AL_ARRAY -- unsized in the
         * header, KEY_MAX+1 elements in the real static data; linked in via
         * allegro_api.h's ICYTOWER_UPSTREAM_ALLEGRO branch) -- not the
         * stand-in's own `volatile char key[127]`. Proves the real static
         * library's data symbol resolves and is writable. */
        for (int i = 0; i <= KEY_MAX; i++)
            key[i] = 0;
        key[KEY_LEFT] = 1;

        Tcontrol c;
        memset(&c, 0, sizeof c);
        c.key_left = KEY_LEFT;
        /* is_left() itself only ever looks at c->flags (control.c has no
         * key[]-to-flags mapping among the 16 promoted functions -- that
         * translation lives in a game function not yet promoted), so this
         * harness does the same translation play() would: */
        c.flags = key[c.key_left] ? CTRL_LEFT : 0;

        int left_when_pressed = is_left(&c);
        CHECK(left_when_pressed == -1, "is_left: -1 (all-bits-set true) when key[KEY_LEFT] is down");

        key[KEY_LEFT] = 0;
        c.flags = key[c.key_left] ? CTRL_LEFT : 0;
        int left_when_released = is_left(&c);
        CHECK(left_when_released == 0, "is_left: 0 when key[KEY_LEFT] is up");
    }

    /* ---- update_frame (src/icytower/update_frame.c) ----
     * Touches reward_time/reward_scale/player_id/ply[]/logic_count -- all
     * plain game_state.h externs, storage supplied by state.c (linked into
     * this same binary), address-free exactly like every other world. */
    {
        Tplayer p;
        memset(&p, 0, sizeof p);
        ply[0] = &p;
        player_id = 0;

        reward_time = 65;    /* > 60: growing band */
        reward_scale = 1000;
        logic_count = 10;    /* % 10 == 0: frame advances */
        p.dead = 5;          /* 0 < dead <= 0x12b: advances by 8 */
        p.edge = 1;          /* edge set: edge_drawn advances */
        p.edge_drawn = 0;
        p.frame = 0;

        update_frame();

        CHECK(reward_time == 64, "update_frame: reward_time decremented");
        CHECK(reward_scale == 1000 + 0xccd, "update_frame: reward_scale grew by 0xccd (>60 band)");
        CHECK(p.dead == 13, "update_frame: dead advanced by 8");
        CHECK(p.edge_drawn == 1, "update_frame: edge_drawn advanced");
        CHECK(p.frame == 1, "update_frame: frame advanced (logic_count %% 10 == 0)");

        /* draining band */
        reward_time = 9;
        reward_scale = 1000;
        update_frame();
        CHECK(reward_scale == 1000 - 0x199a, "update_frame: reward_scale drained by 0x199a (<=9 band)");
        CHECK(reward_time == 8, "update_frame: reward_time decremented again");
    }

    printf("=== %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
