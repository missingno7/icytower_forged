#!/usr/bin/env python3
"""Compiler fingerprint experiment (notes/external_research.md SS1, queued
experiment A / notes/toolchain_fingerprint.md).

For a fixed set of embedded-original targets (game CUs recovered into
src/icytower/*.c, and Allegro 4.4.1 / addons/logg CUs whose C source we have
in third_party/allegro-4.4.1/), this script:

  1. Compiles the SAME source file with TDM-GCC 4.4.1-tdm-1 and -tdm-2
     (third_party/tdm-gcc-4.4.1-tdm-{1,2}/mingw32/bin/gcc.exe) across a flag
     matrix (-O0/O1/O2/O3/Os x with/without -fomit-frame-pointer x default/
     -march=i686/-march=pentium; -mfpmath=387 always -- the default for this
     32-bit non-SSE compiler; no -ffast-math).
  2. For every named function in that CU, extracts:
       - the ORIGINAL bytes embedded in assets/icytower15.exe at the known
         VA/size (artifacts/functions.json), via the fixed-base file-offset
         formula (this EXE has no .reloc section: file_offset = va -
         0x400000 - 0x1000 + 0x400 = va - 0x400C00);
       - our compiled bytes for the same-named function, sliced out of the
         object's .text section between this symbol and the next (COFF
         object files carry no per-symbol size, so isolation-by-next-symbol
         is used; verified stable across optimization levels for this
         compiler/these small non-hot-cold-partitioned functions).
  3. Reads the object's own COFF relocations (objdump -r) that fall inside
     each function and masks those 4-byte fields (dir32/rel32 -- absolute
     address or relative-call/jmp fields the linker would fill in) to 0 in
     BOTH the compiled and the original bytes before any comparison. This
     is the "ignoring absolute immediates/displacements that are
     relocations" the task asks for: we only know our own object's
     relocation positions, so the assumption is that a code-generation
     match keeps those fields at the same offsets in the original.
  4. Scores each (toolchain, flags, function) triple:
       - byte_exact: same length AND masked bytes identical
       - mnemonic_seq: same instruction-mnemonic sequence (capstone,
         disassembled independently on each side, so a relocated
         immediate's changed VALUE does not by itself break mnemonic
         equality -- x86 direct/absolute encodings are fixed-width
         regardless of the immediate's value)
       - length: byte length of each side

Usage:
    python tools_recon/fingerprint_compare.py [--out artifacts/toolchain_fingerprint.json]

Writes a full per-(target,toolchain,flags) score table to --out and prints a
summary (best flag set per target/toolchain) to stdout.
"""
import argparse
import itertools
import json
import re
import subprocess
import sys
from pathlib import Path

try:
    import capstone
except ImportError:
    capstone = None

REPO = Path(__file__).resolve().parent.parent
EXE_PATH = REPO / "assets" / "icytower15.exe"
FUNCTIONS_JSON = REPO / "artifacts" / "functions.json"

IMAGE_BASE = 0x400000
TEXT_VA = 0x1000
TEXT_RAWPTR = 0x400


def file_offset(va):
    """This EXE has no .reloc section (fixed base, VA=0x400000) and .text's
    raw pointer/VA are known from the PE section table (0x400/0x1000), so
    file_offset = va - 0x400000 - 0x1000 + 0x400 = va - 0x400C00."""
    return va - IMAGE_BASE - TEXT_VA + TEXT_RAWPTR


TOOLCHAINS = {
    "tdm-1": REPO / "third_party" / "tdm-gcc-4.4.1-tdm-1" / "mingw32" / "bin",
    "tdm-2": REPO / "third_party" / "tdm-gcc-4.4.1-tdm-2" / "mingw32" / "bin",
}

ALLEGRO_441 = REPO / "third_party" / "allegro-4.4.1"
ALLEGRO_441_BUILD = REPO / "third_party" / "build-allegro-4.4.1"
VORBIS_HEADERS = REPO / "third_party" / "vorbis-headers"

# ---------------------------------------------------------------------------
# Target groups: one compiled CU per group, several named functions checked
# against their embedded VA/size (from artifacts/functions.json).
# ---------------------------------------------------------------------------

