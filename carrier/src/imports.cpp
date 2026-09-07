// imports.cpp - see imports.hpp. Owns g_real[] and the generated import
// table/stub-array definitions (the only TU that #includes those .inc
// files, so they're defined exactly once).
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "imports.hpp"
#include "import_types.hpp"
#include "wrappers.hpp"

#include "../gen/import_table.inc"
#include "../gen/import_names.inc"

// The one definition of g_real (import_types.hpp only declares it extern).
extern "C" void* g_real[PF_MAX_IMPORTS] = {};

// Imports needing carrier-side behavior changes (see wrappers.cpp for the
// evidence per entry). Matched by import name alone - every name below is
// unambiguous across the whole import table.
static const char* kWrapNames[] = {
    "ExitProcess", "exit", "_cexit", "abort",
    "GetModuleFileNameA", "GetCommandLineA",
    // Milestones 5-7 (det.cpp): always wrapped so --det/--digest-out/
    // --record-input/--input-script work; each wrapper forwards to the real
    // function when not asked to behave differently (see wrappers.cpp).
    "Sleep", "QueryPerformanceCounter", "timeGetTime", "time", "clock", "_beginthread",
    "malloc", "calloc", "realloc", "free",
    // Item 3 ("parked timer thread" pass): needed so a parked timer
    // thread's own WaitForSingleObject(stop_event, <finite>) call can be
    // substituted to INFINITE (carrier/NOTES.md) - forwards to the real
    // function unchanged for every other thread/caller.
    "WaitForSingleObject",
    // Milestone 8 (win32_pilot.md sec 5 "pin the LCG"): the RNG state must
    // be carrier-owned to be snapshottable - msvcrt.dll's own per-thread
    // seed is outside every snapshot component. Forwards to the real
    // msvcrt rand/srand unless --det (see det.cpp).
    "rand", "srand",
    // "Environment isolation" pass (carrier/NOTES.md): the guest's own window
    // -management calls (an automated run must never take the operator's
    // foreground), the ad-fetch thread (the one pthread_create call site,
    // whose HTTP result reaches five globals inside the digest domain), and
    // getenv (instrumentation only - the value is always forwarded). Each
    // forwards unchanged unless --det / a non-interactive run asks otherwise.
    "ShowWindow", "SetForegroundWindow", "SetWindowPos", "CreateWindowExA",
    "pthread_create", "getenv",
};
static bool is_wrapped(const char* name) {
    for (const char* w : kWrapNames)
        if (strcmp(w, name) == 0) return true;
    return false;
}

// libpng3.dll and pthreadGC2.dll ship in assets\ next to the game exe (see
// notes/binary_recon.md facts) rather than anywhere on the system DLL
// search path, so they need an explicit path. zlib1.dll also lives there
// but is never a *direct* import of icytower15.exe (only a dependency of
// libpng3.dll) - SetDllDirectoryA below covers it.
static bool needs_assets_path(const char* dll) {
    return _stricmp(dll, "libpng3.dll") == 0 || _stricmp(dll, "pthreadGC2.dll") == 0;
}

struct LoadedDll { char name[64]; HMODULE h; };
static LoadedDll g_loaded[32];
static int g_loaded_count = 0;

static HMODULE load_dll(const char* dll, const ImportsConfig& cfg) {
    for (int i = 0; i < g_loaded_count; ++i)
        if (_stricmp(g_loaded[i].name, dll) == 0) return g_loaded[i].h;

    HMODULE h = nullptr;
    char path[MAX_PATH];
    if (_stricmp(dll, "DDRAW.dll") == 0 && cfg.ddraw_mode == DDrawMode::Local) {
        // assets\ddraw.dll is cnc-ddraw's DirectDraw-compatibility shim, the
        // one the user installed to run the original on Windows 11.
        _snprintf(path, sizeof(path), "%s\\ddraw.dll", cfg.assets_dir);
        path[sizeof(path) - 1] = 0;
        h = LoadLibraryExA(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    } else if (needs_assets_path(dll)) {
        _snprintf(path, sizeof(path), "%s\\%s", cfg.assets_dir, dll);
        path[sizeof(path) - 1] = 0;
        h = LoadLibraryExA(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    } else {
        h = LoadLibraryA(dll); // system DLL, or DDRAW.dll under --ddraw=system
    }
    if (!h) {
        fprintf(stderr, "imports_init: LoadLibrary('%s') FAILED, gle=%lu\n", dll, GetLastError());
    } else {
        fprintf(stderr, "imports_init: loaded %s%s\n", dll,
                (needs_assets_path(dll) || (h && _stricmp(dll, "DDRAW.dll") == 0 && cfg.ddraw_mode == DDrawMode::Local))
                    ? " (assets\\)" : "");
    }
    if (g_loaded_count < 32) {
        strncpy(g_loaded[g_loaded_count].name, dll, sizeof(g_loaded[0].name) - 1);
        g_loaded[g_loaded_count].name[sizeof(g_loaded[0].name) - 1] = 0;
        g_loaded[g_loaded_count].h = h;
        g_loaded_count++;
    }
    return h;
}

bool imports_init(const ImportsConfig& cfg) {
    if (kNumImports > PF_MAX_IMPORTS) {
        fprintf(stderr, "imports_init: kNumImports(%d) exceeds PF_MAX_IMPORTS(%d)\n",
                kNumImports, PF_MAX_IMPORTS);
        return false;
    }
    SetDllDirectoryA(cfg.assets_dir); // safety net for transitive deps (zlib1.dll)

    int failures = 0;
    for (int i = 0; i < kNumImports; ++i) {
        const ImportEntry& e = g_import_table[i];
        HMODULE h = load_dll(e.dll, cfg);
        void* proc = h ? (void*)GetProcAddress(h, e.name) : nullptr;
        if (!proc) {
            fprintf(stderr, "imports_init: GetProcAddress('%s','%s') FAILED\n", e.dll, e.name);
            failures++;
        }
        g_real[i] = proc;

        void* iat_value;
        if (is_wrapped(e.name)) {
            wrappers_bind_real(e.name, proc, i);
            void* wrapper = wrappers_lookup(e.name);
            iat_value = wrapper ? wrapper : proc;
        } else if (cfg.count_imports) {
            iat_value = g_import_stubs[i];
        } else {
            iat_value = proc;
        }
        *(void**)(uintptr_t)e.iat_va = iat_value;
    }
    fprintf(stderr, "imports_init: resolved %d/%d imports (%d failures)\n",
            kNumImports - failures, kNumImports, failures);
    return failures == 0;
}
