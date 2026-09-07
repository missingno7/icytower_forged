// det.cpp - see det.hpp. Milestones 5-7 of win32_pilot.md:
//   A. virtual time      (timer-thread virtualization + Sleep-driven ticks)
//   B. input injection   (script file, delivered at tick T)
//   C. tick sensor        (hardware breakpoint at the play() safepoint)
// Every address below is KNOWN from artifacts/functions.json +
// artifacts/dwarf_info.txt (grep DW_TAG_subprogram), cited in
// carrier/NOTES.md's "Milestones 5-7" section, not re-derived here.
#define NOMINMAX // windows.h's min/max macros break sha256.hpp's std::min<...>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <string>
#include "det.hpp"
#include "snapshot.hpp" // milestones 8-9: safepoint snapshot / in-process rewind
#include "trace.hpp" // pf_count_import - see det.hpp/wrappers.hpp (item 3)
#include "../../port_forge/src/core/sha256.hpp"

// KNOWN (artifacts/functions.json + disasm.txt): Allegro internals this
// module calls directly by address (they're outside the game's own 25 CUs,
// so they're not in carrier/gen/it_funcs.h, which is game-scope only).
#define VA_TIM_HIGH_PERF_THREAD 0x478584u  // wtimer.c tim_win32_high_perf_thread
#define VA_TIM_LOW_PERF_THREAD  0x4783bcu  // wtimer.c tim_win32_low_perf_thread
#define VA_INPUT_THREAD_PROC    0x479a40u  // winput.c input_thread_proc
#define VA_HANDLE_TIMER_TICK    0x45d6c8u  // timer.c: long _handle_timer_tick(int interval)
#define VA_HANDLE_KEY_PRESS     0x43e2f8u  // keyboard.c: void _handle_key_press(int keycode, int scancode)
#define VA_HANDLE_KEY_RELEASE   0x43d8d4u  // keyboard.c: void _handle_key_release(int scancode)
#define VA_SAFEPOINT            0x4124f4u  // main.c play(): once per consumed game tick
#define VA_KEY_DINPUT_SCANCODE  0x46d5a8u  // wkeybd.c: key_dinput_handle_scancode(al=scancode,edx=?) - reg-passed args, no stack args
#define VA_HW_TO_MYCODE         0x4daf80u  // wkeybd.c: unsigned char hw_to_mycode[256] - DIK_* -> Allegro code (item 2)

// --- "Environment isolation" pass (carrier/NOTES.md) ---------------------
// KNOWN, all four read out of artifacts/disasm.txt + artifacts/functions.json
// in this pass (the exact listings are quoted in NOTES.md):
//
//   _switch_in  (dispsw.c, 0x4657e4, 35 B) and _switch_out (0x465808, 35 B)
//   are `void f(void)` and their ENTIRE body is "for i in 0..7: if
//   cb_table[i] then call cb_table[i]()" over switch_in_cb[8] @0x4ea080 /
//   switch_out_cb[8] @0x4ea060. They are reached from EXACTLY two places
//   each - the two tail `jmp`s at the end of _win_switch_in (0x47a47c) and
//   _win_switch_out (0x47a3d4) - and they are the single choke point through
//   which the game's registered switchedToProgram / switchedFromProgram
//   callbacks (notes/library_boundary.md: 3 pairs registered via
//   set_display_switch_callback) are invoked. Those callbacks write hasFocus
//   (0x4bc020), which IS in the 151-global digest domain
//   (carrier/gen/game_globals.inc) and IS read by play() at 0x411c6b/
//   0x411cd7 - so window activation reaches the verdict.
//
//   _handle_mouse_input (mouse.c, 0x45f9bc, 26 B) is `void f(void)` whose
//   whole body is "if (mouse_callback) return; else tail-jmp update_mouse()"
//   - the ONE path from the DirectInput mouse driver's own
//   mouse_dinput_handle (0x461a64, called from the window thread's
//   MsgWaitForMultipleObjects handler table) to the public mouse_x/mouse_y/
//   mouse_b globals the game reads.
//
//   fldads_threadmain (fld_adspot.c, 0x404014) is the start routine of the
//   binary's ONLY pthread_create call (fldads_start, 0x403ac8).
//
// All three patched functions take no arguments and return void, so a plain
// `ret` at their entry is a complete, convention-correct neutralization (the
// two tail-jumped ones return straight to _win_switch_*'s own caller).
#define VA_SWITCH_IN            0x4657e4u
#define VA_SWITCH_OUT           0x465808u
// wdispsw.c: the two functions directx_wnd_proc's WM_ACTIVATE arm calls
// (0x4793bb/0x47941f -> _win_switch_in, 0x479678 -> _win_switch_out); each
// ends in a tail `jmp` to _switch_in/_switch_out above. --inject-real-test
// feeds a scripted `T switch in|out` through THESE, i.e. through the real
// Allegro path, so the capture hook sees it exactly as a real WM_ACTIVATE
// would produce it - the same fallback pattern this carrier already uses for
// keys (carrier/NOTES.md "Input policy and recording" part C).
#define VA_WIN_SWITCH_IN        0x47a47cu
#define VA_WIN_SWITCH_OUT       0x47a3d4u
#define VA_SWITCH_IN_CB         0x4ea080u  // void (*switch_in_cb[8])(void)
#define VA_SWITCH_OUT_CB        0x4ea060u  // void (*switch_out_cb[8])(void)
#define VA_HANDLE_MOUSE_INPUT   0x45f9bcu
#define VA_FLDADS_THREADMAIN    0x404014u

// The virtual epoch det_wrap_time returns when no recording supplies a value
// (unchanged from milestones 5-7 - this is what keeps G1 byte-identical).
#define DET_VIRTUAL_EPOCH 1700000000L

// KNOWN (task brief + Allegro 4.4 timer.h): timer units/second. Confirmed
// against tim_win32_high_perf_thread's own disassembly, which multiplies
// QPC-elapsed-time by the literal constant 0x1234dd == 1193181 before
// calling _handle_timer_tick (see carrier/NOTES.md).
#define TIMERS_PER_SECOND 1193181LL

// KNOWN (DWARF __allegro_KEY_* enum, artifacts/dwarf_info.txt), verified to
// match the task brief exactly.
struct KeyName { const char* name; int code; };
static const KeyName kKeyNames[] = {
    {"KEY_ESC", 59}, {"KEY_ENTER", 67}, {"KEY_SPACE", 75},
    {"KEY_LEFT", 82}, {"KEY_RIGHT", 83}, {"KEY_UP", 84}, {"KEY_DOWN", 85},
};

// ---------------------------------------------------------------------
// Shared state
// ---------------------------------------------------------------------
static bool g_det_mode = false;
static bool g_pace_real = false;
static int g_stop_at_tick = 0;
static DWORD g_main_tid = 0;
static DetShutdownFn g_shutdown = nullptr;

// win32_pilot.md sec 5a: exclusive input policy. main.cpp's parse_args
// resolves the default and validates the real+input-script conflict before
// this is ever set - det_init just records + acts on the final decision.
static InputPolicy g_input_policy = InputPolicy::Real;
static bool g_inject_real_test = false;
static char g_image_path[MAX_PATH] = "";
static volatile LONG g_real_key_violations = 0;

const char* input_policy_name(InputPolicy p) {
    switch (p) {
        case InputPolicy::Real: return "real";
        case InputPolicy::Script: return "script";
        case InputPolicy::None: return "none";
    }
    return "(unknown)";
}
const char* det_input_policy_name() { return input_policy_name(g_input_policy); }
long det_real_key_violations() { return g_real_key_violations; }

// --- "Environment isolation" pass: window / focus / mouse / ad policy -----
static bool g_interactive = false;
static WindowMode g_window_mode = WindowMode::MinNoActive;
const char* window_mode_name(WindowMode m) {
    switch (m) {
        case WindowMode::Normal: return "normal";
        case WindowMode::MinNoActive: return "minnoactive";
        case WindowMode::Hidden: return "hidden";
    }
    return "(unknown)";
}
// Counters, all reported in --report's "environment" object (det_environment_json).
static long g_switch_captured = 0;   // real switch out/in seen at the choke point (input=real)
static long g_switch_suppressed = 0; // real switch out/in dropped outright (input=script/none)
static long g_switch_delivered = 0;  // switch events handed to the game at a tick boundary
static long g_switch_recorded = 0;   // switch events written to --record-input
static long g_mouse_parked = 0;      // _handle_mouse_input calls short-circuited
static long g_ad_thread_suppressed = 0;
static long g_showwindow_substituted = 0, g_setforeground_suppressed = 0,
            g_setwindowpos_noactivate = 0, g_createwindow_devisible = 0;
static long g_time_recorded = 0, g_time_replayed = 0, g_time_underflow = 0,
            g_time_offthread = 0;
static bool g_switch_patched = false, g_mouse_patched = false;

// DET_ISOLATE_OFF=<comma list of: ad,mouse,switch,window> - the determinism
// audit's per-channel knob (same opt-in diagnostic style as DET_DUMP_MEM_TICK
// / DET_INPUT_DELIVER_SUB). It turns OFF one of this pass's isolations so the
// run can be compared against the fully isolated G1 baseline and the audit
// can name the FIRST DIFFERING TICK per channel instead of asserting one.
// Never used by any gate command.
static bool isolate_off(const char* channel) {
    static char list[128];
    static bool loaded = false;
    if (!loaded) {
        if (!GetEnvironmentVariableA("DET_ISOLATE_OFF", list, sizeof(list))) list[0] = 0;
        loaded = true;
    }
    return list[0] && strstr(list, channel) != nullptr;
}

static LONGLONG g_virtual_ms = 0;      // det mode only: accumulated Sleep(ms) on the main thread
static LONGLONG g_units_reported = 0;  // running total already handed to _handle_timer_tick
static ULONGLONG g_start_tick64 = 0;   // non-det mode: real elapsed time baseline (GetTickCount64)

static FILE* g_digest_file = nullptr;
static FILE* g_record_file = nullptr;
static HANDLE g_parked_event = nullptr; // never signaled - park() blocks on it forever

// ---------------------------------------------------------------------
// --trace-input (divergence 005 instrumentation, notes/living_record.md).
//
// Logs every key event in BOTH modes with the full coordinate system the
// carrier stamps and schedules on, so a record run and a replay run can be
// diffed side by side instead of guessed at:
//
//   ms    virtual clock milliseconds (g_virtual_ms; det_now_ms outside --det)
//   T     the carrier tick index used for stamping/scheduling (= ms/20)
//   sub   which Sleep call within tick T this is (0 = the FIRST Sleep call of
//         the tick, which is where deliver_due_input hands over a scripted
//         event). This is the coordinate the old code did NOT record and
//         could not reproduce - see NOTES.md "Divergence 005".
//   cyc   the guest's OWN tick counter, cycle_count @0x506938 (timer.c,
//         incremented by cycle_counter() every 20 ms of Allegro timer time);
//         `pre`/`post` are its values before and after this Sleep call's
//         _handle_timer_tick, so a delivery can be placed on either side of
//         the guest's own tick boundary.
//   sp    number of play() safepoints (digest ticks) seen so far.
//   site  the exact call site.
// ---------------------------------------------------------------------
// True only while the carrier is inside its OWN input handover (see
// keypress_record_hit for the full rationale - divergence 005 residual 2).
static bool g_in_delivery = false;
static FILE* g_trace_input_file = nullptr;
static long long g_sleep_calls = 0;      // total det_wrap_Sleep calls on the main thread
static long long g_sub_in_tick = 0;      // Sleep calls so far within the current carrier tick
static int  g_sub_tick = -1;             // the tick g_sub_in_tick is counting within
static long g_safepoint_count = 0;       // play() safepoints seen so far
static int  g_cyc_pre = 0, g_cyc_post = 0; // guest cycle_count around this Sleep's _handle_timer_tick
#define IT_CYCLE_COUNT (*(volatile int*)(uintptr_t)0x506938u)

static void trace_input(const char* site, const char* what, int code, const char* extra) {
    if (!g_trace_input_file) return;
    fprintf(g_trace_input_file,
            "ms=%lld T=%d sub=%lld sleepn=%lld cyc=%d pre=%d post=%d sp=%ld tid=%lu site=%s %s code=%d%s%s\n",
            (long long)(g_det_mode ? g_virtual_ms : (LONGLONG)(GetTickCount64() - g_start_tick64)),
            (int)((g_det_mode ? g_virtual_ms : (LONGLONG)(GetTickCount64() - g_start_tick64)) / 20),
            g_sub_in_tick, g_sleep_calls, (int)IT_CYCLE_COUNT, g_cyc_pre, g_cyc_post,
            g_safepoint_count, GetCurrentThreadId(), site, what, code,
            extra ? " " : "", extra ? extra : "");
    fflush(g_trace_input_file);
}

static FARPROC g_real_QPC = nullptr, g_real_timeGetTime = nullptr,
               g_real_time = nullptr, g_real_clock = nullptr, g_real_beginthread = nullptr;
static FARPROC g_real_malloc = nullptr, g_real_calloc = nullptr,
               g_real_realloc = nullptr, g_real_free = nullptr;
static FARPROC g_real_Sleep = nullptr;
static FARPROC g_real_WaitForSingleObject = nullptr; // item 3: parked timer thread
static FARPROC g_real_rand = nullptr, g_real_srand = nullptr; // milestone 8: RNG pinning
// "Environment isolation" pass.
static FARPROC g_real_ShowWindow = nullptr, g_real_SetForegroundWindow = nullptr,
               g_real_SetWindowPos = nullptr, g_real_CreateWindowExA = nullptr,
               g_real_pthread_create = nullptr, g_real_getenv = nullptr;

// Import ids (see wrappers.hpp/det_bind_real doc), one per always-installed
// wrapper this file defines - each det_wrap_* below calls pf_count_import
// with its own id (item 3: fixes the report.json gap for these, which are
// wired directly into the guest IAT, bypassing the counting trampoline).
static int g_id_Sleep = -1, g_id_QPC = -1, g_id_timeGetTime = -1, g_id_time = -1,
           g_id_clock = -1, g_id_beginthread = -1,
           g_id_malloc = -1, g_id_calloc = -1, g_id_realloc = -1, g_id_free = -1,
           g_id_WaitForSingleObject = -1, g_id_rand = -1, g_id_srand = -1,
           g_id_ShowWindow = -1, g_id_SetForegroundWindow = -1, g_id_SetWindowPos = -1,
           g_id_CreateWindowExA = -1, g_id_pthread_create = -1, g_id_getenv = -1;

void det_bind_real(const char* name, void* real_proc, int id) {
    if (strcmp(name, "QueryPerformanceCounter") == 0) { g_real_QPC = (FARPROC)real_proc; g_id_QPC = id; }
    else if (strcmp(name, "timeGetTime") == 0) { g_real_timeGetTime = (FARPROC)real_proc; g_id_timeGetTime = id; }
    else if (strcmp(name, "time") == 0) { g_real_time = (FARPROC)real_proc; g_id_time = id; }
    else if (strcmp(name, "clock") == 0) { g_real_clock = (FARPROC)real_proc; g_id_clock = id; }
    else if (strcmp(name, "_beginthread") == 0) { g_real_beginthread = (FARPROC)real_proc; g_id_beginthread = id; }
    else if (strcmp(name, "malloc") == 0) { g_real_malloc = (FARPROC)real_proc; g_id_malloc = id; }
    else if (strcmp(name, "calloc") == 0) { g_real_calloc = (FARPROC)real_proc; g_id_calloc = id; }
    else if (strcmp(name, "realloc") == 0) { g_real_realloc = (FARPROC)real_proc; g_id_realloc = id; }
    else if (strcmp(name, "free") == 0) { g_real_free = (FARPROC)real_proc; g_id_free = id; }
    else if (strcmp(name, "Sleep") == 0) { g_real_Sleep = (FARPROC)real_proc; g_id_Sleep = id; }
    else if (strcmp(name, "WaitForSingleObject") == 0) { g_real_WaitForSingleObject = (FARPROC)real_proc; g_id_WaitForSingleObject = id; }
    else if (strcmp(name, "rand") == 0) { g_real_rand = (FARPROC)real_proc; g_id_rand = id; }
    else if (strcmp(name, "srand") == 0) { g_real_srand = (FARPROC)real_proc; g_id_srand = id; }
    else if (strcmp(name, "ShowWindow") == 0) { g_real_ShowWindow = (FARPROC)real_proc; g_id_ShowWindow = id; }
    else if (strcmp(name, "SetForegroundWindow") == 0) { g_real_SetForegroundWindow = (FARPROC)real_proc; g_id_SetForegroundWindow = id; }
    else if (strcmp(name, "SetWindowPos") == 0) { g_real_SetWindowPos = (FARPROC)real_proc; g_id_SetWindowPos = id; }
    else if (strcmp(name, "CreateWindowExA") == 0) { g_real_CreateWindowExA = (FARPROC)real_proc; g_id_CreateWindowExA = id; }
    else if (strcmp(name, "pthread_create") == 0) { g_real_pthread_create = (FARPROC)real_proc; g_id_pthread_create = id; }
    else if (strcmp(name, "getenv") == 0) { g_real_getenv = (FARPROC)real_proc; g_id_getenv = id; }
}

// ---------------------------------------------------------------------
// Milestone 8: RNG pinning (win32_pilot.md sec 5 "RNG ... wrap: record the
// seed, pin the LCG ... so replay does not depend on the host msvcrt").
//
// Through milestone 7 rand()/srand() were left UNWRAPPED on purpose: the
// effective seed is derived from time() (notes/replay_format.md sec 2),
// which det_wrap_time already pins, and msvcrt's own LCG has no other
// host-entropy input - so replay was already deterministic (measured, 876
// ticks EQUAL). What was still missing for milestone 8 is that the RNG
// STATE lived inside msvcrt.dll's per-thread CRT data, i.e. OUTSIDE every
// region a snapshot can capture (guest image, arena, guest stack). Pinning
// the LCG here moves that state into carrier memory, where it becomes an
// ordinary snapshot component (see snapshot.cpp's CarrierState.rng_state).
//
// KNOWN (win32_pilot.md sec 5, and verified by --rng-selftest below against
// the REAL msvcrt.dll rand() over 1000 values): msvcrt's generator is
//     state = state * 214013 + 2531011;  return (state >> 16) & 0x7fff;
// with the pre-srand default state 1.
// ---------------------------------------------------------------------
static unsigned g_rng_state = 1;      // msvcrt's documented default seed
static long g_rng_calls = 0;          // diagnostics only (report/manifest)
static long g_rng_seeds = 0;

