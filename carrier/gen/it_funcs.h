/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_interop.py from:
 *   artifacts/dwarf_info.txt
 *   artifacts/functions.json
 * scope=game. Re-run gen_interop.py to regenerate; do not hand-edit.
 * See win32_pilot.md SS3: the original image is mapped in-process at its
 * original base 0x400000 with no relocations, so a global at VA X is
 * *(T*)X and a function at VA F is ((ret(__cdecl*)(args))F).
 */

#ifndef IT_FUNCS_H
#define IT_FUNCS_H
#include "it_types.h"

/* HTTPFetchInternal  VA=0x405e9c  size=594  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _HTTPFetchInternal)] */
typedef HTTPResponse * (__cdecl *PFN_HTTPFetchInternal)(const char *, int, const char *, const char *);
#define IT_F_HTTPFetchInternal ((PFN_HTTPFetchInternal)0x405e9c)

/* HTTPGet  VA=0x406194  size=27  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _HTTPGet)] */
typedef HTTPResponse * (__cdecl *PFN_HTTPGet)(const char *);
#define IT_F_HTTPGet ((PFN_HTTPGet)0x406194)

/* HTTPHead  VA=0x406178  size=27  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _HTTPHead)] */
typedef HTTPResponse * (__cdecl *PFN_HTTPHead)(const char *);
#define IT_F_HTTPHead ((PFN_HTTPHead)0x406178)

/* HTTPRequest  VA=0x4060f0  size=136  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _HTTPRequest)] */
typedef HTTPResponse * (__cdecl *PFN_HTTPRequest)(const char *, const char *);
#define IT_F_HTTPRequest ((PFN_HTTPRequest)0x4060f0)

/* SplitURL  VA=0x405984  size=267  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _SplitURL)] */
typedef int (__cdecl *PFN_SplitURL)(const char *, char **, char **, int *);
#define IT_F_SplitURL ((PFN_SplitURL)0x405984)

/* WinMain  VA=0x406cb0  size=50  cu=F:\projects\icytower\trunk\source\main.c  conv=stdcall [KNOWN (COFF symbol _WinMain@16)] */
typedef int (__stdcall *PFN_WinMain)(void *, void *, char *, int);
#define IT_F_WinMain ((PFN_WinMain)0x406cb0)

/* _mangled_main  VA=0x415f10  size=1938  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol __mangled_main)] */
typedef int (__cdecl *PFN__mangled_main)(int, char **);
#define IT_F__mangled_main ((PFN__mangled_main)0x415f10)

/* _strptime  VA=0x41f640  size=2028  cu=F:\projects\icytower\trunk\source\strptime.c  conv=cdecl [KNOWN (COFF symbol __strptime)] */
typedef char * (__cdecl *PFN__strptime)(const char *, const char *, struct it_orig_tm *, int *);
#define IT_F__strptime ((PFN__strptime)0x41f640)

/* add_combo  VA=0x40414c  size=62  cu=F:\projects\icytower\trunk\source\game_data.c  conv=cdecl [KNOWN (COFF symbol _add_combo)] */
typedef void (__cdecl *PFN_add_combo)(Tgame_data *, Tgd_combo *);
#define IT_F_add_combo ((PFN_add_combo)0x40414c)

/* add_floor  VA=0x4167dc  size=608  cu=F:\projects\icytower\trunk\source\map.c  conv=cdecl [KNOWN (COFF symbol _add_floor)] */
typedef void (__cdecl *PFN_add_floor)(Tmap *);
#define IT_F_add_floor ((PFN_add_floor)0x4167dc)

/* add_itr_file  VA=0x41e740  size=360  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _add_itr_file)] */
typedef int (__cdecl *PFN_add_itr_file)(const char *, int, void *);
#define IT_F_add_itr_file ((PFN_add_itr_file)0x41e740)

/* add_jump_sequence  VA=0x4040f4  size=87  cu=F:\projects\icytower\trunk\source\game_data.c  conv=cdecl [KNOWN (COFF symbol _add_jump_sequence)] */
typedef void (__cdecl *PFN_add_jump_sequence)(Tgame_data *, Tgd_jump_sequence *);
#define IT_F_add_jump_sequence ((PFN_add_jump_sequence)0x4040f4)

/* add_profile  VA=0x40c8c0  size=195  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _add_profile)] */
typedef int (__cdecl *PFN_add_profile)(const char *, int, void *);
#define IT_F_add_profile ((PFN_add_profile)0x40c8c0)

/* blit_to_screen  VA=0x40b6bc  size=1415  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _blit_to_screen)] */
typedef void (__cdecl *PFN_blit_to_screen)(BITMAP *);
#define IT_F_blit_to_screen ((PFN_blit_to_screen)0x40b6bc)

/* build_menu_string  VA=0x4174dc  size=415  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _build_menu_string)] */
typedef void (__cdecl *PFN_build_menu_string)(Tmenu *, char *);
#define IT_F_build_menu_string ((PFN_build_menu_string)0x4174dc)

/* calc_replay_checksum  VA=0x41bac4  size=676  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _calc_replay_checksum)] */
typedef int (__cdecl *PFN_calc_replay_checksum)(Treplay *);
#define IT_F_calc_replay_checksum ((PFN_calc_replay_checksum)0x41bac4)

/* calc_replay_checksum_131  VA=0x41ba10  size=177  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _calc_replay_checksum_131)] */
typedef int (__cdecl *PFN_calc_replay_checksum_131)(Treplay *);
#define IT_F_calc_replay_checksum_131 ((PFN_calc_replay_checksum_131)0x41ba10)

/* change_profile  VA=0x40e1cc  size=188  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _change_profile)] */
typedef void (__cdecl *PFN_change_profile)();
#define IT_F_change_profile ((PFN_change_profile)0x40e1cc)

/* checkMenuFocus  VA=0x406f78  size=59  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _checkMenuFocus)] */
typedef void (__cdecl *PFN_checkMenuFocus)();
#define IT_F_checkMenuFocus ((PFN_checkMenuFocus)0x406f78)

/* check_beta_tester  VA=0x40e580  size=254  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _check_beta_tester)] */
typedef int (__cdecl *PFN_check_beta_tester)();
#define IT_F_check_beta_tester ((PFN_check_beta_tester)0x40e580)

/* check_characters  VA=0x40e680  size=345  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _check_characters)] */
typedef int (__cdecl *PFN_check_characters)();
#define IT_F_check_characters ((PFN_check_characters)0x40e680)

/* check_control_key  VA=0x401808  size=58  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _check_control_key)] */
typedef int (__cdecl *PFN_check_control_key)(Tcontrol *, int);
#define IT_F_check_control_key ((PFN_check_control_key)0x401808)

/* check_dir  VA=0x40ffc4  size=103  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _check_dir)] */
typedef int (__cdecl *PFN_check_dir)(const char *, int, void *);
#define IT_F_check_dir ((PFN_check_dir)0x40ffc4)

/* clear_trailing_whitespace  VA=0x402078  size=90  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _clear_trailing_whitespace)] */
typedef void (__cdecl *PFN_clear_trailing_whitespace)(char *);
#define IT_F_clear_trailing_whitespace ((PFN_clear_trailing_whitespace)0x402078)

/* clickedCloseButton  VA=0x406a7c  size=15  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _clickedCloseButton)] */
typedef void (__cdecl *PFN_clickedCloseButton)();
#define IT_F_clickedCloseButton ((PFN_clickedCloseButton)0x406a7c)

/* color_map_callback  VA=0x407c20  size=22  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _color_map_callback)] */
typedef void (__cdecl *PFN_color_map_callback)(int);
#define IT_F_color_map_callback ((PFN_color_map_callback)0x407c20)

/* create_game_data  VA=0x404198  size=186  cu=F:\projects\icytower\trunk\source\game_data.c  conv=cdecl [KNOWN (COFF symbol _create_game_data)] */
typedef Tgame_data * (__cdecl *PFN_create_game_data)();
#define IT_F_create_game_data ((PFN_create_game_data)0x404198)

/* create_particle  VA=0x418490  size=192  cu=F:\projects\icytower\trunk\source\particle.c  conv=cdecl [KNOWN (COFF symbol _create_particle)] */
typedef int (__cdecl *PFN_create_particle)(Tparticle *, int, int);
#define IT_F_create_particle ((PFN_create_particle)0x418490)

/* create_post  VA=0x4014ec  size=157  cu=F:\projects\icytower\trunk\source\beta.c  conv=cdecl [KNOWN (COFF symbol _create_post)] */
typedef Tbeta * (__cdecl *PFN_create_post)();
#define IT_F_create_post ((PFN_create_post)0x4014ec)

/* create_profile  VA=0x41a988  size=823  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _create_profile)] */
typedef Tprofile * (__cdecl *PFN_create_profile)(char *, int);
#define IT_F_create_profile ((PFN_create_profile)0x41a988)

/* create_replay  VA=0x41cce8  size=254  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _create_replay)] */
typedef Treplay * (__cdecl *PFN_create_replay)(int);
#define IT_F_create_replay ((PFN_create_replay)0x41cce8)

