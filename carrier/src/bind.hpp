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
