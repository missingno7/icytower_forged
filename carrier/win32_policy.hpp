// win32_policy.hpp - EVERY Icy Tower fact the port_forge Win32 carrier
// framework needs, in one hand-written, evidence-cited data header.
//
// notes/extraction_plan.md S2: the mechanisms live in
// port_forge/src/platform/win32/*.hpp and carry no target fact; this file
// is the other half - the target facts, and nothing else. Rules for
// editing it:
//
//   * Every value carries the evidence for it on the same line or the line
//     above: a notes/ document, a NOTES.md section, a DWARF symbol name, or
//     a measured run. A value with no citation does not belong here.
//   * Where a table is GENERATED (carrier/gen/bind_table.inc,
//     game_globals.inc, import_table.inc, it_print_globals.inc) this file
//     POINTS AT the generated table. It never restates a generated value:
//     a second copy is a second thing to keep in step.
//   * Nothing here is a mechanism. If a policy value cannot be expressed
//     without also writing code, the mechanism is missing a field - fix
//     that in port_forge, or leave the unit project-side and say why in
//     notes/extraction_plan.md section 4.
//
// The JSON sibling, carrier/win32_policy.json, holds the same kind of data
// for the PYTHON tools (generators, lifter, verdict scripts). The two are
// deliberately separate files: nothing in carrier/src reads a file at run
// time, and the C++ carrier must not gain a startup dependency on a JSON
// parser just to know its own guest's ImageBase.
#pragma once

#include "../port_forge/src/platform/win32/arena.hpp"
#include "../port_forge/src/platform/win32/arg_sensor.hpp"
#include "../port_forge/src/platform/win32/focus_channel.hpp"
#include "../port_forge/src/platform/win32/frame_oracle.hpp"
#include "../port_forge/src/platform/win32/input_channel.hpp"
#include "../port_forge/src/platform/win32/policy.hpp"
#include "../port_forge/src/platform/win32/rng.hpp"
#include "../port_forge/src/platform/win32/snapshot.hpp"
#include "../port_forge/src/platform/win32/threads.hpp"
#include "../port_forge/src/platform/win32/virtual_clock.hpp"

