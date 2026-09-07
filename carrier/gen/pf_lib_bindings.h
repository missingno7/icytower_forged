/* pf_lib_bindings.h -- GENERATED FILE. DO NOT EDIT.
 * Produced by tools/pf_win32_gen_lib_bindings.py from:
 *   artifacts/lib_boundary.json (allow-list: summary.allegro_family_api_names + shared_globals.lib_globals_touched_by_game.globals)
 *   artifacts/dwarf_info.txt, artifacts/functions.json (scope=all DWARF model, reused from gen_interop.py)
 * Generated: 2026-09-07 21:05:01 UTC
 *
 * win32_pilot.md SS7b "NEXT": the library-call layer game-scope
 * pf_bindings.h does not cover. One #define per allow-listed name,
 * binding it to its ORIGINAL address inside the embedded Allegro 4.4.1
 * / logg copy, under its UPSTREAM PUBLIC NAME -- so `src/` writes
 * `blit(...)`, `key[...]`, `install_int(...)` exactly as it would
 * against a real <allegro.h>, and this header resolves those plain
 * names to (*(T*)VA) / ((PFN_LIB_name)VA) at build time, the same
 * trick pf_bindings.h already proved for game-scope names (SS7a).
 *
 * Refuses to bind anything outside the allow-list (SS7b item 2): the
 * only names read by gen_lib_bindings.py are lib_boundary.json's two
 * lists, cross-checked 1:1 against the scope=all DWARF model with 0
 * unresolved and 0 ambiguous -- a future src/ file cannot silently
 * acquire a dependency on an Allegro internal or a vorbis symbol just
 * by naming it; this generator would have to be re-run with a wider
 * allow-list first.
 *
 * Force-included (/FI), together with pf_bindings.h or
 * pf_bindings_src.h, only when address-free src/ is compiled INTO the
 * carrier (win32_pilot.md SS7a). Also defines ICYTOWER_BINDINGS_ACTIVE (idempotently --
 * see pf_bindings_src.h) so src/icytower/allegro_api.h knows to skip
 * its own declarations in that world.
 */

#ifndef PF_LIB_BINDINGS_H
#define PF_LIB_BINDINGS_H

#ifndef ICYTOWER_BINDINGS_ACTIVE
#define ICYTOWER_BINDINGS_ACTIVE 1  /* purity-safe "bindings are active" signal for src/ */
#endif

#include "pf_lib_bindings_types.h"

/* ------------------------------------------------------------------ */
/* functions (100): <name> -> ((PFN_LIB_<name>)VA)                     */
/* PFN_LIB_ (not it_funcs.h's PFN_) so this header can coexist with   */
/* pf_bindings.h/pf_bindings_src.h in the same translation unit.       */
/* ------------------------------------------------------------------ */