/* csv_add_field  VA=0x401a98  size=86  cu=F:\projects\icytower\trunk\source\csv.c  conv=cdecl [KNOWN (COFF symbol _csv_add_field)] */
typedef void (__cdecl *PFN_csv_add_field)(CSVParseContext *, char *);
#define IT_F_csv_add_field ((PFN_csv_add_field)0x401a98)

/* csv_begin  VA=0x401bf0  size=106  cu=F:\projects\icytower\trunk\source\csv.c  conv=cdecl [KNOWN (COFF symbol _csv_begin)] */
typedef CSVParseContext * (__cdecl *PFN_csv_begin)(const unsigned char *, it_orig_size_t);
#define IT_F_csv_begin ((PFN_csv_begin)0x401bf0)

/* csv_destroy  VA=0x401bb8  size=54  cu=F:\projects\icytower\trunk\source\csv.c  conv=cdecl [KNOWN (COFF symbol _csv_destroy)] */
typedef void (__cdecl *PFN_csv_destroy)(CSVParseContext *);
#define IT_F_csv_destroy ((PFN_csv_destroy)0x401bb8)

/* csv_next  VA=0x401af0  size=197  cu=F:\projects\icytower\trunk\source\csv.c  conv=cdecl [KNOWN (COFF symbol _csv_next)] */
typedef int (__cdecl *PFN_csv_next)(CSVParseContext *);
#define IT_F_csv_next ((PFN_csv_next)0x401af0)

/* csv_open  VA=0x401c5c  size=204  cu=F:\projects\icytower\trunk\source\csv.c  conv=cdecl [KNOWN (COFF symbol _csv_open)] */
typedef CSVParseContext * (__cdecl *PFN_csv_open)(const char *);
#define IT_F_csv_open ((PFN_csv_open)0x401c5c)

/* csv_rewind  VA=0x401a80  size=24  cu=F:\projects\icytower\trunk\source\csv.c  conv=cdecl [KNOWN (COFF symbol _csv_rewind)] */
typedef void (__cdecl *PFN_csv_rewind)(CSVParseContext *);
#define IT_F_csv_rewind ((PFN_csv_rewind)0x401a80)

/* custom_alert  VA=0x4027f0  size=132  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _custom_alert)] */
typedef void (__cdecl *PFN_custom_alert)(char *, char *);
#define IT_F_custom_alert ((PFN_custom_alert)0x4027f0)

/* cycle_counter  VA=0x41fed4  size=16  cu=F:\projects\icytower\trunk\source\timer.c  conv=cdecl [KNOWN (COFF symbol _cycle_counter)] */
typedef void (__cdecl *PFN_cycle_counter)(void);
#define IT_F_cycle_counter ((PFN_cycle_counter)0x41fed4)

/* datafile_callback  VA=0x407c14  size=12  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _datafile_callback)] */
typedef void (__cdecl *PFN_datafile_callback)(DATAFILE *);
#define IT_F_datafile_callback ((PFN_datafile_callback)0x407c14)

/* datafile_callback_slow  VA=0x407bf0  size=33  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _datafile_callback_slow)] */
typedef void (__cdecl *PFN_datafile_callback_slow)(DATAFILE *);
#define IT_F_datafile_callback_slow ((PFN_datafile_callback_slow)0x407bf0)

/* delete_profile  VA=0x41a8a8  size=222  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _delete_profile)] */
typedef void (__cdecl *PFN_delete_profile)(char *);
#define IT_F_delete_profile ((PFN_delete_profile)0x41a8a8)

/* destroyHTTPResponse  VA=0x405910  size=116  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _destroyHTTPResponse)] */
typedef void (__cdecl *PFN_destroyHTTPResponse)(HTTPResponse *);
#define IT_F_destroyHTTPResponse ((PFN_destroyHTTPResponse)0x405910)

/* destroy_all  VA=0x4014b0  size=59  cu=F:\projects\icytower\trunk\source\beta.c  conv=cdecl [KNOWN (COFF symbol _destroy_all)] */
typedef void (__cdecl *PFN_destroy_all)(Tbeta *);
#define IT_F_destroy_all ((PFN_destroy_all)0x4014b0)

/* destroy_custom_data  VA=0x401dd8  size=486  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _destroy_custom_data)] */
typedef int (__cdecl *PFN_destroy_custom_data)(Tcustom *);
#define IT_F_destroy_custom_data ((PFN_destroy_custom_data)0x401dd8)

/* destroy_datafile_png  VA=0x41b8e4  size=22  cu=F:\projects\icytower\trunk\source\regpng.c  conv=cdecl [KNOWN (COFF symbol _destroy_datafile_png)] */
typedef void (__cdecl *PFN_destroy_datafile_png)(void *);
#define IT_F_destroy_datafile_png ((PFN_destroy_datafile_png)0x41b8e4)

/* destroy_game_data  VA=0x40418c  size=12  cu=F:\projects\icytower\trunk\source\game_data.c  conv=cdecl [KNOWN (COFF symbol _destroy_game_data)] */
typedef void (__cdecl *PFN_destroy_game_data)(Tgame_data *);
#define IT_F_destroy_game_data ((PFN_destroy_game_data)0x40418c)

/* destroy_hisc_table  VA=0x405818  size=34  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _destroy_hisc_table)] */
typedef void (__cdecl *PFN_destroy_hisc_table)(Thisc_table *);
#define IT_F_destroy_hisc_table ((PFN_destroy_hisc_table)0x405818)

/* destroy_replay  VA=0x41bd68  size=54  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _destroy_replay)] */
typedef void (__cdecl *PFN_destroy_replay)(Treplay *);
#define IT_F_destroy_replay ((PFN_destroy_replay)0x41bd68)

/* do_replay_menu  VA=0x410f98  size=2661  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _do_replay_menu)] */
typedef int (__cdecl *PFN_do_replay_menu)();
#define IT_F_do_replay_menu ((PFN_do_replay_menu)0x410f98)

/* drawSlot  VA=0x406fb4  size=328  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _drawSlot)] */
typedef void (__cdecl *PFN_drawSlot)(BITMAP *, int, int, char *, char *, int);
#define IT_F_drawSlot ((PFN_drawSlot)0x406fb4)

/* draw_buffer  VA=0x4191c8  size=185  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _draw_buffer)] */
typedef int (__cdecl *PFN_draw_buffer)(BITMAP *, char *, int, int);
#define IT_F_draw_buffer ((PFN_draw_buffer)0x4191c8)

/* draw_frame  VA=0x40929c  size=8518  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _draw_frame)] */
typedef void (__cdecl *PFN_draw_frame)(BITMAP *);
#define IT_F_draw_frame ((PFN_draw_frame)0x40929c)

/* draw_menu  VA=0x41767c  size=1118  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _draw_menu)] */
typedef void (__cdecl *PFN_draw_menu)(BITMAP *, Tmenu *, Tmenu_params *, int, int, int);
#define IT_F_draw_menu ((PFN_draw_menu)0x41767c)

/* draw_profile_selector  VA=0x418cd4  size=1268  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _draw_profile_selector)] */
typedef void (__cdecl *PFN_draw_profile_selector)(BITMAP *, Tprofile *, Tavailable_profile *, int, int, int, int, int, int);
#define IT_F_draw_profile_selector ((PFN_draw_profile_selector)0x418cd4)

/* draw_progress_bar  VA=0x407a08  size=486  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _draw_progress_bar)] */
typedef void (__cdecl *PFN_draw_progress_bar)();
#define IT_F_draw_progress_bar ((PFN_draw_progress_bar)0x407a08)

/* draw_replay_selector  VA=0x41be58  size=3726  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _draw_replay_selector)] */
typedef void (__cdecl *PFN_draw_replay_selector)(BITMAP *, Treplay *, Treplay_post *, int, int, int, int, int);
#define IT_F_draw_replay_selector ((PFN_draw_replay_selector)0x41be58)

/* draw_results  VA=0x4076c0  size=839  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _draw_results)] */
typedef void (__cdecl *PFN_draw_results)(BITMAP *, BITMAP *, int, int *, int *, int);
#define IT_F_draw_results ((PFN_draw_results)0x4076c0)

/* draw_reward  VA=0x4070fc  size=581  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _draw_reward)] */
typedef void (__cdecl *PFN_draw_reward)(BITMAP *);
#define IT_F_draw_reward ((PFN_draw_reward)0x4070fc)

/* draw_scroller  VA=0x41f0ec  size=396  cu=F:\projects\icytower\trunk\source\scroller.c  conv=cdecl [KNOWN (COFF symbol _draw_scroller)] */
typedef int (__cdecl *PFN_draw_scroller)(Tscroller *, BITMAP *, int, int, int);
#define IT_F_draw_scroller ((PFN_draw_scroller)0x41f0ec)

/* draw_star_field  VA=0x41f340  size=199  cu=F:\projects\icytower\trunk\source\stars.c  conv=cdecl [KNOWN (COFF symbol _draw_star_field)] */
typedef void (__cdecl *PFN_draw_star_field)(Tstar_field *, BITMAP *, int, int);
#define IT_F_draw_star_field ((PFN_draw_star_field)0x41f340)

