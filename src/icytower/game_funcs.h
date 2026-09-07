/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_src_headers.py (which reuses carrier/gen/gen_interop.py's
 * DWARF parser and type IR -- see that file for the parsing itself) from:
 *   artifacts/dwarf_info.txt
 *   artifacts/functions.json
 * scope=game. Re-run carrier/gen/gen_src_headers.py to regenerate; do not hand-edit.
 *
 * Prototypes of every game-CU function, original names and DWARF
 * parameter names. This is the port's OWN header -- unlike
 * carrier/gen/it_funcs.h, no address, PFN_ typedef or IT_F_ macro ever
 * appears here; a function this port has recovered defines its body in
 * its own src/icytower/<name>.c, and everything else here is just a
 * plain forward declaration so those files can call each other and be
 * called, same as any ordinary C program.
 *
 * Calling convention is explicit only where DWARF/COFF evidence says it
 * is not the MSVC default __cdecl (see carrier/gen/INTEROP_NOTES.md
 * "Calling convention detection"); today that is WinMain alone
 * (__stdcall, verified from its COFF decoration _WinMain@16).
 *
 * Each prototype is wrapped in `#ifndef <name>`/`#endif`: when this
 * file is compiled INTO the carrier/harness world (win32_pilot.md
 * SS7a, ICYTOWER_BINDINGS_ACTIVE), carrier/gen/pf_bindings_src.h or
 * .../pf_bindings_harness.h is force-included first and #defines the
 * plain name of every game function NOT excluded (i.e. not yet
 * promoted into src/) to an address-cast expression -- declaring
 * such a name again here would macro-expand into a syntax error, not
 * a harmless redeclaration. A name IS still declared here whenever no
 * such macro exists: every promoted (--exclude-d) function, and every
 * name in the plain standalone world where no bindings header is
 * force-included at all.
 */
#ifndef ICYTOWER_GAME_FUNCS_H
#define ICYTOWER_GAME_FUNCS_H

#include "game_types.h"

/* ---- F:\projects\icytower\trunk\source\beta.c ---- */
#ifndef create_post
Tbeta * __cdecl create_post();
#endif
#ifndef destroy_all
void __cdecl destroy_all(Tbeta *b);
#endif
#ifndef garble_string
void __cdecl garble_string(char *str, int n);
#endif
#ifndef load_garbled_data
Tbeta * __cdecl load_garbled_data(char *filename);
#endif
#ifndef load_plain_data
Tbeta * __cdecl load_plain_data(char *filename);
#endif
#ifndef read_line
void __cdecl read_line(char *dst, it_orig_FILE *fp);
#endif
#ifndef save_garbled_data
int __cdecl save_garbled_data(Tbeta *b, char *filename);
#endif

/* ---- F:\projects\icytower\trunk\source\control.c ---- */
#ifndef check_control_key
int __cdecl check_control_key(Tcontrol *c, int key);
#endif
#ifndef get_gamepad
Tgamepad * __cdecl get_gamepad();
#endif
#ifndef init_control
void __cdecl init_control(Tcontrol *c);
#endif
#ifndef is_any
int __cdecl is_any(Tcontrol *c);
#endif
#ifndef is_down
int __cdecl is_down(Tcontrol *c);
#endif
#ifndef is_enter
int __cdecl is_enter(Tcontrol *c);
#endif
#ifndef is_fire
int __cdecl is_fire(Tcontrol *c);
#endif
#ifndef is_left
int __cdecl is_left(Tcontrol *c);
#endif
#ifndef is_pause
int __cdecl is_pause(Tcontrol *c);
#endif
#ifndef is_right
int __cdecl is_right(Tcontrol *c);
#endif
#ifndef is_up
int __cdecl is_up(Tcontrol *c);
#endif
#ifndef load_control
void __cdecl load_control(Tcontrol *c, it_orig_FILE *fp);
#endif
#ifndef poll_control
void __cdecl poll_control(Tcontrol *c, int joystick_only);
#endif
#ifndef save_control
void __cdecl save_control(Tcontrol *c, it_orig_FILE *fp);
#endif
#ifndef set_control
void __cdecl set_control(Tcontrol *c, int up, int down, int left, int right, int fire);
#endif

