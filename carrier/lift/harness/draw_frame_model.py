"""draw_frame_model.py -- a Python transliteration of
src/icytower/draw_frame.c, used as the candidate side of
`draw_frame_xcheck.py --model`.

This is the "unicorn cross-check comes first" step this project applies
to any novel algorithm before trusting a hand-trace (PROMOTIONS.md
batches 4, 6 and 9): it verifies the READING of the disassembly
independently of the C transcription.  The default campaign compares the
COMPILED C instead -- see draw_frame_check.c."""
import struct
from draw_frame_xcheck import (S_DEST, S_SWAP, S_REPLAY, bmp_addr, build_model,
                       model_makecol, model_sprintf, DATA_IDS)

LIT = {
    "%d": 0x4d4b83, "score: %d": 0x4d4b8f, "REPLAY": 0x4d4b99,
    "%s Floors": 0x4d4ba0, "%s Speed": 0x4d4baa, "": 0x4d4bb3,
    " - ": 0x4d4bb4, "%s%s%s": 0x4d4bb8, "FPS:%6d / %d": 0x4d4bbf,
    "REC:%6d / %d": 0x4d4bcc, "    %6d  (%d) ": 0x4d4bd9,
    "POS: %d, %d": 0x4d4be8, " dx: %1.2f": 0x4d4bf4, "rjp: %d": 0x4d4bff,
    "any: %6d %6d %6d": 0x4d4c07,
}
M_MYBUF = 0x00190000
M_SCROLLERTEXT = 0x00190100

A_BGTILE, A_FLOOR01, A_SIGN01, A_STAR01 = 1, 17, 101, 117
A_CLOCK, A_CLOCK_HAND, A_COMBO_COUNT = 12, 13, 14
A_COMBO_LIQUID, A_COMBO_METER = 15, 16
A_FONT_BIG_WHITE, A_FONT_MED_WHITE, A_FONT_MONO, A_FONT_SMALL = 50, 52, 53, 54
A_HURRYUP, A_SIDEBLOCK, A_VCR = 67, 100, 127
A_VCR_LEFT, A_VCR_RIGHT, A_VCR_UP = 128, 129, 130
A_FONT_ALLEGRO = 199


def trunc(x):
    return int(x) if x >= 0 else -int(-x)


