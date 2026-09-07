// snapshot.cpp - see snapshot.hpp. Milestones 8-9 of win32_pilot.md.
//
// WHERE the snapshot is taken: the existing tick safepoint, VA 0x4124f4
// inside play(), once per consumed game tick (KNOWN, notes/binary_recon.md
// item l). The DR0 sensor already fires there on the guest MAIN thread from
// inside det.cpp's VEH, which is the one place in this carrier where a full,
// resumable CONTEXT of the guest main thread exists and no host call is in
// flight on it (win32_pilot.md SS6 "Safepoint"; notes/portforge_capsule.md
// SS D "a snapshot may only be taken at a declared semantic SAFE POINT").
//
// WHAT it contains (the four guest components + one carrier component):
//   context.bin  the main thread's CONTEXT, integer + EIP + EFLAGS + x87/SSE
//   data.bin     .data  0x4bc000 + 0x176f4   (the FULL section)
//   bss.bin      .bss   0x4dd000 + 0x36978   (the FULL section)
//   arena.bin    the deterministic heap arena at 0x20000000, used range only
//   stack.bin    the guest stack's LIVE range, [ESP, 0x0e200000)
//   carrier.bin  DetSavedState + BindSavedState (virtual clock, script
//                cursor, RNG state, real-key queue, sensor counters)
//
// The full sections are captured, not just the game-owned globals the
// per-tick digest hashes: Allegro's timer queue, key[] state, mixer and menu
// state all live in those sections and all have to come back for the guest
// to keep running. The host-identity pointers inside them (COM device
// pointers, HANDLEs, an HWND - the ~25 globals carrier/NOTES.md "the digest
// region" identified) stay VALID precisely because this is an in-process
// rewind: the objects they name are still alive in this same process.
//
// WHAT it deliberately does not contain: host objects. See snapshot.hpp's
// scope note and carrier/NOTES.md "Milestones 8-9" for the hazard list.
#define NOMINMAX // windows.h's min/max macros break sha256.hpp's std::min<...>
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include "snapshot.hpp"
#include "det.hpp"
#include "bind.hpp"
#include "../../port_forge/src/platform/win32/symbols.hpp"
#include "../../port_forge/src/core/sha256.hpp"
#include "../win32_policy.hpp"

// ---------------------------------------------------------------------
// The carrier-owned component. One POD, one version stamp; det.cpp and
// bind.cpp each own their half (det.hpp/bind.hpp).
// ---------------------------------------------------------------------
#define PF_CARRIER_STATE_MAGIC   0x53434650u // 'PFCS'
// v2 (divergence 008): BindSavedState's per-function counter arrays grew
// from a stale [8] to bind.hpp's kBindMaxFns (42), so carrier.bin is a
// different size and shape than every snapshot taken before that fix. The
// size check below already rejects those, but the stamp makes the reason
// legible rather than "wrong size for this build".
#define PF_CARRIER_STATE_VERSION 2u

struct CarrierState {
    uint32_t       magic;
    uint32_t       version;
    int32_t        tick;
    uint32_t       x87_cw_live; // fnstcw read at the safepoint (expect 0x037F)
    DetSavedState  det;
    BindSavedState bind;
};

static const char* kFormat = "portforge-win32-carrier-snapshot-v1";

// ---------------------------------------------------------------------
static SnapshotOptions g_opt;
static bool g_any = false;
static bool g_snapshot_done = false;
static bool g_restore_done = false;
// True for the in-run rewind form (--snapshot-at-tick T + --restore-at-tick
// T2 in ONE run): the restore source is the snapshot this same run takes,
// so it must not fire before that snapshot exists.
static bool g_in_run_rewind = false;

static double g_capture_ms = 0.0, g_restore_ms = 0.0;
static size_t g_snapshot_bytes = 0;
static int    g_snapshot_tick = -1, g_restored_tick = -1;

// Trace window (milestone 9).
static FILE* g_trace_file = nullptr;
static long  g_trace_remaining = 0;
static long  g_trace_index = 0;
static CONTEXT g_trace_prev;
static bool  g_trace_have_prev = false;

