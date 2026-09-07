/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_interop.py from:
 *   artifacts/dwarf_info.txt
 *   artifacts/functions.json
 * scope=game. Re-run gen_interop.py to regenerate; do not hand-edit.
 * See win32_pilot.md SS3: the original image is mapped in-process at its
 * original base 0x400000 with no relocations, so a global at VA X is
 * *(T*)X and a function at VA F is ((ret(__cdecl*)(args))F).
 */

#ifndef IT_GLOBALS_H
#define IT_GLOBALS_H
#include "it_types.h"

/* REPLAY_HEADER  VA=0x4d7dd0  cu=F:\projects\icytower\trunk\source\replay.c (static) */
#define IT_G_REPLAY_HEADER (*(const char (*)[6])0x4d7dd0)
static const char (*g_REPLAY_HEADER_p)[6] = (const char (*)[6])0x4d7dd0;

/* _png_compression_level  VA=0x4bc010  cu=F:\projects\icytower\trunk\source\loadpng.c */
#define IT_G__png_compression_level (*(int *)0x4bc010)
static int *g__png_compression_level_p = (int *)0x4bc010;

/* _png_screen_gamma  VA=0x4bc008  cu=F:\projects\icytower\trunk\source\loadpng.c */
#define IT_G__png_screen_gamma (*(double *)0x4bc008)
static double *g__png_screen_gamma_p = (double *)0x4bc008;

/* abb_month  VA=0x4bde00  cu=F:\projects\icytower\trunk\source\strptime.c (static) */
#define IT_G_abb_month (*(const char *(*)[13])0x4bde00)
static const char *(*g_abb_month_p)[13] = (const char *(*)[13])0x4bde00;

/* abb_weekdays  VA=0x4bdda0  cu=F:\projects\icytower\trunk\source\strptime.c (static) */
#define IT_G_abb_weekdays (*(const char *(*)[8])0x4bdda0)
static const char *(*g_abb_weekdays_p)[8] = (const char *(*)[8])0x4bdda0;

/* ampm  VA=0x4bde34  cu=F:\projects\icytower\trunk\source\strptime.c (static) */
#define IT_G_ampm (*(const char *(*)[3])0x4bde34)
static const char *(*g_ampm_p)[3] = (const char *(*)[3])0x4bde34;

/* any11  VA=0x4dd170  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_any11 (*(int *)0x4dd170)
static int *g_any11_p = (int *)0x4dd170;

/* any12  VA=0x4dd174  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_any12 (*(int *)0x4dd174)
static int *g_any12_p = (int *)0x4dd174;

/* any13  VA=0x4dd178  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_any13 (*(int *)0x4dd178)
static int *g_any13_p = (int *)0x4dd178;

/* any21  VA=0x4dd17c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_any21 (*(int *)0x4dd17c)
static int *g_any21_p = (int *)0x4dd17c;

/* any22  VA=0x4dd180  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_any22 (*(int *)0x4dd180)
static int *g_any22_p = (int *)0x4dd180;

/* any23  VA=0x4dd184  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_any23 (*(int *)0x4dd184)
static int *g_any23_p = (int *)0x4dd184;

/* bg_beat  VA=0x4dd2a8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_bg_beat (*(SAMPLE **)0x4dd2a8)
static SAMPLE **g_bg_beat_p = (SAMPLE **)0x4dd2a8;

/* bg_menu  VA=0x4dd2ac  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_bg_menu (*(SAMPLE **)0x4dd2ac)
static SAMPLE **g_bg_menu_p = (SAMPLE **)0x4dd2ac;

/* bg_stripe_ids  VA=0x4dd19c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_bg_stripe_ids (*(int (*)[5])0x4dd19c)
static int (*g_bg_stripe_ids_p)[5] = (int (*)[5])0x4dd19c;

/* black  VA=0x4dd008  cu=F:\projects\icytower\trunk\source\custom.c */
#define IT_G_black (*(RGB *)0x4dd008)
static RGB *g_black_p = (RGB *)0x4dd008;

