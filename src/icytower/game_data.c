/* game_data.c -- getGameDataXML(), the whole body of what
 * `icytower15.exe -check file.itr` prints.
 *
 * Original source: F:\projects\icytower\trunk\source\game_data.c.
 *
 * PROMOTIONS.md batch 15.  This is the last of play()'s coastline that
 * is pure in the useful sense: it calls sprintf, strcat and malloc and
 * nothing else (a call census over artifacts/disasm.txt
 * 0x404254..0x404992 gives 14 x sprintf, 8 x strcat, 1 x malloc, plus
 * the frame's own ___chkstk), touches no bitmap, no file and no input.
 * Its domain is therefore the CONTENT OF THE RETURNED BUFFER, compared
 * byte for byte.
 *
 * notes/replay_format.md SS4 quotes `itrcheck.txt`'s description of this
 * output from the outside ("flags -jumps/-combos/-keys/-sd/-all/-tiny
 * dump an XML summary").  This is that summary's generator, and the five
 * flags are the five ints of `cmdline` (Tcommandline @0x4dd14c:
 * jumps +0, combos +4, sd +8, keys +0xc, tiny +0x10).
 *
 * -------------------------------------------------------------------
 * Findings
 * -------------------------------------------------------------------
 *
 * 1. `-tiny` IS AN EITHER/OR, NOT A FILTER.  0x4046e5 loads
 *    `cmdline.tiny` and branches: the false arm (0x4047aa) appends every
 *    section and then jumps STRAIGHT to the closing tag; the true arm
 *    (0x4046f3) runs the field-by-field comparison and emits the
 *    `<result>match|mismatch</result>` line.  Neither arm can reach the
 *    other's work.  So the verdict element exists ONLY in `-tiny` output,
 *    and the full output -- the one a human reads -- never says whether
 *    the replay verified.  That is surprising enough to be worth stating
 *    plainly; it is unambiguous in the object code.
 *
 * 2. THE ACTUAL-RESULTS ccc/jc ROWS ARE GATED ON THE CLAIMED VALUES.
 *    Both loops that build the `<actual_results>` block test
 *    `gd->replay->ccc[i]` / `->jc[i]` (0x4043f0, 0x40442a -- the REPLAY's
 *    array) and then print `gd->ccc[i]` / `gd->jc[i]` (0x4043f8,
 *    0x404434 -- the MEASURED one).  A level the player actually reached
 *    but the replay header does not claim is therefore invisible in the
 *    XML, while its mismatch still counts in finding 3's total.  Reading
 *    the two loops as "the same loop over the actual data" would be the
 *    natural mistake; the two different base registers are the evidence.
 *
 * 3. The verdict counts EVERY differing field, not just the first: five
 *    scalars plus five ccc plus five jc, fifteen comparisons, summed into
 *    one counter (0x4046f8..0x404740, a `setne` for the first and a
 *    branch-over `inc` for the other fourteen), and the result is
 *    "mismatch" if the counter is non-zero.
 *
 * 4. `<entry flr="...">` IS A TRUNCATING CAST, and the object code says
 *    so explicitly: 0x4045ff..0x40461f saves the x87 control word, forces
 *    RC=11 (toward zero) by `mov $0xc,%ah`, does `fistpl`, and restores.
 *    That is GCC's `(int)<float>`, not lrint and not a rounding print.
 *    The other four columns are pushed as `fstpl` doubles, i.e. the
 *    ordinary float->double promotion of a `%f` argument.
 *
 * 5. The `-sd` column order is NOT the struct order: clk/qpc/tme/dns map
 *    to tc_c_data / tc_q_data / tc_t_data / tc_s_data and flr to
 *    tc_f_data (0x4045f8 reads +0xd8, then +0x588, +0x3f8, +0x268, +0x718
 *    in that push order, which reverses to c,q,t,s,f).  Together with
 *    save_replay()'s interleaved writer this closes the last unnamed
 *    columns of the statistics tail: batch 14 found that
 *    calc_replay_checksum() hashes only c, q and t; this shows what all
 *    five are FOR.
 *
 * 6. Every append is `sprintf(buf, "%s...", buf, ...)` -- the buffer
 *    formatted onto itself, the same C99 7.19.6.6 violation save_profile()
 *    commits and the same decision applies: it is what the original does,
 *    it is what this file spells, and GCC's -Wrestrict warnings on those
 *    lines are the recovery working, not a defect introduced here.
 *
 * -------------------------------------------------------------------
 * Buffer sizes
 * -------------------------------------------------------------------
 * INFERRED from the frame layout (___chkstk 0x485c) and the distances
 * between the seven local buffers' base addresses: sd 5120 (-0x4818),
 * jumps 5120 (-0x3418), combos 5120 (-0x2018), actual 1024 (-0xc18),
 * claimed 1024 (-0x818), game 512 (-0x418), player 256 (-0x218).  The
 * gaps are exact; only the split of the last 280 bytes between `player`
 * and the frame's scalars is not, so `player` is written as 256.  None
 * of them is bounds-checked: 5000 combos at ~45 bytes each would need
 * 45 x the 5120 bytes `combos` has, so a full Tgame_data overruns this
 * frame.  Recovered, not repaired.
 */