/* draw_table  VA=0x404a7c  size=441  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _draw_table)] */
typedef int (__cdecl *PFN_draw_table)(BITMAP *, int, int, char *, Thisc_table *);
#define IT_F_draw_table ((PFN_draw_table)0x404a7c)

/* dumpHTTPResponse  VA=0x405e2c  size=99  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _dumpHTTPResponse)] */
typedef void (__cdecl *PFN_dumpHTTPResponse)(it_orig_FILE *, HTTPResponse *);
#define IT_F_dumpHTTPResponse ((PFN_dumpHTTPResponse)0x405e2c)

/* end_game  VA=0x40e110  size=32  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _end_game)] */
typedef void (__cdecl *PFN_end_game)();
#define IT_F_end_game ((PFN_end_game)0x40e110)

/* enter_hisc_table  VA=0x405790  size=136  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _enter_hisc_table)] */
typedef void (__cdecl *PFN_enter_hisc_table)(Thisc_table *, int, char *);
#define IT_F_enter_hisc_table ((PFN_enter_hisc_table)0x405790)

/* extractHTTPResponse  VA=0x405a90  size=923  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _extractHTTPResponse)] */
typedef HTTPResponse * (__cdecl *PFN_extractHTTPResponse)(const unsigned char *, int);
#define IT_F_extractHTTPResponse ((PFN_extractHTTPResponse)0x405a90)

/* fadeIn  VA=0x40c1c0  size=424  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _fadeIn)] */
typedef void (__cdecl *PFN_fadeIn)(BITMAP *, int);
#define IT_F_fadeIn ((PFN_fadeIn)0x40c1c0)

/* fadeOut  VA=0x40bf5c  size=609  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _fadeOut)] */
typedef void (__cdecl *PFN_fadeOut)(int);
#define IT_F_fadeOut ((PFN_fadeOut)0x40bf5c)

/* first_day  VA=0x41f5c4  size=20  cu=F:\projects\icytower\trunk\source\strptime.c  conv=cdecl [KNOWN (COFF symbol _first_day)] */
typedef int (__cdecl *PFN_first_day)(int);
#define IT_F_first_day ((PFN_first_day)0x41f5c4)

/* fldads_destroy_cache  VA=0x403bb8  size=138  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_destroy_cache)] */
typedef void (__cdecl *PFN_fldads_destroy_cache)();
#define IT_F_fldads_destroy_cache ((PFN_fldads_destroy_cache)0x403bb8)

/* fldads_dump_local_cache  VA=0x403c84  size=156  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_dump_local_cache)] */
typedef void (__cdecl *PFN_fldads_dump_local_cache)();
#define IT_F_fldads_dump_local_cache ((PFN_fldads_dump_local_cache)0x403c84)

/* fldads_get_local_cache_name  VA=0x403c44  size=64  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_get_local_cache_name)] */
typedef const char * (__cdecl *PFN_fldads_get_local_cache_name)(const char *);
#define IT_F_fldads_get_local_cache_name ((PFN_fldads_get_local_cache_name)0x403c44)

/* fldads_get_local_filename_from_url  VA=0x403d4c  size=26  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_get_local_filename_from_url)] */
typedef const char * (__cdecl *PFN_fldads_get_local_filename_from_url)(const char *);
#define IT_F_fldads_get_local_filename_from_url ((PFN_fldads_get_local_filename_from_url)0x403d4c)

/* fldads_get_random_ad  VA=0x403af8  size=192  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_get_random_ad)] */
typedef const FLDAdSpot * (__cdecl *PFN_fldads_get_random_ad)();
#define IT_F_fldads_get_random_ad ((PFN_fldads_get_random_ad)0x403af8)

/* fldads_load_cache_from_csv  VA=0x403d68  size=282  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_load_cache_from_csv)] */
typedef void (__cdecl *PFN_fldads_load_cache_from_csv)(CSVParseContext *);
#define IT_F_fldads_load_cache_from_csv ((PFN_fldads_load_cache_from_csv)0x403d68)

/* fldads_load_local_cache  VA=0x403e84  size=55  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_load_local_cache)] */
typedef void (__cdecl *PFN_fldads_load_local_cache)();
#define IT_F_fldads_load_local_cache ((PFN_fldads_load_local_cache)0x403e84)

/* fldads_start  VA=0x403ac8  size=45  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_start)] */
typedef void (__cdecl *PFN_fldads_start)();
#define IT_F_fldads_start ((PFN_fldads_start)0x403ac8)

/* fldads_threadmain  VA=0x404014  size=223  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_threadmain)] */
typedef void * (__cdecl *PFN_fldads_threadmain)(void *);
#define IT_F_fldads_threadmain ((PFN_fldads_threadmain)0x404014)

/* fldads_update_cache  VA=0x403fa4  size=110  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_update_cache)] */
typedef void (__cdecl *PFN_fldads_update_cache)(unsigned char *, it_orig_size_t);
#define IT_F_fldads_update_cache ((PFN_fldads_update_cache)0x403fa4)

/* fldads_update_local_adimg  VA=0x403ebc  size=230  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _fldads_update_local_adimg)] */
typedef void (__cdecl *PFN_fldads_update_local_adimg)(const char *);
#define IT_F_fldads_update_local_adimg ((PFN_fldads_update_local_adimg)0x403ebc)

/* flush_data  VA=0x41e8a8  size=5  cu=F:\projects\icytower\trunk\source\savepng.c  conv=cdecl [KNOWN (COFF symbol _flush_data)] */
typedef void (__cdecl *PFN_flush_data)(png_structp);
#define IT_F_flush_data ((PFN_flush_data)0x41e8a8)

/* for_each_directory  VA=0x40cbc0  size=109  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _for_each_directory)] */
typedef void (__cdecl *PFN_for_each_directory)(const char *, int (__cdecl *)(const char *, int, void *));
#define IT_F_for_each_directory ((PFN_for_each_directory)0x40cbc0)

/* force_create_profile  VA=0x40d454  size=1538  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _force_create_profile)] */
typedef void (__cdecl *PFN_force_create_profile)();
#define IT_F_force_create_profile ((PFN_force_create_profile)0x40d454)

/* fps_counter  VA=0x41fea4  size=45  cu=F:\projects\icytower\trunk\source\timer.c  conv=cdecl [KNOWN (COFF symbol _fps_counter)] */
typedef void (__cdecl *PFN_fps_counter)(void);
#define IT_F_fps_counter ((PFN_fps_counter)0x41fea4)

/* garble_string  VA=0x401318  size=41  cu=F:\projects\icytower\trunk\source\beta.c  conv=cdecl [KNOWN (COFF symbol _garble_string)] */
typedef void (__cdecl *PFN_garble_string)(char *, int);
#define IT_F_garble_string ((PFN_garble_string)0x401318)

/* generate_checksum  VA=0x404a50  size=44  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _generate_checksum)] */
typedef int (__cdecl *PFN_generate_checksum)(Thisc *);
#define IT_F_generate_checksum ((PFN_generate_checksum)0x404a50)

/* generate_options_checksum  VA=0x4181cc  size=254  cu=F:\projects\icytower\trunk\source\options.c  conv=cdecl [KNOWN (COFF symbol _generate_options_checksum)] */
typedef int (__cdecl *PFN_generate_options_checksum)(Toptions *);
#define IT_F_generate_options_checksum ((PFN_generate_options_checksum)0x4181cc)

/* generate_profile_checksum  VA=0x418a14  size=112  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _generate_profile_checksum)] */
typedef int (__cdecl *PFN_generate_profile_checksum)(Tprofile *);
#define IT_F_generate_profile_checksum ((PFN_generate_profile_checksum)0x418a14)

/* getFloorData  VA=0x416770  size=107  cu=F:\projects\icytower\trunk\source\map.c  conv=cdecl [KNOWN (COFF symbol _getFloorData)] */
typedef void (__cdecl *PFN_getFloorData)(Tmap *, int, int *, int *, int *);
#define IT_F_getFloorData ((PFN_getFloorData)0x416770)

/* getGameDataXML  VA=0x404254  size=1855  cu=F:\projects\icytower\trunk\source\game_data.c  conv=cdecl [KNOWN (COFF symbol _getGameDataXML)] */
typedef char * (__cdecl *PFN_getGameDataXML)(Tgame_data *);
#define IT_F_getGameDataXML ((PFN_getGameDataXML)0x404254)

/* getSampleFromOggDatafile  VA=0x40ca2c  size=32  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _getSampleFromOggDatafile)] */
typedef SAMPLE * (__cdecl *PFN_getSampleFromOggDatafile)(DATAFILE *, int);
#define IT_F_getSampleFromOggDatafile ((PFN_getSampleFromOggDatafile)0x40ca2c)

/* getSocketError  VA=0x405e90  size=12  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _getSocketError)] */
typedef int (__cdecl *PFN_getSocketError)();
#define IT_F_getSocketError ((PFN_getSocketError)0x405e90)

