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
 */
#ifndef ICYTOWER_GAME_FUNCS_H
#define ICYTOWER_GAME_FUNCS_H

#include "game_types.h"

/* ---- F:\projects\icytower\trunk\source\beta.c ---- */
Tbeta * __cdecl create_post();
void __cdecl destroy_all(Tbeta *b);
void __cdecl garble_string(char *str, int n);
Tbeta * __cdecl load_garbled_data(char *filename);
Tbeta * __cdecl load_plain_data(char *filename);
void __cdecl read_line(char *dst, it_orig_FILE *fp);
int __cdecl save_garbled_data(Tbeta *b, char *filename);

/* ---- F:\projects\icytower\trunk\source\control.c ---- */
int __cdecl check_control_key(Tcontrol *c, int key);
Tgamepad * __cdecl get_gamepad();
void __cdecl init_control(Tcontrol *c);
int __cdecl is_any(Tcontrol *c);
int __cdecl is_down(Tcontrol *c);
int __cdecl is_enter(Tcontrol *c);
int __cdecl is_fire(Tcontrol *c);
int __cdecl is_left(Tcontrol *c);
int __cdecl is_pause(Tcontrol *c);
int __cdecl is_right(Tcontrol *c);
int __cdecl is_up(Tcontrol *c);
void __cdecl load_control(Tcontrol *c, it_orig_FILE *fp);
void __cdecl poll_control(Tcontrol *c, int joystick_only);
void __cdecl save_control(Tcontrol *c, it_orig_FILE *fp);

/* ---- F:\projects\icytower\trunk\source\csv.c ---- */
void __cdecl csv_add_field(CSVParseContext *pCtx, char *pStart);
CSVParseContext * __cdecl csv_begin(const unsigned char *pData, it_orig_size_t iDataSize);
void __cdecl csv_destroy(CSVParseContext *pCtx);
int __cdecl csv_next(CSVParseContext *pCtx);
CSVParseContext * __cdecl csv_open(const char *pFilename);
void __cdecl csv_rewind(CSVParseContext *pCtx);

/* ---- F:\projects\icytower\trunk\source\custom.c ---- */
void __cdecl clear_trailing_whitespace(char *data);
void __cdecl custom_alert(char *txt1, char *txt2);
int __cdecl destroy_custom_data(Tcustom *c);
char * __cdecl get_string_data(char *key, char *string);
int __cdecl init_custom(Tcustom *c, const char *name, int uses_datafile);
SAMPLE * __cdecl loadCustomSoundDF(DATAFILE *df, int id);
SAMPLE * __cdecl loadCustomSoundFILE(char *tag, char *filename);
BITMAP * __cdecl load_character_bmp(const char *name, int *uses_datafile, RGB *pal);
int __cdecl load_frames(Tcustom *c);
int __cdecl load_sounds(Tcustom *c);

/* ---- F:\projects\icytower\trunk\source\directories.c ---- */
int __cdecl get_adcache_dir(char *buffer, it_orig_size_t buflen);
int __cdecl get_character_dir(char *buffer, it_orig_size_t buflen, const char *charactername);
int __cdecl get_configfile_path(char *buffer, it_orig_size_t buflen);
int __cdecl get_custom_characters_dir(char *buffer, it_orig_size_t buflen);
int __cdecl get_logfile_path(char *buffer, it_orig_size_t buflen);
int __cdecl get_profile_dir_for_profile(char *buffer, it_orig_size_t buflen, const char *profile);
int __cdecl get_profiles_dir(char *buffer, it_orig_size_t buflen);

/* ---- F:\projects\icytower\trunk\source\fld_adspot.c ---- */
void __cdecl fldads_destroy_cache();
void __cdecl fldads_dump_local_cache();
const char * __cdecl fldads_get_local_cache_name(const char *pFileName);
const char * __cdecl fldads_get_local_filename_from_url(const char *pRemoteName);
const FLDAdSpot * __cdecl fldads_get_random_ad();
void __cdecl fldads_load_cache_from_csv(CSVParseContext *pCsv);
void __cdecl fldads_load_local_cache();
void __cdecl fldads_start();
void * __cdecl fldads_threadmain(void *data);
void __cdecl fldads_update_cache(unsigned char *pData, it_orig_size_t iDataSize);
void __cdecl fldads_update_local_adimg(const char *pRemoteName);
const char * __cdecl get_url_filename(const char *pURL);

/* ---- F:\projects\icytower\trunk\source\game_data.c ---- */
void __cdecl add_combo(Tgame_data *gd, Tgd_combo *c);
void __cdecl add_jump_sequence(Tgame_data *gd, Tgd_jump_sequence *js);
Tgame_data * __cdecl create_game_data();
void __cdecl destroy_game_data(Tgame_data *gd);
char * __cdecl getGameDataXML(Tgame_data *gd);

