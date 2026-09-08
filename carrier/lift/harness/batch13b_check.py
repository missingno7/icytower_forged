#!/usr/bin/env python3
"""batch13b_check.py -- ordered-call-trace oracle for PROMOTIONS.md
batch 13's six tick-path functions:

    play_sound      0x406da4  215 B   src/icytower/sound.c
    startGameMusic  0x40cb30  144 B   src/icytower/sound.c
    stopGameMusic   0x40caf4   58 B   src/icytower/sound.c
    log2file        0x40da58  189 B   src/icytower/logfile.c
    take_screenshot 0x41002c  203 B   src/icytower/screenshot.c
    draw_reward     0x4070fc  581 B   src/icytower/draw_reward.c

Why this is not a lift_check.py SPECS entry
-------------------------------------------
Same two reasons batch 10 (draw_frame) and batch 12 (play) gave, plus one
that is specific to this batch:

  * mechanism B (lift_check.py's shared call-trace domain) records per
    callee a COUNT plus the arguments of its FIRST call.  take_screenshot
    calls sprintf/exists once per loop iteration with a different filename
    each time, and stopGameMusic is DEFINED by the order of three
    independently-guarded teardowns -- first-call capture would compare a
    fraction of either.
  * batch 11's blocker for play_sound was that pf_harness_calltrace.h
    redirects the NAME play_sound to a stub for the whole SPECS build,
    which would rename play_sound's own definition.  batch13b_check.exe
    never links that harness, so the two builds are disjoint and both
    redirects stand.  The eight existing SPECS entries that trace a
    stubbed play_sound are untouched by this file.

How it works
------------
1. The ORIGINAL bytes run under unicorn on the same engine every other
   oracle in this directory uses (port_forge/tools/pf_win32_offline_oracle
   build_guest + its FNINIT/FLDCW convention).  Every library, CRT and
   game callee is hooked at its own VA and stubbed, appending one record
   per call, IN ORDER, with its arguments.
2. Two callees are not reached by a direct `call <va>`:
     - pthread_mutex_lock / pthread_mutex_unlock, which log2file reaches
       as `call *0x514a5c` / `call *0x514a60` (IAT slots; pefile resolves
       them to pthreadGC2.dll).  The slots are rewritten with otherwise
       unused guest VAs and those are hooked like any named callee.
     - GFX_VTABLE's +0xa4 `pivot_scaled_sprite_flip`, which draw_reward
       reaches through Allegro's rotate_scaled_sprite AL_INLINE.  Batch
       9's synthetic-vtable-VA trick, the same one draw_frame_xcheck.py
       uses for its five slots.
3. The compiled candidate is batch13b_check.exe, built straight from
   src/icytower/{sound,logfile,screenshot,draw_reward}.c (build_batch13.sh)
   and driven by a vector FILE, one line per vector -- one process for the
   whole run, not one per vector.
4. Pointers are rendered as stable SYMBOLS (sample / bg_music / bg_midi /
   bg_beat / bmp / reward / sub / file / last_log / sLogMutex) and strings
   as their CONTENT AT CALL TIME, so the guest and host address spaces
   never have to agree.  Resolving a string at comparison time would be
   wrong here for the same reason it is wrong in draw_frame_xcheck.py:
   take_screenshot rewrites its `name` buffer on every loop iteration.

The printf subset
-----------------
log2file is variadic, so its oracle has to format on the unicorn side
too.  Scanning the whole image (every `call 0x40da58` preceded by a
literal format pointer -- 187 of the 188 call sites) yields exactly three
distinct conversions in the entire game: `%s`, `%d`, `%-12s`.  Python's
own `%` operator is byte-identical to C's for all three, so the format
subset this file supports is closed over what the binary can actually
produce, and the vectors exercise the same three plus literal text.

Usage
-----
  python batch13b_check.py --vectors 20000 --seed 20260908
  python batch13b_check.py --fault
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
CHECK_EXE = os.path.join(HERE, "batch13b_check.exe")
VECFILE = os.path.join(HERE, "batch13b_vectors.txt")

# ------------------------------------------------------------ functions
FN = {
    "play_sound":      0x406da4,
    "startGameMusic":  0x40cb30,
    "stopGameMusic":   0x40caf4,
    "log2file":        0x40da58,
    "take_screenshot": 0x41002c,
    "draw_reward":     0x4070fc,
}

# ------------------------------------------------------------- globals
G_ITRCHECK       = 0x4dd168
G_OPTIONS        = 0x4fe528      # flash +0, jump_hold +8, msc +0x24, snd +0x28
G_OPT_FLASH      = G_OPTIONS + 0x00
G_OPT_MSC        = G_OPTIONS + 0x24
G_OPT_SND        = G_OPTIONS + 0x28
G_FAST_FORWARD   = 0x4dd254
G_FAST_FAST_FWD  = 0x4dd258
G_PLAYER_ID      = 0x4fe518
G_PLY            = 0x4ff128
G_ANY11          = 0x4dd170
G_MUSIC_VOICE    = 0x4bc178
G_CUSTOM_BGMUSIC = 0x4fac10      # custom + 0x4d8
G_CUSTOM_BGMIDI  = 0x4fac14      # custom + 0x4dc
G_BG_BEAT        = 0x4dd2a8
G_LAST_LOG       = 0x4f89e8      # char[256]
G_LOGFILENAME    = 0x4dd340      # char[1024]
G_LOG_MUTEX      = 0x4bdb44
G_SCREENSHOT_N   = 0x4dd334
G_KEY            = 0x506988      # Allegro key[]; KEY_F1 == 0x2f
KEY_F1           = 0x2f
G_REWARD_BMP     = 0x4f8af8
G_REWARD_SCALE   = 0x4fac28

# --------------------------------------------------------- scratch VAs
S = 0x7d0000
S_SAMPLE   = S + 0x0000
S_BGMUSIC  = S + 0x0040
S_BGBEAT   = S + 0x0080
S_BGMIDI   = S + 0x00c0
S_PLAYER   = S + 0x0100          # Tplayer; x is a double at offset 0
S_DEST     = S + 0x0200          # BITMAP: w+0 h+4 vtable+0x1c
S_REWARD   = S + 0x0280
S_SUB      = S + 0x0300
S_VTABLE   = S + 0x0400          # GFX_VTABLE, +0xa4 slot used
S_LOGPATH  = S + 0x0800          # the string get_logfile_path writes
S_FILE     = S + 0x0900          # the FILE * fopen hands back

SYMS = {
    0: "NULL", S_SAMPLE: "sample", S_BGMUSIC: "bg_music", S_BGBEAT: "bg_beat",
    S_BGMIDI: "bg_midi", S_DEST: "bmp", S_REWARD: "reward", S_SUB: "sub",
    S_FILE: "file", G_LAST_LOG: "last_log", G_LOG_MUTEX: "sLogMutex",
}

# synthetic callee VAs (never real code)
VT_PIVOT   = 0x7c4000
IAT_LOCK   = 0x7c4100
IAT_UNLOCK = 0x7c4110
IAT_LOCK_SLOT   = 0x514a5c
IAT_UNLOCK_SLOT = 0x514a60

# real callee VAs -> (name, argc)
CALLEES = {
    0x440404: ("play_sample", 5),
    0x440990: ("set_volume", 2),
    0x443648: ("play_midi", 2),
    0x43fe10: ("voice_stop", 1),
    0x43fc64: ("stop_sample", 1),
    0x4438d8: ("stop_midi", 0),
    0x406984: ("new_rand", 0),
    0x446218: ("exists", 1),
    0x44c47c: ("get_palette", 1),
    0x44f448: ("create_sub_bitmap", 5),
    0x452ef8: ("save_bitmap", 3),
    0x44f14c: ("destroy_bitmap", 1),
    0x463398: ("stretch_sprite", 6),
    0x4039f4: ("get_logfile_path", 2),
    0x4bad28: ("fopen", 2),
    0x4badb0: ("vfprintf", 3),
    0x4badb8: ("vsprintf", 3),
    0x4badc0: ("fputc", 2),
    0x4bad30: ("fclose", 1),
    0x4bad60: ("sprintf", 2),          # 2 fixed; varargs read from the fmt
    VT_PIVOT: ("pivot_scaled_sprite_flip", 9),
    IAT_LOCK: ("mutex_lock", 1),
    IAT_UNLOCK: ("mutex_unlock", 1),
}


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def si(v):
    return ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000


# --------------------------------------------------------------------------
# the printf subset (see the module docstring: %s / %d / %-12s is the whole
# of what the image's 187 literal log2file format strings contain)
# --------------------------------------------------------------------------
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
            out.append(fmt[j])
        i = j + 1
    return out


def c_format(fmt, args):
    """Apply the C format with Python's own %; identical for %s/%d/%-12s."""
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
        # the two IAT slots log2file calls through
        mu.mem_write(IAT_LOCK_SLOT, u32(IAT_LOCK))
        mu.mem_write(IAT_UNLOCK_SLOT, u32(IAT_UNLOCK))
        self.trace = []
        self.script = {}
        for va, (name, argc) in CALLEES.items():
            mu.hook_add(UC_HOOK_CODE, self._mk(name, argc), begin=va, end=va)

    # ---- guest helpers ----
    def gstr(self, va, cap=512):
        out = b""
        while len(out) < cap:
            c = bytes(self.mu.mem_read(va + len(out), 1))
            if c == b"\0":
                break
            out += c
        return out.decode("latin1")

    def sym(self, p):
        return SYMS.get(p & 0xFFFFFFFF, "?")

    def w32(self, va, v):
        self.mu.mem_write(va, u32(v))

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

    def dispatch(self, name, argc, a):
        t = self.trace.append
        if name == "play_sample":
            t("play_sample %s %d %d %d %d"
              % (self.sym(a(0)), a(1), a(2), a(3), a(4)))
            return self.script["play_sample_ret"]
        if name == "set_volume":
            t("set_volume %d %d" % (a(0), a(1))); return 0
        if name == "play_midi":
            t("play_midi %s %d" % (self.sym(a(0)), a(1))); return 0
        if name == "voice_stop":
            t("voice_stop %d" % a(0)); return 0
        if name == "stop_sample":
            t("stop_sample %s" % self.sym(a(0))); return 0
        if name == "stop_midi":
            t("stop_midi"); return 0
        if name == "new_rand":
            seq = self.script["rand"]
            v = seq[self.script["rand_i"] % len(seq)] if seq else 0
            self.script["rand_i"] += 1
            t("new_rand -> %d" % v)
            return v
        if name == "exists":
            r = 1 if self.script["exists_i"] < self.script["exists_hits"] else 0
            self.script["exists_i"] += 1
            t('exists "%s" -> %d' % (self.gstr(a(0)), r))
            return r
        if name == "get_palette":
            t("get_palette"); return 0
        if name == "create_sub_bitmap":
            t("create_sub_bitmap %s %d %d %d %d"
              % (self.sym(a(0)), a(1), a(2), a(3), a(4)))
            return S_SUB
        if name == "save_bitmap":
            t('save_bitmap "%s" %s' % (self.gstr(a(0)), self.sym(a(1))))
            return 0
        if name == "destroy_bitmap":
            t("destroy_bitmap %s" % self.sym(a(0))); return 0
        if name == "stretch_sprite":
            t("stretch_sprite %s %s %d %d %d %d"
              % (self.sym(a(0)), self.sym(a(1)), a(2), a(3), a(4), a(5)))
            return 0
        if name == "pivot_scaled_sprite_flip":
            t("pivot_scaled_sprite_flip %s %s %d %d %d %d %d %d %d"
              % (self.sym(a(0)), self.sym(a(1)), a(2), a(3), a(4), a(5),
                 a(6), a(7), a(8)))
            return 0
        if name == "get_logfile_path":
            t("get_logfile_path %d" % a(1))
            self.mu.mem_write(a(0), self.script["logpath"].encode("latin1") + b"\0")
            return 1
        if name == "fopen":
            ok = self.script["fopen_ok"]
            t('fopen "%s" "%s" -> %s'
              % (self.gstr(a(0)), self.gstr(a(1)), "file" if ok else "NULL"))
            return S_FILE if ok else 0
        if name in ("vfprintf", "vsprintf"):
            #      vfprintf(FILE *f, fmt, ap)   vsprintf(char *dst, fmt, ap)
            fmt = self.gstr(a(1))
            text = c_format(fmt, self.read_varargs(fmt, a(2)))
            if name == "vfprintf":
                t('vfprintf %s "%s" -> "%s"' % (self.sym(a(0)), fmt, text))
            else:
                t('vsprintf %s "%s"' % (self.sym(a(0)), fmt))
                self.mu.mem_write(a(0), text.encode("latin1") + b"\0")
            return len(text)
        if name == "fputc":
            t("fputc %d %s" % (a(0), self.sym(a(1)))); return a(0)
        if name == "fclose":
            t("fclose %s" % self.sym(a(0))); return 0
        if name == "sprintf":
            fmt = self.gstr(a(1))
            args, esp_off = [], 2
            for c in conversions(fmt):
                v = a(esp_off); esp_off += 1
                args.append(self.gstr(v & 0xFFFFFFFF) if c == "s" else v)
            text = c_format(fmt, args)
            self.mu.mem_write(a(0), text.encode("latin1") + b"\0")
            t('sprintf "%s" -> "%s"' % (fmt, text))
            return len(text)
        if name == "mutex_lock":
            t("mutex_lock %s" % self.sym(a(0))); return 0
        if name == "mutex_unlock":
            t("mutex_unlock %s" % self.sym(a(0))); return 0
        raise RuntimeError("unhandled callee " + name)

    def read_varargs(self, fmt, ap):
        """`ap` is a plain pointer into the caller's argument block (x86
        cdecl), so the varargs are consecutive dwords starting there."""
        out, off = [], 0
        for c in conversions(fmt):
            v = struct.unpack("<i", bytes(self.mu.mem_read(ap + off, 4)))[0]
            off += 4
            out.append(self.gstr(v & 0xFFFFFFFF) if c == "s" else v)
        return out

    # ---- run one vector ----
    def run(self, va, args, script):
        self.trace = []
        self.script = dict(script)
        self.script.setdefault("rand", [0])
        self.script.setdefault("rand_i", 0)
        self.script.setdefault("exists_i", 0)
        self.script.setdefault("exists_hits", 0)
        self.script.setdefault("play_sample_ret", 0)
        self.script.setdefault("fopen_ok", 0)
        self.script.setdefault("logpath", "logs/it.txt")
        mu = self.mu
        mu.emu_start(pf.CW_STUB, pf.CW_STUB + len(self.stub))
        esp = pf.STACK_BASE + pf.STACK_SIZE - 0x2000
        for v in reversed(args):
            esp -= 4
            mu.mem_write(esp, u32(v))
        esp -= 4
        mu.mem_write(esp, u32(pf.RET_MAGIC))
        mu.reg_write(UC_X86_REG_ESP, esp)
        pc, guard = va, 0
        while True:
            mu.emu_start(pc, pf.RET_MAGIC, count=2000000)
            eip = mu.reg_read(UC_X86_REG_EIP)
            if eip == pf.RET_MAGIC:
                break
            pc = eip
            guard += 1
            if guard > 40000:
                raise RuntimeError("call-hook resume runaway")
        return self.trace


