// wrappers.hpp - the small set of imports that need real carrier-side
// behavior changes, not just counting.
//
// The GENERIC half - the exit path (ExitProcess/exit/_cexit/abort) and
// guest identity (GetModuleFileNameA/GetCommandLineA) - is
// port_forge/src/platform/win32/guest_identity.hpp, with the evidence for
// each substitution beside it. What is left in this file is the TABLE: which
// import names this carrier replaces, and with what. That is project policy,
// because it is the list of channels this project decided to own.
#pragma once
#include "../../port_forge/src/platform/win32/guest_identity.hpp"
#include "../../port_forge/src/platform/win32/imports.hpp"

// The one table of wrapped imports: which names, and what each is replaced
// with. Consumed by pf::win32::imports_init, which wires each entry
// STRAIGHT into the guest IAT (bypassing the counting trampoline - that is
// the point of a wrapper), so every wrapper counts itself via
// pf_count_import. Previously this was two lists that had to be kept in
// step by hand: a name array in imports.cpp and a strcmp chain here.
pf::win32::WrapPolicy wrappers_policy();

// imports_init() calls this right after resolving each wrapped import's
// real address, so the wrapper can forward to it (e.g. GetModuleFileNameA
// with a genuinely foreign handle). `id` is the import's report index,
// forwarded to pf_count_import by each wrapper itself - they are wired
// directly into the guest IAT and never pass through the trampoline.
void wrappers_bind_real(const char* name, void* real_proc, int id);

using ShutdownFn = pf::win32::ShutdownFn;

// Publishes the guest's identity (the absolute image path main.cpp
// resolved, which is also what GetCommandLineA must answer), the carrier's
// own module handle, and the shutdown hook the exit path calls before it
// really terminates the process. See guest_identity.hpp for why the
// carrier's own handle has to be treated as "the main module".
void wrappers_set_guest_identity(const char* image_path, void* carrier_hmodule,
                                 ShutdownFn on_shutdown);
