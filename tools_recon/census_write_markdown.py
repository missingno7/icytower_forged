# -*- coding: utf-8 -*-
import json
from collections import defaultdict, Counter

ROOT = r"D:\Games\DOS\dos_recosystem\icytower_forged"

recs = json.load(open(ROOT + r"\artifacts\import_census.json", encoding="utf-8"))
stats = json.load(open(ROOT + r"\tools_recon\census_summary_stats.json", encoding="utf-8"))
vtbl = json.load(open(ROOT + r"\tools_recon\census_vtable_calls.json", encoding="utf-8"))

recs_sorted = sorted(recs, key=lambda r: (r["dll"].lower(), r["name"].lower()))

def esc(s):
    if s is None:
        return ""
    return str(s).replace("|", "\\|").replace("\n", " ")

def origin_tag(o):
    return {"GAME": "game", "ALLEGRO": "allegro", "CRT": "crt", "VORBIS_OGG": "vorbis/ogg"}.get(o, o.lower())

lines = []
lines.append("# icytower15.exe Win32 Import / API Census")
lines.append("")
lines.append("Target: `D:\\Games\\DOS\\dos_recosystem\\icytower_forged\\assets\\icytower15.exe` "
             "(Icy Tower 1.5.1, PE32 @ ImageBase 0x400000, MinGW GCC 4.4.1, statically linked "
             "Allegro 4.4 + its Windows drivers, statically linked libvorbis/libogg, full "
             "DWARF + COFF symbols present, dynamically linked libpng3.dll/pthreadGC2.dll/zlib1.dll).")
lines.append("")
lines.append("Method (all claims below are marked KNOWN or INFERRED; see per-row rationale):")
lines.append("")
lines.append("- Import table dumped previously to `imports.json` (320 entries, [dll, name, IAT slot VA]).")
lines.append("- Full linear disassembly generated with `objdump -d assets/icytower15.exe` -> "
             "`artifacts/icytower_disasm.txt` (238,295 lines).")
lines.append("- Call-site attribution: parsed every `call *0x51xxxx` / `jmp *0x51xxxx` (direct IAT-slot "
             "indirect call -- the common MinGW dllimport pattern) **and** every named-thunk call "
             "(`call ADDR <_Name@N>` where `ADDR` is a single-instruction `jmp *0x51xxxx` thunk -- the "
             "pattern used for imports linked without `__declspec(dllimport)`, e.g. most of msvcrt.dll "
             "and libpng3.dll). A third, rarer two-instruction thunk idiom "
             "(`mov 0x51xxxx,%eax ; ... ; jmp/call *%eax`) was found and handled specially for "
             "`atexit`/`_onexit` (MinGW's own CRT-startup wrapper functions of the same name forward "
             "to the real import this way).")
lines.append("- Caller -> source-CU attribution: primarily DWARF `.debug_info` compile-unit `low_pc`/"
             "`high_pc` ranges (full source paths, e.g. `F:\\projects\\icytower\\trunk\\source\\main.c` "
             "vs `C:\\Lib\\allegro4\\src\\win\\wddraw.c` -- this is what correctly separates the game's "
             "own `timer.c`/`main.c` from Allegro's own files of the same base name). Functions outside "
             "DWARF coverage (CRT/mingwex glue, libvorbis/libogg object files compiled without `-g`) "
             "fall back to the COFF symbol table's per-function `.file` aux entry.")
lines.append("- 320/320 imports were attributed to at least one call site or direct data reference "
             "except `msvcrt.dll!ftell`, which has **zero** static references anywhere in the "
             "disassembly (dead/unused import -- INFERRED reachable only via a code path not present, "
             "or vestigial from a statically-linked library routine that itself is never called).")
lines.append("")

lines.append("## Summary")
lines.append("")
lines.append(f"Total imports: **{len(recs)}**")
lines.append("")
lines.append("### By class")
lines.append("")
lines.append("| class | count | % of total |")
lines.append("|---|---:|---:|")
for cls in ["DIRECT", "WRAP", "DETERMINISTIC", "SHIM", "UNKNOWN"]:
    c = stats["class_counts"].get(cls, 0)
    if c or cls in ("DIRECT","WRAP","DETERMINISTIC","SHIM"):
        lines.append(f"| {cls} | {c} | {100.0*c/len(recs):.1f}% |")