static int rng_next() {
    g_rng_state = g_rng_state * 214013u + 2531011u;
    return (int)((g_rng_state >> 16) & 0x7fffu);
}

extern "C" int __cdecl det_wrap_rand() {
    pf_count_import(g_id_rand);
    if (g_det_mode) { ++g_rng_calls; return rng_next(); }
    if (g_real_rand) return ((int(__cdecl*)())g_real_rand)();
    return 0;
}

extern "C" void __cdecl det_wrap_srand(unsigned seed) {
    pf_count_import(g_id_srand);
    if (g_det_mode) { g_rng_state = seed; ++g_rng_seeds; return; }
    if (g_real_srand) ((void(__cdecl*)(unsigned))g_real_srand)(seed);
}

// Snapshot accessors (snapshot.cpp).
unsigned det_rng_state() { return g_rng_state; }
void det_set_rng_state(unsigned s) { g_rng_state = s; }
long det_rng_calls() { return g_rng_calls; }
void det_set_rng_calls(long n) { g_rng_calls = n; }

// --rng-selftest: the unit check win32_pilot.md's milestone-8 brief asks for.
// Runs AFTER imports_init (so g_real_rand/g_real_srand point at the REAL
// msvcrt.dll entry points the guest would otherwise have used) and BEFORE
// the guest starts; the process exits with 0 on match, 4 on mismatch.
int det_rng_selftest() {
    if (!g_real_rand || !g_real_srand) {
        fprintf(stderr, "det: --rng-selftest: msvcrt rand/srand were not resolved\n");
        return 4;
    }
    typedef int(__cdecl * RandFn)();
    typedef void(__cdecl * SrandFn)(unsigned);
    static const unsigned kSeeds[] = {1u, 12345u, 0u, 2531011u, 0xdeadbeefu};
    int bad = 0, checked = 0;
    for (unsigned seed : kSeeds) {
        ((SrandFn)g_real_srand)(seed);
        unsigned model = seed;
        for (int i = 0; i < 1000; ++i) {
            int real_v = ((RandFn)g_real_rand)();
            model = model * 214013u + 2531011u;
            int model_v = (int)((model >> 16) & 0x7fffu);
            ++checked;
            if (real_v != model_v) {
                if (++bad <= 5)
                    fprintf(stderr, "det: --rng-selftest MISMATCH seed=%u i=%d real=%d model=%d\n",
                            seed, i, real_v, model_v);
            }
        }
    }
    fprintf(stderr, "det: --rng-selftest: %d values across %d seeds, %d mismatch(es) - %s\n",
            checked, (int)(sizeof(kSeeds) / sizeof(kSeeds[0])), bad, bad ? "FAIL" : "OK");
    printf("rng-selftest: %s (%d values, %d mismatches)\n", bad ? "FAIL" : "OK", checked, bad);
    fflush(stdout);
    return bad ? 4 : 0;
}

// ---------------------------------------------------------------------
// A (extended). Deterministic heap arena, det mode only. MEASURED
// (carrier/NOTES.md "Milestones 5-7"): two --det runs of the identical
// script produced byte-different .data/.bss digests from tick 1 even though
// every other observable (log.txt gameplay lines, RNG-driven tower layout)
// matched, traced to log.txt's own "Graphics mode set. (screen = %d)" line
// printing a different raw pointer value each run - Windows randomizes the
// msvcrt heap's base address per PROCESS (independent of image ASLR), and
// that BITMAP* is a msvcrt-heap pointer stored directly in a .bss global.
// Fix, already anticipated by the architecture doc (win32_pilot.md sec 6,
// "redirect [malloc] to a fixed-address arena so heap contents are ordinary
// guest pages"): in det mode, malloc/calloc/realloc/free are redirected to
// a fixed-address bump allocator that never reclaims memory. A leak-only
// allocator is fine here - total allocation volume for a bounded proof run
// is a few MB, and a pure bump pointer is trivially reproducible: once
// every other nondeterminism source (time/clock/QPC/keyboard/timer thread)
// is pinned, the SEQUENCE of malloc calls is itself deterministic, so the
// same sequence of bump offsets - hence the same fixed addresses - comes
// out every run.
//
// ---------------------------------------------------------------------
// Divergence 004 (notes/living_record.md): the bump-only form above is NOT
// viable for anything that sits in the main MENU, which creates and destroys
// a full-screen ~800 KB bitmap EVERY FRAME (~40 MB/s). A human recording
// (replays/second_human) exhausted the whole 256 MiB in ~450 ticks and
// crashed in main_menu_callback on the first failed allocation. Replaced by
// a real allocator, with the two properties the rest of this carrier needs:
//
//   1. DETERMINISTIC given a deterministic call sequence. Explicit
//      doubly-linked free list, FIRST FIT from the head, LIFO insertion,
//      immediate boundary-tag coalescing of both neighbours, and a bump
//      "top" for memory never handed out before. Every one of those steps is
//      a pure function of the call sequence: no addresses, no timestamps, no
//      randomization, no size-class hashing, no per-run policy. Two runs that
//      make the same malloc/free calls in the same order get byte-identical
//      block addresses (this is what G1/G2/G3 verify).
//   2. ALL ALLOCATOR STATE LIVES INSIDE THE ARENA REGION. The control block
//      (top / free-list head / stats) is at arena offset 0, block headers and
//      footers are in the blocks themselves, and the free-list links live in
//      the payload of the free blocks. So the EXISTING snapshot component
//      ("arena", 0x20000000, `arena_offset` bytes) captures the allocator
//      whole, unchanged - no new snapshot component, no new carrier global.
//      `DetSavedState::arena_offset` keeps its meaning ("bytes of the arena
//      that are live"), it is now just read out of the control block.
//
// Layout, all block sizes and block offsets are multiples of 16:
//
//   [0 .. 64)              ArenaCtl        (top, free_head, stats)
//   [64 .. top)            blocks, each:   ArenaHdr(16) payload ArenaFtr(8)
//   [top .. ARENA_SIZE)    never touched   (bump region)
//
// The footer is the Knuth boundary tag that makes backward coalescing O(1);
// the header is 16 bytes rather than 8 so that every payload is 16-aligned
// (msvcrt's own x86 malloc guarantees 8; 16 is a strict superset and keeps
// the arithmetic trivial). Freeing the block that ends exactly at `top` gives
// its bytes back to the bump region instead of the free list, which is what
// keeps `top` - and therefore the snapshot's arena component - bounded by the
// PEAK LIVE footprint rather than by total allocation volume.
// ---------------------------------------------------------------------
#define ARENA_BASE  ((uintptr_t)0x20000000u)
#define ARENA_SIZE  (256u * 1024u * 1024u)
#define ARENA_ALIGN 16u
#define ARENA_HDR   16u
#define ARENA_FTR   8u
#define ARENA_MIN_BLOCK 32u          // hdr(16) + 8 bytes of free-list links + ftr(8)
#define ARENA_CTL_SIZE  64u
#define ARENA_MAGIC      0x50464152u // 'RAFP' - a block handed out to the guest
#define ARENA_MAGIC_FREE 0x46464152u // 'RAFF' - a block on the free list
#define ARENA_CTL_MAGIC  0x50464143u // 'CAFP'

struct ArenaHdr { uint32_t size; uint32_t magic; uint32_t user; uint32_t pad; };
struct ArenaFtr { uint32_t size; uint32_t magic; };
struct ArenaLink { uint32_t next; uint32_t prev; };   // arena offsets; 0 == null
struct ArenaCtl {
    uint32_t magic;
    uint32_t top;          // first byte of the never-yet-used bump region
    uint32_t free_head;    // head of the explicit free list, 0 = empty
    uint32_t hwm;          // high-water mark: the largest `top` ever reached
    uint32_t live_blocks;
    uint32_t live_bytes;   // payload bytes currently handed out
    uint32_t peak_live_bytes;
    uint32_t n_malloc, n_calloc, n_realloc, n_free, n_free_foreign, n_free_bad;
    uint32_t pad[3];
};

static uint8_t* g_arena_base = nullptr;
static CRITICAL_SECTION g_arena_cs;

static inline ArenaCtl*  a_ctl()             { return (ArenaCtl*)g_arena_base; }
static inline ArenaHdr*  a_hdr(uint32_t off) { return (ArenaHdr*)(g_arena_base + off); }
static inline ArenaLink* a_link(uint32_t off){ return (ArenaLink*)(g_arena_base + off + ARENA_HDR); }
static inline ArenaFtr*  a_ftr(uint32_t off, uint32_t size) {
    return (ArenaFtr*)(g_arena_base + off + size - ARENA_FTR);
}
static inline void a_set_block(uint32_t off, uint32_t size, uint32_t magic, uint32_t user) {
    ArenaHdr* h = a_hdr(off);
    h->size = size; h->magic = magic; h->user = user; h->pad = 0;
    ArenaFtr* f = a_ftr(off, size);
    f->size = size; f->magic = magic;
}

static void arena_init() {
    g_arena_base = (uint8_t*)VirtualAlloc((void*)ARENA_BASE, ARENA_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_arena_base) {
        fprintf(stderr, "det: arena VirtualAlloc(0x%08x) FAILED gle=%lu - falling back to the real heap "
                         "(digest equality will NOT hold across runs; see carrier/NOTES.md)\n",
                (unsigned)ARENA_BASE, GetLastError());
        return;
    }
    InitializeCriticalSection(&g_arena_cs);
    ArenaCtl* c = a_ctl();
    memset(c, 0, sizeof(*c));
    c->magic = ARENA_CTL_MAGIC;
    c->top = ARENA_CTL_SIZE;
    c->free_head = 0;
    c->hwm = ARENA_CTL_SIZE;
    fprintf(stderr, "det: deterministic heap arena at %p, size=%uMB (free-list allocator, "
                     "first-fit + coalescing, state in-arena)\n",
            g_arena_base, ARENA_SIZE / (1024u * 1024u));
}

// --- explicit free list (LIFO insert, first-fit search) ------------------
static void fl_insert(uint32_t off) {
    ArenaCtl* c = a_ctl();
    ArenaLink* n = a_link(off);
    n->prev = 0;
    n->next = c->free_head;
    if (n->next) a_link(n->next)->prev = off;
    c->free_head = off;
}
static void fl_remove(uint32_t off) {
    ArenaCtl* c = a_ctl();
    ArenaLink* n = a_link(off);
    if (n->prev) a_link(n->prev)->next = n->next;
    else         c->free_head = n->next;
    if (n->next) a_link(n->next)->prev = n->prev;
}

// Caller holds g_arena_cs.
static uint32_t arena_carve(size_t n) {
    ArenaCtl* c = a_ctl();
    size_t need = ARENA_HDR + n + ARENA_FTR;
    need = (need + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1);
    if (need < ARENA_MIN_BLOCK) need = ARENA_MIN_BLOCK;
    if (need > ARENA_SIZE) return 0;

    // 1. first fit over the free list
    for (uint32_t off = c->free_head; off; off = a_link(off)->next) {
        uint32_t bs = a_hdr(off)->size;
        if (bs < need) continue;
        fl_remove(off);
        if (bs - need >= ARENA_MIN_BLOCK) {          // split; remainder stays free
            a_set_block(off, (uint32_t)need, ARENA_MAGIC, (uint32_t)n);
            uint32_t rest = off + (uint32_t)need;
            a_set_block(rest, bs - (uint32_t)need, ARENA_MAGIC_FREE, 0);
            if (rest + (bs - (uint32_t)need) == c->top) c->top = rest;  // back to the bump region
            else fl_insert(rest);
        } else {
            a_set_block(off, bs, ARENA_MAGIC, (uint32_t)n);
        }
        return off;
    }

    // 2. nothing fits - take fresh bytes from the bump region
    if (c->top + need > ARENA_SIZE) return 0;
    uint32_t off = c->top;
    c->top += (uint32_t)need;
    if (c->top > c->hwm) c->hwm = c->top;
    a_set_block(off, (uint32_t)need, ARENA_MAGIC, (uint32_t)n);
    return off;
}

static void* arena_alloc(size_t n) {
    if (!g_arena_base) return nullptr;
    EnterCriticalSection(&g_arena_cs);
    uint32_t off = arena_carve(n);
    if (!off) {
        ArenaCtl* c = a_ctl();
        unsigned top = c->top, live = c->live_bytes;
        LeaveCriticalSection(&g_arena_cs);
        fprintf(stderr, "det: arena exhausted (requested %zu, top %u/%u, live %u)\n",
                n, top, ARENA_SIZE, live);
        return nullptr;
    }
    ArenaCtl* c = a_ctl();
    c->live_blocks++;
    c->live_bytes += a_hdr(off)->size;
    if (c->live_bytes > c->peak_live_bytes) c->peak_live_bytes = c->live_bytes;
    LeaveCriticalSection(&g_arena_cs);
    return (void*)(g_arena_base + off + ARENA_HDR);
}

// True only for a pointer this allocator actually handed out and that is
// still live. Anything else (NULL, an interior pointer, or a block msvcrt
// allocated internally and handed to the guest) is NOT ours.
static bool arena_owns(void* p) {
    if (!g_arena_base || !p) return false;
    uintptr_t a = (uintptr_t)p;
    if (a < (uintptr_t)g_arena_base + ARENA_CTL_SIZE + ARENA_HDR) return false;
    if (a >= (uintptr_t)g_arena_base + ARENA_SIZE) return false;
    if ((a - (uintptr_t)g_arena_base - ARENA_HDR) % ARENA_ALIGN != 0) return false;
    uint32_t off = (uint32_t)(a - (uintptr_t)g_arena_base - ARENA_HDR);
    return a_hdr(off)->magic == ARENA_MAGIC;
}

static size_t arena_size_of(void* p) {
    if (!arena_owns(p)) return 0;
    uint32_t off = (uint32_t)((uintptr_t)p - (uintptr_t)g_arena_base - ARENA_HDR);
    return a_hdr(off)->user;
}
// Payload bytes actually available in p's block (>= the requested size).
static size_t arena_capacity_of(void* p) {
    uint32_t off = (uint32_t)((uintptr_t)p - (uintptr_t)g_arena_base - ARENA_HDR);
    return a_hdr(off)->size - ARENA_HDR - ARENA_FTR;
}

static void arena_free(void* p) {
    uint32_t off = (uint32_t)((uintptr_t)p - (uintptr_t)g_arena_base - ARENA_HDR);
    EnterCriticalSection(&g_arena_cs);
    ArenaCtl* c = a_ctl();
    ArenaHdr* h = a_hdr(off);
    if (h->magic != ARENA_MAGIC) { c->n_free_bad++; LeaveCriticalSection(&g_arena_cs); return; }
    uint32_t size = h->size;
    c->live_blocks--;
    c->live_bytes -= size;

    // coalesce forward
    uint32_t nxt = off + size;
    if (nxt < c->top && a_hdr(nxt)->magic == ARENA_MAGIC_FREE) {
        fl_remove(nxt);
        size += a_hdr(nxt)->size;
    }
    // coalesce backward through the previous block's boundary tag
    if (off > ARENA_CTL_SIZE) {
        ArenaFtr* pf = (ArenaFtr*)(g_arena_base + off - ARENA_FTR);
        if (pf->magic == ARENA_MAGIC_FREE && pf->size <= off - ARENA_CTL_SIZE) {
            uint32_t prev = off - pf->size;
            fl_remove(prev);
            off = prev;
            size += pf->size;
        }
    }
    a_set_block(off, size, ARENA_MAGIC_FREE, 0);
    if (off + size == c->top) c->top = off;   // give the tail back to the bump region
    else fl_insert(off);
    LeaveCriticalSection(&g_arena_cs);
}

// Arena statistics, for the shutdown line and the --report JSON. Zero when
// the arena is not active (non-det runs).
void det_arena_stats(unsigned* top, unsigned* hwm, unsigned* live_bytes,
                     unsigned* peak_live_bytes, unsigned* live_blocks) {
    unsigned z = 0;
    if (top) *top = 0; if (hwm) *hwm = 0; if (live_bytes) *live_bytes = 0;
    if (peak_live_bytes) *peak_live_bytes = 0; if (live_blocks) *live_blocks = 0;
    if (!g_arena_base) return;
    ArenaCtl* c = a_ctl();
    (void)z;
    if (top) *top = c->top;
    if (hwm) *hwm = c->hwm;
    if (live_bytes) *live_bytes = c->live_bytes;
    if (peak_live_bytes) *peak_live_bytes = c->peak_live_bytes;
    if (live_blocks) *live_blocks = c->live_blocks;
}
unsigned det_arena_top() { return g_arena_base ? a_ctl()->top : 0; }

extern "C" void* __cdecl det_wrap_malloc(size_t n) {
    pf_count_import(g_id_malloc);
    if (g_det_mode && g_arena_base) { a_ctl()->n_malloc++; return arena_alloc(n); }
    return g_real_malloc ? ((void*(__cdecl*)(size_t))g_real_malloc)(n) : nullptr;
}
extern "C" void* __cdecl det_wrap_calloc(size_t count, size_t size) {
    pf_count_import(g_id_calloc);
    if (g_det_mode && g_arena_base) {
        a_ctl()->n_calloc++;
        size_t n = count * size;
        void* p = arena_alloc(n);
        // MUST zero explicitly now: unlike the bump-only form, a block can be
        // recycled memory, not a fresh (already-zero) VirtualAlloc page.
        if (p && n) memset(p, 0, n);
        return p;
    }
    return g_real_calloc ? ((void*(__cdecl*)(size_t, size_t))g_real_calloc)(count, size) : nullptr;
}
extern "C" void* __cdecl det_wrap_realloc(void* p, size_t n) {
    pf_count_import(g_id_realloc);
    if (g_det_mode && g_arena_base) {
        a_ctl()->n_realloc++;
        if (!p) return arena_alloc(n);                 // realloc(NULL, n) == malloc(n)
        if (!arena_owns(p)) return arena_alloc(n);     // foreign pointer: same as before this change
        size_t cap = arena_capacity_of(p);
        if (n <= cap) {                                // fits in place - msvcrt may do this too
            uint32_t off = (uint32_t)((uintptr_t)p - (uintptr_t)g_arena_base - ARENA_HDR);
            a_hdr(off)->user = (uint32_t)n;
            return p;
        }
        size_t old = arena_size_of(p);
        void* np = arena_alloc(n);
        if (!np) return nullptr;                       // msvcrt: original block stays valid
        if (old) memcpy(np, p, old < n ? old : n);     // growth copies
        arena_free(p);
        return np;
    }
    return g_real_realloc ? ((void*(__cdecl*)(void*, size_t))g_real_realloc)(p, n) : nullptr;
}
extern "C" void __cdecl det_wrap_free(void* p) {
    pf_count_import(g_id_free);
    if (g_det_mode && g_arena_base) {
        a_ctl()->n_free++;
        if (!p) return;                     // free(NULL) is a no-op
        if (!arena_owns(p)) {               // msvcrt-internal block, or already freed:
            a_ctl()->n_free_foreign++;      // leak it, exactly as the bump allocator did
            return;
        }
        arena_free(p);
        return;
    }
    if (g_real_free) ((void(__cdecl*)(void*))g_real_free)(p);
}

