// headless.cpp - see headless.hpp.
//
// The mechanism (an execute breakpoint at the callee's entry, rewriting the
// cdecl argument dwords on the guest stack before its prologue runs) is
// pf::win32::arg_sensor_* in
// port_forge/src/platform/win32/arg_sensor.hpp. This file is two rows of
// data and the decision of when to arm them.
//
// Why this file does NOT include carrier/gen/pf_lib_bindings.h (MEASURED,
// not a preference - same reasoning bind.cpp's own header comment gives for
// it_types.h): pf_lib_bindings.h pulls in pf_lib_bindings_types.h, which
// declares its own BITMAP/GFX_VTABLE types that collide with windows.h the
// moment both are visible in one translation unit, and this file needs
// windows.h for CONTEXT. The constants are therefore restated in
// carrier/win32_policy.hpp as plain integer literals, each cited.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include "headless.hpp"
#include "det.hpp"
#include "../win32_policy.hpp"

namespace {

bool g_headless = false;
bool g_no_sound = false;

} // namespace

void headless_init(const HeadlessOptions& opt) {
    g_headless = opt.headless;
    g_no_sound = opt.no_sound;
    if (g_headless) {
        int slot = pf::win32::arg_sensor_arm(icytower::kSensorSetGfxMode);
        if (slot < 0) {
            fprintf(stderr, "headless: FATAL - no free debug register for the set_gfx_mode "
                            "argument sensor (DR budget exhausted by --det/--bind/--record-input)\n");
            fflush(stderr);
            exit(3);
        }
        fprintf(stderr, "headless: armed set_gfx_mode argument sensor @0x%08lx (DR%d) - "
                        "forcing GFX_GDI, windowed 640x480\n",
                icytower::kSensorSetGfxMode.va, slot);
    }
    if (g_no_sound) {
        int slot = pf::win32::arg_sensor_arm(icytower::kSensorInstallSound);
        if (slot < 0) {
            fprintf(stderr, "headless: FATAL - no free debug register for the install_sound "
                            "argument sensor (DR budget exhausted by --det/--bind/--record-input)\n");
            fflush(stderr);
            exit(3);
        }
        fprintf(stderr, "headless: armed install_sound argument sensor @0x%08lx (DR%d) - "
                        "forcing DIGI_NONE/MIDI_NONE\n",
                icytower::kSensorInstallSound.va, slot);
    }
}

void headless_report_json(FILE* f) {
    if (!g_headless && !g_no_sound) return;
    const unsigned long gfx = icytower::kSensorSetGfxMode.va;
    const unsigned long snd = icytower::kSensorInstallSound.va;
    fprintf(f, "  \"headless\": { \"headless\": %s, \"no_sound\": %s, "
               "\"set_gfx_mode_hits\": %ld, \"set_gfx_mode_seen\": %s, "
               "\"set_gfx_mode_original_card\": \"0x%08x\", "
               "\"install_sound_hits\": %ld, \"install_sound_seen\": %s, "
               "\"install_sound_original_digi\": %d, \"install_sound_original_midi\": %d },\n",
            g_headless ? "true" : "false", g_no_sound ? "true" : "false",
            pf::win32::arg_sensor_hits(gfx),
            pf::win32::arg_sensor_seen(gfx) ? "true" : "false",
            pf::win32::arg_sensor_original(gfx, 0),
            pf::win32::arg_sensor_hits(snd),
            pf::win32::arg_sensor_seen(snd) ? "true" : "false",
            (int)pf::win32::arg_sensor_original(snd, 0),
            (int)pf::win32::arg_sensor_original(snd, 1));
}