lines.append("")
lines.append(f"**DIRECT + WRAP = {stats['class_counts'].get('DIRECT',0)+stats['class_counts'].get('WRAP',0)} "
             f"/ {len(recs)} = {stats['pct_direct_wrap']:.1f}%** of all imports need no behavioral "
             "reimplementation at all -- straight host forward (DIRECT) or forward-through-a-thin-"
             "tracing-shim (WRAP). Only the DETERMINISTIC set (31 imports, 9.7%) needs real interception "
             "logic, and only the SHIM set (4 imports, 1.3%: the DirectX factory functions) needs "
             "actual reimplementation/replacement.")
lines.append("")
lines.append("### By DLL (with class breakdown)")
lines.append("")
lines.append("| dll | total | DIRECT | WRAP | DETERMINISTIC | SHIM |")
lines.append("|---|---:|---:|---:|---:|---:|")
for dll in sorted(stats["dll_counts"], key=str.lower):
    cbd = stats["class_by_dll"][dll]
    lines.append(f"| {dll} | {stats['dll_counts'][dll]} | {cbd.get('DIRECT',0)} | {cbd.get('WRAP',0)} | "
                 f"{cbd.get('DETERMINISTIC',0)} | {cbd.get('SHIM',0)} |")
lines.append("")
lines.append(f"Callback-bearing imports (receive a function pointer into guest/Allegro code, or into "
             f"libpng's I/O shims): **{stats['callback_bearing_count']}** -- listed individually in the "
             "table below (callback column) and summarized in the COM/callback section.")
lines.append("")

# ---------------- full table ----------------
lines.append("## Full import table")
lines.append("")
lines.append("`callers` lists up to 4 distinct calling functions found in the disassembly, tagged by "
             "origin (`game` = icytower's own source under `F:\\projects\\icytower\\trunk\\source\\`, "
             "`allegro` = statically-linked Allegro 4.4 under `C:\\Lib\\allegro4\\`, `crt` = MinGW/"
             "mingwex/libgcc CRT glue, `vorbis/ogg` = statically-linked libvorbis/libogg). All caller "
             "attributions are KNOWN (read from the disassembly + DWARF/COFF symbol tables), not inferred.")
lines.append("")
lines.append("| dll | import | class | cb? | #sites | callers | rationale |")
lines.append("|---|---|---|:-:|---:|---|---|")
for r in recs_sorted:
    callers_str = "; ".join(f"{esc(c['func'])} ({origin_tag(c['origin'])})" for c in r["callers"][:4])
    if not callers_str:
        callers_str = "_(no static call site found)_"
    cb = "Y" if r["callback"] else ""
    n_sites = r["num_callsites"] + r["num_data_refs"]
    lines.append(f"| {r['dll']} | {r['name']} | {r['class']} | {cb} | {n_sites} | {callers_str} | {esc(r['rationale'])} |")
lines.append("")

# ---------------- game surface ----------------
game = [r for r in recs_sorted if "GAME" in r["origins"]]
lines.append("## The game's true OS surface (imports called directly from GAME code, not just Allegro/CRT/libvorbis)")
lines.append("")
lines.append(f"KNOWN, from disassembly: **{len(game)}** of {len(recs)} imports have at least one call "
             "site or direct data reference inside icytower's own source (`F:\\projects\\icytower\\trunk\\"
             "source\\*.c`), as opposed to only being reached through Allegro's platform-abstraction layer, "
             "the CRT startup glue, or the statically-linked libvorbis/libogg codec. This is the surface "
             "a carrier must get *exactly right*, since it is code the game itself wrote, not "
             "third-party plumbing that can be swapped wholesale for a modern equivalent.")
lines.append("")
by_dll_game = defaultdict(list)
for r in game:
    by_dll_game[r["dll"]].append(r)
for dll in sorted(by_dll_game, key=str.lower):
    names = ", ".join(f"`{r['name']}`" for r in sorted(by_dll_game[dll], key=lambda x: x["name"].lower()))
    lines.append(f"- **{dll}** ({len(by_dll_game[dll])}): {names}")
lines.append("")
lines.append("Notes:")
lines.append("- `libpng3.dll` (35 of the 102) is a bundled third-party DLL, not a Windows OS component -- "
             "it inflates this count but isn't part of the \"forward to host Windows\" surface at all; "
             "it should just keep loading as a normal DLL dependency.")
