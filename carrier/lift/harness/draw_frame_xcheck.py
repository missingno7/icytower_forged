#!/usr/bin/env python3
"""draw_frame_xcheck.py -- ordered-call-trace oracle for draw_frame
(0x40929c, 8518 bytes, main.c).  PROMOTIONS.md batch 10.

Why this is not a lift_check.py SPECS entry
-------------------------------------------
draw_frame's comparison domain is an ORDERED SEQUENCE of ~250 library
calls per invocation (blit / draw_sprite / draw_256_sprite /
draw_sprite_h_flip / pivot_scaled_sprite_flip / rect / makecol /
textout_ex / textprintf_ex / textprintf_centre_ex / text_length /
set_clip_rect / sprintf / strcpy / draw_reward / new_rand), plus eleven
memory-domain globals.  lift_check.py's call-trace mechanism
(PROMOTIONS.md batch 8 "mechanism B") records per callee only a COUNT plus
the arguments of its FIRST call -- with 15 draw_sprite() sites and 12
textprintf_ex() sites in one invocation, first-call-capture would compare
roughly 15% of what this function does.  So this file is an additive,
standalone oracle built on the SAME engine (port_forge/tools/
pf_win32_offline_oracle.py's build_guest and its unicorn FNINIT/FLDCW
convention), not a new SPECS row.

How it works
------------
1. `World` seeds one whole game state -- profile, map.room[32], Tplayer,
   Tparticle stars[512], every referenced BITMAP's w/h/colour depth, the
   Treplay and its strings, the three menu captions, and a scripted
   new_rand() sequence -- from one Python RNG seed.
2. `run_original()` maps the real image at 0x400000, writes that state
   into the guest's own globals, hooks every library callee VA AND the
   five GFX_VTABLE slots draw_frame reaches through Allegro AL_INLINEs
   (batch 9's synthetic-vtable-VA trick, generalised: the scratch
   GFX_VTABLE's +0x44/+0x48/+0x50/+0xa4/+0xbc slots carry otherwise
   unused guest VAs at 0x7c4000.., which the engine hooks like any named
   callee), and executes the ORIGINAL bytes -- appending one record per
   call, in order, with its arguments.
3. The candidate side is either the Python model in draw_frame_model.py
   (`--model`: the "unicorn cross-check comes first" step this project
   applies before writing C for a novel algorithm) or the COMPILED
   src/icytower/draw_frame.c driven by draw_frame_check.exe (default) --
   see draw_frame_check.c for that half and for its build line.
4. Pointer arguments are rendered as stable SYMBOLS (data[N] /
   custom.frame[i] / bmp / swap_screen / font / buf#k) and `const char *`
   arguments as their CONTENT AT CALL TIME, so the guest and host address
   spaces never have to agree.  Resolving a string at comparison time
   instead would be wrong: myBuf and scrollerText are each reused for
   several different strings inside one invocation.

Usage
-----
  python draw_frame_xcheck.py --random   --seed 20260908 --vectors 2000
  python draw_frame_xcheck.py --directed --seed 777
  python draw_frame_xcheck.py --model    --seed 1 --vectors 1000
"""
import itertools, os, random, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(PROJ, "port_forge", "tools"))
import pf_win32_offline_oracle as pf
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP

IMAGE = os.path.join(PROJ, "assets", "icytower15.exe")
DRAW_FRAME_VA = 0x40929c

# ---------------------------------------------------------------- globals
G = dict(
    profile=0x4dd27c, player_id=0x4fe518, ply=0x4ff128,
    frame_count=0x506978, logic_count=0x506958, fps=0x506948, lps=0x506968,
    map=0x4f8b18, map_offset=0x4f8e18, last_stripe_y=0x4fa308,
    bg_stripe_ids=0x4dd19c, seed=0x4ff108, hurry_y=0x4ff118,
    options=0x4fe528, data=0x4dd23c, debug=0x4dd160, key=0x506988,
    stars=0x4facc8, swap_screen=0x4dd194, custom=0x4fa738,
    reward_time=0x4fec68, clock_angle=0x4dd248, recording=0x4f8e28,
    is_playing_custom_game=0x4dd188, rec_pos=0x4fec58,
    scroll_count=0x4fec48, scroll_delay=0x4f8ae8, demo=0x4dd250,
    floor_size_selection=0x4fe7b8, scroll_speed_selection=0x4fec78,
    gravity_selection=0x4fac38, ctrl=0x5000c8, allegro_errno=0x4dda78,
    font=0x4ccc94,
    any11=0x4dd170, any12=0x4dd174, any13=0x4dd178,
    any21=0x4dd17c, any22=0x4dd180, any23=0x4dd184,
)

# scratch guest layout (inside the mapped 0x400000..0x800000 window, past
# every real section -- the last one ends at ~0x78b7a0)
S_BASE      = 0x7d0000
S_PROFILE   = S_BASE + 0x0000      # Tprofile, 0x550
S_PLAYER    = S_BASE + 0x0600      # Tplayer,  0xb8
S_DATAFILE  = S_BASE + 0x0700      # DATAFILE[160], 16 each  -> 0xa00
S_BITMAPS   = S_BASE + 0x1200      # 200 BITMAPs, 0x40 each  -> 0x3200
S_VT16      = S_BASE + 0x4500      # GFX_VTABLE (16bpp)
S_VT8       = S_BASE + 0x4700      # GFX_VTABLE (8bpp)
S_SWAP      = S_BASE + 0x4900      # BITMAP (swap_screen)
S_DEST      = S_BASE + 0x4980      # BITMAP (the bmp argument)
S_REPLAY    = S_BASE + 0x4a00      # Treplay, 0x8b0
S_RECORDS   = S_BASE + 0x5400      # Trecord[512]
S_STRINGS   = S_BASE + 0x6800      # caption strings (past Trecord[512])
S_ERRNO     = S_BASE + 0x7000
S_FONT      = S_BASE + 0x7100      # 5 fake FONTs are just the BITMAP slots

