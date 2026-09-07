// frame.hpp - milestone 9a (win32_pilot.md SS4a "the first graphics oracle
// is the game's own off-screen BITMAP before blit_to_screen", SS8 row 9a;
// carrier/NOTES.md "Headless, frame oracle, named globals, .itr workload"):
// a sensor at blit_to_screen's entry that hashes the game's SOURCE BITMAP
// (the argument it is about to present - see frame.cpp's header comment for
// which global that is and how it was confirmed) row-by-row via its own
// line[] pointers and bpp, so the digest is presentation-independent: it
// exists ABOVE the graphics backend (Allegro driver / cnc-ddraw / GDI),
// which is exactly the claim win32_pilot.md sec 4a makes and this sensor is
// built to test.
//
// Mechanism: a hardware execute breakpoint at blit_to_screen's entry (the
// DR-budget table det.hpp/bind.cpp already share) - not an entry patch,
// because nothing needs to change here: the sensor only READS memory
// (the argument pointer, then the BITMAP's own fields), it never rewrites
// an argument the way headless.cpp's set_gfx_mode/install_sound sensors do,
// so there is nothing to "call the original after" - the breakpoint's
// existing RF-flag single-step-over already lets blit_to_screen run
// completely unmodified immediately after the callback returns.
#pragma once
#include <cstdio>

struct FrameOptions {
    int every;                 // --frame-digest-every N (default 1 = every call)
    const char* digest_out;    // --frame-digest-out PATH, or nullptr (sensor inert)
    int dump_at_tick;          // --frame-dump-at T ... (0/negative = none)
    const char* dump_path;     // ... PATH  (PPM, written once, at the first
                                // blit_to_screen call observed at tick T)
};

// Registers the blit_to_screen breakpoint when digest_out or dump_at_tick+
// dump_path is given; otherwise fully inert. Must run after bind_init()/
// det_install_entry_patches() and BEFORE det_arm_main_thread() - same
// ordering constraint as every other det.hpp breakpoint-table consumer.
void frame_init(const FrameOptions& opt);

// Flushes/closes the digest file. Safe to call more than once.
void frame_shutdown();

// --report JSON fragment ("frame_oracle": {...}). Emits nothing when
// frame_init was never given anything to do.
void frame_report_json(FILE* f);
