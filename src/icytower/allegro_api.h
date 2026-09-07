/* allegro_api.h -- GENERATED FILE. DO NOT EDIT.
 * Produced by tools/pf_win32_gen_lib_bindings.py from:
 *   artifacts/lib_boundary.json
 *   artifacts/dwarf_info.txt, artifacts/functions.json (scope=all DWARF model)
 * Generated: 2026-09-07 21:05:01 UTC
 *
 * This port's STAND-IN for <allegro.h>, address-free, upstream spelling
 * only (win32_pilot.md SS7b): declares exactly the 100 functions and 26
 * globals src/ is allowed to call/read (the same lib_boundary.json
 * allow-list carrier/gen/pf_lib_bindings.h binds to addresses), plus the
 * Allegro constants src/ needs (see this generator's module docstring
 * "Constants" for the evidence label on each -- MEASURED, INFERRED-FROM-
 * PROJECT-EVIDENCE or INFERRED-FROM-UPSTREAM-DOCS; none is guest-address
 * derived, macros leave no DWARF trace).
 *
 * Once src/icytower takes a real Allegro dependency (win32_pilot.md SS4
 * option (A): source-built Allegro 4.4.1, static, no-asm, no-DEBUGMODE),
 * every #include "allegro_api.h" in src/ is simply replaced by
 * #include <allegro.h> (+ <loadpng.h>/<logg.h> where used) and this file
 * is deleted; nothing in src/ changes name, type or spelling to make that
 * swap possible, because both worlds already use the same upstream names
 * (carrier/gen/LIB_BINDINGS_NOTES.md "Standalone build swap").
 *
 * Skipped entirely when ICYTOWER_BINDINGS_ACTIVE is defined: pf_lib_bindings.h (force-
 * included ahead of any token in that world) already supplied every name
 * below as an address-backed macro; redeclaring them here would try to
 * parse text with those names already macro-expanded (see
 * carrier/gen/BINDINGS_NOTES.md "selftest_state.h" for the same failure
 * mode already solved once for game-scope names).
 *
 * Skipped, in favour of the real upstream headers, when ICYTOWER_UPSTREAM_ALLEGRO is
 * defined ("Standalone build swap" below): allegro_types.h has already
 * #included <allegro.h> under the same macro, which is where all 100
 * functions and 26 globals below actually come from upstream; this file
 * then only needs <logg.h>, for the allow-list's 2 non-core-Allegro
 * entries (`logg_load`/`logg_load_memory`) that <allegro.h> itself does
 * not declare.
 */

#ifndef ICYTOWER_ALLEGRO_API_H
#define ICYTOWER_ALLEGRO_API_H

#include "allegro_types.h"  /* BITMAP, FONT, RGB, SAMPLE, DATAFILE, PACKFILE, */
                            /* MIDI, PALETTE, fixed -- already generated,     */
                            /* already self-skipping under ICYTOWER_BINDINGS_ACTIVE and    */
                            /* ICYTOWER_UPSTREAM_ALLEGRO (real <allegro.h> in the latter) */

#if defined(ICYTOWER_UPSTREAM_ALLEGRO)

#include <logg.h>  /* logg_load, logg_load_memory -- the allow-list's only */
                   /* two entries <allegro.h> itself does not declare      */

#elif !defined(ICYTOWER_BINDINGS_ACTIVE)

#pragma pack(push, 1)

/* forward declarations (types new to the library-scope surface) */
struct GFX_DRIVER;
struct GFX_MODE;
struct GFX_MODE_LIST;
struct JOYSTICK_AXIS_INFO;
struct JOYSTICK_BUTTON_INFO;
struct JOYSTICK_INFO;
struct JOYSTICK_STICK_INFO;
struct SYSTEM_DRIVER;
struct _DRIVER_INFO;

