# Living record — Icy Tower Win32 carrier pilot

Ledger of what executes, measurements, divergences, promotions, and rejected
approaches. Design decisions live in ../win32_pilot.md; evidence in notes/ and
artifacts/. Newest entries at the bottom of each section.

## Current architecture
- 2026-09-07: presentation: cnc-ddraw `windowed=true` in assets/ddraw.ini (backup artifacts/assets_backup/ddraw.ini.orig). Presentation-layer only; the game still sets 640x480 'fullscreen' and its state is unaffected. Applies to both the original exe and the carrier. Note ddraw.ini has savesettings=1, so cnc-ddraw may rewrite it with window position.
- 2026-09-07: isolated native execution carrier (see win32_pilot.md §3). Code in carrier/.

## What executes successfully
- 2026-09-07: milestones 5–7 done. `carrier.exe --det --pace=fast --input-script scripts/newgame.txt` reaches play() at tick 126 and runs 876 gameplay ticks; per-tick sha256 of the 151 game-owned globals is byte-identical across 5 independent runs (3 by the implementing agent, 2 re-verified by the supervisor with nobody at the keyboard). Negative control (one jump moved 300→301) first differs at T=301. Real-time runs diverge at once. Evidence: carrier/NOTES.md 'Milestones 5-7', artifacts/verify_det_run{1,2}.txt.
- 2026-09-07 run1: carrier/carrier.exe maps icytower15.exe at 0x400000, resolves all 320 imports through counting trampolines, runs the original code natively to MAIN MENU LOOP (assets/log.txt matches the baseline line-for-line except divergence 001). Milestones 2, 3, 4 done. Hand-written 1476 lines, generated 2266 lines (carrier/NOTES.md).

## Import/API classification
- 2026-09-07: static census done, notes/import_census.md (DIRECT 123 / WRAP 162 / DETERMINISTIC 31 / SHIM 4 / UNKNOWN 0).
- 2026-09-07 runtime census (artifacts/run1_report.json, 15 s to menu): 163 of 320 imports called; 4 threads make imports: main (all game logic, file I/O, rand/srand, Sleep 5184x), Allegro window thread (message pump only), ad-fetch pthread (WSOCK32 + file writes), Allegro high-perf timer thread (QPC 1901 + WaitForSingleObject 1900 + critical sections only). The DirectInput input thread makes no imports (COM only). GetProcAddress called 3x (targets not yet decoded). Wrappers needed so far: ExitProcess/exit/_cexit/abort (regain control), GetModuleFileNameA(NULL or carrier handle) → guest path, GetCommandLineA → guest path.

## Determinism model
- 2026-09-07 IMPLEMENTED (KNOWN by measurement): virtual clock driven from the main thread's Sleep wrapper; the two Allegro timer threads are never created (_beginthread wrapper by entry address) and _handle_timer_tick is called synchronously; QPC/timeGetTime/time/clock return virtual values; input injected through Allegro's _handle_key_press/_release at tick T; real keyboard parked via a hardware breakpoint on key_dinput_handle_scancode; malloc family → fixed-address bump arena at 0x20000000. Sensors are hardware breakpoints (DR0-3 + VEH), no code patching.
- 2026-09-07 FINDING (KNOWN): hashing raw .data+.bss never converges even with a deterministic heap: ~20–30 Allegro/CRT/DirectX globals hold host object identities (COM pointers, HANDLEs, HWND) that vary per process. The comparison domain is therefore the game-owned globals (generated list, carrier/gen/gen_game_globals.py). Consequence for the framework: the verdict domain must be generated from ownership (DWARF CU), not from section ranges.
- 2026-09-07: HYPOTHESIS in win32_pilot.md §5. 
- 2026-09-07: RESOLVED (notes/replay_format.md, KNOWN): the QPC/clock/time calls in play() are anti-cheat slow-down telemetry, not simulation inputs; the 20 ms tick global 0x506938 is the only pacing source. The game's own replay = Treplay.random_seed (offset 164, from rand() after srand(time)) + RLE Trecord{key_flags:u8, cycle_count:int} stream; only left/right/fire survive (mask 0x93). Gameplay input surface = seed + per-tick left/right/fire. Menu/profile/character choice precede recording and are outside it.

## Snapshot model
- 2026-09-07: HYPOTHESIS in win32_pilot.md §6. Nothing implemented. Evidence so far supports in-process rewind: guest globals + arena + guest stack + main-thread CONTEXT at the safepoint; rand() state lives in msvcrt (host) and must be externalized by pinning the LCG in the carrier.

## Unsupported / problematic behaviours
- 2026-09-07: 0x400000 range is occupied by NLS mappings before any user code runs; TEMPORARY fix = self-relaunch as suspended child + VirtualAllocEx (carrier/NOTES.md #3). Generic Win32 fact; belongs in the framework loader.
- 2026-09-07: msvcrt's __getmainargs reads the process command line internally, bypassing the IAT; fixed by launching the child with the guest's own command line and passing carrier options via PF_* env vars (NOTES.md #6).

## Measurements
- 2026-09-07: gameplay corpus available without any recording: assets/profiles/MissingNO/replays/*.itr (8 game-native replays, e.g. best_floor_204). Candidate workload for the ORIGINAL vs LIFTED vs NATIVE comparison (win32_pilot.md §4a).
- .text ownership: Allegro 61.5 %, vorbis/ogg 17.0 %, game 16.5 % (125641 B, 253 functions), CRT 3.6 %.

## First-divergence investigations
- 001 (2026-09-07) RESOLVED, not a carrier defect: the line after 'Malformed HTTP response:' is the raw HTTP body from www.icytower.com printed through a single %s; the site returned different bytes in run1. Reproduced: original and carrier run back-to-back both print an empty line. Channel = live network on the ad-fetch thread (DIRECT WSOCK32). Det mode must suppress or record it. notes/divergence_001_log_format.md.

## Native-promotion progress
- 2026-09-07: generated interop from DWARF (carrier/gen/gen_interop.py → it_types.h, it_globals.h, it_funcs.h, it_funcs_table.inc): game scope 156 globals, 242 functions, 70 structs, 801/801 layout checks pass under MSVC x86; scope=all also verified (2376/2376). This is the generated equivalent of OpenLoco's loco_global/Interop::call. Caveats in carrier/gen/INTEROP_NOTES.md (long double opaque, 6 cross-CU size conflicts in Allegro/DirectX types, no calling-convention DWARF → COFF decoration used).
- 2026-09-07: candidates ranked statically (notes/promotion_candidates.md): update_frame 0x406ac4 (120 B, no x87, first pick), jump_player 0x418678 (198 B, 22 x87, fidelity test), is_solid 0x4166dc (pure predicate, negative control). 124/253 game functions statically reachable from the gameplay anchors.
- 0 functions promoted. Lifter (LIFTED form) in progress under carrier/lift/.

## Rejected approaches
- see win32_pilot.md §10.
