#!/usr/bin/env python3
"""batch15b_check.py -- ordered-call-trace oracle for PROMOTIONS.md
batch 15's five functions whose effect is a sequence of library calls
plus the memory they fill:

    create_replay   0x41cce8    254 B   src/icytower/replay.c
    load_replay     0x41cde8   1136 B   src/icytower/replay.c
    getGameDataXML  0x404254   1855 B   src/icytower/game_data.c
    draw_results    0x4076c0    839 B   src/icytower/results.c
    my_alert        0x40cd68   1770 B   src/icytower/alert.c

Why this is not a lift_check.py SPECS entry
-------------------------------------------
The same reasons batches 10/12/13/14 gave, and two that are specific
here: load_replay() makes over five hundred pack_fread() calls whose
BYTES are the .itr file, which mechanism B's "count plus the arguments
of the FIRST call" would summarise into nothing; and my_alert() is an
INPUT-DRIVEN LOOP whose iteration count is decided by the control layer,
which no fixed-shape vector row can express.  icytower_specs.py is
untouched again -- the sixth batch running that way.

How it works
------------
1. The ORIGINAL bytes run under unicorn on the shared
   port_forge/tools/pf_win32_offline_oracle engine.  Every library, CRT
   and control-layer callee is hooked at its own VA and stubbed,
   appending one record per call, IN ORDER, with its arguments.
2. Callees NOT reached by a direct `call <va>`:
     - rectfill(), draw_sprite(), draw_256_sprite(), acquire_bitmap()
       and release_bitmap(), which are Allegro AL_INLINEs dispatching
       through GFX_VTABLE's +0x3c / +0x44 / +0x48 / +0x10 / +0x14 slots.
       Batch 9's synthetic-vtable-VA trick, the same one
       draw_frame_xcheck.py uses.
3. Callees deliberately NOT stubbed and run FOR REAL on both sides:
     - ___chkstk (0x4b2a3c), getGameDataXML's 18524-byte frame probe --
       it is stack manipulation, not an effect, and it runs correctly
       under unicorn.
     - calc_replay_checksum() and destroy_replay(), which load_replay()
       calls; batch 14 promoted both and batch14_check.py / batch14b
       cover them independently, so letting them run links the two
       batches instead of hiding the link behind a stub.
     - create_replay(), which load_replay() calls; this batch promotes
       it and the vectors above test it directly.
     - strcat (0x4bad58) is emulated but records NOTHING -- see
       pf_harness_batch15.h note 1 for why tracing it would be
       asymmetric.
4. The compiled candidate is batch15b_check.exe (build_batch15.sh),
   driven by a BINARY vector file -- one process for the whole run.
5. Pointers are rendered as stable SYMBOLS and strings as their CONTENT
   AT CALL TIME.  pack_fread buffers are rendered as the HEX OF THE
   BYTES they delivered, because for load_replay the sequence of those
   buffers IS the file.

Vector budget
-------------
The kinds cost wildly different amounts of emulation: one load_replay
vector is ~540 traced calls against 3-5 for create_replay.  `--vectors N`
is split by the WEIGHT table below rather than evenly, and the per-kind
count actually run is printed with each result line.

Usage
-----
  python batch15b_check.py --vectors 20000 --seed 20260908
  python batch15b_check.py --fault
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
CHECK_EXE = os.path.join(HERE, "batch15b_check.exe")
VECFILE = os.path.join(HERE, "batch15b_vectors.bin")

FN = {
    "create_replay":  0x41cce8,
    "load_replay":    0x41cde8,
    "getGameDataXML": 0x404254,
    "draw_results":   0x4076c0,
    "my_alert":       0x40cd68,
}

# ------------------------------------------------------------- globals
G_CMDLINE     = 0x4dd14c        # Tcommandline: jumps/combos/sd/keys/tiny
G_SWAP_SCREEN = 0x4dd194
G_NEW_PB      = 0x4dd200        # int[15]
G_DATA        = 0x4dd23c        # DATAFILE *
G_CLOSEBTN    = 0x4dd264
G_PROFILE     = 0x4dd27c        # Tprofile *
G_GFX_DRIVER  = 0x4dda84
G_SCREEN      = 0x4dda8c
G_GUI_FG      = 0x4cc3a0
G_GUI_BG      = 0x4ddaa8
G_CATEGORIES  = 0x4bc080        # char *[15]
G_CTRL        = 0x5000c8
G_MENU_CTRL   = 0x4f8e40        # menu_params.ctrl (menu_params + 8)
G_CYCLE_COUNT = 0x506938
G_KEY         = 0x506988        # volatile char[127]
KEY_ESC       = 0x3b
KEY_ENTER     = 0x43

# --------------------------------------------------------- scratch VAs
S_ARENA   = 0x600000            # malloc arena, 0xA5-filled
ARENA_END = 0x660000
S_GD      = 0x680000            # Tgame_data (0x1d514 bytes)
S_XREPLAY = 0x6a0000            # the Treplay getGameDataXML reads
S_FILE    = 0x6b0000            # the .itr byte image pack_fread serves
S_NAME    = 0x6c0000
S_PROFILE = 0x6c1000
S_STRF    = 0x6c2000
S_STRT    = 0x6c2400
S_DF      = 0x6c4000            # fake DATAFILE[100], 16 bytes each
S_OBJ     = 0x6c6000            # 100 objects, 0x40 stride
S_VT8     = 0x6d0000
S_VT16    = 0x6d0400
S_SWAP    = 0x6d1000
S_SCREEN  = 0x6d1100
S_LOGO    = 0x6d1200
S_TARGET  = 0x6d1300
S_GFX     = 0x6d1400            # GFX_DRIVER (w +0x6c, h +0x70)
S_QUAL    = 0x6d2000            # int[5]
S_QVAL    = 0x6d2100            # int[5]
S_CATS    = 0x6d3000            # 15 x 16-byte strings
S_PACK    = 0x6d4000            # the PACKFILE * pack_fopen hands back

NOBJ = 100
OBJ_STRIDE = 0x40
REPLAY_BYTES = 0x8ac
REPLAY_HEAD = 0x8a8
GD_BYTES = 0x1d514
SPACE = " "

# synthetic vtable-slot VAs (never real code)
VT_RECTFILL = 0x7b0000
VT_DRAW_SPR = 0x7b0010
VT_DRAW_256 = 0x7b0020
VT_ACQUIRE  = 0x7b0030
VT_RELEASE  = 0x7b0040

CALLEES = {
    0x4bad10: ("malloc", 1),
    0x4bad08: ("free", 1),
    0x4bad60: ("sprintf", 2),
    0x4bad58: ("strcat", 2),          # emulated, NOT recorded
    0x4b2dd8: ("stricmp", 2),
    0x445afc: ("pack_fopen", 2),
    0x4449b0: ("pack_fread", 3),
    0x444d78: ("pack_fclose", 1),
    0x40da58: ("log2file", 1),
    0x459f50: ("text_length", 2),
    0x450c98: ("makecol", 3),
    0x45c5d8: ("set_trans_blender", 4),
    0x44bf54: ("drawing_mode", 4),
    0x44c288: ("solid_mode", 0),
    0x456264: ("blit", 8),
    0x44c340: ("vsync", 0),
    0x43dc5c: ("clear_keybuf", 0),
    0x45dea8: ("rest", 1),
    0x45a2a8: ("textprintf_ex", 6),
    0x45a1d0: ("textprintf_right_ex", 6),
    0x45a23c: ("textprintf_centre_ex", 6),
    0x459fcc: ("textout_centre_ex", 7),
    0x459f64: ("textout_right_ex", 7),
    0x401958: ("poll_control", 2),
    0x4018e8: ("is_any", 1),
    0x401874: ("is_left", 1),
    0x401888: ("is_right", 1),
    0x4018a0: ("is_fire", 1),
    0x4018d0: ("is_enter", 1),
    VT_RECTFILL: ("rectfill", 6),
    VT_DRAW_SPR: ("draw_sprite", 4),
    VT_DRAW_256: ("draw_256_sprite", 4),
    VT_ACQUIRE:  ("acquire", 1),
    VT_RELEASE:  ("release", 1),
}

K_CREATE, K_LOAD, K_XML, K_RESULTS, K_ALERT = range(5)


def esc(s):
    """Escape a traced string the way batch15b_check.c's pstr() does."""
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


