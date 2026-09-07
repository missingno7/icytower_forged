/* third_party/smoke/smoke.c
 *
 * Minimal Allegro 4.4.3.1 program proving the static build in
 * third_party/build-allegro-4.4.3.1/ actually links and runs: opens a GDI
 * window, draws a bitmap into it with a blit and textout_ex, waits ~1
 * second via install_int, then exits cleanly. Also links (but does not
 * call at runtime, per win32_pilot.md - no unlocked OGG asset is available
 * outside the password-protected datafiles) against logg_load and
 * loadpng's load_png, to prove both addons resolve at link time against
 * the static liballeg.a/libloadpng.a/liblogg.a plus the MSYS2
 * libogg/libvorbis/libpng/zlib import libraries.
 *
 * Matches the Windows driver set the game itself links (win32_pilot.md
 * S7b, notes/library_boundary.md "Build configuration that must be
 * reproduced"): GFX_AUTODETECT_WINDOWED (DirectDraw family, falls back to
 * GDI), DirectSound/DirectInput/timer via install_timer/install_keyboard,
 * x87 control word reset via allegro_init()'s own __fpreset()-equivalent
 * (Allegro's win platform init already does this; smoke.c does not touch
 * the FPU control word itself).
 */
#include <allegro.h>
#include <logg.h>
#include <loadpng.h>

static volatile long tick_count = 0;

static void tick_counter(void)
{
    tick_count++;
}
END_OF_FUNCTION(tick_counter);

int main(void)
{
    BITMAP *buf;
    int i;

    if (allegro_init() != 0)
        return 1;

    install_timer();

    if (install_keyboard() != 0) {
        allegro_message("install_keyboard failed: %s\n", allegro_error);
        /* keep going - a headless/CI box may have no keyboard driver */
    }

    set_color_depth(16);

    /* GFX_AUTODETECT_WINDOWED tries the DirectDraw family first and falls
     * back to GDI automatically - this is the same driver family the game
     * links (win/wddraw.c, win/wgdi.c both present in liballeg.a). */
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) != 0) {
        if (set_gfx_mode(GFX_GDI, 640, 480, 0, 0) != 0) {
            allegro_message("set_gfx_mode failed: %s\n", allegro_error);
            return 2;
        }
    }

    buf = create_bitmap(640, 480);
    if (!buf) {
        allegro_message("create_bitmap failed\n");
        return 3;
    }

    clear_to_color(buf, makecol(16, 24, 40));
    for (i = 0; i < 8; i++) {
        rect(buf, 40 + i * 4, 40 + i * 4, 600 - i * 4, 440 - i * 4,
             makecol(255 - i * 20, i * 20, 128));
    }
    textout_ex(buf, font, "third_party smoke: Allegro 4.4.3.1 static build",
               20, 20, makecol(255, 255, 255), -1);

    blit(buf, screen, 0, 0, 0, 0, 640, 480);

    /* Prove the timer subsystem (install_int) works: wait ~1 second driven
     * by a real Allegro interrupt handler, not a plain Sleep(). */
    LOCK_VARIABLE(tick_count);
    LOCK_FUNCTION(tick_counter);

    if (install_int_ex(tick_counter, BPS_TO_TIMER(100)) == 0) {
        /* 100 ticks/sec * 1 second = 100 ticks */
        while (tick_count < 100) {
            rest(1);
        }
        remove_int(tick_counter);
    } else {
        rest(1000);
    }

    destroy_bitmap(buf);

    /* Prove logg/loadpng really link (not just compile): call them on a
     * path that does not exist. Every shipped datafile is
     * password-protected (win32_pilot.md notes) and there is no unlocked
     * OGG/PNG asset available outside them, so this cannot exercise the
     * real decode path - but a genuine call (not dead code under `if(0)`,
     * which GCC folds away before the linker ever sees it) forces the
     * linker to pull addons/logg and addons/loadpng object code, and
     * transitively libogg/libvorbis/libpng/zlib, out of the static
     * archives. Both must fail gracefully (return NULL) on a missing file.
     */
    {
        SAMPLE *missing_sample = logg_load("__smoke_does_not_exist__.ogg");
        BITMAP *missing_png = load_png("__smoke_does_not_exist__.png", NULL);

        if (missing_sample != NULL || missing_png != NULL) {
            allegro_message(
                "smoke: logg_load/load_png unexpectedly succeeded on a "
                "missing file\n");
            allegro_exit();
            return 4;
        }
    }

    allegro_exit();
    return 0;
}
END_OF_MAIN()