# --------------------------------------------------------------------------
# vectors
# --------------------------------------------------------------------------
LOG_FORMATS = [
    " play started",
    "  game paused with esc",
    "    destroying %s",
    "loading %s ...",
    "  score %d",
    "%-12s %d",
    "%s = %d",
    "100%% done",
    "%s / %s",
]
LOG_WORDS = ["tower.dat", "profile", "a b c", "", "x", "replays/human_test.txt"]


def gen(rnd, kind):
    """One vector: (exe-line-fields, guest-writes, args, script)."""
    if kind == "play_sound":
        itr = 1 if rnd.random() < 0.08 else 0
        snd = rnd.choice([0, 0, 1, 128, 255, rnd.randrange(0, 256)])
        ff = 1 if rnd.random() < 0.3 else 0
        fff = 1 if rnd.random() < 0.3 else 0
        pid = rnd.randrange(0, 4)
        x = rnd.choice([0.0, 320.0, 640.0, -100.0, 1234.5,
                        rnd.uniform(-2000.0, 2000.0)])
        s_null = 1 if rnd.random() < 0.15 else 0
        pitch = 1 if rnd.random() < 0.5 else 0
        pan = 1 if rnd.random() < 0.5 else 0
        any11 = rnd.randrange(-1000, 1000)
        r = rnd.randrange(0, 65536)
        line = "play_sound|%d|%d|%d|%d|%d|%016x|%d|%d|%d|%d|%d" % (
            itr, snd, ff, fff, pid, struct.unpack("<Q", struct.pack("<d", x))[0],
            s_null, pitch, pan, any11, r)
        writes = [(G_ITRCHECK, u32(itr)), (G_OPT_SND, u32(snd)),
                  (G_FAST_FORWARD, u32(ff)), (G_FAST_FAST_FWD, u32(fff)),
                  (G_PLAYER_ID, u32(pid)),
                  (G_PLY + 4 * pid, u32(S_PLAYER)),
                  (S_PLAYER, struct.pack("<d", x)),
                  (G_ANY11, u32(any11))]
        args = [0 if s_null else S_SAMPLE, pitch, pan]
        return line, writes, args, {"rand": [r]}, [(G_ANY11, "any11")]

    if kind == "startGameMusic":
        msc = rnd.choice([0, 0, 1, 200, 255])
        bm = 1 if rnd.random() < 0.4 else 0
        bmid = 1 if rnd.random() < 0.4 else 0
        bb = 1 if rnd.random() < 0.6 else 0
        vid = rnd.randrange(-5, 40)
        ret = rnd.randrange(-1, 32)
        line = "startGameMusic|%d|%d|%d|%d|%d|%d" % (msc, bm, bmid, bb, vid, ret)
        writes = [(G_OPT_MSC, u32(msc)),
                  (G_CUSTOM_BGMUSIC, u32(S_BGMUSIC if bm else 0)),
                  (G_CUSTOM_BGMIDI, u32(S_BGMIDI if bmid else 0)),
                  (G_BG_BEAT, u32(S_BGBEAT if bb else 0)),
                  (G_MUSIC_VOICE, u32(vid))]
        return (line, writes, [], {"play_sample_ret": ret},
                [(G_MUSIC_VOICE, "gameMusicVoiceID")])

    if kind == "stopGameMusic":
        vid = rnd.choice([-1, -1, 0, 1, 31, rnd.randrange(-40, 40)])
        bm = 1 if rnd.random() < 0.5 else 0
        bmid = 1 if rnd.random() < 0.5 else 0
        line = "stopGameMusic|%d|%d|%d" % (vid, bm, bmid)
        writes = [(G_MUSIC_VOICE, u32(vid)),
                  (G_CUSTOM_BGMUSIC, u32(S_BGMUSIC if bm else 0)),
                  (G_CUSTOM_BGMIDI, u32(S_BGMIDI if bmid else 0))]
        return line, writes, [], {}, [(G_MUSIC_VOICE, "gameMusicVoiceID")]

    if kind == "log2file":
        itr = 1 if rnd.random() < 0.1 else 0
        empty = 1 if rnd.random() < 0.5 else 0
        ok = 0 if rnd.random() < 0.2 else 1
        path = rnd.choice(["logs/it.txt", "C:/Icy Tower/log.txt", "l"])
        fmt = rnd.choice(LOG_FORMATS)
        cv = conversions(fmt)
        a1 = a2 = "0"
        if cv == ["s"]:
            kindc = "s"; a1 = rnd.choice(LOG_WORDS)
        elif cv == ["d"]:
            kindc = "d"; a1 = str(rnd.randrange(-100000, 100000))
        elif cv == ["s", "s"]:
            kindc = "S"; a1 = rnd.choice(LOG_WORDS); a2 = rnd.choice(LOG_WORDS)
        elif cv == ["d", "d"]:
            kindc = "D"; a1 = str(rnd.randrange(-999, 999)); a2 = str(rnd.randrange(-999, 999))
        elif cv == ["s", "d"]:
            kindc = "M"; a1 = rnd.choice(LOG_WORDS); a2 = str(rnd.randrange(-999, 999))
        else:
            kindc = "-"
        line = "log2file|%d|%d|%d|%s|%s|%s|%s|%s" % (
            itr, empty, ok, path, fmt, kindc, a1, a2)
        # guest side: build the same argument block on the guest stack
        va_args = []
        if kindc == "s":
            va_args = [("s", a1)]
        elif kindc == "d":
            va_args = [("i", int(a1))]
        elif kindc == "S":
            va_args = [("s", a1), ("s", a2)]
        elif kindc == "D":
            va_args = [("i", int(a1)), ("i", int(a2))]
        elif kindc == "M":
            va_args = [("s", a1), ("i", int(a2))]
        writes = [(G_ITRCHECK, u32(itr)),
                  (G_LOGFILENAME, b"\0" if empty else b"already/resolved.txt\0"),
                  (G_LAST_LOG, b"\0" * 256)]
        return (line, writes, ("LOG", fmt, va_args),
                {"fopen_ok": ok, "logpath": path},
                [(G_LAST_LOG, "last_log$str")])

    if kind == "take_screenshot":
        n = rnd.choice([0, 1, 9, 99, 9990, 9995, 9998, 9999,
                        rnd.randrange(0, 10000)])
        w = rnd.choice([640, 320, 1, rnd.randrange(0, 2000)])
        h = rnd.choice([480, 240, 1, rnd.randrange(0, 2000)])
        hits = rnd.choice([0, 0, 0, 1, 2, 5, 20])
        line = "take_screenshot|%d|%d|%d|%d" % (n, w, h, hits)
        writes = [(G_SCREENSHOT_N, u32(n)),
                  (S_DEST, u32(w) + u32(h)),
                  (G_ITRCHECK, u32(0)),
                  (G_LOGFILENAME, b"\0"),
                  (G_KEY + KEY_F1, b"\0")]
        return (line, writes, [S_DEST],
                {"exists_hits": hits, "fopen_ok": 0, "logpath": "logs/it.txt"},
                [(G_SCREENSHOT_N, "number__take_screenshot")])

    if kind == "draw_reward":
        flash = rnd.choice([0, 0, 1, 1, 2, 3, rnd.randrange(-3, 6)])
        scale = rnd.choice([0, 1, 0x10000, 0x8000, 0x20000, -0x10000,
                            rnd.randrange(-0x40000, 0x40000),
                            rnd.randrange(0, 0x100000)])
        w = rnd.choice([1, 64, 200, 640, rnd.randrange(0, 1000)])
        h = rnd.choice([1, 32, 100, 480, rnd.randrange(0, 1000)])
        line = "draw_reward|%d|%d|%d|%d" % (flash, scale, w, h)
        writes = [(G_OPT_FLASH, u32(flash)), (G_REWARD_SCALE, u32(scale)),
                  (G_REWARD_BMP, u32(S_REWARD)),
                  (S_REWARD, u32(w) + u32(h)),
                  (S_REWARD + 0x1c, u32(S_VTABLE)),
                  (S_DEST + 0x1c, u32(S_VTABLE)),
                  (S_VTABLE + 0xa4, u32(VT_PIVOT))]
        return line, writes, [S_DEST], {}, []
    raise RuntimeError(kind)


