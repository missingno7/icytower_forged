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

static LONGLONG g_virtual_ms = 0;      // det mode only: accumulated Sleep(ms) on the main thread
static LONGLONG g_units_reported = 0;  // running total already handed to _handle_timer_tick
static ULONGLONG g_start_tick64 = 0;   // non-det mode: real elapsed time baseline (GetTickCount64)

static FILE* g_digest_file = nullptr;
static FILE* g_record_file = nullptr;
static HANDLE g_parked_event = nullptr; // never signaled - park() blocks on it forever

static FARPROC g_real_QPC = nullptr, g_real_timeGetTime = nullptr,
               g_real_time = nullptr, g_real_clock = nullptr, g_real_beginthread = nullptr;
static FARPROC g_real_malloc = nullptr, g_real_calloc = nullptr,
               g_real_realloc = nullptr, g_real_free = nullptr;
static FARPROC g_real_Sleep = nullptr;
static FARPROC g_real_WaitForSingleObject = nullptr; // item 3: parked timer thread
static FARPROC g_real_rand = nullptr, g_real_srand = nullptr; // milestone 8: RNG pinning

// Import ids (see wrappers.hpp/det_bind_real doc), one per always-installed
// wrapper this file defines - each det_wrap_* below calls pf_count_import
// with its own id (item 3: fixes the report.json gap for these, which are
// wired directly into the guest IAT, bypassing the counting trampoline).
static int g_id_Sleep = -1, g_id_QPC = -1, g_id_timeGetTime = -1, g_id_time = -1,
           g_id_clock = -1, g_id_beginthread = -1,
           g_id_malloc = -1, g_id_calloc = -1, g_id_realloc = -1, g_id_free = -1,
           g_id_WaitForSingleObject = -1, g_id_rand = -1, g_id_srand = -1;

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
// ---------------------------------------------------------------------
#define ARENA_BASE  ((uintptr_t)0x20000000u)
#define ARENA_SIZE  (256u * 1024u * 1024u)
#define ARENA_ALIGN 16u
#define ARENA_MAGIC 0x50464152u // 'RAFP'

struct ArenaHeader { uint32_t size; uint32_t magic; };

static uint8_t* g_arena_base = nullptr;
static size_t g_arena_offset = 0;
static CRITICAL_SECTION g_arena_cs;

static void arena_init() {
    g_arena_base = (uint8_t*)VirtualAlloc((void*)ARENA_BASE, ARENA_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_arena_base) {
        fprintf(stderr, "det: arena VirtualAlloc(0x%08x) FAILED gle=%lu - falling back to the real heap "
                         "(digest equality will NOT hold across runs; see carrier/NOTES.md)\n",
                (unsigned)ARENA_BASE, GetLastError());
        return;
    }
    InitializeCriticalSection(&g_arena_cs);
    fprintf(stderr, "det: deterministic heap arena at %p, size=%uMB\n", g_arena_base, ARENA_SIZE / (1024u * 1024u));
}

static void* arena_alloc(size_t n) {
    if (!g_arena_base) return nullptr;
    size_t total = sizeof(ArenaHeader) + n;
    total = (total + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1);
    EnterCriticalSection(&g_arena_cs);
    size_t off = g_arena_offset;
    if (off + total > ARENA_SIZE) {
        LeaveCriticalSection(&g_arena_cs);
        fprintf(stderr, "det: arena exhausted (requested %zu, used %zu/%u)\n", n, off, ARENA_SIZE);
        return nullptr;
    }
    g_arena_offset = off + total;
    LeaveCriticalSection(&g_arena_cs);
    ArenaHeader* h = (ArenaHeader*)(g_arena_base + off);
    h->size = (uint32_t)n;
    h->magic = ARENA_MAGIC;
    return (void*)(h + 1);
}

static size_t arena_size_of(void* p) {
    ArenaHeader* h = ((ArenaHeader*)p) - 1;
    return (h->magic == ARENA_MAGIC) ? h->size : 0;
}