/* _WinMain  raw=__WinMain  VA=0x4628d8  cu=C:\Lib\allegro4\src\win\wsystem.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB__WinMain)(void *, void *, void *, char *, int);
#define _WinMain ((PFN_LIB__WinMain)0x4628d8)
/* _color_load_depth  raw=__color_load_depth  VA=0x44ea5c  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB__color_load_depth)(int, int);
#define _color_load_depth ((PFN_LIB__color_load_depth)0x44ea5c)
/* _fixup_loaded_bitmap  raw=__fixup_loaded_bitmap  VA=0x452d68  cu=C:\Lib\allegro4\src\readbmp.c  conv=cdecl */
typedef BITMAP * (__cdecl *PFN_LIB__fixup_loaded_bitmap)(BITMAP *, RGB *, int);
#define _fixup_loaded_bitmap ((PFN_LIB__fixup_loaded_bitmap)0x452d68)
/* _install_allegro_version_check  raw=__install_allegro_version_check  VA=0x43f390  cu=C:\Lib\allegro4\src\allegro.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB__install_allegro_version_check)(int, int *, int (__cdecl *)(void (__cdecl *)(void)), int);
#define _install_allegro_version_check ((PFN_LIB__install_allegro_version_check)0x43f390)
/* adjust_sample  raw=_adjust_sample  VA=0x44035c  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_adjust_sample)(const SAMPLE *, int, int, int, int);
#define adjust_sample ((PFN_LIB_adjust_sample)0x44035c)
/* alert  raw=_alert  VA=0x44befc  cu=C:\Lib\allegro4\src\gui.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_alert)(const char *, const char *, const char *, const char *, const char *, int, int);
#define alert ((PFN_LIB_alert)0x44befc)
/* allegro_exit  raw=_allegro_exit  VA=0x43ef04  cu=C:\Lib\allegro4\src\allegro.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_allegro_exit)(void);
#define allegro_exit ((PFN_LIB_allegro_exit)0x43ef04)
/* allegro_message  raw=_allegro_message  VA=0x43f2e4  cu=C:\Lib\allegro4\src\allegro.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_allegro_message)(const char *, ...);
#define allegro_message ((PFN_LIB_allegro_message)0x43f2e4)
/* blit  raw=_blit  VA=0x456264  cu=C:\Lib\allegro4\src\blit.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_blit)(BITMAP *, BITMAP *, int, int, int, int, int, int);
#define blit ((PFN_LIB_blit)0x456264)
/* canonicalize_filename  raw=_canonicalize_filename  VA=0x447980  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef char * (__cdecl *PFN_LIB_canonicalize_filename)(char *, const char *, int);
#define canonicalize_filename ((PFN_LIB_canonicalize_filename)0x447980)
/* clear_bitmap  raw=_clear_bitmap  VA=0x44c2b4  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_clear_bitmap)(BITMAP *);
#define clear_bitmap ((PFN_LIB_clear_bitmap)0x44c2b4)
/* clear_keybuf  raw=_clear_keybuf  VA=0x43dc5c  cu=C:\Lib\allegro4\src\keyboard.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_clear_keybuf)(void);
#define clear_keybuf ((PFN_LIB_clear_keybuf)0x43dc5c)
/* create_bitmap  raw=_create_bitmap  VA=0x44f3f0  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef BITMAP * (__cdecl *PFN_LIB_create_bitmap)(int, int);
#define create_bitmap ((PFN_LIB_create_bitmap)0x44f3f0)
/* create_bitmap_ex  raw=_create_bitmap_ex  VA=0x44f27c  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef BITMAP * (__cdecl *PFN_LIB_create_bitmap_ex)(int, int, int);
#define create_bitmap_ex ((PFN_LIB_create_bitmap_ex)0x44f27c)
/* create_sub_bitmap  raw=_create_sub_bitmap  VA=0x44f448  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef BITMAP * (__cdecl *PFN_LIB_create_sub_bitmap)(BITMAP *, int, int, int, int);
#define create_sub_bitmap ((PFN_LIB_create_sub_bitmap)0x44f448)
/* delete_file  raw=_delete_file  VA=0x44623c  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_delete_file)(const char *);
#define delete_file ((PFN_LIB_delete_file)0x44623c)
/* destroy_bitmap  raw=_destroy_bitmap  VA=0x44f14c  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_destroy_bitmap)(BITMAP *);
#define destroy_bitmap ((PFN_LIB_destroy_bitmap)0x44f14c)
/* destroy_midi  raw=_destroy_midi  VA=0x443f7c  cu=C:\Lib\allegro4\src\midi.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_destroy_midi)(MIDI *);
#define destroy_midi ((PFN_LIB_destroy_midi)0x443f7c)
/* destroy_sample  raw=_destroy_sample  VA=0x4408cc  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_destroy_sample)(SAMPLE *);
#define destroy_sample ((PFN_LIB_destroy_sample)0x4408cc)
/* drawing_mode  raw=_drawing_mode  VA=0x44bf54  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_drawing_mode)(int, BITMAP *, int, int);
#define drawing_mode ((PFN_LIB_drawing_mode)0x44bf54)
/* enable_hardware_cursor  raw=_enable_hardware_cursor  VA=0x4605a4  cu=C:\Lib\allegro4\src\mouse.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_enable_hardware_cursor)(void);
#define enable_hardware_cursor ((PFN_LIB_enable_hardware_cursor)0x4605a4)
/* exists  raw=_exists  VA=0x446218  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_exists)(const char *);
#define exists ((PFN_LIB_exists)0x446218)
/* file_exists  raw=_file_exists  VA=0x446150  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_file_exists)(const char *, int, int *);
#define file_exists ((PFN_LIB_file_exists)0x446150)
/* file_select_ex  raw=_file_select_ex  VA=0x466dfc  cu=C:\Lib\allegro4\src\fsel.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_file_select_ex)(const char *, char *, const char *, int, int, int);
#define file_select_ex ((PFN_LIB_file_select_ex)0x466dfc)
/* file_size_ex  raw=_file_size_ex  VA=0x4465c0  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef uint64_t (__cdecl *PFN_LIB_file_size_ex)(const char *);
#define file_size_ex ((PFN_LIB_file_size_ex)0x4465c0)
/* for_each_file_ex  raw=_for_each_file_ex  VA=0x447798  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_for_each_file_ex)(const char *, int, int, int (__cdecl *)(const char *, int, void *), void *);
#define for_each_file_ex ((PFN_LIB_for_each_file_ex)0x447798)
/* generate_332_palette  raw=_generate_332_palette  VA=0x44c3cc  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_generate_332_palette)(RGB *);
#define generate_332_palette ((PFN_LIB_generate_332_palette)0x44c3cc)
/* get_color_conversion  raw=_get_color_conversion  VA=0x44ea50  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_get_color_conversion)(void);
#define get_color_conversion ((PFN_LIB_get_color_conversion)0x44ea50)
/* get_config_string  raw=_get_config_string  VA=0x4646b8  cu=C:\Lib\allegro4\src\config.c  conv=cdecl */
typedef const char * (__cdecl *PFN_LIB_get_config_string)(const char *, const char *, const char *);
#define get_config_string ((PFN_LIB_get_config_string)0x4646b8)
/* get_executable_name  raw=_get_executable_name  VA=0x43edf0  cu=C:\Lib\allegro4\src\allegro.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_get_executable_name)(char *, int);
#define get_executable_name ((PFN_LIB_get_executable_name)0x43edf0)
/* get_extension  raw=_get_extension  VA=0x44681c  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef char * (__cdecl *PFN_LIB_get_extension)(const char *);
#define get_extension ((PFN_LIB_get_extension)0x44681c)
/* get_filename  raw=_get_filename  VA=0x44452c  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef char * (__cdecl *PFN_LIB_get_filename)(const char *);
#define get_filename ((PFN_LIB_get_filename)0x44452c)
/* get_palette  raw=_get_palette  VA=0x44c47c  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_get_palette)(RGB *);
#define get_palette ((PFN_LIB_get_palette)0x44c47c)
/* install_int  raw=_install_int  VA=0x45dd8c  cu=C:\Lib\allegro4\src\timer.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_install_int)(void (__cdecl *)(void), long);
#define install_int ((PFN_LIB_install_int)0x45dd8c)
/* install_joystick  raw=_install_joystick  VA=0x43e958  cu=C:\Lib\allegro4\src\joystick.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_install_joystick)(int);
#define install_joystick ((PFN_LIB_install_joystick)0x43e958)
/* install_keyboard  raw=_install_keyboard  VA=0x43de28  cu=C:\Lib\allegro4\src\keyboard.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_install_keyboard)(void);
#define install_keyboard ((PFN_LIB_install_keyboard)0x43de28)
/* install_mouse  raw=_install_mouse  VA=0x460de0  cu=C:\Lib\allegro4\src\mouse.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_install_mouse)(void);
#define install_mouse ((PFN_LIB_install_mouse)0x460de0)
/* install_sound  raw=_install_sound  VA=0x4417b0  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_install_sound)(int, int, const char *);
#define install_sound ((PFN_LIB_install_sound)0x4417b0)
/* install_timer  raw=_install_timer  VA=0x45da68  cu=C:\Lib\allegro4\src\timer.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_install_timer)(void);
#define install_timer ((PFN_LIB_install_timer)0x45da68)
/* keypressed  raw=_keypressed  VA=0x43dc20  cu=C:\Lib\allegro4\src\keyboard.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_keypressed)(void);
#define keypressed ((PFN_LIB_keypressed)0x43dc20)
/* load_bitmap  raw=_load_bitmap  VA=0x452f90  cu=C:\Lib\allegro4\src\readbmp.c  conv=cdecl */
typedef BITMAP * (__cdecl *PFN_LIB_load_bitmap)(const char *, RGB *);
#define load_bitmap ((PFN_LIB_load_bitmap)0x452f90)
/* load_datafile  raw=_load_datafile  VA=0x4560dc  cu=C:\Lib\allegro4\src\datafile.c  conv=cdecl */
typedef DATAFILE * (__cdecl *PFN_LIB_load_datafile)(const char *);
#define load_datafile ((PFN_LIB_load_datafile)0x4560dc)
/* load_datafile_callback  raw=_load_datafile_callback  VA=0x455d40  cu=C:\Lib\allegro4\src\datafile.c  conv=cdecl */
typedef DATAFILE * (__cdecl *PFN_LIB_load_datafile_callback)(const char *, void (__cdecl *)(DATAFILE *));
#define load_datafile_callback ((PFN_LIB_load_datafile_callback)0x455d40)
/* load_midi  raw=_load_midi  VA=0x443fcc  cu=C:\Lib\allegro4\src\midi.c  conv=cdecl */
typedef MIDI * (__cdecl *PFN_LIB_load_midi)(const char *);
#define load_midi ((PFN_LIB_load_midi)0x443fcc)
/* load_sample  raw=_load_sample  VA=0x4442f4  cu=C:\Lib\allegro4\src\readsmp.c  conv=cdecl */
typedef SAMPLE * (__cdecl *PFN_LIB_load_sample)(const char *);
#define load_sample ((PFN_LIB_load_sample)0x4442f4)
/* load_wav  raw=_load_wav  VA=0x440f14  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef SAMPLE * (__cdecl *PFN_LIB_load_wav)(const char *);
#define load_wav ((PFN_LIB_load_wav)0x440f14)
/* logg_load  raw=_logg_load  VA=0x4205cc  cu=C:\Lib\allegro4\addons\logg\logg.c  conv=cdecl */
typedef SAMPLE * (__cdecl *PFN_LIB_logg_load)(const char *);
#define logg_load ((PFN_LIB_logg_load)0x4205cc)
/* logg_load_memory  raw=_logg_load_memory  VA=0x420688  cu=C:\Lib\allegro4\addons\logg\logg.c  conv=cdecl */
typedef SAMPLE * (__cdecl *PFN_LIB_logg_load_memory)(void *, it_orig_size_t);
#define logg_load_memory ((PFN_LIB_logg_load_memory)0x420688)
/* makecol  raw=_makecol  VA=0x450c98  cu=C:\Lib\allegro4\src\color.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_makecol)(int, int, int);
#define makecol ((PFN_LIB_makecol)0x450c98)
/* makecol_depth  raw=_makecol_depth  VA=0x450bd0  cu=C:\Lib\allegro4\src\color.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_makecol_depth)(int, int, int, int);
#define makecol_depth ((PFN_LIB_makecol_depth)0x450bd0)
/* masked_blit  raw=_masked_blit  VA=0x4560f8  cu=C:\Lib\allegro4\src\blit.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_masked_blit)(BITMAP *, BITMAP *, int, int, int, int, int, int);
#define masked_blit ((PFN_LIB_masked_blit)0x4560f8)
/* pack_fclose  raw=_pack_fclose  VA=0x444d78  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_pack_fclose)(PACKFILE *);
#define pack_fclose ((PFN_LIB_pack_fclose)0x444d78)
/* pack_fopen  raw=_pack_fopen  VA=0x445afc  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef PACKFILE * (__cdecl *PFN_LIB_pack_fopen)(const char *, const char *);
#define pack_fopen ((PFN_LIB_pack_fopen)0x445afc)
/* pack_fread  raw=_pack_fread  VA=0x4449b0  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef long (__cdecl *PFN_LIB_pack_fread)(void *, long, PACKFILE *);
#define pack_fread ((PFN_LIB_pack_fread)0x4449b0)
/* pack_fwrite  raw=_pack_fwrite  VA=0x4449c8  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef long (__cdecl *PFN_LIB_pack_fwrite)(const void *, long, PACKFILE *);
#define pack_fwrite ((PFN_LIB_pack_fwrite)0x4449c8)
/* packfile_password  raw=_packfile_password  VA=0x444590  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_packfile_password)(const char *);
#define packfile_password ((PFN_LIB_packfile_password)0x444590)
/* play_midi  raw=_play_midi  VA=0x443648  cu=C:\Lib\allegro4\src\midi.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_play_midi)(MIDI *, int);
#define play_midi ((PFN_LIB_play_midi)0x443648)
/* play_sample  raw=_play_sample  VA=0x440404  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_play_sample)(const SAMPLE *, int, int, int, int);
#define play_sample ((PFN_LIB_play_sample)0x440404)
/* poll_joystick  raw=_poll_joystick  VA=0x43e654  cu=C:\Lib\allegro4\src\joystick.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_poll_joystick)(void);
#define poll_joystick ((PFN_LIB_poll_joystick)0x43e654)
/* readkey  raw=_readkey  VA=0x43e0d8  cu=C:\Lib\allegro4\src\keyboard.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_readkey)(void);
#define readkey ((PFN_LIB_readkey)0x43e0d8)
/* register_bitmap_file_type  raw=_register_bitmap_file_type  VA=0x45301c  cu=C:\Lib\allegro4\src\readbmp.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_register_bitmap_file_type)(const char *, BITMAP * (__cdecl *)(const char *, RGB *), int (__cdecl *)(const char *, BITMAP *, const RGB *));
#define register_bitmap_file_type ((PFN_LIB_register_bitmap_file_type)0x45301c)
/* register_datafile_object  raw=_register_datafile_object  VA=0x465e68  cu=C:\Lib\allegro4\src\dataregi.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_register_datafile_object)(int, void * (__cdecl *)(PACKFILE *, long), void (__cdecl *)(void *));
#define register_datafile_object ((PFN_LIB_register_datafile_object)0x465e68)
/* remove_mouse  raw=_remove_mouse  VA=0x460388  cu=C:\Lib\allegro4\src\mouse.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_remove_mouse)(void);
#define remove_mouse ((PFN_LIB_remove_mouse)0x460388)
/* replace_extension  raw=_replace_extension  VA=0x4475e4  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef char * (__cdecl *PFN_LIB_replace_extension)(char *, const char *, const char *, int);
#define replace_extension ((PFN_LIB_replace_extension)0x4475e4)
/* replace_filename  raw=_replace_filename  VA=0x4476fc  cu=C:\Lib\allegro4\src\file.c  conv=cdecl */
typedef char * (__cdecl *PFN_LIB_replace_filename)(char *, const char *, const char *, int);
#define replace_filename ((PFN_LIB_replace_filename)0x4476fc)
/* rest  raw=_rest  VA=0x45dea8  cu=C:\Lib\allegro4\src\timer.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_rest)(unsigned int);
#define rest ((PFN_LIB_rest)0x45dea8)
/* save_bitmap  raw=_save_bitmap  VA=0x452ef8  cu=C:\Lib\allegro4\src\readbmp.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_save_bitmap)(const char *, BITMAP *, const RGB *);
#define save_bitmap ((PFN_LIB_save_bitmap)0x452ef8)
/* select_mouse_cursor  raw=_select_mouse_cursor  VA=0x45f9d8  cu=C:\Lib\allegro4\src\mouse.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_select_mouse_cursor)(int);
#define select_mouse_cursor ((PFN_LIB_select_mouse_cursor)0x45f9d8)
/* select_palette  raw=_select_palette  VA=0x44df24  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_select_palette)(const RGB *);
#define select_palette ((PFN_LIB_select_palette)0x44df24)
/* set_alpha_blender  raw=_set_alpha_blender  VA=0x45be04  cu=C:\Lib\allegro4\src\colblend.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_alpha_blender)(void);
#define set_alpha_blender ((PFN_LIB_set_alpha_blender)0x45be04)
/* set_clip_rect  raw=_set_clip_rect  VA=0x44eb70  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_clip_rect)(BITMAP *, int, int, int, int);
#define set_clip_rect ((PFN_LIB_set_clip_rect)0x44eb70)
/* set_close_button_callback  raw=_set_close_button_callback  VA=0x43ee4c  cu=C:\Lib\allegro4\src\allegro.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_set_close_button_callback)(void (__cdecl *)(void));
#define set_close_button_callback ((PFN_LIB_set_close_button_callback)0x43ee4c)
/* set_color_conversion  raw=_set_color_conversion  VA=0x44ea38  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_color_conversion)(int);
#define set_color_conversion ((PFN_LIB_set_color_conversion)0x44ea38)
/* set_color_depth  raw=_set_color_depth  VA=0x44e9d4  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_color_depth)(int);
#define set_color_depth ((PFN_LIB_set_color_depth)0x44e9d4)
/* set_config_file  raw=_set_config_file  VA=0x463fe8  cu=C:\Lib\allegro4\src\config.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_config_file)(const char *);
#define set_config_file ((PFN_LIB_set_config_file)0x463fe8)
/* set_display_switch_callback  raw=_set_display_switch_callback  VA=0x465734  cu=C:\Lib\allegro4\src\dispsw.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_set_display_switch_callback)(int, void (__cdecl *)(void));
#define set_display_switch_callback ((PFN_LIB_set_display_switch_callback)0x465734)
/* set_display_switch_mode  raw=_set_display_switch_mode  VA=0x4656c8  cu=C:\Lib\allegro4\src\dispsw.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_set_display_switch_mode)(int);
#define set_display_switch_mode ((PFN_LIB_set_display_switch_mode)0x4656c8)
/* set_gfx_mode  raw=_set_gfx_mode  VA=0x450688  cu=C:\Lib\allegro4\src\graphics.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_set_gfx_mode)(int, int, int, int, int);
#define set_gfx_mode ((PFN_LIB_set_gfx_mode)0x450688)
/* set_palette  raw=_set_palette  VA=0x44e370  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_palette)(const RGB *);
#define set_palette ((PFN_LIB_set_palette)0x44e370)
/* set_trans_blender  raw=_set_trans_blender  VA=0x45c5d8  cu=C:\Lib\allegro4\src\colblend.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_trans_blender)(int, int, int, int);
#define set_trans_blender ((PFN_LIB_set_trans_blender)0x45c5d8)
/* set_volume  raw=_set_volume  VA=0x440990  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_set_volume)(int, int);
#define set_volume ((PFN_LIB_set_volume)0x440990)
/* show_mouse  raw=_show_mouse  VA=0x4600fc  cu=C:\Lib\allegro4\src\mouse.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_show_mouse)(BITMAP *);
#define show_mouse ((PFN_LIB_show_mouse)0x4600fc)
/* simulate_keypress  raw=_simulate_keypress  VA=0x43e104  cu=C:\Lib\allegro4\src\keyboard.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_simulate_keypress)(int);
#define simulate_keypress ((PFN_LIB_simulate_keypress)0x43e104)
/* solid_mode  raw=_solid_mode  VA=0x44c288  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_solid_mode)(void);
#define solid_mode ((PFN_LIB_solid_mode)0x44c288)
/* stop_midi  raw=_stop_midi  VA=0x4438d8  cu=C:\Lib\allegro4\src\midi.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_stop_midi)(void);
#define stop_midi ((PFN_LIB_stop_midi)0x4438d8)
/* stop_sample  raw=_stop_sample  VA=0x43fc64  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_stop_sample)(const SAMPLE *);
#define stop_sample ((PFN_LIB_stop_sample)0x43fc64)
/* stretch_blit  raw=_stretch_blit  VA=0x4632f8  cu=C:\Lib\allegro4\src\c\cstretch.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_stretch_blit)(BITMAP *, BITMAP *, int, int, int, int, int, int, int, int);
#define stretch_blit ((PFN_LIB_stretch_blit)0x4632f8)
/* stretch_sprite  raw=_stretch_sprite  VA=0x463398  cu=C:\Lib\allegro4\src\c\cstretch.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_stretch_sprite)(BITMAP *, BITMAP *, int, int, int, int);
#define stretch_sprite ((PFN_LIB_stretch_sprite)0x463398)
/* text_height  raw=_text_height  VA=0x45a040  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_text_height)(const FONT *);
#define text_height ((PFN_LIB_text_height)0x45a040)
/* text_length  raw=_text_length  VA=0x459f50  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_text_length)(const FONT *, const char *);
#define text_length ((PFN_LIB_text_length)0x459f50)
/* textout_centre_ex  raw=_textout_centre_ex  VA=0x459fcc  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_textout_centre_ex)(BITMAP *, const FONT *, const char *, int, int, int, int);
#define textout_centre_ex ((PFN_LIB_textout_centre_ex)0x459fcc)
/* textout_ex  raw=_textout_ex  VA=0x459f0c  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_textout_ex)(BITMAP *, const FONT *, const char *, int, int, int, int);
#define textout_ex ((PFN_LIB_textout_ex)0x459f0c)
/* textout_right_ex  raw=_textout_right_ex  VA=0x459f64  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_textout_right_ex)(BITMAP *, const FONT *, const char *, int, int, int, int);
#define textout_right_ex ((PFN_LIB_textout_right_ex)0x459f64)
/* textprintf_centre_ex  raw=_textprintf_centre_ex  VA=0x45a23c  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_textprintf_centre_ex)(BITMAP *, const FONT *, int, int, int, int, const char *, ...);
#define textprintf_centre_ex ((PFN_LIB_textprintf_centre_ex)0x45a23c)
/* textprintf_ex  raw=_textprintf_ex  VA=0x45a2a8  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_textprintf_ex)(BITMAP *, const FONT *, int, int, int, int, const char *, ...);
#define textprintf_ex ((PFN_LIB_textprintf_ex)0x45a2a8)
/* textprintf_right_ex  raw=_textprintf_right_ex  VA=0x45a1d0  cu=C:\Lib\allegro4\src\text.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_textprintf_right_ex)(BITMAP *, const FONT *, int, int, int, int, const char *, ...);
#define textprintf_right_ex ((PFN_LIB_textprintf_right_ex)0x45a1d0)
/* unload_datafile  raw=_unload_datafile  VA=0x454174  cu=C:\Lib\allegro4\src\datafile.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_unload_datafile)(DATAFILE *);
#define unload_datafile ((PFN_LIB_unload_datafile)0x454174)
/* voice_get_position  raw=_voice_get_position  VA=0x43fe54  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef int (__cdecl *PFN_LIB_voice_get_position)(int);
#define voice_get_position ((PFN_LIB_voice_get_position)0x43fe54)
/* voice_stop  raw=_voice_stop  VA=0x43fe10  cu=C:\Lib\allegro4\src\sound.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_voice_stop)(int);
#define voice_stop ((PFN_LIB_voice_stop)0x43fe10)
/* vsync  raw=_vsync  VA=0x44c340  cu=C:\Lib\allegro4\src\gfx.c  conv=cdecl */
typedef void (__cdecl *PFN_LIB_vsync)(void);
#define vsync ((PFN_LIB_vsync)0x44c340)