def c_div(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def c_mod(a, b):
    return a - c_div(a, b) * b


def fixtoi(x):
    x = ((x + 0x80000000) & 0xFFFFFFFF) - 0x80000000
    if x >= 0:
        ff = x >> 16
    else:
        ff = ~((~x & 0xFFFFFFFF) >> 16)
        ff = ((ff + 0x80000000) & 0xFFFFFFFF) - 0x80000000
    return ff + ((x & 0x8000) >> 15)


def model_ftofix(m, x):
    if x > 32767.0:
        m.errno = 34
        return 0x7FFFFFFF
    if x < -32767.0:
        m.errno = 34
        return -0x7FFFFFFF
    return trunc(x * 65536.0 + (-0.5 if x < 0 else 0.5))


def run(w):
    m = build_model(w)
    for s, va in LIT.items():
        m.static_strings[va] = s
    m.static_strings[M_MYBUF] = ""
    m.static_strings[M_SCROLLERTEXT] = ""
    p = w.p
    BMP = S_DEST
    A = bmp_addr

    fo = 17 + w.start_floor * 3
    so = 101 + w.start_floor
    m.frame_count += 1
    level = p["level"]
    if level <= 200:
        max_bg_id = 2
    elif level <= 350:
        max_bg_id = 3
    elif level >= 601:
        max_bg_id = 5
    else:
        max_bg_id = 4

    # ---- draw_background ----
    while c_div(m.map_offset, 256) > m.last_stripe_y:
        m.last_stripe_y += 1
        m.bg[4] = m.bg[3]; m.bg[3] = m.bg[2]; m.bg[2] = m.bg[1]; m.bg[1] = m.bg[0]
        if c_mod(m.new_rand(), 100) > 40:
            m.bg[0] = 0
        else:
            m.bg[0] = c_mod(m.new_rand(), max_bg_id)
            if m.bg[0] == m.bg[1] or m.bg[0] == m.bg[2]:
                m.bg[0] = 0
    for i in range(5):
        tile = A(A_BGTILE + m.bg[i])
        tw, th = m.wh(tile)
        m.t("blit", tile, BMP, 0, 0, 37,
            (i - 1) * 128 + c_div(c_mod(m.map_offset, 256), 2), tw, th)

    # ---- draw_hurry_sign ----
    if -100 < w.hurry_y < 480 and w.opt_flash != 2:
        b = A(A_HURRYUP)
        m.draw_sprite(BMP, b, 320 - c_div(m.wh(b)[0], 2), w.hurry_y)

    # ---- draw_floors ----
    cy, cx = 31, -32
    while cx != 480:
        fl = w.rooms[cy]
        if not fl["empty"]:
            f = fo + fl["tiles"] * 3
            if f > 44:
                f = 44
            if fl["level"] > 4999:
                f += 3
            t = fl["start_tile"]
            m.draw_sprite(BMP, A(A_FLOOR01 + f - 17), t * 16 - 5,
                          cx + c_mod(m.map_offset, 16) - 6)
            t += 1
            while t < fl["end_tile"]:
                m.draw_sprite(BMP, A(A_FLOOR01 + f + 1 - 17), t * 16,
                              cx + c_mod(m.map_offset, 16) - 6)
                t += 1
            m.draw_sprite(BMP, A(A_FLOOR01 + f + 2 - 17), t * 16,
                          cx + c_mod(m.map_offset, 16) - 6)
            if w.debug and w.keyf2:
                m.t("textprintf_ex", BMP, A(A_FONT_ALLEGRO), 520,
                    cx + c_mod(m.map_offset, 16), 15, -1, LIT["%d"],
                    c_div(fl["level"] - 1, 5))
        if fl["sign"]:
            s = so + fl["tiles"]
            if s > 110:
                s = 110
            if fl["level"] > 4999:
                s += 1
            sy = cx + c_mod(m.map_offset, 16) + 10
            b = A(A_SIGN01 + s - 101)
            sw = m.wh(b)[0]
            x = (fl["start_tile"] + c_div(fl["end_tile"] - fl["start_tile"], 2)) * 16
            m.draw_sprite(BMP, b, x, sy)
            c1 = m.makecol(255, 255, 255)
            c2 = m.makecol(0x37, 0x37, 0x37)
            x += c_div(sw, 2)
            for dx, dy, col in ((1, 7, c2), (2, 6, c2), (0, 6, c2),
                                (1, 5, c2), (1, 6, c1)):
                m.t("textprintf_centre_ex", BMP, A(A_FONT_SMALL), x + dx,
                    sy + dy, col, -1, LIT["%d"], fl["sign"])
        cy -= 1
        cx += 16

    # ---- draw_stars ----
    for i in range(512):
        st = w.stars[i]
        if st["intensity"]:
            m.draw_sprite(S_SWAP, A(A_STAR01 + st["color"]),
                          fixtoi(st["x"]), fixtoi(st["y"]))

    # ---- draw_player ----
    _player(m, w, BMP)

    # ---- draw_side_rails ----
    y = -124
    while y != 496:
        m.draw_sprite(BMP, A(A_SIDEBLOCK), 565,
                      y + trunc(1.476 * c_mod(m.map_offset, 84)))
        m.draw_sprite_h_flip(BMP, A(A_SIDEBLOCK), -57,
                             y + trunc(1.476 * c_mod(m.map_offset, 84)))
        y += 124

    # ---- draw_combo_meter ----
    m.draw_sprite(BMP, A(A_COMBO_METER), 22, 100)
    if p["in_combo"]:
        m.t("blit", A(A_COMBO_LIQUID), BMP, 0, 100 - p["in_combo"], 33,
            219 - p["in_combo"], 16, p["in_combo"])
        m.draw_sprite(BMP, A(A_COMBO_COUNT), -8, 210)
        m.t("textprintf_centre_ex", BMP, A(A_FONT_BIG_WHITE), 42, 210, -1, -1,
            LIT["%d"], p["acc_level"])
    elif w.reward_time:
        m.draw_sprite(BMP, A(A_COMBO_COUNT), -8, 210)
        m.t("textprintf_centre_ex", BMP, A(A_FONT_BIG_WHITE), 42, 210, -1, -1,
            LIT["%d"], p["latest_combo"])

    # ---- draw_clock ----
    ox = oy = 0
    lc = w.logic_count
    if 250 < w.hurry_y < 480:
        x = c_mod(lc, 3) + 5
        y = c_mod(lc + 1, 3) + 9
        ox = c_mod(lc, 3) - 1
        oy = c_mod(lc + 1, 3) - 1
    else:
        x, y = 6, 10
    m.draw_sprite(BMP, A(A_CLOCK), x, y)
    if 200 < w.hurry_y < 480:
        ox = c_mod(lc + 2, 3) - 1
        oy = c_mod(lc + 3, 3) - 1
    if w.clock_angle:
        angle = model_ftofix(m, c_mod(w.clock_angle, 1500) * 0.1706666)
    else:
        angle = 0
    m.rotate_sprite(BMP, A(A_CLOCK_HAND), 34 + ox, 28 + oy, angle)

    # ---- draw_score ----
    if w.reward_time:
        m.t("draw_reward", S_SWAP)
    m.t("textprintf_ex", BMP, A(A_FONT_MED_WHITE), 8, 440, -1, -1,
        LIT["score: %d"], p["level"] * 10 + p["score"])

    # ---- draw_replay_hud ----
    if not w.recording:
        _replay_hud(m, w, BMP)

    # ---- draw_debug_overlay ----
    if w.debug and w.keyf2:
        f = A(A_FONT_ALLEGRO)
        m.t("textprintf_ex", BMP, f, 0, 0, 15, -1, LIT["FPS:%6d / %d"],
            w.fps, w.lps)
        m.t("textprintf_ex", BMP, f, 0, 10, 15, -1, LIT["REC:%6d / %d"],
            w.rec_pos, w.demo_size)
        m.t("textprintf_ex", BMP, f, 0, 20, 15, -1, LIT["    %6d  (%d) "],
            w.rec_key, w.rec_cycle)
        m.t("textprintf_ex", BMP, f, 200, 0, 15, -1, LIT["POS: %d, %d"],
            trunc(p["x"]), trunc(p["y"]))
        lo, hi = struct.unpack("<ii", struct.pack("<d", p["sx"]))
        m.t("textprintf_ex", BMP, f, 200, 10, 15, -1, LIT[" dx: %1.2f"],
            lo, hi)
        m.t("textprintf_ex", BMP, f, 200, 20, 15, -1, LIT["rjp: %d"],
            w.opt_jump_hold)
        m.t("textprintf_ex", BMP, f, 400, 0, 15, -1, LIT["any: %6d %6d %6d"],
            w.any[0], w.any[1], w.any[2])
        m.t("textprintf_ex", BMP, f, 400, 10, 15, -1, LIT["any: %6d %6d %6d"],
            w.any[3], w.any[4], w.any[5])

    dom = dict(frame_count=m.frame_count, last_stripe_y=m.last_stripe_y)
    dom["bg0"], dom["bg1"], dom["bg2"] = m.bg[0], m.bg[1], m.bg[2]
    dom["bg3"], dom["bg4"] = m.bg[3], m.bg[4]
    dom["p.frame"] = m.pframe
    dom["scroll_count"] = m.scroll_count
    dom["scroll_delay"] = m.scroll_delay
    dom["errno"] = m.errno
    return m.trace, dom, m


def _player(m, w, BMP):
    p = w.p
    cf = [bmp_addr(150 + i) for i in range(15)]
    lc = w.logic_count
    sx, sy = p["sx"], p["sy"]

    def draw(sprite, ox, oy):
        if not (sx > 0.0):
            m.draw_sprite_h_flip(BMP, sprite, trunc(p["x"]) + ox, trunc(p["y"]) + oy)
        else:
            m.draw_sprite(BMP, sprite, trunc(p["x"]) + ox, trunc(p["y"]) + oy)

    if p["status"] == 0:
        standing = (0.02 > sx) if (sx >= 0.0) else (sx > -0.02)
        if standing:
            m.pframe = 0
            oy = 1 - m.wh(cf[0])[1]
            if p["edge"]:
                fr = cf[13] if (lc & 8) else cf[14]
                if p["edge"] == 2:
                    m.draw_sprite_h_flip(BMP, fr,
                                         trunc(p["x"]) - m.wh(fr)[0] + 11,
                                         trunc(p["y"]) + oy)
                else:
                    m.draw_sprite(BMP, fr, trunc(p["x"]) - 11, trunc(p["y"]) + oy)
                return
            if m.map_offset > 200 and p["y"] > 400.0:
                fr = cf[11]
            elif lc <= 11:
                fr = cf[9]
            elif 24 < lc <= 36:
                fr = cf[10]
            else:
                fr = cf[0]
            draw(fr, -c_div(m.wh(fr)[0], 2), oy)
            return
        running = (not (sx > -0.2)) if (not (sx >= 0.0)) else (not (0.2 > sx))
        if running:
            if m.pframe > 3:
                m.pframe = 0
        else:
            m.pframe = 0
        p_im = 1
        oy = 1 - m.wh(cf[0])[1]
    else:
        if p["status"] == 1:
            p_im = 5 if (-3.0 > sy) else 6
        elif p["status"] in (2, 3):
            p_im = 7 if (sy > 3.0) else 6
        else:
            p_im = 6
        if ((sx > -0.01) if (not (sx >= 0.0)) else (0.01 > sx)):
            p_im = 8
        m.pframe = 0
        oy = 1 - m.wh(cf[0])[1]

    if p["rotate"]:
        fr = cf[12]
        m.rotate_sprite(BMP, fr, trunc(p["x"]) - c_div(m.wh(fr)[0], 2),
                        trunc(p["y"]) - 8 - m.wh(cf[0])[1], p["angle"])
        return
    fr = cf[p_im + m.pframe]
    draw(fr, -c_div(m.wh(fr)[0], 2), oy)


def _replay_hud(m, w, BMP):
    fnt = bmp_addr(A_FONT_MONO)
    if m.frame_count & 8:
        m.static_strings[M_MYBUF] = "REPLAY"
        myPos = 630 - m.text_length(fnt, M_MYBUF)
        c = m.makecol(0, 0, 0)
        m.t("textprintf_ex", BMP, fnt, myPos + 1, 5, c, -1, M_MYBUF)
        c = m.makecol(255, 255, 255)
        m.t("textprintf_ex", BMP, fnt, myPos, 4, c, -1, M_MYBUF)

    if w.custom_game:
        for txt, fmt, ya, yb in ((w.cap_floor, "%s Floors", 16, 15),
                                 (w.cap_speed, "%s Speed", 26, 25)):
            m.static_strings[M_MYBUF] = model_sprintf(fmt, [txt, "", ""])
            m.t("sprintf", M_MYBUF, LIT[fmt],
                w.cap_addr["floor"] if fmt == "%s Floors" else w.cap_addr["speed"])
            myPos = 630 - m.text_length(fnt, M_MYBUF)
            c = m.makecol(0, 0, 0)
            m.t("textout_ex", BMP, fnt, M_MYBUF, myPos + 1, ya, c, -1)
            c = m.makecol(255, 255, 255)
            m.t("textout_ex", BMP, fnt, M_MYBUF, myPos, yb, c, -1)
        m.static_strings[M_MYBUF] = w.cap_grav
        m.t("strcpy", M_MYBUF, w.cap_addr["grav"])
        myPos = 630 - m.text_length(fnt, M_MYBUF)
        c = m.makecol(0, 0, 0)
        m.t("textout_ex", BMP, fnt, M_MYBUF, myPos + 1, 36, c, -1)
        c = m.makecol(255, 255, 255)
        m.t("textout_ex", BMP, fnt, M_MYBUF, myPos, 35, c, -1)

    pos, size = w.rec_pos, w.demo_size
    vcr = bmp_addr(A_VCR)
    vw, vh = m.wh(vcr)
    x = 635 - vw
    y = 475 - vh
    m.draw_sprite(BMP, vcr, x, y)
    if not w.p["dead"]:
        if w.ctrl_flags & 0x01:
            m.draw_sprite(BMP, bmp_addr(A_VCR_LEFT), x + 97, y + 5)
        if w.ctrl_flags & 0x10:
            m.draw_sprite(BMP, bmp_addr(A_VCR_UP), x + 107, y + 5)
        if w.ctrl_flags & 0x02:
            m.draw_sprite(BMP, bmp_addr(A_VCR_RIGHT), x + 117, y + 5)
    y += 10
    x += 10
    m.t("set_clip_rect", BMP, x, 0, 623, 479)
    if w.demo_comment:
        m.static_strings[M_SCROLLERTEXT] = w.demo_name + " - " + w.demo_comment
        m.t("sprintf", M_SCROLLERTEXT, LIT["%s%s%s"], S_REPLAY + 0xc,
            LIT[" - "], S_REPLAY + 0xa8)
    else:
        m.static_strings[M_SCROLLERTEXT] = w.demo_name
        m.t("sprintf", M_SCROLLERTEXT, LIT["%s%s%s"], S_REPLAY + 0xc,
            LIT[""], LIT[""])

    c = m.makecol(150, 150, 160)
    m.t("textout_ex", BMP, fnt, S_REPLAY + 0xc,
        x + 2 - c_div(m.scroll_count, 2), y + 4, c, -1)
    c = m.makecol(200, 200, 210)
    m.t("textout_ex", BMP, fnt, S_REPLAY + 0xc,
        x + 3 - c_div(m.scroll_count, 2), y + 4, c, -1)
    if w.demo_comment:
        c = m.makecol(200, 200, 210)
        ln = m.text_length(fnt, S_REPLAY + 0xc)
        m.t("textout_ex", BMP, fnt, LIT[" - "],
            x + 2 - c_div(m.scroll_count, 2) + ln, y + 4, c, -1)
        c = m.makecol(200, 200, 210)
        ln = m.text_length(fnt, S_REPLAY + 0xc)
        m.t("textout_ex", BMP, fnt, S_REPLAY + 0xa8,
            x + 20 - c_div(m.scroll_count, 2) + ln, y + 4, c, -1)
    m.t("set_clip_rect", BMP, 0, 0, 639, 479)

    if w.demo_comment:
        if m.scroll_delay > 0:
            m.scroll_delay -= 1
        else:
            m.scroll_count += 1
            if c_div(m.scroll_count, 2) > m.text_length(fnt, M_SCROLLERTEXT):
                m.scroll_count = -250

    c = m.makecol(50, 200, 50)
    ln = c_div(pos * 117, size)
    if ln > 116:
        ln = 116
    m.rect(BMP, x, y + 20, x + ln, y + 19, c)
