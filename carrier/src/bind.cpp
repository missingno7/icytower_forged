// bind.cpp - see bind.hpp. Milestones 11-12 of win32_pilot.md:
//   1. binding table + 5-byte entry patch  (SS3 "original address = identity")
//   2. per-invocation sensor, uniform across ORIGINAL/LIFTED/NATIVE  (SS7)
//   3. negative control by fault injection  (SS7)
//   4. migration-map metrics in --report    (SS8a)
//
// Hand-written surface kept deliberately small: ONE asm stub template
// (bind_stub_common) parameterized by function id, ONE record writer used by
// all three forms, ONE generic comparison-domain engine (hash_one_region() /
// fault_addr_generic(), this pass's own addition - see below), and the
// per-function POLICY DATA itself, which is entirely generated
// (carrier/gen/bind_table.inc, from carrier/gen/gen_bind_table.py) rather
// than hand-typed here. Everything else is generated (carrier/gen/it_*.h) or
// already existed (det.cpp's breakpoint table + VEH).
//
// -------------------------------------------------------------------------
// "Binding table generated" pass (2026-09-07): this file used to hand-carry
// a 35-row C++ literal (`kFns[]`) - name, VA, argc, whether the prototype
// returns a value in EAX, the lifted_/native_/src function pointers - PLUS
// one hand-written dom_<fn>()/fa_<fn>() C++ function per row encoding that
// function's comparison domain (which bytes of guest memory its forms must
// agree on) and its --fault-inject byte address. Both are now generated:
//   - carrier/gen/gen_bind_table.py scans src/icytower/*.c (via
//     scan_src_defs.py), cross-references carrier/gen/interop_index.json +
//     it_funcs_table.inc for VA/size/prototype, and carrier/build.cmd's own
//     link line for which lifted_/native_ forms are actually linked in.
//   - Every domain this file used to express as bespoke C++ turned out to
//     reduce to one of five small, generic shapes (a fixed global, an
//     argument-relative pointer, an argument-relative pointer + a byte
//     offset, the ply[player_id] double indirection update_frame uses, or
//     the "counter, then the slot it now indexes" shape add_combo/
//     add_jump_sequence use) - see RegionKind/Region below and
//     carrier/gen/fn_domains.json's own "_region_kinds". fn_domains.json is
//     hand-curated DATA (per-function region lists, not code) that
//     gen_bind_table.py turns into small `static const Region
//     regions_<fn>[]` arrays; hash_one_region()/fault_addr_generic() below
//     are the ONE engine that walks whichever array a row points at. A
//     function absent from fn_domains.json gets the SAME default this file
//     already used for a genuinely leaf function before this pass: an empty
//     region list (EAX only) and no --fault-inject target.
// carrier/gen/bind_table.inc (generated, DO-NOT-EDIT, marked as such in its
// own header) supplies the extern "C" declarations for every src/lifted/
// native symbol this file references AND the `kFns[]`/`kNumFns` table
// itself; this file contains no per-function literal beyond the asm stub's
// own fixed capacity constants (kMaxFns/kMaxArgs, which size the STUB
// TEMPLATE, not any one function's data).
// -------------------------------------------------------------------------
// Why this file does NOT include carrier/gen/it_types.h / it_globals.h
// (MEASURED, not a preference): it_types.h defines `BITMAP` and
// `pthread_mutex_t_` as the GUEST's (Allegro/pthreads-win32) types, which
// collide with wingdi.h's `BITMAP` the moment windows.h is also included -
// `error C2371: 'BITMAP': redefinition; different basic types`. bind.cpp is
// carrier code and needs windows.h (VirtualProtect, CONTEXT, ...), so the
// two headers cannot coexist in this translation unit. The lifted/native .c
// files have the opposite need and include the generated headers without
// windows.h, which is exactly how they are compiled here. Every VA/size a
// generated Region below carries came from carrier/gen/it_globals.h /
// carrier/gen/it_types_check.c via fn_domains.json (that file cites its
// sources per entry); sizeof(Tplayer)==184 / sizeof(Tmap)==772 are
// machine-checked in carrier/gen/it_types_check.c (PASS lines in
// it_types_check_output.txt).
// -------------------------------------------------------------------------
#define NOMINMAX // windows.h's min/max macros break sha256.hpp's std::min<...>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <string>
#include "bind.hpp"
#include "det.hpp"
#include "../../port_forge/src/core/sha256.hpp"

// pf_rt.h's hard-refusal hook (the LIFTED form calls it for a #DE it would
// otherwise have to invent a result for). In the carrier there is no
// "return a plausible value" option: fail loudly.
extern "C" void pf_trap(unsigned int va, const char* why) {
    fprintf(stderr, "\nbind: PF_TRAP in a LIFTED form at guest VA 0x%08x: %s\n", va, why);
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 4);
}