/* blit_mode  VA=0x4dd324  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_blit_mode__blit_to_screen (*(int *)0x4dd324)
static int *g_blit_mode__blit_to_screen_p = (int *)0x4dd324;

/* category_names  VA=0x4bc080  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_category_names (*(char *(*)[15])0x4bc080)
static char *(*g_category_names_p)[15] = (char *(*)[15])0x4bc080;

/* characters  VA=0x4fdcc8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_characters (*(Tcharacter **)0x4fdcc8)
static Tcharacter **g_characters_p = (Tcharacter **)0x4fdcc8;

/* checkMusicVoiceID  VA=0x4bc174  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_checkMusicVoiceID (*(int *)0x4bc174)
static int *g_checkMusicVoiceID_p = (int *)0x4bc174;

/* clock_angle  VA=0x4dd248  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_clock_angle (*(int *)0x4dd248)
static int *g_clock_angle_p = (int *)0x4dd248;

/* closeButtonClicked  VA=0x4dd264  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_closeButtonClicked (*(int *)0x4dd264)
static int *g_closeButtonClicked_p = (int *)0x4dd264;

/* cmdline  VA=0x4dd14c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_cmdline (*(Tcommandline *)0x4dd14c)
static Tcommandline *g_cmdline_p = (Tcommandline *)0x4dd14c;

/* collision_type  VA=0x4dd140  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_collision_type (*(int *)0x4dd140)
static int *g_collision_type_p = (int *)0x4dd140;

/* comboNames  VA=0x4bdd20  cu=F:\projects\icytower\trunk\source\profile.c */
#define IT_G_comboNames (*(char *(*)[10])0x4bdd20)
static char *(*g_comboNames_p)[10] = (char *(*)[10])0x4bdd20;

/* combo_sound  VA=0x4dd280  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_combo_sound (*(SAMPLE *(*)[10])0x4dd280)
static SAMPLE *(*g_combo_sound_p)[10] = (SAMPLE *(*)[10])0x4dd280;

/* count  VA=0x4dd330  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_count__load_character (*(int *)0x4dd330)
static int *g_count__load_character_p = (int *)0x4dd330;

/* count  VA=0x4dd318  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_count__main_menu_callback (*(int *)0x4dd318)
static int *g_count__main_menu_callback_p = (int *)0x4dd318;

/* ctrl  VA=0x5000c8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_ctrl (*(Tcontrol *)0x5000c8)
static Tcontrol *g_ctrl_p = (Tcontrol *)0x5000c8;

/* ctrl_menu  VA=0x4bc1c0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_ctrl_menu (*(Tmenu (*)[6])0x4bc1c0)
static Tmenu (*g_ctrl_menu_p)[6] = (Tmenu (*)[6])0x4bc1c0;

/* curr_char  VA=0x4dd270  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_curr_char (*(int *)0x4dd270)
static int *g_curr_char_p = (int *)0x4dd270;

/* custom  VA=0x4fa738  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_custom (*(Tcustom *)0x4fa738)
static Tcustom *g_custom_p = (Tcustom *)0x4fa738;

/* custom_menu  VA=0x4bcec0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_custom_menu (*(Tmenu (*)[5])0x4bcec0)
static Tmenu (*g_custom_menu_p)[5] = (Tmenu (*)[5])0x4bcec0;

/* cycle_count  VA=0x506938  cu=F:\projects\icytower\trunk\source\timer.c */
#define IT_G_cycle_count (*(volatile int *)0x506938)
static volatile int *g_cycle_count_p = (volatile int *)0x506938;

