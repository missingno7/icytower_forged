#!/usr/bin/env python3
"""batch14b_check.py -- ordered-call-trace oracle for PROMOTIONS.md
batch 14's eight play()-coastline functions whose whole effect is a
sequence of library calls:

    destroy_replay  0x41bd68    54 B   src/icytower/replay.c
    save_replay     0x41dd78  1227 B   src/icytower/replay.c
    myDeleteFile    0x40cd28    63 B   src/icytower/config.c
    save_config     0x40e130   154 B   src/icytower/config.c
    init_scroller   0x41f278   200 B   src/icytower/scroller.c
    fadeOut         0x40bf5c   609 B   src/icytower/fade.c
    fadeIn          0x40c1c0   424 B   src/icytower/fade.c
    save_profile    0x41a3b8  1073 B   src/icytower/profile.c

Why this is not a lift_check.py SPECS entry
-------------------------------------------
Same reasons batches 10/12/13 gave, and one of them is decisive here:
mechanism B records per callee a COUNT plus the arguments of its FIRST
call.  save_replay() makes over five hundred pack_fwrite() calls with a
different buffer each time -- the sequence of those buffers IS the .itr
file -- and the two fades make one nine-call cycle per animation step.
First-call capture would compare a fraction of either.
icytower_specs.py is untouched again, the fifth batch running that way.

How it works
------------
1. The ORIGINAL bytes run under unicorn on the shared
   port_forge/tools/pf_win32_offline_oracle engine.  Every library, CRT
   and unpromoted game callee is hooked at its own VA and stubbed,
   appending one record per call, IN ORDER, with its arguments.
2. Callees NOT reached by a direct `call <va>`:
     - rectfill() and draw_sprite() inside the two fades, which are
       Allegro AL_INLINEs dispatching through GFX_VTABLE's +0x3c / +0x44
       / +0x48 slots.  Batch 9's synthetic-vtable-VA trick, the same one
       draw_frame_xcheck.py uses.
3. Two callees are deliberately NOT stubbed and run for real on both
   sides, because this batch promotes them too and an independent
   oracle already covers them:
     - calc_replay_checksum(), which save_replay() calls; its value is
       compared as DATA, inside the bytes of the pack_fwrite that writes
       it.
     - get_rank()/get_rank_id(), which save_profile() inlines; the rank
       tables are pinned flat on both sides so the rank search does not
       add noise here.
   strcpy() is hooked on the ORIGINAL side but records NOTHING -- see
   pf_harness_batch14.h's note 2 for why tracing it would be asymmetric.
4. The compiled candidate is batch14b_check.exe (build_batch14.sh),
   driven by a vector FILE, one line per vector -- one process for the
   whole run.
5. Pointers are rendered as stable SYMBOLS and strings as their CONTENT
   AT CALL TIME.  pack_fwrite/fwrite buffers are rendered as HEX, since
   what is being compared is the file those calls produce.

Vector budget
-------------
The kinds cost wildly different amounts of emulation: one save_replay
vector is ~550 traced calls and one fadeOut vector at speed 1 would be
~2300, against 2-6 for destroy_replay or myDeleteFile.  `--vectors N` is
therefore split by the WEIGHT table below rather than evenly, and the
per-kind count actually run is printed with each result line so the
number is never implicit.

Usage
-----
  python batch14b_check.py --vectors 20000 --seed 20260908
  python batch14b_check.py --fault
"""
import argparse
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(PROJ, "port_forge", "tools"))
import pf_win32_offline_oracle as pf                      # noqa: E402
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE   # noqa: E402
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EAX,  # noqa: E402
                               UC_X86_REG_EIP)

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")
CHECK_EXE = os.path.join(HERE, "batch14b_check.exe")
VECFILE = os.path.join(HERE, "batch14b_vectors.txt")

FN = {
    "destroy_replay": 0x41bd68,
    "save_replay":    0x41dd78,
    "myDeleteFile":   0x40cd28,
    "save_config":    0x40e130,
    "init_scroller":  0x41f278,
    "fadeOut":        0x40bf5c,
    "fadeIn":         0x40c1c0,
    "save_profile":   0x41a3b8,
}

# ------------------------------------------------------------- globals
G_GFX_DRIVER  = 0x4dda84
G_SCREEN      = 0x4dda8c
G_SWAP_SCREEN = 0x4dd194
G_CYCLE_COUNT = 0x506938
G_OPTIONS     = 0x4fe528
G_HISC_TABLES = 0x4dd1c0        # Thisc_table *[15]
G_RANK_FLOORS = 0x4bdc20
G_RANK_COMBOS = 0x4bdc60
G_RANK_NMLS   = 0x4bdce0
G_RANK_CCCS   = 0x4bdca0
G_RANK_LABELS = 0x4bdbe0

