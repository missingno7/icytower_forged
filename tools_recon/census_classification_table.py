# Manual per-import classification table for the icytower15.exe import census.
# Keyed by import name (msvcrt's duplicate "_stat" entries share one entry; both
# get the same class since they are semantically identical, just two IAT slots).
#
# Each value: (class, callback_bearing(bool), rationale)
# class in {"DIRECT","WRAP","DETERMINISTIC","SHIM","UNKNOWN"}

T = {}

def add(names, cls, cb, rationale):
    if isinstance(names, str):
        names = [names]
    for n in names:
        T[n] = (cls, cb, rationale)

# ---------------------------------------------------------------- DDRAW.dll
add("DirectDrawCreate", "SHIM", False,
    "Legacy DirectDraw COM factory (Allegro wddraw.c _init_directx). cnc-ddraw's "
    "ddraw.dll is already installed in assets/ as a drop-in replacement for this "
    "exact DLL -- first option is 'do nothing', the existing PATH/side-by-side "
    "ddraw.dll shim already intercepts this. If not using cnc-ddraw, forwarding to "
    "host's real ddraw.dll works on Win11 (DirectDraw is still present) but HW "
    "accel/exclusive-mode behavior differs; the returned IDirectDraw2/Surface COM "
    "object is the real boundary (see COM section).")

# ---------------------------------------------------------------- dinput.dll
add("DirectInputCreateA", "SHIM", False,
    "Legacy DirectInput7 factory (wmouse.c/wkeybd.c/wjoydx.c _*_directx_init). "
    "Windows 11 still ships dinput.dll for compatibility so straight host-forward "
    "is plausible; cnc-ddraw does not shim DirectInput so this would bypass it. "
    "Real work happens through the returned IDirectInputDevice(2) vtable (see COM section).")

# ---------------------------------------------------------------- dsound.dll
add("DirectSoundCreate", "SHIM", False,
    "Legacy DirectSound factory (wdsound.c/wdsndmix.c _digi_directsound*_init/detect). "
    "Win11 keeps dsound.dll for compatibility (software emulation via WASAPI under the "
    "hood) so host-forwarding is the pragmatic first option; cnc-ddraw does not shim "
    "DirectSound. Real buffer I/O is through IDirectSoundBuffer vtable calls (COM section).")
add("DirectSoundEnumerateA", "SHIM", True,
    "Enumerates DirectSound devices via an LPDSENUMCALLBACKA passed as a parameter -- "
    "this is a genuine import-level callback (not just a COM vtable one). Callback "
    "target here is Allegro's own internal enumerator (__get_win_digi_driver_list), "
    "not game code, so the re-entrancy risk for a carrier is Allegro-internal, not guest-game.")

# ---------------------------------------------------------------- GDI32.dll
add(["CreateBitmap","CreateCompatibleBitmap","CreateCompatibleDC","CreateDIBitmap",
     "CreatePalette","CreateSolidBrush"], "WRAP", False,
    "Creates a GDI object handle (bitmap/DC/palette/brush). Handle identity/lifetime "
    "is exactly the kind of host-resource boundary WRAP exists for, even though the "
    "call itself is not observability-critical for gameplay.")
add(["DeleteDC","DeleteObject"], "WRAP", False,
    "Destroys a GDI handle -- pair with the Create* wrap for lifecycle tracking/leak detection.")
add(["SelectObject","SelectPalette"], "WRAP", False,
    "Binds a GDI object into a DC's selection slot -- handle-identity boundary worth tracing.")
add(["BitBlt","StretchBlt","StretchDIBits","GetDIBits","GetObjectA",
     "GetPaletteEntries","GetSystemPaletteEntries","RealizePalette",
     "SetPaletteEntries","SetPixel","GetDeviceCaps"], "DIRECT", False,
    "Pure draw/query operation on an already-open DC/bitmap; no new handle is created "
    "and the operation is deterministic given deterministic pixel/palette input. Used by "
    "Allegro's GDI-mode (non-DirectDraw) software blit fallback (wgdi.c/gdi.c).")