/* ---- F:\projects\icytower\trunk\source\csv.c ---- */
#ifndef csv_add_field
void __cdecl csv_add_field(CSVParseContext *pCtx, char *pStart);
#endif
#ifndef csv_begin
CSVParseContext * __cdecl csv_begin(const unsigned char *pData, it_orig_size_t iDataSize);
#endif
#ifndef csv_destroy
void __cdecl csv_destroy(CSVParseContext *pCtx);
#endif
#ifndef csv_next
int __cdecl csv_next(CSVParseContext *pCtx);
#endif
#ifndef csv_open
CSVParseContext * __cdecl csv_open(const char *pFilename);
#endif
#ifndef csv_rewind
void __cdecl csv_rewind(CSVParseContext *pCtx);
#endif

/* ---- F:\projects\icytower\trunk\source\custom.c ---- */
#ifndef clear_trailing_whitespace
void __cdecl clear_trailing_whitespace(char *data);
#endif
#ifndef custom_alert
void __cdecl custom_alert(char *txt1, char *txt2);
#endif
#ifndef destroy_custom_data
int __cdecl destroy_custom_data(Tcustom *c);
#endif
#ifndef get_string_data
char * __cdecl get_string_data(char *key, char *string);
#endif
#ifndef init_custom
int __cdecl init_custom(Tcustom *c, const char *name, int uses_datafile);
#endif
#ifndef loadCustomSoundDF
SAMPLE * __cdecl loadCustomSoundDF(DATAFILE *df, int id);
#endif
#ifndef loadCustomSoundFILE
SAMPLE * __cdecl loadCustomSoundFILE(char *tag, char *filename);
#endif
#ifndef load_character_bmp
BITMAP * __cdecl load_character_bmp(const char *name, int *uses_datafile, RGB *pal);
#endif
#ifndef load_frames
int __cdecl load_frames(Tcustom *c);
#endif
#ifndef load_sounds
int __cdecl load_sounds(Tcustom *c);
#endif

/* ---- F:\projects\icytower\trunk\source\directories.c ---- */
#ifndef get_adcache_dir
int __cdecl get_adcache_dir(char *buffer, it_orig_size_t buflen);
#endif
#ifndef get_character_dir
int __cdecl get_character_dir(char *buffer, it_orig_size_t buflen, const char *charactername);
#endif
#ifndef get_configfile_path
int __cdecl get_configfile_path(char *buffer, it_orig_size_t buflen);
#endif
#ifndef get_custom_characters_dir
int __cdecl get_custom_characters_dir(char *buffer, it_orig_size_t buflen);
#endif
#ifndef get_logfile_path
int __cdecl get_logfile_path(char *buffer, it_orig_size_t buflen);
#endif
#ifndef get_profile_dir_for_profile
int __cdecl get_profile_dir_for_profile(char *buffer, it_orig_size_t buflen, const char *profile);
#endif
#ifndef get_profiles_dir
int __cdecl get_profiles_dir(char *buffer, it_orig_size_t buflen);
#endif

/* ---- F:\projects\icytower\trunk\source\fld_adspot.c ---- */
#ifndef fldads_destroy_cache
void __cdecl fldads_destroy_cache();
#endif
#ifndef fldads_dump_local_cache
void __cdecl fldads_dump_local_cache();
#endif
#ifndef fldads_get_local_cache_name
const char * __cdecl fldads_get_local_cache_name(const char *pFileName);
#endif
#ifndef fldads_get_local_filename_from_url
const char * __cdecl fldads_get_local_filename_from_url(const char *pRemoteName);
#endif
#ifndef fldads_get_random_ad
const FLDAdSpot * __cdecl fldads_get_random_ad();
#endif
#ifndef fldads_load_cache_from_csv
void __cdecl fldads_load_cache_from_csv(CSVParseContext *pCsv);
#endif
#ifndef fldads_load_local_cache
void __cdecl fldads_load_local_cache();
#endif
#ifndef fldads_start
void __cdecl fldads_start();
#endif
#ifndef fldads_threadmain
void * __cdecl fldads_threadmain(void *data);
#endif
#ifndef fldads_update_cache
void __cdecl fldads_update_cache(unsigned char *pData, it_orig_size_t iDataSize);
#endif
#ifndef fldads_update_local_adimg
void __cdecl fldads_update_local_adimg(const char *pRemoteName);
#endif
#ifndef get_url_filename
const char * __cdecl get_url_filename(const char *pURL);
#endif