lines.append("- Excluding libpng3.dll, the game's real Win32 OS surface is **67 imports**: 3x KERNEL32 "
             "(`LoadLibraryA`, `QueryPerformanceCounter`, `QueryPerformanceFrequency`), 1x SHELL32 "
             "(`ShellExecuteA`), 1x USER32 (`LoadCursorA`), 10x WSOCK32 (all of it, the ad-fetch HTTP "
             "client), 49x msvcrt.dll (file I/O, CRT/heap/string/math/time, `rand`/`srand`, `qsort`), "
             "and 3x pthreadGC2.dll (the ad-fetch worker thread).")
lines.append("- The single most consequential entries here for a *deterministic* carrier are "
             "`QueryPerformanceCounter` (core game-loop timing, called from `_play` in main.c) and "
             "`rand`/`srand` (per-run gameplay RNG, called from `_new_game`/`_init_game`/`_create_post`) "
             "-- see the Determinism Boundary section below.")
lines.append("")

# ---------------- COM boundary ----------------
lines.append("## COM boundary (not visible in the static import table)")
lines.append("")
lines.append("**KNOWN, structural point:** `DirectDrawCreate`, `DirectInputCreateA`, and `DirectSoundCreate` "
             "are only the *factory* calls -- they appear once each in the import table and are each "
             "called from exactly one place in Allegro's driver init code. Every actual frame of work "
             "(surface blit/lock, sound-buffer write, keyboard/mouse/joystick poll) happens through "
             "**virtual-call dispatch on the COM object these factories return**, which is *not* an "
             "import and therefore invisible to any import-table census, static or dynamic-loader-based. "
             "A carrier that only forwards the import table will correctly create the DirectX objects but "
             "will not see (and cannot intercept at the import level) any of the real per-frame traffic "
             "through them.")
lines.append("")
lines.append("**INFERRED (estimated) method identity**, from scanning `call *OFFSET(%reg)` instructions "
             "(the standard 32-bit MSVC/MinGW COM vtable-call idiom) inside the specific Allegro source "
             "files that own each COM interface, and matching the byte offsets against the well-known "
             "IUnknown-prefixed (QueryInterface=0x0, AddRef=0x4, Release=0x8) vtable layouts of "
             "`IDirectDraw2`, `IDirectDrawSurface`, `IDirectSound`, `IDirectSoundBuffer`, "
             "`IDirectSoundCaptureBuffer`, and `IDirectInputDeviceA`/`IDirectInputDevice2A` from the "
             "classic DirectX 7-era SDK headers:")
lines.append("")

byfile = defaultdict(list)
for h in vtbl:
    byfile[h["file"]].append(h)