def tr(text):
    """Escape a FORMATTED result the way the candidate's pstr() sees it.

    A `%c` conversion can emit a NUL -- getGameDataXML prints the six
    header bytes of a possibly-corrupt .itr that way -- and sprintf then
    keeps writing past it.  The candidate traces with pstr(buf), which
    stops at the first NUL; so does everything downstream that treats the
    buffer as a string.  The trace has to stop there too."""
    return esc(text.split(chr(0), 1)[0])


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def i32(v):
    return struct.pack("<i", v)


def si32(v):
    return ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000


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
        self.syms = {}
        self.cover = None
        for va, (name, argc) in CALLEES.items():
            mu.hook_add(UC_HOOK_CODE, self._mk(name, argc), begin=va, end=va)

    def enable_coverage(self, ranges):
        """Record every ORIGINAL instruction address actually executed.

        "EQUAL over N vectors" is only worth something if the vectors reach
        the branches, and the honest way to say so is to measure it: one
        UC_HOOK_CODE per function range, a set of addresses, and afterwards
        the fraction of the addresses artifacts/disasm.txt lists for that
        range that the campaign entered at least once.  Slow, so it is a
        separate --coverage run and not part of the equivalence check."""
        self.cover = set()

        def hook(uc, address, size, data):
            self.cover.add(address)
        for lo, hi in ranges:
            self.mu.hook_add(UC_HOOK_CODE, hook, begin=lo, end=hi)

    # ---- guest helpers ----
    def gstr(self, va, cap=200000):
        va &= 0xFFFFFFFF
        out = bytearray()
        while len(out) < cap:
            chunk = bytes(self.mu.mem_read(va + len(out), 256))
            k = chunk.find(b"\0")
            if k >= 0:
                out += chunk[:k]
                break
            out += chunk
        return out.decode("latin1")

    def sym(self, p):
        return self.syms.get(p & 0xFFFFFFFF, "NULL" if not p else "?")

    def hexs(self, va, n):
        if n <= 0:
            return ""
        return "".join("%02x" % c for c in bytes(self.mu.mem_read(va, n)))

    def r32(self, va):
        return struct.unpack("<i", bytes(self.mu.mem_read(va, 4)))[0]

    # ---- the one hook factory ----
    def _mk(self, name, argc):
        def hook(uc, address, size, data):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]

            def a(i):
                return struct.unpack("<i",
                                     bytes(uc.mem_read(esp + 4 + 4 * i, 4)))[0]

            rv = self.dispatch(name, a, esp)
            uc.reg_write(UC_X86_REG_EAX, rv & 0xFFFFFFFF)
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EIP, ret)
            uc.emu_stop()
        return hook

    # ---- C format emulation ----
    def cfmt(self, fmt, esp, first):
        """Format `fmt` from the varargs starting at stack slot `first`."""
        mu = self.mu
        off = [first]

        def pop4(signed=True):
            v = struct.unpack("<i" if signed else "<I",
                              bytes(mu.mem_read(esp + 4 + 4 * off[0], 4)))[0]
            off[0] += 1
            return v

        def pop8():
            v = struct.unpack("<d",
                              bytes(mu.mem_read(esp + 4 + 4 * off[0], 8)))[0]
            off[0] += 2
            return v

        out = []
        i = 0
        while i < len(fmt):
            if fmt[i] != "%":
                out.append(fmt[i])
                i += 1
                continue
            j = i + 1
            spec = "%"
            while j < len(fmt) and fmt[j] in "-+ #0123456789.":
                spec += fmt[j]
                j += 1
            if j >= len(fmt):
                out.append(spec)
                break
            conv = fmt[j]
            j += 1
            if conv == "%":
                out.append("%")
            elif conv in "di":
                out.append((spec + "d") % pop4(True))
            elif conv == "u":
                out.append((spec + "d") % (pop4(False) & 0xFFFFFFFF))
            elif conv in "xX":
                out.append((spec + conv) % (pop4(False) & 0xFFFFFFFF))
            elif conv == "c":
                out.append((spec + "c") % (pop4(True) & 0xFF))
            elif conv == "s":
                # A NULL %s argument is not hypothetical here: my_alert()
                # passes its `func` parameter STRAIGHT to
                # textprintf_centre_ex (finding 2 in src/icytower/alert.c --
                # the " " substitution reaches text_length only).  Both
                # msvcrt and MinGW's own printf render it "(null)".
                sp = pop4(False) & 0xFFFFFFFF
                out.append((spec + "s") % ("(null)" if sp == 0
                                           else self.gstr(sp)))
            elif conv in "feEgG":
                out.append((spec + conv) % pop8())
            else:
                raise RuntimeError("unhandled conversion %%%s" % conv)
            i = j
        return "".join(out)

    # ---- the heap ----
    def malloc(self, n):
        sc = self.script
        seq = sc["malloc_seq"]
        sc["malloc_seq"] = seq + 1
        # unsigned, and against the SAME arena size batch15b_check.c uses:
        # create_replay(negative) asks for ~4 GB and whether that is refused
        # is part of the compared trace.
        if seq == sc["malloc_fail"] or n > (ARENA_END - S_ARENA) - sc["arena"]:
            self.trace.append("malloc %u -> NULL" % (n & 0xFFFFFFFF))
            return 0
        p = S_ARENA + sc["arena"]
        sc["arena"] += (n + 15) & ~15
        nm = "heap%d" % sc["n_heap"]
        sc["n_heap"] += 1
        self.syms[p] = nm
        self.trace.append("malloc %u -> %s" % (n & 0xFFFFFFFF, nm))
        return p

    def dispatch(self, name, a, esp):
        t = self.trace.append
        sc = self.script
        mu = self.mu

        if name == "strcat":            # emulated, deliberately silent
            d = a(0) & 0xFFFFFFFF
            s = self.gstr(a(1) & 0xFFFFFFFF)
            base = d + len(self.gstr(d))
            mu.mem_write(base, s.encode("latin1") + b"\0")
            return d
        if name == "malloc":
            return self.malloc(a(0) & 0xFFFFFFFF)
        if name == "free":
            t("free %s" % self.sym(a(0)))
            return 0
        if name == "sprintf":
            fmt = self.gstr(a(1) & 0xFFFFFFFF)
            text = self.cfmt(fmt, esp, 2)
            mu.mem_write(a(0) & 0xFFFFFFFF, text.encode("latin1") + b"\0")
            t('sprintf "%s" -> "%s"' % (esc(fmt), tr(text)))
            return len(text)
        if name == "stricmp":
            x = self.gstr(a(0) & 0xFFFFFFFF).lower()
            y = self.gstr(a(1) & 0xFFFFFFFF).lower()
            r = 0 if x == y else (1 if x > y else -1)
            t('stricmp "%s" "%s" -> %d'
              % (esc(self.gstr(a(0) & 0xFFFFFFFF)),
                 esc(self.gstr(a(1) & 0xFFFFFFFF)), r))
            return r
        if name == "log2file":
            fmt = self.gstr(a(0) & 0xFFFFFFFF)
            text = self.cfmt(fmt, esp, 1)
            t('log2file "%s" -> "%s"' % (esc(fmt), tr(text)))
            return 0

        # ---- Allegro packfile ----
        if name == "pack_fopen":
            seq = sc["packopen_seq"]
            sc["packopen_seq"] = seq + 1
            ok = sc["packopen1"] if seq == 0 else sc["packopen2"]
            t('pack_fopen "%s" "%s" -> %s'
              % (esc(self.gstr(a(0) & 0xFFFFFFFF)),
                 esc(self.gstr(a(1) & 0xFFFFFFFF)),
                 "packfile" if ok else "NULL"))
            if ok:
                sc["filepos"] = 0
            return S_PACK if ok else 0
        if name == "pack_fread":
            size = a(1)
            n = sc["filelen"] - sc["filepos"]
            if n > size:
                n = size
            if n < 0:
                n = 0
            if n:
                data = bytes(mu.mem_read(S_FILE + sc["filepos"], n))
                mu.mem_write(a(0) & 0xFFFFFFFF, data)
            sc["filepos"] += n
            t("pack_fread %s %d -> %d [%s]"
              % (self.sym(a(2)), size, n, self.hexs(a(0) & 0xFFFFFFFF, n)))
            return n
        if name == "pack_fclose":
            t("pack_fclose %s" % self.sym(a(0)))
            return 0

        # ---- Allegro drawing / timing ----
        if name == "text_length":
            s = self.gstr(a(1) & 0xFFFFFFFF)
            t('text_length %s "%s"' % (self.sym(a(0)), esc(s)))
            return len(s) * 9
        if name == "makecol":
            t("makecol %d %d %d" % (a(0), a(1), a(2)))
            return ((a(0) & 0xFF) << 16) | ((a(1) & 0xFF) << 8) | (a(2) & 0xFF)
        if name == "set_trans_blender":
            t("set_trans_blender %d %d %d %d" % (a(0), a(1), a(2), a(3)))
            return 0
        if name == "drawing_mode":
            t("drawing_mode %d %s %d %d" % (a(0), self.sym(a(1)), a(2), a(3)))
            return 0
        if name == "solid_mode":
            t("solid_mode")
            return 0
        if name == "blit":
            t("blit %s %s %d %d %d %d %d %d"
              % (self.sym(a(0)), self.sym(a(1)), a(2), a(3), a(4), a(5),
                 a(6), a(7)))
            return 0
        if name == "vsync":
            t("vsync")
            return 0
        if name == "clear_keybuf":
            t("clear_keybuf")
            return 0
        if name == "rest":
            t("rest %u" % (a(0) & 0xFFFFFFFF))
            sc["rest"] += 1
            r = sc["rest"]
            if sc["cycle_every"] > 0 and (r % sc["cycle_every"]) == 0:
                mu.mem_write(G_CYCLE_COUNT, u32(1))
            if r == sc["esc_on"]:
                mu.mem_write(G_KEY + KEY_ESC, b"\x01")
            if r == sc["esc_off"]:
                mu.mem_write(G_KEY + KEY_ESC, b"\x00")
            if r == sc["enter_on"]:
                mu.mem_write(G_KEY + KEY_ENTER, b"\x01")
            if r == sc["enter_off"]:
                mu.mem_write(G_KEY + KEY_ENTER, b"\x00")
            if r == sc["close_at"]:
                mu.mem_write(G_CLOSEBTN, u32(1))
            return 0
        if name in ("textprintf_ex", "textprintf_right_ex",
                    "textprintf_centre_ex"):
            fmt = self.gstr(a(6) & 0xFFFFFFFF)
            text = self.cfmt(fmt, esp, 7)
            t('%s %s %s %d %d %d %d "%s"'
              % (name, self.sym(a(0)), self.sym(a(1)), a(2), a(3), a(4), a(5),
                 tr(text)))
            return 0
        if name in ("textout_centre_ex", "textout_right_ex"):
            t('%s %s %s "%s" %d %d %d %d'
              % (name, self.sym(a(0)), self.sym(a(1)),
                 esc(self.gstr(a(2) & 0xFFFFFFFF)), a(3), a(4), a(5), a(6)))
            return 0
        if name == "rectfill":
            t("rectfill %s %d %d %d %d %d"
              % (self.sym(a(0)), a(1), a(2), a(3), a(4), a(5)))
            return 0
        if name in ("draw_sprite", "draw_256_sprite"):
            t("%s %s %s %d %d" % (name, self.sym(a(0)), self.sym(a(1)),
                                  a(2), a(3)))
            return 0
        if name in ("acquire", "release"):
            t("%s %s" % (name, self.sym(a(0))))
            return 0

        # ---- the control layer ----
        if name == "poll_control":
            t("poll_control %s %d" % (self.sym(a(0)), a(1)))
            return 0
        if name in ("is_any", "is_left", "is_right", "is_fire"):
            which = "c" if (a(0) & 0xFFFFFFFF) == G_CTRL else "m"
            q = sc["q"][name + "_" + which]
            v = q.pop(0) if q else 0
            t("%s %s -> %d" % (name, self.sym(a(0)), v))
            return v
        if name == "is_enter":
            q = sc["q"]["is_enter_m"]
            v = q.pop(0) if q else 0
            t("is_enter %s -> %d" % (self.sym(a(0)), v))
            return v
        raise RuntimeError("unhandled callee " + name)

    # ---- run one vector ----
    def run(self, va, args):
        mu = self.mu
        mu.emu_start(pf.CW_STUB, pf.CW_STUB + len(self.stub))
        esp = pf.STACK_BASE + pf.STACK_SIZE - 0x8000
        for v in reversed(args):
            esp -= 4
            mu.mem_write(esp, u32(v))
        esp -= 4
        mu.mem_write(esp, u32(pf.RET_MAGIC))
        mu.reg_write(UC_X86_REG_ESP, esp)
        pc, guard = va, 0
        while True:
            mu.emu_start(pc, pf.RET_MAGIC, count=40000000)
            eip = mu.reg_read(UC_X86_REG_EIP)
            if eip == pf.RET_MAGIC:
                break
            pc = eip
            guard += 1
            if guard > 2000000:
                raise RuntimeError("call-hook resume runaway")
        return mu.reg_read(UC_X86_REG_EAX)

    # ---- fixed world, rewritten before every vector ----
    def world(self):
        mu = self.mu
        self.trace = []
        self.syms = {0: "NULL", S_SWAP: "swap", S_SCREEN: "screen",
                     S_LOGO: "logo", S_TARGET: "target",
                     G_CTRL: "ctrl", G_MENU_CTRL: "menuctrl",
                     S_PACK: "packfile"}
        for i in range(NOBJ):
            self.syms[S_OBJ + OBJ_STRIDE * i] = "obj%d" % i
        mu.mem_write(S_VT8, b"\0" * 0x200)
        mu.mem_write(S_VT16, b"\0" * 0x200)
        mu.mem_write(S_VT8 + 0x00, u32(8))
        mu.mem_write(S_VT16 + 0x00, u32(16))
        for vt in (S_VT8, S_VT16):
            mu.mem_write(vt + 0x10, u32(VT_ACQUIRE))
            mu.mem_write(vt + 0x14, u32(VT_RELEASE))
            mu.mem_write(vt + 0x3c, u32(VT_RECTFILL))
            mu.mem_write(vt + 0x44, u32(VT_DRAW_SPR))
            mu.mem_write(vt + 0x48, u32(VT_DRAW_256))
        mu.mem_write(G_SWAP_SCREEN, u32(S_SWAP))
        mu.mem_write(G_SCREEN, u32(S_SCREEN))
        mu.mem_write(G_GFX_DRIVER, u32(S_GFX))
        mu.mem_write(G_DATA, u32(S_DF))
        mu.mem_write(G_PROFILE, u32(S_PROFILE))
        mu.mem_write(G_CLOSEBTN, u32(0))
        mu.mem_write(G_CYCLE_COUNT, u32(0))
        mu.mem_write(G_GUI_FG, u32(0))
        mu.mem_write(G_GUI_BG, u32(0))
        mu.mem_write(G_KEY + KEY_ESC, b"\x00")
        mu.mem_write(G_KEY + KEY_ENTER, b"\x00")
        for b in (S_SWAP, S_SCREEN, S_LOGO, S_TARGET):
            mu.mem_write(b, b"\0" * 0x40)
            mu.mem_write(b + 0x1c, u32(S_VT16))
        for i in range(NOBJ):
            p = S_OBJ + OBJ_STRIDE * i
            mu.mem_write(p, b"\0" * OBJ_STRIDE)
            mu.mem_write(p + 0x00, u32(32))
            mu.mem_write(p + 0x04, u32(24))
            mu.mem_write(p + 0x1c, u32(S_VT16))
            mu.mem_write(S_DF + 16 * i, u32(p))
        for i in range(15):
            mu.mem_write(S_CATS + 16 * i, ("cat%d" % i).encode() + b"\0")
            mu.mem_write(G_CATEGORIES + 4 * i, u32(S_CATS + 16 * i))
        mu.mem_write(S_ARENA, b"\xA5" * (ARENA_END - S_ARENA))
        self.script = {
            "malloc_seq": 0, "malloc_fail": -1, "arena": 0, "n_heap": 0,
            "packopen_seq": 0, "packopen1": 1, "packopen2": 1,
            "filepos": 0, "filelen": 0,
            "rest": 0, "cycle_every": 1,
            "esc_on": -1, "esc_off": -1, "enter_on": -1, "enter_off": -1,
            "close_at": -1,
            "q": {k: [] for k in ("is_any_c", "is_any_m", "is_left_c",
                                  "is_left_m", "is_right_c", "is_right_m",
                                  "is_fire_c", "is_fire_m", "is_enter_m")},
        }

    def set_objs_depth8(self, d8):
        for i in range(NOBJ):
            self.mu.mem_write(S_OBJ + OBJ_STRIDE * i + 0x1c,
                              u32(S_VT8 if d8 else S_VT16))