// ---------------------------------------------------------------------
// Policy table
// ---------------------------------------------------------------------
namespace {

const unsigned kMaxFns = kBindMaxFns;
                               // == number of BIND_STUB(N) definitions below,
                               // and >= bind_table.inc's kNumFns (currently
                               // also 42 - every function scan_src_defs.py
                               // currently finds in src/icytower/*.c that
                               // has an interop_index.json VA, "binding
                               // table generated" pass). Raise both together
                               // (mechanical, same as Milestone 12 at scale's
                               // own 8->35 bump) the day src/icytower gains
                               // a 43rd function. The value itself lives in
                               // bind.hpp (kBindMaxFns) because snapshot.cpp's
                               // BindSavedState must be exactly this wide -
                               // divergence 008 found it stuck at [8], which
                               // made bind_state_save/load overrun it.
const unsigned kMaxArgs = 10; // widest real cdecl arity among the bound
                               // functions: line_intersect
                               // (x1,y1,x2,y2,x3,y3,x4,y4,px_out,py_out). The
                               // milestone-11 stub always re-pushed exactly 4
                               // dwords "regardless of the real arity" - MEASURED
                               // Milestone 12 at scale that set_control (6 args) and
                               // getFloorData (5 args) need more than that, or
                               // the stub's fixed re-push would silently drop
                               // trailing arguments. See bind_stub_common below:
                               // the "push dword ptr [esp+24]" idiom is
                               // self-correcting (each push shifts esp, so the
                               // same literal offset walks one dword further
                               // back through the caller's frame every time),
                               // so widening it to kMaxArgs repeats needed no
                               // offset arithmetic change, only more repeats
                               // and a bigger post-call `add esp`.
                               // gen_bind_table.py refuses (FATAL) to emit a
                               // row for a function with more than kMaxArgs
                               // cdecl arguments, so this constant and the
                               // generated table can never silently drift
                               // apart.

// ---------------------------------------------------------------------
// The comparison-domain engine (this pass's own addition - see this file's
// header comment). A function's domain is a short list of "regions"; each
// region names a small, generic recipe for finding some guest bytes to hash,
// never bespoke per-function code. carrier/gen/fn_domains.json's own
// "_region_kinds" is the authoritative description of each kind below; kept
// in sync by construction, since gen_bind_table.py's region_c_init() emits
// exactly the field layout this struct expects, in this order.
// ---------------------------------------------------------------------
enum RegionKind {
    RK_GLOBAL,           // {va, len}: hash `len` bytes at the fixed VA `va`.
    RK_ARG,               // {arg_index, len}: hash `len` bytes at *args[arg_index].
    RK_ARG_OFFSET,        // {arg_index, offset, len}: hash `len` bytes at args[arg_index]+offset.
    RK_PLAYER_INDIRECT,   // {va=table_va, extra=index_va, extra2=max_index, len}:
                           // idx = *(unsigned*)index_va; if idx<max_index, hash
                           // `len` bytes at *(void**)(table_va + idx*4).
    RK_COUNTER_INDEXED,   // {arg_index, offset=counter_offset, len=elem_size,
                           // extra=table_offset, extra2=max_index}: hash 4
                           // bytes at args[arg_index]+counter_offset (the
                           // counter itself); if its value cnt is in
                           // [1,max_index], also hash elem_size bytes at
                           // args[arg_index]+table_offset+(cnt-1)*elem_size.
};

struct Region {
    RegionKind kind;
    unsigned   va;         // RK_GLOBAL: the VA. RK_PLAYER_INDIRECT: table_va.
    int        arg_index;  // RK_ARG / RK_ARG_OFFSET / RK_COUNTER_INDEXED.
    unsigned   offset;     // RK_ARG_OFFSET: byte offset. RK_COUNTER_INDEXED: counter_offset.
    unsigned   len;        // region length in bytes (RK_COUNTER_INDEXED: elem_size).
    unsigned   extra;      // RK_PLAYER_INDIRECT: index_va. RK_COUNTER_INDEXED: table_offset.
    unsigned   extra2;     // RK_PLAYER_INDIRECT / RK_COUNTER_INDEXED: max_index.
};

struct FnDesc {
    const char*   name;
    unsigned      va;
    int           argc;      // cdecl dword arguments (it_funcs.h prototype)
    // Does the it_funcs.h prototype return a value in EAX? MEASURED
    // consequence of getting this wrong (carrier/NOTES.md "Milestones
    // 11-12"): update_frame is `void (__cdecl *)()`, so EAX on return is
    // dead - the ORIGINAL leaves 0 there and the LIFTED form leaves 1, and
    // all four call sites overwrite or ignore EAX in their very next
    // instruction. Comparing EAX for a void function is therefore a false
    // positive, so the record writes `eax=void` and keeps the observed
    // value in an informational `raweax=` field the comparator ignores.
    bool          returns_value;
    void*         lifted;    // nullptr = this form does not exist yet
    void*         native;
    void*         src;
    const Region* regions;      // nullptr/num_regions==0 = empty domain (EAX only)
    int           num_regions;
    int           fault_region; // index into `regions` --fault-inject targets, or -1
};

// carrier/gen/bind_table.inc (generated by carrier/gen/gen_bind_table.py):
// the extern "C" declarations for every src/lifted/native symbol below, the
// per-function `regions_<fn>[]` data arrays, and `kFns[]`/`kNumFns`
// themselves. See that file's own header for exactly where each field comes
// from; see carrier/gen/fn_domains.json for the hand-curated domain data.
#include "../gen/bind_table.inc"

// The generated table is the authority on how many rows exist; kMaxFns
// (== bind.hpp's kBindMaxFns, which also sizes BindSavedState's per-function
// counter arrays) is the hand-written capacity that must cover it. Before
// divergence 008 nothing checked this, and BindSavedState was 34 slots too
// narrow - a silent out-of-bounds write on every snapshot/restore. Now the
// day src/icytower gains a 43rd bindable function, the build stops here.
static_assert(kNumFns <= (int)kMaxFns,
              "gen/bind_table.inc has more rows than bind.hpp's kBindMaxFns: "
              "raise kBindMaxFns and add matching BIND_STUB(N) definitions");

// Cheap committed-memory probe (one-entry cache: the domains hit the same
// few regions every invocation). Used so a bad/uninitialized guest pointer
// produces a loud, counted marker in the digest instead of an access
// violation inside the sensor.
long g_domain_read_failures = 0;

bool readable(const void* p, size_t n) {
    static const char* cache_lo = nullptr;
    static const char* cache_hi = nullptr;
    const char* q = (const char*)p;
    if (!q) return false;
    if (q >= cache_lo && q + n <= cache_hi) return true;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    cache_lo = (const char*)mbi.BaseAddress;
    cache_hi = cache_lo + mbi.RegionSize;
    return q + n <= cache_hi;
}

void hash_region(pf::Sha256& s, const void* p, size_t n) {
    if (readable(p, n)) { s.update(p, n); return; }
    ++g_domain_read_failures;
    // Fail loudly but deterministically: a fixed marker plus the offending
    // pointer, so the record differs visibly instead of silently matching.
    static const char kMarker[] = "PF_DOMAIN_UNREADABLE";
    s.update(kMarker, sizeof(kMarker));
    unsigned v = (unsigned)(uintptr_t)p;
    s.update(&v, sizeof(v));
}

// The one engine every region kind funnels through - see RegionKind's own
// comment for what each kind means.
void hash_one_region(pf::Sha256& s, const Region& r, const unsigned* args) {
    switch (r.kind) {
    case RK_GLOBAL:
        hash_region(s, (const void*)(uintptr_t)r.va, r.len);
        break;
    case RK_ARG:
        hash_region(s, (const void*)(uintptr_t)args[r.arg_index], r.len);
        break;
    case RK_ARG_OFFSET:
        hash_region(s, (const void*)((uintptr_t)args[r.arg_index] + r.offset), r.len);
        break;
    case RK_PLAYER_INDIRECT: {
        unsigned idx = 0;
        if (readable((const void*)(uintptr_t)r.extra, 4)) idx = *(const unsigned*)(uintptr_t)r.extra;
        const void* p = nullptr;
        unsigned slot = r.va + idx * 4u;
        if (idx < r.extra2 && readable((const void*)(uintptr_t)slot, 4)) p = *(void* const*)(uintptr_t)slot;
        hash_region(s, p, r.len);
        break;
    }
    case RK_COUNTER_INDEXED: {
        const unsigned char* base = (const unsigned char*)(uintptr_t)args[r.arg_index];
        hash_region(s, base + r.offset, 4);
        unsigned cnt = 0;
        if (readable(base + r.offset, 4)) cnt = *(const unsigned*)(base + r.offset);
        if (cnt >= 1u && cnt <= r.extra2) hash_region(s, base + r.extra + (cnt - 1u) * r.len, r.len);
        break;
    }
    }
}

void domain_hash(int id, pf::Sha256& s, const unsigned* args) {
    const FnDesc& d = kFns[id];
    for (int i = 0; i < d.num_regions; ++i) hash_one_region(s, d.regions[i], args);
}

// The --fault-inject byte address: the base of the function's designated
// fault region (fn_domains.json's own "fault_region", default 0 = the
// first region; -1 = no fault target, matching this file's pre-existing
// "empty domain, no fault address" precedent for a leaf function).
void* fault_addr_generic(int id, const unsigned* args) {
    const FnDesc& d = kFns[id];
    if (d.fault_region < 0 || d.fault_region >= d.num_regions) return nullptr;
    const Region& r = d.regions[d.fault_region];
    switch (r.kind) {
    case RK_GLOBAL:         return (void*)(uintptr_t)r.va;
    case RK_ARG:             return (void*)(uintptr_t)args[r.arg_index];
    case RK_ARG_OFFSET:      return (void*)((uintptr_t)args[r.arg_index] + r.offset);
    case RK_COUNTER_INDEXED: return (void*)((uintptr_t)args[r.arg_index] + r.offset);
    case RK_PLAYER_INDIRECT: return nullptr; // not used as a fault target today
    }
    return nullptr;
}

// ---------------------------------------------------------------------
// Per-function state
// ---------------------------------------------------------------------
enum Form { FORM_NONE = 0, FORM_ORIGINAL, FORM_LIFTED, FORM_NATIVE, FORM_SRC };
// Display-only labels (report JSON, --fn-digest-out's form= field, stderr
// confirmation lines): item 1 of "src binding, tick-boundary real input,
// parked timer thread" (carrier/NOTES.md) makes src/ (win32_pilot.md SS7a's
// address-free clean port) the authoritative NATIVE form going forward.
// carrier/native/*.c (FORM_NATIVE) is kept working for backward
// compatibility - the CLI keyword `native` still binds to it unchanged -
// but every place this carrier prints the form now spells it out as
// "native(transitional)" so nothing looks like it is the authoritative
// native form when it is not. compare_fn_digests.py never compares this
// string (form is deliberately excluded from the verdict), so changing the
// label cannot affect any EQUAL/DIFFERENT result.
const char* form_name(int f) {
    switch (f) {
    case FORM_ORIGINAL: return "original";
    case FORM_LIFTED:   return "lifted";
    case FORM_NATIVE:   return "native(transitional)";
    case FORM_SRC:      return "src";
    default:            return "unbound";
    }
}

int   g_form[kMaxFns];        // FORM_NONE = not selected at all
long  g_crossings[kMaxFns];   // ORIGINAL -> <bound form> entries through the patch
long  g_invocations[kMaxFns]; // k counter (all forms)
long  g_records[kMaxFns];     // complete pre+post records written
unsigned char g_orig_bytes[kMaxFns][5];
bool  g_patched[kMaxFns];

FILE* g_rec = nullptr;
int   g_fault_id = -1;
long  g_fault_k = -1;
long  g_faults_applied = 0;
bool  g_any = false;

// ---------------------------------------------------------------------
// The record: identical shape for all three forms.
// ---------------------------------------------------------------------
struct Rec {
    int      id;
    long     k;
    int      T;
    unsigned args[kMaxArgs];
    char     pre[65];
};
// Depth 8: these are leaf functions (notes/promotion_candidates.md SS2 -
// zero non-game callees, zero indirect calls), so the real depth is 1. The
// stack exists so a re-entrant/nested call is a loud overflow rather than a
// silently overwritten record.
Rec  g_recstack[8];
int  g_depth = 0;

void digest_domain(int id, const unsigned* args, char out[65]) {
    pf::Sha256 s;
    domain_hash(id, s, args);
    std::string hex = s.hex();
    strncpy(out, hex.c_str(), 64);
    out[64] = 0;
}

void record_pre(int id, const unsigned* args) {
    if (g_depth >= (int)(sizeof(g_recstack) / sizeof(g_recstack[0]))) {
        fprintf(stderr, "bind: FATAL - sensor record stack overflow on %s "
                        "(unexpected re-entrancy)\n", kFns[id].name);
        fflush(stderr);
        TerminateProcess(GetCurrentProcess(), 5);
    }
    Rec& r = g_recstack[g_depth++];
    r.id = id;
    r.k = g_invocations[id]++;
    r.T = det_tick();
    for (unsigned i = 0; i < kMaxArgs; ++i) r.args[i] = args ? args[i] : 0u;
    if (g_rec) digest_domain(id, r.args, r.pre); else r.pre[0] = 0;
}

void record_post(int id, unsigned eax) {
    if (g_depth <= 0) return;                 // unpaired post: ignore (see sense_ret)
    Rec& r = g_recstack[--g_depth];
    if (r.id != id) {
        fprintf(stderr, "bind: FATAL - sensor pre/post mismatch (pre=%s post=%s)\n",
                kFns[r.id].name, kFns[id].name);
        fflush(stderr);
        TerminateProcess(GetCurrentProcess(), 5);
    }
    // Negative control (win32_pilot.md SS7): the flip happens AFTER the
    // bound form ran and BEFORE the post digest is taken, so the comparator
    // must name exactly this k with field=post - and, because the flipped
    // byte is a game-owned global inside the per-tick digest scope
    // (carrier/gen/game_globals.inc), the per-tick global digest must first
    // differ at exactly this record's T.
    if (id == g_fault_id && r.k == g_fault_k) {
        void* p = fault_addr_generic(id, r.args);
        if (readable(p, 1)) {
            *(volatile unsigned char*)p ^= 0x01u;
            ++g_faults_applied;
            fprintf(stderr, "bind: --fault-inject fired: %s k=%ld T=%d, flipped bit0 of [0x%08x]\n",
                    kFns[id].name, r.k, r.T, (unsigned)(uintptr_t)p);
        } else {
            fprintf(stderr, "bind: FATAL - --fault-inject target for %s is unreadable\n", kFns[id].name);
            fflush(stderr);
            TerminateProcess(GetCurrentProcess(), 5);
        }
    }
    if (!g_rec) return;
    char post[65];
    digest_domain(id, r.args, post);
    fprintf(g_rec, "fn=%s k=%ld T=%d args=", kFns[id].name, r.k, r.T);
    for (int i = 0; i < kFns[id].argc; ++i)
        fprintf(g_rec, "%s%08x", i ? "," : "", r.args[i]);
    if (kFns[id].returns_value)
        fprintf(g_rec, " pre=%s post=%s eax=%08x form=%s\n",
                r.pre, post, eax, form_name(g_form[id]));
    else
        fprintf(g_rec, " pre=%s post=%s eax=void raweax=%08x form=%s\n",
                r.pre, post, eax, form_name(g_form[id]));
    fflush(g_rec);
    ++g_records[id];
}

} // namespace