# ---------------------------------------------------------------- KERNEL32.dll
add("CloseHandle", "WRAP", False, "Generic handle-close; worth tracing since it applies to events, files, threads alike.")
add("CreateEventA", "WRAP", False, "Creates a named/unnamed sync-object handle (used by Allegro's timer/input threads and fld_adspot's worker thread).")
add(["InitializeCriticalSection","DeleteCriticalSection"], "WRAP", False,
    "Creates/destroys an in-process lock object; identity matters for tracking lock lifetime even though the lock itself is not a kernel handle.")
add(["EnterCriticalSection","LeaveCriticalSection"], "WRAP", False,
    "Lock acquire/release around Allegro's shared state (timer/input threads). Ordering "
    "matters for thread-interleaving reproducibility but this is Allegro plumbing, not "
    "gameplay logic; tracing (not full control) is enough.")
add("DuplicateHandle", "WRAP", False, "Handle-identity duplication boundary.")
add("ExitProcess", "WRAP", False, "Process-termination boundary; the carrier needs to observe this to shut itself down cleanly.")
add(["AddAtomA","FindAtomA","GetAtomNameA"], "WRAP", False,
    "Global atom-table entries, used by Allegro's window-class registration (wwnd.c). Host-global namespace shared across all processes -- worth tracing, low risk.")
add("FormatMessageA", "DIRECT", False, "Pure string formatting of a Win32 error code; no state.")
add(["FreeLibrary","LoadLibraryA","GetModuleHandleA"], "WRAP", False,
    "Dynamic module load/unload boundary. A carrier needs to intercept these because the "
    "guest can load arbitrary further DLLs at runtime -- outside the static import table entirely.")
add("GetProcAddress", "WRAP", False,
    "Dynamic symbol resolution -- this is the reverse of the import table: whatever the "
    "guest resolves through GetProcAddress is INVISIBLE to this static import census. "
    "Needs a runtime hook/log to discover late-bound calls (e.g. optional-API feature "
    "detection patterns common in 2011-era Windows code).")
add(["GetCommandLineA","GetCurrentProcess","GetCurrentThread","GetLogicalDrives",
     "GetModuleFileNameA","GetStartupInfoA","GetTempPathA","GetThreadPriority",
     "SetThreadPriority","GetVersion","GetVersionExA","IsDBCSLeadByteEx",
     "MultiByteToWideChar","WideCharToMultiByte","OutputDebugStringA",
     "SystemParametersInfoA"], "DIRECT", False,
    "Pure query/conversion of host state with no persistent resource created; safe to "
    "forward as-is. (GetVersion/GetVersionExA will report Win11's compatibility-shimmed "
    "version, which is a host quirk, not a carrier concern.)")
add("InterlockedExchange", "DIRECT", False, "Atomic op on guest-owned memory; no host-resource identity involved.")
add(["PulseEvent","ResetEvent","SetEvent"], "WRAP", False,
    "Event-object signaling; identity of the specific event handle matters for correctly "
    "coordinating Allegro's timer/input threads and fld_adspot's worker thread.")
add(["QueryPerformanceCounter","QueryPerformanceFrequency"], "DETERMINISTIC", False,
    "High-resolution time source. QueryPerformanceCounter is called DIRECTLY from GAME "
    "code (_play in main.c, the core game loop) in addition to Allegro's wtimer.c -- this "
    "is the single most important determinism interception point found in this census: "
    "the game's own frame/physics timing reads QPC, not just Allegro's plumbing.")
add("Sleep", "DETERMINISTIC", False,
    "Explicit time-based yield. All static call sites found are Allegro plumbing "
    "(wsystem.c yield-timeslice, wtimer.c rest) or CRT (mingwex dtoa lock backoff); no "
    "GAME-code call site was found, so this only needs control for full determinism if "
    "sub-frame thread interleaving must be pinned down, not for core gameplay logic.")
add("SetUnhandledExceptionFilter", "WRAP", True,
    "Installs a crash-handler callback. Only static caller is MinGW's own CRT startup "
    "(___mingw_CRTStartup) installing its default handler -- not game-specific, fires only "
    "on unhandled crash.")
add(["VirtualProtect","VirtualQuery"], "WRAP", False,
    "Memory-protection manipulation/introspection. Only static callers found are MinGW's "
    "pseudo-reloc.c (___write_memory, handling runtime relocation of the .rdata->rwdata "
    "PE trick), not game anti-tamper as first suspected from beta.c's naming. Still worth "
    "tracing since a carrier lifting arbitrary code should watch page-protection changes.")