# --------------------------------------------------------------------------
# vectors
# --------------------------------------------------------------------------
NAMES = ["", "run.itr", "replays/x.itr", "C:/Icy Tower/replays/a b.itr",
         "a" * 60]
HANDLES = ["guest", "Guest", "GUEST", "MissingNO", "", "guesty", "gues"]
TEXTS = ["", "Are you sure?", "x", "a b c d e f g h i j k l m n o p",
         "Delete this replay?"]


def gen_replay_image(rnd, tc_posts=None, size=None):
    """A 0x8ac-byte Treplay image (its `data` pointer left zero)."""
    def v():
        return rnd.choice([0, 1, -1, rnd.randrange(-1000, 100000),
                           rnd.randrange(-(1 << 31), 1 << 31)])
    b = bytearray(REPLAY_BYTES)
    b[0:6] = b"ITR140" if rnd.random() < 0.85 else bytes(
        rnd.randrange(0, 256) for _ in range(6))
    if size is None:
        size = rnd.choice([0, 1, 2, 5, rnd.randrange(0, 40)])
    struct.pack_into("<i", b, 0x08, size)
    for off in range(0x0c, 0x4c):
        b[off] = rnd.choice([0, 32, rnd.randrange(0, 256), rnd.randrange(32, 127)])
    for off in (0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60):
        struct.pack_into("<i", b, off, v())
    for k in range(5):
        struct.pack_into("<i", b, 0x64 + 4 * k, v())
        struct.pack_into("<i", b, 0x78 + 4 * k, v())
    for off in (0x8c, 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4):
        struct.pack_into("<i", b, off, v())
    for off in range(0xa8, 0xd2):
        b[off] = rnd.choice([0, rnd.randrange(32, 127)])
    if tc_posts is None:
        tc_posts = rnd.choice([0, 1, 2, 6])
    struct.pack_into("<i", b, 0xd4, tc_posts)
    for col in range(5):
        base = 0xd8 + 400 * col
        for k in range(100):
            # only "printable" magnitudes: %2.2f on a 1e30 float is a
            # 30-digit string whose last digits are the CRT's business,
            # not this oracle's.  See the module note in PROMOTIONS.md.
            f = rnd.choice([0.0, 1.0, -1.0, 0.25, 49.94, 100.5,
                            round(rnd.uniform(-9999.0, 9999.0), 3),
                            float(rnd.randrange(-100000, 100000))])
            struct.pack_into("<f", b, base + 4 * k, f)
    return bytes(b), size, tc_posts


