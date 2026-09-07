// bind.cpp - see bind.hpp. Milestones 11-12 of win32_pilot.md:
//   1. binding table + 5-byte entry patch  (SS3 "original address = identity")
//   2. per-invocation sensor, uniform across ORIGINAL/LIFTED/NATIVE  (SS7)
//   3. negative control by fault injection  (SS7)
//   4. migration-map metrics in --report    (SS8a)
//
// Hand-written surface kept deliberately small: ONE asm stub template
// (bind_stub_common) parameterized by function id, ONE record writer used by
// all three forms, and ONE policy table (kFns) whose every address/size is
// cited to generated evidence below. Everything else is generated
// (carrier/gen/it_*.h) or already existed (det.cpp's breakpoint table + VEH).
//
// -------------------------------------------------------------------------
// Why this file does NOT include carrier/gen/it_types.h / it_globals.h
// (MEASURED, not a preference): it_types.h defines `BITMAP` and
// `pthread_mutex_t_` as the GUEST's (Allegro/pthreads-win32) types, which
// collide with wingdi.h's `BITMAP` the moment windows.h is also included -
// `error C2371: 'BITMAP': redefinition; different basic types`. bind.cpp is
// carrier code and needs windows.h (VirtualProtect, CONTEXT, ...), so the
// two headers cannot coexist in this translation unit. The lifted/native .c
// files have the opposite need and include the generated headers without
// windows.h, which is exactly how they are compiled here. The handful of
// addresses/sizes this file needs are therefore restated as constants, each
// with its generated source cited, and each machine-checked elsewhere:
//   sizeof(Tplayer)==184 / sizeof(Tmap)==772  -> carrier/gen/it_types_check.c
//                                                (PASS lines in
//                                                it_types_check_output.txt)
//   every VA below                            -> carrier/gen/it_globals.h,
//                                                carrier/gen/it_funcs.h
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

// The bound forms. Prototypes match carrier/gen/it_funcs.h's PFN_<name>
// exactly (pointer parameters are spelled void* here for the same header
// reason as above; they are only ever passed through, never dereferenced by
// this file with a guest type).
//   PFN_update_frame  void  (__cdecl *)()                  it_funcs.h:956
//   PFN_is_solid      int   (__cdecl *)(Tmap *, int, int)  it_funcs.h:544
//   PFN_jump_player   int   (__cdecl *)(Tplayer *, int)    it_funcs.h:552
extern "C" {
    void __cdecl lifted_update_frame(void);
    int  __cdecl lifted_is_solid(void* m, int cx, int cy);
    int  __cdecl lifted_jump_player(void* p, int forced);
    void __cdecl native_update_frame(void);
    int  __cdecl native_is_solid(void* m, int cx, int cy);
}

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

const unsigned kMaxFns = 8;   // == number of BIND_STUB(N) definitions below
const unsigned kMaxArgs = 4;  // task brief: "up to 4 argument dwords"

// Comparison domains are notes/promotion_candidates.md SS4, verbatim:
//   update_frame -> reward_time, reward_scale, ply[player_id] (Tplayer)
//   is_solid     -> EAX + the Tmap pointed to by its argument
//   jump_player  -> EAX + the Tplayer argument
// EAX is recorded as its own field on the record line rather than folded
// into the digest, so a comparator mismatch is attributable to `eax` or to
// `post` separately instead of being smeared into one hash.
typedef void (*DomainFn)(pf::Sha256& s, const unsigned* args);
typedef void* (*FaultAddrFn)(const unsigned* args);

struct FnDesc {
    const char* name;
    unsigned    va;
    int         argc;      // cdecl dword arguments (it_funcs.h prototype)
    // Does the it_funcs.h prototype return a value in EAX? MEASURED
    // consequence of getting this wrong (carrier/NOTES.md "Milestones
    // 11-12"): update_frame is `void (__cdecl *)()`, so EAX on return is
    // dead - the ORIGINAL leaves 0 there and the LIFTED form leaves 1, and
    // all four call sites (0x411af4/0x41242f/0x41462d/0x41497b in
    // artifacts/disasm.txt) overwrite or ignore EAX in their very next
    // instruction. Comparing EAX for a void function is therefore a false
    // positive, so the record writes `eax=void` and keeps the observed
    // value in an informational `raweax=` field the comparator ignores.
    bool        returns_value;
    void*       lifted;    // nullptr = this form does not exist yet
    void*       native;
    DomainFn    domain;
    FaultAddrFn fault_addr; // byte flipped by --fault-inject (lowest byte)
};

// --- guest globals used by the domains (carrier/gen/it_globals.h) --------
const unsigned VA_reward_time  = 0x4fec68u; // it_globals.h:524  int
const unsigned VA_reward_scale = 0x4fac28u; // it_globals.h:520  fixed (int32)
const unsigned VA_player_id    = 0x4fe518u; // it_globals.h:448  int
const unsigned VA_ply          = 0x4ff128u; // it_globals.h:452  Tplayer *[1000]
const unsigned SZ_Tplayer      = 184u;      // it_types_check.c:717
const unsigned SZ_Tmap         = 772u;      // it_types_check.c:714

