// headless.hpp - milestone 9a (win32_pilot.md SS8 row 9a, carrier/NOTES.md
// "Headless, frame oracle, named globals, .itr workload"): true headless
// (no DirectDraw at all - force Allegro's GDI driver) and --no-sound.
//
// Mechanism: an "argument sensor" - a hardware execute breakpoint (the same
// det.hpp table _switch_in/_handle_mouse_input/bind.cpp's ORIGINAL-form
// sensing already share) at the guest's own set_gfx_mode/install_sound
// entry. Unlike bind.cpp's 5-byte jmp entry patch (a full REPLACEMENT of
// the function), this needs the REAL function body to still run - only the
// arguments change - so a breakpoint that rewrites the guest stack's cdecl
// argument dwords in place, then lets the original instruction execute via
// the existing RF-flag single-step-over (det_veh_handler), is simpler and
// safer than relocating a trampoline: no bytes are patched at all, no
// instruction-length decoding is needed, and the original driver-init code
// runs completely unmodified, just with different inputs.
#pragma once
#include <cstdio>

struct HeadlessOptions {
    bool headless;  // force GDI + windowed 640x480 at set_gfx_mode
    bool no_sound;  // force DIGI_NONE/MIDI_NONE at install_sound
};

// Registers the breakpoint(s) this needs (1 slot for --headless, 1 more for
// --no-sound). Must run after bind_init()/det_install_entry_patches() and
// BEFORE det_arm_main_thread() - same ordering constraint as every other
// det.hpp breakpoint-table consumer. Fails loudly (exit 3) if the DR budget
// is exhausted, exactly like bind.cpp/det.cpp do for their own slots.
void headless_init(const HeadlessOptions& opt);

// --report JSON fragment ("headless": {...}). Emits nothing when neither
// --headless nor --no-sound was given.
void headless_report_json(FILE* f);
