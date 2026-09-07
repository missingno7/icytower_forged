// det.hpp - milestones 5-7: virtual time, input injection, and the tick
// sensor (hardware-breakpoint digest recorder). See carrier/NOTES.md,
// section "Milestones 5-7", for the evidence behind every address and
// design choice here. Everything is inert unless one of the DetOptions
// fields below asks for it - default (non-det, no scripts, no digest,
// no recording) carrier behavior is unchanged.
#pragma once
#include <windows.h>
#include <cstdint>

// win32_pilot.md sec 5a "Input source is an exclusive policy": each run has
// exactly one active input provider, never a mix. Default resolution (see
// main.cpp parse_args): Script when --input-script is given, else Real.
enum class InputPolicy { Real, Script, None };

// Human-readable name ("real"/"script"/"none"), for the stdout banner and
// the --report JSON. Never null.
const char* input_policy_name(InputPolicy p);

// "Environment isolation" pass (carrier/NOTES.md): how the guest's own
// top-level window is allowed to appear on the operator's desktop. An
// automated run must never take the foreground - see det.cpp's
// det_wrap_ShowWindow/det_wrap_SetForegroundWindow/det_wrap_SetWindowPos/
// det_wrap_CreateWindowExA and win32_pilot.md sec 4a (presentation is BELOW
// the PortForge boundary, so suppressing it cannot change game state - which
// is exactly what gate G1 re-verifies).
enum class WindowMode { Normal, MinNoActive, Hidden };
const char* window_mode_name(WindowMode m);