/* ---- F:\projects\icytower\trunk\source\game_data.c ---- */
#ifndef add_combo
void __cdecl add_combo(Tgame_data *gd, Tgd_combo *c);
#endif
#ifndef add_jump_sequence
void __cdecl add_jump_sequence(Tgame_data *gd, Tgd_jump_sequence *js);
#endif
#ifndef create_game_data
Tgame_data * __cdecl create_game_data();
#endif
#ifndef destroy_game_data
void __cdecl destroy_game_data(Tgame_data *gd);
#endif
#ifndef getGameDataXML
char * __cdecl getGameDataXML(Tgame_data *gd);
#endif

/* ---- F:\projects\icytower\trunk\source\hisc.c ---- */
#ifndef destroy_hisc_table
void __cdecl destroy_hisc_table(Thisc_table *table);
#endif
#ifndef draw_table
int __cdecl draw_table(BITMAP *dst, int x, int y, char *header, Thisc_table *table);
#endif
#ifndef enter_hisc_table
void __cdecl enter_hisc_table(Thisc_table *table, int value, char *name);
#endif
#ifndef generate_checksum
int __cdecl generate_checksum(Thisc *entry);
#endif
#ifndef load_hisc_table
int __cdecl load_hisc_table(Thisc_table *table, PACKFILE *fp);
#endif
#ifndef make_hisc_table
Thisc_table * __cdecl make_hisc_table(char *name);
#endif
#ifndef qualify_hisc_table
int __cdecl qualify_hisc_table(Thisc_table *table, int value);
#endif
#ifndef reset_hisc_table
void __cdecl reset_hisc_table(Thisc_table *table, char *name, int hi, int lo);
#endif
#ifndef save_hisc_table
void __cdecl save_hisc_table(Thisc_table *table, PACKFILE *fp);
#endif
#ifndef sort_hisc_table
void __cdecl sort_hisc_table(Thisc_table *table);
#endif
#ifndef view_scores
void __cdecl view_scores(Thisc_table **tables, char **names);
#endif

/* ---- F:\projects\icytower\trunk\source\httpget.c ---- */
#ifndef HTTPFetchInternal
HTTPResponse * __cdecl HTTPFetchInternal(const char *pHost, int iPort, const char *pPathToFile, const char *pMethod);
#endif
#ifndef HTTPGet
HTTPResponse * __cdecl HTTPGet(const char *pURL);
#endif
#ifndef HTTPHead
HTTPResponse * __cdecl HTTPHead(const char *pURL);
#endif
#ifndef HTTPRequest
HTTPResponse * __cdecl HTTPRequest(const char *pURL, const char *pMethod);
#endif
#ifndef SplitURL
int __cdecl SplitURL(const char *pURL, char **ppHost, char **ppPath, int *piPort);
#endif
#ifndef destroyHTTPResponse
void __cdecl destroyHTTPResponse(HTTPResponse *pResponse);
#endif
#ifndef dumpHTTPResponse
void __cdecl dumpHTTPResponse(it_orig_FILE *pOut, HTTPResponse *pResponse);
#endif
#ifndef extractHTTPResponse
HTTPResponse * __cdecl extractHTTPResponse(const unsigned char *pHTTPData, int iResponseBytesCount);  /* static in F:\projects\icytower\trunk\source\httpget.c */
#endif
#ifndef getSocketError
int __cdecl getSocketError();
#endif
#ifndef httpGetLastModified
it_orig_time_t __cdecl httpGetLastModified(HTTPResponse *pResponse);
#endif