def gen_itr_file(rnd):
    """A byte image in save_replay()'s on-disk order (batch 14's finding 2),
    so that a load_replay vector is a REAL .itr layout most of the time --
    magic, count, the fields, the interleaved statistics and the 5-byte
    reversed records -- and a truncated or corrupt one the rest."""
    size = rnd.choice([0, 1, 2, 3, 9, rnd.randrange(0, 30)])
    body = bytearray()
    body += b"ITR140" if rnd.random() < 0.8 else bytes(
        rnd.randrange(0, 256) for _ in range(6))
    body += i32(size)
    body += bytes(rnd.choice([0, rnd.randrange(32, 127)]) for _ in range(32))
    body += bytes(rnd.choice([0, rnd.randrange(32, 127)]) for _ in range(32))
    for _ in range(5):
        body += i32(rnd.randrange(-1000, 100000))
    for _ in range(10):
        body += i32(rnd.randrange(0, 50))
    for _ in range(7):
        body += i32(rnd.randrange(0, 10))
    body += bytes(rnd.choice([0, rnd.randrange(32, 127)]) for _ in range(42))
    checksum_at = len(body)
    body += i32(0)
    body += i32(rnd.choice([0, 1, 3]))
    for _ in range(100):
        for _ in range(5):
            body += struct.pack("<f", round(rnd.uniform(-100.0, 100.0), 2))
    for _ in range(size):
        body += i32(rnd.randrange(0, 100000))
        body += bytes([rnd.randrange(0, 256)])
    # Truncate, but NEVER below ten bytes.  load_replay's first pass reads
    # the 6-byte magic and the 4-byte count into STACK locals and does not
    # check pack_fread's return value (finding 1 in src/icytower/replay.c),
    # so a file shorter than ten bytes makes it compare UNINITIALISED STACK
    # -- a real defect in the original, but one whose value is the two
    # sides' unrelated call histories and therefore not something any
    # oracle can compare.  Everything past byte ten is truncatable and IS
    # covered, because those reads land in the malloc'd Treplay, which both
    # sides pre-fill with 0xA5.
    if rnd.random() < 0.25 and len(body) > 11:   # truncate
        body = body[:rnd.randrange(10, len(body))]
    elif rnd.random() < 0.15:                    # flip one byte
        if body:
            k = rnd.randrange(0, len(body))
            body[k] ^= 1 << rnd.randrange(0, 8)
    return bytes(body), size, checksum_at


