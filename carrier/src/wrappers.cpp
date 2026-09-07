// wrappers.cpp - see wrappers.hpp. Each wrapper below carries a comment
// citing the evidence it's needed and whether it's a permanent (KNOWN)
// requirement or a TEMPORARY carrier-only workaround. Do not add wrappers
// speculatively - only once a run has proven the need (record the proof
// here when you do).
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "wrappers.hpp"
#include "det.hpp"
#include "../../port_forge/src/platform/win32/trace.hpp" // pf_count_import - see wrappers.hpp/det.hpp (item 3)

// The guest identity the framework's wrappers answer with. `command_line`
// IS the image path: the reserved child's real OS command line is already
// that quoted path (bootstrap.hpp reason 2), and the guest's own argv[0]
// logic expects exactly it.
static char g_guest_path[MAX_PATH] = "icytower15.exe";
static pf::win32::GuestIdentityPolicy g_identity = {
    g_guest_path, g_guest_path, /* treat_carrier_hmodule_as_main */ true
};

void wrappers_set_guest_identity(const char* image_path, void* carrier_hmodule,
                                 ShutdownFn on_shutdown) {
    strncpy(g_guest_path, image_path, sizeof(g_guest_path) - 1);
    g_guest_path[sizeof(g_guest_path) - 1] = 0;
    pf::win32::guest_identity_init(g_identity, carrier_hmodule, on_shutdown);
}

void wrappers_bind_real(const char* name, void* real_proc, int id) {
    pf::win32::guest_identity_bind_real(name, real_proc, id);
    det_bind_real(name, real_proc, id); // no-op unless name is one of det.cpp's wrapped imports
}

// The wrapper table. Order is documentation, not semantics: the exit path,
// then guest identity, then the deterministic-execution family that
// det.cpp owns, then the environment-isolation family. Each entry's
// evidence is at its own wrapper (this file, or det.cpp).
static const pf::win32::WrapEntry kWrapTable[] = {
    // KNOWN (notes/binary_recon.md item a): the MinGW entry point never
    // returns - it always ends in ExitProcess, and the CRT exit chain also
    // runs through msvcrt exit/_cexit/abort. Without these the carrier
    // process just vanishes and the report is never flushed.
    { "ExitProcess",          (void*)pf_win32_wrap_ExitProcess },
    { "exit",                 (void*)pf_win32_wrap_exit },
    { "_cexit",               (void*)pf_win32_wrap_cexit },
    { "abort",                (void*)pf_win32_wrap_abort },
    // Guest identity (notes/binary_recon.md item a).
    { "GetModuleFileNameA",   (void*)pf_win32_wrap_GetModuleFileNameA },
    { "GetCommandLineA",      (void*)pf_win32_wrap_GetCommandLineA },
    // Milestones 5-7 (det.cpp) - always installed so --det/--digest-out/
    // --record-input/--input-script work; each forwards to the real
    // function when not asked to behave differently.
    { "Sleep",                (void*)det_wrap_Sleep },
    { "QueryPerformanceCounter", (void*)det_wrap_QueryPerformanceCounter },
    { "timeGetTime",          (void*)det_wrap_timeGetTime },
    { "time",                 (void*)det_wrap_time },
    { "clock",                (void*)det_wrap_clock },
    { "_beginthread",         (void*)det_wrap_beginthread },
    { "malloc",               (void*)det_wrap_malloc },
    { "calloc",               (void*)det_wrap_calloc },
    { "realloc",              (void*)det_wrap_realloc },
    { "free",                 (void*)det_wrap_free },
    // Item 3 ("parked timer thread"): a parked thread's own
    // WaitForSingleObject(stop_event, <finite>) is substituted to INFINITE;
    // forwards unchanged for every other thread/caller.
    { "WaitForSingleObject",  (void*)det_wrap_WaitForSingleObject },
    // Milestone 8 (win32_pilot.md sec 5 "pin the LCG"): the RNG state must
    // be carrier-owned to be snapshottable - msvcrt.dll's own per-thread
    // seed is outside every snapshot component.
    { "rand",                 (void*)det_wrap_rand },
    { "srand",                (void*)det_wrap_srand },
    // "Environment isolation" pass (carrier/NOTES.md): the guest's own
    // window-management calls (an automated run must never take the
    // operator's foreground), the ad-fetch thread (whose HTTP result
    // reaches five globals inside the digest domain), and getenv
    // (instrumentation only - the value is always forwarded).
    { "ShowWindow",           (void*)det_wrap_ShowWindow },
    { "SetForegroundWindow",  (void*)det_wrap_SetForegroundWindow },
    { "SetWindowPos",         (void*)det_wrap_SetWindowPos },
    { "CreateWindowExA",      (void*)det_wrap_CreateWindowExA },
    { "pthread_create",       (void*)det_wrap_pthread_create },
    { "getenv",               (void*)det_wrap_getenv },
    // Divergence 009 (carrier/NOTES.md, notes/living_record.md): a host
    // DEVICE ENUMERATION is a determinism channel even when none of its
    // values is stored in a game global - the guest allocates once per
    // enumerated device, from the shared deterministic arena, so every
    // later arena pointer in the digest domain is displaced when the
    // host's device list changes.
    { "DirectSoundEnumerateA", (void*)det_wrap_DirectSoundEnumerateA },
};

pf::win32::WrapPolicy wrappers_policy() {
    pf::win32::WrapPolicy p;
    p.entries = kWrapTable;
    p.count = (int)(sizeof(kWrapTable) / sizeof(kWrapTable[0]));
    return p;
}
