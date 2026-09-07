// frame.cpp - see frame.hpp.
//
// Which BITMAP is blit_to_screen's argument, and is it the right oracle
// (KNOWN, notes/binary_recon.md item g + artifacts/disasm.txt): every one
// of the ~20 call sites pushes a BITMAP* onto the stack immediately before
// `call 40b6bc <_blit_to_screen>`; the site inside play()'s own per-tick
// path (VA 0x413326, right after the frame-pacing wait, per item l) is:
//     41331e: mov eax, 0x4dd194      ; swap_screen (BITMAP*, main.c, KNOWN
//                                      from carrier/gen/it_globals.h/
//                                      interop_index.json: "swap_screen",
//                                      type "BITMAP *")
//     413323: mov [esp], eax
//     413326: call 40b6bc <_blit_to_screen>
// i.e. blit_to_screen(BITMAP *bmp) takes the SOURCE bitmap as its one cdecl
// argument - draw_frame() (0x40929c) has just finished rendering into that
// same bitmap (swap_screen), and blit_to_screen presents it to the driver's
// own `screen` BITMAP (0x4dda8c, Allegro's own global, NOT hashed here).
// Since the argument IS the oracle bitmap on every call site (not just
// play()'s), this sensor just reads the argument directly - no need to
// special-case swap_screen by name, which also makes it correct for the
// menu/other call sites without any extra work.
//
// BITMAP layout (KNOWN, carrier/gen/it_types.h, DWARF-derived):
//   int w,h,clip,cl,cr,ct,cb;      (offsets 0,4,8,12,16,20,24)
//   GFX_VTABLE *vtable;            (offset 28)
//   void *write_bank,*read_bank,*dat; unsigned long id; void *extra;
//   int x_ofs,y_ofs,seg;           (offsets 32,36,40,44,48,52,56,60)
//   unsigned char *line[h];        (offset 64 - one row pointer per row,
//                                    DWARF gives it as an unbounded array)
// GFX_VTABLE.color_depth is its first member (offset 0) - Allegro's public
// `bitmap_color_depth(bmp)` macro is exactly `(bmp)->vtable->color_depth`
// (win32_pilot.md task brief); bytes-per-pixel = (color_depth+7)/8, the
// standard Allegro BYTES_PER_PIXEL() macro.
//
// Why this file does NOT include carrier/gen/it_types.h (same reasoning as
// bind.cpp's/headless.cpp's own header comments): it collides with
// windows.h's BITMAP. The handful of offsets above are restated as
// constants instead.
#define NOMINMAX // windows.h's min/max macros break sha256.hpp's std::min<...>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include "frame.hpp"
#include "det.hpp"
#include "../../port_forge/src/core/sha256.hpp"

#define VA_BLIT_TO_SCREEN 0x40b6bcu

#define BM_OFF_W      0u
#define BM_OFF_H      4u
#define BM_OFF_VTABLE 28u
#define BM_OFF_LINE   64u
#define VT_OFF_COLOR_DEPTH 0u

// KNOWN (carrier/gen/pf_lib_bindings.h:143, generated from Allegro 4.4.1
// DWARF): void get_palette(RGB *pal) - fills a 256-entry palette (r/g/b in
// Allegro's native 0..63 range, plus a filler byte), used ONLY for the
// --frame-dump-at PPM's 8bpp-indexed case (the digest itself never needs
// palette interpretation - it hashes raw bytes regardless of depth). Called
// directly, like det.cpp already calls other Allegro internals by address
// (e.g. _handle_timer_tick) - safe here because the sensor callback runs
// synchronously ON the guest's own main thread (the thread the breakpoint
// fired on), not a separate one.
typedef void (__cdecl *PFN_get_palette)(unsigned char* pal256x4);
static const PFN_get_palette get_palette = (PFN_get_palette)0x44c47cu;

namespace {

bool readable(const void* p, size_t n) {
    static const char* cache_lo = nullptr;
    static const char* cache_hi = nullptr;
    const char* q = (const char*)p;
    if (!q) return false;
    if (q >= cache_lo && q + n <= cache_hi) return true;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    cache_lo = (const char*)mbi.BaseAddress;
    cache_hi = cache_lo + mbi.RegionSize;
    return q + n <= cache_hi;
}

int g_every = 1;
FILE* g_digest_file = nullptr;
int g_dump_at_tick = 0;
char g_dump_path[MAX_PATH] = "";
bool g_dump_done = false;

long g_calls = 0;
long g_digest_lines = 0;
bool g_armed = false;

// Writes one BITMAP (already validated readable header) as a binary PPM
// (P6): 8/15/16/24/32 bpp all converted to 24-bit RGB. Best-effort pixel
// unpacking per Allegro's own makecol{8,15,16,24,32} conventions (native
// byte order, little-endian x86: 32/24bpp store B,G,R[,pad] low-to-high;
// 16bpp is 5-6-5, 15bpp is 5-5-5, both little-endian words) - exact enough
// for a human to look at a single frame, which is this option's only job
// (the digest above never needs this - it hashes raw bytes unconditionally).
void dump_ppm(const char* path, unsigned bmp, int w, int h, int bpp, unsigned line_off) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "frame: could not open --frame-dump-at path '%s'\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);

