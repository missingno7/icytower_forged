// bind.hpp - milestones 11-12 (win32_pilot.md SS3/SS7/SS7a/SS8/SS8a):
// the binding table, the 5-byte entry patch, and the per-invocation sensor
// that makes ORIGINAL / LIFTED / NATIVE forms of one game function
// comparable from equivalent state during a deterministic replay.
//
// Everything here is inert unless --bind / --bind-file / --fn-digest-out is
// passed: with no options the carrier behaves exactly as it did through
// milestone 7 (the milestone-7 digest proof is re-run unchanged, see
// carrier/NOTES.md "Milestones 11-12").
//
// The three forms are reached through ONE identity - the original address
// (win32_pilot.md SS3):
//
//   ORIGINAL  original bytes execute; sensed with hardware breakpoints
//             (DR slot at the entry, DR slot at the return address read
//             from [esp] at the entry). No bytes are patched.
//   LIFTED    lifted_<name> (carrier/lift/lifted/*.c) reached through a
//             5-byte `jmp rel32` at the original VA into a counting stub.
//   NATIVE    native_<name> (carrier/native/*.c), same mechanism.
//
// The record is IDENTICAL in shape for all three forms, which is what makes
// the comparison meaningful (carrier/scripts/compare_fn_digests.py).
#pragma once
#include <cstdio>

struct BindOptions {
    // --bind name=lifted|native|original[,name=...]   (comma-separated)
    const char* bind_spec;
    // --bind-file PATH: same syntax, one `name=form` per line, # comments.
    const char* bind_file;
    // --fn-digest-out PATH: per-invocation sensor records, one line each.
    const char* fn_digest_out;
    // --fault-inject name:k=N  (negative control, win32_pilot.md SS7):
    // flips one byte of that function's comparison domain immediately
    // after its N-th invocation's bound form returns and BEFORE the
    // post-record is taken, so the comparator must name exactly k=N.
    const char* fault_inject;
};

// Parses the binding spec, opens the record file, installs the entry
// patches and registers the ORIGINAL-form sensor breakpoints.
//
// MUST be called after the guest image is mapped (pe_image_load) and
// BEFORE det_arm_main_thread() (which arms the shared hardware-breakpoint
// table) and before the guest entry point runs. Fails loudly (exit 3) on
// an unknown function name, an unknown form, a missing bound form, or a
// failed entry patch - never silently degrades to ORIGINAL.
void bind_init(const BindOptions& opt);

// Flushes/closes the per-invocation record file. Safe to call twice.
void bind_shutdown();

// Migration-map metrics (win32_pilot.md SS8a) as JSON object members,
// written into the --report JSON by trace_write_report. Emits nothing when
// bind_init was never given any option (keeps the pre-milestone-11 report
// byte-shape for runs that do not use the binding table).
void bind_report_json(FILE* f);

// ---------------------------------------------------------------------
// Milestone 8: the per-invocation sensor's counters are carrier-owned state
// (they live in bind.cpp statics, not in guest memory), so an in-process
// rewind has to rewind them too - otherwise the `k=` index in
// --fn-digest-out would keep counting up across the rewind and
// compare_fn_digests.py could not line the two passes up. Fixed-size POD,
// written verbatim into the snapshot's carrier.bin.
//
// These are PER-BOUND-FUNCTION counters (one slot per binding-table row) -
// NOT nested/re-entrant stub state; the stub keeps no saved state of its
// own (its `id` rides on the guest stack, bind.cpp's bind_stub_common).
// So the arrays must be exactly as wide as the binding table, and their
// width is pinned to it HERE, in the one header both bind.cpp and
// snapshot.cpp include, instead of being restated as a literal:
// divergence 008's own pass found them still sized [8] - a stale relic of
// the milestone-8 era, with a comment that claimed "== bind.cpp's kMaxFns"
// long after kMaxFns had grown 8 -> 35 -> 42, so bind_state_save/load's
// `i < kMaxFns` loops were writing 34 slots past the end of each array
// (and past the end of the enclosing CarrierState) on every
// --snapshot-at-tick / --restore-at-tick. bind.cpp derives its kMaxFns
// from this constant and static_asserts the GENERATED table's kNumFns
// against it, so the two can no longer drift apart silently.
// ---------------------------------------------------------------------
const unsigned kBindMaxFns = 60;  // >= gen/bind_table.inc's kNumFns; bind.cpp
                                   // static_asserts that, and asserts it also
                                   // matches its BIND_STUB(N) count. Raised
                                   // 42 -> 60 (in-vivo verification pass,
                                   // 2026-09-07): a concurrently-running
                                   // batch-8 pass added a new src/icytower
                                   // function (draw_scroller), pushing
                                   // gen/bind_table.inc's row count to 43 and
                                   // tripping bind.cpp's static_assert. Same
                                   // mechanical bump this constant has taken
                                   // twice before (8 -> 35 -> 42, this file's
                                   // own comment above), with headroom this
                                   // time so a few more concurrently-promoted
                                   // functions don't immediately trip it
                                   // again.
struct BindSavedState {
    long long invocations[kBindMaxFns];
    long long crossings[kBindMaxFns];
    long long records[kBindMaxFns];
    long long faults_applied;
    long long domain_read_failures;
};
void bind_state_save(BindSavedState* s);
void bind_state_load(const BindSavedState* s);