/* cycle_loops  VA=0x4dd24c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_cycle_loops (*(int *)0x4dd24c)
static int *g_cycle_loops_p = (int *)0x4dd24c;

/* data  VA=0x4dd23c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_data (*(DATAFILE **)0x4dd23c)
static DATAFILE **g_data_p = (DATAFILE **)0x4dd23c;

/* debug  VA=0x4dd160  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_debug (*(int *)0x4dd160)
static int *g_debug_p = (int *)0x4dd160;

/* demo  VA=0x4dd250  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_demo (*(Treplay **)0x4dd250)
static Treplay **g_demo_p = (Treplay **)0x4dd250;

/* dropped_file_is_not_a_replay  VA=0x4dd16c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_dropped_file_is_not_a_replay (*(int *)0x4dd16c)
static int *g_dropped_file_is_not_a_replay_p = (int *)0x4dd16c;

/* eyecandy_selection  VA=0x4fa278  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_eyecandy_selection (*(Tmenu_selection *)0x4fa278)
static Tmenu_selection *g_eyecandy_selection_p = (Tmenu_selection *)0x4fa278;

/* face  VA=0x4dd31c  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_face__main_menu_callback (*(int *)0x4dd31c)
static int *g_face__main_menu_callback_p = (int *)0x4dd31c;

/* fall_count  VA=0x4dd244  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_fall_count (*(int *)0x4dd244)
static int *g_fall_count_p = (int *)0x4dd244;

/* fast_fast_forward  VA=0x4dd258  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_fast_fast_forward (*(int *)0x4dd258)
static int *g_fast_fast_forward_p = (int *)0x4dd258;

/* fast_forward  VA=0x4dd254  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_fast_forward (*(int *)0x4dd254)
static int *g_fast_forward_p = (int *)0x4dd254;

/* floor_size_modifiers  VA=0x4bdb60  cu=F:\projects\icytower\trunk\source\map.c */
#define IT_G_floor_size_modifiers (*(int (*)[5])0x4bdb60)
static int (*g_floor_size_modifiers_p)[5] = (int (*)[5])0x4bdb60;

/* floor_size_selection  VA=0x4fe7b8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_floor_size_selection (*(Tmenu_selection *)0x4fe7b8)
static Tmenu_selection *g_floor_size_selection_p = (Tmenu_selection *)0x4fe7b8;

/* floors  VA=0x4dd304  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_floors (*(Tmenu_floor_selection *)0x4dd304)
static Tmenu_floor_selection *g_floors_p = (Tmenu_floor_selection *)0x4dd304;

/* fps  VA=0x506948  cu=F:\projects\icytower\trunk\source\timer.c */
#define IT_G_fps (*(volatile int *)0x506948)
static volatile int *g_fps_p = (volatile int *)0x506948;

/* frame_count  VA=0x506978  cu=F:\projects\icytower\trunk\source\timer.c */
#define IT_G_frame_count (*(volatile int *)0x506978)
static volatile int *g_frame_count_p = (volatile int *)0x506978;

/* full_month  VA=0x4bddc0  cu=F:\projects\icytower\trunk\source\strptime.c (static) */
#define IT_G_full_month (*(const char *(*)[13])0x4bddc0)
static const char *(*g_full_month_p)[13] = (const char *(*)[13])0x4bddc0;

/* full_weekdays  VA=0x4bdd80  cu=F:\projects\icytower\trunk\source\strptime.c (static) */
#define IT_G_full_weekdays (*(const char *(*)[8])0x4bdd80)
static const char *(*g_full_weekdays_p)[8] = (const char *(*)[8])0x4bdd80;

/* gFLDADMutex  VA=0x4bc004  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define IT_G_gFLDADMutex (*(pthread_mutex_t *)0x4bc004)
static pthread_mutex_t *g_gFLDADMutex_p = (pthread_mutex_t *)0x4bc004;

/* gFLDADThread  VA=0x4f87d8  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define IT_G_gFLDADThread (*(pthread_t *)0x4f87d8)
static pthread_t *g_gFLDADThread_p = (pthread_t *)0x4f87d8;

/* gameData  VA=0x4dd260  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_gameData (*(Tgame_data **)0x4dd260)
static Tgame_data **g_gameData_p = (Tgame_data **)0x4dd260;

/* gameMusicVoiceID  VA=0x4bc178  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_gameMusicVoiceID (*(int *)0x4bc178)
static int *g_gameMusicVoiceID_p = (int *)0x4bc178;

/* game_menu  VA=0x4bca00  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_game_menu (*(Tmenu (*)[1])0x4bca00)
static Tmenu (*g_game_menu_p)[1] = (Tmenu (*)[1])0x4bca00;

/* gameover_bmp  VA=0x5000f8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_gameover_bmp (*(BITMAP **)0x5000f8)
static BITMAP **g_gameover_bmp_p = (BITMAP **)0x5000f8;

/* gamepad  VA=0x4f8748  cu=F:\projects\icytower\trunk\source\control.c */
#define IT_G_gamepad (*(Tgamepad *)0x4f8748)
static Tgamepad *g_gamepad_p = (Tgamepad *)0x4f8748;