// The codec primitives (blob write/read with a hash, the manifest's
// integrity lookup, other-thread quiescence, the x87 control word, the two
// identity hashes) are pf::win32::snapshot_* in
// port_forge/src/platform/win32/snapshot.hpp, together with the ordering
// rule they depend on. What stays here is what is NOT generic: the manifest
// DOCUMENT (a certification contract carrier/scripts' verdict tools read
// field by field), CarrierState (whose two halves are det.cpp's and
// bind.cpp's), and the region list - which is icytower::kSnapshotDomain.
static double now_ms() { return pf::win32::snapshot_now_ms(); }

static void die(const char* what) {
    fprintf(stderr, "snapshot: FATAL - %s\n", what);
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 6);
}

static void join_path(char* out, size_t n, const char* dir, const char* leaf) {
    _snprintf(out, n, "%s\\%s", dir, leaf);
    out[n - 1] = 0;
}

static bool write_blob(const char* dir, const char* leaf, const void* p, size_t n, std::string* sha) {
    return pf::win32::snapshot_write_blob(dir, leaf, p, n, sha);
}
static bool read_blob(const char* dir, const char* leaf, std::vector<uint8_t>* out) {
    return pf::win32::snapshot_read_blob(dir, leaf, out);
}
static bool manifest_find_sha(const std::string& text, const char* comp, std::string* out) {
    return pf::win32::snapshot_manifest_sha(text, comp, out);
}
static int suspend_other_threads(HANDLE* handles, int cap) {
    return pf::win32::snapshot_suspend_other_threads(handles, cap);
}
static void resume_threads(HANDLE* handles, int n) {
    pf::win32::snapshot_resume_threads(handles, n);
}
static unsigned read_x87_cw() { return pf::win32::snapshot_read_x87_cw(); }

// The four guest regions, by their policy index (icytower::kSnapshotDomain).
static const unsigned long D_VA    = icytower::kSnapshotRegions[icytower::kRegionData].va;
static const unsigned long D_SIZE  = icytower::kSnapshotRegions[icytower::kRegionData].size;
static const unsigned long B_VA    = icytower::kSnapshotRegions[icytower::kRegionBss].va;
static const unsigned long B_SIZE  = icytower::kSnapshotRegions[icytower::kRegionBss].size;
static const unsigned long A_VA    = icytower::kSnapshotRegions[icytower::kRegionArena].va;
static const unsigned long S_VA    = icytower::kSnapshotRegions[icytower::kRegionStack].va;
static const unsigned long S_SIZE  = icytower::kSnapshotRegions[icytower::kRegionStack].size;

// Component descriptor: manifest key AND file basename stem.
struct Component {
    const char* name;
    const void* addr;
    size_t      size;
    std::string sha;
};

