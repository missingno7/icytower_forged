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
    // SRC form (item 1, "src binding..." pass): src/icytower/update_frame.c
    // and is_solid.c, compiled straight into the carrier with
    // carrier/gen/pf_bindings_src.h force-included (see build.cmd), which
    // is what leaves their OWN plain names (`update_frame`, `is_solid`)
    // free instead of macro-redirecting them to their original address
    // (BINDINGS_NOTES.md "Exclusion"). Declared here with void*/int the same
    // way native_is_solid is above (never Tmap*) for the identical reason
    // cited at this file's own header comment: this TU cannot see the
    // carrier's Tmap (it_types.h, via windows.h-colliding headers) or
    // src/'s own Tmap (game_types.h) without pulling in one or the other's
    // transitive dependencies; the mismatch is harmless because C linkage
    // only matches by name, never by parameter type across translation
    // units, and native_is_solid already proves the pattern works.
    void __cdecl update_frame(void);
    int  __cdecl is_solid(void* m, int cx, int cy);
    // src/ form for jump_player (line_intersect.c/jump_player.c/new_rand.c/
    // particle.c are compiled with mingw32 GCC -m32 -mfpmath=387 -mno-sse2
    // for x87-faithful codegen and linked straight into carrier.exe as
    // ordinary COFF objects alongside the MSVC ones - see build.cmd "GCC x87
    // objects" and carrier/NOTES.md "Milestone 12 at scale" SS3 for how the
    // mixed-toolchain link was verified safe: both are 32-bit cdecl COFF,
    // GCC's `_name` decoration matches MSVC's exactly, and MSVC's link.exe
    // accepts the .o files with no wrapping needed).
    int    __cdecl jump_player(void* p, int cheat);
    int    __cdecl line_intersect(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, int* px_out, int* py_out);
    int    __cdecl new_rand(void);
    void   __cdecl update_particle(void* p);
    int    __cdecl create_particle(void* p, int x, int y);
    // Remaining 30 src/ forms - all pure integer, compiled by MSVC with the
    // same /FIpf_bindings_src.h step as update_frame/is_solid (build.cmd).
    void       __cdecl getFloorData(void* m, int cy, int* fy, int* fx1, int* fx2);
    void       __cdecl reset_map(void* m);
    int        __cdecl get_level(void* m, int cy);
    void       __cdecl add_combo(void* gd, void* c);
    void       __cdecl add_jump_sequence(void* gd, void* js);
    void*      __cdecl get_gamepad(void);
    int        __cdecl is_up(void* c);
    int        __cdecl is_down(void* c);
    int        __cdecl is_left(void* c);
    int        __cdecl is_right(void* c);
    int        __cdecl is_fire(void* c);
    int        __cdecl is_pause(void* c);
    int        __cdecl is_enter(void* c);
    int        __cdecl is_any(void* c);
    void       __cdecl set_control(void* c, int up, int down, int left, int right, int fire);
    void       __cdecl init_control(void* c);
    int        __cdecl check_control_key(void* c, int key);
    void       __cdecl reset_particles(void* p);
    void       __cdecl scroll_scroller(void* sc, int step);
    void       __cdecl restart_scroller(void* sc);
    void       __cdecl cycle_counter(void);
    void       __cdecl fps_counter(void);
    void*      __cdecl get_demo(void);
    void*      __cdecl get_controls(void);
    void       __cdecl switchedFromProgram(void);
    void       __cdecl switchedToProgram(void);
    void       __cdecl clickedCloseButton(void);
    int        __cdecl ok_to_play(void);
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

const unsigned kMaxFns = 35;  // == number of BIND_STUB(N) definitions below
                               // (Milestone 12 at scale: all 35 src/icytower
                               // functions bound at once, win32_pilot.md SS8a)
const unsigned kMaxArgs = 10; // widest real cdecl arity among the 35: line_intersect
                               // (x1,y1,x2,y2,x3,y3,x4,y4,px_out,py_out). The
                               // milestone-11 stub always re-pushed exactly 4
                               // dwords "regardless of the real arity" - MEASURED
                               // this pass that set_control (6 args) and
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
    void*       src;       // nullptr = no src/ form exists yet (item 1)
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

