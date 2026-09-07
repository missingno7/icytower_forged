// trace.cpp - counting/tracing trampoline (pf_import_common, pf_on_import)
// and the JSON report writer. See trace.hpp.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "trace.hpp"
#include "import_types.hpp"
#include "../../port_forge/src/platform/win32/symbols.hpp"
#include "det.hpp" // report.json: input_policy / real_key_violations (see det_input_policy_name/det_real_key_violations)
#include "bind.hpp" // report.json: the milestone 11-12 migration map (bind_report_json)
#include "snapshot.hpp" // report.json: milestone 8-9 snapshot sizes/timings
#include "headless.hpp" // report.json: "headless" object (set_gfx_mode/install_sound sensors)
#include "frame.hpp" // report.json: "frame_oracle" object
#include "print_globals.hpp" // report.json: "print_globals" array

#define PF_MAX_THREADS 32

static volatile LONG g_call_count[PF_MAX_IMPORTS];
static bool g_trace_enabled[PF_MAX_IMPORTS];
static bool g_trace_all = false;

static CRITICAL_SECTION g_cs;
static bool g_cs_ready = false;
static FILE* g_trace_file = nullptr;
static volatile LONG g_seq = 0;

// Per-thread attribution: a small fixed table, first-touch registers a slot.
// Kept under g_cs since threads are created rarely (5 background threads
// max per notes/binary_recon.md item b) - contention is not a concern.
static DWORD g_thread_ids[PF_MAX_THREADS];
static LONG g_thread_count[PF_MAX_THREADS][PF_MAX_IMPORTS];
static int g_thread_used = 0;

static int thread_slot_for(DWORD tid) {
    for (int i = 0; i < g_thread_used; ++i) {
        if (g_thread_ids[i] == tid) return i;
    }
    if (g_thread_used < PF_MAX_THREADS) {
        g_thread_ids[g_thread_used] = tid;
        return g_thread_used++;
    }
    return -1; // overflow: still counted globally, just not attributed
}

void trace_init(const char* out_path) {
    InitializeCriticalSection(&g_cs);
    g_cs_ready = true;
    if (out_path && out_path[0]) {
        g_trace_file = fopen(out_path, "w");
        if (!g_trace_file) {
            fprintf(stderr, "trace_init: could not open '%s' for writing\n", out_path);
        }
    }
}

void trace_enable_id(int id) {
    if (id >= 0 && id < PF_MAX_IMPORTS) g_trace_enabled[id] = true;
}

void trace_enable_all() { g_trace_all = true; }

void trace_close() {
    if (g_trace_file) {
        fflush(g_trace_file);
        fclose(g_trace_file);
        g_trace_file = nullptr;
    }
}

// Single-place counting logic (see trace.hpp): both pf_on_import (trampoline-
// routed imports) and pf_count_import (always-installed wrappers that are
// wired directly into the IAT, bypassing the trampoline - wrappers.cpp/
// det.cpp) funnel through here so g_call_count[]/g_thread_count[][] - and
// hence the report.json numbers - are accurate for every import either way.
extern "C" void __cdecl pf_count_import(int id) {
    if (id < 0 || id >= PF_MAX_IMPORTS) return; // corrupt call, don't crash the logger
    InterlockedIncrement(&g_call_count[id]);
    if (!g_cs_ready) return;
    DWORD tid = GetCurrentThreadId();
    EnterCriticalSection(&g_cs);
    int slot = thread_slot_for(tid);
    if (slot >= 0) g_thread_count[slot][id]++;
    LeaveCriticalSection(&g_cs);
}

extern "C" void __cdecl pf_on_import(int id, void* frame) {
    if (id < 0 || id >= PF_MAX_IMPORTS) return; // corrupt call, don't crash the logger
    pf_count_import(id);

    DWORD tid = GetCurrentThreadId();

    bool want_trace = g_trace_all || g_trace_enabled[id];
    // pf_count_import already did the increment/thread-attribution above;
    // this second critical section only guards trace-line emission (needs
    // the frame's return address/args, which pf_count_import never has).
    if (g_cs_ready) {
        EnterCriticalSection(&g_cs);

        if (want_trace && g_trace_file) {
            unsigned long seq = (unsigned long)InterlockedIncrement(&g_seq);
            unsigned long* args = (unsigned long*)frame; // [0]=retaddr, [1..]=args
            unsigned long retaddr = args[0];
            char where[192];
            pf::win32::symbols_describe(retaddr, where, sizeof(where));
            const char* name = (id < kNumImports) ? g_import_names[id] : "<bad-id>";
            fprintf(g_trace_file,
                "%lu tid=%lu %s ret=0x%08lx (%s) args=[0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx]\n",
                seq, (unsigned long)tid, name, retaddr, where,
                args[1], args[2], args[3], args[4], args[5], args[6]);
            // DIAGNOSTIC: decode MessageBoxA/W text - Allegro's own fatal-error
            // reporting path (allegro_message -> sys_directx_message) uses this,
            // and it blocks the calling thread forever waiting for a click no
            // automated run can provide, so its text is the single most useful
            // line in the whole trace for finding out why. Safe because the
            // string, if any, lives in this SAME process's own address space.
            if ((strcmp(name, "msvcrt.dll!_chdir") == 0 || strcmp(name, "msvcrt.dll!fopen") == 0) && args[1]) {
                fprintf(g_trace_file, "    %s arg1(path): %.400s\n", name, (const char*)(uintptr_t)args[1]);
            }
            if (strcmp(name, "USER32.dll!MessageBoxA") == 0 && args[2]) {
                fprintf(g_trace_file, "    MessageBoxA text: %.400s\n", (const char*)(uintptr_t)args[2]);
            } else if (strcmp(name, "USER32.dll!MessageBoxW") == 0 && args[2]) {
                const wchar_t* w = (const wchar_t*)(uintptr_t)args[2];
                char buf[401];
                int n = 0;
                while (n < 400 && w[n]) { buf[n] = (w[n] < 128) ? (char)w[n] : '?'; ++n; }
                buf[n] = 0;
                fprintf(g_trace_file, "    MessageBoxW text: %s\n", buf);
            }
        }
        LeaveCriticalSection(&g_cs);
    }
}

