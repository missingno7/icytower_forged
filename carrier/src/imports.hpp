// imports.hpp - resolves every guest import and writes the guest IAT.
#pragma once

enum class DDrawMode { Local, System };

struct ImportsConfig {
    const char* assets_dir;   // directory containing icytower15.exe + its side DLLs
    DDrawMode ddraw_mode;      // which ddraw.dll DirectDrawCreate binds to
    bool count_imports;        // route non-wrapped imports through the counting trampoline
};

// Loads every DLL named in the generated import table, resolves every
// import via GetProcAddress, and writes each guest IAT slot with either the
// real address (DIRECT), the counting trampoline (TRACE), or a hand-written
// wrapper (WRAP - see wrappers.cpp). Returns false if any import failed to
// resolve (still writes what it could, so partial runs are diagnosable).
bool imports_init(const ImportsConfig& cfg);