// ---------------------------------------------------------------------
// The stub. ONE asm template, parameterized by function id.
//
// A bound function's original VA holds `jmp rel32` -> bind_stub_<id>, which
// pushes its id and falls into bind_stub_common. On entry to
// bind_stub_common the stack is exactly:
//     [esp+0]  id (pushed by the per-function stub)
//     [esp+4]  the ORIGINAL caller's return address
//     [esp+8]  arg0, [esp+12] arg1, ...   (cdecl, caller-cleaned)
//
// The stub (a) counts the ORIGINAL->form crossing and captures the pre
// record, (b) calls the bound form with the arguments untouched, (c)
// captures the post record including EAX, (d) returns to the original
// caller with esp exactly where a normal `ret` would leave it and every
// callee-saved register intact.
//
// Four argument dwords are always re-pushed regardless of the real arity:
// the callees are cdecl (the caller cleans up), so extra pushed dwords are
// harmless, and re-pushing rather than tail-jumping is what makes step (c)
// - which needs EAX and the post-state - possible at all. Reading four
// dwords above the return address is a read of the caller's own committed
// frame, never a write.
// ---------------------------------------------------------------------
extern "C" void __cdecl bind_pre(int id, void* frame);
extern "C" void __cdecl bind_post(int id, void* frame, unsigned eax);
// `naked` is legal only on the DEFINITION (carrier/NOTES.md fix #1), so the
// forward declaration the stub macro jumps to is a plain one.
extern "C" void bind_stub_common();

