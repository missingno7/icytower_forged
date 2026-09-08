/* pf_bindings.h -- GENERATED FILE. DO NOT EDIT.
 * Produced by carrier/gen/gen_bindings.py from:
 *   interop_index.json
 *   it_globals.h (reused cast expressions)
 *   it_funcs.h (reused PFN_* typedefs + cast expressions)
 *   imports.json (IAT slot VAs for GUEST_CRT_IMPORTS)
 * Generated: 2026-09-07 23:56:11 UTC
 * Excluded (compiled natively, name kept free): add_combo, add_floor, add_jump_sequence, asset_sample, assets_standalone_family, assets_standalone_raw, blit_to_screen, check_control_key, clickedCloseButton, create_particle, cycle_counter, draw_background, draw_buffer, draw_clock, draw_combo_meter, draw_debug_overlay, draw_floors, draw_frame, draw_hurry_sign, draw_player, draw_replay_hud, draw_score, draw_scroller, draw_side_rails, draw_star_field, draw_stars, floor_size_modifiers, fps_counter, getFloorData, get_controls, get_demo, get_gamepad, get_level, handle_player_collision_combo, handle_player_collision_old, handle_player_collision_original, handle_player_collision_vector, handle_player_collision_vector_2, handle_player_input, init_control, is_any, is_down, is_enter, is_fire, is_left, is_pause, is_right, is_solid, is_up, it_al_draw_sprite, it_al_fixfloor, it_al_fixfloor2, it_al_fixsin, it_al_fixtoi, it_al_fixtoi2, it_al_ftofix, it_al_rotate_sprite, jump_player, line_intersect, new_rand, ok_to_play, play_jump_sound, poll_control, reset_map, reset_particles, reset_player, restart_scroller, scroll_scroller, set_control, start_reward, switchedFromProgram, switchedToProgram, update_frame, update_particle, update_player
 * Also defines the purity-safe guard: ICYTOWER_BINDINGS_ACTIVE
 *
 * See win32_pilot.md SS7a: this header is forced-included (/FI) ONLY
 * when address-free clean C from src/ is compiled INTO the carrier.
 * It maps every game-scope global/function PLAIN name to its original
 * address, so `extern int reward_scale;` and a bare call to a game
 * function in src/ resolve to (*(T*)VA) / ((PFN)VA) at build time --
 * no address literal and no carrier type ever appears in src/ itself.
 * Re-run gen_bindings.py to regenerate; do not hand-edit.
 */

#ifndef PF_BINDINGS_H
#define PF_BINDINGS_H

#define ICYTOWER_BINDINGS_ACTIVE 1  /* purity-safe "bindings are active" signal for src/ */

#include "pf_bindings_types.h"  /* struct/enum/typedef layouts */
#include "it_funcs.h"           /* PFN_<name> typedefs, reused verbatim */

/* ------------------------------------------------------------------ */
/* CRT functions the GUEST imports: call through the guest IAT slot,   */
/* not the carrier's own linked-in CRT (divergence 008 -- see          */
/* gen_bindings.py's docstring and notes/living_record.md).            */
/* ------------------------------------------------------------------ */
#include <stdlib.h>  /* pulled in FIRST: the macros below must rewrite calls in src/, never this header's own declarations */

/* rand  -> msvcrt.dll!rand IAT slot VA=0x00514944  (the original's own `call _rand -> jmp *[slot]`) */
typedef int (__cdecl *PFN_crt_rand)(void);
#define rand (*(PFN_crt_rand *)0x00514944)
/* srand  -> msvcrt.dll!srand IAT slot VA=0x0051495c  (the original's own `call _srand -> jmp *[slot]`) */
typedef void (__cdecl *PFN_crt_srand)(unsigned);
#define srand (*(PFN_crt_srand *)0x0051495c)

/* ------------------------------------------------------------------ */
/* globals: <name> -> (*(T*)VA), identical to it_globals.h IT_G_<name> */
/* ------------------------------------------------------------------ */