VT_SLOT_BASE = 0x7c4000            # synthetic "callee" VAs for vtable slots
VT_DRAW_SPRITE      = VT_SLOT_BASE + 0x00
VT_DRAW_256_SPRITE  = VT_SLOT_BASE + 0x10
VT_DRAW_SPRITE_HFLIP= VT_SLOT_BASE + 0x20
VT_PIVOT            = VT_SLOT_BASE + 0x30
VT_RECT             = VT_SLOT_BASE + 0x40

LIBS = {                            # va -> (name, argc)
    0x456264: ("blit", 8),
    0x450c98: ("makecol", 3),
    0x45a23c: ("textprintf_centre_ex", 10),
    0x45a2a8: ("textprintf_ex", 10),
    0x459f0c: ("textout_ex", 7),
    0x459f50: ("text_length", 2),
    0x44eb70: ("set_clip_rect", 5),
    0x4bad60: ("sprintf", 5),
    0x4bad48: ("strcpy", 2),
    0x4070fc: ("draw_reward", 1),
    0x406984: ("new_rand", 0),
}
VT = {
    VT_DRAW_SPRITE:       ("draw_sprite", 4),
    VT_DRAW_256_SPRITE:   ("draw_256_sprite", 4),
    VT_DRAW_SPRITE_HFLIP: ("draw_sprite_h_flip", 4),
    VT_PIVOT:             ("pivot_scaled_sprite_flip", 9),
    VT_RECT:              ("rect", 6),
}

# argument kinds per traced call: 'p' pointer, 's' string, 'i' int.
# 's' arguments are resolved to their CONTENT at record time (not at
# comparison time) -- the two stack buffers myBuf/scrollerText are reused
# for several different strings within one invocation, so a deferred read
# would compare the last content written, not the one that call saw.
KINDS = {
    "blit": "ppiiiiii", "makecol": "iii", "textout_ex": "ppsiiii",
    "text_length": "ps", "set_clip_rect": "piiii", "sprintf": "pssss",
    "strcpy": "ps", "draw_reward": "p", "new_rand": "",
    "draw_sprite": "ppii", "draw_256_sprite": "ppii",
    "draw_sprite_h_flip": "ppii",
    "pivot_scaled_sprite_flip": "ppiiiiiii", "rect": "piiiii",
    "textprintf_ex": "ppiiiis", "textprintf_centre_ex": "ppiiiis",
}


def resolve_args(name, args, getstr):
    k = KINDS[name]
    out = []
    for i, a in enumerate(args):
        if i < len(k) and k[i] == "s":
            out.append("s:" + getstr(a & 0xFFFFFFFF))
        else:
            out.append(a)
    return tuple(out)


# datafile indices this function can touch
DATA_IDS = list(range(1, 7)) + [12, 13, 14, 15, 16] + list(range(17, 50)) + \
           [50, 52, 53, 54, 67, 100] + list(range(101, 112)) + \
           list(range(117, 125)) + [127, 128, 129, 130]


def bmp_addr(n):
    return S_BITMAPS + 0x40 * n