/* Win32 scalar typedefs the structs below reference. Plain, not from
 * <windows.h> -- this header must never be included alongside
 * <windows.h> in the same translation unit (it_types.h's BITMAP/
 * pthread_mutex_t_ already collide with wingdi.h/pthread the same way,
 * see carrier/src/bind.cpp's header comment; the same rule applies
 * here, for the same reason).
 */

/* struct bodies + remaining typedefs, dependency order */
struct _DRIVER_INFO {
    int id;
    void *driver;
    int autodetect;
};
struct GFX_MODE {
    int width;
    int height;
    int bpp;
};
typedef struct GFX_MODE GFX_MODE;
struct GFX_MODE_LIST {
    int num_modes;
    GFX_MODE *mode;
};
typedef struct GFX_MODE_LIST GFX_MODE_LIST;
struct GFX_DRIVER {
    int id;
    const char *name;
    const char *desc;
    const char *ascii_name;
    struct BITMAP * (__cdecl *init)(int, int, int, int, int);
    void (__cdecl *exit)(struct BITMAP *);
    int (__cdecl *scroll)(int, int);
    void (__cdecl *vsync)(void);
    void (__cdecl *set_palette)(const struct RGB *, int, int, int);
    int (__cdecl *request_scroll)(int, int);
    int (__cdecl *poll_scroll)(void);
    void (__cdecl *enable_triple_buffer)(void);
    struct BITMAP * (__cdecl *create_video_bitmap)(int, int);
    void (__cdecl *destroy_video_bitmap)(struct BITMAP *);
    int (__cdecl *show_video_bitmap)(struct BITMAP *);
    int (__cdecl *request_video_bitmap)(struct BITMAP *);
    struct BITMAP * (__cdecl *create_system_bitmap)(int, int);
    void (__cdecl *destroy_system_bitmap)(struct BITMAP *);
    int (__cdecl *set_mouse_sprite)(struct BITMAP *, int, int);
    int (__cdecl *show_mouse)(struct BITMAP *, int, int);
    void (__cdecl *hide_mouse)(void);
    void (__cdecl *move_mouse)(int, int);
    void (__cdecl *drawing_mode)(void);
    void (__cdecl *save_video_state)(void);
    void (__cdecl *restore_video_state)(void);
    void (__cdecl *set_blender_mode)(int, int, int, int, int);
    GFX_MODE_LIST * (__cdecl *fetch_mode_list)(void);
    int w;
    int h;
    int linear;
    long bank_size;
    long bank_gran;
    long vid_mem;
    long vid_phys_base;
    int windowed;
};
typedef struct _DRIVER_INFO _DRIVER_INFO;
struct SYSTEM_DRIVER {
    int id;
    const char *name;
    const char *desc;
    const char *ascii_name;
    int (__cdecl *init)(void);
    void (__cdecl *exit)(void);
    void (__cdecl *get_executable_name)(char *, int);
    int (__cdecl *find_resource)(char *, const char *, int);
    void (__cdecl *set_window_title)(const char *);
    int (__cdecl *set_close_button_callback)(void (__cdecl *)(void));
    void (__cdecl *message)(const char *);
    void (__cdecl *assert)(const char *);
    void (__cdecl *save_console_state)(void);
    void (__cdecl *restore_console_state)(void);
    struct BITMAP * (__cdecl *create_bitmap)(int, int, int);
    void (__cdecl *created_bitmap)(struct BITMAP *);
    struct BITMAP * (__cdecl *create_sub_bitmap)(struct BITMAP *, int, int, int, int);
    void (__cdecl *created_sub_bitmap)(struct BITMAP *, struct BITMAP *);
    int (__cdecl *destroy_bitmap)(struct BITMAP *);
    void (__cdecl *read_hardware_palette)(void);
    void (__cdecl *set_palette_range)(const struct RGB *, int, int, int);
    struct GFX_VTABLE * (__cdecl *get_vtable)(int);
    int (__cdecl *set_display_switch_mode)(int);
    void (__cdecl *display_switch_lock)(int, int);
    int (__cdecl *desktop_color_depth)(void);
    int (__cdecl *get_desktop_resolution)(int *, int *);
    void (__cdecl *get_gfx_safe_mode)(int *, struct GFX_MODE *);
    void (__cdecl *yield_timeslice)(void);
    void * (__cdecl *create_mutex)(void);
    void (__cdecl *destroy_mutex)(void *);
    void (__cdecl *lock_mutex)(void *);
    void (__cdecl *unlock_mutex)(void *);
    _DRIVER_INFO * (__cdecl *gfx_drivers)(void);
    _DRIVER_INFO * (__cdecl *digi_drivers)(void);
    _DRIVER_INFO * (__cdecl *midi_drivers)(void);
    _DRIVER_INFO * (__cdecl *keyboard_drivers)(void);
    _DRIVER_INFO * (__cdecl *mouse_drivers)(void);
    _DRIVER_INFO * (__cdecl *joystick_drivers)(void);
    _DRIVER_INFO * (__cdecl *timer_drivers)(void);
};
struct JOYSTICK_AXIS_INFO {
    int pos;
    int d1;
    int d2;
    const char *name;
};
typedef struct JOYSTICK_AXIS_INFO JOYSTICK_AXIS_INFO;
struct JOYSTICK_STICK_INFO {
    int flags;
    int num_axis;
    JOYSTICK_AXIS_INFO axis[3];
    const char *name;
};
struct JOYSTICK_BUTTON_INFO {
    int b;
    const char *name;
};
typedef struct JOYSTICK_STICK_INFO JOYSTICK_STICK_INFO;
typedef struct JOYSTICK_BUTTON_INFO JOYSTICK_BUTTON_INFO;
struct JOYSTICK_INFO {
    int flags;
    int num_sticks;
    int num_buttons;
    JOYSTICK_STICK_INFO stick[5];
    JOYSTICK_BUTTON_INFO button[32];
};
typedef struct GFX_DRIVER GFX_DRIVER;
typedef struct SYSTEM_DRIVER SYSTEM_DRIVER;
typedef struct JOYSTICK_INFO JOYSTICK_INFO;
typedef void *PVOID;
typedef PVOID HANDLE;
typedef HANDLE HICON;
typedef HICON HCURSOR;
typedef unsigned __int64 uint64_t;

