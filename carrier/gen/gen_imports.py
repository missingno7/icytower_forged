#!/usr/bin/env python3
"""gen_imports.py - generates carrier/gen/import_table.inc, import_stubs.cpp,
and import_names.inc from <repo_root>/imports.json.

Input format (imports.json): a JSON array of [dll, name, iat_slot_va_hex]
triples, one per statically-imported function in icytower15.exe's import
directory.

Run: python gen_imports.py <repo_root>/imports.json <repo_root>/carrier/gen

Regenerate whenever imports.json changes. Do not hand-edit the outputs.
"""
import json
import os
import sys


HEADER = """// GENERATED FILE - DO NOT EDIT.
// Produced by carrier/gen/gen_imports.py from {src}.
// Regenerate with: python carrier/gen/gen_imports.py <imports.json> carrier/gen
"""


def main():
    if len(sys.argv) != 3:
        print("usage: gen_imports.py <imports.json> <out_dir>", file=sys.stderr)
        return 1
    src_path = sys.argv[1]
    out_dir = sys.argv[2]
    with open(src_path, "r", encoding="utf-8") as f:
        imports = json.load(f)

    os.makedirs(out_dir, exist_ok=True)
    n = len(imports)
    src_rel = os.path.basename(src_path)

    # --- import_table.inc: the {dll,name,iat_va,id} array -----------------
    table_path = os.path.join(out_dir, "import_table.inc")
    with open(table_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER.format(src=src_rel))
        f.write("// %d import entries.\n\n" % n)
        f.write('extern "C" const ImportEntry g_import_table[%d] = {\n' % n)
        for i, (dll, name, iat_hex) in enumerate(imports):
            f.write('    { "%s", "%s", 0x%08xu, %d },\n' % (
                dll, name, int(iat_hex, 16), i))
        f.write("};\n")
        f.write('extern "C" const int kNumImports = %d;\n' % n)

    # --- import_names.inc: id -> "dll!name" diagnostic strings -------------
    names_path = os.path.join(out_dir, "import_names.inc")
    with open(names_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER.format(src=src_rel))
        f.write('extern "C" const char* g_import_names[%d] = {\n' % n)
        for dll, name, _ in imports:
            f.write('    "%s!%s",\n' % (dll, name))
        f.write("};\n")

    # --- import_stubs.cpp: one naked push-id/jmp trampoline per import -----
    stubs_path = os.path.join(out_dir, "import_stubs.cpp")
    with open(stubs_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER.format(src=src_rel))
        f.write('// One naked trampoline per import: "push <id>; jmp pf_import_common".\n')
        f.write('// Selected into the guest IAT slot when that import is routed through\n')
        f.write('// the counting/tracing path (see src/imports.cpp).\n\n')
        f.write('// naked applies only to the definition (src/trace.cpp) - a plain declaration here.\n')
        f.write('extern "C" void pf_import_common();\n\n')
        for i in range(n):
            f.write('extern "C" __declspec(naked) void pf_stub_%d() {\n' % i)
            f.write('    __asm { push %d }\n' % i)
            f.write('    __asm { jmp pf_import_common }\n')
            f.write('}\n')
        f.write('\nextern "C" void* g_import_stubs[%d] = {\n' % n)
        for i in range(n):
            f.write('    (void*)pf_stub_%d,\n' % i)
        f.write('};\n')

    print("wrote %s (%d entries)" % (table_path, n))
    print("wrote %s" % names_path)
    print("wrote %s" % stubs_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