/* get_adcache_dir  VA=0x4039a4  size=39  cu=F:\projects\icytower\trunk\source\directories.c  conv=cdecl [KNOWN (COFF symbol _get_adcache_dir)] */
typedef int (__cdecl *PFN_get_adcache_dir)(char *, it_orig_size_t);
#define IT_F_get_adcache_dir ((PFN_get_adcache_dir)0x4039a4)

/* get_character_dir  VA=0x403a84  size=67  cu=F:\projects\icytower\trunk\source\directories.c  conv=cdecl [KNOWN (COFF symbol _get_character_dir)] */
typedef int (__cdecl *PFN_get_character_dir)(char *, it_orig_size_t, const char *);
#define IT_F_get_character_dir ((PFN_get_character_dir)0x403a84)

/* get_configfile_path  VA=0x4039cc  size=39  cu=F:\projects\icytower\trunk\source\directories.c  conv=cdecl [KNOWN (COFF symbol _get_configfile_path)] */
typedef int (__cdecl *PFN_get_configfile_path)(char *, it_orig_size_t);
#define IT_F_get_configfile_path ((PFN_get_configfile_path)0x4039cc)

/* get_controls  VA=0x406978  size=10  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _get_controls)] */
typedef Tcontrol * (__cdecl *PFN_get_controls)();
#define IT_F_get_controls ((PFN_get_controls)0x406978)

/* get_custom_characters_dir  VA=0x403994  size=13  cu=F:\projects\icytower\trunk\source\directories.c  conv=cdecl [KNOWN (COFF symbol _get_custom_characters_dir)] */
typedef int (__cdecl *PFN_get_custom_characters_dir)(char *, it_orig_size_t);
#define IT_F_get_custom_characters_dir ((PFN_get_custom_characters_dir)0x403994)

/* get_demo  VA=0x40696c  size=10  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _get_demo)] */
typedef Treplay * (__cdecl *PFN_get_demo)();
#define IT_F_get_demo ((PFN_get_demo)0x40696c)

/* get_gamepad  VA=0x4017fc  size=10  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _get_gamepad)] */
typedef Tgamepad * (__cdecl *PFN_get_gamepad)();
#define IT_F_get_gamepad ((PFN_get_gamepad)0x4017fc)

/* get_gamepad_value  VA=0x40c984  size=166  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _get_gamepad_value)] */
typedef int (__cdecl *PFN_get_gamepad_value)(char *);
#define IT_F_get_gamepad_value ((PFN_get_gamepad_value)0x40c984)

/* get_level  VA=0x416748  size=40  cu=F:\projects\icytower\trunk\source\map.c  conv=cdecl [KNOWN (COFF symbol _get_level)] */
typedef int (__cdecl *PFN_get_level)(Tmap *, int);
#define IT_F_get_level ((PFN_get_level)0x416748)

/* get_logfile_path  VA=0x4039f4  size=39  cu=F:\projects\icytower\trunk\source\directories.c  conv=cdecl [KNOWN (COFF symbol _get_logfile_path)] */
typedef int (__cdecl *PFN_get_logfile_path)(char *, it_orig_size_t);
#define IT_F_get_logfile_path ((PFN_get_logfile_path)0x4039f4)

/* get_profile_dir_for_profile  VA=0x403a44  size=63  cu=F:\projects\icytower\trunk\source\directories.c  conv=cdecl [KNOWN (COFF symbol _get_profile_dir_for_profile)] */
typedef int (__cdecl *PFN_get_profile_dir_for_profile)(char *, it_orig_size_t, const char *);
#define IT_F_get_profile_dir_for_profile ((PFN_get_profile_dir_for_profile)0x403a44)

/* get_profiles_dir  VA=0x403a1c  size=39  cu=F:\projects\icytower\trunk\source\directories.c  conv=cdecl [KNOWN (COFF symbol _get_profiles_dir)] */
typedef int (__cdecl *PFN_get_profiles_dir)(char *, it_orig_size_t);
#define IT_F_get_profiles_dir ((PFN_get_profiles_dir)0x403a1c)

/* get_rank  VA=0x418ad0  size=82  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _get_rank)] */
typedef char * (__cdecl *PFN_get_rank)(Tprofile *);
#define IT_F_get_rank ((PFN_get_rank)0x418ad0)

/* get_rank_id  VA=0x418a84  size=75  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _get_rank_id)] */
typedef int (__cdecl *PFN_get_rank_id)(Tprofile *);
#define IT_F_get_rank_id ((PFN_get_rank_id)0x418a84)

/* get_replay_property  VA=0x41e244  size=1147  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _get_replay_property)] */
typedef int (__cdecl *PFN_get_replay_property)(const char *, int);
#define IT_F_get_replay_property ((PFN_get_replay_property)0x41e244)

/* get_selection_value  VA=0x416a6c  size=10  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _get_selection_value)] */
typedef int (__cdecl *PFN_get_selection_value)(Tmenu_selection *);
#define IT_F_get_selection_value ((PFN_get_selection_value)0x416a6c)

/* get_slider_value  VA=0x416a3c  size=10  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _get_slider_value)] */
typedef int (__cdecl *PFN_get_slider_value)(Tmenu_slider *);
#define IT_F_get_slider_value ((PFN_get_slider_value)0x416a3c)

/* get_sort_method  VA=0x41b9ac  size=10  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _get_sort_method)] */
typedef int (__cdecl *PFN_get_sort_method)();
#define IT_F_get_sort_method ((PFN_get_sort_method)0x41b9ac)

/* get_string  VA=0x40bc44  size=790  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _get_string)] */
typedef int (__cdecl *PFN_get_string)(BITMAP *, char *, int, int, FONT *, int, int, int, int);
#define IT_F_get_string ((PFN_get_string)0x40bc44)

/* get_string_data  VA=0x4020d4  size=86  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _get_string_data)] */
typedef char * (__cdecl *PFN_get_string_data)(char *, char *);
#define IT_F_get_string_data ((PFN_get_string_data)0x4020d4)

/* get_url_filename  VA=0x403d20  size=42  cu=F:\projects\icytower\trunk\source\fld_adspot.c  conv=cdecl [KNOWN (COFF symbol _get_url_filename)] */
typedef const char * (__cdecl *PFN_get_url_filename)(const char *);
#define IT_F_get_url_filename ((PFN_get_url_filename)0x403d20)

/* get_version_str  VA=0x406960  size=10  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _get_version_str)] */
typedef char * (__cdecl *PFN_get_version_str)();
#define IT_F_get_version_str ((PFN_get_version_str)0x406960)

/* handle_menu  VA=0x417d24  size=1120  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _handle_menu)] */
typedef int (__cdecl *PFN_handle_menu)(Tmenu *, Tmenu_params *, Tcontrol *, BITMAP *, void (__cdecl *)(void), int, int, int);
#define IT_F_handle_menu ((PFN_handle_menu)0x417d24)

/* handle_player_collision_combo  VA=0x408358  size=1390  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _handle_player_collision_combo)] */
typedef void (__cdecl *PFN_handle_player_collision_combo)(int, int);
#define IT_F_handle_player_collision_combo ((PFN_handle_player_collision_combo)0x408358)

/* handle_player_collision_old  VA=0x407fd8  size=894  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _handle_player_collision_old)] */
typedef void (__cdecl *PFN_handle_player_collision_old)(int, int);
#define IT_F_handle_player_collision_old ((PFN_handle_player_collision_old)0x407fd8)

/* handle_player_collision_original  VA=0x407e10  size=456  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _handle_player_collision_original)] */
typedef void (__cdecl *PFN_handle_player_collision_original)(int, int);
#define IT_F_handle_player_collision_original ((PFN_handle_player_collision_original)0x407e10)

/* handle_player_collision_vector  VA=0x408d08  size=1071  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _handle_player_collision_vector)] */
typedef void (__cdecl *PFN_handle_player_collision_vector)(int, int);
#define IT_F_handle_player_collision_vector ((PFN_handle_player_collision_vector)0x408d08)

/* handle_player_collision_vector_2  VA=0x4088c8  size=1086  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _handle_player_collision_vector_2)] */
typedef void (__cdecl *PFN_handle_player_collision_vector_2)(int, int);
#define IT_F_handle_player_collision_vector_2 ((PFN_handle_player_collision_vector_2)0x4088c8)

/* handle_player_input  VA=0x40b3e4  size=728  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _handle_player_input)] */
typedef void (__cdecl *PFN_handle_player_input)(Tcontrol *);
#define IT_F_handle_player_input ((PFN_handle_player_input)0x40b3e4)

/* hash  VA=0x41b9c8  size=71  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _hash)] */
typedef unsigned int (__cdecl *PFN_hash)(unsigned int);
#define IT_F_hash ((PFN_hash)0x41b9c8)

/* hash2  VA=0x4189cc  size=71  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _hash2)] */
typedef unsigned int (__cdecl *PFN_hash2)(unsigned int);
#define IT_F_hash2 ((PFN_hash2)0x4189cc)