/* REPLAY_HEADER  VA=0x4d7dd0  type=const char [6]  cu=F:\projects\icytower\trunk\source\replay.c */
#define REPLAY_HEADER (*(const char (*)[6])0x4d7dd0)
/* _png_compression_level  VA=0x4bc010  type=int  cu=F:\projects\icytower\trunk\source\loadpng.c */
#define _png_compression_level (*(int *)0x4bc010)
/* _png_screen_gamma  VA=0x4bc008  type=double  cu=F:\projects\icytower\trunk\source\loadpng.c */
#define _png_screen_gamma (*(double *)0x4bc008)
/* abb_month  VA=0x4bde00  type=const char *[13]  cu=F:\projects\icytower\trunk\source\strptime.c */
#define abb_month (*(const char *(*)[13])0x4bde00)
/* abb_weekdays  VA=0x4bdda0  type=const char *[8]  cu=F:\projects\icytower\trunk\source\strptime.c */
#define abb_weekdays (*(const char *(*)[8])0x4bdda0)
/* ampm  VA=0x4bde34  type=const char *[3]  cu=F:\projects\icytower\trunk\source\strptime.c */
#define ampm (*(const char *(*)[3])0x4bde34)
/* any11  VA=0x4dd170  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define any11 (*(int *)0x4dd170)
/* any12  VA=0x4dd174  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define any12 (*(int *)0x4dd174)
/* any13  VA=0x4dd178  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define any13 (*(int *)0x4dd178)
/* any21  VA=0x4dd17c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define any21 (*(int *)0x4dd17c)
/* any22  VA=0x4dd180  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define any22 (*(int *)0x4dd180)
/* any23  VA=0x4dd184  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define any23 (*(int *)0x4dd184)
/* bg_beat  VA=0x4dd2a8  type=SAMPLE *  cu=F:\projects\icytower\trunk\source\main.c */
#define bg_beat (*(SAMPLE **)0x4dd2a8)
/* bg_menu  VA=0x4dd2ac  type=SAMPLE *  cu=F:\projects\icytower\trunk\source\main.c */
#define bg_menu (*(SAMPLE **)0x4dd2ac)
/* bg_stripe_ids  VA=0x4dd19c  type=int [5]  cu=F:\projects\icytower\trunk\source\main.c */
#define bg_stripe_ids (*(int (*)[5])0x4dd19c)
/* black  VA=0x4dd008  type=RGB  cu=F:\projects\icytower\trunk\source\custom.c */
#define black (*(RGB *)0x4dd008)
/* blit_mode__blit_to_screen  VA=0x4dd324  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define blit_mode__blit_to_screen (*(int *)0x4dd324)
/* category_names  VA=0x4bc080  type=char *[15]  cu=F:\projects\icytower\trunk\source\main.c */
#define category_names (*(char *(*)[15])0x4bc080)
/* characters  VA=0x4fdcc8  type=Tcharacter *  cu=F:\projects\icytower\trunk\source\main.c */
#define characters (*(Tcharacter **)0x4fdcc8)
/* checkMusicVoiceID  VA=0x4bc174  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define checkMusicVoiceID (*(int *)0x4bc174)
/* clock_angle  VA=0x4dd248  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define clock_angle (*(int *)0x4dd248)
/* closeButtonClicked  VA=0x4dd264  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define closeButtonClicked (*(int *)0x4dd264)
/* cmdline  VA=0x4dd14c  type=Tcommandline  cu=F:\projects\icytower\trunk\source\main.c */
#define cmdline (*(Tcommandline *)0x4dd14c)
/* collision_type  VA=0x4dd140  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define collision_type (*(int *)0x4dd140)
/* comboNames  VA=0x4bdd20  type=char *[10]  cu=F:\projects\icytower\trunk\source\profile.c */
#define comboNames (*(char *(*)[10])0x4bdd20)
/* combo_sound  VA=0x4dd280  type=SAMPLE *[10]  cu=F:\projects\icytower\trunk\source\main.c */
#define combo_sound (*(SAMPLE *(*)[10])0x4dd280)
/* count__load_character  VA=0x4dd330  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define count__load_character (*(int *)0x4dd330)
/* count__main_menu_callback  VA=0x4dd318  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define count__main_menu_callback (*(int *)0x4dd318)
/* ctrl  VA=0x5000c8  type=Tcontrol  cu=F:\projects\icytower\trunk\source\main.c */
#define ctrl (*(Tcontrol *)0x5000c8)
/* ctrl_menu  VA=0x4bc1c0  type=Tmenu [6]  cu=F:\projects\icytower\trunk\source\main.c */
#define ctrl_menu (*(Tmenu (*)[6])0x4bc1c0)
/* curr_char  VA=0x4dd270  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define curr_char (*(int *)0x4dd270)
/* custom  VA=0x4fa738  type=Tcustom  cu=F:\projects\icytower\trunk\source\main.c */
#define custom (*(Tcustom *)0x4fa738)
/* custom_menu  VA=0x4bcec0  type=Tmenu [5]  cu=F:\projects\icytower\trunk\source\main.c */
#define custom_menu (*(Tmenu (*)[5])0x4bcec0)
/* cycle_count  VA=0x506938  type=volatile int  cu=F:\projects\icytower\trunk\source\timer.c */
#define cycle_count (*(volatile int *)0x506938)
/* cycle_loops  VA=0x4dd24c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define cycle_loops (*(int *)0x4dd24c)
/* data  VA=0x4dd23c  type=DATAFILE *  cu=F:\projects\icytower\trunk\source\main.c */
#define data (*(DATAFILE **)0x4dd23c)
/* debug  VA=0x4dd160  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define debug (*(int *)0x4dd160)
/* demo  VA=0x4dd250  type=Treplay *  cu=F:\projects\icytower\trunk\source\main.c */
#define demo (*(Treplay **)0x4dd250)
/* dropped_file_is_not_a_replay  VA=0x4dd16c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define dropped_file_is_not_a_replay (*(int *)0x4dd16c)
/* eyecandy_selection  VA=0x4fa278  type=Tmenu_selection  cu=F:\projects\icytower\trunk\source\main.c */
#define eyecandy_selection (*(Tmenu_selection *)0x4fa278)
/* face__main_menu_callback  VA=0x4dd31c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define face__main_menu_callback (*(int *)0x4dd31c)
/* fall_count  VA=0x4dd244  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define fall_count (*(int *)0x4dd244)
/* fast_fast_forward  VA=0x4dd258  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define fast_fast_forward (*(int *)0x4dd258)
/* fast_forward  VA=0x4dd254  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define fast_forward (*(int *)0x4dd254)
/* excluded by --exclude: floor_size_modifiers */
/* floor_size_selection  VA=0x4fe7b8  type=Tmenu_selection  cu=F:\projects\icytower\trunk\source\main.c */
#define floor_size_selection (*(Tmenu_selection *)0x4fe7b8)
/* floors  VA=0x4dd304  type=Tmenu_floor_selection  cu=F:\projects\icytower\trunk\source\main.c */
#define floors (*(Tmenu_floor_selection *)0x4dd304)
/* fps  VA=0x506948  type=volatile int  cu=F:\projects\icytower\trunk\source\timer.c */
#define fps (*(volatile int *)0x506948)
/* frame_count  VA=0x506978  type=volatile int  cu=F:\projects\icytower\trunk\source\timer.c */
#define frame_count (*(volatile int *)0x506978)
/* full_month  VA=0x4bddc0  type=const char *[13]  cu=F:\projects\icytower\trunk\source\strptime.c */
#define full_month (*(const char *(*)[13])0x4bddc0)
/* full_weekdays  VA=0x4bdd80  type=const char *[8]  cu=F:\projects\icytower\trunk\source\strptime.c */
#define full_weekdays (*(const char *(*)[8])0x4bdd80)
/* gFLDADMutex  VA=0x4bc004  type=pthread_mutex_t  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define gFLDADMutex (*(pthread_mutex_t *)0x4bc004)
/* gFLDADThread  VA=0x4f87d8  type=pthread_t  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define gFLDADThread (*(pthread_t *)0x4f87d8)
/* gameData  VA=0x4dd260  type=Tgame_data *  cu=F:\projects\icytower\trunk\source\main.c */
#define gameData (*(Tgame_data **)0x4dd260)
/* gameMusicVoiceID  VA=0x4bc178  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define gameMusicVoiceID (*(int *)0x4bc178)
/* game_menu  VA=0x4bca00  type=Tmenu [1]  cu=F:\projects\icytower\trunk\source\main.c */
#define game_menu (*(Tmenu (*)[1])0x4bca00)
/* gameover_bmp  VA=0x5000f8  type=BITMAP *  cu=F:\projects\icytower\trunk\source\main.c */
#define gameover_bmp (*(BITMAP **)0x5000f8)
/* gamepad  VA=0x4f8748  type=Tgamepad  cu=F:\projects\icytower\trunk\source\control.c */
#define gamepad (*(Tgamepad *)0x4f8748)
/* gdComboStart  VA=0x4dd190  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define gdComboStart (*(int *)0x4dd190)
/* gdLastJumpDiff  VA=0x4dd18c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define gdLastJumpDiff (*(int *)0x4dd18c)
/* gfx_menu  VA=0x4bc700  type=Tmenu [5]  cu=F:\projects\icytower\trunk\source\main.c */
#define gfx_menu (*(Tmenu (*)[5])0x4bc700)
/* giAdCacheSize  VA=0x4dd020  type=int  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define giAdCacheSize (*(int *)0x4dd020)
/* got_joystick  VA=0x4f8b08  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define got_joystick (*(int *)0x4f8b08)
/* gpAdCache  VA=0x4dd024  type=FLDAdSpot *  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define gpAdCache (*(FLDAdSpot **)0x4dd024)
/* gravity_modifier  VA=0x4bdba8  type=double [3]  cu=F:\projects\icytower\trunk\source\player.c */
#define gravity_modifier (*(double (*)[3])0x4bdba8)
/* gravity_selection  VA=0x4fac38  type=Tmenu_selection  cu=F:\projects\icytower\trunk\source\main.c */
#define gravity_selection (*(Tmenu_selection *)0x4fac38)
/* greeting_scroller  VA=0x4fdce8  type=Tscroller  cu=F:\projects\icytower\trunk\source\main.c */
#define greeting_scroller (*(Tscroller *)0x4fdce8)
/* hasFocus  VA=0x4bc020  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define hasFocus (*(int *)0x4bc020)
/* hints  VA=0x4bc0c0  type=char *[45]  cu=F:\projects\icytower\trunk\source\main.c */
#define hints (*(char *(*)[45])0x4bc0c0)
/* hisc_names  VA=0x4bc040  type=char *[15]  cu=F:\projects\icytower\trunk\source\main.c */
#define hisc_names (*(char *(*)[15])0x4bc040)
/* hisc_tables  VA=0x4dd1c0  type=Thisc_table *[15]  cu=F:\projects\icytower\trunk\source\main.c */
#define hisc_tables (*(Thisc_table *(*)[15])0x4dd1c0)
/* hurry_y  VA=0x4ff118  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define hurry_y (*(int *)0x4ff118)
/* in_replay_menu  VA=0x4dd314  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define in_replay_menu (*(int *)0x4dd314)
/* init_ok  VA=0x4dd164  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define init_ok (*(int *)0x4dd164)
/* init_string  VA=0x4bdb3c  type=char [7]  cu=F:\projects\icytower\trunk\source\main.c */
#define init_string (*(char (*)[7])0x4bdb3c)
/* is_playing_custom_game  VA=0x4dd188  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define is_playing_custom_game (*(int *)0x4dd188)
/* itr_file_list  VA=0x500938  type=Treplay_post [1024]  cu=F:\projects\icytower\trunk\source\replay.c */
#define itr_file_list (*(Treplay_post (*)[1024])0x500938)
/* itrcheck  VA=0x4dd168  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define itrcheck (*(int *)0x4dd168)
/* jcLabels  VA=0x4bdbc0  type=char *[5]  cu=F:\projects\icytower\trunk\source\profile.c */
#define jcLabels (*(char *(*)[5])0x4bdbc0)
/* jumpSequence  VA=0x4fa728  type=Tgd_jump_sequence  cu=F:\projects\icytower\trunk\source\main.c */
#define jumpSequence (*(Tgd_jump_sequence *)0x4fa728)
/* SKIPPED: "jump_sound" collides with a struct/union member name elsewhere in scope -- a plain #define would also rewrite that member access (e.g. `x.jump_sound`); see MEMBER_ACCESS_COLLISIONS in gen_bindings.py and PROMOTIONS.md batch 7 */
/* lastFocus  VA=0x4bc024  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define lastFocus (*(int *)0x4bc024)
/* lastMouseB  VA=0x4dd268  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define lastMouseB (*(int *)0x4dd268)
/* last_log  VA=0x4f89e8  type=char [256]  cu=F:\projects\icytower\trunk\source\main.c */
#define last_log (*(char (*)[256])0x4f89e8)
/* last_stripe_y  VA=0x4fa308  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define last_stripe_y (*(int *)0x4fa308)
/* localFilename__fldads_get_local_cache_name  VA=0x4dd040  type=char [256]  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define localFilename__fldads_get_local_cache_name (*(char (*)[256])0x4dd040)
/* logfilename__log2file  VA=0x4dd340  type=char [1024]  cu=F:\projects\icytower\trunk\source\main.c */
#define logfilename__log2file (*(char (*)[1024])0x4dd340)
/* logic_count  VA=0x506958  type=volatile int  cu=F:\projects\icytower\trunk\source\timer.c */
#define logic_count (*(volatile int *)0x506958)
/* lps  VA=0x506968  type=volatile int  cu=F:\projects\icytower\trunk\source\timer.c */
#define lps (*(volatile int *)0x506968)
/* main_menu  VA=0x4bd380  type=Tmenu [7]  cu=F:\projects\icytower\trunk\source\main.c */
#define main_menu (*(Tmenu (*)[7])0x4bd380)
/* map  VA=0x4f8b18  type=Tmap  cu=F:\projects\icytower\trunk\source\main.c */
#define map (*(Tmap *)0x4f8b18)
/* max_speed  VA=0x4bdb80  type=double [5]  cu=F:\projects\icytower\trunk\source\player.c */
#define max_speed (*(double (*)[5])0x4bdb80)
/* menu_params  VA=0x4f8e38  type=Tmenu_params  cu=F:\projects\icytower\trunk\source\main.c */
#define menu_params (*(Tmenu_params *)0x4f8e38)
/* menu_sounds  VA=0x4dd2c8  type=SAMPLE *[2]  cu=F:\projects\icytower\trunk\source\main.c */
#define menu_sounds (*(SAMPLE *(*)[2])0x4dd2c8)
/* msc_volume_slider  VA=0x4bc1a8  type=Tmenu_slider  cu=F:\projects\icytower\trunk\source\main.c */
#define msc_volume_slider (*(Tmenu_slider *)0x4bc1a8)
/* new_personal_best  VA=0x4dd200  type=int [15]  cu=F:\projects\icytower\trunk\source\main.c */
#define new_personal_best (*(int (*)[15])0x4dd200)
/* numProfiles  VA=0x4dd278  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define numProfiles (*(int *)0x4dd278)
/* num_chars  VA=0x4dd26c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define num_chars (*(int *)0x4dd26c)
/* num_itr_files  VA=0x4dd744  type=int  cu=F:\projects\icytower\trunk\source\replay.c */
#define num_itr_files (*(int *)0x4dd744)
/* number__take_screenshot  VA=0x4dd334  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define number__take_screenshot (*(int *)0x4dd334)
/* opt_menu  VA=0x4bcc60  type=Tmenu [4]  cu=F:\projects\icytower\trunk\source\main.c */
#define opt_menu (*(Tmenu (*)[4])0x4bcc60)
/* options  VA=0x4fe528  type=Toptions  cu=F:\projects\icytower\trunk\source\main.c */
#define options (*(Toptions *)0x4fe528)
/* pFLDAd  VA=0x4dd310  type=const FLDAdSpot *  cu=F:\projects\icytower\trunk\source\main.c */
#define pFLDAd (*(const FLDAdSpot **)0x4dd310)
/* pFLDAdBitmap  VA=0x4dd30c  type=BITMAP *  cu=F:\projects\icytower\trunk\source\main.c */
#define pFLDAdBitmap (*(BITMAP **)0x4dd30c)
/* p__datafile_callback_slow  VA=0x4dd328  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define p__datafile_callback_slow (*(int *)0x4dd328)
/* pink  VA=0x4bc000  type=RGB  cu=F:\projects\icytower\trunk\source\custom.c */
#define pink (*(RGB *)0x4bc000)
/* play_char  VA=0x4fa318  type=Tmenu_char_selection  cu=F:\projects\icytower\trunk\source\main.c */
#define play_char (*(Tmenu_char_selection *)0x4fa318)
/* play_menu  VA=0x4bd1c0  type=Tmenu [3]  cu=F:\projects\icytower\trunk\source\main.c */
#define play_menu (*(Tmenu (*)[3])0x4bd1c0)
/* player_id  VA=0x4fe518  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define player_id (*(int *)0x4fe518)
/* ply  VA=0x4ff128  type=Tplayer *[1000]  cu=F:\projects\icytower\trunk\source\main.c */
#define ply (*(Tplayer *(*)[1000])0x4ff128)
/* poster  VA=0x4dd198  type=BITMAP *  cu=F:\projects\icytower\trunk\source\main.c */
#define poster (*(BITMAP **)0x4dd198)
/* profile  VA=0x4dd27c  type=Tprofile *  cu=F:\projects\icytower\trunk\source\main.c */
#define profile (*(Tprofile **)0x4dd27c)
/* profile_menu  VA=0x4bcaa0  type=Tmenu [3]  cu=F:\projects\icytower\trunk\source\main.c */
#define profile_menu (*(Tmenu (*)[3])0x4bcaa0)
/* profiles  VA=0x4dd274  type=Tavailable_profile *  cu=F:\projects\icytower\trunk\source\main.c */
#define profiles (*(Tavailable_profile **)0x4dd274)
/* rankCCCs  VA=0x4bdca0  type=int [12]  cu=F:\projects\icytower\trunk\source\profile.c */
#define rankCCCs (*(int (*)[12])0x4bdca0)
/* rankCombos  VA=0x4bdc60  type=int [12]  cu=F:\projects\icytower\trunk\source\profile.c */
#define rankCombos (*(int (*)[12])0x4bdc60)
/* rankFloors  VA=0x4bdc20  type=int [12]  cu=F:\projects\icytower\trunk\source\profile.c */
#define rankFloors (*(int (*)[12])0x4bdc20)
/* rankLables  VA=0x4bdbe0  type=char *[12]  cu=F:\projects\icytower\trunk\source\profile.c */
#define rankLables (*(char *(*)[12])0x4bdbe0)
/* rankNMLs  VA=0x4bdce0  type=int [12]  cu=F:\projects\icytower\trunk\source\profile.c */
#define rankNMLs (*(int (*)[12])0x4bdce0)
/* rec_pos  VA=0x4fec58  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define rec_pos (*(int *)0x4fec58)
/* rec_seed  VA=0x4fe7a8  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define rec_seed (*(int *)0x4fe7a8)
/* recording  VA=0x4f8e28  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define recording (*(int *)0x4f8e28)
/* rejump  VA=0x4fdcd8  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define rejump (*(int *)0x4fdcd8)
/* replay_directory  VA=0x4fe848  type=char [1024]  cu=F:\projects\icytower\trunk\source\main.c */
#define replay_directory (*(char (*)[1024])0x4fe848)
/* replay_menu  VA=0x4bd7a0  type=Tmenu [5]  cu=F:\projects\icytower\trunk\source\main.c */
#define replay_menu (*(Tmenu (*)[5])0x4bd7a0)
/* reward_bmp  VA=0x4f8af8  type=BITMAP *  cu=F:\projects\icytower\trunk\source\main.c */
#define reward_bmp (*(BITMAP **)0x4f8af8)
/* reward_scale  VA=0x4fac28  type=fixed  cu=F:\projects\icytower\trunk\source\main.c */
#define reward_scale (*(fixed *)0x4fac28)
/* reward_time  VA=0x4fec68  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define reward_time (*(int *)0x4fec68)
/* sLogMutex__log2file  VA=0x4bdb44  type=pthread_mutex_t  cu=F:\projects\icytower\trunk\source\main.c */
#define sLogMutex__log2file (*(pthread_mutex_t *)0x4bdb44)
/* scroll_count  VA=0x4fec48  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define scroll_count (*(int *)0x4fec48)
/* scroll_delay  VA=0x4f8ae8  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define scroll_delay (*(int *)0x4f8ae8)
/* scroll_speed_selection  VA=0x4fec78  type=Tmenu_selection  cu=F:\projects\icytower\trunk\source\main.c */
#define scroll_speed_selection (*(Tmenu_selection *)0x4fec78)
/* scroller_greetings  VA=0x4bdaa0  type=char [156]  cu=F:\projects\icytower\trunk\source\main.c */
#define scroller_greetings (*(char (*)[156])0x4bdaa0)
/* seed  VA=0x4ff108  type=double  cu=F:\projects\icytower\trunk\source\main.c */
#define seed (*(double *)0x4ff108)
/* sfx  VA=0x4dd240  type=DATAFILE *  cu=F:\projects\icytower\trunk\source\main.c */
#define sfx (*(DATAFILE **)0x4dd240)
/* sfx_file  VA=0x4f87e8  type=char [512]  cu=F:\projects\icytower\trunk\source\main.c */
#define sfx_file (*(char (*)[512])0x4f87e8)
/* snd_menu  VA=0x4bc540  type=Tmenu [3]  cu=F:\projects\icytower\trunk\source\main.c */
#define snd_menu (*(Tmenu (*)[3])0x4bc540)
/* snd_volume_slider  VA=0x4bc198  type=Tmenu_slider  cu=F:\projects\icytower\trunk\source\main.c */
#define snd_volume_slider (*(Tmenu_slider *)0x4bc198)
/* someCounter__play  VA=0x4dd320  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define someCounter__play (*(int *)0x4dd320)
/* sort_method  VA=0x4bdd60  type=int  cu=F:\projects\icytower\trunk\source\replay.c */
#define sort_method (*(int *)0x4bdd60)
/* sounds  VA=0x4dd2e0  type=SAMPLE *[9]  cu=F:\projects\icytower\trunk\source\main.c */
#define sounds (*(SAMPLE *(*)[9])0x4dd2e0)
/* speaker  VA=0x4dd2bc  type=SAMPLE *[3]  cu=F:\projects\icytower\trunk\source\main.c */
#define speaker (*(SAMPLE *(*)[3])0x4dd2bc)
/* stars  VA=0x4facc8  type=Tparticle [512]  cu=F:\projects\icytower\trunk\source\main.c */
#define stars (*(Tparticle (*)[512])0x4facc8)
/* start_speeds  VA=0x4bc17c  type=int [6]  cu=F:\projects\icytower\trunk\source\main.c */
#define start_speeds (*(int (*)[6])0x4bc17c)
/* stepIn  VA=0x4dd740  type=int  cu=F:\projects\icytower\trunk\source\menu.c */
#define stepIn (*(int *)0x4dd740)
/* summary_scroller  VA=0x500108  type=Tscroller  cu=F:\projects\icytower\trunk\source\main.c */
#define summary_scroller (*(Tscroller *)0x500108)
/* summary_scroller_message  VA=0x4f8e78  type=char [5120]  cu=F:\projects\icytower\trunk\source\main.c */
#define summary_scroller_message (*(char (*)[5120])0x4f8e78)
/* swap_screen  VA=0x4dd194  type=BITMAP *  cu=F:\projects\icytower\trunk\source\main.c */
#define swap_screen (*(BITMAP **)0x4dd194)
/* testers  VA=0x4dd144  type=Tbeta *  cu=F:\projects\icytower\trunk\source\main.c */
#define testers (*(Tbeta **)0x4dd144)
/* the_tester  VA=0x4dd148  type=Tbeta *  cu=F:\projects\icytower\trunk\source\main.c */
#define the_tester (*(Tbeta **)0x4dd148)
/* tm_year_base  VA=0x4d80cc  type=const int  cu=F:\projects\icytower\trunk\source\strptime.c */
#define tm_year_base (*(const int *)0x4d80cc)
/* uberChecksum  VA=0x4dd25c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define uberChecksum (*(int *)0x4dd25c)
/* value__draw_progress_bar  VA=0x4dd32c  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define value__draw_progress_bar (*(int *)0x4dd32c)
/* version_str  VA=0x4bc194  type=char *  cu=F:\projects\icytower\trunk\source\main.c */
#define version_str (*(char **)0x4bc194)
/* window  VA=0x4bc028  type=int  cu=F:\projects\icytower\trunk\source\main.c */
#define window (*(int *)0x4bc028)
/* working_directory  VA=0x4fed08  type=char [1024]  cu=F:\projects\icytower\trunk\source\main.c */
#define working_directory (*(char (*)[1024])0x4fed08)

