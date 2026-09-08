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
#include "bind.hpp"   // divergence 010: the tick safepoint asks the binding table
                       // which entry address of its callee this run can reach
#include "snapshot.hpp" // milestones 8-9: safepoint snapshot / in-process rewind
#include "../../port_forge/src/platform/win32/trace.hpp" // pf_count_import - see det.hpp/wrappers.hpp (item 3)
#include "../../port_forge/src/platform/win32/arena.hpp"
#include "../../port_forge/src/platform/win32/breakpoints.hpp"
#include "../../port_forge/src/platform/win32/focus_channel.hpp"
#include "../../port_forge/src/platform/win32/input_channel.hpp"
#include "../../port_forge/src/platform/win32/threads.hpp"
#include "../../port_forge/src/platform/win32/virtual_clock.hpp"
#include "../../port_forge/src/platform/win32/rng.hpp"
#include "../../port_forge/src/core/sha256.hpp"
#include "../win32_policy.hpp"

// KNOWN (artifacts/functions.json + disasm.txt): Allegro internals this
// module calls directly by address (they're outside the game's own 25 CUs,
// so they're not in carrier/gen/it_funcs.h, which is game-scope only).
// The tick safepoint is POLICY now, not a literal: icytower::kTickSafepoint
// (carrier/win32_policy.hpp) names the caller whose tick loop it is, the
// callee that loop runs exactly once per consumed tick, and that callee's
// original entry VA. It replaced the literal 0x4124f4 - an address INSIDE
// play()'s own bytes, which stops existing the moment play is bound
// (divergence 010; the full argument is in the policy header and in
// carrier/NOTES.md). resolve_tick_safepoint(), below, turns the policy into
// the one address THIS run can reach.

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
// wdispsw.c: the two functions directx_wnd_proc's WM_ACTIVATE arm calls
// (0x4793bb/0x47941f -> _win_switch_in, 0x479678 -> _win_switch_out); each
// ends in a tail `jmp` to _switch_in/_switch_out above. --inject-real-test
// feeds a scripted `T switch in|out` through THESE, i.e. through the real
// Allegro path, so the capture hook sees it exactly as a real WM_ACTIVATE
// would produce it - the same fallback pattern this carrier already uses for
// keys (carrier/NOTES.md "Input policy and recording" part C).
#define VA_WIN_SWITCH_IN        0x47a47cu
#define VA_WIN_SWITCH_OUT       0x47a3d4u
#define VA_HANDLE_MOUSE_INPUT   0x45f9bcu

// The virtual epoch det_wrap_time returns when no recording supplies a value
// (unchanged from milestones 5-7 - this is what keeps G1 byte-identical).
#define DET_VIRTUAL_EPOCH 1700000000L

// The Allegro KEY_* code table is icytower::kKeyNames
// (carrier/win32_policy.hpp), reached through icytower::kInputBinding.
using pf::win32::KeyName;

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

// The virtual clock, the tick pump's accumulator and the two determinism-
// audit perturbation knobs are pf::win32::* (virtual_clock.hpp).

static FILE* g_digest_file = nullptr;
static FILE* g_record_file = nullptr;
// --dump-assets PATH (det.hpp's own comment) - a plain C string copy (the
// DetOptions pointer may point at Options::dump_assets on main()'s own
// stack-ish storage, but det_init already copies every other such pointer
// the same way input_script/digest_out do - kept here as a raw pointer to
// match that existing convention, valid for the process lifetime).
static const char* g_dump_assets_path = nullptr;
// src/dump_assets.c - a plain-C-signature function, deliberately declared
// here rather than in a shared header: this is the ONLY C++ TU that calls
// it, and its own compile unit force-includes headers (pf_bindings_src.h/
// pf_lib_bindings.h/pf_asset_bindings.h) that redefine BITMAP and would
// collide with this file's <windows.h> if ever seen together (win32_pilot.md
// SS7a; carrier/build.cmd compiles src\dump_assets.c as its own cl
// invocation for exactly this reason).
extern "C" void pf_dump_assets(const char* path);

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
static int  g_safepoint_slot = -1;       // DR slot the tick safepoint claimed, or -1
static int  g_cyc_pre = 0, g_cyc_post = 0; // guest cycle_count around this Sleep's _handle_timer_tick
#define IT_CYCLE_COUNT (*(volatile int*)(uintptr_t)0x506938u)