/* hash3  VA=0x418184  size=71  cu=F:\projects\icytower\trunk\source\options.c  conv=cdecl [KNOWN (COFF symbol _hash3)] */
typedef unsigned int (__cdecl *PFN_hash3)(unsigned int);
#define IT_F_hash3 ((PFN_hash3)0x418184)

/* httpGetLastModified  VA=0x405890  size=126  cu=F:\projects\icytower\trunk\source\httpget.c  conv=cdecl [KNOWN (COFF symbol _httpGetLastModified)] */
typedef it_orig_time_t (__cdecl *PFN_httpGetLastModified)(HTTPResponse *);
#define IT_F_httpGetLastModified ((PFN_httpGetLastModified)0x405890)

/* init_control  VA=0x401790  size=67  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _init_control)] */
typedef void (__cdecl *PFN_init_control)(Tcontrol *);
#define IT_F_init_control ((PFN_init_control)0x401790)

/* init_custom  VA=0x401d28  size=175  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _init_custom)] */
typedef int (__cdecl *PFN_init_custom)(Tcustom *, const char *, int);
#define IT_F_init_custom ((PFN_init_custom)0x401d28)

/* init_game  VA=0x40e7dc  size=5788  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _init_game)] */
typedef int (__cdecl *PFN_init_game)(int, char **);
#define IT_F_init_game ((PFN_init_game)0x40e7dc)

/* init_scroller  VA=0x41f278  size=200  cu=F:\projects\icytower\trunk\source\scroller.c  conv=cdecl [KNOWN (COFF symbol _init_scroller)] */
typedef void (__cdecl *PFN_init_scroller)(Tscroller *, FONT *, char *, int, int, int);
#define IT_F_init_scroller ((PFN_init_scroller)0x41f278)

/* init_star_field  VA=0x41f52c  size=151  cu=F:\projects\icytower\trunk\source\stars.c  conv=cdecl [KNOWN (COFF symbol _init_star_field)] */
typedef void (__cdecl *PFN_init_star_field)(Tstar_field *, int, int, int, int, int, int, int);
#define IT_F_init_star_field ((PFN_init_star_field)0x41f52c)

/* install_timers  VA=0x41fee4  size=88  cu=F:\projects\icytower\trunk\source\timer.c  conv=cdecl [KNOWN (COFF symbol _install_timers)] */
typedef int (__cdecl *PFN_install_timers)();
#define IT_F_install_timers ((PFN_install_timers)0x41fee4)

/* is_any  VA=0x4018e8  size=22  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_any)] */
typedef int (__cdecl *PFN_is_any)(Tcontrol *);
#define IT_F_is_any ((PFN_is_any)0x4018e8)

/* is_custom_replay  VA=0x406b3c  size=66  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _is_custom_replay)] */
typedef int (__cdecl *PFN_is_custom_replay)(Treplay *);
#define IT_F_is_custom_replay ((PFN_is_custom_replay)0x406b3c)

/* is_down  VA=0x40185c  size=22  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_down)] */
typedef int (__cdecl *PFN_is_down)(Tcontrol *);
#define IT_F_is_down ((PFN_is_down)0x40185c)

/* is_enter  VA=0x4018d0  size=22  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_enter)] */
typedef int (__cdecl *PFN_is_enter)(Tcontrol *);
#define IT_F_is_enter ((PFN_is_enter)0x4018d0)

/* is_fire  VA=0x4018a0  size=22  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_fire)] */
typedef int (__cdecl *PFN_is_fire)(Tcontrol *);
#define IT_F_is_fire ((PFN_is_fire)0x4018a0)

/* is_left  VA=0x401874  size=17  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_left)] */
typedef int (__cdecl *PFN_is_left)(Tcontrol *);
#define IT_F_is_left ((PFN_is_left)0x401874)

/* is_pause  VA=0x4018b8  size=22  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_pause)] */
typedef int (__cdecl *PFN_is_pause)(Tcontrol *);
#define IT_F_is_pause ((PFN_is_pause)0x4018b8)

/* is_right  VA=0x401888  size=22  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_right)] */
typedef int (__cdecl *PFN_is_right)(Tcontrol *);
#define IT_F_is_right ((PFN_is_right)0x401888)

/* is_solid  VA=0x4166dc  size=107  cu=F:\projects\icytower\trunk\source\map.c  conv=cdecl [KNOWN (COFF symbol _is_solid)] */
typedef int (__cdecl *PFN_is_solid)(Tmap *, int, int);
#define IT_F_is_solid ((PFN_is_solid)0x4166dc)

/* is_up  VA=0x401844  size=22  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _is_up)] */
typedef int (__cdecl *PFN_is_up)(Tcontrol *);
#define IT_F_is_up ((PFN_is_up)0x401844)

/* jump_player  VA=0x418678  size=198  cu=F:\projects\icytower\trunk\source\player.c  conv=cdecl [KNOWN (COFF symbol _jump_player)] */
typedef int (__cdecl *PFN_jump_player)(Tplayer *, int);
#define IT_F_jump_player ((PFN_jump_player)0x418678)

/* key_to_str  VA=0x416a9c  size=2543  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _key_to_str)] */
typedef void (__cdecl *PFN_key_to_str)(int, char *);
#define IT_F_key_to_str ((PFN_key_to_str)0x416a9c)

/* line_alert  VA=0x409138  size=353  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _line_alert)] */
typedef void (__cdecl *PFN_line_alert)(char *);
#define IT_F_line_alert ((PFN_line_alert)0x409138)

/* line_intersect  VA=0x406b80  size=302  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _line_intersect)] */
typedef int (__cdecl *PFN_line_intersect)(int, int, int, int, int, int, int, int, int *, int *);
#define IT_F_line_intersect ((PFN_line_intersect)0x406b80)

/* loadCustomSoundDF  VA=0x402040  size=54  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _loadCustomSoundDF)] */
typedef SAMPLE * (__cdecl *PFN_loadCustomSoundDF)(DATAFILE *, int);
#define IT_F_loadCustomSoundDF ((PFN_loadCustomSoundDF)0x402040)

/* loadCustomSoundFILE  VA=0x401fc0  size=128  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _loadCustomSoundFILE)] */
typedef SAMPLE * (__cdecl *PFN_loadCustomSoundFILE)(char *, char *);
#define IT_F_loadCustomSoundFILE ((PFN_loadCustomSoundFILE)0x401fc0)

/* loadScrambled  VA=0x40cc30  size=247  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _loadScrambled)] */
typedef BITMAP * (__cdecl *PFN_loadScrambled)(char *);
#define IT_F_loadScrambled ((PFN_loadScrambled)0x40cc30)

/* load_character  VA=0x40fe78  size=330  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _load_character)] */
typedef int (__cdecl *PFN_load_character)(const char *, int, void *);
#define IT_F_load_character ((PFN_load_character)0x40fe78)

/* load_character_bmp  VA=0x4031cc  size=1992  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _load_character_bmp)] */
typedef BITMAP * (__cdecl *PFN_load_character_bmp)(const char *, int *, RGB *);
#define IT_F_load_character_bmp ((PFN_load_character_bmp)0x4031cc)

/* load_control  VA=0x401900  size=42  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _load_control)] */
typedef void (__cdecl *PFN_load_control)(Tcontrol *, it_orig_FILE *);
#define IT_F_load_control ((PFN_load_control)0x401900)

/* load_datafile_png  VA=0x41b8fc  size=109  cu=F:\projects\icytower\trunk\source\regpng.c  conv=cdecl [KNOWN (COFF symbol _load_datafile_png)] */
typedef void * (__cdecl *PFN_load_datafile_png)(PACKFILE *, long);
#define IT_F_load_datafile_png ((PFN_load_datafile_png)0x41b8fc)

/* load_frames  VA=0x402874  size=2392  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _load_frames)] */
typedef int (__cdecl *PFN_load_frames)(Tcustom *);
#define IT_F_load_frames ((PFN_load_frames)0x402874)

/* load_garbled_data  VA=0x40158c  size=313  cu=F:\projects\icytower\trunk\source\beta.c  conv=cdecl [KNOWN (COFF symbol _load_garbled_data)] */
typedef Tbeta * (__cdecl *PFN_load_garbled_data)(char *);
#define IT_F_load_garbled_data ((PFN_load_garbled_data)0x40158c)

/* load_hisc_table  VA=0x4056b4  size=155  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _load_hisc_table)] */
typedef int (__cdecl *PFN_load_hisc_table)(Thisc_table *, PACKFILE *);
#define IT_F_load_hisc_table ((PFN_load_hisc_table)0x4056b4)

/* load_memory_png  VA=0x406604  size=308  cu=F:\projects\icytower\trunk\source\loadpng.c  conv=cdecl [KNOWN (COFF symbol _load_memory_png)] */
typedef BITMAP * (__cdecl *PFN_load_memory_png)(const void *, int, RGB *);
#define IT_F_load_memory_png ((PFN_load_memory_png)0x406604)

/* load_new_ad_image  VA=0x415eac  size=100  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _load_new_ad_image)] */
typedef void (__cdecl *PFN_load_new_ad_image)();
#define IT_F_load_new_ad_image ((PFN_load_new_ad_image)0x415eac)