/* ---- F:\projects\icytower\trunk\source\hisc.c ---- */
void __cdecl destroy_hisc_table(Thisc_table *table);
int __cdecl draw_table(BITMAP *dst, int x, int y, char *header, Thisc_table *table);
void __cdecl enter_hisc_table(Thisc_table *table, int value, char *name);
int __cdecl load_hisc_table(Thisc_table *table, PACKFILE *fp);
Thisc_table * __cdecl make_hisc_table(char *name);
int __cdecl qualify_hisc_table(Thisc_table *table, int value);
void __cdecl reset_hisc_table(Thisc_table *table, char *name, int hi, int lo);
void __cdecl save_hisc_table(Thisc_table *table, PACKFILE *fp);
void __cdecl sort_hisc_table(Thisc_table *table);
void __cdecl view_scores(Thisc_table **tables, char **names);

/* ---- F:\projects\icytower\trunk\source\httpget.c ---- */
HTTPResponse * __cdecl HTTPFetchInternal(const char *pHost, int iPort, const char *pPathToFile, const char *pMethod);
HTTPResponse * __cdecl HTTPGet(const char *pURL);
HTTPResponse * __cdecl HTTPHead(const char *pURL);
HTTPResponse * __cdecl HTTPRequest(const char *pURL, const char *pMethod);
int __cdecl SplitURL(const char *pURL, char **ppHost, char **ppPath, int *piPort);
void __cdecl destroyHTTPResponse(HTTPResponse *pResponse);
void __cdecl dumpHTTPResponse(it_orig_FILE *pOut, HTTPResponse *pResponse);
HTTPResponse * __cdecl extractHTTPResponse(const unsigned char *pHTTPData, int iResponseBytesCount);  /* static in F:\projects\icytower\trunk\source\httpget.c */
int __cdecl getSocketError();
it_orig_time_t __cdecl httpGetLastModified(HTTPResponse *pResponse);

/* ---- F:\projects\icytower\trunk\source\loadpng.c ---- */
BITMAP * __cdecl load_memory_png(const void *buffer, int bufsize, RGB *pal);
BITMAP * __cdecl load_png(const char *filename, RGB *pal);
BITMAP * __cdecl load_png_pf(PACKFILE *fp, RGB *pal);
void __cdecl read_data(png_structp png_ptr, png_bytep data, png_uint_32 length);  /* static in F:\projects\icytower\trunk\source\loadpng.c */
void __cdecl read_data_memory(png_structp png_ptr, png_bytep data, png_uint_32 length);  /* static in F:\projects\icytower\trunk\source\loadpng.c */
BITMAP * __cdecl really_load_png(png_structp png_ptr, png_infop info_ptr, RGB *pal);  /* static in F:\projects\icytower\trunk\source\loadpng.c */