class World(object):
    """The seeded guest state, shared by the unicorn side and the model."""

    def __init__(self, rnd, directed=None):
        self.mem = {}
        self.rnd = rnd
        self.trace = []
        self.rand_seq = [rnd.randrange(0, 65535) for _ in range(400)]
        self.rand_i = 0
        d = directed or {}

        # --- scalars ---------------------------------------------------
        self.start_floor = d.get("start_floor", rnd.choice([0, 0, 0, 1, 5, 9]))
        self.level = d.get("level", rnd.choice(
            [rnd.randrange(0, 250), rnd.randrange(0, 1200), rnd.randrange(0, 6000)]))
        self.map_offset = d.get("map_offset", rnd.randrange(-2000, 60000))
        self.last_stripe_y = d.get("last_stripe_y", self.map_offset // 256 - rnd.randrange(0, 4))
        self.bg_ids = [rnd.randrange(0, 5) for _ in range(5)]
        self.hurry_y = d.get("hurry_y", rnd.choice(
            [-500, -99, 0, 150, 201, 250, 251, 300, 479, 480, 700,
             rnd.randrange(-200, 600)]))
        self.opt_flash = d.get("opt_flash", rnd.choice([0, 1, 2]))
        self.opt_jump_hold = rnd.randrange(0, 2)
        self.logic_count = d.get("logic_count", rnd.choice(
            [0, 5, 11, 12, 24, 25, 36, 37, rnd.randrange(0, 500)]))
        self.frame_count = rnd.randrange(0, 10000)
        self.fps, self.lps = rnd.randrange(0, 100), rnd.randrange(0, 100)
        self.debug = d.get("debug", 1 if rnd.random() < 0.25 else 0)
        self.keyf2 = 1 if rnd.random() < 0.7 else 0
        self.reward_time = d.get("reward_time", rnd.choice([0, 0, 1, 40]))
        self.clock_angle = d.get("clock_angle", rnd.choice(
            [0, 1, 750, 1499, 1500, 40000, -300, rnd.randrange(-5000, 100000)]))
        self.recording = d.get("recording", 1 if rnd.random() < 0.3 else 0)
        self.custom_game = d.get("custom_game", 1 if rnd.random() < 0.4 else 0)
        self.rec_pos = rnd.randrange(0, 400)
        self.demo_size = rnd.randrange(1, 5000)
        self.scroll_count = rnd.choice([-250, -1, 0, 1, 60, rnd.randrange(-300, 3000)])
        self.scroll_delay = rnd.choice([0, -1, 1, 30])
        self.ctrl_flags = rnd.randrange(0, 128)
        self.any = [rnd.randrange(-5, 5) for _ in range(6)]
        self.seed = rnd.randrange(1, 60000)

        # --- map -------------------------------------------------------
        self.rooms = []
        for k in range(32):
            empty = 0 if rnd.random() < 0.75 else -1
            st = rnd.randrange(0, 30)
            self.rooms.append(dict(
                empty=empty,
                start_tile=st,
                end_tile=st + rnd.randrange(0, 8),
                level=rnd.choice([rnd.randrange(0, 5200), rnd.randrange(4990, 5010)]),
                sign=rnd.choice([0, 0, 0, rnd.randrange(1, 2000)]),
                tiles=rnd.choice([0, 1, 2, 3, 5, 9, 10, 20]),
            ))

        # --- player ----------------------------------------------------
        self.p = dict(
            x=rnd.uniform(80, 560), y=rnd.uniform(0, 500),
            sx=rnd.choice([0.0, 0.005, -0.005, 0.015, -0.015, 0.05, -0.05,
                           0.2, -0.2, 0.19, -0.19, 3.0, -3.0,
                           rnd.uniform(-6, 6)]),
            sy=rnd.choice([0.0, 3.0, -3.0, 3.001, -3.001, rnd.uniform(-9, 9)]),
            level=self.level, score=rnd.randrange(0, 100000),
            status=d.get("status", rnd.choice([0, 0, 1, 2, 3, 4])),
            frame=rnd.choice([0, 1, 2, 3, 4, 7]),
            in_combo=rnd.choice([0, 0, 5, 60, 100]),
            acc_level=rnd.randrange(0, 50),
            dead=1 if rnd.random() < 0.3 else 0,
            rotate=1 if rnd.random() < 0.25 else 0,
            angle=rnd.randrange(0, 1 << 24),
            edge=rnd.choice([0, 0, 0, 1, 2]),
            latest_combo=rnd.randrange(0, 40),
        )

        # --- stars -----------------------------------------------------
        self.stars = []
        for i in range(512):
            live = rnd.random() < 0.06
            self.stars.append(dict(
                intensity=rnd.randrange(1, 255) if live else 0,
                x=rnd.randrange(-(1 << 25), 1 << 25),
                y=rnd.randrange(-(1 << 25), 1 << 25),
                color=rnd.randrange(0, 8)))

        # --- bitmaps ---------------------------------------------------
        self.bw, self.bh, self.bdepth = {}, {}, {}
        for n in DATA_IDS:
            self.bw[n] = rnd.randrange(1, 200)
            self.bh[n] = rnd.randrange(1, 200)
            self.bdepth[n] = 8 if rnd.random() < 0.25 else 16
        self.cframe_w, self.cframe_h, self.cframe_depth = [], [], []
        for i in range(15):
            self.cframe_w.append(rnd.randrange(1, 80))
            self.cframe_h.append(rnd.randrange(1, 80))
            self.cframe_depth.append(8 if rnd.random() < 0.25 else 16)

        # --- replay strings --------------------------------------------
        self.demo_name = "run%d" % rnd.randrange(0, 9999)
        self.demo_comment = "" if rnd.random() < 0.4 else "cmt%d" % rnd.randrange(0, 999)
        self.cap_floor = "Cf%d" % rnd.randrange(0, 99)
        self.cap_speed = "Cs%d" % rnd.randrange(0, 99)
        self.cap_grav = "Cg%d" % rnd.randrange(0, 99)
        self.rec_key = rnd.randrange(0, 256)
        self.rec_cycle = rnd.randrange(0, 100000)


# ----------------------------------------------------------- guest writer

def seed_guest(mu, w):
    def wr(va, b):
        mu.mem_write(va, b)

    def wi(va, v):
        wr(va, struct.pack("<i", ((int(v) + 0x80000000) & 0xFFFFFFFF) - 0x80000000))

    def wu(va, v):
        wr(va, struct.pack("<I", int(v) & 0xFFFFFFFF))

    def wd(va, v):
        wr(va, struct.pack("<d", v))

    # vtables (only the slots draw_frame can reach)
    for base, depth in ((S_VT16, 16), (S_VT8, 8)):
        wr(base, b"\0" * 0x200)
        wi(base + 0x00, depth)
        wu(base + 0x44, VT_DRAW_SPRITE)
        wu(base + 0x48, VT_DRAW_256_SPRITE)
        wu(base + 0x50, VT_DRAW_SPRITE_HFLIP)
        wu(base + 0xa4, VT_PIVOT)
        wu(base + 0xbc, VT_RECT)

    def mkbmp(addr, bw, bh, depth):
        wr(addr, b"\0" * 0x40)
        wi(addr, bw); wi(addr + 4, bh)
        wu(addr + 0x1c, S_VT8 if depth == 8 else S_VT16)

    # destination bitmaps: always 16bpp so the DEST vtable is the 16bpp one
    mkbmp(S_DEST, 640, 480, 16)
    mkbmp(S_SWAP, 640, 480, 16)

    # datafile + its bitmaps
    wr(S_DATAFILE, b"\0" * (160 * 16))
    for n in DATA_IDS:
        a = bmp_addr(n)
        mkbmp(a, w.bw[n], w.bh[n], w.bdepth[n])
        wu(S_DATAFILE + 16 * n, a)
    wu(G["data"], S_DATAFILE)

    mkbmp(bmp_addr(199), 0, 0, 16)      # the Allegro `font` stand-in

    # custom.frame[15]
    wr(G["custom"], b"\0" * 0x4d0)
    for i in range(15):
        a = bmp_addr(150 + i)
        mkbmp(a, w.cframe_w[i], w.cframe_h[i], w.cframe_depth[i])
        wu(G["custom"] + 0x80 + 4 * i, a)

    # profile / player
    wr(S_PROFILE, b"\0" * 0x550)
    wi(S_PROFILE + 0x524, w.start_floor)
    wu(G["profile"], S_PROFILE)

    p = w.p
    wr(S_PLAYER, b"\0" * 0xb8)
    wd(S_PLAYER + 0x00, p["x"]); wd(S_PLAYER + 0x08, p["y"])
    wd(S_PLAYER + 0x10, p["sx"]); wd(S_PLAYER + 0x18, p["sy"])
    wi(S_PLAYER + 0x28, p["level"]); wi(S_PLAYER + 0x2c, p["score"])
    wi(S_PLAYER + 0x34, p["status"]); wi(S_PLAYER + 0x3c, p["frame"])
    wi(S_PLAYER + 0x40, p["in_combo"]); wi(S_PLAYER + 0x44, p["acc_level"])
    wi(S_PLAYER + 0x4c, p["dead"]); wi(S_PLAYER + 0x50, p["rotate"])
    wu(S_PLAYER + 0x54, p["angle"]); wi(S_PLAYER + 0x58, p["edge"])
    wi(S_PLAYER + 0x68, p["latest_combo"])
    wu(G["ply"], S_PLAYER)
    wi(G["player_id"], 0)

    # map
    for k in range(32):
        r = w.rooms[k]
        b = G["map"] + 24 * k
        wi(b + 0, r["empty"]); wi(b + 4, r["start_tile"])
        wi(b + 8, r["end_tile"]); wi(b + 12, r["level"])
        wi(b + 16, r["sign"]); wi(b + 20, r["tiles"])
    wi(G["map_offset"], w.map_offset)

    wi(G["last_stripe_y"], w.last_stripe_y)
    for i in range(5):
        wi(G["bg_stripe_ids"] + 4 * i, w.bg_ids[i])
    wi(G["hurry_y"], w.hurry_y)
    wr(G["options"], b"\0" * 0x230)
    wi(G["options"] + 0, w.opt_flash)
    wi(G["options"] + 8, w.opt_jump_hold)
    wi(G["logic_count"], w.logic_count)
    wi(G["frame_count"], w.frame_count)
    wi(G["fps"], w.fps); wi(G["lps"], w.lps)
    wi(G["debug"], w.debug)
    wr(G["key"] + 0x30, bytes([w.keyf2]))
    wi(G["reward_time"], w.reward_time)
    wi(G["clock_angle"], w.clock_angle)
    wi(G["recording"], w.recording)
    wi(G["is_playing_custom_game"], w.custom_game)
    wi(G["rec_pos"], w.rec_pos)
    wi(G["scroll_count"], w.scroll_count)
    wi(G["scroll_delay"], w.scroll_delay)
    wu(G["swap_screen"], S_SWAP)
    wu(G["font"], bmp_addr(199))
    wu(G["allegro_errno"], S_ERRNO)
    wi(S_ERRNO, 0)
    wi(G["seed"], w.seed)
    wr(G["ctrl"], b"\0" * 36)
    wr(G["ctrl"] + 0x20, bytes([w.ctrl_flags]))
    for i, nm in enumerate(["any11", "any12", "any13", "any21", "any22", "any23"]):
        wi(G[nm], w.any[i])

    # stars
    for i, s in enumerate(w.stars):
        b = G["stars"] + 24 * i
        wi(b + 0, s["intensity"]); wu(b + 4, s["x"]); wu(b + 8, s["y"])
        wi(b + 20, s["color"])

    # replay
    wr(S_REPLAY, b"\0" * 0x8b0)
    wi(S_REPLAY + 0x8, w.demo_size)
    wr(S_REPLAY + 0xc, w.demo_name.encode() + b"\0")
    wi(S_REPLAY + 0x8c, 0)                       # floor_shrink
    wi(S_REPLAY + 0x90, 0)                       # floor_size  -> caption[0]
    wi(S_REPLAY + 0x94, 1)                       # start_speed -> caption[1]
    wi(S_REPLAY + 0x9c, 2)                       # gravity     -> caption[2]
    wr(S_REPLAY + 0xa8, w.demo_comment.encode() + b"\0")
    wu(S_REPLAY + 0x8a8, S_RECORDS)
    wu(G["demo"], S_REPLAY)
    wr(S_RECORDS, b"\0" * (512 * 8))
    wr(S_RECORDS + 8 * (w.rec_pos % 512), bytes([w.rec_key]) + b"\0\0\0" +
       struct.pack("<i", w.rec_cycle))
    if w.rec_pos >= 512:
        # keep demo->data[rec_pos] inside the scratch array
        pass

    # menu selection captions
    strs = {}
    off = S_STRINGS
    for key, txt in (("floor", w.cap_floor), ("speed", w.cap_speed),
                     ("grav", w.cap_grav)):
        strs[key] = off
        wr(off, txt.encode() + b"\0")
        off += 64
    for base, idx, key in ((G["floor_size_selection"], 0, "floor"),
                           (G["scroll_speed_selection"], 1, "speed"),
                           (G["gravity_selection"], 2, "grav")):
        wr(base, b"\0" * (8 + 32 * 4))
        wu(base + 8 + 4 * idx, strs[key])
    w.cap_addr = strs
    w.sprintf_area = off
    return


# --------------------------------------------------------- unicorn runner

def vararg_dwords(fmt):
    """dwords the varargs of this format consume (a %f eats two)."""
    n, i = 0, 0
    while i < len(fmt) - 1:
        if fmt[i] == "%":
            j = i + 1
            while j < len(fmt) and fmt[j] not in "diouxXeEfgGcsp%":
                j += 1
            if j < len(fmt):
                if fmt[j] == "%":
                    pass
                elif fmt[j] in "eEfgG":
                    n += 2
                else:
                    n += 1
            i = j + 1
        else:
            i += 1
    return n


_GUEST_CACHE = {}


def build_guest_cached():
    if "g" not in _GUEST_CACHE:
        _GUEST_CACHE["g"] = pf.build_guest(IMAGE)
    return _GUEST_CACHE["g"]


def run_original(guest, w):
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(pf.GUEST_BASE, pf.GUEST_SIZE)
    mu.mem_write(pf.GUEST_BASE, guest)
    mu.mem_map(pf.STACK_BASE, pf.STACK_SIZE)
    mu.mem_write(pf.RET_MAGIC, b"\xF4")
    stub = bytes([0xDB, 0xE3, 0xD9, 0x2D]) + struct.pack("<I", pf.CW_SLOT)
    mu.mem_write(pf.CW_SLOT, struct.pack("<H", pf.CW_INIT))
    mu.mem_write(pf.CW_STUB, stub)

    seed_guest(mu, w)
    trace = []
    state = {"rand_i": 0, "sp_off": 0}

    def gstr(va):
        out = b""
        while len(out) < 256:
            c = bytes(mu.mem_read(va + len(out), 1))
            if c == b"\0":
                break
            out += c
        return out.decode("latin1")

    VARFMT = {"sprintf": 1, "textprintf_ex": 6, "textprintf_centre_ex": 6}

    def mk(name, argc, retfn=None, sideeffect=None):
        fmt_idx = VARFMT.get(name)

        def hook(uc, address, size, data):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]

            def rd(i):
                return struct.unpack("<i", bytes(uc.mem_read(esp + 4 + 4 * i, 4)))[0]
            n = argc
            if fmt_idx is not None:
                n = fmt_idx + 1 + vararg_dwords(gstr(rd(fmt_idx) & 0xFFFFFFFF))
            args = [rd(i) for i in range(n)]
            rv = 0
            if sideeffect is not None:
                sideeffect(uc, args, gstr)
            if retfn is not None:
                rv = retfn(uc, args, gstr)
            trace.append((name, resolve_args(name, args, gstr)))
            uc.reg_write(UC_X86_REG_EAX, rv & 0xFFFFFFFF)
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EIP, ret)
            uc.emu_stop()
        return hook

    def ret_makecol(uc, a, g):
        return model_makecol(a[0], a[1], a[2])

    def ret_textlen(uc, a, g):
        return model_text_length(g(a[1]))

    def ret_newrand(uc, a, g):
        v = w.rand_seq[state["rand_i"] % len(w.rand_seq)]
        state["rand_i"] += 1
        return v

    def se_sprintf(uc, a, g):
        fmt = g(a[1])
        out = model_sprintf(fmt, [g(x) for x in a[2:]])
        uc.mem_write(a[0], out.encode("latin1") + b"\0")

    def se_strcpy(uc, a, g):
        uc.mem_write(a[0], g(a[1]).encode("latin1") + b"\0")

    for va, (name, argc) in LIBS.items():
        rf = {"makecol": ret_makecol, "text_length": ret_textlen,
              "new_rand": ret_newrand}.get(name)
        sf = {"sprintf": se_sprintf, "strcpy": se_strcpy}.get(name)
        mu.hook_add(UC_HOOK_CODE, mk(name, argc, rf, sf), begin=va, end=va)
    for va, (name, argc) in VT.items():
        mu.hook_add(UC_HOOK_CODE, mk(name, argc), begin=va, end=va)

    # go
    mu.emu_start(pf.CW_STUB, pf.CW_STUB + len(stub))
    esp = pf.STACK_BASE + pf.STACK_SIZE - 0x2000
    esp -= 4; mu.mem_write(esp, struct.pack("<I", S_DEST))
    esp -= 4; mu.mem_write(esp, struct.pack("<I", pf.RET_MAGIC))
    mu.reg_write(UC_X86_REG_ESP, esp)
    pc = DRAW_FRAME_VA
    guard = 0
    while True:
        mu.emu_start(pc, pf.RET_MAGIC, count=20000000)
        eip = mu.reg_read(UC_X86_REG_EIP)
        if eip == pf.RET_MAGIC:
            break
        pc = eip
        guard += 1
        if guard > 8000:
            raise RuntimeError("call-hook resume runaway")

    dom = read_domain(mu)
    return trace, dom, gstr