void trace_write_report(const char* report_path) {
    if (!report_path || !report_path[0]) return;
    FILE* f = fopen(report_path, "w");
    if (!f) {
        fprintf(stderr, "trace_write_report: could not open '%s'\n", report_path);
        return;
    }
    fprintf(f, "{\n  \"input_policy\": \"%s\",\n  \"real_key_violations\": %ld,\n",
            det_input_policy_name(), det_real_key_violations());
    // Divergence 004: the deterministic heap arena is now a real allocator,
    // so its high-water mark is a measurement worth reporting (it is also the
    // size of the snapshot's "arena" component). All zero outside --det.
    {
        unsigned top = 0, hwm = 0, live = 0, peak = 0, blocks = 0;
        det_arena_stats(&top, &hwm, &live, &peak, &blocks);
        fprintf(f, "  \"arena\": { \"top\": %u, \"high_water\": %u, \"live_bytes\": %u, "
                   "\"peak_live_bytes\": %u, \"live_blocks\": %u },\n",
                top, hwm, live, peak, blocks);
    }
    // Divergence 005 diagnosis aid: the pinned msvcrt LCG's state and call
    // count. A record run and its replay must agree on both; a disagreement
    // says the two runs took different code paths, not that the input
    // coordinate was wrong.
    fprintf(f, "  \"rng\": { \"state\": %u, \"calls\": %ld },\n",
            det_rng_state(), det_rng_calls());
    // "Environment isolation" pass (carrier/NOTES.md): one object per run
    // saying what happened to every host channel the carrier took ownership
    // of - window policy, activation, mouse, ad thread, recorded clock,
    // getenv. This is the machine-readable half of notes/determinism_audit.md.
    {
        char envbuf[4096]; // divergence 009 added the host's DirectSound device list
        det_environment_json(envbuf, sizeof(envbuf));
        fprintf(f, "  \"environment\": %s,\n", envbuf);
    }
    // Milestones 11-12 migration map (win32_pilot.md SS8a). Emits nothing at
    // all when no function was bound or sensed, so the report shape of every
    // pre-milestone-11 run is unchanged.
    bind_report_json(f);
    snapshot_report_json(f);
    headless_report_json(f);
    frame_report_json(f);
    print_globals_report_json(f);
    fprintf(f, "  \"imports\": [\n");
    bool first = true;
    for (int id = 0; id < kNumImports && id < PF_MAX_IMPORTS; ++id) {
        long c = g_call_count[id];
        if (c == 0) continue;
        if (!first) fprintf(f, ",\n");
        first = false;
        fprintf(f, "    { \"id\": %d, \"name\": \"%s\", \"count\": %ld }",
                id, g_import_names[id], c);
    }
    fprintf(f, "\n  ],\n  \"threads\": [\n");
    for (int t = 0; t < g_thread_used; ++t) {
        fprintf(f, "%s    { \"tid\": %lu, \"calls\": [\n", t == 0 ? "" : ",\n",
                (unsigned long)g_thread_ids[t]);
        bool tfirst = true;
        for (int id = 0; id < kNumImports && id < PF_MAX_IMPORTS; ++id) {
            long c = g_thread_count[t][id];
            if (c == 0) continue;
            if (!tfirst) fprintf(f, ",\n");
            tfirst = false;
            fprintf(f, "      { \"id\": %d, \"name\": \"%s\", \"count\": %ld }",
                    id, g_import_names[id], c);
        }
        fprintf(f, "\n    ] }");
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);
}

// pf_import_common: on entry, esp -> [id][retaddr][args...] (pushed by the
// caller's per-import pf_stub_N). Must preserve every register except
// eax/ecx/edx (the only ones cdecl/stdcall callers treat as volatile),
// must not disturb the [retaddr][args...] area, and must tail-jump to
// g_real[id] with the stack exactly as the original caller left it so
// callees of either convention work without us knowing their prototype.
//
// pushad saves all 8 GPRs (32 bytes); the frame that was at esp+0 before
// pushad is now at esp+32 (id) / esp+36 (retaddr, i.e. the "frame" pointer
// pf_on_import receives). We call pf_on_import(id, frame) cdecl (push
// frame, then id, per right-to-left argument order), clean up with
// add esp,8, popad to restore all 8 registers to the caller's original
// values, then re-read id (still sitting untouched at the original id
// slot), drop it, and jmp through g_real[id].
extern "C" void __declspec(naked) pf_import_common() {
    __asm {
        pushad
        lea eax, [esp+32+4]     // eax = &retaddr = frame pointer for pf_on_import
        push eax
        mov ecx, [esp+36]       // ecx = id (still at its original stack slot)
        push ecx
        call pf_on_import
        add esp, 8
        popad
        mov ecx, [esp]          // ecx = id again (popad restored esp to id's slot)
        add esp, 4              // drop id; esp now == what the real callee expects
        jmp dword ptr [g_real + ecx*4]
    }
}