/* gdComboStart  VA=0x4dd190  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_gdComboStart (*(int *)0x4dd190)
static int *g_gdComboStart_p = (int *)0x4dd190;

/* gdLastJumpDiff  VA=0x4dd18c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_gdLastJumpDiff (*(int *)0x4dd18c)
static int *g_gdLastJumpDiff_p = (int *)0x4dd18c;

/* gfx_menu  VA=0x4bc700  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_gfx_menu (*(Tmenu (*)[5])0x4bc700)
static Tmenu (*g_gfx_menu_p)[5] = (Tmenu (*)[5])0x4bc700;

/* giAdCacheSize  VA=0x4dd020  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define IT_G_giAdCacheSize (*(int *)0x4dd020)
static int *g_giAdCacheSize_p = (int *)0x4dd020;

/* got_joystick  VA=0x4f8b08  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_got_joystick (*(int *)0x4f8b08)
static int *g_got_joystick_p = (int *)0x4f8b08;

/* gpAdCache  VA=0x4dd024  cu=F:\projects\icytower\trunk\source\fld_adspot.c */
#define IT_G_gpAdCache (*(FLDAdSpot **)0x4dd024)
static FLDAdSpot **g_gpAdCache_p = (FLDAdSpot **)0x4dd024;

/* gravity_modifier  VA=0x4bdba8  cu=F:\projects\icytower\trunk\source\player.c */
#define IT_G_gravity_modifier (*(double (*)[3])0x4bdba8)
static double (*g_gravity_modifier_p)[3] = (double (*)[3])0x4bdba8;

/* gravity_selection  VA=0x4fac38  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_gravity_selection (*(Tmenu_selection *)0x4fac38)
static Tmenu_selection *g_gravity_selection_p = (Tmenu_selection *)0x4fac38;

/* greeting_scroller  VA=0x4fdce8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_greeting_scroller (*(Tscroller *)0x4fdce8)
static Tscroller *g_greeting_scroller_p = (Tscroller *)0x4fdce8;

/* hasFocus  VA=0x4bc020  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_hasFocus (*(int *)0x4bc020)
static int *g_hasFocus_p = (int *)0x4bc020;

/* hints  VA=0x4bc0c0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_hints (*(char *(*)[45])0x4bc0c0)
static char *(*g_hints_p)[45] = (char *(*)[45])0x4bc0c0;

/* hisc_names  VA=0x4bc040  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_hisc_names (*(char *(*)[15])0x4bc040)
static char *(*g_hisc_names_p)[15] = (char *(*)[15])0x4bc040;

/* hisc_tables  VA=0x4dd1c0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_hisc_tables (*(Thisc_table *(*)[15])0x4dd1c0)
static Thisc_table *(*g_hisc_tables_p)[15] = (Thisc_table *(*)[15])0x4dd1c0;

/* hurry_y  VA=0x4ff118  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_hurry_y (*(int *)0x4ff118)
static int *g_hurry_y_p = (int *)0x4ff118;

/* in_replay_menu  VA=0x4dd314  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_in_replay_menu (*(int *)0x4dd314)
static int *g_in_replay_menu_p = (int *)0x4dd314;

/* init_ok  VA=0x4dd164  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_init_ok (*(int *)0x4dd164)
static int *g_init_ok_p = (int *)0x4dd164;

/* init_string  VA=0x4bdb3c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_init_string (*(char (*)[7])0x4bdb3c)
static char (*g_init_string_p)[7] = (char (*)[7])0x4bdb3c;

/* is_playing_custom_game  VA=0x4dd188  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_is_playing_custom_game (*(int *)0x4dd188)
static int *g_is_playing_custom_game_p = (int *)0x4dd188;

/* itr_file_list  VA=0x500938  cu=F:\projects\icytower\trunk\source\replay.c */
#define IT_G_itr_file_list (*(Treplay_post (*)[1024])0x500938)
static Treplay_post (*g_itr_file_list_p)[1024] = (Treplay_post (*)[1024])0x500938;

