// print_globals.hpp - milestone 9a item 3 (win32_pilot.md SS8 row 9a,
// carrier/NOTES.md "Headless, frame oracle, named globals, .itr workload"):
// a GENERIC named-globals summary, printed at shutdown.
//
// `--print-globals expr1,expr2,...` where each expr is a DWARF-named game
// global (carrier/gen/interop_index.json / it_globals.h, typed by
// carrier/gen/gen_print_globals.py's generated table
// carrier/gen/it_print_globals.inc - NOT hand-typed here), optionally
// followed by array indexing (`[N]` or `[other_global_name]`, e.g.
// `ply[player_id]`) and one or more struct-member accesses (`->field` or
// `.field`, auto-dereferencing either way for convenience), e.g.
// `ply[player_id]->score`. No Icy-Tower-specific name is hard-coded here -
// scripts/play.py supplies the Icy Tower list (score/floor/combo) as data.
#pragma once
#include <cstdio>

// Parses `spec` (comma-separated expressions) once. Safe to call with a
// null/empty spec (does nothing; print_globals_run/report then no-op).
void print_globals_init(const char* spec);

// Evaluates every parsed expression against the GUEST's live memory and
// prints `global <expr> = <value>` to stdout, one line per expression.
// Call at shutdown, while the guest image/heap/stack are still mapped
// (before TerminateProcess) - main.cpp's carrier_shutdown() calls this.
void print_globals_run();

// Same evaluation, as a JSON array under "print_globals" in --report.
// Emits nothing when print_globals_init was never given a spec.
void print_globals_report_json(FILE* f);
