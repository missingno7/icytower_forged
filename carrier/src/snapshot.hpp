// snapshot.hpp - milestones 8-9 (win32_pilot.md SS6/SS8 rows 8 and 9):
// a stable safepoint snapshot of the running carrier, an IN-PROCESS restore
// from it, and a bounded instruction trace taken from a restored state.
//
// The certification contract is notes/portforge_capsule.md SS D's anchor
// certification, "restore -> suffix == cold -> suffix": the per-tick digest
// stream produced by `cold -> anchor -> suffix` must equal, row for row, the
// stream produced by `restore(anchor) -> suffix`.
//
// Everything here is inert unless one of the SnapshotOptions fields below is
// set, so a plain --det replay behaves exactly as it did through milestone 12.
//
// SCOPE, stated up front: this is an in-process rewind. Host objects (HWND,
// COM device pointers, kernel HANDLEs, GDI objects, the DirectSound mixer)
// are NOT snapshotted and NOT re-bound - they survive because the restore
// happens in the very process that created them. A cross-process restore is
// a later stage and would need the logical-handle layer win32_pilot.md SS6
// defers ("Raw host handles and COM pointers are not durable state; they are
// re-bound").
#pragma once
#include <windows.h>
#include <cstdio>

struct SnapshotOptions {
    int         snapshot_at_tick; // --snapshot-at-tick T   (<=0 = never)
    const char* snapshot_out;     // --snapshot-out DIR
    const char* restore_from;     // --restore-from DIR     (nullptr = no restore)
    // --restore-at-tick T2: rewind while running, at the safepoint whose
    // tick is >= T2. <=0 together with restore_from means "restore at the
    // FIRST safepoint this process reaches", i.e. the cold-start form.
    int         restore_at_tick;
    // --restore-fault: negative control. Flips bit 0 of one byte of the
    // restored .bss (reward_scale, 0x4fac28 - inside the per-tick digest
    // scope, carrier/gen/game_globals.inc), so the comparator must name the
    // restore tick itself as the first difference.
    bool        restore_fault;
    int         trace_window;     // --trace-window N: single-step N instructions after the restore
    const char* trace_window_out; // --trace-window-out PATH (default artifacts/trace_window.txt)
    const char* image_path;       // guest EXE path, for the manifest's image_sha256 identity
};

// Records the options. Call once from main(), before the guest starts.
void snapshot_init(const SnapshotOptions& opt);

// Called from det.cpp's safepoint_hit, on the guest main thread, from inside
// the VEH, with the full CONTEXT available.
//   _pre  performs a due restore (overwrites *ctx with the snapshot's saved
//         CONTEXT); returns true if it did. Runs BEFORE the tick index is
//         read, so the digest line this safepoint writes is the restored
//         tick's line.
//   _post takes a due snapshot, after that tick's digest line was written.
bool snapshot_on_safepoint_pre(CONTEXT* ctx);
void snapshot_on_safepoint_post(CONTEXT* ctx);

// Milestone 9's instruction "microscope": true while the trap-flag trace
// window is open. det_veh_handler claims the resulting single-step
// exceptions and calls snapshot_trace_step for each one.
bool snapshot_trace_active();
void snapshot_trace_step(CONTEXT* ctx);

// --report JSON member (sizes and timings); emits nothing when no snapshot
// option was given, so pre-milestone-8 report shapes are unchanged.
void snapshot_report_json(FILE* f);

// Flushes/closes the trace file. Safe to call twice.
void snapshot_shutdown();