/* itrcheck  VA=0x4dd168  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_itrcheck (*(int *)0x4dd168)
static int *g_itrcheck_p = (int *)0x4dd168;

/* jcLabels  VA=0x4bdbc0  cu=F:\projects\icytower\trunk\source\profile.c */
#define IT_G_jcLabels (*(char *(*)[5])0x4bdbc0)
static char *(*g_jcLabels_p)[5] = (char *(*)[5])0x4bdbc0;

/* jumpSequence  VA=0x4fa728  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_jumpSequence (*(Tgd_jump_sequence *)0x4fa728)
static Tgd_jump_sequence *g_jumpSequence_p = (Tgd_jump_sequence *)0x4fa728;

/* jump_sound  VA=0x4dd2b0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_jump_sound (*(SAMPLE *(*)[3])0x4dd2b0)
static SAMPLE *(*g_jump_sound_p)[3] = (SAMPLE *(*)[3])0x4dd2b0;

/* lastFocus  VA=0x4bc024  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_lastFocus (*(int *)0x4bc024)
static int *g_lastFocus_p = (int *)0x4bc024;

/* lastMouseB  VA=0x4dd268  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_lastMouseB (*(int *)0x4dd268)
static int *g_lastMouseB_p = (int *)0x4dd268;

/* last_log  VA=0x4f89e8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_last_log (*(char (*)[256])0x4f89e8)
static char (*g_last_log_p)[256] = (char (*)[256])0x4f89e8;

/* last_stripe_y  VA=0x4fa308  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_last_stripe_y (*(int *)0x4fa308)
static int *g_last_stripe_y_p = (int *)0x4fa308;

/* localFilename  VA=0x4dd040  cu=F:\projects\icytower\trunk\source\fld_adspot.c (static) */
#define IT_G_localFilename__fldads_get_local_cache_name (*(char (*)[256])0x4dd040)
static char (*g_localFilename__fldads_get_local_cache_name_p)[256] = (char (*)[256])0x4dd040;

/* logfilename  VA=0x4dd340  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_logfilename__log2file (*(char (*)[1024])0x4dd340)
static char (*g_logfilename__log2file_p)[1024] = (char (*)[1024])0x4dd340;

/* logic_count  VA=0x506958  cu=F:\projects\icytower\trunk\source\timer.c */
#define IT_G_logic_count (*(volatile int *)0x506958)
static volatile int *g_logic_count_p = (volatile int *)0x506958;

/* lps  VA=0x506968  cu=F:\projects\icytower\trunk\source\timer.c */
#define IT_G_lps (*(volatile int *)0x506968)
static volatile int *g_lps_p = (volatile int *)0x506968;

/* main_menu  VA=0x4bd380  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_main_menu (*(Tmenu (*)[7])0x4bd380)
static Tmenu (*g_main_menu_p)[7] = (Tmenu (*)[7])0x4bd380;

/* map  VA=0x4f8b18  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_map (*(Tmap *)0x4f8b18)
static Tmap *g_map_p = (Tmap *)0x4f8b18;

/* max_speed  VA=0x4bdb80  cu=F:\projects\icytower\trunk\source\player.c */
#define IT_G_max_speed (*(double (*)[5])0x4bdb80)
static double (*g_max_speed_p)[5] = (double (*)[5])0x4bdb80;

/* menu_params  VA=0x4f8e38  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_menu_params (*(Tmenu_params *)0x4f8e38)
static Tmenu_params *g_menu_params_p = (Tmenu_params *)0x4f8e38;

/* menu_sounds  VA=0x4dd2c8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_menu_sounds (*(SAMPLE *(*)[2])0x4dd2c8)
static SAMPLE *(*g_menu_sounds_p)[2] = (SAMPLE *(*)[2])0x4dd2c8;

/* msc_volume_slider  VA=0x4bc1a8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_msc_volume_slider (*(Tmenu_slider *)0x4bc1a8)
static Tmenu_slider *g_msc_volume_slider_p = (Tmenu_slider *)0x4bc1a8;

/* new_personal_best  VA=0x4dd200  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_new_personal_best (*(int (*)[15])0x4dd200)
static int (*g_new_personal_best_p)[15] = (int (*)[15])0x4dd200;

/* numProfiles  VA=0x4dd278  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_numProfiles (*(int *)0x4dd278)
static int *g_numProfiles_p = (int *)0x4dd278;

/* num_chars  VA=0x4dd26c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_num_chars (*(int *)0x4dd26c)
static int *g_num_chars_p = (int *)0x4dd26c;

/* num_itr_files  VA=0x4dd744  cu=F:\projects\icytower\trunk\source\replay.c */
#define IT_G_num_itr_files (*(int *)0x4dd744)
static int *g_num_itr_files_p = (int *)0x4dd744;