add(["WaitForMultipleObjects","WaitForSingleObject"], "DETERMINISTIC", False,
    "Blocking wait on kernel sync objects, all static call sites are Allegro plumbing "
    "(wkeybd.c input wait, wtimer.c thread join/rest, wwnd.c/winput.c thread start "
    "rendezvous). Thread-scheduling primitive explicitly named in the task brief; needs "
    "control if full multi-thread determinism is required, but does not gate GAME logic directly.")

# ---------------------------------------------------------------- OLE32.dll
add("CoCreateInstance", "WRAP", True,
    "COM activation -- only static callers are wdsinput.c's DirectSound-capture "
    "detect/init (_digi_directsound_capture_detect/init), almost certainly instantiating "
    "a DirectSoundCapture-family COM object for microphone/line-in support. Vtable calls "
    "on the returned object are invisible to this import census (see COM boundary section).")
add("CoInitialize", "WRAP", False, "COM apartment init, called once per new thread from Allegro's wthread.c (__win_thread_init).")
add("CoUninitialize", "WRAP", False, "COM apartment teardown, pairs with CoInitialize.")

# ---------------------------------------------------------------- SHELL32.DLL
add("ShellExecuteA", "WRAP", False,
    "Launches the OS shell / default browser (_open_web_browser in main.c, GAME code -- "
    "almost certainly the menu's 'visit website' link). Security/observability boundary: "
    "a carrier should intercept this rather than let a lifted 2012 binary silently open a "
    "URL through the modern shell.")

# ---------------------------------------------------------------- USER32.dll
add(["GetAsyncKeyState","GetKeyboardState","GetCursorPos","SetCursorPos","ToAscii",
     "MapVirtualKeyA","GetKeyNameTextA"], "DETERMINISTIC", False,
    "Raw input sampling. All static call sites are Allegro's input drivers (wkeybd.c "
    "_update_shifts/_key_dinput_*, wmouse.c _mouse_directx_*/_mouse_dinput_handle) -- GAME "
    "code (player.c/control.c) never calls these directly, it reads Allegro's already-"
    "abstracted key_pressed[]/mouse state. These Win32 calls ARE nonetheless the actual "
    "raw-input sampling points and are the correct place to intercept/record for "
    "deterministic replay, since anything downstream is pure (deterministic) game logic.")
add(["CreateWindowExA","DefWindowProcA","DestroyWindow","AdjustWindowRect","MoveWindow",
     "SetWindowPos","ShowWindow","UpdateWindow","GetClientRect","GetWindowRect",
     "GetWindowLongA","SetWindowLongA","SetClassLongA","GetClassLongA","GetSystemMenu",
     "EnableMenuItem","SetWindowTextA","GetActiveWindow","GetForegroundWindow",
     "SetForegroundWindow","ClientToScreen","IsIconic"], "WRAP", False,
    "Window handle creation/management -- 'windows' are explicitly named in the WRAP "
    "definition. All static callers are Allegro's wwnd.c/wddraw*.c/wsystem.c window driver.")
add("RegisterClassA", "WRAP", True,
    "Registers a window class with a WNDPROC pointer (Allegro's own _directx_wnd_proc in "
    "wwnd.c) -- textbook callback-bearing import; every subsequent message dispatched to "
    "this window re-enters guest (Allegro) code through that pointer.")
add("CallWindowProcA", "WRAP", True, "Explicitly re-invokes a WNDPROC (guest code) by pointer -- callback re-entry point.")
add("DispatchMessageA", "WRAP", True,
    "The Windows message pump's dispatch step -- functionally re-enters the guest WndProc "
    "registered via RegisterClassA. Not a parameter-passed callback, but the same "
    "guest-reentrancy boundary in effect; flagged for completeness.")
add(["GetMessageA","PeekMessageA","MsgWaitForMultipleObjects"], "DETERMINISTIC", False,
    "Message-pump gating: blocks/paces the main loop and is the channel through which real "
    "WM_KEYDOWN/WM_MOUSEMOVE/WM_TIMER/WM_SIZE etc. arrive. Loop pacing and event order here "
    "affect frame timing and input delivery order, so this needs control for a fully "
    "deterministic replay, even though it is Allegro plumbing rather than GAME code.")