// ---------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------
static void do_capture(CONTEXT* ctx, int tick) {
    double t0 = now_ms();
    const char* dir = g_opt.snapshot_out;
    if (!CreateDirectoryA(dir, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        fprintf(stderr, "snapshot: CreateDirectory('%s') failed gle=%lu\n", dir, GetLastError());
        die("could not create the snapshot directory");
    }

    // The guest stack's LIVE range only: everything below ESP is dead, and
    // ESP is measured constant (0x0e1fef30) at every safepoint of this
    // workload (see carrier/NOTES.md), so this is ~4 KB, not 2 MB.
    uintptr_t stack_top = (uintptr_t)S_VA + S_SIZE;
    uintptr_t stack_lo = (uintptr_t)ctx->Esp;
    if (stack_lo < (uintptr_t)S_VA || stack_lo >= stack_top)
        die("guest ESP is outside the fixed guest stack region (--guest-stack=host?)");
    size_t stack_size = (size_t)(stack_top - stack_lo);

    CarrierState cs;
    memset(&cs, 0, sizeof(cs));
    cs.magic = PF_CARRIER_STATE_MAGIC;
    cs.version = PF_CARRIER_STATE_VERSION;
    cs.tick = tick;
    cs.x87_cw_live = read_x87_cw();
    det_state_save(&cs.det);
    bind_state_save(&cs.bind);

    Component comps[6] = {
        { "context", ctx,                                   sizeof(CONTEXT),     std::string() },
        { "data",    (const void*)(uintptr_t)D_VA, D_SIZE, std::string() },
        { "bss",     (const void*)(uintptr_t)B_VA,  B_SIZE,  std::string() },
        { "arena",   (const void*)(uintptr_t)A_VA, cs.det.arena_offset, std::string() },
        { "stack",   (const void*)stack_lo,                  stack_size,          std::string() },
        { "carrier", &cs,                                    sizeof(cs),          std::string() },
    };
    static const char* kFiles[6] = { "context.bin", "data.bin", "bss.bin", "arena.bin", "stack.bin", "carrier.bin" };

    size_t total = 0;
    for (int i = 0; i < 6; ++i) {
        if (!write_blob(dir, kFiles[i], comps[i].addr, comps[i].size, &comps[i].sha))
            die("could not write a snapshot component");
        total += comps[i].size;
    }

    // Image identity: the read-only half of the mapped guest image
    // (headers + .text + .rdata, 0x400000 .. .data) plus, when the path is
    // known, the guest EXE file itself. A restore into a differently-built
    // image is nonsense and this is what a future loader would check.
    std::string image_sha = pf::win32::snapshot_image_identity(icytower::kSnapshotDomain);
    std::string file_sha = pf::win32::snapshot_file_sha(g_opt.image_path);

    char mpath[MAX_PATH];
    join_path(mpath, sizeof(mpath), dir, "manifest.json");
    FILE* m = fopen(mpath, "w");
    if (!m) die("could not write manifest.json");
    fprintf(m, "{\n");
    fprintf(m, "  \"format\": \"%s\",\n", kFormat);
    fprintf(m, "  \"tick\": %d,\n", tick);
    // MEASURED, load-bearing (carrier/NOTES.md "Milestones 8-9"): restoring
    // this snapshot into a DIFFERENT process is not safe, because .data/.bss
    // and the arena are full of host-object identities (DirectDraw surface
    // memory addresses, COM device pointers, HANDLEs, an HWND) that a fresh
    // process assigns differently. The pid is recorded so a restore can SAY
    // so instead of crashing mysteriously.
    fprintf(m, "  \"capture_pid\": %lu,\n", GetCurrentProcessId());
    fprintf(m, "  \"safepoint_va\": \"0x%08lx\",\n", icytower::kSnapshotDomain.safepoint_va);
    fprintf(m, "  \"image_text_sha256\": \"%s\",\n", image_sha.c_str());
    fprintf(m, "  \"image_file_sha256\": \"%s\",\n", file_sha.c_str());
    fprintf(m, "  \"total_bytes\": %u,\n", (unsigned)total);
    fprintf(m, "  \"context\": { \"file\": \"context.bin\", \"size\": %u, \"sha256\": \"%s\",\n",
            (unsigned)sizeof(CONTEXT), comps[0].sha.c_str());
    fprintf(m, "                \"context_flags\": \"0x%08lx\", \"eip\": \"0x%08lx\", "
               "\"esp\": \"0x%08lx\", \"ebp\": \"0x%08lx\", \"eflags\": \"0x%08lx\",\n",
            (unsigned long)ctx->ContextFlags, (unsigned long)ctx->Eip,
            (unsigned long)ctx->Esp, (unsigned long)ctx->Ebp, (unsigned long)ctx->EFlags);
    fprintf(m, "                \"eax\": \"0x%08lx\", \"ebx\": \"0x%08lx\", \"ecx\": \"0x%08lx\", "
               "\"edx\": \"0x%08lx\", \"esi\": \"0x%08lx\", \"edi\": \"0x%08lx\",\n",
            (unsigned long)ctx->Eax, (unsigned long)ctx->Ebx, (unsigned long)ctx->Ecx,
            (unsigned long)ctx->Edx, (unsigned long)ctx->Esi, (unsigned long)ctx->Edi);
    fprintf(m, "                \"x87_control_word_context\": \"0x%04x\", "
               "\"x87_control_word_live\": \"0x%04x\",\n",
            (unsigned)(ctx->FloatSave.ControlWord & 0xffffu), (unsigned)cs.x87_cw_live);
    fprintf(m, "                \"has_floating_point\": %s, \"has_extended_registers\": %s },\n",
            (ctx->ContextFlags & CONTEXT_FLOATING_POINT) ? "true" : "false",
            (ctx->ContextFlags & CONTEXT_EXTENDED_REGISTERS) ? "true" : "false");
    fprintf(m, "  \"data\":  { \"file\": \"data.bin\",  \"va\": \"0x%08x\", \"size\": %u, \"sha256\": \"%s\" },\n",
            D_VA, (unsigned)D_SIZE, comps[1].sha.c_str());
    fprintf(m, "  \"bss\":   { \"file\": \"bss.bin\",   \"va\": \"0x%08x\", \"size\": %u, \"sha256\": \"%s\" },\n",
            B_VA, (unsigned)B_SIZE, comps[2].sha.c_str());
    fprintf(m, "  \"arena\": { \"file\": \"arena.bin\", \"va\": \"0x%08x\", \"size\": %u, \"sha256\": \"%s\", "
               "\"bump_pointer\": %u },\n",
            A_VA, (unsigned)comps[3].size, comps[3].sha.c_str(),
            (unsigned)cs.det.arena_offset);
    fprintf(m, "  \"stack\": { \"file\": \"stack.bin\", \"va\": \"0x%08x\", \"size\": %u, \"sha256\": \"%s\", "
               "\"top\": \"0x%08x\" },\n",
            (unsigned)stack_lo, (unsigned)stack_size, comps[4].sha.c_str(), (unsigned)stack_top);
    fprintf(m, "  \"carrier\": { \"file\": \"carrier.bin\", \"size\": %u, \"sha256\": \"%s\",\n",
            (unsigned)sizeof(cs), comps[5].sha.c_str());
    fprintf(m, "                \"virtual_ms\": %lld, \"units_reported\": %lld, "
               "\"rng_state\": %u, \"rng_calls\": %lld,\n",
            cs.det.virtual_ms, cs.det.units_reported, cs.det.rng_state, cs.det.rng_calls);
    fprintf(m, "                \"input_script_cursor\": %u, \"arena_bump\": %u, "
               "\"real_key_queue_head\": %d, \"real_key_queue_tail\": %d,\n",
            cs.det.script_cursor, cs.det.arena_offset,
            cs.det.real_queue_head, cs.det.real_queue_tail);
    fprintf(m, "                \"fn_invocations\": [%lld, %lld, %lld] },\n",
            cs.bind.invocations[0], cs.bind.invocations[1], cs.bind.invocations[2]);
    fprintf(m, "  \"host_objects\": \"NOT captured (HWND, COM device pointers, kernel HANDLEs, "
               "GDI objects, DirectSound mixer). The in-process rewind relies on them surviving "
               "in this same process - see carrier/NOTES.md 'Milestones 8-9'.\"\n");
    fprintf(m, "}\n");
    fclose(m);

    g_capture_ms = now_ms() - t0;
    g_snapshot_bytes = total;
    g_snapshot_tick = tick;
    fprintf(stderr, "snapshot: captured T=%d -> '%s' (%u bytes across 6 components + manifest, "
                    "%.2f ms; eip=0x%08lx esp=0x%08lx x87cw=0x%04x ctxflags=0x%08lx)\n",
            tick, dir, (unsigned)total, g_capture_ms, (unsigned long)ctx->Eip,
            (unsigned long)ctx->Esp, (unsigned)cs.x87_cw_live, (unsigned long)ctx->ContextFlags);
    fflush(stderr);
}

// ---------------------------------------------------------------------
// Restore
// ---------------------------------------------------------------------
static void do_restore(CONTEXT* ctx) {
    double t0 = now_ms();
    const char* dir = g_opt.restore_from;

    std::vector<uint8_t> vctx, vdata, vbss, varena, vstack, vcarrier;
    std::string manifest;
    {
        std::vector<uint8_t> mraw;
        if (!read_blob(dir, "manifest.json", &mraw)) die("could not read manifest.json");
        manifest.assign((const char*)mraw.data(), mraw.size());
    }
    if (manifest.find(kFormat) == std::string::npos)
        die("manifest.json is not a portforge-win32-carrier-snapshot-v1");
    {
        size_t at = manifest.find("\"capture_pid\":");
        unsigned long cap_pid = at == std::string::npos
                                    ? 0ul : strtoul(manifest.c_str() + at + 14, nullptr, 10);
        if (cap_pid != GetCurrentProcessId()) {
            fprintf(stderr,
                "snapshot: WARNING - this snapshot was captured by pid %lu, this is pid %lu. "
                "This carrier's restore is an IN-PROCESS rewind: .data/.bss and the arena carry "
                "host-object identities (DirectDraw surface memory addresses, COM device pointers, "
                "kernel HANDLEs, an HWND) that a fresh process assigns differently, and nothing "
                "re-binds them. Expect a crash in the first frame that touches a restored "
                "host-backed BITMAP. Use --snapshot-at-tick T --restore-at-tick T2 in ONE run for a "
                "supported rewind; see carrier/NOTES.md 'Milestones 8-9'.\n",
                cap_pid, GetCurrentProcessId());
        }
    }
    if (!read_blob(dir, "context.bin", &vctx) || !read_blob(dir, "data.bin", &vdata) ||
        !read_blob(dir, "bss.bin", &vbss)     || !read_blob(dir, "arena.bin", &varena) ||
        !read_blob(dir, "stack.bin", &vstack) || !read_blob(dir, "carrier.bin", &vcarrier))
        die("could not read all snapshot components");

    // Integrity: every component's sha256 against the manifest.
    struct { const char* key; std::vector<uint8_t>* v; } checks[] = {
        {"context", &vctx}, {"data", &vdata}, {"bss", &vbss},
        {"arena", &varena}, {"stack", &vstack}, {"carrier", &vcarrier},
    };
    for (auto& c : checks) {
        std::string want;
        if (!manifest_find_sha(manifest, c.key, &want)) die("manifest.json has no sha256 for a component");
        std::string got = pf::Sha256::of(c.v->data(), c.v->size());
        if (got != want) {
            fprintf(stderr, "snapshot: component '%s' sha256 %s != manifest %s\n",
                    c.key, got.c_str(), want.c_str());
            die("snapshot component digest mismatch");
        }
    }

    if (vctx.size() != sizeof(CONTEXT)) die("context.bin has the wrong size for this build's CONTEXT");
    if (vdata.size() != D_SIZE || vbss.size() != B_SIZE)
        die(".data/.bss component sizes do not match this image's sections");
    if (vcarrier.size() != sizeof(CarrierState)) die("carrier.bin has the wrong size for this build");

    CONTEXT saved;
    memcpy(&saved, vctx.data(), sizeof(saved));
    CarrierState cs;
    memcpy(&cs, vcarrier.data(), sizeof(cs));
    if (cs.magic != PF_CARRIER_STATE_MAGIC || cs.version != PF_CARRIER_STATE_VERSION)
        die("carrier.bin magic/version mismatch");

    uintptr_t stack_top = (uintptr_t)S_VA + S_SIZE;
    uintptr_t stack_lo = (uintptr_t)saved.Esp;
    if (stack_lo + vstack.size() != stack_top)
        die("stack.bin does not end at the guest stack top");

    // SAFETY, and the one structural constraint of an in-process rewind:
    // this handler is itself running ON the guest stack (a VEH runs on the
    // interrupted thread's stack), with its frames strictly BELOW the
    // current ESP. We are about to overwrite [saved.Esp, stack_top). That
    // is safe exactly when saved.Esp >= current ESP. Both are safepoints
    // inside play() at the same call depth, so they are equal in practice
    // (measured: ESP == 0x0e1fef30 at all 876 safepoints of the milestone-7
    // workload). Refuse loudly rather than smash our own frame if that ever
    // stops holding - the fix would be to do the write-back from a scratch
    // stack, not to relax this check.
    if ((uintptr_t)saved.Esp < (uintptr_t)ctx->Esp) {
        fprintf(stderr, "snapshot: snapshot ESP 0x%08lx is BELOW the live ESP 0x%08lx - the "
                        "write-back would overwrite this exception handler's own frames\n",
                (unsigned long)saved.Esp, (unsigned long)ctx->Esp);
        die("unsafe stack write-back (see snapshot.cpp)");
    }

    HANDLE suspended[32];
    int nsusp = suspend_other_threads(suspended, 32);
    memcpy((void*)(uintptr_t)D_VA, vdata.data(), vdata.size());
    memcpy((void*)(uintptr_t)B_VA, vbss.data(), vbss.size());
    if (!varena.empty()) memcpy((void*)(uintptr_t)A_VA, varena.data(), varena.size());
    if (!vstack.empty()) memcpy((void*)stack_lo, vstack.data(), vstack.size());
    resume_threads(suspended, nsusp);

    det_state_load(&cs.det);
    bind_state_load(&cs.bind);

    // Negative control (win32_pilot.md SS7's "inject a one-byte fault into
    // one role and require the comparator to name it at that tick").
    // reward_scale (0x4fac28) is inside .bss AND inside the per-tick digest
    // scope (carrier/gen/game_globals.inc), so the very first digest line
    // written after this restore must already differ.
    if (g_opt.restore_fault) {
        volatile unsigned char* p =
            (volatile unsigned char*)(uintptr_t)icytower::kSnapshotDomain.fault_probe_va;
        *p ^= 1u;
        fprintf(stderr, "snapshot: --restore-fault fired: flipped bit0 of [0x004fac28] "
                        "(reward_scale, restored .bss) - the first difference must be named "
                        "at the restore tick itself\n");
    }

    // Resume the guest thread at the snapshot's EIP with the snapshot's
    // registers and stack. The DEBUG registers are deliberately taken from
    // the LIVE context, not the snapshot: DR0-DR3 belong to this run's
    // sensor table (which may differ from the capturing run's - a different
    // --bind, --record-input, ...), and rewinding them would disarm the very
    // safepoint breakpoint that has to keep firing. det_veh_handler then
    // sets EFLAGS.RF on the way out, so the breakpoint at the restored EIP
    // (which IS the safepoint, 0x4124f4) does not immediately re-trigger.
    DWORD dr0 = ctx->Dr0, dr1 = ctx->Dr1, dr2 = ctx->Dr2, dr3 = ctx->Dr3, dr7 = ctx->Dr7;
    DWORD live_flags = ctx->ContextFlags;
    *ctx = saved;
    ctx->Dr0 = dr0; ctx->Dr1 = dr1; ctx->Dr2 = dr2; ctx->Dr3 = dr3;
    ctx->Dr6 = 0;   ctx->Dr7 = dr7;
    ctx->ContextFlags = saved.ContextFlags | live_flags | CONTEXT_DEBUG_REGISTERS;

    g_restore_ms = now_ms() - t0;
    g_restored_tick = cs.tick;
    fprintf(stderr, "snapshot: restored T=%d from '%s' in %.2f ms "
                    "(%d other thread(s) suspended for the write-back; eip=0x%08lx esp=0x%08lx "
                    "x87cw=0x%04x, %u arena bytes, script cursor %u, rng 0x%08x)\n",
            cs.tick, dir, g_restore_ms, nsusp, (unsigned long)ctx->Eip, (unsigned long)ctx->Esp,
            (unsigned)cs.x87_cw_live, (unsigned)varena.size(), cs.det.script_cursor, cs.det.rng_state);
    fflush(stderr);

    // Milestone 9: open the trace window right where the restore lands.
    if (g_opt.trace_window > 0) {
        const char* path = (g_opt.trace_window_out && g_opt.trace_window_out[0])
                               ? g_opt.trace_window_out : "trace_window.txt";
        g_trace_file = fopen(path, "w");
        if (!g_trace_file) {
            fprintf(stderr, "snapshot: could not open --trace-window-out '%s'\n", path);
        } else {
            fprintf(g_trace_file,
                    "# instruction trace from a snapshot (carrier --trace-window)\n"
                    "# restored T=%d, %ld instructions, single-stepped with EFLAGS.TF in the VEH\n"
                    "# columns: index eip symbol | changed registers\n",
                    cs.tick, (long)g_opt.trace_window);
            g_trace_remaining = g_opt.trace_window;
            g_trace_index = 0;
            g_trace_have_prev = false;
            ctx->EFlags |= 0x100; // TF
            fprintf(stderr, "snapshot: --trace-window %ld armed -> '%s'\n",
                    (long)g_opt.trace_window, path);
        }
    }
}

// ---------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------
// Snapshot files are opened LATE - at a safepoint, long after main() has
// SetCurrentDirectory'd to assets\ and the guest's own startup chdir'd
// again (carrier/NOTES.md fix #6). Every path is therefore made absolute
// HERE, in main(), while the cwd is still the one the operator typed the
// command in, so `--snapshot-out ../artifacts_task/snap400` means what it
// looks like.
static char g_snapshot_out_abs[MAX_PATH];
static char g_restore_from_abs[MAX_PATH];
static char g_trace_out_abs[MAX_PATH];

static const char* absolutize(const char* p, char* buf, size_t n) {
    if (!p || !p[0]) return nullptr;
    if (GetFullPathNameA(p, (DWORD)n, buf, nullptr) == 0) return p;
    return buf;
}

void snapshot_init(const SnapshotOptions& opt) {
    g_opt = opt;
    g_opt.snapshot_out = absolutize(opt.snapshot_out, g_snapshot_out_abs, sizeof(g_snapshot_out_abs));
    g_opt.restore_from = absolutize(opt.restore_from, g_restore_from_abs, sizeof(g_restore_from_abs));
    g_opt.trace_window_out = absolutize(
        (opt.trace_window_out && opt.trace_window_out[0]) ? opt.trace_window_out : "trace_window.txt",
        g_trace_out_abs, sizeof(g_trace_out_abs));
    g_any = (opt.snapshot_at_tick > 0 && opt.snapshot_out && opt.snapshot_out[0]) ||
            (opt.restore_from && opt.restore_from[0]);
    if (!g_any) return;
    // --restore-at-tick without --restore-from means "rewind, in this same
    // run, to the snapshot this run just took" - the in-run rewind form.
    if ((!g_opt.restore_from || !g_opt.restore_from[0]) && g_opt.restore_at_tick > 0)
        g_opt.restore_from = g_opt.snapshot_out;
    g_in_run_rewind = g_opt.snapshot_at_tick > 0 && g_opt.snapshot_out && g_opt.restore_from &&
                      _stricmp(g_opt.snapshot_out, g_opt.restore_from) == 0;
    fprintf(stderr, "snapshot: snapshot_at_tick=%d out=%s | restore_from=%s at_tick=%d fault=%d "
                    "| trace_window=%d\n",
            g_opt.snapshot_at_tick, g_opt.snapshot_out ? g_opt.snapshot_out : "(none)",
            g_opt.restore_from ? g_opt.restore_from : "(none)", g_opt.restore_at_tick,
            (int)g_opt.restore_fault, g_opt.trace_window);
}

bool snapshot_on_safepoint_pre(CONTEXT* ctx) {
    if (!g_any || g_restore_done || !g_opt.restore_from || !g_opt.restore_from[0]) return false;
    // restore_at_tick <= 0: the cold-start form - restore at the FIRST
    // safepoint this process reaches (i.e. after the guest has been started
    // and has reached ITS OWN first safepoint, so every host object the
    // guest references already exists).
    if (g_opt.restore_at_tick > 0 && det_tick() < g_opt.restore_at_tick) return false;
    // An in-run rewind must not fire before its own snapshot was taken.
    if (g_in_run_rewind && !g_snapshot_done) return false;
    g_restore_done = true;
    do_restore(ctx);
    return true;
}

void snapshot_on_safepoint_post(CONTEXT* ctx) {
    if (!g_any || g_snapshot_done) return;
    if (g_opt.snapshot_at_tick <= 0 || !g_opt.snapshot_out || !g_opt.snapshot_out[0]) return;
    if (det_tick() < g_opt.snapshot_at_tick) return;
    g_snapshot_done = true;
    do_capture(ctx, det_tick());
}

// ---------------------------------------------------------------------
// Milestone 9: the instruction "microscope".
//
// A bounded, trap-flag single-step trace taken from a restored state. This
// is the recovery win32_pilot.md SS3's "what native execution gives up"
// table promises for instruction-level tracing ("trap-flag stepping through
// a VEH over a bounded window"). Symbol names come from symbols_describe,
// i.e. artifacts/functions.json - the same resolver the crash handler uses,
// so carrier-side addresses (0x10000000+) are visibly nonsense guest names,
// which is itself the tell that the trace left the guest.
// ---------------------------------------------------------------------
bool snapshot_trace_active() { return g_trace_remaining > 0 && g_trace_file != nullptr; }

void snapshot_trace_step(CONTEXT* ctx) {
    if (!snapshot_trace_active()) return;
    char where[192];
    pf::win32::symbols_describe((unsigned long)ctx->Eip, where, sizeof(where));
    fprintf(g_trace_file, "%6ld %08lx %s", g_trace_index, (unsigned long)ctx->Eip, where);
    if (g_trace_have_prev) {
        struct { const char* n; DWORD a, b; } r[] = {
            {"eax", g_trace_prev.Eax, ctx->Eax}, {"ebx", g_trace_prev.Ebx, ctx->Ebx},
            {"ecx", g_trace_prev.Ecx, ctx->Ecx}, {"edx", g_trace_prev.Edx, ctx->Edx},
            {"esi", g_trace_prev.Esi, ctx->Esi}, {"edi", g_trace_prev.Edi, ctx->Edi},
            {"ebp", g_trace_prev.Ebp, ctx->Ebp}, {"esp", g_trace_prev.Esp, ctx->Esp},
        };
        const char* sep = " |";
        for (auto& x : r)
            if (x.a != x.b) { fprintf(g_trace_file, "%s %s=%08lx", sep, x.n, (unsigned long)x.b); sep = ""; }
    }
    fputc('\n', g_trace_file);
    g_trace_prev = *ctx;
    g_trace_have_prev = true;
    ++g_trace_index;
    if (--g_trace_remaining > 0) {
        ctx->EFlags |= 0x100;   // keep stepping
    } else {
        ctx->EFlags &= ~0x100u; // window closed - full speed again
        fprintf(g_trace_file, "# window closed after %ld instructions\n", g_trace_index);
        fflush(g_trace_file);
        fclose(g_trace_file);
        g_trace_file = nullptr;
        fprintf(stderr, "snapshot: --trace-window closed after %ld instructions\n", g_trace_index);
    }
}

void snapshot_report_json(FILE* f) {
    if (!g_any) return;
    fprintf(f, "  \"snapshot\": {\n");
    fprintf(f, "    \"captured_tick\": %d,\n", g_snapshot_tick);
    fprintf(f, "    \"captured_bytes\": %u,\n", (unsigned)g_snapshot_bytes);
    fprintf(f, "    \"capture_ms\": %.3f,\n", g_capture_ms);
    fprintf(f, "    \"restored_tick\": %d,\n", g_restored_tick);
    fprintf(f, "    \"restore_ms\": %.3f,\n", g_restore_ms);
    fprintf(f, "    \"trace_window_instructions\": %ld\n", g_trace_index);
    fprintf(f, "  },\n");
}

void snapshot_shutdown() {
    if (g_trace_file) {
        fprintf(g_trace_file, "# run ended with %ld instruction(s) of the window unused\n", g_trace_remaining);
        fflush(g_trace_file);
        fclose(g_trace_file);
        g_trace_file = nullptr;
    }
}