# --------------------------------------------------------- scratch VAs
S = 0x7a0000
S_SWAP    = S + 0x0000          # BITMAP swap_screen
S_SCREEN  = S + 0x0080          # BITMAP screen
S_TMP     = S + 0x0100          # BITMAP create_bitmap() hands back
S_SRC     = S + 0x0180          # BITMAP fadeIn's source
S_VT8     = S + 0x0200          # GFX_VTABLE, color_depth 8
S_VT16    = S + 0x0400          # GFX_VTABLE, color_depth 16
S_GFX     = S + 0x0600          # GFX_DRIVER (w +0x6c, h +0x70)
S_FONT    = S + 0x0700
S_CTRL    = S + 0x0740
S_TM      = S + 0x0780          # struct tm localtime() returns
S_FILE    = S + 0x07c0          # FILE * fopen() hands back
S_PACK    = S + 0x07e0          # PACKFILE * pack_fopen() hands back
S_TABLES  = S + 0x0800          # 15 x Thisc_table (36 bytes each)
S_LABELS  = S + 0x0c00          # 12 x 16-byte rank labels
S_PAGE    = S + 0x0d00          # 4 profile page buffers, 64 bytes apart
S_OLDREP  = S + 0x1000          # the Treplay load_replay() hands back
S_PROFILE = S + 0x2000          # Tprofile
S_SCROLL  = S + 0x2800          # Tscroller (2072 bytes)
S_TEXT    = S + 0x6000          # the scroller text buffer (8 KB, so the
                                # longest generated multi-line text fits)
S_REPLAY  = S + 0x3600          # Treplay
S_RECS    = S + 0x4000          # Trecord[]

REPLAY_BYTES = 0x8ac
PROFILE_BYTES = 0x550
SCROLLER_BYTES = 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 512 * 4

SYMS = {
    0: "NULL", S_SWAP: "swap", S_SCREEN: "screen", S_TMP: "tmp",
    S_SRC: "src", S_FONT: "font", S_FILE: "file", S_PACK: "packfile",
    S_OLDREP: "old", S_CTRL: "ctrl", S_REPLAY: "replay", S_RECS: "records",
    S_PAGE + 0x00: "page_general", S_PAGE + 0x40: "page_basic",
    S_PAGE + 0x80: "page_advanced", S_PAGE + 0xc0: "page_extra",
    G_OPTIONS: "options",
}

# synthetic vtable-slot VAs (never real code)
VT_RECTFILL   = 0x7b0000
VT_DRAW_SPR   = 0x7b0010
VT_DRAW_256   = 0x7b0020

# real callee VAs -> (name, argc)
CALLEES = {
    0x4bad08: ("free", 1),
    0x4bad60: ("sprintf", 2),
    0x44623c: ("delete_file", 1),
    0x40da58: ("log2file", 1),
    0x4039cc: ("get_configfile_path", 2),
    0x403a44: ("get_profile_dir_for_profile", 3),
    0x445afc: ("pack_fopen", 2),
    0x4449c8: ("pack_fwrite", 3),
    0x444d78: ("pack_fclose", 1),
    0x4183e4: ("save_options", 2),
    0x405630: ("save_hisc_table", 2),
    0x45a040: ("text_height", 1),
    0x459f50: ("text_length", 2),
    0x44f3f0: ("create_bitmap", 2),
    0x44f14c: ("destroy_bitmap", 1),
    0x456264: ("blit", 8),
    0x45c5d8: ("set_trans_blender", 4),
    0x44bf54: ("drawing_mode", 4),
    0x44c288: ("solid_mode", 0),
    0x450c98: ("makecol", 3),
    0x40b6bc: ("blit_to_screen", 1),
    0x45dea8: ("rest", 1),
    0x4bad78: ("time", 1),
    0x4badf8: ("localtime", 1),
    0x4bad48: ("strcpy", 2),           # emulated, NOT recorded
    0x41cde8: ("load_replay", 1),
    0x446150: ("file_exists", 3),
    0x4b2de0: ("mkdir", 1),
    0x418a14: ("generate_profile_checksum", 1),
    0x4bad28: ("fopen", 2),
    0x4bad00: ("fwrite", 4),
    0x4bad70: ("fprintf", 2),
    0x4badf0: ("fputs", 2),
    0x4badc0: ("fputc", 2),
    0x4bad30: ("fclose", 1),
    0x406978: ("get_controls", 0),
    0x40192c: ("save_control", 2),
    0x4196a8: ("profile_data_page_general", 2),
    0x4193d0: ("profile_data_page_basic", 1),
    0x419284: ("profile_data_page_advanced", 1),
    0x419650: ("profile_data_page_extra", 1),
    VT_RECTFILL: ("rectfill", 6),
    VT_DRAW_SPR: ("draw_sprite", 4),
    VT_DRAW_256: ("draw_256_sprite", 4),
}

