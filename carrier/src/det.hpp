// det.hpp - milestones 5-7: virtual time, input injection, and the tick
// sensor (hardware-breakpoint digest recorder). See carrier/NOTES.md,
// section "Milestones 5-7", for the evidence behind every address and
// design choice here. Everything is inert unless one of the DetOptions
// fields below asks for it - default (non-det, no scripts, no digest,
// no recording) carrier behavior is unchanged.
#pragma once
#include <windows.h>
#include <cstdint>

struct DetOptions {
    bool det_mode;            // --det
    bool pace_real;           // --pace=real (default fast: never really Sleep in det mode)
    const char* input_script; // --input-script PATH, or nullptr
    const char* record_input; // --record-input PATH, or nullptr
    const char* digest_out;   // --digest-out PATH, or nullptr
    int stop_at_tick;         // --stop-at-tick T, <=0 = unbounded
};

typedef void (*DetShutdownFn)(const char* reason);

// Parses/opens everything DetOptions asks for (input script, digest/record
// files) and captures the calling thread as "the guest main thread" - call
// this once, early in main(), from the same thread that will later jump
// into the guest entry point. `shutdown_hook` is the same carrier_shutdown
// used by wrap_ExitProcess/etc (see wrappers.hpp) - det_shutdown() and
// --stop-at-tick both call it before terminating.
void det_init(const DetOptions& opt, DetShutdownFn shutdown_hook);

// Arms the tick-safepoint / key-event hardware breakpoints (whichever are
// needed per det_init's options) on the guest main thread. Must run from a
// SEPARATE thread (Windows does not support a thread setting its own debug
// registers) - this spawns and joins that helper thread itself. Call once,
// after imports_init(), before jumping into the guest entry point.
void det_arm_main_thread();

// Arms the same breakpoint table directly on an arbitrary (already-running)
// thread - called from a helper thread with `thread` suspended first. Used
// by det_arm_main_thread() and, for --record-input, by det_wrap_beginthread
// to also arm the real DirectInput input thread (key events are logged from
// there, not the main thread - see det.cpp).
void det_arm_thread(HANDLE thread);

// The vectored exception handler for our hardware breakpoints. Install with
// AddVectoredExceptionHandler(1, det_veh_handler) AFTER main.cpp's own
// veh_handler is registered, so this one runs first (last-registered-first
// order) and returns EXCEPTION_CONTINUE_SEARCH for anything that isn't one
// of ours, falling through to the fatal-crash handler unchanged.
LONG WINAPI det_veh_handler(EXCEPTION_POINTERS* ep);

// Flushes/closes the digest and record files. Safe to call more than once.
// carrier_shutdown() (main.cpp) calls this alongside trace_close().
void det_shutdown();

// imports_init() (via wrappers_bind_real) calls this for every import det.cpp
// wraps, so the wrapper can forward to the real function when not in --det
// mode (or, for _beginthread, when the start address isn't one of the two
// virtualized Allegro threads).
void det_bind_real(const char* name, void* real_proc);

// Wrapped imports - see wrappers.cpp's wrappers_lookup() for how these are
// installed, and det.cpp for the evidence/semantics comment on each one.
extern "C" {
    uintptr_t __cdecl det_wrap_beginthread(void(__cdecl* start_address)(void*),
                                            unsigned stack_size, void* arglist);
    void __stdcall det_wrap_Sleep(DWORD ms);
    BOOL __stdcall det_wrap_QueryPerformanceCounter(LARGE_INTEGER* out);
    DWORD __stdcall det_wrap_timeGetTime();
    long __cdecl det_wrap_time(long* out);
    long __cdecl det_wrap_clock();
    // Deterministic heap arena (det mode only - see det.cpp), needed because
    // the real msvcrt heap's base address is randomized per-process by
    // Windows and pointers into it get baked directly into .data/.bss
    // globals (e.g. Allegro's `screen` BITMAP*), which the digest sensor
    // (part C) would otherwise see as spurious per-run differences.
    void* __cdecl det_wrap_malloc(size_t n);
    void* __cdecl det_wrap_calloc(size_t count, size_t size);
    void* __cdecl det_wrap_realloc(void* p, size_t n);
    void __cdecl det_wrap_free(void* p);
}