/* ------------------------------------------------------------------ */
/* functions: <name> -> ((PFN_<name>)VA), identical to it_funcs.h      */
/* IT_F_<name>. A name in --exclude is omitted so its own native       */
/* definition in src/ is not redirected to the original address.       */
/* ------------------------------------------------------------------ */

/* HTTPFetchInternal  VA=0x405e9c  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: HTTPResponse * HTTPFetchInternal(const char *, int, const char *, const char *) */
#define HTTPFetchInternal ((PFN_HTTPFetchInternal)0x405e9c)
/* HTTPGet  VA=0x406194  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: HTTPResponse * HTTPGet(const char *) */
#define HTTPGet ((PFN_HTTPGet)0x406194)
/* HTTPHead  VA=0x406178  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: HTTPResponse * HTTPHead(const char *) */
#define HTTPHead ((PFN_HTTPHead)0x406178)
/* HTTPRequest  VA=0x4060f0  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: HTTPResponse * HTTPRequest(const char *, const char *) */
#define HTTPRequest ((PFN_HTTPRequest)0x4060f0)
/* SplitURL  VA=0x405984  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: int SplitURL(const char *, char **, char **, int *) */
#define SplitURL ((PFN_SplitURL)0x405984)
/* WinMain  VA=0x406cb0  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int WinMain(void *, void *, char *, int) */
#define WinMain ((PFN_WinMain)0x406cb0)
/* _mangled_main  VA=0x415f10  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int _mangled_main(int, char **) */
#define _mangled_main ((PFN__mangled_main)0x415f10)
/* _strptime  VA=0x41f640  cu=F:\projects\icytower\trunk\source\strptime.c */
/* prototype: char * _strptime(const char *, const char *, struct it_orig_tm *, int *) */
#define _strptime ((PFN__strptime)0x41f640)
/* excluded by --exclude (compiled natively): add_combo */
/* excluded by --exclude (compiled natively): add_floor */
/* add_itr_file  VA=0x41e740  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: int add_itr_file(const char *, int, void *) */
#define add_itr_file ((PFN_add_itr_file)0x41e740)
/* excluded by --exclude (compiled natively): add_jump_sequence */
/* add_profile  VA=0x40c8c0  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int add_profile(const char *, int, void *) */
#define add_profile ((PFN_add_profile)0x40c8c0)
/* excluded by --exclude (compiled natively): blit_to_screen */
/* build_menu_string  VA=0x4174dc  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: void build_menu_string(Tmenu *, char *) */
#define build_menu_string ((PFN_build_menu_string)0x4174dc)
/* calc_replay_checksum  VA=0x41bac4  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: int calc_replay_checksum(Treplay *) */
#define calc_replay_checksum ((PFN_calc_replay_checksum)0x41bac4)
/* calc_replay_checksum_131  VA=0x41ba10  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: int calc_replay_checksum_131(Treplay *) */
#define calc_replay_checksum_131 ((PFN_calc_replay_checksum_131)0x41ba10)
/* change_profile  VA=0x40e1cc  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void change_profile() */
#define change_profile ((PFN_change_profile)0x40e1cc)
/* checkMenuFocus  VA=0x406f78  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void checkMenuFocus() */
#define checkMenuFocus ((PFN_checkMenuFocus)0x406f78)
/* check_beta_tester  VA=0x40e580  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int check_beta_tester() */
#define check_beta_tester ((PFN_check_beta_tester)0x40e580)
/* check_characters  VA=0x40e680  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int check_characters() */
#define check_characters ((PFN_check_characters)0x40e680)
/* excluded by --exclude (compiled natively): check_control_key */
/* check_dir  VA=0x40ffc4  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int check_dir(const char *, int, void *) */
#define check_dir ((PFN_check_dir)0x40ffc4)
/* clear_trailing_whitespace  VA=0x402078  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: void clear_trailing_whitespace(char *) */
#define clear_trailing_whitespace ((PFN_clear_trailing_whitespace)0x402078)
/* excluded by --exclude (compiled natively): clickedCloseButton */
/* color_map_callback  VA=0x407c20  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void color_map_callback(int) */
#define color_map_callback ((PFN_color_map_callback)0x407c20)
/* create_game_data  VA=0x404198  cu=F:\projects\icytower\trunk\source\game_data.c */
/* prototype: Tgame_data * create_game_data() */
#define create_game_data ((PFN_create_game_data)0x404198)
/* excluded by --exclude (compiled natively): create_particle */
/* create_post  VA=0x4014ec  cu=F:\projects\icytower\trunk\source\beta.c */
/* prototype: Tbeta * create_post() */
#define create_post ((PFN_create_post)0x4014ec)
/* create_profile  VA=0x41a988  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: Tprofile * create_profile(char *, int) */
#define create_profile ((PFN_create_profile)0x41a988)
/* create_replay  VA=0x41cce8  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: Treplay * create_replay(int) */
#define create_replay ((PFN_create_replay)0x41cce8)
/* csv_add_field  VA=0x401a98  cu=F:\projects\icytower\trunk\source\csv.c */
/* prototype: void csv_add_field(CSVParseContext *, char *) */
#define csv_add_field ((PFN_csv_add_field)0x401a98)
/* csv_begin  VA=0x401bf0  cu=F:\projects\icytower\trunk\source\csv.c */
/* prototype: CSVParseContext * csv_begin(const unsigned char *, it_orig_size_t) */
#define csv_begin ((PFN_csv_begin)0x401bf0)
/* csv_destroy  VA=0x401bb8  cu=F:\projects\icytower\trunk\source\csv.c */
/* prototype: void csv_destroy(CSVParseContext *) */
#define csv_destroy ((PFN_csv_destroy)0x401bb8)
/* csv_next  VA=0x401af0  cu=F:\projects\icytower\trunk\source\csv.c */
/* prototype: int csv_next(CSVParseContext *) */
#define csv_next ((PFN_csv_next)0x401af0)
/* csv_open  VA=0x401c5c  cu=F:\projects\icytower\trunk\source\csv.c */
/* prototype: CSVParseContext * csv_open(const char *) */
#define csv_open ((PFN_csv_open)0x401c5c)
/* csv_rewind  VA=0x401a80  cu=F:\projects\icytower\trunk\source\csv.c */
/* prototype: void csv_rewind(CSVParseContext *) */
#define csv_rewind ((PFN_csv_rewind)0x401a80)
/* custom_alert  VA=0x4027f0  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: void custom_alert(char *, char *) */
#define custom_alert ((PFN_custom_alert)0x4027f0)
/* excluded by --exclude (compiled natively): cycle_counter */
/* datafile_callback  VA=0x407c14  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void datafile_callback(DATAFILE *) */
#define datafile_callback ((PFN_datafile_callback)0x407c14)
/* datafile_callback_slow  VA=0x407bf0  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void datafile_callback_slow(DATAFILE *) */
#define datafile_callback_slow ((PFN_datafile_callback_slow)0x407bf0)
/* delete_profile  VA=0x41a8a8  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: void delete_profile(char *) */
#define delete_profile ((PFN_delete_profile)0x41a8a8)
/* destroyHTTPResponse  VA=0x405910  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: void destroyHTTPResponse(HTTPResponse *) */
#define destroyHTTPResponse ((PFN_destroyHTTPResponse)0x405910)
/* destroy_all  VA=0x4014b0  cu=F:\projects\icytower\trunk\source\beta.c */
/* prototype: void destroy_all(Tbeta *) */
#define destroy_all ((PFN_destroy_all)0x4014b0)
/* destroy_custom_data  VA=0x401dd8  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: int destroy_custom_data(Tcustom *) */
#define destroy_custom_data ((PFN_destroy_custom_data)0x401dd8)
/* destroy_datafile_png  VA=0x41b8e4  cu=F:\projects\icytower\trunk\source\regpng.c */
/* prototype: void destroy_datafile_png(void *) */
#define destroy_datafile_png ((PFN_destroy_datafile_png)0x41b8e4)
/* destroy_game_data  VA=0x40418c  cu=F:\projects\icytower\trunk\source\game_data.c */
/* prototype: void destroy_game_data(Tgame_data *) */
#define destroy_game_data ((PFN_destroy_game_data)0x40418c)
/* destroy_hisc_table  VA=0x405818  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: void destroy_hisc_table(Thisc_table *) */
#define destroy_hisc_table ((PFN_destroy_hisc_table)0x405818)
/* destroy_replay  VA=0x41bd68  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: void destroy_replay(Treplay *) */
#define destroy_replay ((PFN_destroy_replay)0x41bd68)
/* do_replay_menu  VA=0x410f98  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int do_replay_menu() */
#define do_replay_menu ((PFN_do_replay_menu)0x410f98)
/* drawSlot  VA=0x406fb4  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void drawSlot(BITMAP *, int, int, char *, char *, int) */
#define drawSlot ((PFN_drawSlot)0x406fb4)
/* excluded by --exclude (compiled natively): draw_buffer */
/* excluded by --exclude (compiled natively): draw_frame */
/* draw_menu  VA=0x41767c  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: void draw_menu(BITMAP *, Tmenu *, Tmenu_params *, int, int, int) */
#define draw_menu ((PFN_draw_menu)0x41767c)
/* draw_profile_selector  VA=0x418cd4  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: void draw_profile_selector(BITMAP *, Tprofile *, Tavailable_profile *, int, int, int, int, int, int) */
#define draw_profile_selector ((PFN_draw_profile_selector)0x418cd4)
/* draw_progress_bar  VA=0x407a08  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void draw_progress_bar() */
#define draw_progress_bar ((PFN_draw_progress_bar)0x407a08)
/* draw_replay_selector  VA=0x41be58  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: void draw_replay_selector(BITMAP *, Treplay *, Treplay_post *, int, int, int, int, int) */
#define draw_replay_selector ((PFN_draw_replay_selector)0x41be58)
/* draw_results  VA=0x4076c0  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void draw_results(BITMAP *, BITMAP *, int, int *, int *, int) */
#define draw_results ((PFN_draw_results)0x4076c0)
/* draw_reward  VA=0x4070fc  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void draw_reward(BITMAP *) */
#define draw_reward ((PFN_draw_reward)0x4070fc)
/* excluded by --exclude (compiled natively): draw_scroller */
/* excluded by --exclude (compiled natively): draw_star_field */
/* draw_table  VA=0x404a7c  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: int draw_table(BITMAP *, int, int, char *, Thisc_table *) */
#define draw_table ((PFN_draw_table)0x404a7c)
/* dumpHTTPResponse  VA=0x405e2c  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: void dumpHTTPResponse(it_orig_FILE *, HTTPResponse *) */
#define dumpHTTPResponse ((PFN_dumpHTTPResponse)0x405e2c)
/* end_game  VA=0x40e110  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void end_game() */
#define end_game ((PFN_end_game)0x40e110)
/* enter_hisc_table  VA=0x405790  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: void enter_hisc_table(Thisc_table *, int, char *) */
#define enter_hisc_table ((PFN_enter_hisc_table)0x405790)
/* extractHTTPResponse  VA=0x405a90  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: HTTPResponse * extractHTTPResponse(const unsigned char *, int) */
#define extractHTTPResponse ((PFN_extractHTTPResponse)0x405a90)
/* fadeIn  VA=0x40c1c0  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void fadeIn(BITMAP *, int) */
#define fadeIn ((PFN_fadeIn)0x40c1c0)
/* fadeOut  VA=0x40bf5c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void fadeOut(int) */
#define fadeOut ((PFN_fadeOut)0x40bf5c)
/* first_day  VA=0x41f5c4  cu=F:\projects\icytower\trunk\source\strptime.c */
/* prototype: int first_day(int) */
#define first_day ((PFN_first_day)0x41f5c4)
/* fldads_destroy_cache  VA=0x403bb8  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void fldads_destroy_cache() */
#define fldads_destroy_cache ((PFN_fldads_destroy_cache)0x403bb8)
/* fldads_dump_local_cache  VA=0x403c84  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void fldads_dump_local_cache() */
#define fldads_dump_local_cache ((PFN_fldads_dump_local_cache)0x403c84)
/* fldads_get_local_cache_name  VA=0x403c44  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: const char * fldads_get_local_cache_name(const char *) */
#define fldads_get_local_cache_name ((PFN_fldads_get_local_cache_name)0x403c44)
/* fldads_get_local_filename_from_url  VA=0x403d4c  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: const char * fldads_get_local_filename_from_url(const char *) */
#define fldads_get_local_filename_from_url ((PFN_fldads_get_local_filename_from_url)0x403d4c)
/* fldads_get_random_ad  VA=0x403af8  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: const FLDAdSpot * fldads_get_random_ad() */
#define fldads_get_random_ad ((PFN_fldads_get_random_ad)0x403af8)
/* fldads_load_cache_from_csv  VA=0x403d68  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void fldads_load_cache_from_csv(CSVParseContext *) */
#define fldads_load_cache_from_csv ((PFN_fldads_load_cache_from_csv)0x403d68)
/* fldads_load_local_cache  VA=0x403e84  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void fldads_load_local_cache() */
#define fldads_load_local_cache ((PFN_fldads_load_local_cache)0x403e84)
/* fldads_start  VA=0x403ac8  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void fldads_start() */
#define fldads_start ((PFN_fldads_start)0x403ac8)
/* fldads_threadmain  VA=0x404014  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void * fldads_threadmain(void *) */
#define fldads_threadmain ((PFN_fldads_threadmain)0x404014)
/* fldads_update_cache  VA=0x403fa4  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void fldads_update_cache(unsigned char *, it_orig_size_t) */
#define fldads_update_cache ((PFN_fldads_update_cache)0x403fa4)
/* fldads_update_local_adimg  VA=0x403ebc  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: void fldads_update_local_adimg(const char *) */
#define fldads_update_local_adimg ((PFN_fldads_update_local_adimg)0x403ebc)
/* flush_data  VA=0x41e8a8  cu=F:\projects\icytower\trunk\source\savepng.c */
/* prototype: void flush_data(png_structp) */
#define flush_data ((PFN_flush_data)0x41e8a8)
/* for_each_directory  VA=0x40cbc0  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void for_each_directory(const char *, int (__cdecl *)(const char *, int, void *)) */
#define for_each_directory ((PFN_for_each_directory)0x40cbc0)
/* force_create_profile  VA=0x40d454  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void force_create_profile() */
#define force_create_profile ((PFN_force_create_profile)0x40d454)
/* excluded by --exclude (compiled natively): fps_counter */
/* garble_string  VA=0x401318  cu=F:\projects\icytower\trunk\source\beta.c */
/* prototype: void garble_string(char *, int) */
#define garble_string ((PFN_garble_string)0x401318)
/* generate_checksum  VA=0x404a50  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: int generate_checksum(Thisc *) */
#define generate_checksum ((PFN_generate_checksum)0x404a50)
/* generate_options_checksum  VA=0x4181cc  cu=F:\projects\icytower\trunk\source\options.c */
/* prototype: int generate_options_checksum(Toptions *) */
#define generate_options_checksum ((PFN_generate_options_checksum)0x4181cc)
/* generate_profile_checksum  VA=0x418a14  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: int generate_profile_checksum(Tprofile *) */
#define generate_profile_checksum ((PFN_generate_profile_checksum)0x418a14)
/* excluded by --exclude (compiled natively): getFloorData */
/* getGameDataXML  VA=0x404254  cu=F:\projects\icytower\trunk\source\game_data.c */
/* prototype: char * getGameDataXML(Tgame_data *) */
#define getGameDataXML ((PFN_getGameDataXML)0x404254)
/* getSampleFromOggDatafile  VA=0x40ca2c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: SAMPLE * getSampleFromOggDatafile(DATAFILE *, int) */
#define getSampleFromOggDatafile ((PFN_getSampleFromOggDatafile)0x40ca2c)
/* getSocketError  VA=0x405e90  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: int getSocketError() */
#define getSocketError ((PFN_getSocketError)0x405e90)
/* get_adcache_dir  VA=0x4039a4  cu=F:\projects\icytower\trunk\source\directories.c */
/* prototype: int get_adcache_dir(char *, it_orig_size_t) */
#define get_adcache_dir ((PFN_get_adcache_dir)0x4039a4)
/* get_character_dir  VA=0x403a84  cu=F:\projects\icytower\trunk\source\directories.c */
/* prototype: int get_character_dir(char *, it_orig_size_t, const char *) */
#define get_character_dir ((PFN_get_character_dir)0x403a84)
/* get_configfile_path  VA=0x4039cc  cu=F:\projects\icytower\trunk\source\directories.c */
/* prototype: int get_configfile_path(char *, it_orig_size_t) */
#define get_configfile_path ((PFN_get_configfile_path)0x4039cc)
/* excluded by --exclude (compiled natively): get_controls */
/* get_custom_characters_dir  VA=0x403994  cu=F:\projects\icytower\trunk\source\directories.c */
/* prototype: int get_custom_characters_dir(char *, it_orig_size_t) */
#define get_custom_characters_dir ((PFN_get_custom_characters_dir)0x403994)
/* excluded by --exclude (compiled natively): get_demo */
/* excluded by --exclude (compiled natively): get_gamepad */
/* get_gamepad_value  VA=0x40c984  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int get_gamepad_value(char *) */
#define get_gamepad_value ((PFN_get_gamepad_value)0x40c984)
/* excluded by --exclude (compiled natively): get_level */
/* get_logfile_path  VA=0x4039f4  cu=F:\projects\icytower\trunk\source\directories.c */
/* prototype: int get_logfile_path(char *, it_orig_size_t) */
#define get_logfile_path ((PFN_get_logfile_path)0x4039f4)
/* get_profile_dir_for_profile  VA=0x403a44  cu=F:\projects\icytower\trunk\source\directories.c */
/* prototype: int get_profile_dir_for_profile(char *, it_orig_size_t, const char *) */
#define get_profile_dir_for_profile ((PFN_get_profile_dir_for_profile)0x403a44)
/* get_profiles_dir  VA=0x403a1c  cu=F:\projects\icytower\trunk\source\directories.c */
/* prototype: int get_profiles_dir(char *, it_orig_size_t) */
#define get_profiles_dir ((PFN_get_profiles_dir)0x403a1c)
/* get_rank  VA=0x418ad0  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: char * get_rank(Tprofile *) */
#define get_rank ((PFN_get_rank)0x418ad0)
/* get_rank_id  VA=0x418a84  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: int get_rank_id(Tprofile *) */
#define get_rank_id ((PFN_get_rank_id)0x418a84)
/* get_replay_property  VA=0x41e244  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: int get_replay_property(const char *, int) */
#define get_replay_property ((PFN_get_replay_property)0x41e244)
/* get_selection_value  VA=0x416a6c  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: int get_selection_value(Tmenu_selection *) */
#define get_selection_value ((PFN_get_selection_value)0x416a6c)
/* get_slider_value  VA=0x416a3c  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: int get_slider_value(Tmenu_slider *) */
#define get_slider_value ((PFN_get_slider_value)0x416a3c)
/* get_sort_method  VA=0x41b9ac  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: int get_sort_method() */
#define get_sort_method ((PFN_get_sort_method)0x41b9ac)
/* get_string  VA=0x40bc44  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int get_string(BITMAP *, char *, int, int, FONT *, int, int, int, int) */
#define get_string ((PFN_get_string)0x40bc44)
/* get_string_data  VA=0x4020d4  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: char * get_string_data(char *, char *) */
#define get_string_data ((PFN_get_string_data)0x4020d4)
/* get_url_filename  VA=0x403d20  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
/* prototype: const char * get_url_filename(const char *) */
#define get_url_filename ((PFN_get_url_filename)0x403d20)
/* get_version_str  VA=0x406960  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: char * get_version_str() */
#define get_version_str ((PFN_get_version_str)0x406960)
/* handle_menu  VA=0x417d24  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: int handle_menu(Tmenu *, Tmenu_params *, Tcontrol *, BITMAP *, void (__cdecl *)(void), int, int, int) */
#define handle_menu ((PFN_handle_menu)0x417d24)
/* excluded by --exclude (compiled natively): handle_player_collision_combo */
/* excluded by --exclude (compiled natively): handle_player_collision_old */
/* excluded by --exclude (compiled natively): handle_player_collision_original */
/* excluded by --exclude (compiled natively): handle_player_collision_vector */
/* excluded by --exclude (compiled natively): handle_player_collision_vector_2 */
/* excluded by --exclude (compiled natively): handle_player_input */
/* hash  VA=0x41b9c8  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: unsigned int hash(unsigned int) */
#define hash ((PFN_hash)0x41b9c8)
/* hash2  VA=0x4189cc  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: unsigned int hash2(unsigned int) */
#define hash2 ((PFN_hash2)0x4189cc)
/* hash3  VA=0x418184  cu=F:\projects\icytower\trunk\source\options.c */
/* prototype: unsigned int hash3(unsigned int) */
#define hash3 ((PFN_hash3)0x418184)
/* httpGetLastModified  VA=0x405890  cu=F:\projects\icytower\trunk\source\httpget.c */
/* prototype: it_orig_time_t httpGetLastModified(HTTPResponse *) */
#define httpGetLastModified ((PFN_httpGetLastModified)0x405890)
/* excluded by --exclude (compiled natively): init_control */
/* init_custom  VA=0x401d28  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: int init_custom(Tcustom *, const char *, int) */
#define init_custom ((PFN_init_custom)0x401d28)
/* init_game  VA=0x40e7dc  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int init_game(int, char **) */
#define init_game ((PFN_init_game)0x40e7dc)
/* init_scroller  VA=0x41f278  cu=F:\projects\icytower\trunk\source\scroller.c */
/* prototype: void init_scroller(Tscroller *, FONT *, char *, int, int, int) */
#define init_scroller ((PFN_init_scroller)0x41f278)
/* init_star_field  VA=0x41f52c  cu=F:\projects\icytower\trunk\source\stars.c */
/* prototype: void init_star_field(Tstar_field *, int, int, int, int, int, int, int) */
#define init_star_field ((PFN_init_star_field)0x41f52c)
/* install_timers  VA=0x41fee4  cu=F:\projects\icytower\trunk\source\timer.c */
/* prototype: int install_timers() */
#define install_timers ((PFN_install_timers)0x41fee4)
/* excluded by --exclude (compiled natively): is_any */
/* is_custom_replay  VA=0x406b3c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int is_custom_replay(Treplay *) */
#define is_custom_replay ((PFN_is_custom_replay)0x406b3c)
/* excluded by --exclude (compiled natively): is_down */
/* excluded by --exclude (compiled natively): is_enter */
/* excluded by --exclude (compiled natively): is_fire */
/* excluded by --exclude (compiled natively): is_left */
/* excluded by --exclude (compiled natively): is_pause */
/* excluded by --exclude (compiled natively): is_right */
/* excluded by --exclude (compiled natively): is_solid */
/* excluded by --exclude (compiled natively): is_up */
/* excluded by --exclude (compiled natively): jump_player */
/* key_to_str  VA=0x416a9c  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: void key_to_str(int, char *) */
#define key_to_str ((PFN_key_to_str)0x416a9c)
/* line_alert  VA=0x409138  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void line_alert(char *) */
#define line_alert ((PFN_line_alert)0x409138)
/* excluded by --exclude (compiled natively): line_intersect */
/* loadCustomSoundDF  VA=0x402040  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: SAMPLE * loadCustomSoundDF(DATAFILE *, int) */
#define loadCustomSoundDF ((PFN_loadCustomSoundDF)0x402040)
/* loadCustomSoundFILE  VA=0x401fc0  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: SAMPLE * loadCustomSoundFILE(char *, char *) */
#define loadCustomSoundFILE ((PFN_loadCustomSoundFILE)0x401fc0)
/* loadScrambled  VA=0x40cc30  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: BITMAP * loadScrambled(char *) */
#define loadScrambled ((PFN_loadScrambled)0x40cc30)
/* load_character  VA=0x40fe78  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int load_character(const char *, int, void *) */
#define load_character ((PFN_load_character)0x40fe78)
/* load_character_bmp  VA=0x4031cc  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: BITMAP * load_character_bmp(const char *, int *, RGB *) */
#define load_character_bmp ((PFN_load_character_bmp)0x4031cc)
/* load_control  VA=0x401900  cu=F:\projects\icytower\trunk\source\control.c */
/* prototype: void load_control(Tcontrol *, it_orig_FILE *) */
#define load_control ((PFN_load_control)0x401900)
/* load_datafile_png  VA=0x41b8fc  cu=F:\projects\icytower\trunk\source\regpng.c */
/* prototype: void * load_datafile_png(PACKFILE *, long) */
#define load_datafile_png ((PFN_load_datafile_png)0x41b8fc)
/* load_frames  VA=0x402874  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: int load_frames(Tcustom *) */
#define load_frames ((PFN_load_frames)0x402874)
/* load_garbled_data  VA=0x40158c  cu=F:\projects\icytower\trunk\source\beta.c */
/* prototype: Tbeta * load_garbled_data(char *) */
#define load_garbled_data ((PFN_load_garbled_data)0x40158c)
/* load_hisc_table  VA=0x4056b4  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: int load_hisc_table(Thisc_table *, PACKFILE *) */
#define load_hisc_table ((PFN_load_hisc_table)0x4056b4)
/* load_memory_png  VA=0x406604  cu=F:\projects\icytower\trunk\source\loadpng.c */
/* prototype: BITMAP * load_memory_png(const void *, int, RGB *) */
#define load_memory_png ((PFN_load_memory_png)0x406604)
/* load_new_ad_image  VA=0x415eac  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void load_new_ad_image() */
#define load_new_ad_image ((PFN_load_new_ad_image)0x415eac)
/* load_options  VA=0x41839c  cu=F:\projects\icytower\trunk\source\options.c */
/* prototype: void load_options(Toptions *, PACKFILE *) */
#define load_options ((PFN_load_options)0x41839c)
/* load_plain_data  VA=0x4016c8  cu=F:\projects\icytower\trunk\source\beta.c */
/* prototype: Tbeta * load_plain_data(char *) */
#define load_plain_data ((PFN_load_plain_data)0x4016c8)
/* load_png  VA=0x406910  cu=F:\projects\icytower\trunk\source\loadpng.c */
/* prototype: BITMAP * load_png(const char *, RGB *) */
#define load_png ((PFN_load_png)0x406910)
/* load_png_pf  VA=0x406788  cu=F:\projects\icytower\trunk\source\loadpng.c */
/* prototype: BITMAP * load_png_pf(PACKFILE *, RGB *) */
#define load_png_pf ((PFN_load_png_pf)0x406788)
/* load_profile  VA=0x41a7ec  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: Tprofile * load_profile(char *) */
#define load_profile ((PFN_load_profile)0x41a7ec)
/* load_replay  VA=0x41cde8  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: Treplay * load_replay(const char *) */
#define load_replay ((PFN_load_replay)0x41cde8)
/* load_sound  VA=0x40ca4c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void load_sound(SAMPLE **, char *, BITMAP *, int) */
#define load_sound ((PFN_load_sound)0x40ca4c)
/* load_sounds  VA=0x40212c  cu=F:\projects\icytower\trunk\source\custom.c */
/* prototype: int load_sounds(Tcustom *) */
#define load_sounds ((PFN_load_sounds)0x40212c)
/* loadpng_init  VA=0x41b990  cu=F:\projects\icytower\trunk\source\regpng.c */
/* prototype: int loadpng_init() */
#define loadpng_init ((PFN_loadpng_init)0x41b990)
/* log2file  VA=0x40da58  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void log2file(const char *, ...) */
#define log2file ((PFN_log2file)0x40da58)
/* main_menu_callback  VA=0x4100f8  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void main_menu_callback() */
#define main_menu_callback ((PFN_main_menu_callback)0x4100f8)
/* make_hisc_table  VA=0x40583c  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: Thisc_table * make_hisc_table(char *) */
#define make_hisc_table ((PFN_make_hisc_table)0x40583c)
/* match_string  VA=0x41f5d8  cu=F:\projects\icytower\trunk\source\strptime.c */
/* prototype: int match_string(const char **, const char **) */
#define match_string ((PFN_match_string)0x41f5d8)
/* myDeleteFile  VA=0x40cd28  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void myDeleteFile(char *, char *) */
#define myDeleteFile ((PFN_myDeleteFile)0x40cd28)
/* my_alert  VA=0x40cd68  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int my_alert(char *, char *, int, int) */
#define my_alert ((PFN_my_alert)0x40cd68)
/* my_strcmp  VA=0x41e6c0  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: int my_strcmp(const void *, const void *) */
#define my_strcmp ((PFN_my_strcmp)0x41e6c0)
/* new_game  VA=0x40dc9c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int new_game() */
#define new_game ((PFN_new_game)0x40dc9c)
/* excluded by --exclude (compiled natively): new_rand */
/* new_srand  VA=0x406a04  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void new_srand(int) */
#define new_srand ((PFN_new_srand)0x406a04)
/* excluded by --exclude (compiled natively): ok_to_play */
/* open_web_browser  VA=0x40e510  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void open_web_browser(const char *) */
#define open_web_browser ((PFN_open_web_browser)0x40e510)
/* play  VA=0x411a00  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int play() */
#define play ((PFN_play)0x411a00)
/* excluded by --exclude (compiled natively): play_jump_sound */
/* play_menu_move  VA=0x406ea4  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void play_menu_move() */
#define play_menu_move ((PFN_play_menu_move)0x406ea4)
/* play_menu_select  VA=0x406e7c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void play_menu_select() */
#define play_menu_select ((PFN_play_menu_select)0x406e7c)
/* play_sound  VA=0x406da4  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void play_sound(SAMPLE *, int, int) */
#define play_sound ((PFN_play_sound)0x406da4)
/* excluded by --exclude (compiled natively): poll_control */
/* profile_data_page_advanced  VA=0x419284  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: char * profile_data_page_advanced(Tprofile *) */
#define profile_data_page_advanced ((PFN_profile_data_page_advanced)0x419284)
/* profile_data_page_basic  VA=0x4193d0  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: char * profile_data_page_basic(Tprofile *) */
#define profile_data_page_basic ((PFN_profile_data_page_basic)0x4193d0)
/* profile_data_page_extra  VA=0x419650  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: char * profile_data_page_extra(Tprofile *) */
#define profile_data_page_extra ((PFN_profile_data_page_extra)0x419650)
/* profile_data_page_general  VA=0x4196a8  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: char * profile_data_page_general(Tprofile *, char *) */
#define profile_data_page_general ((PFN_profile_data_page_general)0x4196a8)
/* pwd_garble_string  VA=0x4073c4  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void pwd_garble_string(char *, int) */
#define pwd_garble_string ((PFN_pwd_garble_string)0x4073c4)
/* qualify_hisc_table  VA=0x404994  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: int qualify_hisc_table(Thisc_table *, int) */
#define qualify_hisc_table ((PFN_qualify_hisc_table)0x404994)
/* read_data  VA=0x4068c4  cu=F:\projects\icytower\trunk\source\loadpng.c */
/* prototype: void read_data(png_structp, png_bytep, png_uint_32) */
#define read_data ((PFN_read_data)0x4068c4)
/* read_data_memory  VA=0x406738  cu=F:\projects\icytower\trunk\source\loadpng.c */
/* prototype: void read_data_memory(png_structp, png_bytep, png_uint_32) */
#define read_data_memory ((PFN_read_data_memory)0x406738)
/* read_line  VA=0x401460  cu=F:\projects\icytower\trunk\source\beta.c */
/* prototype: void read_line(char *, it_orig_FILE *) */
#define read_line ((PFN_read_line)0x401460)
/* really_load_png  VA=0x4061b0  cu=F:\projects\icytower\trunk\source\loadpng.c */
/* prototype: BITMAP * really_load_png(png_structp, png_infop, RGB *) */
#define really_load_png ((PFN_really_load_png)0x4061b0)
/* really_save_png  VA=0x41e8b0  cu=F:\projects\icytower\trunk\source\savepng.c */
/* prototype: int really_save_png(PACKFILE *, BITMAP *, const RGB *) */
#define really_save_png ((PFN_really_save_png)0x41e8b0)
/* rebuild_profile_list  VA=0x40c758  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int rebuild_profile_list(Tavailable_profile **) */
#define rebuild_profile_list ((PFN_rebuild_profile_list)0x40c758)
/* register_png_datafile_object  VA=0x41b8c0  cu=F:\projects\icytower\trunk\source\regpng.c */
/* prototype: void register_png_datafile_object(int) */
#define register_png_datafile_object ((PFN_register_png_datafile_object)0x41b8c0)
/* register_png_file_type  VA=0x41b96c  cu=F:\projects\icytower\trunk\source\regpng.c */
/* prototype: void register_png_file_type() */
#define register_png_file_type ((PFN_register_png_file_type)0x41b96c)
/* replaceBadCharacters  VA=0x407344  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void replaceBadCharacters(char *, char) */
#define replaceBadCharacters ((PFN_replaceBadCharacters)0x407344)
/* replay_menu_callback  VA=0x4073f8  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void replay_menu_callback() */
#define replay_menu_callback ((PFN_replay_menu_callback)0x4073f8)
/* replay_selector  VA=0x41d258  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: Treplay * replay_selector(Tcontrol *, char *) */
#define replay_selector ((PFN_replay_selector)0x41d258)
/* reset_hisc_table  VA=0x405750  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: void reset_hisc_table(Thisc_table *, char *, int, int) */
#define reset_hisc_table ((PFN_reset_hisc_table)0x405750)
/* excluded by --exclude (compiled natively): reset_map */
/* reset_menu  VA=0x41748c  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: void reset_menu(Tmenu *, Tmenu_params *, int) */
#define reset_menu ((PFN_reset_menu)0x41748c)
/* reset_options  VA=0x4182cc  cu=F:\projects\icytower\trunk\source\options.c */
/* prototype: void reset_options(Toptions *) */
#define reset_options ((PFN_reset_options)0x4182cc)
/* excluded by --exclude (compiled natively): reset_particles */
/* excluded by --exclude (compiled natively): reset_player */
/* excluded by --exclude (compiled natively): restart_scroller */
/* run_demo  VA=0x415e0c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void run_demo(char *) */
#define run_demo ((PFN_run_demo)0x415e0c)
/* save_config  VA=0x40e130  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void save_config() */
#define save_config ((PFN_save_config)0x40e130)
/* save_control  VA=0x40192c  cu=F:\projects\icytower\trunk\source\control.c */
/* prototype: void save_control(Tcontrol *, it_orig_FILE *) */
#define save_control ((PFN_save_control)0x40192c)
/* save_garbled_data  VA=0x401344  cu=F:\projects\icytower\trunk\source\beta.c */
/* prototype: int save_garbled_data(Tbeta *, char *) */
#define save_garbled_data ((PFN_save_garbled_data)0x401344)
/* save_hisc_table  VA=0x405630  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: void save_hisc_table(Thisc_table *, PACKFILE *) */
#define save_hisc_table ((PFN_save_hisc_table)0x405630)
/* save_options  VA=0x4183e4  cu=F:\projects\icytower\trunk\source\options.c */
/* prototype: void save_options(Toptions *, PACKFILE *) */
#define save_options ((PFN_save_options)0x4183e4)
/* save_png  VA=0x41f000  cu=F:\projects\icytower\trunk\source\savepng.c */
/* prototype: int save_png(const char *, BITMAP *, const RGB *) */
#define save_png ((PFN_save_png)0x41f000)
/* save_profile  VA=0x41a3b8  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: int save_profile(Tprofile *) */
#define save_profile ((PFN_save_profile)0x41a3b8)
/* save_replay  VA=0x41dd78  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: int save_replay(const char *, const char *, Treplay *, int, int) */
#define save_replay ((PFN_save_replay)0x41dd78)
/* excluded by --exclude (compiled natively): scroll_scroller */
/* scroll_star_field  VA=0x41f408  cu=F:\projects\icytower\trunk\source\stars.c */
/* prototype: void scroll_star_field(Tstar_field *, double, double) */
#define scroll_star_field ((PFN_scroll_star_field)0x41f408)
/* select_profile  VA=0x41acc0  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: Tprofile * select_profile(Tprofile *, Tavailable_profile *, int, Tcontrol *) */
#define select_profile ((PFN_select_profile)0x41acc0)
/* excluded by --exclude (compiled natively): set_control */
/* set_current_avatar  VA=0x406ce4  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void set_current_avatar() */
#define set_current_avatar ((PFN_set_current_avatar)0x406ce4)
/* set_next_rank_message  VA=0x418b24  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: void set_next_rank_message(char *, Tprofile *) */
#define set_next_rank_message ((PFN_set_next_rank_message)0x418b24)
/* set_selection_value  VA=0x416a78  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: int set_selection_value(Tmenu_selection *, int) */
#define set_selection_value ((PFN_set_selection_value)0x416a78)
/* set_slider_value  VA=0x416a48  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: int set_slider_value(Tmenu_slider *, int) */
#define set_slider_value ((PFN_set_slider_value)0x416a48)
/* set_sort_method  VA=0x41b9b8  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: void set_sort_method(int) */
#define set_sort_method ((PFN_set_sort_method)0x41b9b8)
/* show_credits  VA=0x40c4dc  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void show_credits() */
#define show_credits ((PFN_show_credits)0x40c4dc)
/* show_instructions  VA=0x40c368  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void show_instructions() */
#define show_instructions ((PFN_show_instructions)0x40c368)
/* show_name  VA=0x406d44  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: int show_name(const char *, int, void *) */
#define show_name ((PFN_show_name)0x406d44)
/* sort_hisc_table  VA=0x4049bc  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: void sort_hisc_table(Thisc_table *) */
#define sort_hisc_table ((PFN_sort_hisc_table)0x4049bc)
/* startGameMusic  VA=0x40cb30  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void startGameMusic() */
#define startGameMusic ((PFN_startGameMusic)0x40cb30)
/* startMenuMusic  VA=0x406d68  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void startMenuMusic() */
#define startMenuMusic ((PFN_startMenuMusic)0x406d68)
/* excluded by --exclude (compiled natively): start_reward */
/* stopGameMusic  VA=0x40caf4  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void stopGameMusic() */
#define stopGameMusic ((PFN_stopGameMusic)0x40caf4)
/* stopMenuMusic  VA=0x406f5c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void stopMenuMusic() */
#define stopMenuMusic ((PFN_stopMenuMusic)0x406f5c)
/* strptime  VA=0x41fe2c  cu=F:\projects\icytower\trunk\source\strptime.c */
/* prototype: char * strptime(const char *, const char *, struct it_orig_tm *) */
#define strptime ((PFN_strptime)0x41fe2c)
/* excluded by --exclude (compiled natively): switchedFromProgram */
/* excluded by --exclude (compiled natively): switchedToProgram */
/* syncOptionsFromProfile  VA=0x40c820  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void syncOptionsFromProfile() */
#define syncOptionsFromProfile ((PFN_syncOptionsFromProfile)0x40c820)
/* syncProfileFromOptions  VA=0x406a14  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void syncProfileFromOptions() */
#define syncProfileFromOptions ((PFN_syncProfileFromOptions)0x406a14)
/* take_screenshot  VA=0x41002c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void take_screenshot(BITMAP *) */
#define take_screenshot ((PFN_take_screenshot)0x41002c)
/* testWindowResolution  VA=0x40db18  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void testWindowResolution() */
#define testWindowResolution ((PFN_testWindowResolution)0x40db18)
/* timegm  VA=0x41fe50  cu=F:\projects\icytower\trunk\source\timecompat.c */
/* prototype: it_orig_time_t timegm(struct it_orig_tm *) */
#define timegm ((PFN_timegm)0x41fe50)
/* uninit_game  VA=0x40e288  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void uninit_game() */
#define uninit_game ((PFN_uninit_game)0x40e288)
/* update_file_list  VA=0x41bda0  cu=F:\projects\icytower\trunk\source\replay.c */
/* prototype: void update_file_list(char *) */
#define update_file_list ((PFN_update_file_list)0x41bda0)
/* excluded by --exclude (compiled natively): update_frame */
/* update_game_menu  VA=0x417adc  cu=F:\projects\icytower\trunk\source\menu.c */
/* prototype: int update_game_menu(BITMAP *, Tmenu *, Tmenu_params *, Tcontrol *, int, int, void **) */
#define update_game_menu ((PFN_update_game_menu)0x417adc)
/* excluded by --exclude (compiled natively): update_particle */
/* excluded by --exclude (compiled natively): update_player */
/* update_reward  VA=0x406a8c  cu=F:\projects\icytower\trunk\source\main.c */
/* prototype: void update_reward() */
#define update_reward ((PFN_update_reward)0x406a8c)
/* view_profile  VA=0x419aec  cu=F:\projects\icytower\trunk\source\profile.c */
/* prototype: int view_profile(Tprofile *) */
#define view_profile ((PFN_view_profile)0x419aec)
/* view_scores  VA=0x404c38  cu=F:\projects\icytower\trunk\source\hisc.c */
/* prototype: void view_scores(Thisc_table **, char **) */
#define view_scores ((PFN_view_scores)0x404c38)
/* write_data  VA=0x41f074  cu=F:\projects\icytower\trunk\source\savepng.c */
/* prototype: void write_data(png_structp, png_bytep, png_uint_32) */
#define write_data ((PFN_write_data)0x41f074)

#endif /* PF_BINDINGS_H */