# Known offset->name maps (INFERRED from standard DX7-era vtables)
IDIRECTDRAW2 = {
    "0xc":"Compact","0x10":"CreateClipper","0x14":"CreatePalette","0x18":"CreateSurface",
    "0x1c":"DuplicateSurface","0x20":"EnumDisplayModes","0x24":"EnumSurfaces",
    "0x28":"FlipToGDISurface","0x2c":"GetCaps","0x30":"GetDisplayMode","0x34":"GetFourCCCodes",
    "0x38":"GetGDISurface","0x3c":"GetMonitorFrequency","0x40":"GetScanLine",
    "0x44":"GetVerticalBlankStatus","0x48":"Initialize","0x4c":"RestoreDisplayMode",
    "0x50":"SetCooperativeLevel","0x54":"SetDisplayMode","0x58":"WaitForVerticalBlank",
    "0x5c":"GetAvailableVidMem",
}
IDIRECTDRAWSURFACE = {
    "0xc":"AddAttachedSurface","0x10":"AddOverlayDirtyRect","0x14":"Blt","0x18":"BltBatch",
    "0x1c":"BltFast","0x20":"DeleteAttachedSurface","0x24":"EnumAttachedSurfaces",
    "0x28":"EnumOverlayZOrders","0x2c":"Flip","0x30":"GetAttachedSurface","0x34":"GetBltStatus",
    "0x38":"GetCaps","0x3c":"GetClipper","0x40":"GetColorKey","0x44":"GetDC",
    "0x48":"GetFlipStatus","0x4c":"GetOverlayPosition","0x50":"GetPalette",
    "0x54":"GetPixelFormat","0x58":"GetSurfaceDesc","0x5c":"Initialize","0x60":"IsLost",
    "0x64":"Lock","0x68":"ReleaseDC","0x6c":"Restore","0x70":"SetClipper","0x74":"SetColorKey",
    "0x78":"SetOverlayPosition","0x7c":"SetPalette","0x80":"Unlock","0x84":"UpdateOverlay",
    "0x88":"UpdateOverlayDisplay","0x8c":"UpdateOverlayZOrder",
}
IDIRECTSOUND = {
    "0xc":"CreateSoundBuffer","0x10":"GetCaps","0x14":"DuplicateSoundBuffer",
    "0x18":"SetCooperativeLevel","0x1c":"Compact","0x20":"GetSpeakerConfig",
    "0x24":"SetSpeakerConfig","0x28":"Initialize",
}
IDIRECTSOUNDBUFFER = {
    "0xc":"GetCaps","0x10":"GetCurrentPosition","0x14":"GetFormat","0x18":"GetVolume",
    "0x1c":"GetPan","0x20":"GetFrequency","0x24":"GetStatus","0x28":"Initialize","0x2c":"Lock",
    "0x30":"Play","0x34":"SetCurrentPosition","0x38":"SetFormat","0x3c":"SetVolume",
    "0x40":"SetPan","0x44":"SetFrequency","0x48":"Stop","0x4c":"Unlock","0x50":"Restore",
}
IDIRECTSOUNDCAPTUREBUFFER = {
    "0xc":"GetCaps","0x10":"GetCurrentPosition","0x14":"GetFormat","0x18":"GetStatus",
    "0x1c":"Initialize","0x20":"Lock","0x24":"Start","0x28":"Stop","0x2c":"Unlock",
}
IDIRECTINPUTDEVICE = {
    "0xc":"GetCapabilities","0x10":"EnumObjects","0x14":"GetProperty","0x18":"SetProperty",
    "0x1c":"Acquire","0x20":"Unacquire","0x24":"GetDeviceState","0x28":"GetDeviceData",
    "0x2c":"SetDataFormat","0x30":"SetEventNotification","0x34":"SetCooperativeLevel",
    "0x38":"GetObjectInfo","0x3c":"GetDeviceInfo","0x40":"RunControlPanel","0x44":"Initialize",
    "0x48":"CreateEffect(IDirectInputDevice2)","0x4c":"EnumEffects(IDirectInputDevice2)",
    "0x50":"GetEffectInfo(IDirectInputDevice2)","0x54":"GetForceFeedbackState(IDirectInputDevice2)",
    "0x58":"SendForceFeedbackCommand(IDirectInputDevice2)",
    "0x5c":"EnumCreatedEffectObjects(IDirectInputDevice2)","0x60":"Escape(IDirectInputDevice2)",
    "0x64":"Poll(IDirectInputDevice2)",
}

file_iface = {
    "wddraw.c": [("IDirectDraw2", IDIRECTDRAW2), ("IDirectDrawSurface (primary surf. palette)", IDIRECTDRAWSURFACE)],
    "wddaccel.c": [("IDirectDrawSurface", IDIRECTDRAWSURFACE)],
    "wddbmp.c": [("IDirectDrawSurface", IDIRECTDRAWSURFACE)],
    "wddbmpl.c": [("IDirectDrawSurface", IDIRECTDRAWSURFACE)],
    "wddfull.c": [("IDirectDrawSurface", IDIRECTDRAWSURFACE)],
    "wddwin.c": [("IDirectDrawSurface", IDIRECTDRAWSURFACE)],
    "wddovl.c": [("IDirectDrawSurface (overlay)", IDIRECTDRAWSURFACE)],
    "wddmode.c": [("IDirectDrawSurface", IDIRECTDRAWSURFACE)],
    "wddlock.c": [("IDirectDrawSurface", IDIRECTDRAWSURFACE)],
    "wdsound.c": [("IDirectSound / IDirectSoundBuffer", IDIRECTSOUNDBUFFER)],
    "wdsndmix.c": [("IDirectSoundBuffer (software mixer)", IDIRECTSOUNDBUFFER)],
    "wdsinput.c": [("IDirectSoundCaptureBuffer", IDIRECTSOUNDCAPTUREBUFFER)],
    "wkeybd.c": [("IDirectInputDeviceA (keyboard)", IDIRECTINPUTDEVICE)],
    "wmouse.c": [("IDirectInputDeviceA (mouse)", IDIRECTINPUTDEVICE)],
    "wjoydx.c": [("IDirectInputDevice(2)A (joystick)", IDIRECTINPUTDEVICE)],
    "winput.c": [("IDirectInputDeviceA (generic)", IDIRECTINPUTDEVICE)],
}