def gen_queue(rnd, maxlen, p_one):
    n = rnd.randrange(0, maxlen + 1)
    return [1 if rnd.random() < p_one else 0 for _ in range(n)]


def pack_queue(q):
    return i32(len(q)) + b"".join(i32(v) for v in q)


def gen(rnd, kind):
    """One vector: (kind_number, payload bytes, setup dict)."""
    if kind == "create_replay":
        size = rnd.choice([0, 1, 2, 7, -1, -5, rnd.randrange(0, 500)])
        fail = rnd.choice([-1, -1, -1, 0, 1])
        return K_CREATE, i32(size) + i32(fail), {"malloc_fail": fail,
                                                 "size": size}

    if kind == "load_replay":
        name = rnd.choice(NAMES)
        ok1 = 0 if rnd.random() < 0.12 else 1
        ok2 = 0 if rnd.random() < 0.12 else 1
        fail = rnd.choice([-1, -1, -1, -1, 0, 1])
        content, size, _ = gen_itr_file(rnd)
        nb = name.encode("latin1")
        p = (i32(ok1) + i32(ok2) + i32(fail) + i32(len(nb)) + nb
             + i32(len(content)) + content)
        return K_LOAD, p, {"packopen1": ok1, "packopen2": ok2,
                           "malloc_fail": fail, "name": name,
                           "content": content}

    if kind == "getGameDataXML":
        rb, size, tcp = gen_replay_image(rnd)
        flags = [1 if rnd.random() < 0.5 else 0 for _ in range(5)]
        gd = [rnd.choice([0, 1, -1, rnd.randrange(-1000, 100000)])
              for _ in range(5)]
        ccc = [rnd.choice([0, 1, rnd.randrange(-5, 40)]) for _ in range(5)]
        jc = [rnd.choice([0, 1, rnd.randrange(-5, 40)]) for _ in range(5)]
        if rnd.random() < 0.25:
            # DIRECTED: make the game data agree with the replay exactly.
            # Fifteen independent random comparisons are a mismatch with
            # probability ~1, so without this the `"match"` arm at 0x404746
            # is never entered -- which the --coverage run says out loud.
            gd = list(struct.unpack_from("<5i", rb, 0x50))
            ccc = list(struct.unpack_from("<5i", rb, 0x64))
            jc = list(struct.unpack_from("<5i", rb, 0x78))
        nc = rnd.choice([0, 1, 2, 5])
        nj = rnd.choice([0, 1, 3, 6])
        p = rb
        p += b"".join(i32(x) for x in flags)
        p += b"".join(i32(x) for x in gd)
        p += b"".join(i32(x) for x in ccc)
        p += b"".join(i32(x) for x in jc)
        p += i32(nc)
        combos = []
        for _ in range(nc):
            c = [rnd.randrange(0, 300), rnd.randrange(0, 300),
                 rnd.randrange(0, 60)]
            combos.append(c)
            p += b"".join(i32(x) for x in c)
        p += i32(nj)
        jumps = []
        for _ in range(nj):
            j = [rnd.randrange(0, 300), rnd.randrange(0, 30),
                 rnd.randrange(0, 20)]
            jumps.append(j)
            p += b"".join(i32(x) for x in j)
        keys = [rnd.randrange(0, 5000) for _ in range(3)]
        p += b"".join(i32(x) for x in keys)
        return K_XML, p, {"replay": rb, "flags": flags, "gd": gd, "ccc": ccc,
                          "jc": jc, "combos": combos, "jumps": jumps,
                          "keys": keys}

    if kind == "draw_results":
        w = rnd.choice([0, 1, 200, 321, 640, rnd.randrange(0, 900)])
        h = rnd.choice([0, 1, 40, 100, rnd.randrange(0, 300)])
        logo8 = 1 if rnd.random() < 0.4 else 0
        obj8 = 1 if rnd.random() < 0.4 else 0
        y = rnd.choice([0, 10, 100, -20, rnd.randrange(-100, 400)])
        qual = [rnd.choice([0, 1, 3, -1, rnd.randrange(-2, 6)])
                for _ in range(5)]
        qval = [rnd.randrange(-1000, 100000) for _ in range(5)]
        showq = 1 if rnd.random() < 0.7 else 0
        npb = [rnd.choice([0, 1, -1, rnd.randrange(-2, 4)]) for _ in range(15)]
        handle = rnd.choice(HANDLES).encode("latin1")[:31]
        handle = handle + b"\0" * (32 - len(handle))
        p = (i32(w) + i32(h) + i32(logo8) + i32(obj8) + i32(y)
             + b"".join(i32(x) for x in qual)
             + b"".join(i32(x) for x in qval)
             + i32(showq)
             + b"".join(i32(x) for x in npb)
             + handle)
        return K_RESULTS, p, {"w": w, "h": h, "logo8": logo8, "obj8": obj8,
                              "y": y, "qual": qual, "qval": qval,
                              "showq": showq, "npb": npb, "handle": handle}

    if kind == "my_alert":
        fnull = 1 if rnd.random() < 0.2 else 0
        tnull = 1 if rnd.random() < 0.3 else 0
        choice = 1 if rnd.random() < 0.6 else 0
        hint = 1 if rnd.random() < 0.4 else 0
        gnull = 1 if rnd.random() < 0.15 else 0
        w = rnd.choice([640, 800, 0, rnd.randrange(0, 1200)])
        h = rnd.choice([480, 600, 0, rnd.randrange(0, 1000)])
        obj8 = 1 if rnd.random() < 0.4 else 0
        cyc = rnd.choice([1, 1, 2, 3])
        # the timeline is keyed on the rest() count; -1 means "never"
        esc_on = rnd.choice([-1, -1, rnd.randrange(1, 25)])
        esc_off = -1 if esc_on < 0 else esc_on + rnd.randrange(1, 6)
        enter_on = rnd.choice([-1, -1, rnd.randrange(1, 25)])
        enter_off = -1 if enter_on < 0 else enter_on + rnd.randrange(1, 6)
        # a backstop, so a vector whose queues run dry still terminates
        close_at = rnd.randrange(20, 90)
        qs = [gen_queue(rnd, 6, 0.5),      # is_any ctrl
              gen_queue(rnd, 6, 0.5),      # is_any menu
              gen_queue(rnd, 12, 0.3),     # is_left ctrl
              gen_queue(rnd, 12, 0.3),     # is_left menu
              gen_queue(rnd, 12, 0.3),     # is_right ctrl
              gen_queue(rnd, 12, 0.3),     # is_right menu
              gen_queue(rnd, 12, 0.2),     # is_fire ctrl
              gen_queue(rnd, 12, 0.2),     # is_fire menu
              gen_queue(rnd, 12, 0.2)]     # is_enter menu
        sf = rnd.choice(TEXTS).encode("latin1")
        st = rnd.choice(TEXTS).encode("latin1")
        p = (i32(fnull) + i32(tnull) + i32(choice) + i32(hint) + i32(gnull)
             + i32(w) + i32(h) + i32(obj8) + i32(cyc)
             + i32(esc_on) + i32(esc_off) + i32(enter_on) + i32(enter_off)
             + i32(close_at)
             + b"".join(pack_queue(q) for q in qs)
             + i32(len(sf)) + sf + i32(len(st)) + st)
        return K_ALERT, p, {"fnull": fnull, "tnull": tnull, "choice": choice,
                            "hint": hint, "gnull": gnull, "w": w, "h": h,
                            "obj8": obj8, "cyc": cyc,
                            "esc_on": esc_on, "esc_off": esc_off,
                            "enter_on": enter_on, "enter_off": enter_off,
                            "close_at": close_at, "qs": qs,
                            "func": sf.decode("latin1"),
                            "txt": st.decode("latin1")}
    raise RuntimeError(kind)


