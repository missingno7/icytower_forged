// imports.hpp - the project's thin front for
// port_forge/src/platform/win32/imports.hpp.
//
// Only one thing about import resolution is still a run-time CHOICE rather
// than policy: which ddraw.dll the guest's DirectDrawCreate binds to. The
// rest (the sidecar list, the wrapper table) is data - see
// carrier/win32_policy.hpp and src/wrappers.cpp.
#pragma once
#include "../../port_forge/src/platform/win32/imports.hpp"

enum class DDrawMode { Local, System };

struct ImportsConfig {
    const char* assets_dir;   // directory containing icytower15.exe + its side DLLs
    DDrawMode   ddraw_mode;   // which ddraw.dll DirectDrawCreate binds to
    bool        count_imports; // route non-wrapped imports through the counting trampoline
};

// Builds the sidecar/wrapper policy from carrier/win32_policy.hpp and runs
// pf::win32::imports_init. Returns false if any import failed to resolve.
bool imports_init(const ImportsConfig& cfg);