/* ------------------------------------------------------------------ */
/* globals (26): <name> -> (*(T*)VA)                                  */
/* ------------------------------------------------------------------ */

/* _rgb_a_shift_32  raw=__rgb_a_shift_32  VA=0x4cc9f4  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_a_shift_32 (*(int *)0x4cc9f4)
/* _rgb_b_shift_15  raw=__rgb_b_shift_15  VA=0x4cc9d8  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_b_shift_15 (*(int *)0x4cc9d8)
/* _rgb_b_shift_16  raw=__rgb_b_shift_16  VA=0x4cc9e0  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_b_shift_16 (*(int *)0x4cc9e0)
/* _rgb_b_shift_24  raw=__rgb_b_shift_24  VA=0x4cc9e8  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_b_shift_24 (*(int *)0x4cc9e8)
/* _rgb_b_shift_32  raw=__rgb_b_shift_32  VA=0x4cc9f0  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_b_shift_32 (*(int *)0x4cc9f0)
/* _rgb_g_shift_15  raw=__rgb_g_shift_15  VA=0x4cc9d4  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_g_shift_15 (*(int *)0x4cc9d4)
/* _rgb_g_shift_16  raw=__rgb_g_shift_16  VA=0x4cc9dc  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_g_shift_16 (*(int *)0x4cc9dc)
/* _rgb_g_shift_24  raw=__rgb_g_shift_24  VA=0x4cc9e4  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_g_shift_24 (*(int *)0x4cc9e4)
/* _rgb_g_shift_32  raw=__rgb_g_shift_32  VA=0x4cc9ec  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_g_shift_32 (*(int *)0x4cc9ec)
/* _rgb_r_shift_15  raw=__rgb_r_shift_15  VA=0x4e8668  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_r_shift_15 (*(int *)0x4e8668)
/* _rgb_r_shift_16  raw=__rgb_r_shift_16  VA=0x4e866c  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_r_shift_16 (*(int *)0x4e866c)
/* _rgb_r_shift_24  raw=__rgb_r_shift_24  VA=0x4e8670  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_r_shift_24 (*(int *)0x4e8670)
/* _rgb_r_shift_32  raw=__rgb_r_shift_32  VA=0x4e8674  type=int  cu=C:\Lib\allegro4\src\graphics.c */
#define _rgb_r_shift_32 (*(int *)0x4e8674)
/* _win_hcursor  raw=__win_hcursor  VA=0x4e8e20  type=HCURSOR  cu=C:\Lib\allegro4\src\win\wmouse.c */
#define _win_hcursor (*(HCURSOR *)0x4e8e20)
/* allegro_errno  raw=_allegro_errno  VA=0x4dda78  type=int *  cu=C:\Lib\allegro4\src\allegro.c */
#define allegro_errno (*(int **)0x4dda78)
/* font  raw=_font  VA=0x4ccc94  type=FONT *  cu=C:\Lib\allegro4\src\font.c */
#define font (*(FONT **)0x4ccc94)
/* gfx_driver  raw=_gfx_driver  VA=0x4dda84  type=GFX_DRIVER *  cu=C:\Lib\allegro4\src\allegro.c */
#define gfx_driver (*(GFX_DRIVER **)0x4dda84)
/* gui_bg_color  raw=_gui_bg_color  VA=0x4ddaa8  type=int  cu=C:\Lib\allegro4\src\allegro.c */
#define gui_bg_color (*(int *)0x4ddaa8)
/* gui_fg_color  raw=_gui_fg_color  VA=0x4cc3a0  type=int  cu=C:\Lib\allegro4\src\allegro.c */
#define gui_fg_color (*(int *)0x4cc3a0)
/* joy  raw=_joy  VA=0x506a88  type=JOYSTICK_INFO [8]  cu=C:\Lib\allegro4\src\joystick.c */
#define joy (*(JOYSTICK_INFO (*)[8])0x506a88)
/* key  raw=_key  VA=0x506988  type=volatile char [127]  cu=C:\Lib\allegro4\src\keyboard.c */
#define key (*(volatile char (*)[127])0x506988)
/* mouse_b  raw=_mouse_b  VA=0x4e8cf8  type=volatile int  cu=C:\Lib\allegro4\src\mouse.c */
#define mouse_b (*(volatile int *)0x4e8cf8)
/* mouse_x  raw=_mouse_x  VA=0x4e8ce8  type=volatile int  cu=C:\Lib\allegro4\src\mouse.c */
#define mouse_x (*(volatile int *)0x4e8ce8)
/* mouse_y  raw=_mouse_y  VA=0x4e8cec  type=volatile int  cu=C:\Lib\allegro4\src\mouse.c */
#define mouse_y (*(volatile int *)0x4e8cec)
/* screen  raw=_screen  VA=0x4dda8c  type=BITMAP *  cu=C:\Lib\allegro4\src\allegro.c */
#define screen (*(BITMAP **)0x4dda8c)
/* system_driver  raw=_system_driver  VA=0x4dda74  type=SYSTEM_DRIVER *  cu=C:\Lib\allegro4\src\allegro.c */
#define system_driver (*(SYSTEM_DRIVER **)0x4dda74)