KINDS = ["create_replay", "load_replay", "getGameDataXML", "draw_results",
         "my_alert"]

WEIGHT = {"create_replay": 1.0, "load_replay": 0.02, "getGameDataXML": 0.10,
          "draw_results": 0.60, "my_alert": 0.20}

QNAMES = ["is_any_c", "is_any_m", "is_left_c", "is_left_m",
          "is_right_c", "is_right_m", "is_fire_c", "is_fire_m", "is_enter_m"]


def original_result(orig, kind, st):
    mu = orig.mu
    orig.world()
    sc = orig.script

    if kind == "create_replay":
        sc["malloc_fail"] = st["malloc_fail"]
        eax = orig.run(FN[kind], [st["size"] & 0xFFFFFFFF])
        out = list(orig.trace)
        out.append("ret %s" % orig.sym(eax))
        if eax:
            out.append("replay [%s]" % orig.hexs(eax, REPLAY_HEAD))
            dp = struct.unpack("<I", bytes(mu.mem_read(eax + REPLAY_HEAD, 4)))[0]
            out.append("data %s" % orig.sym(dp))
            if dp:
                nb = 0x20 + st["size"] * 8
                nb = max(0x20, min(8192, nb))
                out.append("records [%s]" % orig.hexs(dp, nb))
        return out

    if kind == "load_replay":
        sc["packopen1"] = st["packopen1"]
        sc["packopen2"] = st["packopen2"]
        sc["malloc_fail"] = st["malloc_fail"]
        sc["filelen"] = len(st["content"])
        if st["content"]:
            mu.mem_write(S_FILE, st["content"])
        nb = st["name"].encode("latin1")
        mu.mem_write(S_NAME, nb + b"\0")
        eax = orig.run(FN[kind], [S_NAME])
        out = list(orig.trace)
        out.append("ret %s" % orig.sym(eax))
        if eax:
            size = struct.unpack("<i", bytes(mu.mem_read(eax + 8, 4)))[0]
            nbytes = 0x20 + size * 8
            nbytes = max(0x20, min(8192, nbytes))
            out.append("replay [%s]" % orig.hexs(eax, REPLAY_HEAD))
            dp = struct.unpack("<I", bytes(mu.mem_read(eax + REPLAY_HEAD, 4)))[0]
            out.append("data %s" % orig.sym(dp))
            if dp:
                out.append("records [%s]" % orig.hexs(dp, nbytes))
        return out

    if kind == "getGameDataXML":
        mu.mem_write(S_GD, b"\0" * GD_BYTES)
        mu.mem_write(S_XREPLAY, st["replay"])
        mu.mem_write(S_XREPLAY + REPLAY_HEAD, u32(0))
        mu.mem_write(S_GD + 0x00, u32(S_XREPLAY))
        for k, v in enumerate(st["flags"]):
            mu.mem_write(G_CMDLINE + 4 * k, i32(v))
        for k, v in enumerate(st["gd"]):
            mu.mem_write(S_GD + 0x04 + 4 * k, i32(v))
        for k, v in enumerate(st["ccc"]):
            mu.mem_write(S_GD + 0x18 + 4 * k, i32(v))
        for k, v in enumerate(st["jc"]):
            mu.mem_write(S_GD + 0x2c + 4 * k, i32(v))
        mu.mem_write(S_GD + 0x40, i32(len(st["combos"])))
        for k, c in enumerate(st["combos"]):
            for j in range(3):
                mu.mem_write(S_GD + 0x44 + 12 * k + 4 * j, i32(c[j]))
        mu.mem_write(S_GD + 0xeaa4, i32(len(st["jumps"])))
        for k, j2 in enumerate(st["jumps"]):
            for j in range(3):
                mu.mem_write(S_GD + 0xeaa8 + 12 * k + 4 * j, i32(j2[j]))
        for k, v in enumerate(st["keys"]):
            mu.mem_write(S_GD + 0x1d508 + 4 * k, i32(v))
        eax = orig.run(FN[kind], [S_GD])
        out = list(orig.trace)
        s = orig.gstr(eax)
        out.append('xml "%s"' % esc(s))
        out.append("xmllen %d" % len(s))
        return out

    if kind == "draw_results":
        mu.mem_write(S_LOGO + 0x00, i32(st["w"]))
        mu.mem_write(S_LOGO + 0x04, i32(st["h"]))
        mu.mem_write(S_LOGO + 0x1c, u32(S_VT8 if st["logo8"] else S_VT16))
        orig.set_objs_depth8(st["obj8"])
        for k, v in enumerate(st["qual"]):
            mu.mem_write(S_QUAL + 4 * k, i32(v))
        for k, v in enumerate(st["qval"]):
            mu.mem_write(S_QVAL + 4 * k, i32(v))
        for k, v in enumerate(st["npb"]):
            mu.mem_write(G_NEW_PB + 4 * k, i32(v))
        mu.mem_write(S_PROFILE, b"\0" * 0x60)
        mu.mem_write(S_PROFILE + 6, st["handle"])
        orig.run(FN[kind], [S_TARGET, S_LOGO, st["y"] & 0xFFFFFFFF,
                            S_QUAL, S_QVAL, st["showq"]])
        return list(orig.trace)

    if kind == "my_alert":
        mu.mem_write(G_GFX_DRIVER, u32(0 if st["gnull"] else S_GFX))
        mu.mem_write(S_GFX + 0x6c, i32(st["w"]))
        mu.mem_write(S_GFX + 0x70, i32(st["h"]))
        orig.set_objs_depth8(st["obj8"])
        sc["cycle_every"] = st["cyc"]
        sc["esc_on"] = st["esc_on"]
        sc["esc_off"] = st["esc_off"]
        sc["enter_on"] = st["enter_on"]
        sc["enter_off"] = st["enter_off"]
        sc["close_at"] = st["close_at"]
        for nm, q in zip(QNAMES, st["qs"]):
            sc["q"][nm] = list(q)
        mu.mem_write(S_STRF, st["func"].encode("latin1") + b"\0")
        mu.mem_write(S_STRT, st["txt"].encode("latin1") + b"\0")
        eax = orig.run(FN[kind],
                       [0 if st["fnull"] else S_STRF,
                        0 if st["tnull"] else S_STRT,
                        st["choice"], st["hint"]])
        out = list(orig.trace)
        out.append("ret %d" % si32(eax))
        out.append("gui %d %d" % (orig.r32(G_GUI_FG), orig.r32(G_GUI_BG)))
        return out
    raise RuntimeError(kind)


