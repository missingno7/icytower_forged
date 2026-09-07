// report.cpp - see report.hpp.
#include <windows.h>
#include <cstdio>
#include "report.hpp"
#include "../../port_forge/src/platform/win32/trace.hpp"
#include "det.hpp"            // input_policy / real_key_violations / arena / rng / environment
#include "bind.hpp"           // the milestone 11-12 migration map
#include "snapshot.hpp"       // milestone 8-9 snapshot sizes/timings
#include "headless.hpp"       // "headless" object (set_gfx_mode/install_sound sensors)
#include "frame.hpp"          // "frame_oracle" object
#include "print_globals.hpp"  // "print_globals" array

void carrier_write_report(const char* report_path) {
    if (!report_path || !report_path[0]) return;
    FILE* f = fopen(report_path, "w");
    if (!f) {
        fprintf(stderr, "carrier_write_report: could not open '%s'\n", report_path);
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
    pf::win32::trace_write_import_counts(f);
    fprintf(f, "}\n");
    fclose(f);
}