add(["PostMessageA","PostQuitMessage","SendMessageA","RegisterWindowMessageA"], "WRAP", False,
    "Message-routing calls; SendMessageA in particular re-enters a WndProc synchronously "
    "(same caveat as DispatchMessageA) but has no static caller among the sampled sites "
    "beyond Allegro's own window/thread plumbing.")
add(["BeginPaint","EndPaint","InvalidateRect","RedrawWindow","GetDC","ReleaseDC"], "WRAP", False,
    "DC/paint-cycle handle boundary tied to the window's GDI surface.")
add(["LoadCursorA","SetCursor","LoadIconA","CreateIconIndirect","DestroyIcon"], "WRAP", False,
    "GDI cursor/icon resource handles -- window-chrome, not gameplay-relevant, but a "
    "resource-identity boundary per the WRAP definition.")
add("SetTimer", "DETERMINISTIC", True,
    "Textbook callback-bearing import (optional TIMERPROC pointer) and, more importantly, "
    "a WM_TIMER-driven time source. Only static caller is Allegro's window proc "
    "(_directx_wnd_proc in wwnd.c) with a single call site -- likely a periodic "
    "housekeeping/redraw tick rather than the main game clock (that is QueryPerformanceCounter).")
add("KillTimer", "WRAP", False, "Pairs with SetTimer to cancel the timer.")
add(["MessageBoxA","MessageBoxW"], "WRAP", False,
    "Modal UI that blocks the calling thread on real user input. Static callers found: "
    "Allegro's wthread.c (__win_thread_init error path) and (via data_refs on __iob) "
    "CRT/Allegro assert/error paths. Blocking on a real human click is a nondeterminism "
    "risk for any replay/automation harness, worth flagging even though class is WRAP not DETERMINISTIC.")
add("GetSystemMetrics", "DIRECT", False, "Host display-metrics query (screen size etc.), pure read, no handle created.")

# ---------------------------------------------------------------- WINMM.DLL
add("timeGetTime", "DETERMINISTIC", False,
    "Millisecond time source. All static call sites are Allegro's wtimer.c low-perf timer "
    "thread (_tim_win32_low_perf_thread/_tim_win32_rest) -- Allegro plumbing, not called "
    "directly by GAME code (which uses QueryPerformanceCounter instead, see above).")
add(["joyGetDevCapsA","joyGetNumDevs"], "WRAP", False, "Joystick device enumeration/capability query, not per-frame -- Allegro's wjoyw32.c fallback driver.")
add("joyGetPosEx", "DETERMINISTIC", False,
    "Per-poll joystick position read (input). Only static caller is Allegro's wjoyw32.c "
    "(_joystick_win32_poll/_init) -- legacy WinMM joystick fallback driver used when "
    "DirectInput joystick isn't available; feeds the same abstracted joystick state GAME "
    "code (control.c's _get_gamepad et al.) reads indirectly.")
add(["midiInClose","midiInGetDevCapsA","midiInGetNumDevs","midiInReset","midiInStart",
     "midiInStop","midiOutClose","midiOutGetDevCapsA","midiOutGetNumDevs","midiOutGetVolume",
     "midiOutReset","midiOutSetVolume","midiOutShortMsg"], "WRAP", False,
    "MIDI device I/O, still natively supported on Windows 11 (no shim needed) -- device-"
    "handle identity/state boundary, Allegro's midi.c driver.")
add(["midiInOpen","midiOutOpen"], "WRAP", True,
    "Opens a MIDI device with an optional callback (function pointer or window/thread "
    "handle) for input/completion notification -- callback-bearing.")
add(["waveOutClose","waveOutGetPosition","waveOutGetVolume","waveOutPause",
     "waveOutPrepareHeader","waveOutReset","waveOutRestart","waveOutSetVolume",
     "waveOutUnprepareHeader","waveOutWrite"], "WRAP", False,
    "Waveform-audio device/buffer I/O, still natively supported on Win11 -- device-handle "
    "and audio-buffer identity boundary (Allegro's wsndwo.c waveOut digital sound driver).")
add("waveOutOpen", "WRAP", True,
    "Opens a waveOut device with an optional callback (function/window/thread/event) for "
    "buffer-completion notification -- callback-bearing.")

