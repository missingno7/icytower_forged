// wrappers.hpp - the small set of imports that need real carrier-side
// behavior changes, not just counting. See wrappers.cpp for the evidence
// behind each one.
#pragma once

// Returns the wrapper function pointer for `name` (matched by import name
// only - all current wrapped imports are unambiguous by name alone), or
// nullptr if `name` isn't wrapped.
void* wrappers_lookup(const char* name);

// imports_init() calls this right after resolving each wrapped import's
// real address, so the wrapper can forward to it (e.g. GetModuleFileNameA
// with hModule != NULL).
void wrappers_bind_real(const char* name, void* real_proc);

// The absolute path main.cpp resolved for the guest image, used by the
// GetModuleFileNameA(NULL,...) and GetCommandLineA wrappers so the game's
// own directory/cwd/log-path logic (notes/binary_recon.md item a) sees the
// same string it would have seen running standalone.
void wrappers_set_guest_image_path(const char* path);

// carrier's own module handle (GetModuleHandleA(NULL), captured once from
// carrier's own code before the guest runs). GetModuleHandleA is left
// DIRECT (not wrapped - see imports.cpp policy), so guest code that calls
// GetModuleHandleA(NULL) legitimately gets carrier's real handle back; but
// when it then passes that handle to GetModuleFileNameA, the wrapper needs
// to recognize "this is asking about the main module" even though hModule
// isn't NULL - see wrap_GetModuleFileNameA's comment for the evidence.
void wrappers_set_carrier_hmodule(void* hmodule);

typedef void (*ShutdownFn)(const char* reason);
// Called once, from the ExitProcess/exit/_cexit/abort wrappers, before they
// really terminate the process - this is the carrier's only guaranteed
// chance to flush the trace log and write the report.
void wrappers_set_shutdown_hook(ShutdownFn fn);
