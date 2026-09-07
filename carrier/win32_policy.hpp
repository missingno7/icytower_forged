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
#include "../port_forge/src/platform/win32/policy.hpp"
#include "../port_forge/src/platform/win32/rng.hpp"

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

}  // namespace icytower