/* number  VA=0x4dd334  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_number__take_screenshot (*(int *)0x4dd334)
static int *g_number__take_screenshot_p = (int *)0x4dd334;

/* opt_menu  VA=0x4bcc60  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_opt_menu (*(Tmenu (*)[4])0x4bcc60)
static Tmenu (*g_opt_menu_p)[4] = (Tmenu (*)[4])0x4bcc60;

/* options  VA=0x4fe528  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_options (*(Toptions *)0x4fe528)
static Toptions *g_options_p = (Toptions *)0x4fe528;

/* pFLDAd  VA=0x4dd310  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_pFLDAd (*(const FLDAdSpot **)0x4dd310)
static const FLDAdSpot **g_pFLDAd_p = (const FLDAdSpot **)0x4dd310;

/* pFLDAdBitmap  VA=0x4dd30c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_pFLDAdBitmap (*(BITMAP **)0x4dd30c)
static BITMAP **g_pFLDAdBitmap_p = (BITMAP **)0x4dd30c;

/* p  VA=0x4dd328  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_p__datafile_callback_slow (*(int *)0x4dd328)
static int *g_p__datafile_callback_slow_p = (int *)0x4dd328;

/* pink  VA=0x4bc000  cu=F:\projects\icytower\trunk\source\custom.c */
#define IT_G_pink (*(RGB *)0x4bc000)
static RGB *g_pink_p = (RGB *)0x4bc000;

/* play_char  VA=0x4fa318  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_play_char (*(Tmenu_char_selection *)0x4fa318)
static Tmenu_char_selection *g_play_char_p = (Tmenu_char_selection *)0x4fa318;

/* play_menu  VA=0x4bd1c0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_play_menu (*(Tmenu (*)[3])0x4bd1c0)
static Tmenu (*g_play_menu_p)[3] = (Tmenu (*)[3])0x4bd1c0;

/* player_id  VA=0x4fe518  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_player_id (*(int *)0x4fe518)
static int *g_player_id_p = (int *)0x4fe518;

/* ply  VA=0x4ff128  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_ply (*(Tplayer *(*)[1000])0x4ff128)
static Tplayer *(*g_ply_p)[1000] = (Tplayer *(*)[1000])0x4ff128;

/* poster  VA=0x4dd198  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_poster (*(BITMAP **)0x4dd198)
static BITMAP **g_poster_p = (BITMAP **)0x4dd198;

/* profile  VA=0x4dd27c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_profile (*(Tprofile **)0x4dd27c)
static Tprofile **g_profile_p = (Tprofile **)0x4dd27c;

/* profile_menu  VA=0x4bcaa0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_profile_menu (*(Tmenu (*)[3])0x4bcaa0)
static Tmenu (*g_profile_menu_p)[3] = (Tmenu (*)[3])0x4bcaa0;

/* profiles  VA=0x4dd274  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_profiles (*(Tavailable_profile **)0x4dd274)
static Tavailable_profile **g_profiles_p = (Tavailable_profile **)0x4dd274;

/* rankCCCs  VA=0x4bdca0  cu=F:\projects\icytower\trunk\source\profile.c */
#define IT_G_rankCCCs (*(int (*)[12])0x4bdca0)
static int (*g_rankCCCs_p)[12] = (int (*)[12])0x4bdca0;

/* rankCombos  VA=0x4bdc60  cu=F:\projects\icytower\trunk\source\profile.c */
#define IT_G_rankCombos (*(int (*)[12])0x4bdc60)
static int (*g_rankCombos_p)[12] = (int (*)[12])0x4bdc60;

