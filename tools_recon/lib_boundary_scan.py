#!/usr/bin/env python3
"""lib_boundary_scan.py -- ownership map + boundary-crossing inventory.

Read-only over artifacts/. Produces artifacts/lib_boundary.json:

  ownership          .text bytes/functions per CU family
  game_to_lib        every direct call from a game function to a non-game one
  lib_to_game        callbacks: game function VAs materialised as immediates
                     inside game code, and which library function receives them
  shared_globals     library-owned data touched by game code, and game-owned
                     data touched by library code
  internal_edges     game -> library symbols that are not public Allegro API
  fnptr_tables       .data/.rdata dwords that are function entry points,
                     grouped by owning data symbol, with cross-family flags

Usage:  python tools_recon/lib_boundary_scan.py [--root <repo root>]
"""
import argparse
import bisect
import collections
import json
import os
import re
import sys

# ---------------------------------------------------------------- families

ALLEGRO_PUBLIC_PREFIXES = ()  # see is_internal_symbol()


# basenames of the 25 game CUs (compile_units.txt), for bare COFF File records
GAME_BASENAMES = {
    "beta.c", "control.c", "csv.c", "custom.c", "directories.c",
    "fld_adspot.c", "game_data.c", "hisc.c", "httpget.c", "loadpng.c",
    "main.c", "map.c", "menu.c", "options.c", "particle.c", "player.c",
    "profile.c", "regpng.c", "replay.c", "savepng.c", "scroller.c",
    "stars.c", "strptime.c", "timecompat.c", "timer.c",
}


def classify(cu, origin, bare_ok=False):
    """CU path + coarse origin -> (family, subfamily).

    bare_ok: accept a bare COFF File basename ("main.c") as a game CU.  Only
    safe for DATA symbols; MinGW's own libmingw32 main.c collides with the
    game's main.c in .text (_main at 0x4b2740 is the CRT's, not the game's).
    """
    c = (cu or "").replace("\\", "/").lower()
    base = c.rsplit("/", 1)[-1]

    if "projects/icytower/trunk/source" in c or (bare_ok and c == base and
                                                 base in GAME_BASENAMES):
        vendored = {
            "loadpng.c": "loadpng addon (vendored)",
            "savepng.c": "loadpng addon (vendored)",
            "regpng.c": "loadpng addon (vendored)",
            "strptime.c": "BSD/glibc strptime (vendored)",
            "csv.c": "csv helper (unclear)",
            "httpget.c": "http helper (unclear)",
            "timecompat.c": "time helper (unclear)",
        }
        if base in vendored:
            return "game_vendored", vendored[base]
        return "game", "icytower"

    if "allegro4/addons/logg" in c:
        return "allegro_addon", "logg"
    if "allegro4/src/win/" in c:
        return "allegro_win", "win drivers"
    if "allegro4/src/c/" in c:
        return "allegro_c", "C blitters"
    if "allegro4/src/misc/" in c:
        return "allegro_misc", "misc"
    if "allegro4/src/" in c:
        return "allegro_core", "core"

    if base in ("framing.c", "bitwise.c"):
        return "libogg", "libogg"
    VORBIS = {
        "analysis.c", "bitrate.c", "block.c", "codebook.c", "envelope.c",
        "floor0.c", "floor1.c", "info.c", "lpc.c", "lsp.c", "mapping0.c",
        "mdct.c", "psy.c", "registry.c", "res0.c", "sharedbook.c",
        "smallft.c", "synthesis.c", "window.c", "misc.c", "sharedbook.c",
    }
    if base == "vorbisfile.c":
        return "libvorbisfile", "libvorbisfile"
    if base in VORBIS:
        return "libvorbis", "libvorbis"

    if base in ("dinput.c", "dxguid.c"):
        return "dx_importlib", "DirectX import libs"

    if origin == "libgcc/crt":
        return "crt", origin
    # remaining 'other' COFF files are mingw CRT bits
    return "crt", "mingw CRT/misc"