/* ---- F:\projects\icytower\trunk\source\loadpng.c ---- */
#ifndef load_memory_png
BITMAP * __cdecl load_memory_png(const void *buffer, int bufsize, RGB *pal);
#endif
#ifndef load_png
BITMAP * __cdecl load_png(const char *filename, RGB *pal);
#endif
#ifndef load_png_pf
BITMAP * __cdecl load_png_pf(PACKFILE *fp, RGB *pal);
#endif
#ifndef read_data
void __cdecl read_data(png_structp png_ptr, png_bytep data, png_uint_32 length);  /* static in F:\projects\icytower\trunk\source\loadpng.c */
#endif
#ifndef read_data_memory
void __cdecl read_data_memory(png_structp png_ptr, png_bytep data, png_uint_32 length);  /* static in F:\projects\icytower\trunk\source\loadpng.c */
#endif
#ifndef really_load_png
BITMAP * __cdecl really_load_png(png_structp png_ptr, png_infop info_ptr, RGB *pal);  /* static in F:\projects\icytower\trunk\source\loadpng.c */
#endif

/* ---- F:\projects\icytower\trunk\source\main.c ---- */
#ifndef WinMain
int __stdcall WinMain(void *hInst, void *hPrev, char *Cmd, int nShow);
#endif
#ifndef _mangled_main
int __cdecl _mangled_main(int argc, char **argv);
#endif
#ifndef add_profile
int __cdecl add_profile(const char *filename, int attrib, void *param);
#endif
#ifndef blit_to_screen
void __cdecl blit_to_screen(BITMAP *bmp);
#endif
#ifndef change_profile
void __cdecl change_profile();
#endif
#ifndef checkMenuFocus
void __cdecl checkMenuFocus();
#endif
#ifndef check_beta_tester
int __cdecl check_beta_tester();
#endif
#ifndef check_characters
int __cdecl check_characters();
#endif
#ifndef check_dir
int __cdecl check_dir(const char *filename, int attrib, void *param);
#endif
#ifndef clickedCloseButton
void __cdecl clickedCloseButton();
#endif
#ifndef color_map_callback
void __cdecl color_map_callback(int p);
#endif
#ifndef datafile_callback
void __cdecl datafile_callback(DATAFILE *d);
#endif
#ifndef datafile_callback_slow
void __cdecl datafile_callback_slow(DATAFILE *d);
#endif
#ifndef do_replay_menu
int __cdecl do_replay_menu();
#endif
#ifndef drawSlot
void __cdecl drawSlot(BITMAP *dst, int x, int y, char *title, char *text, int color);
#endif
#ifndef draw_frame
void __cdecl draw_frame(BITMAP *bmp);
#endif
#ifndef draw_progress_bar
void __cdecl draw_progress_bar();
#endif
#ifndef draw_results
void __cdecl draw_results(BITMAP *bmp, BITMAP *logo, int y, int *qualified, int *qValues, int showQ);
#endif
#ifndef draw_reward
void __cdecl draw_reward(BITMAP *bmp);
#endif
#ifndef end_game
void __cdecl end_game();
#endif
#ifndef fadeIn
void __cdecl fadeIn(BITMAP *bmp, int speed);
#endif
#ifndef fadeOut
void __cdecl fadeOut(int speed);
#endif
#ifndef for_each_directory
void __cdecl for_each_directory(const char *basedir, int (__cdecl *cb)(const char *, int, void *));
#endif
#ifndef force_create_profile
void __cdecl force_create_profile();
#endif
#ifndef getSampleFromOggDatafile
SAMPLE * __cdecl getSampleFromOggDatafile(DATAFILE *df, int id);
#endif
#ifndef get_controls
Tcontrol * __cdecl get_controls();
#endif
#ifndef get_demo
Treplay * __cdecl get_demo();
#endif
#ifndef get_gamepad_value
int __cdecl get_gamepad_value(char *dir);
#endif
#ifndef get_string
int __cdecl get_string(BITMAP *bmp, char *string, int w, int max_chars, FONT *f, int pos_x, int pos_y, int colour, int bg_color);
#endif
#ifndef get_version_str
char * __cdecl get_version_str();
#endif
#ifndef handle_player_collision_combo
void __cdecl handle_player_collision_combo(int lastX, int lastY);
#endif
#ifndef handle_player_collision_old
void __cdecl handle_player_collision_old(int lastX, int lastY);
#endif
#ifndef handle_player_collision_original
void __cdecl handle_player_collision_original(int lastX, int lastY);
#endif
#ifndef handle_player_collision_vector
void __cdecl handle_player_collision_vector(int lastX, int lastY);
#endif
#ifndef handle_player_collision_vector_2
void __cdecl handle_player_collision_vector_2(int lastX, int lastY);
#endif
#ifndef handle_player_input
void __cdecl handle_player_input(Tcontrol *ctrl);
#endif
#ifndef init_game
int __cdecl init_game(int argc, char **argv);
#endif
#ifndef is_custom_replay
int __cdecl is_custom_replay(Treplay *rep);
#endif
#ifndef line_alert
void __cdecl line_alert(char *txt);
#endif
#ifndef line_intersect
int __cdecl line_intersect(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy, int *ix, int *iy);
#endif
#ifndef loadScrambled
BITMAP * __cdecl loadScrambled(char *fileName);
#endif
#ifndef load_character
int __cdecl load_character(const char *filename, int attrib, void *param);
#endif
#ifndef load_new_ad_image
void __cdecl load_new_ad_image();
#endif
#ifndef load_sound
void __cdecl load_sound(SAMPLE **dest, char *fname, BITMAP *bmp, int y);
#endif
#ifndef log2file
void __cdecl log2file(const char *format, ...);
#endif
#ifndef main_menu_callback
void __cdecl main_menu_callback(void);
#endif
#ifndef myDeleteFile
void __cdecl myDeleteFile(char *path, char *file);
#endif
#ifndef my_alert
int __cdecl my_alert(char *func, char *txt, int choice, int enter_hint);
#endif
#ifndef new_game
int __cdecl new_game();
#endif
#ifndef new_rand
int __cdecl new_rand();
#endif
#ifndef new_srand
void __cdecl new_srand(int s);
#endif
#ifndef ok_to_play
int __cdecl ok_to_play();
#endif
#ifndef open_web_browser
void __cdecl open_web_browser(const char *pURL);
#endif
#ifndef play
int __cdecl play();
#endif
#ifndef play_jump_sound
void __cdecl play_jump_sound(Tplayer *p);
#endif
#ifndef play_menu_move
void __cdecl play_menu_move();
#endif
#ifndef play_menu_select
void __cdecl play_menu_select();
#endif
#ifndef play_sound
void __cdecl play_sound(SAMPLE *s, int pitch, int please_pan);
#endif
#ifndef pwd_garble_string
void __cdecl pwd_garble_string(char *str, int key);
#endif
#ifndef rebuild_profile_list
int __cdecl rebuild_profile_list(Tavailable_profile **profs);
#endif
#ifndef replaceBadCharacters
void __cdecl replaceBadCharacters(char *string, char newChar);
#endif
#ifndef replay_menu_callback
void __cdecl replay_menu_callback(void);
#endif
#ifndef run_demo
void __cdecl run_demo(char *file_name);
#endif
#ifndef save_config
void __cdecl save_config();
#endif
#ifndef set_current_avatar
void __cdecl set_current_avatar();
#endif
#ifndef show_credits
void __cdecl show_credits();
#endif
#ifndef show_instructions
void __cdecl show_instructions();
#endif
#ifndef show_name
int __cdecl show_name(const char *filename, int attrib, void *param);
#endif
#ifndef startGameMusic
void __cdecl startGameMusic();
#endif
#ifndef startMenuMusic
void __cdecl startMenuMusic();
#endif
#ifndef start_reward
int __cdecl start_reward(int lev);
#endif
#ifndef stopGameMusic
void __cdecl stopGameMusic();
#endif
#ifndef stopMenuMusic
void __cdecl stopMenuMusic();
#endif
#ifndef switchedFromProgram
void __cdecl switchedFromProgram();
#endif
#ifndef switchedToProgram
void __cdecl switchedToProgram();
#endif
#ifndef syncOptionsFromProfile
void __cdecl syncOptionsFromProfile();
#endif
#ifndef syncProfileFromOptions
void __cdecl syncProfileFromOptions();
#endif
#ifndef take_screenshot
void __cdecl take_screenshot(BITMAP *bmp);
#endif
#ifndef testWindowResolution
void __cdecl testWindowResolution();
#endif
#ifndef uninit_game
void __cdecl uninit_game();
#endif
#ifndef update_frame
void __cdecl update_frame();
#endif
#ifndef update_reward
void __cdecl update_reward();
#endif