// The single carrier tick clock (part A: "virtual clock defines T"). Works
// in BOTH modes so the same --input-script and the digest sensor behave
// identically whether or not --det is given - this is what makes the
// milestone-7 "run twice without --det, streams differ" negative control
// meaningful (real Sleep jitter + real thread scheduling reach T instead of
// our pinned arithmetic). det mode: T from the virtual clock. Non-det:
// T from real elapsed wall time (GetTickCount64) - approximate, diagnostic
// only, never claimed deterministic.
static LONGLONG det_now_ms() {
    return g_det_mode ? g_virtual_ms : (LONGLONG)(GetTickCount64() - g_start_tick64);
}
static int det_current_tick() { return (int)(det_now_ms() / 20); }

// Public alias (det.hpp) - bind.cpp keys its per-invocation records on the
// same T the per-tick digest lines use.
int det_tick() { return det_current_tick(); }

// --trace-input bookkeeping, called from det_wrap_Sleep once the clock for
// this Sleep call has advanced: `sub` counts Sleep calls within the tick T
// that this call belongs to (0 == the first Sleep call of a new tick).
static void sub_tick_advance() {
    int t_now = det_current_tick();
    if (t_now != g_sub_tick) { g_sub_tick = t_now; g_sub_in_tick = 0; }
    else ++g_sub_in_tick;
}

// ---------------------------------------------------------------------
// B. Input script
// ---------------------------------------------------------------------
// "Environment isolation" pass: a script/recording is no longer keys only.
// Every channel the carrier owns is recorded and replayed in the SAME file,
// at the SAME tick-boundary handover point (divergence 005's rule), with one
// line shape per channel:
//     T press|release KEY_NAME|<scancode>     keyboard   (unchanged)
//     T switch in|out                         window activation (item 2)
//     T time <seconds>                        wall clock (item 4)
// `time` events are NOT delivered at a tick boundary - they are CONSUMED by
// det_wrap_time when the guest calls time(), in recorded order - so they live
// in their own vector (g_time_values) instead of g_script.
enum class EvKind { Press, Release, SwitchIn, SwitchOut };
struct ScriptEvent { int tick; EvKind kind; int scancode; };
static std::vector<ScriptEvent> g_script;
static size_t g_script_cursor = 0;
struct TimeEvent { int tick; long value; };
static std::vector<TimeEvent> g_time_values;
static size_t g_time_cursor = 0;

static int resolve_key(const char* tok) {
    for (const KeyName& k : kKeyNames)
        if (_stricmp(k.name, tok) == 0) return k.code;
    return atoi(tok);
}

// item 2 diagnostic ONLY: key_dinput_handle_scancode's own "scancode"
// argument is NOT the Allegro internal code kKeyNames/g_script use (that's
// what _handle_key_press/_handle_key_release take) - it is the RAW
// DirectInput DIK_* hardware scancode, translated through the game's own
// `_hw_to_mycode[256]` table (wkeybd.c) before it reaches
// _handle_key_press/_handle_key_release. MEASURED by reading
// _hw_to_mycode's actual bytes out of assets/icytower15.exe at its DWARF/
// COFF-confirmed VA (0x4daf80, see disasm around 0x46d660/0x46d71e which
// index it with `movzbl 0x4daf80(%ebx),%ebx`): hw_to_mycode[0x01]==59,
// [0x1c]==67, [0x39]==75, [0xcb]==82, [0xcd]==83, [0xc8]==84, [0xd0]==85 -
// i.e. exactly the standard PC/AT scancode-set-1 DIK_* values for these 7
// keys, confirmed against kKeyNames' Allegro codes one for one. First
// attempt at --inject-real-test fed the Allegro code directly as
// key_dinput_handle_scancode's scancode argument (wrong - it double-
// translates through _hw_to_mycode[allegro_code], landing on an unrelated
// key) and it corrupted enough internal state to leak the deterministic
// heap arena empty within a few hundred ticks (a real, reproduced failure,
// not a hypothetical) - fixed by translating to the DIK code here instead.
// GENERATED at runtime (item 2, "tick-boundary real input" pass), not
// hand-listed: read directly out of the mapped guest image's own
// hw_to_mycode[256] table (VA_HW_TO_MYCODE) the first time it's needed -
// safe any time after pe_image_load has mapped the guest (main.cpp: always
// true by the time any tick is delivered). hw_to_mycode[dik] IS the
// DIK->Allegro direction already, read directly, no table needed for that
// side (see dik_to_allegro below); allegro_to_dik is built once as its
// inverse, first occurrence wins for any Allegro code with more than one
// DIK alias. Superset of the old 7-entry hand-written kDikMap (ESC/ENTER/
// SPACE/arrows verified to match it exactly - see carrier/NOTES.md), so
// every existing --inject-real-test script keeps working unchanged, and any
// OTHER key used in a future script gets a mapping automatically instead of
// needing kDikMap hand-edited (the old, now-removed limitation).
static int g_allegro_to_dik[128];
static bool g_dik_tables_built = false;

static void build_dik_tables() {
    if (g_dik_tables_built) return;
    for (int i = 0; i < 128; ++i) g_allegro_to_dik[i] = -1;
    const unsigned char* hw_to_mycode = (const unsigned char*)(uintptr_t)VA_HW_TO_MYCODE;
    int mapped = 0;
    for (int dik = 0; dik < 256; ++dik) {
        int allegro = hw_to_mycode[dik];
        if (allegro > 0 && allegro < 128 && g_allegro_to_dik[allegro] < 0) {
            g_allegro_to_dik[allegro] = dik;
            ++mapped;
        }
    }
    g_dik_tables_built = true;
    fprintf(stderr, "det: built Allegro->DIK table from the guest's own hw_to_mycode[256] "
                     "(VA=0x%08x): %d of 128 possible Allegro codes have a DIK mapping\n",
            VA_HW_TO_MYCODE, mapped);
}

static int allegro_to_dik(int allegro_code) {
    build_dik_tables();
    if (allegro_code < 0 || allegro_code >= 128) return -1;
    return g_allegro_to_dik[allegro_code]; // -1 = no mapping - see deliver_due_input's inject_real_test branch
}

// Forward direction for item 2's real-input capture path below: the guest's
// own table gives this directly, no inversion needed.
static int dik_to_allegro(int dik_code) {
    if (dik_code < 0 || dik_code > 255) return 0;
    const unsigned char* hw_to_mycode = (const unsigned char*)(uintptr_t)VA_HW_TO_MYCODE;
    return hw_to_mycode[dik_code];
}

// Reverse of resolve_key, for --record-input: emit the same KEY_NAME tokens
// --input-script reads, not raw scancodes, so a recorded file is exactly the
// format --input-script parses (falls back to the raw number for a scancode
// outside the 7-name table - still valid input, since resolve_key's own
// fallback is atoi()).
static const char* scancode_to_name(int sc) {
    for (const KeyName& k : kKeyNames)
        if (k.code == sc) return k.name;
    return nullptr;
}

// item 2 (win32_pilot.md / carrier/NOTES.md "Input policy and recording"):
// DWARF-confirmed prototype (artifacts/dwarf_info.txt, wkeybd.c line 321):
// void key_dinput_handle_scancode(int scancode, int pressed) - but KNOWN
// (disasm at 0x46d5a8, carrier/NOTES.md) both args arrive in registers
// (AL/EAX=scancode, EDX=pressed), never on the stack, so a plain C
// function-pointer cast (which would push cdecl stack args) cannot call it
// correctly. This naked shim loads the two cdecl stack args (how ITS OWN
// caller, i.e. deliver_due_input below, passes them) into EAX/EDX and calls
// the real function directly - `call ecx` with the absolute address in ecx
// is a normal direct call, no memory indirection. Only reachable in
// --inject-real-test (a diagnostic option; normal Script-mode delivery
// bypasses key_dinput_handle_scancode entirely, calling
// _handle_key_press/_handle_key_release directly, same as before).
// A plain global (not a literal inside the __asm block - MASM inline asm
// doesn't accept the C `0x...u` suffix VA_KEY_DINPUT_SCANCODE expands to) so
// the naked function below can `mov ecx, kKeyDinputVA` (loads the stored
// value, since MASM treats a bare identifier as a memory operand) and then
// `call ecx` - a register-indirect call to that address, equivalent to a
// direct call to the literal VA.
static const DWORD kKeyDinputVA = VA_KEY_DINPUT_SCANCODE;

extern "C" void __declspec(naked) __cdecl call_key_dinput_handle_scancode(int scancode, int pressed) {
    __asm {
        mov eax, [esp+4]
        mov edx, [esp+8]
        mov ecx, kKeyDinputVA
        call ecx
        ret
    }
}

static void load_script(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "det: could not open --input-script '%s'\n", path); return; }
    char line[256];
    long bad = 0;
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == '\n' || *p == 0 || *p == '\r') continue;
        int tick; char verb[16]; char arg[32];
        if (sscanf(p, "%d %15s %31s", &tick, verb, arg) != 3) continue;
        if (_stricmp(verb, "time") == 0) {
            TimeEvent t; t.tick = tick; t.value = atol(arg);
            g_time_values.push_back(t);
            continue;
        }
        ScriptEvent e;
        e.tick = tick;
        e.scancode = 0;
        if (_stricmp(verb, "switch") == 0) {
            if (_stricmp(arg, "in") == 0) e.kind = EvKind::SwitchIn;
            else if (_stricmp(arg, "out") == 0) e.kind = EvKind::SwitchOut;
            else { ++bad; continue; }
        } else if (_stricmp(verb, "press") == 0) {
            e.kind = EvKind::Press; e.scancode = resolve_key(arg);
        } else if (_stricmp(verb, "release") == 0) {
            e.kind = EvKind::Release; e.scancode = resolve_key(arg);
        } else {
            ++bad; continue;
        }
        g_script.push_back(e);
    }
    fclose(f);
    // stable_sort, not sort: two events at the SAME tick must keep their file
    // order (a `switch out` immediately followed by `switch in` at one tick is
    // a real recording shape, and swapping them would invert the focus state).
    std::stable_sort(g_script.begin(), g_script.end(),
                     [](const ScriptEvent& a, const ScriptEvent& b) { return a.tick < b.tick; });
    if (bad) {
        fprintf(stderr, "det: FATAL - %ld unrecognized event line(s) in '%s' "
                        "(expected `T press|release KEY`, `T switch in|out`, `T time <secs>`)\n",
                bad, path);
        exit(2); // fail loudly: a silently-skipped event is an unreplayable recording
    }
    fprintf(stderr, "det: loaded %zu input events and %zu recorded time value(s) from '%s'\n",
            g_script.size(), g_time_values.size(), path);
}

// ---------------------------------------------------------------------
// "Environment isolation" pass, item 2: window activation as a controlled
// channel.
//
// THE CHANNEL, MEASURED (all addresses cited at the #defines at the top of
// this file): a foreign window taking the foreground makes Windows send
// WM_ACTIVATEAPP to the guest's own window thread; directx_wnd_proc
// (0x4791e0) calls _win_switch_out/_win_switch_in (wdispsw.c), each of which
// ends in a tail `jmp` to _switch_out/_switch_in (dispsw.c) - a bare loop
// over an 8-entry callback table. Three of those entries are the game's own
// switchedFromProgram/switchedToProgram (main.c, recovered verbatim in
// src/icytower/main_state.c: `hasFocus = 0;` / `hasFocus = 1;`). hasFocus
// (0x4bc020) and lastFocus (0x4bc024) are both inside the 151-global digest
// domain, and play() compares them at 0x411c6b/0x411cd7 and restarts the
// game music (writing checkMusicVoiceID @0x4bc174, also in the domain) when
// they differ. So the operator's desktop CAN change the verdict.
//
// WHY NOT set_display_switch_mode (the other option the task offered): RULED
// OUT BY EVIDENCE. _win_switch_out's disassembly branches on
// get_display_switch_mode only to decide whether to ALSO reset an event and
// drop the thread priority; BOTH arms end in the same `jmp _switch_out`, so
// no switch mode - SWITCH_NONE, SWITCH_BACKGROUND or otherwise - stops the
// callbacks. (The game already runs in SWITCH_BACKGROUND: _win_reset_switch_
// mode at 0x47a508 calls set_display_switch_mode(3).) The callback
// dispatchers are the only real choke point, so that is where the carrier
// takes ownership.
//
// MECHANISM: a 5-byte `jmp rel32` entry patch (the same technique bind.cpp
// already uses for LIFTED/NATIVE forms) to a carrier stub, NOT a hardware
// breakpoint - the DR budget is fully spoken for (DR0 safepoint, DR1
// key_dinput, DR2/DR3 reserved for bind.cpp's ORIGINAL-form sensing, which
// gate G2 needs). Both patched functions are `void f(void)` reached by a tail
// jmp, so the stub's plain `ret` is convention-correct.
//
//   --input=script|none : the stub counts and returns. NOTHING reaches the
//                         game - the operator cannot perturb the run.
//   --input=real        : the stub counts and QUEUES the event; it is handed
//                         to the game from the main thread at the next tick
//                         boundary (the same handover point keys use -
//                         divergence 005's rule) by running the guest's own
//                         callback table, and written to --record-input as
//                         `T switch in|out`.
//   replay of such a recording (--input=script) : the `T switch in|out` lines
//                         are delivered at their tick through the same call.
// ---------------------------------------------------------------------

// Byte-for-byte what _switch_in/_switch_out do (their disassembly is quoted
// in carrier/NOTES.md): call every non-null entry of the guest's own 8-slot
// callback table, in index order. Used for DELIVERY in both modes, so the
// patched originals are never re-entered.
static void run_switch_callbacks(bool switch_in) {
    typedef void(__cdecl * CbFn)(void);
    CbFn* tab = (CbFn*)(uintptr_t)(switch_in ? VA_SWITCH_IN_CB : VA_SWITCH_OUT_CB);
    for (int i = 0; i < 8; ++i)
        if (tab[i]) tab[i]();
}

static const int kSwitchQueueCap = 64;
static unsigned char g_switch_queue[kSwitchQueueCap];
static int g_switch_head = 0, g_switch_tail = 0;
static CRITICAL_SECTION g_switch_cs;
static bool g_switch_cs_inited = false;

static void switch_queue_init() {
    if (!g_switch_cs_inited) { InitializeCriticalSection(&g_switch_cs); g_switch_cs_inited = true; }
}
static void switch_queue_push(bool switch_in) {
    switch_queue_init();
    EnterCriticalSection(&g_switch_cs);
    int next = (g_switch_tail + 1) % kSwitchQueueCap;
    if (next != g_switch_head) {
        g_switch_queue[g_switch_tail] = switch_in ? 1 : 0;
        g_switch_tail = next;
    } else {
        fprintf(stderr, "det: switch-event queue FULL, dropping a switch %s event\n",
                switch_in ? "in" : "out");
    }
    LeaveCriticalSection(&g_switch_cs);
}
static bool switch_queue_pop(bool* switch_in) {
    if (!g_switch_cs_inited) return false;
    bool got = false;
    EnterCriticalSection(&g_switch_cs);
    if (g_switch_head != g_switch_tail) {
        *switch_in = g_switch_queue[g_switch_head] != 0;
        g_switch_head = (g_switch_head + 1) % kSwitchQueueCap;
        got = true;
    }
    LeaveCriticalSection(&g_switch_cs);
    return got;
}

// The two entry-patch stubs. Called (jumped to) from the guest's WINDOW
// thread, with the guest stack and the caller's return address at [esp] -
// a normal cdecl void(void) frame, which is exactly what MSVC emits here.
static void switch_hook(bool switch_in) {
    if (g_input_policy == InputPolicy::Real) {
        ++g_switch_captured;
        switch_queue_push(switch_in);
        trace_input("_switch_in/out(capture,window thread)",
                    switch_in ? "switch-in" : "switch-out", switch_in ? 1 : 0, "phase=capture");
    } else {
        ++g_switch_suppressed;
        if (g_switch_suppressed <= 20)
            fprintf(stderr, "det: T=%d SUPPRESSED window switch %s (input=%s; the operator's desktop "
                            "may not reach the guest in an automated run)\n",
                    det_current_tick(), switch_in ? "in" : "out", input_policy_name(g_input_policy));
    }
}
extern "C" void __cdecl det_stub_switch_in() { switch_hook(true); }
extern "C" void __cdecl det_stub_switch_out() { switch_hook(false); }

// Item 3: the DirectInput MOUSE, parked exactly the way the keyboard is.
//
// CENSUS (the task's "find the readers of mouse_x/mouse_y/mouse_b in game
// code"): across the WHOLE binary, game-owned code reads them in exactly ONE
// function - main_menu_callback (main.c) - 4x mouse_b (0x4e8cf8), 1x mouse_x
// (0x4e8ce8), 1x mouse_y (0x4e8cec), writing lastMouseB (0x4dd268, which IS
// in the digest domain). Every other reader is Allegro's own (mouse.c,
// gui.c's default_mouse_*). And that one reader sits INSIDE a branch guarded
// by `pFLDAd != 0` (0x410140): it is the click hit-test on the fetched AD
// BANNER. So the mouse reaches game state only through the ad, which item 5
// suppresses in --det anyway. Decision (documented rather than assumed):
// PARK the mouse in every carrier-owned run, in both record and script mode -
// there is nothing worth recording, and parking it removes the second of the
// two suspects "Divergences 004 and 005" left open.
extern "C" void __cdecl det_stub_handle_mouse_input() { ++g_mouse_parked; }