PAGES = ["general-page\n", "basic-page\n", "advanced-page\n", "extra-page\n"]


def esc(s):
    """Escape a traced string the way batch14b_check.c's pstr() does.

    Trace records are compared LINE BY LINE, so a traced string that
    itself contains a newline (a page builder's output, a multi-line
    scroller text) would split one record into several and desynchronise
    the whole block."""
    out = []
    for ch in s:
        c = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif 0x20 <= c < 0x7f:
            out.append(ch)
        else:
            out.append("\\x%02x" % c)
    return "".join(out)


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def conversions(fmt):
    out, i = [], 0
    while i < len(fmt):
        if fmt[i] != "%":
            i += 1
            continue
        j = i + 1
        while j < len(fmt) and fmt[j] in "-+ #0123456789.":
            j += 1
        if j < len(fmt):
            if fmt[j] == "%":
                i = j + 1
                continue
            out.append(fmt[j:j + 1])
        i = j + 1
    return out


def c_format(fmt, args):
    return fmt % tuple(args) if args else fmt.replace("%%", "%")


# --------------------------------------------------------------------------
# ORIGINAL side
# --------------------------------------------------------------------------
_GUEST = {}


def guest_bytes():
    if "g" not in _GUEST:
        _GUEST["g"] = pf.build_guest(IMAGE)
    return _GUEST["g"]