# ---------------------------------------------------------------- WSOCK32.DLL
add("WSAStartup", "WRAP", False,
    "Winsock negotiation, called once from GAME's _init_game (main.c) unconditionally at "
    "startup -- one-time handshake, no data-dependent effect on replay by itself.")
add(["socket","closesocket","setsockopt","htons","WSAGetLastError"], "WRAP", False,
    "Socket/protocol setup and error-query calls, all from GAME's _HTTPFetchInternal "
    "(httpget.c) -- non-data-bearing plumbing around the actual network I/O below.")
add(["connect","recv","send","gethostbyname"], "DETERMINISTIC", False,
    "Actual network I/O -- explicitly named in the task's DETERMINISTIC/network category. "
    "KNOWN: the sole caller of every one of these is GAME code (_HTTPFetchInternal in "
    "httpget.c), used by the ad-banner subsystem (fld_adspot.c) to fetch remote ad images/"
    "config, not by core gameplay/scoring logic (hisc.c's own high-score code has no "
    "network calls in this census). Content/timing/failure of these calls should NOT be "
    "allowed to affect gameplay determinism; recommend stubbing/sandboxing the whole "
    "ad-fetch path for replay purposes rather than trying to make live network I/O deterministic.")

# ---------------------------------------------------------------- libpng3.dll
_png_all = ["png_create_info_struct","png_create_read_struct","png_create_write_struct",
    "png_destroy_read_struct","png_destroy_write_struct","png_error","png_get_IHDR",
    "png_get_PLTE","png_get_gAMA","png_get_io_ptr","png_get_rowbytes","png_get_sRGB",
    "png_get_valid","png_read_end","png_read_info","png_read_row","png_read_update_info",
    "png_set_IHDR","png_set_PLTE","png_set_bgr","png_set_compression_level","png_set_expand",
    "png_set_gamma","png_set_gray_to_rgb","png_set_interlace_handling","png_set_packing",
    "png_set_sig_bytes","png_set_strip_16","png_set_tRNS_to_alpha","png_sig_cmp",
    "png_write_end","png_write_info","png_write_row"]
add(_png_all, "DIRECT", False,
    "libpng3.dll is a bundled third-party DLL shipped in assets/, not a Windows OS "
    "component -- it should simply keep being loaded/called as an ordinary DLL dependency, "
    "not 'forwarded to host Windows'. All static callers are GAME code's own PNG wrapper "
    "(loadpng.c/savepng.c/regpng.c: _really_load_png et al.). No observability/determinism "
    "concern beyond deterministic decode of deterministic file bytes.")
add(["png_set_read_fn","png_set_write_fn"], "DIRECT", True,
    "Registers a custom stream read/write function pointer that libpng calls back into "
    "during png_read_*/png_write_* -- textbook callback-bearing import (explicit example "
    "in the task brief). Callback target is GAME code (loadpng.c/savepng.c's own I/O shims).")

# ---------------------------------------------------------------- pthreadGC2.dll
add("pthread_create", "DETERMINISTIC", True,
    "Thread creation with a start-routine function pointer -- callback-bearing, and "
    "thread creation affects scheduling/timing nondeterminism. Sole static caller is "
    "GAME's _fldads_start (fld_adspot.c) spinning up the ad-fetch background thread -- "
    "again the ad subsystem, not core gameplay, so its nondeterminism is containable by "
    "sandboxing that one thread rather than needing full scheduler control.")
add(["pthread_mutex_lock","pthread_mutex_unlock"], "WRAP", False,
    "Mutex used to guard the ad-cache data shared between _fldads_start's worker thread "
    "and the main thread; lock-identity/ordering boundary, ancillary to gameplay.")

# ---------------------------------------------------------------- msvcrt.dll
_file_io = ["_chdir","_close","_dup","_getcwd","_lseek","_mkdir","_open","_read","_rmdir",
    "_stat","_unlink","_write","_findclose","_findfirst","_findnext","_getdcwd","_getdrive",
    "_setmode","_wfindfirst","_wfindnext","_wgetdcwd","_wopen","_wstat","_wunlink",
    "fclose","fopen","fread","fwrite","fseek","ftell","fflush","fgets","fputc","fputs",
    "tmpnam","perror"]
