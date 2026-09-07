// imports.cpp - the PROJECT half of import resolution.
//
// The mechanism (DLL loading by policy, GetProcAddress, the WRAP/TRACE/
// DIRECT slot dispositions, the IAT write) lives in
// port_forge/src/platform/win32/imports.hpp. This file is the only TU that
// #includes the generated table/name arrays, so they are defined exactly
// once, and it is where the Icy Tower sidecar list becomes a
// pf::win32::SidecarDllPolicy.
// This TU is also the one that asks trace.hpp for the naked import
// trampoline's definition (see that header: inline assembly is not a C++
// reference, so an inline definition would be emitted nowhere). It has to
// be the first include, before anything can pull trace.hpp in ahead of it.
#define PF_WIN32_TRACE_IMPLEMENTATION
#include "../../port_forge/src/platform/win32/trace.hpp"

#include <windows.h>
#include <cstring>
#include "imports.hpp"
#include "wrappers.hpp"
#include "../win32_policy.hpp"

#include "../gen/import_table.inc"
#include "../gen/import_names.inc"

bool imports_init(const ImportsConfig& cfg) {
    // assets\ddraw.dll is cnc-ddraw's DirectDraw-compatibility shim, the one
    // the user installed to run the original on Windows 11; --ddraw=system
    // asks for the host's own instead, which is the only field of the
    // sidecar policy this run can change.
    pf::win32::SidecarDll sidecars[icytower::kSidecarCount];
    memcpy(sidecars, icytower::kSidecars, sizeof(sidecars));
    if (cfg.ddraw_mode == DDrawMode::System) {
        for (int i = 0; i < icytower::kSidecarCount; ++i) {
            if (_stricmp(sidecars[i].dll, "DDRAW.dll") == 0)
                sidecars[i].source = pf::win32::SidecarDll::System;
        }
    }

    pf::win32::ImportsConfig fcfg;
    fcfg.sidecars.entries = sidecars;
    fcfg.sidecars.count = icytower::kSidecarCount;
    fcfg.sidecars.assets_dir = cfg.assets_dir;
    fcfg.wraps = wrappers_policy();
    fcfg.count_imports = cfg.count_imports;
    fcfg.bind_real = wrappers_bind_real;
    return pf::win32::imports_init(fcfg);
}