// File scope (not the anonymous namespace) so MSVC inline asm can name it.
static void* g_bind_target_c[kMaxFns];        // filled by bind_init

// Both entry points validate `id` before indexing anything. The id is
// produced by asm, so a stack-offset mistake in the stub template shows up
// here as a named refusal instead of a wild read - which is exactly how the
// one such mistake this pass had was found (carrier/NOTES.md "Milestones
// 11-12", stub post-frame off-by-4).
static void bind_check_id(int id, const char* where) {
    if (id >= 0 && id < kNumFns) return;
    fprintf(stderr, "bind: FATAL - %s got function id %d, outside 0..%d "
                    "(the stub's stack arithmetic is wrong)\n", where, id, kNumFns - 1);
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 5);
}

extern "C" void __cdecl bind_pre(int id, void* frame) {
    bind_check_id(id, "bind_pre");
    ++g_crossings[id];
    record_pre(id, (const unsigned*)((char*)frame + 4));
}

extern "C" void __cdecl bind_post(int id, void* /*frame*/, unsigned eax) {
    bind_check_id(id, "bind_post");
    record_post(id, eax);
}

extern "C" void __declspec(naked) bind_stub_common() {
    __asm {
        // ---- (a) pre record; every register and flag preserved ----------
        pushad                              // esp -= 32
        pushfd                              // esp -= 4   (call it P)
        lea  eax, [esp+40]                  // P+40 = &retaddr  (the "frame")
        push eax
        mov  ecx, [esp+40]                  // P+36 = id
        push ecx
        call bind_pre                       // cdecl(int id, void* frame)
        add  esp, 8
        popfd
        popad                               // esp back to [id][ret][args...]

        // ---- (b) call the bound form with the original arguments --------
        // Widened from 4 to kMaxArgs(10) dwords this pass (carrier/NOTES.md
        // "Milestone 12 at scale"): set_control has 6 args and getFloorData
        // has 5, both more than the milestone-11 stub's hardcoded 4 - a
        // fixed 4-dword re-push would silently drop their trailing
        // arguments. The idiom is self-correcting: at entry (right after
        // `push ebx`) arg9 sits at [esp+48] (id=+4, ret=+8, arg0=+12, ...,
        // arg9=+12+9*4=+48); every `push` shifts esp down by 4, so the SAME
        // literal `[esp+48]` walks one dword further back through the
        // caller's frame each repeat, ending with arg0 pushed last (on top,
        // as cdecl needs) after exactly kMaxArgs(10) repeats - no offset
        // arithmetic changes for functions with fewer real args: the extra
        // high-numbered "args" are just unused dwords read from further up
        // the caller's own committed frame, same as the milestone-11
        // comment already established for the width-4 case.
        push ebx                            // ebx is ours to use across the
        mov  ebx, [esp+4]                   // call (callee-saved by the C
                                            // callee); restored below.
        push dword ptr [esp+48]             // arg9
        push dword ptr [esp+48]             // arg8
        push dword ptr [esp+48]             // arg7
        push dword ptr [esp+48]             // arg6
        push dword ptr [esp+48]             // arg5
        push dword ptr [esp+48]             // arg4
        push dword ptr [esp+48]             // arg3
        push dword ptr [esp+48]             // arg2
        push dword ptr [esp+48]             // arg1
        push dword ptr [esp+48]             // arg0
        call dword ptr [g_bind_target_c + ebx*4]
        add  esp, 40                        // cdecl: caller cleans (kMaxArgs*4)

        // ---- (c) post record; EAX (the return value) preserved -----------
        // esp here is S (= [savedebx][id][retaddr][args...]); after pushad
        // +pushfd, esp is Q = S-36, so savedebx=Q+36, id=Q+40, ret=Q+44,
        // and pushad's saved EAX = Q+32. Each `push` below shifts the frame
        // another 4 bytes, which is why the last two reads are both +48.
        pushad
        pushfd                              // call it Q
        push dword ptr [esp+32]             // Q+32 = EAX saved by pushad
        lea  ecx, [esp+48]                  // Q+44 = &retaddr
        push ecx
        mov  edx, [esp+48]                  // Q+40 = id
        push edx
        call bind_post                      // cdecl(int, void*, unsigned)
        add  esp, 12
        popfd
        popad                               // restores EAX = return value

        // ---- (d) return to the original caller --------------------------
        pop  ebx                            // caller's ebx
        add  esp, 4                         // drop id -> esp points at retaddr
        ret
    }
}

