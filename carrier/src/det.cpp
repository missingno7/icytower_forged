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

// Import ids (see wrappers.hpp/det_bind_real doc), one per always-installed
// wrapper this file defines - each det_wrap_* below calls pf_count_import
// with its own id (item 3: fixes the report.json gap for these, which are
// wired directly into the guest IAT, bypassing the counting trampoline).
static int g_id_Sleep = -1, g_id_QPC = -1, g_id_timeGetTime = -1, g_id_time = -1,
           g_id_clock = -1, g_id_beginthread = -1,
           g_id_malloc = -1, g_id_calloc = -1, g_id_realloc = -1, g_id_free = -1;

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
struct DikMap { int allegro_code; int dik_code; };
static const DikMap kDikMap[] = {
    {59, 0x01}, {67, 0x1C}, {75, 0x39},
    {82, 0xCB}, {83, 0xCD}, {84, 0xC8}, {85, 0xD0},
};
static int allegro_to_dik(int allegro_code) {
    for (const DikMap& m : kDikMap)
        if (m.allegro_code == allegro_code) return m.dik_code;
    return -1; // no mapping - see deliver_due_input's inject_real_test branch
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
                                 "scancode=%d, skipping (add it to kDikMap if needed)\n",
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
// A. Virtual clock + thread virtualization
// ---------------------------------------------------------------------
static DWORD WINAPI parked_thread_proc(LPVOID) {
    WaitForSingleObject(g_parked_event, INFINITE); // never signaled: blocks forever, ~0% CPU
    return 0;
}
// A fake-but-real thread handle for a virtualized Allegro thread: the guest
// stores/CloseHandle's/WaitForSingleObject's this normally (all DIRECT,
// unwrapped imports), so it must be a genuine kernel handle, just one that
// never does anything.
static uintptr_t make_parked_handle() {
    HANDLE h = CreateThread(nullptr, 0, parked_thread_proc, nullptr, 0, nullptr);
    return (uintptr_t)h;
}

extern "C" uintptr_t __cdecl det_wrap_beginthread(void(__cdecl* start)(void*),
                                                    unsigned stack_size, void* arglist) {
    pf_count_import(g_id_beginthread);
    uintptr_t start_va = (uintptr_t)(void*)start;
    fprintf(stderr, "det: _beginthread(start=0x%p, stack=%u)\n", (void*)start, stack_size);
    if (g_det_mode && (start_va == VA_TIM_HIGH_PERF_THREAD || start_va == VA_TIM_LOW_PERF_THREAD)) {
        fprintf(stderr, "det: timer thread virtualized (entry=0x%p)\n", (void*)start);
        return make_parked_handle();
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
        if (g_pace_real) ::Sleep(ms);
    } else {
        ::Sleep(ms);
        deliver_due_input(); // real-time T, see det_now_ms()
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

static void keypress_record_hit(CONTEXT* ctx) {
    if (!g_record_file) return;
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 8); // cdecl entry: [esp]=ret,[esp+4]=keycode,[esp+8]=scancode
    const char* nm = scancode_to_name(scancode);
    if (nm) fprintf(g_record_file, "%d press %s\n", det_current_tick(), nm);
    else fprintf(g_record_file, "%d press %d\n", det_current_tick(), scancode);
    fflush(g_record_file);
}
static void keyrelease_record_hit(CONTEXT* ctx) {
    if (!g_record_file) return;
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 4); // cdecl entry: [esp]=ret,[esp+4]=scancode
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
    if (!handled) return EXCEPTION_CONTINUE_SEARCH; // not one of ours
    ctx->Dr6 = 0;
    ctx->EFlags |= 0x10000; // RF (resume flag): step past this instruction once without retriggering
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

    bool need_safepoint = opt.stop_at_tick > 0;
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
    // the one case that can't coexist with parking) before this runs; the
    // Real+inject_real_test diagnostic exception is what leaves the real
    // path UNparked here on purpose (see call_key_dinput_handle_scancode).
    if (g_input_policy != InputPolicy::Real) {
        register_breakpoint(VA_KEY_DINPUT_SCANCODE, neutralize_keyboard_hit);
        fprintf(stderr, "det: real keyboard PARKED (input=%s; key_dinput_handle_scancode short-circuited)\n",
                input_policy_name(g_input_policy));
    } else {
        fprintf(stderr, "det: real keyboard ACTIVE (input=real)\n");
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

void det_shutdown() {
    if (g_digest_file) { fflush(g_digest_file); fclose(g_digest_file); g_digest_file = nullptr; }
    if (g_record_file) { fflush(g_record_file); fclose(g_record_file); g_record_file = nullptr; }
}