/* rankFloors  VA=0x4bdc20  cu=F:\projects\icytower\trunk\source\profile.c */
#define IT_G_rankFloors (*(int (*)[12])0x4bdc20)
static int (*g_rankFloors_p)[12] = (int (*)[12])0x4bdc20;

/* rankLables  VA=0x4bdbe0  cu=F:\projects\icytower\trunk\source\profile.c */
#define IT_G_rankLables (*(char *(*)[12])0x4bdbe0)
static char *(*g_rankLables_p)[12] = (char *(*)[12])0x4bdbe0;

/* rankNMLs  VA=0x4bdce0  cu=F:\projects\icytower\trunk\source\profile.c */
#define IT_G_rankNMLs (*(int (*)[12])0x4bdce0)
static int (*g_rankNMLs_p)[12] = (int (*)[12])0x4bdce0;

/* rec_pos  VA=0x4fec58  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_rec_pos (*(int *)0x4fec58)
static int *g_rec_pos_p = (int *)0x4fec58;

/* rec_seed  VA=0x4fe7a8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_rec_seed (*(int *)0x4fe7a8)
static int *g_rec_seed_p = (int *)0x4fe7a8;

/* recording  VA=0x4f8e28  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_recording (*(int *)0x4f8e28)
static int *g_recording_p = (int *)0x4f8e28;

/* rejump  VA=0x4fdcd8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_rejump (*(int *)0x4fdcd8)
static int *g_rejump_p = (int *)0x4fdcd8;

/* replay_directory  VA=0x4fe848  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_replay_directory (*(char (*)[1024])0x4fe848)
static char (*g_replay_directory_p)[1024] = (char (*)[1024])0x4fe848;

/* replay_menu  VA=0x4bd7a0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_replay_menu (*(Tmenu (*)[5])0x4bd7a0)
static Tmenu (*g_replay_menu_p)[5] = (Tmenu (*)[5])0x4bd7a0;

/* reward_bmp  VA=0x4f8af8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_reward_bmp (*(BITMAP **)0x4f8af8)
static BITMAP **g_reward_bmp_p = (BITMAP **)0x4f8af8;

/* reward_scale  VA=0x4fac28  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_reward_scale (*(fixed *)0x4fac28)
static fixed *g_reward_scale_p = (fixed *)0x4fac28;

/* reward_time  VA=0x4fec68  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_reward_time (*(int *)0x4fec68)
static int *g_reward_time_p = (int *)0x4fec68;

/* sLogMutex  VA=0x4bdb44  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_sLogMutex__log2file (*(pthread_mutex_t *)0x4bdb44)
static pthread_mutex_t *g_sLogMutex__log2file_p = (pthread_mutex_t *)0x4bdb44;

/* scroll_count  VA=0x4fec48  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_scroll_count (*(int *)0x4fec48)
static int *g_scroll_count_p = (int *)0x4fec48;

/* scroll_delay  VA=0x4f8ae8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_scroll_delay (*(int *)0x4f8ae8)
static int *g_scroll_delay_p = (int *)0x4f8ae8;

/* scroll_speed_selection  VA=0x4fec78  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_scroll_speed_selection (*(Tmenu_selection *)0x4fec78)
static Tmenu_selection *g_scroll_speed_selection_p = (Tmenu_selection *)0x4fec78;

/* scroller_greetings  VA=0x4bdaa0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_scroller_greetings (*(char (*)[156])0x4bdaa0)
static char (*g_scroller_greetings_p)[156] = (char (*)[156])0x4bdaa0;

/* seed  VA=0x4ff108  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_seed (*(double *)0x4ff108)
static double *g_seed_p = (double *)0x4ff108;

/* sfx  VA=0x4dd240  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_sfx (*(DATAFILE **)0x4dd240)
static DATAFILE **g_sfx_p = (DATAFILE **)0x4dd240;

/* sfx_file  VA=0x4f87e8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_sfx_file (*(char (*)[512])0x4f87e8)
static char (*g_sfx_file_p)[512] = (char (*)[512])0x4f87e8;

/* snd_menu  VA=0x4bc540  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_snd_menu (*(Tmenu (*)[3])0x4bc540)
static Tmenu (*g_snd_menu_p)[3] = (Tmenu (*)[3])0x4bc540;

/* snd_volume_slider  VA=0x4bc198  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_snd_volume_slider (*(Tmenu_slider *)0x4bc198)
static Tmenu_slider *g_snd_volume_slider_p = (Tmenu_slider *)0x4bc198;

/* someCounter  VA=0x4dd320  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_someCounter__play (*(int *)0x4dd320)
static int *g_someCounter__play_p = (int *)0x4dd320;

/* sort_method  VA=0x4bdd60  cu=F:\projects\icytower\trunk\source\replay.c */
#define IT_G_sort_method (*(int *)0x4bdd60)
static int *g_sort_method_p = (int *)0x4bdd60;