KINDS = ["play_sound", "startGameMusic", "stopGameMusic", "log2file",
         "take_screenshot", "draw_reward"]


def original_domain(orig, dom):
    out = []
    for va, name in dom:
        if name.endswith("$str"):
            out.append('%s "%s"' % (name[:-4], orig.gstr(va, 256)))
        else:
            out.append("%s %d" % (name, orig.r32(va)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vectors", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--only", default=None, help="one KIND only")
    ap.add_argument("--fault", action="store_true",
                    help="negative control: corrupt one ORIGINAL-side record "
                         "at vector 5 of each kind and require a DIFFER")
    args = ap.parse_args()

    kinds = [args.only] if args.only else KINDS
    per = max(1, args.vectors // len(kinds))
    if args.fault:
        per = 40

    rnd = random.Random(args.seed)
    orig = Original()

    lines, expected = [], []
    for kind in kinds:
        for k in range(per):
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
            blocks.append(cur); cur = None
        elif cur is not None and ln:
            cur.append(ln)
    if len(blocks) != len(expected):
        print("PROTOCOL: %d candidate blocks for %d vectors"
              % (len(blocks), len(expected)))
        sys.exit(1)

    fails = {k: 0 for k in kinds}
    shown = 0
    for i, (kind, k, writes, callargs, script, dom) in enumerate(expected):
        for va, b in writes:
            orig.mu.mem_write(va, b)
        if isinstance(callargs, tuple) and callargs and callargs[0] == "LOG":
            _, fmt, va_args = callargs
            # lay the format string and the varargs out on the guest stack
            sp = S + 0x1000
            fmt_va = sp
            orig.mu.mem_write(fmt_va, fmt.encode("latin1") + b"\0")
            sp += 0x200
            argvals = []
            for t, v in va_args:
                if t == "s":
                    orig.mu.mem_write(sp, v.encode("latin1") + b"\0")
                    argvals.append(sp); sp += 0x100
                else:
                    argvals.append(v)
            cargs = [fmt_va] + argvals
        else:
            cargs = list(callargs)
        trace = orig.run(FN[kind], cargs, script)
        got = trace + original_domain(orig, dom)
        cand = list(blocks[i])
        if args.fault and k == 5:
            # Corrupt the ORIGINAL side's first record.  When the vector
            # produces NO records at all -- draw_reward with options.flash
            # >= 2 draws nothing and writes nothing -- inject a spurious
            # one instead, so that "this function correctly did nothing"
            # is itself under test rather than trivially passing.
            if got:
                got[0] = got[0] + " FAULT"
            else:
                got = ["FAULT (spurious record injected into an empty trace)"]
        if got != cand:
            fails[kind] += 1
            if shown < 6:
                shown += 1
                print("  DIFFER %s vector %d" % (kind, k))
                print("    vector: %s" % lines[i])
                for a, b in zip(got + [""] * 40, cand + [""] * 40):
                    if a != b:
                        print("    original: %s" % a)
                        print("    candidate: %s" % b)
                        break
    bad = 0
    for kind in kinds:
        n = per
        print("%-16s %d of %d vectors differ" % (kind, fails[kind], n))
        if args.fault:
            if fails[kind] == 0:
                print("NEGATIVE CONTROL FAILED: %s fault not detected" % kind)
                bad = 1
        else:
            bad |= (1 if fails[kind] else 0)
    sys.exit(bad)


if __name__ == "__main__":
    main()