/* ---- F:\projects\icytower\trunk\source\main.c ---- */
int __stdcall WinMain(void *hInst, void *hPrev, char *Cmd, int nShow);
int __cdecl _mangled_main(int argc, char **argv);
int __cdecl add_profile(const char *filename, int attrib, void *param);
void __cdecl blit_to_screen(BITMAP *bmp);
void __cdecl change_profile();
void __cdecl checkMenuFocus();
int __cdecl check_beta_tester();
int __cdecl check_characters();
int __cdecl check_dir(const char *filename, int attrib, void *param);
void __cdecl clickedCloseButton();
void __cdecl color_map_callback(int p);
void __cdecl datafile_callback(DATAFILE *d);
void __cdecl datafile_callback_slow(DATAFILE *d);
int __cdecl do_replay_menu();
void __cdecl drawSlot(BITMAP *dst, int x, int y, char *title, char *text, int color);
void __cdecl draw_frame(BITMAP *bmp);
void __cdecl draw_progress_bar();
void __cdecl draw_results(BITMAP *bmp, BITMAP *logo, int y, int *qualified, int *qValues, int showQ);
void __cdecl draw_reward(BITMAP *bmp);
void __cdecl end_game();
void __cdecl fadeIn(BITMAP *bmp, int speed);
void __cdecl fadeOut(int speed);
void __cdecl for_each_directory(const char *basedir, int (__cdecl *cb)(const char *, int, void *));
void __cdecl force_create_profile();
SAMPLE * __cdecl getSampleFromOggDatafile(DATAFILE *df, int id);
Tcontrol * __cdecl get_controls();
Treplay * __cdecl get_demo();
int __cdecl get_gamepad_value(char *dir);
int __cdecl get_string(BITMAP *bmp, char *string, int w, int max_chars, FONT *f, int pos_x, int pos_y, int colour, int bg_color);
char * __cdecl get_version_str();
void __cdecl handle_player_collision_combo(int lastX, int lastY);
void __cdecl handle_player_collision_old(int lastX, int lastY);
void __cdecl handle_player_collision_original(int lastX, int lastY);
void __cdecl handle_player_collision_vector(int lastX, int lastY);
void __cdecl handle_player_collision_vector_2(int lastX, int lastY);
void __cdecl handle_player_input(Tcontrol *ctrl);
int __cdecl init_game(int argc, char **argv);
void __cdecl line_alert(char *txt);
int __cdecl line_intersect(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy, int *ix, int *iy);
BITMAP * __cdecl loadScrambled(char *fileName);
int __cdecl load_character(const char *filename, int attrib, void *param);
void __cdecl load_new_ad_image();
void __cdecl load_sound(SAMPLE **dest, char *fname, BITMAP *bmp, int y);
void __cdecl log2file(const char *format, ...);
void __cdecl main_menu_callback(void);
void __cdecl myDeleteFile(char *path, char *file);
int __cdecl my_alert(char *func, char *txt, int choice, int enter_hint);
int __cdecl new_game();
int __cdecl new_rand();
int __cdecl ok_to_play();
void __cdecl open_web_browser(const char *pURL);
int __cdecl play();
void __cdecl play_jump_sound(Tplayer *p);
void __cdecl play_menu_move();
void __cdecl play_menu_select();
void __cdecl play_sound(SAMPLE *s, int pitch, int please_pan);
void __cdecl pwd_garble_string(char *str, int key);
int __cdecl rebuild_profile_list(Tavailable_profile **profs);
void __cdecl replaceBadCharacters(char *string, char newChar);
void __cdecl replay_menu_callback(void);
void __cdecl run_demo(char *file_name);
void __cdecl save_config();
void __cdecl set_current_avatar();
void __cdecl show_credits();
void __cdecl show_instructions();
int __cdecl show_name(const char *filename, int attrib, void *param);
void __cdecl startGameMusic();
void __cdecl startMenuMusic();
int __cdecl start_reward(int lev);
void __cdecl stopGameMusic();
void __cdecl stopMenuMusic();
void __cdecl switchedFromProgram();
void __cdecl switchedToProgram();
void __cdecl syncOptionsFromProfile();
void __cdecl take_screenshot(BITMAP *bmp);
void __cdecl testWindowResolution();
void __cdecl uninit_game();
void __cdecl update_frame();

/* ---- F:\projects\icytower\trunk\source\map.c ---- */
void __cdecl add_floor(Tmap *m);
void __cdecl getFloorData(Tmap *m, int cy, int *fy, int *fx1, int *fx2);
int __cdecl get_level(Tmap *m, int cy);
int __cdecl is_solid(Tmap *m, int cx, int cy);
void __cdecl reset_map(Tmap *m);

/* ---- F:\projects\icytower\trunk\source\menu.c ---- */
void __cdecl build_menu_string(Tmenu *m, char *dest);
void __cdecl draw_menu(BITMAP *bmp, Tmenu *m, Tmenu_params *mp, int cx, int y, int dx);
int __cdecl get_selection_value(Tmenu_selection *s);
int __cdecl get_slider_value(Tmenu_slider *s);
int __cdecl handle_menu(Tmenu *menu, Tmenu_params *mp, Tcontrol *ctrl, BITMAP *bmp, void (__cdecl *callback)(void), int x, int y, int dx);
void __cdecl key_to_str(int k, char *dest);
void __cdecl reset_menu(Tmenu *m, Tmenu_params *mp, int sel_pos);
int __cdecl set_selection_value(Tmenu_selection *s, int v);
int __cdecl set_slider_value(Tmenu_slider *s, int v);
int __cdecl update_game_menu(BITMAP *bmp, Tmenu *m, Tmenu_params *mp, Tcontrol *ctrl, int x, int y, void **data);

/* ---- F:\projects\icytower\trunk\source\options.c ---- */
int __cdecl generate_options_checksum(Toptions *o);
void __cdecl load_options(Toptions *o, PACKFILE *fp);
void __cdecl reset_options(Toptions *o);
void __cdecl save_options(Toptions *o, PACKFILE *fp);

/* ---- F:\projects\icytower\trunk\source\particle.c ---- */
int __cdecl create_particle(Tparticle *p, int x, int y);
void __cdecl reset_particles(Tparticle *p);
void __cdecl update_particle(Tparticle *p);

/* ---- F:\projects\icytower\trunk\source\player.c ---- */
int __cdecl jump_player(Tplayer *p, int cheat);
void __cdecl reset_player(Tplayer *p);
void __cdecl update_player(Tplayer *p);