static void trace_input(const char* site, const char* what, int code, const char* extra) {
    if (!g_trace_input_file) return;
    fprintf(g_trace_input_file,
            "ms=%lld T=%d sub=%lld sleepn=%lld cyc=%d pre=%d post=%d sp=%ld tid=%lu site=%s %s code=%d%s%s\n",
            pf::win32::clock_now_ms(), pf::win32::clock_tick(),
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
// Divergence 009 (host device enumeration, see det_wrap_DirectSoundEnumerateA).
static FARPROC g_real_DirectSoundEnumerateA = nullptr;

// Import ids (see wrappers.hpp/det_bind_real doc), one per always-installed
// wrapper this file defines - each det_wrap_* below calls pf_count_import
// with its own id (item 3: fixes the report.json gap for these, which are
// wired directly into the guest IAT, bypassing the counting trampoline).
static int g_id_Sleep = -1, g_id_QPC = -1, g_id_timeGetTime = -1, g_id_time = -1,
           g_id_clock = -1, g_id_beginthread = -1,
           g_id_malloc = -1, g_id_calloc = -1, g_id_realloc = -1, g_id_free = -1,
           g_id_WaitForSingleObject = -1, g_id_rand = -1, g_id_srand = -1,
           g_id_ShowWindow = -1, g_id_SetForegroundWindow = -1, g_id_SetWindowPos = -1,
           g_id_CreateWindowExA = -1, g_id_pthread_create = -1, g_id_getenv = -1,
           g_id_DirectSoundEnumerateA = -1;

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
    else if (strcmp(name, "DirectSoundEnumerateA") == 0) { g_real_DirectSoundEnumerateA = (FARPROC)real_proc; g_id_DirectSoundEnumerateA = id; }
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
// The generator itself, its state, and the selftest are
// pf::win32::rng_* (port_forge/src/platform/win32/rng.hpp). Only the
// wrappers stay here: whether this run is deterministic at all, the import
// counting, and the forward to the real msvcrt entry points are carrier
// composition, not the generator.
extern "C" int __cdecl det_wrap_rand() {
    pf_count_import(g_id_rand);
    if (g_det_mode) return pf::win32::rng_next();
    if (g_real_rand) return ((int(__cdecl*)())g_real_rand)();
    return 0;
}

extern "C" void __cdecl det_wrap_srand(unsigned seed) {
    pf_count_import(g_id_srand);
    if (g_det_mode) { pf::win32::rng_seed(seed); return; }
    if (g_real_srand) ((void(__cdecl*)(unsigned))g_real_srand)(seed);
}

// Snapshot accessors (snapshot.cpp).
unsigned det_rng_state() { return pf::win32::rng_state(); }
void det_set_rng_state(unsigned s) { pf::win32::rng_set_state(s); }
long det_rng_calls() { return pf::win32::rng_calls(); }
void det_set_rng_calls(long n) { pf::win32::rng_set_calls(n); }

// --rng-selftest: the unit check win32_pilot.md's milestone-8 brief asks
// for. Runs AFTER imports_init (so g_real_rand/g_real_srand point at the
// REAL msvcrt.dll entry points the guest would otherwise have used) and
// BEFORE the guest starts; the process exits with 0 on match, 4 on
// mismatch.
int det_rng_selftest() { return pf::win32::rng_selftest(g_real_rand, g_real_srand); }

// ---------------------------------------------------------------------
// A (extended). The deterministic heap arena, det mode only. The whole
// rationale - why a fixed-address heap is needed at all (Windows
// randomizes the CRT heap base per process and the guest bakes heap
// pointers into .bss), why a bump-only allocator was NOT enough
// (divergence 004: the main menu creates and destroys an ~800 KB bitmap
// every frame and exhausted 256 MiB in ~450 ticks), and the two properties
// the allocator has to keep - is in
// port_forge/src/platform/win32/arena.hpp, with the code it explains.
// ---------------------------------------------------------------------
// The allocator itself is pf::win32::arena_*
// (port_forge/src/platform/win32/arena.hpp); its placement is
// icytower::kArena (carrier/win32_policy.hpp). Only the four wrappers stay
// here, because "is this run deterministic", the import counting and the
// forward to the real msvcrt heap are carrier composition.
void det_arena_stats(unsigned* top, unsigned* hwm, unsigned* live_bytes,
                     unsigned* peak_live_bytes, unsigned* live_blocks) {
    pf::win32::ArenaStats s = pf::win32::arena_stats();
    if (top) *top = s.top;
    if (hwm) *hwm = s.hwm;
    if (live_bytes) *live_bytes = s.live_bytes;
    if (peak_live_bytes) *peak_live_bytes = s.peak_live_bytes;
    if (live_blocks) *live_blocks = s.live_blocks;
}
unsigned det_arena_top() { return pf::win32::arena_top(); }

extern "C" void* __cdecl det_wrap_malloc(size_t n) {
    pf_count_import(g_id_malloc);
    if (g_det_mode && pf::win32::arena_active()) {
        pf::win32::arena_count_malloc();
        return pf::win32::arena_alloc(n);
    }
    return g_real_malloc ? ((void*(__cdecl*)(size_t))g_real_malloc)(n) : nullptr;
}
extern "C" void* __cdecl det_wrap_calloc(size_t count, size_t size) {
    pf_count_import(g_id_calloc);
    if (g_det_mode && pf::win32::arena_active()) {
        pf::win32::arena_count_calloc();
        size_t n = count * size;
        void* p = pf::win32::arena_alloc(n);
        // MUST zero explicitly: unlike a bump-only allocator, a block can be
        // recycled memory, not a fresh (already-zero) VirtualAlloc page.
        if (p && n) memset(p, 0, n);
        return p;
    }
    return g_real_calloc ? ((void*(__cdecl*)(size_t, size_t))g_real_calloc)(count, size) : nullptr;
}
extern "C" void* __cdecl det_wrap_realloc(void* p, size_t n) {
    pf_count_import(g_id_realloc);
    if (g_det_mode && pf::win32::arena_active()) {
        pf::win32::arena_count_realloc();
        if (!p) return pf::win32::arena_alloc(n);              // realloc(NULL, n) == malloc(n)
        if (!pf::win32::arena_owns(p)) return pf::win32::arena_alloc(n); // foreign pointer
        size_t cap = pf::win32::arena_capacity_of(p);
        if (n <= cap) {                                        // fits in place - msvcrt may do this too
            pf::win32::arena_set_user_size(p, n);
            return p;
        }
        size_t old = pf::win32::arena_size_of(p);
        void* np = pf::win32::arena_alloc(n);
        if (!np) return nullptr;                               // msvcrt: original block stays valid
        if (old) memcpy(np, p, old < n ? old : n);             // growth copies
        pf::win32::arena_free(p);
        return np;
    }
    return g_real_realloc ? ((void*(__cdecl*)(void*, size_t))g_real_realloc)(p, n) : nullptr;
}
extern "C" void __cdecl det_wrap_free(void* p) {
    pf_count_import(g_id_free);
    if (g_det_mode && pf::win32::arena_active()) {
        pf::win32::arena_count_free();
        if (!p) return;                            // free(NULL) is a no-op
        if (!pf::win32::arena_owns(p)) {           // msvcrt-internal block, or already freed:
            pf::win32::arena_count_free_foreign(); // leak it, exactly as the bump allocator did
            return;
        }
        pf::win32::arena_free(p);
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
static LONGLONG det_now_ms() { return pf::win32::clock_now_ms(); }
static int det_current_tick() { return pf::win32::clock_tick(); }

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
// The script's own shape (one file, one line per owned channel), its
// parser, the tick-ordered event vector, the key-name table lookups and the
// two scancode spaces are all pf::win32::* (input_channel.hpp), driven by
// icytower::kInputBinding. This file keeps only the two aliases the rest of
// it reads through.
using EvKind = pf::win32::InputEventKind;
using ScriptEvent = pf::win32::InputEvent;
#define g_script        (pf::win32::script_events())
#define g_time_values   (pf::win32::time_events())
static int resolve_key(const char* t)      { return pf::win32::resolve_key(t); }
static const char* scancode_to_name(int c) { return pf::win32::scancode_to_name(c); }
static int allegro_to_dik(int code)        { return pf::win32::internal_to_hw(code); }
static int dik_to_allegro(int dik)         { return pf::win32::hw_to_internal(dik); }

static void load_script(const char* path) {
    // A malformed recording is FATAL here rather than in the framework:
    // whether a partially-understood script may still be replayed is the
    // carrier's question, and this one answers no.
    if (!pf::win32::load_input_script(path)) exit(2);
}

// The ONE piece of the capture side that does not move: a naked shim that
// calls key_dinput_handle_scancode with its arguments in the registers it
// actually reads them from.
//
// DWARF says (artifacts/dwarf_info.txt, wkeybd.c line 321)
// `void key_dinput_handle_scancode(int scancode, int pressed)`, but KNOWN
// from the disassembly at 0x46d5a8 both arguments arrive in REGISTERS
// (AL/EAX = scancode, EDX = pressed), never on the stack - a GCC
// -mregparm-style entry - so an ordinary function-pointer cast, which would
// push cdecl stack arguments, cannot call it. InputBindingPolicy could carry
// a `capture_abi` field, but a naked shim cannot be parameterized by one
// without a stub per ABI, and one ABI has been seen. It stays here, cited,
// until a second target brings a second convention (notes/extraction_plan.md
// section 4).
//
// kKeyDinputVA is a plain global rather than a literal inside the __asm
// block: MASM inline assembly does not accept C's `0x...u` suffix, and it
// treats a bare identifier as a memory operand - so `mov ecx, kKeyDinputVA`
// loads the stored address and `call ecx` is a normal direct call to it.
static const DWORD kKeyDinputVA = (DWORD)icytower::kInputBinding.capture_va;

extern "C" void __declspec(naked) __cdecl call_key_dinput_handle_scancode(int scancode, int pressed) {
    __asm {
        mov eax, [esp+4]
        mov edx, [esp+8]
        mov ecx, kKeyDinputVA
        call ecx
        ret
    }
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

// The queue, the guest-callback-table replay ("byte-for-byte what
// _switch_in/_switch_out do", so the patched originals are never
// re-entered) and the save/load of the pending queue are pf::win32::*
// (focus_channel.hpp), driven by icytower::kFocusChannel.
static void run_switch_callbacks(bool in) { pf::win32::run_switch_callbacks(in); }
static void switch_queue_push(bool in)    { pf::win32::focus_queue_push(in); }
static bool switch_queue_pop(bool* in)    { return pf::win32::focus_queue_pop(in); }

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

// 5-byte `jmp rel32` entry patch: pf::win32::patch_entry_jmp
// (port_forge/src/platform/win32/breakpoints.hpp). Failing to take control
// of an entry point is fatal HERE rather than in the framework, because
// whether a carrier can survive an unowned channel is the carrier's own
// question: this one cannot - a run that silently left the real
// window-activation or mouse path in place would report determinism it did
// not have.
static void patch_entry_jmp(DWORD_PTR va, void* target, const char* what) {
    if (!pf::win32::patch_entry_jmp(va, target, what)) exit(3);
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
    // Diagnostic: PF_KEY_ASCII=0 restores the pre-2026-09-07 behaviour (ascii
    // always 0) so a recording digest made under it can be re-attributed.
    { static int mode = -1; if (mode < 0) { char b[8]; mode = (GetEnvironmentVariableA("PF_KEY_ASCII", b, sizeof b) > 0 && b[0] == '0') ? 0 : 1; } if (mode == 0) return 0; }
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
    while (pf::win32::script_cursor() < g_script.size() &&
           g_script[pf::win32::script_cursor()].tick <= T) {
        const ScriptEvent& e = g_script[pf::win32::script_cursor()];
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
            pf::win32::set_script_cursor(pf::win32::script_cursor() + 1);
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
                pf::win32::set_script_cursor(pf::win32::script_cursor() + 1);
                continue;
            }
            g_in_delivery = true;
            call_key_dinput_handle_scancode(dik, press ? 1 : 0);
            g_in_delivery = false;
        } else if (press) {
            g_in_delivery = true;
            ((PressFn)(void*)(uintptr_t)icytower::kInputBinding.deliver_press_va)(ascii_for_allegro_code(e.scancode), e.scancode);
            g_in_delivery = false;
        } else {
            g_in_delivery = true;
            ((ReleaseFn)(void*)(uintptr_t)icytower::kInputBinding.deliver_release_va)(e.scancode);
            g_in_delivery = false;
        }
        fprintf(stderr, "det: T=%d delivered %s scancode=%d%s\n", T, press ? "press" : "release", e.scancode,
                g_inject_real_test ? " (via key_dinput_handle_scancode, --inject-real-test)" : "");
        trace_input("deliver_due_input(Sleep,after _handle_timer_tick)",
                    press ? "press" : "release", e.scancode,
                    g_inject_real_test ? "via=key_dinput_handle_scancode" : "via=_handle_key_press/release");
        pf::win32::set_script_cursor(pf::win32::script_cursor() + 1);
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
// Capacity of the real-key capture ring (pf::win32, input_channel.hpp) -
// restated here only for the storm summary's "queue full (%d events)" line.
static const int kRealQueueCap = pf::win32::detail::kRealQueueCap;
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

// The capture ring is pf::win32::* (input_channel.hpp): capture runs on
// whichever thread the guest's input handler runs on, delivery runs on the
// main thread at a tick boundary, and a fixed-capacity ring is the handover
// - fixed so a snapshot can carry it as POD.
using RealKeyEvent = pf::win32::RealKeyEvent;
static bool queue_real_key(int allegro_code, bool press) {
    return pf::win32::real_queue_push(allegro_code, press);
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
    } else if (!queue_real_key(allegro_code, press)) {
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
    while (pf::win32::real_queue_pop(&e)) {
        trace_input("drain_real_key_queue(Sleep,after _handle_timer_tick)",
                    e.press ? "press" : "release", e.internal_code, "phase=deliver");
        g_in_delivery = true;
        if (e.press) ((PressFn)(void*)(uintptr_t)icytower::kInputBinding.deliver_press_va)(0, e.internal_code);
        else ((ReleaseFn)(void*)(uintptr_t)icytower::kInputBinding.deliver_release_va)(e.internal_code);
        g_in_delivery = false;
        fprintf(stderr, "det: T=%d delivered real %s scancode=%d (captured at tick boundary)\n",
                T, e.press ? "press" : "release", e.internal_code);
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
// The four thread dispositions (ParkReal / VirtualizeStub / Suppress /
// Passthrough), the parked-thread registry, the real-thread-with-
// unconditional-waits trick that fixes divergence 003's exit hang, and the
// never-signaled stub handle all live in
// port_forge/src/platform/win32/threads.hpp, with the reason ParkReal had
// to replace VirtualizeStub written down beside them. WHICH entry VAs get
// which disposition is icytower::kThreads (carrier/win32_policy.hpp).

extern "C" DWORD __stdcall det_wrap_WaitForSingleObject(HANDLE h, DWORD ms) {
    pf_count_import(g_id_WaitForSingleObject);
    // A parked thread's own wait becomes unconditional, so it can only
    // resume when the guest itself signals the object (real exit), never on
    // a timeout - which would otherwise run a timer tick from the wrong
    // thread and reintroduce exactly the race milestones 5-7 removed. See
    // port_forge/src/platform/win32/threads.hpp.
    ms = pf::win32::parked_wait_timeout(ms);
    if (g_real_WaitForSingleObject)
        return ((DWORD(__stdcall*)(HANDLE, DWORD))g_real_WaitForSingleObject)(h, ms);
    return WAIT_FAILED;
}

extern "C" uintptr_t __cdecl det_wrap_beginthread(void(__cdecl* start)(void*),
                                                    unsigned stack_size, void* arglist) {
    pf_count_import(g_id_beginthread);
    uintptr_t start_va = (uintptr_t)(void*)start;
    fprintf(stderr, "det: _beginthread(start=0x%p, stack=%u)\n", (void*)start, stack_size);
    if (g_det_mode) {
        // icytower::kThreads (carrier/win32_policy.hpp) says which entry VA
        // gets which disposition; port_forge/src/platform/win32/threads.hpp
        // says what each disposition means and why ParkReal had to replace
        // VirtualizeStub for the timer threads.
        switch (pf::win32::thread_disposition((unsigned long)start_va)) {
        case pf::win32::ThreadDisposition::ParkReal:
            fprintf(stderr, "det: timer thread PARKED (entry=0x%p): running the ORIGINAL entry point "
                            "on a real thread whose WaitForSingleObject calls are substituted to "
                            "INFINITE (carrier/NOTES.md 'parked timer thread' - fixes divergence 003, "
                            "the _tim_win32_exit join hang, generically)\n", (void*)start);
            return pf::win32::make_parked_real_handle(start, arglist);
        case pf::win32::ThreadDisposition::VirtualizeStub:
            fprintf(stderr, "det: input thread virtualized (entry=0x%p) - synthetic key events "
                            "drive key[] instead\n", (void*)start);
            return pf::win32::make_virtualized_handle();
        case pf::win32::ThreadDisposition::Suppress:
            fprintf(stderr, "det: thread SUPPRESSED (entry=0x%p)\n", (void*)start);
            return 0;
        case pf::win32::ThreadDisposition::Passthrough:
            break;
        }
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
    if (h != 0 && h != (uintptr_t)-1) pf::win32::arm_thread((HANDLE)h);
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
        // ORDER MATTERS, and it is the divergence-005 order: the clock
        // advances FIRST, then sub_tick_advance decides which sub-tick slot
        // of the (possibly new) tick this Sleep call is, then the guest's
        // tick function runs. virtual_clock.hpp keeps advance and pump
        // separate for exactly this reason.
        pf::win32::virtual_clock_advance_ms(ms);
        sub_tick_advance();
        g_cyc_pre = IT_CYCLE_COUNT;
        pf::win32::virtual_clock_pump();
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
extern "C" BOOL __stdcall det_wrap_QueryPerformanceCounter(LARGE_INTEGER* out) {
    pf_count_import(g_id_QPC);
    if (g_det_mode) {
        if (out) out->QuadPart = pf::win32::clock_perturbed_ms(); // fake 1000 Hz counter tied to the virtual clock
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
    if (g_det_mode) return (DWORD)pf::win32::clock_perturbed_ms();
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
        long epoch = (long)(DET_VIRTUAL_EPOCH + pf::win32::virtual_ms() / 1000) + pf::win32::clock_time_offset_s();
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
            if (pf::win32::time_cursor() < g_time_values.size()) {
                v = g_time_values[pf::win32::time_cursor()].value;
                pf::win32::set_time_cursor(pf::win32::time_cursor() + 1);
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
    if (g_det_mode) return (long)pf::win32::clock_perturbed_ms();
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
    if (g_det_mode && !isolate_off("ad") &&
        pf::win32::thread_disposition((unsigned long)(uintptr_t)(void*)start) ==
            pf::win32::ThreadDisposition::Suppress) {
        ++g_ad_thread_suppressed;
        fprintf(stderr, "det: ad-fetch thread SUPPRESSED (pthread_create(fldads_threadmain @0x%08x) "
                        "is a no-op in --det; the game observes the constant 'no ads' result - "
                        "carrier/NOTES.md 'Environment isolation')\n",
                (unsigned)(uintptr_t)(void*)start);
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
// Divergence 009: a host ENUMERATION result reaches the digest domain
// through the deterministic arena's allocation POSITIONS.
//
// THE GENERIC RULE this implements (carrier/win32_policy.json,
// "digest_domain.host_enumeration_policy"): in a carrier-owned deterministic
// run the guest may never observe a host enumeration - the set, the order,
// the count or the names of whatever devices the OS happens to report - even
// when no game global stores the enumerated values themselves. The guest
// turns an enumeration into ALLOCATIONS (one per item, sized by the item's
// name), and every later allocation from the shared deterministic arena is
// displaced by exactly that prefix. Since ~27 of the 151 digest-domain
// globals hold arena pointers (`sounds`, `combo_sound`, `bg_beat`, `data`,
// `swap_screen`, `ply`, `custom`, ... - measured with pf_inspect on a
// tick-237 snapshot), a host that gains or loses one device changes the
// digest from the very first gameplay tick while the GAME OUTCOME is
// identical. That is a defect of the verdict domain, not of the game.
//
// MEASURED, this host, this day (all numbers from --report JSONs, which is
// how the channel was finally named):
//   _get_win_digi_driver_list (wddsnd.c, 0x47ac18) calls DirectSoundEnumerateA
//   once with DSEnumCallback (0x47bcf4). That callback SKIPS the NULL-GUID
//   "primary" entry and, for every real render device, does
//   _al_malloc(strlen(description)+1) + _al_sane_strncpy into
//   _dsalmix_name_list[16] (0x4ed3a0), stores the GUID pointer in
//   _dsalmix_guid_list[16] (0x4ed3e0) and bumps _dsalmix_count (0x4ecb64,
//   capped at 16). _get_win_digi_driver_list then allocates once more per
//   device in _get_dsalmix_driver, reallocs the driver list, and mallocs a
//   0xb8-byte DIGI_DRIVER copy per device for DIGI_DIRECTX(i).
//   Per device that is ~10 arena blocks / ~992 bytes and 3 strncat calls, so
//   the whole channel is legible in any --report: strncat == 3*devices + 3.
//   17:00-19:23: strncat=27 (8 devices), arena top 28468560. From ~19:40:
//   strncat=21 (6 devices), arena top 28466576 - two NVIDIA HDMI monitor
//   audio endpoints left the host. Nothing in git changed; every replay
//   diverged at T=237, the first digest line.
//
// THE FIX: this wrapper. Whenever the carrier owns determinism it does NOT
// let the guest see the host's list. It runs the real enumeration ITSELF,
// into carrier statics (host memory - never the arena), and then invokes the
// guest's callback with a CONSTANT, carrier-owned list: a fixed number of
// devices (kDetDSoundDevices, default 1) with fixed-length synthetic
// descriptions. The one thing kept from the host is each device's GUID
// VALUE, copied into carrier memory and handed back so the later
// DirectSoundCreate still opens a real device - and a GUID never reaches the
// digest domain, because _dsalmix_guid_list is one of Allegro's globals, not
// one of the game's 151.
//
// Delivering exactly ONE device is not a behavioural change on this host:
// DirectSoundEnumerate lists the default render device first among the real
// ones, and the guest was already opening index 0 (MEASURED: the traced
// DirectSoundCreate lpGuid equals _dsalmix_guid_list[0]). It also collapses
// the second half of the channel - the device NAMES, whose lengths are
// malloc sizes.
//
// Knobs, both diagnostics in the DET_ISOLATE_OFF / DET_PERTURB_* style,
// never used by a gate:
//   DET_ISOLATE_OFF=dsound   forward to the host enumeration unchanged
//                            (negative control: reproduces the host-dependent
//                            stream this host produces today)
//   DET_DSOUND_DEVICES=N     deliver N synthetic devices instead of 1
//                            (positive control: the digest must move with N)
// ---------------------------------------------------------------------
typedef BOOL (__stdcall* DetDSEnumCallbackA)(void* lpGuid, const char* desc,
                                             const char* mod, void* ctx);
static const int kDetDSoundMax = 16;      // Allegro's own _dsalmix_* cap
static const int kDetDSoundDevices = 1;   // the constant list's size

struct DetDSoundDevice {
    unsigned char guid[16];
    char name[64];
};
static DetDSoundDevice g_ds_host[kDetDSoundMax];
static int  g_ds_host_count = 0;   // real non-primary render devices the host reported
static int  g_ds_delivered = -1;   // devices handed to the guest (-1 = never enumerated)
static bool g_ds_normalized = false;

static int det_dsound_devices_wanted() {
    char v[16];
    if (GetEnvironmentVariableA("DET_DSOUND_DEVICES", v, sizeof(v)) && v[0])
        return atoi(v);
    return kDetDSoundDevices;
}

// Carrier-side harvest callback. Runs on the guest main thread but allocates
// nothing: it copies the GUID bytes and the description into carrier statics.
static BOOL __stdcall det_ds_harvest(void* lpGuid, const char* desc,
                                     const char* /*mod*/, void* /*ctx*/) {
    if (!lpGuid) return TRUE; // the NULL-GUID "primary" entry, which DSEnumCallback skips too
    if (g_ds_host_count < kDetDSoundMax) {
        memcpy(g_ds_host[g_ds_host_count].guid, lpGuid, 16);
        strncpy(g_ds_host[g_ds_host_count].name, desc ? desc : "",
                sizeof(g_ds_host[0].name) - 1);
        g_ds_host[g_ds_host_count].name[sizeof(g_ds_host[0].name) - 1] = 0;
        ++g_ds_host_count;
    }
    return TRUE;
}

extern "C" long __stdcall det_wrap_DirectSoundEnumerateA(void* cb, void* ctx) {
    pf_count_import(g_id_DirectSoundEnumerateA);
    typedef long (__stdcall* FnEnum)(void*, void*);
    FnEnum real = (FnEnum)g_real_DirectSoundEnumerateA;

    bool carrier_owns = g_det_mode || g_input_policy != InputPolicy::Real;
    if (!real) return 0;
    if (!carrier_owns || isolate_off("dsound")) {
        if (carrier_owns)
            fprintf(stderr, "det: DET_ISOLATE_OFF=dsound - the HOST DirectSound device list is "
                            "passed to the guest UNCONTROLLED (audit mode; divergence 009)\n");
        return real(cb, ctx);
    }

    g_ds_host_count = 0;
    long hr = real((void*)det_ds_harvest, nullptr);

    int want = det_dsound_devices_wanted();
    if (want < 0) want = 0;
    if (want > g_ds_host_count) want = g_ds_host_count;
    if (want > kDetDSoundMax) want = kDetDSoundMax;

    DetDSEnumCallbackA guest = (DetDSEnumCallbackA)cb;
    int delivered = 0;
    if (guest) {
        for (int i = 0; i < want; ++i) {
            // CONSTANT length for i < 10, and constant content: this string is
            // the malloc size DSEnumCallback asks the arena for.
            char name[48];
            _snprintf(name, sizeof(name), "PortForge Deterministic Audio Device %d", i);
            name[sizeof(name) - 1] = 0;
            ++delivered;
            if (!guest(g_ds_host[i].guid, name, "", ctx)) break;
        }
    }
    g_ds_delivered = delivered;
    g_ds_normalized = true;

    fprintf(stderr, "det: DirectSound enumeration NORMALIZED - host reported %d render device(s), "
                    "the guest was given %d constant carrier-owned one(s) "
                    "(a host enumeration must not reach the digest domain - "
                    "carrier/NOTES.md 'Divergence 009')\n",
            g_ds_host_count, delivered);
    for (int i = 0; i < g_ds_host_count; ++i)
        fprintf(stderr, "det:   host dsound device [%d] '%s'%s\n", i, g_ds_host[i].name,
                i < delivered ? "  (GUID reused for the synthetic device)" : "");
    if (g_ds_host_count == 0)
        fprintf(stderr, "det: WARNING - this host reports NO DirectSound render device; Allegro will "
                        "fall back to its WaveOut mixer and the digest of this run is not comparable "
                        "with a run made on a host that has one (determinism_audit.md row 12)\n");
    return hr;
}

// ---------------------------------------------------------------------
// C. Tick sensor: generic {VA, callback} hardware-breakpoint table.
// Dr0-Dr3 give up to 4 simultaneous exec breakpoints; slot 0 is always the
// play() safepoint when digest/stop-at-tick is requested, slots 1-2 are the
// key-event recorder when --record-input is requested.
// ---------------------------------------------------------------------
// The table itself, the four-slot budget, the RF resume-flag rule, the
// arm-from-inside-your-own-handler trick and the arming helper thread are
// pf::win32::* (port_forge/src/platform/win32/breakpoints.hpp). What stays
// here is WHO registers WHICH slot in WHAT ORDER - the composition, which
// is what actually decides this carrier's DR budget (det_init, below).
static int register_breakpoint(DWORD_PTR va, void (*cb)(CONTEXT*)) {
    return pf::win32::register_breakpoint(va, cb);
}
int det_register_breakpoint(DWORD_PTR va, void (*cb)(CONTEXT*)) {
    return pf::win32::register_breakpoint(va, cb);
}
void det_ctx_arm_slot(CONTEXT* ctx, int slot, DWORD_PTR va) {
    pf::win32::ctx_arm_slot(ctx, slot, va);
}
void det_ctx_disarm_slot(CONTEXT* ctx, int slot) {
    pf::win32::ctx_disarm_slot(ctx, slot);
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
    // --dump-assets PATH (det.hpp's own comment): once, at the FIRST
    // safepoint this run ever reaches. By construction this is always
    // after the guest's own init_game() has loaded every datafile (that
    // happens once, early, well before play() -- the only place this
    // safepoint fires -- is ever entered), so "first safepoint" is a
    // convenient, always-late-enough hook without needing a dedicated
    // "just after loading" breakpoint of its own.
    if (g_dump_assets_path) {
        static bool dumped_assets = false;
        if (!dumped_assets) {
            pf_dump_assets(g_dump_assets_path);
            dumped_assets = true;
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

// TEMPORARY diagnostic (".itr workload" investigation, carrier/NOTES.md
// "Allegro inline primitives; .itr workload") - env-var-only, opt-in, same
// convention as DET_DUMP_MEM_TICK/DET_INPUT_DELIVER_SUB above. Two fixed
// VAs inside _replay_selector (0x41d258, artifacts/disasm.txt), hand-
// disassembled, not guessed:
//   0x41d671 - the `call _readkey` site reached ONLY when either of
//              _replay_selector's two `call _keypressed(); test eax,eax`
//              checks (0x41d416/0x41d41d and 0x41d664/0x41d669) found a
//              nonzero result - i.e. a breakpoint here proves keypressed()
//              actually returned true at least once, independent of the
//              local debounce counter's exact value (which this sensor
//              does NOT need to read - both call sites converge on this
//              one address only on the "true" edge).
//   0x41d9dd - the jump table's own "confirm the highlighted entry"
//              handler (checks the per-entry directory byte, calls
//              play_menu_select() - carrier/scripts/play_itr.txt's header
//              comment). A hit here proves the scancode-indexed dispatch
//              at 0x41d685 actually landed on the confirm handler, not
//              just that SOME key was read back.
// Neither VA is a function entry (mid-function addresses), which is fine -
// pf::win32::register_breakpoint only needs an instruction boundary, and
// both were read directly off disasm.txt's own byte columns to confirm one.
// -0x424(%ebp) is the loop's own "cursor" local (indexes both
// itr_file_list, VA 0x500938, stride 0x18=24 bytes, and the per-entry
// directory-flag byte array at 0x50093c, same stride - both read directly
// off ctx->Ebp, no CONTEXT trickery needed since these breakpoints fire
// INSIDE _replay_selector's own frame). num_itr_files (VA 0x4dd744) is a
// plain global, read the same way --print-globals would.
static void trace_replay_selector_readkey_hit(CONTEXT* ctx) {
    int cursor = *(int*)(uintptr_t)(ctx->Ebp - 0x424);
    int num_itr = *(volatile int*)(uintptr_t)0x4dd744u;
    fprintf(stderr, "det: TRACE _replay_selector: keypressed()==true, about to call readkey() "
                    "(VA=0x41d671, T=%d, cursor=%d, num_itr_files=%d)\n",
            det_current_tick(), cursor, num_itr);
}
static void trace_replay_selector_confirm_hit(CONTEXT* ctx) {
    int cursor = *(int*)(uintptr_t)(ctx->Ebp - 0x424);
    int num_itr = *(volatile int*)(uintptr_t)0x4dd744u;
    unsigned char is_dir = *(unsigned char*)(uintptr_t)(0x50093cu + (uint32_t)cursor * 24u);
    // itr_file_list (VA 0x500938, same stride/index as the directory-flag
    // array 4 bytes later at 0x50093c) stores a heap string pointer in each
    // record's first dword - used once, this pass, to identify BY NAME
    // which .itr file cursor=1 actually selects (carrier/scripts/
    // play_itr.txt's own header comment / NOTES.md "corpus gates" section
    // name the result: profiles/MissingNO/replays/last_game.itr). Read
    // directly rather than left as a standing per-run print, to keep this
    // diagnostic's steady-state output the same shape it always was.
    const char* name_ptr = *(const char* const*)(uintptr_t)(0x500938u + (uint32_t)cursor * 24u);
    fprintf(stderr, "det: TRACE _replay_selector: CONFIRM HANDLER REACHED (VA=0x41d9dd, T=%d, "
                    "cursor=%d, num_itr_files=%d, is_dir_byte=%d, name=\"%s\")\n",
            det_current_tick(), cursor, num_itr, (int)is_dir, name_ptr ? name_ptr : "(null)");
}

void det_arm_thread(HANDLE thread) { pf::win32::arm_thread(thread); }

// ---------------------------------------------------------------------
// Divergence 010: which of the tick safepoint's two entry addresses this
// run can actually reach.
//
// icytower::kTickSafepoint names a CALLEE (update_player) that the caller's
// (play's) tick loop runs exactly once per consumed tick. That callee has
// two possible entry points and exactly one of them executes per run:
//
//   caller ORIGINAL  -> the guest VA. Correct even when the CALLEE itself
//                       is bound: the call still lands on the callee's
//                       original address, which then holds bind.cpp's
//                       5-byte `jmp rel32`, and an exec breakpoint fires on
//                       the address, not on the bytes at it.
//   caller BOUND     -> the carrier's own linked symbol for the callee's
//                       src form. carrier/gen/pf_bindings_src.h leaves a
//                       promoted name undefined on purpose, so src play.c's
//                       `update_player(...)` is a direct call to carrier
//                       code and the guest VA is never executed.
//
// Resolved here rather than in det_init because det_init runs BEFORE
// bind_init (main.cpp: the guest image is not even mapped yet), so "is the
// caller bound this run" is not yet knowable there. ctx_arm_slot is the
// framework's public "point slot N at VA" primitive; it also writes the
// debug registers of the CONTEXT handed to it, which is why a throwaway
// one is used - the real arming is arm_thread_by_id, which reads the
// slot table this call updated.
static void resolve_tick_safepoint() {
    if (g_safepoint_slot < 0) return;   // no digest/stop-at-tick this run
    const icytower::TickSafepointPolicy& P = icytower::kTickSafepoint;
    DWORD_PTR va = P.callee_va;
    const char* why = "caller is ORIGINAL: the callee's guest entry";
    if (bind_is_bound(P.caller_name)) {
        void* sym = bind_src_symbol(P.callee_name);
        if (!sym) {
            fprintf(stderr,
                    "det: FATAL - the tick safepoint needs %s's src form (because '%s' is bound to "
                    "%s, so its calls never reach the guest entry 0x%08lx), but the binding table "
                    "has no src symbol for it.\n",
                    P.callee_name, P.caller_name, bind_form_name(P.caller_name), P.callee_va);
            fflush(stderr);
            TerminateProcess(GetCurrentProcess(), 6);
        }
        va = (DWORD_PTR)sym;
        why = "caller is bound: the carrier's own src symbol for the callee";
    }
    CONTEXT throwaway;
    ZeroMemory(&throwaway, sizeof(throwaway));
    pf::win32::ctx_arm_slot(&throwaway, g_safepoint_slot, va);
    fprintf(stderr,
            "det: tick safepoint = %s() entry at 0x%08lx (DR%d) - %s; caller '%s' is %s\n",
            P.callee_name, (unsigned long)va, g_safepoint_slot, why,
            P.caller_name, bind_form_name(P.caller_name));
}

void det_arm_main_thread() {
    resolve_tick_safepoint();
    pf::win32::arm_thread_by_id(g_main_tid);
}

// The framework's handler owns the DR6 decode, the RF resume flag and the
// fall-through to the fatal-crash dump. The bounded-instruction tracer is
// a SECOND kind of single step (trap flag, Dr6 bit 14) and is registered
// with it as a pair of hooks from det_init, so snapshot.cpp keeps its
// window without this file's handler having to know about it.
LONG WINAPI det_veh_handler(EXCEPTION_POINTERS* ep) { return pf::win32::veh_handler(ep); }

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
    pf::win32::virtual_clock_init(icytower::kTick, g_det_mode);
    pf::win32::threads_init(icytower::kThreads);
    // BEFORE load_script, below: the script parser resolves KEY_* names
    // through this policy's table, and an empty table makes every name
    // atoi() to 0. Measured the hard way - the run then delivered
    // scancode 0 for every event, never reached play(), and produced an
    // EMPTY digest rather than a wrong one.
    pf::win32::input_channel_init(icytower::kInputBinding);
    pf::win32::focus_channel_init(icytower::kFocusChannel);
    // The bounded-instruction tracer is the second kind of single step the
    // shared VEH has to claim (trap flag, Dr6 bit 14 - not one of the four
    // hardware slots); snapshot.cpp owns it, so it is registered as a pair
    // of hooks rather than known to breakpoints.hpp by name.
    pf::win32::set_trace_hooks(snapshot_trace_active, snapshot_trace_step);
    pf::win32::rng_init(icytower::kRng);
    if (g_det_mode) pf::win32::arena_init(icytower::kArena);

    bool need_safepoint = opt.stop_at_tick > 0 || opt.force_safepoint;
    if (opt.digest_out && opt.digest_out[0]) {
        g_digest_file = fopen(opt.digest_out, "w");
        if (!g_digest_file) fprintf(stderr, "det: could not open --digest-out '%s'\n", opt.digest_out);
        need_safepoint = true;
    }
    if (opt.dump_assets && opt.dump_assets[0]) {
        g_dump_assets_path = opt.dump_assets;
        need_safepoint = true;
    }
    // The slot is claimed HERE (registration order == DR slot order, and
    // the tick safepoint has always been DR0) but its ADDRESS is only
    // resolved in det_arm_main_thread(), which main.cpp calls after
    // bind_init() - see resolve_tick_safepoint().
    if (need_safepoint)
        g_safepoint_slot = register_breakpoint(icytower::kTickSafepoint.callee_va, safepoint_hit);

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
        register_breakpoint(icytower::kInputBinding.capture_va, neutralize_keyboard_hit);
        fprintf(stderr, "det: real keyboard PARKED (input=%s; key_dinput_handle_scancode short-circuited)\n",
                input_policy_name(g_input_policy));
    } else if (!g_inject_real_test) {
        register_breakpoint(icytower::kInputBinding.capture_va, real_key_capture_hit);
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
        register_breakpoint(icytower::kInputBinding.deliver_press_va, keypress_record_hit);
        register_breakpoint(icytower::kInputBinding.deliver_release_va, keyrelease_record_hit);
    }
    if (opt.input_script && opt.input_script[0]) load_script(opt.input_script);

    // DET_TRACE_REPLAY_SELECTOR=1 (temporary diagnostic, see the two
    // callbacks' own comment above): 2 extra breakpoints, opt-in only -
    // every OTHER run in this project registers at most 2 of the 4 DR
    // slots (safepoint + one input hook), leaving headroom for exactly
    // these two without touching the normal option surface.
    {
        char buf[8];
        if (GetEnvironmentVariableA("DET_TRACE_REPLAY_SELECTOR", buf, sizeof(buf)) && buf[0] == '1') {
            register_breakpoint(0x0041d671u, trace_replay_selector_readkey_hit);
            register_breakpoint(0x0041d9ddu, trace_replay_selector_confirm_hit);
            fprintf(stderr, "det: DET_TRACE_REPLAY_SELECTOR=1 - armed 2 diagnostic breakpoints "
                            "inside _replay_selector (0x41d671, 0x41d9dd)\n");
        }
    }

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
    if (!isolate_off("switch")) {
        patch_entry_jmp(icytower::kFocusChannel.switch_in_va, (void*)det_stub_switch_in, "_switch_in (dispsw.c)");
        patch_entry_jmp(icytower::kFocusChannel.switch_out_va, (void*)det_stub_switch_out, "_switch_out (dispsw.c)");
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
    // Divergence 009: the host's own DirectSound render-device list, verbatim.
    // It is deliberately reported even though the guest never sees it - it is
    // the one number that makes a future arena-shift drift attributable in
    // one diff instead of a day of bisection.
    char dsnames[768]; dsnames[0] = 0;
    {
        size_t dused = 0;
        for (int i = 0; i < g_ds_host_count; ++i) {
            char esc[64]; size_t e = 0;
            for (const char* p = g_ds_host[i].name; *p && e < sizeof(esc) - 2; ++p) {
                unsigned char c = (unsigned char)*p;
                if (c == '"' || c == '\\' || c < 0x20 || c > 0x7e) esc[e++] = '?';
                else esc[e++] = (char)c;
            }
            esc[e] = 0;
            int w = _snprintf(dsnames + dused, sizeof(dsnames) - dused, "%s\"%s\"", i ? "," : "", esc);
            if (w < 0 || dused + (size_t)w >= sizeof(dsnames) - 1) break;
            dused += (size_t)w;
        }
        dsnames[sizeof(dsnames) - 1] = 0;
    }
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
              "\"dsound_normalized\":%s,\"dsound_host_devices\":%d,\"dsound_delivered\":%d,"
              "\"dsound_host_names\":[%s],"
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
              pf::win32::detail::perturb_clock_ms(), pf::win32::clock_time_offset_s(),
              g_ds_normalized ? "true" : "false", g_ds_host_count, g_ds_delivered, dsnames,
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
    s->virtual_ms = pf::win32::virtual_ms();
    s->units_reported = pf::win32::units_reported();
    s->rng_state = pf::win32::rng_state();
    s->rng_calls = pf::win32::rng_calls();
    s->script_cursor = (unsigned)pf::win32::script_cursor();
    // "How many bytes of the arena are live" - now read out of the arena's
    // OWN control block (divergence 004 rewrite): the allocator's whole state
    // (top, free list, block headers/footers) lives inside [0, top), so the
    // existing "arena" snapshot component captures the allocator unchanged
    // and this field keeps its exact old meaning and use (snapshot.cpp).
    s->arena_offset = det_arena_top();
    s->real_key_violations = g_real_key_violations;
    s->last_drain_tick = g_last_drain_tick;
    memcpy(s->key_held, g_key_held, sizeof(g_key_held));
    pf::win32::real_queue_save(&s->real_queue_head, &s->real_queue_tail,
                               s->real_queue_code, s->real_queue_press);
    // "Environment isolation": the two new carrier-owned cursors/queues, for
    // exactly the reason the script cursor and the key queue are already here
    // (win32_pilot.md sec 6) - a rewind that moved the game back but left the
    // recorded-clock cursor or a pending switch event running forward would
    // not line up.
    s->time_cursor = (int)pf::win32::time_cursor();
    pf::win32::focus_queue_save(&s->switch_queue_head, &s->switch_queue_tail,
                                s->switch_queue_dir);
}

void det_state_load(const DetSavedState* s) {
    pf::win32::set_virtual_ms(s->virtual_ms);
    pf::win32::set_units_reported(s->units_reported);
    pf::win32::rng_set_state(s->rng_state);
    pf::win32::rng_set_calls((long)s->rng_calls);
    pf::win32::set_script_cursor((size_t)s->script_cursor);
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
    pf::win32::real_queue_load(s->real_queue_head, s->real_queue_tail,
                               s->real_queue_code, s->real_queue_press);
    pf::win32::set_time_cursor((size_t)s->time_cursor);
    pf::win32::focus_queue_load(s->switch_queue_head, s->switch_queue_tail,
                                s->switch_queue_dir);
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
            det_current_tick(), pf::win32::virtual_ms(), g_sleep_calls, g_safepoint_count,
            (int)IT_CYCLE_COUNT);
    // Audit evidence for the activation channel: WHO is registered in the
    // guest's own switch callback tables, and WHOSE window procedure the
    // guest window actually has (cnc-ddraw subclasses it - see
    // notes/determinism_audit.md). Printed unconditionally at shutdown so
    // every archived stderr carries it.
    {
        void** in_cb = (void**)(uintptr_t)icytower::kFocusChannel.cb_table_in_va;
        void** out_cb = (void**)(uintptr_t)icytower::kFocusChannel.cb_table_out_va;
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
    if (pf::win32::arena_active()) {
        pf::win32::ArenaStats a = pf::win32::arena_stats();
        fprintf(stderr,
                "det: arena high-water %u bytes (%.2f MB), live %u bytes in %u blocks, "
                "peak live %u bytes; calls malloc=%u calloc=%u realloc=%u free=%u "
                "(foreign %u, bad %u)\n",
                a.hwm, a.hwm / (1024.0 * 1024.0), a.live_bytes, a.live_blocks,
                a.peak_live_bytes, a.n_malloc, a.n_calloc, a.n_realloc, a.n_free,
                a.n_free_foreign, a.n_free_bad);
    }
}