// --- additional guest globals/sizes for the 32 functions added this pass
// (carrier/NOTES.md "Milestone 12 at scale") - every VA/size cited to
// carrier/gen/it_globals.h / it_types_check.c exactly like the block above.
const unsigned VA_gamepad            = 0x4f8748u; // it_globals.h:244  Tgamepad
const unsigned SZ_Tgamepad           = 144u;      // it_types_check.c:153
const unsigned VA_ctrl               = 0x5000c8u; // it_globals.h:128  Tcontrol
const unsigned SZ_Tcontrol           = 36u;       // it_types_check.c:143
const unsigned VA_demo               = 0x4dd250u; // it_globals.h:163  Treplay*
const unsigned VA_hasFocus           = 0x4bc020u; // it_globals.h:284  int
const unsigned VA_closeButtonClicked = 0x4dd264u; // it_globals.h:100  int
const unsigned VA_cycle_count        = 0x506938u; // it_globals.h:148  volatile int
const unsigned VA_frame_count        = 0x506978u; // it_globals.h:208  volatile int
const unsigned VA_logic_count        = 0x506958u; // it_globals.h:364  volatile int
const unsigned VA_fps                = 0x506948u; // it_globals.h:203  volatile int
const unsigned VA_lps                = 0x506968u; // it_globals.h:367  volatile int
const unsigned VA_seed               = 0x4ff108u; // it_globals.h:547  double
// Tgame_data field offsets (it_types_check.c:266-278); gd is args[0].
const unsigned OFF_comboPosts = 64u,    OFF_combos = 68u,    SZ_Tgd_combo = 12u;
const unsigned OFF_jumpPosts  = 60068u, OFF_jumps  = 60072u, SZ_Tgd_jump_sequence = 12u;
// Tscroller.offset (it_types_check.c:703); the only field
// scroll_scroller/restart_scroller ever write.
const unsigned OFF_Tscroller_offset = 24u;
// Tparticle (it_types_check.c:746); reset_particles/create_particle's `p`
// argument is the base of a fixed 512-element array (particle.c's own
// header comment), update_particle's is a single element.
const unsigned SZ_Tparticle       = 24u;
const unsigned SZ_Tparticle_array = 24u * 512u; // 12288

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

// --- domains for the 32 functions added this pass (Milestone 12 at scale).
// Each domain restates PROMOTIONS.md's own description of what the function
// reads/writes (its own source header comment, cited per function below).

// getFloorData(Tmap *m, int cy, int *fy, int *fx1, int *fx2): writes into
// the Tmap AND into three caller-owned int outputs (map.c's own comment) -
// the domain covers both, same "whole Tmap" convention dom_is_solid uses
// plus the three scalar outputs.
void dom_getFloorData(pf::Sha256& s, const unsigned* args) {
    hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tmap);
    hash_region(s, (const void*)(uintptr_t)args[2], 4);
    hash_region(s, (const void*)(uintptr_t)args[3], 4);
    hash_region(s, (const void*)(uintptr_t)args[4], 4);
}
void* fa_getFloorData(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

void dom_reset_map(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tmap); }
void* fa_reset_map(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

void dom_get_level(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tmap); }
void* fa_get_level(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

// add_combo/add_jump_sequence (game_data.c): each writes a counter field and
// (if not full) the slot the counter now indexes - add_combo.c/
// add_jump_sequence.c's own header comments. The domain reads the counter
// AFTER the call to find which slot to hash, exactly like dom_update_frame
// reads player_id to find which Tplayer to hash.
void dom_add_combo(pf::Sha256& s, const unsigned* args) {
    const unsigned char* gd = (const unsigned char*)(uintptr_t)args[0];
    hash_region(s, gd + OFF_comboPosts, 4);
    unsigned cp = 0;
    if (readable(gd + OFF_comboPosts, 4)) cp = *(const unsigned*)(gd + OFF_comboPosts);
    if (cp >= 1u && cp <= 5000u) hash_region(s, gd + OFF_combos + (cp - 1u) * SZ_Tgd_combo, SZ_Tgd_combo);
}
void* fa_add_combo(const unsigned* args) { return (void*)((uintptr_t)args[0] + OFF_comboPosts); }

void dom_add_jump_sequence(pf::Sha256& s, const unsigned* args) {
    const unsigned char* gd = (const unsigned char*)(uintptr_t)args[0];
    hash_region(s, gd + OFF_jumpPosts, 4);
    unsigned jp = 0;
    if (readable(gd + OFF_jumpPosts, 4)) jp = *(const unsigned*)(gd + OFF_jumpPosts);
    if (jp >= 1u && jp <= 5000u) hash_region(s, gd + OFF_jumps + (jp - 1u) * SZ_Tgd_jump_sequence, SZ_Tgd_jump_sequence);
}
void* fa_add_jump_sequence(const unsigned* args) { return (void*)((uintptr_t)args[0] + OFF_jumpPosts); }

// get_gamepad()/get_controls(): "return &<fixed global>" (control.c/
// main_state.c) - the return value is a compile-time-constant address, so
// EAX alone (already a separate compared field) is the whole story; an
// empty domain is correct, not an oversight (matches ok_to_play's own
// documented "no domain worth adding" precedent, PROMOTIONS.md).
void dom_empty(pf::Sha256&, const unsigned*) {}
void* fa_none(const unsigned*) { return nullptr; }

void dom_is_ctrl_flag(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tcontrol); }
void* fa_ctrl(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

void dom_set_control(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tcontrol); }
void dom_init_control(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tcontrol); }
void dom_check_control_key(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tcontrol); }