// 5-byte `jmp rel32` entry patch, same technique/protections as bind.cpp's
// (VirtualProtect + FlushInstructionCache even though the image is mapped RWX,
// so the patch keeps working when that TEMPORARY is retired). Fails loudly.
static void patch_entry_jmp(DWORD_PTR va, void* target, const char* what) {
    unsigned char* p = (unsigned char*)va;
    intptr_t rel = (intptr_t)target - (intptr_t)(va + 5);
    if (rel > 0x7fffffff || rel < -0x7fffffff) {
        fprintf(stderr, "det: FATAL - %s stub is out of jmp rel32 range of 0x%08x\n", what, (unsigned)va);
        exit(3);
    }
    DWORD old = 0;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) {
        fprintf(stderr, "det: FATAL - VirtualProtect(%s @0x%08x) failed gle=%lu\n",
                what, (unsigned)va, GetLastError());
        exit(3);
    }
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)rel;
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    fprintf(stderr, "det: %s @0x%08x -> carrier stub (5-byte jmp rel32)\n", what, (unsigned)va);
}

// Called from the Sleep wrapper (main thread, both modes) right after the
// clock advances. keycode is passed as 0 for every synthetic event: KNOWN
// (disasm of key_dinput_handle_scancode, 0x46d7a7/0x46d7ac) the real
// DirectInput path sometimes passes -1 ("no ASCII") too, and only the
// scancode-indexed key[] array (read by poll_control/is_left/is_right/...,
// see notes/replay_format.md sec 1) drives menu+gameplay input - keycode
// feeds Allegro's separate ASCII/readkey() text-entry API, unused here.
// DET_INPUT_DELIVER_SUB=k (diagnostic env var, same opt-in style as
// DET_DUMP_MEM_TICK below): hold every scripted event back until Sleep call
// number k WITHIN its tick instead of the tick's first Sleep call (sub=0).
// This is the negative control for the divergence-005 root cause: if the
// SUB-TICK position of the handover - not the tick index - is what decides
// which game tick sees the key, then k>=1 must move the whole run one game
// tick later, and k=0 must reproduce the baseline exactly. MEASURED: it does
// (carrier/NOTES.md "Divergence 005").
static int deliver_sub_slot() {
    static int slot = -1;
    if (slot < 0) {
        char buf[16];
        slot = GetEnvironmentVariableA("DET_INPUT_DELIVER_SUB", buf, sizeof(buf)) ? atoi(buf) : 0;
    }
    return slot;
}

// .itr workload investigation (2026-09-07): the real DirectInput driver
// (third_party/allegro-4.4.1/src/win/wkeybd.c handle_key_press, lines
// ~250-292) does NOT pass a constant 0 as _handle_key_press's first
// argument ("unicode" in that file, "keycode" here) - it calls
// ToAscii(vkey,...) and passes the RESULT. For a non-printable key
// (arrows, function keys: mycode < KEY_MODIFIERS and ToAscii yields
// nothing) that result IS 0 - exactly what this synthetic injection
// already sent, which is why ordinary gameplay input (is_left/is_right/...
// reading the scancode-indexed key[] array, set unconditionally at
// _handle_key_press's very first line regardless of this argument) was
// never affected by the shortcut. But for KEY_ENTER/KEY_SPACE/KEY_ESC,
// real ToAscii() returns a genuine ASCII character ('\r'=13, ' '=32,
// ESC=27), and this file's own header comment ("keycode feeds Allegro's
// separate ASCII/readkey() text-entry API, unused here") was wrong for
// any caller that reads THAT api: Allegro's readkey()-buffered queue
// (keyboard.c add_key(&key_buffer,...)) receives an event with ascii=0
// instead of the real one whenever this carrier injects one of those
// three keys. carrier/scripts/play_itr.txt's replay_selector confirm
// handler dispatches through exactly that queue (keypressed()/readkey(),
// not the key[] array) - passing an always-0 ascii is why its confirm
// never fired. Restores the real value for every key this carrier's
// script format can name (kKeyNames above); 0 (unchanged) for the rest.
static int ascii_for_allegro_code(int code) {
    switch (code) {
        case 67: return 13;  // KEY_ENTER -> '\r' (ToAscii(VK_RETURN))
        case 75: return 32;  // KEY_SPACE -> ' '  (ToAscii(VK_SPACE))
        case 59: return 27;  // KEY_ESC   -> ESC  (ToAscii(VK_ESCAPE))
        default: return 0;   // arrows/etc: real ToAscii() also yields 0 here
    }
}

static void deliver_due_input() {
    if (g_script.empty()) return;
    if (g_sub_in_tick != deliver_sub_slot()) return;
    int T = det_current_tick();
    typedef void(__cdecl * PressFn)(int, int);
    typedef void(__cdecl * ReleaseFn)(int);
    while (g_script_cursor < g_script.size() && g_script[g_script_cursor].tick <= T) {
        const ScriptEvent& e = g_script[g_script_cursor];
        // "Environment isolation" item 2: a recorded window-activation event
        // is delivered here, at the same tick-boundary handover point keys
        // use, by running the guest's own switch callback table - never by
        // re-entering the patched _switch_in/_switch_out.
        if (e.kind == EvKind::SwitchIn || e.kind == EvKind::SwitchOut) {
            bool in = (e.kind == EvKind::SwitchIn);
            typedef void(__cdecl * VoidFn)(void);
            // g_in_delivery is set across the WHOLE switch handover, not just
            // around a key call: Allegro's own _win_switch_out releases every
            // held key (key_dinput_unacquire -> _handle_key_release) on its way
            // to the callbacks. MEASURED: without this, a recording made
            // through the real path replayed one tick later diverged at T=301
            // of the newgame_switch workload, because the record run's
            // switch-out released the held KEY_RIGHT and the replay's did not.
            // With the flag set, those releases are recorded as ordinary key
            // events at the same tick and the replay reproduces them.
            g_in_delivery = true;
            if (g_inject_real_test) {
                // Feed it through Allegro's REAL _win_switch_in/_win_switch_out,
                // whose tail jmp lands on the patched dispatcher - so the
                // carrier's capture hook sees it exactly as a genuine
                // WM_ACTIVATE would. Used to exercise the record path for the
                // switch-OUT direction, which this host's OS never delivers
                // (notes/determinism_audit.md).
                ((VoidFn)(void*)(in ? VA_WIN_SWITCH_IN : VA_WIN_SWITCH_OUT))();
            } else {
                run_switch_callbacks(in);
                ++g_switch_delivered;
            }
            g_in_delivery = false;
            fprintf(stderr, "det: T=%d delivered switch %s (from script%s)\n", T, in ? "in" : "out",
                    g_inject_real_test ? ", via _win_switch_in/out, --inject-real-test" : "");
            trace_input("deliver_due_input(Sleep,after _handle_timer_tick)",
                        in ? "switch-in" : "switch-out", in ? 1 : 0,
                        g_inject_real_test ? "phase=deliver via=_win_switch_in/out"
                                           : "phase=deliver via=switch_cb_table");
            ++g_script_cursor;
            continue;
        }
        bool press = (e.kind == EvKind::Press);
        if (g_inject_real_test) {
            // item 2 diagnostic: feed through the REAL DirectInput path's own
            // entry point instead of calling _handle_key_press/_handle_key_release
            // directly, so --record-input's breakpoints (also at those two
            // functions) see the event exactly as they would from a live
            // human keystroke - only reachable with input_policy==Real (the
            // neutralize-keyboard breakpoint is not installed there, so the
            // real function actually runs instead of being no-op'd). Must
            // translate g_script's Allegro-internal code to the raw DIK code
            // key_dinput_handle_scancode itself expects (see allegro_to_dik).
            int dik = allegro_to_dik(e.scancode);
            if (dik < 0) {
                fprintf(stderr, "det: T=%d --inject-real-test has no DIK mapping for Allegro "
                                 "scancode=%d, skipping (not present in the guest's own "
                                 "hw_to_mycode[256] table - see build_dik_tables)\n",
                        T, e.scancode);
                ++g_script_cursor;
                continue;
            }
            g_in_delivery = true;
            call_key_dinput_handle_scancode(dik, press ? 1 : 0);
            g_in_delivery = false;
        } else if (press) {
            g_in_delivery = true;
            ((PressFn)(void*)VA_HANDLE_KEY_PRESS)(ascii_for_allegro_code(e.scancode), e.scancode);
            g_in_delivery = false;
        } else {
            g_in_delivery = true;
            ((ReleaseFn)(void*)VA_HANDLE_KEY_RELEASE)(e.scancode);
            g_in_delivery = false;
        }
        fprintf(stderr, "det: T=%d delivered %s scancode=%d%s\n", T, press ? "press" : "release", e.scancode,
                g_inject_real_test ? " (via key_dinput_handle_scancode, --inject-real-test)" : "");
        trace_input("deliver_due_input(Sleep,after _handle_timer_tick)",
                    press ? "press" : "release", e.scancode,
                    g_inject_real_test ? "via=key_dinput_handle_scancode" : "via=_handle_key_press/release");
        ++g_script_cursor;
    }
}

// ---------------------------------------------------------------------
// Item 2 ("tick-boundary real input" pass, carrier/NOTES.md; divergence 002,
// notes/living_record.md): with --input=real, real key events used to reach
// Allegro's key[] state DIRECTLY from key_dinput_handle_scancode, called
// from the real window thread's message pump - i.e. at whatever real,
// asynchronous instant DirectInput/the window proc happened to run,
// completely independent of the main thread's 20ms tick loop. A
// --record-input recording made that way logs T = det_current_tick() read
// at that same asynchronous instant, which can land either just BEFORE or
// just AFTER the main thread's own tick boundary relative to the event's
// "true" tick - a MEASURED +-1 tick error per event (divergence 002: replay
// of a real human recording first differed at the very first gameplay
// tick). Replay, by construction, always injects at an exact tick boundary
// (deliver_due_input above), so record and replay were using two DIFFERENT
// delivery mechanisms with two different timing sources - not reproducible
// even in principle.
//
// Fix: capture the raw event at the SAME breakpoint (key_dinput_handle_
// scancode's entry) instead of letting it run, queue it, and NEUTRALIZE the
// original call exactly like neutralize_keyboard_hit already does for
// Script/None mode (same mechanism, different intent: here the event is
// preserved, not dropped). The queued events are then drained and delivered
// through _handle_key_press/_handle_key_release - the EXACT SAME function
// calls, from the EXACT SAME call site (drain_real_key_queue, called
// immediately after deliver_due_input from det_wrap_Sleep on the main
// thread) that scripted replay already uses - so the recording is now
// produced by the very path that replays it, and --record-input's
// breakpoints (also at _handle_key_press/_handle_key_release, unchanged)
// see the delivery-time T, not the arrival-time T. Worst-case added
// latency: one tick (20ms in --det), since the queue is drained once per
// Sleep call.
//
// Only installed for input_policy==Real AND !inject_real_test (det_init
// below) - --inject-real-test deliberately keeps the OLD unneutralized
// behavior (its whole point is exercising the real function body's own
// auto-repeat semantics; carrier/NOTES.md "Input policy and recording" part
// C documents that finding and it must keep working unchanged - re-verified
// after this pass, see carrier/NOTES.md).
// ---------------------------------------------------------------------
// Item C (exit-time storm): Allegro's keyboard shutdown (key_dinput_exit ->
// its "release everything that could be down" sweep) drives
// key_dinput_handle_scancode once for EVERY DIK code, all inside a single
// carrier tick. With --input=real that produced hundreds of individually
// printed "no Allegro mapping ... dropped" lines plus a burst of "capture
// queue full" lines - pure noise that buried the real output of a session.
//
// The events themselves are still handled exactly as before (unmapped ones
// dropped, over-capacity ones dropped, the recording-hygiene rule in
// keyrelease_record_hit unchanged and untouched); only the LOGGING is
// aggregated: counts are accumulated per tick and emitted as ONE summary
// line when the tick advances (from drain_real_key_queue, main thread) or at
// det_shutdown. A single isolated event still prints its own detail, so the
// diagnostic value of the message is not lost for the non-storm case.
// ---------------------------------------------------------------------
static const int kRealQueueCap = 256;   // capacity of the real-key capture ring buffer below
static CRITICAL_SECTION g_storm_cs;
static bool g_storm_cs_inited = false;
static int  g_storm_tick = -1;
static unsigned g_storm_nomap = 0;      // events with hw_to_mycode[dik]==0
static unsigned g_storm_qfull = 0;      // events dropped because the ring buffer was full
static unsigned g_storm_delivered = 0;  // events accepted in the same tick (context for the summary)
static unsigned char g_storm_seen[256]; // which DIK codes appeared, for the distinct count
static int g_storm_first_dik = -1, g_storm_last_dik = -1;

static void storm_init() {
    if (!g_storm_cs_inited) { InitializeCriticalSection(&g_storm_cs); g_storm_cs_inited = true; }
}

// Emits the summary for whatever tick is currently accumulating, if any.
// Safe to call from any thread and more than once.
static void exit_storm_flush() {
    if (!g_storm_cs_inited) return;
    EnterCriticalSection(&g_storm_cs);
    unsigned nomap = g_storm_nomap, qfull = g_storm_qfull, delivered = g_storm_delivered;
    int T = g_storm_tick, first = g_storm_first_dik, last = g_storm_last_dik;
    unsigned distinct = 0;
    for (int i = 0; i < 256; ++i) if (g_storm_seen[i]) ++distinct;
    g_storm_nomap = g_storm_qfull = g_storm_delivered = 0;
    g_storm_tick = -1; g_storm_first_dik = g_storm_last_dik = -1;
    memset(g_storm_seen, 0, sizeof(g_storm_seen));
    LeaveCriticalSection(&g_storm_cs);
    if (!nomap && !qfull) return;
    if (nomap + qfull == 1 && nomap == 1) {
        fprintf(stderr, "det: T=%d real key event dik=0x%02x has no Allegro mapping "
                         "(hw_to_mycode[dik]==0), dropped\n", T, (unsigned)first);
    } else if (nomap + qfull == 1) {
        fprintf(stderr, "det: T=%d real-input capture queue full (%d events), dropped one\n",
                T, kRealQueueCap);
    } else {
        fprintf(stderr, "det: T=%d real-key event storm collapsed: %u dropped (%u unmapped, "
                         "%u queue-full), %u delivered, %u distinct DIK codes 0x%02x..0x%02x - "
                         "this is Allegro's keyboard-shutdown release sweep, not gameplay input\n",
                T, nomap + qfull, nomap, qfull, delivered, distinct,
                (unsigned)(first < 0 ? 0 : first), (unsigned)(last < 0 ? 0 : last));
    }
}

// kind: 0 = delivered, 1 = no Allegro mapping, 2 = capture queue full.
static void storm_note(int T, int dik, int kind) {
    storm_init();
    bool need_flush = false;
    EnterCriticalSection(&g_storm_cs);
    if (g_storm_tick != T && g_storm_tick != -1) need_flush = true;
    LeaveCriticalSection(&g_storm_cs);
    if (need_flush) exit_storm_flush();
    EnterCriticalSection(&g_storm_cs);
    g_storm_tick = T;
    if (dik >= 0 && dik < 256) {
        g_storm_seen[dik] = 1;
        if (g_storm_first_dik < 0) g_storm_first_dik = dik;
        g_storm_last_dik = dik;
    }
    if (kind == 0) g_storm_delivered++;
    else if (kind == 1) g_storm_nomap++;
    else g_storm_qfull++;
    LeaveCriticalSection(&g_storm_cs);
}

struct RealKeyEvent { int allegro_code; bool press; };
static RealKeyEvent g_real_queue[kRealQueueCap];
static int g_real_queue_head = 0, g_real_queue_tail = 0; // ring buffer, mod kRealQueueCap
static CRITICAL_SECTION g_real_queue_cs;
static bool g_real_queue_cs_inited = false;

static void real_queue_init() {
    if (!g_real_queue_cs_inited) { InitializeCriticalSection(&g_real_queue_cs); g_real_queue_cs_inited = true; }
}

// Called from the VEH callback on whichever thread hit the breakpoint (the
// real window thread, measured - carrier/NOTES.md "Milestones 5-7" part B).
static bool real_queue_push(int allegro_code, bool press) {
    real_queue_init();
    bool ok;
    EnterCriticalSection(&g_real_queue_cs);
    int next = (g_real_queue_tail + 1) % kRealQueueCap;
    ok = (next != g_real_queue_head);
    if (ok) {
        g_real_queue[g_real_queue_tail].allegro_code = allegro_code;
        g_real_queue[g_real_queue_tail].press = press;
        g_real_queue_tail = next;
    }
    LeaveCriticalSection(&g_real_queue_cs);
    return ok; // item C: the caller aggregates the "queue full" report, see storm_note
}

// Called from the main thread only (drain_real_key_queue).
static bool real_queue_pop(RealKeyEvent* out) {
    if (!g_real_queue_cs_inited) return false;
    bool got = false;
    EnterCriticalSection(&g_real_queue_cs);
    if (g_real_queue_head != g_real_queue_tail) {
        *out = g_real_queue[g_real_queue_head];
        g_real_queue_head = (g_real_queue_head + 1) % kRealQueueCap;
        got = true;
    }
    LeaveCriticalSection(&g_real_queue_cs);
    return got;
}

// The breakpoint callback: key_dinput_handle_scancode(scancode, pressed) -
// reg-passed args (EAX=scancode/DIK, EDX=pressed), KNOWN from its
// disassembly at entry (0x46d5a8, same fact call_key_dinput_handle_scancode
// above already relies on). Queues the translated event and neutralizes the
// call (pop return address into EIP), same technique as
// neutralize_keyboard_hit below.
static void real_key_capture_hit(CONTEXT* ctx) {
    int dik = (int)(unsigned char)ctx->Eax;
    bool press = ctx->Edx != 0;
    int allegro_code = dik_to_allegro(dik);
    int T = det_current_tick();
    trace_input("key_dinput_handle_scancode(capture,window thread)",
                press ? "press" : "release", allegro_code, "phase=capture");
    if (allegro_code == 0) {
        storm_note(T, dik, 1);          // no Allegro mapping - item C aggregates the log line
    } else if (!real_queue_push(allegro_code, press)) {
        storm_note(T, dik, 2);          // capture queue full
    } else {
        storm_note(T, dik, 0);
    }
    DWORD ret = *(DWORD*)(uintptr_t)ctx->Esp;
    ctx->Esp += 4;
    ctx->Eip = ret;
}

