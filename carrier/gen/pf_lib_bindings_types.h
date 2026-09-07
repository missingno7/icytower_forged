/* pf_lib_bindings_types.h -- GENERATED FILE. DO NOT EDIT.
 * Produced by tools/pf_win32_gen_lib_bindings.py.
 * Generated: 2026-09-07 21:05:01 UTC
 *
 * Additive type provider for pf_lib_bindings.h: the 23 struct/typedef
 * entities reachable from the library-scope allow-list that are NOT
 * already reachable from game scope (and therefore not already in
 * carrier/gen/it_types.h) -- GFX_DRIVER, GFX_MODE, GFX_MODE_LIST,
 * JOYSTICK_*, SYSTEM_DRIVER, _DRIVER_INFO, plus the Win32 scalar
 * typedefs those structs reference. Everything else (BITMAP, FONT, RGB,
 * SAMPLE, DATAFILE, PACKFILE, MIDI, PALETTE, fixed, ...) comes from the
 * #include below, verbatim, never redefined here.
 * Re-run gen_lib_bindings.py to regenerate; do not hand-edit.
 */

#ifndef PF_LIB_BINDINGS_TYPES_H
#define PF_LIB_BINDINGS_TYPES_H

#include "it_types.h"  /* game-scope types this file must not redefine */

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

#endif /* PF_LIB_BINDINGS_TYPES_H */
