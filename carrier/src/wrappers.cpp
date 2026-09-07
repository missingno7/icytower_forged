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

static char g_guest_path[MAX_PATH] = "icytower15.exe";
static ShutdownFn g_shutdown = nullptr;
static FARPROC g_real_GetModuleFileNameA = nullptr;
static void* g_carrier_hmodule = nullptr;

// import ids (imports.cpp's loop index == report.json's "id"), captured at
// bind time so each wrap_* below can call pf_count_import(id) itself - these
// wrappers are wired directly into the guest IAT (imports.cpp's
// is_wrapped()/wrappers_lookup() path), bypassing pf_import_common's
// counting trampoline entirely, which is exactly the gap item 3 fixes.
static int g_id_ExitProcess = -1, g_id_exit = -1, g_id_cexit = -1, g_id_abort = -1,
           g_id_GetModuleFileNameA = -1, g_id_GetCommandLineA = -1;

void wrappers_set_guest_image_path(const char* path) {
    strncpy(g_guest_path, path, sizeof(g_guest_path) - 1);
    g_guest_path[sizeof(g_guest_path) - 1] = 0;
}

void wrappers_set_carrier_hmodule(void* hmodule) { g_carrier_hmodule = hmodule; }

void wrappers_set_shutdown_hook(ShutdownFn fn) { g_shutdown = fn; }

void wrappers_bind_real(const char* name, void* real_proc, int id) {
    if (strcmp(name, "GetModuleFileNameA") == 0) { g_real_GetModuleFileNameA = (FARPROC)real_proc; g_id_GetModuleFileNameA = id; }
    else if (strcmp(name, "GetCommandLineA") == 0) g_id_GetCommandLineA = id;
    else if (strcmp(name, "ExitProcess") == 0) g_id_ExitProcess = id;
    else if (strcmp(name, "exit") == 0) g_id_exit = id;
    else if (strcmp(name, "_cexit") == 0) g_id_cexit = id;
    else if (strcmp(name, "abort") == 0) g_id_abort = id;
    det_bind_real(name, real_proc, id); // no-op unless name is one of det.cpp's wrapped imports
}

// KNOWN need (notes/binary_recon.md item a: the MinGW entry point
// _WinMainCRTStartup never returns - it always ends in ExitProcess; the CRT
// exit chain also runs through msvcrt exit/_cexit/abort). Without
// intercepting these the carrier process just vanishes and the counting
// report + trace log are never flushed to disk.
extern "C" void __stdcall wrap_ExitProcess(UINT code) {
    pf_count_import(g_id_ExitProcess);
    if (g_shutdown) g_shutdown("guest ExitProcess");
    ::ExitProcess(code);
}
extern "C" void __cdecl wrap_exit(int code) {
    pf_count_import(g_id_exit);
    if (g_shutdown) g_shutdown("guest exit");
    ::ExitProcess((UINT)code);
}
extern "C" void __cdecl wrap__cexit() {
    pf_count_import(g_id_cexit);
    if (g_shutdown) g_shutdown("guest _cexit");
    // Real msvcrt _cexit() runs atexit/static-dtor cleanup and returns; it
    // does not itself terminate the process. But by the point mingw's CRT
    // startup chain reaches _cexit it is already on its way to ExitProcess
    // (notes item a, the __cexit -> ExitProcess tail of the startup chain),
    // so treating it as terminal here guarantees the report is written even
    // if some other path skips the final ExitProcess call. TEMPORARY
    // simplification: a _cexit that's reached mid-run for another reason
    // would also terminate early; no such call site is known to exist.
    ::ExitProcess(0);
}
extern "C" void __cdecl wrap_abort() {
    pf_count_import(g_id_abort);
    if (g_shutdown) g_shutdown("guest abort");
    ::ExitProcess(3); // matches msvcrt abort()'s conventional exit code
}

// KNOWN need (notes/binary_recon.md item a): _mangled_main's startup
// sequence resolves the exe's directory (replace_filename + chdir) and the
// log.txt path from GetModuleFileNameA. The real GetModuleFileNameA under
// the carrier would return carrier.exe's own path, pointing that logic at
// the wrong place.
//
// MEASURED (see carrier/NOTES.md): the game's get_executable_name() does
// NOT call GetModuleFileNameA(NULL,...) directly - it calls
// GetModuleHandleA(NULL) first (left DIRECT, not wrapped, per policy) and
// passes THAT handle to GetModuleFileNameA. Since GetModuleHandleA(NULL)
// legitimately returns carrier's own real module handle (carrier.exe truly
// is the process's main module - the guest was never loaded through the
// real Windows loader), a hModule != NULL check alone forwarded straight
// to the real GetModuleFileNameA and returned carrier.exe's own path,
// which is what actually caused the game to chdir into carrier\ and then
// fail to find data\loading.dat. Treating hModule == carrier's own handle
// the same as hModule == NULL fixes it.
extern "C" DWORD __stdcall wrap_GetModuleFileNameA(HMODULE hModule, LPSTR buf, DWORD size) {
    pf_count_import(g_id_GetModuleFileNameA);
    if (hModule == nullptr || (void*)hModule == g_carrier_hmodule) {
        DWORD len = (DWORD)strlen(g_guest_path);
        DWORD n = (len < size) ? len : (size > 0 ? size - 1 : 0);
        if (size > 0) {
            memcpy(buf, g_guest_path, n);
            buf[n] = 0;
        }
        return n;
    }
    // hModule != NULL means the guest is asking about a module it (or the
    // host loader) actually loaded via LoadLibrary/GetModuleHandle - those
    // calls aren't wrapped, so the handle is a genuine host HMODULE and
    // forwarding to the real function is correct.
    if (g_real_GetModuleFileNameA) {
        typedef DWORD(__stdcall * Fn)(HMODULE, LPSTR, DWORD);
        return ((Fn)g_real_GetModuleFileNameA)(hModule, buf, size);
    }
    return 0;
}

// KNOWN need (notes/binary_recon.md item a): mingw's generated _main()
// calls GetCommandLineA() before WinMain runs. The real one would return
// the carrier's own command line (--trace-imports=... etc), which Allegro's
// _WinMain/__getmainargs would at best ignore and at worst try to parse as
// game arguments.
extern "C" LPSTR __stdcall wrap_GetCommandLineA() {
    pf_count_import(g_id_GetCommandLineA);
    return g_guest_path;
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
    { "ExitProcess",          (void*)wrap_ExitProcess },
    { "exit",                 (void*)wrap_exit },
    { "_cexit",               (void*)wrap__cexit },
    { "abort",                (void*)wrap_abort },
    // Guest identity (notes/binary_recon.md item a).
    { "GetModuleFileNameA",   (void*)wrap_GetModuleFileNameA },
    { "GetCommandLineA",      (void*)wrap_GetCommandLineA },
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