/* ---- F:\projects\icytower\trunk\source\profile.c ---- */
Tprofile * __cdecl create_profile(char *handle, int overwrite);
void __cdecl delete_profile(char *handle);
int __cdecl draw_buffer(BITMAP *bmp, char *buffer, int x, int y);
void __cdecl draw_profile_selector(BITMAP *bmp, Tprofile *current_profile, Tavailable_profile *profiles, int numProfiles, int selection, int offset, int max_posts, int x, int y);
int __cdecl generate_profile_checksum(Tprofile *p);
Tprofile * __cdecl load_profile(char *handle);
char * __cdecl profile_data_page_advanced(Tprofile *p);
char * __cdecl profile_data_page_basic(Tprofile *p);
char * __cdecl profile_data_page_extra(Tprofile *p);
char * __cdecl profile_data_page_general(Tprofile *p, char *filler);
int __cdecl save_profile(Tprofile *p);
Tprofile * __cdecl select_profile(Tprofile *current_profile, Tavailable_profile *profiles, int numProfiles, Tcontrol *ctrl);
void __cdecl set_next_rank_message(char *buf, Tprofile *p);
int __cdecl view_profile(Tprofile *profile);

/* ---- F:\projects\icytower\trunk\source\regpng.c ---- */
void __cdecl destroy_datafile_png(void *data);  /* static in F:\projects\icytower\trunk\source\regpng.c */
void * __cdecl load_datafile_png(PACKFILE *f, long size);  /* static in F:\projects\icytower\trunk\source\regpng.c */
int __cdecl loadpng_init(void);
void __cdecl register_png_datafile_object(int id);
void __cdecl register_png_file_type(void);

/* ---- F:\projects\icytower\trunk\source\replay.c ---- */
int __cdecl add_itr_file(const char *filename, int attrib, void *param);
int __cdecl calc_replay_checksum(Treplay *r);
int __cdecl calc_replay_checksum_131(Treplay *r);
Treplay * __cdecl create_replay(int size);
void __cdecl destroy_replay(Treplay *r);
void __cdecl draw_replay_selector(BITMAP *bmp, Treplay *rep, Treplay_post *file_list, int selection, int offset, int max_posts, int x, int y);
int __cdecl get_replay_property(const char *filename, int property);
int __cdecl get_sort_method();
Treplay * __cdecl load_replay(const char *filename);
int __cdecl my_strcmp(const void *c, const void *d);
Treplay * __cdecl replay_selector(Tcontrol *ctrl, char *path);
int __cdecl save_replay(const char *path, const char *file, Treplay *r, int size, int make_new_date);
void __cdecl set_sort_method(int sm);
void __cdecl update_file_list(char *path);

/* ---- F:\projects\icytower\trunk\source\savepng.c ---- */
void __cdecl flush_data(png_structp png_ptr);  /* static in F:\projects\icytower\trunk\source\savepng.c */
int __cdecl really_save_png(PACKFILE *fp, BITMAP *bmp, const RGB *pal);  /* static in F:\projects\icytower\trunk\source\savepng.c */
int __cdecl save_png(const char *filename, BITMAP *bmp, const RGB *pal);
void __cdecl write_data(png_structp png_ptr, png_bytep data, png_uint_32 length);  /* static in F:\projects\icytower\trunk\source\savepng.c */

/* ---- F:\projects\icytower\trunk\source\scroller.c ---- */
int __cdecl draw_scroller(Tscroller *sc, BITMAP *bmp, int x, int y, int color);
void __cdecl init_scroller(Tscroller *sc, FONT *f, char *t, int w, int h, int horiz);
void __cdecl restart_scroller(Tscroller *sc);
void __cdecl scroll_scroller(Tscroller *sc, int step);

/* ---- F:\projects\icytower\trunk\source\stars.c ---- */
void __cdecl draw_star_field(Tstar_field *sf, BITMAP *bmp, int x, int y);
void __cdecl init_star_field(Tstar_field *sf, int w, int h, int num, int first_col, int last_col, int dep, int cc);
void __cdecl scroll_star_field(Tstar_field *sf, double xstep, double ystep);

/* ---- F:\projects\icytower\trunk\source\strptime.c ---- */
char * __cdecl _strptime(const char *buf, const char *format, struct it_orig_tm *timeptr, int *gmt);  /* static in F:\projects\icytower\trunk\source\strptime.c */
int __cdecl first_day(int year);  /* static in F:\projects\icytower\trunk\source\strptime.c */
int __cdecl match_string(const char **buf, const char **strs);  /* static in F:\projects\icytower\trunk\source\strptime.c */
char * __cdecl strptime(const char *buf, const char *format, struct it_orig_tm *timeptr);

/* ---- F:\projects\icytower\trunk\source\timecompat.c ---- */
it_orig_time_t __cdecl timegm(struct it_orig_tm *ptm);

/* ---- F:\projects\icytower\trunk\source\timer.c ---- */
void __cdecl cycle_counter(void);
void __cdecl fps_counter(void);
int __cdecl install_timers();

#endif /* ICYTOWER_GAME_FUNCS_H */