#pragma pack(pop)

/* ---- constants (see this generator's "Constants" note for evidence) ---- */

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

/* ---- globals (26), extern, no address (win32_pilot.md SS7a style) ---- */

extern int _rgb_a_shift_32;  /* raw=__rgb_a_shift_32 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_b_shift_15;  /* raw=__rgb_b_shift_15 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_b_shift_16;  /* raw=__rgb_b_shift_16 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_b_shift_24;  /* raw=__rgb_b_shift_24 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_b_shift_32;  /* raw=__rgb_b_shift_32 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_g_shift_15;  /* raw=__rgb_g_shift_15 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_g_shift_16;  /* raw=__rgb_g_shift_16 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_g_shift_24;  /* raw=__rgb_g_shift_24 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_g_shift_32;  /* raw=__rgb_g_shift_32 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_r_shift_15;  /* raw=__rgb_r_shift_15 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_r_shift_16;  /* raw=__rgb_r_shift_16 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_r_shift_24;  /* raw=__rgb_r_shift_24 cu=C:\Lib\allegro4\src\graphics.c */
extern int _rgb_r_shift_32;  /* raw=__rgb_r_shift_32 cu=C:\Lib\allegro4\src\graphics.c */
extern HCURSOR _win_hcursor;  /* raw=__win_hcursor cu=C:\Lib\allegro4\src\win\wmouse.c */
extern int *allegro_errno;  /* raw=_allegro_errno cu=C:\Lib\allegro4\src\allegro.c */
extern FONT *font;  /* raw=_font cu=C:\Lib\allegro4\src\font.c */
extern GFX_DRIVER *gfx_driver;  /* raw=_gfx_driver cu=C:\Lib\allegro4\src\allegro.c */
extern int gui_bg_color;  /* raw=_gui_bg_color cu=C:\Lib\allegro4\src\allegro.c */
extern int gui_fg_color;  /* raw=_gui_fg_color cu=C:\Lib\allegro4\src\allegro.c */
extern JOYSTICK_INFO joy[8];  /* raw=_joy cu=C:\Lib\allegro4\src\joystick.c */
extern volatile char key[127];  /* raw=_key cu=C:\Lib\allegro4\src\keyboard.c */
extern volatile int mouse_b;  /* raw=_mouse_b cu=C:\Lib\allegro4\src\mouse.c */
extern volatile int mouse_x;  /* raw=_mouse_x cu=C:\Lib\allegro4\src\mouse.c */
extern volatile int mouse_y;  /* raw=_mouse_y cu=C:\Lib\allegro4\src\mouse.c */
extern BITMAP *screen;  /* raw=_screen cu=C:\Lib\allegro4\src\allegro.c */
extern SYSTEM_DRIVER *system_driver;  /* raw=_system_driver cu=C:\Lib\allegro4\src\allegro.c */