extern "C" void* __cdecl det_wrap_malloc(size_t n) {
    pf_count_import(g_id_malloc);
    if (g_det_mode && g_arena_base) return arena_alloc(n);
    return g_real_malloc ? ((void*(__cdecl*)(size_t))g_real_malloc)(n) : nullptr;
}
extern "C" void* __cdecl det_wrap_calloc(size_t count, size_t size) {
    pf_count_import(g_id_calloc);
    if (g_det_mode && g_arena_base) return arena_alloc(count * size); // fresh VirtualAlloc pages are already zero
    return g_real_calloc ? ((void*(__cdecl*)(size_t, size_t))g_real_calloc)(count, size) : nullptr;
}
extern "C" void* __cdecl det_wrap_realloc(void* p, size_t n) {
    pf_count_import(g_id_realloc);
    if (g_det_mode && g_arena_base) {
        if (!p) return arena_alloc(n);
        size_t old_size = arena_size_of(p);
        void* np = arena_alloc(n);
        if (np && old_size) memcpy(np, p, old_size < n ? old_size : n); // old block intentionally leaked
        return np;
    }
    return g_real_realloc ? ((void*(__cdecl*)(void*, size_t))g_real_realloc)(p, n) : nullptr;
}
extern "C" void __cdecl det_wrap_free(void* p) {
    pf_count_import(g_id_free);
    if (g_det_mode && g_arena_base) return; // bump allocator: free is a no-op by design
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

// ---------------------------------------------------------------------
// B. Input script
// ---------------------------------------------------------------------
struct ScriptEvent { int tick; bool press; int scancode; };
static std::vector<ScriptEvent> g_script;
static size_t g_script_cursor = 0;

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
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == '\n' || *p == 0 || *p == '\r') continue;
        int tick; char verb[16]; char key[32];
        if (sscanf(p, "%d %15s %31s", &tick, verb, key) != 3) continue;
        ScriptEvent e;
        e.tick = tick;
        e.press = (_stricmp(verb, "press") == 0);
        e.scancode = resolve_key(key);
        g_script.push_back(e);
    }
    fclose(f);
    std::sort(g_script.begin(), g_script.end(),
              [](const ScriptEvent& a, const ScriptEvent& b) { return a.tick < b.tick; });
    fprintf(stderr, "det: loaded %zu input events from '%s'\n", g_script.size(), path);
}