struct DetOptions {
    bool det_mode;            // --det
    bool pace_real;           // --pace=real (default fast: never really Sleep in det mode)
    InputPolicy input_policy; // --input=real|script|none (main.cpp resolves the default + validates)
    const char* input_script; // --input-script PATH, or nullptr (only loaded/delivered when input_policy==Script,
                               // UNLESS inject_real_test is set - see below)
    const char* record_input; // --record-input PATH, or nullptr
    const char* digest_out;   // --digest-out PATH, or nullptr
    int stop_at_tick;         // --stop-at-tick T, <=0 = unbounded
    const char* image_path;   // guest EXE path, for --record-input's "# image_sha256:" header line
    // Hidden diagnostic (item 2, win32_pilot.md / carrier/NOTES.md "Input
    // policy and recording"): only meaningful with input_policy==Real and
    // input_script set (the ONE exception to the real+input-script error).
    // Instead of calling _handle_key_press/_handle_key_release directly
    // (the normal Script-mode delivery path), each scripted event is fed
    // through key_dinput_handle_scancode - the REAL DirectInput path's own
    // entry point - so --record-input's breakpoints (also at
    // _handle_key_press/_handle_key_release) are exercised by the actual
    // recording hook, not bypassed by it. Lets an automated pass test the
    // record/replay round trip without a live human keyboard.
    bool inject_real_test;
    // Milestones 8-9: the snapshot/restore machinery hangs off the SAME tick
    // safepoint sensor (VA 0x4124f4) the digest uses, so a --snapshot-at-tick
    // or --restore-from run needs that breakpoint armed even without
    // --digest-out / --stop-at-tick. main.cpp sets this from the snapshot
    // options; det_init ORs it into its own need_safepoint decision.
    bool force_safepoint;
    // --trace-input PATH ("-" = stderr), divergence 005 diagnostic: logs every
    // delivered/captured/stamped key event in BOTH modes with the virtual
    // clock ms, the carrier tick T, the sub-tick Sleep index, the guest's own
    // cycle_count (0x506938) before and after _handle_timer_tick, the
    // safepoint count and the exact call site. See det.cpp's trace_input.
    const char* trace_input;
    // --- "Environment isolation" pass (carrier/NOTES.md) ------------------
    // --interactive: a human is at the keyboard/screen for this run
    // (scripts/play.py passes it for its interactive and --record-replay
    // modes). ONLY an --interactive run may call SetForegroundWindow on the
    // guest window or let the guest's own SetForegroundWindow/ShowWindow
    // calls through; an automated run must never take the operator's
    // foreground.
    bool interactive;
    // How the guest window is shown when !interactive (main.cpp defaults it
    // to MinNoActive; --window=normal|minnoactive|hidden overrides).
    WindowMode window_mode;
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

// "Environment isolation" pass: installs the 5-byte entry patches that put
// the window-activation channel (_switch_in/_switch_out) and the DirectInput
// mouse (_handle_mouse_input) under carrier control. Must run AFTER
// pe_image_load has mapped the guest (there is nothing to patch before that)
// and before the guest entry point - main.cpp calls it next to bind_init().
// Inert outside --det when the input policy is Real (i.e. the plain oracle).
void det_install_entry_patches();

// Arms the same breakpoint table directly on an arbitrary (already-running)
// thread - called from a helper thread with `thread` suspended first. Used
// by det_arm_main_thread() and, for --record-input, by det_wrap_beginthread
// to also arm the real DirectInput input thread (key events are logged from
// there, not the main thread - see det.cpp).
void det_arm_thread(HANDLE thread);

// ---------------------------------------------------------------------
// Shared hardware-breakpoint table (milestones 11-12, bind.cpp).
//
// The {VA, callback} table below Dr0-Dr3 was always generic (carrier/NOTES.md
// "Milestones 5-7" part C); these three functions are the only thing that
// was missing for a second consumer. Slot order is registration order:
// det_init registers the tick safepoint first (DR0) and, when the input
// policy is not Real, the keyboard neutralization second (DR1), so a replay
// run leaves DR2/DR3 for bind.cpp's ORIGINAL-form entry/return sensing.
// ---------------------------------------------------------------------

// Adds a slot. Returns its index (0..3, i.e. which DrN it will occupy) or
// -1 if all four are taken. Must be called BEFORE det_arm_main_thread().
// `va` may be 0 for a slot that is armed later, from inside a callback,
// with det_ctx_arm_slot.
int det_register_breakpoint(DWORD_PTR va, void (*cb)(CONTEXT*));

// Arms / disarms one slot by editing the CONTEXT a VEH callback is about to
// resume. This is the only way to move a breakpoint from inside the
// breakpoint's own handler: a thread cannot SetThreadContext itself, but
// the context returned with EXCEPTION_CONTINUE_EXECUTION is applied
// wholesale, debug registers included. Call only from a callback invoked by
// det_veh_handler.
void det_ctx_arm_slot(CONTEXT* ctx, int slot, DWORD_PTR va);
void det_ctx_disarm_slot(CONTEXT* ctx, int slot);

// The carrier tick index T (virtual_ms/20 in --det). Same value the
// per-tick digest lines are keyed by, so a per-invocation record and a
// per-tick digest can be lined up.
int det_tick();

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
// virtualized Allegro threads). `id` is the import's g_real[]/report.json
// index (imports.cpp's loop index) - stashed so each det_wrap_* can call
// pf_count_import(id) itself (item 3: these wrappers are wired directly into
// the guest IAT, bypassing pf_import_common's counting trampoline entirely -
// see trace.hpp's pf_count_import for the single-place fix).
void det_bind_real(const char* name, void* real_proc, int id);

// ---------------------------------------------------------------------
// Milestone 8: the carrier-owned ("externalized") half of a snapshot.
//
// win32_pilot.md sec 6 lists this explicitly alongside the guest pages:
// virtual time, replay cursor, RNG state. Everything below lives in
// det.cpp's own statics - NOT in guest memory - so it is invisible to the
// image/arena/stack components and has to be carried separately. POD,
// fixed-size, memcpy-able: snapshot.cpp writes it verbatim into
// carrier.bin and hashes it like any other component.
// ---------------------------------------------------------------------
struct DetSavedState {
    long long virtual_ms;      // the virtual clock; det_tick() == virtual_ms/20
    long long units_reported;  // total timer units already handed to _handle_timer_tick
    unsigned  rng_state;       // pinned msvcrt LCG state (see det_wrap_rand)
    long long rng_calls;
    unsigned  script_cursor;   // --input-script replay cursor
    unsigned  arena_offset;    // deterministic heap arena bump pointer
    int       real_queue_head, real_queue_tail;
    int       real_queue_code[256];   // kRealQueueCap
    unsigned char real_queue_press[256];
    unsigned char key_held[256];      // --record-input hygiene filter state
    long      real_key_violations;
    int       last_drain_tick;        // divergence 005: the tick the real-key queue was last drained at
    // "Environment isolation" pass: the two new carrier-owned channels.
    int       time_cursor;            // replay cursor into the recorded `T time <v>` values
    int       switch_queue_head, switch_queue_tail;
    unsigned char switch_queue_dir[64]; // kSwitchQueueCap; 1 = switch in, 0 = switch out
};

void det_state_save(DetSavedState* s);
void det_state_load(const DetSavedState* s);

// Guest-memory regions the snapshot captures, exposed here so snapshot.cpp
// does not re-derive them (KNOWN: carrier/NOTES.md, notes/binary_recon.md).
#define PF_GUEST_DATA_VA   0x004bc000u
#define PF_GUEST_DATA_SIZE 0x000176f4u
#define PF_GUEST_BSS_VA    0x004dd000u
#define PF_GUEST_BSS_SIZE  0x00036978u
#define PF_GUEST_ARENA_VA  0x20000000u
#define PF_GUEST_STACK_VA  0x0e000000u
#define PF_GUEST_STACK_SZ  0x00200000u

// Milestone 8: the pinned RNG state, as an externalized snapshot component
// (snapshot.cpp), plus the --rng-selftest unit check against the real
// msvcrt.dll rand() (returns a process exit code: 0 = OK, 4 = mismatch).
unsigned det_rng_state();
void det_set_rng_state(unsigned s);
long det_rng_calls();
void det_set_rng_calls(long n);
int det_rng_selftest();

// Deterministic heap arena statistics (divergence 004: the bump-only arena
// was replaced by a first-fit + coalescing free-list allocator whose whole
// state lives inside the arena region). `top` is the number of arena bytes
// that are live - the same value the snapshot's "arena" component stores -
// and `hwm` is the high-water mark of that value over the whole run. All
// zero when the arena is not active (non-det runs).
void det_arena_stats(unsigned* top, unsigned* hwm, unsigned* live_bytes,
                     unsigned* peak_live_bytes, unsigned* live_blocks);
unsigned det_arena_top();

// --report JSON accessors (trace.cpp's trace_write_report calls these).
const char* det_input_policy_name(); // "real" | "script" | "none" | "(unset)" before det_init runs
long det_real_key_violations();      // see det_veh_handler's neutralize_keyboard_hit

// "Environment isolation" pass: one JSON object summarising every channel
// this pass took ownership of (focus/activation, mouse, ad thread, window
// policy, recorded clock, getenv). Written by trace.cpp into --report's
// top-level "environment" field; `buf` gets a `{...}` object, never null.
void det_environment_json(char* buf, size_t n);

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
    // Item 3 ("parked timer thread" pass, carrier/NOTES.md): generic fix for
    // divergence 003 (notes/living_record.md) - _tim_win32_exit's join loop
    // (WaitForSingleObject(timer_thread_handle, 100) while WAIT_TIMEOUT)
    // never succeeded because the OLD virtualized timer thread blocked
    // forever on a carrier-private event nobody ever signaled. Now the
    // virtualized thread body IS the original entry point (det_wrap_
    // beginthread), and this wrapper substitutes INFINITE for any FINITE
    // WaitForSingleObject timeout called FROM that thread (see det.cpp's
    // det_is_parked_thread) - so the thread parks in its own real wait loop
    // on the guest's own stop_event and exits cleanly when the guest
    // signals it, satisfying the join without ever taking the
    // WAIT_TIMEOUT/_handle_timer_tick branch (tick delivery stays exactly
    // as before: synchronous, from det_wrap_Sleep on the main thread).
    DWORD __stdcall det_wrap_WaitForSingleObject(HANDLE h, DWORD ms);
    // Deterministic heap arena (det mode only - see det.cpp), needed because
    // the real msvcrt heap's base address is randomized per-process by
    // Windows and pointers into it get baked directly into .data/.bss
    // globals (e.g. Allegro's `screen` BITMAP*), which the digest sensor
    // (part C) would otherwise see as spurious per-run differences.
    // Milestone 8 (win32_pilot.md sec 5 "pin the LCG"): moves the RNG state
    // out of msvcrt.dll's per-thread CRT data - which no snapshot component
    // can reach - into carrier memory, where it becomes part of the
    // snapshot. Verified against the real msvcrt rand() by --rng-selftest.
    int  __cdecl det_wrap_rand();
    void __cdecl det_wrap_srand(unsigned seed);
    void* __cdecl det_wrap_malloc(size_t n);
    void* __cdecl det_wrap_calloc(size_t count, size_t size);
    void* __cdecl det_wrap_realloc(void* p, size_t n);
    void __cdecl det_wrap_free(void* p);
    // --- "Environment isolation" pass -------------------------------------
    // Window policy (item 1): an automated run's guest window must never take
    // the operator's foreground. Evidence for each substitution is at the
    // wrapper in det.cpp; all four forward unchanged when --interactive.
    BOOL __stdcall det_wrap_ShowWindow(HWND h, int cmd);
    BOOL __stdcall det_wrap_SetForegroundWindow(HWND h);
    BOOL __stdcall det_wrap_SetWindowPos(HWND h, HWND after, int x, int y, int cx, int cy, UINT flags);
    HWND __stdcall det_wrap_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style,
                                            int x, int y, int w, int hgt, HWND parent,
                                            HMENU menu, HINSTANCE inst, LPVOID param);
    // Network ad fetch (item 5): the ONE pthread_create call site in the whole
    // binary is fldads_start -> fldads_threadmain (0x404014), and five
    // fld_adspot.c globals are inside the 151-global digest domain, so the
    // live HTTP result CAN reach the verdict. Suppressed in --det.
    int __cdecl det_wrap_pthread_create(void* th, void* attr, void* (__cdecl* start)(void*), void* arg);
    // Environment variables (item 5): instrumentation only - forwards every
    // call, but records the distinct names asked for and whether the host had
    // a value, so the channel can be reported as evidence instead of assumed.
    char* __cdecl det_wrap_getenv(const char* name);
}
