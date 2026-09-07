/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_src_headers.py (which reuses carrier/gen/gen_interop.py's
 * DWARF parser and type IR -- see that file for the parsing itself) from:
 *   artifacts/dwarf_info.txt
 *   artifacts/functions.json
 * scope=game. Re-run carrier/gen/gen_src_headers.py to regenerate; do not hand-edit.
 *
 * Ordinary extern declarations of every game-CU global (win32_pilot.md
 * SS7a). No address appears here or anywhere else in src/: each name is
 * declared exactly as an application would declare a global it does not
 * own the storage for. Two things give it real storage and a real
 * address, depending on which world this file is compiled into:
 *
 *   - INTO THE CARRIER: the generated bindings header
 *     (carrier/gen/pf_bindings_src.h, or its PF_MEM-wrapped harness
 *     twin) is force-included (/FI) ahead of every other token in the
 *     translation unit. It #defines each of these names to
 *     (*(T*)original_address), so an extern re-declaration of an
 *     already-macro-expanded name would not parse. The ICYTOWER_BINDINGS_ACTIVE
 *     guard, defined by that same generated header, is what lets this
 *     file detect that and skip its own declarations.
 *   - STANDALONE: no bindings header is force-included, so the guard
 *     is undefined, the extern declarations below are the only
 *     declaration of these names, and state.c supplies their storage
 *     (win32_pilot.md SS7a: "a state.c defines the globals and the
 *     bindings header is absent").
 *
 * A global whose original C source declared it `static` (file scope or
 * function-local) is marked as such in its comment -- that is a note
 * about the ORIGINAL program's linkage, kept for provenance; every name
 * below is still declared `extern` here uniformly and given storage in
 * state.c, exactly like carrier/gen/it_globals.h treats the same
 * distinction as a comment rather than a different declaration.
 * A name is disambiguated with a __<file> suffix only where two
 * different game CUs actually collide on it (see GENERATED.md).
 */
#ifndef ICYTOWER_GAME_STATE_H
#define ICYTOWER_GAME_STATE_H

#include "game_types.h"

#ifndef ICYTOWER_BINDINGS_ACTIVE

/* ---- F:\projects\icytower\trunk\source\control.c ---- */
extern Tgamepad gamepad;

/* ---- F:\projects\icytower\trunk\source\custom.c ---- */
extern RGB black;
extern RGB pink;

/* ---- F:\projects\icytower\trunk\source\fld_adspot.c ---- */
extern pthread_mutex_t gFLDADMutex;
extern pthread_t gFLDADThread;
extern int giAdCacheSize;
extern FLDAdSpot *gpAdCache;
extern char localFilename__fldads_get_local_cache_name[256];  /* static local in fldads_get_local_cache_name(), F:\projects\icytower\trunk\source\fld_adspot.c */

/* ---- F:\projects\icytower\trunk\source\loadpng.c ---- */
extern int _png_compression_level;
extern double _png_screen_gamma;

/* ---- F:\projects\icytower\trunk\source\main.c ---- */
extern int any11;
extern int any12;
extern int any13;
extern int any21;
extern int any22;
extern int any23;
extern SAMPLE *bg_beat;
extern SAMPLE *bg_menu;
extern int bg_stripe_ids[5];
extern int blit_mode__blit_to_screen;  /* static local in blit_to_screen(), F:\projects\icytower\trunk\source\main.c */
extern char *category_names[15];
extern Tcharacter *characters;
extern int checkMusicVoiceID;
extern int clock_angle;
extern int closeButtonClicked;
extern Tcommandline cmdline;
extern int collision_type;
extern SAMPLE *combo_sound[10];
extern int count__load_character;  /* static local in load_character(), F:\projects\icytower\trunk\source\main.c */
extern int count__main_menu_callback;  /* static local in main_menu_callback(), F:\projects\icytower\trunk\source\main.c */
extern Tcontrol ctrl;
extern Tmenu ctrl_menu[6];
extern int curr_char;
extern Tcustom custom;
extern Tmenu custom_menu[5];
extern int cycle_loops;
extern DATAFILE *data;
extern int debug;
extern Treplay *demo;
extern int dropped_file_is_not_a_replay;
extern Tmenu_selection eyecandy_selection;
extern int face__main_menu_callback;  /* static local in main_menu_callback(), F:\projects\icytower\trunk\source\main.c */
extern int fall_count;
extern int fast_fast_forward;
extern int fast_forward;
extern Tmenu_selection floor_size_selection;
extern Tmenu_floor_selection floors;
extern Tgame_data *gameData;
extern int gameMusicVoiceID;
extern Tmenu game_menu[1];
extern BITMAP *gameover_bmp;
extern int gdComboStart;
extern int gdLastJumpDiff;
extern Tmenu gfx_menu[5];
extern int got_joystick;
extern Tmenu_selection gravity_selection;
extern Tscroller greeting_scroller;
extern int hasFocus;
extern char *hints[45];
extern char *hisc_names[15];
extern Thisc_table *hisc_tables[15];
extern int hurry_y;
extern int in_replay_menu;
extern int init_ok;
extern char init_string[7];
extern int is_playing_custom_game;
extern int itrcheck;
extern Tgd_jump_sequence jumpSequence;
extern SAMPLE *jump_sound[3];
extern int lastFocus;
extern int lastMouseB;
extern char last_log[256];
extern int last_stripe_y;
extern char logfilename__log2file[1024];  /* static local in log2file(), F:\projects\icytower\trunk\source\main.c */
extern Tmenu main_menu[7];
extern Tmap map;
extern Tmenu_params menu_params;
extern SAMPLE *menu_sounds[2];
extern Tmenu_slider msc_volume_slider;
extern int new_personal_best[15];
extern int numProfiles;
extern int num_chars;
extern int number__take_screenshot;  /* static local in take_screenshot(), F:\projects\icytower\trunk\source\main.c */
extern Tmenu opt_menu[4];
extern Toptions options;
extern const FLDAdSpot *pFLDAd;
extern BITMAP *pFLDAdBitmap;
extern int p__datafile_callback_slow;  /* static local in datafile_callback_slow(), F:\projects\icytower\trunk\source\main.c */
extern Tmenu_char_selection play_char;
extern Tmenu play_menu[3];
extern int player_id;
extern Tplayer *ply[1000];
extern BITMAP *poster;
extern Tprofile *profile;
extern Tmenu profile_menu[3];
extern Tavailable_profile *profiles;
extern int rec_pos;
extern int rec_seed;
extern int recording;
extern int rejump;
extern char replay_directory[1024];
extern Tmenu replay_menu[5];
extern BITMAP *reward_bmp;
extern fixed reward_scale;
extern int reward_time;
extern pthread_mutex_t sLogMutex__log2file;  /* static local in log2file(), F:\projects\icytower\trunk\source\main.c */
extern int scroll_count;
extern int scroll_delay;
extern Tmenu_selection scroll_speed_selection;
extern char scroller_greetings[156];
extern double seed;
extern DATAFILE *sfx;
extern char sfx_file[512];
extern Tmenu snd_menu[3];
extern Tmenu_slider snd_volume_slider;
extern int someCounter__play;  /* static local in play(), F:\projects\icytower\trunk\source\main.c */
extern SAMPLE *sounds[9];
extern SAMPLE *speaker[3];
extern Tparticle stars[512];
extern int start_speeds[6];
extern Tscroller summary_scroller;
extern char summary_scroller_message[5120];
extern BITMAP *swap_screen;
extern Tbeta *testers;
extern Tbeta *the_tester;
extern int uberChecksum;
extern int value__draw_progress_bar;  /* static local in draw_progress_bar(), F:\projects\icytower\trunk\source\main.c */
extern char *version_str;
extern int window;
extern char working_directory[1024];

/* ---- F:\projects\icytower\trunk\source\map.c ---- */
extern int floor_size_modifiers[5];

/* ---- F:\projects\icytower\trunk\source\menu.c ---- */
extern int stepIn;

/* ---- F:\projects\icytower\trunk\source\player.c ---- */
extern double gravity_modifier[3];
extern double max_speed[5];

/* ---- F:\projects\icytower\trunk\source\profile.c ---- */
extern char *comboNames[10];
extern char *jcLabels[5];
extern int rankCCCs[12];
extern int rankCombos[12];
extern int rankFloors[12];
extern char *rankLables[12];
extern int rankNMLs[12];

/* ---- F:\projects\icytower\trunk\source\replay.c ---- */
extern const char REPLAY_HEADER[6];  /* static in F:\projects\icytower\trunk\source\replay.c */
extern Treplay_post itr_file_list[1024];
extern int num_itr_files;
extern int sort_method;

/* ---- F:\projects\icytower\trunk\source\strptime.c ---- */
extern const char *abb_month[13];  /* static in F:\projects\icytower\trunk\source\strptime.c */
extern const char *abb_weekdays[8];  /* static in F:\projects\icytower\trunk\source\strptime.c */
extern const char *ampm[3];  /* static in F:\projects\icytower\trunk\source\strptime.c */
extern const char *full_month[13];  /* static in F:\projects\icytower\trunk\source\strptime.c */
extern const char *full_weekdays[8];  /* static in F:\projects\icytower\trunk\source\strptime.c */
extern const int tm_year_base;

/* ---- F:\projects\icytower\trunk\source\timer.c ---- */
extern volatile int cycle_count;
extern volatile int fps;
extern volatile int frame_count;
extern volatile int logic_count;
extern volatile int lps;

#endif /* !ICYTOWER_BINDINGS_ACTIVE */

#endif /* ICYTOWER_GAME_STATE_H */