/* load_options  VA=0x41839c  size=70  cu=F:\projects\icytower\trunk\source\options.c  conv=cdecl [KNOWN (COFF symbol _load_options)] */
typedef void (__cdecl *PFN_load_options)(Toptions *, PACKFILE *);
#define IT_F_load_options ((PFN_load_options)0x41839c)

/* load_plain_data  VA=0x4016c8  size=197  cu=F:\projects\icytower\trunk\source\beta.c  conv=cdecl [KNOWN (COFF symbol _load_plain_data)] */
typedef Tbeta * (__cdecl *PFN_load_plain_data)(char *);
#define IT_F_load_plain_data ((PFN_load_plain_data)0x4016c8)

/* load_png  VA=0x406910  size=79  cu=F:\projects\icytower\trunk\source\loadpng.c  conv=cdecl [KNOWN (COFF symbol _load_png)] */
typedef BITMAP * (__cdecl *PFN_load_png)(const char *, RGB *);
#define IT_F_load_png ((PFN_load_png)0x406910)

/* load_png_pf  VA=0x406788  size=316  cu=F:\projects\icytower\trunk\source\loadpng.c  conv=cdecl [KNOWN (COFF symbol _load_png_pf)] */
typedef BITMAP * (__cdecl *PFN_load_png_pf)(PACKFILE *, RGB *);
#define IT_F_load_png_pf ((PFN_load_png_pf)0x406788)

/* load_profile  VA=0x41a7ec  size=188  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _load_profile)] */
typedef Tprofile * (__cdecl *PFN_load_profile)(char *);
#define IT_F_load_profile ((PFN_load_profile)0x41a7ec)

/* load_replay  VA=0x41cde8  size=1136  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _load_replay)] */
typedef Treplay * (__cdecl *PFN_load_replay)(const char *);
#define IT_F_load_replay ((PFN_load_replay)0x41cde8)

/* load_sound  VA=0x40ca4c  size=166  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _load_sound)] */
typedef void (__cdecl *PFN_load_sound)(SAMPLE **, char *, BITMAP *, int);
#define IT_F_load_sound ((PFN_load_sound)0x40ca4c)

/* load_sounds  VA=0x40212c  size=1729  cu=F:\projects\icytower\trunk\source\custom.c  conv=cdecl [KNOWN (COFF symbol _load_sounds)] */
typedef int (__cdecl *PFN_load_sounds)(Tcustom *);
#define IT_F_load_sounds ((PFN_load_sounds)0x40212c)

/* loadpng_init  VA=0x41b990  size=27  cu=F:\projects\icytower\trunk\source\regpng.c  conv=cdecl [KNOWN (COFF symbol _loadpng_init)] */
typedef int (__cdecl *PFN_loadpng_init)(void);
#define IT_F_loadpng_init ((PFN_loadpng_init)0x41b990)

/* log2file  VA=0x40da58  size=189  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _log2file)] */
typedef void (__cdecl *PFN_log2file)(const char *, ...);
#define IT_F_log2file ((PFN_log2file)0x40da58)

/* main_menu_callback  VA=0x4100f8  size=3741  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _main_menu_callback)] */
typedef void (__cdecl *PFN_main_menu_callback)(void);
#define IT_F_main_menu_callback ((PFN_main_menu_callback)0x4100f8)

/* make_hisc_table  VA=0x40583c  size=84  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _make_hisc_table)] */
typedef Thisc_table * (__cdecl *PFN_make_hisc_table)(char *);
#define IT_F_make_hisc_table ((PFN_make_hisc_table)0x40583c)

/* match_string  VA=0x41f5d8  size=103  cu=F:\projects\icytower\trunk\source\strptime.c  conv=cdecl [KNOWN (COFF symbol _match_string)] */
typedef int (__cdecl *PFN_match_string)(const char **, const char **);
#define IT_F_match_string ((PFN_match_string)0x41f5d8)

/* myDeleteFile  VA=0x40cd28  size=63  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _myDeleteFile)] */
typedef void (__cdecl *PFN_myDeleteFile)(char *, char *);
#define IT_F_myDeleteFile ((PFN_myDeleteFile)0x40cd28)

/* my_alert  VA=0x40cd68  size=1770  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _my_alert)] */
typedef int (__cdecl *PFN_my_alert)(char *, char *, int, int);
#define IT_F_my_alert ((PFN_my_alert)0x40cd68)

/* my_strcmp  VA=0x41e6c0  size=128  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _my_strcmp)] */
typedef int (__cdecl *PFN_my_strcmp)(const void *, const void *);
#define IT_F_my_strcmp ((PFN_my_strcmp)0x41e6c0)

/* new_game  VA=0x40dc9c  size=1139  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _new_game)] */
typedef int (__cdecl *PFN_new_game)();
#define IT_F_new_game ((PFN_new_game)0x40dc9c)

/* new_rand  VA=0x406984  size=128  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _new_rand)] */
typedef int (__cdecl *PFN_new_rand)();
#define IT_F_new_rand ((PFN_new_rand)0x406984)

/* new_srand  VA=0x406a04  size=14  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _new_srand)] */
typedef void (__cdecl *PFN_new_srand)(int);
#define IT_F_new_srand ((PFN_new_srand)0x406a04)

/* ok_to_play  VA=0x406a50  size=10  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _ok_to_play)] */
typedef int (__cdecl *PFN_ok_to_play)();
#define IT_F_ok_to_play ((PFN_ok_to_play)0x406a50)

/* open_web_browser  VA=0x40e510  size=111  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _open_web_browser)] */
typedef void (__cdecl *PFN_open_web_browser)(const char *);
#define IT_F_open_web_browser ((PFN_open_web_browser)0x40e510)

/* play  VA=0x411a00  size=17420  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _play)] */
typedef int (__cdecl *PFN_play)();
#define IT_F_play ((PFN_play)0x411a00)

/* play_jump_sound  VA=0x406ecc  size=141  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _play_jump_sound)] */
typedef void (__cdecl *PFN_play_jump_sound)(Tplayer *);
#define IT_F_play_jump_sound ((PFN_play_jump_sound)0x406ecc)

/* play_menu_move  VA=0x406ea4  size=37  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _play_menu_move)] */
typedef void (__cdecl *PFN_play_menu_move)();
#define IT_F_play_menu_move ((PFN_play_menu_move)0x406ea4)

/* play_menu_select  VA=0x406e7c  size=37  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _play_menu_select)] */
typedef void (__cdecl *PFN_play_menu_select)();
#define IT_F_play_menu_select ((PFN_play_menu_select)0x406e7c)

/* play_sound  VA=0x406da4  size=215  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _play_sound)] */
typedef void (__cdecl *PFN_play_sound)(SAMPLE *, int, int);
#define IT_F_play_sound ((PFN_play_sound)0x406da4)

/* poll_control  VA=0x401958  size=294  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _poll_control)] */
typedef void (__cdecl *PFN_poll_control)(Tcontrol *, int);
#define IT_F_poll_control ((PFN_poll_control)0x401958)

/* profile_data_page_advanced  VA=0x419284  size=332  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _profile_data_page_advanced)] */
typedef char * (__cdecl *PFN_profile_data_page_advanced)(Tprofile *);
#define IT_F_profile_data_page_advanced ((PFN_profile_data_page_advanced)0x419284)

/* profile_data_page_basic  VA=0x4193d0  size=637  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _profile_data_page_basic)] */
typedef char * (__cdecl *PFN_profile_data_page_basic)(Tprofile *);
#define IT_F_profile_data_page_basic ((PFN_profile_data_page_basic)0x4193d0)

/* profile_data_page_extra  VA=0x419650  size=85  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _profile_data_page_extra)] */
typedef char * (__cdecl *PFN_profile_data_page_extra)(Tprofile *);
#define IT_F_profile_data_page_extra ((PFN_profile_data_page_extra)0x419650)

/* profile_data_page_general  VA=0x4196a8  size=1091  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _profile_data_page_general)] */
typedef char * (__cdecl *PFN_profile_data_page_general)(Tprofile *, char *);
#define IT_F_profile_data_page_general ((PFN_profile_data_page_general)0x4196a8)

/* pwd_garble_string  VA=0x4073c4  size=52  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _pwd_garble_string)] */
typedef void (__cdecl *PFN_pwd_garble_string)(char *, int);
#define IT_F_pwd_garble_string ((PFN_pwd_garble_string)0x4073c4)

/* qualify_hisc_table  VA=0x404994  size=39  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _qualify_hisc_table)] */
typedef int (__cdecl *PFN_qualify_hisc_table)(Thisc_table *, int);
#define IT_F_qualify_hisc_table ((PFN_qualify_hisc_table)0x404994)

/* read_data  VA=0x4068c4  size=76  cu=F:\projects\icytower\trunk\source\loadpng.c  conv=cdecl [KNOWN (COFF symbol _read_data)] */
typedef void (__cdecl *PFN_read_data)(png_structp, png_bytep, png_uint_32);
#define IT_F_read_data ((PFN_read_data)0x4068c4)

