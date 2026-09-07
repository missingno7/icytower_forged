// symbols.hpp - loads artifacts/functions.json at startup (name+va+size for
// every recovered guest function) and resolves an arbitrary guest VA to
// "function+offset" for trace lines and the VEH crash dump.
#pragma once

// Loads and sorts the function table from `path` (artifacts/functions.json).
// Returns false (and leaves lookups answering "<unknown>") if the file is
// missing or unparseable - diagnostics degrade gracefully, they don't block
// the carrier from running.
bool symbols_load(const char* path);

// Writes "name+0x<offset>" (or "<unknown>+0x<va>" if nothing contains va)
// into buf (size bytes). Never fails/throws.
void symbols_describe(unsigned long va, char* buf, int buf_size);