lines.append("| Allegro source file | inferred interface | vtable-call sites found | offsets -> inferred method |")
lines.append("|---|---|---:|---|")
for fname in ["wddraw.c","wddaccel.c","wddbmp.c","wddbmpl.c","wddfull.c","wddwin.c","wddovl.c",
              "wddmode.c","wddlock.c","wdsound.c","wdsndmix.c","wdsinput.c","wkeybd.c","wmouse.c",
              "wjoydx.c","winput.c"]:
    hits = byfile.get(fname, [])
    if not hits:
        lines.append(f"| {fname} | _(no offset-call pattern found)_ | 0 | |")
        continue
    offs = sorted(set(h["offset"] for h in hits), key=lambda x: int(x,16))
    iface_list = file_iface.get(fname, [])
    mapped = []
    for off in offs:
        if off == "0x8":
            mapped.append("0x8=Release(IUnknown)")
            continue
        name = None
        for _, table in iface_list:
            if off in table:
                name = table[off]
                break
        mapped.append(f"{off}={name or '?'}")
    iface_names = " / ".join(i[0] for i in iface_list)
    lines.append(f"| {fname} | {iface_names} | {len(hits)} | {', '.join(mapped)} |")
lines.append("")
lines.append("Caveats (all of the above vtable-offset mapping is **INFERRED**, not verified against a "
             "known-good DirectX header for this exact build): offsets are read straight off "
             "`call *0xNN(%eax)`-style instructions in the disassembly (KNOWN), but the method name at "
             "each offset is my recollection of the classic (non-COM-idl-regenerated) DirectX 7 SDK "
             "vtable layout, not cross-checked against `dxguid`/`ddraw.h` structure definitions from "
             "this codebase. The wjoydx.c offset `0x64` (`Escape`) implies `IDirectInputDevice2A`, not "
             "the plain `IDirectInputDeviceA` used by keyboard/mouse -- plausible since force-feedback/"
             "vendor-escape calls are joystick-specific, but unverified. `IDirectDrawClipper` and "
             "`IDirectDrawPalette` objects are created (`CreateClipper`/`CreatePalette`, both visible as "
             "`IDirectDraw2` calls above) but their own small vtables were not separately identified as "
             "distinct call clusters in this pass.")
lines.append("")
lines.append("Practical implication for the carrier: forwarding the 4 SHIM factory imports "
             "(`DirectDrawCreate`, `DirectInputCreateA`, `DirectSoundCreate`, and indirectly "
             "`DirectSoundEnumerateA`) is necessary but nowhere near sufficient -- the carrier must also "
             "either (a) forward the *entire* COM object (proxy every vtable slot to the real host "
             "DirectX/cnc-ddraw implementation), or (b) replace the object with a compatible "
             "reimplementation exposing the ~15-45 methods per interface enumerated above.")
lines.append("")

# ---------------- determinism boundary ----------------
det = [r for r in recs_sorted if r["class"] == "DETERMINISTIC"]
lines.append("## Determinism boundary: minimal interception set for replayable GAME logic")
lines.append("")
lines.append("Goal: the smallest set of interception points such that the *game's own logic* -- as "
             "opposed to Allegro's plumbing, which can be left non-deterministic as long as it still "
             "delivers the same eventual results to the game -- becomes replayable.")
lines.append("")
lines.append("**KNOWN, from disassembly (GAME code calls these Win32/CRT imports directly):**")
lines.append("")
lines.append("1. **`KERNEL32.dll!QueryPerformanceCounter`** -- called directly from `_play` (main.c), "
             "the core game loop, 9 static call sites split between GAME and Allegro. This is the frame/"
             "physics clock. **Must be virtualized to a recorded/replayed logical clock** for the game's "
             "own per-frame delta-time computation to be reproducible; Allegro's separate use of it "
             "(wtimer.c) can ride along on the same virtual clock or be left alone since it only affects "
             "audio mixing cadence, not game state.")
lines.append("2. **`msvcrt.dll!rand` / `msvcrt.dll!srand`** -- called directly from `_new_game`/"
             "`_init_game` (main.c) and `_create_post` (beta.c). This is the game's own gameplay PRNG "
             "(msvcrt's LCG, not a game-authored one). **Must intercept `srand`'s seed value (record it) "
             "and either forward to the exact same msvcrt `rand()` algorithm on every host, or replace "
             "both with a bundled, version-pinned PRNG** so replay does not depend on which Windows "
             "version's msvcrt.dll happens to be forwarded to.")