    unsigned char pal[1024]; // 256 * {r,g,b,filler}
    bool have_pal = false;
    if (bpp == 1) { get_palette(pal); have_pal = true; }

    for (int y = 0; y < h; ++y) {
        unsigned rowptr_addr = bmp + line_off + (unsigned)y * 4u;
        if (!readable((const void*)(uintptr_t)rowptr_addr, 4)) break;
        unsigned row = *(const unsigned*)(uintptr_t)rowptr_addr;
        if (!readable((const void*)(uintptr_t)row, (size_t)w * (size_t)bpp)) break;
        const unsigned char* px = (const unsigned char*)(uintptr_t)row;
        for (int x = 0; x < w; ++x) {
            unsigned char rgb[3] = { 0, 0, 0 };
            const unsigned char* p = px + (size_t)x * bpp;
            switch (bpp) {
            case 1: {
                unsigned idx = p[0];
                if (have_pal) {
                    rgb[0] = (unsigned char)(pal[idx * 4 + 0] * 255 / 63);
                    rgb[1] = (unsigned char)(pal[idx * 4 + 1] * 255 / 63);
                    rgb[2] = (unsigned char)(pal[idx * 4 + 2] * 255 / 63);
                }
                break;
            }
            case 2: {
                unsigned v = p[0] | (p[1] << 8);
                // 5-6-5 (16bpp is Allegro's default 16-bit format).
                unsigned r5 = (v >> 11) & 0x1f, g6 = (v >> 5) & 0x3f, b5 = v & 0x1f;
                rgb[0] = (unsigned char)((r5 * 255) / 31);
                rgb[1] = (unsigned char)((g6 * 255) / 63);
                rgb[2] = (unsigned char)((b5 * 255) / 31);
                break;
            }
            case 3: case 4: {
                // B,G,R[,pad] - Allegro's makecol24/32 native byte order.
                rgb[0] = p[2]; rgb[1] = p[1]; rgb[2] = p[0];
                break;
            }
            default: break;
            }
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    fprintf(stderr, "frame: wrote --frame-dump-at PPM '%s' (%dx%d, %d bpp)\n", path, w, h, bpp);
}

void on_blit_to_screen(CONTEXT* ctx) {
    unsigned* sp = (unsigned*)(uintptr_t)ctx->Esp;
    unsigned bmp = sp[1]; // cdecl arg0: BITMAP *
    ++g_calls;
    int T = det_tick();

    if (!readable((const void*)(uintptr_t)bmp, BM_OFF_LINE)) {
        fprintf(stderr, "frame: blit_to_screen argument 0x%08x (T=%d) - BITMAP header unreadable, skipped\n", bmp, T);
        return;
    }
    int w = *(const int*)(uintptr_t)(bmp + BM_OFF_W);
    int h = *(const int*)(uintptr_t)(bmp + BM_OFF_H);
    unsigned vtable = *(const unsigned*)(uintptr_t)(bmp + BM_OFF_VTABLE);
    int bpp = 4;
    if (readable((const void*)(uintptr_t)vtable, 4)) {
        int color_depth = *(const int*)(uintptr_t)vtable;
        bpp = (color_depth + 7) / 8;
        if (bpp <= 0 || bpp > 4) bpp = 4;
    }
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) {
        fprintf(stderr, "frame: blit_to_screen argument 0x%08x (T=%d) - implausible w=%d h=%d, skipped\n", bmp, T, w, h);
        return;
    }

    bool do_digest = g_digest_file && (g_every <= 1 || (g_calls - 1) % g_every == 0);
    if (do_digest) {
        pf::Sha256 s;
        bool ok = true;
        for (int y = 0; y < h; ++y) {
            unsigned rowptr_addr = bmp + BM_OFF_LINE + (unsigned)y * 4u;
            if (!readable((const void*)(uintptr_t)rowptr_addr, 4)) { ok = false; break; }
            unsigned row = *(const unsigned*)(uintptr_t)rowptr_addr;
            size_t nbytes = (size_t)w * (size_t)bpp;
            if (!readable((const void*)(uintptr_t)row, nbytes)) { ok = false; break; }
            s.update((const void*)(uintptr_t)row, nbytes);
        }
        if (ok) {
            std::string hex = s.hex();
            fprintf(g_digest_file, "%d %s %d %d %d\n", T, hex.c_str(), w, h, bpp);
            fflush(g_digest_file);
            ++g_digest_lines;
        } else {
            fprintf(g_digest_file, "%d PF_FRAME_UNREADABLE %d %d %d\n", T, w, h, bpp);
            fflush(g_digest_file);
        }
    }

    if (!g_dump_done && g_dump_path[0] && T == g_dump_at_tick) {
        dump_ppm(g_dump_path, bmp, w, h, bpp, BM_OFF_LINE);
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

    int slot = det_register_breakpoint(VA_BLIT_TO_SCREEN, on_blit_to_screen);
    if (slot < 0) {
        fprintf(stderr, "frame: FATAL - no free debug register for the blit_to_screen "
                        "frame-oracle sensor (DR budget exhausted by --det/--bind/--record-input)\n");
        fflush(stderr);
        exit(3);
    }
    g_armed = true;
    fprintf(stderr, "frame: armed blit_to_screen frame-oracle sensor @0x%08x (DR%d), every=%d%s%s\n",
            VA_BLIT_TO_SCREEN, slot, g_every,
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