// Called from det_wrap_Sleep on the main thread, right after
// deliver_due_input - the exact tick-boundary delivery point script mode
// already uses. This is what makes T at delivery equal T at recording:
// --record-input's breakpoints sit at _handle_key_press/_handle_key_release,
// which this function calls directly, synchronously, from the main thread.
// DIVERGENCE 005 FIX (notes/living_record.md; carrier/NOTES.md "Divergence
// 005"). Item 2 above made record and replay use the same FUNCTION calls
// (_handle_key_press/_handle_key_release) from the same THREAD (main) - but
// not from the same POINT IN TIME, and that residue is the whole of 005.
//
// MEASURED with --trace-input: the guest's idle loops call rest(1), so
// det_wrap_Sleep runs ~20 times per carrier tick (T = virtual_ms/20). The old
// drain ran on EVERY one of those calls, so a real key captured at an
// arbitrary instant was delivered at the very next Sleep call - i.e. at
// sub-tick position 0..19 of tick T, wherever the human happened to press.
// deliver_due_input, in contrast, can only ever fire a scripted event at
// sub-tick position 0 (the FIRST Sleep call whose T reaches the event's
// tick). Both stamped and scheduled on T, so the recording looked consistent;
// but the guest's OWN 20 ms tick (cycle_count @0x506938, incremented inside
// _handle_timer_tick) lands at ONE specific sub-tick position, so an event
// delivered at sub=13 of tick T and the same event replayed at sub=0 of tick
// T fall on OPPOSITE SIDES of the guest's tick boundary - the game consumes
// it one game tick earlier on replay. That is exactly the observed "recorded
// first safepoint T=249, replay T=248", and exactly why shifting every event
// +1 tick over-corrected (it fixed the first 51 ticks and broke T=300).
//
// The rule that makes stamp == delivery tick in BOTH modes: deliver a
// captured real event at the SAME drain point a scripted event uses - the
// first Sleep call of a new carrier tick - and stamp it there. The tick index
// then fully determines the delivery point, in both directions, and a
// recording is replayable by construction. Cost: at most one extra carrier
// tick (20 ms) of latency for a human, on top of the tick-boundary latency
// item 2 already introduced.
static int g_last_drain_tick = -1;

static void drain_real_key_queue() {
    if (g_input_policy != InputPolicy::Real) return; // nothing was ever queued
    int T = det_current_tick();
    if (T == g_last_drain_tick) return;   // not the first Sleep call of this tick
    g_last_drain_tick = T;
    exit_storm_flush(); // item C: one summary line per tick, from the main thread
    RealKeyEvent e;
    typedef void(__cdecl * PressFn)(int, int);
    typedef void(__cdecl * ReleaseFn)(int);
    while (real_queue_pop(&e)) {
        trace_input("drain_real_key_queue(Sleep,after _handle_timer_tick)",
                    e.press ? "press" : "release", e.allegro_code, "phase=deliver");
        g_in_delivery = true;
        if (e.press) ((PressFn)(void*)VA_HANDLE_KEY_PRESS)(0, e.allegro_code);
        else ((ReleaseFn)(void*)VA_HANDLE_KEY_RELEASE)(e.allegro_code);
        g_in_delivery = false;
        fprintf(stderr, "det: T=%d delivered real %s scancode=%d (captured at tick boundary)\n",
                T, e.press ? "press" : "release", e.allegro_code);
    }
    // "Environment isolation" item 2: window activation is handed over at the
    // SAME point, in the same tick, for exactly the reason divergence 005
    // established for keys - an asynchronous handover cannot be replayed, and
    // a recording must log what the carrier handed over, not what the host
    // did. The recording line is written HERE (not at the capture point) so
    // stamp == delivery tick by construction.
    bool sw_in;
    while (switch_queue_pop(&sw_in)) {
        g_in_delivery = true;               // see deliver_due_input's switch arm
        run_switch_callbacks(sw_in);
        g_in_delivery = false;
        ++g_switch_delivered;
        if (g_record_file) {
            fprintf(g_record_file, "%d switch %s\n", T, sw_in ? "in" : "out");
            fflush(g_record_file);
            ++g_switch_recorded;
        }
        fprintf(stderr, "det: T=%d delivered real switch %s (captured at tick boundary)\n",
                T, sw_in ? "in" : "out");
        trace_input("drain_real_key_queue(Sleep,after _handle_timer_tick)",
                    sw_in ? "switch-in" : "switch-out", sw_in ? 1 : 0, "phase=deliver");
    }
}

// ---------------------------------------------------------------------
// A. Virtual clock + thread virtualization
// ---------------------------------------------------------------------
static DWORD WINAPI parked_thread_proc(LPVOID) {
    WaitForSingleObject(g_parked_event, INFINITE); // never signaled: blocks forever, ~0% CPU
    return 0;
}
// A fake-but-real thread handle for a virtualized Allegro thread: the guest
// stores/CloseHandle's/WaitForSingleObject's this normally (all DIRECT,
// unwrapped imports), so it must be a genuine kernel handle, just one that
// never does anything. Still used for VA_INPUT_THREAD_PROC below (measured,
// carrier/NOTES.md: never actually spawned in this build, so it is dead
// code kept only as a guard - not worth the added real-thread machinery
// item 3 below adds specifically to fix the two timer threads' exit hang).
static uintptr_t make_parked_handle() {
    HANDLE h = CreateThread(nullptr, 0, parked_thread_proc, nullptr, 0, nullptr);
    return (uintptr_t)h;
}

// ---------------------------------------------------------------------
// Item 3 ("parked timer thread" pass, carrier/NOTES.md; divergence 003,
// notes/living_record.md): _tim_win32_exit (0x478488) does
// SetEvent(stop_event@0x4ec050) then loops WaitForSingleObject(
// timer_thread_handle@0x4ec054, 100) while it returns WAIT_TIMEOUT (0x102).
// The OLD virtualized timer thread (make_parked_handle above) blocked
// forever on OUR OWN never-signaled event, so that handle never became
// signaled and the join spun forever - the exit hang.
//
// KNOWN (artifacts/disasm.txt, cited in det.hpp's declaration of
// det_wrap_WaitForSingleObject): both _tim_win32_high_perf_thread (0x478584)
// and _tim_win32_low_perf_thread (0x4783bc) loop on
// WaitForSingleObject(stop_event@0x4ec050, <small ms>) and branch to
// __win_thread_exit (a normal return) the FIRST time that call returns
// anything other than WAIT_TIMEOUT - i.e. the original code already knows
// how to exit cleanly the moment its wait is satisfied; it just needs an
// actual signal to arrive, not a fake handle.
//
// Generic fix: run the ORIGINAL entry point on a REAL host thread (so it is
// a genuine, joinable kernel object - CloseHandle/WaitForSingleObject from
// guest code keep working exactly as before), but register that thread's id
// as "parked". det_wrap_WaitForSingleObject (below) substitutes INFINITE
// for any FINITE timeout a parked thread asks for, so its own
// WaitForSingleObject(stop_event, 15-or-100) call never returns
// WAIT_TIMEOUT and therefore never reaches the _handle_timer_tick call just
// above it in either thread's loop (tick delivery is UNCHANGED: still only
// from det_wrap_Sleep on the main thread, synchronous, milestone 5-7's
// design) - the thread simply blocks in that one real wait until the guest
// itself calls SetEvent(stop_event) at shutdown (_tim_win32_exit), at which
// point WaitForSingleObject returns non-timeout, the guest's own code falls
// through to __win_thread_exit, and the thread function returns for real -
// satisfying _tim_win32_exit's join loop by construction, no carrier-side
// polling or timeout needed.
// ---------------------------------------------------------------------
static const int kMaxParkedThreads = 8;
static DWORD g_parked_thread_ids[kMaxParkedThreads];
static int g_parked_thread_count = 0;
static CRITICAL_SECTION g_parked_cs;
static bool g_parked_cs_inited = false;

static void ensure_parked_cs() {
    if (!g_parked_cs_inited) { InitializeCriticalSection(&g_parked_cs); g_parked_cs_inited = true; }
}

static void register_parked_thread(DWORD tid) {
    ensure_parked_cs();
    EnterCriticalSection(&g_parked_cs);
    if (g_parked_thread_count < kMaxParkedThreads) g_parked_thread_ids[g_parked_thread_count++] = tid;
    else fprintf(stderr, "det: WARNING - parked-thread table full, thread %lu not tracked\n", tid);
    LeaveCriticalSection(&g_parked_cs);
}

// Declared in det.hpp indirectly via det_wrap_WaitForSingleObject; kept
// file-local since only that wrapper needs it.
static bool det_is_parked_thread(DWORD tid) {
    if (!g_parked_cs_inited) return false; // nothing registered yet - cheap common case
    bool found = false;
    EnterCriticalSection(&g_parked_cs);
    for (int i = 0; i < g_parked_thread_count; ++i)
        if (g_parked_thread_ids[i] == tid) { found = true; break; }
    LeaveCriticalSection(&g_parked_cs);
    return found;
}

struct ParkedRealThreadArgs { void (__cdecl* start)(void*); void* arglist; };

static DWORD WINAPI parked_real_thread_proc(LPVOID pv) {
    ParkedRealThreadArgs* a = (ParkedRealThreadArgs*)pv;
    void (__cdecl* start)(void*) = a->start;
    void* arglist = a->arglist;
    free(a);
    start(arglist); // the ORIGINAL guest entry point, called exactly as
                     // _beginthread itself would (cdecl, one void* arg) -
                     // real execution, real x87/CRT thread-local init via
                     // its own __win_thread_init call, real wait loop.
    return 0;        // reached only after the guest's own code returns
                      // (i.e. after its WaitForSingleObject was satisfied).
}

// Creates the thread SUSPENDED, registers its id as parked, THEN resumes -
// so det_wrap_WaitForSingleObject already knows about it before the thread
// can possibly make its first (substitutable) wait call. Mirrors
// make_parked_handle's "must be a genuine kernel handle" requirement above.
static uintptr_t make_parked_real_handle(void(__cdecl* start)(void*), void* arglist) {
    ParkedRealThreadArgs* a = (ParkedRealThreadArgs*)malloc(sizeof(ParkedRealThreadArgs));
    if (!a) { fprintf(stderr, "det: make_parked_real_handle: out of memory\n"); return 0; }
    a->start = start;
    a->arglist = arglist;
    DWORD tid = 0;
    HANDLE h = CreateThread(nullptr, 0, parked_real_thread_proc, a, CREATE_SUSPENDED, &tid);
    if (!h) {
        fprintf(stderr, "det: make_parked_real_handle: CreateThread failed gle=%lu\n", GetLastError());
        free(a);
        return 0;
    }
    register_parked_thread(tid);
    ResumeThread(h);
    return (uintptr_t)h;
}

extern "C" DWORD __stdcall det_wrap_WaitForSingleObject(HANDLE h, DWORD ms) {
    pf_count_import(g_id_WaitForSingleObject);
    if (ms != INFINITE && det_is_parked_thread(GetCurrentThreadId())) {
        // See the big comment above make_parked_real_handle: a parked
        // thread's own wait becomes unconditional, so it can only resume
        // when the guest itself signals the object (real exit), never on a
        // timeout (which would otherwise run a timer tick from the wrong
        // thread and reintroduce exactly the race milestone 5-7 removed).
        ms = INFINITE;
    }
    if (g_real_WaitForSingleObject)
        return ((DWORD(__stdcall*)(HANDLE, DWORD))g_real_WaitForSingleObject)(h, ms);
    return WAIT_FAILED;
}

extern "C" uintptr_t __cdecl det_wrap_beginthread(void(__cdecl* start)(void*),
                                                    unsigned stack_size, void* arglist) {
    pf_count_import(g_id_beginthread);
    uintptr_t start_va = (uintptr_t)(void*)start;
    fprintf(stderr, "det: _beginthread(start=0x%p, stack=%u)\n", (void*)start, stack_size);
    if (g_det_mode && (start_va == VA_TIM_HIGH_PERF_THREAD || start_va == VA_TIM_LOW_PERF_THREAD)) {
        fprintf(stderr, "det: timer thread PARKED (entry=0x%p): running the ORIGINAL entry point "
                        "on a real thread whose WaitForSingleObject calls are substituted to "
                        "INFINITE (carrier/NOTES.md 'parked timer thread' - fixes divergence 003, "
                        "the _tim_win32_exit join hang, generically)\n", (void*)start);
        return make_parked_real_handle(start, arglist);
    }
    if (g_det_mode && start_va == VA_INPUT_THREAD_PROC) {
        fprintf(stderr, "det: input thread virtualized (entry=0x%p) - synthetic key events drive key[] instead\n",
                (void*)start);
        return make_parked_handle();
    }
    uintptr_t h = 0;
    if (g_real_beginthread) {
        typedef uintptr_t(__cdecl * Fn)(void(__cdecl*)(void*), unsigned, void*);
        h = ((Fn)g_real_beginthread)(start, stack_size, arglist);
    }
    // Hardware breakpoints are per-thread. Any real (non-virtualized) thread
    // this carrier spawns might be the one that ends up calling
    // key_dinput_handle_scancode (det mode's neutralize breakpoint) or
    // _handle_key_press/_handle_key_release (--record-input's breakpoints) -
    // measured (carrier/NOTES.md): it is the real window thread here, not a
    // dedicated input thread. Arming every spawned thread with the current
    // table is a no-op when g_bp_count==0 and otherwise makes this correct
    // regardless of which Allegro thread turns out to own DirectInput.
    if (h != 0 && h != (uintptr_t)-1) det_arm_thread((HANDLE)h);
    return h;
}

// item 2 (win32_pilot.md): "state in NOTES whether the carrier should call
// SetForegroundWindow on the guest window ... do it if cheap." It is cheap -
// EnumWindows filtered by our own process id, no need to hook
// RegisterClassA/CreateWindowExA to capture the HWND at creation time. Only
// matters for a live human playing with --input=real (play.py's plain/
// --record-replay invocation); tried once (on det_wrap_Sleep's first call
// after the window plausibly exists) and never retried once it succeeds -
// harmless if it never finds a window (e.g. running headless/automated).
static bool g_tried_focus = false;
struct FocusSearch { DWORD pid; HWND found; };
// Same search, but WITHOUT the IsWindowVisible filter - an automated run's
// window is minimized/hidden by design, so the shutdown diagnostic must be
// able to find it anyway.
static BOOL CALLBACK focus_enum_proc_any(HWND hwnd, LPARAM lparam);
static BOOL CALLBACK focus_enum_proc(HWND hwnd, LPARAM lparam) {
    FocusSearch* s = (FocusSearch*)lparam;
    DWORD wnd_pid = 0;
    GetWindowThreadProcessId(hwnd, &wnd_pid);
    if (wnd_pid == s->pid && IsWindowVisible(hwnd)) {
        s->found = hwnd;
        return FALSE; // stop enumerating
    }
    return TRUE;
}
// "Environment isolation" item 1: this is now gated on --interactive, NOT on
// input_policy==Real. Taking the operator's foreground is only ever correct
// when a human is actually there to look at the window; an automated
// --input=real run (e.g. carrier/scripts/sendinput_session.py's focus-theft
// test, or any headless record run) must not steal focus on its own.
static void try_focus_guest_window_once() {
    if (g_tried_focus || !g_interactive) return;
    FocusSearch s = {GetCurrentProcessId(), nullptr};
    EnumWindows(focus_enum_proc, (LPARAM)&s);
    if (s.found) {
        SetForegroundWindow(s.found);
        ShowWindow(s.found, SW_RESTORE);
        fprintf(stderr, "det: focused guest window hwnd=%p (--interactive)\n", (void*)s.found);
        g_tried_focus = true; // succeeded - stop trying
    }
}

static BOOL CALLBACK focus_enum_proc_any(HWND hwnd, LPARAM lparam) {
    FocusSearch* s = (FocusSearch*)lparam;
    DWORD wnd_pid = 0;
    GetWindowThreadProcessId(hwnd, &wnd_pid);
    char cls[64] = "";
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (wnd_pid == s->pid && strcmp(cls, "AllegroWindow") == 0) { s->found = hwnd; return FALSE; }
    return TRUE;
    // else: window doesn't exist yet (still starting up) - retried on the
    // next Sleep call, cheap since it's a handful of EnumWindows calls total.
}

extern "C" void __stdcall det_wrap_Sleep(DWORD ms) {
    pf_count_import(g_id_Sleep);
    if (GetCurrentThreadId() != g_main_tid) { ::Sleep(ms); return; }
    try_focus_guest_window_once();
    // --trace-input bookkeeping: `sub` is which Sleep call within the current
    // carrier tick this is. T only advances once every ~20 Sleep calls (the
    // guest calls rest(1) from its idle loops), so "the tick a key event is
    // stamped with" is a MUCH coarser coordinate than "the point at which the
    // event is handed to the game" - which is exactly what divergence 005
    // turned out to be about. See NOTES.md.
    ++g_sleep_calls;

    if (g_det_mode) {
        // KNOWN (disasm of tim_win32_high_perf_thread, carrier/NOTES.md):
        // the real timer thread converts QPC-elapsed time to timer units via
        // elapsed_qpc * TIMERS_PER_SECOND / qpc_frequency, then calls
        // _handle_timer_tick(units) and waits again - a running remainder is
        // preserved because it always measures from the last checkpoint.
        // Reproduced here with virtual elapsed ms instead of QPC: keeping a
        // running TOTAL (g_units_reported) and diffing on every call gives
        // the same full-precision "no drift" property without a separate
        // remainder variable.
        g_virtual_ms += ms;
        sub_tick_advance();
        LONGLONG total_units = g_virtual_ms * TIMERS_PER_SECOND / 1000;
        LONGLONG delta = total_units - g_units_reported;
        g_units_reported = total_units;
        g_cyc_pre = IT_CYCLE_COUNT;
        if (delta > 0) {
            typedef long(__cdecl * TickFn)(int);
            ((TickFn)(void*)VA_HANDLE_TIMER_TICK)((int)delta);
        }
        g_cyc_post = IT_CYCLE_COUNT;
        if (g_trace_input_file && g_cyc_post != g_cyc_pre)
            trace_input("sleep", "guest-tick-boundary", g_cyc_post, nullptr);
        // Both providers hand over at the SAME point - see deliver_due_input /
        // drain_real_key_queue (divergence 005 fix).
        deliver_due_input();
        drain_real_key_queue(); // item 2: real events captured since the last tick
        if (g_pace_real) ::Sleep(ms);
    } else {
        ::Sleep(ms);
        sub_tick_advance();
        deliver_due_input(); // real-time T, see det_now_ms()
        drain_real_key_queue();
    }
}