PUBLIC_ALLEGRO_DATA = {
    # public Allegro globals documented in allegro.h / the manual
    "_key", "_key_shifts", "_mouse_x", "_mouse_y", "_mouse_z", "_mouse_b",
    "_mouse_pos", "_mouse_sprite", "_screen", "_font", "_black_palette",
    "_desktop_palette", "_default_palette", "_joy", "_num_joysticks",
    "_num_joystick_buttons", "_allegro_error", "_allegro_errno",
    "_gfx_capabilities", "_palette_color", "_gfx_driver", "_digi_driver",
    "_midi_driver", "_keyboard_driver", "_mouse_driver", "_joystick_driver",
    "_timer_driver", "_system_driver", "_gfx_driver_list", "_os_type",
    "_os_version", "_os_revision", "_os_multitasking", "_cpu_family",
    "_cpu_model", "_cpu_capabilities", "_cpu_vendor",
    "_midi_pos", "_midi_time", "_midi_loop_start", "_midi_loop_end",
    "_midi_card", "_digi_card", "_midi_input_card", "_digi_input_card",
    "_allegro_id", "_allegro_404_char",
    "_screen_w", "_screen_h", "_virtual_w", "_virtual_h",
    "_retrace_count", "_retrace_proc", "_keyboard_callback",
    "_keyboard_ucallback", "_keyboard_lowlevel_callback",
    "_mouse_callback", "_three_finger_flag", "_key_led_flag",
    "_set_uformat", "_errno",
}


def is_internal_symbol(name):
    """True for a library symbol that is NOT part of the documented API.

    MinGW prefixes every C symbol with one '_'.  Allegro's own internal
    symbols additionally start with '_' in the source, so they show up with
    two leading underscores; a few (_handle_timer_tick style) are declared in
    allegro/internal/aintern.h.
    """
    if not name.startswith("_"):
        return False
    stripped = name[1:]
    return stripped.startswith("_")


# ---------------------------------------------------------------- loading

def load_functions(root):
    fns = json.load(open(os.path.join(root, "artifacts", "functions.json")))
    out = []
    for f in fns:
        va = int(f["va"], 16)
        fam, sub = classify(f.get("compile_unit"), f.get("origin"))
        out.append({
            "name": f["name"], "va": va, "size": f["size"],
            "cu": f.get("compile_unit"), "origin": f.get("origin"),
            "family": fam, "sub": sub, "source": f.get("source"),
        })
    out.sort(key=lambda x: x["va"])
    return out


DW_CU = re.compile(r"^\s*<(\d+)><([0-9a-f]+)>: Abbrev Number: \d+"
                   r"(?: \((DW_TAG_\w+)\))?")
DW_ATTR = re.compile(r"^\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*)$")
DW_ADDR = re.compile(r"DW_OP_addr:\s*([0-9a-f]+)")


def load_dwarf_globals(root):
    """VA -> (name, CU path) for every DW_TAG_variable with DW_OP_addr."""
    out = {}
    cu = None
    cur = None

    def flush():
        if cur and cur.get("addr") is not None and cur.get("name"):
            out.setdefault(cur["addr"], (cur["name"], cur["cu"]))

    with open(os.path.join(root, "artifacts", "dwarf_info.txt"),
              errors="replace") as fh:
        for line in fh:
            m = DW_CU.match(line)
            if m:
                flush()
                tag = m.group(3)
                if tag == "DW_TAG_compile_unit":
                    cu = None
                    cur = {"kind": "cu"}
                elif tag == "DW_TAG_variable":
                    cur = {"kind": "var", "name": None, "addr": None, "cu": cu}
                else:
                    cur = None
                continue
            m = DW_ATTR.match(line)
            if m and cur:
                k, v = m.group(1), m.group(2).strip()
                if cur["kind"] == "cu":
                    if k == "DW_AT_name":
                        cu = v
                else:
                    if k == "DW_AT_name":
                        cur["name"] = v
                    elif k == "DW_AT_location":
                        mm = DW_ADDR.search(v)
                        if mm:
                            cur["addr"] = int(mm.group(1), 16)
    flush()
    return out


# Data symbols with no DWARF CU: name-based attribution of last resort.
VORBIS_DATA_PREFIX = ("_vorbis_", "__vorbis_", "_ov_", "_floor", "_res",
                      "_mapping", "_vorbis")