lines.append("3. **Raw input sampling** (`USER32.dll!GetAsyncKeyState`, `GetKeyboardState`, "
             "`GetCursorPos`, `SetCursorPos`; `WINMM.DLL!joyGetPosEx`) -- KNOWN: none of these are "
             "called by GAME code directly; all are Allegro driver internals (wkeybd.c/wmouse.c/"
             "wjoydx.c). Nonetheless they are the literal raw-input sample points feeding Allegro's "
             "abstracted key/mouse/joystick state, which GAME code (player.c/control.c) then reads as "
             "pure (deterministic) memory. **Recording/replaying at this exact boundary is sufficient** "
             "-- there is no need to go lower (e.g. into DirectInput's `IDirectInputDeviceA::"
             "GetDeviceData`/`GetDeviceState` vtable calls, see COM section) unless Allegro's own "
             "internal event ordering/timing between WM_* messages and these polls turns out to matter.")
lines.append("4. **Message-pump gating** (`USER32.dll!GetMessageA`/`PeekMessageA`/"
             "`MsgWaitForMultipleObjects`) -- Allegro plumbing that paces the main loop and delivers "
             "WM_KEYDOWN/WM_SIZE/WM_TIMER etc. Needed only if replay must reproduce exact frame pacing "
             "under contention (e.g. window defocus, OS-level message storms); for pure logic-replay "
             "given a recorded input+clock+RNG stream, this layer does not need separate control.")
lines.append("5. **`WSOCK32.DLL` (all 10 imports) and `pthreadGC2.dll!pthread_create`** -- KNOWN: sole "
             "static caller is `_HTTPFetchInternal`/`_fldads_start` (httpget.c/fld_adspot.c), the "
             "ad-banner subsystem. **Not part of core gameplay determinism** -- hisc.c's high-score "
             "logic has no network calls in this census. Recommended treatment: stub/sandbox the entire "
             "ad-fetch path for replay/testing rather than trying to make live network I/O deterministic.")
lines.append("6. **`msvcrt.dll!time`/`clock`/`localtime`/`gmtime`/`mktime`** -- used for timestamping "
             "(save/profile/hiscore/replay metadata), not for per-frame logic; needs virtualizing for "
             "byte-identical save files but not for gameplay replay correctness per se.")
lines.append("")
lines.append("**Conclusion (INFERRED synthesis of the above KNOWN facts):** the minimal interception set "
             "for *replayable game logic* is exactly **{QueryPerformanceCounter, srand-seed-capture, "
             "rand-algorithm-pinning, raw keyboard/mouse/joystick input sampling}** -- 3 Win32 imports "
             "plus the 2 CRT RNG imports plus a version-pinning decision, five to six intercepts in "
             "total. Everything else in the DETERMINISTIC class (Sleep, WaitFor*, SetTimer, "
             "_beginthread/pthread_create, the message pump, network I/O) governs Allegro's internal "
             "plumbing or the non-gameplay ad subsystem and can be left to run non-deterministically "
             "(wall-clock real time, real thread scheduling, real network) as long as the four items "
             "above are pinned, since none of that plumbing's timing is read back into game state -- "
             "it only decides *when* the next deterministic frame gets computed, not *what* it computes.")
lines.append("")
lines.append("Full DETERMINISTIC-class list for reference:")
lines.append("")
for r in det:
    lines.append(f"- `{r['dll']}!{r['name']}`" + (" (callback-bearing)" if r["callback"] else "") +
                 f" -- origins: {', '.join(origin_tag(o) for o in r['origins']) or 'none found'}")
lines.append("")

lines.append("## GetProcAddress caveat (dynamic-linking blind spot)")
lines.append("")
lines.append("**KNOWN:** `KERNEL32.dll!GetProcAddress` is itself imported (1 slot, WRAP class). "
             "**INFERRED:** whatever the guest resolves through it at runtime is by definition invisible "
             "to this or any static import census -- a carrier that only proxies the statically-declared "
             "IAT will miss any late-bound optional-API usage. No evidence was collected in this pass "
             "about which symbols (if any) are actually looked up through it; recommend a runtime "
             "hook/log on `GetProcAddress` before considering the import surface fully characterized.")
lines.append("")

md = "\n".join(lines)
open(ROOT + r"\notes\import_census.md", "w", encoding="utf-8").write(md)
print("wrote notes/import_census.md,", len(md), "bytes,", len(lines), "lines")
