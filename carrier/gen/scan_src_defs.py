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

--list-build-files {msvc,gcc} ("binding table generated" pass, carrier/
NOTES.md): prints the comma-separated list of src/icytower/*.c FILES (not
function names) build.cmd should compile through the named toolchain, so the
MSVC-vs-GCC-x87 split (carrier/NOTES.md "Milestone 12 at scale" SS3;
win32_pilot.md SS6a) is derived mechanically from each file's own content
instead of being hand-listed and left to drift the moment a new file with
real floating point (or without) is added. A file qualifies for either list
only if it DEFINES at least one function this repo's own
carrier/gen/interop_index.json recognizes as a real game function (VA
present) -- this is what correctly excludes assets_standalone.c (defines
functions, but none with an interop_index.json VA: harness/self-test code,
not part of the binding table) while still catching every promoted
src/icytower/*.c file automatically. Within that set, a file goes to `gcc`
if it contains the token `double` or `float` OUTSIDE of a comment or string
literal (real floating-point code, needing the mingw32 GCC
-mfpmath=387 -mno-sse2 x87-faithful codegen build.cmd already uses for
jump_player.c/line_intersect.c/new_rand.c/particle.c -- MSVC's cl.exe has no
/arch override on this 32-bit target and compiles `double` through
SSE/plain-double codegen instead, PROMOTIONS.md's own measured numbers), and
to `msvc` otherwise. Over-inclusion in `gcc` is harmless (a pure-integer file
compiles identically either way; add_floor's own PROMOTIONS.md entry already
calls GCC "the toolchain of record" for the file it shares with three
integer-only functions, map.c, for exactly this reason) so the check
deliberately does not try to distinguish "this double is really exercised by
arithmetic" from "this double is only ever move/compared" -- it only avoids
counting a `double`/`float` mentioned purely in a comment.
"""
import argparse
import json
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


# Strip C comments and string/char literals before the double/float token
# check, so a header comment mentioning "double" in prose (update_player.c's
# own header block has several) cannot by itself decide the toolchain -- only
# genuine code tokens do. Deliberately simple (no line-continuation, no
# trigraphs): this codebase's own style never needs either.
_STRIP_RE = re.compile(
    r'/\*.*?\*/|//[^\n]*|"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'',
    re.DOTALL)
_FLOAT_TOKEN_RE = re.compile(r'\b(?:double|float)\b')


def file_needs_gcc_x87(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    code_only = _STRIP_RE.sub(' ', text)
    return bool(_FLOAT_TOKEN_RE.search(code_only))


def load_build_blockers(script_dir):
    """carrier/gen/build_blockers.json -- see that file's own '_purpose' for
    why this is a small hand-curated exception list rather than something
    --list-build-files could derive from file content the way the
    MSVC-vs-GCC-x87 split does."""
    path = os.path.join(script_dir, 'build_blockers.json')
    if not os.path.isfile(path):
        return {}
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f).get('files', {})


def load_game_function_names(script_dir):
    """carrier/gen/interop_index.json's function names -- used to tell a real
    game-function source file (belongs in build.cmd's compile list) apart
    from a harness/self-test file that merely defines SOME function
    (assets_standalone.c: assets_standalone_family/_raw, asset_sample --
    none of them a recovered game function)."""
    interop_path = os.path.join(script_dir, 'interop_index.json')
    with open(interop_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    return {fn['name'] for fn in data['functions']}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    default_src_dir = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'icytower'))
    ap.add_argument('--src-dir', default=default_src_dir,
                     help='directory to scan (default: %(default)s)')
    ap.add_argument('--format', choices=['csv', 'lines'], default='csv')
    ap.add_argument('--list-build-files', choices=['msvc', 'gcc'], default=None,
                     help='print the space-separated *.c file list for the named '
                          'toolchain instead of the function-name exclude list')
    ap.add_argument('--prefix', default='',
                     help='prepended to each filename in --list-build-files output '
                          '(e.g. ..\\src\\icytower\\), so build.cmd can drop the '
                          'result straight into a cl/gcc command line with no '
                          'further cmd.exe string surgery')
    ap.add_argument('--ext', default=None,
                     help="replace each filename's trailing .c with this "
                          "(e.g. .obj/.o), for deriving build.cmd's link-line "
                          "object list from the same --list-build-files output "
                          "instead of a second hand-maintained list")
    args = ap.parse_args()

    src_dir = os.path.abspath(args.src_dir)
    if not os.path.isdir(src_dir):
        print(f"scan_src_defs: '{src_dir}' does not exist - nothing to exclude", file=sys.stderr)
        return

    per_file = {}   # fn -> [names]
    names = []
    for fn in sorted(os.listdir(src_dir)):
        if not fn.endswith('.c') or fn == 'state.c':
            continue  # state.c: storage only (win32_pilot.md SS7a), never a function body
        found = scan_file(os.path.join(src_dir, fn))
        if not found:
            print(f"scan_src_defs: WARNING - no function definitions found in {fn} "
                  f"(exclusion list will be missing whatever it defines)", file=sys.stderr)
        per_file[fn] = found
        names.extend(found)

    if args.list_build_files:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        game_fn_names = load_game_function_names(script_dir)
        blockers = load_build_blockers(script_dir)
        msvc_files, gcc_files = [], []
        for fn, found in per_file.items():
            if not any(n in game_fn_names for n in found):
                continue  # defines nothing this repo recognizes as a game function
            if fn in blockers:
                print(f"scan_src_defs: SKIPPING {fn} (build_blockers.json: {blockers[fn]})",
                      file=sys.stderr)
                continue
            dest = gcc_files if file_needs_gcc_x87(os.path.join(src_dir, fn)) else msvc_files
            dest.append(fn)
        chosen = gcc_files if args.list_build_files == 'gcc' else msvc_files
        if args.ext:
            chosen = [fn[:-2] + args.ext if fn.endswith('.c') else fn for fn in chosen]
        print(' '.join(args.prefix + fn for fn in chosen))
        print(f"scan_src_defs: msvc={{{', '.join(msvc_files)}}} gcc(x87)={{{', '.join(gcc_files)}}}",
              file=sys.stderr)
        return

    if args.format == 'csv':
        print(','.join(names))
    else:
        for n in names:
            print(n)
    print(f"scan_src_defs: {len(names)} function(s) found in {src_dir}: {', '.join(names) if names else '(none)'}",
          file=sys.stderr)


if __name__ == '__main__':
    main()
