#!/usr/bin/env python3
"""check_native_layer.py -- tier-0 purity gate for src/ (win32_pilot.md SS7a).

src/ is the clean port: recovered semantics only, no PortForge type, no
carrier header, no guest address. This walks src/**/*.c,*.h and refuses
(non-zero exit, file:line per violation) any of:
  - a literal address (hex or decimal) inside a guest memory range
  - an #include of a carrier/generated-interop path
  - an identifier starting with a carrier-reserved prefix
  - inline asm

Usage: check_native_layer.py [DIR]   (default: <repo>/src)
"""
import re
import sys
from pathlib import Path

# (low, high) inclusive, from win32_pilot.md: image base..end, guest heap
# arena, guest stack region.
GUEST_RANGES = [
    (0x00400000, 0x0078c000),
    (0x20000000, 0x2fffffff),
    (0x0e000000, 0x0e1fffff),
]

HEX_ADDR_RE = re.compile(r'\b0[xX][0-9a-fA-F]+\b')
DEC_ADDR_RE = re.compile(r'\b[0-9]{6,10}\b')
INCLUDE_RE = re.compile(r'#\s*include\s*[<"]([^">]+)[">]')
BANNED_INCLUDE_SUBSTR = (
    'carrier/', 'gen/', 'port_forge',
    'it_types.h', 'it_globals.h', 'it_funcs.h', 'pf_rt.h', 'pf_bindings',
    'pf_harness',
)
# Carrier vocabulary only. A bare `pf_` prefix is NOT banned: Allegro's own
# PACKFILE_VTABLE members (pf_fclose, pf_getc, ...) legitimately carry it and
# the port must keep the library's real names.
BANNED_IDENT_RE = re.compile(
    r'(?:PF_[A-Za-z0-9_]*|pf_rt|pf_x87_t|pf_bindings[A-Za-z0-9_]*'
    r'|pf_harness[A-Za-z0-9_]*|pf_lift[A-Za-z0-9_]*|pf_import[A-Za-z0-9_]*'
    r'|pf_on_[A-Za-z0-9_]*|IT_G_[A-Za-z0-9_]*|IT_F_[A-Za-z0-9_]*'
    r'|PFN_[A-Za-z0-9_]*|lifted_[A-Za-z0-9_]*)')
ASM_RE = re.compile(r'\b(?:__asm__|__asm|_asm|asm)\b')
COMMENT_RE = re.compile(r'/\*.*?\*/|//[^\n]*', re.S)


def in_guest_range(v):
    return any(lo <= v <= hi for lo, hi in GUEST_RANGES)


def strip_comments(text):
    # Blank out comment bodies (keep newlines, so line numbers stay
    # accurate) so a comment merely *mentioning* an address or "asm"
    # doesn't trip the gate -- only code does.
    return COMMENT_RE.sub(lambda m: ''.join(c if c == '\n' else ' ' for c in m.group(0)), text)


def check_file(path):
    violations = []
    text = strip_comments(path.read_text(encoding='utf-8', errors='replace'))
    for lineno, line in enumerate(text.splitlines(), 1):
        for m in HEX_ADDR_RE.finditer(line):
            if in_guest_range(int(m.group(0), 16)):
                violations.append((lineno, 'guest address literal %s' % m.group(0)))
        for m in DEC_ADDR_RE.finditer(line):
            if in_guest_range(int(m.group(0))):
                violations.append((lineno, 'guest address literal %s (decimal)' % m.group(0)))
        im = INCLUDE_RE.search(line)
        if im:
            inc = im.group(1)
            for bad in BANNED_INCLUDE_SUBSTR:
                if bad in inc:
                    violations.append((lineno, 'carrier/generated-interop include: #include "%s"' % inc))
                    break
        for m in BANNED_IDENT_RE.finditer(line):
            violations.append((lineno, 'carrier-reserved identifier: %s' % m.group(0)))
        if ASM_RE.search(line):
            violations.append((lineno, 'inline asm'))
    return violations


def main(argv):
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent / 'src'
    files = sorted(list(root.rglob('*.c')) + list(root.rglob('*.h'))) if root.is_dir() else []

    total = 0
    for f in files:
        for lineno, msg in check_file(f):
            print('%s:%d: %s' % (f, lineno, msg))
            total += 1

    print('---')
    print('check_native_layer: scanned %d file(s) under %s, %d violation(s)' %
          (len(files), root, total))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