GROUPS = [
    {
        "group": "game/update_frame.c",
        "source": REPO / "src" / "icytower" / "update_frame.c",
        "include_dirs": [REPO / "src" / "icytower"],
        "defines": [],
        "extra_inc": [],
        "functions": ["update_frame"],
    },
    {
        "group": "game/is_solid.c",
        "source": REPO / "src" / "icytower" / "is_solid.c",
        "include_dirs": [REPO / "src" / "icytower"],
        "defines": [],
        "extra_inc": [],
        "functions": ["is_solid"],
    },
    {
        "group": "game/add_combo.c",
        "source": REPO / "src" / "icytower" / "add_combo.c",
        "include_dirs": [REPO / "src" / "icytower"],
        "defines": [],
        "extra_inc": [],
        "functions": ["add_combo"],
    },
    {
        "group": "game/control.c",
        "source": REPO / "src" / "icytower" / "control.c",
        "include_dirs": [REPO / "src" / "icytower"],
        "defines": [],
        "extra_inc": [],
        "functions": [
            "get_gamepad", "is_up", "is_down", "is_left", "is_right",
            "is_fire", "is_pause", "is_enter", "is_any",
            "set_control", "init_control", "check_control_key",
        ],
    },
    {
        "group": "allegro/timer.c",
        "source": ALLEGRO_441 / "src" / "timer.c",
        "include_dirs": [ALLEGRO_441 / "include", ALLEGRO_441_BUILD / "include"],
        "defines": ["ALLEGRO_STATICLINK"],
        "extra_inc": [],
        "functions": [
            "_handle_timer_tick", "rest_int", "timer_can_simulate_retrace",
            "timer_simulate_retrace", "timer_is_using_retrace",
            "remove_timer_int", "remove_int", "remove_param_int",
            "remove_timer", "install_timer", "install_timer_int",
            "install_param_int_ex", "install_param_int", "install_int_ex",
            "install_int", "rest_callback", "rest",
        ],
    },
    {
        "group": "allegro/color.c",
        "source": ALLEGRO_441 / "src" / "color.c",
        "include_dirs": [ALLEGRO_441 / "include", ALLEGRO_441_BUILD / "include"],
        "defines": ["ALLEGRO_STATICLINK"],
        "extra_inc": [],
        "functions": [
            "getr_depth", "getg_depth", "getb_depth", "getr", "getg", "getb",
            "geta", "bestfit_color", "makecol8", "makeacol_depth", "makeacol",
            "makecol_depth", "makecol", "rgb_to_hsv", "create_light_table",
            "create_trans_table", "create_color_table", "create_blender_table",
            "create_rgb_table", "hsv_to_rgb",
        ],
    },
    {
        "group": "allegro/c/cblit32.c",
        "source": ALLEGRO_441 / "src" / "c" / "cblit32.c",
        "include_dirs": [ALLEGRO_441 / "include", ALLEGRO_441_BUILD / "include"],
        "defines": ["ALLEGRO_STATICLINK", "ALLEGRO_COLOR32"],
        "extra_inc": [],
        "functions": [
            "_linear_clear_to_color32", "_linear_blit32_end",
            "_linear_masked_blit32", "_linear_blit_backward32",
            "_linear_blit32",
        ],
    },
    {
        "group": "allegro/blit.c",
        "source": ALLEGRO_441 / "src" / "blit.c",
        "include_dirs": [ALLEGRO_441 / "include", ALLEGRO_441_BUILD / "include"],
        "defines": ["ALLEGRO_STATICLINK"],
        "extra_inc": [],
        "functions": [
            "masked_blit", "blit", "get_replacement_mask_color",
            "dither_blit", "blit_from_24", "blit_from_32",
            "_blit_between_formats",
        ],
    },
    {
        "group": "allegro/addons/logg/logg.c",
        "source": ALLEGRO_441 / "addons" / "logg" / "logg.c",
        "include_dirs": [
            ALLEGRO_441 / "include", ALLEGRO_441_BUILD / "include",
            ALLEGRO_441 / "addons" / "logg",
        ],
        "defines": ["ALLEGRO_STATICLINK"],
        "extra_inc": [VORBIS_HEADERS],
        # only the 11 functions that exist in upstream 4.4.1 logg.c; the
        # other 7 embedded logg_* names are the game's own vendored
        # extension (notes/external_research.md SS2) and have no upstream
        # source to compile for comparison.
        "functions": [
            "logg_load", "logg_get_buffer_size", "logg_set_buffer_size",
            "logg_open_file_for_streaming", "read_ogg_data",
            "logg_play_stream", "logg_get_stream", "logg_update_stream",
            "logg_stop_stream", "logg_restart_stream", "logg_destroy_stream",
        ],
    },
]

OPT_LEVELS = ["-O0", "-O1", "-O2", "-O3", "-Os"]
FOMIT = [[], ["-fomit-frame-pointer"]]
MARCH = [[], ["-march=i686"], ["-march=pentium"]]


def flag_combos():
    for opt, fomit, march in itertools.product(OPT_LEVELS, FOMIT, MARCH):
        flags = [opt, "-mfpmath=387"] + fomit + march
        label = opt + ("+omit" if fomit else "") + (
            "+" + march[0].split("=")[1] if march else ""
        )
        yield label, flags