class Original(object):
    def __init__(self):
        g = guest_bytes()
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(pf.GUEST_BASE, pf.GUEST_SIZE)
        mu.mem_write(pf.GUEST_BASE, g)
        mu.mem_map(pf.STACK_BASE, pf.STACK_SIZE)
        mu.mem_write(pf.RET_MAGIC, b"\xF4")
        self.stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + u32(pf.CW_SLOT)
        mu.mem_write(pf.CW_SLOT, struct.pack("<H", pf.CW_INIT))
        mu.mem_write(pf.CW_STUB, self.stub)
        self.trace = []
        self.script = {}
        for va, (name, argc) in CALLEES.items():
            mu.hook_add(UC_HOOK_CODE, self._mk(name, argc), begin=va, end=va)

    # ---- guest helpers ----
    def gstr(self, va, cap=16384):   # long enough for the biggest scroller text
        out = b""
        while len(out) < cap:
            c = bytes(self.mu.mem_read(va + len(out), 1))
            if c == b"\0":
                break
            out += c
        return out.decode("latin1")

    def sym(self, p):
        return SYMS.get(p & 0xFFFFFFFF, "?")

    def hexs(self, va, n):
        return "".join("%02x" % c for c in bytes(self.mu.mem_read(va, n)))

    def r32(self, va):
        return struct.unpack("<i", bytes(self.mu.mem_read(va, 4)))[0]

    # ---- the one hook factory ----
    def _mk(self, name, argc):
        def hook(uc, address, size, data):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]

            def a(i):
                return struct.unpack("<i", bytes(uc.mem_read(esp + 4 + 4 * i, 4)))[0]

            rv = self.dispatch(name, argc, a)
            uc.reg_write(UC_X86_REG_EAX, rv & 0xFFFFFFFF)
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EIP, ret)
            uc.emu_stop()
        return hook

    def varargs(self, fmt, a, first):
        args, off = [], first
        for c in conversions(fmt):
            v = a(off)
            off += 1
            args.append(self.gstr(v & 0xFFFFFFFF) if c == "s" else v)
        return args

    def dispatch(self, name, argc, a):
        t = self.trace.append
        sc = self.script
        mu = self.mu

        # --- CRT / not recorded ---
        if name == "strcpy":
            # emulated, deliberately silent (pf_harness_batch14.h note 2)
            s = self.gstr(a(1) & 0xFFFFFFFF)
            mu.mem_write(a(0) & 0xFFFFFFFF, s.encode("latin1") + b"\0")
            return a(0)

        if name == "free":
            t("free %s" % self.sym(a(0))); return 0
        if name == "sprintf":
            fmt = self.gstr(a(1) & 0xFFFFFFFF)
            text = c_format(fmt, self.varargs(fmt, a, 2))
            mu.mem_write(a(0) & 0xFFFFFFFF, text.encode("latin1") + b"\0")
            t('sprintf "%s" -> "%s"' % (esc(fmt), esc(text)))
            return len(text)
        if name == "delete_file":
            t('delete_file "%s"' % esc(self.gstr(a(0) & 0xFFFFFFFF))); return 0
        if name == "log2file":
            fmt = self.gstr(a(0) & 0xFFFFFFFF)
            text = c_format(fmt, self.varargs(fmt, a, 1))
            t('log2file "%s" -> "%s"' % (esc(fmt), esc(text)))
            return 0
        if name in ("get_configfile_path", "get_profile_dir_for_profile"):
            if name == "get_configfile_path":
                t("get_configfile_path %d" % a(1))
            else:
                t('get_profile_dir_for_profile %d "%s"'
                  % (a(1), esc(self.gstr(a(2) & 0xFFFFFFFF))))
            mu.mem_write(a(0) & 0xFFFFFFFF, sc["dir"].encode("latin1") + b"\0")
            return 1
        if name == "pack_fopen":
            ok = sc["packfopen_ok"]
            t('pack_fopen "%s" "%s" -> %s'
              % (esc(self.gstr(a(0) & 0xFFFFFFFF)),
                 esc(self.gstr(a(1) & 0xFFFFFFFF)),
                 "packfile" if ok else "NULL"))
            return S_PACK if ok else 0
        if name == "pack_fwrite":
            t("pack_fwrite %s %d [%s]"
              % (self.sym(a(2)), a(1), self.hexs(a(0) & 0xFFFFFFFF, a(1))))
            return a(1)
        if name == "pack_fclose":
            t("pack_fclose %s" % self.sym(a(0))); return 0
        if name == "save_options":
            t("save_options %s %s" % (self.sym(a(0)), self.sym(a(1)))); return 0
        if name == "save_hisc_table":
            p = a(0) & 0xFFFFFFFF
            k = (p - S_TABLES) // 36 if S_TABLES <= p < S_TABLES + 15 * 36 else -1
            t("save_hisc_table table%d %s" % (k, self.sym(a(1)))); return 0
        if name == "file_exists":
            t('file_exists "%s" %d -> %d'
              % (esc(self.gstr(a(0) & 0xFFFFFFFF)), a(1), sc["exists"]))
            return sc["exists"]
        if name == "mkdir":
            t('mkdir "%s"' % esc(self.gstr(a(0) & 0xFFFFFFFF))); return 0
        if name == "load_replay":
            ok = sc["load_replay_ok"]
            t('load_replay "%s" -> %s'
              % (esc(self.gstr(a(0) & 0xFFFFFFFF)), "old" if ok else "NULL"))
            return S_OLDREP if ok else 0
        if name == "generate_profile_checksum":
            t("generate_profile_checksum -> %d" % sc["checksum"])
            return sc["checksum"]
        if name == "get_controls":
            t("get_controls"); return S_CTRL
        if name == "save_control":
            t("save_control %s %s" % (self.sym(a(0)), self.sym(a(1)))); return 0
        if name == "profile_data_page_general":
            t('profile_data_page_general "%s"'
              % esc(self.gstr(a(1) & 0xFFFFFFFF)))
            return S_PAGE + 0x00
        if name == "profile_data_page_basic":
            t("profile_data_page_basic"); return S_PAGE + 0x40
        if name == "profile_data_page_advanced":
            t("profile_data_page_advanced"); return S_PAGE + 0x80
        if name == "profile_data_page_extra":
            t("profile_data_page_extra"); return S_PAGE + 0xc0
        if name == "fopen":
            ok = sc["fopen_ok"]
            t('fopen "%s" "%s" -> %s'
              % (esc(self.gstr(a(0) & 0xFFFFFFFF)),
                 esc(self.gstr(a(1) & 0xFFFFFFFF)),
                 "file" if ok else "NULL"))
            return S_FILE if ok else 0
        if name == "fwrite":
            n = a(1) * a(2)
            t("fwrite %s %u %u [%s]"
              % (self.sym(a(3)), a(1) & 0xFFFFFFFF, a(2) & 0xFFFFFFFF,
                 self.hexs(a(0) & 0xFFFFFFFF, n)))
            return a(2)
        if name == "fprintf":
            fmt = self.gstr(a(1) & 0xFFFFFFFF)
            text = c_format(fmt, self.varargs(fmt, a, 2))
            t('fprintf %s "%s" -> "%s"'
              % (self.sym(a(0)), esc(fmt), esc(text)))
            return len(text)
        if name == "fputs":
            t('fputs %s "%s"'
              % (self.sym(a(1)), esc(self.gstr(a(0) & 0xFFFFFFFF))))
            return 0
        if name == "fputc":
            t("fputc %d %s" % (a(0), self.sym(a(1)))); return a(0)
        if name == "fclose":
            t("fclose %s" % self.sym(a(0))); return 0
        if name == "time":
            t("time %s" % ("&t" if a(0) else "NULL"))
            if a(0):
                mu.mem_write(a(0) & 0xFFFFFFFF, struct.pack("<i", sc["time"]))
            return sc["time"]
        if name == "localtime":
            v = self.r32(a(0) & 0xFFFFFFFF) if a(0) else 0
            t("localtime %d" % v)
            mu.mem_write(S_TM, struct.pack("<9i", *sc["tm"]))
            return S_TM

        # --- Allegro drawing ---
        if name == "text_height":
            t("text_height %s" % self.sym(a(0))); return 17
        if name == "text_length":
            s = self.gstr(a(1) & 0xFFFFFFFF)
            t('text_length %s "%s"' % (self.sym(a(0)), esc(s)))
            return len(s) * 9
        if name == "create_bitmap":
            t("create_bitmap %d %d" % (a(0), a(1))); return S_TMP
        if name == "destroy_bitmap":
            t("destroy_bitmap %s" % self.sym(a(0))); return 0
        if name == "blit":
            t("blit %s %s %d %d %d %d %d %d"
              % (self.sym(a(0)), self.sym(a(1)), a(2), a(3), a(4), a(5),
                 a(6), a(7)))
            return 0
        if name == "set_trans_blender":
            t("set_trans_blender %d %d %d %d" % (a(0), a(1), a(2), a(3))); return 0
        if name == "drawing_mode":
            t("drawing_mode %d %s %d %d" % (a(0), self.sym(a(1)), a(2), a(3)))
            return 0
        if name == "solid_mode":
            t("solid_mode"); return 0
        if name == "makecol":
            t("makecol %d %d %d" % (a(0), a(1), a(2))); return 0x123456
        if name == "blit_to_screen":
            t("blit_to_screen %s" % self.sym(a(0))); return 0
        if name == "rest":
            t("rest %u" % (a(0) & 0xFFFFFFFF))
            sc["rest_i"] += 1
            if sc["rest_i"] >= sc["rest_n"]:
                sc["rest_i"] = 0
                mu.mem_write(G_CYCLE_COUNT, u32(1))
            return 0
        if name == "rectfill":
            t("rectfill %s %d %d %d %d %d"
              % (self.sym(a(0)), a(1), a(2), a(3), a(4), a(5)))
            return 0
        if name in ("draw_sprite", "draw_256_sprite"):
            t("%s %s %s %d %d" % (name, self.sym(a(0)), self.sym(a(1)),
                                  a(2), a(3)))
            return 0
        raise RuntimeError("unhandled callee " + name)

    # ---- run one vector ----
    def run(self, va, args, script):
        self.trace = []
        self.script = dict(script)
        self.script.setdefault("rest_i", 0)
        self.script.setdefault("rest_n", 1)
        self.script.setdefault("dir", "cfg/")
        self.script.setdefault("exists", 0)
        self.script.setdefault("fopen_ok", 0)
        self.script.setdefault("packfopen_ok", 0)
        self.script.setdefault("load_replay_ok", 0)
        self.script.setdefault("checksum", 0)
        self.script.setdefault("time", 0)
        self.script.setdefault("tm", [0] * 9)
        mu = self.mu
        mu.emu_start(pf.CW_STUB, pf.CW_STUB + len(self.stub))
        esp = pf.STACK_BASE + pf.STACK_SIZE - 0x4000
        for v in reversed(args):
            esp -= 4
            mu.mem_write(esp, u32(v))
        esp -= 4
        mu.mem_write(esp, u32(pf.RET_MAGIC))
        mu.reg_write(UC_X86_REG_ESP, esp)
        pc, guard = va, 0
        while True:
            mu.emu_start(pc, pf.RET_MAGIC, count=20000000)
            eip = mu.reg_read(UC_X86_REG_EIP)
            if eip == pf.RET_MAGIC:
                break
            pc = eip
            guard += 1
            if guard > 400000:
                raise RuntimeError("call-hook resume runaway")
        return mu.reg_read(UC_X86_REG_EAX)

    # ---- fixed world, rewritten before every vector ----
    def world(self):
        mu = self.mu
        mu.mem_write(S_VT8, b"\0" * 0x200)
        mu.mem_write(S_VT16, b"\0" * 0x200)
        mu.mem_write(S_VT8 + 0x00, u32(8))
        mu.mem_write(S_VT16 + 0x00, u32(16))
        for vt in (S_VT8, S_VT16):
            mu.mem_write(vt + 0x3c, u32(VT_RECTFILL))
            mu.mem_write(vt + 0x44, u32(VT_DRAW_SPR))
            mu.mem_write(vt + 0x48, u32(VT_DRAW_256))
        mu.mem_write(G_SWAP_SCREEN, u32(S_SWAP))
        mu.mem_write(G_SCREEN, u32(S_SCREEN))
        mu.mem_write(S_SWAP + 0x1c, u32(S_VT16))
        mu.mem_write(S_SCREEN + 0x1c, u32(S_VT16))
        for i in range(15):
            mu.mem_write(G_HISC_TABLES + 4 * i, u32(S_TABLES + 36 * i))
        for i in range(12):
            mu.mem_write(S_LABELS + 16 * i, ("rank%d" % i).encode() + b"\0")
            mu.mem_write(G_RANK_LABELS + 4 * i, u32(S_LABELS + 16 * i))
            for tab in (G_RANK_FLOORS, G_RANK_COMBOS, G_RANK_NMLS, G_RANK_CCCS):
                mu.mem_write(tab + 4 * i, u32(0))
        for i, p in enumerate(PAGES):
            mu.mem_write(S_PAGE + 0x40 * i, p.encode("latin1") + b"\0")
        mu.mem_write(S_OLDREP, b"\0" * REPLAY_BYTES)
        mu.mem_write(S_OLDREP + 0x2c, b"OLD DATE 1999\0")