/* ---- F:\projects\icytower\trunk\source\map.c ---- */
#ifndef add_floor
void __cdecl add_floor(Tmap *m);
#endif
#ifndef getFloorData
void __cdecl getFloorData(Tmap *m, int cy, int *fy, int *fx1, int *fx2);
#endif
#ifndef get_level
int __cdecl get_level(Tmap *m, int cy);
#endif
#ifndef is_solid
int __cdecl is_solid(Tmap *m, int cx, int cy);
#endif
#ifndef reset_map
void __cdecl reset_map(Tmap *m);
#endif

/* ---- F:\projects\icytower\trunk\source\menu.c ---- */
#ifndef build_menu_string
void __cdecl build_menu_string(Tmenu *m, char *dest);
#endif
#ifndef draw_menu
void __cdecl draw_menu(BITMAP *bmp, Tmenu *m, Tmenu_params *mp, int cx, int y, int dx);
#endif
#ifndef get_selection_value
int __cdecl get_selection_value(Tmenu_selection *s);
#endif
#ifndef get_slider_value
int __cdecl get_slider_value(Tmenu_slider *s);
#endif
#ifndef handle_menu
int __cdecl handle_menu(Tmenu *menu, Tmenu_params *mp, Tcontrol *ctrl, BITMAP *bmp, void (__cdecl *callback)(void), int x, int y, int dx);
#endif
#ifndef key_to_str
void __cdecl key_to_str(int k, char *dest);
#endif
#ifndef reset_menu
void __cdecl reset_menu(Tmenu *m, Tmenu_params *mp, int sel_pos);
#endif
#ifndef set_selection_value
int __cdecl set_selection_value(Tmenu_selection *s, int v);
#endif
#ifndef set_slider_value
int __cdecl set_slider_value(Tmenu_slider *s, int v);
#endif
#ifndef update_game_menu
int __cdecl update_game_menu(BITMAP *bmp, Tmenu *m, Tmenu_params *mp, Tcontrol *ctrl, int x, int y, void **data);
#endif

