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

Thin delegator (notes/extraction_plan.md S1): the purity gate itself now
lives in the port_forge submodule as tools/pf_native_purity.py, parametrized
on the guest memory ranges, banned-include substrings and banned-identifier
patterns this project used to hardcode (see that tool's own module
docstring; this project's values live in carrier/win32_policy.json's
"purity_gate" section). This wrapper passes them explicitly instead of
relying on that tool's icytower_forged-shaped fallback default, so the
documented zero/one-argument invocation above keeps working unchanged.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
TOOLS_DIR = os.path.join(ROOT, 'port_forge', 'tools')
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_native_purity as _impl  # noqa: E402

POLICY_PATH = os.path.join(ROOT, 'carrier', 'win32_policy.json')


def main(argv):
    target_dir = argv[1] if len(argv) > 1 else os.path.join(ROOT, 'src')

    with open(POLICY_PATH, encoding='utf-8') as f:
        policy = json.load(f)
    gate = policy['purity_gate']

    import tempfile
    fd, config_path = tempfile.mkstemp(prefix='pf_purity_config_', suffix='.json')
    with os.fdopen(fd, 'w', encoding='utf-8') as f:
        json.dump(gate, f)

    return _impl.main([argv[0], target_dir, '--config', config_path])


if __name__ == '__main__':
    sys.exit(main(sys.argv))