OGG_DATA_PREFIX = ("_ogg_", "_oggpack")


def load_data_symbols(root, sections):
    syms = json.load(open(os.path.join(root, "artifacts", "coff_symbols.json")))
    dwg = load_dwarf_globals(root)
    data_secs = {".data", ".rdata", ".bss"}
    ds = []
    for s in syms:
        if s.get("is_section_symbol"):
            continue
        if s.get("section_name") not in data_secs:
            continue
        va = s["va"]
        cu = None
        attribution = None
        if va in dwg:
            cu = dwg[va][1]
            attribution = "dwarf"
        elif "." in s["name"]:
            # a function-local static: the COFF File record is trustworthy
            cu = s.get("file")
            attribution = "coff_static"
        if cu:
            fam, sub = classify(cu, None, bare_ok=True)
        else:
            nm = s["name"]
            if nm.startswith(VORBIS_DATA_PREFIX):
                fam, sub, attribution = "libvorbis", "libvorbis", "name"
            elif nm.startswith(OGG_DATA_PREFIX):
                fam, sub, attribution = "libogg", "libogg", "name"
            else:
                fam, sub, attribution = "unknown", "unattributed", "none"
        ds.append({
            "name": s["name"], "va": va, "sec": s["section_name"],
            "file": cu or s.get("file"), "family": fam, "sub": sub,
            "attribution": attribution,
        })
    ds.sort(key=lambda x: (x["va"], x["name"]))
    # size = distance to the next distinct VA in the same section
    for i, s in enumerate(ds):
        s["size"] = 4
        for j in range(i + 1, len(ds)):
            if ds[j]["sec"] != s["sec"]:
                continue
            if ds[j]["va"] > s["va"]:
                s["size"] = ds[j]["va"] - s["va"]
                break
    return ds


# ---------------------------------------------------------------- disasm

FUNC_HDR = re.compile(r"^([0-9a-f]{8}) <(.+)>:$")
INSN = re.compile(r"^\s+([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$")
CALL_DIRECT = re.compile(r"^(call|jmp)\s+([0-9a-f]+) <([^>]+)>")
CALL_IND_ABS = re.compile(r"^call\s+\*(0x[0-9a-f]+)")
IMM = re.compile(r"\$0x([0-9a-f]{5,8})")
ABSMEM = re.compile(r"(?<![$\w])0x([0-9a-f]{6,8})(?!\()")