/* sounds  VA=0x4dd2e0  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_sounds (*(SAMPLE *(*)[9])0x4dd2e0)
static SAMPLE *(*g_sounds_p)[9] = (SAMPLE *(*)[9])0x4dd2e0;

/* speaker  VA=0x4dd2bc  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_speaker (*(SAMPLE *(*)[3])0x4dd2bc)
static SAMPLE *(*g_speaker_p)[3] = (SAMPLE *(*)[3])0x4dd2bc;

/* stars  VA=0x4facc8  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_stars (*(Tparticle (*)[512])0x4facc8)
static Tparticle (*g_stars_p)[512] = (Tparticle (*)[512])0x4facc8;

/* start_speeds  VA=0x4bc17c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_start_speeds (*(int (*)[6])0x4bc17c)
static int (*g_start_speeds_p)[6] = (int (*)[6])0x4bc17c;

/* stepIn  VA=0x4dd740  cu=F:\projects\icytower\trunk\source\menu.c */
#define IT_G_stepIn (*(int *)0x4dd740)
static int *g_stepIn_p = (int *)0x4dd740;

/* summary_scroller  VA=0x500108  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_summary_scroller (*(Tscroller *)0x500108)
static Tscroller *g_summary_scroller_p = (Tscroller *)0x500108;

/* summary_scroller_message  VA=0x4f8e78  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_summary_scroller_message (*(char (*)[5120])0x4f8e78)
static char (*g_summary_scroller_message_p)[5120] = (char (*)[5120])0x4f8e78;

/* swap_screen  VA=0x4dd194  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_swap_screen (*(BITMAP **)0x4dd194)
static BITMAP **g_swap_screen_p = (BITMAP **)0x4dd194;

/* testers  VA=0x4dd144  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_testers (*(Tbeta **)0x4dd144)
static Tbeta **g_testers_p = (Tbeta **)0x4dd144;

/* the_tester  VA=0x4dd148  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_the_tester (*(Tbeta **)0x4dd148)
static Tbeta **g_the_tester_p = (Tbeta **)0x4dd148;

/* tm_year_base  VA=0x4d80cc  cu=F:\projects\icytower\trunk\source\strptime.c */
#define IT_G_tm_year_base (*(const int *)0x4d80cc)
static const int *g_tm_year_base_p = (const int *)0x4d80cc;

/* uberChecksum  VA=0x4dd25c  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_uberChecksum (*(int *)0x4dd25c)
static int *g_uberChecksum_p = (int *)0x4dd25c;

/* value  VA=0x4dd32c  cu=F:\projects\icytower\trunk\source\main.c (static) */
#define IT_G_value__draw_progress_bar (*(int *)0x4dd32c)
static int *g_value__draw_progress_bar_p = (int *)0x4dd32c;

/* version_str  VA=0x4bc194  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_version_str (*(char **)0x4bc194)
static char **g_version_str_p = (char **)0x4bc194;

/* window  VA=0x4bc028  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_window (*(int *)0x4bc028)
static int *g_window_p = (int *)0x4bc028;

/* working_directory  VA=0x4fed08  cu=F:\projects\icytower\trunk\source\main.c */
#define IT_G_working_directory (*(char (*)[1024])0x4fed08)
static char (*g_working_directory_p)[1024] = (char (*)[1024])0x4fed08;

#endif /* IT_GLOBALS_H */