def load_targets():
    data = json.loads(FUNCTIONS_JSON.read_text(encoding="utf-8"))
    by_name = {}
    for e in data:
        name = e.get("name")
        if not name:
            continue
        # DWARF-recovery artifact: some names are recorded as
        # "(indirect string, offset: 0x...): realname" -- unwrap to the
        # real trailing name instead of dropping the entry.
        if name.startswith("(indirect string"):
            idx = name.rfind("): ")
            if idx == -1:
                continue
            name = name[idx + 3:]
        by_name.setdefault(name, e)
    return by_name


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def compile_unit(toolchain, group, flags, out_dir):
    gcc = TOOLCHAINS[toolchain] / "gcc.exe"
    out_dir.mkdir(parents=True, exist_ok=True)
    obj = out_dir / (group["source"].stem + ".o")
    cmd = [str(gcc), "-m32", "-c"] + flags
    for d in group["defines"]:
        cmd.append("-D" + d)
    for inc in group["include_dirs"] + group["extra_inc"]:
        cmd.append("-I" + str(inc))
    cmd += [str(group["source"]), "-o", str(obj)]
    proc = run(cmd)
    if proc.returncode != 0:
        return None, proc.stdout + proc.stderr
    return obj, proc.stdout + proc.stderr


SYM_RE = re.compile(r"^([0-9a-fA-F]{8})\s+([a-zA-Z])\s+(\S+)$")


def read_symbols(toolchain, obj):
    nm = TOOLCHAINS[toolchain] / "nm.exe"
    proc = run([str(nm), "-n", str(obj)])
    syms = []
    for line in proc.stdout.splitlines():
        m = SYM_RE.match(line.strip())
        if not m:
            continue
        addr, kind, name = m.groups()
        if kind.upper() != "T":
            continue  # only .text (code) symbols
        if name in (".text", ".data", ".bss", ".rdata"):
            continue  # section symbols nm -n also lists at offset 0
        # strip exactly the ONE leading underscore mingw's cdecl name
        # decoration adds -- do NOT strip further, some real C names
        # (e.g. _handle_timer_tick, _blit_between_formats) start with an
        # underscore themselves.
        real_name = name[1:] if name.startswith("_") else name
        syms.append((int(addr, 16), real_name))
    syms.sort()
    return syms


RELOC_RE = re.compile(
    r"^([0-9a-fA-F]{8})\s+(\S+)\s+(\S+)"
)


def read_relocations(toolchain, obj):
    objdump = TOOLCHAINS[toolchain] / "objdump.exe"
    proc = run([str(objdump), "-r", str(obj)])
    relocs = []
    in_text = False
    for line in proc.stdout.splitlines():
        if line.startswith("RELOCATION RECORDS FOR"):
            in_text = "[.text]" in line
            continue
        if not in_text:
            continue
        m = RELOC_RE.match(line.strip())
        if not m:
            continue
        off, rtype, sym = m.groups()
        if off.upper() == "OFFSET":
            continue
        relocs.append((int(off, 16), rtype, sym))
    return relocs


def extract_text(toolchain, obj):
    objcopy = TOOLCHAINS[toolchain] / "objcopy.exe"
    bin_path = obj.with_suffix(".textbin")
    proc = run([str(objcopy), "-O", "binary", "--only-section=.text", str(obj), str(bin_path)])
    if proc.returncode != 0 or not bin_path.exists():
        return b""
    data = bin_path.read_bytes()
    return data


RELOC_WIDTH = {
    "dir32": 4, "DIR32": 4, "dir32nb": 4, "DIR32NB": 4,
    "rel32": 4, "REL32": 4, "secrel32": 4, "SECREL32": 4,
}


def mask(data, relocs_in_func, func_start):
    b = bytearray(data)
    for off, rtype, _sym in relocs_in_func:
        width = RELOC_WIDTH.get(rtype, 4)
        local = off - func_start
        for i in range(local, min(local + width, len(b))):
            if 0 <= i < len(b):
                b[i] = 0
    return bytes(b)


_cs = None


def get_capstone():
    global _cs
    if _cs is None and capstone is not None:
        _cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        _cs.detail = False
    return _cs


def mnemonics(data):
    cs = get_capstone()
    if cs is None or not data:
        return []
    return [insn.mnemonic for insn in cs.disasm(data, 0)]


def seq_ratio(a, b):
    if not a and not b:
        return 1.0
    # simple LCS-based ratio, fine for short sequences (<400 insns here)
    n, m = len(a), len(b)
    if n == 0 or m == 0:
        return 0.0
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            if a[i - 1] == b[j - 1]:
                dp[i][j] = dp[i - 1][j - 1] + 1
            else:
                dp[i][j] = max(dp[i - 1][j], dp[i][j - 1])
    lcs = dp[n][m]
    return 2.0 * lcs / (n + m)