#define BIND_STUB(N)                                        \
    static void __declspec(naked) bind_stub_##N() {         \
        __asm push N                                        \
        __asm jmp bind_stub_common                          \
    }
BIND_STUB(0)  BIND_STUB(1)  BIND_STUB(2)  BIND_STUB(3)  BIND_STUB(4)
BIND_STUB(5)  BIND_STUB(6)  BIND_STUB(7)  BIND_STUB(8)  BIND_STUB(9)
BIND_STUB(10) BIND_STUB(11) BIND_STUB(12) BIND_STUB(13) BIND_STUB(14)
BIND_STUB(15) BIND_STUB(16) BIND_STUB(17) BIND_STUB(18) BIND_STUB(19)
BIND_STUB(20) BIND_STUB(21) BIND_STUB(22) BIND_STUB(23) BIND_STUB(24)
BIND_STUB(25) BIND_STUB(26) BIND_STUB(27) BIND_STUB(28) BIND_STUB(29)
BIND_STUB(30) BIND_STUB(31) BIND_STUB(32) BIND_STUB(33) BIND_STUB(34)
BIND_STUB(35) BIND_STUB(36) BIND_STUB(37) BIND_STUB(38) BIND_STUB(39)
BIND_STUB(40) BIND_STUB(41) BIND_STUB(42) BIND_STUB(43) BIND_STUB(44)
BIND_STUB(45) BIND_STUB(46) BIND_STUB(47) BIND_STUB(48) BIND_STUB(49)
BIND_STUB(50) BIND_STUB(51) BIND_STUB(52) BIND_STUB(53) BIND_STUB(54)
BIND_STUB(55) BIND_STUB(56) BIND_STUB(57) BIND_STUB(58) BIND_STUB(59)
#undef BIND_STUB