/* ------------------------------------------------------------------ */
/* AL_INLINE vtable-dispatch macros (23): <name> -> one call through   */
/* the first argument's OWN bmp->vtable-><slot>(...) -- these have NO  */
/* VA of their own (upstream `static inline` in allegro/inline/        */
/* {gfx,draw}.inl compiles them directly into the CALLER's .text, so   */
/* no distinct callee address can ever appear in a call-edge census)   */
/* -- see this generator's AL_INLINE_VTABLE_DISPATCH comment for       */
/* exactly which upstream names qualify (a straight one-call passthrough */
/* with no branch/rounding to reproduce) and which do not.              */
/* ------------------------------------------------------------------ */

/* getpixel  GFX_VTABLE slot=getpixel  argc=3  (no VA -- inlined vtable dispatch, not a call target) */
#define getpixel(a0, a1, a2) ((a0)->vtable->getpixel((a0), (a1), (a2)))
/* putpixel  GFX_VTABLE slot=putpixel  argc=4  (no VA -- inlined vtable dispatch, not a call target) */
#define putpixel(a0, a1, a2, a3) ((a0)->vtable->putpixel((a0), (a1), (a2), (a3)))
/* vline  GFX_VTABLE slot=vline  argc=5  (no VA -- inlined vtable dispatch, not a call target) */
#define vline(a0, a1, a2, a3, a4) ((a0)->vtable->vline((a0), (a1), (a2), (a3), (a4)))
/* hline  GFX_VTABLE slot=hline  argc=5  (no VA -- inlined vtable dispatch, not a call target) */
#define hline(a0, a1, a2, a3, a4) ((a0)->vtable->hline((a0), (a1), (a2), (a3), (a4)))
/* line  GFX_VTABLE slot=line  argc=6  (no VA -- inlined vtable dispatch, not a call target) */
#define line(a0, a1, a2, a3, a4, a5) ((a0)->vtable->line((a0), (a1), (a2), (a3), (a4), (a5)))
/* fastline  GFX_VTABLE slot=fastline  argc=6  (no VA -- inlined vtable dispatch, not a call target) */
#define fastline(a0, a1, a2, a3, a4, a5) ((a0)->vtable->fastline((a0), (a1), (a2), (a3), (a4), (a5)))
/* rectfill  GFX_VTABLE slot=rectfill  argc=6  (no VA -- inlined vtable dispatch, not a call target) */
#define rectfill(a0, a1, a2, a3, a4, a5) ((a0)->vtable->rectfill((a0), (a1), (a2), (a3), (a4), (a5)))
/* triangle  GFX_VTABLE slot=triangle  argc=8  (no VA -- inlined vtable dispatch, not a call target) */
#define triangle(a0, a1, a2, a3, a4, a5, a6, a7) ((a0)->vtable->triangle((a0), (a1), (a2), (a3), (a4), (a5), (a6), (a7)))
/* polygon  GFX_VTABLE slot=polygon  argc=4  (no VA -- inlined vtable dispatch, not a call target) */
#define polygon(a0, a1, a2, a3) ((a0)->vtable->polygon((a0), (a1), (a2), (a3)))
/* rect  GFX_VTABLE slot=rect  argc=6  (no VA -- inlined vtable dispatch, not a call target) */
#define rect(a0, a1, a2, a3, a4, a5) ((a0)->vtable->rect((a0), (a1), (a2), (a3), (a4), (a5)))
/* circle  GFX_VTABLE slot=circle  argc=5  (no VA -- inlined vtable dispatch, not a call target) */
#define circle(a0, a1, a2, a3, a4) ((a0)->vtable->circle((a0), (a1), (a2), (a3), (a4)))
/* circlefill  GFX_VTABLE slot=circlefill  argc=5  (no VA -- inlined vtable dispatch, not a call target) */
#define circlefill(a0, a1, a2, a3, a4) ((a0)->vtable->circlefill((a0), (a1), (a2), (a3), (a4)))
/* ellipse  GFX_VTABLE slot=ellipse  argc=6  (no VA -- inlined vtable dispatch, not a call target) */
#define ellipse(a0, a1, a2, a3, a4, a5) ((a0)->vtable->ellipse((a0), (a1), (a2), (a3), (a4), (a5)))
/* ellipsefill  GFX_VTABLE slot=ellipsefill  argc=6  (no VA -- inlined vtable dispatch, not a call target) */
#define ellipsefill(a0, a1, a2, a3, a4, a5) ((a0)->vtable->ellipsefill((a0), (a1), (a2), (a3), (a4), (a5)))
/* arc  GFX_VTABLE slot=arc  argc=7  (no VA -- inlined vtable dispatch, not a call target) */
#define arc(a0, a1, a2, a3, a4, a5, a6) ((a0)->vtable->arc((a0), (a1), (a2), (a3), (a4), (a5), (a6)))
/* spline  GFX_VTABLE slot=spline  argc=3  (no VA -- inlined vtable dispatch, not a call target) */
#define spline(a0, a1, a2) ((a0)->vtable->spline((a0), (a1), (a2)))
/* floodfill  GFX_VTABLE slot=floodfill  argc=4  (no VA -- inlined vtable dispatch, not a call target) */
#define floodfill(a0, a1, a2, a3) ((a0)->vtable->floodfill((a0), (a1), (a2), (a3)))
/* clear_to_color  GFX_VTABLE slot=clear_to_color  argc=2  (no VA -- inlined vtable dispatch, not a call target) */
#define clear_to_color(a0, a1) ((a0)->vtable->clear_to_color((a0), (a1)))
/* draw_sprite_v_flip  GFX_VTABLE slot=draw_sprite_v_flip  argc=4  (no VA -- inlined vtable dispatch, not a call target) */
#define draw_sprite_v_flip(a0, a1, a2, a3) ((a0)->vtable->draw_sprite_v_flip((a0), (a1), (a2), (a3)))
/* draw_sprite_h_flip  GFX_VTABLE slot=draw_sprite_h_flip  argc=4  (no VA -- inlined vtable dispatch, not a call target) */
#define draw_sprite_h_flip(a0, a1, a2, a3) ((a0)->vtable->draw_sprite_h_flip((a0), (a1), (a2), (a3)))
/* draw_sprite_vh_flip  GFX_VTABLE slot=draw_sprite_vh_flip  argc=4  (no VA -- inlined vtable dispatch, not a call target) */
#define draw_sprite_vh_flip(a0, a1, a2, a3) ((a0)->vtable->draw_sprite_vh_flip((a0), (a1), (a2), (a3)))
/* draw_lit_sprite  GFX_VTABLE slot=draw_lit_sprite  argc=5  (no VA -- inlined vtable dispatch, not a call target) */
#define draw_lit_sprite(a0, a1, a2, a3, a4) ((a0)->vtable->draw_lit_sprite((a0), (a1), (a2), (a3), (a4)))
/* draw_gouraud_sprite  GFX_VTABLE slot=draw_gouraud_sprite  argc=8  (no VA -- inlined vtable dispatch, not a call target) */
#define draw_gouraud_sprite(a0, a1, a2, a3, a4, a5, a6, a7) ((a0)->vtable->draw_gouraud_sprite((a0), (a1), (a2), (a3), (a4), (a5), (a6), (a7)))