def parse_disasm(path):
    """Yield (func_va, func_name, insn_va, mnemonic_and_operands)."""
    cur_va = None
    cur_name = None
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            m = FUNC_HDR.match(line)
            if m:
                cur_va = int(m.group(1), 16)
                cur_name = m.group(2)
                continue
            m = INSN.match(line)
            if m and cur_va is not None:
                yield cur_va, cur_name, int(m.group(1), 16), m.group(3).strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))))
    args = ap.parse_args()
    root = args.root

    sections = {
        ".text": (0x401000, 0x4BB048),
        ".data": (0x4BC000, 0x4D36F4),
        ".rdata": (0x4D4000, 0x4DC984),
        ".bss": (0x4DD000, 0x513978),
        ".idata": (0x514000, 0x516494),
    }

    fns = load_functions(root)

    # ---- reclassify the linker-generated import thunks (ff 25 -> .idata).
    # They carry no COFF File record and land in the trailing block that
    # objdump attributes to cygming-crtend.c, which would otherwise count
    # every DLL import the game makes as "CRT code".
    import pefile as _pefile
    _pe = _pefile.PE(os.path.join(root, "assets", "icytower15.exe"))
    _text = None
    for s in _pe.sections:
        if s.Name.decode().rstrip("\x00") == ".text":
            _text = (0x400000 + s.VirtualAddress, s.get_data())
    _iat = {}
    if hasattr(_pe, "DIRECTORY_ENTRY_IMPORT"):
        for entry in _pe.DIRECTORY_ENTRY_IMPORT:
            dll = entry.dll.decode()
            for imp in entry.imports:
                _iat[imp.address] = (dll, (imp.name or b"").decode()
                                     if imp.name else "ord%d" % (imp.ordinal or 0))
    _pe.close()
    n_thunk = 0
    for f in fns:
        off = f["va"] - _text[0]
        if off < 0 or off + 6 > len(_text[1]):
            continue
        b = _text[1][off:off + 6]
        if b[0] == 0xFF and b[1] == 0x25:
            slot = int.from_bytes(b[2:6], "little")
            if 0x514000 <= slot < 0x516494:
                dll, nm = _iat.get(slot, ("?", "?"))
                f["family"] = "import_thunk"
                f["sub"] = dll.lower()
                f["import"] = "%s!%s" % (dll, nm)
                f["iat_slot"] = "0x%x" % slot
                n_thunk += 1
    print("import thunks reclassified:", n_thunk)

    fn_va = [f["va"] for f in fns]
    by_va = {f["va"]: f for f in fns}

    def owner(va):
        i = bisect.bisect_right(fn_va, va) - 1
        if i < 0:
            return None
        f = fns[i]
        if va < f["va"] + max(f["size"], 1):
            return f
        return f if va < f["va"] + 64 else None  # tolerate size==0 records

    dsyms = load_data_symbols(root, sections)
    dva = [d["va"] for d in dsyms]
    data_diag = {
        "symbols": len(dsyms),
        "by_family": dict(collections.Counter(d["family"] for d in dsyms)),
        "by_attribution": dict(collections.Counter(d["attribution"]
                                                   for d in dsyms)),
    }

    def data_owner(va):
        i = bisect.bisect_right(dva, va) - 1
        if i < 0:
            return None
        d = dsyms[i]
        if va < d["va"] + max(d["size"], 4):
            return d
        return None

    # ---------------- ownership map
    own = collections.defaultdict(lambda: {"functions": 0, "bytes": 0,
                                           "cus": set(), "subs": set()})
    for f in fns:
        e = own[f["family"]]
        e["functions"] += 1
        e["bytes"] += f["size"]
        e["cus"].add(f["cu"])
        e["subs"].add(f["sub"])
    ownership = {}
    total_bytes = sum(f["size"] for f in fns)
    for k, v in sorted(own.items(), key=lambda kv: -kv[1]["bytes"]):
        ownership[k] = {
            "functions": v["functions"], "bytes": v["bytes"],
            "pct_text": round(100.0 * v["bytes"] / total_bytes, 2),
            "cu_count": len(v["cus"]),
            "cus": sorted(x for x in v["cus"] if x),
            "subfamilies": sorted(v["subs"]),
        }

    unattributed_touched = collections.Counter()
    unattributed_by_game = collections.Counter()

    # ---------------- iterate the disassembly once
    game_fams = {"game", "game_vendored"}

    g2l = collections.defaultdict(lambda: {"count": 0, "callers": set(),
                                           "sites": []})
    l2g_call = collections.defaultdict(lambda: {"count": 0, "callers": set()})
    imm_refs = []            # game-fn VA materialised as an immediate
    pending_imm = []         # (insn_va, target_fn, caller_fn) awaiting a call
    game_reads_lib = collections.defaultdict(
        lambda: {"count": 0, "callers": set()})
    lib_reads_game = collections.defaultdict(
        lambda: {"count": 0, "callers": set()})
    iat_calls = collections.defaultdict(lambda: {"count": 0, "callers": set()})
    tail_jmp_g2l = collections.defaultdict(lambda: {"count": 0,
                                                    "callers": set()})

    for fva, fname, iva, text in parse_disasm(
            os.path.join(root, "artifacts", "disasm.txt")):
        caller = by_va.get(fva)
        if caller is None:
            continue
        cfam = caller["family"]
        caller_is_game = cfam in game_fams

        m = CALL_DIRECT.match(text)
        if m:
            kind = m.group(1)
            tva = int(m.group(2), 16)
            tname = m.group(3)
            callee = by_va.get(tva) or owner(tva)
            tfam = callee["family"] if callee else "unknown"
            if caller_is_game and tfam not in game_fams:
                bucket = g2l if kind == "call" else tail_jmp_g2l
                e = bucket[tname]
                e["count"] += 1
                e["callers"].add(caller["name"])
                e.setdefault("caller_families", set()).add(cfam)
                if kind == "call":
                    e.setdefault("va", tva)
                    e.setdefault("family", tfam)
                    e.setdefault("cu", callee["cu"] if callee else None)
                    if callee and callee.get("import"):
                        e.setdefault("import", callee["import"])
                else:
                    e.setdefault("va", tva)
                    e.setdefault("family", tfam)
            elif (not caller_is_game) and tfam in game_fams:
                e = l2g_call[tname]
                e["count"] += 1
                e["callers"].add(caller["name"])
                e.setdefault("va", tva)
                e.setdefault("caller_family", cfam)
            # drain pending immediates: a call right after a push imm
            if pending_imm:
                for p in pending_imm:
                    p["receiver"] = tname
                    p["receiver_family"] = tfam
                    imm_refs.append(p)
                pending_imm = []
            continue

        m = CALL_IND_ABS.match(text)
        if m:
            slot = int(m.group(1), 16)
            e = iat_calls["slot_%08x" % slot]
            e["count"] += 1
            e["callers"].add(caller["name"])
            e.setdefault("caller_family", cfam)
            if pending_imm:
                for p in pending_imm:
                    p["receiver"] = "*0x%08x" % slot
                    p["receiver_family"] = "import/vtable"
                    imm_refs.append(p)
                pending_imm = []
            continue

        # ---- immediates that are function entry points
        if caller_is_game:
            for mm in IMM.finditer(text):
                v = int(mm.group(1), 16)
                t = by_va.get(v)
                if t is not None and t["family"] in game_fams:
                    pending_imm.append({
                        "site_va": "0x%x" % iva,
                        "in_function": caller["name"],
                        "in_function_va": "0x%x" % fva,
                        "target": t["name"],
                        "target_va": "0x%x" % v,
                        "insn": text,
                        "receiver": None,
                        "receiver_family": None,
                    })
            if len(pending_imm) > 8:
                imm_refs.extend(pending_imm)
                pending_imm = []

        # ---- absolute data references
        if "call" in text or "jmp" in text:
            continue
        for mm in ABSMEM.finditer(text):
            v = int(mm.group(1), 16)
            if not (0x4BC000 <= v < 0x514000):
                continue
            d = data_owner(v)
            if d is None:
                continue
            dfam = d["family"]
            if caller_is_game and dfam not in game_fams and dfam != "unknown":
                e = game_reads_lib[d["name"]]
                e["count"] += 1
                e["callers"].add(caller["name"])
                e.setdefault("va", "0x%x" % d["va"])
                e.setdefault("family", dfam)
                e.setdefault("sec", d["sec"])
                e.setdefault("cu", d["file"])
            elif dfam == "unknown" and caller_is_game:
                unattributed_by_game[d["name"]] += 1
            elif dfam == "unknown" and not caller_is_game:
                unattributed_touched[d["name"]] += 1
            elif (not caller_is_game) and dfam in game_fams:
                e = lib_reads_game[d["name"]]
                e["count"] += 1
                e["callers"].add(caller["name"])
                e.setdefault("va", "0x%x" % d["va"])
                e.setdefault("caller_family", cfam)
                e.setdefault("cu", d["file"])

    if pending_imm:
        imm_refs.extend(pending_imm)

    # ---------------- function-pointer tables in .data/.rdata
    import pefile
    pe = pefile.PE(os.path.join(root, "assets", "icytower15.exe"))
    fnptr = collections.defaultdict(list)
    entry = {f["va"]: f for f in fns}
    for secname in (".data", ".rdata"):
        for s in pe.sections:
            if s.Name.decode().rstrip("\x00") != secname:
                continue
            base = 0x400000 + s.VirtualAddress
            raw = s.get_data()
            for off in range(0, len(raw) - 3, 4):
                v = int.from_bytes(raw[off:off + 4], "little")
                t = entry.get(v)
                if t is None:
                    continue
                addr = base + off
                d = data_owner(addr)
                holder = d["name"] if d else "?"
                hfam = d["family"] if d else "unknown"
                fnptr[holder].append({
                    "holder_va": "0x%x" % (d["va"] if d else addr),
                    "holder_family": hfam,
                    "holder_cu": d["file"] if d else None,
                    "slot_va": "0x%x" % addr,
                    "slot_off": (addr - d["va"]) if d else 0,
                    "target": t["name"],
                    "target_va": "0x%x" % v,
                    "target_family": t["family"],
                })
    pe.close()

    cross = {}
    for holder, slots in fnptr.items():
        hf = slots[0]["holder_family"]
        mixed = [s for s in slots
                 if (hf in game_fams) != (s["target_family"] in game_fams)]
        if mixed:
            cross[holder] = mixed

    # ---------------- internal-symbol edges
    LIBFAM = {"allegro_core", "allegro_c", "allegro_win", "allegro_misc",
              "allegro_addon", "libogg", "libvorbis", "libvorbisfile"}
    internal = {}
    for name, e in g2l.items():
        if is_internal_symbol(name) and e.get("family") in LIBFAM:
            internal[name] = {
                "va": "0x%x" % e["va"], "family": e.get("family"),
                "cu": e.get("cu"), "count": e["count"],
                "callers": sorted(str(x) for x in e["callers"]),
            }
    internal_data = {}
    for name, e in game_reads_lib.items():
        if (is_internal_symbol(name) and name not in PUBLIC_ALLEGRO_DATA
                and e.get("family") in LIBFAM):
            internal_data[name] = {
                "va": e["va"], "family": e.get("family"),
                "cu": e.get("cu"), "count": e["count"],
                "readers": sorted(str(x) for x in e["callers"]),
            }

    def dump(d, keyname="callers"):
        out = {}
        for k, v in sorted(d.items(), key=lambda kv: -kv[1]["count"]):
            vv = dict(v)
            vv[keyname] = sorted(str(x) for x in v["callers"])
            if isinstance(vv.get("caller_families"), set):
                vv["caller_families"] = sorted(vv["caller_families"])
            if keyname != "callers":
                vv.pop("callers", None)
            if "va" in vv and isinstance(vv["va"], int):
                vv["va"] = "0x%x" % vv["va"]
            vv.pop("sites", None)
            out[k] = vv
        return out

    fam_of_callee = collections.Counter()
    for name, e in g2l.items():
        fam_of_callee[e.get("family")] += 1

    # ---------------- summary slices
    LIBFAMS = {"allegro_core", "allegro_c", "allegro_win", "allegro_misc",
               "allegro_addon", "libogg", "libvorbis", "libvorbisfile"}
    dll_surface = collections.Counter()
    dll_by_caller = collections.defaultdict(collections.Counter)
    for name, e in g2l.items():
        if e.get("family") != "import_thunk":
            continue
        dll = (e.get("import") or "?!?").split("!")[0]
        dll_surface[dll] += 1
        for cf in e.get("caller_families", set()):
            dll_by_caller[cf][dll] += 1
    allegro_api = sorted(set(
        [n for n, e in g2l.items() if e.get("family") in LIBFAMS] +
        [n for n, e in tail_jmp_g2l.items() if e.get("family") in LIBFAMS]))
    allegro_api_from_pure_game = sorted(
        n for n, e in g2l.items()
        if e.get("family") in LIBFAMS and "game" in e.get("caller_families", ()))
    cb_lib = [r for r in imm_refs
              if r.get("receiver_family") not in ("game", "game_vendored")]
    cb_game = [r for r in imm_refs
               if r.get("receiver_family") in ("game", "game_vendored")]

    summary = {
        "distinct_library_functions_called_by_game": len(allegro_api),
        "allegro_family_api_names": allegro_api,
        "allegro_api_reached_from_non_vendored_game_cus":
            len(allegro_api_from_pure_game),
        "distinct_dll_imports_called_by_game": sum(dll_surface.values()),
        "dll_imports_by_dll": dict(dll_surface),
        "dll_imports_by_caller_family":
            {k: dict(v) for k, v in dll_by_caller.items()},
        "callbacks_into_game_from_library_registration": len(cb_lib),
        "callbacks_game_to_game": len(cb_game),
        "distinct_game_functions_passed_to_libraries":
            len(set(r["target"] for r in cb_lib)),
        "library_globals_touched_by_game": len(game_reads_lib),
        "game_globals_touched_by_library": len(lib_reads_game),
        "internal_library_symbol_edges_functions": len(internal),
        "internal_library_symbol_edges_data": len(internal_data),
        "fnptr_tables_with_cross_family_slots": len(cross),
    }

    result = {
        "generated_by": "tools_recon/lib_boundary_scan.py",
        "summary": summary,
        "inputs": ["artifacts/functions.json", "artifacts/disasm.txt",
                   "artifacts/coff_symbols.json", "assets/icytower15.exe"],
        "text_total_bytes": total_bytes,
        "ownership": ownership,
        "game_to_lib": {
            "distinct_callees": len(g2l),
            "total_call_sites": sum(e["count"] for e in g2l.values()),
            "callees_by_family": dict(fam_of_callee),
            "callees": dump(g2l),
        },
        "game_to_lib_tailjmp": {
            "distinct_callees": len(tail_jmp_g2l),
            "callees": dump(tail_jmp_g2l),
        },
        "lib_to_game_direct_calls": {
            "distinct": len(l2g_call),
            "callees": dump(l2g_call),
        },
        "callbacks": {
            "count": len(imm_refs),
            "to_library_count": len(cb_lib),
            "to_game_count": len(cb_game),
            "by_receiver": dict(collections.Counter(
                r["receiver"] or "<no call seen>" for r in imm_refs)),
            "refs": sorted(imm_refs, key=lambda r: r["site_va"]),
        },
        "data_symbol_attribution": data_diag,
        "unattributed_data_touched_by_game_code": {
            "distinct": len(unattributed_by_game),
            "all": dict(unattributed_by_game.most_common()),
        },
        "unattributed_data_touched_by_library_code": {
            "distinct": len(unattributed_touched),
            "top": dict(unattributed_touched.most_common(30)),
        },
        "shared_globals": {
            "lib_globals_touched_by_game": {
                "distinct": len(game_reads_lib),
                "total_sites": sum(e["count"] for e in game_reads_lib.values()),
                "globals": dump(game_reads_lib, "readers"),
            },
            "game_globals_touched_by_lib": {
                "distinct": len(lib_reads_game),
                "total_sites": sum(e["count"] for e in lib_reads_game.values()),
                "globals": dump(lib_reads_game, "readers"),
            },
        },
        "internal_symbol_edges": {
            "functions": internal,
            "data": internal_data,
        },
        "indirect_iat_calls_from_game": {
            k: {"count": v["count"], "callers": sorted(str(x) for x in v["callers"]),
                "caller_family": v.get("caller_family")}
            for k, v in sorted(iat_calls.items(), key=lambda kv: -kv[1]["count"])
            if v.get("caller_family") in game_fams
        },
        "fnptr_tables": {
            "tables_with_cross_family_slots": len(cross),
            "cross": cross,
            "all_table_holders": {
                k: {"slots": len(v), "holder_family": v[0]["holder_family"],
                    "holder_cu": v[0]["holder_cu"],
                    "target_families": sorted(set(s["target_family"] for s in v))}
                for k, v in sorted(fnptr.items(), key=lambda kv: -len(kv[1]))
            },
        },
    }

    outp = os.path.join(root, "artifacts", "lib_boundary.json")
    with open(outp, "w") as fh:
        json.dump(result, fh, indent=1, sort_keys=False)
    print("wrote", outp)
    print("ownership:")
    for k, v in ownership.items():
        print("  %-16s %5d fn %8d B %6.2f%%  %d CUs"
              % (k, v["functions"], v["bytes"], v["pct_text"], v["cu_count"]))
    print("game->lib distinct callees:", len(g2l),
          "sites:", sum(e["count"] for e in g2l.values()))
    print("  by family:", dict(fam_of_callee))
    print("lib->game direct calls:", len(l2g_call))
    print("callback immediates:", len(imm_refs))
    print("lib globals touched by game:", len(game_reads_lib))
    print("game globals touched by lib:", len(lib_reads_game))
    print("internal fn edges:", len(internal), "internal data:",
          len(internal_data))
    print("fnptr tables with cross-family slots:", len(cross))


if __name__ == "__main__":
    sys.exit(main())