namespace icytower {

// ---------------------------------------------------------------------
// The guest image and its address space.
//
// KNOWN (notes/binary_recon.md): icytower15.exe is PE32, ImageBase
// 0x400000, SizeOfImage 0x38c000, entry RVA 0x1110, NO base relocations,
// NO TLS directory. Because there are no relocations the image cannot run
// rebased - require_fixed_base is true, and pe_image_load fails loudly
// rather than mapping it anywhere else.
//
// The base/size are restated here (rather than read from the file) for one
// reason only: the range must be RESERVED before the file is opened. See
// port_forge/src/platform/win32/bootstrap.hpp for the measured race.
//
// The guest stack at 0x0e000000 is carrier-owned and 2 MiB
// (carrier/NOTES.md "Milestones 8-9"): a CONSTANT stack address is what
// makes the snapshot's stack component comparable between two runs - the
// measured ESP at all 876 safepoints of the G1 workload is 0x0e1fef30.
inline constexpr pf::win32::GuestImagePolicy kGuestImage = {
    /* image_base         */ 0x00400000ul,
    /* size_of_image      */ 0x0038c000ul,
    /* require_fixed_base */ true,
    /* apply_relocations  */ false,
    /* stack_va           */ 0x0e000000ul,
    /* stack_size         */ 2ul * 1024ul * 1024ul,
};

// Environment variable marking the reserved child process
// (port_forge .../bootstrap.hpp reason 1). Project-owned so two different
// carriers on one host cannot confuse each other's children.
inline constexpr const char* kChildMarkerEnv = "PF_CHILD";

// ---------------------------------------------------------------------
// Sidecar DLLs: imports that are NOT on the system search path.
//
// KNOWN (notes/binary_recon.md): libpng3.dll and pthreadGC2.dll ship in
// assets\ next to the game exe rather than anywhere on the system DLL
// search path, so they need an explicit path. zlib1.dll also lives there
// but is never a *direct* import of icytower15.exe (only a dependency of
// libpng3.dll) - the SetDllDirectoryA the framework does with `assets_dir`
// covers it, which is exactly why that field exists.
//
// DDRAW.dll is assets\ddraw.dll, cnc-ddraw's DirectDraw-compatibility
// shim - the one the user installed to run the original on Windows 11.
// src/imports.cpp flips this one row to System for --ddraw=system.
inline constexpr int kSidecarCount = 3;
inline constexpr pf::win32::SidecarDll kSidecars[kSidecarCount] = {
    { "DDRAW.dll",      pf::win32::SidecarDll::AssetsDir, nullptr },
    { "libpng3.dll",    pf::win32::SidecarDll::AssetsDir, nullptr },
    { "pthreadGC2.dll", pf::win32::SidecarDll::AssetsDir, nullptr },
};

// ---------------------------------------------------------------------
// The deterministic heap arena's placement.
//
// 0x20000000 is free in this process and stays free: it is above the guest
// image [0x400000, 0x78c000), above the fixed guest stack at 0x0e000000,
// and above carrier.exe itself (build.cmd links it /BASE:0x10000000, ~1.2
// MB), so [0x20000000, 0x30000000) clears all three with a wide margin. It
// is the third of the three ranges carrier/win32_policy.json's purity gate
// names as guest address space.
//
// 256 MiB is sized by measurement, not by guess: the human_test workload's
// arena high-water mark is ~29.7 MB (28.32 MB live at peak) with the menu
// churn divergence 004 documents, and the bump region has to absorb
// fragmentation on top of that.
inline constexpr pf::win32::ArenaPolicy kArena = {
    /* base_va */ 0x20000000ul,
    /* size    */ 256ul * 1024ul * 1024ul,
    /* align   */ 16u,
};

// The guest links a msvcrt-family CRT (notes/binary_recon.md: mingw gcc
// 4.4.1 against msvcrt.dll), so the pinned generator is msvcrt's own LCG
// with its documented pre-srand state of 1. --rng-selftest verifies that
// against the REAL msvcrt.dll rand() over 5000 values before the guest
// starts, so this is a checked claim rather than an assumption.
inline constexpr pf::win32::RngPolicy kRng = {
    pf::win32::RngPolicy::MsvcrtLcg,
    /* seed_default */ 1u,
};

// ---------------------------------------------------------------------
// The tick pump.
//
// KNOWN (artifacts/functions.json + artifacts/disasm.txt, cited in
// carrier/NOTES.md "Milestones 5-7"): 0x45d6c8 is Allegro 4.4's
// timer.c:_handle_timer_tick(int interval) - it takes the DELTA in timer
// units since the last call, which is exactly TickPolicy's
// AccumulatedUnits.
//
// 1193181 is the PC interval-timer frequency Allegro's own timer.h fixes as
// TIMERS_PER_SECOND. It is not inferred from the header: tim_win32_high_
// perf_thread's disassembly multiplies QPC-elapsed time by the literal
// 0x1234dd == 1193181 before calling _handle_timer_tick.
//
// 20 ms per carrier tick is this project's chosen coordinate granularity
// (50 Hz), the unit every digest line, input event and snapshot anchor in
// carrier/NOTES.md is keyed by.
//
// MainThreadSleep is the pump, and it is true HERE because Allegro's idle
// loops call rest(1) - i.e. Sleep on the guest main thread. It is the only
// implemented pump; see virtual_clock.hpp for why a second one waits for a
// second target rather than being invented now.
inline constexpr pf::win32::TickPolicy kTick = {
    /* tick_fn_va        */ 0x0045d6c8ul,
    /* tick_arg_kind     */ pf::win32::TickPolicy::AccumulatedUnits,
    /* units_per_second  */ 1193181LL,
    /* tick_divisor_ms   */ 20LL,
    /* pump              */ pf::win32::TickPolicy::MainThreadSleep,
};

// ---------------------------------------------------------------------
// Threads the guest starts, and what happens to each.
//
// KNOWN (artifacts/functions.json + artifacts/dwarf_info.txt):
//   0x478584 wtimer.c tim_win32_high_perf_thread
//   0x4783bc wtimer.c tim_win32_low_perf_thread
// Both loop on WaitForSingleObject(stop_event@0x4ec050, <small ms>) and
// exit on the first non-WAIT_TIMEOUT return, so ParkReal (run the original
// entry on a real thread with unconditional waits) satisfies
// _tim_win32_exit's join loop by construction - divergence 003
// (notes/living_record.md), and the reason threads.hpp documents ParkReal
// as replacing VirtualizeStub rather than complementing it.
//
//   0x479a40 winput.c input_thread_proc
// MEASURED (carrier/NOTES.md): never actually spawned in this build - only
// the timer and window threads are. VirtualizeStub is kept as a guard, not
// because it fires.
//
//   0x404014 fld_adspot.c fldads_threadmain
// The ONE pthread_create call site in the whole binary. Its live HTTP
// result reaches five fld_adspot.c globals that are inside the 151-global
// digest domain, so it is Suppressed outright in a carrier-owned run.
inline constexpr unsigned long kThreadsParkReal[] = { 0x00478584ul, 0x004783bcul };
inline constexpr unsigned long kThreadsVirtualize[] = { 0x00479a40ul };
inline constexpr unsigned long kThreadsSuppress[] = { 0x00404014ul };
inline constexpr pf::win32::ThreadPolicy kThreads = {
    kThreadsParkReal,   2,
    kThreadsVirtualize, 1,
    kThreadsSuppress,   1,
};

// ---------------------------------------------------------------------
// The keyboard channel.
//
// KNOWN (artifacts/functions.json + artifacts/dwarf_info.txt):
//   0x43e2f8 keyboard.c  void _handle_key_press(int keycode, int scancode)
//   0x43d8d4 keyboard.c  void _handle_key_release(int scancode)
//   0x46d5a8 wkeybd.c    key_dinput_handle_scancode(int scancode, int pressed)
//   0x4daf80 wkeybd.c    unsigned char hw_to_mycode[256]
//
// The last two are the capture side and the translation table between the
// two scancode spaces. hw_to_mycode is READ OUT OF THE MAPPED IMAGE rather
// than copied here (input_channel.hpp inverts it at first use): a
// hand-written copy covers the keys someone thought of and silently
// mistranslates the rest. MEASURED at 0x4daf80 (disasm around
// 0x46d660/0x46d71e index it with `movzbl 0x4daf80(%ebx),%ebx`):
// hw_to_mycode[0x01]==59, [0x1c]==67, [0x39]==75, [0xcb]==82, [0xcd]==83,
// [0xc8]==84, [0xd0]==85 - the standard scancode-set-1 DIK_* values for
// these seven keys, matching kKeyNames one for one.
//
// kKeyNames is the DWARF __allegro_KEY_* enum (artifacts/dwarf_info.txt),
// restricted to the seven keys this game's scripts can name.
inline constexpr pf::win32::KeyName kKeyNames[] = {
    {"KEY_ESC", 59}, {"KEY_ENTER", 67}, {"KEY_SPACE", 75},
    {"KEY_LEFT", 82}, {"KEY_RIGHT", 83}, {"KEY_UP", 84}, {"KEY_DOWN", 85},
};

inline constexpr pf::win32::InputBindingPolicy kInputBinding = {
    /* deliver_press_va   */ 0x0043e2f8ul,
    /* deliver_release_va */ 0x0043d8d4ul,
    /* capture_va         */ 0x0046d5a8ul,
    /* scancode_map_va    */ 0x004daf80ul,
    /* key_names          */ kKeyNames,
    /* key_name_count     */ 7,
};

// ---------------------------------------------------------------------
// Window activation.
//
// MEASURED (carrier/NOTES.md "Environment isolation" item 2): a foreign
// window taking the foreground makes Windows send WM_ACTIVATEAPP to the
// guest's window thread; directx_wnd_proc (0x4791e0) calls
// _win_switch_out/_win_switch_in (wdispsw.c, 0x47a3d4/0x47a47c), each of
// which ends in a tail `jmp` to _switch_out/_switch_in (dispsw.c,
// 0x465808/0x4657e4) - a bare loop over an 8-entry callback table
// (0x4ea060/0x4ea080). Three entries are the game's own
// switchedFromProgram/switchedToProgram, which write hasFocus (0x4bc020)
// and lastFocus (0x4bc024) - both inside the 151-global digest domain, and
// both compared by play() at 0x411c6b/0x411cd7, which restarts the game
// music (checkMusicVoiceID @0x4bc174, also in the domain) when they differ.
// So the operator's desktop CAN change the verdict, which is why this
// channel is owned.
//
// RULED OUT BY EVIDENCE: set_display_switch_mode. _win_switch_out's
// disassembly branches on get_display_switch_mode only to decide whether to
// ALSO reset an event and drop the thread priority; BOTH arms end in the
// same `jmp _switch_out`, so no switch mode stops the callbacks. The
// dispatchers are the only real choke point.
inline constexpr pf::win32::FocusChannelPolicy kFocusChannel = {
    /* switch_in_va    */ 0x004657e4ul,
    /* switch_out_va   */ 0x00465808ul,
    /* cb_table_in_va  */ 0x004ea080ul,
    /* cb_table_out_va */ 0x004ea060ul,
    /* cb_table_len    */ 8,
};

// ---------------------------------------------------------------------
// Argument sensors: --headless and --no-sound.
//
// KNOWN (carrier/gen/interop_index.json, DWARF-confirmed COFF symbols
// _set_gfx_mode / _install_sound - notes/binary_recon.md items g/h):
//   set_gfx_mode(int card, int w, int h, int v_w, int v_h)  graphics.c 0x450688
//   install_sound(int digi, int midi, const char *cfg_path) sound.c    0x4417b0
// Both cdecl, so at the sensor's hit (EIP == va, before the callee's own
// prologue) the guest ESP is exactly [retaddr][arg0][arg1]..., unmodified
// since the caller's `call`.
//
// 0x47444942 is GFX_GDI (carrier/gen/pf_lib_bindings.h:420, generated from
// Allegro 4.4.1 DWARF - the FOURCC-style value <allegro/gfx.h> defines for
// the GDI software driver). notes/binary_recon.md item g confirms _gfx_gdi
// is one of the two gfx-driver families actually compiled into this binary.
// 640x480 windowed is this project's headless size; v_w/v_h (slots 4 and 5)
// are deliberately NOT overridden - GFX_GDI has no page-flipping or
// virtual-screen concept and Allegro's own GDI driver ignores them.
//
// DIGI_NONE == MIDI_NONE == 0 (Allegro 4's public digi.h/midi.h, stable
// across the whole 4.x series - not FOURCC-encoded like GFX_*, so not
// re-derived from disassembly). notes/binary_recon.md item h confirms
// _digi_none is compiled in alongside _digi_directsound.
inline constexpr pf::win32::ArgOverride kGfxOverrides[] = {
    { 1, 0x47444942u },  // card = GFX_GDI
    { 2, 640u },         // w
    { 3, 480u },         // h
};
inline constexpr pf::win32::ArgSensor kSensorSetGfxMode = {
    /* va             */ 0x00450688ul,
    /* overrides      */ kGfxOverrides,
    /* override_count */ 3,
    /* capture_count  */ 3,
    /* label          */ "set_gfx_mode",
};

inline constexpr pf::win32::ArgOverride kSoundOverrides[] = {
    { 1, 0u },  // digi = DIGI_NONE
    { 2, 0u },  // midi = MIDI_NONE
};
inline constexpr pf::win32::ArgSensor kSensorInstallSound = {
    /* va             */ 0x004417b0ul,
    /* overrides      */ kSoundOverrides,
    /* override_count */ 2,
    /* capture_count  */ 2,
    /* label          */ "install_sound",
};

// ---------------------------------------------------------------------
// The frame oracle.
//
// KNOWN (notes/binary_recon.md item g + artifacts/disasm.txt): every one of
// the ~20 call sites pushes a BITMAP* immediately before
// `call 40b6bc <_blit_to_screen>`. The site inside play()'s own per-tick
// path (0x413326, right after the frame-pacing wait, item l) is:
//     41331e: mov eax, 0x4dd194   ; swap_screen (BITMAP*, main.c)
//     413323: mov [esp], eax
//     413326: call 40b6bc <_blit_to_screen>
// draw_frame() (0x40929c) has just finished rendering into that same
// bitmap, and blit_to_screen presents it to the driver's own `screen`
// BITMAP (0x4dda8c, Allegro's own global, deliberately NOT the oracle).
// Because the ARGUMENT is the source bitmap at every call site, the sensor
// reads the argument rather than special-casing swap_screen by name - which
// also makes it correct for the menu and other call sites for free.
//
// BITMAP layout (KNOWN, carrier/gen/it_types.h, DWARF-derived):
//   int w,h,clip,cl,cr,ct,cb;   at 0,4,8,12,16,20,24
//   GFX_VTABLE *vtable;         at 28
//   void *write_bank,*read_bank,*dat; unsigned long id; void *extra;
//   int x_ofs,y_ofs,seg;        at 32..60
//   unsigned char *line[h];     at 64
// GFX_VTABLE.color_depth is its first member (offset 0) - Allegro's public
// bitmap_color_depth(bmp) macro is exactly (bmp)->vtable->color_depth.
//
// 0x44c47c is get_palette(RGB *pal) (carrier/gen/pf_lib_bindings.h:143),
// used ONLY for the 8bpp case of --frame-dump-at's human-readable PPM; the
// digest never needs palette interpretation because it hashes raw bytes.
inline constexpr pf::win32::FrameOraclePolicy kFrameOracle = {
    /* present_fn_va    */ 0x0040b6bcul,
    /* bitmap_arg_index */ 0,
    /* w_off            */ 0u,
    /* h_off            */ 4u,
    /* vtable_off       */ 28u,
    /* line_array_off   */ 64u,
    /* color_depth_off  */ 0u,
    /* palette_fn_va    */ 0x0044c47cul,
};

// ---------------------------------------------------------------------
// The snapshot domain.
//
// KNOWN (notes/binary_recon.md, carrier/NOTES.md "Milestones 8-9"):
//   .data  0x004bc000 + 0x000176f4
//   .bss   0x004dd000 + 0x00036978
// plus the two carrier-owned regions: the deterministic arena (kArena,
// captured only up to its live `top`, because the allocator's whole state
// lives inside that prefix) and the fixed guest stack (kGuestImage's
// stack_va/stack_size, captured as a LIVE RANGE from ESP upward - below ESP
// is dead, and ESP is the measured constant 0x0e1fef30 at all 876
// safepoints of the G1 workload, so this is ~4 KB rather than 2 MB).
//
// image_identity is [0x400000, .data): headers + .text + .rdata, the
// read-only half. Restoring into a differently-built image is nonsense.
//
// safepoint_va 0x4124f4 is main.c play(), once per consumed game tick.
//
// fault_probe_va 0x4fac28 is reward_scale, chosen because it is inside .bss
// AND inside the 151-global digest domain (carrier/gen/game_globals.inc),
// so --restore-fault's flipped bit must make the very first post-restore
// digest line differ - which is what makes it a real negative control
// rather than a gesture.
inline constexpr pf::win32::SnapshotRegion kSnapshotRegions[] = {
    { "data",  0x004bc000ul, 0x000176f4ul, pf::win32::SnapshotRegion::Full },
    { "bss",   0x004dd000ul, 0x00036978ul, pf::win32::SnapshotRegion::Full },
    { "arena", 0x20000000ul, 0ul,          pf::win32::SnapshotRegion::LiveRange },
    { "stack", 0x0e000000ul, 0x00200000ul, pf::win32::SnapshotRegion::LiveRange },
};
enum { kRegionData = 0, kRegionBss = 1, kRegionArena = 2, kRegionStack = 3 };

inline constexpr pf::win32::SnapshotDomainPolicy kSnapshotDomain = {
    /* regions             */ kSnapshotRegions,
    /* region_count        */ 4,
    /* image_identity_va   */ 0x00400000ul,
    /* image_identity_size */ 0x004bc000ul - 0x00400000ul,
    /* safepoint_va        */ 0x004124f4ul,
    /* fault_probe_va      */ 0x004fac28ul,
};

}  // namespace icytower