/* ------------------------------------------------------------------ */
/* MEMBER-ACCESS COLLISION -- the class replay.c, handle_player_input.c */
/* and draw_frame.c already document.  `Treplay.rejump` (the replay's   */
/* own copy of the difficulty setting) shares its name with the         */
/* top-level global `int rejump` @0x4fdcd8, which the carrier's         */
/* generated bindings rewrite with a blunt textual #define -- turning   */
/* `r->rejump` into a syntax error in that world.  Dropping the binding */
/* for this translation unit is right on the merits and not just        */
/* expedient: getGameDataXML reports what a REPLAY claims, and must     */
/* never read the live difficulty global.  The real fix is the          */
/* context-sensitive rewrite gen_bindings.py's own                      */
/* MEMBER_ACCESS_COLLISIONS comment describes.                          */
/* ------------------------------------------------------------------ */
#ifdef rejump
#undef rejump
#endif

#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *getGameDataXML(Tgame_data *gd)
{
    char *xml;
    char player[256];
    char game[512];
    char claimed[1024];
    char actual[1024];
    char combos[5120];
    char jumps[5120];
    char keys[256];
    char sd[5120];
    Treplay *r;
    int i;
    int diff;

    xml = (char *)malloc(128000);
    r = gd->replay;

    sprintf(player, "  <player>\n    <name>%s</name>\n  </player>\n", r->name);

    sprintf(game,
            "  <game>\n    <comment>%s</comment>\n    <settings>\n"
            "      <floor_shrink>%d</floor_shrink>\n"
            "      <floor_size>%d</floor_size>\n"
            "      <speed_increase>%d</speed_increase>\n"
            "      <start_speed>%d</start_speed>\n"
            "      <gravity>%d</gravity>\n"
            "      <rejump>%d</rejump>\n"
            "    </settings>\n  </game>\n",
            r->comment, r->floor_shrink, r->floor_size, r->speed_increase,
            r->start_speed, r->gravity, r->rejump);

    /* --- what the replay CLAIMS ------------------------------------- */
    sprintf(claimed,
            "      <score>%d</score>\n      <floor>%d</floor>\n"
            "      <combo>%d</combo>\n"
            "      <no_combo_floor>%d</no_combo_floor>\n"
            "      <lost_combo>%d</lost_combo>\n",
            r->score, r->floor, r->combo, r->no_combo_top_floor,
            r->biggest_lost_combo);
    for (i = 1; i < 6; i++)
        if (r->ccc[i - 1] > 0)
            sprintf(claimed, "%s      <ccc level=\"%d\">%d</ccc>\n",
                    claimed, i, r->ccc[i - 1]);
    for (i = 1; i < 6; i++)
        if (r->jc[i - 1] > 0)
            sprintf(claimed, "%s      <js level=\"%d\">%d</js>\n",
                    claimed, i, r->jc[i - 1]);

    /* --- what replaying it actually PRODUCED ------------------------ */
    sprintf(actual,
            "      <score>%d</score>\n      <floor>%d</floor>\n"
            "      <combo>%d</combo>\n"
            "      <no_combo_floor>%d</no_combo_floor>\n"
            "      <lost_combo>%d</lost_combo>\n",
            gd->score, gd->floor, gd->combo, gd->no_combo_top_floor,
            gd->biggest_lost_combo);
    /* the guard is the REPLAY's array, the value is the game data's --
     * finding 2 above. */
    for (i = 1; i < 6; i++)
        if (r->ccc[i - 1] > 0)
            sprintf(actual, "%s      <ccc level=\"%d\">%d</ccc>\n",
                    actual, i, gd->ccc[i - 1]);
    for (i = 1; i < 6; i++)
        if (r->jc[i - 1] > 0)
            sprintf(actual, "%s      <js level=\"%d\">%d</js>\n",
                    actual, i, gd->jc[i - 1]);

    strcpy(combos, "    <combos>\n");
    for (i = 0; i < gd->comboPosts; i++)
        sprintf(combos, "%s      <combo start=\"%d\" end=\"%d\">%d</combo>\n",
                combos, gd->combos[i].start, gd->combos[i].end,
                gd->combos[i].length);
    strcat(combos, "    </combos>\n");

    strcpy(jumps, "    <jumps>\n");
    for (i = 0; i < gd->jumpPosts; i++)
        sprintf(jumps,
                "%s      <sequence dist=\"%d\" start=\"%d\">%d</sequence>\n",
                jumps, gd->jumps[i].dist, gd->jumps[i].start,
                gd->jumps[i].num);
    strcat(jumps, "    </jumps>\n");

    sprintf(keys, "  <keys>\n    <left>%d</left>\n    <right>%d</right>\n"
                  "    <jump>%d</jump>\n  </keys>\n",
            gd->left, gd->right, gd->jump);

    strcpy(sd, "  <sd>\n");
    for (i = 0; i < r->tc_posts; i++)
        sprintf(sd, "%s    <entry clk=\"%2.2f\" qpc=\"%2.2f\" tme=\"%2.2f\""
                    " dns=\"%2.2f\" flr=\"%d\" />\n",
                sd, r->tc_c_data[i], r->tc_q_data[i], r->tc_t_data[i],
                r->tc_s_data[i], (int)r->tc_f_data[i]);
    strcat(sd, "  </sd>\n");

    sprintf(xml, "<itrcheck_results file_status=\"ok\" header=\"%c%c%c%c%c%c\""
                 " date=\"%s\">\n",
            r->header[0], r->header[1], r->header[2], r->header[3],
            r->header[4], r->header[5], r->date);

    if (cmdline.tiny) {
        diff = 0;
        if (gd->score != r->score) diff++;
        if (gd->floor != r->floor) diff++;
        if (gd->combo != r->combo) diff++;
        if (gd->no_combo_top_floor != r->no_combo_top_floor) diff++;
        if (gd->biggest_lost_combo != r->biggest_lost_combo) diff++;
        for (i = 0; i < 5; i++) {
            if (gd->ccc[i] != r->ccc[i]) diff++;
            if (gd->jc[i] != r->jc[i]) diff++;
        }
        sprintf(xml, "%s  <result>%s</result>\n", xml,
                diff ? "mismatch" : "match");
    } else {
        strcat(xml, player);
        strcat(xml, game);
        strcat(xml, "  <results>\n");
        strcat(xml, "    <claimed_results>\n");
        strcat(xml, claimed);
        strcat(xml, "    </claimed_results>\n");
        strcat(xml, "    <actual_results>\n");
        strcat(xml, actual);
        strcat(xml, "    </actual_results>\n");
        if (cmdline.combos)
            strcat(xml, combos);
        if (cmdline.jumps)
            strcat(xml, jumps);
        strcat(xml, "  </results>\n");
        if (cmdline.keys)
            strcat(xml, keys);
        if (cmdline.sd)
            strcat(xml, sd);
    }

    strcat(xml, "</itrcheck_results>\n");
    return xml;
}