/* read_data_memory  VA=0x406738  size=80  cu=F:\projects\icytower\trunk\source\loadpng.c  conv=cdecl [KNOWN (COFF symbol _read_data_memory)] */
typedef void (__cdecl *PFN_read_data_memory)(png_structp, png_bytep, png_uint_32);
#define IT_F_read_data_memory ((PFN_read_data_memory)0x406738)

/* read_line  VA=0x401460  size=80  cu=F:\projects\icytower\trunk\source\beta.c  conv=cdecl [KNOWN (COFF symbol _read_line)] */
typedef void (__cdecl *PFN_read_line)(char *, it_orig_FILE *);
#define IT_F_read_line ((PFN_read_line)0x401460)

/* really_load_png  VA=0x4061b0  size=1106  cu=F:\projects\icytower\trunk\source\loadpng.c  conv=cdecl [KNOWN (COFF symbol _really_load_png)] */
typedef BITMAP * (__cdecl *PFN_really_load_png)(png_structp, png_infop, RGB *);
#define IT_F_really_load_png ((PFN_really_load_png)0x4061b0)

/* really_save_png  VA=0x41e8b0  size=1870  cu=F:\projects\icytower\trunk\source\savepng.c  conv=cdecl [KNOWN (COFF symbol _really_save_png)] */
typedef int (__cdecl *PFN_really_save_png)(PACKFILE *, BITMAP *, const RGB *);
#define IT_F_really_save_png ((PFN_really_save_png)0x41e8b0)

/* rebuild_profile_list  VA=0x40c758  size=198  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _rebuild_profile_list)] */
typedef int (__cdecl *PFN_rebuild_profile_list)(Tavailable_profile **);
#define IT_F_rebuild_profile_list ((PFN_rebuild_profile_list)0x40c758)

/* register_png_datafile_object  VA=0x41b8c0  size=35  cu=F:\projects\icytower\trunk\source\regpng.c  conv=cdecl [KNOWN (COFF symbol _register_png_datafile_object)] */
typedef void (__cdecl *PFN_register_png_datafile_object)(int);
#define IT_F_register_png_datafile_object ((PFN_register_png_datafile_object)0x41b8c0)

/* register_png_file_type  VA=0x41b96c  size=36  cu=F:\projects\icytower\trunk\source\regpng.c  conv=cdecl [KNOWN (COFF symbol _register_png_file_type)] */
typedef void (__cdecl *PFN_register_png_file_type)(void);
#define IT_F_register_png_file_type ((PFN_register_png_file_type)0x41b96c)

/* replaceBadCharacters  VA=0x407344  size=125  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _replaceBadCharacters)] */
typedef void (__cdecl *PFN_replaceBadCharacters)(char *, char);
#define IT_F_replaceBadCharacters ((PFN_replaceBadCharacters)0x407344)

/* replay_menu_callback  VA=0x4073f8  size=712  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _replay_menu_callback)] */
typedef void (__cdecl *PFN_replay_menu_callback)(void);
#define IT_F_replay_menu_callback ((PFN_replay_menu_callback)0x4073f8)

/* replay_selector  VA=0x41d258  size=2845  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _replay_selector)] */
typedef Treplay * (__cdecl *PFN_replay_selector)(Tcontrol *, char *);
#define IT_F_replay_selector ((PFN_replay_selector)0x41d258)

/* reset_hisc_table  VA=0x405750  size=64  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _reset_hisc_table)] */
typedef void (__cdecl *PFN_reset_hisc_table)(Thisc_table *, char *, int, int);
#define IT_F_reset_hisc_table ((PFN_reset_hisc_table)0x405750)

/* reset_map  VA=0x4166a4  size=53  cu=F:\projects\icytower\trunk\source\map.c  conv=cdecl [KNOWN (COFF symbol _reset_map)] */
typedef void (__cdecl *PFN_reset_map)(Tmap *);
#define IT_F_reset_map ((PFN_reset_map)0x4166a4)

/* reset_menu  VA=0x41748c  size=79  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _reset_menu)] */
typedef void (__cdecl *PFN_reset_menu)(Tmenu *, Tmenu_params *, int);
#define IT_F_reset_menu ((PFN_reset_menu)0x41748c)

/* reset_options  VA=0x4182cc  size=205  cu=F:\projects\icytower\trunk\source\options.c  conv=cdecl [KNOWN (COFF symbol _reset_options)] */
typedef void (__cdecl *PFN_reset_options)(Toptions *);
#define IT_F_reset_options ((PFN_reset_options)0x4182cc)

/* reset_particles  VA=0x418420  size=27  cu=F:\projects\icytower\trunk\source\particle.c  conv=cdecl [KNOWN (COFF symbol _reset_particles)] */
typedef void (__cdecl *PFN_reset_particles)(Tparticle *);
#define IT_F_reset_particles ((PFN_reset_particles)0x418420)

/* reset_player  VA=0x418550  size=296  cu=F:\projects\icytower\trunk\source\player.c  conv=cdecl [KNOWN (COFF symbol _reset_player)] */
typedef void (__cdecl *PFN_reset_player)(Tplayer *);
#define IT_F_reset_player ((PFN_reset_player)0x418550)

/* restart_scroller  VA=0x41f0d0  size=28  cu=F:\projects\icytower\trunk\source\scroller.c  conv=cdecl [KNOWN (COFF symbol _restart_scroller)] */
typedef void (__cdecl *PFN_restart_scroller)(Tscroller *);
#define IT_F_restart_scroller ((PFN_restart_scroller)0x41f0d0)

/* run_demo  VA=0x415e0c  size=159  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _run_demo)] */
typedef void (__cdecl *PFN_run_demo)(char *);
#define IT_F_run_demo ((PFN_run_demo)0x415e0c)

/* save_config  VA=0x40e130  size=154  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _save_config)] */
typedef void (__cdecl *PFN_save_config)();
#define IT_F_save_config ((PFN_save_config)0x40e130)

/* save_control  VA=0x40192c  size=42  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _save_control)] */
typedef void (__cdecl *PFN_save_control)(Tcontrol *, it_orig_FILE *);
#define IT_F_save_control ((PFN_save_control)0x40192c)

/* save_garbled_data  VA=0x401344  size=283  cu=F:\projects\icytower\trunk\source\beta.c  conv=cdecl [KNOWN (COFF symbol _save_garbled_data)] */
typedef int (__cdecl *PFN_save_garbled_data)(Tbeta *, char *);
#define IT_F_save_garbled_data ((PFN_save_garbled_data)0x401344)

/* save_hisc_table  VA=0x405630  size=129  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _save_hisc_table)] */
typedef void (__cdecl *PFN_save_hisc_table)(Thisc_table *, PACKFILE *);
#define IT_F_save_hisc_table ((PFN_save_hisc_table)0x405630)

/* save_options  VA=0x4183e4  size=58  cu=F:\projects\icytower\trunk\source\options.c  conv=cdecl [KNOWN (COFF symbol _save_options)] */
typedef void (__cdecl *PFN_save_options)(Toptions *, PACKFILE *);
#define IT_F_save_options ((PFN_save_options)0x4183e4)

/* save_png  VA=0x41f000  size=115  cu=F:\projects\icytower\trunk\source\savepng.c  conv=cdecl [KNOWN (COFF symbol _save_png)] */
typedef int (__cdecl *PFN_save_png)(const char *, BITMAP *, const RGB *);
#define IT_F_save_png ((PFN_save_png)0x41f000)

/* save_profile  VA=0x41a3b8  size=1073  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _save_profile)] */
typedef int (__cdecl *PFN_save_profile)(Tprofile *);
#define IT_F_save_profile ((PFN_save_profile)0x41a3b8)

/* save_replay  VA=0x41dd78  size=1227  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _save_replay)] */
typedef int (__cdecl *PFN_save_replay)(const char *, const char *, Treplay *, int, int);
#define IT_F_save_replay ((PFN_save_replay)0x41dd78)

/* scroll_scroller  VA=0x41f0c0  size=14  cu=F:\projects\icytower\trunk\source\scroller.c  conv=cdecl [KNOWN (COFF symbol _scroll_scroller)] */
typedef void (__cdecl *PFN_scroll_scroller)(Tscroller *, int);
#define IT_F_scroll_scroller ((PFN_scroll_scroller)0x41f0c0)

/* scroll_star_field  VA=0x41f408  size=290  cu=F:\projects\icytower\trunk\source\stars.c  conv=cdecl [KNOWN (COFF symbol _scroll_star_field)] */
typedef void (__cdecl *PFN_scroll_star_field)(Tstar_field *, double, double);
#define IT_F_scroll_star_field ((PFN_scroll_star_field)0x41f408)

/* select_profile  VA=0x41acc0  size=3070  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _select_profile)] */
typedef Tprofile * (__cdecl *PFN_select_profile)(Tprofile *, Tavailable_profile *, int, Tcontrol *);
#define IT_F_select_profile ((PFN_select_profile)0x41acc0)

