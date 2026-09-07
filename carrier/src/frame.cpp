// frame.cpp - see frame.hpp.
//
// The mechanism (a breakpoint at the present function's entry, the guarded
// reads, the row-by-row surface digest and the PPM writer) is
// pf::win32::frame_* in port_forge/src/platform/win32/frame_oracle.hpp;
// which function and which struct offsets is icytower::kFrameOracle
// (carrier/win32_policy.hpp), where the evidence for each lives. What stays
// here is what this carrier DOES with a frame: the --frame-digest-every
// stride, the digest stream keyed by carrier tick, and the one-shot dump.
//
// Why this file does NOT include carrier/gen/it_types.h (same reasoning as
// bind.cpp's/headless.cpp's own header comments): it collides with
// windows.h's BITMAP. The offsets are policy data instead.
#define NOMINMAX // windows.h's min/max macros break sha256.hpp's std::min<...>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include "frame.hpp"
#include "det.hpp"
#include "../win32_policy.hpp"

namespace {

int g_every = 1;
FILE* g_digest_file = nullptr;
int g_dump_at_tick = 0;
char g_dump_path[MAX_PATH] = "";
bool g_dump_done = false;

long g_calls = 0;
long g_digest_lines = 0;
bool g_armed = false;

void on_blit_to_screen(CONTEXT* ctx) {
    unsigned bmp = pf::win32::frame_argument(ctx);
    ++g_calls;
    int T = det_tick();

    pf::win32::FrameSurface s = pf::win32::frame_read_surface(bmp);
    if (!s.plausible) {
        fprintf(stderr, "frame: blit_to_screen argument 0x%08x (T=%d) - BITMAP header "
                        "unreadable or implausible (w=%d h=%d), skipped\n", bmp, T, s.w, s.h);
        return;
    }

    bool do_digest = g_digest_file && (g_every <= 1 || (g_calls - 1) % g_every == 0);
    if (do_digest) {
        std::string hex;
        if (pf::win32::frame_digest(s, &hex)) {
            fprintf(g_digest_file, "%d %s %d %d %d\n", T, hex.c_str(), s.w, s.h, s.bpp);
            ++g_digest_lines;
        } else {
            // A row that became unreadable partway is reported as such, in
            // line, so the stream stays aligned by tick instead of silently
            // going short.
            fprintf(g_digest_file, "%d PF_FRAME_UNREADABLE %d %d %d\n", T, s.w, s.h, s.bpp);
        }
        fflush(g_digest_file);
    }

    if (!g_dump_done && g_dump_path[0] && T == g_dump_at_tick) {
        pf::win32::frame_dump_ppm(g_dump_path, s);
        g_dump_done = true;
    }
}

} // namespace

void frame_init(const FrameOptions& opt) {
    g_every = opt.every > 0 ? opt.every : 1;
    if (opt.digest_out && opt.digest_out[0]) {
        g_digest_file = fopen(opt.digest_out, "w");
        if (!g_digest_file) {
            fprintf(stderr, "frame: FATAL - could not open --frame-digest-out '%s'\n", opt.digest_out);
            exit(3);
        }
    }
    g_dump_at_tick = opt.dump_at_tick;
    if (opt.dump_path && opt.dump_path[0]) {
        strncpy(g_dump_path, opt.dump_path, sizeof(g_dump_path) - 1);
        g_dump_path[sizeof(g_dump_path) - 1] = 0;
    }
    bool want_dump = g_dump_at_tick > 0 && g_dump_path[0];
    if (!g_digest_file && !want_dump) return; // fully inert

    pf::win32::frame_oracle_init(icytower::kFrameOracle);
    int slot = pf::win32::frame_oracle_arm(on_blit_to_screen);
    if (slot < 0) {
        fprintf(stderr, "frame: FATAL - no free debug register for the blit_to_screen "
                        "frame-oracle sensor (DR budget exhausted by --det/--bind/--record-input)\n");
        fflush(stderr);
        exit(3);
    }
    g_armed = true;
    fprintf(stderr, "frame: armed blit_to_screen frame-oracle sensor @0x%08lx (DR%d), every=%d%s%s\n",
            icytower::kFrameOracle.present_fn_va, slot, g_every,
            g_digest_file ? " digest_out=yes" : "",
            want_dump ? " dump_at=yes" : "");
}

void frame_shutdown() {
    if (g_digest_file) { fflush(g_digest_file); fclose(g_digest_file); g_digest_file = nullptr; }
}

void frame_report_json(FILE* f) {
    if (!g_armed) return;
    fprintf(f, "  \"frame_oracle\": { \"every\": %d, \"calls\": %ld, \"digest_lines\": %ld, "
               "\"dump_at_tick\": %d, \"dump_done\": %s },\n",
            g_every, g_calls, g_digest_lines, g_dump_at_tick, g_dump_done ? "true" : "false");
}