/* ---- F:\projects\icytower\trunk\source\options.c ---- */
#ifndef generate_options_checksum
int __cdecl generate_options_checksum(Toptions *o);
#endif
#ifndef hash3
unsigned int __cdecl hash3(unsigned int a);
#endif
#ifndef load_options
void __cdecl load_options(Toptions *o, PACKFILE *fp);
#endif
#ifndef reset_options
void __cdecl reset_options(Toptions *o);
#endif
#ifndef save_options
void __cdecl save_options(Toptions *o, PACKFILE *fp);
#endif

/* ---- F:\projects\icytower\trunk\source\particle.c ---- */
#ifndef create_particle
int __cdecl create_particle(Tparticle *p, int x, int y);
#endif
#ifndef reset_particles
void __cdecl reset_particles(Tparticle *p);
#endif
#ifndef update_particle
void __cdecl update_particle(Tparticle *p);
#endif

/* ---- F:\projects\icytower\trunk\source\player.c ---- */
#ifndef jump_player
int __cdecl jump_player(Tplayer *p, int cheat);
#endif
#ifndef reset_player
void __cdecl reset_player(Tplayer *p);
#endif
#ifndef update_player
void __cdecl update_player(Tplayer *p);
#endif

/* ---- F:\projects\icytower\trunk\source\profile.c ---- */
#ifndef create_profile
Tprofile * __cdecl create_profile(char *handle, int overwrite);
#endif
#ifndef delete_profile
void __cdecl delete_profile(char *handle);
#endif
#ifndef draw_buffer
int __cdecl draw_buffer(BITMAP *bmp, char *buffer, int x, int y);
#endif
#ifndef draw_profile_selector
void __cdecl draw_profile_selector(BITMAP *bmp, Tprofile *current_profile, Tavailable_profile *profiles, int numProfiles, int selection, int offset, int max_posts, int x, int y);
#endif
#ifndef generate_profile_checksum
int __cdecl generate_profile_checksum(Tprofile *p);
#endif
#ifndef get_rank
char * __cdecl get_rank(Tprofile *profile);
#endif
#ifndef get_rank_id
int __cdecl get_rank_id(Tprofile *profile);
#endif
#ifndef hash2
unsigned int __cdecl hash2(unsigned int a);
#endif
#ifndef load_profile
Tprofile * __cdecl load_profile(char *handle);
#endif
#ifndef profile_data_page_advanced
char * __cdecl profile_data_page_advanced(Tprofile *p);
#endif
#ifndef profile_data_page_basic
char * __cdecl profile_data_page_basic(Tprofile *p);
#endif
#ifndef profile_data_page_extra
char * __cdecl profile_data_page_extra(Tprofile *p);
#endif
#ifndef profile_data_page_general
char * __cdecl profile_data_page_general(Tprofile *p, char *filler);
#endif
#ifndef save_profile
int __cdecl save_profile(Tprofile *p);
#endif
#ifndef select_profile
Tprofile * __cdecl select_profile(Tprofile *current_profile, Tavailable_profile *profiles, int numProfiles, Tcontrol *ctrl);
#endif
#ifndef set_next_rank_message
void __cdecl set_next_rank_message(char *buf, Tprofile *p);
#endif
#ifndef view_profile
int __cdecl view_profile(Tprofile *profile);
#endif