# --------------------------------------------------------------------------
# the shared per-vector Treplay generator (mirrored in batch14b_check.c)
# --------------------------------------------------------------------------
def fill_replay(seed, size):
    st = seed & 0xFFFFFFFF
    b = bytearray()
    for _ in range(REPLAY_BYTES):
        st = (st * 1103515245 + 12345) & 0xFFFFFFFF
        b.append((st >> 16) & 0xFF)
    b[0:6] = b"ITR15\0"
    struct.pack_into("<i", b, 0x08, size)
    recs = bytearray()
    for _ in range(size):
        st = (st * 1103515245 + 12345) & 0xFFFFFFFF
        kf = (st >> 16) & 0xFF
        st = (st * 1103515245 + 12345) & 0xFFFFFFFF
        recs += bytes([kf, 0, 0, 0]) + struct.pack("<I", st)
    return bytes(b), bytes(recs)


# --------------------------------------------------------------------------
# vectors
# --------------------------------------------------------------------------
PATHS = ["", "replays/", "C:/Icy Tower/replays/", "a/"]
FILES = ["run.itr", "", "x", "a very long replay file name.itr"]
HANDLES = ["", "bob", "Player One", "abcdefghijklmnopqrstuvwxyz012"]
TEXTS = [
    "",
    "hello",
    "one\\ntwo\\nthree",
    "\\n",
    "\\n\\n\\n",
    "line\\n",
    "\\nleading",
    "a\\nb\\nc\\nd\\ne\\nf\\ng\\nh",
    "no newline at all, just a long horizontal scroller message",
]


