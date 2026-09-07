// trace.hpp - the counting/tracing trampoline path every TRACE-mode import
// goes through, plus the report writer.
#pragma once

// Opens (or no-ops if out_path is null) the trace-line output file.
void trace_init(const char* out_path);

// Marks import id as one that should also emit a text trace line (in
// addition to being counted, which always happens for anything routed
// through pf_import_common).
void trace_enable_id(int id);
void trace_enable_all();

// Flushes/closes the trace file. Safe to call more than once.
void trace_close();

// Writes the per-import and per-thread call-count report as JSON to
// report_path. Safe to call at most once meaningfully; safe to call more.
void trace_write_report(const char* report_path);

// The naked trampoline every generated pf_stub_N jumps into. On entry the
// stack is [id][return address][args...] (pushed by the per-import stub).
// Preserves every register except eax/ecx/edx, tail-jumps to g_real[id]
// with the stack exactly as the original caller left it.
// naked applies only to the definition (trace.cpp) - a plain declaration here.
extern "C" void pf_import_common();

// Called by pf_import_common with the id and a pointer to the
// [return address][args...] frame (i.e. one dword past the id). cdecl,
// called from asm - must not throw, must be safe to call from any thread.
extern "C" void __cdecl pf_on_import(int id, void* frame);