/* ---- functions (100), plain prototypes, no address ---- */

int _WinMain(void *, void *, void *, char *, int);  /* raw=__WinMain cu=C:\Lib\allegro4\src\win\wsystem.c */
int _color_load_depth(int, int);  /* raw=__color_load_depth cu=C:\Lib\allegro4\src\graphics.c */
BITMAP * _fixup_loaded_bitmap(BITMAP *, RGB *, int);  /* raw=__fixup_loaded_bitmap cu=C:\Lib\allegro4\src\readbmp.c */
int _install_allegro_version_check(int, int *, int (__cdecl *)(void (__cdecl *)(void)), int);  /* raw=__install_allegro_version_check cu=C:\Lib\allegro4\src\allegro.c */
void adjust_sample(const SAMPLE *, int, int, int, int);  /* raw=_adjust_sample cu=C:\Lib\allegro4\src\sound.c */
int alert(const char *, const char *, const char *, const char *, const char *, int, int);  /* raw=_alert cu=C:\Lib\allegro4\src\gui.c */
void allegro_exit(void);  /* raw=_allegro_exit cu=C:\Lib\allegro4\src\allegro.c */
void allegro_message(const char *, ...);  /* raw=_allegro_message cu=C:\Lib\allegro4\src\allegro.c */
void blit(BITMAP *, BITMAP *, int, int, int, int, int, int);  /* raw=_blit cu=C:\Lib\allegro4\src\blit.c */
char * canonicalize_filename(char *, const char *, int);  /* raw=_canonicalize_filename cu=C:\Lib\allegro4\src\file.c */
void clear_bitmap(BITMAP *);  /* raw=_clear_bitmap cu=C:\Lib\allegro4\src\gfx.c */
void clear_keybuf(void);  /* raw=_clear_keybuf cu=C:\Lib\allegro4\src\keyboard.c */
BITMAP * create_bitmap(int, int);  /* raw=_create_bitmap cu=C:\Lib\allegro4\src\graphics.c */
BITMAP * create_bitmap_ex(int, int, int);  /* raw=_create_bitmap_ex cu=C:\Lib\allegro4\src\graphics.c */
BITMAP * create_sub_bitmap(BITMAP *, int, int, int, int);  /* raw=_create_sub_bitmap cu=C:\Lib\allegro4\src\graphics.c */
int delete_file(const char *);  /* raw=_delete_file cu=C:\Lib\allegro4\src\file.c */
void destroy_bitmap(BITMAP *);  /* raw=_destroy_bitmap cu=C:\Lib\allegro4\src\graphics.c */
void destroy_midi(MIDI *);  /* raw=_destroy_midi cu=C:\Lib\allegro4\src\midi.c */
void destroy_sample(SAMPLE *);  /* raw=_destroy_sample cu=C:\Lib\allegro4\src\sound.c */
void drawing_mode(int, BITMAP *, int, int);  /* raw=_drawing_mode cu=C:\Lib\allegro4\src\gfx.c */
void enable_hardware_cursor(void);  /* raw=_enable_hardware_cursor cu=C:\Lib\allegro4\src\mouse.c */
int exists(const char *);  /* raw=_exists cu=C:\Lib\allegro4\src\file.c */
int file_exists(const char *, int, int *);  /* raw=_file_exists cu=C:\Lib\allegro4\src\file.c */
int file_select_ex(const char *, char *, const char *, int, int, int);  /* raw=_file_select_ex cu=C:\Lib\allegro4\src\fsel.c */
uint64_t file_size_ex(const char *);  /* raw=_file_size_ex cu=C:\Lib\allegro4\src\file.c */
int for_each_file_ex(const char *, int, int, int (__cdecl *)(const char *, int, void *), void *);  /* raw=_for_each_file_ex cu=C:\Lib\allegro4\src\file.c */
void generate_332_palette(RGB *);  /* raw=_generate_332_palette cu=C:\Lib\allegro4\src\gfx.c */
int get_color_conversion(void);  /* raw=_get_color_conversion cu=C:\Lib\allegro4\src\graphics.c */
const char * get_config_string(const char *, const char *, const char *);  /* raw=_get_config_string cu=C:\Lib\allegro4\src\config.c */
void get_executable_name(char *, int);  /* raw=_get_executable_name cu=C:\Lib\allegro4\src\allegro.c */
char * get_extension(const char *);  /* raw=_get_extension cu=C:\Lib\allegro4\src\file.c */
char * get_filename(const char *);  /* raw=_get_filename cu=C:\Lib\allegro4\src\file.c */
void get_palette(RGB *);  /* raw=_get_palette cu=C:\Lib\allegro4\src\gfx.c */
int install_int(void (__cdecl *)(void), long);  /* raw=_install_int cu=C:\Lib\allegro4\src\timer.c */
int install_joystick(int);  /* raw=_install_joystick cu=C:\Lib\allegro4\src\joystick.c */
int install_keyboard(void);  /* raw=_install_keyboard cu=C:\Lib\allegro4\src\keyboard.c */
int install_mouse(void);  /* raw=_install_mouse cu=C:\Lib\allegro4\src\mouse.c */
int install_sound(int, int, const char *);  /* raw=_install_sound cu=C:\Lib\allegro4\src\sound.c */
int install_timer(void);  /* raw=_install_timer cu=C:\Lib\allegro4\src\timer.c */
int keypressed(void);  /* raw=_keypressed cu=C:\Lib\allegro4\src\keyboard.c */
BITMAP * load_bitmap(const char *, RGB *);  /* raw=_load_bitmap cu=C:\Lib\allegro4\src\readbmp.c */
DATAFILE * load_datafile(const char *);  /* raw=_load_datafile cu=C:\Lib\allegro4\src\datafile.c */
DATAFILE * load_datafile_callback(const char *, void (__cdecl *)(DATAFILE *));  /* raw=_load_datafile_callback cu=C:\Lib\allegro4\src\datafile.c */
MIDI * load_midi(const char *);  /* raw=_load_midi cu=C:\Lib\allegro4\src\midi.c */
SAMPLE * load_sample(const char *);  /* raw=_load_sample cu=C:\Lib\allegro4\src\readsmp.c */
SAMPLE * load_wav(const char *);  /* raw=_load_wav cu=C:\Lib\allegro4\src\sound.c */
SAMPLE * logg_load(const char *);  /* raw=_logg_load cu=C:\Lib\allegro4\addons\logg\logg.c */
SAMPLE * logg_load_memory(void *, it_orig_size_t);  /* raw=_logg_load_memory cu=C:\Lib\allegro4\addons\logg\logg.c */
int makecol(int, int, int);  /* raw=_makecol cu=C:\Lib\allegro4\src\color.c */
int makecol_depth(int, int, int, int);  /* raw=_makecol_depth cu=C:\Lib\allegro4\src\color.c */
void masked_blit(BITMAP *, BITMAP *, int, int, int, int, int, int);  /* raw=_masked_blit cu=C:\Lib\allegro4\src\blit.c */
int pack_fclose(PACKFILE *);  /* raw=_pack_fclose cu=C:\Lib\allegro4\src\file.c */
PACKFILE * pack_fopen(const char *, const char *);  /* raw=_pack_fopen cu=C:\Lib\allegro4\src\file.c */
long pack_fread(void *, long, PACKFILE *);  /* raw=_pack_fread cu=C:\Lib\allegro4\src\file.c */
long pack_fwrite(const void *, long, PACKFILE *);  /* raw=_pack_fwrite cu=C:\Lib\allegro4\src\file.c */
void packfile_password(const char *);  /* raw=_packfile_password cu=C:\Lib\allegro4\src\file.c */
int play_midi(MIDI *, int);  /* raw=_play_midi cu=C:\Lib\allegro4\src\midi.c */
int play_sample(const SAMPLE *, int, int, int, int);  /* raw=_play_sample cu=C:\Lib\allegro4\src\sound.c */
int poll_joystick(void);  /* raw=_poll_joystick cu=C:\Lib\allegro4\src\joystick.c */
int readkey(void);  /* raw=_readkey cu=C:\Lib\allegro4\src\keyboard.c */
void register_bitmap_file_type(const char *, BITMAP * (__cdecl *)(const char *, RGB *), int (__cdecl *)(const char *, BITMAP *, const RGB *));  /* raw=_register_bitmap_file_type cu=C:\Lib\allegro4\src\readbmp.c */
void register_datafile_object(int, void * (__cdecl *)(PACKFILE *, long), void (__cdecl *)(void *));  /* raw=_register_datafile_object cu=C:\Lib\allegro4\src\dataregi.c */
void remove_mouse(void);  /* raw=_remove_mouse cu=C:\Lib\allegro4\src\mouse.c */
char * replace_extension(char *, const char *, const char *, int);  /* raw=_replace_extension cu=C:\Lib\allegro4\src\file.c */
char * replace_filename(char *, const char *, const char *, int);  /* raw=_replace_filename cu=C:\Lib\allegro4\src\file.c */
void rest(unsigned int);  /* raw=_rest cu=C:\Lib\allegro4\src\timer.c */
int save_bitmap(const char *, BITMAP *, const RGB *);  /* raw=_save_bitmap cu=C:\Lib\allegro4\src\readbmp.c */
void select_mouse_cursor(int);  /* raw=_select_mouse_cursor cu=C:\Lib\allegro4\src\mouse.c */
void select_palette(const RGB *);  /* raw=_select_palette cu=C:\Lib\allegro4\src\gfx.c */
void set_alpha_blender(void);  /* raw=_set_alpha_blender cu=C:\Lib\allegro4\src\colblend.c */
void set_clip_rect(BITMAP *, int, int, int, int);  /* raw=_set_clip_rect cu=C:\Lib\allegro4\src\graphics.c */
int set_close_button_callback(void (__cdecl *)(void));  /* raw=_set_close_button_callback cu=C:\Lib\allegro4\src\allegro.c */
void set_color_conversion(int);  /* raw=_set_color_conversion cu=C:\Lib\allegro4\src\graphics.c */
void set_color_depth(int);  /* raw=_set_color_depth cu=C:\Lib\allegro4\src\graphics.c */
void set_config_file(const char *);  /* raw=_set_config_file cu=C:\Lib\allegro4\src\config.c */
int set_display_switch_callback(int, void (__cdecl *)(void));  /* raw=_set_display_switch_callback cu=C:\Lib\allegro4\src\dispsw.c */
int set_display_switch_mode(int);  /* raw=_set_display_switch_mode cu=C:\Lib\allegro4\src\dispsw.c */
int set_gfx_mode(int, int, int, int, int);  /* raw=_set_gfx_mode cu=C:\Lib\allegro4\src\graphics.c */
void set_palette(const RGB *);  /* raw=_set_palette cu=C:\Lib\allegro4\src\gfx.c */
void set_trans_blender(int, int, int, int);  /* raw=_set_trans_blender cu=C:\Lib\allegro4\src\colblend.c */
void set_volume(int, int);  /* raw=_set_volume cu=C:\Lib\allegro4\src\sound.c */
void show_mouse(BITMAP *);  /* raw=_show_mouse cu=C:\Lib\allegro4\src\mouse.c */
void simulate_keypress(int);  /* raw=_simulate_keypress cu=C:\Lib\allegro4\src\keyboard.c */
void solid_mode(void);  /* raw=_solid_mode cu=C:\Lib\allegro4\src\gfx.c */
void stop_midi(void);  /* raw=_stop_midi cu=C:\Lib\allegro4\src\midi.c */
void stop_sample(const SAMPLE *);  /* raw=_stop_sample cu=C:\Lib\allegro4\src\sound.c */
void stretch_blit(BITMAP *, BITMAP *, int, int, int, int, int, int, int, int);  /* raw=_stretch_blit cu=C:\Lib\allegro4\src\c\cstretch.c */
void stretch_sprite(BITMAP *, BITMAP *, int, int, int, int);  /* raw=_stretch_sprite cu=C:\Lib\allegro4\src\c\cstretch.c */
int text_height(const FONT *);  /* raw=_text_height cu=C:\Lib\allegro4\src\text.c */
int text_length(const FONT *, const char *);  /* raw=_text_length cu=C:\Lib\allegro4\src\text.c */
void textout_centre_ex(BITMAP *, const FONT *, const char *, int, int, int, int);  /* raw=_textout_centre_ex cu=C:\Lib\allegro4\src\text.c */
void textout_ex(BITMAP *, const FONT *, const char *, int, int, int, int);  /* raw=_textout_ex cu=C:\Lib\allegro4\src\text.c */
void textout_right_ex(BITMAP *, const FONT *, const char *, int, int, int, int);  /* raw=_textout_right_ex cu=C:\Lib\allegro4\src\text.c */
void textprintf_centre_ex(BITMAP *, const FONT *, int, int, int, int, const char *, ...);  /* raw=_textprintf_centre_ex cu=C:\Lib\allegro4\src\text.c */
void textprintf_ex(BITMAP *, const FONT *, int, int, int, int, const char *, ...);  /* raw=_textprintf_ex cu=C:\Lib\allegro4\src\text.c */
void textprintf_right_ex(BITMAP *, const FONT *, int, int, int, int, const char *, ...);  /* raw=_textprintf_right_ex cu=C:\Lib\allegro4\src\text.c */
void unload_datafile(DATAFILE *);  /* raw=_unload_datafile cu=C:\Lib\allegro4\src\datafile.c */
int voice_get_position(int);  /* raw=_voice_get_position cu=C:\Lib\allegro4\src\sound.c */
void voice_stop(int);  /* raw=_voice_stop cu=C:\Lib\allegro4\src\sound.c */
void vsync(void);  /* raw=_vsync cu=C:\Lib\allegro4\src\gfx.c */

#endif /* ICYTOWER_UPSTREAM_ALLEGRO / !ICYTOWER_BINDINGS_ACTIVE */

#endif /* ICYTOWER_ALLEGRO_API_H */