namespace {
void* const kStubs[kMaxFns] = {
    (void*)bind_stub_0,  (void*)bind_stub_1,  (void*)bind_stub_2,  (void*)bind_stub_3,  (void*)bind_stub_4,
    (void*)bind_stub_5,  (void*)bind_stub_6,  (void*)bind_stub_7,  (void*)bind_stub_8,  (void*)bind_stub_9,
    (void*)bind_stub_10, (void*)bind_stub_11, (void*)bind_stub_12, (void*)bind_stub_13, (void*)bind_stub_14,
    (void*)bind_stub_15, (void*)bind_stub_16, (void*)bind_stub_17, (void*)bind_stub_18, (void*)bind_stub_19,
    (void*)bind_stub_20, (void*)bind_stub_21, (void*)bind_stub_22, (void*)bind_stub_23, (void*)bind_stub_24,
    (void*)bind_stub_25, (void*)bind_stub_26, (void*)bind_stub_27, (void*)bind_stub_28, (void*)bind_stub_29,
    (void*)bind_stub_30, (void*)bind_stub_31, (void*)bind_stub_32, (void*)bind_stub_33, (void*)bind_stub_34,
    (void*)bind_stub_35, (void*)bind_stub_36, (void*)bind_stub_37, (void*)bind_stub_38, (void*)bind_stub_39,
    (void*)bind_stub_40, (void*)bind_stub_41, (void*)bind_stub_42, (void*)bind_stub_43, (void*)bind_stub_44,
    (void*)bind_stub_45, (void*)bind_stub_46, (void*)bind_stub_47, (void*)bind_stub_48, (void*)bind_stub_49,
    (void*)bind_stub_50, (void*)bind_stub_51, (void*)bind_stub_52, (void*)bind_stub_53, (void*)bind_stub_54,
    (void*)bind_stub_55, (void*)bind_stub_56, (void*)bind_stub_57, (void*)bind_stub_58, (void*)bind_stub_59,
};
// A short kStubs initializer would zero-fill silently (a null stub pointer =
// a crash the first time that row is bound), so pin the count too.
static_assert(sizeof(kStubs) / sizeof(kStubs[0]) == kMaxFns,
              "kStubs has fewer entries than kMaxFns: add BIND_STUB(N) definitions");

// ---------------------------------------------------------------------
// ORIGINAL-form sensing: hardware breakpoints, no patched bytes.
//
// DR BUDGET (documented limit): DR0 is the tick safepoint, DR1 is the
// key_dinput_handle_scancode neutralization in script mode (det.cpp), so a
// replay run has DR2 and DR3 free - one for the function entry, one for the
// return address read from [esp] at that entry. ORIGINAL-form sensing
// therefore supports EXACTLY ONE function per run. That is enough:
// verification is per function (win32_pilot.md SS3 "lifting is per function
// and verified per function"), and the LIFTED/NATIVE forms are sensed by
// the stub, which needs no debug register at all.
// ---------------------------------------------------------------------
int g_sense_id = -1;      // the one ORIGINAL-form function, or -1
int g_slot_entry = -1;
int g_slot_ret = -1;
bool g_orig_inside = false;
unsigned g_orig_args[kMaxArgs];

void sense_entry(CONTEXT* ctx) {
    const unsigned* sp = (const unsigned*)(uintptr_t)ctx->Esp;
    if (!readable(sp, 4 + 4 * kMaxArgs)) {
        fprintf(stderr, "bind: FATAL - guest stack unreadable at the %s entry breakpoint\n",
                kFns[g_sense_id].name);
        fflush(stderr);
        TerminateProcess(GetCurrentProcess(), 5);
    }
    unsigned retaddr = sp[0];               // cdecl: [esp] = return address
    for (unsigned i = 0; i < kMaxArgs; ++i) g_orig_args[i] = sp[1 + i];
    record_pre(g_sense_id, g_orig_args);
    g_orig_inside = true;
    // Arm the return-site slot by editing the CONTEXT that is about to be
    // resumed - no SetThreadContext needed (and none is possible: a thread
    // cannot set its own debug registers).
    det_ctx_arm_slot(ctx, g_slot_ret, retaddr);
}

void sense_ret(CONTEXT* ctx) {
    if (!g_orig_inside) return;             // return site reached some other way
    g_orig_inside = false;
    det_ctx_disarm_slot(ctx, g_slot_ret);
    record_post(g_sense_id, (unsigned)ctx->Eax);
}

// ---------------------------------------------------------------------
// The 5-byte entry patch.
// ---------------------------------------------------------------------
bool patch_entry(int id, void* stub) {
    unsigned char* p = (unsigned char*)(uintptr_t)kFns[id].va;
    DWORD old = 0;
    // The image is mapped PAGE_EXECUTE_READWRITE today (carrier/NOTES.md
    // "TEMPORARY simplifications: Everything RWX"), so this is redundant -
    // done properly anyway so the patch keeps working when that TEMPORARY
    // is retired and .text becomes read-execute.
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) {
        fprintf(stderr, "bind: VirtualProtect(0x%08x, 5) failed gle=%lu\n",
                kFns[id].va, GetLastError());
        return false;
    }
    memcpy(g_orig_bytes[id], p, 5);
    intptr_t rel = (intptr_t)((char*)stub - (char*)(p + 5));
    if (rel != (intptr_t)(int32_t)rel) {
        fprintf(stderr, "bind: stub for %s is out of jmp rel32 range\n", kFns[id].name);
        return false;
    }
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)rel;
    DWORD tmp = 0;
    VirtualProtect(p, 5, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    g_patched[id] = true;
    return true;
}