DISASM = os.path.join(PROJ, "artifacts", "disasm.txt")
_ADDR_CACHE = {}


def fn_addresses(name):
    """Every instruction address artifacts/disasm.txt lists for `name`."""
    import re as _re
    if name in _ADDR_CACHE:
        return _ADDR_CACHE[name]
    pat = _re.compile(r"^([0-9a-f]{6,8}) <_?%s>:" % _re.escape(name))
    out, started = [], False
    with open(DISASM, encoding="utf-8", errors="replace") as f:
        for ln in f:
            if not started:
                if pat.match(ln):
                    started = True
                continue
            if _re.match(r"^[0-9a-f]{6,8} <", ln):
                break
            # objdump WRAPS a long instruction onto a second line that
            # carries an address but no mnemonic ("  41cdfb:	00 ").
            # Counting those as instructions would understate coverage by
            # exactly the number of wrapped encodings, which is a lot in
            # this code (every `movl $imm,disp(%esp)` wraps).
            m = _re.match(r"^\s+([0-9a-f]{6,8}):	[0-9a-f ]+	\S", ln)
            if m:
                out.append(int(m.group(1), 16))
    _ADDR_CACHE[name] = out
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vectors", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--only", default=None)
    ap.add_argument("--fault", action="store_true",
                    help="negative control: corrupt one ORIGINAL-side record "
                         "at vector 5 of each kind and require a DIFFER")
    ap.add_argument("--coverage", action="store_true",
                    help="also report, per function, the fraction of the "
                         "instruction addresses artifacts/disasm.txt lists "
                         "for it that these vectors actually executed")
    args = ap.parse_args()

    kinds = [args.only] if args.only else KINDS
    base = max(1, args.vectors // len(kinds))
    counts = {k: (20 if args.fault else max(1, int(base * WEIGHT[k])))
              for k in kinds}

    rnd = random.Random(args.seed)
    recs, expected = [], []
    for kind in kinds:
        for k in range(counts[kind]):
            knum, payload, st = gen(rnd, kind)
            recs.append(struct.pack("<II", knum, len(payload)) + payload)
            expected.append((kind, k, st))

    with open(VECFILE, "wb") as f:
        f.write(b"".join(recs))

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

    orig = Original()
    if args.coverage:
        orig.enable_coverage([(FN[k], FN[k] + 8000) for k in kinds])
    fails = {k: 0 for k in kinds}
    shown = 0
    for i, (kind, k, st) in enumerate(expected):
        got = original_result(orig, kind, st)
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
                for a, b in zip(got + [""] * 40000, cand + [""] * 40000):
                    if a != b:
                        print("    original:  %s" % a[:300])
                        print("    candidate: %s" % b[:300])
                        break
    if args.coverage:
        print("")
        for kind in kinds:
            addrs = fn_addresses(kind)
            hit = sum(1 for a in addrs if a in orig.cover)
            print("%-16s coverage %d/%d instructions (%.1f%%)"
                  % (kind, hit, len(addrs),
                     100.0 * hit / len(addrs) if addrs else 0.0))
            miss = [a for a in addrs if a not in orig.cover]
            if miss:
                print("                 first unexecuted: %s"
                      % " ".join("0x%x" % a for a in miss[:12]))
        print("")
    bad = 0
    for kind in kinds:
        print("%-16s %d of %d vectors differ" % (kind, fails[kind],
                                                 counts[kind]))
        if args.fault:
            if fails[kind] == 0:
                print("NEGATIVE CONTROL FAILED: %s fault not detected" % kind)
                bad = 1
        else:
            bad |= (1 if fails[kind] else 0)
    sys.exit(bad)


if __name__ == "__main__":
    main()