long g_domain_read_failures = 0;

// Cheap committed-memory probe (one-entry cache: the domains hit the same
// few regions every invocation). Used so a bad/uninitialized guest pointer
// produces a loud, counted marker in the digest instead of an access
// violation inside the sensor.
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

void dom_update_frame(pf::Sha256& s, const unsigned* /*args*/) {
    hash_region(s, (const void*)VA_reward_time, 4);
    hash_region(s, (const void*)VA_reward_scale, 4);
    unsigned pid = 0;
    if (readable((const void*)VA_player_id, 4)) pid = *(const unsigned*)VA_player_id;
    unsigned slot = VA_ply + pid * 4u;
    const void* p = nullptr;
    if (pid < 1000u && readable((const void*)slot, 4)) p = *(void* const*)slot;
    hash_region(s, p, SZ_Tplayer);
}
void* fa_update_frame(const unsigned*) { return (void*)VA_reward_scale; }

void dom_is_solid(pf::Sha256& s, const unsigned* args) {
    hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tmap);
}
void* fa_is_solid(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

void dom_jump_player(pf::Sha256& s, const unsigned* args) {
    hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tplayer);
}
void* fa_jump_player(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

const FnDesc kFns[] = {
    // name           VA          argc  ret?   lifted                      native                      domain            fault addr
    { "update_frame", 0x406ac4u,  0,    false, (void*)lifted_update_frame, (void*)native_update_frame, dom_update_frame, fa_update_frame },
    { "is_solid",     0x4166dcu,  3,    true,  (void*)lifted_is_solid,     (void*)native_is_solid,     dom_is_solid,     fa_is_solid     },
    { "jump_player",  0x418678u,  2,    true,  (void*)lifted_jump_player,  nullptr,                    dom_jump_player,  fa_jump_player  },
};
const int kNumFns = (int)(sizeof(kFns) / sizeof(kFns[0]));

// ---------------------------------------------------------------------
// Per-function state
// ---------------------------------------------------------------------
enum Form { FORM_NONE = 0, FORM_ORIGINAL, FORM_LIFTED, FORM_NATIVE };
const char* form_name(int f) {
    switch (f) {
    case FORM_ORIGINAL: return "original";
    case FORM_LIFTED:   return "lifted";
    case FORM_NATIVE:   return "native";
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
    kFns[id].domain(s, args);
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
        void* p = kFns[id].fault_addr(r.args);
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
        push ebx                            // ebx is ours to use across the
        mov  ebx, [esp+4]                   // call (callee-saved by the C
                                            // callee); restored below.
        push dword ptr [esp+24]             // arg3
        push dword ptr [esp+24]             // arg2
        push dword ptr [esp+24]             // arg1
        push dword ptr [esp+24]             // arg0
        call dword ptr [g_bind_target_c + ebx*4]
        add  esp, 16                        // cdecl: caller cleans

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
BIND_STUB(0) BIND_STUB(1) BIND_STUB(2) BIND_STUB(3)
BIND_STUB(4) BIND_STUB(5) BIND_STUB(6) BIND_STUB(7)
#undef BIND_STUB

namespace {
void* const kStubs[kMaxFns] = {
    (void*)bind_stub_0, (void*)bind_stub_1, (void*)bind_stub_2, (void*)bind_stub_3,
    (void*)bind_stub_4, (void*)bind_stub_5, (void*)bind_stub_6, (void*)bind_stub_7,
};

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
    if (id < 0) die("--bind: unknown function '%s' (known: update_frame, is_solid, jump_player)", fname);
    int form;
    if (_stricmp(fform, "lifted") == 0) form = FORM_LIFTED;
    else if (_stricmp(fform, "native") == 0) form = FORM_NATIVE;
    else if (_stricmp(fform, "original") == 0) form = FORM_ORIGINAL;
    else die("--bind: unknown form '%s' for '%s' (expected lifted|native|original)", fform, fname);
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
        void* target = (g_form[id] == FORM_LIFTED) ? kFns[id].lifted : kFns[id].native;
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
    // NATIVE->ORIGINAL: a bound form calling back into an original address.
    // All three candidates are leaves (notes/promotion_candidates.md SS2:
    // imports_used=[], indirect_calls=0, no non-game callees), so this is 0
    // by construction and there is no interop call macro to count through
    // yet - reported as such rather than as a measured zero.
    fprintf(f, "    \"crossings_native_to_original\": \"not instrumented "
               "(all bound candidates are leaves; 0 by construction)\",\n");
    fprintf(f, "    \"invocations_sensed\": %ld,\n", total_rec);
    fprintf(f, "    \"domain_read_failures\": %ld,\n", g_domain_read_failures);
    fprintf(f, "    \"faults_injected\": %ld\n", g_faults_applied);
    fprintf(f, "  },\n");
}