add(_file_io, "WRAP", False,
    "File-handle / filesystem-path boundary ('files, handles' -- explicit WRAP examples). "
    "Used throughout GAME code for config/profile/hiscore/replay/custom-character/ad-cache "
    "persistence (custom.c, profile.c, hisc.c, replay.c, fld_adspot.c) as well as Allegro's "
    "file.c/wfile.c and CRT startup. A carrier needs to virtualize/observe this boundary "
    "regardless of determinism, simply to relocate the game's data directory correctly.")
add(["malloc","calloc","realloc","free","memcpy","memmove","memset","memchr"], "DIRECT", False,
    "Pure heap/memory operations on guest-owned memory; no host-resource identity involved, "
    "used everywhere (GAME, Allegro, libvorbis/libogg).")
add(["strcmp","strcpy","strcat","strncmp","strncpy","strncat","strchr","strpbrk","strlen",
     "_strdup","_stricmp","_strnicmp","wcslen","sprintf","vsprintf","sscanf","printf"], "DIRECT", False,
    "Pure string/buffer operations, deterministic given deterministic inputs; printf writes "
    "to the process's (normally invisible, GUI-subsystem) stdout stream.")
add(["fprintf","vfprintf"], "WRAP", False,
    "Formatted write through a FILE* handle (frequently the log/ad-cache/response-dump "
    "files in httpget.c/fld_adspot.c/profile.c, or __iob's stdout/stderr) -- file-handle "
    "boundary, grouped with the other file I/O above.")
add(["acos","atan","cos","sin","sqrt","pow","log","log10","exp","ceil","floor","fmod",
     "frexp","ldexp","_hypot"], "DIRECT", False,
    "Pure deterministic math library calls, no host-resource identity. NOTE (INFERRED "
    "determinism risk, not a class change): this is a 32-bit GCC 4.4.1 build with no "
    "-mfpmath flag evidence found, i.e. it likely targets the x87 80-bit FPU by default. "
    "Forwarding these to a modern host libm (SSE2, 64-bit double throughout) can change "
    "the last bit or two of a transcendental result versus the original XP-era msvcrt.dll. "
    "Not expected to matter for this game's visible logic (no evidence any of these feed "
    "position/collision determinism the way rand()/QPC do) but worth a regression check "
    "before assuming bit-exact replay across old-binary vs carrier runs.")
add(["time","clock","localtime","gmtime","mktime"], "DETERMINISTIC", False,
    "Wall-clock/CPU-clock time sources -- explicit 'time' category. Used for timestamping "
    "(hi-score dates, profile/save data, replay files, log entries) and possibly session "
    "pacing; must be virtualized for reproducible replay/testing.")
add(["_tzset","_tzname"], "WRAP", False, "Host timezone configuration query -- affects localtime() results, environment-dependent.")
add("rand", "DETERMINISTIC", False,
    "KNOWN, HIGH PRIORITY: called directly by GAME code -- _create_post (beta.c, hidden/"
    "obfuscated module), _fldads_get_random_ad (fld_adspot.c, ad rotation -- not gameplay), "
    "and _new_game (main.c). The _new_game call site is the concerning one: this is the "
    "core per-run gameplay RNG (likely floor/obstacle layout, item spawns). Because rand() "
    "here is msvcrt's own PRNG algorithm (LCG-based, NOT reimplemented by the game), a "
    "carrier that forwards this to a *different* CRT/host rand() implementation, or a host "
    "whose msvcrt.dll rand() was patched/changed across Windows versions, would silently "
    "desync all replays. This must be intercepted with a game-controlled deterministic PRNG "
    "seeded/recorded explicitly, not naively forwarded.")
add("srand", "DETERMINISTIC", False,
    "KNOWN: called directly by GAME code (_new_game x2, _init_game -- main.c), i.e. the "
    "game seeds its own gameplay RNG itself (likely from time() or similar at run start). "
    "This is exactly the seed-capture point a replay system must record.")
add("qsort", "DIRECT", True,
    "Callback-bearing (comparator function pointer -- explicit example in task brief). "
    "GAME's only use (_update_file_list, replay.c) sorts the replay-browser file list for "
    "UI display, not gameplay/replay-affecting data; other static callers are Allegro (gui.c "
    "focus order, graphics.c mode list, quantize.c palette generation) and libvorbis/libogg "
    "internals (codebook/psy/floor setup) -- none of these affect game determinism. INFERRED "
    "caveat: msvcrt's qsort is not guaranteed stable, so a different host CRT's qsort could "
    "reorder equal-key elements differently; irrelevant here since no observed use sorts "
    "anything gameplay- or score-critical, but flagged in case that assumption is wrong "
    "for an unobserved dynamic code path.")
