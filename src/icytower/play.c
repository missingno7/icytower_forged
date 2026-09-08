/* play.c -- one game.  The outer tick loop, the game-over/results screen,
 * the initials entry, the replay save and the profile/high-score commit.
 *
 * Recovered from artifacts/disasm.txt 0x411a00..0x415e0b (17420 bytes,
 * F:\projects\icytower\trunk\source\main.c per DWARF, source lines
 * 3405..5021, plus 971..975 from an inlined helper).  Prototype from
 * src/icytower/game_funcs.h: `int play(void)`.
 *
 * ============================================================ STRUCTURE
 *
 * PROMOTIONS.md batch 11 decoded .debug_line for this function
 * (artifacts/play_line_map.txt: 790 rows, 696 distinct source lines) and
 * concluded that `play()` must be recovered as ONE file bound at ONE
 * address, because its two halves share ~47 ebp-relative locals and
 * binding is all-or-nothing per address.  That is what this file is.
 *
 * The regions the line table shows, and where each one lives here:
 *
 *   3405-3539  setup / first-frame draw          play() prologue
 *   3540-3680  tick preamble, music, telemetry   tick_music_sample(),
 *                                                anticheat_post_sample()
 *   3681-3800  simulation core                   play() body
 *   3801-3830  collision dispatch (5 variants)   play() body
 *   3831-4035  score/floor/combo/death           play() body
 *   4036-4116  screenshot key + update_frame     play() body
 *   4117-4185  pause block A (ESC)               pause_screen_esc()
 *   4186-4248  pause block B (pause key)         pause_screen_pause()
 *   4249-4318  replay transport keys             play() body
 *   4319-4370  frame skip + draw + rest()        play() body
 *   4371-4460  post-game key-sequence scan       collect_game_data()
 *   4461-4630  replay saving (.itr)              save_personal_bests()
 *   4631-4990  game over / high score / rank     play() body
 *   4991-5021  teardown / return                 play() epilogue
 *
 * Only the genuinely self-contained regions became `static` helpers; the
 * rest stays in `play()` because it reads and writes the shared frame.
 * The helpers are `static`, so the compiler is free to inline them
 * straight back into one body -- the same readability-only decomposition
 * batch 10 used for `draw_frame`.
 *
 * ============================================ syncProfileFromOptions()
 *
 * artifacts/play_line_map.txt calls its three-site inlined helper "the
 * inlined clear() helper (main.c 972-975)".  It is really
 * `syncProfileFromOptions` (DWARF <0x1c973>, main.c decl_line 971,
 * DW_AT_inline = 1, so the abstract DIE carries no DW_AT_low_pc) -- and
 * an out-of-line copy DOES exist, at 0x406a14, whose fourteen
 * instructions are exactly the four assignments GCC inlined at
 * 0x4139f3 / 0x413e23 / 0x414494.  So it is called here by name, like
 * every other still-ORIGINAL callee, and src/icytower/game_funcs.h's
 * prototype for it resolves.  The line map's name for the helper is the
 * one correction this file makes to it.
 *
 * ================================================== ASSETS (ASSETS.md)
 *
 * This function holds the last TWO of the census's seven computed
 * `data[N]` index sites (src/icytower/ASSETS.md, "What remains genuinely
 * open"), and reading it resolves both:
 *
 *   0x4146e4 / 0x4149e6   data[gameover_bmp_id]  -- the results-screen
 *       logo.  `gameover_bmp_id` is not a range at all: it takes exactly
 *       two literal values, 55 and 62, assigned at 0x413f3c/0x413f4f and
 *       0x414fe3/0x414ffa.  55 = GAMEOVER (ASSET_DATA_GAMEOVER),
 *       62 = HIGHSCORE (ASSET_DATA_HIGHSCORE).  It is a local only
 *       because one `draw_results()` call site serves both screens.
 *       Recovered here as the two ids themselves, so no arithmetic on an
 *       asset_id happens at all.
 *
 * A third computed site lives in the same region and is a real range:
 *   0x414a4a / 0x414fb3   data[74 + new_rank_id] -- the rank medal.
 *       74 = RANK_00; RANK_00..RANK_11 are data 74..85, twelve objects
 *       generated contiguously by assets_table.inc from the manifest's own
 *       consecutive RANK_00..RANK_11 names, and `get_rank_id()` returns an
 *       index into the 12-entry rankLables[]/rankFloors[] tables, so
 *       `ASSET_DATA_RANK_00 + new_rank_id` never leaves that one family --
 *       the same argument start_reward.c and draw_frame.c already use.
 *
 * All other datafile references here are literal: FONT_BIG_WHITE (50),
 * FONT_MED_WHITE (52), FONT_SMALL (54), HEROFACE000 (58), TITLE_BG (126).
 *
 * ================================================== WHAT IS STILL ORIGINAL
 *
 * Five game functions this file calls are still ORIGINAL, each for the
 * blocker PROMOTIONS.md batch 11 recorded: `play_sound` (0x406da4, the
 * callee eight SPECS entries trace through CALLTRACE_PLAY_SOUND_VA),
 * `log2file` (0x40da58, writes a FILE* behind a mutex), `take_screenshot`
 * (0x41002c, writes a PNG), `startGameMusic`/`stopGameMusic` (0x40cb30 /
 * 0x40caf4, need three new Allegro call-trace slots).  They are called by
 * name; game_funcs.h declares them and the carrier's generated bindings
 * resolve each name to the original's address.  Many more (draw_results,
 * save_replay, load_replay, save_profile, get_rank_id, enter_hisc_table,
 * do_replay_menu, ...) are ORIGINAL too -- this is the game's top-level
 * driver, so almost everything it reaches is a whole subsystem.
 *
 * ==================================================== VERIFICATION
 *
 * See PROMOTIONS.md batch 12 and artifacts/src_equivalence.json.
 * Short version: the tick body's ordered call trace is checked offline
 * over one tick and over the outer-loop control paths
 * (carrier/lift/harness/play_xcheck.py + play_check.c); the whole
 * function's authority is the in-vivo per-tick digest + frame oracle over
 * replays/human_test.txt, because the game-over half is a UI loop driven
 * by readkey() and by wall-clock time that no offline domain can express.
 * The results/high-score/rank/initials regions are IN-VIVO-PENDING.
 */
#include <stdio.h>              /* sprintf */
#include <string.h>             /* strcpy, strlen */
#include <stdlib.h>             /* free */
#include <time.h>               /* time, clock, clock_t */

/* ------------------------------------------------------------------ */
/* Carrier-world MEMBER_ACCESS_COLLISIONS, worked around HERE rather    */
/* than in the generator (which another agent owns) -- the same         */
/* disposition draw_frame.c and handle_player_input.c already record.   */
/*                                                                      */
/*   Treplay.data    (the recorded input stream: `demo->data[i]`)       */
/*                   vs the global `DATAFILE *data` @0x4dd23c           */
/*   Treplay.rejump  (the replay's saved jump-hold setting)             */
/*                   vs the global `int rejump` @0x4fdcd8               */
/*                                                                      */
/* Both globals are rewritten by carrier/gen/pf_bindings_src.h as blunt */
/* textual #defines, which turns any `->data` / `->rejump` into a       */
/* syntax error.  This translation unit legitimately wants NEITHER      */
/* global: datafile objects are reached through ASSETS.md's             */
/* asset_bitmap()/asset_font() seam, and the jump-hold setting is read  */
/* from `options.jump_hold`, which is where play() actually reads it.   */
/* The real fix is still gen_bindings.py's own documented               */
/* context-sensitive rewrite; reported, not attempted here.             */
/* ------------------------------------------------------------------ */
#ifdef data
#undef data
#endif
#ifdef rejump
#undef rejump
#endif

#include "allegro_api.h"
#include "assets.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

/* ------------------------------------------------------------------ */
/* Allegro AL_INLINE primitives this build's headers do not supply --   */
/* the same generator gap batches 8/9/10/11 reported for rectfill /     */
/* putpixel / line / draw_sprite / acquire_screen.  Bodies transcribed  */
/* from allegro-4.4.3.1/include/allegro/inline/draw.inl (hline:47,      */
/* vline:57, acquire_screen/release_screen from gfx.h).  Guarded per    */
/* name: real <allegro.h> and the carrier's pf_lib_bindings.h always    */
/* win where they define one, so the source under test is byte-         */
/* identical in every world.                                            */
/* ------------------------------------------------------------------ */
#ifndef ICYTOWER_UPSTREAM_ALLEGRO

#ifndef hline
#define hline(b, x1, y, x2, c) ((b)->vtable->hline((b), (x1), (y), (x2), (c)))
#endif

#ifndef vline
#define vline(b, x, y1, y2, c) ((b)->vtable->vline((b), (x), (y1), (y2), (c)))
#endif

#ifndef draw_sprite
static void it_al_draw_sprite(BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    if (sprite->vtable->color_depth == 8)
        bmp->vtable->draw_256_sprite(bmp, sprite, x, y);
    else
        bmp->vtable->draw_sprite(bmp, sprite, x, y);
}
#define draw_sprite(b, s, x, y) it_al_draw_sprite((b), (s), (x), (y))
#endif

#ifndef rectfill
#define rectfill(b, x1, y1, x2, y2, c)     ((b)->vtable->rectfill((b), (x1), (y1), (x2), (y2), (c)))
#endif

/* Allegro spells the current display size as a pair of macros that
 * tolerate a NULL driver; the original's own NULL-guarded loads at
 * 0x415547/0x41574f are exactly this expansion. */
#ifndef SCREEN_W
#define SCREEN_W (gfx_driver ? gfx_driver->w : 0)
#define SCREEN_H (gfx_driver ? gfx_driver->h : 0)
#endif

#ifndef acquire_screen
#define acquire_screen()  \
    do { if (screen->vtable->acquire) screen->vtable->acquire(screen); } while (0)
#endif

#ifndef release_screen
#define release_screen()  \
    do { if (screen->vtable->release) screen->vtable->release(screen); } while (0)
#endif