/* ---- F:\projects\icytower\trunk\source\regpng.c ---- */
#ifndef destroy_datafile_png
void __cdecl destroy_datafile_png(void *data);  /* static in F:\projects\icytower\trunk\source\regpng.c */
#endif
#ifndef load_datafile_png
void * __cdecl load_datafile_png(PACKFILE *f, long size);  /* static in F:\projects\icytower\trunk\source\regpng.c */
#endif
#ifndef loadpng_init
int __cdecl loadpng_init(void);
#endif
#ifndef register_png_datafile_object
void __cdecl register_png_datafile_object(int id);
#endif
#ifndef register_png_file_type
void __cdecl register_png_file_type(void);
#endif

/* ---- F:\projects\icytower\trunk\source\replay.c ---- */
#ifndef add_itr_file
int __cdecl add_itr_file(const char *filename, int attrib, void *param);
#endif
#ifndef calc_replay_checksum
int __cdecl calc_replay_checksum(Treplay *r);
#endif
#ifndef calc_replay_checksum_131
int __cdecl calc_replay_checksum_131(Treplay *r);
#endif
#ifndef create_replay
Treplay * __cdecl create_replay(int size);
#endif
#ifndef destroy_replay
void __cdecl destroy_replay(Treplay *r);
#endif
#ifndef draw_replay_selector
void __cdecl draw_replay_selector(BITMAP *bmp, Treplay *rep, Treplay_post *file_list, int selection, int offset, int max_posts, int x, int y);
#endif
#ifndef get_replay_property
int __cdecl get_replay_property(const char *filename, int property);
#endif
#ifndef get_sort_method
int __cdecl get_sort_method();
#endif
#ifndef hash
unsigned int __cdecl hash(unsigned int a);
#endif
#ifndef load_replay
Treplay * __cdecl load_replay(const char *filename);
#endif
#ifndef my_strcmp
int __cdecl my_strcmp(const void *c, const void *d);
#endif
#ifndef replay_selector
Treplay * __cdecl replay_selector(Tcontrol *ctrl, char *path);
#endif
#ifndef save_replay
int __cdecl save_replay(const char *path, const char *file, Treplay *r, int size, int make_new_date);
#endif
#ifndef set_sort_method
void __cdecl set_sort_method(int sm);
#endif
#ifndef update_file_list
void __cdecl update_file_list(char *path);
#endif