def gen_tm(rnd):
    return [rnd.randrange(0, 60), rnd.randrange(0, 60), rnd.randrange(0, 24),
            rnd.choice([1, 9, 10, 28, 31, rnd.randrange(1, 32)]),
            rnd.choice([0, 8, 9, 11, rnd.randrange(0, 12)]),
            rnd.choice([70, 100, 126, rnd.randrange(0, 200)]),
            rnd.randrange(0, 7), rnd.randrange(0, 366), rnd.randrange(-1, 2)]


def gen(rnd, kind):
    """One vector: (exe-line, guest-writes, args, script, domain)."""
    if kind == "destroy_replay":
        rnull = 1 if rnd.random() < 0.2 else 0
        dnull = 1 if rnd.random() < 0.4 else 0
        line = "destroy_replay|%d|%d" % (rnull, dnull)
        writes = [(S_REPLAY, b"\0" * REPLAY_BYTES),
                  (S_REPLAY + 0x8a8, u32(0 if dnull else S_RECS))]
        return line, writes, [0 if rnull else S_REPLAY], {}, []

    if kind == "myDeleteFile":
        p = rnd.choice(PATHS)
        f = rnd.choice(FILES)
        line = "myDeleteFile|%s|%s" % (p, f)
        writes = [(S_TEXT, p.encode("latin1") + b"\0"),
                  (S_TEXT + 0x400, f.encode("latin1") + b"\0")]
        return line, writes, [S_TEXT, S_TEXT + 0x400], {}, []

    if kind == "save_config":
        d = rnd.choice(["cfg/tower.cfg", "", "C:/Icy Tower/tower.cfg"])
        ok = 0 if rnd.random() < 0.25 else 1
        line = "save_config|%s|%d" % (d, ok)
        return line, [], [], {"dir": d, "packfopen_ok": ok}, []

    if kind == "init_scroller":
        txt = rnd.choice(TEXTS)
        if rnd.random() < 0.15:
            txt = "\\n".join("l%d" % i for i in range(rnd.randrange(0, 600)))
        w = rnd.choice([0, 1, 640, rnd.randrange(0, 2000)])
        h = rnd.choice([0, 1, 480, rnd.randrange(0, 2000)])
        horiz = 1 if rnd.random() < 0.4 else 0
        line = "init_scroller|%s|%d|%d|%d" % (txt, w, h, horiz)
        real = txt.replace("\\n", "\n")
        writes = [(S_SCROLL, b"\0" * SCROLLER_BYTES),
                  (S_TEXT, real.encode("latin1") + b"\0")]
        return (line, writes, [S_SCROLL, S_FONT, S_TEXT, w, h, horiz],
                {}, [("scroller", len(real))])

    if kind in ("fadeOut", "fadeIn"):
        speed = rnd.choice([8, 16, 32, 51, 64, 85, 128, 255, 256, 300,
                            rnd.randrange(6, 256)])
        gnull = 1 if rnd.random() < 0.1 else 0
        w = rnd.choice([640, 800, 0, rnd.randrange(0, 2000)])
        h = rnd.choice([480, 600, 0, rnd.randrange(0, 2000)])
        d8 = 1 if rnd.random() < 0.5 else 0
        rest_n = rnd.choice([1, 1, 2, 3])
        line = "%s|%d|%d|%d|%d|%d|%d" % (kind, speed, gnull, w, h, d8, rest_n)
        writes = [(G_GFX_DRIVER, u32(0 if gnull else S_GFX)),
                  (S_GFX + 0x6c, u32(w)), (S_GFX + 0x70, u32(h)),
                  (G_CYCLE_COUNT, u32(0))]
        if kind == "fadeOut":
            writes.append((S_TMP + 0x1c, u32(S_VT8 if d8 else S_VT16)))
            args = [speed]
        else:
            writes.append((S_SRC + 0x1c, u32(S_VT8 if d8 else S_VT16)))
            writes.append((S_TMP + 0x1c, u32(S_VT16)))
            args = [S_SRC, speed]
        return line, writes, args, {"rest_n": rest_n}, []

    if kind == "save_replay":
        p = rnd.choice(PATHS)
        f = rnd.choice(FILES)
        seed = rnd.randrange(1, 1 << 32)
        size = rnd.choice([0, 1, 2, 7, rnd.randrange(0, 60)])
        newdate = 1 if rnd.random() < 0.6 else 0
        packok = 0 if rnd.random() < 0.2 else 1
        loadok = 1 if rnd.random() < 0.5 else 0
        tv = rnd.randrange(0, 1 << 30)
        tm = gen_tm(rnd)
        line = ("save_replay|%s|%s|%u|%d|%d|%d|%d|%d|" % (
            p, f, seed, size, newdate, packok, loadok, tv)
            + "|".join(str(x) for x in tm))
        rb, recs = fill_replay(seed, size)
        writes = [(S_REPLAY, rb), (S_REPLAY + 0x8a8, u32(S_RECS)),
                  (S_TEXT, p.encode("latin1") + b"\0"),
                  (S_TEXT + 0x400, f.encode("latin1") + b"\0")]
        if recs:
            writes.append((S_RECS, recs))
        return (line, writes, [S_TEXT, S_TEXT + 0x400, S_REPLAY, size, newdate],
                {"packfopen_ok": packok, "load_replay_ok": loadok,
                 "time": tv, "tm": tm},
                [("save_replay", 0)])

    if kind == "save_profile":
        d = rnd.choice(["profiles/bob/", "", "C:/IT/p/"])
        handle = rnd.choice(HANDLES)
        exists = 1 if rnd.random() < 0.5 else 0
        fopen_ok = 0 if rnd.random() < 0.3 else 1
        cs = rnd.randrange(-(1 << 31), 1 << 31)
        tv = rnd.randrange(0, 1 << 30)
        tm = gen_tm(rnd)
        line = ("save_profile|%s|%s|%d|%d|%d|%d|" % (
            d, handle, exists, fopen_ok, cs, tv)
            + "|".join(str(x) for x in tm))
        prof = bytearray(PROFILE_BYTES)
        hb = handle.encode("latin1")[:31]
        prof[6:6 + len(hb)] = hb
        writes = [(S_PROFILE, bytes(prof))]
        return (line, writes, [S_PROFILE],
                {"dir": d, "exists": exists, "fopen_ok": fopen_ok,
                 "checksum": cs, "time": tv, "tm": tm},
                [("save_profile", 0)])
    raise RuntimeError(kind)


