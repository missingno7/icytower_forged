# Library compatibility table (generated)

Companion to `notes/library_compat_verdict.md`. One row per required symbol
in the library boundary (`artifacts/lib_boundary.json`): 100 Allegro-family
functions, 26 Allegro globals (incl. the internal edge `_win_hcursor`), 24
callback registrations. Generated 2026-09-07.

| # | symbol | kind | classification | candidate | evidence | adapter |
|---:|---|---|---|---|---|---|
| 1 | `__WinMain` | function | PUBLIC + AVAILABLE | Allegro 4.4.3.1 source build (src/win/wsystem.c) | platform/alwin.h:32 | none |
| 2 | `__color_load_depth` | function | INTERNAL NOT EXPORTED | Allegro 4.4.3.1 source build (internal call within the same static link unit as vendored loadpng.c) | internal/aintern.h:390 | none (recommended static/source-build path); DLL-export shim only if Allegro were split into a separate DLL (rejected option) |
| 3 | `__fixup_loaded_bitmap` | function | INTERNAL NOT EXPORTED | Allegro 4.4.3.1 source build (internal call within the same static link unit as vendored loadpng.c) | internal/aintern.h:394 | none (recommended static/source-build path); DLL-export shim only if Allegro were split into a separate DLL (rejected option) |
| 4 | `__install_allegro_version_check` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | system.h:80 | none |
| 5 | `_adjust_sample` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:153 | none |
| 6 | `_alert` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gui.h:217 | none |
| 7 | `_allegro_exit` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | system.h:94 | none |
| 8 | `_allegro_message` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | system.h:96 | none |
| 9 | `_blit` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | draw.h:55 | none |
| 10 | `_canonicalize_filename` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:30 | none |
| 11 | `_clear_bitmap` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:427 | none |
| 12 | `_clear_keybuf` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | keyboard.h:73 | none |
| 13 | `_create_bitmap` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:419 | none |
| 14 | `_create_bitmap_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:420 | none |
| 15 | `_create_sub_bitmap` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:421 | none |
| 16 | `_delete_file` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:44 | none |
| 17 | `_destroy_bitmap` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:424 | none |
| 18 | `_destroy_midi` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | midi.h:124 | none |
| 19 | `_destroy_sample` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:149 | none |
| 20 | `_drawing_mode` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | draw.h:37 | none |
| 21 | `_enable_hardware_cursor` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:62 | none |
| 22 | `_exists` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:41 | none |
| 23 | `_file_exists` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:40 | none |
| 24 | `_file_select_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gui.h:219 | none |
| 25 | `_file_size_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:42 | none |
| 26 | `_for_each_file_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:45 | none |
| 27 | `_generate_332_palette` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:92 | none |
| 28 | `_get_color_conversion` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:409 | none |
| 29 | `_get_config_string` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | config.h:41 | none |
| 30 | `_get_executable_name` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | system.h:97 | none |
| 31 | `_get_extension` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:38 | none |
| 32 | `_get_filename` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:37 | none |
| 33 | `_get_palette` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:78 | none |
| 34 | `_install_int` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | timer.h:59 | none |
| 35 | `_install_joystick` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | joystick.h:128 | none |
| 36 | `_install_keyboard` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | keyboard.h:50 | none |
| 37 | `_install_mouse` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:56 | none |
| 38 | `_install_sound` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | sound.h:34 | none |
| 39 | `_install_timer` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | timer.h:55 | none |
| 40 | `_keypressed` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | keyboard.h:68 | none |
| 41 | `_load_bitmap` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:91 | none |
| 42 | `_load_datafile` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:75 | none |
| 43 | `_load_datafile_callback` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:76 | none |
| 44 | `_load_midi` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | midi.h:122 | none |
| 45 | `_load_sample` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:142 | none |
| 46 | `_load_wav` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:143 | none |
| 47 | `_logg_load` | function | PUBLIC + AVAILABLE | logg addon source (Allegro 4.4.3.1 addons/logg) + MSYS2 mingw-w64-i686 libogg/libvorbis | addons/logg/logg.h:35 | none |
| 48 | `_logg_load_memory` | function | UNKNOWN | NONE FOUND -- not in any candidate source | (no header declares this name; see note) | shim: reimplement ~7 functions as a memory-backed ov_open_callbacks() reader against libvorbisfile's public API (upstream has always supported in-memory callbacks; only Icy Tower's own glue is missing) |
| 49 | `_makecol` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:127 | none |
| 50 | `_makecol_depth` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:129 | none |
| 51 | `_masked_blit` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | draw.h:56 | none |
| 52 | `_pack_fclose` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:149 | none |
| 53 | `_pack_fopen` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:147 | none |
| 54 | `_pack_fread` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:165 | none |
| 55 | `_pack_fwrite` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:166 | none |
| 56 | `_packfile_password` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:146 | none |
| 57 | `_play_midi` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | midi.h:125 | none |
| 58 | `_play_sample` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:151 | none |
| 59 | `_poll_joystick` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | joystick.h:131 | none |
| 60 | `_readkey` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | keyboard.h:69 | none |
| 61 | `_register_bitmap_file_type` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:108 | none |
| 62 | `_register_datafile_object` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:87 | none |
| 63 | `_remove_mouse` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:57 | none |
| 64 | `_replace_extension` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:35 | none |
| 65 | `_replace_filename` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:34 | none |
| 66 | `_rest` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | timer.h:68 | none |
| 67 | `_save_bitmap` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:100 | none |
| 68 | `_select_mouse_cursor` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:108 | none |
| 69 | `_select_palette` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:89 | none |
| 70 | `_set_alpha_blender` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:106 | none |
| 71 | `_set_clip_rect` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:425 | none |
| 72 | `_set_close_button_callback` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | system.h:98 | none |
| 73 | `_set_color_conversion` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:408 | none |
| 74 | `_set_color_depth` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:406 | none |
| 75 | `_set_config_file` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | config.h:28 | none |
| 76 | `_set_display_switch_callback` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:453 | none |
| 77 | `_set_display_switch_mode` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:451 | none |
| 78 | `_set_gfx_mode` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:412 | none |
| 79 | `_set_palette` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:74 | none |
| 80 | `_set_trans_blender` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:108 | none |
| 81 | `_set_volume` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | sound.h:40 | none |
| 82 | `_show_mouse` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:99 | none |
| 83 | `_simulate_keypress` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | keyboard.h:71 | none |
| 84 | `_solid_mode` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | draw.h:39 | none |
| 85 | `_stop_midi` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | midi.h:127 | none |
| 86 | `_stop_sample` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:152 | none |
| 87 | `_stretch_blit` | function | PUBLIC + AVAILABLE | Allegro 4.4.3.1 source build (src/c/cstretch.c, C-only blitters) | draw.h:57 | none |
| 88 | `_stretch_sprite` | function | PUBLIC + AVAILABLE | Allegro 4.4.3.1 source build (src/c/cstretch.c, C-only blitters) | draw.h:59 | none |
| 89 | `_text_height` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:45 | none |
| 90 | `_text_length` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:44 | none |
| 91 | `_textout_centre_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:37 | none |
| 92 | `_textout_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:36 | none |
| 93 | `_textout_right_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:38 | none |
| 94 | `_textprintf_centre_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:41 | none |
| 95 | `_textprintf_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:40 | none |
| 96 | `_textprintf_right_ex` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:42 | none |
| 97 | `_unload_datafile` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:78 | none |
| 98 | `_voice_get_position` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:172 | none |
| 99 | `_voice_stop` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | digi.h:160 | none |
| 100 | `_vsync` | function | PUBLIC + AVAILABLE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:428 | none |
| 101 | `_key` | global (101 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | keyboard.h:62 (AL_ARRAY) | none |
| 102 | `_screen` | global (67 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:302 (AL_VAR) | none |
| 103 | `_font` | global (39 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | text.h:34 (AL_VAR) | none |
| 104 | `_gfx_driver` | global (27 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:119 (AL_VAR) | none |
| 105 | `_joy` | global (6 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | joystick.h:92 (AL_ARRAY) | none |
| 106 | `_mouse_b` | global (4 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:82 (AL_VAR) | none |
| 107 | `_gui_fg_color` | global (3 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gui.h:185 (AL_VAR) | none |
| 108 | `_gui_bg_color` | global (3 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gui.h:187 (AL_VAR) | none |
| 109 | `_allegro_errno` | global (3 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | base.h:94 (AL_VAR) | none |
| 110 | `_system_driver` | global (2 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | system.h:244 (AL_VAR) | none |
| 111 | `__win_hcursor` | global (2 sites) | INTERNAL NOT EXPORTED | Allegro 4.4.3.1 source build (platform/aintwin.h, internal) | platform/aintwin.h:129 (AL_VAR) | one-liner: #include <allegro/platform/aintwin.h>, or a 3-line extern HCURSOR shim |
| 112 | `_mouse_x` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:78 (AL_VAR) | none |
| 113 | `_mouse_y` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | mouse.h:79 (AL_VAR) | none |
| 114 | `__rgb_r_shift_32` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:57 | none |
| 115 | `__rgb_g_shift_32` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:58 | none |
| 116 | `__rgb_b_shift_32` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:59 | none |
| 117 | `__rgb_a_shift_32` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:60 | none |
| 118 | `__rgb_r_shift_24` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:54 | none |
| 119 | `__rgb_g_shift_24` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:55 | none |
| 120 | `__rgb_b_shift_24` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:56 | none |
| 121 | `__rgb_r_shift_15` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:48 | none |
| 122 | `__rgb_g_shift_15` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:49 | none |
| 123 | `__rgb_b_shift_15` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:50 | none |
| 124 | `__rgb_r_shift_16` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:51 | none |
| 125 | `__rgb_g_shift_16` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:52 | none |
| 126 | `__rgb_b_shift_16` | global (1 sites) | GLOBAL SHARED STATE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | color.h:53 | none |
| 127 | `_set_display_switch_callback` | callback (6x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | gfx.h:453, AL_METHOD(void,cb,(void)) unchanged | none |
| 128 | `_png_set_read_fn` | callback (2x) | CALLBACK EDGE | libpng3.dll 1.2.34 (already shipped) | png_rw_ptr callback, unchanged in libpng 1.2.x (candidates.md sec 4) | none |
| 129 | `_for_each_file_ex` | callback (2x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | file.h:45, AL_METHOD(int,callback,(...)) unchanged | none |
| 130 | `_load_datafile_callback` | callback (2x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:76, AL_METHOD(void,callback,(DATAFILE*)) unchanged | none |
| 131 | `_register_datafile_object` | callback (2x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:87, AL_METHOD load/destroy unchanged | none |
| 132 | `_register_bitmap_file_type` | callback (2x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | datafile.h:108, AL_METHOD load/save unchanged | none |
| 133 | `_png_set_write_fn` | callback (2x) | CALLBACK EDGE | libpng3.dll 1.2.34 (already shipped) | png_rw_ptr callback, unchanged in libpng 1.2.x (candidates.md sec 4) | none |
| 134 | `_install_int` | callback (2x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | timer.h:59, AL_METHOD(void,proc,(void)) unchanged | none |
| 135 | `*0x00514a58` | callback (1x) | CALLBACK EDGE | pthreads-win32 (pthreadGC2.dll, already shipped) | standard POSIX pthread_create signature, ABI-stable | none |
| 136 | `__WinMain` | callback (1x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | platform/alwin.h:32, AL_FUNC(int,_WinMain,(void*,void*,void*,char*,int)) | none |
| 137 | `_set_close_button_callback` | callback (1x) | CALLBACK EDGE | AGS lib-allegro / upstream Allegro 4.4.3.1 source build | system.h:98, AL_METHOD(void,proc,(void)) unchanged | none |
| 138 | `_qsort` | callback (1x) | CALLBACK EDGE | msvcrt.dll (system CRT, already present) | standard C qsort comparator signature | none |

## Notes column (per-row detail too long for the table)

- **`__WinMain`**: public by END_OF_MAIN() macro (alwin.h)
- **`__color_load_depth`**: internal header (include/allegro/internal/), not reachable via <allegro.h>; upstream loadpng.c calls it too (same-CU/same-link-unit call)
- **`__fixup_loaded_bitmap`**: internal header (include/allegro/internal/), not reachable via <allegro.h>; upstream loadpng.c calls it too (same-CU/same-link-unit call)
- **`__install_allegro_version_check`**: public by allegro_init() macro (system.h)
- **`_logg_load_memory`**: NOT declared in Allegro 4.4.1, 4.4.3.1, or AGS lib-allegro v4.4.3.1-agspatch-3 logg.h/logg.c (all three logg.c are byte-identical, 4730 B, 11 functions). Game's logg CU has 18 functions incl. 7 (logg_load_internal, logg_load_memory, logg_vf_memfile_seek/tell/read/close, _ov_header_fseek_wrap = 667 of 2041 B) with no upstream match -- a custom memory-stream extension the Icy Tower developers added to their private logg.c copy.
- **`_key`**: 101 access site(s), reader(s): blit_to_screen, draw_frame, get_string, handle_menu, handle_player_collision_combo, handle_player_collision_vector, handle_player_collision_vector_2, main_menu_callback, my_alert, play, show_credits, show_instructions, take_screenshot, update_game_menu, view_profile, view_scores
- **`_screen`**: 67 access site(s), reader(s): blit_to_screen, draw_progress_bar, fadeOut, force_create_profile, handle_player_collision_combo, handle_player_collision_vector, handle_player_collision_vector_2, init_game, line_alert, load_frames, my_alert, play, replay_selector, select_profile, testWindowResolution, view_profile, view_scores
- **`_font`**: 39 access site(s), reader(s): draw_frame, draw_profile_selector, draw_progress_bar, draw_replay_selector, init_game, load_sound, replay_selector, select_profile
- **`_gfx_driver`**: 27 access site(s), reader(s): fadeIn, fadeOut, force_create_profile, line_alert, main_menu_callback, my_alert, play, replay_selector, select_profile, view_profile, view_scores
- **`_joy`**: 6 access site(s), reader(s): init_game, poll_control
- **`_mouse_b`**: 4 access site(s), reader(s): main_menu_callback
- **`_gui_fg_color`**: 3 access site(s), reader(s): custom_alert, my_alert, replay_selector
- **`_gui_bg_color`**: 3 access site(s), reader(s): custom_alert, my_alert, replay_selector
- **`_allegro_errno`**: 3 access site(s), reader(s): draw_frame, load_character
- **`_system_driver`**: 2 access site(s), reader(s): init_game
- **`__win_hcursor`**: 2 access site(s), reader(s): main_menu_callback -- the one genuine internal-symbol edge named in the task
- **`_mouse_x`**: 1 access site(s), reader(s): main_menu_callback
- **`_mouse_y`**: 1 access site(s), reader(s): main_menu_callback
- **`__rgb_r_shift_32`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_g_shift_32`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_b_shift_32`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_a_shift_32`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_r_shift_24`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_g_shift_24`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_b_shift_24`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_r_shift_15`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_g_shift_15`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_b_shift_15`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_r_shift_16`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_g_shift_16`**: 1 access site(s), reader(s): really_save_png
- **`__rgb_b_shift_16`**: 1 access site(s), reader(s): really_save_png
- **`_set_display_switch_callback`**: 6 registration site(s); game functions registered: switchedFromProgram, switchedToProgram
- **`_png_set_read_fn`**: 2 registration site(s); game functions registered: read_data, read_data_memory
- **`_for_each_file_ex`**: 2 registration site(s); game functions registered: add_itr_file, add_profile
- **`_load_datafile_callback`**: 2 registration site(s); game functions registered: datafile_callback, datafile_callback_slow
- **`_register_datafile_object`**: 2 registration site(s); game functions registered: destroy_datafile_png, load_datafile_png
- **`_register_bitmap_file_type`**: 2 registration site(s); game functions registered: load_png, save_png
- **`_png_set_write_fn`**: 2 registration site(s); game functions registered: flush_data, write_data
- **`_install_int`**: 2 registration site(s); game functions registered: cycle_counter, fps_counter
- **`*0x00514a58`**: 1 registration site(s); game functions registered: fldads_threadmain
- **`__WinMain`**: 1 registration site(s); game functions registered: _mangled_main
- **`_set_close_button_callback`**: 1 registration site(s); game functions registered: clickedCloseButton
- **`_qsort`**: 1 registration site(s); game functions registered: my_strcmp