// Called from the Sleep wrapper (main thread, both modes) right after the
// clock advances. keycode is passed as 0 for every synthetic event: KNOWN
// (disasm of key_dinput_handle_scancode, 0x46d7a7/0x46d7ac) the real
// DirectInput path sometimes passes -1 ("no ASCII") too, and only the
// scancode-indexed key[] array (read by poll_control/is_left/is_right/...,
// see notes/replay_format.md sec 1) drives menu+gameplay input - keycode
// feeds Allegro's separate ASCII/readkey() text-entry API, unused here.
static void deliver_due_input() {
    if (g_script.empty()) return;
    int T = det_current_tick();
    typedef void(__cdecl * PressFn)(int, int);
    typedef void(__cdecl * ReleaseFn)(int);
    while (g_script_cursor < g_script.size() && g_script[g_script_cursor].tick <= T) {
        const ScriptEvent& e = g_script[g_script_cursor];
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
            call_key_dinput_handle_scancode(dik, e.press ? 1 : 0);
        } else if (e.press) {
            ((PressFn)(void*)VA_HANDLE_KEY_PRESS)(0, e.scancode);
        } else {
            ((ReleaseFn)(void*)VA_HANDLE_KEY_RELEASE)(e.scancode);
        }
        fprintf(stderr, "det: T=%d delivered %s scancode=%d%s\n", T, e.press ? "press" : "release", e.scancode,
                g_inject_real_test ? " (via key_dinput_handle_scancode, --inject-real-test)" : "");
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
struct RealKeyEvent { int allegro_code; bool press; };
static const int kRealQueueCap = 256;
static RealKeyEvent g_real_queue[kRealQueueCap];
static int g_real_queue_head = 0, g_real_queue_tail = 0; // ring buffer, mod kRealQueueCap
static CRITICAL_SECTION g_real_queue_cs;
static bool g_real_queue_cs_inited = false;

static void real_queue_init() {
    if (!g_real_queue_cs_inited) { InitializeCriticalSection(&g_real_queue_cs); g_real_queue_cs_inited = true; }
}

// Called from the VEH callback on whichever thread hit the breakpoint (the
// real window thread, measured - carrier/NOTES.md "Milestones 5-7" part B).
static void real_queue_push(int allegro_code, bool press) {
    real_queue_init();
    EnterCriticalSection(&g_real_queue_cs);
    int next = (g_real_queue_tail + 1) % kRealQueueCap;
    if (next != g_real_queue_head) {
        g_real_queue[g_real_queue_tail].allegro_code = allegro_code;
        g_real_queue[g_real_queue_tail].press = press;
        g_real_queue_tail = next;
    } else {
        fprintf(stderr, "det: WARNING - real-input capture queue full (%d events), dropping one\n", kRealQueueCap);
    }
    LeaveCriticalSection(&g_real_queue_cs);
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
    if (allegro_code != 0) {
        real_queue_push(allegro_code, press);
    } else {
        fprintf(stderr, "det: T=%d real key event dik=0x%02x has no Allegro mapping "
                         "(hw_to_mycode[dik]==0), dropped\n", det_current_tick(), dik);
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
static void drain_real_key_queue() {
    if (g_input_policy != InputPolicy::Real) return; // nothing was ever queued
    RealKeyEvent e;
    int T = det_current_tick();
    typedef void(__cdecl * PressFn)(int, int);
    typedef void(__cdecl * ReleaseFn)(int);
    while (real_queue_pop(&e)) {
        if (e.press) ((PressFn)(void*)VA_HANDLE_KEY_PRESS)(0, e.allegro_code);
        else ((ReleaseFn)(void*)VA_HANDLE_KEY_RELEASE)(e.allegro_code);
        fprintf(stderr, "det: T=%d delivered real %s scancode=%d (captured at tick boundary)\n",
                T, e.press ? "press" : "release", e.allegro_code);
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
static void try_focus_guest_window_once() {
    if (g_tried_focus || g_input_policy != InputPolicy::Real) return;
    FocusSearch s = {GetCurrentProcessId(), nullptr};
    EnumWindows(focus_enum_proc, (LPARAM)&s);
    if (s.found) {
        SetForegroundWindow(s.found);
        ShowWindow(s.found, SW_RESTORE);
        fprintf(stderr, "det: focused guest window hwnd=%p (input_policy=real)\n", (void*)s.found);
        g_tried_focus = true; // succeeded - stop trying
    }
    // else: window doesn't exist yet (still starting up) - retried on the
    // next Sleep call, cheap since it's a handful of EnumWindows calls total.
}

extern "C" void __stdcall det_wrap_Sleep(DWORD ms) {
    pf_count_import(g_id_Sleep);
    if (GetCurrentThreadId() != g_main_tid) { ::Sleep(ms); return; }
    try_focus_guest_window_once();

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
        LONGLONG total_units = g_virtual_ms * TIMERS_PER_SECOND / 1000;
        LONGLONG delta = total_units - g_units_reported;
        g_units_reported = total_units;
        if (delta > 0) {
            typedef long(__cdecl * TickFn)(int);
            ((TickFn)(void*)VA_HANDLE_TIMER_TICK)((int)delta);
        }
        deliver_due_input();
        drain_real_key_queue(); // item 2: real events captured since the last tick
        if (g_pace_real) ::Sleep(ms);
    } else {
        ::Sleep(ms);
        deliver_due_input(); // real-time T, see det_now_ms()
        drain_real_key_queue();
    }
}

// pre-det semantics (KNOWN, notes/binary_recon.md item c): real hardware
// QueryPerformanceCounter, called by Allegro's timer thread internally (not
// created in det mode) and by 6 one-shot anti-cheat/statistics call sites
// inside play() - never read back into physics/replay/RNG.
extern "C" BOOL __stdcall det_wrap_QueryPerformanceCounter(LARGE_INTEGER* out) {
    pf_count_import(g_id_QPC);
    if (g_det_mode) {
        if (out) out->QuadPart = g_virtual_ms; // fake 1000 Hz counter tied to the virtual clock
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
    if (g_det_mode) return (DWORD)g_virtual_ms;
    if (g_real_timeGetTime) return ((DWORD(__stdcall*)())g_real_timeGetTime)();
    return 0;
}

// pre-det: msvcrt time(), wall-clock epoch seconds. KNOWN (notes/replay_format.md
// sec 2) this feeds all 3 srand() call sites (init_game, new_game x2) plus the
// qpc/clock/time anti-cheat trio in play(). Pinning it is what makes the
// tower-layout RNG seed reproducible across --det runs WITHOUT separately
// wrapping rand()/srand(): rand()'s LCG is already a pure function of the
// seed, and time() was the only host-entropy input to that seed.
extern "C" long __cdecl det_wrap_time(long* out) {
    pf_count_import(g_id_time);
    long v;
    if (g_det_mode) v = (long)(1700000000 + g_virtual_ms / 1000);
    else if (g_real_time) v = ((long(__cdecl*)(long*))g_real_time)(nullptr);
    else v = 0;
    if (out) *out = v;
    return v;
}

// pre-det: msvcrt clock(), CLOCKS_PER_SEC=1000 clock_t. KNOWN only read by
// play()'s anti-cheat trio (qpc/clock/time), never gameplay.
extern "C" long __cdecl det_wrap_clock() {
    pf_count_import(g_id_clock);
    if (g_det_mode) return (long)g_virtual_ms;
    if (g_real_clock) return ((long(__cdecl*)())g_real_clock)();
    return 0;
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

static void keypress_record_hit(CONTEXT* ctx) {
    if (!g_record_file) return;
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 8); // cdecl entry: [esp]=ret,[esp+4]=keycode,[esp+8]=scancode
    if (scancode >= 0 && scancode < 256) g_key_held[scancode] = true;
    const char* nm = scancode_to_name(scancode);
    if (nm) fprintf(g_record_file, "%d press %s\n", det_current_tick(), nm);
    else fprintf(g_record_file, "%d press %d\n", det_current_tick(), scancode);
    fflush(g_record_file);
}
static void keyrelease_record_hit(CONTEXT* ctx) {
    if (!g_record_file) return;
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 4); // cdecl entry: [esp]=ret,[esp+4]=scancode
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

    fprintf(stderr,
            "det: det_mode=%d pace=%s input=%s inject_real_test=%d stop_at_tick=%d digest_out=%s record_input=%s input_script=%s\n",
            g_det_mode, g_pace_real ? "real" : "fast", input_policy_name(g_input_policy), g_inject_real_test, g_stop_at_tick,
            opt.digest_out && opt.digest_out[0] ? opt.digest_out : "(none)",
            opt.record_input && opt.record_input[0] ? opt.record_input : "(none)",
            opt.input_script && opt.input_script[0] ? opt.input_script : "(none)");
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
    s->arena_offset = (unsigned)g_arena_offset;
    s->real_key_violations = g_real_key_violations;
    memcpy(s->key_held, g_key_held, sizeof(g_key_held));
    if (g_real_queue_cs_inited) EnterCriticalSection(&g_real_queue_cs);
    s->real_queue_head = g_real_queue_head;
    s->real_queue_tail = g_real_queue_tail;
    for (int i = 0; i < kRealQueueCap; ++i) {
        s->real_queue_code[i] = g_real_queue[i].allegro_code;
        s->real_queue_press[i] = g_real_queue[i].press ? 1 : 0;
    }
    if (g_real_queue_cs_inited) LeaveCriticalSection(&g_real_queue_cs);
}

void det_state_load(const DetSavedState* s) {
    g_virtual_ms = s->virtual_ms;
    g_units_reported = s->units_reported;
    g_rng_state = s->rng_state;
    g_rng_calls = (long)s->rng_calls;
    g_script_cursor = (size_t)s->script_cursor;
    // The arena is bump-only, so rewinding the bump pointer is exactly the
    // right semantics: every allocation made AFTER the snapshot is simply
    // forgotten and its bytes will be handed out again in the same order
    // (carrier/NOTES.md "Milestones 8-9", hazard list).
    g_arena_offset = (size_t)s->arena_offset;
    g_real_key_violations = s->real_key_violations;
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
}

void det_shutdown() {
    if (g_digest_file) { fflush(g_digest_file); fclose(g_digest_file); g_digest_file = nullptr; }
    if (g_record_file) { fflush(g_record_file); fclose(g_record_file); g_record_file = nullptr; }
}