KINDS = ["destroy_replay", "myDeleteFile", "save_config", "init_scroller",
         "fadeOut", "fadeIn", "save_replay", "save_profile"]

# emulation cost per vector differs by ~three orders of magnitude; see the
# module docstring's "Vector budget".
WEIGHT = {"destroy_replay": 1.0, "myDeleteFile": 1.0, "save_config": 1.0,
          "init_scroller": 1.0, "fadeOut": 0.06, "fadeIn": 0.06,
          "save_replay": 0.02, "save_profile": 0.25}


def original_domain(orig, dom, eax):
    mu = orig.mu
    out = []
    for what, extra in dom:
        if what == "scroller":
            out.append("sc %d %d %d %d %d %d %d" % (
                orig.r32(S_SCROLL + 0x00), orig.r32(S_SCROLL + 0x0c),
                orig.r32(S_SCROLL + 0x10), orig.r32(S_SCROLL + 0x14),
                orig.r32(S_SCROLL + 0x18), orig.r32(S_SCROLL + 0x1c),
                orig.r32(S_SCROLL + 0x20)))
            rows = max(0, min(512, orig.r32(S_SCROLL + 0x1c)))
            for i in range(rows):
                p = orig.r32(S_SCROLL + 0x24 + 4 * i) & 0xFFFFFFFF
                out.append('line %d %d "%s"'
                           % (i, p - S_TEXT, esc(orig.gstr(p))))
            out.append("text [%s]" % orig.hexs(S_TEXT, extra + 1))
        elif what == "save_replay":
            out.append("ret %d" % (((eax + 0x80000000) & 0xFFFFFFFF) - 0x80000000))
            out.append("date [%s]" % orig.hexs(S_REPLAY + 0x2c, 32))
            out.append("size %d checksum %d"
                       % (orig.r32(S_REPLAY + 0x08), orig.r32(S_REPLAY + 0x4c)))
        elif what == "save_profile":
            out.append("ret %d" % (((eax + 0x80000000) & 0xFFFFFFFF) - 0x80000000))
            out.append("checksum %d" % orig.r32(S_PROFILE + 0x28))
            out.append("saveDate [%s]" % orig.hexs(S_PROFILE + 0x540, 16))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vectors", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--only", default=None)
    ap.add_argument("--fault", action="store_true",
                    help="negative control: corrupt one ORIGINAL-side record "
                         "at vector 5 of each kind and require a DIFFER")
    args = ap.parse_args()

    kinds = [args.only] if args.only else KINDS
    base = max(1, args.vectors // len(kinds))
    counts = {k: (20 if args.fault else max(1, int(base * WEIGHT[k])))
              for k in kinds}

    rnd = random.Random(args.seed)
    orig = Original()

    lines, expected = [], []
    for kind in kinds:
        for k in range(counts[kind]):
            line, writes, callargs, script, dom = gen(rnd, kind)
            lines.append(line)
            expected.append((kind, k, writes, callargs, script, dom))

    with open(VECFILE, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    out = subprocess.check_output([CHECK_EXE, VECFILE]).decode("latin1")
    blocks, cur = [], None
    for ln in out.replace("\r\n", "\n").split("\n"):
        if ln.startswith("V "):
            cur = []
        elif ln == "E":
            blocks.append(cur)
            cur = None
        elif cur is not None and ln:
            cur.append(ln)
    if len(blocks) != len(expected):
        print("PROTOCOL: %d candidate blocks for %d vectors"
              % (len(blocks), len(expected)))
        sys.exit(1)

    fails = {k: 0 for k in kinds}
    shown = 0
    for i, (kind, k, writes, callargs, script, dom) in enumerate(expected):
        orig.world()
        for va, b in writes:
            orig.mu.mem_write(va, b)
        eax = orig.run(FN[kind], callargs, script)
        got = orig.trace + original_domain(orig, dom, eax)
        cand = list(blocks[i])
        if args.fault and k == 5:
            got = list(got)
            if got:
                got[0] = got[0] + " FAULT"
            else:
                got = ["FAULT (spurious record injected into an empty trace)"]
        if got != cand:
            fails[kind] += 1
            if shown < 8:
                shown += 1
                print("  DIFFER %s vector %d" % (kind, k))
                print("    vector: %s" % lines[i])
                for a, b in zip(got + [""] * 4000, cand + [""] * 4000):
                    if a != b:
                        print("    original:  %s" % a[:300])
                        print("    candidate: %s" % b[:300])
                        break
    bad = 0
    for kind in kinds:
        print("%-16s %d of %d vectors differ" % (kind, fails[kind], counts[kind]))
        if args.fault:
            if fails[kind] == 0:
                print("NEGATIVE CONTROL FAILED: %s fault not detected" % kind)
                bad = 1
        else:
            bad |= (1 if fails[kind] else 0)
    sys.exit(bad)


if __name__ == "__main__":
    main()
