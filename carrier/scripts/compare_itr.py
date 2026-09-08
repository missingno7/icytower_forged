#!/usr/bin/env python3
"""compare_itr.py -- byte-compare two .itr replay files the game itself wrote.

Why this exists (divergence 011): PROMOTIONS.md batch 14 promoted
`save_replay`, and the offline oracle it was verified with proves the ordered
call trace and the BYTES handed to each `pack_fwrite` -- it structurally
cannot prove that those bytes reach the disk. The in-vivo oracle for a file
writer is the FILE, so the run-to-the-game's-own-exit check compares the
`last_game.itr` an all-bound run saved against the one an unbound run saved.

One column has to be masked, and exactly one:

  tc_s_data[0..99]   the music-sync channel of play()'s anti-cheat telemetry
                     (`play.c` 3641, `demo->tc_s_data[n] = 50.0 * accMusics /
                     totMusics`).  `accMusics` accumulates
                     `voice_get_position(checkMusicVoiceID)` -- the real
                     playback cursor of a real DirectSound voice, i.e. the
                     HOST's audio clock, which no carrier wrapper pins.
                     MEASURED: two UNBOUND runs of replays/human_test.txt
                     over the same recording, back to back, differ in exactly
                     six bytes, all inside tc_s_data[0] and tc_s_data[1], and
                     in nothing else -- so this is pre-existing host
                     nondeterminism in the ORIGINAL machine code, not
                     anything a promotion introduced.
                     It is also, by construction, outside the integrity
                     check: `calc_replay_checksum` (0x41bac4) hashes
                     tc_c_data / tc_q_data / tc_t_data and NOT tc_s_data or
                     tc_f_data (PROMOTIONS.md batch 14 finding 3), so a
                     difference here cannot change the stored checksum -- and
                     the checksum field IS compared, unmasked, below.

Nothing else is masked.  In particular `date` (which carries the
"ICYTOWERISGREAT" watermark and IS hashed) and the three hashed statistics
columns are compared byte for byte, as is every gameplay field and every
RLE input record.

On-disk layout is save_replay's own WRITE ORDER, which is not the struct
order -- see src/icytower/replay.c's header and notes/replay_format.md.

Usage:  python compare_itr.py A.itr B.itr [--no-mask]
Exit 0 on EQUAL, 1 on DIFFER (same contract as compare_digests.py).
"""
import argparse
import struct
import sys

# save_replay()'s write order (src/icytower/replay.c 0x41de60..0x41e1ec).
HDR = 6 + 4 + 32 + 32          # header[6] size name[32] date[32]
OFF_SCORE = HDR                # 74
OFF_FLOOR = HDR + 4            # 78
FIXED = (HDR                   # ... through random_seed
         + 4 * 5               # score floor combo no_combo_top_floor biggest_lost_combo
         + 4 * 5 + 4 * 5       # ccc[5] jc[5]
         + 4 * 7)              # floor_shrink floor_size start_speed speed_increase
                               # gravity rejump random_seed
OFF_COMMENT = FIXED            # 162
OFF_CHECKSUM = OFF_COMMENT + 42        # 204 -- written OUT of struct order
OFF_TC_POSTS = OFF_CHECKSUM + 4        # 208
OFF_STATS = OFF_TC_POSTS + 4           # 212 -- 100 x {c q t s f}, interleaved
STAT_ROWS = 100
STAT_STRIDE = 20
STAT_S_IN_ROW = 12                     # c q t | s | f  -> the 4th column
OFF_RECORDS = OFF_STATS + STAT_ROWS * STAT_STRIDE   # 2212, then 5 bytes each


def masked_ranges():
    """[(start, end), ...] -- the tc_s_data column, one 4-byte run per row."""
    return [(OFF_STATS + i * STAT_STRIDE + STAT_S_IN_ROW,
             OFF_STATS + i * STAT_STRIDE + STAT_S_IN_ROW + 4)
            for i in range(STAT_ROWS)]


def field_name(off):
    if off < 6:
        return 'header[%d]' % off
    if off < 10:
        return 'size'
    if off < 42:
        return 'name[%d]' % (off - 10)
    if off < 74:
        return 'date[%d]' % (off - 42)
    for base, n in ((OFF_SCORE, 'score'), (OFF_SCORE + 4, 'floor'),
                    (OFF_SCORE + 8, 'combo'), (OFF_SCORE + 12, 'no_combo_top_floor'),
                    (OFF_SCORE + 16, 'biggest_lost_combo')):
        if base <= off < base + 4:
            return n
    if 94 <= off < 114:
        return 'ccc[%d]' % ((off - 94) // 4)
    if 114 <= off < 134:
        return 'jc[%d]' % ((off - 114) // 4)
    for i, n in enumerate(('floor_shrink', 'floor_size', 'start_speed',
                           'speed_increase', 'gravity', 'rejump', 'random_seed')):
        if 134 + 4 * i <= off < 138 + 4 * i:
            return n
    if OFF_COMMENT <= off < OFF_CHECKSUM:
        return 'comment[%d]' % (off - OFF_COMMENT)
    if OFF_CHECKSUM <= off < OFF_TC_POSTS:
        return 'checksum'
    if OFF_TC_POSTS <= off < OFF_STATS:
        return 'tc_posts'
    if OFF_STATS <= off < OFF_RECORDS:
        row, col = divmod(off - OFF_STATS, STAT_STRIDE)
        return 'tc_%s_data[%d]' % ('cqtsf'[col // 4], row)
    row, col = divmod(off - OFF_RECORDS, 5)
    return 'record[%d].%s' % (row, 'cycle_count' if col < 4 else 'key_flags')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--no-mask', action='store_true',
                    help='compare every byte, tc_s_data included (use to '
                         'reproduce the host-audio nondeterminism this '
                         'tool documents, not to certify a promotion)')
    args = ap.parse_args()

    a = open(args.a, 'rb').read()
    b = open(args.b, 'rb').read()
    if len(a) != len(b):
        print('DIFFER (size %d vs %d, %s vs %s)' % (len(a), len(b), args.a, args.b))
        return 1

    mask = set()
    if not args.no_mask:
        for lo, hi in masked_ranges():
            mask.update(range(lo, hi))

    diffs = [i for i in range(len(a)) if a[i] != b[i] and i not in mask]
    score, floor = struct.unpack_from('<II', a, OFF_SCORE)
    checksum = struct.unpack_from('<I', a, OFF_CHECKSUM)[0]
    if not diffs:
        print('EQUAL (%d bytes, score=%d floor=%d checksum=0x%08x, '
              'tc_s_data %smasked, %s vs %s)'
              % (len(a), score, floor, checksum,
                 '' if not args.no_mask else 'NOT ', args.a, args.b))
        return 0
    print('DIFFER (%d byte(s) outside the mask; first at offset %d = %s: '
          '%02x vs %02x) %s vs %s'
          % (len(diffs), diffs[0], field_name(diffs[0]),
             a[diffs[0]], b[diffs[0]], args.a, args.b))
    for off in diffs[:16]:
        print('  %6d  %-24s %02x vs %02x' % (off, field_name(off), a[off], b[off]))
    return 1


if __name__ == '__main__':
    sys.exit(main())