int lookup_fn(const char* name) {
    for (int i = 0; i < kNumFns; ++i)
        if (strcmp(kFns[i].name, name) == 0) return i;
    return -1;
}

void die(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "bind: FATAL - ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(3);
}

// One `name=form` pair. Fails loudly on anything it does not understand.
void apply_one(const char* item) {
    char buf[128];
    strncpy(buf, item, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* eq = strchr(buf, '=');
    if (!eq) die("--bind item '%s' is not name=lifted|native|original", item);
    *eq = 0;
    const char* fname = buf;
    const char* fform = eq + 1;
    int id = lookup_fn(fname);
    if (id < 0) {
        fprintf(stderr, "bind: FATAL - --bind: unknown function '%s' (known: ", fname);
        for (int i = 0; i < kNumFns; ++i) fprintf(stderr, "%s%s", i ? ", " : "", kFns[i].name);
        fprintf(stderr, ")\n");
        fflush(stderr);
        exit(3);
    }
    int form;
    if (_stricmp(fform, "lifted") == 0) form = FORM_LIFTED;
    // `native` keeps pointing at carrier/native (transitional) - see
    // form_name's comment. `src` is the new, authoritative address-free
    // form (win32_pilot.md SS7a).
    else if (_stricmp(fform, "native") == 0) form = FORM_NATIVE;
    else if (_stricmp(fform, "src") == 0) form = FORM_SRC;
    else if (_stricmp(fform, "original") == 0) form = FORM_ORIGINAL;
    else die("--bind: unknown form '%s' for '%s' (expected lifted|native|src|original)", fform, fname);
    if (g_form[id] != FORM_NONE && g_form[id] != form)
        die("--bind: '%s' bound twice, to %s and %s", fname, form_name(g_form[id]), form_name(form));
    g_form[id] = form;
    g_any = true;
}

void apply_spec(const char* spec) {
    if (!spec || !spec[0]) return;
    char buf[512];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* tok = strtok(buf, ",");
    while (tok) { apply_one(tok); tok = strtok(nullptr, ","); }
}

void apply_file(const char* path) {
    if (!path || !path[0]) return;
    FILE* f = fopen(path, "r");
    if (!f) die("--bind-file '%s' could not be opened", path);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        char* end = p + strlen(p);
        while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
        if (!*p || *p == '#') continue;
        apply_one(p);
    }
    fclose(f);
}

void parse_fault(const char* spec) {
    if (!spec || !spec[0]) return;
    char buf[128];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* colon = strchr(buf, ':');
    if (!colon) die("--fault-inject '%s' is not name:k=N", spec);
    *colon = 0;
    int id = lookup_fn(buf);
    if (id < 0) die("--fault-inject: unknown function '%s'", buf);
    const char* kv = colon + 1;
    if (strncmp(kv, "k=", 2) != 0) die("--fault-inject '%s': expected 'k=N' after the colon", spec);
    g_fault_id = id;
    g_fault_k = atol(kv + 2);
    if (g_fault_k < 0) die("--fault-inject: k must be >= 0");
}

} // namespace

