# Win32 carrier extraction plan — unit inventory, target layout, stages

Read-only inventory taken 2026-09-07 against `carrier/src` (7 403 lines),
`carrier/gen` (5 405), `carrier/lift` + `carrier/lift/harness` (~5 200 lines
of Python/C, excluding vector `.bin` fixtures), `carrier/scripts` +
`scripts/` (~2 300), and port_forge on `experimental/win32` (65ec092).
Nothing was run and no carrier code was modified.

Vocabulary: **GENERIC** = no Icy Tower fact in the unit at all;
**GENERIC-WITH-POLICY-INPUT** = the mechanism is target-independent but today
reads an Icy Tower constant it must instead receive; **ICY** = the fact
itself, which stays in this repository as data.

## 1. Unit table

### 1a. `carrier/src` — the carrier core (7 403 lines)

| unit | lines | class | Icy Tower assumptions embedded TODAY | becomes |
|---|---|---|---|---|
| `pe_image.hpp/.cpp` | 223 | GENERIC | header comment only ("no `.reloc`, no TLS"); the code re-derives base/size from the file. The *caller* passes the constants. | `GuestImagePolicy{ image_base, size_of_image, require_fixed_base, apply_relocations }`. CyberStorm has 21 926 relocs, so `require_fixed_base` must become a field, not a comment. |
| `imports.hpp`, `import_types.hpp`, `imports.cpp` | 180 | GENERIC-WITH-POLICY-INPUT | `needs_assets_path()` hardcodes `libpng3.dll`/`pthreadGC2.dll`; `DDRAW.dll` + `DDrawMode::Local` is the cnc-ddraw path policy; `kWrapNames[]` is a 28-name literal; DLL key is case-sensitive. | `SidecarDllPolicy{ {dll, Source::AssetsDir\|System\|AbsolutePath} [] }` (covers cnc-ddraw, libpng3, pthreadGC2, CyberStorm's `_INMM.dll`) + `WrapPolicy{ names[] }` supplied by the project's wrapper table. Key on `lowercase(dll)`. |
| `symbols.hpp/.cpp` | 126 | GENERIC | path `artifacts/functions.json` is composed by `main.cpp`, not here. | unchanged; takes a path. |
| `trace.hpp/.cpp` | 276 | GENERIC | `pf_on_import`'s two arg-decoding special cases (`MessageBoxA`, `_chdir`/`fopen`) are Win32-generic, not Icy. | unchanged. |
| `wrappers.hpp/.cpp` | 200 | GENERIC-WITH-POLICY-INPUT | `g_guest_path` default `"icytower15.exe"`; the `GetModuleFileNameA(carrier_hmodule)` equivalence and `GetCommandLineA` substitution are *generic guest-identity policy*, discovered on Icy Tower but true of any mapped-not-loaded guest. `wrappers_lookup`'s 28-entry `strcmp` chain mixes generic (exit path, identity, time) with det-owned names. | `GuestIdentityPolicy{ image_path, argv0, treat_carrier_hmodule_as_main }` + a `WrapperTable{ name → fn }` the project fills. Exit path (`ExitProcess/exit/_cexit/abort`), identity (2), time/QPC/Sleep/clock (5), heap (4), RNG (2), `WaitForSingleObject` are all GENERIC. |
| `main.cpp` — bootstrap | ~640 | GENERIC-WITH-POLICY-INPUT | `GUEST_IMAGE_BASE_HINT 0x400000`, `GUEST_IMAGE_SIZE_HINT 0x38c000`, `GUEST_STACK_BASE 0x0e000000`, `GUEST_STACK_SIZE 2 MiB`, default image `<root>\assets\icytower15.exe`, `<root>\artifacts\functions.json`. The `relaunch_as_reserved_child` create-suspended/`VirtualAllocEx`/resume trick, `pf_launch_guest_fixed`'s TEB `fs:[4]/fs:[8]` swap, the `PF_*` env transport and the VEH+EBP stack walk are all pure mechanism. | `GuestImagePolicy` + `GuestStackPolicy{ va, size }` + `PathsPolicy{ image, symbols_json }`. Everything else is `pf::win32::Bootstrap`. |
| `main.cpp` — `Options`/`parse_args`/env transport | ~300 | GENERIC | 45 flags, none named after Icy Tower. | `pf::win32::CarrierOptions` verbatim; the project's `main.cpp` keeps only flag→policy wiring. |
| `det.cpp/.hpp` — arena (first-fit + coalescing, `0x20000000`) | ~330 | GENERIC-WITH-POLICY-INPUT | `ARENA_BASE 0x20000000`, `ARENA_SIZE 256 MiB`. | `ArenaPolicy{ base_va, size, align }`. Why it exists (msvcrt heap base is per-process randomised, pointers get baked into `.bss`) is target-independent. |
| `det.cpp` — RNG pinning + `--rng-selftest` | ~90 | GENERIC-WITH-POLICY-INPUT | the pinned LCG *is* msvcrt's; selftest compares against the resolved real `rand`. | `RngPolicy{ kind = MsvcrtLcg\|WatcomLcg\|Custom, seed_default }`. Both projects link msvcrt-family CRTs, so the msvcrt LCG stays the shipped default. |
| `det.cpp` — DR0–DR3 breakpoint table, `det_veh_handler`, RF single-step-over, `det_ctx_arm_slot` | ~250 | GENERIC | none. Already documented as generic in NOTES.md ("Milestones 5-7 part C"); `headless.cpp`, `frame.cpp`, `bind.cpp` and `snapshot.cpp` are already four independent consumers. | `pf::win32::BreakpointTable` — moves as-is. **This is the single highest-value unit in the extraction.** |
| `det.cpp` — virtual clock + tick pump (`det_wrap_Sleep`) | ~200 | GENERIC-WITH-POLICY-INPUT | `VA_HANDLE_TIMER_TICK 0x45d6c8`, `TIMERS_PER_SECOND 1193181` (Allegro's PIT convention), `tick = virtual_ms/20`, `DET_VIRTUAL_EPOCH`, `IT_CYCLE_COUNT (*(int*)0x506938)`, and "`Sleep` on the main thread is the pump" (true because Allegro's idle loops call `rest(1)`). | `TickPolicy{ tick_fn_va, tick_arg_kind = AccumulatedUnits\|Interval\|None, units_per_second, tick_divisor_ms, pump = Import("Sleep", main_thread_only)\|Safepoint, progress_probe_va }`. The accumulator shape (running total, diff per call, no drift) is generic — see §4. |
| `det.cpp` — thread virtualization / parking | ~230 | GENERIC-WITH-POLICY-INPUT | `VA_TIM_HIGH_PERF_THREAD 0x478584`, `VA_TIM_LOW_PERF_THREAD 0x4783bc`, `VA_INPUT_THREAD_PROC 0x479a40`, `VA_FLDADS_THREADMAIN 0x404014`. | `ThreadPolicy{ park_real[]{entry_va}, virtualize_stub[]{entry_va}, suppress[]{entry_va} }`. `det_wrap_beginthread`/`det_wrap_pthread_create` dispatch on the list; `det_wrap_WaitForSingleObject`'s finite→INFINITE substitution for registered parked threads is pure mechanism. |
| `det.cpp` — input script parse, tick-indexed queue, `deliver_due_input`, sub-tick slot | ~380 | GENERIC-WITH-POLICY-INPUT | `kKeyNames[]` is the Allegro `KEY_*` enum; `VA_HANDLE_KEY_PRESS 0x43e2f8`, `VA_HANDLE_KEY_RELEASE 0x43d8d4`; `g_allegro_to_dik`/`dik_to_allegro` built from `VA_HW_TO_MYCODE 0x4daf80`. | `InputBindingPolicy{ deliver_press_va, deliver_release_va, deliver_abi = Cdecl2\|Cdecl1\|Fastcall, capture_va, capture_abi, scancode_map_va, key_names[]{name,code} }`. The *shape* — "capture at guest function X, deliver at tick boundary via guest function Y" — is exactly the generic statement; only X, Y and the code table are data. |
| `det.cpp` — real-key capture, ring buffer, `drain_real_key_queue`, exit-storm collapse, `--record-input`, `--trace-input` | ~420 | GENERIC-WITH-POLICY-INPUT | reaches through `VA_KEY_DINPUT_SCANCODE 0x46d5a8` (register-passed args: `al`=scancode) and `hw_to_mycode`. | same `InputBindingPolicy`; the ring buffer, hygiene filter, violation counter and the divergence-005 ordering invariant (deliver **then** drain, at the same point in both providers) move verbatim. |
| `det.cpp` — window/focus policy wrappers (`ShowWindow`, `SetForegroundWindow`, `SetWindowPos`, `CreateWindowExA`), `--interactive`, `WindowMode` | ~200 | GENERIC-WITH-POLICY-INPUT | `focus_enum_proc_any` matches window class `"AllegroWindow"`. | `WindowPolicy{ guest_window_class, mode, interactive }`. The "an automated run never takes the operator's foreground" rule is framework policy, not Icy policy. |
| `det.cpp` — switch-in/out channel (entry patches + tick-boundary queue) | ~180 | GENERIC-WITH-POLICY-INPUT | `VA_SWITCH_IN 0x4657e4`, `VA_SWITCH_OUT 0x465808`, `VA_SWITCH_IN_CB 0x4ea080`, `VA_SWITCH_OUT_CB 0x4ea060`, `VA_WIN_SWITCH_IN/_OUT`, callback array width 8. | `FocusChannelPolicy{ switch_in_va, switch_out_va, cb_table_in_va, cb_table_out_va, cb_table_len, real_path_in_va, real_path_out_va }`. `patch_entry_jmp` + `det_stub_*` is generic. |
| `det.cpp` — mouse parking | ~30 | ICY (one address) | `VA_HANDLE_MOUSE_INPUT 0x45f9bc`. | folds into `ThreadPolicy.suppress[]` / a `NeutralizePolicy{ va[] }` — "write `ret` at a void(void) entry" is one generic operation. |
| `det.cpp` — `DetSavedState` save/load, `det_environment_json`, report accessors, perturbation knobs | ~270 | GENERIC | none (queue capacities are its own). | moves as-is. |
| `bind.hpp/.cpp` | 958 | GENERIC-WITH-POLICY-INPUT | includes `../gen/bind_table.inc` (generated); `kBindMaxFns = 42` pinned to this project's table; header comments cite `src/icytower/*.c`; the ORIGINAL-form DR sensing, the 5-byte `jmp rel32` entry patch, the one stub template (`BIND_STUB(N)`), the per-invocation record shape and `--fault-inject` carry no Icy fact. | the **generated table** is the policy: `bind_table.inc` rows `{name, va, argc, returns_value, lifted_fn, native_fn, src_fn, domain_fn, fault_addr_fn}` stay in the project's `carrier/gen/`; `kBindMaxFns` becomes a template/`constexpr` parameter taken from the generated `kNumFns`. |
| `snapshot.hpp/.cpp` | 697 | GENERIC-WITH-POLICY-INPUT | `PF_GUEST_DATA_VA/SIZE`, `PF_GUEST_BSS_VA/SIZE`, `PF_GUEST_ARENA_VA`, `PF_GUEST_STACK_VA/SZ` (from `det.hpp`); manifest literal `"safepoint_va": "0x004124f4"`; image identity hashes `[0x400000, data_va)`; `--restore-fault` flips `reward_scale` at `0x4fac28`; ESP measured constant `0x0e1fef30`. | `SnapshotDomainPolicy{ regions[]{name, va, size, capture = Full\|LiveRange}, image_identity_range, safepoint_va, fault_probe_va }`. The codec, the `portforge-win32-carrier-snapshot-v1` manifest, `suspend_other_threads`, the pre/post ordering at the safepoint and the certification contract are all GENERIC. |
| `frame.hpp/.cpp` | 291 | GENERIC-WITH-POLICY-INPUT | `VA_BLIT_TO_SCREEN 0x40b6bc`; `BM_OFF_W/H/VTABLE/LINE` and `VT_OFF_COLOR_DEPTH` are Allegro `BITMAP` offsets; `get_palette` at `0x44c47c`. | `FrameOraclePolicy{ present_fn_va, bitmap_arg_index, layout{w_off, h_off, vtable_off, line_array_off, color_depth_off}, palette_fn_va }`. The "hash the source surface row-by-row above the backend" claim is the generic part; the PPM writer is generic. |
| `headless.hpp/.cpp` | 162 | ICY (mechanism generic) | `VA_SET_GFX_MODE 0x450688`, `VA_INSTALL_SOUND 0x4417b0`, `PF_GFX_GDI 0x47444942`, `DIGI_NONE/MIDI_NONE`, 640×480, cdecl arg slots 1..3. | the *mechanism* — "at guest VA X, before the prologue, overwrite cdecl arg slot i with value v, then RF-step over" — becomes `pf::win32::ArgumentSensor` driven by `ArgSensorPolicy{ {va, {slot, value}[] , label} [] }`. `--headless`/`--no-sound` become two rows of project data, not two C++ files. |
| `print_globals.hpp/.cpp` | 408 | GENERIC | none — the header states it explicitly, and `scripts/play.py` supplies `score,floor,combo` as data. Includes the generated `../gen/it_print_globals.inc`. | moves as-is; the generated table is the policy input. **The reference example of what "generated table, no invented API" looks like.** |

**`carrier/src` split**: GENERIC ≈ **1 200** lines (pe_image, symbols, trace,
print_globals, the option/env layer, `DetSavedState`/report);
GENERIC-WITH-POLICY-INPUT ≈ **5 350**; ICY-only C++ ≈ **850** (the VA `#define`
blocks, the Allegro key/`hw_to_mycode` tables, `headless.cpp`'s constants,
the ddraw/sidecar path list, the `BITMAP`/`GFX_VTABLE` offsets, the ad thread).

### 1b. `carrier/gen` — generators (5 405 lines)

| unit | lines | class | Icy assumptions | becomes |
|---|---|---|---|---|
| `gen_imports.py` | 84 | GENERIC | reads `imports.json` (`[dll, name, iat_va]`). | `tools/pf_win32_gen_imports.py`. Fix the case-sensitive DLL key (CyberStorm has `KERNEL32.dll` **and** `KERNEL32.DLL`). |
| `gen_interop.py` | 1 379 | GENERIC | `--scope game` = CUs under `F:\projects\icytower\trunk\source\`. | `tools/pf_win32_gen_interop.py`; the scope prefix is already a CLI argument → `--cu-prefix`. |
| `gen_bindings.py` | 512 | GENERIC | none (sits on `interop_index.json`). | `tools/pf_win32_gen_bindings.py`. Carries divergence 008's rule: emit indirect calls through the guest's own IAT slot for every CRT name clean code uses. |
| `gen_lib_bindings.py` | 919 | GENERIC-WITH-POLICY-INPUT | consumes `artifacts/lib_boundary.json`'s Allegro allow-list (100 functions, 26 globals, `_win_hcursor`, `logg_*`). | `tools/pf_win32_gen_lib_bindings.py`; the allow-list is the project's census file. |
| `gen_src_headers.py` | 815 | GENERIC | the game/library type-origin split is computed, not listed. | `tools/pf_win32_gen_src_headers.py`. |
| `gen_bind_table.py` | 308 | GENERIC-WITH-POLICY-INPUT | scans `src/icytower/*.c`; parses `carrier/build.cmd`'s command line for linked `lifted_`/`native_` forms; reads hand-curated `gen/fn_domains.json`. | `tools/pf_win32_gen_bind_table.py` with `--src-dir`, `--link-manifest`, `--domains`. Parsing a `.cmd` file must become reading a build manifest. |
| `gen_print_globals.py` | 265 | GENERIC | none. | `tools/pf_win32_gen_print_globals.py`. |
| `gen_game_globals.py` | 112 | GENERIC-WITH-POLICY-INPUT | "game-owned" = DWARF CU scope; needs `coff_symbols.json` for sizes. | `tools/pf_win32_gen_digest_domain.py` with `--ownership dwarf-cu\|section\|list`. See §4. |
| `scan_src_defs.py` | 194 | GENERIC | `src/icytower/*.c`, excludes `state.c`. | `tools/pf_win32_scan_src_defs.py`, `--src-dir`/`--exclude`. |
| `gen_assets.py`, `check_assets.py` | 817 | ICY | Allegro datafile format, `CHAR_SLOT` names, `data[N].dat`/`sfx[N].dat` families, the 7 computed-index sites, the three passworded datafiles. | stays in this repo. Only the *manifest schema* (`schemas/portforge-asset-manifest-v1`) is a framework artifact. |

**`carrier/gen` split**: GENERIC ≈ **3 360**, POLICY-INPUT ≈ **1 230**, ICY ≈ **815**.

### 1c. Lifter, harness, scripts

| unit | lines | class | Icy assumptions | becomes |
|---|---|---|---|---|
| `lift/pf_lift.py` | 2 261 | GENERIC | reads the interop header for the prototype at a VA; `PF_MEM()` identity default. | `tools/pf_win32_lift.py`. |
| `lift/lifted/pf_rt.h`, `pf_x87_soft.h` | — | GENERIC | none; the CW=0x037F/PC=11 rule is a *finding*, not a target fact. | `src/platform/win32/lift_rt.hpp`, `x87_soft.hpp`. |
| `lift/harness/lift_check.py` | 1 550 | GENERIC-WITH-POLICY-INPUT | the `SPECS` dict (per-function vector shapes) is Icy data. | `tools/pf_win32_lift_check.py` + `--specs` JSON in the project. |
| `harness/src_check.c`, `gcc_check.c`, `native_check.c`, `lift_check.c`, `call_trace_stubs.c`, `harness_rand.c`, `pf_harness_*.h`, `x87_*` probes | ~1 400 | GENERIC | none. | `tools/win32_harness/` (or `src/platform/win32/harness/`). |
| `scripts/compare_digests.py` | 86 | GENERIC | none. | `tools/pf_win32_compare_digests.py`. |
| `scripts/compare_fn_digests.py` | 123 | GENERIC | none. | `tools/pf_win32_compare_fn_digests.py`. |
| `scripts/certify_snapshot.py` | 222 | GENERIC | none — implements pf/capsule §D's anchor contract. | `tools/pf_win32_certify_snapshot.py`. |
| `scripts/pf_inspect.py` | 527 | GENERIC-WITH-POLICY-INPUT | reads `interop_index.json`/`it_types.h`/`it_types_check.c`; snapshot format is generic. | `tools/pf_win32_inspect.py`, paths as arguments. **DWARF-dependent** — see §5. |
| `scripts/bind_all.py` | 188 | GENERIC-WITH-POLICY-INPUT | the DR-budget-one-ORIGINAL-per-run loop is mechanism; `replays/human_test.txt` and the 35-function list are data. | `tools/pf_win32_bind_all.py`, `--functions`/`--recording`. |
| `scripts/sendinput_session.py`, `desktop_perturb_test.py` | 777 | GENERIC-WITH-POLICY-INPUT | `"AllegroWindow"` class; scancode tables. | `tools/pf_win32_desktop_probe.py` + `WindowPolicy`. |
| `scripts/gates.ps1`, `restore_assets.ps1`, `*.txt` bindfiles/scripts | ~250 | ICY | the four gate command lines, `assets/tower.cfg`, `assets/profiles/`. | stays; the project declares its gate workflows (pf/74). |
| `scripts/play.py` | 355 | ICY | project contract; explicitly refuses `player_runtime.py` (no `portforge.project.json`, no `game.json`). | stays. See §4. |
| `scripts/check_native_layer.py` | 98 | GENERIC-WITH-POLICY-INPUT | `GUEST_RANGES` triple, the `it_*`/`IT_G_` banned-identifier list. | `tools/pf_win32_purity.py` with `--guest-range` (repeatable) and a `--banned-prefixes` file. |
| `tools_recon/*.py` | ~2 700 | ICY (recon) | Allegro datafile/DWARF/census tooling for this binary. | stays; `lib_boundary_scan.py` and `census_*` are candidates only after a second target proves the shape. |

## 2. Target layout in port_forge

### 2.1 `src/platform/win32/` — header-only, `namespace pf::win32`

Matching `src/platform/win16/`: every unit a `.hpp` with `inline`
functions/variables, one `pf_platform_win32` CMake INTERFACE library.
**No `.cpp` in `src/`, and no framework build target for the carrier.** The
carrier is the *project's* composition root — the same disposition
`config/build-targets-v2.json` already grants `tools/pf_main.cpp` et al.
(`"generated-project-root"`) — so `carrier/build.cmd` (32-bit MSVC + mingw32
GCC objects) remains the only thing that compiles these headers.

```
src/platform/win32/
  policy.hpp          every struct in §1 (GuestImagePolicy, SidecarDllPolicy,
                      TickPolicy, ThreadPolicy, InputBindingPolicy,
                      FocusChannelPolicy, WindowPolicy, ArgSensorPolicy,
                      SnapshotDomainPolicy, FrameOraclePolicy, ArenaPolicy,
                      RngPolicy, GuestIdentityPolicy, PathsPolicy)
  pe_image.hpp        map at ImageBase; reserve-before-first-instruction
  bootstrap.hpp       create-suspended/VirtualAllocEx/resume relaunch,
                      PF_* env transport, CarrierOptions, guest stack + TEB swap
  imports.hpp         DLL resolution by policy, IAT write, DIRECT/TRACE/WRAP
  trace.hpp           pf_import_common trampoline, counting, report JSON
  symbols.hpp         functions.json → name+offset
  diagnostics.hpp     VEH, EBP stack walk, crash dump
  breakpoints.hpp     the DR0-DR3 table, det_veh_handler, RF step-over,
                      det_ctx_arm_slot  (four consumers already)
  virtual_clock.hpp   accumulator + tick pump + Sleep/QPC/time/clock wrappers
  threads.hpp         park/virtualize/suppress by entry VA
  arena.hpp           fixed-address first-fit allocator
  rng.hpp             pinned LCG + selftest
  input_channel.hpp   capture-at-X / deliver-at-tick-boundary-via-Y, queues,
                      recording, the deliver-then-drain ordering invariant
  focus_channel.hpp   entry-patch neutralization + tick-boundary replay
  arg_sensor.hpp      rewrite cdecl arg slots at a breakpoint
  bind_engine.hpp     5-byte entry patch, one stub template, ORIGINAL sensing,
                      per-invocation record, fault injection
  snapshot.hpp        region codec, manifest v1, in-process restore, trace window
  frame_oracle.hpp    surface digest above the backend + PPM dump
  print_globals.hpp   expression evaluator over a generated typed table
  lift_rt.hpp, x87_soft.hpp
  harness/            offline oracle C sources (built by the project)
```

**Boundary lint** (`scripts/check_boundaries.py`): add
`"src/platform/win32": ["src/core", "src/replay", "src/hooks"]`.
`src/core` for `sha256.hpp` (already included today via a relative
`../../port_forge/...` path — that relative include disappears with the move),
`json.hpp`, `io.hpp`, `restore_transaction.hpp` (the snapshot write-back is
exactly its shape), `observe.hpp`; `src/replay` so `input_script.hpp` /
`input_delivery.hpp` can be adopted at S4 rather than re-copied. Both reach
only `src/core`, so there is no cycle. No `src/arch/x86_32` entry: the carrier
executes natively and never instantiates `Machine32` — and staying out keeps
the PM raw-memory-write lint irrelevant here.

**pf/76 compliance**: nothing in this list re-implements a declared framework
surface. `sha256`, `json`, `io`, `restore_transaction` are consumed, not
copied. Where a framework surface *does* exist (`src/replay/input_script.hpp`,
`input_delivery.hpp`, `artifact.hpp`), the plan adopts it at S4 and does not
grow a second one: `input_channel.hpp` owns the Win32 *capture/delivery
points*, never "is this event due".

### 2.2 Project side

```
carrier/win32_policy.hpp    ONE hand-written file: every policy struct above,
                            filled with Icy Tower's addresses and tables.
                            ~250 lines, all of it evidence-cited data.
carrier/gen/*.inc,*.h       generated tables (import_table, bind_table,
                            game_globals/digest domain, it_print_globals,
                            asset bindings) — unchanged, project-owned.
carrier/main.cpp            builds the policy, wires CarrierOptions, runs
                            pf::win32::Bootstrap. ~200 lines.
carrier/wrappers.cpp        only wrappers whose *behaviour* is project-chosen.
carrier/build.cmd           unchanged 32-bit MSVC + mingw32 link.
```

### 2.3 `tools/` homes

Every moved generator/lifter/script becomes `tools/pf_win32_<verb>.py` with a
module docstring whose first line is its purpose — `generate_tool_index.py`
reads exactly that and `--check` fails on a tool without one. Names from §1c.
The harness `.c` files carry `// PortForge - <purpose>` first lines.

### 2.4 The 32-bit MSVC build question

Recommendation: **do not register a carrier build target at all.**
`config/build-targets-v2.json` only enumerates composition roots under
`tools/` and `tests/`; the carrier is neither. `check_build_targets.py`
therefore stays green with zero changes, and `build.py`/CMake on a Linux or
64-bit-MinGW host never sees a 32-bit MSVC compiler.

If a framework-side Win32 tool binary is ever needed, extend
`build_target_registry.HOSTS` with `"windows-x86-msvc"` and
`host_supported()` with
`if host == "windows-x86-msvc": return os.name == "nt" and _msvc32_available()`
(a `vswhere`/`PF_MSVC32` probe), plus a CMake guard
`if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)`. That is a one-value extension of
an existing enum, not a new mechanism — but it is not needed by this plan and
should not land speculatively.

## 3. Stages

Gates for every stage: **project** — G1 `newgame` two-run digest EQUAL over
876 ticks; G2 `update_frame` src-vs-original per-invocation EQUAL over 877
ticks; G3 `bind_all` all-35-bound over `human_test` EQUAL at 2293 ticks;
snapshot rewind certification EQUAL (`certify_snapshot.py rewind --anchor 400`
+ `fn`); purity gate (`check_native_layer.py` on `src/`, exit 0); plus the
negative controls (`--fault-inject`, `--restore-fault`, `DET_PERTURB_TIME`).
**Framework** — `check_boundaries.py`, `check_build_targets.py`,
`generate_tool_index.py --check`, `check_doc_status.py`/`check_doc_links.py`,
`check_platform_registry.py`, and `tools/pf_gate_all.py` (pf/74: every sibling
project's gates against the same port_forge commit).

### S1 — generators to `tools/` (Python only, no runtime risk)

Move `gen_imports`, `gen_interop`, `gen_bindings`, `gen_lib_bindings`,
`gen_src_headers`, `gen_bind_table`, `gen_print_globals`, `gen_game_globals`→
`pf_win32_gen_digest_domain`, `scan_src_defs`, `check_native_layer` →
`tools/pf_win32_*.py`. `gen_assets`/`check_assets`/`tools_recon` stay.
*Policy interface introduced*: CLI arguments only — `--cu-prefix`,
`--src-dir`, `--link-manifest`, `--domains`, `--ownership`, `--guest-range`,
`--banned-prefixes`. No new file format.
*Project keeps*: thin `carrier/gen/gen_*.py` shims that `sys.path.insert` the
submodule `tools/` and call `main()` — so `build.cmd` is untouched in S1 and
becomes a direct call in S2.
*Risk*: **low**. Byte-identical regeneration is checkable.
*Verification*: regenerate every `carrier/gen/*` artifact and require a
byte-for-byte diff against the committed copy; then the full gate set (a
regenerated `bind_table.inc` or `game_globals.inc` that changed would move G1
and G3 immediately). `generate_tool_index.py --check` green.

### S2 — carrier core to `src/platform/win32/`

Move, in this order (each is independently gate-checkable): `pe_image` →
`symbols`+`trace`+`diagnostics` → `imports` → `breakpoints` → `arena`+`rng` →
`virtual_clock`+`threads` → `input_channel`+`focus_channel` → `arg_sensor` →
`bind_engine` → `snapshot` → `frame_oracle` → `print_globals` → `bootstrap`.
*Policy interface introduced*: `src/platform/win32/policy.hpp` (§2.1) and the
project's `carrier/win32_policy.hpp`. Wrappers split: **generic** = the exit
path (`ExitProcess/exit/_cexit/abort`), guest identity
(`GetModuleFileNameA`/`GetCommandLineA` under `GuestIdentityPolicy`), time
virtualization (`Sleep`/`QPC`/`timeGetTime`/`time`/`clock`), heap (`malloc`
family), `WaitForSingleObject`, `rand`/`srand`, and the four window calls;
**project** = which VAs the thread/input/focus policies name, and any wrapper
whose *semantics* are game-specific.
*Risk*: **high** — this is live code under a 5-byte-patch, hardware-breakpoint,
naked-asm carrier. Mitigations: one unit per commit; the `#include
"../../port_forge/src/core/sha256.hpp"` relative paths in `det.cpp`/`frame.cpp`
disappear (they are the current pf/76 smell); keep the deliver-then-drain
ordering and the `bind_init` → `det_install_entry_patches` → `headless_init` →
`frame_init` → `det_arm_main_thread` sequence byte-for-byte.
*Verification*: full gate set after **every** unit, plus `carrier.exe` size and
the `--report` JSON shape compared against the pre-move run. G3
(`bind_all`, 35 functions × 2 runs) is the real regression net.

### S3 — lifter, offline harness, verdict scripts to `tools/`

`pf_lift.py`, `lift_check.py`, the harness `.c`/`.h`, `compare_digests.py`,
`compare_fn_digests.py`, `certify_snapshot.py`, `pf_inspect.py`,
`bind_all.py`, `sendinput_session.py`, `desktop_perturb_test.py`.
`pf_rt.h`/`pf_x87_soft.h` → `src/platform/win32/`.
*Policy interface*: `--specs` JSON for the harness vector shapes; everything
else is CLI paths.
*Risk*: **low-medium** — offline, but `pf_lift.py` output feeds bound code, so
a behaviour change lands in G2/G3.
*Verification*: re-lift all 65 covered functions and diff the generated `.c`
byte-for-byte; re-run the offline vector corpus (160 000+ vectors) to EQUAL;
then G2 and G3.

### S4 — platform registration in `config/platforms-v1.json`

**Recommendation: NO, not at this stage.** Three concrete reasons:
1. `check_platform_registry.py` binds *every* closed schema `platform` enum to
   the ordered registry. Adding `win32` forces a simultaneous edit of every
   `schemas/*.schema.json` enum — a wide, low-information change.
2. pf/46 then expects a boundary profile (`portforge-boundary-profile-v1`), a
   `ReplayRuntimeAdapter`, `ReplayExecutionIdentity`, a versioned
   `CanonicalState` projection registered in the canonical-schema guard, and
   capability claims with digest-bound evidence. The carrier has **none** of
   these: `scripts/play.py` documents at length why it does not implement
   `player_runtime.py`'s declared-runtime contract (no `portforge.project.json`,
   no `game.json`, a different execution shape). Registering now creates
   `scaffolded` claims with nothing behind them — precisely what pf/46 forbids
   ("do not edit a claim to `conformant` to make `doctor` quiet").
3. Nothing in S1–S3 needs the registry: the boundary lint keys on directory
   paths, and the tool index on filenames.
*Register when*: the carrier emits a `ReplayArtifactV2`-shaped recording and a
`CanonicalState` projection — i.e. when `--record-input`/`--digest-out` are
replaced by the shared session's artifact, not before. That is a deliberate
later stage, and the digest/`fn-digest` formats are the thing to migrate.

### S5 — branch disposition

- **Lands on `main`**: S1 and S3 (Python tools + header-only `lift_rt.hpp`/
  `x87_soft.hpp`). They add files under `tools/`, break nothing on any host,
  are covered by `generate_tool_index.py --check`, and other projects can
  ignore them. `pf_gate_all.py` still runs, and passing it is the admission.
- **Stays on `experimental/win32`**: S2, plus `docs/90` amendments, until
  either (a) a second Win32 target exercises the policy structs — CyberStorm
  is the candidate — or (b) all five project gates plus `pf_gate_all` are
  green across two independent runs. `src/platform/win32` is dead code for
  every existing project, so merging early costs nothing but proves nothing.
- **`icytower_forged`** tracks `experimental/win32` through S2, and switches
  its submodule pin to `main` once S2 merges. Do not split the pin across
  branches: `pf_gate_all` measures one commit.

## 4. What must NOT move yet

1. **`det.cpp`'s Allegro tick delivery.** Read the code: the arithmetic is
   `total = virtual_ms * TIMERS_PER_SECOND / 1000; delta = total - reported;
   _handle_timer_tick((int)delta)`. The *accumulator* (running total, diff per
   call, no separate remainder, no drift) is generic and reproduces what
   `tim_win32_high_perf_thread` does with QPC. The *units* are not: `1193181`
   is the PIT frequency Allegro's `timer.h` fixes, `_handle_timer_tick(int
   interval)` is Allegro's own signature, and "a `Sleep` call on the main
   thread is a tick opportunity" is true only because Allegro's idle loops
   call `rest(1)`. **Verdict**: move the accumulator as
   `pf::win32::VirtualClock` with `TickPolicy{tick_fn_va, tick_arg_kind,
   units_per_second, pump}`; do **not** move the pump *choice* — a target
   whose main loop blocks in `MsgWaitForMultipleObjects` needs a different
   pump, and inventing that enum now with one instance would be an API
   invented for Icy Tower. Ship `pump = Import("Sleep", main_thread_only)` as
   the only value until a second target adds one.
2. **`snapshot.cpp`'s single-main-thread assumption.** The whole design rests
   on "the main thread is stopped at a safepoint, ESP is the measured constant
   `0x0e1fef30` at all 876 safepoints, other threads are suspended for the
   write-back, host handles survive because the restore is in-process". That
   is not a generic Win32 property; it is a property of *this* game's loop.
   Move the codec, the manifest and `suspend_other_threads`; leave the
   constant-ESP assertion and the region list as project policy, and do not
   generalise toward cross-process restore (measured to fail — NOTES.md §G).
3. **The digest-domain generator (`gen_game_globals.py`).** Generic *given a
   CU-scope config*, which is exactly what CyberStorm does not have. Move it
   at S1 with `--ownership dwarf-cu` as the only implemented mode, and record
   `section` and `explicit-list` as unimplemented. Do not invent the other
   modes before a target needs them.
4. **`scripts/play.py` vs `scripts/player_runtime.py`.** Two different
   contracts. `play.py` stays in this repo verbatim; its header already
   records the decision and the revisit condition. Adopting `player_runtime`
   is S4 work, not extraction work.
5. **`src/replay/input_script.hpp` / `input_delivery.hpp` adoption.** pf/76 §C
   makes `input_delivery.hpp` the ONE rule for "is this event due". The
   carrier currently answers that itself, at tick granularity, with the
   divergence-005 sub-tick coordinate. Move the Win32 *capture and delivery
   points* at S2; adopt the shared due-rule only once the sub-tick coordinate
   has an equivalent in `input_delivery.hpp`. Copying it would create the
   second implementation pf/76 exists to prevent; forcing it early would
   re-open divergence 005.
6. **`gen_assets.py` / the `.itr` format / passwords / `tools_recon`.** Named
   Icy-Tower-specific by `win32_pilot.md` §7c and `docs/90` §4. Only the
   manifest *schemas* graduate, to `schemas/`.

### 4a. Appended after S2 (2026-09-07): what stayed project-side, and why

S2 moved nine of the eleven units in §3's order. Two did not, and one
mechanism inside a moved unit did not. Recorded here rather than left as a
gap, per this section's own rule ("do not force it").

7. **The binding engine (`carrier/src/bind.cpp`, 871 lines).** Blocked on a
   real design question, not on effort. `kBindMaxFns`, `kStubs[]`, the
   `static_assert`s and the three per-function counter arrays in
   `BindSavedState` are all sized by `gen/bind_table.inc`'s `kNumFns`, which
   `build.cmd` REGENERATES on every build from whatever `src/icytower/*.c`
   currently defines (it has already moved 8 → 35 → 42 → 60 under
   concurrently-running promotion work). A framework header cannot own an
   array whose width the consuming project's build decides, without either
   threading a `constexpr` size parameter through `bind_pre`/`bind_post`/
   the stub table/the saved state, or keeping a second copy of the count —
   and the 60 naked stubs must be emitted project-side in any case, for the
   same reason `trace.hpp`'s trampoline is (inline assembly is not a C++
   reference, so an `inline` definition is emitted by no TU at all;
   measured: LNK2019 `_pf_import_common` referenced in `pf_stub_0`).
   *What DID move*: everything the engine shares with the other consumers —
   the DR0-DR3 table, `ctx_arm_slot`/`ctx_disarm_slot`, the RF step-over and
   `patch_entry_jmp` — at unit 5a. `bind.cpp` now calls
   `pf::win32::register_breakpoint` and `pf::win32::patch_entry_jmp`
   through det.cpp's thin aliases like every other consumer.
   *Revisit when*: a second target needs a binding table, which is also
   when the right size parameterization becomes visible instead of guessed.

8. **The named-globals evaluator (`carrier/src/print_globals.cpp`, 379
   lines).** Genuinely generic — no Icy Tower name appears in it, and it
   walks the generated `gen/it_print_globals.inc` typed table — but moving
   it requires the framework header to own that table's struct types
   (`PfPGVal`/`PfPGGlobal`/`PfPGMember`/`PfPGStructSize`), which the
   GENERATOR currently emits. That is a change to
   `tools/pf_win32_gen_print_globals.py` with its own regenerate-and-
   byte-diff verification loop — S1 territory, not a carrier-core move.
   Doing it inside this pass would have meant an ungated generator change
   in the middle of a gated live-code refactor.

9. **The register-passed capture shim.** `InputBindingPolicy` was planned
   to carry a `capture_abi` field. It does not: the shim that calls
   `key_dinput_handle_scancode` with its arguments in EAX/EDX is a naked
   function, and a naked function cannot be parameterized by an ABI enum
   without one stub per ABI value. One ABI has been seen. The shim stays in
   `det.cpp` with the disassembly citation beside it, and the field is not
   invented until a second convention appears.

Also recorded, because it is the pass's one real finding about the gates:
the **absolute-reference gate is the one that works**. A first attempt at
unit 5b fused the virtual clock's advance with its tick delivery, which
moved the sub-tick coordinate by one slot and shifted `human_test`'s first
recorded tick from 237 to 238. G1, G2 and G3 all stayed EQUAL — they
compare a binary against itself, and a uniformly shifted run is
self-consistent. Only G4, the run compared against the stored 2293-tick
baseline, caught it. Any future stage that drops G4 to save time is
dropping the only gate that can see a whole-run shift.

## 5. CyberStorm applicability (detail in `notes/cyberstorm_census_preview.md`)

PE32, ImageBase 0x400000, SizeOfImage 0x126000, GUI subsystem, **21 926
relocations present**, no TLS, **no debug directory at all**, Watcom C++
(`W?…$n(…)` export mangling), sections `BEGTEXT`/`DGROUP`/`.bss`/`.idata`/
`.edata`/`.reloc`/`.rsrc`, 162 imports over KERNEL32/USER32/GDI32 plus the
sidecar `_INMM.dll`.

- **Unchanged**: PE mapper (base is the same and free), import census →
  trampolines, the wrapper set, VEH+stack walk, breakpoint table, arg sensor,
  bind engine + entry patch, arena, RNG, virtual clock *mechanism*, snapshot
  codec, lifter + offline oracle, all four verdict scripts.
- **Needs a non-DWARF symbol source**: every generator in §1b except
  `gen_imports`, plus `pf_inspect.py` and the digest domain. port_forge
  already ships `tools/pf_structural_discovery.py`, `tools/pf_match.py` and
  `scripts/function_fingerprint.py` — that is the bridge to build, and it is
  the reason `--ownership` must be a flag from day one.
- **Assumptions that break**: "no relocations ⇒ must load at ImageBase"
  (`require_fixed_base` becomes a field, and a rebasing path becomes possible);
  section names; DWARF-CU ownership for the digest domain; DLL keys as
  case-sensitive strings; and every Allegro-shaped policy field (there is no
  `_handle_timer_tick`, no `blit_to_screen`, no `hw_to_mycode`) — which is the
  point of making them fields.
