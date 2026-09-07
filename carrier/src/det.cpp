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
#include "det.hpp"
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

void det_bind_real(const char* name, void* real_proc) {
    if (strcmp(name, "QueryPerformanceCounter") == 0) g_real_QPC = (FARPROC)real_proc;
    else if (strcmp(name, "timeGetTime") == 0) g_real_timeGetTime = (FARPROC)real_proc;
    else if (strcmp(name, "time") == 0) g_real_time = (FARPROC)real_proc;
    else if (strcmp(name, "clock") == 0) g_real_clock = (FARPROC)real_proc;
    else if (strcmp(name, "_beginthread") == 0) g_real_beginthread = (FARPROC)real_proc;
    else if (strcmp(name, "malloc") == 0) g_real_malloc = (FARPROC)real_proc;
    else if (strcmp(name, "calloc") == 0) g_real_calloc = (FARPROC)real_proc;
    else if (strcmp(name, "realloc") == 0) g_real_realloc = (FARPROC)real_proc;
    else if (strcmp(name, "free") == 0) g_real_free = (FARPROC)real_proc;
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
    if (g_det_mode && g_arena_base) return arena_alloc(n);
    return g_real_malloc ? ((void*(__cdecl*)(size_t))g_real_malloc)(n) : nullptr;
}
extern "C" void* __cdecl det_wrap_calloc(size_t count, size_t size) {
    if (g_det_mode && g_arena_base) return arena_alloc(count * size); // fresh VirtualAlloc pages are already zero
    return g_real_calloc ? ((void*(__cdecl*)(size_t, size_t))g_real_calloc)(count, size) : nullptr;
}
extern "C" void* __cdecl det_wrap_realloc(void* p, size_t n) {
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
        if (e.press) ((PressFn)(void*)VA_HANDLE_KEY_PRESS)(0, e.scancode);
        else ((ReleaseFn)(void*)VA_HANDLE_KEY_RELEASE)(e.scancode);
        fprintf(stderr, "det: T=%d delivered %s scancode=%d\n", T, e.press ? "press" : "release", e.scancode);
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

extern "C" void __stdcall det_wrap_Sleep(DWORD ms) {
    if (GetCurrentThreadId() != g_main_tid) { ::Sleep(ms); return; }

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
// This makes the real keyboard fully inert in det mode regardless of which
// thread ends up calling it - synthetic --input-script events (which call
// _handle_key_press/_handle_key_release directly, bypassing this function
// entirely) are unaffected.
static void neutralize_keyboard_hit(CONTEXT* ctx) {
    DWORD ret = *(DWORD*)(uintptr_t)ctx->Esp;
    ctx->Esp += 4;
    ctx->Eip = ret;
}

static void keypress_record_hit(CONTEXT* ctx) {
    if (!g_record_file) return;
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 8); // cdecl entry: [esp]=ret,[esp+4]=keycode,[esp+8]=scancode
    fprintf(g_record_file, "%d press %d\n", det_current_tick(), scancode);
    fflush(g_record_file);
}
static void keyrelease_record_hit(CONTEXT* ctx) {
    if (!g_record_file) return;
    int scancode = *(int*)(uintptr_t)(ctx->Esp + 4); // cdecl entry: [esp]=ret,[esp+4]=scancode
    fprintf(g_record_file, "%d release %d\n", det_current_tick(), scancode);
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

// ---------------------------------------------------------------------
void det_init(const DetOptions& opt, DetShutdownFn shutdown_hook) {
    g_det_mode = opt.det_mode;
    g_pace_real = opt.pace_real;
    g_stop_at_tick = opt.stop_at_tick;
    g_shutdown = shutdown_hook;
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

    if (g_det_mode) {
        register_breakpoint(VA_KEY_DINPUT_SCANCODE, neutralize_keyboard_hit);
        fprintf(stderr, "det: real keyboard neutralized (key_dinput_handle_scancode short-circuited)\n");
    }

    if (opt.record_input && opt.record_input[0]) {
        g_record_file = fopen(opt.record_input, "w");
        if (!g_record_file) fprintf(stderr, "det: could not open --record-input '%s'\n", opt.record_input);
        register_breakpoint(VA_HANDLE_KEY_PRESS, keypress_record_hit);
        register_breakpoint(VA_HANDLE_KEY_RELEASE, keyrelease_record_hit);
    }
    if (opt.input_script && opt.input_script[0]) load_script(opt.input_script);

    fprintf(stderr, "det: det_mode=%d pace=%s stop_at_tick=%d digest_out=%s record_input=%s input_script=%s\n",
            g_det_mode, g_pace_real ? "real" : "fast", g_stop_at_tick,
            opt.digest_out && opt.digest_out[0] ? opt.digest_out : "(none)",
            opt.record_input && opt.record_input[0] ? opt.record_input : "(none)",
            opt.input_script && opt.input_script[0] ? opt.input_script : "(none)");
}

void det_shutdown() {
    if (g_digest_file) { fflush(g_digest_file); fclose(g_digest_file); g_digest_file = nullptr; }
    if (g_record_file) { fflush(g_record_file); fclose(g_record_file); g_record_file = nullptr; }
}