/* set_control  VA=0x4017d4  size=38  cu=F:\projects\icytower\trunk\source\control.c  conv=cdecl [KNOWN (COFF symbol _set_control)] */
typedef void (__cdecl *PFN_set_control)(Tcontrol *, int, int, int, int, int);
#define IT_F_set_control ((PFN_set_control)0x4017d4)

/* set_current_avatar  VA=0x406ce4  size=96  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _set_current_avatar)] */
typedef void (__cdecl *PFN_set_current_avatar)();
#define IT_F_set_current_avatar ((PFN_set_current_avatar)0x406ce4)

/* set_next_rank_message  VA=0x418b24  size=431  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _set_next_rank_message)] */
typedef void (__cdecl *PFN_set_next_rank_message)(char *, Tprofile *);
#define IT_F_set_next_rank_message ((PFN_set_next_rank_message)0x418b24)

/* set_selection_value  VA=0x416a78  size=33  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _set_selection_value)] */
typedef int (__cdecl *PFN_set_selection_value)(Tmenu_selection *, int);
#define IT_F_set_selection_value ((PFN_set_selection_value)0x416a78)

/* set_slider_value  VA=0x416a48  size=33  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _set_slider_value)] */
typedef int (__cdecl *PFN_set_slider_value)(Tmenu_slider *, int);
#define IT_F_set_slider_value ((PFN_set_slider_value)0x416a48)

/* set_sort_method  VA=0x41b9b8  size=13  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _set_sort_method)] */
typedef void (__cdecl *PFN_set_sort_method)(int);
#define IT_F_set_sort_method ((PFN_set_sort_method)0x41b9b8)

/* show_credits  VA=0x40c4dc  size=634  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _show_credits)] */
typedef void (__cdecl *PFN_show_credits)();
#define IT_F_show_credits ((PFN_show_credits)0x40c4dc)

/* show_instructions  VA=0x40c368  size=370  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _show_instructions)] */
typedef void (__cdecl *PFN_show_instructions)();
#define IT_F_show_instructions ((PFN_show_instructions)0x40c368)

/* show_name  VA=0x406d44  size=36  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _show_name)] */
typedef int (__cdecl *PFN_show_name)(const char *, int, void *);
#define IT_F_show_name ((PFN_show_name)0x406d44)

/* sort_hisc_table  VA=0x4049bc  size=147  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _sort_hisc_table)] */
typedef void (__cdecl *PFN_sort_hisc_table)(Thisc_table *);
#define IT_F_sort_hisc_table ((PFN_sort_hisc_table)0x4049bc)

/* startGameMusic  VA=0x40cb30  size=144  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _startGameMusic)] */
typedef void (__cdecl *PFN_startGameMusic)();
#define IT_F_startGameMusic ((PFN_startGameMusic)0x40cb30)

/* startMenuMusic  VA=0x406d68  size=59  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _startMenuMusic)] */
typedef void (__cdecl *PFN_startMenuMusic)();
#define IT_F_startMenuMusic ((PFN_startMenuMusic)0x406d68)

/* start_reward  VA=0x407c38  size=472  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _start_reward)] */
typedef int (__cdecl *PFN_start_reward)(int);
#define IT_F_start_reward ((PFN_start_reward)0x407c38)

/* stopGameMusic  VA=0x40caf4  size=58  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _stopGameMusic)] */
typedef void (__cdecl *PFN_stopGameMusic)();
#define IT_F_stopGameMusic ((PFN_stopGameMusic)0x40caf4)

/* stopMenuMusic  VA=0x406f5c  size=25  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _stopMenuMusic)] */
typedef void (__cdecl *PFN_stopMenuMusic)();
#define IT_F_stopMenuMusic ((PFN_stopMenuMusic)0x406f5c)

/* strptime  VA=0x41fe2c  size=35  cu=F:\projects\icytower\trunk\source\strptime.c  conv=cdecl [KNOWN (COFF symbol _strptime)] */
typedef char * (__cdecl *PFN_strptime)(const char *, const char *, struct it_orig_tm *);
#define IT_F_strptime ((PFN_strptime)0x41fe2c)

/* switchedFromProgram  VA=0x406a5c  size=15  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _switchedFromProgram)] */
typedef void (__cdecl *PFN_switchedFromProgram)();
#define IT_F_switchedFromProgram ((PFN_switchedFromProgram)0x406a5c)

/* switchedToProgram  VA=0x406a6c  size=15  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _switchedToProgram)] */
typedef void (__cdecl *PFN_switchedToProgram)();
#define IT_F_switchedToProgram ((PFN_switchedToProgram)0x406a6c)

/* syncOptionsFromProfile  VA=0x40c820  size=157  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _syncOptionsFromProfile)] */
typedef void (__cdecl *PFN_syncOptionsFromProfile)();
#define IT_F_syncOptionsFromProfile ((PFN_syncOptionsFromProfile)0x40c820)

/* syncProfileFromOptions  VA=0x406a14  size=58  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _syncProfileFromOptions)] */
typedef void (__cdecl *PFN_syncProfileFromOptions)();
#define IT_F_syncProfileFromOptions ((PFN_syncProfileFromOptions)0x406a14)

/* take_screenshot  VA=0x41002c  size=203  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _take_screenshot)] */
typedef void (__cdecl *PFN_take_screenshot)(BITMAP *);
#define IT_F_take_screenshot ((PFN_take_screenshot)0x41002c)

/* testWindowResolution  VA=0x40db18  size=388  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _testWindowResolution)] */
typedef void (__cdecl *PFN_testWindowResolution)();
#define IT_F_testWindowResolution ((PFN_testWindowResolution)0x40db18)

/* timegm  VA=0x41fe50  size=84  cu=F:\projects\icytower\trunk\source\timecompat.c  conv=cdecl [KNOWN (COFF symbol _timegm)] */
typedef it_orig_time_t (__cdecl *PFN_timegm)(struct it_orig_tm *);
#define IT_F_timegm ((PFN_timegm)0x41fe50)

/* uninit_game  VA=0x40e288  size=646  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _uninit_game)] */
typedef void (__cdecl *PFN_uninit_game)();
#define IT_F_uninit_game ((PFN_uninit_game)0x40e288)

/* update_file_list  VA=0x41bda0  size=184  cu=F:\projects\icytower\trunk\source\replay.c  conv=cdecl [KNOWN (COFF symbol _update_file_list)] */
typedef void (__cdecl *PFN_update_file_list)(char *);
#define IT_F_update_file_list ((PFN_update_file_list)0x41bda0)

/* update_frame  VA=0x406ac4  size=120  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _update_frame)] */
typedef void (__cdecl *PFN_update_frame)();
#define IT_F_update_frame ((PFN_update_frame)0x406ac4)

/* update_game_menu  VA=0x417adc  size=583  cu=F:\projects\icytower\trunk\source\menu.c  conv=cdecl [KNOWN (COFF symbol _update_game_menu)] */
typedef int (__cdecl *PFN_update_game_menu)(BITMAP *, Tmenu *, Tmenu_params *, Tcontrol *, int, int, void **);
#define IT_F_update_game_menu ((PFN_update_game_menu)0x417adc)

/* update_particle  VA=0x41843c  size=83  cu=F:\projects\icytower\trunk\source\particle.c  conv=cdecl [KNOWN (COFF symbol _update_particle)] */
typedef void (__cdecl *PFN_update_particle)(Tparticle *);
#define IT_F_update_particle ((PFN_update_particle)0x41843c)

/* update_player  VA=0x418740  size=651  cu=F:\projects\icytower\trunk\source\player.c  conv=cdecl [KNOWN (COFF symbol _update_player)] */
typedef void (__cdecl *PFN_update_player)(Tplayer *);
#define IT_F_update_player ((PFN_update_player)0x418740)

/* update_reward  VA=0x406a8c  size=55  cu=F:\projects\icytower\trunk\source\main.c  conv=cdecl [KNOWN (COFF symbol _update_reward)] */
typedef void (__cdecl *PFN_update_reward)();
#define IT_F_update_reward ((PFN_update_reward)0x406a8c)

/* view_profile  VA=0x419aec  size=2249  cu=F:\projects\icytower\trunk\source\profile.c  conv=cdecl [KNOWN (COFF symbol _view_profile)] */
typedef int (__cdecl *PFN_view_profile)(Tprofile *);
#define IT_F_view_profile ((PFN_view_profile)0x419aec)

/* view_scores  VA=0x404c38  size=2552  cu=F:\projects\icytower\trunk\source\hisc.c  conv=cdecl [KNOWN (COFF symbol _view_scores)] */
typedef void (__cdecl *PFN_view_scores)(Thisc_table **, char **);
#define IT_F_view_scores ((PFN_view_scores)0x404c38)

/* write_data  VA=0x41f074  size=76  cu=F:\projects\icytower\trunk\source\savepng.c  conv=cdecl [KNOWN (COFF symbol _write_data)] */
typedef void (__cdecl *PFN_write_data)(png_structp, png_bytep, png_uint_32);
#define IT_F_write_data ((PFN_write_data)0x41f074)

#endif /* IT_FUNCS_H */