/* ------------------------------------------------------------------ */
/* constants (150): KEY_*, GFX_*, DRAW_MODE_*, MASK_COLOR_* -- hand-    */
/* transcribed, not DWARF-derived (macros leave no DWARF trace); see  */
/* this generator's module docstring "Constants" and                 */
/* LIB_BINDINGS_NOTES.md for the evidence label on each.              */
/* ------------------------------------------------------------------ */

/* MEASURED */
#define KEY_MAX 0x7f

/* INFERRED-FROM-PROJECT-EVIDENCE */
#define GFX_DIRECTX_ACCEL 0x44584143
#define GFX_DIRECTX_SOFT 0x4458534f
#define GFX_DIRECTX_SAFE 0x44585341
#define GFX_DIRECTX_WIN 0x4458574e
#define GFX_DIRECTX_OVL 0x44584f56
#define GFX_GDI 0x47444942

/* INFERRED-FROM-UPSTREAM-DOCS */
#define KEY_A 1
#define KEY_B 2
#define KEY_C 3
#define KEY_D 4
#define KEY_E 5
#define KEY_F 6
#define KEY_G 7
#define KEY_H 8
#define KEY_I 9
#define KEY_J 0xa
#define KEY_K 0xb
#define KEY_L 0xc
#define KEY_M 0xd
#define KEY_N 0xe
#define KEY_O 0xf
#define KEY_P 0x10
#define KEY_Q 0x11
#define KEY_R 0x12
#define KEY_S 0x13
#define KEY_T 0x14
#define KEY_U 0x15
#define KEY_V 0x16
#define KEY_W 0x17
#define KEY_X 0x18
#define KEY_Y 0x19
#define KEY_Z 0x1a
#define KEY_0 0x1b
#define KEY_1 0x1c
#define KEY_2 0x1d
#define KEY_3 0x1e
#define KEY_4 0x1f
#define KEY_5 0x20
#define KEY_6 0x21
#define KEY_7 0x22
#define KEY_8 0x23
#define KEY_9 0x24
#define KEY_0_PAD 0x25
#define KEY_1_PAD 0x26
#define KEY_2_PAD 0x27
#define KEY_3_PAD 0x28
#define KEY_4_PAD 0x29
#define KEY_5_PAD 0x2a
#define KEY_6_PAD 0x2b
#define KEY_7_PAD 0x2c
#define KEY_8_PAD 0x2d
#define KEY_9_PAD 0x2e
#define KEY_F1 0x2f
#define KEY_F2 0x30
#define KEY_F3 0x31
#define KEY_F4 0x32
#define KEY_F5 0x33
#define KEY_F6 0x34
#define KEY_F7 0x35
#define KEY_F8 0x36
#define KEY_F9 0x37
#define KEY_F10 0x38
#define KEY_F11 0x39
#define KEY_F12 0x3a
#define KEY_ESC 0x3b
#define KEY_TILDE 0x3c
#define KEY_MINUS 0x3d
#define KEY_EQUALS 0x3e
#define KEY_BACKSPACE 0x3f
#define KEY_TAB 0x40
#define KEY_OPENBRACE 0x41
#define KEY_CLOSEBRACE 0x42
#define KEY_ENTER 0x43
#define KEY_COLON 0x44
#define KEY_QUOTE 0x45
#define KEY_BACKSLASH 0x46
#define KEY_BACKSLASH2 0x47
#define KEY_COMMA 0x48
#define KEY_STOP 0x49
#define KEY_SLASH 0x4a
#define KEY_SPACE 0x4b
#define KEY_INSERT 0x4c
#define KEY_DEL 0x4d
#define KEY_HOME 0x4e
#define KEY_END 0x4f
#define KEY_PGUP 0x50
#define KEY_PGDN 0x51
#define KEY_LEFT 0x52
#define KEY_RIGHT 0x53
#define KEY_UP 0x54
#define KEY_DOWN 0x55
#define KEY_SLASH_PAD 0x56
#define KEY_ASTERISK 0x57
#define KEY_MINUS_PAD 0x58
#define KEY_PLUS_PAD 0x59
#define KEY_DEL_PAD 0x5a
#define KEY_ENTER_PAD 0x5b
#define KEY_PRTSCR 0x5c
#define KEY_PAUSE 0x5d
#define KEY_ABNT_C1 0x5e
#define KEY_YEN 0x5f
#define KEY_KANA 0x60
#define KEY_CONVERT 0x61
#define KEY_NOCONVERT 0x62
#define KEY_AT 0x63
#define KEY_CIRCUMFLEX 0x64
#define KEY_COLON2 0x65
#define KEY_KANJI 0x66
#define KEY_EQUALS_PAD 0x67
#define KEY_BACKQUOTE 0x68
#define KEY_SEMICOLON 0x69
#define KEY_COMMAND 0x6a
#define KEY_UNKNOWN1 0x6b
#define KEY_UNKNOWN2 0x6c
#define KEY_UNKNOWN3 0x6d
#define KEY_UNKNOWN4 0x6e
#define KEY_UNKNOWN5 0x6f
#define KEY_UNKNOWN6 0x70
#define KEY_UNKNOWN7 0x71
#define KEY_UNKNOWN8 0x72
#define KEY_LSHIFT 0x73
#define KEY_RSHIFT 0x74
#define KEY_LCONTROL 0x75
#define KEY_RCONTROL 0x76
#define KEY_ALT 0x77
#define KEY_ALTGR 0x78
#define KEY_LWIN 0x79
#define KEY_RWIN 0x7a
#define KEY_MENU 0x7b
#define KEY_SCRLOCK 0x7c
#define KEY_NUMLOCK 0x7d
#define KEY_CAPSLOCK 0x7e
#define KEY_MODIFIERS 0x73
#define GFX_AUTODETECT 0x4155544f
#define GFX_AUTODETECT_FULLSCREEN 0x41555446
#define GFX_AUTODETECT_WINDOWED 0x41555457
#define GFX_SAFE 0x53414645
#define GFX_TEXT 0
#define DRAW_MODE_SOLID 0
#define DRAW_MODE_XOR 1
#define DRAW_MODE_COPY_PATTERN 2
#define DRAW_MODE_SOLID_PATTERN 3
#define DRAW_MODE_MASKED_PATTERN 4
#define DRAW_MODE_TRANS 5
#define MASK_COLOR_8 0xff
#define MASK_COLOR_15 0x7c1f
#define MASK_COLOR_16 0xf81f
#define MASK_COLOR_24 0xff00ff
#define MASK_COLOR_32 0xff00ff

#endif /* PF_LIB_BINDINGS_H */