void dom_reset_particles(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tparticle_array); }
void* fa_particle_array(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

// update_particle(Tparticle *p): one particle (24 bytes) PLUS the game's
// RNG seed (particle.c's own comment: "1 time in 5 ... rerolls color",
// via up to two new_rand() calls) - seed is a real side effect this
// function's forms must also agree on.
void dom_update_particle(pf::Sha256& s, const unsigned* args) {
    hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tparticle);
    hash_region(s, (const void*)VA_seed, 8);
}
void* fa_update_particle(const unsigned* args) { return (void*)(uintptr_t)args[0]; }

// create_particle(Tparticle *p, int x, int y): scans all 512 slots (only
// one, if any, is written - particle.c's own comment), so the whole array
// is hashed rather than guessing the slot from EAX (EAX==0 is ambiguous
// between "no free slot" and "found slot 0", a pre-existing game quirk, not
// something this domain needs to resolve) - plus seed, same reason as
// update_particle (up to three new_rand() draws).
void dom_create_particle(pf::Sha256& s, const unsigned* args) {
    hash_region(s, (const void*)(uintptr_t)args[0], SZ_Tparticle_array);
    hash_region(s, (const void*)VA_seed, 8);
}

void dom_scroller_offset(pf::Sha256& s, const unsigned* args) { hash_region(s, (const void*)((uintptr_t)args[0] + OFF_Tscroller_offset), 4); }
void* fa_scroller(const unsigned* args) { return (void*)((uintptr_t)args[0] + OFF_Tscroller_offset); }

void dom_cycle_counter(pf::Sha256& s, const unsigned*) { hash_region(s, (const void*)VA_cycle_count, 4); }
void* fa_cycle_counter(const unsigned*) { return (void*)VA_cycle_count; }

void dom_fps_counter(pf::Sha256& s, const unsigned*) {
    hash_region(s, (const void*)VA_fps, 4);
    hash_region(s, (const void*)VA_frame_count, 4);
    hash_region(s, (const void*)VA_lps, 4);
    hash_region(s, (const void*)VA_logic_count, 4);
}
void* fa_fps_counter(const unsigned*) { return (void*)VA_fps; }

void dom_get_demo(pf::Sha256& s, const unsigned*) { hash_region(s, (const void*)VA_demo, 4); }