DOMAIN = [("frame_count", 0x506978), ("last_stripe_y", 0x4fa308),
          ("bg0", 0x4dd19c), ("bg1", 0x4dd1a0), ("bg2", 0x4dd1a4),
          ("bg3", 0x4dd1a8), ("bg4", 0x4dd1ac),
          ("p.frame", S_PLAYER + 0x3c),
          ("scroll_count", 0x4fec48), ("scroll_delay", 0x4f8ae8),
          ("errno", S_ERRNO)]


def read_domain(mu):
    return {n: struct.unpack("<i", bytes(mu.mem_read(va, 4)))[0]
            for n, va in DOMAIN}


# ------------------------------------------------------------- the model

def model_makecol(r, g, b):
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff)


def model_text_length(s):
    return 7 * len(s) + 1


def model_sprintf(fmt, args):
    out, ai, i = "", 0, 0
    while i < len(fmt):
        if fmt[i] == "%" and i + 1 < len(fmt) and fmt[i + 1] == "s":
            out += args[ai]; ai += 1; i += 2
        else:
            out += fmt[i]; i += 1
    return out


def trunc(x):
    return int(x) if x >= 0 else -int(-x)


def c_div(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def c_mod(a, b):
    return a - c_div(a, b) * b


class Model(object):
    def __init__(self, w):
        self.w = w
        self.trace = []
        self.rand_i = 0
        self.map_offset = w.map_offset
        self.last_stripe_y = w.last_stripe_y
        self.bg = list(w.bg_ids)
        self.frame_count = w.frame_count
        self.pframe = w.p["frame"]
        self.scroll_count = w.scroll_count
        self.scroll_delay = w.scroll_delay
        self.errno = 0
        self.strings = {}

    # ---- primitives -------------------------------------------------
    def t(self, name, *args):
        sargs = [((int(a) + 0x80000000) & 0xFFFFFFFF) - 0x80000000 for a in args]
        self.trace.append((name, resolve_args(
            name, sargs, lambda u: self.static_strings.get(u, "?%x" % u))))

    def new_rand(self):
        self.t("new_rand")
        v = self.w.rand_seq[self.rand_i % len(self.w.rand_seq)]
        self.rand_i += 1
        return v

    def makecol(self, r, g, b):
        self.t("makecol", r, g, b)
        return model_makecol(r, g, b)

    def text_length(self, fnt, s_va):
        self.t("text_length", fnt, s_va)
        return model_text_length(self.static_strings.get(s_va, ""))

    def draw_sprite(self, dest, sprite, x, y):
        if self.depth_of(sprite) == 8:
            self.t("draw_256_sprite", dest, sprite, x, y)
        else:
            self.t("draw_sprite", dest, sprite, x, y)

    def draw_sprite_h_flip(self, dest, sprite, x, y):
        self.t("draw_sprite_h_flip", dest, sprite, x, y)

    def rotate_sprite(self, dest, sprite, x, y, angle):
        sw, sh = self.wh(sprite)
        self.t("pivot_scaled_sprite_flip", dest, sprite,
               (x << 16) + (sw << 15), (y << 16) + (sh << 15),
               sw << 15, sh << 15, angle, 0x10000, 0)

    def rect(self, dest, x1, y1, x2, y2, c):
        self.t("rect", dest, x1, y1, x2, y2, c)

    # ---- bitmap table ------------------------------------------------
    def wh(self, addr):
        return self.bmp[addr]

    def depth_of(self, addr):
        return self.depth[addr]


def build_model(w):
    m = Model(w)
    m.bmp, m.depth = {}, {}
    for n in DATA_IDS:
        m.bmp[bmp_addr(n)] = (w.bw[n], w.bh[n])
        m.depth[bmp_addr(n)] = w.bdepth[n]
    for i in range(15):
        m.bmp[bmp_addr(150 + i)] = (w.cframe_w[i], w.cframe_h[i])
        m.depth[bmp_addr(150 + i)] = w.cframe_depth[i]
    m.bmp[bmp_addr(199)] = (0, 0); m.depth[bmp_addr(199)] = 16
    m.bmp[S_DEST] = (640, 480); m.depth[S_DEST] = 16
    m.bmp[S_SWAP] = (640, 480); m.depth[S_SWAP] = 16
    m.static_strings = {
        S_REPLAY + 0xc: w.demo_name,
        S_REPLAY + 0xa8: w.demo_comment,
        w.cap_addr["floor"]: w.cap_floor,
        w.cap_addr["speed"]: w.cap_speed,
        w.cap_addr["grav"]: w.cap_grav,
    }
    return m


import os, random, struct, sys, json
import draw_frame_model as M

# argument kind tables: 'p' pointer (symbolised), 's' string (content), 'i' int
# (argument-kind table: see KINDS above)


def norm_trace(trace, getstr, symtab):
    out = []
    for name, args in trace:
        kinds = KINDS[name]
        row = [name]
        for i, a in enumerate(args):
            k = kinds[i] if i < len(kinds) else "i"
            u = (a & 0xFFFFFFFF) if isinstance(a, int) else 0
            if k == "s":
                row.append(a)
            elif k == "p":
                row.append(symtab(u))
            else:
                row.append(a)
        out.append(tuple(row))
    return out


def make_symtab():
    known = {}
    for n in DATA_IDS:
        known[bmp_addr(n)] = "data[%d]" % n
    for i in range(15):
        known[bmp_addr(150 + i)] = "custom.frame[%d]" % i
    known[bmp_addr(199)] = "font"
    known[S_DEST] = "bmp"
    known[S_SWAP] = "swap_screen"
    seq = {}

    def sym(u):
        if u in known:
            return known[u]
        if u not in seq:
            seq[u] = "buf#%d" % len(seq)
        return seq[u]
    return sym


def orig_getstr_factory(mu):
    def g(va):
        out = b""
        while len(out) < 256:
            c = bytes(mu.mem_read(va + len(out), 1))
            if c == b"\0":
                break
            out += c
        return out.decode("latin1")
    return g


def run_model_campaign(seed, n):
    guest = build_guest_cached()
    diffs = 0
    first = None
    for v in range(n):
        rnd = random.Random(seed * 1000003 + v)
        w = World(rnd)
        try:
            otrace, odom, ogetstr = run_original(guest, w)
        except Exception as e:
            print("vector %d: ORIGINAL raised %r" % (v, e))
            diffs += 1
            if first is None:
                first = (v, "exception", repr(e))
            continue
        mtrace, mdom, mm = M.run(w)
        mgetstr = lambda u: mm.static_strings.get(u, "?%x" % u)
        a = norm_trace(otrace, ogetstr, make_symtab())
        b = norm_trace(mtrace, mgetstr, make_symtab())
        if a != b:
            diffs += 1
            if first is None:
                k = 0
                while k < min(len(a), len(b)) and a[k] == b[k]:
                    k += 1
                first = (v, "trace@%d/%d,%d" % (k, len(a), len(b)),
                         a[max(0, k - 2):k + 3], b[max(0, k - 2):k + 3])
            continue
        if odom != mdom:
            diffs += 1
            if first is None:
                first = (v, "domain",
                         {k: (odom[k], mdom[k]) for k in odom if odom[k] != mdom[k]})
    print("vectors=%d  differ=%d" % (n, diffs))
    if first:
        print("FIRST DIFFERENCE:")
        for part in first:
            print("   ", part)
    return 1 if diffs else 0




import os, random, struct, subprocess, sys

EXE = os.path.join(HERE, "draw_frame_check.exe")
VEC = os.path.join(HERE, "draw_frame_vectors.bin")
TRC = os.path.join(HERE, "draw_frame_ctrace.txt")


def wi(f, v):
    f.write(struct.pack("<i", ((int(v) + 0x80000000) & 0xFFFFFFFF) - 0x80000000))


def wd(f, v):
    f.write(struct.pack("<d", v))


def ws(f, s):
    b = s.encode("latin1")
    wi(f, len(b))
    f.write(b)


def export(worlds, path):
    with open(path, "wb") as f:
        f.write(struct.pack("<I", 0x31564644))
        wi(f, len(worlds))
        for w in worlds:
            p = w.p
            for v in (w.start_floor, p["level"], w.map_offset, w.last_stripe_y):
                wi(f, v)
            for v in w.bg_ids:
                wi(f, v)
            for v in (w.hurry_y, w.opt_flash, w.opt_jump_hold, w.logic_count,
                      w.frame_count, w.fps, w.lps, w.debug, w.keyf2,
                      w.reward_time, w.clock_angle, w.recording, w.custom_game,
                      w.rec_pos, w.demo_size, w.scroll_count, w.scroll_delay,
                      w.ctrl_flags):
                wi(f, v)
            for v in w.any:
                wi(f, v)
            for r in w.rooms:
                for k in ("empty", "start_tile", "end_tile", "level", "sign", "tiles"):
                    wi(f, r[k])
            for k in ("x", "y", "sx", "sy"):
                wd(f, p[k])
            for k in ("score", "status", "frame", "in_combo", "acc_level",
                      "dead", "rotate", "angle", "edge", "latest_combo"):
                wi(f, p[k])
            for s in w.stars:
                wi(f, s["intensity"]); wi(f, s["x"]); wi(f, s["y"]); wi(f, s["color"])
            wi(f, len(DATA_IDS))
            for n in DATA_IDS:
                wi(f, n); wi(f, w.bw[n]); wi(f, w.bh[n]); wi(f, w.bdepth[n])
            for i in range(15):
                wi(f, w.cframe_w[i]); wi(f, w.cframe_h[i]); wi(f, w.cframe_depth[i])
            ws(f, w.demo_name); ws(f, w.demo_comment)
            ws(f, w.cap_floor); ws(f, w.cap_speed); ws(f, w.cap_grav)
            wi(f, w.rec_key); wi(f, w.rec_cycle)
            wi(f, len(w.rand_seq))
            for v in w.rand_seq:
                wi(f, v)


SKIP = {"sprintf", "strcpy"}


def render(trace, getstr, symtab):
    rows = norm_trace(trace, getstr, symtab)
    out = []
    for row in rows:
        if row[0] in SKIP:
            continue
        out.append("|".join(str(x) for x in row))
    return out


def run_random_campaign(seed, n):
    guest = build_guest_cached()
    worlds, orig = [], []
    for v in range(n):
        rnd = random.Random(seed * 1000003 + v)
        w = World(rnd)
        ot, od, gs = run_original(guest, w)
        worlds.append(w)
        orig.append((render(ot, gs, make_symtab()), od))
    export(worlds, VEC)
    subprocess.check_call([EXE, VEC, TRC])

    lines = open(TRC).read().splitlines()
    cur, cvec, cdom = None, [], []
    for ln in lines:
        if ln.startswith("#vec"):
            cur = []
            cvec.append(cur)
        elif ln.startswith("#dom|"):
            cdom.append([int(x) for x in ln.split("|")[1:]])
        elif ln:
            cur.append(ln)

    diffs, first = 0, None
    dkeys = ["frame_count", "last_stripe_y", "bg0", "bg1", "bg2", "bg3", "bg4",
             "p.frame", "scroll_count", "scroll_delay", "errno"]
    for v in range(n):
        a, od = orig[v]
        b = cvec[v]
        cd = dict(zip(dkeys, cdom[v]))
        if a != b:
            diffs += 1
            if first is None:
                k = 0
                while k < min(len(a), len(b)) and a[k] == b[k]:
                    k += 1
                first = (v, "trace@%d (orig %d lines, C %d lines)" % (k, len(a), len(b)),
                         a[max(0, k - 2):k + 3], b[max(0, k - 2):k + 3])
            continue
        if od != cd:
            diffs += 1
            if first is None:
                first = (v, "domain",
                         {k: (od[k], cd[k]) for k in od if od[k] != cd[k]})
    print("vectors=%d  differ=%d" % (n, diffs))
    if first:
        print("FIRST DIFFERENCE:")
        for part in first:
            print("   ", part)
    return 1 if diffs else 0




import itertools, os, random, subprocess, sys

SX = [0.0, 0.01, -0.01, 0.009999, -0.009999, 0.0100001, -0.0100001,
      0.02, -0.02, 0.0199999, -0.0199999, 0.0200001, -0.0200001,
      0.2, -0.2, 0.1999999, -0.1999999, 0.2000001, -0.2000001, 5.0, -5.0]
SY = [0.0, 3.0, -3.0, 3.0000001, -3.0000001, 2.9999999, -2.9999999]
LC = [0, 8, 11, 12, 24, 25, 36, 37]
HY = [-101, -100, -99, 199, 200, 201, 249, 250, 251, 478, 479, 480, 481]
CA = [0, 1, -1, 1499, 1500, 1501, 40000]
MO = [-1, 0, 199, 200, 201, 255, 256, 257, 4000]
PY_ = [0.0, 399.0, 400.0, 401.0, 500.0]


def build(seed):
    worlds = []
    n = 0
    for st in (0, 1, 2, 3, 4):
        for sx in SX:
            for sy in (3.0, -3.0):
                n += 1
                w = World(random.Random(seed + n))
                w.p["status"] = st
                w.p["sx"] = sx
                w.p["sy"] = sy
                w.p["rotate"] = 0
                w.p["edge"] = 0
                worlds.append(w)
    for st in (1, 2, 3):
        for sy in SY:
            n += 1
            w = World(random.Random(seed + n))
            w.p["status"] = st
            w.p["sy"] = sy
            w.p["rotate"] = 0
            worlds.append(w)
    for lc, mo, py in itertools.product(LC, MO, PY_):
        n += 1
        w = World(random.Random(seed + n))
        w.p["status"] = 0
        w.p["sx"] = 0.0
        w.p["edge"] = 0
        w.p["rotate"] = 0
        w.p["y"] = py
        w.logic_count = lc
        w.map_offset = mo
        w.last_stripe_y = mo // 256
        worlds.append(w)
    for hy, ca in itertools.product(HY, CA):
        n += 1
        w = World(random.Random(seed + n))
        w.hurry_y = hy
        w.clock_angle = ca
        w.opt_flash = 0
        worlds.append(w)
    for edge, lcb, rot in itertools.product((0, 1, 2), (0, 8), (0, 1)):
        for fr in (0, 3, 4, 7):
            n += 1
            w = World(random.Random(seed + n))
            w.p["status"] = 0
            w.p["sx"] = 0.5
            w.p["edge"] = edge
            w.p["rotate"] = rot
            w.p["frame"] = fr
            w.logic_count = lcb
            worlds.append(w)
    for tiles, lvl, sf in itertools.product((0, 5, 9, 10, 20), (100, 4999, 5000), (0, 1, 9)):
        n += 1
        w = World(random.Random(seed + n))
        w.start_floor = sf
        for r in w.rooms:
            r["tiles"] = tiles
            r["level"] = lvl
            r["empty"] = 0
            r["sign"] = 42
        worlds.append(w)
    for rec, cg, dbg, cmt in itertools.product((0, 1), (0, 1), (0, 1), (0, 1)):
        for sc in (-250, 0, 1, 2000):
            n += 1
            w = World(random.Random(seed + n))
            w.recording = rec
            w.custom_game = cg
            w.debug = dbg
            w.keyf2 = 1
            w.demo_comment = "" if not cmt else "a comment"
            w.scroll_count = sc
            w.scroll_delay = 0 if n % 2 else 5
            worlds.append(w)
    return worlds


def run_directed_campaign(seed):
    worlds = build(seed)
    guest = build_guest_cached()
    orig = []
    for w in worlds:
        ot, od, gs = run_original(guest, w)
        orig.append((render(ot, gs, make_symtab()), od))
    export(worlds, VEC)
    subprocess.check_call([EXE, VEC, TRC])
    lines = open(TRC).read().splitlines()
    cur, cvec, cdom = None, [], []
    for ln in lines:
        if ln.startswith("#vec"):
            cur = []
            cvec.append(cur)
        elif ln.startswith("#dom|"):
            cdom.append([int(x) for x in ln.split("|")[1:]])
        elif ln:
            cur.append(ln)
    dkeys = ["frame_count", "last_stripe_y", "bg0", "bg1", "bg2", "bg3", "bg4",
             "p.frame", "scroll_count", "scroll_delay", "errno"]
    diffs, first = 0, None
    for v in range(len(worlds)):
        a, od = orig[v]
        b = cvec[v]
        cd = dict(zip(dkeys, cdom[v]))
        if a != b or od != cd:
            diffs += 1
            if first is None:
                k = 0
                while k < min(len(a), len(b)) and a[k] == b[k]:
                    k += 1
                first = (v, a[max(0, k - 1):k + 2], b[max(0, k - 1):k + 2],
                         {kk: (od[kk], cd[kk]) for kk in od if od[kk] != cd[kk]})
    print("directed vectors=%d  differ=%d" % (len(worlds), diffs))
    if first:
        print("FIRST DIFFERENCE:", first)
    return 1 if diffs else 0




# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--random", action="store_true",
                    help="random campaign: ORIGINAL vs compiled draw_frame.c")
    ap.add_argument("--directed", action="store_true",
                    help="directed boundary campaign: ORIGINAL vs compiled")
    ap.add_argument("--model", action="store_true",
                    help="ORIGINAL vs the Python model (draw_frame_model.py)")
    ap.add_argument("--seed", type=int, default=20260908)
    ap.add_argument("--vectors", type=int, default=2000)
    a = ap.parse_args()
    if a.model:
        return run_model_campaign(a.seed, a.vectors)
    if a.directed:
        return run_directed_campaign(a.seed)
    return run_random_campaign(a.seed, a.vectors)


if __name__ == "__main__":
    sys.exit(main())
