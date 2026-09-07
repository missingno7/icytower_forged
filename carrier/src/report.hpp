// report.hpp - this carrier's --report JSON: which sections it has, and in
// what order.
//
// The document is PROJECT composition. port_forge contributes exactly one
// well-known pair of fields to it (pf::win32::trace_write_import_counts's
// "imports"/"threads" arrays); every other section is a question this
// project decided to ask, answered by whichever subsystem owns it.
#pragma once

// Writes the run's report to `report_path` (no-op when null/empty).
void carrier_write_report(const char* report_path);