void dom_focus_flag_from(const unsigned va, pf::Sha256& s) { hash_region(s, (const void*)(uintptr_t)va, 4); }
void dom_switchedFromProgram(pf::Sha256& s, const unsigned*) { dom_focus_flag_from(VA_hasFocus, s); }
void dom_switchedToProgram(pf::Sha256& s, const unsigned*) { dom_focus_flag_from(VA_hasFocus, s); }
void* fa_hasFocus(const unsigned*) { return (void*)VA_hasFocus; }
void dom_clickedCloseButton(pf::Sha256& s, const unsigned*) { hash_region(s, (const void*)VA_closeButtonClicked, 4); }
void* fa_closeButtonClicked(const unsigned*) { return (void*)VA_closeButtonClicked; }

// ok_to_play(): "return 1" unconditionally, no globals - PROMOTIONS.md's
// own note: "negative control not attempted ... there is none here to
// flip." Empty domain, no fault address (fa_none), matching that.
void dom_new_rand(pf::Sha256& s, const unsigned*) { hash_region(s, (const void*)VA_seed, 8); }
void* fa_new_rand(const unsigned*) { return (void*)VA_seed; }

// line_intersect(...): touches nothing but its own two int* outputs
// (line_intersect.c's own header comment: "the function's only two stores
// outside its own frame are exactly those two ... the comparison domain is
// provably complete"), args[8]=px_out, args[9]=py_out.
void dom_line_intersect(pf::Sha256& s, const unsigned* args) {
    hash_region(s, (const void*)(uintptr_t)args[8], 4);
    hash_region(s, (const void*)(uintptr_t)args[9], 4);
}
void* fa_line_intersect(const unsigned* args) { return (void*)(uintptr_t)args[8]; }