// ---------------------------------------------------------------------
void bind_init(const BindOptions& opt) {
    apply_spec(opt.bind_spec);
    apply_file(opt.bind_file);
    parse_fault(opt.fault_inject);

    bool want_records = opt.fn_digest_out && opt.fn_digest_out[0];
    if (!g_any && !want_records && g_fault_id < 0) return;   // fully inert

    if (g_fault_id >= 0 && g_form[g_fault_id] == FORM_NONE)
        die("--fault-inject names '%s' but it is not bound (add --bind %s=...)",
            kFns[g_fault_id].name, kFns[g_fault_id].name);

    if (want_records) {
        g_rec = fopen(opt.fn_digest_out, "w");
        if (!g_rec) die("could not open --fn-digest-out '%s'", opt.fn_digest_out);
    }

    for (int id = 0; id < kNumFns; ++id) {
        if (g_form[id] == FORM_NONE) continue;
        if (g_form[id] == FORM_ORIGINAL) {
            if (g_sense_id >= 0)
                die("only ONE function can be sensed in its ORIGINAL form per run "
                    "(DR budget: DR0=tick safepoint, DR1=key neutralization, "
                    "DR2=entry, DR3=return); '%s' and '%s' were both asked for",
                    kFns[g_sense_id].name, kFns[id].name);
            g_sense_id = id;
            continue;
        }
        void* target = nullptr;
        switch (g_form[id]) {
        case FORM_LIFTED: target = kFns[id].lifted; break;
        case FORM_NATIVE: target = kFns[id].native; break;
        case FORM_SRC:    target = kFns[id].src;    break;
        default: break;
        }
        if (!target)
            die("no %s form exists for '%s' (nothing named %s_%s is linked into the carrier)",
                form_name(g_form[id]), kFns[id].name, form_name(g_form[id]), kFns[id].name);
        g_bind_target_c[id] = target;
        if (!patch_entry(id, kStubs[id]))
            die("could not install the entry patch for '%s' at 0x%08x", kFns[id].name, kFns[id].va);
        fprintf(stderr, "bind: %s @0x%08x -> %s (5-byte jmp rel32 -> stub %d)\n",
                kFns[id].name, kFns[id].va, form_name(g_form[id]), id);
    }

    if (g_sense_id >= 0) {
        g_slot_entry = det_register_breakpoint(kFns[g_sense_id].va, sense_entry);
        g_slot_ret = det_register_breakpoint(0, sense_ret);   // armed dynamically
        if (g_slot_entry < 0 || g_slot_ret < 0)
            die("no free debug register for ORIGINAL-form sensing of '%s' "
                "(DR0=tick safepoint, DR1=key neutralization are already taken; "
                "drop --record-input, which also wants two slots)",
                kFns[g_sense_id].name);
        fprintf(stderr, "bind: %s @0x%08x -> original (DR%d entry, DR%d return)\n",
                kFns[g_sense_id].name, kFns[g_sense_id].va, g_slot_entry, g_slot_ret);
    }

    if (g_fault_id >= 0)
        fprintf(stderr, "bind: --fault-inject armed: %s k=%ld (one byte of its comparison "
                        "domain is flipped after that invocation, before its post record)\n",
                kFns[g_fault_id].name, g_fault_k);
}

void bind_shutdown() {
    if (g_rec) { fflush(g_rec); fclose(g_rec); g_rec = nullptr; }
}

// Milestone 8 (bind.hpp's BindSavedState): the sensor's own counters are
// carrier-owned state and must rewind with everything else.
void bind_state_save(BindSavedState* s) {
    memset(s, 0, sizeof(*s));
    for (unsigned i = 0; i < kMaxFns; ++i) {
        s->invocations[i] = g_invocations[i];
        s->crossings[i] = g_crossings[i];
        s->records[i] = g_records[i];
    }
    s->faults_applied = g_faults_applied;
    s->domain_read_failures = g_domain_read_failures;
}

void bind_state_load(const BindSavedState* s) {
    for (unsigned i = 0; i < kMaxFns; ++i) {
        g_invocations[i] = (long)s->invocations[i];
        g_crossings[i] = (long)s->crossings[i];
        g_records[i] = (long)s->records[i];
    }
    g_faults_applied = (long)s->faults_applied;
    g_domain_read_failures = (long)s->domain_read_failures;
}

void bind_report_json(FILE* f) {
    if (!g_any && g_sense_id < 0) return;
    // win32_pilot.md SS8a migration map.
    fprintf(f, "  \"binding\": {\n    \"functions\": [\n");
    bool first = true;
    long total_cross = 0, total_rec = 0;
    for (int id = 0; id < kNumFns; ++id) {
        if (g_form[id] == FORM_NONE) continue;
        if (!first) fprintf(f, ",\n");
        first = false;
        fprintf(f, "      { \"name\": \"%s\", \"va\": \"0x%08x\", \"form\": \"%s\", "
                   "\"entry_patched\": %s, \"crossings_original_to_form\": %ld, "
                   "\"invocations_sensed\": %ld }",
                kFns[id].name, kFns[id].va, form_name(g_form[id]),
                g_patched[id] ? "true" : "false", g_crossings[id], g_records[id]);
        total_cross += g_crossings[id];
        total_rec += g_records[id];
    }
    fprintf(f, "\n    ],\n");
    fprintf(f, "    \"crossings_original_to_bound_form\": %ld,\n", total_cross);
    // NATIVE->ORIGINAL: a bound form calling back into an original address
    // (which would re-enter its own stub, per bind.hpp's "Known gaps").
    // Milestone 12 at scale: two of the bound functions (update_particle,
    // create_particle) DO call another one (new_rand) - but as a plain C
    // symbol call resolved at link time (pf_bindings_src.h leaves a
    // promoted function's own name free, BINDINGS_NOTES.md "Exclusion"),
    // never through the guest VA 0x406984, so it never touches the entry
    // patch or re-enters a stub regardless of whether new_rand is itself
    // separately bound in the same run. Still nothing to count through a
    // typed interop call macro, so still reported as such rather than as a
    // measured zero.
    fprintf(f, "    \"crossings_native_to_original\": \"not instrumented "
               "(no bound candidate calls back into a patched original "
               "address; src-to-src calls like update_particle/"
               "create_particle -> new_rand resolve directly by symbol, "
               "never through the guest VA; 0 by construction)\",\n");
    fprintf(f, "    \"invocations_sensed\": %ld,\n", total_rec);
    fprintf(f, "    \"domain_read_failures\": %ld,\n", g_domain_read_failures);
    fprintf(f, "    \"faults_injected\": %ld\n", g_faults_applied);
    fprintf(f, "  },\n");
}