#endif /* !ICYTOWER_UPSTREAM_ALLEGRO */

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* Win32 / CRT entry points the generated headers do not cover (they
 * carry the game and Allegro scopes only).  The original reaches all
 * four through the import table (artifacts/imports.json:
 * KERNEL32.QueryPerformanceCounter/Frequency, MSVCRT.mkdir/stricmp), so
 * they are declared here the way any application declares an OS entry
 * point it links against.  `LARGE_INTEGER` is spelled with its named
 * `u` member, which is valid both against real <windows.h> (where the
 * union carries BOTH an anonymous struct and an identical one named `u`)
 * and against the stand-in below -- the anonymous-struct spelling is not
 * portable to C89.                                                     */
#ifndef _WINDOWS_
typedef union it_large_integer {
    struct { unsigned int LowPart; int HighPart; } u;
    double _align;
} LARGE_INTEGER;
int __stdcall QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
int __stdcall QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
#endif
#ifndef mkdir
int mkdir(const char *path);
#endif
#ifndef stricmp
int stricmp(const char *a, const char *b);
#endif

/* main.c:3473-3480 -- the anti-cheat telemetry channels a fresh
 * recording starts from.  100 posts x 5 channels, all zeroed. */
static void clear_replay_telemetry(void)
{
    int i;

    demo->tc_posts = 0;                         /* 3474 */
    for (i = 0; i < 100; i++) {                 /* 3475 */
        demo->tc_c_data[i] = 0;                 /* 3476 */
        demo->tc_q_data[i] = 0;                 /* 3477 */
        demo->tc_t_data[i] = 0;                 /* 3478 */
        demo->tc_s_data[i] = 0;                 /* 3479 */
        demo->tc_f_data[i] = 0;                 /* 3480 */
    }
}

/* The three wall clocks the time-cheat detector samples are re-based
 * whenever the game was legitimately not running: after a screenshot,
 * after either pause screen, and after each telemetry post.  Emitted
 * verbatim at 0x412c6b, 0x413064, 0x41360d and 0x41441f. */
static void restart_time_cheat_window(clock_t *clockTimeStart, int *qpc_start,
                                      int *timeTimeStart, int *time_cheat_count)
{
    LARGE_INTEGER li;

    *clockTimeStart = clock();
    QueryPerformanceCounter(&li);
    *qpc_start = li.u.LowPart;
    *timeTimeStart = time(NULL);
    *time_cheat_count = 0;
}

/* The music-position channel is re-based the same way, but only when a
 * check-music voice is actually playing.  0x412c0d, 0x413006, 0x4135af. */
static void resync_music_counter(int *musicCounter, float *accMusics, int *totMusics)
{
    if (checkMusicVoiceID >= 0) {
        *musicCounter = (int)(50.0f * voice_get_position(checkMusicVoiceID) / 44000.0f);
        *totMusics = 0;
        *accMusics = 0;
    }
}

/* Both pause screens dim the back buffer the same way first: every other
 * scanline blacked out, then every other column.  Note the destination:
 * `bmp` is the back buffer, so the curtain is part of the frame that is
 * then blitted, not something drawn straight to the screen.
 * 0x412d58-0x412db5 and 0x413364-0x4133c1. */
static void draw_pause_curtain(BITMAP *bmp)
{
    int i;

    for (i = 0; i < 640; i += 2) {              /* 4122 / 4192 */
        vline(bmp, i, 0, 480, 0);               /* 4123 / 4193 */
        hline(bmp, 0, i, 640, 0);               /* 4124 / 4194 */
    }
}

/* main.c:4389-4418 -- what a finished game hands to the game-data
 * record: the final scores, and a per-control press census walked out of
 * the recorded input stream itself (so it counts what was PLAYED, not
 * what the keyboard driver saw).  0x4137ab-0x4138ee. */
static void collect_game_data(void)
{
    Tplayer *p = ply[player_id];
    Tgame_data *gd = gameData;
    /* the seven control bits, in the order the census reports them:
     * jump, left, right, up, down, enter, and the 0x80 end-of-input
     * terminator handle_player_input writes (notes/replay_format.md). */
    int key_flag[7];
    int keys_pressed[7];
    int last_keys[7];
    int i, j, k;

    gd->score = p->level * 10 + p->score;            /* 4389 */
    gd->floor = p->level;                            /* 4390 */
    gd->combo = p->best_combo;                       /* 4391 */
    gd->no_combo_top_floor = p->no_combo_top_floor;  /* 4392 */
    gd->biggest_lost_combo = p->biggest_lost_combo;  /* 4393 */

    for (i = 0; i < 5; i++)                          /* 4395 */
        gd->ccc[i] = p->ccc[i];
    for (i = 0; i < 5; i++)                          /* 4398 */
        gd->jc[i] = p->jcTop[i];

    for (i = 0; i < 7; i++)                          /* 4402 */
        keys_pressed[i] = 0;
    key_flag[0] = 0x10;                              /* 4403 */
    key_flag[1] = 0x01;
    key_flag[2] = 0x02;
    key_flag[3] = 0x04;
    key_flag[4] = 0x08;
    key_flag[5] = 0x20;
    key_flag[6] = 0x80;
    for (i = 0; i < 7; i++)                          /* 4404 */
        last_keys[i] = 0;

    for (i = 0; i < demo->size; i++) {               /* 4406 */
        k = demo->data[i].key_flags;
        for (j = 0; j < 7; j++) {                    /* 4408 */
            if (!last_keys[j] && (k & key_flag[j]))  /* 4409 */
                keys_pressed[j]++;                   /* 4410 */
            last_keys[j] = k & key_flag[j];          /* 4412 */
        }
    }

    gd->jump  = keys_pressed[0];                     /* 4416 */
    gd->left  = keys_pressed[1];                     /* 4417 */
    gd->right = keys_pressed[2];                     /* 4418 */
}

/* main.c:4500-4624 -- the replay-saving region.  Copies the final result
 * into the Treplay header, folds the game into the running profile
 * totals, then writes up to eleven .itr files: one per personal best
 * this game beat, plus `last_game.itr`, whose checksum becomes
 * `uberChecksum`.  0x413a74-0x413e22, with the five fixed best-record
 * arms living out of line at 0x41529e / 0x41532f / 0x4153c6 / 0x415200 /
 * 0x415987.
 *
 * `profile->best_replay_names[]` is indexed by fixed slot: 0 score,
 * 1 combo, 2 floor, 3 lost-combo, 4 no-combo, 5..9 the five CCC tiers,
 * 10..14 the five jump-sequence tiers -- the same fifteen slots
 * `new_personal_best[15]` flags. */
static void save_personal_bests(int quit, int numComboJumps, int totComboFloors)
{
    Tplayer *p = ply[player_id];
    char fbuf[2048];
    Treplay *rr;
    int i;

    if (quit)                                        /* 4500 */
        return;

    demo->score = p->level * 10 + p->score;           /* 4503 */
    demo->floor = p->level;                           /* 4504 */
    demo->combo = p->best_combo;                      /* 4505 */
    demo->rejump = options.jump_hold;                 /* 4506 */
    demo->no_combo_top_floor = p->no_combo_top_floor; /* 4507 */
    demo->biggest_lost_combo = p->biggest_lost_combo; /* 4508 */
    for (i = 0; i < 5; i++)                           /* 4510 */
        demo->ccc[i] = p->ccc[i];
    for (i = 0; i < 5; i++)                           /* 4513 */
        demo->jc[i] = p->jcTop[i];

    if (!is_playing_custom_game) {                    /* 4519 */
        profile->games_played++;                      /* 4520 */
        profile->total_floors += demo->floor;         /* 4522 */
        profile->total_score += demo->score;          /* 4523 */
        profile->total_combos += numComboJumps;       /* 4524 */
        profile->total_combo_floors += totComboFloors;/* 4525 */
        for (i = 0; i < 5; i++) {                     /* 4526 */
            if (p->ccc[i] > 0) {                      /* 4527 */
                profile->cccNum[i]++;                 /* 4528 */
                profile->cccTotal[i] += p->ccc[i];    /* 4529 */
            }
        }
    } else {
        profile->custom_games_played++;               /* 4534 */
    }

    if (!file_exists(replay_directory, -1, NULL))     /* 4541 */
        mkdir(replay_directory);                      /* 4543 */

    if (!is_playing_custom_game) {                    /* 4550 */
        if (profile->best_floor < demo->floor) {      /* 4551 */
            profile->best_floor = demo->floor;        /* 4552 */
            myDeleteFile(replay_directory, profile->best_replay_names[2]);  /* 4553 */
            sprintf(profile->best_replay_names[2], "%s_best_floor_%d.itr",  /* 4554 */
                    profile->handle, demo->floor);
            save_replay(replay_directory, profile->best_replay_names[2],    /* 4555 */
                        demo, rec_pos + 2, TRUE);
            new_personal_best[2] = TRUE;              /* 4556 */
        }
        if (profile->best_combo < demo->combo) {      /* 4559 */
            profile->best_combo = demo->combo;        /* 4560 */
            myDeleteFile(replay_directory, profile->best_replay_names[1]);  /* 4561 */
            sprintf(profile->best_replay_names[1], "%s_best_combo_%d.itr",  /* 4562 */
                    profile->handle, demo->combo);
            save_replay(replay_directory, profile->best_replay_names[1],    /* 4563 */
                        demo, rec_pos + 2, TRUE);
            new_personal_best[1] = TRUE;              /* 4564 */
        }
        if (profile->best_score < demo->score) {      /* 4567 */
            profile->best_score = demo->score;        /* 4568 */
            myDeleteFile(replay_directory, profile->best_replay_names[0]);  /* 4569 */
            sprintf(profile->best_replay_names[0], "%s_best_score_%d.itr",  /* 4570 */
                    profile->handle, demo->score);
            save_replay(replay_directory, profile->best_replay_names[0],    /* 4571 */
                        demo, rec_pos + 2, TRUE);
            new_personal_best[0] = TRUE;              /* 4572 */
        }
        if (profile->no_combo_top_floor < p->no_combo_top_floor) {          /* 4575 */
            profile->no_combo_top_floor = p->no_combo_top_floor;            /* 4576 */
            myDeleteFile(replay_directory, profile->best_replay_names[4]);  /* 4577 */
            sprintf(profile->best_replay_names[4], "%s_best_no_combo_%d.itr", /* 4578 */
                    profile->handle, demo->no_combo_top_floor);
            save_replay(replay_directory, profile->best_replay_names[4],    /* 4579 */
                        demo, rec_pos + 2, TRUE);
            new_personal_best[4] = TRUE;              /* 4580 */
        }
        if (profile->biggest_lost_combo < p->biggest_lost_combo) {          /* 4583 */
            profile->biggest_lost_combo = p->biggest_lost_combo;            /* 4584 */
            myDeleteFile(replay_directory, profile->best_replay_names[3]);  /* 4585 */
            sprintf(profile->best_replay_names[3], "%s_best_lost_combo_%d.itr", /* 4586 */
                    profile->handle, demo->biggest_lost_combo);
            save_replay(replay_directory, profile->best_replay_names[3],    /* 4587 */
                        demo, rec_pos + 2, TRUE);
            new_personal_best[3] = TRUE;              /* 4588 */
        }
        for (i = 1; i < 6; i++) {                                           /* 4591 */
            if (profile->ccc[i - 1] < p->ccc[i - 1]) {                      /* 4592 */
                profile->ccc[i - 1] = p->ccc[i - 1];                        /* 4593 */
                myDeleteFile(replay_directory, profile->best_replay_names[4 + i]); /* 4594 */
                sprintf(profile->best_replay_names[4 + i], "%s_best_cc%d_%d.itr", /* 4595 */
                        profile->handle, i, p->ccc[i - 1]);
                save_replay(replay_directory, profile->best_replay_names[4 + i],   /* 4596 */
                            demo, rec_pos + 2, TRUE);
                new_personal_best[4 + i] = TRUE;                            /* 4597 */
            }
        }
        for (i = 1; i < 6; i++) {                                           /* 4601 */
            if (profile->jc[i - 1] < p->jcTop[i - 1]) {                     /* 4602 */
                profile->jc[i - 1] = p->jcTop[i - 1];                       /* 4603 */
                myDeleteFile(replay_directory, profile->best_replay_names[9 + i]); /* 4604 */
                sprintf(profile->best_replay_names[9 + i], "%s_best_jj%d_%d.itr",  /* 4605 */
                        profile->handle, i, p->jcTop[i - 1]);
                save_replay(replay_directory, profile->best_replay_names[9 + i],   /* 4606 */
                            demo, rec_pos + 2, TRUE);
                new_personal_best[9 + i] = TRUE;                            /* 4607 */
            }
        }
    }

    if (save_replay(replay_directory, "last_game.itr",                      /* 4613 */
                    demo, rec_pos + 2, TRUE) < 0) {
        my_alert("Failed to save replay.", "(last_game.itr)", 0, TRUE);     /* 4614 */
        uberChecksum = 0;                                                   /* 4615 */
    } else {
        sprintf(fbuf, "%slast_game.itr", replay_directory);                 /* 4620 */
        rr = load_replay(fbuf);                                             /* 4621 */
        if (rr) {                                                           /* 4622 */
            uberChecksum = calc_replay_checksum(demo);                      /* 4623 */
            destroy_replay(rr);                                             /* 4624 */
        }
    }
}