const FnDesc kFns[] = {
    // name                   VA          argc ret?   lifted                      native                      src                     domain                    fault addr
    { "update_frame",         0x406ac4u,  0,   false, (void*)lifted_update_frame, (void*)native_update_frame, (void*)update_frame,     dom_update_frame,         fa_update_frame },
    { "is_solid",             0x4166dcu,  3,   true,  (void*)lifted_is_solid,     (void*)native_is_solid,     (void*)is_solid,         dom_is_solid,             fa_is_solid },
    { "jump_player",          0x418678u,  2,   true,  (void*)lifted_jump_player,  nullptr,                    (void*)jump_player,      dom_jump_player,          fa_jump_player },
    // --- Milestone 12 at scale: the remaining 32 functions, src-only (no
    // lifted/native form exists for any of them; the offline harness
    // proved each one, PROMOTIONS.md, before this pass bound it in-vivo).
    { "getFloorData",         0x416770u,  5,   false, nullptr, nullptr, (void*)getFloorData,         dom_getFloorData,         fa_getFloorData },
    { "reset_map",            0x4166a4u,  1,   false, nullptr, nullptr, (void*)reset_map,            dom_reset_map,            fa_reset_map },
    { "get_level",            0x416748u,  2,   true,  nullptr, nullptr, (void*)get_level,            dom_get_level,            fa_get_level },
    { "add_combo",            0x40414cu,  2,   false, nullptr, nullptr, (void*)add_combo,            dom_add_combo,            fa_add_combo },
    { "add_jump_sequence",    0x4040f4u,  2,   false, nullptr, nullptr, (void*)add_jump_sequence,    dom_add_jump_sequence,    fa_add_jump_sequence },
    { "get_gamepad",          0x4017fcu,  0,   true,  nullptr, nullptr, (void*)get_gamepad,          dom_empty,                fa_none },
    { "is_up",                0x401844u,  1,   true,  nullptr, nullptr, (void*)is_up,                dom_is_ctrl_flag,         fa_ctrl },
    { "is_down",              0x40185cu,  1,   true,  nullptr, nullptr, (void*)is_down,              dom_is_ctrl_flag,         fa_ctrl },
    { "is_left",              0x401874u,  1,   true,  nullptr, nullptr, (void*)is_left,              dom_is_ctrl_flag,         fa_ctrl },
    { "is_right",             0x401888u,  1,   true,  nullptr, nullptr, (void*)is_right,             dom_is_ctrl_flag,         fa_ctrl },
    { "is_fire",               0x4018a0u,  1,   true,  nullptr, nullptr, (void*)is_fire,              dom_is_ctrl_flag,         fa_ctrl },
    { "is_pause",              0x4018b8u,  1,   true,  nullptr, nullptr, (void*)is_pause,             dom_is_ctrl_flag,         fa_ctrl },
    { "is_enter",              0x4018d0u,  1,   true,  nullptr, nullptr, (void*)is_enter,             dom_is_ctrl_flag,         fa_ctrl },
    { "is_any",                0x4018e8u,  1,   true,  nullptr, nullptr, (void*)is_any,               dom_is_ctrl_flag,         fa_ctrl },
    { "set_control",           0x4017d4u,  6,   false, nullptr, nullptr, (void*)set_control,          dom_set_control,          fa_ctrl },
    { "init_control",          0x401790u,  1,   false, nullptr, nullptr, (void*)init_control,         dom_init_control,         fa_ctrl },
    { "check_control_key",     0x401808u,  2,   true,  nullptr, nullptr, (void*)check_control_key,    dom_check_control_key,    fa_ctrl },
    { "reset_particles",       0x418420u,  1,   false, nullptr, nullptr, (void*)reset_particles,      dom_reset_particles,      fa_particle_array },
    { "update_particle",       0x41843cu,  1,   false, nullptr, nullptr, (void*)update_particle,      dom_update_particle,      fa_update_particle },
    { "create_particle",       0x418490u,  3,   true,  nullptr, nullptr, (void*)create_particle,      dom_create_particle,      fa_particle_array },
    { "scroll_scroller",       0x41f0c0u,  2,   false, nullptr, nullptr, (void*)scroll_scroller,      dom_scroller_offset,      fa_scroller },
    { "restart_scroller",      0x41f0d0u,  1,   false, nullptr, nullptr, (void*)restart_scroller,     dom_scroller_offset,      fa_scroller },
    { "cycle_counter",         0x41fed4u,  0,   false, nullptr, nullptr, (void*)cycle_counter,        dom_cycle_counter,        fa_cycle_counter },
    { "fps_counter",           0x41fea4u,  0,   false, nullptr, nullptr, (void*)fps_counter,          dom_fps_counter,          fa_fps_counter },
    { "get_demo",              0x40696cu,  0,   true,  nullptr, nullptr, (void*)get_demo,             dom_get_demo,             fa_none },
    { "get_controls",          0x406978u,  0,   true,  nullptr, nullptr, (void*)get_controls,         dom_empty,                fa_none },
    { "switchedFromProgram",   0x406a5cu,  0,   false, nullptr, nullptr, (void*)switchedFromProgram,  dom_switchedFromProgram,  fa_hasFocus },
    { "switchedToProgram",     0x406a6cu,  0,   false, nullptr, nullptr, (void*)switchedToProgram,    dom_switchedToProgram,    fa_hasFocus },
    { "clickedCloseButton",    0x406a7cu,  0,   false, nullptr, nullptr, (void*)clickedCloseButton,   dom_clickedCloseButton,   fa_closeButtonClicked },
    { "ok_to_play",            0x406a50u,  0,   true,  nullptr, nullptr, (void*)ok_to_play,           dom_empty,                fa_none },
    { "new_rand",              0x406984u,  0,   true,  nullptr, nullptr, (void*)new_rand,             dom_new_rand,             fa_new_rand },
    { "line_intersect",        0x406b80u,  10,  true,  nullptr, nullptr, (void*)line_intersect,       dom_line_intersect,       fa_line_intersect },
};
const int kNumFns = (int)(sizeof(kFns) / sizeof(kFns[0]));

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
    // Milestone 12 at scale: two of the 35 (update_particle, create_particle)
    // DO call another one of the 35 (new_rand) - but as a plain C symbol
    // call resolved at link time (pf_bindings_src.h leaves new_rand's own
    // name free, BINDINGS_NOTES.md "Exclusion"), never through the guest VA
    // 0x406984, so it never touches the entry patch or re-enters a stub
    // regardless of whether new_rand is itself separately bound in the same
    // run. Every other one of the 35 has 0 non-game/non-src callees
    // (PROMOTIONS.md, per-function notes). Still nothing to count through a
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