// pre-det semantics (KNOWN, notes/binary_recon.md item c): real hardware
// QueryPerformanceCounter, called by Allegro's timer thread internally (not
// created in det mode) and by 6 one-shot anti-cheat/statistics call sites
// inside play() - never read back into physics/replay/RNG.
// ---------------------------------------------------------------------
// Determinism-audit instrumentation ("Environment isolation" item 5): two
// opt-in perturbation knobs, in the same style as DET_DUMP_MEM_TICK /
// DET_INPUT_DELIVER_SUB. They turn "the census says play()'s clock/QPC calls
// are telemetry, not simulation inputs" from a citation into a MEASUREMENT:
//
//   DET_PERTURB_CLOCK=<ms>  offsets what clock()/QPC/timeGetTime return.
//                           G1 must stay EQUAL  -> those three do not reach
//                           the digest domain.        (negative control)
//   DET_PERTURB_TIME=<secs> offsets what time() returns.
//                           G1 must CHANGE      -> time() does reach it, so
//                           recording it (item 4) is load-bearing.
//                                                     (positive control)
// ---------------------------------------------------------------------
static long long perturb_clock_ms() {
    static long long v = -1;
    if (v < 0) { char b[24]; v = GetEnvironmentVariableA("DET_PERTURB_CLOCK", b, sizeof(b)) ? _atoi64(b) : 0; }
    return v;
}
static long perturb_time_s() {
    static long v = -1;
    if (v < 0) { char b[24]; v = GetEnvironmentVariableA("DET_PERTURB_TIME", b, sizeof(b)) ? atol(b) : 0; }
    return v;
}

extern "C" BOOL __stdcall det_wrap_QueryPerformanceCounter(LARGE_INTEGER* out) {
    pf_count_import(g_id_QPC);
    if (g_det_mode) {
        if (out) out->QuadPart = g_virtual_ms + perturb_clock_ms(); // fake 1000 Hz counter tied to the virtual clock
        return TRUE;
    }
    if (g_real_QPC) return ((BOOL(__stdcall*)(LARGE_INTEGER*))g_real_QPC)(out);
    return FALSE;
}

// pre-det: WINMM millisecond counter. KNOWN it is called only inside
// tim_win32_low_perf_thread (never created in det mode); wrapped anyway for
// completeness/documentation and in case a future low-perf-timer path calls it.
extern "C" DWORD __stdcall det_wrap_timeGetTime() {
    pf_count_import(g_id_timeGetTime);
    if (g_det_mode) return (DWORD)(g_virtual_ms + perturb_clock_ms());
    if (g_real_timeGetTime) return ((DWORD(__stdcall*)())g_real_timeGetTime)();
    return 0;
}

// pre-det: msvcrt time(), wall-clock epoch seconds. KNOWN (notes/replay_format.md
// sec 2) this feeds all 3 srand() call sites (init_game, new_game x2) plus the
// qpc/clock/time anti-cheat trio in play(). Pinning it is what makes the
// tower-layout RNG seed reproducible across --det runs WITHOUT separately
// wrapping rand()/srand(): rand()'s LCG is already a pure function of the
// seed, and time() was the only host-entropy input to that seed.
//
// "Environment isolation" item 4 (win32_pilot.md sec 4a's rule, and
// notes/living_record.md's own FOLLOW-UP entry: "record the observed
// time()/clock values as events in record mode and replay them, so
// interactive sessions vary while replays stay exact"). Three cases, in
// priority order, so that G1 - a script run with no recorded values - is
// byte-for-byte what it was before this pass:
//
//   1. the loaded script carried `T time <v>` lines  -> return them in order
//      (a replay reproduces the recording's tower exactly);
//   2. --record-input is active                      -> answer from the REAL
//      clock and append `T time <v>` to the recording (so two sessions made
//      minutes apart get different seeds and different towers);
//   3. otherwise                                     -> the constant virtual
//      epoch, exactly as milestones 5-7 defined it.
//
// Guard: only the guest MAIN thread may record/replay a value. The only
// other time() caller in the binary is fldads_threadmain (suppressed in --det
// by det_wrap_pthread_create), so an off-thread call is a real anomaly - it
// is counted, logged and answered from the constant epoch rather than being
// allowed to consume a recorded value out of order.
extern "C" long __cdecl det_wrap_time(long* out) {
    pf_count_import(g_id_time);
    long v;
    if (g_det_mode) {
        long epoch = (long)(DET_VIRTUAL_EPOCH + g_virtual_ms / 1000) + perturb_time_s();
        bool main_thread = (GetCurrentThreadId() == g_main_tid);
        if (!main_thread && (!g_time_values.empty() || g_record_file)) {
            ++g_time_offthread;
            if (g_time_offthread <= 5)
                fprintf(stderr, "det: time() called from thread %lu (not the guest main thread) while the "
                                "clock channel is recorded/replayed - answered from the constant epoch, "
                                "NOT from the recording (call #%ld)\n",
                        GetCurrentThreadId(), g_time_offthread);
            v = epoch;
        } else if (!g_time_values.empty()) {
            if (g_time_cursor < g_time_values.size()) {
                v = g_time_values[g_time_cursor++].value;
                ++g_time_replayed;
            } else {
                ++g_time_underflow;
                if (g_time_underflow <= 5)
                    fprintf(stderr, "det: recorded clock UNDERFLOW - the guest asked for time() more times "
                                    "than the recording holds (%zu values); falling back to the constant "
                                    "epoch from call #%ld on\n",
                            g_time_values.size(), g_time_underflow);
                v = epoch;
            }
        } else if (g_record_file && g_real_time) {
            v = ((long(__cdecl*)(long*))g_real_time)(nullptr);
            fprintf(g_record_file, "%d time %ld\n", det_current_tick(), v);
            fflush(g_record_file);
            ++g_time_recorded;
        } else {
            v = epoch;
        }
    }
    else if (g_real_time) v = ((long(__cdecl*)(long*))g_real_time)(nullptr);
    else v = 0;
    if (out) *out = v;
    return v;
}

// pre-det: msvcrt clock(), CLOCKS_PER_SEC=1000 clock_t. KNOWN only read by
// play()'s anti-cheat trio (qpc/clock/time), never gameplay.
extern "C" long __cdecl det_wrap_clock() {
    pf_count_import(g_id_clock);
    if (g_det_mode) return (long)(g_virtual_ms + perturb_clock_ms());
    if (g_real_clock) return ((long(__cdecl*)())g_real_clock)();
    return 0;
}

// ---------------------------------------------------------------------
// "Environment isolation" item 1: the guest's own window-management calls.
//
// MEASURED (artifacts/disasm.txt, quoted in carrier/NOTES.md): Allegro's
// create_directx_window (0x478eb8) creates the window with dwStyle=0xCA0000
// (WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX - note: NO WS_VISIBLE) and then does
// ShowWindow(hwnd, SW_SHOWNORMAL) / SetForegroundWindow(hwnd) /
// UpdateWindow(hwnd); set_video_mode (wddmode.c) repeats the ShowWindow +
// SetForegroundWindow pair when the graphics mode is set. Those two calls -
// and nothing else - are what puts the guest window in front of whatever the
// operator is doing. All four wrappers below forward UNCHANGED when
// --interactive; otherwise they substitute a non-activating form.
//
// This is a PRESENTATION-layer substitution, strictly below the PortForge
// boundary (win32_pilot.md sec 4a), so it must not change game state - which
// is exactly what gate G1 re-verifies after the change.
// ---------------------------------------------------------------------
extern "C" BOOL __stdcall det_wrap_ShowWindow(HWND h, int cmd) {
    pf_count_import(g_id_ShowWindow);
    int use = cmd;
    if (!g_interactive && g_window_mode != WindowMode::Normal && !isolate_off("window")) {
        // Only the ACTIVATING show commands are substituted; SW_HIDE and the
        // already-non-activating forms pass through untouched (Allegro also
        // calls ShowWindow on the console window at exit).
        if (cmd == SW_SHOWNORMAL || cmd == SW_SHOWMAXIMIZED || cmd == SW_SHOW ||
            cmd == SW_RESTORE || cmd == SW_SHOWDEFAULT) {
            use = (g_window_mode == WindowMode::Hidden) ? SW_HIDE : SW_SHOWMINNOACTIVE;
            ++g_showwindow_substituted;
            fprintf(stderr, "det: ShowWindow(%p, %d) -> %d (window=%s, not --interactive)\n",
                    (void*)h, cmd, use, window_mode_name(g_window_mode));
        }
    }
    if (g_real_ShowWindow) return ((BOOL(__stdcall*)(HWND, int))g_real_ShowWindow)(h, use);
    return FALSE;
}

extern "C" BOOL __stdcall det_wrap_SetForegroundWindow(HWND h) {
    pf_count_import(g_id_SetForegroundWindow);
    if (!g_interactive && !isolate_off("window")) {
        ++g_setforeground_suppressed;
        fprintf(stderr, "det: SetForegroundWindow(%p) SUPPRESSED (not --interactive)\n", (void*)h);
        return TRUE; // Allegro ignores the result; TRUE keeps its own logic on the success path
    }
    if (g_real_SetForegroundWindow) return ((BOOL(__stdcall*)(HWND))g_real_SetForegroundWindow)(h);
    return FALSE;
}

extern "C" BOOL __stdcall det_wrap_SetWindowPos(HWND h, HWND after, int x, int y,
                                                int cx, int cy, UINT flags) {
    pf_count_import(g_id_SetWindowPos);
    UINT use = flags;
    if (!g_interactive && !isolate_off("window") && !(flags & SWP_NOACTIVATE)) {
        use |= SWP_NOACTIVATE;
        ++g_setwindowpos_noactivate;
    }
    if (g_real_SetWindowPos)
        return ((BOOL(__stdcall*)(HWND, HWND, int, int, int, int, UINT))g_real_SetWindowPos)(
            h, after, x, y, cx, cy, use);
    return FALSE;
}

// DET_TRACE_WNDMSG=1 (audit diagnostic): subclass the guest window right
// after it is created and log every activation-class message it actually
// receives - WM_ACTIVATE(0x06), WM_SETFOCUS(0x07), WM_KILLFOCUS(0x08),
// WM_ACTIVATEAPP(0x1c), WM_NCACTIVATE(0x86) - then forward to Allegro's own
// directx_wnd_proc unchanged. This is the "wrap the message path" instrument
// the task named; it is used to MEASURE whether the operator's desktop can
// reach the guest at all, and it changes no behaviour.
static WNDPROC g_orig_wndproc = nullptr;
static long g_wndmsg_activate = 0;
static bool trace_wndmsg() {
    static int v = -1;
    if (v < 0) { char b[8]; v = GetEnvironmentVariableA("DET_TRACE_WNDMSG", b, sizeof(b)) && b[0] != '0'; }
    return v != 0;
}
static LRESULT CALLBACK det_wndmsg_trace_proc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_ACTIVATE || m == WM_SETFOCUS || m == WM_KILLFOCUS ||
        m == WM_ACTIVATEAPP || m == WM_NCACTIVATE) {
        ++g_wndmsg_activate;
        fprintf(stderr, "det: [wndmsg] T=%d hwnd=%p msg=0x%02x wparam=0x%08x lparam=0x%08x\n",
                det_current_tick(), (void*)h, m, (unsigned)wp, (unsigned)lp);
        fflush(stderr);
    }
    return CallWindowProcA(g_orig_wndproc, h, m, wp, lp);
}

extern "C" HWND __stdcall det_wrap_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style,
                                                   int x, int y, int w, int hgt, HWND parent,
                                                   HMENU menu, HINSTANCE inst, LPVOID param) {
    pf_count_import(g_id_CreateWindowExA);
    DWORD use_style = style, use_ex = ex;
    if (!g_interactive && g_window_mode != WindowMode::Normal && !isolate_off("window")) {
        // Belt and braces: the measured call already passes no WS_VISIBLE, so
        // this normally changes nothing (the counter says whether it ever
        // fires). WS_EX_NOACTIVATE is the OS-level guarantee that this window
        // can never take the foreground, however it is later shown or clicked.
        if (use_style & WS_VISIBLE) { use_style &= ~(DWORD)WS_VISIBLE; ++g_createwindow_devisible; }
        use_ex |= WS_EX_NOACTIVATE;
        fprintf(stderr, "det: CreateWindowExA class='%s' style=0x%08lx->0x%08lx ex=0x%08lx->0x%08lx "
                        "(window=%s, not --interactive)\n",
                cls ? cls : "(atom)", style, use_style, ex, use_ex, window_mode_name(g_window_mode));
    }
    HWND h = nullptr;
    if (g_real_CreateWindowExA)
        h = ((HWND(__stdcall*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU,
                               HINSTANCE, LPVOID))g_real_CreateWindowExA)(
            use_ex, cls, name, use_style, x, y, w, hgt, parent, menu, inst, param);
    if (h && trace_wndmsg() && !g_orig_wndproc) {
        g_orig_wndproc = (WNDPROC)(LONG_PTR)SetWindowLongA(h, GWL_WNDPROC, (LONG)(LONG_PTR)det_wndmsg_trace_proc);
        fprintf(stderr, "det: DET_TRACE_WNDMSG - subclassed guest window %p (original wndproc=%p)\n",
                (void*)h, (void*)g_orig_wndproc);
    }
    return h;
}

// ---------------------------------------------------------------------
// "Environment isolation" item 5: the network ad fetch.
//
// MEASURED: fldads_start (0x403ac8) is the binary's ONLY pthread_create call
// site and its start routine is always fldads_threadmain (0x404014); nothing
// ever joins the handle (pthread_join is not imported at all - the only
// pthreadGC2 imports are pthread_create/pthread_mutex_lock/unlock). The
// thread fetches an ad list over HTTP from www.icytower.com, writes a local
// CSV/PNG cache into assets\, and fills gpAdCache/giAdCacheSize - and FIVE
// fld_adspot.c globals (giAdCacheSize, gpAdCache,
// localFilename__fldads_get_local_cache_name, pFLDAdBitmap, pFLDAd) are
// inside the 151-global digest domain, so the live network result CAN reach
// the verdict. It is also the source of divergence 001. Suppressed in --det:
// the game's observable result becomes the constant "no ads" (pFLDAd stays
// NULL, which is also what makes main_menu_callback's mouse hit-test
// unreachable - see det_stub_handle_mouse_input).
// ---------------------------------------------------------------------
extern "C" int __cdecl det_wrap_pthread_create(void* th, void* attr,
                                               void* (__cdecl* start)(void*), void* arg) {
    pf_count_import(g_id_pthread_create);
    if (g_det_mode && (uintptr_t)(void*)start == VA_FLDADS_THREADMAIN && !isolate_off("ad")) {
        ++g_ad_thread_suppressed;
        fprintf(stderr, "det: ad-fetch thread SUPPRESSED (pthread_create(fldads_threadmain @0x%08x) "
                        "is a no-op in --det; the game observes the constant 'no ads' result - "
                        "carrier/NOTES.md 'Environment isolation')\n", VA_FLDADS_THREADMAIN);
        return 0; // fldads_start ignores the return value (disasm 0x403aed: call, leave, ret)
    }
    if (g_real_pthread_create)
        return ((int(__cdecl*)(void*, void*, void* (__cdecl*)(void*), void*))g_real_pthread_create)(
            th, attr, start, arg);
    return -1;
}

// ---------------------------------------------------------------------
// "Environment isolation" item 5: environment variables. Instrumentation
// only - the value is forwarded unchanged - but the distinct names asked for
// and whether the host actually had a value are recorded, so the channel can
// be reported from evidence instead of assumed inert.
// ---------------------------------------------------------------------
static const int kMaxEnvNames = 24;
static char g_env_names[kMaxEnvNames][48];
static long g_env_hits[kMaxEnvNames];
static bool g_env_found[kMaxEnvNames];
static int g_env_count = 0;
static long g_env_calls = 0;

extern "C" char* __cdecl det_wrap_getenv(const char* name) {
    pf_count_import(g_id_getenv);
    char* v = nullptr;
    if (g_real_getenv) v = ((char*(__cdecl*)(const char*))g_real_getenv)(name);
    ++g_env_calls;
    const char* n = name ? name : "(null)";
    for (int i = 0; i < g_env_count; ++i) {
        if (strcmp(g_env_names[i], n) == 0) { ++g_env_hits[i]; g_env_found[i] = g_env_found[i] || (v != nullptr); return v; }
    }
    if (g_env_count < kMaxEnvNames) {
        strncpy(g_env_names[g_env_count], n, sizeof(g_env_names[0]) - 1);
        g_env_names[g_env_count][sizeof(g_env_names[0]) - 1] = 0;
        g_env_hits[g_env_count] = 1;
        g_env_found[g_env_count] = (v != nullptr);
        ++g_env_count;
    }
    return v;
}

// ---------------------------------------------------------------------
// C. Tick sensor: generic {VA, callback} hardware-breakpoint table.
// Dr0-Dr3 give up to 4 simultaneous exec breakpoints; slot 0 is always the
// play() safepoint when digest/stop-at-tick is requested, slots 1-2 are the
// key-event recorder when --record-input is requested.
// ---------------------------------------------------------------------
struct BpSlot { DWORD_PTR va; void (*on_hit)(CONTEXT*); };
static BpSlot g_bp[4];
static int g_bp_count = 0;

static int register_breakpoint(DWORD_PTR va, void (*cb)(CONTEXT*)) {
    if (g_bp_count >= 4) { fprintf(stderr, "det: breakpoint table full, dropping 0x%p\n", (void*)va); return -1; }
    g_bp[g_bp_count].va = va;
    g_bp[g_bp_count].on_hit = cb;
    return g_bp_count++;
}

// Milestones 11-12 (bind.cpp): the same table, from a second consumer. See
// det.hpp for the slot-budget rationale.
int det_register_breakpoint(DWORD_PTR va, void (*cb)(CONTEXT*)) { return register_breakpoint(va, cb); }

