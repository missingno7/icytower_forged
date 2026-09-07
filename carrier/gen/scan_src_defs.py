#!/usr/bin/env python3
"""scan_src_defs.py -- lists the top-level function names DEFINED in
src/icytower/*.c (excluding state.c, which only supplies extern storage for
the standalone build and never defines a function -- src/README.md).

Purpose (win32_pilot.md SS7a, item 1 of the "src binding" pass): build.cmd
needs gen_bindings.py's --exclude list to contain exactly the functions
src/ currently defines, so pf_bindings_src.h does not also try to redirect
those names to their own original address (BINDINGS_NOTES.md's "Exclusion"
section - a function compiled natively into the carrier must keep its own
plain name). Hand-maintaining that list would silently drift the moment a
new file is added to src/icytower/ (exactly the class of bug BINDINGS_NOTES.md
warns about for renaming); this script derives it mechanically from the
source files themselves instead, every time build.cmd runs.

Not a general C parser - deliberately as small as reasonably possible for
this codebase's actual style (return type on the same or a preceding line,
`name(args)` immediately followed by a `{` starting its own line, exactly
how every function in native/*.c, lift/lifted/*.c and src/icytower/*.c is
written - see e.g. src/icytower/update_frame.c). A prototype (`name(args);`)
never matches because the required trailing `{` on its own line is absent.

Usage:
    python scan_src_defs.py [--src-dir DIR] [--format csv|lines]
Prints the comma-separated (default) or newline-separated function names to
stdout; build.cmd captures stdout with `for /f` into an --exclude argument.
Warns (stderr) about any src/*.c file with zero definitions found (this
would silently break the exclusion list, so it must not go unnoticed) but
still exits 0 -- an empty src/ is a legitimate, if unusual, state.
"""
import argparse
import os
import re
import sys

# A definition: some return-type tokens, an identifier, a parenthesized
# argument list with no ';', '{' or '}' inside it (rules out prototypes and
# nested parens-with-braces), then only whitespace up to a '{' that starts
# the next line - i.e. the opening brace of the function body sits alone on
# its own line, exactly this codebase's style.
DEF_RE = re.compile(
    r'^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\n\{',
    re.MULTILINE)


def scan_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    return [m.group(1) for m in DEF_RE.finditer(text)]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    default_src_dir = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'icytower'))
    ap.add_argument('--src-dir', default=default_src_dir,
                     help='directory to scan (default: %(default)s)')
    ap.add_argument('--format', choices=['csv', 'lines'], default='csv')
    args = ap.parse_args()

    src_dir = os.path.abspath(args.src_dir)
    if not os.path.isdir(src_dir):
        print(f"scan_src_defs: '{src_dir}' does not exist - nothing to exclude", file=sys.stderr)
        return

    names = []
    for fn in sorted(os.listdir(src_dir)):
        if not fn.endswith('.c') or fn == 'state.c':
            continue  # state.c: storage only (win32_pilot.md SS7a), never a function body
        found = scan_file(os.path.join(src_dir, fn))
        if not found:
            print(f"scan_src_defs: WARNING - no function definitions found in {fn} "
                  f"(exclusion list will be missing whatever it defines)", file=sys.stderr)
        names.extend(found)

    if args.format == 'csv':
        print(','.join(names))
    else:
        for n in names:
            print(n)
    print(f"scan_src_defs: {len(names)} function(s) found in {src_dir}: {', '.join(names) if names else '(none)'}",
          file=sys.stderr)


if __name__ == '__main__':
    main()
