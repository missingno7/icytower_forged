// headless.cpp - see headless.hpp.
//
// Why this file does NOT include carrier/gen/pf_lib_bindings.h (MEASURED,
// not a preference - same reasoning bind.cpp's own header comment gives for
// it_types.h): pf_lib_bindings.h pulls in pf_lib_bindings_types.h, which
// declares its own BITMAP/GFX_VTABLE types that collide with windows.h the
// moment both are visible in one translation unit, and this file needs
// windows.h for CONTEXT/VirtualProtect. The two constants this file needs
// are therefore restated here as plain integer literals, each cited to its
// generated source line.
#include <windows.h>
#include <cstdio>
#include "headless.hpp"
#include "det.hpp"

// KNOWN (carrier/gen/interop_index.json, DWARF-confirmed COFF symbols
// _set_gfx_mode / _install_sound - notes/binary_recon.md items g/h):
//   set_gfx_mode(int card, int w, int h, int v_w, int v_h)   graphics.c 0x450688
//   install_sound(int digi, int midi, const char *cfg_path)  sound.c    0x4417b0
// cdecl, so at the breakpoint's hit (EIP == va, i.e. BEFORE the callee's own
// prologue runs) the guest ESP is exactly [retaddr][arg0][arg1]...,
// unmodified since the caller's `call` instruction - see notes/
// binary_recon.md item g/h and carrier/NOTES.md's "Headless..." section.
#define VA_SET_GFX_MODE  0x450688u
#define VA_INSTALL_SOUND 0x4417b0u

// KNOWN (carrier/gen/pf_lib_bindings.h:420, generated from Allegro 4.4.1
// DWARF - the FOURCC-style value Allegro's public <allegro/gfx.h> defines
// for the GDI software driver; notes/binary_recon.md item g confirms GDI
// (_gfx_gdi) is one of the two gfx-driver families actually compiled into
// this binary, alongside the DirectDraw family).
#define PF_GFX_GDI 0x47444942u

// KNOWN (Allegro 4's public API, allegro/digi.h + allegro/midi.h - stable
// across the whole 4.x series; not FOURCC-encoded like the GFX_* constants,
// so not re-derived from disassembly): DIGI_NONE=0 selects the compiled-in
// "no sound device" driver (notes/binary_recon.md item h confirms _digi_none
// is compiled in alongside _digi_directsound); MIDI_NONE=0 likewise.
#define PF_DIGI_NONE 0
#define PF_MIDI_NONE 0

namespace {

bool g_headless = false;
bool g_no_sound = false;
long g_gfx_hits = 0;
long g_sound_hits = 0;
int g_gfx_orig_card = 0, g_gfx_orig_w = 0, g_gfx_orig_h = 0;
int g_sound_orig_digi = 0, g_sound_orig_midi = 0;
bool g_gfx_seen = false, g_sound_seen = false;

// Called with EIP == VA_SET_GFX_MODE, i.e. before the callee's prologue has
// run. ctx->Esp is exactly what the caller's `call` left: [retaddr][card]
// [w][h][v_w][v_h]. Rewriting those dwords in guest memory (not the CONTEXT
// itself) is all that is needed - det_veh_handler resumes execution at the
// unmodified original instruction right after, which then reads the NEW
// argument values normally, same as if the caller had pushed them.
void on_set_gfx_mode(CONTEXT* ctx) {
    unsigned* sp = (unsigned*)(uintptr_t)ctx->Esp;
    g_gfx_seen = true;
    g_gfx_orig_card = (int)sp[1];
    g_gfx_orig_w = (int)sp[2];
    g_gfx_orig_h = (int)sp[3];
    sp[1] = PF_GFX_GDI;
    sp[2] = 640;
    sp[3] = 480;
    // v_w/v_h (sp[4], sp[5]) left as the guest passed them: GFX_GDI has no
    // page-flipping/virtual-screen concept, so forcing them is unnecessary
    // (Allegro's own GDI driver ignores them - notes/binary_recon.md item g).
    ++g_gfx_hits;
    fprintf(stderr,
        "headless: set_gfx_mode(card=0x%08x,w=%d,h=%d) -> (card=GFX_GDI,w=640,h=480)  [hit %ld]\n",
        (unsigned)g_gfx_orig_card, g_gfx_orig_w, g_gfx_orig_h, g_gfx_hits);
}

void on_install_sound(CONTEXT* ctx) {
    unsigned* sp = (unsigned*)(uintptr_t)ctx->Esp;
    g_sound_seen = true;
    g_sound_orig_digi = (int)sp[1];
    g_sound_orig_midi = (int)sp[2];
    sp[1] = (unsigned)PF_DIGI_NONE;
    sp[2] = (unsigned)PF_MIDI_NONE;
    ++g_sound_hits;
    fprintf(stderr,
        "headless: --no-sound: install_sound(digi=%d,midi=%d) -> (DIGI_NONE,MIDI_NONE)  [hit %ld]\n",
        g_sound_orig_digi, g_sound_orig_midi, g_sound_hits);
}

} // namespace

void headless_init(const HeadlessOptions& opt) {
    g_headless = opt.headless;
    g_no_sound = opt.no_sound;
    if (g_headless) {
        int slot = det_register_breakpoint(VA_SET_GFX_MODE, on_set_gfx_mode);
        if (slot < 0) {
            fprintf(stderr, "headless: FATAL - no free debug register for the set_gfx_mode "
                            "argument sensor (DR budget exhausted by --det/--bind/--record-input)\n");
            fflush(stderr);
            exit(3);
        }
        fprintf(stderr, "headless: armed set_gfx_mode argument sensor @0x%08x (DR%d) - "
                        "forcing GFX_GDI, windowed 640x480\n", VA_SET_GFX_MODE, slot);
    }
    if (g_no_sound) {
        int slot = det_register_breakpoint(VA_INSTALL_SOUND, on_install_sound);
        if (slot < 0) {
            fprintf(stderr, "headless: FATAL - no free debug register for the install_sound "
                            "argument sensor (DR budget exhausted by --det/--bind/--record-input)\n");
            fflush(stderr);
            exit(3);
        }
        fprintf(stderr, "headless: armed install_sound argument sensor @0x%08x (DR%d) - "
                        "forcing DIGI_NONE/MIDI_NONE\n", VA_INSTALL_SOUND, slot);
    }
}

void headless_report_json(FILE* f) {
    if (!g_headless && !g_no_sound) return;
    fprintf(f, "  \"headless\": { \"headless\": %s, \"no_sound\": %s, "
               "\"set_gfx_mode_hits\": %ld, \"set_gfx_mode_seen\": %s, "
               "\"set_gfx_mode_original_card\": \"0x%08x\", "
               "\"install_sound_hits\": %ld, \"install_sound_seen\": %s, "
               "\"install_sound_original_digi\": %d, \"install_sound_original_midi\": %d },\n",
            g_headless ? "true" : "false", g_no_sound ? "true" : "false",
            g_gfx_hits, g_gfx_seen ? "true" : "false", (unsigned)g_gfx_orig_card,
            g_sound_hits, g_sound_seen ? "true" : "false",
            g_sound_orig_digi, g_sound_orig_midi);
}