void det_ctx_arm_slot(CONTEXT* ctx, int slot, DWORD_PTR va) {
    if (slot < 0 || slot > 3) return;
    g_bp[slot].va = va;
    DWORD* drs[4] = {&ctx->Dr0, &ctx->Dr1, &ctx->Dr2, &ctx->Dr3};
    *drs[slot] = (DWORD)va;
    ctx->Dr7 |= (1u << (slot * 2));   // Ln local-enable; RW/LEN stay 0 = execute, 1 byte
    // NtContinue only reloads DR0-DR7 when the context it is handed claims
    // to carry them; the exception context we were given may not.
    ctx->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
}

void det_ctx_disarm_slot(CONTEXT* ctx, int slot) {
    if (slot < 0 || slot > 3) return;
    g_bp[slot].va = 0;
    DWORD* drs[4] = {&ctx->Dr0, &ctx->Dr1, &ctx->Dr2, &ctx->Dr3};
    *drs[slot] = 0;
    ctx->Dr7 &= ~(1u << (slot * 2));
    ctx->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
}

// MEASURED (carrier/NOTES.md "Milestones 5-7", two documented attempts):
// hashing the FULL .data+.bss range never converges to equal across two
// --det runs, even with the deterministic heap arena active. Byte-diffing
// raw dumps (DET_DUMP_MEM_TICK/DET_DUMP_MEM_PATH above) and mapping
// differing offsets to symbols (artifacts/coff_symbols.json), twice, on two
// different pairs of runs, found a *different* set of ~20-30 differing
// globals each time - always Allegro/CRT/DirectX internals (COM device
// pointers, mutex/thread/event HANDLEs, an HWND, MinGW runtime pointers,
// audio/input ring buffers), NEVER a game-CU global, and never converging
// (ASLR entropy sometimes coincidentally matches between two runs, so which
// bytes visibly differ isn't even stable - per-byte exclusion is an
// unbounded chase). log.txt was BYTE-IDENTICAL between every pair of runs
// tried (gameplay itself IS deterministic) - only host-object *identity*
// bytes vary, exactly what the architecture doc (win32_pilot.md sec 6)
// already calls "not durable state; re-bound", outside the "guest pages"
// model, and sec 4's COM boundary: "a proxy is introduced only if... needed"
// (not yet built). Alternative adopted instead of chasing individual bytes
// (carrier/gen/gen_game_globals.py, evidence + rationale in its docstring):
// hash only the game-owned globals - the same DWARF `game` scope
// carrier/gen/it_globals.h already uses - which structurally excludes this
// whole category (confirmed: of 151 game globals, only 3 are themselves
// host handles - gFLDADMutex/sLogMutex/gFLDADThread, the ad-fetch thread's
// own mutex/thread - excluded by name in the generator).
struct GameGlobal { uint32_t va; uint32_t size; };
#include "../gen/game_globals.inc"

static void hash_game_globals(pf::Sha256& sha) {
    for (const GameGlobal& g : kGameGlobals)
        sha.update((const void*)(uintptr_t)g.va, g.size);
}

static void safepoint_hit(CONTEXT* ctx) {
    // Milestones 8-9: an in-process rewind happens HERE, before T is read
    // and before the digest line is written, so the line this safepoint
    // emits is already the RESTORED tick's line. That is what makes
    // "restore -> suffix == cold -> suffix" (notes/portforge_capsule.md SS D)
    // a byte-for-byte comparison of two digest streams with no fixups: the
    // cold run's T=400 line and the restored run's first line are computed
    // from the same memory at the same safepoint.
    snapshot_on_safepoint_pre(ctx);
    int T = det_current_tick();
    ++g_safepoint_count; // --trace-input: "safepoint count so far" coordinate
    // TEMPORARY diagnostic (see carrier/NOTES.md "Milestones 5-7"): dump raw
    // .data+.bss once, at the tick named by DET_DUMP_MEM_TICK, to the path
    // named by DET_DUMP_MEM_PATH - used to find exactly which bytes differ
    // between two --det runs when the digest doesn't match. Not part of the
    // normal option surface (env-var only, opt-in, checked once per run).
    {
        static bool dumped = false;
        char tickbuf[16], pathbuf[MAX_PATH];
        if (!dumped && GetEnvironmentVariableA("DET_DUMP_MEM_TICK", tickbuf, sizeof(tickbuf)) &&
            T == atoi(tickbuf) && GetEnvironmentVariableA("DET_DUMP_MEM_PATH", pathbuf, sizeof(pathbuf))) {
            FILE* f = fopen(pathbuf, "wb");
            if (f) {
                fwrite((const void*)(uintptr_t)0x4bc000u, 1, 0x176f4u, f);
                fwrite((const void*)(uintptr_t)0x4dd000u, 1, 0x36978u, f);
                fclose(f);
            }
            dumped = true;
        }
    }
    if (g_digest_file) {
        // Digest = sha256 over kGameGlobals (see hash_game_globals above for
        // why this is game-owned globals rather than the full .data/.bss
        // range). Guest runs at its real, unrebased addresses (no
        // relocations), so plain pointer reads are correct and sufficient.
        pf::Sha256 sha;
        hash_game_globals(sha);
        std::string hex = sha.hex();
        fprintf(g_digest_file, "%d %s esp=%08lx ebp=%08lx ebx=%08lx esi=%08lx edi=%08lx\n",
                T, hex.c_str(), (unsigned long)ctx->Esp, (unsigned long)ctx->Ebp,
                (unsigned long)ctx->Ebx, (unsigned long)ctx->Esi, (unsigned long)ctx->Edi);
        fflush(g_digest_file);
    }
    // Taken AFTER the digest line for the same tick, from the same memory at
    // the same instant - see snapshot_on_safepoint_pre's comment above.
    snapshot_on_safepoint_post(ctx);
    if (g_stop_at_tick > 0 && T >= g_stop_at_tick) {
        fprintf(stderr, "det: --stop-at-tick %d reached at T=%d, shutting down.\n", g_stop_at_tick, T);
        det_shutdown();
        if (g_shutdown) g_shutdown("det --stop-at-tick reached");
        TerminateProcess(GetCurrentProcess(), 0);
    }
}

// KNOWN (measured, see carrier/NOTES.md "Milestones 5-7"): the DirectInput
// input thread (input_thread_proc, VA_INPUT_THREAD_PROC) is never actually
// spawned in this build/config - only the timer and window threads are
// (verified: logging every _beginthread call site showed exactly those two).
// key_dinput_handle_scancode still runs (from the real window thread's
// message pump) and calls the real Win32 GetKeyboardState - i.e. it can
// observe the HOST's real keyboard, which is a live nondeterminism source
// this carrier's own automation could not rule out (measured: two supposedly
// identical --det runs of the same script diverged at the main menu,
// tracked down to this). Per win32_pilot.md part B's documented fallback
// ("leave [the thread] and neutralize the keyboard by never acquiring"):
// in det mode, short-circuit key_dinput_handle_scancode itself at entry -
// pop the return address into EIP (equivalent to an immediate `ret`; safe
// because its scancode/device args arrive in EAX/EDX per its disassembly,
// never on the stack, so there is nothing of the caller's to clean up).
// This makes the real keyboard fully inert regardless of which thread ends
// up calling it - synthetic --input-script events (which call
// _handle_key_press/_handle_key_release directly, bypassing this function
// entirely, or - only under --inject-real-test - through
// call_key_dinput_handle_scancode, which is never neutralized because that
// diagnostic requires input_policy==Real) are unaffected.
//
// Installed (det_init, below) whenever input_policy != Real - i.e. whenever
// the real keyboard is NOT the declared provider. Per win32_pilot.md sec 5a
// ("each NONDETERMINISTIC channel has exactly one active provider per run"),
// every hit here while parked is therefore, by definition, an attempted real
// key event that must never reach the game: not a normal/expected event
// (the neutralize mechanism is a fallback measure, not a proof the source is
// silent - see carrier/NOTES.md's "input thread never spawned" finding), so
// count and log it as a violation rather than silently absorbing it.
static void neutralize_keyboard_hit(CONTEXT* ctx) {
    LONG n = InterlockedIncrement(&g_real_key_violations);
    if (n <= 20) { // cap log spam; the count itself (report.json) is unbounded
        fprintf(stderr,
                "det: VIOLATION - real key event reached key_dinput_handle_scancode while "
                "--input=%s (violation #%ld, T=%d) - neutralized, NOT delivered to the game\n",
                input_policy_name(g_input_policy), n, det_current_tick());
    }
    DWORD ret = *(DWORD*)(uintptr_t)ctx->Esp;
    ctx->Esp += 4;
    ctx->Eip = ret;
}

// Item 4 ("recording hygiene" pass, carrier/NOTES.md): at exit, Allegro's
// own keyboard shutdown path releases every scancode it thinks COULD be
// down, one release call per scancode, all at one tick - MEASURED: ~120
// release lines at the tail of a real recording, none of them a real
// gameplay event. Rule: track which scancodes are "currently held" per our
// OWN recorded stream (set on a press we recorded, cleared on the matching
// release); a release for a scancode NOT in that set - never recorded
// pressed, OR already recorded released once - is not a real event and is
// dropped rather than written. Because filtered lines are simply never
// written, the file naturally ends at the last GENUINE press/release pair
// instead of at Allegro's exit-time flush, with no separate "trim the tail"
// pass needed. Applies uniformly regardless of which input source produced
// the press/release call (script direct injection, item 2's real-input
// capture-and-replay, or --inject-real-test's real path) - all three funnel
// through this same pair of breakpoints.
static bool g_key_held[256];

// DIVERGENCE 005, second residual (MEASURED with --trace-input): the guest
// itself calls _handle_key_press - Allegro's own KEY REPEAT, driven from
// _handle_timer_tick (keyboard.c's repeat timer; observed 240 ms after the
// original press, i.e. Allegro 4's 250 ms default repeat delay). Those calls
// arrive at ARBITRARY sub-tick positions, because _handle_timer_tick runs on
// every one of the ~10 Sleep calls per carrier tick. The old recorder stamped
// them too, so a recording contained events the carrier never delivered - and
// replaying them as ordinary scripted events put them at sub=0, a DIFFERENT
// position from where the guest originally generated them. Measured effect: a
// SendInput session diverged at its very first safepoint.
//
// Rule: a recording is a log of what the INPUT PROVIDER handed to the game,
// not of every _handle_key_press the guest happens to make. Only calls made
// from inside the carrier's own delivery point are recorded (g_in_delivery).
// The guest's repeats are a CONSEQUENCE of key[] state plus Allegro's timer,
// both of which the replay reproduces on its own, so dropping them from the
// file is not a loss of information.
//
// --inject-real-test is unaffected by construction: it calls
// key_dinput_handle_scancode from INSIDE deliver_due_input, so the real
// function's own auto-repeat behaviour (carrier/NOTES.md "Input policy and
// recording" part C) still happens with g_in_delivery set and is still
// recorded, exactly as before. (g_in_delivery itself is declared near the top
// of this file, since deliver_due_input sets it long before this point.)
static void keypress_record_hit(CONTEXT* ctx) {
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 8); // cdecl entry: [esp]=ret,[esp+4]=keycode,[esp+8]=scancode
    trace_input(g_in_delivery ? "_handle_key_press(record stamp)"
                              : "_handle_key_press(GUEST-INTERNAL, not recorded)",
                "press", scancode, g_in_delivery ? "phase=stamp" : "phase=guest-repeat");
    if (!g_record_file || !g_in_delivery) return;
    if (scancode >= 0 && scancode < 256) g_key_held[scancode] = true;
    const char* nm = scancode_to_name(scancode);
    if (nm) fprintf(g_record_file, "%d press %s\n", det_current_tick(), nm);
    else fprintf(g_record_file, "%d press %d\n", det_current_tick(), scancode);
    fflush(g_record_file);
}
static void keyrelease_record_hit(CONTEXT* ctx) {
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 4); // cdecl entry: [esp]=ret,[esp+4]=scancode
    trace_input(g_in_delivery ? "_handle_key_release(record stamp)"
                              : "_handle_key_release(GUEST-INTERNAL, not recorded)",
                "release", scancode, g_in_delivery ? "phase=stamp" : "phase=guest-internal");
    // Item C's exit-time storm is exactly this case: Allegro's keyboard
    // shutdown releases every scancode from OUTSIDE any delivery, so the
    // g_in_delivery gate drops the whole sweep on its own; the "currently
    // held" hygiene rule below is kept unchanged as the second filter.
    if (!g_record_file || !g_in_delivery) return;
    if (scancode < 0 || scancode >= 256 || !g_key_held[scancode]) {
        return; // not a real, still-open press of ours - drop it (see comment above)
    }
    g_key_held[scancode] = false;
    const char* nm = scancode_to_name(scancode);
    if (nm) fprintf(g_record_file, "%d release %s\n", det_current_tick(), nm);
    else fprintf(g_record_file, "%d release %d\n", det_current_tick(), scancode);
    fflush(g_record_file);
}

void det_arm_thread(HANDLE thread) {
    if (g_bp_count == 0) return;
    if (SuspendThread(thread) == (DWORD)-1) {
        fprintf(stderr, "det_arm_thread: SuspendThread failed gle=%lu\n", GetLastError());
        return;
    }
    CONTEXT ctx;
    ZeroMemory(&ctx, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(thread, &ctx)) {
        fprintf(stderr, "det_arm_thread: GetThreadContext failed gle=%lu\n", GetLastError());
        ResumeThread(thread);
        return;
    }
    DWORD* drs[4] = {&ctx.Dr0, &ctx.Dr1, &ctx.Dr2, &ctx.Dr3};
    for (int i = 0; i < g_bp_count; ++i) {
        if (g_bp[i].va == 0) continue; // slot registered but armed later from a VEH callback (det_ctx_arm_slot)
        *drs[i] = (DWORD)g_bp[i].va;
        ctx.Dr7 |= (1u << (i * 2)); // Li local-enable bit (L0=bit0, L1=bit2, ...); RW/LEN bits stay 0 (execute, 1 byte)
    }
    ctx.Dr6 = 0;
    if (!SetThreadContext(thread, &ctx)) {
        fprintf(stderr, "det_arm_thread: SetThreadContext failed gle=%lu\n", GetLastError());
    }
    ResumeThread(thread);
}

static DWORD WINAPI arm_main_thread_helper(LPVOID) {
    HANDLE h = OpenThread(THREAD_ALL_ACCESS, FALSE, g_main_tid);
    if (!h) { fprintf(stderr, "det: OpenThread(main) failed gle=%lu\n", GetLastError()); return 1; }
    det_arm_thread(h);
    CloseHandle(h);
    return 0;
}

void det_arm_main_thread() {
    if (g_bp_count == 0) return;
    HANDLE helper = CreateThread(nullptr, 0, arm_main_thread_helper, nullptr, 0, nullptr);
    if (!helper) { fprintf(stderr, "det: could not start arm-sensor helper thread, gle=%lu\n", GetLastError()); return; }
    WaitForSingleObject(helper, INFINITE);
    CloseHandle(helper);
    fprintf(stderr, "det: armed %d hardware breakpoint(s) on the guest main thread\n", g_bp_count);
}

LONG WINAPI det_veh_handler(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* ctx = ep->ContextRecord;
    DWORD dr6 = ctx->Dr6;
    bool handled = false;
    for (int i = 0; i < g_bp_count; ++i) {
        if (dr6 & (1u << i)) {
            handled = true;
            g_bp[i].on_hit(ctx);
        }
    }
    if (handled) {
        ctx->Dr6 = 0;
        ctx->EFlags |= 0x10000; // RF (resume flag): step past this instruction once without retriggering
    } else if (snapshot_trace_active()) {
        // Milestone 9's "--trace-window": a TRAP-FLAG single step, not one of
        // our four hardware breakpoints. Dr6 bit 14 (BS) is set instead of
        // bits 0-3, so the loop above found nothing - claim it here rather
        // than letting it fall through to main.cpp's fatal-crash handler.
        ctx->Dr6 = 0;
    } else {
        return EXCEPTION_CONTINUE_SEARCH; // not one of ours
    }
    // Logs this instruction and re-arms (or, at the end of the window,
    // clears) EFlags.TF in the context we are about to resume.
    if (snapshot_trace_active()) snapshot_trace_step(ctx);
    return EXCEPTION_CONTINUE_EXECUTION;
}

// --record-input header (item 2: "T press|release KEY_NAME ... plus # header
// lines: date, image sha256, policy, pace"). sha256 of the guest image is
// computed once, here, from opt.image_path (main.cpp's wrappers_set_guest_
// image_path target) - same pf::Sha256 already used for the digest sensor.
static bool sha256_file_hex(const char* path, char* out_hex, size_t out_n) {
    if (!path || !path[0]) { strncpy(out_hex, "(no image path)", out_n - 1); out_hex[out_n - 1] = 0; return false; }
    FILE* f = fopen(path, "rb");
    if (!f) { strncpy(out_hex, "(unreadable)", out_n - 1); out_hex[out_n - 1] = 0; return false; }
    pf::Sha256 sha;
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) sha.update(buf, n);
    fclose(f);
    std::string hex = sha.hex();
    strncpy(out_hex, hex.c_str(), out_n - 1);
    out_hex[out_n - 1] = 0;
    return true;
}