add(["exit","abort","_cexit"], "WRAP", False, "Process-termination boundary, same rationale as ExitProcess -- carrier needs to observe for clean teardown.")
add("_assert", "WRAP", False, "Assertion-failure path (message + abort); static callers are all CRT/pseudo-reloc internals here, not GAME asserts, but worth tracing since it terminates the process.")
add(["atexit","_onexit"], "WRAP", True,
    "Registers an exit-time cleanup callback -- callback-bearing. Reached in this binary "
    "only through the CRT's own internal _atexit/__onexit trampoline wrappers (crt1.c), "
    "which forward to these msvcrt imports via a load-then-indirect-jump thunk rather than "
    "a direct call/jmp *IAT -- a second import-thunk idiom distinct from the usual single-"
    "instruction 'jmp *0x51xxxx' pattern, found while parsing the disassembly.")
add("signal", "WRAP", True, "Installs a signal handler callback (SIGABRT etc.) -- callback-bearing, standard CRT plumbing.")
add("_errno", "DIRECT", False, "Thread-local last-error query, pure read.")
add("_setjmp", "DIRECT", False, "Non-local jump / saved CPU register state; no host-resource identity, deterministic.")
add("getenv", "WRAP", False,
    "Reads a HOST environment variable. Static callers include GAME's loadpng.c "
    "(_really_load_png -- likely a debug/override env var) and Allegro's allegro.c/file.c "
    "(resource path discovery). Environment differs between the original XP-era machine and "
    "any carrier host, so this needs virtualizing for reproducible directory/config resolution.")
add(["localeconv","toupper","_isctype"], "DIRECT", False, "Locale/ctype queries; static, no host-identity or determinism concern for gameplay.")
add(["__set_app_type","__getmainargs","__p__environ","__p__fmode","__mb_cur_max",
     "__lc_codepage","_iob","_pctype","_onexit"], "DIRECT", False,
    "MinGW/CRT-internal startup data and initialization routines invoked once from the "
    "compiler-generated CRT glue (mingw's own main.c/crt1.c), not by GAME or Allegro code. "
    "No gameplay/determinism impact; several of these (__lc_codepage, __mb_cur_max, _iob, "
    "_pctype, _tzname) are DATA imports read directly rather than called.")

_stat_note = ("KNOWN, two separate IAT slots share this name: one used by GAME's fld_adspot.c "
    "(_fldads_load_cache_from_csv/_update_local_adimg/_threadmain -- checking cached ad-image "
    "file timestamps/existence) and one used by Allegro's wfile.c (__al_file_time/"
    "__al_file_size_ex -- generic file-info queries). Same WRAP rationale as the rest of the "
    "file-I/O family; listed once here, both slots appear in the machine-readable JSON.")
T["_stat"] = ("WRAP", False, _stat_note)

add("_beginthread", "DETERMINISTIC", True,
    "Thread creation with a start-routine function pointer -- explicit callback-bearing "
    "example in the task brief. All 4 static callers are Allegro plumbing (wtimer.c's two "
    "timer threads, wwnd.c's directx window thread, winput.c's input-event thread) -- not "
    "called by GAME code directly (GAME's own background thread, fld_adspot's ad fetch, "
    "uses pthread_create instead, see pthreadGC2.dll). Thread creation affects scheduling/"
    "timing nondeterminism, hence DETERMINISTIC, but since every caller is Allegro-internal "
    "plumbing (timer tick, window-message pump, input polling) rather than gameplay logic, "
    "the practical fix is to make Allegro's OWN abstractions (its timer callbacks, its "
    "input polling results) deterministic/replayable, not to literally control thread "
    "creation timing itself.")
add(["atof","strtol"], "DIRECT", False, "Pure string-to-number parsing, deterministic given deterministic input text.")
add("strerror", "DIRECT", False, "Pure errno-to-string table lookup, no host state.")

print("classification table entries:", len(T))