def evaluate_function(exe_data, toolchain, group_label, flags_label, func_name,
                       target, sym_addr, sym_end, text_bytes, relocs):
    va = int(target["va"], 16) if isinstance(target["va"], str) else target["va"]
    size_orig = target["size"]
    off = file_offset(va)
    orig = exe_data[off: off + size_orig]

    mine = text_bytes[sym_addr:sym_end]
    size_mine = len(mine)

    relocs_in_func = [r for r in relocs if sym_addr <= r[0] < sym_end]
    mine_masked = mask(mine, relocs_in_func, sym_addr)
    # apply the SAME relative offsets/widths to the original bytes (best
    # effort -- only valid if orig is at least as long)
    orig_masked = bytearray(orig)
    for off_r, rtype, _sym in relocs_in_func:
        width = RELOC_WIDTH.get(rtype, 4)
        local = off_r - sym_addr
        for i in range(local, min(local + width, len(orig_masked))):
            if 0 <= i < len(orig_masked):
                orig_masked[i] = 0
    orig_masked = bytes(orig_masked)

    byte_exact = (size_mine == size_orig) and (mine_masked == orig_masked)

    mn_mine = mnemonics(mine)
    mn_orig = mnemonics(orig)
    mratio = seq_ratio(mn_mine, mn_orig)

    return {
        "toolchain": toolchain,
        "group": group_label,
        "flags": flags_label,
        "function": func_name,
        "va": hex(va),
        "size_orig": size_orig,
        "size_mine": size_mine,
        "n_relocs": len(relocs_in_func),
        "byte_exact_masked": byte_exact,
        "mnemonic_seq_ratio": round(mratio, 4),
        "mnemonic_len_mine": len(mn_mine),
        "mnemonic_len_orig": len(mn_orig),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(REPO / "artifacts" / "toolchain_fingerprint.json"))
    ap.add_argument("--work", default=str(REPO / "third_party" / "fingerprint_work"))
    ap.add_argument("--groups", nargs="*", help="restrict to these group names")
    args = ap.parse_args()

    if capstone is None:
        print("ERROR: capstone not installed (pip install capstone)", file=sys.stderr)
        sys.exit(1)

    exe_data = EXE_PATH.read_bytes()
    targets = load_targets()
    work_root = Path(args.work)

    results = []
    groups = GROUPS
    if args.groups:
        groups = [g for g in GROUPS if g["group"] in args.groups]

    for group in groups:
        group_label = group["group"]
        for toolchain in TOOLCHAINS:
            for flags_label, flags in flag_combos():
                out_dir = work_root / toolchain / group_label.replace("/", "_") / flags_label.replace("/", "_")
                obj, log = compile_unit(toolchain, group, flags, out_dir)
                if obj is None:
                    print(f"COMPILE FAIL {toolchain} {group_label} {flags_label}: {log[:300]}", file=sys.stderr)
                    continue
                syms = read_symbols(toolchain, obj)
                relocs = read_relocations(toolchain, obj)
                text_bytes = extract_text(toolchain, obj)
                sym_by_name = {n: i for i, (a, n) in enumerate(syms)}
                for func_name in group["functions"]:
                    if func_name not in sym_by_name or func_name not in targets:
                        continue
                    idx = sym_by_name[func_name]
                    sym_addr = syms[idx][0]
                    sym_end = syms[idx + 1][0] if idx + 1 < len(syms) else len(text_bytes)
                    res = evaluate_function(
                        exe_data, toolchain, group_label, flags_label, func_name,
                        targets[func_name], sym_addr, sym_end, text_bytes, relocs,
                    )
                    results.append(res)
        print(f"done group {group_label}: {sum(1 for r in results if r['group']==group_label)} rows", file=sys.stderr)

    Path(args.out).write_text(json.dumps(results, indent=2), encoding="utf-8")
    print(f"wrote {args.out} ({len(results)} rows)", file=sys.stderr)

    # summary: best (byte_exact first, then mnemonic ratio) per function per toolchain
    best = {}
    for r in results:
        key = (r["group"], r["function"], r["toolchain"])
        cur = best.get(key)
        score = (1 if r["byte_exact_masked"] else 0, r["mnemonic_seq_ratio"])
        if cur is None or score > cur[0]:
            best[key] = (score, r)
    print("\n=== BEST PER FUNCTION/TOOLCHAIN ===")
    for (group_label, func_name, toolchain), (score, r) in sorted(best.items()):
        print(f"{group_label:32s} {func_name:28s} {toolchain:6s} flags={r['flags']:16s} "
              f"byte_exact={r['byte_exact_masked']} mnem_ratio={r['mnemonic_seq_ratio']:.3f} "
              f"len(mine={r['size_mine']},orig={r['size_orig']})")


if __name__ == "__main__":
    main()