// ---------------------------------------------------------------------
void det_init(const DetOptions& opt, DetShutdownFn shutdown_hook) {
    g_det_mode = opt.det_mode;
    g_pace_real = opt.pace_real;
    g_stop_at_tick = opt.stop_at_tick;
    g_shutdown = shutdown_hook;
    g_input_policy = opt.input_policy;
    g_inject_real_test = opt.inject_real_test;
    g_interactive = opt.interactive;
    g_window_mode = opt.window_mode;
    strncpy(g_image_path, opt.image_path ? opt.image_path : "", sizeof(g_image_path) - 1);
    g_image_path[sizeof(g_image_path) - 1] = 0;
    g_main_tid = GetCurrentThreadId();
    g_start_tick64 = GetTickCount64();
    g_parked_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (g_det_mode) arena_init();

    bool need_safepoint = opt.stop_at_tick > 0 || opt.force_safepoint;
    if (opt.digest_out && opt.digest_out[0]) {
        g_digest_file = fopen(opt.digest_out, "w");
        if (!g_digest_file) fprintf(stderr, "det: could not open --digest-out '%s'\n", opt.digest_out);
        need_safepoint = true;
    }
    if (need_safepoint) register_breakpoint(VA_SAFEPOINT, safepoint_hit);

    // win32_pilot.md sec 5a: exclusive input policy. Real keyboard input is
    // parked (neutralize_keyboard_hit) whenever it is NOT the declared
    // provider - i.e. for Script AND None, not just in --det. Previously
    // this was gated on g_det_mode alone, which allowed the real keyboard
    // and a script to both reach Allegro in a non-det --input-script run -
    // exactly the defect win32_pilot.md sec 5a names. main.cpp's parse_args
    // already resolved/validated the policy (erroring on Real+input-script,
    // the one case that can't coexist with parking) before this runs.
    //
    // Item 2 ("tick-boundary real input" pass): input_policy==Real now gets
    // its OWN breakpoint at the same VA - real_key_capture_hit - instead of
    // leaving the real path completely unmonitored. This is what fixes
    // divergence 002 (see real_key_capture_hit/drain_real_key_queue's own
    // comments above): the event is captured and queued instead of running
    // straight through, then redelivered at the next tick boundary through
    // the SAME call site --input-script uses. The ONE exception is
    // --inject-real-test, which needs the real function's body to actually
    // execute (unneutralized) for its synthetic record/replay round trip -
    // see deliver_due_input's inject_real_test branch and carrier/NOTES.md
    // "Input policy and recording" part C, re-verified unchanged this pass.
    if (g_input_policy != InputPolicy::Real) {
        register_breakpoint(VA_KEY_DINPUT_SCANCODE, neutralize_keyboard_hit);
        fprintf(stderr, "det: real keyboard PARKED (input=%s; key_dinput_handle_scancode short-circuited)\n",
                input_policy_name(g_input_policy));
    } else if (!g_inject_real_test) {
        register_breakpoint(VA_KEY_DINPUT_SCANCODE, real_key_capture_hit);
        fprintf(stderr, "det: real keyboard CAPTURED at tick boundaries (input=real; events queued at "
                        "key_dinput_handle_scancode and delivered from the main thread's tick loop - "
                        "carrier/NOTES.md 'tick-boundary real input', fixes divergence 002)\n");
    } else {
        fprintf(stderr, "det: real keyboard ACTIVE, UNCAPTURED (input=real, --inject-real-test: the real "
                        "key_dinput_handle_scancode path runs unmodified for the synthetic round trip)\n");
    }

    if (opt.record_input && opt.record_input[0]) {
        g_record_file = fopen(opt.record_input, "w");
        if (!g_record_file) {
            fprintf(stderr, "det: could not open --record-input '%s'\n", opt.record_input);
        } else {
            char datebuf[32];
            SYSTEMTIME st; GetLocalTime(&st);
            _snprintf(datebuf, sizeof(datebuf), "%04d-%02d-%02d %02d:%02d:%02d",
                      st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            datebuf[sizeof(datebuf) - 1] = 0;
            char hexbuf[80];
            sha256_file_hex(g_image_path, hexbuf, sizeof(hexbuf));
            // Same "T verb KEY_NAME" shape --input-script reads; '#' lines
            // are comments there (load_script skips them), so a recorded
            // file is directly replayable with --input-script.
            fprintf(g_record_file, "# date: %s\n", datebuf);
            fprintf(g_record_file, "# image_sha256: %s\n", hexbuf);
            fprintf(g_record_file, "# policy: %s\n", input_policy_name(g_input_policy));
            fprintf(g_record_file, "# pace: %s\n", g_pace_real ? "real" : "fast");
            fflush(g_record_file);
        }
        register_breakpoint(VA_HANDLE_KEY_PRESS, keypress_record_hit);
        register_breakpoint(VA_HANDLE_KEY_RELEASE, keyrelease_record_hit);
    }
    if (opt.input_script && opt.input_script[0]) load_script(opt.input_script);

    // --trace-input PATH (divergence 005): the diagnostic that found the
    // cause. "-" means stderr. Works in BOTH modes and with either input
    // provider, deliberately: its whole point is a side-by-side diff of a
    // record run and a replay run.
    if (opt.trace_input && opt.trace_input[0]) {
        if (strcmp(opt.trace_input, "-") == 0) {
            g_trace_input_file = stderr;
        } else {
            g_trace_input_file = fopen(opt.trace_input, "w");
            if (!g_trace_input_file)
                fprintf(stderr, "det: could not open --trace-input '%s'\n", opt.trace_input);
        }
        if (g_trace_input_file)
            fprintf(g_trace_input_file,
                    "# --trace-input: ms=virtual clock, T=carrier tick (ms/20), sub=Sleep call within T "
                    "(0 = the tick's FIRST Sleep, where scripted events are delivered), sleepn=total Sleep "
                    "calls, cyc/pre/post=guest cycle_count @0x506938 around this Sleep's _handle_timer_tick, "
                    "sp=play() safepoints so far, tid=thread, site=call site\n");
    }

    fprintf(stderr,
            "det: interactive=%d window=%s (an automated run never takes the operator's foreground - "
            "carrier/NOTES.md 'Environment isolation')\n",
            (int)g_interactive, window_mode_name(g_window_mode));
    fprintf(stderr,
            "det: det_mode=%d pace=%s input=%s inject_real_test=%d stop_at_tick=%d digest_out=%s record_input=%s input_script=%s\n",
            g_det_mode, g_pace_real ? "real" : "fast", input_policy_name(g_input_policy), g_inject_real_test, g_stop_at_tick,
            opt.digest_out && opt.digest_out[0] ? opt.digest_out : "(none)",
            opt.record_input && opt.record_input[0] ? opt.record_input : "(none)",
            opt.input_script && opt.input_script[0] ? opt.input_script : "(none)");
}

// ---------------------------------------------------------------------
// "Environment isolation" pass: the entry patches. Separate from det_init
// because the guest image is not mapped yet when det_init runs (main.cpp
// calls det_init before pe_image_load, and this next to bind_init).
//
// Installed whenever the carrier owns determinism for this run:
//   --det           (any input policy), or
//   input != real   (script/none, the same rule that parks the keyboard).
// A plain, un-det, --input=real oracle run is left completely untouched.
// ---------------------------------------------------------------------
void det_install_entry_patches() {
    bool carrier_owns = g_det_mode || g_input_policy != InputPolicy::Real;
    if (!carrier_owns) {
        fprintf(stderr, "det: entry patches NOT installed (plain oracle run: no --det and input=real) - "
                        "window activation and the mouse reach the game exactly as they would standalone\n");
        return;
    }
    switch_queue_init();
    if (!isolate_off("switch")) {
        patch_entry_jmp(VA_SWITCH_IN, (void*)det_stub_switch_in, "_switch_in (dispsw.c)");
        patch_entry_jmp(VA_SWITCH_OUT, (void*)det_stub_switch_out, "_switch_out (dispsw.c)");
        g_switch_patched = true;
    } else {
        fprintf(stderr, "det: DET_ISOLATE_OFF=switch - window activation left UNCONTROLLED (audit mode)\n");
    }
    if (!isolate_off("mouse")) {
        patch_entry_jmp(VA_HANDLE_MOUSE_INPUT, (void*)det_stub_handle_mouse_input,
                        "_handle_mouse_input (mouse.c)");
        g_mouse_patched = true;
    } else {
        fprintf(stderr, "det: DET_ISOLATE_OFF=mouse - the real mouse left UNCONTROLLED (audit mode)\n");
    }
    fprintf(stderr,
            "det: window-activation channel %s; DirectInput mouse PARKED "
            "(game code reads mouse_x/y/b in exactly one function, main_menu_callback's "
            "pFLDAd-guarded ad hit-test - see carrier/NOTES.md 'Environment isolation')\n",
            g_input_policy == InputPolicy::Real
                ? "CAPTURED at the tick boundary and recorded as `T switch in|out`"
                : "SUPPRESSED (nothing the operator does reaches the guest)");
}

// --report's "environment" object: one place that says what this run did to
// every channel the "Environment isolation" pass took ownership of.
void det_environment_json(char* buf, size_t n) {
    char envs[1024]; envs[0] = 0;
    size_t used = 0;
    for (int i = 0; i < g_env_count; ++i) {
        int w = _snprintf(envs + used, sizeof(envs) - used, "%s{\"name\":\"%s\",\"calls\":%ld,\"found\":%s}",
                          i ? "," : "", g_env_names[i], g_env_hits[i], g_env_found[i] ? "true" : "false");
        if (w < 0 || used + (size_t)w >= sizeof(envs) - 1) break;
        used += (size_t)w;
    }
    envs[sizeof(envs) - 1] = 0;
    _snprintf(buf, n,
              "{\"interactive\":%s,\"window_mode\":\"%s\","
              "\"showwindow_substituted\":%ld,\"setforeground_suppressed\":%ld,"
              "\"setwindowpos_noactivate\":%ld,\"createwindow_devisible\":%ld,"
              "\"switch_patched\":%s,\"switch_captured\":%ld,\"switch_suppressed\":%ld,"
              "\"switch_delivered\":%ld,\"switch_recorded\":%ld,"
              "\"mouse_patched\":%s,\"mouse_events_parked\":%ld,"
              "\"ad_thread_suppressed\":%ld,"
              "\"time_recorded\":%ld,\"time_replayed\":%ld,\"time_available\":%zu,"
              "\"time_underflow\":%ld,\"time_offthread\":%ld,"
              "\"perturb_clock_ms\":%lld,\"perturb_time_s\":%ld,"
              "\"getenv_calls\":%ld,\"getenv_names\":[%s]}",
              g_interactive ? "true" : "false", window_mode_name(g_window_mode),
              g_showwindow_substituted, g_setforeground_suppressed,
              g_setwindowpos_noactivate, g_createwindow_devisible,
              g_switch_patched ? "true" : "false", g_switch_captured, g_switch_suppressed,
              g_switch_delivered, g_switch_recorded,
              g_mouse_patched ? "true" : "false", g_mouse_parked,
              g_ad_thread_suppressed,
              g_time_recorded, g_time_replayed, g_time_values.size(),
              g_time_underflow, g_time_offthread,
              perturb_clock_ms(), perturb_time_s(),
              g_env_calls, envs);
    buf[n - 1] = 0;
}

// ---------------------------------------------------------------------
// Milestone 8: carrier-owned snapshot state (det.hpp's DetSavedState).
// Everything here is a det.cpp static, i.e. outside the guest image, the
// arena and the guest stack - so a snapshot that only captured guest
// memory would rewind the game but not the clock, the script cursor or the
// RNG, and the replay would not line up. See snapshot.cpp.
// ---------------------------------------------------------------------
void det_state_save(DetSavedState* s) {
    memset(s, 0, sizeof(*s));
    s->virtual_ms = g_virtual_ms;
    s->units_reported = g_units_reported;
    s->rng_state = g_rng_state;
    s->rng_calls = g_rng_calls;
    s->script_cursor = (unsigned)g_script_cursor;
    // "How many bytes of the arena are live" - now read out of the arena's
    // OWN control block (divergence 004 rewrite): the allocator's whole state
    // (top, free list, block headers/footers) lives inside [0, top), so the
    // existing "arena" snapshot component captures the allocator unchanged
    // and this field keeps its exact old meaning and use (snapshot.cpp).
    s->arena_offset = det_arena_top();
    s->real_key_violations = g_real_key_violations;
    s->last_drain_tick = g_last_drain_tick;
    memcpy(s->key_held, g_key_held, sizeof(g_key_held));
    if (g_real_queue_cs_inited) EnterCriticalSection(&g_real_queue_cs);
    s->real_queue_head = g_real_queue_head;
    s->real_queue_tail = g_real_queue_tail;
    for (int i = 0; i < kRealQueueCap; ++i) {
        s->real_queue_code[i] = g_real_queue[i].allegro_code;
        s->real_queue_press[i] = g_real_queue[i].press ? 1 : 0;
    }
    if (g_real_queue_cs_inited) LeaveCriticalSection(&g_real_queue_cs);
    // "Environment isolation": the two new carrier-owned cursors/queues, for
    // exactly the reason the script cursor and the key queue are already here
    // (win32_pilot.md sec 6) - a rewind that moved the game back but left the
    // recorded-clock cursor or a pending switch event running forward would
    // not line up.
    s->time_cursor = (int)g_time_cursor;
    if (g_switch_cs_inited) EnterCriticalSection(&g_switch_cs);
    s->switch_queue_head = g_switch_head;
    s->switch_queue_tail = g_switch_tail;
    memcpy(s->switch_queue_dir, g_switch_queue, sizeof(g_switch_queue));
    if (g_switch_cs_inited) LeaveCriticalSection(&g_switch_cs);
}

void det_state_load(const DetSavedState* s) {
    g_virtual_ms = s->virtual_ms;
    g_units_reported = s->units_reported;
    g_rng_state = s->rng_state;
    g_rng_calls = (long)s->rng_calls;
    g_script_cursor = (size_t)s->script_cursor;
    // Nothing to do for the arena: snapshot.cpp has already memcpy'd
    // [0x20000000, +arena_offset) back, and that range CONTAINS the whole
    // allocator - control block (top/free_head/stats) at offset 0, block
    // headers, boundary-tag footers and free-list links inside the blocks.
    // The rewind therefore restores the allocator exactly, including which
    // blocks were free and in what free-list order, so the post-restore
    // allocation sequence reproduces the pre-restore addresses.
    g_real_key_violations = s->real_key_violations;
    g_last_drain_tick = s->last_drain_tick;
    memcpy(g_key_held, s->key_held, sizeof(g_key_held));
    real_queue_init();
    EnterCriticalSection(&g_real_queue_cs);
    g_real_queue_head = s->real_queue_head;
    g_real_queue_tail = s->real_queue_tail;
    for (int i = 0; i < kRealQueueCap; ++i) {
        g_real_queue[i].allegro_code = s->real_queue_code[i];
        g_real_queue[i].press = s->real_queue_press[i] != 0;
    }
    LeaveCriticalSection(&g_real_queue_cs);
    g_time_cursor = (size_t)s->time_cursor;
    switch_queue_init();
    EnterCriticalSection(&g_switch_cs);
    g_switch_head = s->switch_queue_head;
    g_switch_tail = s->switch_queue_tail;
    memcpy(g_switch_queue, s->switch_queue_dir, sizeof(g_switch_queue));
    LeaveCriticalSection(&g_switch_cs);
}

void det_shutdown() {
    if (g_digest_file) { fflush(g_digest_file); fclose(g_digest_file); g_digest_file = nullptr; }
    if (g_record_file) { fflush(g_record_file); fclose(g_record_file); g_record_file = nullptr; }
    exit_storm_flush();
    if (g_trace_input_file && g_trace_input_file != stderr) {
        fclose(g_trace_input_file); g_trace_input_file = nullptr;
    }
    fprintf(stderr, "det: shutdown at T=%d (virtual_ms=%lld, %lld main-thread Sleep calls, "
                    "%ld safepoints, guest cycle_count=%d)\n",
            det_current_tick(), (long long)g_virtual_ms, g_sleep_calls, g_safepoint_count,
            (int)IT_CYCLE_COUNT);
    // Audit evidence for the activation channel: WHO is registered in the
    // guest's own switch callback tables, and WHOSE window procedure the
    // guest window actually has (cnc-ddraw subclasses it - see
    // notes/determinism_audit.md). Printed unconditionally at shutdown so
    // every archived stderr carries it.
    {
        void** in_cb = (void**)(uintptr_t)VA_SWITCH_IN_CB;
        void** out_cb = (void**)(uintptr_t)VA_SWITCH_OUT_CB;
        char line[256]; int n = 0;
        n += _snprintf(line + n, sizeof(line) - n, "det: switch_in_cb =");
        for (int i = 0; i < 8; ++i) n += _snprintf(line + n, sizeof(line) - n, " %p", in_cb[i]);
        fprintf(stderr, "%s\n", line);
        n = 0;
        n += _snprintf(line + n, sizeof(line) - n, "det: switch_out_cb=");
        for (int i = 0; i < 8; ++i) n += _snprintf(line + n, sizeof(line) - n, " %p", out_cb[i]);
        fprintf(stderr, "%s\n", line);
        FocusSearch s = {GetCurrentProcessId(), nullptr};
        EnumWindows(focus_enum_proc_any, (LPARAM)&s);
        if (s.found) {
            LONG wp = GetWindowLongA(s.found, GWL_WNDPROC);
            RECT r = {0, 0, 0, 0};
            GetWindowRect(s.found, &r);
            fprintf(stderr, "det: guest window %p wndproc=0x%08lx (%s) style=0x%08lx exstyle=0x%08lx "
                            "rect=(%ld,%ld,%ld,%ld) activation-messages seen=%ld\n",
                    (void*)s.found, (unsigned long)wp,
                    ((unsigned long)wp == 0x4791e0ul) ? "Allegro's own directx_wnd_proc"
                                                      : "SUBCLASSED - not Allegro's directx_wnd_proc@0x4791e0",
                    (unsigned long)GetWindowLongA(s.found, GWL_STYLE),
                    (unsigned long)GetWindowLongA(s.found, GWL_EXSTYLE),
                    r.left, r.top, r.right, r.bottom, g_wndmsg_activate);
        }
    }
    fprintf(stderr,
            "det: environment isolation - window switch: captured=%ld suppressed=%ld delivered=%ld "
            "recorded=%ld; mouse events parked=%ld; ad thread suppressed=%ld; "
            "clock: recorded=%ld replayed=%ld/%zu underflow=%ld offthread=%ld; getenv calls=%ld "
            "over %d distinct name(s)\n",
            g_switch_captured, g_switch_suppressed, g_switch_delivered, g_switch_recorded,
            g_mouse_parked, g_ad_thread_suppressed,
            g_time_recorded, g_time_replayed, g_time_values.size(), g_time_underflow,
            g_time_offthread, g_env_calls, g_env_count);
    for (int i = 0; i < g_env_count; ++i)
        fprintf(stderr, "det:   getenv(\"%s\") x%ld -> %s\n", g_env_names[i], g_env_hits[i],
                g_env_found[i] ? "a value" : "NULL (unset on this host)");
    if (g_arena_base) {
        ArenaCtl* c = a_ctl();
        fprintf(stderr,
                "det: arena high-water %u bytes (%.2f MB), live %u bytes in %u blocks, "
                "peak live %u bytes; calls malloc=%u calloc=%u realloc=%u free=%u "
                "(foreign %u, bad %u)\n",
                c->hwm, c->hwm / (1024.0 * 1024.0), c->live_bytes, c->live_blocks,
                c->peak_live_bytes, c->n_malloc, c->n_calloc, c->n_realloc, c->n_free,
                c->n_free_foreign, c->n_free_bad);
    }
}