/* ================================================================== */
/* main.c:3405-5021 -- play()                                          */
/* ================================================================== */
int play(void)
{
    /* --- 3408-3468: the frame the two halves share (DWARF gives 47 of
     * these an ebp-relative slot; several slots are reused between the
     * tick half and the game-over half, which is exactly why play() has
     * to be recovered as one function). --------------------------- */
    int playing = TRUE;                 /* 3408 */
    int old_map_pos;                    /* 3409 */
    int level;                          /* 3410 */
    int diff;                           /* 3411 */
    int i;                              /* 3412 */
    int quit;                           /* 3413 */
    int scroll_acc;                     /* 3414 */
    int scroll;                         /* 3415 */
    int speeds[9];                      /* 3417 */
    int next_speed;                     /* 3418 */
    int next_aight;                     /* 3419 */
    int allow_smpl;                     /* 3420 */
    int game_over;                      /* 3421 */
    int falling;                        /* 3422 */
    int shake;                          /* 3423 */
    int step_count;                     /* 3425 */
    int next_floor;                     /* 3426 */
    int play_again = FALSE;             /* 3427 */
    int lastX, lastY;                   /* 3429 */
    int numComboJumps;                  /* 3430 */
    int totComboFloors;                 /* 3431 */
    int startTime;                      /* 3432 */
    int endTime;                        /* 3433 */
    int lastJumpLength;                 /* 3434 */
    int oldUnlockedFloors;              /* 3436 */
    int current_rank_id;                /* 3437 */
    Tcontrol rec_ctrl;                  /* 3439 */

    /* The anti-cheat window.  Three independent wall clocks plus the
     * music position; every 1000 logic ticks their disagreement is
     * posted into the replay (see "the time-cheat post" below). */
    int time_cheat_count;               /* 3449 */
    clock_t clockTimeStart, clockTimeEnd;   /* 3451 */
    double clockElapsed;                /* 3452 */
    double totClockTimes = 0;           /* 3453 -- never accumulated */
    int qpc_start, qpc_end;             /* 3456 */
    double qpc_elapsed;                 /* 3458 */
    double totQPCTimes = 0;             /* 3459 -- never accumulated */
    int timeTimeStart, timeTimeEnd;     /* 3461 */
    int timeElapsed;                    /* 3462 */
    double totTimeTimes = 0;            /* 3463 -- never accumulated */
    int musicCounter;                   /* 3465 */
    int lastMusicPos;                   /* 3466 */
    float accMusics;                    /* 3467 */
    int totMusics;                      /* 3468 */

    LARGE_INTEGER li;                   /* 3518 */
    int qpc_freq;                       /* 3523 */
    int vgp;                            /* 3575 */
    float a, b;                         /* 3580, 3581 */
    double clockSpeed, qpcSpeed, timeSpeed;  /* 3601 */
    int rewResult;                      /* 3842 */
    Tgd_combo c;                        /* 3847 */
    int p;                              /* 4030 */
    int pauseTime, fc, ca, addTime;     /* 4063/4066, 4117-4119, 4159 */
    int ffstep;                         /* 4321 */

    /* the game-over half's own locals (4644-4790) */
    int gotHigh;                        /* 4645 (DWARF: gotHigh) */
    int qualify[15];                    /* 4648 */
    int qualifyValue[15];               /* 4649 */
    int gameover_bmp_id;                /* 4670 */
    int done;                           /* 4684 */
    int alpha_pos;                      /* 4685 */
    int pos;                            /* 4686 */
    char letters[31];                   /* 4687 */
    int len;                            /* 4688 */
    char buf[8];                        /* 4689 */
    int skip_keys;                      /* 4690 */
    int isGuest;                        /* 4692 */
    float hy;                           /* 4644 */
    int new_rank_id;                    /* 4787 */
    int rank_bmp_id;                    /* 4788 */
    int rank_y;                         /* 4789 */
    int scrollerY;                      /* 4784 */
    int k, j;                           /* 4407 / 4866 */
    char guestName[4];                  /* 4941 */
    char postName[32];                  /* 4942 */

    /* 3417 -- the fall_count thresholds at which the scroll speed steps
     * up.  The last two are effectively "never". */
    speeds[0] = 1500;   speeds[1] = 3000;   speeds[2] = 4500;
    speeds[3] = 6000;   speeds[4] = 7500;   speeds[5] = 9000;
    speeds[6] = 10500;  speeds[7] = 1800000; speeds[8] = 9000000;

    rec_ctrl = ctrl;                                        /* 3439 */

    if (itrcheck) {                                         /* 3441 */
        oldUnlockedFloors = 0;
        current_rank_id = 0;
    } else {
        oldUnlockedFloors = profile->best_floor / 100;      /* 3442 */
        current_rank_id = get_rank_id(profile);             /* 3443 */
    }

    if (recording)                                          /* 3473 */
        clear_replay_telemetry();                           /* 3474-3480 */

    log2file(" setting up play data");                      /* 3485 */
    fall_count = 0;                                         /* 3486 */
    clock_angle = 0;                                        /* 3487 */
    map.offset = 0;                                         /* 3488 */
    fast_forward = 0;                                       /* 3490 */
    fast_fast_forward = 0;                                  /* 3491 */
    update_frame();                                         /* 3493 */

    if (!itrcheck) {                                        /* 3494 */
        draw_frame(swap_screen);                            /* 3495 */
        fadeIn(swap_screen, 16);                            /* 3497 */
        play_sound(custom.bg_music, 0, 0);                  /* 3498 */
        startGameMusic();                                   /* 3499 */
    }
    if (!itrcheck) {                                        /* 3502 */
        if (bg_beat)                                        /* 3503 */
            checkMusicVoiceID = play_sample(bg_beat, 128, 1000, 1, TRUE); /* 3504 */
    }

    cycle_count = 0;                                        /* 3510 */
    log2file(" play started");                              /* 3512 */
    startTime = time(NULL);                                 /* 3513 */

    QueryPerformanceCounter(&li);                           /* 3519 */
    qpc_start = li.u.LowPart;                               /* 3520 */
    QueryPerformanceFrequency(&li);                         /* 3522 */
    qpc_freq = li.u.LowPart;                                /* 3523 */
    clockTimeStart = clock();                               /* 3528 */
    timeTimeStart = time(NULL);                             /* 3530 */

    totMusics = 0;
    accMusics = 0;
    lastMusicPos = 0;
    musicCounter = 0;
    time_cheat_count = 0;
    lastJumpLength = 0;
    endTime = 0;
    totComboFloors = 0;
    numComboJumps = 0;
    next_floor = -1;
    step_count = 0;
    shake = 0;
    falling = 0;
    game_over = 0;
    allow_smpl = TRUE;
    next_aight = 50;
    next_speed = 0;
    scroll = -1;
    quit = FALSE;

    /* ============================================================== */
    /* THE TICK -- main.c:3540..4370, 8073 bytes, 46% of play().       */
    /* Loop head 0x411c30, bottom test 0x41250a, back edge 0x41251a.   */
    /* ============================================================== */
    while (playing                                          /* 3534 */
           && !closeButtonClicked) {                        /* 3536 */

        cycle_count = 0;                                    /* 3540 */
        logic_count++;                                      /* 3542 */
        step_count++;                                       /* 3543 */
        fall_count++;                                       /* 3544 */
        time_cheat_count++;                                 /* 3545 */
        musicCounter++;                                     /* 3546 */

        /* ---- music start/stop on focus change, and the music-position
         * channel of the time-cheat telemetry (3549-3585) ---------- */
        if (!itrcheck) {                                    /* 3549 */
            if (hasFocus != lastFocus) {                    /* 3550 */
                if (hasFocus) {                             /* 3551 */
                    if (bg_beat)                            /* 3552 */
                        checkMusicVoiceID =                 /* 3553 */
                            play_sample(bg_beat, 128, 1000, 1, TRUE);
                    startGameMusic();                       /* 3559 */
                    totMusics = 0;
                    accMusics = 0;
                    musicCounter = 0;
                } else {
                    if (checkMusicVoiceID >= 0)             /* 3562 */
                        voice_stop(checkMusicVoiceID);      /* 3563 */
                    checkMusicVoiceID = -1;                 /* 3565 */
                    stopGameMusic();                        /* 3567 */
                }
                lastFocus = hasFocus;                       /* 3569 */
            }
            if (!itrcheck && checkMusicVoiceID >= 0) {      /* 3574 */
                vgp = voice_get_position(checkMusicVoiceID);/* 3575 */
                if (vgp < lastMusicPos)                     /* 3576 */
                    musicCounter = 0;
                a = vgp / 44000.0f;                         /* 3580 */
                if (a > 0.01) {                             /* 3583 */
                    /* `b` is a real float local: the x87 store that
                     * rounds this quotient to single precision before
                     * the divide is what the original does. */
                    b = musicCounter / 50.0f;               /* 3584 */
                    accMusics += a / b;
                    totMusics++;                            /* 3585 */
                }
                lastMusicPos = vgp;
            }
        }

        /* ---- the time-cheat post (3595-3661).  Every 1000 logic ticks
         * of an unbroken recording, the three wall clocks and the music
         * position are compared and one row is appended to the replay.
         *
         * NOTE on `clockSpeed`: read straight off 0x4142c4-0x4144e2 the
         * true arm is `1000.0 * clockElapsed / clockElapsed / 20.0`,
         * which is algebraically the constant 50.0 -- the same value the
         * other three channels carry when the game runs at its nominal
         * 50 ticks/second.  GCC could not fold it (it may not assume
         * x/x == 1 for floating point), so the multiply-then-divide pair
         * really is in the object code; the expression below reproduces
         * it exactly.  So the `c` channel of the anti-cheat telemetry
         * never varies, which is a property of the original, not of this
         * recovery. ------------------------------------------------ */
        if (recording && map.offset > 100                   /* 3595 */
            && !ply[player_id]->dead) {
            if (time_cheat_count == 1000) {                 /* 3597 */
                clockTimeEnd = clock();                     /* 3604 */
                clockElapsed = (clockTimeEnd - clockTimeStart) * 50.0 / 1000.0; /* 3605 */
                if (clockElapsed > 0)                       /* 3606 */
                    clockSpeed = 1000.0 * clockElapsed / clockElapsed / 20.0;
                else
                    clockSpeed = -0.05;

                QueryPerformanceFrequency(&li);             /* 3610 */
                qpc_freq = li.u.LowPart;                    /* 3611 */
                QueryPerformanceCounter(&li);               /* 3612 */
                qpc_end = li.u.LowPart;
                qpc_elapsed = (qpc_end - qpc_start) * 50.0 / qpc_freq;  /* 3615 */
                qpcSpeed = qpc_elapsed / 20.0;

                timeTimeEnd = time(NULL);                   /* 3623 */
                timeElapsed = timeTimeEnd - timeTimeStart;
                timeSpeed = 50.0 * timeElapsed / 20.0;

                demo->tc_c_data[demo->tc_posts] = clockSpeed + totClockTimes; /* 3636 */
                demo->tc_q_data[demo->tc_posts] = qpcSpeed + totQPCTimes;     /* 3637 */
                demo->tc_t_data[demo->tc_posts] = timeSpeed + totTimeTimes;   /* 3638 */
                demo->tc_f_data[demo->tc_posts] = ply[player_id]->level;      /* 3639 */
                if (totMusics)                                                /* 3640 */
                    demo->tc_s_data[demo->tc_posts] = 50.0 * accMusics / totMusics; /* 3641 */
                demo->tc_posts++;                           /* 3643 */
                if (demo->tc_posts > 99)
                    demo->tc_posts = 99;

                restart_time_cheat_window(&clockTimeStart, &qpc_start,   /* 3654-3661 */
                                          &timeTimeStart, &time_cheat_count);
                totMusics = 0;
                accMusics = 0;
            }
        }

        /* ---- 3681-3692: the debug reward keys.  `debug` has no store
         * anywhere in the image (PROMOTIONS.md batch 9), so this whole
         * block is unreachable in vivo; recovered as-is. ------------ */
        if (debug) {                                        /* 3681 */
            if (key[KEY_1] && allow_smpl) start_reward(5);     /* 3682 */
            if (key[KEY_2] && allow_smpl) start_reward(7);     /* 3683 */
            if (key[KEY_3] && allow_smpl) start_reward(15);    /* 3684 */
            if (key[KEY_4] && allow_smpl) start_reward(25);    /* 3685 */
            if (key[KEY_5] && allow_smpl) start_reward(35);    /* 3686 */
            if (key[KEY_6] && allow_smpl) start_reward(50);    /* 3687 */
            if (key[KEY_7] && allow_smpl) start_reward(70);    /* 3688 */
            if (key[KEY_8] && allow_smpl) start_reward(100);   /* 3689 */
            if (key[KEY_9] && allow_smpl) start_reward(140);   /* 3690 */
            if (key[KEY_0] && allow_smpl) start_reward(200);   /* 3691 */
            allow_smpl = !(key[KEY_1] || key[KEY_2] || key[KEY_3]   /* 3692 */
                        || key[KEY_4] || key[KEY_5] || key[KEY_6]
                        || key[KEY_7] || key[KEY_8] || key[KEY_9]
                        || key[KEY_0]);
        }

        /* ---- 3698-3711: the input -> simulation seam ------------- */
        lastX = (int)ply[player_id]->x;                     /* 3698 */
        lastY = (int)ply[player_id]->y;                     /* 3699 */

        handle_player_input(&ctrl);                         /* 3702 */
        update_player(ply[player_id]);                      /* 3703 */

        if (!itrcheck) {                                    /* 3706 */
            if (ply[player_id]->rotate && ply[player_id]->in_combo   /* 3707 */
                && !options.flash)
                create_particle(stars, (int)ply[player_id]->x,       /* 3708 */
                                (int)ply[player_id]->y - 16);
            for (i = 0; i < 512; i++)                       /* 3710 */
                if (stars[i].intensity)                     /* 3711 */
                    update_particle(&stars[i]);
        }

        /* ---- 3717-3763: scrolling.  `scroll_acc` is how many pixels
         * the world moved this tick; it is published in `any13`. --- */
        old_map_pos = map.offset;                           /* 3717 */

        if (ply[player_id]->y < 160) {                      /* 3719 */
            if (ply[player_id]->y < 140) scroll_acc = 2;    /* 3721 */
            else                         scroll_acc = 1;
            if (ply[player_id]->y < 120) scroll_acc++;      /* 3722 */
            if (ply[player_id]->y < 100) scroll_acc++;      /* 3723 */
            if (ply[player_id]->y < 80)  scroll_acc++;      /* 3724 */
            if (ply[player_id]->y < 60)  scroll_acc++;      /* 3725 */
            if (ply[player_id]->y < 40)  scroll_acc += 2;   /* 3726 */
            if (ply[player_id]->y < 20)  scroll_acc += 2;   /* 3727 */
            if (ply[player_id]->y < 0)   scroll_acc += 3;   /* 3728 */
            map.offset += scroll_acc;                       /* 3729 */
            ply[player_id]->y += scroll_acc;                /* 3731 */
            lastY += scroll_acc;                            /* 3732 */
        } else {
            scroll_acc = 0;
        }

        if (!ply[player_id]->dead)                          /* 3736 */
            clock_angle++;

        if (map.offset > 100 && !ply[player_id]->dead) {    /* 3738 */
            if (scroll == -1)                               /* 3739 */
                scroll = start_speeds[demo->start_speed];   /* 3740 */
            if (scroll) {                                   /* 3742 */
                map.offset += scroll;                       /* 3751 */
                scroll_acc += scroll;                       /* 3752 */
                ply[player_id]->y += scroll;                /* 3753 */
                lastY += scroll;                            /* 3754 */
            } else {
                if (step_count & 1) {                       /* 3743 */
                    map.offset++;                           /* 3744 */
                    scroll_acc++;                           /* 3745 */
                    ply[player_id]->y += 1;                 /* 3746 */
                    lastY++;                                /* 3747 */
                }
            }
        } else {
            if (!ply[player_id]->dead) {                    /* 3757 */
                clock_angle = 0;                            /* 3758 */
                fall_count = 0;
            }
        }

        any13 = scroll_acc;                                 /* 3763 */

        if (hurry_y > -100 && hurry_y < 480)                /* 3765 */
            hurry_y -= 2;

        if (demo->speed_increase) {                         /* 3766 */
            if (!ply[player_id]->dead                       /* 3767 */
                && speeds[next_speed] < fall_count
                && scroll <= 4) {
                ply[player_id]->ccc[next_speed] = ply[player_id]->level; /* 3768 */
                next_speed++;                               /* 3770 */
                scroll++;                                   /* 3771 */
                hurry_y = 479;                              /* 3772 */
                play_sound(speaker[0], 0, 0);               /* 3773 */
                play_sound(sounds[4], 0, 0);                /* 3774 */
            }
        }

        if (scroll == 5) {                                  /* 3778 */
            fall_count -= 45;                               /* 3779 */
            if (!ply[player_id]->dead)                      /* 3780 */
                clock_angle -= 45;
        }

        if (old_map_pos % 16 > map.offset % 16              /* 3783 */
            || scroll_acc > 15)                             /* 3787 */
            add_floor(&map);                                /* 3789 */

        /* ---- 3814-3826: the collision dispatch.  Five algorithms
         * behind one runtime selector; `collision_type` has no store
         * anywhere in the image either, so mode 0 is the live one. -- */
        switch (collision_type) {                           /* 3814 */
        case 0:                                             /* 3815 */
            handle_player_collision_original(lastX, lastY);  /* 3816 */
            break;
        case 1:                                             /* 3817 */
            handle_player_collision_old(lastX, lastY);       /* 3818 */
            break;
        case 2:                                             /* 3819 */
            handle_player_collision_vector(lastX, lastY);    /* 3820 */
            break;
        case 3:                                             /* 3821 */
            handle_player_collision_vector_2(lastX, lastY);  /* 3822 */
            break;
        case 4:                                             /* 3823 */
            handle_player_collision_combo(lastX, lastY);     /* 3824 */
            break;
        default:
            allegro_message("unknown collision type");       /* 3826 */
            break;
        }

        /* ---- 3833-3969: score / combo / floor accounting --------- */
        if (ply[player_id]->rotate)                          /* 3833 */
            ply[player_id]->angle += 8 << 16;                /* 3834 */

        if (ply[player_id]->in_combo) {                      /* 3837 */
            ply[player_id]->in_combo--;                      /* 3838 */
            if (!ply[player_id]->in_combo) {                 /* 3839 */
                if (ply[player_id]->acc_jumps > 1) {         /* 3840 */
                    ply[player_id]->score +=                 /* 3841 */
                        ply[player_id]->acc_level * ply[player_id]->acc_level;
                    rewResult = start_reward(ply[player_id]->acc_level); /* 3842 */
                    if (recording && !is_playing_custom_game)           /* 3843 */
                        profile->rewards[rewResult]++;
                    totComboFloors += ply[player_id]->acc_level;        /* 3844 */
                    numComboJumps++;                                    /* 3845 */

                    c.length = ply[player_id]->acc_level;    /* 3848 */
                    c.start = gdComboStart;                  /* 3849 */
                    c.end = gdComboStart + ply[player_id]->acc_level;   /* 3850 */
                    add_combo(gameData, &c);                 /* 3851 */

                    ply[player_id]->latest_combo = ply[player_id]->acc_level; /* 3853 */
                    if (ply[player_id]->acc_level > ply[player_id]->best_combo) /* 3854 */
                        ply[player_id]->best_combo = ply[player_id]->acc_level; /* 3855 */
                }
            }
        }

        if (!ply[player_id]->status) {                       /* 3862 */
            level = (get_level(&map, (int)ply[player_id]->y) - 1) / 10;  /* 3864 */

            diff = level - ply[player_id]->level;            /* 3870 */
            if (diff) {
                if (gdLastJumpDiff == diff) {                /* 3871 */
                    jumpSequence.num++;                      /* 3882 */
                } else {
                    jumpSequence.dist = gdLastJumpDiff;      /* 3874 */
                    add_jump_sequence(gameData, &jumpSequence); /* 3875 */
                    jumpSequence.num = 1;                    /* 3878 */
                    jumpSequence.start = level - diff;       /* 3879 */
                }
                gdLastJumpDiff = diff;                       /* 3885 */
            }

            if (level >= ply[player_id]->level) {            /* 3891 */
                diff = level - ply[player_id]->level;        /* 3893 */

                if (diff != lastJumpLength && diff) {        /* 3896 */
                    for (i = 0; i < 5; i++) {                /* 3897 */
                        if (ply[player_id]->jc[i] > ply[player_id]->jcTop[i])  /* 3900 */
                            ply[player_id]->jcTop[i] = ply[player_id]->jc[i];  /* 3901 */
                        ply[player_id]->jc[i] = 0;           /* 3904 */
                    }
                    lastJumpLength = 0;
                }

                if (diff > 0) {                              /* 3911 */
                    if (diff <= 5)                           /* 3912 */
                        ply[player_id]->jc[diff - 1]++;      /* 3913 */
                    if (diff == 1) {                         /* 3919 */
                        lastJumpLength = 1;
                    } else {
                        if (ply[player_id]->in_combo) {      /* 3920 */
                            ply[player_id]->acc_level += diff;   /* 3921 */
                            ply[player_id]->acc_jumps++;        /* 3922 */
                            ply[player_id]->in_combo = 100;     /* 3923 */
                        } else {
                            ply[player_id]->acc_level = diff;   /* 3926 */
                            ply[player_id]->acc_jumps = 1;      /* 3927 */
                            ply[player_id]->in_combo = 100;     /* 3928 */
                        }
                        lastJumpLength = diff;
                    }
                }

                if (diff == 1 && ply[player_id]->in_combo)   /* 3932 */
                    ply[player_id]->in_combo = 1;            /* 3933 */
                /* 3936/3937 belong to THIS arm only: when the player fell
                 * below its own recorded floor the original jumps straight
                 * from 0x412b66 to 0x412b70 (line 3962) and never touches
                 * gdComboStart. */
                if (!ply[player_id]->in_combo)               /* 3936 */
                    gdComboStart = level;                    /* 3937 */
            } else {
                if (ply[player_id]->in_combo)                /* 3943 */
                    ply[player_id]->in_combo = 1;
                for (i = 0; i < 5; i++) {                    /* 3945 */
                    if (ply[player_id]->jc[i] > ply[player_id]->jcTop[i])  /* 3948 */
                        ply[player_id]->jcTop[i] = ply[player_id]->jc[i];  /* 3949 */
                    ply[player_id]->jc[i] = 0;               /* 3952 */
                }
                lastJumpLength = 0;
            }

            ply[player_id]->level = level;                   /* 3962 */

            if (!numComboJumps) {                            /* 3967 */
                if (ply[player_id]->no_combo_top_floor < ply[player_id]->level)
                    ply[player_id]->no_combo_top_floor = gdComboStart;  /* 3969 */
            }
        }

        /* ---- 3976-4046: death, the falling sound and the "aight" -- */
        if (ply[player_id]->y > 540 && !ply[player_id]->dead) {   /* 3976 */
            playing = !itrcheck;                             /* 3977 */

            if (ply[player_id]->in_combo                     /* 3978 */
                && ply[player_id]->acc_jumps > 1)
                ply[player_id]->biggest_lost_combo =         /* 3979 */
                    ply[player_id]->acc_level;
            ply[player_id]->in_combo = 0;                    /* 3981 */
            ply[player_id]->dead = 1;                        /* 3982 */
            play_sound(custom.falling, 0, 1);                /* 3983 */
            endTime = time(NULL);                            /* 3985 */

            for (i = 0; i < 5; i++) {                        /* 3988 */
                if (ply[player_id]->jc[i] > ply[player_id]->jcTop[i])  /* 3991 */
                    ply[player_id]->jcTop[i] = ply[player_id]->jc[i];  /* 3992 */
                ply[player_id]->jc[i] = 0;                   /* 3995 */
            }

            jumpSequence.dist = gdLastJumpDiff;              /* 3999 */
            add_jump_sequence(gameData, &jumpSequence);      /* 4000 */

            if (!numComboJumps) {                            /* 4003 */
                if (ply[player_id]->no_combo_top_floor < ply[player_id]->level)
                    ply[player_id]->no_combo_top_floor = ply[player_id]->level; /* 4004 */
            }
            lastJumpLength = 0;
            falling = 1;
        }

        if (ply[player_id]->y > 900 && !game_over) {         /* 4010 */
            play_sound(speaker[1], 0, 0);                    /* 4012 */
            game_over = 2;
        }

        if (falling)                                         /* 4015 */
            falling++;
        if (falling > ply[player_id]->level * 5 || falling > 250) {  /* 4016 */
            play_sound(sounds[6], 0, 1);                     /* 4017 */
            if (custom.falling)                              /* 4018 */
                stop_sample(custom.falling);                 /* 4019 */
            ply[player_id]->shake = 24;                      /* 4022 */
            falling = 0;
        }

        if (next_aight <= ply[player_id]->level) {           /* 4027 */
            play_sound(sounds[2], 0, 0);                     /* 4028 */
            if (!options.flash) {                            /* 4029 */
                for (i = 0; i < next_aight / 2; i++) {
                    p = create_particle(stars, new_rand() % 600 + 20, 480);  /* 4030 */
                    stars[p].sy = ((new_rand() % 200) << 16) / 10;           /* 4031 */
                }
            }
            if (next_aight > 999)                            /* 4033 */
                next_aight += 500;                           /* 4034 */
            else
                next_aight += 50;                            /* 4037 */
        }

        if (!ply[player_id]->edge)                           /* 4042 */
            ply[player_id]->edge_drawn = 0;
        if (ply[player_id]->edge_drawn) {                    /* 4043 */
            if (ply[player_id]->edge_drawn == 11             /* 4044 */
                && !ply[player_id]->status)
                play_sound(custom.edge, 1, 1);
            if (ply[player_id]->edge_drawn == 50)            /* 4045 */
                ply[player_id]->edge_drawn = 0;              /* 4046 */
        }

        /* 4049/4056 -- when the game ends after death.  The `debug` arm
         * reads inverted next to the recording arm (dead <= 99 stops the
         * game rather than dead > 99); `debug` is never set in the
         * shipped image, so it is unreachable, and it is recovered
         * exactly as the branch is written rather than "corrected". */
        if (debug) {                                         /* 4049 */
            if (ply[player_id]->dead <= 99)                  /* 4050 */
                playing = FALSE;
        } else if (recording) {                              /* 4056 */
            if (ply[player_id]->dead > 100)
                playing = FALSE;
        }

        /* ---- 4062-4090: F1 screenshot, and the wall-clock rebase it
         * needs so that time spent writing a PNG is not read as a
         * cheat. -------------------------------------------------- */
        if (!itrcheck && key[KEY_F1]) {                      /* 4062 */
            pauseTime = time(NULL);                          /* 4063 */
            take_screenshot(swap_screen);                    /* 4064 */
            while (key[KEY_F1])                              /* 4065 */
                ;
            addTime = time(NULL) - pauseTime;                /* 4066 */
            if (addTime > 0)                                 /* 4067 */
                startTime += addTime;                        /* 4068 */
            resync_music_counter(&musicCounter, &accMusics, &totMusics); /* 4075-4077 */
            restart_time_cheat_window(&clockTimeStart, &qpc_start,       /* 4083-4090 */
                                      &timeTimeStart, &time_cheat_count);
        }

        if (ply[player_id]->shake) {                         /* 4094 */
            ply[player_id]->shake--;                         /* 4095 */
            shake = new_rand() % 8;                          /* 4097 */
        }

        update_frame();                                      /* 4100 */

        /* 4104 -- the `quit` arm of this test cannot be reached: nothing
         * before this point in a tick sets `quit`, and every setter that
         * runs later also clears `playing`, which ends the loop.  GCC
         * accordingly emitted only the `quit = TRUE` store there. */
        if (quit || closeButtonClicked) {                    /* 4104 */
            quit = TRUE;
            playing = FALSE;
        }

        /* ============ 4109-4245: the two pause screens ============= */
        if (recording) {                                     /* 4109 */
            if (key[KEY_ESC]) {                              /* 4110 */
                if (ply[player_id]->dead) {                  /* 4111 */
                    log2file("  player quit after dying");   /* 4112 */
                    playing = FALSE;
                } else {
                    /* ---- pause block A: ESC, "really exit?" ------ */
                    pauseTime = time(NULL);                  /* 4117 */
                    fc = fall_count;                         /* 4118 */
                    ca = clock_angle;                        /* 4119 */
                    log2file("  game paused with esc");      /* 4120 */

                    draw_pause_curtain(swap_screen);         /* 4122-4124 */

                    textout_centre_ex(swap_screen,           /* 4126 */
                        asset_font(ASSET_DATA_FONT_BIG_WHITE),
                        "DO YOU REALLY WANT TO EXIT?", 320, 160, -1, -1);
                    textout_centre_ex(swap_screen,           /* 4127 */
                        asset_font(ASSET_DATA_FONT_MED_WHITE),
                        "Press any key to resume", 320, 210, -1, -1);
                    textout_centre_ex(swap_screen,           /* 4128 */
                        asset_font(ASSET_DATA_FONT_MED_WHITE),
                        "Press ESC to exit", 320, 240, -1, -1);
                    blit_to_screen(swap_screen);             /* 4129 */
                    play_sound(custom.yo, 0, 1);             /* 4130 */

                    poll_control(&ctrl, FALSE);              /* 4132 */
                    while (is_any(&ctrl) || is_pause(&ctrl)  /* 4133 */
                           || (!closeButtonClicked && key[KEY_ESC])) {
                        poll_control(&ctrl, FALSE);          /* 4134 */
                        rest(2);                             /* 4135 */
                    }
                    clear_keybuf();                          /* 4137 */
                    while (!keypressed() && !is_any(&ctrl)   /* 4138 */
                           && !is_pause(&ctrl) && !closeButtonClicked
                           && !key[KEY_ESC]) {
                        poll_control(&ctrl, FALSE);          /* 4139 */
                        rest(2);                             /* 4140 */
                    }
                    while (!closeButtonClicked && is_pause(&ctrl)) {  /* 4142 */
                        poll_control(&ctrl, FALSE);          /* 4143 */
                        rest(2);                             /* 4144 */
                    }
                    if (key[KEY_ESC]) {                      /* 4146 */
                        log2file("  game quit from esc pause"); /* 4150 */
                        profile->games_quit++;               /* 4151 */
                        endTime = time(NULL);                /* 4152 */
                        quit = TRUE;
                        playing = FALSE;
                    }
                    clear_keybuf();                          /* 4154 */
                    fall_count = fc;                         /* 4156 */
                    clock_angle = ca;                        /* 4157 */
                    log2file("  game unpaused");             /* 4158 */
                    addTime = time(NULL) - pauseTime;        /* 4159 */
                    if (addTime > 0)                         /* 4160 */
                        startTime += addTime;                /* 4161 */
                    resync_music_counter(&musicCounter, &accMusics, &totMusics); /* 4168-4170 */
                    restart_time_cheat_window(&clockTimeStart, &qpc_start,       /* 4175-4182 */
                                              &timeTimeStart, &time_cheat_count);
                }
            }

            if (is_pause(&ctrl) && !ply[player_id]->dead) {  /* 4186 */
                /* ---- pause block B: the pause key ------------- */
                pauseTime = time(NULL);                      /* 4187 */
                fc = fall_count;                             /* 4188 */
                ca = clock_angle;                            /* 4189 */
                log2file("  game paused with pause key");    /* 4190 */

                draw_pause_curtain(swap_screen);             /* 4192-4194 */

                textout_centre_ex(swap_screen,               /* 4196 */
                    asset_font(ASSET_DATA_FONT_BIG_WHITE),
                    "Game Paused", 320, 160, -1, -1);
                textout_centre_ex(swap_screen,               /* 4197 */
                    asset_font(ASSET_DATA_FONT_MED_WHITE),
                    "Press any key to resume", 320, 210, -1, -1);
                blit_to_screen(swap_screen);                 /* 4198 */
                play_sound(custom.yo, 0, 1);                 /* 4199 */

                poll_control(&ctrl, FALSE);                  /* 4200 */
                while (is_any(&ctrl) || is_pause(&ctrl)) {   /* 4201 */
                    poll_control(&ctrl, FALSE);              /* 4202 */
                    rest(2);                                 /* 4203 */
                }
                clear_keybuf();                              /* 4206 */
                while (!keypressed() && !is_any(&ctrl)       /* 4207 */
                       && !is_pause(&ctrl) && !key[KEY_ESC]) {
                    poll_control(&ctrl, FALSE);              /* 4208 */
                    rest(2);                                 /* 4209 */
                }
                poll_control(&ctrl, FALSE);                  /* 4212 */
                while (is_pause(&ctrl) || key[KEY_ESC]) {    /* 4213 */
                    poll_control(&ctrl, FALSE);              /* 4214 */
                    rest(2);                                 /* 4215 */
                }
                fall_count = fc;                             /* 4219 */
                clock_angle = ca;                            /* 4220 */
                log2file("  game unpaused");                 /* 4221 */
                addTime = time(NULL) - pauseTime;            /* 4222 */
                if (addTime > 0)                             /* 4223 */
                    startTime += addTime;                    /* 4224 */
                resync_music_counter(&musicCounter, &accMusics, &totMusics); /* 4231-4233 */
                restart_time_cheat_window(&clockTimeStart, &qpc_start,       /* 4238-4245 */
                                          &timeTimeStart, &time_cheat_count);
            }
        } else if (!itrcheck) {
            /* ======== 4249-4310: replay transport controls ========= */
            poll_control(&rec_ctrl, FALSE);                  /* 4251 */

            if (ply[player_id]->dead) {                      /* 4253 */
                log2file("  replay ended after death");      /* 4255 */
                playing = FALSE;
            }
            if (key[KEY_ESC]) {                              /* 4264 */
                log2file("  quit from replay");              /* 4265 */
                quit = TRUE;
                playing = FALSE;
            }
            if (key[KEY_SPACE] && !ply[player_id]->dead) {   /* 4271 */
                log2file("  replay paused");                 /* 4272 */
                while (key[KEY_SPACE])                       /* 4273 */
                    poll_control(&rec_ctrl, TRUE);
                while (!key[KEY_SPACE] && !key[KEY_RIGHT]    /* 4274 */
                       && !key[KEY_ESC] && !key[KEY_UP]) {
                    poll_control(&rec_ctrl, TRUE);           /* 4275 */
                    if (key[KEY_F1]) {                       /* 4276 */
                        take_screenshot(swap_screen);        /* 4277 */
                        while (key[KEY_F1])                  /* 4278 */
                            ;
                    }
                }
                while (key[KEY_SPACE])                       /* 4281 */
                    poll_control(&rec_ctrl, TRUE);
                fast_forward = 0;                            /* 4282 */
                fast_fast_forward = 0;                       /* 4283 */
                log2file("  replay unpaused");               /* 4284 */
            }
            if (key[KEY_RIGHT]) {                            /* 4287 */
                fast_forward++;                              /* 4288 */
                fast_fast_forward = 0;                       /* 4289 */
            } else {
                fast_forward = 0;                            /* 4292 */
            }
            if (key[KEY_UP]) {                               /* 4295 */
                if (!ply[player_id]->dead                    /* 4296 */
                    && ply[player_id]->level < demo->floor - 10) {
                    fast_fast_forward++;                     /* 4297 */
                    fast_forward = 0;                        /* 4298 */
                    next_floor = ((ply[player_id]->level + 100) / 100) * 100; /* 4299 */
                    if (next_floor > demo->floor - 10)       /* 4301 */
                        next_floor = demo->floor - 10;
                } else {
                    fast_fast_forward = 0;                   /* 4310 */
                    next_floor = -1;
                }
            } else {
                if (ply[player_id]->level >= next_floor      /* 4309 */
                    || ply[player_id]->dead) {
                    fast_fast_forward = 0;                   /* 4310 */
                    next_floor = -1;
                }
            }
        }

        /* ======== 4319-4370: frame skipping, drawing, pacing ======= */
        if (!itrcheck) {                                     /* 4319 */
            someCounter__play++;                             /* 4324 */

            ffstep = fast_forward ? 4 : 1;                   /* 4327 */
            if (fast_fast_forward)                           /* 4330 */
                ffstep = 32;

            if (!quit && someCounter__play % ffstep == 0) {   /* 4337 */
                draw_frame(swap_screen);                      /* 4338 */

                if (ply[player_id]->shake) {                  /* 4346 */
                    acquire_screen();
                    blit(swap_screen, swap_screen, 0, shake, 0, 0,  /* 4348 */
                         swap_screen->w, swap_screen->h);
                    blit_to_screen(swap_screen);              /* 4349 */
                    release_screen();
                } else {
                    blit_to_screen(swap_screen);              /* 4353 */
                }

                if (!debug) {                                 /* 4356 */
                    while (!cycle_count)                      /* 4357 */
                        rest(2);
                } else {
                    if (key[KEY_TAB] && key[KEY_LSHIFT]) {    /* 4360 */
                        while (cycle_count <= 7)              /* 4361 */
                            ;
                    } else {
                        while (!cycle_count)                  /* 4363 */
                            rest(2);
                    }
                }
            }
        }

        if (!itrcheck)                                        /* 4369 */
            rest(2);
    }
    /* ==================== end of the tick loop ==================== */

    /* ---- 4374-4426: what the finished game reports -------------- */
    if (recording) {                                          /* 4374 */
        addTime = endTime - startTime;                        /* 4375 */
        if (addTime > 0)                                      /* 4376 */
            profile->seconds_spent_playing += addTime;        /* 4377 */
    } else {
        collect_game_data();                                  /* 4389-4418 */
        if (itrcheck) {                                       /* 4421 */
            char *xmlStr = getGameDataXML(gameData);          /* 4422 */
            printf("%s", xmlStr);                             /* 4423 */
            free(xmlStr);                                     /* 4424 */
        }
        if (itrcheck)                                         /* 4426 */
            return 0;
    }

    log2file(" play ended");                                  /* 4456 */
    fast_forward = 0;                                         /* 4457 */
    fast_fast_forward = 0;                                    /* 4458 */

    if (recording)                                            /* 4500 */
        save_personal_bests(quit, numComboJumps, totComboFloors);  /* 4503-4624 */

    syncProfileFromOptions();                                 /* 972-975 */
    save_profile(profile);                                    /* 4641 */

    /* ================================================================
     * 4643-4990 -- the game-over half.  Two animation loops (the results
     * card sliding in, then the rank/scroller/initials screen), the
     * high-score commit, and the start-floor unlock notice.
     *
     * IN-VIVO-PENDING as a whole: every loop here is driven by
     * `readkey()`, `keypressed()` and the 50 Hz `cycle_count` timer, so
     * the offline harness has no domain that can express it.  The
     * in-vivo per-tick digest over a recorded game is the oracle.
     * ============================================================== */
    if (!quit && !closeButtonClicked) {                       /* 4643 */

        for (i = 0; i < 15; i++)                              /* 4650 */
            qualify[i] = 0;

        qualifyValue[0] = ply[player_id]->level * 10 + ply[player_id]->score; /* 4652 */
        qualifyValue[2] = ply[player_id]->level;              /* 4653 */
        qualifyValue[1] = ply[player_id]->best_combo;         /* 4654 */
        qualifyValue[3] = ply[player_id]->biggest_lost_combo; /* 4655 */
        qualifyValue[4] = ply[player_id]->no_combo_top_floor; /* 4656 */
        for (i = 0; i < 5; i++) {                             /* 4657 */
            qualifyValue[5 + i] = ply[player_id]->ccc[i];     /* 4658 */
            qualifyValue[10 + i] = ply[player_id]->jcTop[i];  /* 4659 */
        }

        gotHigh = 0;
        for (i = 0; i < 15; i++) {                            /* 4662 */
            qualify[i] = qualify_hisc_table(hisc_tables[i], qualifyValue[i]); /* 4663 */
            gotHigh += qualify[i];                            /* 4664 */
        }

        if (!recording)                                       /* 4668 */
            gotHigh = 0;

        /* 4670 -- ASSETS.md's two open computed data[N] sites are these:
         * the logo is one of exactly two literal objects, not a range. */
        if (gotHigh > 0)
            gameover_bmp_id = ASSET_DATA_HIGHSCORE;
        else
            gameover_bmp_id = ASSET_DATA_GAMEOVER;
        if (is_playing_custom_game)                           /* 4671 */
            gameover_bmp_id = ASSET_DATA_GAMEOVER;

        if (gotHigh && !is_playing_custom_game) {             /* 4673 */
            log2file(" player qualified for highscore");      /* 4674 */
            play_sound(sounds[7], 0, 0);                      /* 4675 */
        } else {
            log2file(" player did not qualify for highscore");/* 4678 */
            play_sound(speaker[1], 0, 0);                     /* 4679 */
        }

        if (!debug) {                                         /* 4683 */
            strcpy(letters, "ABCDEFGHIJKLMNOPQRSTUVWXYZ .\244"); /* 4687 */
            len = strlen(letters) - 1;                        /* 4688 */
            buf[0] = '.'; buf[1] = 0;                         /* 4689 */
            buf[2] = '.'; buf[3] = 0;
            buf[4] = '.'; buf[5] = 0;
            buf[6] = 0;   buf[7] = 0;

            isGuest = (stricmp(profile->handle, "guest") == 0); /* 4692 */

            hy = 480;                                         /* 4693 */

            /* ---- 4695-4734: the results card slides up to y=130 --- */
            while (hy > 140) {                                /* 4695 */
                cycle_count = 0;                              /* 4696 */
                ply[player_id]->dead -= 16;                   /* 4697 */
                hy += (130 - hy) * 0.1;                       /* 4699 */
                update_frame();                               /* 4700 */
                for (i = 0; i < 512; i++)                     /* 4701 */
                    if (stars[i].intensity)
                        update_particle(&stars[i]);
                if (hurry_y > -100 && hurry_y < 480)          /* 4702 */
                    hurry_y -= 2;
                draw_frame(swap_screen);                      /* 4703 */
                draw_results(swap_screen,                     /* 4704 */
                             asset_bitmap((asset_id)gameover_bmp_id),
                             (int)hy, qualify, qualifyValue,
                             !is_playing_custom_game && recording != 0);
                if (isGuest && gotHigh && !is_playing_custom_game && recording) /* 4705 */
                    textout_centre_ex(swap_screen,            /* 4706 */
                        asset_font(ASSET_DATA_FONT_MED_WHITE),
                        "Enter your initials", 320, (int)(hy + hy + 80), -1, -1);

                if (falling)                                  /* 4709 */
                    falling++;
                if (falling > ply[player_id]->level * 5 || falling > 250) { /* 4710 */
                    play_sound(sounds[6], 0, 1);              /* 4711 */
                    if (custom.falling)                       /* 4712 */
                        stop_sample(custom.falling);
                    ply[player_id]->shake = 24;               /* 4714 */
                    falling = 0;
                }

                if (ply[player_id]->shake) {                  /* 4716 */
                    acquire_screen();
                    blit(swap_screen, screen, 0, new_rand() % 8, 0, 0,  /* 4718 */
                         swap_screen->w, swap_screen->h);
                    release_screen();
                    ply[player_id]->shake--;                  /* 4720 */
                } else {
                    blit_to_screen(swap_screen);              /* 4722 */
                }

                if (key[KEY_F1]) {                            /* 4725 */
                    take_screenshot(swap_screen);             /* 4726 */
                    while (key[KEY_F1])                       /* 4727 */
                        ;
                }
                if (!(key[KEY_TAB] && key[KEY_LSHIFT]))       /* 4731 */
                    while (!cycle_count)                      /* 4734 */
                        rest(2);
            }

            ply[player_id]->dead = 0;                         /* 4737 */
            clear_keybuf();                                   /* 4738 */

            /* ---- 4742-4775: the message the summary scroller runs.
             * NOTE: on the `gotHigh` arm the original copies
             * "New personal records!    " into the buffer and then
             * unconditionally strcpy()s the hint (or the guest notice)
             * over the top of it -- lines 4754-4769, which evidently
             * meant to append the per-category record list, emitted no
             * code at all, and `skipCategories`/`h`/`achs` (declared at
             * 4750-4752) have no DWARF location.  Recovered as the image
             * behaves, not as the author appears to have intended. -- */
            if (!recording) {                                 /* 4742 */
                summary_scroller_message[0] = 0;              /* 4743 */
            } else if (is_playing_custom_game) {              /* 4745 */
                strcpy(summary_scroller_message,              /* 4746 */
                       "Custom mode is crazy fun but does not add to your "
                       "profile. Play a normal game to get your name in the "
                       "highscore lists and to build up your profile!");
            } else if (gotHigh) {                             /* 4749 */
                strcpy(summary_scroller_message, "New personal records!    "); /* 4753 */
                if (isGuest)                                  /* 4770 */
                    strcpy(summary_scroller_message,
                           "You're playing in guest mode. Start a profile and "
                           "record your progress, your highscores and your best "
                           "replays!");
                else
                    strcpy(summary_scroller_message, hints[new_rand() % 45]);
            } else {
                if (isGuest)                                  /* 4775 */
                    strcpy(summary_scroller_message,
                           "You're playing in guest mode. Start a profile and "
                           "record your progress, your highscores and your best "
                           "replays!");
                else
                    strcpy(summary_scroller_message, hints[new_rand() % 45]);
            }

            init_scroller(&summary_scroller,                  /* 4781 */
                          asset_font(ASSET_DATA_FONT_SMALL),
                          summary_scroller_message, 640, 30, TRUE);
            scroll_scroller(&summary_scroller, -150);         /* 4782 */

            scrollerY = -20;                                  /* 4784 */
            new_rank_id = get_rank_id(profile);               /* 4787 */
            rank_bmp_id = ASSET_DATA_RANK_00 + new_rank_id;   /* 4788 */
            rank_y = 580;                                     /* 4789 */
            alpha_pos = 0;                                    /* 4685 */
            pos = 0;                                          /* 4686 */
            skip_keys = 0;                                    /* 4690 */
            done = 20;                                        /* 4684 */

            /* ---- 4792-4932: the results screen proper ------------ */
            while (done) {                                    /* 4792 */
                if (closeButtonClicked)                       /* 4793 */
                    return 0;

                cycle_count = 0;                              /* 4797 */
                step_count++;                                 /* 4798 */
                update_frame();                               /* 4800 */

                if (key[KEY_F1]) {                            /* 4802 */
                    take_screenshot(swap_screen);             /* 4803 */
                    while (key[KEY_F1])                       /* 4804 */
                        ;
                }
                if (hurry_y > -100 && hurry_y < 480)          /* 4808 */
                    hurry_y -= 2;
                draw_frame(swap_screen);                      /* 4809 */
                draw_results(swap_screen,                     /* 4810 */
                             asset_bitmap((asset_id)gameover_bmp_id),
                             (int)hy, qualify, qualifyValue,
                             !is_playing_custom_game && recording != 0);

                if (isGuest && gotHigh && !is_playing_custom_game && recording) { /* 4811 */
                    textout_centre_ex(swap_screen,            /* 4812 */
                        asset_font(ASSET_DATA_FONT_MED_WHITE),
                        "Enter your initials", 320, (int)(hy + hy + 80), -1, -1);
                    /* the three initial slots, the current one blinking */
                    if (pos != 0 || (step_count & 4))         /* 4814 */
                        textout_centre_ex(swap_screen,
                            asset_font(ASSET_DATA_FONT_MED_WHITE),
                            &buf[0], 300, (int)(hy + hy + 120), -1, -1);
                    if (pos != 1 || (step_count & 4))         /* 4815 */
                        textout_centre_ex(swap_screen,
                            asset_font(ASSET_DATA_FONT_MED_WHITE),
                            &buf[2], 320, (int)(hy + hy + 120), -1, -1);
                    if (pos != 2 || (step_count & 4))         /* 4816 */
                        textout_centre_ex(swap_screen,
                            asset_font(ASSET_DATA_FONT_MED_WHITE),
                            &buf[4], 340, (int)(hy + hy + 120), -1, -1);
                    if (pos == 3 && (step_count & 4))         /* 4817 */
                        textout_centre_ex(swap_screen,
                            asset_font(ASSET_DATA_FONT_MED_WHITE),
                            "%", 360, (int)(hy + hy + 120), -1, -1);
                }

                if (current_rank_id != new_rank_id) {         /* 4820 */
                    draw_sprite(swap_screen,                  /* 4821 */
                                asset_bitmap((asset_id)rank_bmp_id), 20, rank_y);
                    textout_ex(swap_screen,                   /* 4822 */
                        asset_font(ASSET_DATA_FONT_MED_WHITE),
                        "rank up!", 20, rank_y + 70, -1, -1);
                    rank_y += (320 - rank_y) * 0.1;           /* 4823 */
                }

                if (summary_scroller_message[0]) {            /* 4827 */
                    scroll_scroller(&summary_scroller, -2);   /* 4828 */
                    drawing_mode(5, NULL, 0, 0);              /* 4829 */
                    set_trans_blender(0, 0, 0, 110);          /* 4830 */
                    /* three stacked translucent bands, 20/18/16 tall, that
                     * give the scroller its soft shadow.  These are
                     * rectfill (vtable +0x3c) at 0x414e13/0x414e5d/0x414ea7,
                     * NOT hline -- the y2 argument is what shrinks. */
                    rectfill(swap_screen, 0, scrollerY, 639,             /* 4831 */
                             scrollerY + 20, makecol(0, 0, 0));
                    rectfill(swap_screen, 0, scrollerY, 639,             /* 4832 */
                             scrollerY + 18, makecol(0, 0, 0));
                    rectfill(swap_screen, 0, scrollerY, 639,             /* 4833 */
                             scrollerY + 16, makecol(0, 0, 0));
                    solid_mode();                             /* 4834 */
                    draw_scroller(&summary_scroller, swap_screen, TRUE,  /* 4835 */
                                  scrollerY, makecol(150, 150, 150));
                    if (!draw_scroller(&summary_scroller, swap_screen, FALSE, /* 4836 */
                                       scrollerY, makecol(200, 200, 200)))
                        restart_scroller(&summary_scroller);
                    scrollerY += (0 - scrollerY) * 0.1;       /* 4838 */
                }

                if (falling)                                  /* 4843 */
                    falling++;
                if (falling > ply[player_id]->level * 5 || falling > 250) { /* 4844 */
                    play_sound(sounds[6], 0, 1);              /* 4845 */
                    if (custom.falling)                       /* 4846 */
                        stop_sample(custom.falling);          /* 4847 */
                    ply[player_id]->shake = 24;               /* 4850 */
                    falling = 0;
                }

                if (ply[player_id]->shake) {                  /* 4852 */
                    acquire_screen();
                    blit(swap_screen, screen, 0, new_rand() % 8, 0, 0,  /* 4855 */
                         swap_screen->w, swap_screen->h);
                    release_screen();
                    ply[player_id]->shake--;                  /* 4857 */
                } else {
                    blit_to_screen(swap_screen);              /* 4860 */
                }

                /* ---- 4863-4915: the three-letter initials entry --- */
                if (isGuest && gotHigh && !is_playing_custom_game && recording) { /* 4863 */
                    poll_control(&ctrl, FALSE);               /* 4864 */
                    if (keypressed() && done == 20) {         /* 4865 */
                        k = (readkey() & 0xff) - 32;          /* 4866 */
                        if (k == -24) k = 0244;               /* 4867 -- backspace glyph */
                        else if (k == 14) k = '.';            /* 4868 */
                        else if (k == 1) k = '!';             /* 4869 */
                        if (k != 32) {                        /* 4870 */
                            for (j = 0; j < len; j++) {       /* 4871 */
                                if (letters[j] == k) {        /* 4872 */
                                    buf[pos * 2] = letters[j];/* 4875 */
                                    pos++;                    /* 4876 */
                                    if (pos == 3)             /* 4877 */
                                        done = 19;
                                    alpha_pos = j;
                                    skip_keys = 100;
                                }
                            }
                        }
                    }

                    if (skip_keys) {                          /* 4883 */
                        skip_keys--;
                    } else {
                        if (is_right(&ctrl)) {                /* 4884 */
                            alpha_pos++;                      /* 4885 */
                            skip_keys = 8;                    /* 4886 */
                            if (alpha_pos > len)
                                alpha_pos = 0;
                        }
                        if (is_left(&ctrl)) {                 /* 4889 */
                            skip_keys = 8;                    /* 4891 */
                            alpha_pos--;
                            if (alpha_pos < 0)
                                alpha_pos = len;
                        }
                        if (is_fire(&ctrl)) {                 /* 4894 */
                            if (letters[alpha_pos] == 0244) { /* 4895 */
                                if (pos) {
                                    buf[pos * 2] = '.';       /* 4896 */
                                    pos--;                    /* 4897 */
                                }
                            } else if (pos > 1) {             /* 4899 */
                                if (done == 20) {             /* 4900 */
                                    done = 19;
                                    pos++;
                                }
                            } else {
                                pos++;
                            }
                            skip_keys = 100;                  /* 4902 */
                        }
                        if (key[KEY_DEL] || key[KEY_BACKSPACE]) {  /* 4906 */
                            buf[pos * 2] = '.';               /* 4907 */
                            skip_keys = 7;                    /* 4908 */
                            if (pos)
                                pos--;
                        } else if (skip_keys) {               /* 4912 */
                            skip_keys--;
                        }
                    }
                    if (!is_any(&ctrl) && !key[KEY_DEL] && !key[KEY_BACKSPACE]) /* 4913 */
                        skip_keys = 0;
                    if (pos <= 2)                             /* 4915 */
                        buf[pos * 2] = letters[alpha_pos];
                }

                if (done != 20)                               /* 4918 */
                    done--;

                poll_control(&ctrl, FALSE);                   /* 4920 */
                if (!(isGuest && gotHigh && !is_playing_custom_game)) { /* 4921 */
                    if (keypressed() || is_fire(&ctrl)) {     /* 4922 */
                        if (done == 20)                       /* 4923 */
                            done = 14;
                    }
                }

                if (!(key[KEY_TAB] && key[KEY_LSHIFT]))       /* 4929 */
                    while (!cycle_count)                      /* 4932 */
                        rest(2);
            }

            /* ---- 4940-4951: commit the high scores --------------- */
            if (!is_playing_custom_game && recording) {       /* 4940 */
                guestName[0] = buf[0];                        /* 4941 */
                guestName[1] = buf[2];
                guestName[2] = buf[4];
                guestName[3] = 0;
                strcpy(postName, isGuest ? guestName : profile->handle);  /* 4943 */
                for (i = 0; i < 15; i++) {                    /* 4948 */
                    if (qualify[i] > 0) {                     /* 4949 */
                        enter_hisc_table(hisc_tables[i], qualifyValue[i], postName); /* 4950 */
                        sort_hisc_table(hisc_tables[i]);      /* 4951 */
                    }
                }
            }
        }

        /* ---- 4963-4987: a new start floor was unlocked ----------- */
        if (recording && !debug && !is_playing_custom_game) { /* 4963 */
            i = ply[player_id]->level / 100;                  /* 4964 */
            if (oldUnlockedFloors < i && i <= 9) {            /* 4966 */
                fadeOut(16);                                  /* 4968 */
                blit(asset_bitmap(ASSET_DATA_TITLE_BG), swap_screen,  /* 4971 */
                     0, 0, 0, 0, 640, 480);
                set_trans_blender(0, 0, 0, 158);              /* 4974 */
                drawing_mode(5, NULL, 0, 0);                  /* 4975 */
                rectfill(swap_screen, 0, 0, SCREEN_W, SCREEN_H, makecol(0, 0, 0)); /* 4976 */
                solid_mode();                                 /* 4977 */
                draw_sprite(swap_screen,                      /* 4980 */
                    asset_bitmap(ASSET_DATA_HEROFACE_000),
                    320 - asset_bitmap(ASSET_DATA_HEROFACE_000)->w / 2, 20);
                textout_centre_ex(swap_screen,                /* 4981 */
                    asset_font(ASSET_DATA_FONT_MED_WHITE),
                    "A new start floor", 320, 300, -1, -1);
                textout_centre_ex(swap_screen,                /* 4982 */
                    asset_font(ASSET_DATA_FONT_MED_WHITE),
                    "has been unlocked!", 320, 350, -1, -1);
                textout_centre_ex(swap_screen,                /* 4983 */
                    asset_font(ASSET_DATA_FONT_MED_WHITE),
                    "(Get it in the options menu)", 320, 440, -1, -1);
                play_sound(sounds[2], 0, 0);                  /* 4984 */
                fadeIn(swap_screen, 16);                      /* 4985 */
                while (key[KEY_ESC] || key[KEY_ENTER] || key[KEY_SPACE])  /* 4986 */
                    ;
                while (!key[KEY_ESC] && !key[KEY_ENTER] && !key[KEY_SPACE]) /* 4987 */
                    ;
            }
        }

        save_config();                                        /* 4994 */
        stopGameMusic();                                      /* 4997 */
        if (checkMusicVoiceID >= 0)                           /* 4998 */
            voice_stop(checkMusicVoiceID);                    /* 4999 */

        play_again = FALSE;                                   /* 5002 */
        if (recording && !debug) {                            /* 5003 */
            in_replay_menu = TRUE;
            play_again = do_replay_menu();                    /* 5004 */
            in_replay_menu = FALSE;                           /* 5005 */
        }
    }

    if (recording)                                            /* 5011 */
        play_sound(speaker[2], 0, 0);
    stopGameMusic();                                          /* 5013 */
    if (checkMusicVoiceID >= 0)                               /* 5014 */
        voice_stop(checkMusicVoiceID);                        /* 5015 */
    clear_bitmap(screen);

    return play_again;                                        /* 5021 */
}