/* ---- F:\projects\icytower\trunk\source\savepng.c ---- */
#ifndef flush_data
void __cdecl flush_data(png_structp png_ptr);  /* static in F:\projects\icytower\trunk\source\savepng.c */
#endif
#ifndef really_save_png
int __cdecl really_save_png(PACKFILE *fp, BITMAP *bmp, const RGB *pal);  /* static in F:\projects\icytower\trunk\source\savepng.c */
#endif
#ifndef save_png
int __cdecl save_png(const char *filename, BITMAP *bmp, const RGB *pal);
#endif
#ifndef write_data
void __cdecl write_data(png_structp png_ptr, png_bytep data, png_uint_32 length);  /* static in F:\projects\icytower\trunk\source\savepng.c */
#endif

/* ---- F:\projects\icytower\trunk\source\scroller.c ---- */
#ifndef draw_scroller
int __cdecl draw_scroller(Tscroller *sc, BITMAP *bmp, int x, int y, int color);
#endif
#ifndef init_scroller
void __cdecl init_scroller(Tscroller *sc, FONT *f, char *t, int w, int h, int horiz);
#endif
#ifndef restart_scroller
void __cdecl restart_scroller(Tscroller *sc);
#endif
#ifndef scroll_scroller
void __cdecl scroll_scroller(Tscroller *sc, int step);
#endif

/* ---- F:\projects\icytower\trunk\source\stars.c ---- */
#ifndef draw_star_field
void __cdecl draw_star_field(Tstar_field *sf, BITMAP *bmp, int x, int y);
#endif
#ifndef init_star_field
void __cdecl init_star_field(Tstar_field *sf, int w, int h, int num, int first_col, int last_col, int dep, int cc);
#endif
#ifndef scroll_star_field
void __cdecl scroll_star_field(Tstar_field *sf, double xstep, double ystep);
#endif

/* ---- F:\projects\icytower\trunk\source\strptime.c ---- */
#ifndef _strptime
char * __cdecl _strptime(const char *buf, const char *format, struct it_orig_tm *timeptr, int *gmt);  /* static in F:\projects\icytower\trunk\source\strptime.c */
#endif
#ifndef first_day
int __cdecl first_day(int year);  /* static in F:\projects\icytower\trunk\source\strptime.c */
#endif
#ifndef match_string
int __cdecl match_string(const char **buf, const char **strs);  /* static in F:\projects\icytower\trunk\source\strptime.c */
#endif
#ifndef strptime
char * __cdecl strptime(const char *buf, const char *format, struct it_orig_tm *timeptr);
#endif

/* ---- F:\projects\icytower\trunk\source\timecompat.c ---- */
#ifndef timegm
it_orig_time_t __cdecl timegm(struct it_orig_tm *ptm);
#endif

/* ---- F:\projects\icytower\trunk\source\timer.c ---- */
#ifndef cycle_counter
void __cdecl cycle_counter(void);
#endif
#ifndef fps_counter
void __cdecl fps_counter(void);
#endif
#ifndef install_timers
int __cdecl install_timers();
#endif

#endif /* ICYTOWER_GAME_FUNCS_H */
